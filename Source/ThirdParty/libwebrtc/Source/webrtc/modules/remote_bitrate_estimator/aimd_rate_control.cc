/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/remote_bitrate_estimator/aimd_rate_control.h"

#include <inttypes.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>

#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "modules/remote_bitrate_estimator/overuse_detector.h"
#include "rtc_base/checks.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_minmax.h"

namespace webrtc {
namespace {

constexpr TimeDelta kDefaultRtt = TimeDelta::Millis<200>();
constexpr double kDefaultBackoffFactor = 0.85;

constexpr char kBweBackOffFactorExperiment[] = "WebRTC-BweBackOffFactor";

bool IsEnabled(const WebRtcKeyValueConfig& field_trials,
               absl::string_view key) {
  return field_trials.Lookup(key).find("Enabled") == 0;
}

double ReadBackoffFactor(const WebRtcKeyValueConfig& key_value_config) {
  std::string experiment_string =
      key_value_config.Lookup(kBweBackOffFactorExperiment);
  double backoff_factor;
  int parsed_values =
      sscanf(experiment_string.c_str(), "Enabled-%lf", &backoff_factor);
  if (parsed_values == 1) {
    if (backoff_factor >= 1.0) {
      RTC_LOG(WARNING) << "Back-off factor must be less than 1.";
    } else if (backoff_factor <= 0.0) {
      RTC_LOG(WARNING) << "Back-off factor must be greater than 0.";
    } else {
      return backoff_factor;
    }
  }
  RTC_LOG(LS_WARNING) << "Failed to parse parameters for AimdRateControl "
                         "experiment from field trial string. Using default.";
  return kDefaultBackoffFactor;
}

}  // namespace

AimdRateControl::AimdRateControl(const WebRtcKeyValueConfig* key_value_config)
    : AimdRateControl(key_value_config, /* send_side =*/false) {}

AimdRateControl::AimdRateControl(const WebRtcKeyValueConfig* key_value_config,
                                 bool send_side)
    : min_configured_bitrate_(congestion_controller::GetMinBitrate()),
      max_configured_bitrate_(DataRate::kbps(30000)),
      current_bitrate_(max_configured_bitrate_),
      latest_estimated_throughput_(current_bitrate_),
      link_capacity_(),
      rate_control_state_(kRcHold),
      time_last_bitrate_change_(Timestamp::MinusInfinity()),
      time_last_bitrate_decrease_(Timestamp::MinusInfinity()),
      time_first_throughput_estimate_(Timestamp::MinusInfinity()),
      bitrate_is_initialized_(false),
      beta_(IsEnabled(*key_value_config, kBweBackOffFactorExperiment)
                ? ReadBackoffFactor(*key_value_config)
                : kDefaultBackoffFactor),
      in_alr_(false),
      rtt_(kDefaultRtt),
      send_side_(send_side),
      in_experiment_(!AdaptiveThresholdExperimentIsDisabled(*key_value_config)),
      no_bitrate_increase_in_alr_(
          IsEnabled(*key_value_config,
                    "WebRTC-DontIncreaseDelayBasedBweInAlr")),
      smoothing_experiment_(
          IsEnabled(*key_value_config, "WebRTC-Audio-BandwidthSmoothing")),
      estimate_bounded_backoff_(
          IsEnabled(*key_value_config, "WebRTC-Bwe-EstimateBoundedBackoff")),
      estimate_bounded_increase_(
          IsEnabled(*key_value_config, "WebRTC-Bwe-EstimateBoundedIncrease")),
      initial_backoff_interval_("initial_backoff_interval"),
      low_throughput_threshold_("low_throughput", DataRate::Zero()),
      capacity_deviation_ratio_threshold_("cap_thr", 0.2),
      capacity_limit_deviation_factor_("cap_lim", 1) {
  // E.g
  // WebRTC-BweAimdRateControlConfig/initial_backoff_interval:100ms,
  // low_throughput:50kbps/
  ParseFieldTrial({&initial_backoff_interval_, &low_throughput_threshold_},
                  key_value_config->Lookup("WebRTC-BweAimdRateControlConfig"));
  if (initial_backoff_interval_) {
    RTC_LOG(LS_INFO) << "Using aimd rate control with initial back-off interval"
                     << " " << ToString(*initial_backoff_interval_) << ".";
  }
  RTC_LOG(LS_INFO) << "Using aimd rate control with back off factor " << beta_;
  ParseFieldTrial(
      {&capacity_deviation_ratio_threshold_, &capacity_limit_deviation_factor_},
      key_value_config->Lookup("WebRTC-Bwe-AimdRateControl-NetworkState"));
}

AimdRateControl::~AimdRateControl() {}

void AimdRateControl::SetStartBitrate(DataRate start_bitrate) {
  current_bitrate_ = start_bitrate;
  latest_estimated_throughput_ = current_bitrate_;
  bitrate_is_initialized_ = true;
}

void AimdRateControl::SetMinBitrate(DataRate min_bitrate) {
  min_configured_bitrate_ = min_bitrate;
  current_bitrate_ = std::max(min_bitrate, current_bitrate_);
}

bool AimdRateControl::ValidEstimate() const {
  return bitrate_is_initialized_;
}

TimeDelta AimdRateControl::GetFeedbackInterval() const {
  // Estimate how often we can send RTCP if we allocate up to 5% of bandwidth
  // to feedback.
  const DataSize kRtcpSize = DataSize::bytes(80);
  const DataRate rtcp_bitrate = current_bitrate_ * 0.05;
  const TimeDelta interval = kRtcpSize / rtcp_bitrate;
  const TimeDelta kMinFeedbackInterval = TimeDelta::ms(200);
  const TimeDelta kMaxFeedbackInterval = TimeDelta::ms(1000);
  return interval.Clamped(kMinFeedbackInterval, kMaxFeedbackInterval);
}

bool AimdRateControl::TimeToReduceFurther(Timestamp at_time,
                                          DataRate estimated_throughput) const {
  const TimeDelta bitrate_reduction_interval =
      rtt_.Clamped(TimeDelta::ms(10), TimeDelta::ms(200));
  if (at_time - time_last_bitrate_change_ >= bitrate_reduction_interval) {
    return true;
  }
  if (ValidEstimate()) {
    // TODO(terelius/holmer): Investigate consequences of increasing
    // the threshold to 0.95 * LatestEstimate().
    const DataRate threshold = 0.5 * LatestEstimate();
    return estimated_throughput < threshold;
  }
  return false;
}

bool AimdRateControl::InitialTimeToReduceFurther(Timestamp at_time) const {
  if (!initial_backoff_interval_) {
    return ValidEstimate() &&
           TimeToReduceFurther(at_time,
                               LatestEstimate() / 2 - DataRate::bps(1));
  }
  // TODO(terelius): We could use the RTT (clamped to suitable limits) instead
  // of a fixed bitrate_reduction_interval.
  if (time_last_bitrate_decrease_.IsInfinite() ||
      at_time - time_last_bitrate_decrease_ >= *initial_backoff_interval_) {
    return true;
  }
  return false;
}

DataRate AimdRateControl::LatestEstimate() const {
  return current_bitrate_;
}

void AimdRateControl::SetRtt(TimeDelta rtt) {
  rtt_ = rtt;
}

DataRate AimdRateControl::Update(const RateControlInput* input,
                                 Timestamp at_time) {
  RTC_CHECK(input);

  // Set the initial bit rate value to what we're receiving the first half
  // second.
  // TODO(bugs.webrtc.org/9379): The comment above doesn't match to the code.
  if (!bitrate_is_initialized_) {
    const TimeDelta kInitializationTime = TimeDelta::seconds(5);
    RTC_DCHECK_LE(kBitrateWindowMs, kInitializationTime.ms());
    if (time_first_throughput_estimate_.IsInfinite()) {
      if (input->estimated_throughput)
        time_first_throughput_estimate_ = at_time;
    } else if (at_time - time_first_throughput_estimate_ >
                   kInitializationTime &&
               input->estimated_throughput) {
      current_bitrate_ = *input->estimated_throughput;
      bitrate_is_initialized_ = true;
    }
  }

  current_bitrate_ = ChangeBitrate(current_bitrate_, *input, at_time);
  return current_bitrate_;
}

void AimdRateControl::SetInApplicationLimitedRegion(bool in_alr) {
  in_alr_ = in_alr;
}

void AimdRateControl::SetEstimate(DataRate bitrate, Timestamp at_time) {
  bitrate_is_initialized_ = true;
  DataRate prev_bitrate = current_bitrate_;
  current_bitrate_ = ClampBitrate(bitrate, bitrate);
  time_last_bitrate_change_ = at_time;
  if (current_bitrate_ < prev_bitrate) {
    time_last_bitrate_decrease_ = at_time;
  }
}

void AimdRateControl::SetNetworkStateEstimate(
    const absl::optional<NetworkStateEstimate>& estimate) {
  network_estimate_ = estimate;
}

double AimdRateControl::GetNearMaxIncreaseRateBpsPerSecond() const {
  RTC_DCHECK(!current_bitrate_.IsZero());
  const TimeDelta kFrameInterval = TimeDelta::seconds(1) / 30;
  DataSize frame_size = current_bitrate_ * kFrameInterval;
  const DataSize kPacketSize = DataSize::bytes(1200);
  double packets_per_frame = std::ceil(frame_size / kPacketSize);
  DataSize avg_packet_size = frame_size / packets_per_frame;

  // Approximate the over-use estimator delay to 100 ms.
  TimeDelta response_time = rtt_ + TimeDelta::ms(100);
  if (in_experiment_)
    response_time = response_time * 2;
  double increase_rate_bps_per_second =
      (avg_packet_size / response_time).bps<double>();
  double kMinIncreaseRateBpsPerSecond = 4000;
  return std::max(kMinIncreaseRateBpsPerSecond, increase_rate_bps_per_second);
}

TimeDelta AimdRateControl::GetExpectedBandwidthPeriod() const {
  const TimeDelta kMinPeriod =
      smoothing_experiment_ ? TimeDelta::ms(500) : TimeDelta::seconds(2);
  const TimeDelta kDefaultPeriod = TimeDelta::seconds(3);
  const TimeDelta kMaxPeriod = TimeDelta::seconds(50);

  double increase_rate_bps_per_second = GetNearMaxIncreaseRateBpsPerSecond();
  if (!last_decrease_)
    return smoothing_experiment_ ? kMinPeriod : kDefaultPeriod;
  double time_to_recover_decrease_seconds =
      last_decrease_->bps() / increase_rate_bps_per_second;
  TimeDelta period = TimeDelta::seconds(time_to_recover_decrease_seconds);
  return period.Clamped(kMinPeriod, kMaxPeriod);
}

DataRate AimdRateControl::ChangeBitrate(DataRate new_bitrate,
                                        const RateControlInput& input,
                                        Timestamp at_time) {
  DataRate estimated_throughput =
      input.estimated_throughput.value_or(latest_estimated_throughput_);
  if (input.estimated_throughput)
    latest_estimated_throughput_ = *input.estimated_throughput;

  // An over-use should always trigger us to reduce the bitrate, even though
  // we have not yet established our first estimate. By acting on the over-use,
  // we will end up with a valid estimate.
  if (!bitrate_is_initialized_ &&
      input.bw_state != BandwidthUsage::kBwOverusing)
    return current_bitrate_;

  ChangeState(input, at_time);

  switch (rate_control_state_) {
    case kRcHold:
      break;

    case kRcIncrease:
      if (estimated_throughput > link_capacity_.UpperBound())
        link_capacity_.Reset();

      // Do not increase the delay based estimate in alr since the estimator
      // will not be able to get transport feedback necessary to detect if
      // the new estimate is correct.
      if (!(send_side_ && in_alr_ && no_bitrate_increase_in_alr_)) {
        if (link_capacity_.has_estimate()) {
          // The link_capacity estimate is reset if the measured throughput
          // is too far from the estimate. We can therefore assume that our
          // target rate is reasonably close to link capacity and use additive
          // increase.
          DataRate additive_increase =
              AdditiveRateIncrease(at_time, time_last_bitrate_change_);
          new_bitrate += additive_increase;
        } else {
          // If we don't have an estimate of the link capacity, use faster ramp
          // up to discover the capacity.
          DataRate multiplicative_increase = MultiplicativeRateIncrease(
              at_time, time_last_bitrate_change_, new_bitrate);
          new_bitrate += multiplicative_increase;
        }
      }

      time_last_bitrate_change_ = at_time;
      break;

    case kRcDecrease:
      // TODO(srte): Remove when |estimate_bounded_backoff_| has been validated.
      if (network_estimate_ && capacity_deviation_ratio_threshold_ &&
          !estimate_bounded_backoff_) {
        estimated_throughput = std::max(estimated_throughput,
                                        network_estimate_->link_capacity_lower);
      }
      if (estimated_throughput > low_throughput_threshold_) {
        // Set bit rate to something slightly lower than the measured throughput
        // to get rid of any self-induced delay.
        new_bitrate = estimated_throughput * beta_;
        if (new_bitrate > current_bitrate_) {
          // Avoid increasing the rate when over-using.
          if (link_capacity_.has_estimate()) {
            new_bitrate = beta_ * link_capacity_.estimate();
          }
        }
        if (estimate_bounded_backoff_ && network_estimate_) {
          new_bitrate = std::max(
              new_bitrate, network_estimate_->link_capacity_lower * beta_);
        }
      } else {
        new_bitrate = estimated_throughput;
        if (link_capacity_.has_estimate()) {
          new_bitrate = std::max(new_bitrate, link_capacity_.estimate());
        }
        new_bitrate = std::min(new_bitrate, low_throughput_threshold_.Get());
      }
      new_bitrate = std::min(new_bitrate, current_bitrate_);

      if (bitrate_is_initialized_ && estimated_throughput < current_bitrate_) {
        constexpr double kDegradationFactor = 0.9;
        if (smoothing_experiment_ &&
            new_bitrate < kDegradationFactor * beta_ * current_bitrate_) {
          // If bitrate decreases more than a normal back off after overuse, it
          // indicates a real network degradation. We do not let such a decrease
          // to determine the bandwidth estimation period.
          last_decrease_ = absl::nullopt;
        } else {
          last_decrease_ = current_bitrate_ - new_bitrate;
        }
      }
      if (estimated_throughput < link_capacity_.LowerBound()) {
        // The current throughput is far from the estimated link capacity. Clear
        // the estimate to allow an immediate update in OnOveruseDetected.
        link_capacity_.Reset();
      }

      bitrate_is_initialized_ = true;
      link_capacity_.OnOveruseDetected(estimated_throughput);
      // Stay on hold until the pipes are cleared.
      rate_control_state_ = kRcHold;
      time_last_bitrate_change_ = at_time;
      time_last_bitrate_decrease_ = at_time;
      break;

    default:
      assert(false);
  }
  return ClampBitrate(new_bitrate, estimated_throughput);
}

DataRate AimdRateControl::ClampBitrate(DataRate new_bitrate,
                                       DataRate estimated_throughput) const {
  // Allow the estimate to increase as long as alr is not detected to ensure
  // that there is no BWE values that can make the estimate stuck at a too
  // low bitrate. If an encoder can not produce the bitrate necessary to
  // fully use the capacity, alr will sooner or later trigger.
  if (!(send_side_ && no_bitrate_increase_in_alr_)) {
    // Don't change the bit rate if the send side is too far off.
    // We allow a bit more lag at very low rates to not too easily get stuck if
    // the encoder produces uneven outputs.
    const DataRate max_bitrate =
        1.5 * estimated_throughput + DataRate::kbps(10);
    if (new_bitrate > current_bitrate_ && new_bitrate > max_bitrate) {
      new_bitrate = std::max(current_bitrate_, max_bitrate);
    }
  }

  if (network_estimate_ &&
      (estimate_bounded_increase_ || capacity_limit_deviation_factor_)) {
    DataRate upper_bound = network_estimate_->link_capacity_upper;
    new_bitrate = std::min(new_bitrate, upper_bound);
  }
  new_bitrate = std::max(new_bitrate, min_configured_bitrate_);
  return new_bitrate;
}

DataRate AimdRateControl::MultiplicativeRateIncrease(
    Timestamp at_time,
    Timestamp last_time,
    DataRate current_bitrate) const {
  double alpha = 1.08;
  if (last_time.IsFinite()) {
    auto time_since_last_update = at_time - last_time;
    alpha = pow(alpha, std::min(time_since_last_update.seconds<double>(), 1.0));
  }
  DataRate multiplicative_increase =
      std::max(current_bitrate * (alpha - 1.0), DataRate::bps(1000));
  return multiplicative_increase;
}

DataRate AimdRateControl::AdditiveRateIncrease(Timestamp at_time,
                                               Timestamp last_time) const {
  double time_period_seconds = (at_time - last_time).seconds<double>();
  double data_rate_increase_bps =
      GetNearMaxIncreaseRateBpsPerSecond() * time_period_seconds;
  return DataRate::bps(data_rate_increase_bps);
}

void AimdRateControl::ChangeState(const RateControlInput& input,
                                  Timestamp at_time) {
  switch (input.bw_state) {
    case BandwidthUsage::kBwNormal:
      if (rate_control_state_ == kRcHold) {
        time_last_bitrate_change_ = at_time;
        rate_control_state_ = kRcIncrease;
      }
      break;
    case BandwidthUsage::kBwOverusing:
      if (rate_control_state_ != kRcDecrease) {
        rate_control_state_ = kRcDecrease;
      }
      break;
    case BandwidthUsage::kBwUnderusing:
      rate_control_state_ = kRcHold;
      break;
    default:
      assert(false);
  }
}

}  // namespace webrtc
