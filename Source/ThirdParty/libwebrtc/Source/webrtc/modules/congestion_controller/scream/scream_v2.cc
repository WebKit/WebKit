/*
 *  Copyright 2025 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/scream/scream_v2.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "api/environment/environment.h"
#include "api/transport/ecn_marking.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "logging/rtc_event_log/events/rtc_event_bwe_update_scream.h"
#include "rtc_base/checks.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/logging.h"

namespace webrtc {

namespace {

// Returns the size of packets that have been acked (including lost
// packets) and not marked as CE.
DataSize DataUnitsAckedAndNotMarked(const TransportPacketsFeedback& msg) {
  DataSize acked_not_marked = DataSize::Zero();
  for (const PacketResult& packet : msg.PacketsWithFeedback()) {
    if (packet.ecn != EcnMarking::kCe) {
      acked_not_marked += packet.sent_packet.size;
    }
  }
  return acked_not_marked;
}

bool HasCeMarking(const TransportPacketsFeedback& msg) {
  for (const auto& packet : msg.PacketsWithFeedback()) {
    if (packet.ecn == EcnMarking::kCe) {
      return true;
    }
  }
  return false;
}

bool HasLostPackets(const TransportPacketsFeedback& msg) {
  for (const auto& packet : msg.PacketsWithFeedback()) {
    if (!packet.IsReceived()) {
      return true;
    }
  }
  return false;
}

}  // namespace

ScreamV2::ScreamV2(const Environment& env)
    : env_(env),
      params_(env_.field_trials()),
      ref_window_(params_.min_ref_window.Get()),
      delay_based_congestion_control_(params_) {}

void ScreamV2::SetTargetBitrateConstraints(DataRate min, DataRate max) {
  RTC_DCHECK_GE(max, min);
  min_target_bitrate_ = min;
  max_target_bitrate_ = max;
  RTC_LOG_F(LS_INFO) << "min_target_bitrate_=" << min_target_bitrate_
                     << " max_target_bitrate_=" << max_target_bitrate_;
}

DataRate ScreamV2::OnTransportPacketsFeedback(
    const TransportPacketsFeedback& msg) {
  delay_based_congestion_control_.OnTransportPacketsFeedback(msg);
  UpdateL4SAlpha(msg);
  UpdateRefWindowAndTargetRate(msg);
  env_.event_log().Log(std::make_unique<RtcEventBweUpdateScream>(
      ref_window_, msg.data_in_flight, target_rate_, msg.smoothed_rtt,
      delay_based_congestion_control_.queue_delay(),
      /*l4s_marked_permille*/ l4s_alpha_ * 1000));
  return target_rate_;
}

void ScreamV2::UpdateL4SAlpha(const TransportPacketsFeedback& msg) {
  // 4.2.1.3.
  const std::vector<PacketResult> received_packets = msg.ReceivedWithSendInfo();
  if (received_packets.empty()) {
    return;
  }
  double data_units_marked = 0;
  for (const PacketResult& packet : received_packets) {
    if (packet.ecn == EcnMarking::kCe) {
      ++data_units_marked;
    }
  }

  double fraction_marked = data_units_marked / received_packets.size();
  l4s_alpha_ = params_.l4s_avg_g.Get() * fraction_marked +
               (1.0 - params_.l4s_avg_g.Get()) * l4s_alpha_;
}

