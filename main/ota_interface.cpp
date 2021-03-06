#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"

#include <string>

#include "ota_interface.h"
#include "main.h"

#define TAG "OTA"

static struct
{
  // This state is a very bad and lazy way of cordinating multiple OTA handles.
  OTA::STATE state;
} ota;

/**
  @brief  Factory function to construct a OTA::Handle object from a string type
  
  @param  target Name of OTA target
  @retval esp_err_t
*/
OTA::Handle* OTA::construct_handle(const std::string& target)
{
  if (target.compare("spiffs") == 0)
    return new OTA::SpiffsHandle();
  
  if (target.compare("firmware") == 0)
    return new OTA::AppHandle();

  return nullptr;
}

/**
  @brief  Helper function to cleanup OTA state at end or in case of error
  
  @param  none
  @retval esp_err_t
*/
esp_err_t OTA::AppHandle::cleanup()
{
  // Remove timeout timer
  if (this->timeout_timer != NULL)
  {
    if (xTimerDelete(this->timeout_timer, 0) == pdPASS)
      this->timeout_timer = NULL;
    else
      ESP_LOGW(TAG, "Failed to delete timeout timer.");
  }

  // Check for non-initalized or error state 
  if (ota.state != OTA::STATE_IN_PROGRESS)
  {
    ota.state = STATE_IDLE;
    return ESP_ERR_INVALID_STATE;
  }

  esp_err_t result = esp_ota_end(this->handle);
  if (result != ESP_OK) 
  {
    ESP_LOGE(TAG, "esp_ota_end failed, err=0x%x.", result);
    ota.state = STATE_IDLE;
    return result;
  }

  ota.state = STATE_IDLE;

  return ESP_OK;
}

/**
  @brief  Initialize an OTA update. Checks partitions and begins OTA process.
  
  @param  none
  @retval esp_err_t
*/
esp_err_t OTA::AppHandle::start()
{
  // Don't attempt to re-init an ongoing OTA
  if (ota.state != OTA::STATE_IDLE)
    return ESP_ERR_INVALID_STATE;

  // Check that the active and boot partition are the same otherwise we might be trying to double update
  const esp_partition_t* boot = esp_ota_get_boot_partition();
  const esp_partition_t* active = esp_ota_get_running_partition();
  if (boot != active)
    return ESP_ERR_INVALID_STATE;

  ESP_LOGI(TAG, "Boot partition type %d subtype %d at offset 0x%x.", boot->type, boot->subtype, boot->address);
  ESP_LOGI(TAG, "Active partition type %d subtype %d at offset 0x%x.", active->type, active->subtype, active->address);

  // Grab next update target
  const esp_partition_t* target = esp_ota_get_next_update_partition(NULL);
  if (target == NULL)
    return ESP_ERR_NOT_FOUND;

  ESP_LOGI(TAG, "Target partition type %d subtype %d at offset 0x%x.", target->type, target->subtype, target->address);

  esp_err_t result = esp_ota_begin(target, OTA_SIZE_UNKNOWN, &this->handle);
  if (result != ESP_OK)
  {
    ESP_LOGE(TAG, "esp_ota_begin failed, error=0x%x.", result);
    return result;
  }

  // Create a timer that will handle timeout events
  this->timeout_timer = xTimerCreate("OTATimeout", pdMS_TO_TICKS(5000), false, (void*)this, [](TimerHandle_t t){
    ESP_LOGW(TAG, "Timeout during update. Performing cleanup...");
    OTA::Handle* handle = (OTA::Handle*) pvTimerGetTimerID(t);
    if (handle != nullptr)
      handle->cleanup();
  });

  // Start the timer
  if (xTimerStart(this->timeout_timer, pdMS_TO_TICKS(100)) != pdPASS)
    ESP_LOGE(TAG, "Failed to start timeout timer.");

  ota.state = OTA::STATE_IN_PROGRESS;
  return ESP_OK;
}

/**
  @brief  Write firmware data during OTA
  
  @param  data Data buffer to write to handle
  @param  length Length of data buffer
  @retval esp_err_t
*/
esp_err_t OTA::AppHandle::write(uint8_t* data, uint16_t length)
{
  // Check for non-initialize or error state
  if (ota.state != OTA::STATE_IN_PROGRESS)
    return ESP_ERR_INVALID_STATE;

  esp_err_t result = esp_ota_write(this->handle, data, length);
  if (result != ESP_OK) 
  {
    ESP_LOGE(TAG, "esp_ota_write failed, err=0x%x.", result);
    ota.state = OTA::STATE_ERROR;
    return result;
  }
  
  // Reset timeout timer
  if (this->timeout_timer != NULL)
    xTimerReset(this->timeout_timer, pdMS_TO_TICKS(10));

  return ESP_OK;
}

