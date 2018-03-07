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

#include <utility>

#include "modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor_config.h"

namespace webrtc {

RtcEventAudioNetworkAdaptation::RtcEventAudioNetworkAdaptation(
    std::unique_ptr<AudioEncoderRuntimeConfig> config)
    : config_(std::move(config)) {}

RtcEventAudioNetworkAdaptation::~RtcEventAudioNetworkAdaptation() = default;

RtcEvent::Type RtcEventAudioNetworkAdaptation::GetType() const {
  return RtcEvent::Type::AudioNetworkAdaptation;
}

bool RtcEventAudioNetworkAdaptation::IsConfigEvent() const {
  return false;
}

}  // namespace webrtc
