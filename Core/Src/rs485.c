#include "rs485.h"

#include "app_config.h"
#include "buck.h"
#include "coil_ctrl.h"
#include "current_sensor.h"
#include "safety.h"
#include "uart_app.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static UART_HandleTypeDef *g_rs485_uart = NULL;
static uint8_t g_rx_byte = 0;
static char g_line[96];
static volatile uint32_t g_idx = 0;
static volatile uint8_t g_line_ready = 0;

static double g_iset_abs = 0.0;
static uint32_t g_last_heartbeat_tick = 0u;
static uint8_t g_hb_watchdog_armed = 0u;
static uint32_t g_mcu_hb_tick = 0u;

static int to_centiamp(double i)
{
  if (i < 0.0)
  {
    i = -i;
  }
  return (int)(i * 100.0 + 0.5);
}

static void trim(char *s)
{
  size_t n;
  char *p;

  n = strlen(s);
  while ((n > 0) && isspace((unsigned char)s[n - 1]))
  {
    s[n - 1] = '\0';
    n--;
  }

  p = s;
  while ((*p != '\0') && isspace((unsigned char)*p))
  {
    p++;
  }

  if (p != s)
  {
    memmove(s, p, strlen(p) + 1);
  }
}

static int streq_ci(const char *a, const char *b)
{
  while ((*a != '\0') && (*b != '\0'))
  {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
    {
      return 0;
    }
    a++;
    b++;
  }

  return (*a == '\0' && *b == '\0');
}

static void heartbeat_arm(void)
{
  g_hb_watchdog_armed = 1u;
  g_last_heartbeat_tick = HAL_GetTick();
}

static void request_current(int dir, double i_abs)
{
  coil_ctrl_request(dir, i_abs);
  heartbeat_arm();
}

static void print_help(void)
{
  int max100 = to_centiamp(CFG_I_MAX_A);

  pc_print("Commands:\r\n");
  pc_printf("  I 1.50     -> corriente positiva 1.50A (max %d.%02dA)\r\n", max100 / 100, max100 % 100);
  pc_print("  I -1.50    -> corriente negativa 1.50A\r\n"
           "  ON         -> enciende salida con la ultima ISET\r\n"
           "  OFF | 0    -> apaga salida\r\n"
           "  READ       -> lee tension medida y estado\r\n"
           "  STATUS     -> estado completo en JSON\r\n");
  pc_printf("  HB | PING  -> latido del PC (SAFE OFF si %lu ms sin comandos)\r\n",
            (unsigned long)CFG_PC_HEARTBEAT_TIMEOUT_MS);
  pc_print("  HELP\r\n");
}

static void print_status_json(void)
{
  int iset100 = to_centiamp(g_iset_abs);
  int iapp100 = to_centiamp(coil_ctrl_get_applied_abs());
  float meas = current_sensor_get_current_mA();
  int meas_c = (int)(meas * 100.0f + ((meas >= 0.0f) ? 0.5f : -0.5f));
  const char *meas_sign = (meas_c < 0) ? "-" : "";
  int v100 = 0;

  if (meas_c < 0)
  {
    meas_c = -meas_c;
  }

  pc_printf("{\"state\":\"%s\",\"on\":%d,\"dir\":%d,\"iset_A\":%d.%02d,\"iapplied_A\":%d.%02d,",
            coil_ctrl_state_name(), (int)coil_ctrl_is_on(), coil_ctrl_get_dir(),
            iset100 / 100, iset100 % 100, iapp100 / 100, iapp100 % 100);
  pc_printf("\"imeas_mA\":%s%d.%02d,\"sensor_ok\":%d,\"hb_armed\":%d,\"last_fault\":\"%s\",",
            meas_sign, meas_c / 100, meas_c % 100,
            (int)current_sensor_has_valid_sample(), (int)g_hb_watchdog_armed,
            coil_ctrl_fault_name());
  if (buck_read_vmeas_cV(&v100))
  {
    pc_printf("\"vout_V\":%d.%02d,", v100 / 100, v100 % 100);
  }
  else
  {
    pc_print("\"vout_V\":null,");
  }
  pc_printf("\"iwdg_reset\":%d,\"uptime_ms\":%lu}\r\n",
            (int)safety_reset_was_iwdg(), (unsigned long)HAL_GetTick());
}

static void handle_current_cmd(const char *line)
{
  const char *p = line;
  char *end = NULL;
  double i;
  double i_abs;
  int req_dir;
  int i100;

  if ((*p == 'I') || (*p == 'i'))
  {
    p++;
    while ((*p != '\0') && (isspace((unsigned char)*p) || (*p == '=')))
    {
      p++;
    }
  }

  /* Sin signo = positiva (como indica HELP); el signo no depende del estado anterior */
  req_dir = (*p == '-') ? -1 : +1;

  i = strtod(p, &end);
  if (end == p)
  {
    pc_print("ERR CMD\r\n");
    return;
  }

  while ((*end != '\0') && isspace((unsigned char)*end))
  {
    end++;
  }
  if (*end != '\0')
  {
    pc_print("ERR CMD\r\n");
    return;
  }

  i_abs = (i < 0.0) ? -i : i;

  /* Escrito asi para rechazar tambien NaN/inf ("I nan") */
  if (!((i_abs > 0.0) && (i_abs <= CFG_I_MAX_A)))
  {
    i100 = to_centiamp(CFG_I_MAX_A);
    pc_printf("ERR RANGE 0..%d.%02d\r\n", i100 / 100, i100 % 100);
    return;
  }

  g_iset_abs = i_abs;
  request_current(req_dir, g_iset_abs);

  i100 = to_centiamp(g_iset_abs);
  pc_printf("OK SET DIR=%d I=%d.%02dA\r\n", req_dir, i100 / 100, i100 % 100);
}

