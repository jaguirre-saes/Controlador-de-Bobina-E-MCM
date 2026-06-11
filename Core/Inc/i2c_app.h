#ifndef I2C_APP_H
#define I2C_APP_H

#include "main.h"

void i2c_app_init(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef i2c_app_mem_read16(uint16_t dev_addr_7b, uint8_t reg_addr, uint16_t *value);
HAL_StatusTypeDef i2c_app_mem_write16(uint16_t dev_addr_7b, uint8_t reg_addr, uint16_t value);

#endif /* I2C_APP_H */
