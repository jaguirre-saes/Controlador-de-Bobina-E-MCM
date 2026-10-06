#include "coil_ctrl.h"

#include "app_config.h"
#include "buck.h"
#include "current_sensor.h"
#include "hbridge.h"
#include "uart_app.h"

typedef enum
{
  CC_IDLE = 0,
  CC_ON_SEQ,
  CC_RAMP,
  CC_RUNNING,
  CC_OFF_SEQ
} cc_state_t;

/* Pasos de la secuencia de encendido (mismo orden y tiempos que antes) */
enum
{
  ON_COIL_DISABLE = 0,
  ON_BUCK_OUTPUT_OFF,
  ON_SET_DIR,
  ON_BUCK_ZERO_CURRENT,
  ON_BUCK_SET_VLIMIT,
  ON_BUCK_OUTPUT_ON,
  ON_COIL_ENABLE,
  ON_NUM_STEPS
};
static const uint16_t k_on_delay_ms[ON_NUM_STEPS] = { 200u, 250u, 300u, 80u, 120u, 250u, 100u };

/* Pasos de la secuencia de apagado */
enum
{
  OFF_COIL_DISABLE = 0,
  OFF_BUCK_ZERO_CURRENT,
  OFF_BUCK_OUTPUT_OFF,
  OFF_NUM_STEPS
};
static const uint16_t k_off_delay_ms[OFF_NUM_STEPS] = { 150u, 80u, 200u };

static cc_state_t g_state = CC_IDLE;
static uint8_t    g_step = 0u;
static uint32_t   g_t0 = 0u;
static uint32_t   g_wait_ms = 0u;

static int        g_dir = +1;
static double     g_target_abs = 0.0;
static double     g_applied_abs = 0.0;
static cc_fault_t g_last_fault = CC_FAULT_NONE;

static int to_centiamp(double i)
{
  if (i < 0.0)
  {
    i = -i;
  }
  return (int)(i * 100.0 + 0.5);
}

static void enter_state(cc_state_t s)
{
  g_state = s;
  g_step = 0u;
  g_t0 = HAL_GetTick();
  g_wait_ms = 0u;  /* el primer paso se ejecuta en la siguiente llamada a task */
}

static void emergency_off(cc_fault_t reason)
{
  buck_current_watchdog_reset();

  coil_enable(0);
  buck_set_current_abs(0.0);
  buck_output(0);

  g_applied_abs = 0.0;
  g_target_abs = 0.0;
  g_last_fault = reason;
  g_state = CC_IDLE;
  current_sensor_set_output_active(0);
}

static void run_on_step(uint8_t step)
{
  switch (step)
  {
    case ON_COIL_DISABLE:      coil_enable(0); break;
    case ON_BUCK_OUTPUT_OFF:   buck_output(0); break;
    case ON_SET_DIR:           coil_set_dir(g_dir); break;
    case ON_BUCK_ZERO_CURRENT: buck_set_current_abs(0.0); g_applied_abs = 0.0; break;
    case ON_BUCK_SET_VLIMIT:   buck_set_voltage_abs(CFG_V_LIMIT_V); break;
    case ON_BUCK_OUTPUT_ON:    buck_output(1); break;
    case ON_COIL_ENABLE:       coil_enable(1); break;
    default: break;
  }
}

static void run_off_step(uint8_t step)
{
  switch (step)
  {
    case OFF_COIL_DISABLE:      coil_enable(0); break;
    case OFF_BUCK_ZERO_CURRENT: buck_set_current_abs(0.0); g_applied_abs = 0.0; break;
    case OFF_BUCK_OUTPUT_OFF:   buck_output(0); break;
    default: break;
  }
}

static void ramp_step(void)
{
  int i100;

  if (g_applied_abs == g_target_abs)
  {
    g_state = CC_RUNNING;
    buck_current_watchdog_arm((float)(g_applied_abs * 1000.0));
    i100 = to_centiamp(g_applied_abs);
    pc_printf("OK READY DIR=%d I=%d.%02dA\r\n", g_dir, i100 / 100, i100 % 100);
    return;
  }

  if (g_applied_abs < g_target_abs)
  {
    g_applied_abs += CFG_RAMP_STEP_A;
    if (g_applied_abs > g_target_abs)
    {
      g_applied_abs = g_target_abs;
    }
  }
  else
  {
    g_applied_abs -= CFG_RAMP_STEP_A;
    if (g_applied_abs < g_target_abs)
    {
      g_applied_abs = g_target_abs;
    }
  }

  buck_set_current_abs(g_applied_abs);
  g_wait_ms = CFG_RAMP_STEP_DELAY_MS;
}

