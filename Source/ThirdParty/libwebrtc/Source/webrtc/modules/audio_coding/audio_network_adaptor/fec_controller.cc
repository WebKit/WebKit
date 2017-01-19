/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/audio_network_adaptor/fec_controller.h"

#include <limits>
#include <utility>

#include "webrtc/base/checks.h"

namespace webrtc {

FecController::Config::Threshold::Threshold(int low_bandwidth_bps,
                                            float low_bandwidth_packet_loss,
                                            int high_bandwidth_bps,
                                            float high_bandwidth_packet_loss)
    : low_bandwidth_bps(low_bandwidth_bps),
      low_bandwidth_packet_loss(low_bandwidth_packet_loss),
      high_bandwidth_bps(high_bandwidth_bps),
      high_bandwidth_packet_loss(high_bandwidth_packet_loss) {}

FecController::Config::Config(bool initial_fec_enabled,
                              const Threshold& fec_enabling_threshold,
                              const Threshold& fec_disabling_threshold,
                              int time_constant_ms,
                              const Clock* clock)
    : initial_fec_enabled(initial_fec_enabled),
      fec_enabling_threshold(fec_enabling_threshold),
      fec_disabling_threshold(fec_disabling_threshold),
      time_constant_ms(time_constant_ms),
      clock(clock) {}

FecController::FecController(const Config& config)
    : config_(config),
      fec_enabled_(config.initial_fec_enabled),
      packet_loss_smoothed_(
          new SmoothingFilterImpl(config_.time_constant_ms, config_.clock)),
      fec_enabling_threshold_info_(config_.fec_enabling_threshold),
      fec_disabling_threshold_info_(config_.fec_disabling_threshold) {
  RTC_DCHECK_LE(fec_enabling_threshold_info_.slope, 0);
  RTC_DCHECK_LE(fec_enabling_threshold_info_.slope, 0);
  RTC_DCHECK_LE(
      GetPacketLossThreshold(config_.fec_enabling_threshold.low_bandwidth_bps,
                             config_.fec_disabling_threshold,
                             fec_disabling_threshold_info_),
      config_.fec_enabling_threshold.low_bandwidth_packet_loss);
  RTC_DCHECK_LE(
      GetPacketLossThreshold(config_.fec_enabling_threshold.high_bandwidth_bps,
                             config_.fec_disabling_threshold,
                             fec_disabling_threshold_info_),
      config_.fec_enabling_threshold.high_bandwidth_packet_loss);
}

FecController::FecController(const Config& config,
                             std::unique_ptr<SmoothingFilter> smoothing_filter)
    : FecController(config) {
  packet_loss_smoothed_ = std::move(smoothing_filter);
}

FecController::~FecController() = default;

void FecController::MakeDecision(
    const NetworkMetrics& metrics,
    AudioNetworkAdaptor::EncoderRuntimeConfig* config) {
  RTC_DCHECK(!config->enable_fec);
  RTC_DCHECK(!config->uplink_packet_loss_fraction);

  if (metrics.uplink_packet_loss_fraction)
    packet_loss_smoothed_->AddSample(*metrics.uplink_packet_loss_fraction);

  fec_enabled_ = fec_enabled_ ? !FecDisablingDecision(metrics)
                              : FecEnablingDecision(metrics);

  config->enable_fec = rtc::Optional<bool>(fec_enabled_);

  auto packet_loss_fraction = packet_loss_smoothed_->GetAverage();
  config->uplink_packet_loss_fraction = rtc::Optional<float>(
      packet_loss_fraction ? *packet_loss_fraction : 0.0);
}

FecController::ThresholdInfo::ThresholdInfo(
    const Config::Threshold& threshold) {
  int bandwidth_diff_bps =
      threshold.high_bandwidth_bps - threshold.low_bandwidth_bps;
  float packet_loss_diff = threshold.high_bandwidth_packet_loss -
                           threshold.low_bandwidth_packet_loss;
  slope = bandwidth_diff_bps == 0 ? 0.0 : packet_loss_diff / bandwidth_diff_bps;
  offset =
      threshold.low_bandwidth_packet_loss - slope * threshold.low_bandwidth_bps;
}

float FecController::GetPacketLossThreshold(
    int bandwidth_bps,
    const Config::Threshold& threshold,
    const ThresholdInfo& threshold_info) const {
  if (bandwidth_bps < threshold.low_bandwidth_bps)
    return std::numeric_limits<float>::max();
  if (bandwidth_bps >= threshold.high_bandwidth_bps)
    return threshold.high_bandwidth_packet_loss;
  return threshold_info.offset + threshold_info.slope * bandwidth_bps;
}

bool FecController::FecEnablingDecision(const NetworkMetrics& metrics) const {
  if (!metrics.uplink_bandwidth_bps)
    return false;

  auto packet_loss = packet_loss_smoothed_->GetAverage();
  if (!packet_loss)
    return false;

  return *packet_loss >= GetPacketLossThreshold(*metrics.uplink_bandwidth_bps,
                                                config_.fec_enabling_threshold,
                                                fec_enabling_threshold_info_);
}

bool FecController::FecDisablingDecision(const NetworkMetrics& metrics) const {
  if (!metrics.uplink_bandwidth_bps)
    return false;

  auto packet_loss = packet_loss_smoothed_->GetAverage();
  if (!packet_loss)
    return false;

  return *packet_loss <= GetPacketLossThreshold(*metrics.uplink_bandwidth_bps,
                                                config_.fec_disabling_threshold,
                                                fec_disabling_threshold_info_);
}

}  // namespace webrtc
