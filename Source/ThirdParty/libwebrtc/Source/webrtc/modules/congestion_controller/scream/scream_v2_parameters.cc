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
      l4s_avg_g_up("L4sAvgGUp", 1.0 / 8.0),
      l4s_avg_g_down("L4sAvgGDown", 1.0 / 128.0),
      smoothed_rtt_avg_g_up("SmoothedRttAvgGUp", 1.0 / 8.0),
      smoothed_rtt_avg_g_down("SmoothedRttAvgGDown", 1.0 / 8.0),
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
      ref_window_overhead_min("RefWinMin", 1.5),
      ref_window_overhead_max("RefWinMax", 3.0),
      queue_delay_avg_g("QDelayAvgG", 1.0 / 4.0),
      queue_delay_dev_avg_g("QDelayDevAvgG", 1.0 / 64.0),
      queue_delay_dev_normalization("QDelayDevNorm", TimeDelta::Millis(25)),
      base_delay_window_length("BaseDelayWindowLength", 10),
      base_delay_history_update_interval("BaseDelayHistoryUpdateInterval",
                                         TimeDelta::Minutes(1)),
      queue_delay_target("QDelayTarget", TimeDelta::Millis(60)),
      queue_delay_drain_threshold("QDelayDrainThreshold", TimeDelta::Millis(5)),
      queue_delay_drain_period("QDelayDrainPeriod", TimeDelta::Seconds(20)),
      queue_delay_drain_rtts("QDelayDrainRtts", 5),
      periodic_padding_interval("PeriodicPadding", TimeDelta::Seconds(6)),
      periodic_padding_duration("PaddingDuration", TimeDelta::Seconds(3)),
      allow_padding_after_last_congestion_time(
          "AllowPaddingAfterLastCongestionTimeout",
          TimeDelta::Seconds(1)),
      pacing_factor("PacingFactor", 1.1),
      feedback_hold_time_avg_g("FeedbackHoldTimeAvgG", 1.0 / 8.0) {
  ParseFieldTrial({&min_ref_window,
                   &l4s_avg_g_up,
                   &l4s_avg_g_down,
                   &smoothed_rtt_avg_g_up,
                   &smoothed_rtt_avg_g_down,
                   &max_segment_size,
                   &bytes_in_flight_head_room,
                   &beta_loss,
                   &post_congestion_delay_rtts,
                   &multiplicative_increase_factor,
                   &virtual_rtt,
                   &backoff_scale_factor_close_to_ref_window_i,
                   &number_of_rtts_between_reset_ref_window_i_on_congestion,
                   &ref_window_overhead_min,
                   &ref_window_overhead_max,
                   &queue_delay_avg_g,
                   &queue_delay_dev_avg_g,
                   &queue_delay_dev_normalization,
                   &base_delay_window_length,
                   &base_delay_history_update_interval,
                   &queue_delay_target,
                   &queue_delay_drain_threshold,
                   &queue_delay_drain_period,
                   &queue_delay_drain_rtts,
                   &periodic_padding_interval,
                   &periodic_padding_duration,
                   &allow_padding_after_last_congestion_time,
                   &pacing_factor,
                   &feedback_hold_time_avg_g},
                  trials.Lookup("WebRTC-Bwe-ScreamV2"));
}

}  // namespace webrtc
