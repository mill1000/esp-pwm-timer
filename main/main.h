#ifndef __MAIN_H__
#define __MAIN_H__

typedef enum
{
  // Events fired for maintaining schedule oop
  MAIN_EVENT_SYSTEM_TIME_UPDATED   = 1 << 0,
  MAIN_EVENT_LED_TIMER_EXPIRED     = 1 << 1,
  // Events fired when configs changed
  MAIN_EVENT_TIMER_CONFIG_UPDATE   = 1 << 2,
  MAIN_EVENT_CHANNEL_CONFIG_UPDATE = 1 << 3,
  MAIN_EVENT_SCHEDULE_UPDATE       = 1 << 4,
  MAIN_EVENT_ALL                   = 0x00FFFFFF, // 24 bits max
} MAIN_EVENT;

void signal_event(MAIN_EVENT event);

#endif