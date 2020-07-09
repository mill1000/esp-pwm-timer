#ifndef __OTA_INTERFACE_H__
#define __OTA_INTERFACE_H__

#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "freertos/timers.h"

#include <string>

namespace OTA
{
  class Handle
  {
    public:
      virtual esp_err_t start(void) = 0;
      virtual esp_err_t write(uint8_t* data, uint16_t length) = 0;
      virtual esp_err_t end(void) = 0;
      virtual esp_err_t cleanup(void) = 0;

      virtual ~Handle () {}
  };

  class AppHandle : public Handle
  {
    public:
      AppHandle() {}

      esp_err_t start(void);
      esp_err_t write(uint8_t* data, uint16_t length);
      esp_err_t end(void);
      esp_err_t cleanup(void);

    private:
      TimerHandle_t timeoutTimer;
      esp_ota_handle_t handle;
  };

  class SpiffsHandle : public Handle
  {
    public:
      SpiffsHandle() {}

      esp_err_t start(void);
      esp_err_t write(uint8_t* data, uint16_t length);
      esp_err_t end(void);
      esp_err_t cleanup(void);

    private:
      TimerHandle_t timeoutTimer;
      uint32_t written = 0;
      const esp_partition_t* partition = nullptr;
  };

  typedef enum
  {
    STATE_IDLE,
    STATE_IN_PROGRESS,
    STATE_REBOOT,
    STATE_ERROR,
  } STATE;

  OTA::Handle* construct_handle(const std::string& target);
};

#endif