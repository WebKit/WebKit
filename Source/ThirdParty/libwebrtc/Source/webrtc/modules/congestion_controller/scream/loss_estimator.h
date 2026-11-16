/*
 *  Copyright 2026 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_CONGESTION_CONTROLLER_SCREAM_LOSS_ESTIMATOR_H_
#define MODULES_CONGESTION_CONTROLLER_SCREAM_LOSS_ESTIMATOR_H_

#include <stdint.h>

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/congestion_controller/scream/scream_feedback.h"
#include "modules/congestion_controller/scream/scream_v2_parameters.h"

namespace webrtc {

// Estimates the short-term congestion level of the connection using a biased
// asymmetric step filter (counting a weighted sum of RTTs with loss minus
// RTTs without loss).
//
// The congestion level is updated on RTT boundaries. If one or more packets are
// reported lost during an RTT interval, the filter treats the interval as
// a loss event.
//
// To handle packet reordering robustly, the estimator maintains a count of
// outstanding unrecovered lost packets. When a packet is reported lost for the
// first time, the outstanding count increments. If a subsequently received
// feedback report indicates that an out-of-order packet has been recovered,
// the count decrements. If the unrecovered count reaches zero, the connection
// is considered loss-free, and the congestion level is reset to zero.
//
// Asymmetric Step Filter Motivation:
// A simple sliding window or traditional EWMA is highly sensitive to spurious
// uniform losses (e.g. 1% uniform random loss in wireless links). Because we
// transmit many packets per RTT (e.g. 50), a 1% packet loss scales up to a
// ~40% RTT loss event probability.
//
// This filter solves this by using an asymmetric step filter (leaky bucket)
// scaled between [0.0, 1.0] (with step-up of +1/3 on loss, step-down of -0.5 on
// no loss). This guarantees that the expected drift per RTT under 1% uniform
// loss is strongly negative (-0.5), keeping the congestion level at ~0.0.
// Meanwhile, it takes exactly 3 consecutive loss RTTs to climb to 1.0 to react.
class LossEstimator {
 public:
  explicit LossEstimator(const ScreamV2Parameters& params);
  ~LossEstimator() = default;

  // Updates the congestion level estimate. Returns true if a loss has been
  // detected in this feedback message, false otherwise.
  bool Update(const ScreamFeedback& parsed, TimeDelta rtt);

  // Returns the congestion level, which is a value between 0.0 (loss-free)
  // and 1.0 (congested).
  double congestion_level() const { return congestion_level_; }

  // Returns true if the short-term congestion level has reached the threshold.
  bool congested() const { return congestion_level_ >= 0.99; }

 private:
  const TimeDelta virtual_rtt_;
  const int rtts_with_loss_before_backoff_;
  const int lossless_rtts_before_clear_;

  // `loss_event_this_rtt_` tracks whether any packet loss was detected
  // specifically within the current RTT interval. Consumed and reset on RTT
  // boundaries. Needed separately from `unrecovered_lost_packets_` because
  // unrecovered counts persist across RTT updates to track delayed reordered
  // recoveries.
  bool loss_event_this_rtt_ = false;
  double congestion_level_ = 0.0;
  int unrecovered_lost_packets_ = 0;
  Timestamp last_loss_or_recovery_time_ = Timestamp::MinusInfinity();
  Timestamp last_rtt_update_time_ = Timestamp::MinusInfinity();
};

}  // namespace webrtc
#endif  // MODULES_CONGESTION_CONTROLLER_SCREAM_LOSS_ESTIMATOR_H_
