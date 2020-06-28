#include <cstring>

#include "esp_wifi.h"
#include "esp_err.h"
#include "esp_log.h"

#include "wifi.h"

#define TAG "WiFi"

static uint32_t retry_count = WiFi::RETRY_COUNT;

/**
  @brief  Event handler for WIFI_EVENT base

  @param  arg Argument supplied when registering handler
  @param  event_base esp_event_base_t of event
  @param  event_id Event ID of generated event
  @param  event_data Pointer to event data structure
  @retval none
*/
static void wifiEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
  // We should only get WIFI_EVENTs in this handler
  assert(event_base == WIFI_EVENT);

  switch (event_id)
  {
    case WIFI_EVENT_STA_START:
      esp_wifi_connect();
      break;

    case WIFI_EVENT_STA_CONNECTED:
      ESP_LOGI(TAG, "Connected.");
      break;

    case WIFI_EVENT_STA_DISCONNECTED:
    {
      if (retry_count--) 
      {
        ESP_LOGI(TAG, "Retrying connection... (%d/%d)", retry_count, WiFi::RETRY_COUNT);
        esp_wifi_connect();
      }
      else
        ESP_LOGE(TAG, "Failed to connect.");
      
      break;
    } 
    
    default:
      ESP_LOGW(TAG, "Unhandled event. Base: '%s' ID: %d.", event_base, event_id);
      break;
  }
}

/**
  @brief  Event handler for IP_EVENT base

  @param  arg Argument supplied when registering handler
  @param  event_base esp_event_base_t of event
  @param  event_id Event ID of generated event
  @param  event_data Pointer to event data structure
  @retval none
*/
static void ipEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
  // We should only get IP_EVENTs in this handler
  assert(event_base == IP_EVENT);
  
  switch (event_id)
  {
    case IP_EVENT_STA_GOT_IP:
    {
      ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
      ESP_LOGI(TAG, "Assigned IP: %s", ip4addr_ntoa(&event->ip_info.ip));
      
      // Reset retires
      retry_count = WiFi::RETRY_COUNT;

      break;
    }

    default:
      ESP_LOGW(TAG, "Unhandled event. Base: '%s' ID: %d.", event_base, event_id);
    break;
  }
}

/**
  @brief  Initialize WiFi in station mode and connect to configured network.

  @param  none
  @retval none
*/
void WiFi::init_station()
{
  tcpip_adapter_init();

  ESP_ERROR_CHECK(esp_event_loop_create_default());

  wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&config));

  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifiEventHandler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ipEventHandler, NULL));

  wifi_config_t wifi_config;
  memset(&wifi_config, 0, sizeof(wifi_config_t));

  // Set configured SSID and password
  strcpy((char*)wifi_config.sta.ssid,     CONFIG_ESP_WIFI_SSID);
  strcpy((char*)wifi_config.sta.password, CONFIG_ESP_WIFI_PASSWORD);
  
  // Enable protected management frames
  wifi_config.sta.pmf_cfg.capable = true;
  wifi_config.sta.pmf_cfg.required = false;

  ESP_LOGI(TAG, "Connecting to SSID '%s'...", wifi_config.sta.ssid);
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());
}