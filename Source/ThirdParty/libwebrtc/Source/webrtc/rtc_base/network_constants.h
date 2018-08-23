/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_NETWORK_CONSTANTS_H_
#define RTC_BASE_NETWORK_CONSTANTS_H_

#include <stdint.h>

namespace rtc {

static const uint16_t kNetworkCostMax = 999;
static const uint16_t kNetworkCostHigh = 900;
static const uint16_t kNetworkCostUnknown = 50;
static const uint16_t kNetworkCostLow = 10;
static const uint16_t kNetworkCostMin = 0;

enum AdapterType {
  // This enum resembles the one in Chromium net::ConnectionType.
  ADAPTER_TYPE_UNKNOWN = 0,
  ADAPTER_TYPE_ETHERNET = 1 << 0,
  ADAPTER_TYPE_WIFI = 1 << 1,
  ADAPTER_TYPE_CELLULAR = 1 << 2,
  ADAPTER_TYPE_VPN = 1 << 3,
  ADAPTER_TYPE_LOOPBACK = 1 << 4,
  // ADAPTER_TYPE_ANY is used for a network, which only contains a single "any
  // address" IP address (INADDR_ANY for IPv4 or in6addr_any for IPv6), and can
  // use any/all network interfaces. Whereas ADAPTER_TYPE_UNKNOWN is used
  // when the network uses a specific interface/IP, but its interface type can
  // not be determined or not fit in this enum.
  ADAPTER_TYPE_ANY = 1 << 5,
};

}  // namespace rtc

#endif  // RTC_BASE_NETWORK_CONSTANTS_H_
