#include "nvs_flash.h"
#include "esp_log.h"

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

  // Open a second namespace just to hold the schedule
  if (schedule.open(&helper_callback) != ESP_OK)
  {
    ESP_LOGE(TAG, "Error opening NVS namespace '%s'.", SCHEDULE_NAMESPACE);
    return;
  }
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
  
  @param  config channel_config_t to save to the NVS
  @retval none
*/
void NVS::save_channel_config(const channel_config_t& config)
{
  char key[16] = {0};
  snprintf(key, 16, "channel%d", config.id);

  parameters.nvs_set<channel_config_t>(std::string(key), config);

  parameters.commit();
}

/**
  @brief  Fetch a PWM channel configuration from the NVS
  
  @param  id ID of timer
  @retval channel_config_t
*/
channel_config_t NVS::get_channel_config(uint32_t id)
{
  char key[16] = {0};
  snprintf(key, 16, "channel%d", id);
  
  channel_config_t config;
  parameters.nvs_get<channel_config_t>(std::string(key), config);

  return config;
}


/**
  @brief  Erase all data in the schedule NVS. Does not commit!
  
  @param  none
  @retval none
*/
void NVS::erase_schedule()
{
  schedule.erase_all();
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
  parameters.nvs_set<std::string>(tod, json);
}