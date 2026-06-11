#ifndef CURRENT_SENSOR_H
#define CURRENT_SENSOR_H

#include "main.h"

void current_sensor_init(void);
void current_sensor_task(void);
float current_sensor_get_current_mA(void);
uint8_t current_sensor_is_ready(void);

#endif /* CURRENT_SENSOR_H */
