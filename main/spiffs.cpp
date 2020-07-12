#include "esp_spiffs.h"
#include "esp_log.h"
#include "esp_err.h"

#include <cerrno>
#include <string.h>
#include <sys/stat.h>

#include "spiffs.h"

#define TAG "SPIFFS"

/**
  @brief  Initalize and mount the SPIFFS

  @param  none
  @retval none
*/
void SPIFFS::init()
{
  esp_vfs_spiffs_conf_t config = 
  {
    .base_path = ROOT_DIR,
    .partition_label = NULL,
    .max_files = 5,
    .format_if_mount_failed = true,
  };

  esp_err_t result = esp_vfs_spiffs_register(&config);
  if (result != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to initialize filesystem. Error: %s", esp_err_to_name(result));
    return;
  }
  
  ESP_LOGI(TAG, "Mount OK.");

  // Check for root directory
  // Tricks the VFS into understanding a directory exists at the root of this FS
  struct stat st;
  bool exists = (stat(ROOT_DIR, &st) == 0);
  if (!exists || !S_ISDIR(st.st_mode))
  {
    ESP_LOGI(TAG, "Creating root directory.");
    if (mkdir(ROOT_DIR, ACCESSPERMS) != 0)
    {
      ESP_LOGE(TAG, "Failed to create root directory. Error: %s", strerror(errno));
      return;
    }
  }
}

/**
  @brief  Remount the SPIFFS in case the partition has changed underneath

  @param  none
  @retval none
*/
void SPIFFS::remount()
{
  if (esp_spiffs_mounted(nullptr))
  {
    // Unregister/demount if counted
    esp_err_t result = esp_vfs_spiffs_unregister(nullptr);
    if (result != ESP_OK)
      ESP_LOGW(TAG, "Failed to unregister with VFS. Error: %s",  esp_err_to_name(result));
  }

  // Remount
  SPIFFS::init();
}