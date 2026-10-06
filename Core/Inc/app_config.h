#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* ==========================================================================
 *  CONFIGURACION CENTRAL DEL CONTROLADOR DE BOBINA
 *  Todos los limites, tiempos y calibraciones se cambian SOLO aqui.
 * ========================================================================== */

/* --------------------------------------------------------------------------
 *  LIMITES DE SALIDA
 * -------------------------------------------------------------------------- */
#define CFG_I_MAX_A                 6.0     /* Corriente maxima aceptada (A)            */
#define CFG_I_TRIP_A                (CFG_I_MAX_A * 1.10) /* Sobrecorriente medida -> apagado inmediato (A) */
#define CFG_V_MAX_V                 60.0    /* Tension maxima que se programa al buck (V) */
#define CFG_V_LIMIT_V               CFG_V_MAX_V /* Limite de tension usado al encender   */

/* Rampa de corriente */
#define CFG_RAMP_STEP_A             0.5     /* Paso de la rampa (A)                     */
#define CFG_RAMP_STEP_DELAY_MS      500u    /* Tiempo entre pasos (ms)                  */

/* --------------------------------------------------------------------------
 *  WATCHDOG DE CONCORDANCIA DE CORRIENTE (INA219 vs setpoint)
 * -------------------------------------------------------------------------- */
#define CFG_WDG_CHECK_MS            500u    /* Periodo de comprobacion (ms)             */
#define CFG_WDG_ERROR_PCT           10      /* Error maximo permitido (%)               */
#define CFG_WDG_TRIP_COUNT          5       /* Fallos consecutivos para apagar          */
#define CFG_WDG_MIN_mA              100.0f  /* Por debajo no se compara (mA)            */

/* --------------------------------------------------------------------------
 *  WATCHDOG HARDWARE (IWDG1): reinicia el MCU si el programa se cuelga.
 *  Debe ser mayor que la operacion bloqueante mas larga (~1.5 s con UART
 *  colgada). Maximo 8000 ms.
 * -------------------------------------------------------------------------- */
#define CFG_IWDG_TIMEOUT_MS         3000u

/* --------------------------------------------------------------------------
 *  PINES DEL PUENTE H
 * -------------------------------------------------------------------------- */
#define CFG_COIL_EN_PORT            GPIOA
#define CFG_COIL_EN_PIN             GPIO_PIN_0
#define CFG_COIL_DIR_PORT           GPIOB
#define CFG_COIL_DIR_PIN            GPIO_PIN_0

/* --------------------------------------------------------------------------
 *  COMUNICACION CON EL PC (USART3)
 * -------------------------------------------------------------------------- */
#define CFG_PC_HEARTBEAT_TIMEOUT_MS 5000u   /* Sin comandos/HB -> SAFE OFF (ms)         */
#define CFG_MCU_ALIVE_INTERVAL_MS   1000u   /* Periodo del mensaje ALIVE (ms)           */
#define CFG_REPORT_INTERVAL_MS      1000u   /* Periodo del informe IMEAS/JSON (ms)      */

/* --------------------------------------------------------------------------
 *  BUCK (UART5, protocolo :AAwRR=valor,)
 * -------------------------------------------------------------------------- */
#define CFG_BUCK_ADDRESS            1u
#define CFG_BUCK_UART_TIMEOUT_MS    500u
#define CFG_BUCK_REPLY_TIMEOUT_MS   300u

/* Correccion de 2 puntos de la salida del buck (banco, R=15.1 Ohm):
 *   real = SLOPE * setpoint - OFFSET  ->  setpoint = (deseada + OFFSET) / SLOPE
 *   Por debajo de CAL_MIN_A no se corrige (el modelo no vale cerca de 0). */
#define CFG_BUCK_CAL_SLOPE          1.0184
#define CFG_BUCK_CAL_OFFSET_A       0.0648
#define CFG_BUCK_CAL_MIN_A          0.100

/* --------------------------------------------------------------------------
 *  SENSOR DE CORRIENTE INA219 (I2C2)
 * -------------------------------------------------------------------------- */
#define CFG_INA219_ADDR_7B          0x40u
#define CFG_INA219_SAMPLE_MS        100u
/* Shunt 18.8 mOhm: Cal = 0.04096 / (0.0001 A * 0.0188 Ohm) = 21787 */
#define CFG_INA219_CAL_VALUE        21787u
#define CFG_INA219_CURRENT_LSB_mA   0.1f
/* Calibracion lineal de 2 puntos vs multimetro: I_real = raw * GAIN + OFFSET */
#define CFG_INA219_GAIN             3.4241f
#define CFG_INA219_OFFSET_mA        (-65.0f)
#define CFG_INA219_DEADBAND_mA      100.0f  /* |I| menor -> se reporta 0 */
/* Con la salida OFF las lecturas negativas (transitorio de apagado) se reportan 0 */

#endif /* APP_CONFIG_H */
