#ifndef COIL_CTRL_H
#define COIL_CTRL_H

#include "main.h"

/* Control no bloqueante de la salida (buck + puente H).
 * Las peticiones se aceptan al instante y coil_ctrl_task() ejecuta la
 * secuencia de encendido/rampa/apagado sin HAL_Delay. */

typedef enum
{
  CC_FAULT_NONE = 0,
  CC_FAULT_WDG_MISMATCH,
  CC_FAULT_OVERCURRENT,
  CC_FAULT_HB_TIMEOUT
} cc_fault_t;

void coil_ctrl_init(void);   /* apaga buck y puente H (arranque seguro) */
void coil_ctrl_task(void);

void coil_ctrl_request(int dir, double i_abs);
void coil_ctrl_off(cc_fault_t reason);

uint8_t coil_ctrl_is_on(void);           /* encendido o encendiendose */
double coil_ctrl_get_applied_abs(void);  /* setpoint enviado al buck (A) */
double coil_ctrl_get_target_abs(void);   /* setpoint objetivo (A)        */
int coil_ctrl_get_dir(void);
const char *coil_ctrl_state_name(void);
const char *coil_ctrl_fault_name(void);

#endif /* COIL_CTRL_H */
