#ifndef __JSON_H__
#define __JSON_H__

#include <string>
#include "nlohmann/json.hpp"

namespace JSON
{
  bool parse_settings(const std::string& jString);

  template <typename T> T get_or_default(const nlohmann::json& json, const std::string& key, const T& default_value)
  {
    // Is key is not null fetch value otherwise use default
    // Need because this lib is fucking dumb: https://github.com/nlohmann/json/issues/1163
    return json[key].is_null() ? default_value : json[key].get<T>();
  }
}

#endif