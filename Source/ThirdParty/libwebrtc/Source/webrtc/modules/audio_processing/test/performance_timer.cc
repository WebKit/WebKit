/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/test/performance_timer.h"

#include <math.h>

#include <numeric>

#include "webrtc/base/checks.h"

namespace webrtc {
namespace test {

PerformanceTimer::PerformanceTimer(int num_frames_to_process)
    : clock_(webrtc::Clock::GetRealTimeClock()) {
  timestamps_us_.reserve(num_frames_to_process);
}

PerformanceTimer::~PerformanceTimer() = default;

void PerformanceTimer::StartTimer() {
  start_timestamp_us_ = rtc::Optional<int64_t>(clock_->TimeInMicroseconds());
}

void PerformanceTimer::StopTimer() {
  RTC_DCHECK(start_timestamp_us_);
  timestamps_us_.push_back(clock_->TimeInMicroseconds() - *start_timestamp_us_);
}

double PerformanceTimer::GetDurationAverage() const {
  RTC_DCHECK(!timestamps_us_.empty());
  return static_cast<double>(
             std::accumulate(timestamps_us_.begin(), timestamps_us_.end(), 0)) /
         timestamps_us_.size();
}

double PerformanceTimer::GetDurationStandardDeviation() const {
  RTC_DCHECK(!timestamps_us_.empty());
  double average_duration = GetDurationAverage();

  double variance = std::accumulate(
      timestamps_us_.begin(), timestamps_us_.end(), 0.0,
      [average_duration](const double& a, const int64_t& b) {
        return a + (b - average_duration) * (b - average_duration);
      });

  return sqrt(variance / timestamps_us_.size());
}

}  // namespace test
}  // namespace webrtc
