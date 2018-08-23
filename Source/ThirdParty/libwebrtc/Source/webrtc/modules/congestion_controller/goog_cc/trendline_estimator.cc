/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/goog_cc/trendline_estimator.h"

#include <math.h>

#include <algorithm>

#include "absl/types/optional.h"
#include "modules/remote_bitrate_estimator/test/bwe_test_logging.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_minmax.h"

namespace webrtc {

namespace {
absl::optional<double> LinearFitSlope(
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
    return absl::nullopt;
  return numerator / denominator;
}

constexpr double kMaxAdaptOffsetMs = 15.0;
constexpr double kOverUsingTimeThreshold = 10;
constexpr int kMinNumDeltas = 60;

}  // namespace

enum { kDeltaCounterMax = 1000 };

TrendlineEstimator::TrendlineEstimator(size_t window_size,
                                       double smoothing_coef,
                                       double threshold_gain)
    : window_size_(window_size),
      smoothing_coef_(smoothing_coef),
      threshold_gain_(threshold_gain),
      num_of_deltas_(0),
      first_arrival_time_ms_(-1),
      accumulated_delay_(0),
      smoothed_delay_(0),
      delay_hist_(),
      trendline_(0),
      k_up_(0.0087),
      k_down_(0.039),
      overusing_time_threshold_(kOverUsingTimeThreshold),
      threshold_(12.5),
      last_update_ms_(-1),
      prev_offset_(0.0),
      time_over_using_(-1),
      overuse_counter_(0),
      hypothesis_(BandwidthUsage::kBwNormal) {}

TrendlineEstimator::~TrendlineEstimator() {}

void TrendlineEstimator::Update(double recv_delta_ms,
                                double send_delta_ms,
                                int64_t arrival_time_ms) {
  const double delta_ms = recv_delta_ms - send_delta_ms;
  ++num_of_deltas_;
  if (num_of_deltas_ > kDeltaCounterMax)
    num_of_deltas_ = kDeltaCounterMax;
  if (first_arrival_time_ms_ == -1)
    first_arrival_time_ms_ = arrival_time_ms;

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
      static_cast<double>(arrival_time_ms - first_arrival_time_ms_),
      smoothed_delay_));
  if (delay_hist_.size() > window_size_)
    delay_hist_.pop_front();
  if (delay_hist_.size() == window_size_) {
    // Only update trendline_ if it is possible to fit a line to the data.
    trendline_ = LinearFitSlope(delay_hist_).value_or(trendline_);
  }

  BWE_TEST_LOGGING_PLOT(1, "trendline_slope", arrival_time_ms, trendline_);

  Detect(trendline_slope(), send_delta_ms, num_of_deltas(), arrival_time_ms);
}

BandwidthUsage TrendlineEstimator::State() const {
  return hypothesis_;
}

void TrendlineEstimator::Detect(double offset,
                                double ts_delta,
                                int num_of_deltas,
                                int64_t now_ms) {
  if (num_of_deltas < 2) {
    hypothesis_ = BandwidthUsage::kBwNormal;
    return;
  }
  const double T = std::min(num_of_deltas, kMinNumDeltas) * offset;
  BWE_TEST_LOGGING_PLOT(1, "T", now_ms, T);
  BWE_TEST_LOGGING_PLOT(1, "threshold", now_ms, threshold_);
  if (T > threshold_) {
    if (time_over_using_ == -1) {
      // Initialize the timer. Assume that we've been
      // over-using half of the time since the previous
      // sample.
      time_over_using_ = ts_delta / 2;
    } else {
      // Increment timer
      time_over_using_ += ts_delta;
    }
    overuse_counter_++;
    if (time_over_using_ > overusing_time_threshold_ && overuse_counter_ > 1) {
      if (offset >= prev_offset_) {
        time_over_using_ = 0;
        overuse_counter_ = 0;
        hypothesis_ = BandwidthUsage::kBwOverusing;
      }
    }
  } else if (T < -threshold_) {
    time_over_using_ = -1;
    overuse_counter_ = 0;
    hypothesis_ = BandwidthUsage::kBwUnderusing;
  } else {
    time_over_using_ = -1;
    overuse_counter_ = 0;
    hypothesis_ = BandwidthUsage::kBwNormal;
  }
  prev_offset_ = offset;

  UpdateThreshold(T, now_ms);
}

void TrendlineEstimator::UpdateThreshold(double modified_offset,
                                         int64_t now_ms) {
  if (last_update_ms_ == -1)
    last_update_ms_ = now_ms;

  if (fabs(modified_offset) > threshold_ + kMaxAdaptOffsetMs) {
    // Avoid adapting the threshold to big latency spikes, caused e.g.,
    // by a sudden capacity drop.
    last_update_ms_ = now_ms;
    return;
  }

  const double k = fabs(modified_offset) < threshold_ ? k_down_ : k_up_;
  const int64_t kMaxTimeDeltaMs = 100;
  int64_t time_delta_ms = std::min(now_ms - last_update_ms_, kMaxTimeDeltaMs);
  threshold_ += k * (fabs(modified_offset) - threshold_) * time_delta_ms;
  threshold_ = rtc::SafeClamp(threshold_, 6.f, 600.f);
  last_update_ms_ = now_ms;
}

}  // namespace webrtc
