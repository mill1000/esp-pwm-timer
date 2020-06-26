#ifndef __LED_H__
#define __LED_H__

#include <set>
#include <list>
#include <string>
#include <map>

#include "driver/ledc.h"
#include "driver/gpio.h"

#define LED_TIMER       LEDC_TIMER_0
#define LED_RESOLUTION  LEDC_TIMER_10_BIT
#define LED_MODE        LEDC_HIGH_SPEED_MODE

#ifdef __cplusplus
extern "C"{
#endif


void ledInit(uint32_t frequency_Hz = 120);
void ledSetIntensity(ledc_channel_t channel, double intensity, uint32_t fade_ms = 5000);

#ifdef __cplusplus
}
#endif

#endif