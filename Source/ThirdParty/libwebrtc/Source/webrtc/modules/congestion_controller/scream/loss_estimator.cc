/*
 *  Copyright 2026 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/congestion_controller/scream/loss_estimator.h"

#include <algorithm>

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/congestion_controller/scream/scream_feedback.h"
#include "modules/congestion_controller/scream/scream_v2_parameters.h"

namespace webrtc {

LossEstimator::LossEstimator(const ScreamV2Parameters& params)
    : virtual_rtt_(params.virtual_rtt.Get()),
      rtts_with_loss_before_backoff_(
          params.rtts_with_loss_before_backoff.Get()),
      lossless_rtts_before_clear_(params.lossless_rtts_before_clear.Get()) {}

bool LossEstimator::Update(const ScreamFeedback& parsed, TimeDelta rtt) {
  const TimeDelta max_rtt = std::max(virtual_rtt_, rtt);
  if (parsed.feedback_time - last_loss_or_recovery_time_ > max_rtt) {
    // Reset the unrecovered lost packet count if no new losses or recoveries
    // were reported for a full RTT. This prevents old, permanently lost
    // packets (which will never be recovered) from keeping this counter
    // positive indefinitely.
    unrecovered_lost_packets_ = 0;
  }

  bool has_lost_packets = false;
  if (parsed.num_lost_packets > 0 || parsed.num_recovered_packets > 0) {
    last_loss_or_recovery_time_ = parsed.feedback_time;
  }

  if (parsed.num_recovered_packets > 0) {
    unrecovered_lost_packets_ =
        std::max(0, unrecovered_lost_packets_ - parsed.num_recovered_packets);
    if (unrecovered_lost_packets_ == 0) {
      congestion_level_ = 0.0;
      loss_event_this_rtt_ = false;
    }
  }

  if (parsed.num_lost_packets > 0) {
    has_lost_packets = true;
    unrecovered_lost_packets_ += parsed.num_lost_packets;
    loss_event_this_rtt_ = true;
  }

  if (parsed.feedback_time - last_rtt_update_time_ >= max_rtt) {
    last_rtt_update_time_ = parsed.feedback_time;
    if (loss_event_this_rtt_) {
      // Asymmetric step filter: increment by 1.0 /
      // rtts_with_loss_before_backoff_ on a [0.0, 1.0] normalized scale. This
      // takes exactly rtts_with_loss_before_backoff_ consecutive RTTs with loss
      // to trigger backoff (reaching the 1.0 threshold).
      congestion_level_ = std::min(
          1.0, congestion_level_ + 1.0 / rtts_with_loss_before_backoff_);
    } else {
      // Asymmetric step filter: decrement by 1.0 / lossless_rtts_before_clear_
      // on a [0.0, 1.0] normalized scale.
      // Assuming ~50 packets are transmitted per RTT, a 1% uniform random
      // packet loss rate results in a ~40% probability of at least one loss
      // event per RTT. Under this noise profile (40% loss RTTs, 60% lossless
      // RTTs), the expected drift per RTT is strongly negative:
      // 0.4 * (+1.0/3) + 0.6 * (-1.0/2) = -0.167 (or -0.5 on a [0, 3] scale).
      // This completely filters out spurious uniform random losses, while
      // preserving memory of recent congestion after one lossless RTT (drops
      // to 0.5, allowing fast 2-RTT back-to-back reaction).
      congestion_level_ =
          std::max(0.0, congestion_level_ - 1.0 / lossless_rtts_before_clear_);
    }
    loss_event_this_rtt_ = false;
  }
  return has_lost_packets;
}

}  // namespace webrtc
