#ifndef __NVS_PARAMETERS_H__
#define __NVS_PARAMETERS_H__

#include <string>
#include <vector>
#include <cstring>

#include "nvs_flash.h"
#include "esp_err.h"

/**
  @brief  Class to help abstract some NVS actions
*/
class nvs_helper
{
  public:
    // Typedef the callback format for easier use
    typedef void (*nvs_callback_t)(const std::string&, esp_err_t);

    // Result of last operation
    esp_err_t result = ESP_OK;

    void initalize(nvs_handle handle = 0, nvs_callback_t callback = NULL)
    {
      this->handle = handle;
      this->callback = callback;
    }

    esp_err_t commit(void)
    {
      assert(handle);
      
      result = nvs_commit(handle);
      if (result != ESP_OK && callback != NULL)
        callback("COMMIT", result);

      return result;
    }

    esp_err_t erase_all(void)
    {
      assert(handle);

      result = nvs_erase_all(handle);
      if (result != ESP_OK && callback != NULL)
        callback("ERASE", result);

      return result;
    }

    template<typename T> esp_err_t nvs_set(const std::string& key, const T& value)
    {
      assert(handle);

      result = _nvs_set(key.c_str(), value);
      if (result != ESP_OK && callback != NULL)
        callback(key, result);

      return result;
    }

    template<typename T> esp_err_t nvs_get(const std::string& key, T& value)
    {
      assert(handle);

      result = _nvs_get(key.c_str(), value);
      if (result != ESP_OK && callback != NULL)
        callback(key, result);

      return result;
    }

  private:
    nvs_handle_t handle;
    nvs_callback_t callback = NULL;

    /**
      @brief  Overloaded wrapper for nvs_set to expand to nvs_set_T
    */
    esp_err_t _nvs_set(const char* key, uint8_t value)
    {
      return nvs_set_u8(handle, key, value);
    }

    esp_err_t _nvs_set(const char* key, int8_t value)
    {
      return nvs_set_i8(handle, key, value);
    }

    esp_err_t _nvs_set(const char* key, uint16_t value)
    {
      return nvs_set_u16(handle, key, value);
    }

    esp_err_t _nvs_set(const char* key, int16_t value)
    {
      return nvs_set_i16(handle, key, value);
    }

    esp_err_t _nvs_set(const char* key, uint32_t value)
    {
      return nvs_set_u32(handle, key, value);
    }

    esp_err_t _nvs_set(const char* key, int32_t value)
    {
      return nvs_set_i32(handle, key, value);
    }

    esp_err_t _nvs_set(const char* key, uint64_t value)
    {
      return nvs_set_u64(handle, key, value);
    }

    esp_err_t _nvs_set(const char* key, int64_t value)
    {
      return nvs_set_i64(handle, key, value);
    }

    esp_err_t _nvs_set(const char* key, const char* value)
    {
      return nvs_set_str(handle, key, value);
    }

    esp_err_t _nvs_set(const char* key, const std::string& value)
    {
      return nvs_set_str(handle, key, value.c_str());
    }

    template<typename T> esp_err_t _nvs_set(const char* key, const T& value)
    {
      return nvs_set_blob(handle, key, &value, sizeof(T));
    }

    /**
      @brief  Overloaded wrapper for nvs_get to expand to nvs_get_T
    */
    esp_err_t _nvs_get(const char* key, uint8_t& value)
    {
      return nvs_get_u8(handle, key, &value);
    }

    esp_err_t _nvs_get(const char* key, int8_t& value)
    {
      return nvs_get_i8(handle, key, &value);
    }

    esp_err_t _nvs_get(const char* key, uint16_t& value)
    {
      return nvs_get_u16(handle, key, &value);
    }

    esp_err_t _nvs_get(const char* key, int16_t& value)
    {
      return nvs_get_i16(handle, key, &value);
    }

    esp_err_t _nvs_get(const char* key, uint32_t& value)
    {
      return nvs_get_u32(handle, key, &value);
    }

    esp_err_t _nvs_get(const char* key, int32_t& value)
    {
      return nvs_get_i32(handle, key, &value);
    }

    esp_err_t _nvs_get(const char* key, uint64_t& value)
    {
      return nvs_get_u64(handle, key, &value);
    }

    esp_err_t _nvs_get(const char* key, int64_t& value)
    {
      return nvs_get_i64(handle, key, &value);
    }

    esp_err_t _nvs_get(const char* key, std::string& value)
    {
      size_t length = 0;
      esp_err_t result = nvs_get_str(handle, key, NULL, &length);
      if (result != ESP_OK)
        return result;

      std::string buffer;
      buffer.resize(length);
      
      result = nvs_get_str(handle, key, &buffer[0], &length);

      if (result == ESP_OK)
      {
        // Truncate the NULL character from the string
        buffer.resize(length - 1);

        // Swap with input reference
        value.swap(buffer);
      }

      return result;
    }

    template<typename T> esp_err_t _nvs_get(const char* key, T& value)
    {
      size_t length = sizeof(T);
      return nvs_get_blob(handle, key, &value, &length);
    }
};

/**
  @brief  Class that represent a stored NVS parameter (key-value pair)
*/
template<typename T> class parameter_t
{
  public:
    parameter_t(nvs_helper& _helper, const char* key) : helper(_helper)
    {
      this->key = key;
    }

    parameter_t& operator=(const T& other)
    {
      this->set(other);
      return *this;
    }

    operator T()
    {
      T value = T();
      this->get(value);

      return value;
    }

    bool exists()
    {
      T value = T();    
      return this->get(value) == ESP_OK;
    }

  protected:
    nvs_helper& helper;
    const char* key;
    
    esp_err_t set(T value)
    {
      return helper.nvs_set(key, value);
    }

    esp_err_t get(T& value)
    {
      return helper.nvs_get(key, value);
    }

};

/**
  @brief  Class that represent a stored NVS parameter (key-value pair)
          with cached access in RAM
*/
template<typename T> class cached_parameter_t : public parameter_t<T>
{
  public:
    cached_parameter_t(nvs_helper& _helper, const char* key, T defaultValue = T()) : parameter_t<T>(_helper, key)
    {
      this->cache = defaultValue;
    }

    cached_parameter_t& operator=(const T& other)
    {
      if (this->set(other) == ESP_OK)
      {
        cache = other;
        cached = true;
      }

      return *this;
    }

    operator T()
    {
      if (cached)
        return cache;

      T value = T();
      if (this->get(value) == ESP_OK)
      {
          cache = value;
          cached = true;
      }

      return cache;
    }

  protected:
    T cache = T();
    bool cached = false;
};

#endif