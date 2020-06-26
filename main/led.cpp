#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"

#include <cstring>

#include "led.h"

#define TAG "LED"

void ledInit(uint32_t frequency_Hz)
{
  // Configure timer 0
  ledc_timer_config_t timer_config;
  memset(&timer_config, 0, sizeof(ledc_timer_config_t));

  timer_config.speed_mode = LED_MODE;
  timer_config.duty_resolution = LED_RESOLUTION;
  timer_config.timer_num = LED_TIMER;
  timer_config.freq_hz = frequency_Hz;
  timer_config.clk_cfg = LEDC_AUTO_CLK;

  ledc_timer_config(&timer_config);

  // Configure channel0
  ledc_channel_config_t channel_config;
  memset(&channel_config, 0, sizeof(ledc_channel_config_t));

  channel_config.channel    = LEDC_CHANNEL_0;
  channel_config.duty       = 0;
  channel_config.gpio_num   = 18;
  channel_config.speed_mode = LED_MODE;
  channel_config.hpoint     = 0;
  channel_config.timer_sel  = LED_TIMER;

  ledc_channel_config(&channel_config);

  // Enable fade function
  ledc_fade_func_install(0);
}

void ledSetIntensity(ledc_channel_t channel, double intensity, uint32_t fade_ms)
{
  // Clamp from 0 - 1
  intensity = std::max(intensity, 0.0);
  intensity = std::min(intensity, 1.0);

  // Calculate integer intensity based on resolution
  uint32_t range = intensity * ((1 << LED_RESOLUTION) - 1);
  
  ESP_LOGD(TAG, "Setting channel %d to %d with fade of %d ms.", channel, range, fade_ms);

  ledc_set_fade_with_time(LED_MODE, channel, range, fade_ms);
  ledc_fade_start(LED_MODE, channel, LEDC_FADE_NO_WAIT);
}

