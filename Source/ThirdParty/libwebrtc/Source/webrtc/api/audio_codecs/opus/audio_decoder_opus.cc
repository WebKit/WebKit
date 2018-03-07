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

#include <memory>
#include <utility>
#include <vector>

#include "common_types.h"  // NOLINT(build/include)
#include "modules/audio_coding/codecs/opus/audio_decoder_opus.h"
#include "rtc_base/ptr_util.h"

namespace webrtc {

rtc::Optional<AudioDecoderOpus::Config> AudioDecoderOpus::SdpToConfig(
    const SdpAudioFormat& format) {
  const auto num_channels = [&]() -> rtc::Optional<int> {
    auto stereo = format.parameters.find("stereo");
    if (stereo != format.parameters.end()) {
      if (stereo->second == "0") {
        return 1;
      } else if (stereo->second == "1") {
        return 2;
      } else {
        return rtc::nullopt;  // Bad stereo parameter.
      }
    }
    return 1;  // Default to mono.
  }();
  if (STR_CASE_CMP(format.name.c_str(), "opus") == 0 &&
      format.clockrate_hz == 48000 && format.num_channels == 2 &&
      num_channels) {
    return Config{*num_channels};
  } else {
    return rtc::nullopt;
  }
}

void AudioDecoderOpus::AppendSupportedDecoders(
    std::vector<AudioCodecSpec>* specs) {
  AudioCodecInfo opus_info{48000, 1, 64000, 6000, 510000};
  opus_info.allow_comfort_noise = false;
  opus_info.supports_network_adaption = true;
  SdpAudioFormat opus_format(
      {"opus", 48000, 2, {{"minptime", "10"}, {"useinbandfec", "1"}}});
  specs->push_back({std::move(opus_format), std::move(opus_info)});
}

std::unique_ptr<AudioDecoder> AudioDecoderOpus::MakeAudioDecoder(
    Config config) {
  return rtc::MakeUnique<AudioDecoderOpusImpl>(config.num_channels);
}

}  // namespace webrtc
