#include "buck.h"

#include "uart_app.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static UART_HandleTypeDef *g_buck_uart = NULL;
static uint8_t g_buck_addr = 1u;

void buck_init(UART_HandleTypeDef *huart, uint8_t address)
{
  g_buck_uart = huart;
  g_buck_addr = address;
}

HAL_StatusTypeDef buck_send(const char *cmd)
{
  if ((g_buck_uart == NULL) || (cmd == NULL))
  {
    return HAL_ERROR;
  }

  return HAL_UART_Transmit(g_buck_uart, (uint8_t *)cmd, (uint16_t)strlen(cmd), 500);
}

int buck_readline(char *out, size_t maxlen, uint32_t timeout_ms)
{
  uint32_t t0;
  size_t i = 0;
  uint8_t b = 0;

  if ((g_buck_uart == NULL) || (out == NULL) || (maxlen == 0u))
  {
    return -1;
  }

  out[0] = '\0';
  t0 = HAL_GetTick();

  while (((HAL_GetTick() - t0) < timeout_ms) && (i < (maxlen - 1u)))
  {
    if (HAL_UART_Receive(g_buck_uart, &b, 1, 10) == HAL_OK)
    {
      out[i++] = (char)b;
      if (b == '\n')
      {
        break;
      }
    }
  }

  out[i] = '\0';
  return (int)i;
}

int parse_int_after_equal(const char *resp, int *out_val)
{
  const char *eq;
  int v = 0;
  int any = 0;

  if ((resp == NULL) || (out_val == NULL))
  {
    return 0;
  }

  eq = strchr(resp, '=');
  if (eq == NULL)
  {
    return 0;
  }

  eq++;

  while ((*eq != '\0') && isdigit((unsigned char)*eq))
  {
    any = 1;
    v = (v * 10) + (*eq - '0');
    eq++;
  }

  if (!any)
  {
    return 0;
  }

  *out_val = v;
  return 1;
}

void buck_set_voltage_abs(double v_abs)
{
  char cmd[40];
  int v100;

  if (g_buck_uart == NULL)
  {
    return;
  }

  if (v_abs < 0.0)
  {
    v_abs = -v_abs;
  }

  if (v_abs > 60.0)
  {
    v_abs = 60.0;
  }

  v100 = (int)(v_abs * 100.0 + 0.5);
  snprintf(cmd, sizeof(cmd), ":%02uw10=%d,\r\n", (unsigned)g_buck_addr, v100);
  (void)buck_send(cmd);
}

void buck_set_current_abs(double i_abs)
{
  char cmd[40];
  int i1000;

  if (g_buck_uart == NULL)
  {
    return;
  }

  if (i_abs < 0.0)
  {
    i_abs = -i_abs;
  }

  if (i_abs > 5.0)
  {
    i_abs = 5.0;
  }

  i1000 = (int)(i_abs * 1000.0 + 0.5);
  snprintf(cmd, sizeof(cmd), ":%02uw11=%d,\r\n", (unsigned)g_buck_addr, i1000);
  (void)buck_send(cmd);
}

void buck_output(uint8_t on)
{
  char cmd[32];

  if (g_buck_uart == NULL)
  {
    return;
  }

  snprintf(cmd, sizeof(cmd), ":%02uw12=%d,\r\n", (unsigned)g_buck_addr, on ? 1 : 0);
  (void)buck_send(cmd);
}

void buck_read_vmeas_print(void)
{
  char req[32];
  char resp[80];
  int n;
  int v100 = 0;
  int v_int;
  int v_dec;

  if (g_buck_uart == NULL)
  {
    pc_print("ERR UART5\r\n");
    return;
  }

  snprintf(req, sizeof(req), ":%02ur30=0,\r\n", (unsigned)g_buck_addr);

  if (buck_send(req) != HAL_OK)
  {
    pc_print("VMEAS=ERR\r\n");
    return;
  }

  HAL_Delay(3);

  n = buck_readline(resp, sizeof(resp), 300);
  if (n <= 0)
  {
    pc_print("VMEAS=ERR\r\n");
    return;
  }

  if (!parse_int_after_equal(resp, &v100))
  {
    pc_print("VMEAS=ERR\r\n");
    return;
  }

  v_int = v100 / 100;
  v_dec = v100 % 100;
  pc_printf("VMEAS=%d.%02d\r\n", v_int, v_dec);
}
