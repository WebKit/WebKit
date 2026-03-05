/*
 *  Copyright 2025 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/scream/delay_based_congestion_control.h"

#include <algorithm>

#include "api/transport/network_types.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "modules/congestion_controller/scream/scream_v2_parameters.h"

namespace webrtc {

DelayBasedCongestionControl::DelayBasedCongestionControl(
    ScreamV2Parameters params)
    : params_(params),
      base_delay_history_(params_.base_delay_window_length.Get()) {
  base_delay_history_.Insert(TimeDelta::PlusInfinity());
}

void DelayBasedCongestionControl::OnTransportPacketsFeedback(
    const TransportPacketsFeedback& msg) {
  if (msg.PacketsWithFeedback().empty()) {
    return;
  }
  last_smoothed_rtt_ = msg.smoothed_rtt;
  TimeDelta one_way_delay = TimeDelta::PlusInfinity();
  for (const PacketResult& packet : msg.SortedByReceiveTime()) {
    one_way_delay = packet.receive_time - packet.sent_packet.send_time;
    next_base_delay_ = std::min(next_base_delay_, one_way_delay);

    if (params_.use_all_packets_when_calculating_queue_delay.Get()) {
      UpdateQueueDelayAverage(one_way_delay);
    }
  }

  if (!params_.use_all_packets_when_calculating_queue_delay.Get() &&
      (msg.feedback_time - last_update_qdelay_avg_time_) >=
          std::min(params_.virtual_rtt.Get(), msg.smoothed_rtt)) {
    last_update_qdelay_avg_time_ = msg.feedback_time;
    UpdateQueueDelayAverage(one_way_delay);
  }

  if (msg.feedback_time - last_base_delay_update_ >=
      params_.base_delay_history_update_interval.Get()) {
    base_delay_history_.Insert(next_base_delay_);
    last_base_delay_update_ = msg.feedback_time;
    next_base_delay_ = TimeDelta::PlusInfinity();
  }
}

void DelayBasedCongestionControl::UpdateQueueDelayAverage(
    TimeDelta one_way_delay) {
  TimeDelta current_qdelay =
      one_way_delay - std::min(next_base_delay_, base_delay_history_.GetMin());
  // `queue_delay_avg_` is updated with a slow attack,fast decay EWMA
  // filter.
  if (current_qdelay < queue_delay_avg_) {
    queue_delay_avg_ = current_qdelay;
  } else {
    queue_delay_avg_ =
        params_.queue_delay_avg_g.Get() * current_qdelay +
        (1.0 - params_.queue_delay_avg_g.Get()) * queue_delay_avg_;
  }
}

bool DelayBasedCongestionControl::IsQueueDelayDetected() const {
  return queue_delay_avg_ > params_.queue_delay_target.Get() *
                                params_.queue_delay_increased_threshold.Get();
}

bool DelayBasedCongestionControl::ShouldReduceReferenceWindow() const {
  return (queue_delay_avg_ > params_.queue_delay_target.Get() *
                                 params_.queue_delay_threshold.Get());
}

DataSize DelayBasedCongestionControl::UpdateReferenceWindow(
    DataSize ref_window,
    double ref_window_mss_ratio,
    double virtual_alpha_lim) const {
  // `min_delay_based_bwe_`put a lower bound on the reference window.
  DataSize min_allowed_reference_window =
      min_delay_based_bwe_ * last_smoothed_rtt_;

  if (ref_window < min_allowed_reference_window) {
    return min_allowed_reference_window;
  }
  double l4s_alpha_v = std::clamp(
      2.0 * virtual_alpha_lim *
          (queue_delay_avg_ - params_.queue_delay_target.Get() *
                                  params_.queue_delay_threshold.Get()) /
          (params_.queue_delay_target.Get() *
           params_.queue_delay_threshold.Get()),
      0.0, 1.0);
  double backoff = l4s_alpha_v * params_.queue_delay_threshold.Get();
  backoff /= std::max(1.0, last_smoothed_rtt_ / params_.virtual_rtt);
  backoff *= std::max(0.5, 1.0 - ref_window_mss_ratio);

  return std::max(min_allowed_reference_window, (1 - backoff) * ref_window);
}

}  // namespace webrtc
