#ifndef __MAIN_H__
#define __MAIN_H__

#define pdMS_TO_TICKS64( xTimeInMs ) ( ( ( uint64_t ) ( xTimeInMs ) * configTICK_RATE_HZ ) / ( uint64_t ) 1000 )

typedef enum
{
  // Events fired for maintaining schedule oop
  MAIN_EVENT_SYSTEM_TIME_UPDATED   = 1 << 0,
  MAIN_EVENT_LED_TIMER_EXPIRED     = 1 << 1,
  // Events fired when configs changed
  MAIN_EVENT_CONFIG_UPDATE    = 1 << 2,
  MAIN_EVENT_SCHEDULE_UPDATE  = 1 << 3,
  // System events
  MAIN_EVENT_REBOOT           = 1 << 4,
  MAIN_EVENT_REMOUNT_SPIFFS   = 1 << 5,
  MAIN_EVENT_RECONFIGURE_SNTP = 1 << 6,
  MAIN_EVENT_ALL              = 0x00FFFFFF, // 24 bits max
} MAIN_EVENT;

void signal_event(MAIN_EVENT event);

#endif