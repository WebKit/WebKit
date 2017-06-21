/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/audio_codecs/g722/audio_decoder_g722.h"

#include <memory>
#include <vector>

#include "webrtc/base/ptr_util.h"
#include "webrtc/base/safe_conversions.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/audio_coding/codecs/g722/audio_decoder_g722.h"

namespace webrtc {

rtc::Optional<AudioDecoderG722::Config> AudioDecoderG722::SdpToConfig(
    const SdpAudioFormat& format) {
  return STR_CASE_CMP(format.name.c_str(), "g722") == 0 &&
                 format.clockrate_hz == 8000
             ? rtc::Optional<Config>(Config())
             : rtc::Optional<Config>();
}

void AudioDecoderG722::AppendSupportedDecoders(
    std::vector<AudioCodecSpec>* specs) {
  specs->push_back({{"g722", 8000, 1}, {16000, 1, 64000}});
}

std::unique_ptr<AudioDecoder> AudioDecoderG722::MakeAudioDecoder(
    Config config) {
  return rtc::MakeUnique<AudioDecoderG722Impl>();
}

}  // namespace webrtc
