#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"

#include <cstring>

#include "ledc_interface.h"
#include "nvs_interface.h"
#include "schedule.h"

#define TAG "LED"

/**
  @brief  Initalize the LEDC peripheral and enable fade function
  
  @param  none
  @retval none
*/
void LEDC::init()
{
  // Configure timer 0
  timer_config_t timer = NVS::get_timer_config(0);
  configure_timer(timer);

  // Configure channel0
  channel_config_t channel = NVS::get_channel_config(0);
  configure_channel(channel);

  // Enable fade function
  ledc_fade_func_install(0);
}

/**
  @brief  Configure a LEDC timer with the provided configuration
  
  @param  config timer_config_t
  @retval none
*/
void LEDC::configure_timer(const timer_config_t& config)
{
  if (config.id >= LEDC_TIMER_MAX)
  {
    ESP_LOGW(TAG, "Invalid timer ID: %d", config.id);
    return;
  }

  ledc_timer_config_t timer_config;
  memset(&timer_config, 0, sizeof(ledc_timer_config_t));

  timer_config.speed_mode = LED_MODE;
  timer_config.duty_resolution = LED_RESOLUTION;
  timer_config.timer_num = (ledc_timer_t) config.id;
  timer_config.freq_hz = config.frequency_Hz;
  timer_config.clk_cfg = LEDC_AUTO_CLK;

  ESP_LOGI(TAG, "Timer %d set to %d Hz", config.id, config.frequency_Hz);

  ledc_timer_config(&timer_config);
}

/**
  @brief  Configure a LEDC channel with the provided configuration
  
  @param  config channel_config_t
  @retval none
*/
void LEDC::configure_channel(const channel_config_t& config)
{
  if (config.id >= LEDC_CHANNEL_MAX)
  {
    ESP_LOGW(TAG, "Invalid channel ID: %d", config.id);
    return;
  }

  if (config.enabled == false)
  {
    ESP_LOGI(TAG, "Channel %d: disabled", config.id);
    ledc_stop(LED_MODE, config.id, 0);
    return;
  }

  if (config.gpio == GPIO_NUM_NC || config.gpio >= GPIO_NUM_MAX)
  {
    ESP_LOGW(TAG, "Invalid GPIO: %d", config.gpio);
    return;
  }

  if (config.timer >= LEDC_TIMER_MAX)
  {
    ESP_LOGW(TAG, "Invalid timer: %d", config.gpio);
    return;
  }

  ledc_channel_config_t channel_config;
  memset(&channel_config, 0, sizeof(ledc_channel_config_t));

  channel_config.channel    = config.id;
  channel_config.duty       = 0;
  channel_config.gpio_num   = config.gpio;
  channel_config.speed_mode = LED_MODE;
  channel_config.hpoint     = 0;
  channel_config.timer_sel  = config.timer;

  ESP_LOGI(TAG, "Channel %d: GPIO: %d Timer: %d Name: %s", config.id, config.gpio, config.timer, "");//config.name.c_str());

  ledc_channel_config(&channel_config);
}

/**
  @brief  Set the relative intensity of the target channel with a fade
  
  @param  channel Target channel
  @param  intensity Desired intensity from 0 - 100 %
  @param  fade_ms Fade time in milliseconds. Defaults to 5 seconds.
  @retval none
*/
void LEDC::set_intensity(ledc_channel_t channel, double intensity, uint32_t fade_ms)
{
  // Clamp from 0 - 1
  intensity = std::max(intensity, 0.0);
  intensity = std::min(intensity, 100.0);

  // Scale to 0 - 1
  intensity /= 100.0;

  // Calculate integer intensity based on resolution
  uint32_t range = intensity * ((1 << LED_RESOLUTION) - 1);
  
  ESP_LOGD(TAG, "Setting channel %d to %d with fade of %d ms.", channel, range, fade_ms);

  ledc_set_fade_with_time(LED_MODE, channel, range, fade_ms);
  ledc_fade_start(LED_MODE, channel, LEDC_FADE_NO_WAIT);
}

