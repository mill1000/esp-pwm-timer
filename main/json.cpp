#include "esp_log.h"

#include "json.h"
#include "schedule.h"
#include "nvs_interface.h"
#include "main.h"
#include "nlohmann/json.hpp"

#define TAG "JSON"

/**
  @brief  Parse the JSON settings received from the web interface 
          and store to the NVS
  
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
   
    ESP_LOGD(TAG, "Timers: %s", timers.dump().c_str());
   
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
    
    ESP_LOGD(TAG, "Channels: %s", channels.dump().c_str());

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
      std::string name = JSON::get_or_default<std::string>(channel, "name");

      NVS::save_channel_config(name, config);
    }
  }

  // If timers or channels object was present we want to notify of config change
  if (root.contains("timers") || root.contains("channels"))
    signal_event(MAIN_EVENT_CONFIG_UPDATE);

  // Parse schedule object
  if (root.contains("schedule"))
  {
    nlohmann::json schedule = root.at("schedule");
    
    ESP_LOGD(TAG, "Schedule: %s", schedule.dump().c_str());

    NVS::erase_schedule();

    for (auto& kv : schedule.items())
      NVS::save_schedule_entry_json(kv.key(), kv.value().dump());

    NVS::commit_schedule();

    signal_event(MAIN_EVENT_SCHEDULE_UPDATE);
  }

  if (root.contains("system"))
  {
    nlohmann::json system = root.at("system");

    ESP_LOGD(TAG, "System: %s", system.dump().c_str());
    
    std::string hostname = JSON::get_or_default<std::string>(system, "hostname");
    NVS::save_hostname(hostname);

    if (system.contains("ntp_servers"))
    {
      std::vector<std::string> servers = system["ntp_servers"].get<std::vector<std::string>>();
      NVS::save_ntp_servers(servers);
    }
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

/**
  @brief  Build a JSON string of the settings saved in NVS
  
  @param  none
  @retval std::string
*/
std::string JSON::get_settings()
{
  // Create & populate timer configuration object
  nlohmann::json timers = nlohmann::json::object();
  for (uint8_t i = 0; i < LEDC_TIMER_MAX; i++)
  {
    timer_config_t config = NVS::get_timer_config(i);
    
    // This fucking JSON lib
    nlohmann::json& timer = timers[std::to_string(i)];
    timer["id"] = config.id;
    timer["freq"] = config.frequency_Hz;
  }

  // Create & populate channel object
  nlohmann::json channels = nlohmann::json::object();
  for (uint8_t i = 0; i < LEDC_CHANNEL_MAX; i++)
  {
    auto data = NVS::get_channel_config(i);

    std::string& name = data.first;
    channel_config_t config = data.second;
    
    nlohmann::json& channel = channels[std::to_string(i)];
    channel["id"] = config.id;
    channel["enabled"] = config.enabled;
    channel["timer"] = config.timer;
    
    // Special handling for GPIO and name
    JSON::set_if_valid<gpio_num_t>(channel, "gpio", config.gpio, [](gpio_num_t g) { return g != GPIO_NUM_NC; });
    JSON::set_if_valid<std::string>(channel, "name", name, [](const std::string& s) { return !s.empty(); });
  }

  // Create & populate schedule object
  nlohmann::json schedule = nlohmann::json::object();

  std::map<std::string, std::string> scheduleJson = NVS::get_schedule_json();
  for (auto& kv : scheduleJson)
    schedule[kv.first] = nlohmann::json::parse(kv.second);

  // Create & populate system object
  nlohmann::json system = nlohmann::json::object();

  JSON::set_if_valid<std::string>(system, "hostname", NVS::get_hostname(), [](const std::string& s) { return !s.empty(); });

  system["ntp_servers"] = nlohmann::json(NVS::get_ntp_servers());

  // Add all the objects to our root
  nlohmann::json root;
  root["timers"] = timers;
  root["channels"] = channels;
  root["schedule"] = schedule;
  root["system"] = system;

  return root.dump();
}