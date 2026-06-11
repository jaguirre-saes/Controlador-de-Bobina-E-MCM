#include "rs485.h"

#include "buck.h"
#include "hbridge.h"
#include "uart_app.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUCK_VLIMIT_DEFAULT_V     60.0
#define BUCK_ISET_STEP_A          0.5
#define BUCK_ISET_STEP_DELAY_MS   500u
#define BUCK_ISET_MAX_A           5.0
#define RS485_HEARTBEAT_TIMEOUT_MS 5000u
#define RS485_TTL_LOSS_TIMEOUT_MS  5000u
#define RS485_TTL_RECONNECT_MS     200u
#define MCU_HB_INTERVAL_MS         1000u

static UART_HandleTypeDef *g_rs485_uart = NULL;
static uint8_t g_rx_byte = 0;
static char g_line[96];
static uint32_t g_idx = 0;
static volatile uint8_t g_line_ready = 0;

static double g_iset_abs = 0.0;
static int g_dir = +1;
static uint8_t g_on = 0;
static double g_iapplied_abs = 0.0;
static volatile uint32_t g_last_heartbeat_tick = 0u;
static volatile uint8_t g_hb_watchdog_armed = 0u;  /* solo se activa via comando HB */
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
  while ((n > 0) &&
         (s[n - 1] == '\r' || s[n - 1] == '\n' || isspace((unsigned char)s[n - 1])))
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

static void ramp_buck_current(double from_abs, double to_abs)
{
  double current;

  if (from_abs < 0.0)
  {
    from_abs = 0.0;
  }

  if (to_abs < 0.0)
  {
    to_abs = 0.0;
  }

  if (to_abs > BUCK_ISET_MAX_A)
  {
    to_abs = BUCK_ISET_MAX_A;
  }

  current = from_abs;
  while (current < to_abs)
  {
    current += BUCK_ISET_STEP_A;
    if (current > to_abs)
    {
      current = to_abs;
    }

    buck_set_current_abs(current);
    HAL_Delay(BUCK_ISET_STEP_DELAY_MS);
  }

  while (current > to_abs)
  {
    current -= BUCK_ISET_STEP_A;
    if (current < to_abs)
    {
      current = to_abs;
    }

    buck_set_current_abs(current);
    HAL_Delay(BUCK_ISET_STEP_DELAY_MS);
  }
}

static void apply_state(uint8_t on, int dir, double i_abs)
{
  if ((!on) || (i_abs <= 0.0))
  {
    coil_enable(0);
    HAL_Delay(150);

    buck_set_current_abs(0.0);
    HAL_Delay(80);

    buck_output(0);
    HAL_Delay(200);

    g_iapplied_abs = 0.0;

    return;
  }

  if (i_abs > BUCK_ISET_MAX_A)
  {
    i_abs = BUCK_ISET_MAX_A;
  }

  coil_enable(0);
  HAL_Delay(200);

  buck_output(0);
  HAL_Delay(250);

  coil_set_dir(dir);
  HAL_Delay(300);

  buck_set_current_abs(0.0);
  HAL_Delay(80);

  buck_set_voltage_abs(BUCK_VLIMIT_DEFAULT_V);
  HAL_Delay(120);

  buck_output(1);
  HAL_Delay(250);

  coil_enable(1);
  HAL_Delay(100);

  ramp_buck_current(0.0, i_abs);
  g_iapplied_abs = i_abs;
}

