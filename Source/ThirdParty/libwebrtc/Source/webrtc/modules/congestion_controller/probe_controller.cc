/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/congestion_controller/probe_controller.h"

#include <algorithm>
#include <initializer_list>

#include "webrtc/base/logging.h"
#include "webrtc/base/safe_conversions.h"
#include "webrtc/system_wrappers/include/metrics.h"

namespace webrtc {

namespace {
// Maximum waiting time from the time of initiating probing to getting
// the measured results back.
constexpr int64_t kMaxWaitingTimeForProbingResultMs = 1000;

// Value of |min_bitrate_to_probe_further_bps_| that indicates
// further probing is disabled.
constexpr int kExponentialProbingDisabled = 0;

// Default probing bitrate limit. Applied only when the application didn't
// specify max bitrate.
constexpr int64_t kDefaultMaxProbingBitrateBps = 5000000;

// This is a limit on how often probing can be done when there is a BW
// drop detected in ALR.
constexpr int64_t kAlrProbingIntervalMinMs = 5000;

// Interval between probes when ALR periodic probing is enabled.
constexpr int64_t kAlrPeriodicProbingIntervalMs = 5000;

// Minimum probe bitrate percentage to probe further for repeated probes,
// relative to the previous probe. For example, if 1Mbps probe results in
// 80kbps, then we'll probe again at 1.6Mbps. In that case second probe won't be
// sent if we get 600kbps from the first one.
constexpr int kRepeatedProbeMinPercentage = 70;

}  // namespace

ProbeController::ProbeController(PacedSender* pacer, Clock* clock)
    : pacer_(pacer),
      clock_(clock),
      network_state_(kNetworkUp),
      state_(State::kInit),
      min_bitrate_to_probe_further_bps_(kExponentialProbingDisabled),
      time_last_probing_initiated_ms_(0),
      estimated_bitrate_bps_(0),
      start_bitrate_bps_(0),
      max_bitrate_bps_(0),
      last_alr_probing_time_(clock_->TimeInMilliseconds()),
      enable_periodic_alr_probing_(false),
      mid_call_probing_waiting_for_result_(false) {}

void ProbeController::SetBitrates(int64_t min_bitrate_bps,
                                  int64_t start_bitrate_bps,
                                  int64_t max_bitrate_bps) {
  rtc::CritScope cs(&critsect_);

  if (start_bitrate_bps > 0)  {
    start_bitrate_bps_ = start_bitrate_bps;
  } else if (start_bitrate_bps_ == 0) {
    start_bitrate_bps_ = min_bitrate_bps;
  }

  // The reason we use the variable |old_max_bitrate_pbs| is because we
  // need to set |max_bitrate_bps_| before we call InitiateProbing.
  int64_t old_max_bitrate_bps = max_bitrate_bps_;
  max_bitrate_bps_ = max_bitrate_bps;

  switch (state_) {
    case State::kInit:
      if (network_state_ == kNetworkUp)
        InitiateExponentialProbing();
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

        InitiateProbing(clock_->TimeInMilliseconds(), {max_bitrate_bps}, false);
      }
      break;
  }
}

void ProbeController::OnNetworkStateChanged(NetworkState network_state) {
  rtc::CritScope cs(&critsect_);
  network_state_ = network_state;
  if (network_state_ == kNetworkUp && state_ == State::kInit)
    InitiateExponentialProbing();
}

void ProbeController::InitiateExponentialProbing() {
  RTC_DCHECK(network_state_ == kNetworkUp);
  RTC_DCHECK(state_ == State::kInit);
  RTC_DCHECK_GT(start_bitrate_bps_, 0);

  // When probing at 1.8 Mbps ( 6x 300), this represents a threshold of
  // 1.2 Mbps to continue probing.
  InitiateProbing(clock_->TimeInMilliseconds(),
                  {3 * start_bitrate_bps_, 6 * start_bitrate_bps_}, true);
}

