/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/median_slope_estimator.h"

#include <algorithm>
#include <vector>

#include "modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "modules/remote_bitrate_estimator/test/bwe_test_logging.h"
#include "rtc_base/logging.h"

namespace webrtc {

constexpr unsigned int kDeltaCounterMax = 1000;

MedianSlopeEstimator::MedianSlopeEstimator(size_t window_size,
                                           double threshold_gain)
    : window_size_(window_size),
      threshold_gain_(threshold_gain),
      num_of_deltas_(0),
      accumulated_delay_(0),
      delay_hist_(),
      median_filter_(0.5),
      trendline_(0) {}

MedianSlopeEstimator::~MedianSlopeEstimator() {}

MedianSlopeEstimator::DelayInfo::DelayInfo(int64_t time,
                                           double delay,
                                           size_t slope_count)
    : time(time), delay(delay) {
  slopes.reserve(slope_count);
}

MedianSlopeEstimator::DelayInfo::~DelayInfo() = default;

void MedianSlopeEstimator::Update(double recv_delta_ms,
                                  double send_delta_ms,
                                  int64_t arrival_time_ms) {
  const double delta_ms = recv_delta_ms - send_delta_ms;
  ++num_of_deltas_;
  if (num_of_deltas_ > kDeltaCounterMax)
    num_of_deltas_ = kDeltaCounterMax;

  accumulated_delay_ += delta_ms;
  BWE_TEST_LOGGING_PLOT(1, "accumulated_delay_ms", arrival_time_ms,
                        accumulated_delay_);

  // If the window is full, remove the |window_size_| - 1 slopes that belong to
  // the oldest point.
  if (delay_hist_.size() == window_size_) {
    for (double slope : delay_hist_.front().slopes) {
      const bool success = median_filter_.Erase(slope);
      RTC_CHECK(success);
    }
    delay_hist_.pop_front();
  }
  // Add |window_size_| - 1 new slopes.
  for (auto& old_delay : delay_hist_) {
    if (arrival_time_ms - old_delay.time != 0) {
      // The C99 standard explicitly states that casts and assignments must
      // perform the associated conversions. This means that |slope| will be
      // a 64-bit double even if the division is computed using, e.g., 80-bit
      // extended precision. I believe this also holds in C++ even though the
      // C++11 standard isn't as explicit. Furthermore, there are good reasons
      // to believe that compilers couldn't perform optimizations that break
      // this assumption even if they wanted to.
      double slope = (accumulated_delay_ - old_delay.delay) /
                     static_cast<double>(arrival_time_ms - old_delay.time);
      median_filter_.Insert(slope);
      // We want to avoid issues with different rounding mode / precision
      // which we might get if we recomputed the slope when we remove it.
      old_delay.slopes.push_back(slope);
    }
  }
  delay_hist_.emplace_back(arrival_time_ms, accumulated_delay_,
                           window_size_ - 1);
  // Recompute the median slope.
  if (delay_hist_.size() == window_size_)
    trendline_ = median_filter_.GetPercentileValue();

  BWE_TEST_LOGGING_PLOT(1, "trendline_slope", arrival_time_ms, trendline_);
}

}  // namespace webrtc
