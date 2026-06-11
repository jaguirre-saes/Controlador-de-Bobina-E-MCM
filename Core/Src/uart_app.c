#include "uart_app.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static UART_HandleTypeDef *g_pc_uart = NULL;

void uart_app_set_pc_uart(UART_HandleTypeDef *huart)
{
  g_pc_uart = huart;
}

void pc_print(const char *s)
{
  if ((g_pc_uart == NULL) || (s == NULL))
  {
    return;
  }

  HAL_UART_Transmit(g_pc_uart, (uint8_t *)s, (uint16_t)strlen(s), 500);
}

void pc_printf(const char *fmt, ...)
{
  char buf[160];
  va_list ap;
  int n;

  if ((g_pc_uart == NULL) || (fmt == NULL))
  {
    return;
  }

  va_start(ap, fmt);
  n = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  if (n < 0)
  {
    return;
  }

  if (n >= (int)sizeof(buf))
  {
    n = (int)sizeof(buf) - 1;
  }

  HAL_UART_Transmit(g_pc_uart, (uint8_t *)buf, (uint16_t)n, 500);
}
