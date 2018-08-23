/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_GOOG_CC_BITRATE_ESTIMATOR_H_
#define MODULES_CONGESTION_CONTROLLER_GOOG_CC_BITRATE_ESTIMATOR_H_

#include <vector>

#include "absl/types/optional.h"

namespace webrtc {

// Computes a bayesian estimate of the throughput given acks containing
// the arrival time and payload size. Samples which are far from the current
// estimate or are based on few packets are given a smaller weight, as they
// are considered to be more likely to have been caused by, e.g., delay spikes
// unrelated to congestion.
class BitrateEstimator {
 public:
  BitrateEstimator();
  virtual ~BitrateEstimator();
  virtual void Update(int64_t now_ms, int bytes);

  virtual absl::optional<uint32_t> bitrate_bps() const;

  virtual void ExpectFastRateChange();

 private:
  float UpdateWindow(int64_t now_ms, int bytes, int rate_window_ms);
  int sum_;
  int64_t initial_window_ms_;
  int64_t current_window_ms_;
  int64_t prev_time_ms_;
  float bitrate_estimate_;
  float bitrate_estimate_var_;
};

}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_GOOG_CC_BITRATE_ESTIMATOR_H_
