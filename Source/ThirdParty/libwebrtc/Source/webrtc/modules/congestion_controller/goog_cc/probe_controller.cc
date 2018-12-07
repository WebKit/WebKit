/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/goog_cc/probe_controller.h"

#include <algorithm>
#include <initializer_list>

#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "system_wrappers/include/field_trial.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {

namespace {
// The minimum number probing packets used.
constexpr int kMinProbePacketsSent = 5;

// The minimum probing duration in ms.
constexpr int kMinProbeDurationMs = 15;

// Maximum waiting time from the time of initiating probing to getting
// the measured results back.
constexpr int64_t kMaxWaitingTimeForProbingResultMs = 1000;

// Value of |min_bitrate_to_probe_further_bps_| that indicates
// further probing is disabled.
constexpr int kExponentialProbingDisabled = 0;

// Default probing bitrate limit. Applied only when the application didn't
// specify max bitrate.
constexpr int64_t kDefaultMaxProbingBitrateBps = 5000000;

// Interval between probes when ALR periodic probing is enabled.
constexpr int64_t kAlrPeriodicProbingIntervalMs = 5000;

// Minimum probe bitrate percentage to probe further for repeated probes,
// relative to the previous probe. For example, if 1Mbps probe results in
// 80kbps, then we'll probe again at 1.6Mbps. In that case second probe won't be
// sent if we get 600kbps from the first one.
constexpr int kRepeatedProbeMinPercentage = 70;

// If the bitrate drops to a factor |kBitrateDropThreshold| or lower
// and we recover within |kBitrateDropTimeoutMs|, then we'll send
// a probe at a fraction |kProbeFractionAfterDrop| of the original bitrate.
constexpr double kBitrateDropThreshold = 0.66;
constexpr int kBitrateDropTimeoutMs = 5000;
constexpr double kProbeFractionAfterDrop = 0.85;

// Timeout for probing after leaving ALR. If the bitrate drops significantly,
// (as determined by the delay based estimator) and we leave ALR, then we will
// send a probe if we recover within |kLeftAlrTimeoutMs| ms.
constexpr int kAlrEndedTimeoutMs = 3000;

// The expected uncertainty of probe result (as a fraction of the target probe
// This is a limit on how often probing can be done when there is a BW
// drop detected in ALR.
constexpr int64_t kMinTimeBetweenAlrProbesMs = 5000;

// bitrate). Used to avoid probing if the probe bitrate is close to our current
// estimate.
constexpr double kProbeUncertainty = 0.05;

// Use probing to recover faster after large bitrate estimate drops.
constexpr char kBweRapidRecoveryExperiment[] =
    "WebRTC-BweRapidRecoveryExperiment";

}  // namespace

ProbeController::ProbeController() : enable_periodic_alr_probing_(false) {
  Reset(0);
  in_rapid_recovery_experiment_ = webrtc::field_trial::FindFullName(
                                      kBweRapidRecoveryExperiment) == "Enabled";
}

ProbeController::~ProbeController() {}

std::vector<ProbeClusterConfig> ProbeController::SetBitrates(
    int64_t min_bitrate_bps,
    int64_t start_bitrate_bps,
    int64_t max_bitrate_bps,
    int64_t at_time_ms) {
  if (start_bitrate_bps > 0) {
    start_bitrate_bps_ = start_bitrate_bps;
    estimated_bitrate_bps_ = start_bitrate_bps;
  } else if (start_bitrate_bps_ == 0) {
    start_bitrate_bps_ = min_bitrate_bps;
  }

  // The reason we use the variable |old_max_bitrate_pbs| is because we
  // need to set |max_bitrate_bps_| before we call InitiateProbing.
  int64_t old_max_bitrate_bps = max_bitrate_bps_;
  max_bitrate_bps_ = max_bitrate_bps;

  switch (state_) {
    case State::kInit:
      if (network_available_)
        return InitiateExponentialProbing(at_time_ms);
      break;

    case State::kWaitingForProbingResult:
      break;

    case State::kProbingComplete:
      // If the new max bitrate is higher than the old max bitrate and the
      // estimate is lower than the new max bitrate then initiate probing.
      if (estimated_bitrate_bps_ != 0 &&
          old_max_bitrate_bps < max_bitrate_bps_ &&
          estimated_bitrate_bps_ < max_bitrate_bps_) {
        // The assumption is that if we jump more than 20% in the bandwidth
        // estimate or if the bandwidth estimate is within 90% of the new
        // max bitrate then the probing attempt was successful.
        mid_call_probing_succcess_threshold_ =
            std::min(estimated_bitrate_bps_ * 1.2, max_bitrate_bps_ * 0.9);
        mid_call_probing_waiting_for_result_ = true;
        mid_call_probing_bitrate_bps_ = max_bitrate_bps_;

        RTC_HISTOGRAM_COUNTS_10000("WebRTC.BWE.MidCallProbing.Initiated",
                                   max_bitrate_bps_ / 1000);

        return InitiateProbing(at_time_ms, {max_bitrate_bps}, false);
      }
      break;
  }
  return std::vector<ProbeClusterConfig>();
}

