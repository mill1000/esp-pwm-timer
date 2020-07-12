#ifndef __SPIFFS_H__
#define __SPIFFS_H__

namespace SPIFFS
{
  constexpr const char* ROOT_DIR = "/spiffs";

  void init(void);
  void remount(void);
}

#endif