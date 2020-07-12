#include "nvs_flash.h"
#include "esp_log.h"

#include <string>
#include <vector>

#include "nvs_interface.h"
#include "nvs_parameters.h"

#define TAG "NVS"

static NvsHelper parameters(NVS::PARAMETER_NAMESPACE);
static NvsHelper schedule(NVS::SCHEDULE_NAMESPACE);

/**
  @brief  Callback function for the NVS helper to report errors 
  
  @param  name Namespace that caused the error
  @param  key The paramter key that caused the error
  @param  result The esp_error_t of the error
  @retval none
*/
static void helper_callback(const std::string& name, const std::string& key, esp_err_t result)
{
  ESP_LOGW(TAG, "NVS Error. Namespace '%s' Key '%s' Error: %s", name.c_str(), key.c_str(), esp_err_to_name(result));
}

/**
  @brief  Open the NVS namespace and initalize our parameter object
  
  @param  none
  @retval none
*/
void NVS::init()
{
  ESP_LOGI(TAG, "Initializing NVS interface.");

  // Open a namespace to hold our parameters
  if (parameters.open(&helper_callback) != ESP_OK)
  {
    ESP_LOGE(TAG, "Error opening NVS namespace '%s'.", PARAMETER_NAMESPACE);
    return;
  }

  uint8_t version = UINT8_MAX;
  if (parameters.nvs_get<uint8_t>("version", version) == ESP_ERR_NVS_NOT_FOUND)
  {
    ESP_LOGW(TAG, "Invalid NVS version in namespace '%s'. Erasing.", PARAMETER_NAMESPACE);
    reset_configuration();
  }

  // Open a second namespace just to hold the schedule
  if (schedule.open(&helper_callback) != ESP_OK)
  {
    ESP_LOGE(TAG, "Error opening NVS namespace '%s'.", SCHEDULE_NAMESPACE);
    return;
  }

  version = UINT8_MAX;
  if (schedule.nvs_get<uint8_t>("version", version) == ESP_ERR_NVS_NOT_FOUND)
  {
    ESP_LOGW(TAG, "Invalid NVS version in namespace '%s'. Erasing.", SCHEDULE_NAMESPACE);
    erase_schedule();
  }
}

/**
  @brief  Set default configuration parameters
  
  @param  none
  @retval none
*/
void NVS::reset_configuration()
{
  parameters.erase_all();
  
  for (uint8_t i = 0; i < LEDC_TIMER_MAX; i++)
    save_timer_config((timer_config_t){.id = (ledc_timer_t)i, .frequency_Hz = 500});
 
  for (uint8_t i = 0; i < LEDC_CHANNEL_MAX; i++)
    save_channel_config("", (channel_config_t){.id = (ledc_channel_t)i, .timer = LEDC_TIMER_0, .gpio = GPIO_NUM_NC, .enabled = false});
  
  // Save default hostname
  save_hostname(CONFIG_LWIP_LOCAL_HOSTNAME);

  // Save the version too
  parameters.nvs_set<uint8_t>("version", NVS_VERSION);
  parameters.commit();
}

/**
  @brief  Save a PWM timer configuration to the NVS
  
  @param  config timer_config_t to save to the NVS
  @retval none
*/
void NVS::save_timer_config(const timer_config_t& config)
{
  // Fuckin stringstreams were truncating the key!
  char key[16] = {0};
  snprintf(key, 16, "timer%d", config.id);

  parameters.nvs_set<timer_config_t>(std::string(key), config);

  parameters.commit();
}

/**
  @brief  Fetch a PWM timer configuration from the NVS
  
  @param  id ID of timer
  @retval timer_config_t
*/
timer_config_t NVS::get_timer_config(uint32_t id)
{
  // Fuckin stringstreams were truncating the key!
  char key[16] = {0};
  snprintf(key, 16, "timer%d", id);
  
  timer_config_t config;
  parameters.nvs_get<timer_config_t>(std::string(key), config);

  return config;
}

/**
  @brief  Save a PWM channel configuration to the NVS
  
  @param  name Friendly name of the channel
  @param  config channel_config_t to save to the NVS
  @retval none
*/
void NVS::save_channel_config(const std::string& name, const channel_config_t& config)
{
  char key[16] = {0};
  snprintf(key, 16, "channel%d", config.id);

  parameters.nvs_set<channel_config_t>(std::string(key), config);

  parameters.nvs_set<std::string>(std::string(key) + "_name", name);

  parameters.commit();
}

