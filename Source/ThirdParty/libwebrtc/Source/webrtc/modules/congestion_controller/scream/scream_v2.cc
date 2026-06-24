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

#include "api/environment/environment.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "logging/rtc_event_log/events/rtc_event_bwe_update_scream.h"
#include "modules/congestion_controller/scream/delay_based_congestion_control.h"
#include "modules/congestion_controller/scream/scream_feedback.h"
#include "rtc_base/checks.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/logging.h"

namespace webrtc {



ScreamV2::ScreamV2(const Environment& env)
    : env_(env),
      params_(env_.field_trials()),
      ref_window_(params_.min_ref_window.Get()),
      loss_estimator_(params_),
      delay_based_congestion_control_(params_) {}

void ScreamV2::SetTargetBitrateConstraints(DataRate min,
                                           DataRate max,
                                           DataRate start) {
  RTC_DCHECK_GE(max, min);
  min_target_bitrate_ = min;
  max_target_bitrate_ = max;
  if (!first_feedback_processed_) {
    target_rate_ = start;
  }
  RTC_LOG_F(LS_VERBOSE) << "min_target_bitrate_=" << min_target_bitrate_
                        << " max_target_bitrate_=" << max_target_bitrate_
                        << " start_bitrate_=" << target_rate_;
}

void ScreamV2::OnPacketSent(DataSize data_in_flight) {
  max_data_in_flight_this_rtt_ =
      std::max(max_data_in_flight_this_rtt_, data_in_flight);
}

void ScreamV2::OnTransportPacketsFeedback(const TransportPacketsFeedback& msg) {
  ScreamFeedback feedback = ParseScreamFeedback(msg);
  if (feedback.num_received_packets == 0) {
    RTC_LOG(LS_INFO) << "No received packets in feedback, ignoring.";
    return;
  }
  max_data_in_flight_this_rtt_ =
      std::max(max_data_in_flight_this_rtt_, feedback.data_in_flight);

  if (params_.enable_alr.Get()) {
    is_application_limited_ = max_allowed_ref_window() < ref_window_;
  }

  delay_based_congestion_control_.Update(feedback, is_application_limited_);

  if (!is_application_limited_) {
    UpdateFeedbackHoldTime(feedback);
  }

  if (!first_feedback_processed_) {
    ref_window_ =
        std::max(params_.min_ref_window.Get(),
                 target_rate_ * (delay_based_congestion_control_.rtt() +
                                 feedback_hold_time_));
    RTC_LOG(LS_INFO) << "Initial RTT: "
                     << delay_based_congestion_control_.rtt().ms()
                     << " feedback_hold_time: " << feedback_hold_time_.ms()
                     << "ms, Start Bitrate: " << target_rate_.kbps() << "kbps"
                     << " ref_window_=" << ref_window_.bytes();
    first_feedback_processed_ = true;
  }

  UpdateL4SAlpha(feedback);
  UpdateRefWindow(feedback);
  UpdateTargetRate(feedback);
  env_.event_log().Log(std::make_unique<RtcEventBweUpdateScream>(
      ref_window_, feedback.data_in_flight, target_rate_,
      delay_based_congestion_control_.rtt(),
      delay_based_congestion_control_.queue_delay(),
      /*l4s_marked_permille*/ l4s_alpha_ * 1000));

  if (feedback.feedback_time - last_data_in_flight_update_ >=
      std::max(params_.virtual_rtt.Get(),
               delay_based_congestion_control_.rtt())) {
    last_data_in_flight_update_ = feedback.feedback_time;
    max_data_in_flight_prev_rtt_ = max_data_in_flight_this_rtt_;
    max_data_in_flight_this_rtt_ = DataSize::Zero();
  }
}

void ScreamV2::UpdateL4SAlpha(const ScreamFeedback& parsed) {
  // 4.2.1.3.
  if (parsed.num_received_packets == 0) {
    return;
  }

  double fraction_marked = static_cast<double>(parsed.num_ce_marked_packets) /
                           parsed.num_received_packets;
  // Fast attack slow decay EWMA filter.
  if (fraction_marked > l4s_alpha_) {
    l4s_alpha_ = std::min(params_.l4s_avg_g_up.Get() * fraction_marked +
                              (1.0 - params_.l4s_avg_g_up.Get()) * l4s_alpha_,
                          1.0);
  } else {
    l4s_alpha_ = (1.0 - params_.l4s_avg_g_down.Get()) * l4s_alpha_;
  }
}
void ScreamV2::UpdateRefWindow(const ScreamFeedback& parsed) {
  bool is_ce = parsed.num_ce_marked_packets > 0;
  bool is_loss =
      loss_estimator_.Update(parsed, delay_based_congestion_control_.rtt());
  bool is_virtual_ce = false;
  if (delay_based_congestion_control_.IsQueueDelayDetected()) {
    is_virtual_ce = true;
  }
  if (is_loss) {
    if (!loss_estimator_.congested() &&
        !delay_based_congestion_control_.IsQueueDelayDetected()) {
      is_loss = false;
    }
  }

  DataSize previous_ref_window = ref_window_;

  if ((is_virtual_ce || is_ce || is_loss) &&
      parsed.feedback_time - last_reaction_to_congestion_time_ >=
          std::min(delay_based_congestion_control_.rtt(),
                   params_.virtual_rtt.Get())) {
    last_reaction_to_congestion_time_ = parsed.feedback_time;
    double backoff = 0.0;
    if (is_loss) {  // Back off due to loss
      backoff = 1.0 - params_.beta_loss.Get();
    } else if (is_ce) {  // Backoff due to ECN-CE marking
      backoff = l4s_alpha_ / 2.0;
      // Scale down backoff when RTT is high as several backoff events occur
      // per RTT
      backoff /= std::max(
          1.0, delay_based_congestion_control_.rtt() / params_.virtual_rtt);

      if (!delay_based_congestion_control_.IsQueueDelayDetected()) {
        // Scale down backoff if close to the last known max reference window
        // This is complemented with a scale down of the reference window
        // increase
        backoff *=
            std::max(0.25, ref_window_scale_factor_close_to_ref_window_i());
        // Counterbalance the limitation in reference window increase when the
        // queue delay varies. This helps to avoid starvation in the presence
        // of competing TCP Prague flows.
        backoff *=
            std::max(0.1, delay_based_congestion_control_
                              .ref_window_scale_factor_due_to_avg_min_delay());
      }

      if (parsed.feedback_time - last_reaction_to_congestion_time_ >
          params_.number_of_rtts_between_reset_ref_window_i_on_congestion
                  .Get() *
              std::max(params_.virtual_rtt.Get(),
                       delay_based_congestion_control_.rtt())) {
        // A long time(> 100 RTTs) since last congested because
        // link throughput exceeds max video bitrate. (or first congestion)
        // There is a certain risk that ref_wnd has increased way above
        // bytes in flight, so we reduce it here to get it better on
        // track and thus the congestion episode is shortened
        ref_window_ = std::clamp(max_data_in_flight_prev_rtt_,
                                 params_.min_ref_window.Get(), ref_window_);
        // In addition, bump up l4sAlpha to a more credible value
        // This may over react but it is better than
        // excessive queue delay
        l4s_alpha_ = 0.25;
      }
    } else if (is_virtual_ce) {  // Back off due to delay
      backoff = delay_based_congestion_control_.l4s_alpha_v() / 2.0;
      backoff /= std::max(1.0, delay_based_congestion_control_.rtt() /
                                   params_.virtual_rtt.Get());
    }
    ref_window_ = (1.0 - backoff) * ref_window_;
  }

  // Increase ref_window.
  // 4.2.2.2.  Reference Window Increase
  if ((!is_ce && loss_congestion_level() < 0.01 && !is_virtual_ce) ||
      last_reaction_to_congestion_time_ == parsed.feedback_time) {
    // Allow increase if no event has occurred, or we are at the same time
    // backing off.
    // Just because there is a CE event, does not mean we send too much. At
    // rates close to the capacity, it is quite likely that one packet is CE
    // marked in every feedback.
    double increase_scale_factor = ref_window_mss_ratio();

    // Limit increase for small RTTs
    if (delay_based_congestion_control_.rtt() + feedback_hold_time_ <
        params_.virtual_rtt.Get()) {
      double rtt_ratio =
          (delay_based_congestion_control_.rtt() + feedback_hold_time_) /
          params_.virtual_rtt.Get();
      increase_scale_factor = increase_scale_factor * (rtt_ratio * rtt_ratio);
    }

    // Limit increase when close to the last inflection point.
    increase_scale_factor =
        increase_scale_factor *
        std::max(0.25, ref_window_scale_factor_close_to_ref_window_i());

    // Put a additional restriction on reference window growth if rtt varies a
    // lot.
    // Better to enforce a slow increase in reference window and get
    // a more stable bitrate.
    increase_scale_factor = increase_scale_factor *
                            delay_based_congestion_control_
                                .ref_window_scale_factor_due_to_avg_min_delay();
    increase_scale_factor =
        increase_scale_factor *
        delay_based_congestion_control_
            .ref_window_scale_factor_due_to_latency_difference();

    // Use lower multiplicative scale factor if congestion was detected
    // recently.
    const TimeDelta max_of_virtual_and_smothed_rtt = std::max(
        params_.virtual_rtt.Get(), delay_based_congestion_control_.rtt());
    double post_congestion_scale =
        std::clamp((parsed.feedback_time - last_reaction_to_congestion_time_) /
                       (params_.post_congestion_delay_rtts.Get() *
                        max_of_virtual_and_smothed_rtt),
                   0.0, 1.0);
    double multiplicative_scale =
        1.0 + (ref_window_multiplicative_scale_factor() - 1.0) *
                  post_congestion_scale *
                  ref_window_scale_factor_close_to_ref_window_i();
    RTC_DCHECK_GE(multiplicative_scale, 1.0);
    increase_scale_factor = increase_scale_factor * multiplicative_scale;

    DataSize increase = parsed.acked_not_marked_size * increase_scale_factor;
    last_ref_window_increase_scale_factor_ = increase_scale_factor;
    DataSize max_ref_window = max_allowed_ref_window();
    if (ref_window_ < max_ref_window) {
      ref_window_ = std::clamp(ref_window_ + increase,
                               params_.min_ref_window.Get(), max_ref_window);
    }
  }

  if (previous_ref_window < ref_window_) {
    // Allow setting a new `ref_window_i` if `ref_window_` increase.
    // It means that `ref_window_i` can increase if `rew_window` increase and
    // there is a congestion event.
    allow_ref_window_i_update_ = true;
  }
  if (previous_ref_window > ref_window_) {
    last_ref_window_decrease_time_ = parsed.feedback_time;
    if (allow_ref_window_i_update_) {
      ref_window_i_ = previous_ref_window;
      allow_ref_window_i_update_ = false;
    }
  }

  RTC_LOG_IF(LS_VERBOSE, previous_ref_window != ref_window_)
      << "ScreamV2: "
      << ", ref_window = " << ref_window_ << " ref_window_i_=" << ref_window_i_
      << ", change=" << ref_window_.bytes() - previous_ref_window.bytes()
      << " bytes "
      << ", l4s_alpha=" << l4s_alpha_ << ", is_ce=" << is_ce
      << " is_virtual_ce=" << is_virtual_ce << " is_loss=" << is_loss
      << " smoothed_rtt=" << delay_based_congestion_control_.rtt().ms()
      << ", queue_delay=" << delay_based_congestion_control_.queue_delay().ms()
      << " feedback_hold" << feedback_hold_time_.ms()
      << ", target_rate =" << target_rate_.kbps();
}

DataSize ScreamV2::max_data_in_flight() const {
  // 4.3.1. Send Window Calculation
  double ref_window_overhead =
      params_.ref_window_overhead_min.Get() +
      (params_.ref_window_overhead_max.Get() -
       params_.ref_window_overhead_min.Get()) *
          delay_based_congestion_control_
              .ref_window_scale_factor_due_to_avg_min_delay(
                  /*allow_zero=*/true);

  return ref_window_ * ref_window_overhead;
}

DataSize ScreamV2::max_allowed_ref_window() const {
  // 4.2.2.2.
  // Increase ref_window only if bytes in flight is large enough.
  // Quite a lot of slack is allowed here to avoid that bitrate locks to low
  // values.
  return std::max(
      params_.max_segment_size.Get() +
          std::max(max_data_in_flight_this_rtt_, max_data_in_flight_prev_rtt_) *
              params_.bytes_in_flight_head_room.Get(),
      params_.min_ref_window.Get());
}

void ScreamV2::UpdateFeedbackHoldTime(const ScreamFeedback& parsed) {
  if (feedback_hold_time_.IsZero() &&
      params_.feedback_hold_time_avg_g.Get() > 0.0) {
    feedback_hold_time_ = parsed.feedback_hold_time;
  }
  feedback_hold_time_ =
      parsed.feedback_hold_time * params_.feedback_hold_time_avg_g.Get() +
      (1.0 - params_.feedback_hold_time_avg_g.Get()) * feedback_hold_time_;
}

void ScreamV2::UpdateTargetRate(const ScreamFeedback& parsed) {
  // Avoid division by zero.
  const TimeDelta non_zero_smoothed_rtt =
      std::max(delay_based_congestion_control_.rtt(), TimeDelta::Millis(1));

  double scale_target_rate = 1.0;
  // Scale down target rate slightly when the reference window is very small
  // compared to MSS
  scale_target_rate *=
      (1.0 - std::clamp(ref_window_mss_ratio() - 0.1, 0.0, 0.2));

  DataRate target_rate =
      scale_target_rate *
      (ref_window_ / (non_zero_smoothed_rtt + feedback_hold_time_));

  if (!delay_based_congestion_control_.IsQueueDrainedInTime(
          parsed.feedback_time)) {
    // If estimated min queue delay is too high for too long, target rate is
    // reduced for a period of time.
    // If the min queue delay is still too high, the queue delay estimate is
    // reset. This can happen if the one way delay increase for other reasons
    // than self congestion.
    if (drain_queue_start_.IsInfinite()) {
      drain_queue_start_ = parsed.feedback_time;
      RTC_LOG(LS_INFO) << "Reduce target rate to attempt to drain queue.";
    }
    if (parsed.feedback_time - drain_queue_start_ <
        std::max(TimeDelta::Millis(100), params_.queue_delay_drain_rtts.Get() *
                                             non_zero_smoothed_rtt)) {
      target_rate = 0.5 * target_rate;
    } else {
      RTC_LOG(LS_INFO) << "Reset queue delay estimate due to high queue delay.";
      delay_based_congestion_control_.ResetQueueDelay();
    }
  } else {
    drain_queue_start_ = Timestamp::MinusInfinity();
  }

  // TODO: bugs.webrtc.org/447037083 -  Consider implementing 4.4, compensation
  // for increased pacer delay.
  target_rate =
      std::clamp(target_rate, min_target_bitrate_, max_target_bitrate_);

  target_rate_ = target_rate;
}

}  // namespace webrtc
