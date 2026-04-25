/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio_codecs/opus/audio_decoder_opus.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/match.h"
#include "api/audio_codecs/audio_decoder.h"
#include "api/audio_codecs/audio_format.h"
#include "api/environment/environment.h"
#include "api/field_trials_view.h"
#include "modules/audio_coding/codecs/opus/audio_decoder_opus.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace {

int GetDefaultNumChannels(const FieldTrialsView& field_trials) {
  return field_trials.IsEnabled("WebRTC-Audio-OpusDecodeStereoByDefault") ? 2
                                                                          : 1;
}

}  // namespace

std::optional<AudioDecoderOpus::Config> AudioDecoderOpus::SdpToConfig(
    const SdpAudioFormat& format) {
  if (!absl::EqualsIgnoreCase(format.name, "opus") ||
      format.clockrate_hz != 48000 || format.num_channels != 2) {
    return std::nullopt;
  }

  Config config;

  // Parse the "stereo" codec parameter. If set, it overrides the default number
  // of channels.
  const auto stereo_param = format.parameters.find("stereo");
  if (stereo_param != format.parameters.end()) {
    if (stereo_param->second == "0") {
      config.num_channels = 1;
    } else if (stereo_param->second == "1") {
      config.num_channels = 2;
    } else {
      // Malformed stereo parameter.
      return std::nullopt;
    }
  }

  if (!config.IsOk()) {
    RTC_DCHECK_NOTREACHED();
    return std::nullopt;
  }
  return config;
}

void AudioDecoderOpus::AppendSupportedDecoders(
    std::vector<AudioCodecSpec>* specs) {
  AudioCodecInfo opus_info{48000, 1, 64000, 6000, 510000};
  opus_info.allow_comfort_noise = false;
  opus_info.supports_network_adaption = true;
  SdpAudioFormat opus_format(
      {"opus", 48000, 2, {{"minptime", "10"}, {"useinbandfec", "1"}}});
  specs->push_back({.format = std::move(opus_format), .info = opus_info});
}

std::unique_ptr<AudioDecoder> AudioDecoderOpus::MakeAudioDecoder(
    const Environment& env,
    Config config) {
  if (!config.IsOk()) {
    RTC_DCHECK_NOTREACHED();
    return nullptr;
  }
  return std::make_unique<AudioDecoderOpusImpl>(
      env.field_trials(),
      config.num_channels.value_or(GetDefaultNumChannels(env.field_trials())),
      config.sample_rate_hz);
}

}  // namespace webrtc
