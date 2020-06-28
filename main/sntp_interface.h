#ifndef __SNTP_INTERFACE_H__
#define __SNTP_INTERFACE_H__

namespace SNTP
{
  typedef void (*sync_callback)(void);

  void init(const std::string& timezone, sync_callback callback);
}

#endif