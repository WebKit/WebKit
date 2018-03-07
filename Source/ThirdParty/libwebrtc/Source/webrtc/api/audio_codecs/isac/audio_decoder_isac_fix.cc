/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio_codecs/isac/audio_decoder_isac_fix.h"

#include "common_types.h"  // NOLINT(build/include)
#include "modules/audio_coding/codecs/isac/fix/include/audio_decoder_isacfix.h"
#include "rtc_base/ptr_util.h"

namespace webrtc {

rtc::Optional<AudioDecoderIsacFix::Config> AudioDecoderIsacFix::SdpToConfig(
    const SdpAudioFormat& format) {
  return STR_CASE_CMP(format.name.c_str(), "ISAC") == 0 &&
                 format.clockrate_hz == 16000 && format.num_channels == 1
             ? rtc::Optional<Config>(Config())
             : rtc::nullopt;
}

void AudioDecoderIsacFix::AppendSupportedDecoders(
    std::vector<AudioCodecSpec>* specs) {
  specs->push_back({{"ISAC", 16000, 1}, {16000, 1, 32000, 10000, 32000}});
}

std::unique_ptr<AudioDecoder> AudioDecoderIsacFix::MakeAudioDecoder(
    Config config) {
  return rtc::MakeUnique<AudioDecoderIsacFixImpl>(16000);
}

}  // namespace webrtc