static void handle_pc_line(const char *line_in)
{
  char line[sizeof(g_line)];
  int i100;

  snprintf(line, sizeof(line), "%s", line_in);
  trim(line);

  if (line[0] == '\0')
  {
    return;
  }

  /* Cualquier comando cuenta como actividad del PC */
  g_last_heartbeat_tick = HAL_GetTick();

  if (streq_ci(line, "HELP"))
  {
    print_help();
    return;
  }

  if (streq_ci(line, "HB") || streq_ci(line, "PING"))
  {
    heartbeat_arm();
    return;
  }

  if (streq_ci(line, "STATUS"))
  {
    print_status_json();
    return;
  }

  if (streq_ci(line, "OFF") || (strcmp(line, "0") == 0))
  {
    g_hb_watchdog_armed = 0u;
    g_iset_abs = 0.0;
    coil_ctrl_off(CC_FAULT_NONE);
    pc_print("OK OFF\r\n");
    return;
  }

  if (streq_ci(line, "ON"))
  {
    if (g_iset_abs <= 0.0)
    {
      pc_print("ERR ISET=0\r\n");
      return;
    }

    request_current(coil_ctrl_get_dir(), g_iset_abs);
    i100 = to_centiamp(g_iset_abs);
    pc_printf("OK ON DIR=%d I=%d.%02dA\r\n", coil_ctrl_get_dir(), i100 / 100, i100 % 100);
    return;
  }

  if (streq_ci(line, "READ"))
  {
    buck_read_vmeas_print();
    current_sensor_print_debug();
    i100 = to_centiamp(g_iset_abs);
    pc_printf("LOCAL: ON=%d DIR=%d ISET=%d.%02dA STATE=%s\r\n", (int)coil_ctrl_is_on(),
              coil_ctrl_get_dir(), i100 / 100, i100 % 100, coil_ctrl_state_name());
    return;
  }

  handle_current_cmd(line);
}

void rs485_init(UART_HandleTypeDef *huart)
{
  g_rs485_uart = huart;
  g_idx = 0;
  g_line_ready = 0;
  g_last_heartbeat_tick = HAL_GetTick();
  g_hb_watchdog_armed = 0u;
  g_mcu_hb_tick = HAL_GetTick();
  (void)HAL_UART_Receive_IT(g_rs485_uart, &g_rx_byte, 1);
}

/* Si la recepcion por interrupcion se ha quedado parada (error de UART o
 * Receive_IT fallido) el MCU dejaria de atender comandos: la relanzamos. */
static void rx_ensure_running(void)
{
  if (g_rs485_uart == NULL)
  {
    return;
  }

  if (g_rs485_uart->RxState == HAL_UART_STATE_READY)
  {
    g_idx = 0;
    (void)HAL_UART_Receive_IT(g_rs485_uart, &g_rx_byte, 1);
  }
}

void rs485_poll(void)
{
  char line_copy[sizeof(g_line)];
  uint8_t has_line;

  rx_ensure_running();

  has_line = 0u;
  __disable_irq();
  if (g_line_ready)
  {
    memcpy(line_copy, g_line, sizeof(line_copy));
    g_line_ready = 0;
    has_line = 1u;
  }
  __enable_irq();

  /* Procesar la linea ANTES de evaluar el timeout: un HB recien llegado
   * no debe provocar un SAFE OFF. */
  if (has_line)
  {
    handle_pc_line(line_copy);
  }

  /* Watchdog del PC: se arma con HB/PING, ON o I y se desarma con OFF.
   * Si el PC deja de enviar comandos durante el timeout, se apaga la salida. */
  if (g_hb_watchdog_armed && coil_ctrl_is_on())
  {
    if ((HAL_GetTick() - g_last_heartbeat_tick) > CFG_PC_HEARTBEAT_TIMEOUT_MS)
    {
      g_iset_abs = 0.0;
      coil_ctrl_off(CC_FAULT_HB_TIMEOUT);
      g_last_heartbeat_tick = HAL_GetTick();
      pc_print("SAFE OFF HEARTBEAT TIMEOUT\r\n");
    }
  }

  if ((HAL_GetTick() - g_mcu_hb_tick) >= CFG_MCU_ALIVE_INTERVAL_MS)
  {
    g_mcu_hb_tick = HAL_GetTick();
    pc_print("ALIVE\r\n");
  }
}

void rs485_uart_rx_cplt_callback(UART_HandleTypeDef *huart)
{
  if ((g_rs485_uart == NULL) || (huart->Instance != g_rs485_uart->Instance))
  {
    return;
  }

  /* El ISR solo acumula bytes; la linea se procesa en rs485_poll(). */
  if (!g_line_ready)
  {
    if ((g_rx_byte == '\r') || (g_rx_byte == '\n'))
    {
      if (g_idx > 0)
      {
        g_line[g_idx] = '\0';
        g_idx = 0;
        g_line_ready = 1;
      }
    }
    else
    {
      if (g_idx < (sizeof(g_line) - 1))
      {
        g_line[g_idx++] = (char)g_rx_byte;
      }
      else
      {
        g_idx = 0;  /* linea demasiado larga: descartar */
      }
    }
  }

  (void)HAL_UART_Receive_IT(g_rs485_uart, &g_rx_byte, 1);
}

void rs485_uart_error_callback(UART_HandleTypeDef *huart)
{
  if ((g_rs485_uart == NULL) || (huart->Instance != g_rs485_uart->Instance))
  {
    return;
  }

  /* Descartar solo la linea a medias; una linea completa pendiente
   * (g_line_ready) se conserva para que rs485_poll() la procese. */
  g_idx = 0;
  (void)HAL_UART_Receive_IT(g_rs485_uart, &g_rx_byte, 1);
}
