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
  FieldTrialParameter<double> l4s_avg_g;

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

  // Excessive data in flight correction
  FieldTrialParameter<double> data_in_flight_limit;
  FieldTrialParameter<double> max_data_in_flight_limit_compensation;

  // Exponentially Weighted Moving Average (EWMA) factor for updating queue
  // delay.
  FieldTrialParameter<double> queue_delay_avg_g;

  // Determines the length of the base delay history when estimating one way
  // delay (owd)
  FieldTrialParameter<int> base_delay_window_length;
  // Determines how often the base delay history is updated.
  FieldTrialParameter<TimeDelta> base_delay_history_update_interval;

  // Initial queue delay target.
  // TODO: bugs.webrtc.org/447037083 -  Consider implementing 4.2.1.4.1.
  // Competing Flows Compensation.
  FieldTrialParameter<TimeDelta> queue_delay_target;

  // Queue delay is "detected" if queue delay is higher than
  // `queue_delay_target`* `queue_delay_increased_threshold`.
  FieldTrialParameter<double> queue_delay_increased_threshold;

  // Reference window should be reduced if average queue delay is above
  // `queue_delay_target`* `queue_delay_threshold`
  FieldTrialParameter<double> queue_delay_threshold;

  // Use all packets when calculating `queue_delay_average`. This is per default
  // true. This is in contrast with
  // https://datatracker.ietf.org/doc/draft-johansson-ccwg-rfc8298bis-screamv2/
  // that specifies that only the last packet reported in a feedback packet
  // received every min(virtual_rtt, smoothed rtt) is used.
  FieldTrialParameter<bool> use_all_packets_when_calculating_queue_delay;

  // Padding is periodically used in order to increase target rate even if a
  // stream does not produce a high enough rate.
  FieldTrialParameter<TimeDelta> periodic_padding_interval;

  // Duration padding is used when periodic padding start.
  FieldTrialParameter<TimeDelta> periodic_padding_duration;
};

}  // namespace webrtc
#endif  // MODULES_CONGESTION_CONTROLLER_SCREAM_SCREAM_V2_PARAMETERS_H_
