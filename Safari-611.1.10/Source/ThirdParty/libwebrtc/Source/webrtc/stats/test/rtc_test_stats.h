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
#include <string>
#include <vector>

#include "api/stats/rtc_stats.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class RTC_EXPORT RTCTestStats : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCTestStats(const std::string& id, int64_t timestamp_us);
  RTCTestStats(const RTCTestStats& other);
  ~RTCTestStats() override;

  RTCStatsMember<bool> m_bool;
  RTCStatsMember<int32_t> m_int32;
  RTCStatsMember<uint32_t> m_uint32;
  RTCStatsMember<int64_t> m_int64;
  RTCStatsMember<uint64_t> m_uint64;
  RTCStatsMember<double> m_double;
  RTCStatsMember<std::string> m_string;
  RTCStatsMember<std::vector<bool>> m_sequence_bool;
  RTCStatsMember<std::vector<int32_t>> m_sequence_int32;
  RTCStatsMember<std::vector<uint32_t>> m_sequence_uint32;
  RTCStatsMember<std::vector<int64_t>> m_sequence_int64;
  RTCStatsMember<std::vector<uint64_t>> m_sequence_uint64;
  RTCStatsMember<std::vector<double>> m_sequence_double;
  RTCStatsMember<std::vector<std::string>> m_sequence_string;
};

}  // namespace webrtc

#endif  // STATS_TEST_RTC_TEST_STATS_H_