/**
  @brief  Fetch a PWM channel configuration from the NVS
  
  @param  id ID of timer
  @retval std::pair<std::string, channel_config_t>
*/
std::pair<std::string, channel_config_t> NVS::get_channel_config(uint32_t id)
{
  char key[16] = {0};
  snprintf(key, 16, "channel%d", id);
  
  channel_config_t config;
  parameters.nvs_get<channel_config_t>(std::string(key), config);

  std::string name;
  parameters.nvs_get<std::string>(std::string(key) + "_name", name);

  return std::make_pair(name, config);
}

/**
  @brief  Erase all data in the schedule NVS. Does not commit!
  
  @param  none
  @retval none
*/
void NVS::erase_schedule()
{
  schedule.erase_all();

  // Restore the version byte 
  schedule.nvs_set<uint8_t>("version", NVS_VERSION);
  schedule.commit();
}

/**
  @brief  Commit changes in the schedule NVS.
  
  @param  none
  @retval none
*/
void NVS::commit_schedule()
{
  schedule.commit();
}

/**
  @brief  Save a schedule entry JSON representation
  
  @param  tod Time Of Day key for the schedule entry
  @param  json JSON object for the TOD 
  @retval none
*/
void NVS::save_schedule_entry_json(const std::string& tod, const std::string& json)
{
  schedule.nvs_set<std::string>(tod, json);
}

/**
  @brief  Fetch all schedule entry JSON from NVS
  
  @param  none
  @retval std::map<std::string, std::string>
*/
std::map<std::string, std::string> NVS::get_schedule_json()
{ 
  // Find all keys in the schedule NVS
  const std::vector<std::string> keys = schedule.nvs_find(NVS_TYPE_STR);

  std::map<std::string, std::string> scheduleJson;
  for (auto& k : keys)
  {
    if (schedule.nvs_get<std::string>(k, scheduleJson[k]) != ESP_OK)
      ESP_LOGW(TAG, "Failed to get NVS schedule entry for '%s'", k.c_str());
  }

  return scheduleJson;
}

/**
  @brief  Save device hostname to NVS
  
  @param  hostname Hostname of device
  @retval none
*/
void NVS::save_hostname(const std::string& hostname)
{
  parameters.nvs_set<std::string>("hostname", hostname);

  parameters.commit();
}

/**
  @brief  Fetch device hostname from NVS
  
  @param  none
  @retval std::string
*/
std::string NVS::get_hostname()
{
  std::string hostname = "";
  parameters.nvs_get<std::string>("hostname", hostname);
  
  return hostname;
}

/**
  @brief  Save NTP servers to NVS
  
  @param  servers Vector of server hostnames/addresses
  @retval none
*/
void NVS::save_ntp_servers(const std::vector<std::string>& servers)
{
  if (servers.size() > CONFIG_LWIP_DHCP_MAX_NTP_SERVERS)
    ESP_LOGW(TAG, "Discarded additional NTP servers. Storing first %d.", CONFIG_LWIP_DHCP_MAX_NTP_SERVERS);
  
  for (uint8_t i = 0; i < servers.size() && i < CONFIG_LWIP_DHCP_MAX_NTP_SERVERS; i++)
  {
    char key[16] = {0};
    snprintf(key, 16, "ntp%d", i);

    parameters.nvs_set<std::string>(key, servers[i]);
  }

  parameters.commit();
}

/**
  @brief  Fetch NTP servers from NVS
  
  @param  none
  @retval std::vector<std::string>
*/
std::vector<std::string> NVS::get_ntp_servers()
{
  std::vector<std::string> servers;

  for (uint8_t i = 0; i < CONFIG_LWIP_DHCP_MAX_NTP_SERVERS; i++)
  {
    char key[16] = {0};
    snprintf(key, 16, "ntp%d", i);

    std::string ntp;
    if (parameters.nvs_get<std::string>(key, ntp) == ESP_OK)
      servers.push_back(ntp);
  }
  
  return servers;
}