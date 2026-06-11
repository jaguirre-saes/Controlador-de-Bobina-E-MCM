#ifndef UART_APP_H
#define UART_APP_H

#include "main.h"

void uart_app_set_pc_uart(UART_HandleTypeDef *huart);
void pc_print(const char *s);
void pc_printf(const char *fmt, ...);

#endif /* UART_APP_H */