void ScreamV2::UpdateRefWindowAndTargetRate(
    const TransportPacketsFeedback& msg) {
  max_data_in_flight_this_rtt_ =
      std::max(max_data_in_flight_this_rtt_, msg.data_in_flight);

  // Avoid division by zero.
  const TimeDelta non_zero_smoothed_rtt =
      std::max(msg.smoothed_rtt, TimeDelta::Millis(1));

  bool is_ce = HasCeMarking(msg);
  bool is_loss = HasLostPackets(msg);
  bool is_virtual_ce = false;
  double virtual_alpha_lim =
      ((2 * params_.max_segment_size.Get()) / non_zero_smoothed_rtt) /
      target_rate_;
  if (l4s_alpha_ < virtual_alpha_lim &&
      delay_based_congestion_control_.ShouldReduceReferenceWindow()) {
    // L4S does not seem to be enabled and queue has grown.
    is_virtual_ce = true;
  }

  DataSize previous_ref_window = ref_window_;

  if ((is_virtual_ce || is_ce || is_loss) &&
      msg.feedback_time - last_reaction_to_congestion_time_ >=
          std::min(msg.smoothed_rtt, params_.virtual_rtt.Get())) {
    last_reaction_to_congestion_time_ = msg.feedback_time;
    if (is_loss) {  // Back off due to loss
      ref_window_ = ref_window_ * params_.beta_loss.Get() /
                    std::max(1.0, msg.smoothed_rtt / params_.virtual_rtt);
    }
    if (is_ce) {  // Backoff due to ECN-CE marking
      double backoff = l4s_alpha_ / 2.0;
      // Scale down backoff when RTT is high as several backoff events occur per
      // RTT
      backoff /= std::max(1.0, msg.smoothed_rtt / params_.virtual_rtt);
      //  Increase stability for very small ref_wnd
      backoff *= std::max(0.5, 1.0 - ref_window_mss_ratio());

      if (!delay_based_congestion_control_.IsQueueDelayDetected()) {
        // Scale down backoff if close to the last known max reference window
        // This is complemented with a scale down of the reference window
        // increase
        backoff *=
            std::max(0.25, ref_window_scale_factor_close_to_ref_window_i());
      }

      if (msg.feedback_time - last_reaction_to_congestion_time_ >
          params_.number_of_rtts_between_reset_ref_window_i_on_congestion
                  .Get() *
              std::max(params_.virtual_rtt.Get(), msg.smoothed_rtt)) {
        // A long time(> 100 RTTs) since last congested because
        // link throughput exceeds max video bitrate. (or first congestion)
        // There is a certain risk that ref_wnd has increased way above
        // bytes in flight, so we reduce it here to get it better on
        // track and thus the congestion episode is shortened
        ref_window_ = std::clamp(max_data_in_flight_prev_rtt_,
                                 params_.min_ref_window.Get(), ref_window_);
        // Also, we back off a little extra if needed because alpha is quite
        // likely very low  This can in some cases be an over - reaction but as
        // this function should kick in relatively seldom it should not be a too
        // big concern
        backoff = std::max(backoff, 0.25);
        // In addition, bump up l4sAlpha to a more credible value
        // This may over react but it is better than
        // excessive queue delay
        l4s_alpha_ = 0.25;
      }
      ref_window_ = (1.0 - backoff) * ref_window_;
    }  // is_ce
    if (is_virtual_ce) {  // Back off due to delay
      ref_window_ = delay_based_congestion_control_.UpdateReferenceWindow(
          ref_window_, ref_window_mss_ratio(), virtual_alpha_lim);
    }

    if (allow_ref_window_i_update_) {
      ref_window_i_ = ref_window_;
      allow_ref_window_i_update_ = false;
    }
  }

  const TimeDelta max_of_virtual_and_smothed_rtt =
      std::max(params_.virtual_rtt.Get(), msg.smoothed_rtt);

  // Increase ref_window.
  // 4.2.2.2.  Reference Window Increase
  if ((!is_ce && !is_loss && !is_virtual_ce) ||
      last_reaction_to_congestion_time_ == msg.feedback_time) {
    // Allow increase if no event has occurred, or we are at the same time is
    // backing off.
    // Just because there is a CE event, does not mean we send too much. At
    // rates close to the capacity, it is quite likely that one packet is CE
    // marked in every feedback.
    DataSize increase =
        DataUnitsAckedAndNotMarked(msg) * ref_window_mss_ratio();

    // Limit increase for small RTTs
    if (msg.smoothed_rtt < params_.virtual_rtt.Get()) {
      double rtt_ratio = msg.smoothed_rtt / params_.virtual_rtt.Get();
      increase = increase * (rtt_ratio * rtt_ratio);
    }
    if (l4s_alpha_ < virtual_alpha_lim) {
      // Limit increase if delay is increased.
      increase = increase * delay_based_congestion_control_.scale_increase();
    }
    // Limit reference window increase when close to the last inflection
    // point.
    increase = increase *
               std::max(0.25, ref_window_scale_factor_close_to_ref_window_i());
    // Limit reference window increase when the reference window close to
    // max segment size.
    increase = increase * std::max(0.5, 1.0 - ref_window_mss_ratio());

    // Use lower multiplicative scale factor if congestion was detected
    // recently.
    double post_congestion_scale =
        std::clamp((msg.feedback_time - last_reaction_to_congestion_time_) /
                       (params_.post_congestion_delay_rtts.Get() *
                        max_of_virtual_and_smothed_rtt),
                   0.0, 1.0);
    double multiplicative_scale =
        1.0 + (ref_window_multiplicative_scale_factor() - 1.0) *
                  post_congestion_scale *
                  ref_window_scale_factor_close_to_ref_window_i();
    RTC_DCHECK_GE(multiplicative_scale, 1.0);
    increase = increase * multiplicative_scale;

    // Increase ref_window only if bytes in flight is large enough.
    // Quite a lot of slack is allowed here to avoid that bitrate locks to low
    // values. Increase is inhibited if max target bitrate is reached.
    DataSize max_allowed_ref_window =
        std::max(params_.max_segment_size.Get() +
                     std::max(max_data_in_flight_this_rtt_,
                              max_data_in_flight_prev_rtt_) *
                         params_.bytes_in_flight_head_room.Get(),
                 params_.min_ref_window.Get());

    if (ref_window_ < max_allowed_ref_window) {
      ref_window_ =
          std::clamp(ref_window_ + increase, params_.min_ref_window.Get(),
                     max_allowed_ref_window);
    }
  }

  double scale_target_rate = 1.0;
  if (delay_based_congestion_control_.IsQueueDelayDetected()) {
    // 4.4 Limit bitrate if data in flight is close to or
    // exceeds `ref_window_`. This helps to avoid large rate fluctuations and
    // variations in RTT.
    // Note that `delay_based_congestion_control_.IsQueueDelayDetected()`may use
    // a lower ratio between queue delay and target delay compared to the RFC.
    // With a higher ratio, RTT and target rate fluctuate more.
    double data_in_flight_ratio = msg.data_in_flight / ref_window_;
    if (data_in_flight_ratio > params_.data_in_flight_limit.Get()) {
      scale_target_rate /=
          std::min(params_.max_data_in_flight_limit_compensation.Get(),
                   data_in_flight_ratio / params_.data_in_flight_limit.Get());
    }
  }
  // Scale down target rate slightly when the reference window is very small
  // compared to MSS
  scale_target_rate =
      scale_target_rate *
      (1.0 - std::clamp(ref_window_mss_ratio() - 0.1, 0.0, 0.2));
  target_rate_ =
      std::clamp(scale_target_rate * (ref_window_ / non_zero_smoothed_rtt),
                 min_target_bitrate_, max_target_bitrate_);

  RTC_LOG_IF(LS_VERBOSE, previous_ref_window != ref_window_)
      << "ScreamV2: "
      << ", ref_window = " << ref_window_ << " ref_window_i_=" << ref_window_i_
      << ", change=" << ref_window_.bytes() - previous_ref_window.bytes()
      << " bytes "
      << ", l4s_alpha=" << l4s_alpha_
      << ", scale_target_rate=" << scale_target_rate << ", is_ce=" << is_ce
      << " is_virtual_ce=" << is_virtual_ce << " is_loss=" << is_loss
      << " smoothed_rtt=" << msg.smoothed_rtt.ms()
      << ", queue_delay=" << delay_based_congestion_control_.queue_delay().ms()
      << ", target_rate_=" << target_rate_.kbps();

  if (previous_ref_window < ref_window_) {
    // Allow setting a new `ref_window_i` if `ref_window_` increase.
    // It means that `ref_window_i` can increase if `rew_window` increase and
    // there is a congestion event.
    allow_ref_window_i_update_ = true;
  }
  if (msg.feedback_time - last_data_in_flight_update_ >=
      max_of_virtual_and_smothed_rtt) {
    last_data_in_flight_update_ = msg.feedback_time;
    max_data_in_flight_prev_rtt_ = max_data_in_flight_this_rtt_;
    max_data_in_flight_this_rtt_ = DataSize::Zero();
  }
}

}  // namespace webrtc
