/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/nethelper.h"

#include "rtc_base/checks.h"
#include "rtc_base/ipaddress.h"

namespace cricket {

const char UDP_PROTOCOL_NAME[] = "udp";
const char TCP_PROTOCOL_NAME[] = "tcp";
const char SSLTCP_PROTOCOL_NAME[] = "ssltcp";
const char TLS_PROTOCOL_NAME[] = "tls";

int GetIpOverhead(int addr_family) {
  switch (addr_family) {
    case AF_INET:  // IPv4
      return 20;
    case AF_INET6:  // IPv6
      return 40;
    default:
      RTC_NOTREACHED() << "Invaild address family.";
      return 0;
  }
}

int GetProtocolOverhead(const std::string& protocol) {
  if (protocol == TCP_PROTOCOL_NAME || protocol == SSLTCP_PROTOCOL_NAME) {
    return 20;
  }
  return 8;
}

}  // namespace cricket
