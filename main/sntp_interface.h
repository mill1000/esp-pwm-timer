#ifndef __SNTP_INTERFACE_H__
#define __SNTP_INTERFACE_H__

#include <string>
#include <array>

namespace SNTP
{
  typedef void (*sync_callback_t)(void);
  typedef std::array<std::string, CONFIG_LWIP_DHCP_MAX_NTP_SERVERS> server_list_t;

  void init(const std::string& timezone, const server_list_t& servers, sync_callback_t callback);
  void reconfigure(const std::string& timezone, const server_list_t& servers);
}

#endif