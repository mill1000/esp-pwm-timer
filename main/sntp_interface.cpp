#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_err.h"
#include "esp_sntp.h"
#include "esp_log.h"

#include <string>

#include "sntp_interface.h"

#define TAG "SNTP"

/**
  @brief  Initalize the SNTP with the given timezone and callback
  
  @param  timezone Timezone string for TZ enviroment variable
  @param  callback sync_callback_t time sync callback funtion
  @retval none
*/
void SNTP::init(const std::string& timezone, sync_callback_t callback)
{
  static sync_callback_t s_callback = callback;

  // Configure timezome for conversion
  setenv("TZ", timezone.c_str(), 1);
  tzset();

  // Add 2 servers to the pool
  sntp_setservername(0, "0.pool.ntp.org");
  sntp_setservername(1, "1.pool.ntp.org");

  // Use polling, and immediately update time
  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
  
  // Set a callback so we know it's working
  sntp_set_time_sync_notification_cb([](struct timeval* tv)
  {
    struct tm* timeinfo = localtime(&tv->tv_sec);
    ESP_LOGI(TAG, "System time set to %s", asctime(timeinfo));

    if (s_callback != nullptr)
      s_callback();
  });

  sntp_init();
}