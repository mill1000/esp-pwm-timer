#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "esp_sntp.h"
#include "esp_log.h"

#include <string>

#include "wifi.h"
#define TAG "Main"

typedef enum
{
  MAIN_EVENT_SYSTEM_TIME_UPDATED = 1 << 0,
  MAIN_EVENT_ALL                 = 0x00FFFFFF, // 24 bits max
} MAIN_EVENT;

static EventGroupHandle_t mainEventGroup;

static void sntpInit(const std::string& timezone)
{
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

    if (mainEventGroup != NULL)
      xEventGroupSetBits(mainEventGroup, MAIN_EVENT_SYSTEM_TIME_UPDATED);
  });

  sntp_init();
}

extern "C" void app_main()
{
  // Initalize NVS
  esp_err_t result = nvs_flash_init();
  if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    result = nvs_flash_init();
  }
  ESP_ERROR_CHECK(result);

  // Initalize WiFi and connect to configured network
  wifiInitStation();

  // Create an event group to run the main loop from
  mainEventGroup = xEventGroupCreate();
  if (mainEventGroup == NULL)
    ESP_LOGE(TAG, "Failed to create main event group.");

  // Configure NTP for MST/MDT
  sntpInit("MST7MDT,M3.2.0,M11.1.0");

  // Wait for initial time sync
  xEventGroupWaitBits(mainEventGroup, MAIN_EVENT_SYSTEM_TIME_UPDATED, pdTRUE, pdFALSE, portMAX_DELAY);

  while (true)
  {
    EventBits_t events = xEventGroupWaitBits(mainEventGroup, MAIN_EVENT_ALL, pdTRUE, pdFALSE, portMAX_DELAY);

    if (events & MAIN_EVENT_SYSTEM_TIME_UPDATED)
    {
    }
  }
}
