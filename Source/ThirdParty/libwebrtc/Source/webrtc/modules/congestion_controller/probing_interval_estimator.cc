/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/congestion_controller/probing_interval_estimator.h"

#include <algorithm>
#include <utility>

namespace webrtc {

namespace {
constexpr int kMinIntervalMs = 2000;
constexpr int kDefaultIntervalMs = 3000;
constexpr int kMaxIntervalMs = 50000;
}

ProbingIntervalEstimator::ProbingIntervalEstimator(
    const AimdRateControl* aimd_rate_control)
    : ProbingIntervalEstimator(kMinIntervalMs,
                               kMaxIntervalMs,
                               kDefaultIntervalMs,
                               aimd_rate_control) {}

ProbingIntervalEstimator::ProbingIntervalEstimator(
    int64_t min_interval_ms,
    int64_t max_interval_ms,
    int64_t default_interval_ms,
    const AimdRateControl* aimd_rate_control)
    : min_interval_ms_(min_interval_ms),
      max_interval_ms_(max_interval_ms),
      default_interval_ms_(default_interval_ms),
      aimd_rate_control_(aimd_rate_control) {}

int64_t ProbingIntervalEstimator::GetIntervalMs() const {
  rtc::Optional<int> bitrate_drop =
      aimd_rate_control_->GetLastBitrateDecreaseBps();
  int increase_rate = aimd_rate_control_->GetNearMaxIncreaseRateBps();

  if (!bitrate_drop || increase_rate <= 0)
    return default_interval_ms_;

  return std::min(
      max_interval_ms_,
      std::max(static_cast<int64_t>(1000 * (*bitrate_drop) / increase_rate),
               min_interval_ms_));
}

}  // namespace webrtc
