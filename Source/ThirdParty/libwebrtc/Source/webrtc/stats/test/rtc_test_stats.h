/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef STATS_TEST_RTC_TEST_STATS_H_
#define STATS_TEST_RTC_TEST_STATS_H_

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/stats/rtc_stats.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class RTC_EXPORT RTCTestStats : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();
  RTCTestStats(const std::string& id, Timestamp timestamp);
  ~RTCTestStats() override;

  absl::optional<bool> m_bool;
  absl::optional<int32_t> m_int32;
  absl::optional<uint32_t> m_uint32;
  absl::optional<int64_t> m_int64;
  absl::optional<uint64_t> m_uint64;
  absl::optional<double> m_double;
  absl::optional<std::string> m_string;
  absl::optional<std::vector<bool>> m_sequence_bool;
  absl::optional<std::vector<int32_t>> m_sequence_int32;
  absl::optional<std::vector<uint32_t>> m_sequence_uint32;
  absl::optional<std::vector<int64_t>> m_sequence_int64;
  absl::optional<std::vector<uint64_t>> m_sequence_uint64;
  absl::optional<std::vector<double>> m_sequence_double;
  absl::optional<std::vector<std::string>> m_sequence_string;
  absl::optional<std::map<std::string, uint64_t>> m_map_string_uint64;
  absl::optional<std::map<std::string, double>> m_map_string_double;
};

}  // namespace webrtc

#endif  // STATS_TEST_RTC_TEST_STATS_H_
