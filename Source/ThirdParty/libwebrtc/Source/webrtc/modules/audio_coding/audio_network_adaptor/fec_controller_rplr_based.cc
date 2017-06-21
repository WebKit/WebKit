/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/audio_network_adaptor/fec_controller_rplr_based.h"

#include <limits>
#include <utility>

#include "webrtc/base/checks.h"

namespace webrtc {

FecControllerRplrBased::Config::Config(
    bool initial_fec_enabled,
    const ThresholdCurve& fec_enabling_threshold,
    const ThresholdCurve& fec_disabling_threshold)
    : initial_fec_enabled(initial_fec_enabled),
      fec_enabling_threshold(fec_enabling_threshold),
      fec_disabling_threshold(fec_disabling_threshold) {}

FecControllerRplrBased::FecControllerRplrBased(const Config& config)
    : config_(config), fec_enabled_(config.initial_fec_enabled) {
  RTC_DCHECK(config_.fec_disabling_threshold <= config_.fec_enabling_threshold);
}

FecControllerRplrBased::~FecControllerRplrBased() = default;

void FecControllerRplrBased::UpdateNetworkMetrics(
    const NetworkMetrics& network_metrics) {
  if (network_metrics.uplink_bandwidth_bps)
    uplink_bandwidth_bps_ = network_metrics.uplink_bandwidth_bps;
  if (network_metrics.uplink_recoverable_packet_loss_fraction) {
    uplink_recoverable_packet_loss_ =
        network_metrics.uplink_recoverable_packet_loss_fraction;
  }
}

void FecControllerRplrBased::MakeDecision(AudioEncoderRuntimeConfig* config) {
  RTC_DCHECK(!config->enable_fec);
  RTC_DCHECK(!config->uplink_packet_loss_fraction);

  fec_enabled_ = fec_enabled_ ? !FecDisablingDecision() : FecEnablingDecision();

  config->enable_fec = rtc::Optional<bool>(fec_enabled_);
  config->uplink_packet_loss_fraction = rtc::Optional<float>(
      uplink_recoverable_packet_loss_ ? *uplink_recoverable_packet_loss_ : 0.0);
}

bool FecControllerRplrBased::FecEnablingDecision() const {
  if (!uplink_bandwidth_bps_ || !uplink_recoverable_packet_loss_) {
    return false;
  } else {
    // Enable when above the curve or exactly on it.
    return !config_.fec_enabling_threshold.IsBelowCurve(
        {static_cast<float>(*uplink_bandwidth_bps_),
         *uplink_recoverable_packet_loss_});
  }
}

bool FecControllerRplrBased::FecDisablingDecision() const {
  if (!uplink_bandwidth_bps_ || !uplink_recoverable_packet_loss_) {
    return false;
  } else {
    // Disable when below the curve.
    return config_.fec_disabling_threshold.IsBelowCurve(
        {static_cast<float>(*uplink_bandwidth_bps_),
         *uplink_recoverable_packet_loss_});
  }
}

}  // namespace webrtc