std::vector<ProbeClusterConfig> ProbeController::OnMaxTotalAllocatedBitrate(
    int64_t max_total_allocated_bitrate,
    int64_t at_time_ms) {
  // TODO(philipel): Should |max_total_allocated_bitrate| be used as a limit for
  //                 ALR probing?
  if (state_ == State::kProbingComplete &&
      max_total_allocated_bitrate != max_total_allocated_bitrate_ &&
      estimated_bitrate_bps_ != 0 &&
      (max_bitrate_bps_ <= 0 || estimated_bitrate_bps_ < max_bitrate_bps_) &&
      estimated_bitrate_bps_ < max_total_allocated_bitrate) {
    max_total_allocated_bitrate_ = max_total_allocated_bitrate;
    return InitiateProbing(at_time_ms, {max_total_allocated_bitrate}, false);
  }
  max_total_allocated_bitrate_ = max_total_allocated_bitrate;
  return std::vector<ProbeClusterConfig>();
}

std::vector<ProbeClusterConfig> ProbeController::OnNetworkAvailability(
    NetworkAvailability msg) {
  network_available_ = msg.network_available;

  if (!network_available_ && state_ == State::kWaitingForProbingResult) {
    state_ = State::kProbingComplete;
    min_bitrate_to_probe_further_bps_ = kExponentialProbingDisabled;
  }

  if (network_available_ && state_ == State::kInit && start_bitrate_bps_ > 0)
    return InitiateExponentialProbing(msg.at_time.ms());
  return std::vector<ProbeClusterConfig>();
}

std::vector<ProbeClusterConfig> ProbeController::InitiateExponentialProbing(
    int64_t at_time_ms) {
  RTC_DCHECK(network_available_);
  RTC_DCHECK(state_ == State::kInit);
  RTC_DCHECK_GT(start_bitrate_bps_, 0);

  // When probing at 1.8 Mbps ( 6x 300), this represents a threshold of
  // 1.2 Mbps to continue probing.
  return InitiateProbing(
      at_time_ms, {3 * start_bitrate_bps_, 6 * start_bitrate_bps_}, true);
}

std::vector<ProbeClusterConfig> ProbeController::SetEstimatedBitrate(
    int64_t bitrate_bps,
    int64_t at_time_ms) {
  int64_t now_ms = at_time_ms;

  if (mid_call_probing_waiting_for_result_ &&
      bitrate_bps >= mid_call_probing_succcess_threshold_) {
    RTC_HISTOGRAM_COUNTS_10000("WebRTC.BWE.MidCallProbing.Success",
                               mid_call_probing_bitrate_bps_ / 1000);
    RTC_HISTOGRAM_COUNTS_10000("WebRTC.BWE.MidCallProbing.ProbedKbps",
                               bitrate_bps / 1000);
    mid_call_probing_waiting_for_result_ = false;
  }
  std::vector<ProbeClusterConfig> pending_probes;
  if (state_ == State::kWaitingForProbingResult) {
    // Continue probing if probing results indicate channel has greater
    // capacity.
    RTC_LOG(LS_INFO) << "Measured bitrate: " << bitrate_bps
                     << " Minimum to probe further: "
                     << min_bitrate_to_probe_further_bps_;

    if (min_bitrate_to_probe_further_bps_ != kExponentialProbingDisabled &&
        bitrate_bps > min_bitrate_to_probe_further_bps_) {
      // Double the probing bitrate.
      pending_probes = InitiateProbing(now_ms, {2 * bitrate_bps}, true);
    }
  }

  if (bitrate_bps < kBitrateDropThreshold * estimated_bitrate_bps_) {
    time_of_last_large_drop_ms_ = now_ms;
    bitrate_before_last_large_drop_bps_ = estimated_bitrate_bps_;
  }

  estimated_bitrate_bps_ = bitrate_bps;
  return pending_probes;
}

void ProbeController::EnablePeriodicAlrProbing(bool enable) {
  enable_periodic_alr_probing_ = enable;
}

void ProbeController::SetAlrStartTimeMs(
    absl::optional<int64_t> alr_start_time_ms) {
  alr_start_time_ms_ = alr_start_time_ms;
}
void ProbeController::SetAlrEndedTimeMs(int64_t alr_end_time_ms) {
  alr_end_time_ms_.emplace(alr_end_time_ms);
}

