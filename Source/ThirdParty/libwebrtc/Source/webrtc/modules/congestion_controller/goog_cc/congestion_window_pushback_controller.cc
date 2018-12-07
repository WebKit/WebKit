/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <string>

#include "modules/congestion_controller/goog_cc/congestion_window_pushback_controller.h"
#include "rtc_base/checks.h"
#include "rtc_base/format_macros.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {

// When CongestionWindowPushback is enabled, the pacer is oblivious to
// the congestion window. The relation between outstanding data and
// the congestion window affects encoder allocations directly.
// This experiment is build on top of congestion window experiment.
const char kCongestionPushbackExperiment[] = "WebRTC-CongestionWindowPushback";
const uint32_t kDefaultMinPushbackTargetBitrateBps = 30000;

bool ReadCongestionWindowPushbackExperimentParameter(
    uint32_t* min_pushback_target_bitrate_bps) {
  RTC_DCHECK(min_pushback_target_bitrate_bps);
  std::string experiment_string =
      webrtc::field_trial::FindFullName(kCongestionPushbackExperiment);
  int parsed_values = sscanf(experiment_string.c_str(), "Enabled-%" PRIu32,
                             min_pushback_target_bitrate_bps);
  if (parsed_values == 1) {
    RTC_CHECK_GE(*min_pushback_target_bitrate_bps, 0)
        << "Min pushback target bitrate must be greater than or equal to 0.";
    return true;
  }
  return false;
}

CongestionWindowPushbackController::CongestionWindowPushbackController() {
  if (!ReadCongestionWindowPushbackExperimentParameter(
          &min_pushback_target_bitrate_bps_)) {
    min_pushback_target_bitrate_bps_ = kDefaultMinPushbackTargetBitrateBps;
  }
}

CongestionWindowPushbackController::CongestionWindowPushbackController(
    uint32_t min_pushback_target_bitrate_bps)
    : min_pushback_target_bitrate_bps_(min_pushback_target_bitrate_bps) {}

void CongestionWindowPushbackController::UpdateOutstandingData(
    size_t outstanding_bytes) {
  outstanding_bytes_ = outstanding_bytes;
}

void CongestionWindowPushbackController::UpdateMaxOutstandingData(
    size_t max_outstanding_bytes) {
  DataSize data_window = DataSize::bytes(max_outstanding_bytes);
  if (current_data_window_) {
    data_window = (data_window + current_data_window_.value()) / 2;
  }
  current_data_window_ = data_window;
}

void CongestionWindowPushbackController::SetDataWindow(DataSize data_window) {
  current_data_window_ = data_window;
}

uint32_t CongestionWindowPushbackController::UpdateTargetBitrate(
    uint32_t bitrate_bps) {
  if (!current_data_window_ || current_data_window_->IsZero())
    return bitrate_bps;
  double fill_ratio =
      outstanding_bytes_ / static_cast<double>(current_data_window_->bytes());
  if (fill_ratio > 1.5) {
    encoding_rate_ratio_ *= 0.9;
  } else if (fill_ratio > 1) {
    encoding_rate_ratio_ *= 0.95;
  } else if (fill_ratio < 0.1) {
    encoding_rate_ratio_ = 1.0;
  } else {
    encoding_rate_ratio_ *= 1.05;
    encoding_rate_ratio_ = std::min(encoding_rate_ratio_, 1.0);
  }
  uint32_t adjusted_target_bitrate_bps =
      static_cast<uint32_t>(bitrate_bps * encoding_rate_ratio_);

  // Do not adjust below the minimum pushback bitrate but do obey if the
  // original estimate is below it.
  bitrate_bps = adjusted_target_bitrate_bps < min_pushback_target_bitrate_bps_
                    ? std::min(bitrate_bps, min_pushback_target_bitrate_bps_)
                    : adjusted_target_bitrate_bps;
  return bitrate_bps;
}

}  // namespace webrtc