static void handle_pc_line(const char *line_in)
{
  char line[96];
  const char *p;
  char *end = NULL;
  double i;
  double i_abs;
  int req_dir;

  snprintf(line, sizeof(line), "%s", line_in);
  trim(line);

  if (line[0] == '\0')
  {
    return;
  }

  g_last_heartbeat_tick = HAL_GetTick();

  if (streq_ci(line, "HELP"))
  {
    pc_print(
      "Commands:\r\n"
      "  I 1.50     -> corriente positiva 1.50A\r\n"
      "  I -1.50    -> corriente negativa 1.50A\r\n"
      "  ON         -> enciende salida\r\n"
      "  OFF        -> apaga salida\r\n"
      "  READ       -> lee tension medida y estado\r\n"
      "  HB | PING  -> latido del portatil (watchdog 5s)\r\n"
      "  Auto SAFE  -> si TTL cae 5s: buck OFF; reconexion: reset MCU\r\n"
      "  HELP\r\n");
    return;
  }

  if (streq_ci(line, "HB") || streq_ci(line, "PING"))
  {
    /* HB/PING: arma el watchdog y resetea el timer. Silencioso. */
    g_hb_watchdog_armed = 1u;
    g_last_heartbeat_tick = HAL_GetTick();
    return;
  }

  if (streq_ci(line, "OFF") || (strcmp(line, "0") == 0))
  {
    g_on = 0;
    g_iset_abs = 0.0;
    g_hb_watchdog_armed = 0u;  /* al apagar manualmente, desarmar el watchdog */
    apply_state(0, g_dir, 0.0);
    pc_print("OK OFF\r\n");
    return;
  }

  if (streq_ci(line, "ON"))
  {
    int i100;

    if (g_iset_abs <= 0.0)
    {
      pc_print("ERR ISET=0\r\n");
      return;
    }

    if (g_on && (g_iapplied_abs > 0.0))
    {
      ramp_buck_current(g_iapplied_abs, g_iset_abs);
      g_iapplied_abs = g_iset_abs;
    }
    else
    {
      g_on = 1;
      apply_state(1, g_dir, g_iset_abs);
    }

    g_on = 1;
    i100 = to_centiamp(g_iset_abs);
    g_hb_watchdog_armed = 1u;  /* armar watchdog al encender el buck */
    g_last_heartbeat_tick = HAL_GetTick();
    pc_printf("OK ON DIR=%d I=%d.%02dA\r\n", g_dir, i100 / 100, i100 % 100);
    return;
  }

  if (streq_ci(line, "READ"))
  {
    int i100;

    buck_read_vmeas_print();
    i100 = to_centiamp(g_iset_abs);
    pc_printf("LOCAL: ON=%d DIR=%d ISET=%d.%02dA\r\n", (int)g_on, g_dir, i100 / 100, i100 % 100);
    return;
  }

  p = line;
  while ((*p != '\0') && isspace((unsigned char)*p))
  {
    p++;
  }

  if (*p == 'I' || *p == 'i')
  {
    p++;
    while ((*p != '\0') && (isspace((unsigned char)*p) || *p == '='))
    {
      p++;
    }
  }

  while ((*p != '\0') && isspace((unsigned char)*p))
  {
    p++;
  }

  req_dir = g_dir;
  if (*p == '-')
  {
    req_dir = -1;
  }
  else if (*p == '+')
  {
    req_dir = +1;
  }

  i = strtod(p, &end);
  if (end == p)
  {
    pc_print("ERR CMD\r\n");
    return;
  }

  i_abs = (i < 0.0) ? -i : i;
  if (i_abs <= 0.0)
  {
    pc_print("ERR RANGE\r\n");
    return;
  }

  if (i_abs > BUCK_ISET_MAX_A)
  {
    pc_print("ERR RANGE 0..5\r\n");
    return;
  }

  g_iset_abs = i_abs;
  g_on = 1;

  if ((g_iapplied_abs > 0.0) && (req_dir == g_dir))
  {
    ramp_buck_current(g_iapplied_abs, g_iset_abs);
    g_iapplied_abs = g_iset_abs;
  }
  else
  {
    g_dir = req_dir;
    apply_state(1, g_dir, g_iset_abs);
  }

  {
    int i100;
    i100 = to_centiamp(g_iset_abs);
    pc_printf("OK SET DIR=%d I=%d.%02dA\r\n", g_dir, i100 / 100, i100 % 100);
  }

  /* Armar watchdog y reiniciar timer tras aplicar la corriente */
  g_hb_watchdog_armed = 1u;
  g_last_heartbeat_tick = HAL_GetTick();
}

void rs485_init(UART_HandleTypeDef *huart)
{
  g_rs485_uart = huart;
  g_idx = 0;
  g_line_ready = 0;
  g_last_heartbeat_tick = HAL_GetTick();
  g_hb_watchdog_armed = 0u;  /* se arma solo cuando el PC envie el comando HB */
  g_mcu_hb_tick = HAL_GetTick();
  (void)HAL_UART_Receive_IT(g_rs485_uart, &g_rx_byte, 1);
}

void rs485_poll(void)
{
  char line_copy[sizeof(g_line)];
  uint8_t has_line;
  uint32_t now;

  now = HAL_GetTick();
  /* Watchdog solo actua si fue armado explicitamente via HB/PING.           */
  /* Sin HB (PuTTY manual): watchdog inactivo, buck funciona sin limite.     */
  /* Con ttl_heartbeat.ps1: envia HB cada 1s, watchdog activo.              */
  if (g_hb_watchdog_armed && (g_on || (g_iapplied_abs > 0.0)))
  {
    if ((now - g_last_heartbeat_tick) > RS485_HEARTBEAT_TIMEOUT_MS)
    {
      g_on = 0;
      g_iset_abs = 0.0;
      apply_state(0, g_dir, 0.0);
      g_last_heartbeat_tick = now;
      pc_print("SAFE OFF HEARTBEAT TIMEOUT\r\n");
    }
  }

  /* MCU -> PC periodic heartbeat */
  if ((now - g_mcu_hb_tick) >= MCU_HB_INTERVAL_MS)
  {
    g_mcu_hb_tick = now;
    pc_print("ALIVE\r\n");
  }

  has_line = 0u;
  __disable_irq();
  if (g_line_ready)
  {
    memcpy(line_copy, g_line, sizeof(line_copy));
    g_line_ready = 0;
    has_line = 1u;
  }
  __enable_irq();

  if (has_line)
  {
    handle_pc_line(line_copy);
  }
}

void rs485_uart_rx_cplt_callback(UART_HandleTypeDef *huart)
{
  if ((g_rs485_uart == NULL) || (huart->Instance != g_rs485_uart->Instance))
  {
    return;
  }

  /* El ISR SOLO acumula bytes en el buffer de linea.                         */
  /* El timer del watchdog lo resetea UNICAMENTE el comando HB/PING           */
  /* procesado en handle_pc_line(). No se hace aqui para evitar que           */
  /* cualquier ruido electrico en RX (pulsos de STLINK al reconectar,         */
  /* framing errors, etc.) mantenga vivo el watchdog sin querer.             */
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
        g_idx = 0;
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

  g_idx = 0;
  g_line_ready = 0;
  (void)HAL_UART_Receive_IT(g_rs485_uart, &g_rx_byte, 1);
}
