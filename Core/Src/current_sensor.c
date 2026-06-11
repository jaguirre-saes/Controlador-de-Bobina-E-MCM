#include "current_sensor.h"

#include "i2c_app.h"
#include "uart_app.h"

/* Default profile: INA219 on address 0x40. */
#define SENSOR_I2C_ADDR_7B       0x40u
#define INA219_REG_CONFIG         0x00u
#define INA219_REG_CALIBRATION    0x05u
#define INA219_REG_CURRENT        0x04u

#define SENSOR_SAMPLE_MS          100u

/* Shunt measured: 18.8 mOhm. Cal = 0.04096 / (0.0001 A * 0.0188 Ohm) = 21787. */
#define INA219_CAL_VALUE          21787u
#define INA219_CURRENT_LSB_mA     0.1f
/* Empirical trim from bench: 1.000 A real / 0.310 A reported = 3.225806. */
#define INA219_GAIN_CORRECTION    3.225806f

static uint8_t g_sensor_ready = 0;
static uint32_t g_last_sample_ms = 0;
static float g_current_mA = 0.0f;
static uint8_t g_last_sample_ok = 0;

static uint8_t current_sensor_read_once(float *out_mA)
{
  uint16_t raw;
  int16_t signed_raw;

  if (i2c_app_mem_read16(SENSOR_I2C_ADDR_7B, INA219_REG_CURRENT, &raw) != HAL_OK)
  {
    return 0;
  }

  signed_raw = (int16_t)raw;
  *out_mA = ((float)signed_raw * INA219_CURRENT_LSB_mA) * INA219_GAIN_CORRECTION;
  return 1;
}

void current_sensor_init(void)
{
  uint16_t cfg;

  g_sensor_ready = 0;

  if (i2c_app_mem_write16(SENSOR_I2C_ADDR_7B, INA219_REG_CALIBRATION, INA219_CAL_VALUE) != HAL_OK)
  {
    pc_print("SENSOR ERR: calibration\r\n");
    return;
  }

  if (i2c_app_mem_read16(SENSOR_I2C_ADDR_7B, INA219_REG_CONFIG, &cfg) != HAL_OK)
  {
    pc_print("SENSOR ERR: no response\r\n");
    return;
  }

  g_sensor_ready = 1;
  g_last_sample_ok = 0;
  g_last_sample_ms = HAL_GetTick();
  pc_printf("SENSOR OK: INA219 CFG=0x%04X\r\n", cfg);
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

  if ((now - g_last_sample_ms) >= SENSOR_SAMPLE_MS)
  {
    g_last_sample_ms = now;
    if (current_sensor_read_once(&sample))
    {
      g_current_mA = sample;
      g_last_sample_ok = 1;
    }
    else
    {
      g_last_sample_ok = 0;
    }
  }
}

float current_sensor_get_current_mA(void)
{
  return g_current_mA;
}

uint8_t current_sensor_is_ready(void)
{
  return g_sensor_ready;
}
