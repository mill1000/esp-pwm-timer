#ifndef __SCHEDULE_H__
#define __SCHEDULE_H__

#include <set>
#include <list>
#include <string>
#include <map>

#include "driver/ledc.h"
#include "driver/gpio.h"

class Schedule
{
  public:
  // Typedef some names for LED usage
  typedef int   led_channel_t;
  typedef float led_intensity_t;

  // Create a type to help distinguish that we work with TOD
  typedef time_t time_of_day_t;

  // Short-hand for channel->intensity map
  typedef std::map<led_channel_t, led_intensity_t> state_change_map_t;

  void insert(time_of_day_t time, led_channel_t channel, led_intensity_t intensity)
  {
    schedule[time][channel] = intensity;
  }

  time_of_day_t next(time_of_day_t now) const
  {
    auto it = schedule.upper_bound(now);

    if (it != schedule.end())
      return it->first;
    else // Loop to start
      return schedule.begin()->first;
  }

  void reset()
  {
    schedule.clear();
  }

  const state_change_map_t& operator[](time_of_day_t time) const
  {
    static state_change_map_t empty;
    
    if (schedule.count(time))
      return schedule.at(time);
    else
      return empty;
  }

  static time_of_day_t get_time_of_day(time_t now)
  {
    return now % seconds_per_day;
  }

  static time_of_day_t delta(time_of_day_t next, time_of_day_t prev)
  {
    return ((next - prev) + seconds_per_day) % seconds_per_day;
  }

  private:
    static constexpr int seconds_per_day = 86400;

    std::map<time_of_day_t, state_change_map_t> schedule;
};

#endif