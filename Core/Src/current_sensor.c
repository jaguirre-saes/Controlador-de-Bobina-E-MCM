#include "current_sensor.h"

#include "app_config.h"
#include "i2c_app.h"
#include "uart_app.h"

#define INA219_REG_CONFIG         0x00u
#define INA219_REG_SHUNT_V        0x01u
#define INA219_REG_BUS_V          0x02u
#define INA219_REG_CURRENT        0x04u
#define INA219_REG_CALIBRATION    0x05u

static uint8_t g_sensor_ready = 0;
static uint32_t g_last_sample_ms = 0;
static float g_current_mA = 0.0f;
static float g_calibrated_mA = 0.0f;   /* valor calibrado sin recortar (diagnostico) */
static uint8_t g_last_sample_ok = 0;
static uint8_t g_output_active = 0u;

static uint8_t current_sensor_read_once(float *out_mA)
{
  uint16_t raw;

  if (i2c_app_mem_read16(CFG_INA219_ADDR_7B, INA219_REG_CURRENT, &raw) != HAL_OK)
  {
    return 0;
  }

  *out_mA = ((float)(int16_t)raw * CFG_INA219_CURRENT_LSB_mA) * CFG_INA219_GAIN + CFG_INA219_OFFSET_mA;
  return 1;
}

static float apply_output_filters(float i_mA)
{
  if ((i_mA > -CFG_INA219_DEADBAND_mA) && (i_mA < CFG_INA219_DEADBAND_mA))
  {
    return 0.0f;
  }

  /* Con la salida apagada el buck no puede entregar corriente negativa:
   * el valor negativo es el transitorio tras el apagado. Se reporta 0.
   * Las lecturas POSITIVAS con salida OFF si se muestran (serian anomalas). */
  if (!g_output_active && (i_mA < 0.0f))
  {
    return 0.0f;
  }

  return i_mA;
}

void current_sensor_init(void)
{
  uint16_t cfg;

  g_sensor_ready = 0;

  if (i2c_app_mem_write16(CFG_INA219_ADDR_7B, INA219_REG_CALIBRATION, CFG_INA219_CAL_VALUE) != HAL_OK)
  {
    pc_print("SENSOR ERR: calibration\r\n");
    return;
  }

  if (i2c_app_mem_read16(CFG_INA219_ADDR_7B, INA219_REG_CONFIG, &cfg) != HAL_OK)
  {
    pc_print("SENSOR ERR: no response\r\n");
    return;
  }

  g_sensor_ready = 1;
  g_last_sample_ok = 0;
  g_last_sample_ms = HAL_GetTick();
  pc_printf("SENSOR OK: INA219 CFG=0x%04X\r\n", cfg);
}

void current_sensor_set_output_active(uint8_t active)
{
  g_output_active = active ? 1u : 0u;
}

void current_sensor_task(void)
{
  uint32_t now;
  float sample;

  if (!g_sensor_ready)
  {
    return;
  }

  now = HAL_GetTick();

  if ((now - g_last_sample_ms) >= CFG_INA219_SAMPLE_MS)
  {
    g_last_sample_ms = now;
    if (current_sensor_read_once(&sample))
    {
      g_calibrated_mA = sample;
      g_current_mA = apply_output_filters(sample);
      g_last_sample_ok = 1;
    }
    else
    {
      g_last_sample_ok = 0;
    }
  }
}

void current_sensor_print_debug(void)
{
  uint16_t vsh;
  uint16_t vbus;
  int vsh_uV;
  int vbus_mV;

  if (!g_sensor_ready)
  {
    pc_print("INA219: NOT READY\r\n");
    return;
  }

  if ((i2c_app_mem_read16(CFG_INA219_ADDR_7B, INA219_REG_SHUNT_V, &vsh) != HAL_OK) ||
      (i2c_app_mem_read16(CFG_INA219_ADDR_7B, INA219_REG_BUS_V, &vbus) != HAL_OK))
  {
    pc_print("INA219: I2C ERR\r\n");
    return;
  }

  vsh_uV  = (int)(int16_t)vsh * 10;   /* LSB 10 uV */
  vbus_mV = (int)(vbus >> 3) * 4;     /* LSB 4 mV  */

  pc_printf("INA219: VSHUNT=%duV VBUS=%dmV SIN_FILTRO=%dmA\r\n",
            vsh_uV, vbus_mV, (int)g_calibrated_mA);
}

float current_sensor_get_current_mA(void)
{
  return g_current_mA;
}

uint8_t current_sensor_is_ready(void)
{
  return g_sensor_ready;
}

uint8_t current_sensor_has_valid_sample(void)
{
  return (uint8_t)(g_sensor_ready && g_last_sample_ok);
}
