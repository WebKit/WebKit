/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio_codecs/opus/audio_encoder_opus.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "api/audio_codecs/audio_encoder.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/audio_codecs/audio_format.h"
#include "api/audio_codecs/opus/audio_encoder_opus_config.h"
#include "api/environment/environment.h"
#include "modules/audio_coding/codecs/opus/audio_encoder_opus.h"
#include "rtc_base/checks.h"

namespace webrtc {

std::optional<AudioEncoderOpusConfig> AudioEncoderOpus::SdpToConfig(
    const SdpAudioFormat& format) {
  return AudioEncoderOpusImpl::SdpToConfig(format);
}

void AudioEncoderOpus::AppendSupportedEncoders(
    std::vector<AudioCodecSpec>* specs) {
  AudioEncoderOpusImpl::AppendSupportedEncoders(specs);
}

AudioCodecInfo AudioEncoderOpus::QueryAudioEncoder(
    const AudioEncoderOpusConfig& config) {
  return AudioEncoderOpusImpl::QueryAudioEncoder(config);
}

std::unique_ptr<AudioEncoder> AudioEncoderOpus::MakeAudioEncoder(
    const Environment& env,
    AudioEncoderOpusConfig config,
    const AudioEncoderFactory::Options& options) {
  if (!config.IsOk()) {
    RTC_DCHECK_NOTREACHED();
    return nullptr;
  }
  return std::make_unique<AudioEncoderOpusImpl>(env, std::move(config),
                                                options.payload_type);
}

}  // namespace webrtc