std::vector<ProbeClusterConfig> ProbeController::RequestProbe(
    int64_t at_time_ms) {
  // Called once we have returned to normal state after a large drop in
  // estimated bandwidth. The current response is to initiate a single probe
  // session (if not already probing) at the previous bitrate.
  //
  // If the probe session fails, the assumption is that this drop was a
  // real one from a competing flow or a network change.
  bool in_alr = alr_start_time_ms_.has_value();
  bool alr_ended_recently =
      (alr_end_time_ms_.has_value() &&
       at_time_ms - alr_end_time_ms_.value() < kAlrEndedTimeoutMs);
  if (in_alr || alr_ended_recently || in_rapid_recovery_experiment_) {
    if (state_ == State::kProbingComplete) {
      uint32_t suggested_probe_bps =
          kProbeFractionAfterDrop * bitrate_before_last_large_drop_bps_;
      uint32_t min_expected_probe_result_bps =
          (1 - kProbeUncertainty) * suggested_probe_bps;
      int64_t time_since_drop_ms = at_time_ms - time_of_last_large_drop_ms_;
      int64_t time_since_probe_ms = at_time_ms - last_bwe_drop_probing_time_ms_;
      if (min_expected_probe_result_bps > estimated_bitrate_bps_ &&
          time_since_drop_ms < kBitrateDropTimeoutMs &&
          time_since_probe_ms > kMinTimeBetweenAlrProbesMs) {
        RTC_LOG(LS_INFO) << "Detected big bandwidth drop, start probing.";
        // Track how often we probe in response to bandwidth drop in ALR.
        RTC_HISTOGRAM_COUNTS_10000(
            "WebRTC.BWE.BweDropProbingIntervalInS",
            (at_time_ms - last_bwe_drop_probing_time_ms_) / 1000);
        return InitiateProbing(at_time_ms, {suggested_probe_bps}, false);
        last_bwe_drop_probing_time_ms_ = at_time_ms;
      }
    }
  }
  return std::vector<ProbeClusterConfig>();
}

std::vector<ProbeClusterConfig> ProbeController::InitiateCapacityProbing(
    int64_t bitrate_bps,
    int64_t at_time_ms) {
  if (state_ != State::kWaitingForProbingResult) {
    RTC_DCHECK(network_available_);
    return InitiateProbing(at_time_ms, {2 * bitrate_bps}, true);
  }
  return std::vector<ProbeClusterConfig>();
}

void ProbeController::Reset(int64_t at_time_ms) {
  network_available_ = true;
  state_ = State::kInit;
  min_bitrate_to_probe_further_bps_ = kExponentialProbingDisabled;
  time_last_probing_initiated_ms_ = 0;
  estimated_bitrate_bps_ = 0;
  start_bitrate_bps_ = 0;
  max_bitrate_bps_ = 0;
  int64_t now_ms = at_time_ms;
  last_bwe_drop_probing_time_ms_ = now_ms;
  alr_end_time_ms_.reset();
  mid_call_probing_waiting_for_result_ = false;
  time_of_last_large_drop_ms_ = now_ms;
  bitrate_before_last_large_drop_bps_ = 0;
  max_total_allocated_bitrate_ = 0;
}

std::vector<ProbeClusterConfig> ProbeController::Process(int64_t at_time_ms) {
  int64_t now_ms = at_time_ms;

  if (now_ms - time_last_probing_initiated_ms_ >
      kMaxWaitingTimeForProbingResultMs) {
    mid_call_probing_waiting_for_result_ = false;

    if (state_ == State::kWaitingForProbingResult) {
      RTC_LOG(LS_INFO) << "kWaitingForProbingResult: timeout";
      state_ = State::kProbingComplete;
      min_bitrate_to_probe_further_bps_ = kExponentialProbingDisabled;
    }
  }

  if (enable_periodic_alr_probing_ && state_ == State::kProbingComplete) {
    // Probe bandwidth periodically when in ALR state.
    if (alr_start_time_ms_ && estimated_bitrate_bps_ > 0) {
      int64_t next_probe_time_ms =
          std::max(*alr_start_time_ms_, time_last_probing_initiated_ms_) +
          kAlrPeriodicProbingIntervalMs;
      if (now_ms >= next_probe_time_ms) {
        return InitiateProbing(now_ms, {estimated_bitrate_bps_ * 2}, true);
      }
    }
  }
  return std::vector<ProbeClusterConfig>();
}

std::vector<ProbeClusterConfig> ProbeController::InitiateProbing(
    int64_t now_ms,
    std::initializer_list<int64_t> bitrates_to_probe,
    bool probe_further) {
  std::vector<ProbeClusterConfig> pending_probes;
  for (int64_t bitrate : bitrates_to_probe) {
    RTC_DCHECK_GT(bitrate, 0);
    int64_t max_probe_bitrate_bps =
        max_bitrate_bps_ > 0 ? max_bitrate_bps_ : kDefaultMaxProbingBitrateBps;
    if (bitrate > max_probe_bitrate_bps) {
      bitrate = max_probe_bitrate_bps;
      probe_further = false;
    }

    ProbeClusterConfig config;
    config.at_time = Timestamp::ms(now_ms);
    config.target_data_rate = DataRate::bps(rtc::dchecked_cast<int>(bitrate));
    config.target_duration = TimeDelta::ms(kMinProbeDurationMs);
    config.target_probe_count = kMinProbePacketsSent;
    pending_probes.push_back(config);
  }
  time_last_probing_initiated_ms_ = now_ms;
  if (probe_further) {
    state_ = State::kWaitingForProbingResult;
    min_bitrate_to_probe_further_bps_ =
        (*(bitrates_to_probe.end() - 1)) * kRepeatedProbeMinPercentage / 100;
  } else {
    state_ = State::kProbingComplete;
    min_bitrate_to_probe_further_bps_ = kExponentialProbingDisabled;
  }
  return pending_probes;
}

}  // namespace webrtc
