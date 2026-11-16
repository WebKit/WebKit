/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/events/rtc_event_audio_network_adaptation.h"

#include <memory>

#include "absl/memory/memory.h"
#include "modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor_config.h"

namespace webrtc {

RtcEventAudioNetworkAdaptation::RtcEventAudioNetworkAdaptation(
    const AudioEncoderRuntimeConfig& config)
    : bitrate_bps_(config.bitrate_bps),
      frame_length_ms_(config.frame_length_ms),
      uplink_packet_loss_fraction_(config.uplink_packet_loss_fraction),
      enable_fec_(config.enable_fec),
      enable_dtx_(config.enable_dtx),
      num_channels_(config.num_channels) {}

RtcEventAudioNetworkAdaptation::~RtcEventAudioNetworkAdaptation() = default;

std::unique_ptr<RtcEventAudioNetworkAdaptation>
RtcEventAudioNetworkAdaptation::Copy() const {
  return absl::WrapUnique(new RtcEventAudioNetworkAdaptation(*this));
}

}  // namespace webrtc
