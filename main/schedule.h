#ifndef __SCHEDULE_H__
#define __SCHEDULE_H__

#include <set>
#include <list>
#include <string>
#include <map>

#include "driver/ledc.h"
#include "driver/gpio.h"

typedef struct timer_config_t
{
  ledc_timer_t id;
  int32_t frequency_Hz;

  bool operator==(const timer_config_t& other) const 
  {
    return (id == other.id) && (frequency_Hz == other.frequency_Hz);
  }

  bool operator!=(const timer_config_t& other) const { return !(*this == other); }
} timer_config_t;

typedef struct channel_config_t
{
  ledc_channel_t id;
  ledc_timer_t timer;
  gpio_num_t gpio;
  bool enabled;

  bool operator==(const channel_config_t& other) const 
  {
    return (id == other.id) && (timer == other.timer) && (gpio == other.gpio) && (enabled == other.enabled);
  }

  bool operator!=(const channel_config_t& other) const { return !(*this == other); }
} channel_config_t;

class Schedule
{
  public:
    // Typedef some names for LED usage
    typedef ledc_channel_t led_channel_t;
    typedef float led_intensity_t;

    // Create a type to help distinguish that we work with TOD
    typedef time_t time_of_day_t;

    // Short-hand for channel->intensity map
    typedef std::map<led_channel_t, led_intensity_t> entry_t;

    static constexpr time_of_day_t INVALID_TOD = (time_of_day_t) -1;
    static constexpr int MAX_SCHEDULE_ERROR = 5; // 5 seconds

    void insert(time_of_day_t time, led_channel_t channel, led_intensity_t intensity)
    {
      schedule[time][channel] = intensity;
    }

    void set(time_of_day_t time, const entry_t& entry)
    {
      schedule[time] = entry;
    }

    time_of_day_t next(time_of_day_t now) const
    {
      if (schedule.empty())
        return INVALID_TOD;

      auto it = schedule.upper_bound(now);
      
      // Loop to start
      if (it == schedule.end())
        it = schedule.begin();
      
      return it->first;
    }

    time_of_day_t prev(time_of_day_t now) const
    {
      if (schedule.empty())
        return INVALID_TOD;

      auto it = schedule.upper_bound(now);
      
      // Loop to end
      if (it == schedule.begin())
        it = schedule.end();
      
      // Get element before
      it--;

      return it->first;
    }

    void reset()
    {
      schedule.clear();
    }

    const entry_t& operator[](time_of_day_t time) const
    {
      static entry_t empty;
      
      if (schedule.count(time))
        return schedule.at(time);
      else
        return empty;
    }

    static time_of_day_t get_time_of_day()
    {
      time_t utc = time(nullptr);
      struct tm* local_tm = localtime(&utc);

      time_t now = local_tm->tm_hour * 3600 + local_tm->tm_min * 60 + local_tm->tm_sec;

      return now % seconds_per_day;
    }

    static time_of_day_t get_time_of_day(const std::string& tod)
    {
      struct tm timeinfo;
      strptime(tod.c_str(), "%H:%M", &timeinfo);

      time_t time = timeinfo.tm_hour * 3600 + timeinfo.tm_min * 60;

      return time % seconds_per_day;
    }

    static time_of_day_t delta(time_of_day_t next, time_of_day_t prev)
    {
      if (next == INVALID_TOD || prev == INVALID_TOD)
        return INVALID_TOD;

      return ((next - prev) + seconds_per_day) % seconds_per_day;
    }

  private:
    static constexpr int seconds_per_day = 86400;

    std::map<time_of_day_t, entry_t> schedule;
};

#endif