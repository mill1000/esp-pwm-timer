#include "esp_log.h"

#include "json.h"
#include "schedule.h"
#include "nvs_interface.h"
#include "nlohmann/json.hpp"

#define TAG "JSON"

bool JSON::parse_settings(const std::string& jString)
{
  nlohmann::json root = nlohmann::json::parse(jString, nullptr, false);
  if (root.is_discarded())
  {
    ESP_LOGE(TAG, "Invalid JSON received: %s", jString.c_str());
    return false;
  }

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

  if (root.contains("schedule"))
  {
    nlohmann::json schedule = root.at("schedule");
    
    ESP_LOGI(TAG, "Schedule: %s", schedule.dump().c_str());

    for (auto& entry : schedule.items())
    {

    }
  }

  return true;
}
