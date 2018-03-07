/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio_codecs/isac/audio_decoder_isac_float.h"

#include "common_types.h"  // NOLINT(build/include)
#include "modules/audio_coding/codecs/isac/main/include/audio_decoder_isac.h"
#include "rtc_base/ptr_util.h"

namespace webrtc {

rtc::Optional<AudioDecoderIsacFloat::Config> AudioDecoderIsacFloat::SdpToConfig(
    const SdpAudioFormat& format) {
  if (STR_CASE_CMP(format.name.c_str(), "ISAC") == 0 &&
      (format.clockrate_hz == 16000 || format.clockrate_hz == 32000) &&
      format.num_channels == 1) {
    Config config;
    config.sample_rate_hz = format.clockrate_hz;
    return config;
  } else {
    return rtc::nullopt;
  }
}

void AudioDecoderIsacFloat::AppendSupportedDecoders(
    std::vector<AudioCodecSpec>* specs) {
  specs->push_back({{"ISAC", 16000, 1}, {16000, 1, 32000, 10000, 32000}});
  specs->push_back({{"ISAC", 32000, 1}, {32000, 1, 56000, 10000, 56000}});
}

std::unique_ptr<AudioDecoder> AudioDecoderIsacFloat::MakeAudioDecoder(
    Config config) {
  RTC_DCHECK(config.IsOk());
  return rtc::MakeUnique<AudioDecoderIsacFloatImpl>(config.sample_rate_hz);
}

}  // namespace webrtc
