#ifndef __JSON_H__
#define __JSON_H__

#include <string>

#include "nlohmann/json.hpp"
#include "schedule.h"

namespace JSON
{
  template <typename T> T get_or_default(const nlohmann::json& json, const std::string& key, const T& default_value = T())
  {
    // If key is not null fetch value otherwise use default
    // Need because this lib is fucking dumb: https://github.com/nlohmann/json/issues/1163
    return json[key].is_null() ? default_value : json[key].get<T>();
  }

  bool parse_settings(const std::string& jString);
  std::string get_settings(void);

  Schedule::entry_t parse_schedule_entry(const std::string& jEntry);
}

#endif