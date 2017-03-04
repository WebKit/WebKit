/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/congestion_controller/delay_based_bwe.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "webrtc/base/checks.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/logging/rtc_event_log/rtc_event_log.h"
#include "webrtc/modules/congestion_controller/include/congestion_controller.h"
#include "webrtc/modules/pacing/paced_sender.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test_logging.h"
#include "webrtc/system_wrappers/include/field_trial.h"
#include "webrtc/system_wrappers/include/metrics.h"
#include "webrtc/typedefs.h"

namespace {
constexpr int kTimestampGroupLengthMs = 5;
constexpr int kAbsSendTimeFraction = 18;
constexpr int kAbsSendTimeInterArrivalUpshift = 8;
constexpr int kInterArrivalShift =
    kAbsSendTimeFraction + kAbsSendTimeInterArrivalUpshift;
constexpr double kTimestampToMs =
    1000.0 / static_cast<double>(1 << kInterArrivalShift);
// This ssrc is used to fulfill the current API but will be removed
// after the API has been changed.
constexpr uint32_t kFixedSsrc = 0;
constexpr int kInitialRateWindowMs = 500;
constexpr int kRateWindowMs = 150;

// Parameters for linear least squares fit of regression line to noisy data.
constexpr size_t kDefaultTrendlineWindowSize = 15;
constexpr double kDefaultTrendlineSmoothingCoeff = 0.9;
constexpr double kDefaultTrendlineThresholdGain = 4.0;

// Parameters for Theil-Sen robust fitting of line to noisy data.
constexpr size_t kDefaultMedianSlopeWindowSize = 20;
constexpr double kDefaultMedianSlopeThresholdGain = 4.0;

constexpr int kMaxConsecutiveFailedLookups = 5;

const char kBitrateEstimateExperiment[] = "WebRTC-ImprovedBitrateEstimate";
const char kBweTrendlineFilterExperiment[] = "WebRTC-BweTrendlineFilter";
const char kBweMedianSlopeFilterExperiment[] = "WebRTC-BweMedianSlopeFilter";

bool BitrateEstimateExperimentIsEnabled() {
  return webrtc::field_trial::IsEnabled(kBitrateEstimateExperiment);
}

bool TrendlineFilterExperimentIsEnabled() {
  std::string experiment_string =
      webrtc::field_trial::FindFullName(kBweTrendlineFilterExperiment);
  // The experiment is enabled iff the field trial string begins with "Enabled".
  return experiment_string.find("Enabled") == 0;
}

bool MedianSlopeFilterExperimentIsEnabled() {
  std::string experiment_string =
      webrtc::field_trial::FindFullName(kBweMedianSlopeFilterExperiment);
  // The experiment is enabled iff the field trial string begins with "Enabled".
  return experiment_string.find("Enabled") == 0;
}

bool ReadTrendlineFilterExperimentParameters(size_t* window_size,
                                             double* smoothing_coef,
                                             double* threshold_gain) {
  RTC_DCHECK(TrendlineFilterExperimentIsEnabled());
  RTC_DCHECK(!MedianSlopeFilterExperimentIsEnabled());
  RTC_DCHECK(window_size != nullptr);
  RTC_DCHECK(smoothing_coef != nullptr);
  RTC_DCHECK(threshold_gain != nullptr);
  std::string experiment_string =
      webrtc::field_trial::FindFullName(kBweTrendlineFilterExperiment);
  int parsed_values = sscanf(experiment_string.c_str(), "Enabled-%zu,%lf,%lf",
                             window_size, smoothing_coef, threshold_gain);
  if (parsed_values == 3) {
    RTC_CHECK_GT(*window_size, 1) << "Need at least 2 points to fit a line.";
    RTC_CHECK(0 <= *smoothing_coef && *smoothing_coef <= 1)
        << "Coefficient needs to be between 0 and 1 for weighted average.";
    RTC_CHECK_GT(*threshold_gain, 0) << "Threshold gain needs to be positive.";
    return true;
  }
  LOG(LS_WARNING) << "Failed to parse parameters for BweTrendlineFilter "
                     "experiment from field trial string. Using default.";
  *window_size = kDefaultTrendlineWindowSize;
  *smoothing_coef = kDefaultTrendlineSmoothingCoeff;
  *threshold_gain = kDefaultTrendlineThresholdGain;
  return false;
}

bool ReadMedianSlopeFilterExperimentParameters(size_t* window_size,
                                               double* threshold_gain) {
  RTC_DCHECK(!TrendlineFilterExperimentIsEnabled());
  RTC_DCHECK(MedianSlopeFilterExperimentIsEnabled());
  RTC_DCHECK(window_size != nullptr);
  RTC_DCHECK(threshold_gain != nullptr);
  std::string experiment_string =
      webrtc::field_trial::FindFullName(kBweMedianSlopeFilterExperiment);
  int parsed_values = sscanf(experiment_string.c_str(), "Enabled-%zu,%lf",
                             window_size, threshold_gain);
  if (parsed_values == 2) {
    RTC_CHECK_GT(*window_size, 1) << "Need at least 2 points to fit a line.";
    RTC_CHECK_GT(*threshold_gain, 0) << "Threshold gain needs to be positive.";
    return true;
  }
  LOG(LS_WARNING) << "Failed to parse parameters for BweMedianSlopeFilter "
                     "experiment from field trial string. Using default.";
  *window_size = kDefaultMedianSlopeWindowSize;
  *threshold_gain = kDefaultMedianSlopeThresholdGain;
  return false;
}
}  // namespace