/**
  @brief  Finalize an OTA update. Sets boot partitions and returns callback to reboot
  
  @param  none
  @retval OTA::end_result_t
*/
OTA::end_result_t OTA::AppHandle::end()
{
  // Construct result object
  OTA::end_result_t result;
  result.callback = nullptr;

  // Perform clean up operations
  result.status = cleanup();
  if (result.status != ESP_OK) 
    return result;

  const esp_partition_t* target = esp_ota_get_next_update_partition(NULL);
  result.status = esp_ota_set_boot_partition(target);
  if (result.status != ESP_OK) 
  {
    ESP_LOGE(TAG, "esp_ota_set_boot_partition failed, err=0x%x.", result.status);
    ota.state = STATE_IDLE;
    return result;
  }

  const esp_partition_t* boot = esp_ota_get_boot_partition();
  ESP_LOGI(TAG, "Boot partition type %d subtype %d at offset 0x%x.", boot->type, boot->subtype, boot->address);

  // Success. Update status and set reboot callback
  result.status = ESP_OK;
  result.callback = []() {
    ota.state = OTA::STATE_REBOOT;
    signal_event(MAIN_EVENT_REBOOT);
  };
  
  return result;
}

/**
  @brief  Helper function to cleanup OTA update of SPIFFS partion at end or in case of error
  
  @param  none
  @retval esp_err_t
*/
esp_err_t OTA::SpiffsHandle::cleanup()
{
  // Remove timeout timer
  if (this->timeout_timer != NULL)
  {
    if (xTimerDelete(this->timeout_timer, 0) == pdPASS)
      this->timeout_timer = NULL;
    else
      ESP_LOGW(TAG, "Failed to delete timeout timer.");
  }

  // Check for non-initalized or error state 
  if (ota.state != OTA::STATE_IN_PROGRESS)
  {
    ota.state = STATE_IDLE;
    return ESP_ERR_INVALID_STATE;
  }

  // Reset state
  ota.state = STATE_IDLE;

  return ESP_OK;
}

/**
  @brief  Initialize an OTA update of the SPIFFS partition.
  
  @param  none
  @retval esp_err_t
*/
esp_err_t OTA::SpiffsHandle::start()
{
  // Don't attempt to re-init an ongoing OTA
  if (ota.state != OTA::STATE_IDLE)
    return ESP_ERR_INVALID_STATE;
  
  // Locate target SPIFFS partition
  const esp_partition_t* part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, NULL);
  if (part == nullptr)
  {
    ESP_LOGE(TAG, "Failed to find SPIFFS partition for update.");
    return ESP_ERR_NOT_FOUND;
  }

  ESP_LOGI(TAG, "SPIFFS partition type %d subtype %d at offset 0x%x.", part->type, part->subtype, part->address);

  // Sanity check the partition table
  part = esp_partition_verify(part);
  if (part == nullptr)
  {
    ESP_LOGE(TAG, "SPIFFS partition failed verify check.");
    return ESP_ERR_NOT_FOUND;
  }

  // Erase SPIFFS partition
  esp_err_t result = esp_partition_erase_range(part, 0, part->size);
  if (result != ESP_OK)
  {
    ESP_LOGE(TAG, "esp_partition_erase_range failed, err=0x%x.", result);
    return result;
  }

  // Save the target partition
  this->partition = part;

  // Create a timer that will handle timeout events
  this->timeout_timer = xTimerCreate("OTATimeout", pdMS_TO_TICKS(5000), false, (void*)this, [](TimerHandle_t t){
    ESP_LOGW(TAG, "Timeout during update. Performing cleanup...");
    OTA::Handle* handle = (OTA::Handle*) pvTimerGetTimerID(t);
    if (handle != nullptr)
      handle->cleanup();
  });

  // Start the timer
  if (xTimerStart(this->timeout_timer, pdMS_TO_TICKS(100)) != pdPASS)
    ESP_LOGE(TAG, "Failed to start timeout timer.");

  ota.state = OTA::STATE_IN_PROGRESS;
  return ESP_OK;
}

/**
  @brief  Write data to the SPIFFS partition during OTA
  
  @param  data Data buffer to write to handle
  @param  length Length of data buffer
  @retval esp_err_t
*/
esp_err_t OTA::SpiffsHandle::write(uint8_t* data, uint16_t length)
{
  // Check for non-initialize or error state
  if (ota.state != OTA::STATE_IN_PROGRESS)
    return ESP_ERR_INVALID_STATE;

  assert(this->partition);
  esp_err_t result = esp_partition_write(this->partition, this->written, data, length);
  if (result != ESP_OK) 
  {
    ESP_LOGE(TAG, "esp_partition_write failed, err=0x%x.", result);
    ota.state = OTA::STATE_ERROR;
    return result;
  }
  
  this->written += length;
  
  // Reset timeout timer
  if (this->timeout_timer != NULL)
    xTimerReset(this->timeout_timer, pdMS_TO_TICKS(10));

  return ESP_OK;
}

/**
  @brief  Finalize an update of the SPIFFS
  
  @param  none
  @retval esp_err_t
*/
OTA::end_result_t OTA::SpiffsHandle::end()
{
  OTA::end_result_t result;
  result.status = cleanup();
  result.callback =  []() {
    signal_event(MAIN_EVENT_REMOUNT_SPIFFS);
  };

  return result;
}