static void check_protections(void)
{
  float i_mA;
  int i_int;

  if (buck_current_watchdog_is_tripped())
  {
    emergency_off(CC_FAULT_WDG_MISMATCH);
    pc_print("WDG RESET: envia I <valor> para volver a encender\r\n");
    return;
  }

  if ((g_state == CC_IDLE) || !current_sensor_has_valid_sample())
  {
    return;
  }

  i_mA = current_sensor_get_current_mA();
  if (i_mA < 0.0f)
  {
    i_mA = -i_mA;
  }

  if (i_mA > (float)(CFG_I_TRIP_A * 1000.0))
  {
    emergency_off(CC_FAULT_OVERCURRENT);
    i_int = (int)(i_mA + 0.5f);
    pc_printf("OVERCURRENT TRIP: INA=%dmA > %dmA - APAGANDO BUCK\r\n",
              i_int, (int)(CFG_I_TRIP_A * 1000.0 + 0.5));
    pc_print("Envia I <valor> para volver a encender\r\n");
  }
}

void coil_ctrl_init(void)
{
  /* Arranque seguro: si el MCU se reinicio con el buck encendido,
   * el buck seguiria dando la ultima corriente. Se apaga siempre. */
  coil_enable(0);
  buck_set_current_abs(0.0);
  buck_output(0);

  g_state = CC_IDLE;
  g_applied_abs = 0.0;
  g_target_abs = 0.0;
  g_dir = +1;
  g_last_fault = CC_FAULT_NONE;
  current_sensor_set_output_active(0);
  pc_print("SAFE BOOT: buck OFF\r\n");
}

void coil_ctrl_task(void)
{
  check_protections();

  if (g_state == CC_IDLE || g_state == CC_RUNNING)
  {
    return;
  }

  if ((HAL_GetTick() - g_t0) < g_wait_ms)
  {
    return;
  }
  g_t0 = HAL_GetTick();

  switch (g_state)
  {
    case CC_ON_SEQ:
      if (g_step < ON_NUM_STEPS)
      {
        run_on_step(g_step);
        g_wait_ms = k_on_delay_ms[g_step];
        g_step++;
      }
      else
      {
        enter_state(CC_RAMP);
      }
      break;

    case CC_RAMP:
      ramp_step();
      break;

    case CC_OFF_SEQ:
      if (g_step < OFF_NUM_STEPS)
      {
        run_off_step(g_step);
        g_wait_ms = k_off_delay_ms[g_step];
        g_step++;
      }
      else
      {
        g_state = CC_IDLE;
        current_sensor_set_output_active(0);
      }
      break;

    default:
      break;
  }
}

void coil_ctrl_request(int dir, double i_abs)
{
  dir = (dir < 0) ? -1 : +1;

  if (i_abs < 0.0)
  {
    i_abs = -i_abs;
  }
  if (!(i_abs <= CFG_I_MAX_A))
  {
    i_abs = CFG_I_MAX_A;
  }
  if (i_abs <= 0.0)
  {
    coil_ctrl_off(CC_FAULT_NONE);
    return;
  }

  g_target_abs = i_abs;
  g_last_fault = CC_FAULT_NONE;
  current_sensor_set_output_active(1);

  switch (g_state)
  {
    case CC_ON_SEQ:
      if (dir != g_dir)
      {
        g_dir = dir;
        enter_state(CC_ON_SEQ);
      }
      /* misma direccion: la rampa ira al nuevo objetivo */
      break;

    case CC_RAMP:
    case CC_RUNNING:
      buck_current_watchdog_reset();
      if (dir != g_dir)
      {
        g_dir = dir;
        enter_state(CC_ON_SEQ);
      }
      else if (g_state == CC_RUNNING)
      {
        enter_state(CC_RAMP);
      }
      break;

    case CC_IDLE:
    case CC_OFF_SEQ:
    default:
      g_dir = dir;
      buck_current_watchdog_reset();
      enter_state(CC_ON_SEQ);
      break;
  }
}

void coil_ctrl_off(cc_fault_t reason)
{
  buck_current_watchdog_reset();
  g_target_abs = 0.0;
  if (reason != CC_FAULT_NONE)
  {
    g_last_fault = reason;
  }

  if (g_state != CC_OFF_SEQ)
  {
    enter_state(CC_OFF_SEQ);
  }
}

uint8_t coil_ctrl_is_on(void)
{
  return (uint8_t)((g_state == CC_ON_SEQ) || (g_state == CC_RAMP) || (g_state == CC_RUNNING));
}

double coil_ctrl_get_applied_abs(void)
{
  return g_applied_abs;
}

double coil_ctrl_get_target_abs(void)
{
  return g_target_abs;
}

int coil_ctrl_get_dir(void)
{
  return g_dir;
}

const char *coil_ctrl_state_name(void)
{
  switch (g_state)
  {
    case CC_IDLE:    return "OFF";
    case CC_ON_SEQ:  return "STARTING";
    case CC_RAMP:    return "RAMP";
    case CC_RUNNING: return "ON";
    case CC_OFF_SEQ: return "STOPPING";
    default:         return "?";
  }
}

const char *coil_ctrl_fault_name(void)
{
  switch (g_last_fault)
  {
    case CC_FAULT_NONE:         return "NONE";
    case CC_FAULT_WDG_MISMATCH: return "WDG_MISMATCH";
    case CC_FAULT_OVERCURRENT:  return "OVERCURRENT";
    case CC_FAULT_HB_TIMEOUT:   return "HB_TIMEOUT";
    default:                    return "?";
  }
}
