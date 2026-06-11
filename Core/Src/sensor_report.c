#include "sensor_report.h"

#include "buck.h"
#include "current_sensor.h"
#include "uart_app.h"

#define REPORT_INTERVAL_MS  1000u

static uint32_t g_last_report_ms = 0;

void sensor_report_init(void)
{
  g_last_report_ms = HAL_GetTick();
}

void sensor_report_task(void)
{
  uint32_t now = HAL_GetTick();

  if ((now - g_last_report_ms) < REPORT_INTERVAL_MS)
  {
    return;
  }
  g_last_report_ms = now;

  /* Collect intensity from INA219 current sensor */
  float intensity = current_sensor_get_current_mA();
  int i_centi = (int)(intensity * 100.0f + ((intensity >= 0.0f) ? 0.5f : -0.5f));
  int i_sign  = 1;
  if (i_centi < 0) { i_sign = -1; i_centi = -i_centi; }

  /* Read current measured by the buck itself */
  float buck_mA = 0.0f;
  int   b_centi = 0;
  int   b_sign  = 1;
  if (buck_read_imeas_mA(&buck_mA))
  {
    b_centi = (int)(buck_mA * 100.0f + ((buck_mA >= 0.0f) ? 0.5f : -0.5f));
    if (b_centi < 0) { b_sign = -1; b_centi = -b_centi; }
  }

  pc_printf("IMEAS: INA219=%s%d.%02dmA  BUCK=%s%d.%02dmA\r\n",
            (i_sign < 0) ? "-" : "", i_centi / 100, i_centi % 100,
            (b_sign < 0) ? "-" : "", b_centi / 100, b_centi % 100);

  /* TODO: replace stubs with real sensor reads when available */
  int temp_centi   = 0;   /* temp_sensor_get_temp_C() * 100 */
  int mTx_centi    = 0;   /* mag_sensor_get_mT_x()   * 100 */
  int mTy_centi    = 0;   /* mag_sensor_get_mT_y()   * 100 */
  int mTz_centi    = 0;   /* mag_sensor_get_mT_z()   * 100 */

  pc_printf("{\"temp\":%d.%02d,\"intensity\":%s%d.%02d,\"mT_x\":%d.%02d,\"mT_y\":%d.%02d,\"mT_z\":%d.%02d}\r\n",
            temp_centi / 100, temp_centi % 100,
            (i_sign < 0) ? "-" : "", i_centi / 100, i_centi % 100,
            mTx_centi / 100, mTx_centi % 100,
            mTy_centi / 100, mTy_centi % 100,
            mTz_centi / 100, mTz_centi % 100);
}
