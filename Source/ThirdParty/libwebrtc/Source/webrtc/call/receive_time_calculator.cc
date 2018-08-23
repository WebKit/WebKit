/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/receive_time_calculator.h"
#include "absl/memory/memory.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace {
using ::webrtc::field_trial::FindFullName;
using ::webrtc::field_trial::IsEnabled;

const char kBweReceiveTimeCorrection[] = "WebRTC-BweReceiveTimeCorrection";
}  // namespace

ReceiveTimeCalculator::ReceiveTimeCalculator(int64_t min_delta_ms,
                                             int64_t max_delta_diff_ms)
    : min_delta_us_(min_delta_ms * 1000),
      max_delta_diff_us_(max_delta_diff_ms * 1000) {}

std::unique_ptr<ReceiveTimeCalculator>
ReceiveTimeCalculator::CreateFromFieldTrial() {
  if (!IsEnabled(kBweReceiveTimeCorrection))
    return nullptr;
  int min, max;
  if (sscanf(FindFullName(kBweReceiveTimeCorrection).c_str(), "Enabled,%d,%d",
             &min, &max) != 2) {
    RTC_LOG(LS_WARNING) << "Invalid number of parameters provided.";
    return nullptr;
  }
  return absl::make_unique<ReceiveTimeCalculator>(min, max);
}

int64_t ReceiveTimeCalculator::ReconcileReceiveTimes(int64_t packet_time_us_,
                                                     int64_t safe_time_us_) {
  if (!receive_time_offset_us_) {
    receive_time_offset_us_ = safe_time_us_ - packet_time_us_;
  } else {
    int64_t safe_delta_us = safe_time_us_ - last_safe_time_us_;
    int64_t packet_delta_us_ = packet_time_us_ - last_packet_time_us_;
    int64_t delta_diff = packet_delta_us_ - safe_delta_us;
    // Packet time should not decrease significantly, a large decrease indicates
    // a reset of the packet time clock and we should reset the offest
    // parameter. The safe reference time can increase in large jumps if the
    // thread measuring it is backgrounded for longer periods. But if the packet
    // time increases significantly more than the safe time, it indicates a
    // clock reset and we should reset the offset.

    if (packet_delta_us_ < min_delta_us_ || delta_diff > max_delta_diff_us_) {
      RTC_LOG(LS_WARNING) << "Received a clock jump of " << delta_diff
                          << " resetting offset";
      receive_time_offset_us_ = safe_time_us_ - packet_time_us_;
    }
  }
  last_packet_time_us_ = packet_time_us_;
  last_safe_time_us_ = safe_time_us_;
  return packet_time_us_ + *receive_time_offset_us_;
}
}  // namespace webrtc
