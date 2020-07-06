#ifndef __OTA_INTERFACE_H__
#define __OTA_INTERFACE_H__

namespace OTA
{
  typedef enum
  {
    STATE_IDLE,
    STATE_IN_PROGRESS,
    STATE_REBOOT,
    STATE_ERROR,
  } STATE;

  esp_err_t start(void);
  esp_err_t write(uint8_t* data, uint16_t length);
  esp_err_t end(void);
};

#endif