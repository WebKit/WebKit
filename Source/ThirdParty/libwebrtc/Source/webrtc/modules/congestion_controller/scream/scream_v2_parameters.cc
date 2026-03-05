/*
 *  Copyright 2025 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/scream/scream_v2_parameters.h"

#include "api/field_trials_view.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "rtc_base/experiments/field_trial_parser.h"

namespace webrtc {

ScreamV2Parameters::ScreamV2Parameters(const FieldTrialsView& trials)
    : min_ref_window("MinRefWindow", DataSize::Bytes(3000)),
      l4s_avg_g("L4sAvgG", 1.0 / 16.0),
      max_segment_size("MaxSegmentSize", DataSize::Bytes(1000)),
      bytes_in_flight_head_room("BytesInFlightHeadRoom", 1.1),
      beta_loss("BetaLoss", 0.7),
      post_congestion_delay_rtts("PostCongestionDelayRtts", 100),
      multiplicative_increase_factor("MultiplicativeIncreaseFactor", 0.02),
      virtual_rtt("VirtualRtt", TimeDelta::Millis(25)),
      // backoff_scale_factor_close_to_ref_window_i is set lower than in the
      // rfc(8.0). This means that increase/decrease around ref_window_i is
      // slower in this implementation.
      backoff_scale_factor_close_to_ref_window_i(
          "BackoffScaleFactorCloseToRefWindowI",
          2.0),
      number_of_rtts_between_reset_ref_window_i_on_congestion(
          "NumberOfRttsBetweenResetRefWindowIOnCongestion",
          100),
      data_in_flight_limit("DataInFlightLimit", 0.9),
      max_data_in_flight_limit_compensation("MaxDataInFlightLimitCompensation",
                                            1.5),
      queue_delay_avg_g("QDelayAvgG", 1.0 / 4.0),
      base_delay_window_length("BaseDelayWindowLength", 10),
      base_delay_history_update_interval("BaseDelayHistoryUpdateInterval",
                                         TimeDelta::Minutes(1)),
      queue_delay_target("QDelayTarget", TimeDelta::Millis(100)),
      queue_delay_increased_threshold("QDelayIncreasedThreshold", 0.25),
      queue_delay_threshold("QDelayThreshold", 0.5),
      use_all_packets_when_calculating_queue_delay(
          "UseAllPacketsWhenCalculatingQDelay",
          true),
      periodic_padding_interval("PeriodicPadding", TimeDelta::Seconds(10)),
      periodic_padding_duration("PaddingDuration", TimeDelta::Seconds(1)) {
  ParseFieldTrial({&min_ref_window,
                   &l4s_avg_g,
                   &max_segment_size,
                   &bytes_in_flight_head_room,
                   &beta_loss,
                   &post_congestion_delay_rtts,
                   &multiplicative_increase_factor,
                   &virtual_rtt,
                   &backoff_scale_factor_close_to_ref_window_i,
                   &number_of_rtts_between_reset_ref_window_i_on_congestion,
                   &data_in_flight_limit,
                   &max_data_in_flight_limit_compensation,
                   &queue_delay_avg_g,
                   &base_delay_window_length,
                   &base_delay_history_update_interval,
                   &queue_delay_target,
                   &queue_delay_increased_threshold,
                   &queue_delay_threshold,
                   &use_all_packets_when_calculating_queue_delay,
                   &periodic_padding_interval,
                   &periodic_padding_duration},
                  trials.Lookup("WebRTC-Bwe-ScreamV2"));
}

}  // namespace webrtc
