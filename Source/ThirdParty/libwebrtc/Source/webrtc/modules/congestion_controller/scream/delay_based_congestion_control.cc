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
#include <vector>

#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/congestion_controller/scream/scream_v2_parameters.h"
#include "rtc_base/checks.h"

namespace webrtc {

DelayBasedCongestionControl::DelayBasedCongestionControl(
    ScreamV2Parameters params)
    : params_(params),
      base_delay_history_(params_.base_delay_window_length.Get()) {
  ResetQueueDelay();
}

void DelayBasedCongestionControl::OnTransportPacketsFeedback(
    const TransportPacketsFeedback& msg) {
  if (msg.PacketsWithFeedback().empty()) {
    return;
  }
  std::vector<PacketResult> received_packets = msg.SortedByReceiveTime();
  if (received_packets.empty()) {
    return;
  }
  TimeDelta one_way_delay_sum;
  TimeDelta min_one_way_delay = TimeDelta::PlusInfinity();

  for (const PacketResult& packet : received_packets) {
    TimeDelta one_way_delay =
        packet.receive_time - packet.sent_packet.send_time;
    next_base_delay_ = std::min(next_base_delay_, one_way_delay);
    one_way_delay_sum += one_way_delay;
    min_one_way_delay = std::min(min_one_way_delay, one_way_delay);
  }
  // `arrival_time_offset` is null if TWCC is used. We assume feedback was sent
  // when the last sent packet was received.
  TimeDelta rtt_sample = std::max(
      msg.feedback_time - received_packets.back().sent_packet.send_time -
          received_packets.back().arrival_time_offset.value_or(
              TimeDelta::Zero()),
      TimeDelta::Zero());
  UpdateSmoothedRtt(rtt_sample);

  TimeDelta min_queue_delay = min_one_way_delay - min_base_delay();
  if (min_queue_delay > params_.queue_delay_drain_threshold.Get()) {
    if (min_queue_delay_above_threshold_start_.IsInfinite()) {
      min_queue_delay_above_threshold_start_ = msg.feedback_time;
    }
  } else {
    min_queue_delay_above_threshold_start_ = Timestamp::MinusInfinity();
  }
  UpdateQueueDelayAverage(one_way_delay_sum /
                          static_cast<int>(received_packets.size()));

  if (msg.feedback_time - last_base_delay_update_ >=
      params_.base_delay_history_update_interval.Get()) {
    base_delay_history_.Insert(next_base_delay_);
    last_base_delay_update_ = msg.feedback_time;
    next_base_delay_ = TimeDelta::PlusInfinity();
  }
}

void DelayBasedCongestionControl::UpdateQueueDelayAverage(
    TimeDelta one_way_delay) {
  TimeDelta current_qdelay = one_way_delay - min_base_delay();

  // `queue_delay_avg_` is updated with a slow attack,fast decay EWMA
  // filter.
  if (current_qdelay < queue_delay_avg_) {
    queue_delay_avg_ = current_qdelay;
  } else {
    queue_delay_avg_ =
        params_.queue_delay_avg_g.Get() * current_qdelay +
        (1.0 - params_.queue_delay_avg_g.Get()) * queue_delay_avg_;
  }

  queue_delay_dev_norm_ =
      (1.0 - params_.queue_delay_dev_avg_g.Get()) * queue_delay_dev_norm_ +
      params_.queue_delay_dev_avg_g.Get() *
          std::clamp((current_qdelay -
                      params_.queue_delay_dev_normalization.Get() / 4) /
                         params_.queue_delay_dev_normalization.Get(),
                     0.0, 0.2);
}

void DelayBasedCongestionControl::UpdateSmoothedRtt(TimeDelta rtt_sample) {
  RTC_DCHECK(rtt_sample >= TimeDelta::Zero());
  if (last_smoothed_rtt_.IsZero()) {
    last_smoothed_rtt_ = rtt_sample;
  } else {
    double g = params_.smoothed_rtt_avg_g_up.Get();
    if (rtt_sample < last_smoothed_rtt_) {
      g = params_.smoothed_rtt_avg_g_down.Get();
    }
    last_smoothed_rtt_ = rtt_sample * g + last_smoothed_rtt_ * (1.0 - g);
  }
}

void DelayBasedCongestionControl::ResetQueueDelay() {
  last_base_delay_update_ = Timestamp::MinusInfinity();
  next_base_delay_ = TimeDelta::PlusInfinity();
  base_delay_history_.Reset();
  // Insert a start value to ensure GetMin returns a sensible value when empty.
  base_delay_history_.Insert(TimeDelta::PlusInfinity());

  min_queue_delay_above_threshold_start_ = Timestamp::MinusInfinity();
  last_update_qdelay_avg_time_ = Timestamp::MinusInfinity();
  queue_delay_avg_ = TimeDelta::PlusInfinity();
  queue_delay_dev_norm_ = 0.0;
}

DataSize DelayBasedCongestionControl::UpdateReferenceWindow(
    DataSize ref_window,
    double ref_window_mss_ratio) const {
  // `min_delay_based_bwe_`put a lower bound on the reference window.
  DataSize min_allowed_reference_window =
      min_delay_based_bwe_ * last_smoothed_rtt_;

  if (ref_window < min_allowed_reference_window) {
    return min_allowed_reference_window;
  }

  double backoff = l4s_alpha_v() / 2.0;  // Reduce by 50% if l4s_alpha_v = 1.0;
  backoff /= std::max(1.0, last_smoothed_rtt_ / params_.virtual_rtt);
  backoff *= std::max(0.5, 1.0 - ref_window_mss_ratio);

  return std::max(min_allowed_reference_window, (1 - backoff) * ref_window);
}

double DelayBasedCongestionControl::l4s_alpha_v() const {
  // 4.2.2.1
  double l4s_alpha_v =
      (queue_delay_avg_ - params_.queue_delay_target.Get() / 2) /
      (params_.queue_delay_target.Get() / 2);
  return std::clamp(l4s_alpha_v, 0.0, 1.0);
}

}  // namespace webrtc