namespace webrtc {

DelayBasedBwe::BitrateEstimator::BitrateEstimator()
    : sum_(0),
      current_win_ms_(0),
      prev_time_ms_(-1),
      bitrate_estimate_(-1.0f),
      bitrate_estimate_var_(50.0f),
      old_estimator_(kBitrateWindowMs, 8000),
      in_experiment_(BitrateEstimateExperimentIsEnabled()) {}

void DelayBasedBwe::BitrateEstimator::Update(int64_t now_ms, int bytes) {
  if (!in_experiment_) {
    old_estimator_.Update(bytes, now_ms);
    rtc::Optional<uint32_t> rate = old_estimator_.Rate(now_ms);
    bitrate_estimate_ = -1.0f;
    if (rate)
      bitrate_estimate_ = *rate / 1000.0f;
    return;
  }
  int rate_window_ms = kRateWindowMs;
  // We use a larger window at the beginning to get a more stable sample that
  // we can use to initialize the estimate.
  if (bitrate_estimate_ < 0.f)
    rate_window_ms = kInitialRateWindowMs;
  float bitrate_sample = UpdateWindow(now_ms, bytes, rate_window_ms);
  if (bitrate_sample < 0.0f)
    return;
  if (bitrate_estimate_ < 0.0f) {
    // This is the very first sample we get. Use it to initialize the estimate.
    bitrate_estimate_ = bitrate_sample;
    return;
  }
  // Define the sample uncertainty as a function of how far away it is from the
  // current estimate.
  float sample_uncertainty =
      10.0f * std::abs(bitrate_estimate_ - bitrate_sample) / bitrate_estimate_;
  float sample_var = sample_uncertainty * sample_uncertainty;
  // Update a bayesian estimate of the rate, weighting it lower if the sample
  // uncertainty is large.
  // The bitrate estimate uncertainty is increased with each update to model
  // that the bitrate changes over time.
  float pred_bitrate_estimate_var = bitrate_estimate_var_ + 5.f;
  bitrate_estimate_ = (sample_var * bitrate_estimate_ +
                       pred_bitrate_estimate_var * bitrate_sample) /
                      (sample_var + pred_bitrate_estimate_var);
  bitrate_estimate_var_ = sample_var * pred_bitrate_estimate_var /
                          (sample_var + pred_bitrate_estimate_var);
}

float DelayBasedBwe::BitrateEstimator::UpdateWindow(int64_t now_ms,
                                                    int bytes,
                                                    int rate_window_ms) {
  // Reset if time moves backwards.
  if (now_ms < prev_time_ms_) {
    prev_time_ms_ = -1;
    sum_ = 0;
    current_win_ms_ = 0;
  }
  if (prev_time_ms_ >= 0) {
    current_win_ms_ += now_ms - prev_time_ms_;
    // Reset if nothing has been received for more than a full window.
    if (now_ms - prev_time_ms_ > rate_window_ms) {
      sum_ = 0;
      current_win_ms_ %= rate_window_ms;
    }
  }
  prev_time_ms_ = now_ms;
  float bitrate_sample = -1.0f;
  if (current_win_ms_ >= rate_window_ms) {
    bitrate_sample = 8.0f * sum_ / static_cast<float>(rate_window_ms);
    current_win_ms_ -= rate_window_ms;
    sum_ = 0;
  }
  sum_ += bytes;
  return bitrate_sample;
}

rtc::Optional<uint32_t> DelayBasedBwe::BitrateEstimator::bitrate_bps() const {
  if (bitrate_estimate_ < 0.f)
    return rtc::Optional<uint32_t>();
  return rtc::Optional<uint32_t>(bitrate_estimate_ * 1000);
}

DelayBasedBwe::DelayBasedBwe(RtcEventLog* event_log, Clock* clock)
    : in_trendline_experiment_(TrendlineFilterExperimentIsEnabled()),
      in_median_slope_experiment_(MedianSlopeFilterExperimentIsEnabled()),
      event_log_(event_log),
      clock_(clock),
      inter_arrival_(),
      kalman_estimator_(),
      trendline_estimator_(),
      detector_(),
      receiver_incoming_bitrate_(),
      last_update_ms_(-1),
      last_seen_packet_ms_(-1),
      uma_recorded_(false),
      trendline_window_size_(kDefaultTrendlineWindowSize),
      trendline_smoothing_coeff_(kDefaultTrendlineSmoothingCoeff),
      trendline_threshold_gain_(kDefaultTrendlineThresholdGain),
      probing_interval_estimator_(&rate_control_),
      median_slope_window_size_(kDefaultMedianSlopeWindowSize),
      median_slope_threshold_gain_(kDefaultMedianSlopeThresholdGain),
      consecutive_delayed_feedbacks_(0),
      last_logged_bitrate_(0),
      last_logged_state_(kBwNormal) {
  if (in_trendline_experiment_) {
    ReadTrendlineFilterExperimentParameters(&trendline_window_size_,
                                            &trendline_smoothing_coeff_,
                                            &trendline_threshold_gain_);
    LOG(LS_INFO) << "Trendline filter experiment enabled with parameters "
                 << trendline_window_size_ << ',' << trendline_smoothing_coeff_
                 << ',' << trendline_threshold_gain_;
  }
  if (in_median_slope_experiment_) {
    ReadMedianSlopeFilterExperimentParameters(&median_slope_window_size_,
                                              &median_slope_threshold_gain_);
    LOG(LS_INFO) << "Median-slope filter experiment enabled with parameters "
                 << median_slope_window_size_ << ','
                 << median_slope_threshold_gain_;
  }
  if (!in_trendline_experiment_ && !in_median_slope_experiment_) {
    LOG(LS_INFO) << "No overuse experiment enabled. Using Kalman filter.";
  }

  network_thread_.DetachFromThread();
}

DelayBasedBwe::Result DelayBasedBwe::IncomingPacketFeedbackVector(
    const std::vector<PacketInfo>& packet_feedback_vector) {
  RTC_DCHECK(network_thread_.CalledOnValidThread());
  if (!uma_recorded_) {
    RTC_HISTOGRAM_ENUMERATION(kBweTypeHistogram,
                              BweNames::kSendSideTransportSeqNum,
                              BweNames::kBweNamesMax);
    uma_recorded_ = true;
  }
  Result aggregated_result;
  bool delayed_feedback = true;
  for (const auto& packet_info : packet_feedback_vector) {
    if (packet_info.send_time_ms < 0)
      continue;
    delayed_feedback = false;
    Result result = IncomingPacketInfo(packet_info);
    if (result.updated)
      aggregated_result = result;
  }
  if (delayed_feedback) {
    ++consecutive_delayed_feedbacks_;
  } else {
    consecutive_delayed_feedbacks_ = 0;
  }
  if (consecutive_delayed_feedbacks_ >= kMaxConsecutiveFailedLookups) {
    aggregated_result =
        OnLongFeedbackDelay(packet_feedback_vector.back().arrival_time_ms);
    consecutive_delayed_feedbacks_ = 0;
  }
  return aggregated_result;
}

DelayBasedBwe::Result DelayBasedBwe::OnLongFeedbackDelay(
    int64_t arrival_time_ms) {
  // Estimate should always be valid since a start bitrate always is set in the
  // Call constructor. An alternative would be to return an empty Result here,
  // or to estimate the throughput based on the feedback we received.
  RTC_DCHECK(rate_control_.ValidEstimate());
  rate_control_.SetEstimate(rate_control_.LatestEstimate() / 2,
                            arrival_time_ms);
  Result result;
  result.updated = true;
  result.probe = false;
  result.target_bitrate_bps = rate_control_.LatestEstimate();
  LOG(LS_WARNING) << "Long feedback delay detected, reducing BWE to "
                  << result.target_bitrate_bps;
  return result;
}

DelayBasedBwe::Result DelayBasedBwe::IncomingPacketInfo(
    const PacketInfo& info) {
  int64_t now_ms = clock_->TimeInMilliseconds();

  receiver_incoming_bitrate_.Update(info.arrival_time_ms, info.payload_size);
  Result result;
  // Reset if the stream has timed out.
  if (last_seen_packet_ms_ == -1 ||
      now_ms - last_seen_packet_ms_ > kStreamTimeOutMs) {
    inter_arrival_.reset(
        new InterArrival((kTimestampGroupLengthMs << kInterArrivalShift) / 1000,
                         kTimestampToMs, true));
    kalman_estimator_.reset(new OveruseEstimator(OverUseDetectorOptions()));
    trendline_estimator_.reset(new TrendlineEstimator(
        trendline_window_size_, trendline_smoothing_coeff_,
        trendline_threshold_gain_));
    median_slope_estimator_.reset(new MedianSlopeEstimator(
        median_slope_window_size_, median_slope_threshold_gain_));
  }
  last_seen_packet_ms_ = now_ms;

  uint32_t send_time_24bits =
      static_cast<uint32_t>(
          ((static_cast<uint64_t>(info.send_time_ms) << kAbsSendTimeFraction) +
           500) /
          1000) &
      0x00FFFFFF;
  // Shift up send time to use the full 32 bits that inter_arrival works with,
  // so wrapping works properly.
  uint32_t timestamp = send_time_24bits << kAbsSendTimeInterArrivalUpshift;

  uint32_t ts_delta = 0;
  int64_t t_delta = 0;
  int size_delta = 0;
  if (inter_arrival_->ComputeDeltas(timestamp, info.arrival_time_ms, now_ms,
                                    info.payload_size, &ts_delta, &t_delta,
                                    &size_delta)) {
    double ts_delta_ms = (1000.0 * ts_delta) / (1 << kInterArrivalShift);
    if (in_trendline_experiment_) {
      trendline_estimator_->Update(t_delta, ts_delta_ms, info.arrival_time_ms);
      detector_.Detect(trendline_estimator_->trendline_slope(), ts_delta_ms,
                       trendline_estimator_->num_of_deltas(),
                       info.arrival_time_ms);
    } else if (in_median_slope_experiment_) {
      median_slope_estimator_->Update(t_delta, ts_delta_ms,
                                      info.arrival_time_ms);
      detector_.Detect(median_slope_estimator_->trendline_slope(), ts_delta_ms,
                       median_slope_estimator_->num_of_deltas(),
                       info.arrival_time_ms);
    } else {
      kalman_estimator_->Update(t_delta, ts_delta_ms, size_delta,
                                detector_.State(), info.arrival_time_ms);
      detector_.Detect(kalman_estimator_->offset(), ts_delta_ms,
                       kalman_estimator_->num_of_deltas(),
                       info.arrival_time_ms);
    }
  }

  int probing_bps = 0;
  if (info.pacing_info.probe_cluster_id != PacedPacketInfo::kNotAProbe) {
    probing_bps = probe_bitrate_estimator_.HandleProbeAndEstimateBitrate(info);
  }
  rtc::Optional<uint32_t> acked_bitrate_bps =
      receiver_incoming_bitrate_.bitrate_bps();
  // Currently overusing the bandwidth.
  if (detector_.State() == kBwOverusing) {
    if (acked_bitrate_bps &&
        rate_control_.TimeToReduceFurther(now_ms, *acked_bitrate_bps)) {
      result.updated =
          UpdateEstimate(info.arrival_time_ms, now_ms, acked_bitrate_bps,
                         &result.target_bitrate_bps);
    }
  } else if (probing_bps > 0) {
    // No overuse, but probing measured a bitrate.
    rate_control_.SetEstimate(probing_bps, info.arrival_time_ms);
    result.probe = true;
    result.updated =
        UpdateEstimate(info.arrival_time_ms, now_ms, acked_bitrate_bps,
                       &result.target_bitrate_bps);
  }
  if (!result.updated &&
      (last_update_ms_ == -1 ||
       now_ms - last_update_ms_ > rate_control_.GetFeedbackInterval())) {
    result.updated =
        UpdateEstimate(info.arrival_time_ms, now_ms, acked_bitrate_bps,
                       &result.target_bitrate_bps);
  }
  if (result.updated) {
    last_update_ms_ = now_ms;
    BWE_TEST_LOGGING_PLOT(1, "target_bitrate_bps", now_ms,
                          result.target_bitrate_bps);
    if (event_log_ && (result.target_bitrate_bps != last_logged_bitrate_ ||
                       detector_.State() != last_logged_state_)) {
      event_log_->LogDelayBasedBweUpdate(result.target_bitrate_bps,
                                         detector_.State());
      last_logged_bitrate_ = result.target_bitrate_bps;
      last_logged_state_ = detector_.State();
    }
  }

  return result;
}

bool DelayBasedBwe::UpdateEstimate(int64_t arrival_time_ms,
                                   int64_t now_ms,
                                   rtc::Optional<uint32_t> acked_bitrate_bps,
                                   uint32_t* target_bitrate_bps) {
  // TODO(terelius): RateControlInput::noise_var is deprecated and will be
  // removed. In the meantime, we set it to zero.
  const RateControlInput input(detector_.State(), acked_bitrate_bps, 0);
  rate_control_.Update(&input, now_ms);
  *target_bitrate_bps = rate_control_.UpdateBandwidthEstimate(now_ms);
  return rate_control_.ValidEstimate();
}

void DelayBasedBwe::OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) {
  rate_control_.SetRtt(avg_rtt_ms);
}

