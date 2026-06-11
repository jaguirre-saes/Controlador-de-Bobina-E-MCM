#include "i2c_app.h"

static I2C_HandleTypeDef *g_i2c = NULL;

void i2c_app_init(I2C_HandleTypeDef *hi2c)
{
  g_i2c = hi2c;
}

HAL_StatusTypeDef i2c_app_mem_read16(uint16_t dev_addr_7b, uint8_t reg_addr, uint16_t *value)
{
  uint8_t buf[2];
  HAL_StatusTypeDef st;

  if ((g_i2c == NULL) || (value == NULL))
  {
    return HAL_ERROR;
  }

  st = HAL_I2C_Mem_Read(g_i2c, (uint16_t)(dev_addr_7b << 1), reg_addr, I2C_MEMADD_SIZE_8BIT, buf, 2, 100);
  if (st != HAL_OK)
  {
    return st;
  }

  *value = (uint16_t)(((uint16_t)buf[0] << 8) | (uint16_t)buf[1]);
  return HAL_OK;
}

HAL_StatusTypeDef i2c_app_mem_write16(uint16_t dev_addr_7b, uint8_t reg_addr, uint16_t value)
{
  uint8_t buf[2];

  if (g_i2c == NULL)
  {
    return HAL_ERROR;
  }

  buf[0] = (uint8_t)((value >> 8) & 0xFFu);
  buf[1] = (uint8_t)(value & 0xFFu);

  return HAL_I2C_Mem_Write(g_i2c, (uint16_t)(dev_addr_7b << 1), reg_addr, I2C_MEMADD_SIZE_8BIT, buf, 2, 100);
}
