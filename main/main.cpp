#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include <string>

#include "main.h"
#include "wifi.h"
#include "http.h"
#include "sntp_interface.h"
#include "schedule.h"
#include "ledc_interface.h"
#include "nvs_interface.h"
#include "json.h"

#define TAG "Main"

static EventGroupHandle_t mainEventGroup;

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

  // Initalizer our own NVS interface
  NVS::init();

  // Initalize WiFi and connect to configured network
  WiFi::init_station();

  // Start the HTTP task
  xTaskCreate(httpTask, "HTTPTask", 8192, NULL, 1, NULL);

  // Create an event group to run the main loop from
  mainEventGroup = xEventGroupCreate();
  if (mainEventGroup == NULL)
    ESP_LOGE(TAG, "Failed to create main event group.");

  // Construct a timer to handle the scheduled events
  TimerHandle_t scheduleTimer = xTimerCreate("ScheduleTimer", pdMS_TO_TICKS(1000), false, (void*) Schedule::INVALID_TOD, 
    [](TimerHandle_t t) {
      signal_event(MAIN_EVENT_LED_TIMER_EXPIRED);
    });

  if (scheduleTimer == NULL)
    ESP_LOGE(TAG, "Failed to create schedule timer.");
  
  if (xTimerStart(scheduleTimer, pdMS_TO_TICKS(1000)) != pdPASS)
    ESP_LOGE(TAG, "Failed to start schedule timer.");

  // Configure NTP for MST/MDT
  SNTP::init("MST7MDT,M3.2.0,M11.1.0", [](){
    signal_event(MAIN_EVENT_SYSTEM_TIME_UPDATED);
  });

  Schedule schedule;

  // Init LED peripherals
  LEDC::init();

  // Wait for initial time sync
  xEventGroupWaitBits(mainEventGroup, MAIN_EVENT_SYSTEM_TIME_UPDATED, pdTRUE, pdFALSE, portMAX_DELAY);

  while (true)
  {
    EventBits_t events = xEventGroupWaitBits(mainEventGroup, MAIN_EVENT_ALL, pdTRUE, pdFALSE, portMAX_DELAY);

    if (events & MAIN_EVENT_CONFIG_UPDATE)
    {
      LEDC::reconfigure();
    }

    if (events & MAIN_EVENT_SCHEDULE_UPDATE)
    {
      schedule.reset();

      std::map<std::string, std::string> scheduleJson = NVS::get_schedule_json();

      ESP_LOGI(TAG, "New schedule.");
      for (auto& kv : scheduleJson)
      {
        Schedule::time_of_day_t tod = Schedule::get_time_of_day(kv.first);
        Schedule::entry_t entry = JSON::parse_schedule_entry(kv.second);

        schedule.set(tod, entry);
        ESP_LOGI(TAG, "TOD '%s' (%ld).", kv.first.c_str(), tod);
        for (auto& ci : entry)
          ESP_LOGI(TAG, "\tChannel %d Intensity: %g", ci.first, ci.second);
      }

      Schedule::time_of_day_t prev = schedule.prev(Schedule::get_time_of_day(time(NULL)));

      // Reset event ID to previous and trigger timer update
      vTimerSetTimerID(scheduleTimer, (void*) prev);
      xEventGroupSetBits(mainEventGroup, MAIN_EVENT_LED_TIMER_EXPIRED);
    }

    if (events & MAIN_EVENT_SYSTEM_TIME_UPDATED)
    {
      // Calculate time of day, and delta to next scheduled event
      Schedule::time_of_day_t tod = Schedule::get_time_of_day(time(NULL));
      Schedule::time_of_day_t delta = Schedule::delta(schedule.next(tod), tod);

      // If invalid TOD don't do anything
      if (delta == Schedule::INVALID_TOD)
        continue;

      // Calculate the error between the timer's expiration and expected time delta
      TickType_t remainingTicks = xTimerGetExpiryTime(scheduleTimer) - xTaskGetTickCount();
      double error = delta - (remainingTicks / (double) pdMS_TO_TICKS(1000));

      ESP_LOGD(TAG, "System time updated. Timer error: %f", error);

      if (abs(error) > Schedule::MAX_SCHEDULE_ERROR)
      {
        ESP_LOGW(TAG, "Timer delta and actual delta differ by more than %d seconds. Updating timer.", Schedule::MAX_SCHEDULE_ERROR);

        // Reset event ID and trigger timer update
        vTimerSetTimerID(scheduleTimer, (void*) Schedule::INVALID_TOD);
        xEventGroupSetBits(mainEventGroup, MAIN_EVENT_LED_TIMER_EXPIRED);
      }
    }

    if (events & MAIN_EVENT_LED_TIMER_EXPIRED)
    {
      // Calculate time of day, next TOD and delta to next scheduled event
      Schedule::time_of_day_t tod = Schedule::get_time_of_day(time(NULL));
      Schedule::time_of_day_t next = schedule.next(tod);

      // If invalid TOD don't do anything
      if (next == Schedule::INVALID_TOD)
      {
        ESP_LOGW(TAG, "Invalid TOD. Empty schedule?");
        continue;
      }

      Schedule::time_of_day_t delta = Schedule::delta(next, tod);

      ESP_LOGI(TAG, "Timer update. TOD: %ld, Next: %ld, Delta: %ld", tod, next, delta);

      // Reset timer for next schedule event
      if (xTimerChangePeriod(scheduleTimer, pdMS_TO_TICKS(delta * 1000), pdMS_TO_TICKS(1000)) != pdPASS)
        ESP_LOGE(TAG, "Failed to update schedule timer for next event.");

      // Pull the expected TOD from the timer ID
      Schedule::time_of_day_t expectedTOD = (Schedule::time_of_day_t) pvTimerGetTimerID(scheduleTimer);

      // Save the "event ID" of the next event
      vTimerSetTimerID(scheduleTimer, (void*) next);

      if (abs(tod - expectedTOD) > Schedule::MAX_SCHEDULE_ERROR && expectedTOD != Schedule::INVALID_TOD) // Ignore init conditions
        ESP_LOGW(TAG, "Expected TOD and actual TOD differ by more than %d seconds.", Schedule::MAX_SCHEDULE_ERROR);

      // Execute state changes for TOD
      for (auto pair : schedule[expectedTOD])
      {
        ESP_LOGI(TAG, "Setting channel %d to %g", pair.first, pair.second);
        LEDC::set_intensity((ledc_channel_t) pair.first, pair.second, 0);
      }
    }
  }
}

/**
  @brief  Signal an event to the main loop

  @param  event MAIN_EVENT to signal
  @retval none
*/
void signal_event(MAIN_EVENT event)
{
  if (mainEventGroup != NULL)
    xEventGroupSetBits(mainEventGroup, event);
}