#ifndef RS485_H
#define RS485_H

#include "main.h"

void rs485_init(UART_HandleTypeDef *huart);
void rs485_poll(void);
void rs485_uart_rx_cplt_callback(UART_HandleTypeDef *huart);
void rs485_uart_error_callback(UART_HandleTypeDef *huart);
double rs485_get_iapplied_abs(void);

#endif /* RS485_H */
