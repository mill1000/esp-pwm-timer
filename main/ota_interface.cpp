#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"

#include "ota_interface.h"

#define LOG_TAG "OTA"

#define OTA_STRUCT_DEFAULT()   {.handle = 0, .state = OTA::STATE_IDLE, .timeoutTimer = NULL}
static struct
{
  esp_ota_handle_t handle;
  OTA::STATE        state;
  TimerHandle_t    timeoutTimer;
} ota;

/**
  @brief  Helper function to cleanup OTA state at end or in case of error
  
  @param  none
  @retval esp_err_t
*/
static esp_err_t OTA::cleanup()
{
  // Remove timeout timer
  if (ota.timeoutTimer != NULL)
  {
    if (xTimerDelete(ota.timeoutTimer, 0) == pdPASS)
      ota.timeoutTimer = NULL;
    else
      ESP_LOGW(LOG_TAG, "Failed to delete timeout timer.");
  }

  // Check for non-initalized or error state 
  if (ota.state != OTA::STATE_IN_PROGRESS)
  {
    ota = OTA_STRUCT_DEFAULT();
    return ESP_ERR_INVALID_STATE;
  }

  esp_err_t result = esp_ota_end(ota.handle);
  if (result != ESP_OK) 
  {
    ESP_LOGE(LOG_TAG, "esp_ota_end failed, err=0x%x.", result);
    ota = OTA_STRUCT_DEFAULT();
    return result;
  }

  return ESP_OK;
}

/**
  @brief  Initalize an OTA update. Checks paritions and begins OTA process.
  
  @param  none
  @retval esp_err_t
*/
esp_err_t OTA::start()
{
  // Don't attempt to re-init an ongoing OTA
  if (ota.state != OTA::STATE_IDLE)
    return ESP_ERR_INVALID_STATE;

  // Check that the active and boot parition are the same otherwise we might be trying to double update
  const esp_partition_t* boot = esp_ota_get_boot_partition();
  const esp_partition_t* active = esp_ota_get_running_partition();
  if (boot != active)
    return ESP_ERR_INVALID_STATE;

  ESP_LOGI(LOG_TAG, "Boot partition type %d subtype %d at offset 0x%x.", boot->type, boot->subtype, boot->address);
  ESP_LOGI(LOG_TAG, "Active partition type %d subtype %d at offset 0x%x.", active->type, active->subtype, active->address);

  // Grab next update target
  const esp_partition_t* target = esp_ota_get_next_update_partition(NULL);
  if (target == NULL)
    return ESP_ERR_NOT_FOUND;

  ESP_LOGI(LOG_TAG, "Target partition type %d subtype %d at offset 0x%x.", target->type, target->subtype, target->address);

  esp_err_t result = esp_ota_begin(target, OTA_SIZE_UNKNOWN, &ota.handle);
  if (result != ESP_OK)
  {
    ESP_LOGE(LOG_TAG, "esp_ota_begin failed, error=0x%x.", result);
    return result;
  }

  // Create a timer that will handle timeout events
  ota.timeoutTimer = xTimerCreate("OTATimeout", pdMS_TO_TICKS(5000), false, 0, [](TimerHandle_t){
    ESP_LOGW(LOG_TAG, "Timeout during update. Performing cleanup...");
    cleanup();
  });

  // Start the timer
  if (xTimerStart(ota.timeoutTimer, pdMS_TO_TICKS(100)) != pdPASS)
    ESP_LOGE(LOG_TAG, "Failed to start timeout timer.");

  ota.state = OTA::STATE_IN_PROGRESS;
  return ESP_OK;
}

/**
  @brief  Write firmware data during OTA
  
  @param  data Data buffer to write to handle
  @param  length Length of data buffer
  @retval esp_err_t
*/
esp_err_t OTA::write(uint8_t* data, uint16_t length)
{
  // Check for non-initalize or error state
  if (ota.state != OTA::STATE_IN_PROGRESS)
    return ESP_ERR_INVALID_STATE;

  esp_err_t result = esp_ota_write(ota.handle, data, length);
  if (result != ESP_OK) 
  {
    ESP_LOGE(LOG_TAG, "esp_ota_write failed, err=0x%x.", result);
    ota.state = OTA::STATE_ERROR;
    return result;
  }
  
  // Reset timeout timer
  if (ota.timeoutTimer != NULL)
    xTimerReset(ota.timeoutTimer, pdMS_TO_TICKS(10));

  return ESP_OK;
}

/**
  @brief  Finalize an OTA update. Sets boot paritions and reboots device
  
  @param  none
  @retval esp_err_t
*/
esp_err_t OTA::end()
{
  // Perform clean up operations
  esp_err_t result = cleanup();
  if (result != ESP_OK) 
    return result;

  const esp_partition_t* target = esp_ota_get_next_update_partition(NULL);
  result = esp_ota_set_boot_partition(target);
  if (result != ESP_OK) 
  {
    ESP_LOGE(LOG_TAG, "esp_ota_set_boot_partition failed, err=0x%x.", result);
    ota = OTA_STRUCT_DEFAULT();
    return result;
  }

  const esp_partition_t* boot = esp_ota_get_boot_partition();
  ESP_LOGI(LOG_TAG, "Boot partition type %d subtype %d at offset 0x%x.", boot->type, boot->subtype, boot->address);

  // Create a timer that will reset the device
  TimerHandle_t rebootTimer = xTimerCreate("OTATimer", pdMS_TO_TICKS(3000), false, 0, [](TimerHandle_t){
    ESP_LOGI(LOG_TAG, "Rebooting...");
    ota.state = OTA::STATE_IDLE;
    esp_restart();
  });
  
  // Start the reboot timer
  if (xTimerStart(rebootTimer, pdMS_TO_TICKS(1000)) != pdPASS)
    ESP_LOGE(LOG_TAG, "Failed to start reboot timer.");

  ota.state = OTA::STATE_REBOOT;
  return ESP_OK;
}