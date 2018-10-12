/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_FAKE_MDNS_RESPONDER_H_
#define RTC_BASE_FAKE_MDNS_RESPONDER_H_

#include <map>
#include <memory>
#include <string>

#include "rtc_base/mdns_responder_interface.h"

#include "rtc_base/helpers.h"

namespace webrtc {

class FakeMDnsResponder : public MDnsResponderInterface {
 public:
  FakeMDnsResponder() = default;
  ~FakeMDnsResponder() = default;

  void CreateNameForAddress(const rtc::IPAddress& addr,
                            NameCreatedCallback callback) override {
    std::string name;
    if (addr_name_map_.find(addr) != addr_name_map_.end()) {
      name = addr_name_map_[addr];
    } else {
      name = std::to_string(next_available_id_++) + ".local";
      addr_name_map_[addr] = name;
    }
    callback(addr, name);
  }
  void RemoveNameForAddress(const rtc::IPAddress& addr,
                            NameRemovedCallback callback) override {
    auto it = addr_name_map_.find(addr);
    if (it != addr_name_map_.end()) {
      addr_name_map_.erase(it);
    }
    callback(it != addr_name_map_.end());
  }

 private:
  uint32_t next_available_id_ = 0;
  std::map<rtc::IPAddress, std::string> addr_name_map_;
};

}  // namespace webrtc

#endif  // RTC_BASE_FAKE_MDNS_RESPONDER_H_
