#include "safety.h"

#include "app_config.h"

#define IWDG_KEY_START    0xCCCCu
#define IWDG_KEY_UNLOCK   0x5555u
#define IWDG_KEY_RELOAD   0xAAAAu
#define IWDG_PR_DIV64     4u
#define IWDG_TICK_HZ      (LSI_VALUE / 64u)
#define IWDG_RELOAD       ((CFG_IWDG_TIMEOUT_MS * IWDG_TICK_HZ) / 1000u)

#if (IWDG_RELOAD < 1u) || (IWDG_RELOAD > 0xFFFu)
#error "CFG_IWDG_TIMEOUT_MS fuera de rango (1..8000 ms)"
#endif

static uint8_t g_iwdg_reset = 0u;
static uint8_t g_iwdg_reset_read = 0u;

void safety_iwdg_start(void)
{
  uint32_t guard = 1000000u;

  /* Con el debugger parado, el IWDG se congela (no resetea en un breakpoint) */
  DBGMCU->APB4FZ1 |= DBGMCU_APB4FZ1_DBG_IWDG1;

  IWDG1->KR = IWDG_KEY_START;
  IWDG1->KR = IWDG_KEY_UNLOCK;
  IWDG1->PR = IWDG_PR_DIV64;
  IWDG1->RLR = IWDG_RELOAD;

  while (((IWDG1->SR & (IWDG_SR_PVU | IWDG_SR_RVU)) != 0u) && (guard > 0u))
  {
    guard--;
  }

  IWDG1->KR = IWDG_KEY_RELOAD;
}

void safety_iwdg_kick(void)
{
  IWDG1->KR = IWDG_KEY_RELOAD;
}

uint8_t safety_reset_was_iwdg(void)
{
  if (!g_iwdg_reset_read)
  {
    g_iwdg_reset_read = 1u;
    g_iwdg_reset = (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDG1RST) != 0u) ? 1u : 0u;
    __HAL_RCC_CLEAR_RESET_FLAGS();
  }
  return g_iwdg_reset;
}

void safety_force_coil_off(void)
{
  /* Si el reloj del GPIO aun no esta activo, el pin sigue en estado de reset
   * (entrada) y la escritura no tiene efecto, lo cual tambien es seguro. */
  CFG_COIL_EN_PORT->BSRR = (uint32_t)CFG_COIL_EN_PIN << 16u;
}
