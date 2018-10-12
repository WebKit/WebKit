/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_MDNS_RESPONDER_INTERFACE_H_
#define RTC_BASE_MDNS_RESPONDER_INTERFACE_H_

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "rtc_base/ipaddress.h"
#include "rtc_base/socketaddress.h"

namespace webrtc {

// Defines an mDNS responder that can be used in ICE candidate gathering, where
// the local IP addresses of host candidates are obfuscated by mDNS hostnames.
class MDnsResponderInterface {
 public:
  using NameCreatedCallback =
      std::function<void(const rtc::IPAddress&, const std::string&)>;
  using NameRemovedCallback = std::function<void(bool)>;

  MDnsResponderInterface() = default;
  virtual ~MDnsResponderInterface() = default;

  // Asynchronously creates a type-4 UUID hostname for an IP address. The
  // created name should be given to |callback| with the address that it
  // represents.
  virtual void CreateNameForAddress(const rtc::IPAddress& addr,
                                    NameCreatedCallback callback) = 0;
  // Removes the name mapped to the given address if there is such an
  // name-address mapping previously created via CreateNameForAddress. The
  // result of whether an associated name-address mapping is removed should be
  // given to |callback|.
  virtual void RemoveNameForAddress(const rtc::IPAddress& addr,
                                    NameRemovedCallback callback) = 0;
};

}  // namespace webrtc

#endif  // RTC_BASE_MDNS_RESPONDER_INTERFACE_H_
