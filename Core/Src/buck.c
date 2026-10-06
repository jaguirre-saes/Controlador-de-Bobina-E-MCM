#include "buck.h"

#include "app_config.h"
#include "current_sensor.h"
#include "uart_app.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static UART_HandleTypeDef *g_buck_uart = NULL;
static uint8_t g_buck_addr = CFG_BUCK_ADDRESS;

void buck_init(UART_HandleTypeDef *huart, uint8_t address)
{
  g_buck_uart = huart;
  g_buck_addr = address;
}

HAL_StatusTypeDef buck_send(const char *cmd)
{
  if ((g_buck_uart == NULL) || (cmd == NULL))
  {
    return HAL_ERROR;
  }

  return HAL_UART_Transmit(g_buck_uart, (uint8_t *)cmd, (uint16_t)strlen(cmd), CFG_BUCK_UART_TIMEOUT_MS);
}

int buck_readline(char *out, size_t maxlen, uint32_t timeout_ms)
{
  uint32_t t0;
  size_t i = 0;
  uint8_t b = 0;

  if ((g_buck_uart == NULL) || (out == NULL) || (maxlen == 0u))
  {
    return -1;
  }

  out[0] = '\0';
  t0 = HAL_GetTick();

  while (((HAL_GetTick() - t0) < timeout_ms) && (i < (maxlen - 1u)))
  {
    if (HAL_UART_Receive(g_buck_uart, &b, 1, 10) == HAL_OK)
    {
      out[i++] = (char)b;
      if (b == '\n')
      {
        break;
      }
    }
  }

  out[i] = '\0';
  return (int)i;
}

int parse_int_after_equal(const char *resp, int *out_val)
{
  const char *eq;
  int v = 0;
  int any = 0;

  if ((resp == NULL) || (out_val == NULL))
  {
    return 0;
  }

  eq = strchr(resp, '=');
  if (eq == NULL)
  {
    return 0;
  }

  eq++;

  while ((*eq != '\0') && isdigit((unsigned char)*eq))
  {
    any = 1;
    v = (v * 10) + (*eq - '0');
    eq++;
  }

  if (!any)
  {
    return 0;
  }

  *out_val = v;
  return 1;
}

void buck_set_voltage_abs(double v_abs)
{
  char cmd[40];
  int v100;

  if (g_buck_uart == NULL)
  {
    return;
  }

  if (v_abs < 0.0)
  {
    v_abs = -v_abs;
  }

  /* !(x <= max) tambien atrapa NaN */
  if (!(v_abs <= CFG_V_MAX_V))
  {
    v_abs = CFG_V_MAX_V;
  }

  v100 = (int)(v_abs * 100.0 + 0.5);
  snprintf(cmd, sizeof(cmd), ":%02uw10=%d,\r\n", (unsigned)g_buck_addr, v100);
  (void)buck_send(cmd);
}

/* Correccion de 2 puntos (ver app_config.h):
 *   setpoint 0.500A -> real 0.4444A  (6.71 V / 15.1 Ohm)
 *   setpoint 1.000A -> real 0.9536A  (14.4 V / 15.1 Ohm) */
static double buck_cal_compensate(double i_abs)
{
  if (i_abs < CFG_BUCK_CAL_MIN_A)
  {
    return i_abs;
  }
  return (i_abs + CFG_BUCK_CAL_OFFSET_A) / CFG_BUCK_CAL_SLOPE;
}

void buck_set_current_abs(double i_abs)
{
  char cmd[40];
  int i1000;

  if (g_buck_uart == NULL)
  {
    return;
  }

  if (i_abs < 0.0)
  {
    i_abs = -i_abs;
  }

  /* !(x <= max) tambien atrapa NaN */
  if (!(i_abs <= CFG_I_MAX_A))
  {
    i_abs = CFG_I_MAX_A;
  }

  i_abs = buck_cal_compensate(i_abs);

  i1000 = (int)(i_abs * 1000.0 + 0.5);
  snprintf(cmd, sizeof(cmd), ":%02uw11=%d,\r\n", (unsigned)g_buck_addr, i1000);
  (void)buck_send(cmd);
}

void buck_output(uint8_t on)
{
  char cmd[32];

  if (g_buck_uart == NULL)
  {
    return;
  }

  snprintf(cmd, sizeof(cmd), ":%02uw12=%d,\r\n", (unsigned)g_buck_addr, on ? 1 : 0);
  (void)buck_send(cmd);
}

int buck_read_vmeas_cV(int *out_v100)
{
  char req[32];
  char resp[80];
  int n;
  int v100 = 0;

  if ((g_buck_uart == NULL) || (out_v100 == NULL))
  {
    return 0;
  }

  snprintf(req, sizeof(req), ":%02ur30=0,\r\n", (unsigned)g_buck_addr);

  if (buck_send(req) != HAL_OK)
  {
    return 0;
  }

  HAL_Delay(3);

  n = buck_readline(resp, sizeof(resp), CFG_BUCK_REPLY_TIMEOUT_MS);
  if ((n <= 0) || !parse_int_after_equal(resp, &v100))
  {
    return 0;
  }

  *out_v100 = v100;
  return 1;
}

