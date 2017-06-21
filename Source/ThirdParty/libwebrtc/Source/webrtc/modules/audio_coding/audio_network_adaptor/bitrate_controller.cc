/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/audio_network_adaptor/bitrate_controller.h"

#include <algorithm>

#include "webrtc/base/checks.h"
#include "webrtc/system_wrappers/include/field_trial.h"

namespace webrtc {
namespace audio_network_adaptor {

BitrateController::Config::Config(int initial_bitrate_bps,
                                  int initial_frame_length_ms)
    : initial_bitrate_bps(initial_bitrate_bps),
      initial_frame_length_ms(initial_frame_length_ms) {}

BitrateController::Config::~Config() = default;

BitrateController::BitrateController(const Config& config)
    : config_(config),
      bitrate_bps_(config_.initial_bitrate_bps),
      frame_length_ms_(config_.initial_frame_length_ms) {
  RTC_DCHECK_GT(bitrate_bps_, 0);
  RTC_DCHECK_GT(frame_length_ms_, 0);
}

BitrateController::~BitrateController() = default;

void BitrateController::UpdateNetworkMetrics(
    const NetworkMetrics& network_metrics) {
  if (network_metrics.target_audio_bitrate_bps)
    target_audio_bitrate_bps_ = network_metrics.target_audio_bitrate_bps;
  if (network_metrics.overhead_bytes_per_packet)
    overhead_bytes_per_packet_ = network_metrics.overhead_bytes_per_packet;
}

void BitrateController::MakeDecision(AudioEncoderRuntimeConfig* config) {
  // Decision on |bitrate_bps| should not have been made.
  RTC_DCHECK(!config->bitrate_bps);
  if (target_audio_bitrate_bps_ && overhead_bytes_per_packet_) {
    // Current implementation of BitrateController can only work when
    // |metrics.target_audio_bitrate_bps| includes overhead is enabled. This is
    // currently governed by the following field trial.
    RTC_DCHECK(
        webrtc::field_trial::IsEnabled("WebRTC-SendSideBwe-WithOverhead"));
    if (config->frame_length_ms)
      frame_length_ms_ = *config->frame_length_ms;
    int overhead_rate_bps =
        *overhead_bytes_per_packet_ * 8 * 1000 / frame_length_ms_;
    bitrate_bps_ = std::max(0, *target_audio_bitrate_bps_ - overhead_rate_bps);
  }
  config->bitrate_bps = rtc::Optional<int>(bitrate_bps_);
}

}  // namespace audio_network_adaptor
}  // namespace webrtc
