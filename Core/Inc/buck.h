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
int  buck_read_vmeas_cV(int *out_v100);   /* tension de salida en centivoltios; 1 = OK */
int  buck_read_imeas_mA(float *out_mA);

/* Watchdog: compara la corriente del INA219 con el setpoint aplicado.
 * Si el error supera CFG_WDG_ERROR_PCT durante CFG_WDG_TRIP_COUNT muestras
 * consecutivas, apaga el buck. Volver a armar cada vez que cambie el setpoint. */
void buck_current_watchdog_task(void);
void buck_current_watchdog_arm(float setpoint_mA);  /* al encender / cambiar I */
void buck_current_watchdog_reset(void);             /* al apagar             */
uint8_t buck_current_watchdog_is_tripped(void);


#endif /* BUCK_H */
