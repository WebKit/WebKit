/*
 *  Copyright 2025 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_CONGESTION_CONTROLLER_SCREAM_SCREAM_V2_PARAMETERS_H_
#define MODULES_CONGESTION_CONTROLLER_SCREAM_SCREAM_V2_PARAMETERS_H_

#include "api/field_trials_view.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "rtc_base/experiments/field_trial_parser.h"

namespace webrtc {

struct ScreamV2Parameters {
  explicit ScreamV2Parameters(const FieldTrialsView& trials);
  ScreamV2Parameters(const ScreamV2Parameters& params) = default;

  // Minimum Reference Window
  FieldTrialParameter<DataSize> min_ref_window;

  // Exponentially Weighted Moving Average (EWMA) factor for l4s_alpha.
  FieldTrialParameter<double> l4s_avg_g_up;
  FieldTrialParameter<double> l4s_avg_g_down;

  // Exponentially Weighted Moving Average (EWMA) factor for smoothed rtt.
  FieldTrialParameter<double> smoothed_rtt_avg_g_up;
  FieldTrialParameter<double> smoothed_rtt_avg_g_down;

  // Maximum Segment Size (MSS)
  // Size of the largest data segment that a sender is able to transmit. I.e
  // largest possible IP packet.
  FieldTrialParameter<DataSize> max_segment_size;

  // Headroom for bytes in flight when increasing reference window.
  FieldTrialParameter<double> bytes_in_flight_head_room;

  // Reference window scale factor due to loss event.
  FieldTrialParameter<double> beta_loss;

  // Determines how many RTTs after a congestion event the reference window
  // growth should be cautious.
  FieldTrialParameter<int> post_congestion_delay_rtts;

  // Determines how much (as a fraction of ref_window) that the ref_window can
  // increase per RTT.
  FieldTrialParameter<double> multiplicative_increase_factor;

  // This mimics Prague's RTT fairness such that flows with RTT below
  // virtual_rtt should get a roughly equal share over an L4S path.
  FieldTrialParameter<TimeDelta> virtual_rtt;

  // Increase and decrease of ref window is slower close to the last
  // inflection point. Both increase and decrease is scaled by
  // (backoff_scale_factor_close_to_ref_window_i* (ref_window_i -
  // ref_window)) / ref_window_i) ^2
  FieldTrialParameter<double> backoff_scale_factor_close_to_ref_window_i;

  // If CE is detected and this number of RTTs has passed since last
  // congestion, ref_window_i will be reset.
  FieldTrialParameter<int>
      number_of_rtts_between_reset_ref_window_i_on_congestion;

  // Max/Min used for calculating how much larger the send window is allowed to
  // be than the ref window.
  FieldTrialParameter<double> ref_window_overhead_min;
  FieldTrialParameter<double> ref_window_overhead_max;

  // Exponentially Weighted Moving Average (EWMA) factor for updating queue
  // delay.
  FieldTrialParameter<double> queue_delay_avg_g;
  FieldTrialParameter<double> queue_delay_dev_avg_g;

  // Normalization factor for queue delay variation.
  FieldTrialParameter<TimeDelta> queue_delay_dev_normalization;

  // Determines the length of the base delay history when estimating one way
  // delay (owd)
  FieldTrialParameter<int> base_delay_window_length;
  // Determines how often the base delay history is updated.
  FieldTrialParameter<TimeDelta> base_delay_history_update_interval;

  // Reference window is reduced if average queue delay is above
  // `queue_delay_target/2`
  // TODO: bugs.webrtc.org/447037083 -  Consider implementing 4.2.1.4.1.
  // Competing Flows Compensation.
  FieldTrialParameter<TimeDelta> queue_delay_target;

  // If the minimum queue delay is below this threshold, queues are deamed to be
  // drained.
  FieldTrialParameter<TimeDelta> queue_delay_drain_threshold;
  // If the minimum queue delay has been above `queue_delay_drain_threshold` for
  // longer than `queue_delay_drain_period`, an attempt it made to drain the
  // queues, and if that fails, resets the estimates.
  FieldTrialParameter<TimeDelta> queue_delay_drain_period;
  // Number of RTTs where the target rate is reduced to attempt to drain.
  FieldTrialParameter<int> queue_delay_drain_rtts;

  // Padding is periodically used in order to increase target rate even if a
  // stream does not produce a high enough rate.
  FieldTrialParameter<TimeDelta> periodic_padding_interval;
  // Max duration padding is used when periodic padding start.
  // Padding is stopped if congestion occur.
  FieldTrialParameter<TimeDelta> periodic_padding_duration;
  // Padding is allowed to be used after this duration since the last
  // time reference window was reduced but at least `periodic_padding_interval`
  // must have passed since last time padding was used.
  FieldTrialParameter<TimeDelta> allow_padding_after_last_congestion_time;

  // Factor multiplied by the current target rate to decide the pacing rate.
  FieldTrialParameter<double> pacing_factor;

  // Exponentially Weighted Moving Average (EWMA) factor for calculating average
  // time feedback is delayed by the receiver. I.e the time from a packet is
  // received until feedback is sent. If zero, this delay is ignored.
  FieldTrialParameter<double> feedback_hold_time_avg_g;
};

}  // namespace webrtc
#endif  // MODULES_CONGESTION_CONTROLLER_SCREAM_SCREAM_V2_PARAMETERS_H_
