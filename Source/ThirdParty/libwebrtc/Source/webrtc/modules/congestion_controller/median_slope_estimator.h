/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_MODULES_CONGESTION_CONTROLLER_MEDIAN_SLOPE_ESTIMATOR_H_
#define WEBRTC_MODULES_CONGESTION_CONTROLLER_MEDIAN_SLOPE_ESTIMATOR_H_

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <vector>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/numerics/percentile_filter.h"

namespace webrtc {

class MedianSlopeEstimator {
 public:
  // |window_size| is the number of points required to compute a trend line.
  // |threshold_gain| is used to scale the trendline slope for comparison to
  // the old threshold. Once the old estimator has been removed (or the
  // thresholds been merged into the estimators), we can just set the
  // threshold instead of setting a gain.
  MedianSlopeEstimator(size_t window_size, double threshold_gain);
  ~MedianSlopeEstimator();

  // Update the estimator with a new sample. The deltas should represent deltas
  // between timestamp groups as defined by the InterArrival class.
  void Update(double recv_delta_ms,
              double send_delta_ms,
              int64_t arrival_time_ms);

  // Returns the estimated trend k multiplied by some gain.
  // 0 < k < 1   ->  the delay increases, queues are filling up
  //   k == 0    ->  the delay does not change
  //   k <  0    ->  the delay decreases, queues are being emptied
  double trendline_slope() const { return trendline_ * threshold_gain_; }

  // Returns the number of deltas which the current estimator state is based on.
  unsigned int num_of_deltas() const { return num_of_deltas_; }

 private:
  struct DelayInfo {
    DelayInfo(int64_t time, double delay, size_t slope_count)
        : time(time), delay(delay) {
      slopes.reserve(slope_count);
    }
    int64_t time;
    double delay;
    std::vector<double> slopes;
  };
  // Parameters.
  const size_t window_size_;
  const double threshold_gain_;
  // Used by the existing threshold.
  unsigned int num_of_deltas_;
  // Theil-Sen robust line fitting
  double accumulated_delay_;
  std::deque<DelayInfo> delay_hist_;
  PercentileFilter<double> median_filter_;
  double trendline_;

  RTC_DISALLOW_COPY_AND_ASSIGN(MedianSlopeEstimator);
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_CONGESTION_CONTROLLER_MEDIAN_SLOPE_ESTIMATOR_H_