bool DelayBasedBwe::LatestEstimate(std::vector<uint32_t>* ssrcs,
                                   uint32_t* bitrate_bps) const {
  // Currently accessed from both the process thread (see
  // ModuleRtpRtcpImpl::Process()) and the configuration thread (see
  // Call::GetStats()). Should in the future only be accessed from a single
  // thread.
  RTC_DCHECK(ssrcs);
  RTC_DCHECK(bitrate_bps);
  if (!rate_control_.ValidEstimate())
    return false;

  *ssrcs = {kFixedSsrc};
  *bitrate_bps = rate_control_.LatestEstimate();
  return true;
}

void DelayBasedBwe::SetStartBitrate(int start_bitrate_bps) {
  LOG(LS_WARNING) << "BWE Setting start bitrate to: " << start_bitrate_bps;
  rate_control_.SetStartBitrate(start_bitrate_bps);
}

void DelayBasedBwe::SetMinBitrate(int min_bitrate_bps) {
  // Called from both the configuration thread and the network thread. Shouldn't
  // be called from the network thread in the future.
  rate_control_.SetMinBitrate(min_bitrate_bps);
}

int64_t DelayBasedBwe::GetProbingIntervalMs() const {
  return probing_interval_estimator_.GetIntervalMs();
}
}  // namespace webrtc
