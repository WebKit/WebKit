/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/clock_aligner.h"

#include <algorithm>
#include <optional>

#include "api/environment/environment.h"
#include "api/sequence_checker.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace {

constexpr TimeDelta kMaxStaleness = TimeDelta::Millis(500);
constexpr TimeDelta kMaxIdleGap = TimeDelta::Seconds(1);
constexpr double kMaxDriftRate = 0.001;

}  // namespace

ClockAligner::ClockAligner(const Environment& env)
    : env_(env),
      disable_non_monotonic_clock_fix_(
          env_.field_trials().IsDisabled("WebRTC-ClockAligner")) {}

Timestamp ClockAligner::Align(Timestamp time) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  Timestamp current_time = env_.clock().CurrentTime();
  if (disable_non_monotonic_clock_fix_) {
    return AlignAssumingMonotonicClock(time, current_time);
  }
  return AlignNonMonotonicClock(time, current_time);
}

Timestamp ClockAligner::AlignAssumingMonotonicClock(Timestamp time,
                                                    Timestamp current_time) {
  if (!time_offset_.has_value() || time + *time_offset_ > current_time) {
    // Estimate timestamp offset from first packet arrival time.
    // This may be wrong if packets have been buffered in the socket before
    // we read the first packet and `time_offset_` may then have to
    // be set again to ensure no arrival times are set in the future.
    time_offset_ = current_time - time;
  }
  Timestamp arrival_time = time + *time_offset_;
  RTC_DCHECK_LE(arrival_time, current_time);
  return arrival_time;
}

Timestamp ClockAligner::AlignNonMonotonicClock(Timestamp time,
                                               Timestamp current_time) {
  TimeDelta sample_offset = current_time - time;
  if (!time_offset_.has_value() || !last_current_time_.has_value() ||
      !last_time_.has_value()) {
    time_offset_ = sample_offset;
  } else {
    // Userspace monotonic elapsed time since last packet read.
    TimeDelta delta_mono =
        std::max(TimeDelta::Zero(), current_time - *last_current_time_);
    // Inter-arrival time from external clock. Not necessarily monotonic.
    TimeDelta delta_time = time - *last_time_;

    if (delta_time < TimeDelta::Zero()) {
      time_offset_ = sample_offset;
    } else if (sample_offset < *time_offset_) {
      time_offset_ = sample_offset;
    } else if (delta_mono > kMaxIdleGap) {
      time_offset_ = sample_offset;
    } else if (sample_offset - *time_offset_ > kMaxStaleness) {
      time_offset_ = sample_offset;
    } else {
      // Keep offset steady to preserve inter-arrival spacing,
      // while allowing slow positive drift tracking bounded by max_drift_rate.
      TimeDelta max_drift = delta_mono * kMaxDriftRate;
      *time_offset_ = std::min(sample_offset, *time_offset_ + max_drift);
    }
  }
  last_current_time_ = current_time;
  last_time_ = time;
  Timestamp arrival_time = time + *time_offset_;
  RTC_DCHECK_LE(arrival_time, current_time);
  return arrival_time;
}

}  // namespace webrtc
