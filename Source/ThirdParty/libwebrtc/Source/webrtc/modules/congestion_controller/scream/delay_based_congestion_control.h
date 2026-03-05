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

  // Returns true if queue delay is detected, but it may be low and does not
  // necessarily mean reference window should be reduced. From 4.2.2.1.
  // (queue_qdelay >= queue_delay_target * 0.25)
  bool IsQueueDelayDetected() const;

  // Returns true if queue delay is detected and reference window should be
  // reduced. From 4.2.2.1. (queue_qdelay >= queue_delay_target*0.5)
  bool ShouldReduceReferenceWindow() const;

  DataSize UpdateReferenceWindow(DataSize rew_window,
                                 double ref_window_mss_ratio,
                                 double virtual_alpha_lim) const;

  double scale_increase() const {
    return std::clamp(1 - queue_delay_avg_ / (params_.queue_delay_target.Get() *
                                              params_.queue_delay_threshold),
                      0.1, 1.0);
  }

  // Public for testing and logging.
  TimeDelta queue_delay() const { return queue_delay_avg_; }

 private:
  void UpdateQueueDelayAverage(TimeDelta one_way_delay);

  const ScreamV2Parameters params_;

  DataRate min_delay_based_bwe_;

  // For computing min one way delay and compensate for clock drift.
  // Based on https://datatracker.ietf.org/doc/html/rfc6817
  Timestamp last_base_delay_update_ = Timestamp::MinusInfinity();
  TimeDelta next_base_delay_ = TimeDelta::PlusInfinity();
  WindowedMinFilter<TimeDelta> base_delay_history_;

  TimeDelta last_smoothed_rtt_ = TimeDelta::Zero();
  Timestamp last_update_qdelay_avg_time_ = Timestamp::MinusInfinity();
  TimeDelta queue_delay_avg_ = TimeDelta::PlusInfinity();
};

}  // namespace webrtc
#endif  // MODULES_CONGESTION_CONTROLLER_SCREAM_DELAY_BASED_CONGESTION_CONTROL_H_