void buck_read_vmeas_print(void)
{
  int v100 = 0;

  if (g_buck_uart == NULL)
  {
    pc_print("ERR UART5\r\n");
    return;
  }

  if (!buck_read_vmeas_cV(&v100))
  {
    pc_print("VMEAS=ERR\r\n");
    return;
  }

  pc_printf("VMEAS=%d.%02d\r\n", v100 / 100, v100 % 100);
}

/* --------------------------------------------------------------------------
 * Registro r31: corriente medida por el buck (mismas unidades que w11, mA).
 * Ajustar BUCK_REG_IMEAS si el numero de registro es distinto en el equipo.
 * -------------------------------------------------------------------------- */
#define BUCK_REG_IMEAS          31u

static uint32_t g_wdg_last_ms    = 0u;
static int      g_wdg_err_cnt   = 0;
static uint8_t  g_wdg_tripped   = 0u;
static uint8_t  g_wdg_active    = 0u;   /* solo activo mientras buck ON */
static float    g_wdg_setpt_mA  = 0.0f; /* setpoint real como referencia */

int buck_read_imeas_mA(float *out_mA)
{
  char req[32];
  char resp[80];
  int  n;
  int  i_raw = 0;

  if ((g_buck_uart == NULL) || (out_mA == NULL))
  {
    return 0;
  }

  snprintf(req, sizeof(req), ":%02ur%u=0,\r\n", (unsigned)g_buck_addr, (unsigned)BUCK_REG_IMEAS);

  if (buck_send(req) != HAL_OK)
  {
    return 0;
  }

  HAL_Delay(3);

  n = buck_readline(resp, sizeof(resp), CFG_BUCK_REPLY_TIMEOUT_MS);
  if (n <= 0)
  {
    return 0;
  }

  if (!parse_int_after_equal(resp, &i_raw))
  {
    return 0;
  }

  /* El registro devuelve el valor en las mismas unidades que w11:
   * entero = corriente * 1000, es decir, directamente en mA. */
  *out_mA = (float)i_raw;
  return 1;
}

void buck_current_watchdog_arm(float setpoint_mA)
{
  g_wdg_setpt_mA = (setpoint_mA < 0.0f) ? -setpoint_mA : setpoint_mA;
  g_wdg_err_cnt  = 0;
  g_wdg_tripped  = 0u;
  g_wdg_active   = 1u;
  g_wdg_last_ms  = HAL_GetTick();  /* primera comprobacion tras un periodo completo */
}

void buck_current_watchdog_reset(void)
{
  g_wdg_err_cnt = 0;
  g_wdg_tripped = 0u;
  g_wdg_active  = 0u;
  g_wdg_setpt_mA = 0.0f;
}

uint8_t buck_current_watchdog_is_tripped(void)
{
  return g_wdg_tripped;
}

void buck_current_watchdog_task(void)
{
  uint32_t now;
  float    ina_mA;
  float    ref;
  float    diff;
  int      err_pct;
  int      ina_int;
  int      buck_int;

  if (!g_wdg_active || g_wdg_tripped)
  {
    return;
  }

  now = HAL_GetTick();
  if ((now - g_wdg_last_ms) < CFG_WDG_CHECK_MS)
  {
    return;
  }
  g_wdg_last_ms = now;

  /* Sin muestra valida (sensor ausente o fallo I2C) no se compara:
   * evita disparos/omisiones por un valor antiguo. */
  if (!current_sensor_has_valid_sample())
  {
    return;
  }

  ina_mA = current_sensor_get_current_mA();
  if (ina_mA < 0.0f)
  {
    ina_mA = -ina_mA;
  }

  /* Usar el setpoint conocido como referencia (el registro r31 no responde) */
  ref = (ina_mA > g_wdg_setpt_mA) ? ina_mA : g_wdg_setpt_mA;
  if (ref < CFG_WDG_MIN_mA)
  {
    g_wdg_err_cnt = 0;
    return;
  }

  diff    = (ina_mA > g_wdg_setpt_mA) ? (ina_mA - g_wdg_setpt_mA) : (g_wdg_setpt_mA - ina_mA);
  err_pct = (int)((diff / ref) * 100.0f + 0.5f);

  if (err_pct > CFG_WDG_ERROR_PCT)
  {
    g_wdg_err_cnt++;
    ina_int  = (int)(ina_mA          + 0.5f);
    buck_int = (int)(g_wdg_setpt_mA  + 0.5f);
    pc_printf("WDG SENSOR ERR %d/%d: INA=%dmA SETPT=%dmA ERR=%d%%\r\n",
              g_wdg_err_cnt, CFG_WDG_TRIP_COUNT, ina_int, buck_int, err_pct);

    if (g_wdg_err_cnt >= CFG_WDG_TRIP_COUNT)
    {
      g_wdg_tripped = 1u;
      pc_printf("WDG TRIP: >%d%% error x%d - APAGANDO BUCK\r\n",
                CFG_WDG_ERROR_PCT, CFG_WDG_TRIP_COUNT);
      /* El apagado lo ejecuta coil_ctrl_task() en esta misma vuelta del bucle */
    }
  }
  else
  {
    g_wdg_err_cnt = 0;
  }
}