void ProbeController::SetEstimatedBitrate(int64_t bitrate_bps) {
  rtc::CritScope cs(&critsect_);
  int64_t now_ms = clock_->TimeInMilliseconds();

  if (mid_call_probing_waiting_for_result_ &&
      bitrate_bps >= mid_call_probing_succcess_threshold_) {
    RTC_HISTOGRAM_COUNTS_10000("WebRTC.BWE.MidCallProbing.Success",
                               mid_call_probing_bitrate_bps_ / 1000);
    RTC_HISTOGRAM_COUNTS_10000("WebRTC.BWE.MidCallProbing.ProbedKbps",
                               bitrate_bps / 1000);
    mid_call_probing_waiting_for_result_ = false;
  }

  if (state_ == State::kWaitingForProbingResult) {
    // Continue probing if probing results indicate channel has greater
    // capacity.
    LOG(LS_INFO) << "Measured bitrate: " << bitrate_bps
                 << " Minimum to probe further: "
                 << min_bitrate_to_probe_further_bps_;

    if (min_bitrate_to_probe_further_bps_ != kExponentialProbingDisabled &&
        bitrate_bps > min_bitrate_to_probe_further_bps_) {
      // Double the probing bitrate.
      InitiateProbing(now_ms, {2 * bitrate_bps}, true);
    }
  }

  // Detect a drop in estimated BW when operating in ALR and not already
  // probing. The current response is to initiate a single probe session at the
  // previous bitrate and immediately use the reported bitrate as the new
  // bitrate.
  //
  // If the probe session fails, the assumption is that this drop was a
  // real one from a competing flow or something else on the network and
  // it ramps up from bitrate_bps.
  if (state_ == State::kProbingComplete &&
      pacer_->GetApplicationLimitedRegionStartTime() &&
      bitrate_bps < 2 * estimated_bitrate_bps_ / 3 &&
      (now_ms - last_alr_probing_time_) > kAlrProbingIntervalMinMs) {
    LOG(LS_INFO) << "Detected big BW drop in ALR, start probe.";
    // Track how often we probe in response to BW drop in ALR.
    RTC_HISTOGRAM_COUNTS_10000("WebRTC.BWE.AlrProbingIntervalInS",
                               (now_ms - last_alr_probing_time_) / 1000);
    InitiateProbing(now_ms, {estimated_bitrate_bps_}, false);
    last_alr_probing_time_ = now_ms;

    // TODO(isheriff): May want to track when we did ALR probing in order
    // to reset |last_alr_probing_time_| if we validate that it was a
    // drop due to exogenous event.
  }

  estimated_bitrate_bps_ = bitrate_bps;
}

void ProbeController::EnablePeriodicAlrProbing(bool enable) {
  rtc::CritScope cs(&critsect_);
  enable_periodic_alr_probing_ = enable;
}

void ProbeController::Process() {
  rtc::CritScope cs(&critsect_);

  int64_t now_ms = clock_->TimeInMilliseconds();

  if (now_ms - time_last_probing_initiated_ms_ >
      kMaxWaitingTimeForProbingResultMs) {
    mid_call_probing_waiting_for_result_ = false;

    if (state_ == State::kWaitingForProbingResult) {
      LOG(LS_INFO) << "kWaitingForProbingResult: timeout";
      state_ = State::kProbingComplete;
      min_bitrate_to_probe_further_bps_ = kExponentialProbingDisabled;
    }
  }

  if (state_ != State::kProbingComplete || !enable_periodic_alr_probing_)
    return;

  // Probe bandwidth periodically when in ALR state.
  rtc::Optional<int64_t> alr_start_time =
      pacer_->GetApplicationLimitedRegionStartTime();
  if (alr_start_time) {
    int64_t next_probe_time_ms =
        std::max(*alr_start_time, time_last_probing_initiated_ms_) +
        kAlrPeriodicProbingIntervalMs;
    if (now_ms >= next_probe_time_ms) {
      InitiateProbing(now_ms, {estimated_bitrate_bps_ * 2}, true);
    }
  }
}

void ProbeController::InitiateProbing(
    int64_t now_ms,
    std::initializer_list<int64_t> bitrates_to_probe,
    bool probe_further) {
  for (int64_t bitrate : bitrates_to_probe) {
    int64_t max_probe_bitrate_bps =
        max_bitrate_bps_ > 0 ? max_bitrate_bps_ : kDefaultMaxProbingBitrateBps;
    if (bitrate > max_probe_bitrate_bps) {
      bitrate = max_probe_bitrate_bps;
      probe_further = false;
    }
    pacer_->CreateProbeCluster(rtc::dchecked_cast<int>(bitrate));
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
}

}  // namespace webrtc
