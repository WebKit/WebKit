/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_TIMING_TIMESTAMP_EXTRAPOLATOR_H_
#define MODULES_VIDEO_CODING_TIMING_TIMESTAMP_EXTRAPOLATOR_H_

#include <stdint.h>

#include <memory>
#include <optional>

#include "absl/strings/string_view.h"
#include "api/field_trials_view.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/experiments/struct_parameters_parser.h"
#include "rtc_base/numerics/sequence_number_unwrapper.h"

namespace webrtc {

// The `TimestampExtrapolator` is an adaptive filter that estimates the
// local clock time of incoming RTP timestamps. It's main purpose is to handle
// clock drift and clock offset, not to model network behaviour.
//
// The mechanisms applied for this are:
//  * Recursive least-squares filter for estimating clock skew and clock offset.
//  * Hard reset on wall clock timeout.
//  * Hard reset on large incoming RTP timestamp jumps.
//  * Hard outlier rejection based on the difference between the predicted and
//    the actual wall clock time for an RTP timestamp.
//  * Soft outlier attenuation based on the integrated (CUSUM style) difference
//    between predicted and actual wall clock time for an RTP timestamp.
//
// Not all of the mechanisms are enabled by default. Use the field trial string
// to experiment with different settings.
//
// Not thread safe.
class TimestampExtrapolator {
 public:
  // Configuration struct for overriding some constants and behaviour,
  // configurable through field trials.
  struct Config {
    inline static constexpr absl::string_view kFieldTrialsKey =
        "WebRTC-TimestampExtrapolatorConfig";

    // Factory function that parses the field trials and returns a `Config`
    // with validated values.
    static Config ParseAndValidate(const FieldTrialsView& field_trials);

    std::unique_ptr<StructParametersParser> Parser() {
      // clang-format off
      return StructParametersParser::Create(
        "hard_reset_timeout", &hard_reset_timeout,
        "hard_reset_rtp_timestamp_jump_threshold",
          &hard_reset_rtp_timestamp_jump_threshold,
        "outlier_rejection_startup_delay", &outlier_rejection_startup_delay,
        "outlier_rejection_max_consecutive", &outlier_rejection_max_consecutive,
        "outlier_rejection_forgetting_factor",
          &outlier_rejection_forgetting_factor,
        "outlier_rejection_stddev", &outlier_rejection_stddev,
        "alarm_threshold", &alarm_threshold,
        "acc_drift", &acc_drift,
        "acc_max_error", &acc_max_error,
        "reset_full_cov_on_alarm", &reset_full_cov_on_alarm);
      // clang-format on
    }

    bool OutlierRejectionEnabled() const {
      return outlier_rejection_stddev.has_value();
    }

    // -- Hard reset behaviour --

    // If a frame has not been received within this timeout, do a full reset.
    TimeDelta hard_reset_timeout = TimeDelta::Seconds(10);

    // A jump in the RTP timestamp of this magnitude, not accounted for by the
    // passage of time, is considered a source clock replacement and will
    // trigger a filter reset. 900000 ticks = 10 seconds.
    // (Only enabled if hard outlier rejection is enabled.)
    int hard_reset_rtp_timestamp_jump_threshold = 900'000;

    // -- Hard outlier rejection --

    // Number of frames to wait before starting to update the residual
    // statistics. 300 frames = 10 seconds@30fps.
    int outlier_rejection_startup_delay = 300;

    // Number of consecutive frames that are allowed to be treated as outliers.
    // If more frames than these are outliers, hard outlier rejection stops
    // and soft outlier attentuation starts.
    // 150 frames = 5 seconds@30fps.
    int outlier_rejection_max_consecutive = 150;

    // Smoothing factor for the residual statistics.
    // Half-life is log(0.5)/log(0.999) ~= 693 frames ~= 23 seconds@30fps.
    double outlier_rejection_forgetting_factor = 0.999;

    // If set, will reject outliers based on this number of standard deviations
    // of the filtered residuals.
    // Setting this field to non-nullopt enables hard outlier rejection.
    std::optional<double> outlier_rejection_stddev = 2.0;

    // -- Soft outlier attenuation --

    // Alarm on sudden delay change if the (filtered; see below) accumulated
    // residuals are larger than this number of RTP ticks. After the
    // startup period, an alarm will result in a full or partial reset of the
    // uncertainty covariance (see `reset_full_cov_on_alarm` below).
    int alarm_threshold = 60000;  // 666 ms <=> 20 frames@30fps.

    // Acceptable level of per-frame drift in the detector (in RTP ticks).
    int acc_drift = 6600;  // 73 ms <=> 2.2 frames@30fps.

    // Max limit on residuals in the detector (in RTP ticks).
    // TODO(brandtr): Increase from this unreasonably low value.
    int acc_max_error = 7000;  // 77 ms <=> 2.3 frames@30fps.

    // If true, reset the entire uncertainty covariance matrix on alarms.
    // If false, only reset the offset variance term.
    // TODO(brandtr): Flip so that the frequency term won't get hit tpp badly
    // when a large delay spike happens.
    bool reset_full_cov_on_alarm = false;
  };

  TimestampExtrapolator(Timestamp start, const FieldTrialsView& field_trials);
  ~TimestampExtrapolator();

  // Update the filter with a new incoming local timestamp/RTP timestamp pair.
  void Update(Timestamp now, uint32_t ts90khz);

  // Return the expected local timestamp for an RTP timestamp.
  std::optional<Timestamp> ExtrapolateLocalTime(uint32_t timestamp90khz) const;

  void Reset(Timestamp start);

  Config GetConfigForTest() const { return config_; }

 private:
  void SoftReset();
  bool OutlierDetection(double residual);
  bool DelayChangeDetection(double residual);

  const Config config_;

  double w_[2];
  double p_[2][2];
  Timestamp start_;
  Timestamp prev_;
  std::optional<int64_t> first_unwrapped_timestamp_;
  RtpTimestampUnwrapper unwrapper_;
  std::optional<int64_t> prev_unwrapped_timestamp_;
  int packet_count_;

  // Running residual statistics for the hard outlier rejection.
  double residual_mean_;
  double residual_variance_;
  int outliers_consecutive_count_;

  // Integrated residual statistics for the soft outlier attenuation.
  double detector_accumulator_pos_;
  double detector_accumulator_neg_;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_TIMING_TIMESTAMP_EXTRAPOLATOR_H_
