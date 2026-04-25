/*
 *  Copyright 2025 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_CONGESTION_CONTROLLER_SCREAM_DELAY_BASED_CONGESTION_CONTROL_H_
#define MODULES_CONGESTION_CONTROLLER_SCREAM_DELAY_BASED_CONGESTION_CONTROL_H_

#include <algorithm>

#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/congestion_controller/scream/scream_v2_parameters.h"
#include "rtc_base/numerics/windowed_min_filter.h"

namespace webrtc {

// Implements logic necessary for backing off if queue delay increase as
// proposed in
// https://datatracker.ietf.org/doc/draft-johansson-ccwg-rfc8298bis-screamv2/.
// The functionality is split out into a separate class since it may be possible
// to replace this logic with the trend line filter from Goog CC or a ML model.
class DelayBasedCongestionControl {
 public:
  explicit DelayBasedCongestionControl(ScreamV2Parameters params);

  void OnTransportPacketsFeedback(const TransportPacketsFeedback& msg);

  // Set a limit on how much the reference window can be reduced due to
  // increased delay.
  void SetMinDelayBasedBwe(DataRate min_delay_based_bwe) {
    min_delay_based_bwe_ = min_delay_based_bwe;
  }

  // Returns true if queue delay is detected and above a threshold.
  bool IsQueueDelayDetected() const {
    return queue_delay_avg_.IsFinite() &&
           queue_delay_avg_ > params_.queue_delay_target.Get() / 2;
  }

  DataSize UpdateReferenceWindow(DataSize rew_window,
                                 double ref_window_mss_ratio) const;

  // Returns false if the minimum queue delay has been above the drain threshold
  // for a prolonged time. This can happen if minimum possible latency has
  // increased, or queues has been filled for a longer period of time without
  // being drained.
  bool IsQueueDrainedInTime(Timestamp now) const {
    return min_queue_delay_above_threshold_start_.IsInfinite() ||
           (now - min_queue_delay_above_threshold_start_ <
            params_.queue_delay_drain_period.Get());
  }

  // Resets queue delay estimates to start values.
  void ResetQueueDelay();

  TimeDelta queue_delay() const { return queue_delay_avg_; }

  double queue_delay_dev_norm() const { return queue_delay_dev_norm_; }

  // Scales the reference window increase/decrease due to increased delay if L4S
  // does not appear to be used. See 4.2.2.1.
  double ref_window_scale_factor_due_to_increased_delay() const {
    return std::clamp(
        1 - queue_delay_avg_ / (params_.queue_delay_target.Get() / 4), 0.1,
        1.0);
  }

  // Scales the reference window increase/decrease due to delay variation.
  // See 4.2.2.1 and 4.2.2.2. The scale factor is 0.0 if the delay variation
  // is higher than the threshold, and decreases linearly to 1.0 if the delay
  // variation is zero.
  double ref_window_scale_factor_due_to_delay_variation(
      double ref_window_mss_ratio) const {
    // 4.2.1.4.
    double queue_delay_dev_norm_threshold =
        std::max(0.05, 0.1 * (1.0 - ref_window_mss_ratio / 0.1));
    return std::clamp((queue_delay_dev_norm_threshold - queue_delay_dev_norm_) /
                          queue_delay_dev_norm_threshold,
                      0.0, 1.0);
  }

  TimeDelta rtt() const { return last_smoothed_rtt_; }

  double l4s_alpha_v() const;

 private:
  TimeDelta min_base_delay() const {
    return std::min(next_base_delay_, base_delay_history_.GetMin());
  }
  void UpdateSmoothedRtt(TimeDelta rtt_sample);
  void UpdateQueueDelayAverage(TimeDelta one_way_delay);

  const ScreamV2Parameters params_;

  DataRate min_delay_based_bwe_;

  // For computing min one way delay and compensate for clock drift.
  // Based on https://datatracker.ietf.org/doc/html/rfc6817
  Timestamp last_base_delay_update_ = Timestamp::MinusInfinity();
  TimeDelta next_base_delay_ = TimeDelta::PlusInfinity();
  WindowedMinFilter<TimeDelta> base_delay_history_;

  Timestamp min_queue_delay_above_threshold_start_ = Timestamp::MinusInfinity();
  TimeDelta last_smoothed_rtt_ = TimeDelta::Zero();
  Timestamp last_update_qdelay_avg_time_ = Timestamp::MinusInfinity();
  TimeDelta queue_delay_avg_ = TimeDelta::PlusInfinity();
  double queue_delay_dev_norm_ = 0.0;
};

}  // namespace webrtc
#endif  // MODULES_CONGESTION_CONTROLLER_SCREAM_DELAY_BASED_CONGESTION_CONTROL_H_
