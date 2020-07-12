#ifndef __NVS_INTERFACE_H__
#define __NVS_INTERFACE_H__

#include <vector>
#include <string>
#include <map>

#include "schedule.h"

namespace NVS
{
  constexpr const char* PARAMETER_NAMESPACE = "pwm_config";
  constexpr const char* SCHEDULE_NAMESPACE = "pwm_schedule";

  constexpr uint8_t NVS_VERSION = 0;

  void init(void);

  void reset_configuration(void);
  void check_required_configuration(void);

  void save_timer_config(const timer_config_t& config);
  timer_config_t get_timer_config(uint32_t id);

  void save_channel_config(const std::string& name, const channel_config_t& config);
  std::pair<std::string, channel_config_t> get_channel_config(uint32_t id);

  void erase_schedule(void);
  void commit_schedule(void);
  void save_schedule_entry_json(const std::string& tod, const std::string& json);
  std::map<std::string, std::string> get_schedule_json(void);

  void save_hostname(const std::string& hostname);
  std::string get_hostname(void);

  void save_ntp_server(uint8_t index, const std::string& server);
  std::string get_ntp_server(uint8_t index);

  void save_timezone(const std::string& tz);
  std::string get_timezone(void);
}

#endif