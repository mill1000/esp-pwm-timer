#ifndef __LEDC_INTERFACE_H__
#define __LEDC_INTERFACE_H__

#include <set>
#include <list>
#include <string>
#include <map>

#include "driver/ledc.h"
#include "driver/gpio.h"

#include "schedule.h"

namespace LEDC
{
  constexpr ledc_timer_bit_t LED_RESOLUTION = LEDC_TIMER_10_BIT;
  constexpr ledc_mode_t LED_MODE = LEDC_HIGH_SPEED_MODE;
  
  void init(void);
  
  void configure_timer(const timer_config_t& config);
  void configure_channel(const channel_config_t& config);

  void set_intensity(ledc_channel_t channel, double intensity, uint32_t fade_ms = 5000);
}

#endif