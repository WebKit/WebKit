/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/timing/timestamp_extrapolator.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <optional>

#include "api/field_trials_view.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/sequence_number_unwrapper.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {

namespace {

constexpr int kMinimumSamplesToLogEstimatedClockDrift =
    3000;  // 100 seconds at 30 fps.
constexpr double kLambda = 1;
constexpr int kStartUpFilterDelayInPackets = 2;
constexpr double kP00 = 1.0;
constexpr double kP11 = 1e10;
constexpr double kStartResidualVariance = 3000 * 3000;

}  // namespace

TimestampExtrapolator::Config TimestampExtrapolator::Config::ParseAndValidate(
    const FieldTrialsView& field_trials) {
  // Parse.
  Config config;
  config.Parser()->Parse(field_trials.Lookup(kFieldTrialsKey));

  // Validate.
  Config defaults;
  if (config.hard_reset_timeout <= TimeDelta::Zero()) {
    RTC_LOG(LS_WARNING) << "Skipping invalid hard_reset_timeout="
                        << config.hard_reset_timeout;
    config.hard_reset_timeout = defaults.hard_reset_timeout;
  }
  if (config.hard_reset_rtp_timestamp_jump_threshold <= 0) {
    RTC_LOG(LS_WARNING)
        << "Skipping invalid hard_reset_rtp_timestamp_jump_threshold="
        << config.hard_reset_rtp_timestamp_jump_threshold;
    config.hard_reset_rtp_timestamp_jump_threshold =
        defaults.hard_reset_rtp_timestamp_jump_threshold;
  }
  if (config.outlier_rejection_startup_delay < 0) {
    RTC_LOG(LS_WARNING) << "Skipping invalid outlier_rejection_startup_delay="
                        << config.outlier_rejection_startup_delay;
    config.outlier_rejection_startup_delay =
        defaults.outlier_rejection_startup_delay;
  }
  if (config.outlier_rejection_max_consecutive <= 0) {
    RTC_LOG(LS_WARNING) << "Skipping invalid outlier_rejection_max_consecutive="
                        << config.outlier_rejection_max_consecutive;
    config.outlier_rejection_max_consecutive =
        defaults.outlier_rejection_max_consecutive;
  }
  if (config.outlier_rejection_forgetting_factor < 0 ||
      config.outlier_rejection_forgetting_factor >= 1) {
    RTC_LOG(LS_WARNING)
        << "Skipping invalid outlier_rejection_forgetting_factor="
        << config.outlier_rejection_forgetting_factor;
    config.outlier_rejection_forgetting_factor =
        defaults.outlier_rejection_forgetting_factor;
  }
  if (config.outlier_rejection_stddev.has_value() &&
      *config.outlier_rejection_stddev <= 0) {
    RTC_LOG(LS_WARNING) << "Skipping invalid outlier_rejection_stddev="
                        << *config.outlier_rejection_stddev;
    config.outlier_rejection_stddev = defaults.outlier_rejection_stddev;
  }
  if (config.alarm_threshold <= 0) {
    RTC_LOG(LS_WARNING) << "Skipping invalid alarm_threshold="
                        << config.alarm_threshold;
    config.alarm_threshold = defaults.alarm_threshold;
  }
  if (config.acc_drift < 0) {
    RTC_LOG(LS_WARNING) << "Skipping invalid acc_drift=" << config.acc_drift;
    config.acc_drift = defaults.acc_drift;
  }
  if (config.acc_max_error <= 0) {
    RTC_LOG(LS_WARNING) << "Skipping invalid acc_max_error="
                        << config.acc_max_error;
    config.acc_max_error = defaults.acc_max_error;
  }

  return config;
}

TimestampExtrapolator::TimestampExtrapolator(
    Timestamp start,
    const FieldTrialsView& field_trials)
    : config_(Config::ParseAndValidate(field_trials)),
      start_(Timestamp::Zero()),
      prev_(Timestamp::Zero()),
      packet_count_(0),
      residual_mean_(0),
      residual_variance_(kStartResidualVariance),
      outliers_consecutive_count_(0),
      detector_accumulator_pos_(0),
      detector_accumulator_neg_(0) {
  Reset(start);
}

TimestampExtrapolator::~TimestampExtrapolator() {
  if (packet_count_ >= kMinimumSamplesToLogEstimatedClockDrift) {
    // Relative clock drift per million (ppm).
    double clock_drift_ppm = 1e6 * (w_[0] - 90.0) / 90.0;
    RTC_HISTOGRAM_COUNTS_100000("WebRTC.Video.EstimatedClockDrift_ppm",
                                static_cast<int>(std::abs(clock_drift_ppm)));
  }
}

