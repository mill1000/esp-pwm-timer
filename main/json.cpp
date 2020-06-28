#include "esp_log.h"

#include "json.h"
#include "schedule.h"
#include "nvs_interface.h"
#include "main.h"
#include "nlohmann/json.hpp"

#define TAG "JSON"

/**
  @brief  Parse the entire JSON string received from the web interface
  
  @param  jString JSON string received from web interface
  @retval bool - JSON was valid and processed
*/
bool JSON::parse_settings(const std::string& jString)
{
  nlohmann::json root = nlohmann::json::parse(jString, nullptr, false);
  if (root.is_discarded())
  {
    ESP_LOGE(TAG, "Invalid JSON received: %s", jString.c_str());
    return false;
  }

  // Parse timers object
  if (root.contains("timers"))
  {
    nlohmann::json timers = root.at("timers");
   
    ESP_LOGI(TAG, "Timers: %s", timers.dump().c_str());
   
    for (auto& kv : timers.items())
    {
      nlohmann::json timer = kv.value();

      // Make sure IDs match!
      assert(std::stoi(kv.key()) == timer["id"].get<uint32_t>());
      
      timer_config_t config;
      config.id = timer["id"].get<ledc_timer_t>();
      config.frequency_Hz = timer["freq"].get<uint32_t>();

      NVS::save_timer_config(config);
    }
  }

  // Parse channels object
  if (root.contains("channels"))
  {
    nlohmann::json channels = root.at("channels");
    
    ESP_LOGI(TAG, "Channels: %s", channels.dump().c_str());

    for (auto& kv : channels.items())
    {
      nlohmann::json channel = kv.value();

      // Make sure IDs match!
      assert(std::stoi(kv.key()) == channel["id"].get<uint32_t>());
      
      channel_config_t config;
      config.id = channel["id"].get<ledc_channel_t>();
      config.enabled = channel["enabled"].get<bool>();
      config.timer = JSON::get_or_default<ledc_timer_t>(channel, "timer", LEDC_TIMER_MAX);
      config.gpio = JSON::get_or_default<gpio_num_t>(channel, "gpio", GPIO_NUM_NC);
      //config.name = channel["name"].get<std::string>();

      NVS::save_channel_config(config);
    }
  }

  // If timers or channels object was present we want to notify of config change
  if (root.contains("timers") || root.contains("channels"))
    signal_event(MAIN_EVENT_CONFIG_UPDATE);

  // Parse schedule object
  if (root.contains("schedule"))
  {
    nlohmann::json schedule = root.at("schedule");
    
    ESP_LOGI(TAG, "Schedule: %s", schedule.dump().c_str());

    NVS::erase_schedule();

    for (auto& kv : schedule.items())
      NVS::save_schedule_entry_json(kv.key(), kv.value().dump());

    NVS::commit_schedule();

    signal_event(MAIN_EVENT_SCHEDULE_UPDATE);
  }

  return true;
}

/**
  @brief  Parse JSON representing a single entry of the schedule into a C++ type
  
  @param  jEntry JSON string represenation of entry
  @retval Schedule::entry_t
*/
Schedule::entry_t JSON::parse_schedule_entry(const std::string& jEntry)
{
  nlohmann::json root = nlohmann::json::parse(jEntry, nullptr, false);
  if (root.is_discarded())
  {
    ESP_LOGE(TAG, "Invalid schedule entry JSON '%s'", jEntry.c_str());

    Schedule::entry_t empty;
    return empty;
  }

  Schedule::entry_t entry;

  for (auto& kv : root.items())
  {
    // Skip the duplicate TOD entry within the object
    if (kv.key() == "tod")
      continue;

    ledc_channel_t channel = (ledc_channel_t) std::stoi(kv.key());
    entry[channel] = kv.value().get<float>();
  }

  return entry;
}