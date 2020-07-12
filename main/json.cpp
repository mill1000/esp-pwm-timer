#include "esp_log.h"

#include "json.h"
#include "schedule.h"
#include "nvs_interface.h"
#include "main.h"
#include "nlohmann/json.hpp"

#define TAG "JSON"

/**
  @brief  Parse the provided JSON object for timer configurations
  
  @param  timers JSON object for timers
  @retval none
*/
static void parse_timer_json(const nlohmann::json& timers)
{
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

/**
  @brief  Parse the provided JSON object for channel configurations
  
  @param  timers JSON object for channels
  @retval none
*/
static void parse_channel_json(const nlohmann::json& channels)
{
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

/**
  @brief  Parse the provided JSON object for schedule data
  
  @param  timers JSON object for schedule
  @retval none
*/
static void parse_schedule_json(const nlohmann::json& schedule)
{
  ESP_LOGD(TAG, "Schedule: %s", schedule.dump().c_str());

  NVS::erase_schedule();

  for (auto& kv : schedule.items())
    NVS::save_schedule_entry_json(kv.key(), kv.value().dump());

  NVS::commit_schedule();

  signal_event(MAIN_EVENT_SCHEDULE_UPDATE);
}

/**
  @brief  Parse the provided JSON object for system settings
  
  @param  timers JSON object for system
  @retval none
*/
static void parse_system_json(const nlohmann::json& system)
{ 
  ESP_LOGD(TAG, "System: %s", system.dump().c_str());
  
  std::string hostname = JSON::get_or_default<std::string>(system, "hostname");
  NVS::save_hostname(hostname);

  if (system.contains("ntp_servers"))
  {
    nlohmann::json ntp_servers = system.at("ntp_servers");

    for (uint8_t i = 0; i < CONFIG_LWIP_DHCP_MAX_NTP_SERVERS; i++)
    {
      std::string server = JSON::get_or_default<std::string>(ntp_servers, i);
      NVS::save_ntp_server(i, server);
    }
  }
}

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
    parse_timer_json(root.at("timers"));

  // Parse channels object
  if (root.contains("channels"))
    parse_channel_json(root.at("channels"));

  // If timers or channels object was present we want to notify of config change
  if (root.contains("timers") || root.contains("channels"))
    signal_event(MAIN_EVENT_CONFIG_UPDATE);

  // Parse schedule object
  if (root.contains("schedule"))
    parse_schedule_json(root.at("schedule"));

  if (root.contains("system"))
    parse_system_json(root.at("system"));

  return true;
}

/**
  @brief  Build JSON object containing timer configurations
  
  @param  none
  @retval nlohmann::json
*/
static nlohmann::json get_timer_json()
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

  return timers;
}

/**
  @brief  Build JSON object containing channel configurations
  
  @param  none
  @retval nlohmann::json
*/
static nlohmann::json get_channel_json()
{
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

  return channels;
}

/**
  @brief  Build JSON object containing schedule data
  
  @param  none
  @retval nlohmann::json
*/
static nlohmann::json get_schedule_json()
{
  // Create & populate schedule object
  nlohmann::json schedule = nlohmann::json::object();

  std::map<std::string, std::string> scheduleJson = NVS::get_schedule_json();
  for (auto& kv : scheduleJson)
    schedule[kv.first] = nlohmann::json::parse(kv.second);

  return schedule;
}

/**
  @brief  Build JSON object containing system configuration
  
  @param  none
  @retval nlohmann::json
*/
static nlohmann::json get_system_json()
{
  // Create & populate system object
  nlohmann::json system = nlohmann::json::object();

  JSON::set_if_valid<std::string>(system, "hostname", NVS::get_hostname(), [](const std::string& s) { return !s.empty(); });

  system["ntp_servers"] = 
  {
    NVS::get_ntp_server(0),
    NVS::get_ntp_server(1),
  };

  return system;
}

/**
  @brief  Build a JSON string of the settings saved in NVS
  
  @param  none
  @retval std::string
*/
std::string JSON::get_settings()
{
  // Add all the objects to our root
  nlohmann::json root;
  root["timers"] = get_timer_json();
  root["channels"] = get_channel_json();
  root["schedule"] = get_schedule_json();
  root["system"] = get_system_json();

  return root.dump();
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