void TimestampExtrapolator::Reset(Timestamp start) {
  start_ = start;
  prev_ = start_;
  first_unwrapped_timestamp_ = std::nullopt;
  prev_unwrapped_timestamp_ = std::nullopt;
  w_[0] = 90.0;
  w_[1] = 0;
  p_[0][0] = kP00;
  p_[1][1] = kP11;
  p_[0][1] = p_[1][0] = 0;
  unwrapper_ = RtpTimestampUnwrapper();
  packet_count_ = 0;
  // Hard outlier rejection.
  residual_mean_ = 0.0;
  residual_variance_ = kStartResidualVariance;
  outliers_consecutive_count_ = 0;
  // Soft outlier attenuation.
  detector_accumulator_pos_ = 0;
  detector_accumulator_neg_ = 0;
}

void TimestampExtrapolator::Update(Timestamp now, uint32_t ts90khz) {
  // Hard reset based local clock timeouts.
  if (now - prev_ > config_.hard_reset_timeout) {
    Reset(now);
  } else {
    prev_ = now;
  }

  const int64_t unwrapped_ts90khz = unwrapper_.Unwrap(ts90khz);

  // Hard reset based on large RTP timestamp jumps. This is only enabled if
  // outlier rejection is enabled, since that feature would by itself
  // consistently block any long-term static offset changes due to, e.g.,
  // remote clock source replacements.
  if (config_.OutlierRejectionEnabled() && prev_unwrapped_timestamp_) {
    // Predict the expected RTP timestamp change based on elapsed wall clock
    // time.
    int64_t expected_rtp_diff =
        static_cast<int64_t>((now - prev_).ms() * w_[0]);
    int64_t actual_rtp_diff = unwrapped_ts90khz - *prev_unwrapped_timestamp_;
    int64_t rtp_jump = actual_rtp_diff - expected_rtp_diff;
    if (std::abs(rtp_jump) > config_.hard_reset_rtp_timestamp_jump_threshold) {
      RTC_LOG(LS_WARNING) << "Large jump in RTP timestamp detected at "
                          << unwrapped_ts90khz
                          << ". Difference between actual and expected change: "
                          << rtp_jump << " ticks. Resetting filter.";
      Reset(now);
    }
  }

  // Remove offset to prevent badly scaled matrices
  const TimeDelta offset = now - start_;
  double t_ms = offset.ms();

  if (!first_unwrapped_timestamp_) {
    // Make an initial guess of the offset,
    // should be almost correct since t_ms - start
    // should about zero at this time.
    w_[1] = -w_[0] * t_ms;
    first_unwrapped_timestamp_ = unwrapped_ts90khz;
  }

  double residual =
      (static_cast<double>(unwrapped_ts90khz) - *first_unwrapped_timestamp_) -
      t_ms * w_[0] - w_[1];

  // Hard outlier rejection: reject outliers and avoid updating the filter state
  // for frames whose residuals are too large.
  if (config_.OutlierRejectionEnabled() && OutlierDetection(residual)) {
    ++outliers_consecutive_count_;
    if (outliers_consecutive_count_ <=
        config_.outlier_rejection_max_consecutive) {
      // This appears to be a transient spike. Reject it.
      return;
    } else {
      // This appears to be a persistent delay change. Force the filter to
      // adapt.
      SoftReset();
    }
  }
  // Frame is an inlier, or we have reached `outlier_rejection_max_consecutive`.
  outliers_consecutive_count_ = 0;

  // Soft outlier attenuation: boost the filter's uncertainty if the integrated
  // delay has changed too much.
  // TODO(brandtr): Move the packet count check into DelayChangeDetection.
  if (DelayChangeDetection(residual) &&
      packet_count_ >= kStartUpFilterDelayInPackets) {
    // Force the filter to adjust its offset parameter by changing
    // the uncertainties. Don't do this during startup.
    SoftReset();
  }

  // If hard outlier rejection is enabled, we handle large RTP timestamp jumps
  // above.
  if (!config_.OutlierRejectionEnabled() &&
      (prev_unwrapped_timestamp_ &&
       unwrapped_ts90khz < prev_unwrapped_timestamp_)) {
    // Drop reordered frames.
    return;
  }

  // Update recursive least squares filter.
  // TODO(b/428657776): Document better.

  // T = [t(k) 1]';
  // that = T'*w;
  // K = P*T/(lambda + T'*P*T);
  double K[2];
  K[0] = p_[0][0] * t_ms + p_[0][1];
  K[1] = p_[1][0] * t_ms + p_[1][1];
  double TPT = kLambda + t_ms * K[0] + K[1];
  K[0] /= TPT;
  K[1] /= TPT;
  // w = w + K*(ts(k) - that);
  w_[0] = w_[0] + K[0] * residual;
  w_[1] = w_[1] + K[1] * residual;
  // P = 1/lambda*(P - K*T'*P);
  double p00 =
      1 / kLambda * (p_[0][0] - (K[0] * t_ms * p_[0][0] + K[0] * p_[1][0]));
  double p01 =
      1 / kLambda * (p_[0][1] - (K[0] * t_ms * p_[0][1] + K[0] * p_[1][1]));
  p_[1][0] =
      1 / kLambda * (p_[1][0] - (K[1] * t_ms * p_[0][0] + K[1] * p_[1][0]));
  p_[1][1] =
      1 / kLambda * (p_[1][1] - (K[1] * t_ms * p_[0][1] + K[1] * p_[1][1]));
  p_[0][0] = p00;
  p_[0][1] = p01;

  prev_unwrapped_timestamp_ = unwrapped_ts90khz;
  if (packet_count_ < kStartUpFilterDelayInPackets ||
      packet_count_ < kMinimumSamplesToLogEstimatedClockDrift) {
    packet_count_++;
  }
}

