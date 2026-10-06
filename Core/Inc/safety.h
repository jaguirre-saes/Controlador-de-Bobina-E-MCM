#ifndef SAFETY_H
#define SAFETY_H

#include "main.h"

/* Watchdog hardware IWDG1 (timeout en app_config.h) */
void safety_iwdg_start(void);
void safety_iwdg_kick(void);

/* 1 si el ultimo reset lo provoco el IWDG. Llamar una vez al arrancar. */
uint8_t safety_reset_was_iwdg(void);

/* Desactiva el puente H escribiendo directamente el registro GPIO.
 * Seguro de llamar desde Error_Handler y desde los manejadores de fallo. */
void safety_force_coil_off(void);

#endif /* SAFETY_H */
