/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_TOOLS_NETWORK_TESTER_CONFIG_READER_H_
#define WEBRTC_TOOLS_NETWORK_TESTER_CONFIG_READER_H_

#include <fstream>
#include <string>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/optional.h"

#include "webrtc/base/ignore_wundef.h"

#ifdef WEBRTC_NETWORK_TESTER_PROTO
RTC_PUSH_IGNORING_WUNDEF()
#include "webrtc/tools/network_tester/network_tester_config.pb.h"
RTC_POP_IGNORING_WUNDEF()
using webrtc::network_tester::config::NetworkTesterAllConfigs;
#else
class NetworkTesterConfigs;
#endif  // WEBRTC_NETWORK_TESTER_PROTO

namespace webrtc {

class ConfigReader {
 public:
  struct Config {
    int packet_send_interval_ms;
    int packet_size;
    int execution_time_ms;
  };
  explicit ConfigReader(const std::string& config_file_path);
  ~ConfigReader();

  rtc::Optional<Config> GetNextConfig();

 private:
  NetworkTesterAllConfigs proto_all_configs_;
  int proto_config_index_;
  RTC_DISALLOW_COPY_AND_ASSIGN(ConfigReader);
};

}  // namespace webrtc

#endif  // WEBRTC_TOOLS_NETWORK_TESTER_CONFIG_READER_H_