std::optional<Timestamp> TimestampExtrapolator::ExtrapolateLocalTime(
    uint32_t timestamp90khz) const {
  int64_t unwrapped_ts90khz = unwrapper_.PeekUnwrap(timestamp90khz);

  if (!first_unwrapped_timestamp_) {
    return std::nullopt;
  }
  if (packet_count_ < kStartUpFilterDelayInPackets) {
    constexpr double kRtpTicksPerMs = 90;
    TimeDelta diff = TimeDelta::Millis(
        (unwrapped_ts90khz - *prev_unwrapped_timestamp_) / kRtpTicksPerMs);
    if (prev_.us() + diff.us() < 0) {
      // Prevent the construction of a negative Timestamp.
      // This scenario can occur when the RTP timestamp wraps around.
      return std::nullopt;
    }
    return prev_ + diff;
  }
  if (w_[0] < 1e-3) {
    return start_;
  }
  double timestamp_diff =
      static_cast<double>(unwrapped_ts90khz - *first_unwrapped_timestamp_);
  TimeDelta diff = TimeDelta::Millis(
      static_cast<int64_t>((timestamp_diff - w_[1]) / w_[0] + 0.5));
  if (start_.us() + diff.us() < 0) {
    // Prevent the construction of a negative Timestamp.
    // This scenario can occur when the RTP timestamp wraps around.
    return std::nullopt;
  }
  return start_ + diff;
}

void TimestampExtrapolator::SoftReset() {
  if (config_.reset_full_cov_on_alarm) {
    p_[0][0] = kP00;
    p_[0][1] = p_[1][0] = 0;
  }
  p_[1][1] = kP11;
}

bool TimestampExtrapolator::OutlierDetection(double residual) {
  if (!config_.outlier_rejection_stddev.has_value()) {
    return false;
  }

  if (packet_count_ >= config_.outlier_rejection_startup_delay) {
    double threshold =
        *config_.outlier_rejection_stddev * std::sqrt(residual_variance_);
    // Outlier frames trigger the alarm.
    // We intentionally use a symmetric detection here, meaning that
    // significantly early frames are also alarmed on. The main reason is to
    // ensure a symmetric update to the running statistics below.
    if (std::abs(residual - residual_mean_) > threshold) {
      // Alarm.
      return true;
    }
  }

  // Update residual statistics only with inliers.
  double forgetting_factor = config_.outlier_rejection_forgetting_factor;
  residual_mean_ =
      forgetting_factor * residual_mean_ + (1.0 - forgetting_factor) * residual;
  double residual_deviation = residual - residual_mean_;
  double squared_residual_deviation = residual_deviation * residual_deviation;
  residual_variance_ =
      std::max(forgetting_factor * residual_variance_ +
                   (1.0 - forgetting_factor) * squared_residual_deviation,
               1.0);

  return false;
}

bool TimestampExtrapolator::DelayChangeDetection(double residual) {
  // CUSUM detection of sudden delay changes
  double acc_max_error = static_cast<double>(config_.acc_max_error);
  residual = (residual > 0) ? std::min(residual, acc_max_error)
                            : std::max(residual, -acc_max_error);
  detector_accumulator_pos_ = std::max(
      detector_accumulator_pos_ + residual - config_.acc_drift, double{0});
  detector_accumulator_neg_ = std::min(
      detector_accumulator_neg_ + residual + config_.acc_drift, double{0});
  if (detector_accumulator_pos_ > config_.alarm_threshold ||
      detector_accumulator_neg_ < -config_.alarm_threshold) {
    // Alarm
    detector_accumulator_pos_ = detector_accumulator_neg_ = 0;
    return true;
  }
  return false;
}

}  // namespace webrtc
