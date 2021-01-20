/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/test/performance_timer.h"

#include <math.h>

#include <numeric>

#include "rtc_base/checks.h"

namespace webrtc {
namespace test {

PerformanceTimer::PerformanceTimer(int num_frames_to_process)
    : clock_(webrtc::Clock::GetRealTimeClock()) {
  timestamps_us_.reserve(num_frames_to_process);
}

PerformanceTimer::~PerformanceTimer() = default;

void PerformanceTimer::StartTimer() {
  start_timestamp_us_ = clock_->TimeInMicroseconds();
}

void PerformanceTimer::StopTimer() {
  RTC_DCHECK(start_timestamp_us_);
  timestamps_us_.push_back(clock_->TimeInMicroseconds() - *start_timestamp_us_);
}

double PerformanceTimer::GetDurationAverage() const {
  return GetDurationAverage(0);
}

double PerformanceTimer::GetDurationStandardDeviation() const {
  return GetDurationStandardDeviation(0);
}

double PerformanceTimer::GetDurationAverage(
    size_t number_of_warmup_samples) const {
  RTC_DCHECK_GT(timestamps_us_.size(), number_of_warmup_samples);
  const size_t number_of_samples =
      timestamps_us_.size() - number_of_warmup_samples;
  return static_cast<double>(
             std::accumulate(timestamps_us_.begin() + number_of_warmup_samples,
                             timestamps_us_.end(), static_cast<int64_t>(0))) /
         number_of_samples;
}

double PerformanceTimer::GetDurationStandardDeviation(
    size_t number_of_warmup_samples) const {
  RTC_DCHECK_GT(timestamps_us_.size(), number_of_warmup_samples);
  const size_t number_of_samples =
      timestamps_us_.size() - number_of_warmup_samples;
  RTC_DCHECK_GT(number_of_samples, 0);
  double average_duration = GetDurationAverage(number_of_warmup_samples);

  double variance = std::accumulate(
      timestamps_us_.begin() + number_of_warmup_samples, timestamps_us_.end(),
      0.0, [average_duration](const double& a, const int64_t& b) {
        return a + (b - average_duration) * (b - average_duration);
      });

  return sqrt(variance / number_of_samples);
}

}  // namespace test
}  // namespace webrtc
