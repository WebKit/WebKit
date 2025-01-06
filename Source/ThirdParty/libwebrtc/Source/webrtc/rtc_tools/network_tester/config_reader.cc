/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_tools/network_tester/config_reader.h"

#include <fstream>
#include <iterator>
#include <string>

#include "rtc_base/checks.h"

namespace webrtc {

ConfigReader::ConfigReader(const std::string& config_file_path)
    : proto_config_index_(0) {
  std::ifstream config_stream(config_file_path,
                              std::ios_base::in | std::ios_base::binary);
  RTC_DCHECK(config_stream.is_open())
      << "Config " << config_file_path << " open failed";
  RTC_DCHECK(config_stream.good());
  std::string config_data((std::istreambuf_iterator<char>(config_stream)),
                          (std::istreambuf_iterator<char>()));
  if (config_data.size() > 0) {
    proto_all_configs_.ParseFromString(config_data);
  }
}

ConfigReader::~ConfigReader() = default;

std::optional<ConfigReader::Config> ConfigReader::GetNextConfig() {
#ifdef WEBRTC_NETWORK_TESTER_PROTO
  if (proto_config_index_ >= proto_all_configs_.configs_size())
    return std::nullopt;
  auto proto_config = proto_all_configs_.configs(proto_config_index_++);
  RTC_DCHECK(proto_config.has_packet_send_interval_ms());
  RTC_DCHECK(proto_config.has_packet_size());
  RTC_DCHECK(proto_config.has_execution_time_ms());
  Config config;
  config.packet_send_interval_ms = proto_config.packet_send_interval_ms();
  config.packet_size = proto_config.packet_size();
  config.execution_time_ms = proto_config.execution_time_ms();
  return config;
#else
  return std::nullopt;
#endif  //  WEBRTC_NETWORK_TESTER_PROTO
}

}  // namespace webrtc
