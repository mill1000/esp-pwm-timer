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
  @brief  Initialize the SNTP with the given timezone and callback
  
  @param  timezone Timezone string for TZ environment variable
  @param  servers Array of server hostname & address to use
  @param  callback sync_callback_t time sync callback function
  @retval none
*/
void SNTP::init(const std::string& timezone, const server_list_t& servers, sync_callback_t callback)
{
  static sync_callback_t s_callback = callback;

  // Configure timezone and servers
  reconfigure(timezone, servers);

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

/**
  @brief  Reconfigure the SNTP system
  
  @param  timezone Timezone string for TZ environment variable
  @param  servers Array of server hostname & address to use
  @retval none
*/
void SNTP::reconfigure(const std::string& timezone, const server_list_t& servers)
{
  // Configure timezone for conversion
  setenv("TZ", timezone.c_str(), 1);
  tzset();

  ESP_LOGI(TAG, "TZ: '%s'", timezone.c_str());

  // Add servers to the list
  uint8_t index = 0;
  for (auto& server : servers)
  {
    sntp_setservername(index, server.c_str());
    ESP_LOGI(TAG, "Server %d: '%s'", index, server.c_str());
    index++;
  }
}