#include "hbridge.h"

static GPIO_TypeDef *g_en_port = NULL;
static uint16_t g_en_pin = 0;
static GPIO_TypeDef *g_dir_port = NULL;
static uint16_t g_dir_pin = 0;

void hbridge_init(GPIO_TypeDef *en_port, uint16_t en_pin, GPIO_TypeDef *dir_port, uint16_t dir_pin)
{
  g_en_port = en_port;
  g_en_pin = en_pin;
  g_dir_port = dir_port;
  g_dir_pin = dir_pin;
}

void coil_enable(uint8_t on)
{
  if (g_en_port == NULL)
  {
    return;
  }

  HAL_GPIO_WritePin(g_en_port, g_en_pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void coil_set_dir(int dir)
{
  if (g_dir_port == NULL)
  {
    return;
  }

  HAL_GPIO_WritePin(g_dir_port, g_dir_pin, (dir < 0) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
