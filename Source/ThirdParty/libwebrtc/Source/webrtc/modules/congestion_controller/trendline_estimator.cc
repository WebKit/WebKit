/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/trendline_estimator.h"

#include <algorithm>

#include "api/optional.h"
#include "modules/remote_bitrate_estimator/test/bwe_test_logging.h"
#include "rtc_base/checks.h"

namespace webrtc {

namespace {
rtc::Optional<double> LinearFitSlope(
    const std::deque<std::pair<double, double>>& points) {
  RTC_DCHECK(points.size() >= 2);
  // Compute the "center of mass".
  double sum_x = 0;
  double sum_y = 0;
  for (const auto& point : points) {
    sum_x += point.first;
    sum_y += point.second;
  }
  double x_avg = sum_x / points.size();
  double y_avg = sum_y / points.size();
  // Compute the slope k = \sum (x_i-x_avg)(y_i-y_avg) / \sum (x_i-x_avg)^2
  double numerator = 0;
  double denominator = 0;
  for (const auto& point : points) {
    numerator += (point.first - x_avg) * (point.second - y_avg);
    denominator += (point.first - x_avg) * (point.first - x_avg);
  }
  if (denominator == 0)
    return rtc::nullopt;
  return numerator / denominator;
}
}  // namespace

enum { kDeltaCounterMax = 1000 };

TrendlineEstimator::TrendlineEstimator(size_t window_size,
                                       double smoothing_coef,
                                       double threshold_gain)
    : window_size_(window_size),
      smoothing_coef_(smoothing_coef),
      threshold_gain_(threshold_gain),
      num_of_deltas_(0),
      first_arrival_time_ms(-1),
      accumulated_delay_(0),
      smoothed_delay_(0),
      delay_hist_(),
      trendline_(0) {}

TrendlineEstimator::~TrendlineEstimator() {}

void TrendlineEstimator::Update(double recv_delta_ms,
                                double send_delta_ms,
                                int64_t arrival_time_ms) {
  const double delta_ms = recv_delta_ms - send_delta_ms;
  ++num_of_deltas_;
  if (num_of_deltas_ > kDeltaCounterMax)
    num_of_deltas_ = kDeltaCounterMax;
  if (first_arrival_time_ms == -1)
    first_arrival_time_ms = arrival_time_ms;

  // Exponential backoff filter.
  accumulated_delay_ += delta_ms;
  BWE_TEST_LOGGING_PLOT(1, "accumulated_delay_ms", arrival_time_ms,
                        accumulated_delay_);
  smoothed_delay_ = smoothing_coef_ * smoothed_delay_ +
                    (1 - smoothing_coef_) * accumulated_delay_;
  BWE_TEST_LOGGING_PLOT(1, "smoothed_delay_ms", arrival_time_ms,
                        smoothed_delay_);

  // Simple linear regression.
  delay_hist_.push_back(std::make_pair(
      static_cast<double>(arrival_time_ms - first_arrival_time_ms),
      smoothed_delay_));
  if (delay_hist_.size() > window_size_)
    delay_hist_.pop_front();
  if (delay_hist_.size() == window_size_) {
    // Only update trendline_ if it is possible to fit a line to the data.
    trendline_ = LinearFitSlope(delay_hist_).value_or(trendline_);
  }

  BWE_TEST_LOGGING_PLOT(1, "trendline_slope", arrival_time_ms, trendline_);
}

}  // namespace webrtc
