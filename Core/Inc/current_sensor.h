#ifndef CURRENT_SENSOR_H
#define CURRENT_SENSOR_H

#include "main.h"

void current_sensor_init(void);
void current_sensor_task(void);
float current_sensor_get_current_mA(void);
uint8_t current_sensor_is_ready(void);
uint8_t current_sensor_has_valid_sample(void);

/* Informa al sensor si la salida esta activa. Con la salida apagada
 * las lecturas negativas se reportan como 0. */
void current_sensor_set_output_active(uint8_t active);
void current_sensor_print_debug(void);

#endif /* CURRENT_SENSOR_H */
