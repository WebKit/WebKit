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
#include "webrtc/system_wrappers/include/metrics.h"

namespace webrtc {

namespace {

// Number of deltas between probes per cluster. On the very first cluster,
// we will need kProbeDeltasPerCluster + 1 probes, but on a cluster following
// another, we need kProbeDeltasPerCluster probes.
constexpr int kProbeDeltasPerCluster = 5;

// Maximum waiting time from the time of initiating probing to getting
// the measured results back.
constexpr int64_t kMaxWaitingTimeForProbingResultMs = 1000;

// Value of |min_bitrate_to_probe_further_bps_| that indicates
// further probing is disabled.
constexpr int kExponentialProbingDisabled = 0;

// Default probing bitrate limit. Applied only when the application didn't
// specify max bitrate.
constexpr int kDefaultMaxProbingBitrateBps = 100000000;

// This is a limit on how often probing can be done when there is a BW
// drop detected in ALR region.
constexpr int kAlrProbingIntervalLimitMs = 5000;

}  // namespace

ProbeController::ProbeController(PacedSender* pacer, Clock* clock)
    : pacer_(pacer),
      clock_(clock),
      state_(State::kInit),
      min_bitrate_to_probe_further_bps_(kExponentialProbingDisabled),
      time_last_probing_initiated_ms_(0),
      estimated_bitrate_bps_(0),
      max_bitrate_bps_(0),
      last_alr_probing_time_(clock_->TimeInMilliseconds()) {}

void ProbeController::SetBitrates(int min_bitrate_bps,
                                  int start_bitrate_bps,
                                  int max_bitrate_bps) {
  rtc::CritScope cs(&critsect_);
  if (state_ == State::kInit) {
    // When probing at 1.8 Mbps ( 6x 300), this represents a threshold of
    // 1.2 Mbps to continue probing.
    InitiateProbing({3 * start_bitrate_bps, 6 * start_bitrate_bps},
                    4 * start_bitrate_bps);
  }

  int old_max_bitrate_bps = max_bitrate_bps_;
  max_bitrate_bps_ = max_bitrate_bps;

  // Only do probing if:
  //   we are mid-call, which we consider to be if
  //     exponential probing is not active and
  //     |estimated_bitrate_bps_| is valid (> 0) and
  //     the current bitrate is lower than the new |max_bitrate_bps|, and
  //     |max_bitrate_bps_| was increased.
  if (state_ != State::kWaitingForProbingResult &&
      estimated_bitrate_bps_ != 0 &&
      estimated_bitrate_bps_ < old_max_bitrate_bps &&
      max_bitrate_bps_ > old_max_bitrate_bps) {
    InitiateProbing({max_bitrate_bps}, kExponentialProbingDisabled);
  }
}

void ProbeController::SetEstimatedBitrate(int bitrate_bps) {
  rtc::CritScope cs(&critsect_);
  if (state_ == State::kWaitingForProbingResult) {
    if ((clock_->TimeInMilliseconds() - time_last_probing_initiated_ms_) >
        kMaxWaitingTimeForProbingResultMs) {
      LOG(LS_INFO) << "kWaitingForProbingResult: timeout";
      state_ = State::kProbingComplete;
      min_bitrate_to_probe_further_bps_ = kExponentialProbingDisabled;
    } else {
      // Continue probing if probing results indicate channel has greater
      // capacity.
      LOG(LS_INFO) << "Measured bitrate: " << bitrate_bps
                   << " Minimum to probe further: "
                   << min_bitrate_to_probe_further_bps_;
      if (min_bitrate_to_probe_further_bps_ != kExponentialProbingDisabled &&
          bitrate_bps > min_bitrate_to_probe_further_bps_) {
        // Double the probing bitrate and expect a minimum of 25% gain to
        // continue probing.
        InitiateProbing({2 * bitrate_bps}, 1.25 * bitrate_bps);
      }
    }
  } else {
    // A drop in estimated BW when operating in ALR and not already probing.
    // The current response is to initiate a single probe session at the
    // previous bitrate and immediately use the reported bitrate as the new
    // bitrate.
    //
    // If the probe session fails, the assumption is that this drop was a
    // real one from a competing flow or something else on the network and
    // it ramps up from bitrate_bps.
    if (pacer_->InApplicationLimitedRegion() &&
        bitrate_bps < 0.5 * estimated_bitrate_bps_) {
      int64_t now_ms = clock_->TimeInMilliseconds();
      if ((now_ms - last_alr_probing_time_) > kAlrProbingIntervalLimitMs) {
        LOG(LS_INFO) << "Detected big BW drop in ALR, start probe.";
        // Track how often we probe in response to BW drop in ALR.
        RTC_HISTOGRAM_COUNTS_10000("WebRTC.BWE.AlrProbingIntervalInS",
                                   (now_ms - last_alr_probing_time_) / 1000);
        InitiateProbing({estimated_bitrate_bps_}, kExponentialProbingDisabled);
        last_alr_probing_time_ = now_ms;
      }
    }
    // TODO(isheriff): May want to track when we did ALR probing in order
    // to reset |last_alr_probing_time_| if we validate that it was a
    // drop due to exogenous event.
  }
  estimated_bitrate_bps_ = bitrate_bps;
}

void ProbeController::InitiateProbing(
    std::initializer_list<int> bitrates_to_probe,
    int min_bitrate_to_probe_further_bps) {
  bool first_cluster = true;
  for (int bitrate : bitrates_to_probe) {
    int max_probe_bitrate_bps =
        max_bitrate_bps_ > 0 ? max_bitrate_bps_ : kDefaultMaxProbingBitrateBps;
    bitrate = std::min(bitrate, max_probe_bitrate_bps);
    if (first_cluster) {
      pacer_->CreateProbeCluster(bitrate, kProbeDeltasPerCluster + 1);
      first_cluster = false;
    } else {
      pacer_->CreateProbeCluster(bitrate, kProbeDeltasPerCluster);
    }
  }
  min_bitrate_to_probe_further_bps_ = min_bitrate_to_probe_further_bps;
  time_last_probing_initiated_ms_ = clock_->TimeInMilliseconds();
  if (min_bitrate_to_probe_further_bps == kExponentialProbingDisabled)
    state_ = State::kProbingComplete;
  else
    state_ = State::kWaitingForProbingResult;
}

}  // namespace webrtc
