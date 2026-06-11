#ifndef BUCK_H
#define BUCK_H

#include "main.h"

void buck_init(UART_HandleTypeDef *huart, uint8_t address);
HAL_StatusTypeDef buck_send(const char *cmd);
int buck_readline(char *out, size_t maxlen, uint32_t timeout_ms);
int parse_int_after_equal(const char *resp, int *out_val);
void buck_set_voltage_abs(double v_abs);
void buck_set_current_abs(double i_abs);
void buck_output(uint8_t on);
void buck_read_vmeas_print(void);
int  buck_read_imeas_mA(float *out_mA);

/* Watchdog: compara INA219 vs medicion interna del buck.
 * Si el error supera 10% durante 5 muestras consecutivas, apaga el buck.
 * Llamar buck_current_watchdog_reset() al cambiar el setpoint o encender. */
void buck_current_watchdog_task(void);
void buck_current_watchdog_reset(void);


#endif /* BUCK_H */
