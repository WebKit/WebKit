/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/audio_codecs/g722/audio_encoder_g722.h"

#include <memory>
#include <vector>

#include "webrtc/base/ptr_util.h"
#include "webrtc/base/safe_conversions.h"
#include "webrtc/modules/audio_coding/codecs/g722/audio_encoder_g722.h"

namespace webrtc {

rtc::Optional<AudioEncoderG722Config> AudioEncoderG722::SdpToConfig(
    const SdpAudioFormat& format) {
  return AudioEncoderG722Impl::SdpToConfig(format);
}

void AudioEncoderG722::AppendSupportedEncoders(
    std::vector<AudioCodecSpec>* specs) {
  const SdpAudioFormat fmt = {"g722", 8000, 1};
  const AudioCodecInfo info = QueryAudioEncoder(*SdpToConfig(fmt));
  specs->push_back({fmt, info});
}

AudioCodecInfo AudioEncoderG722::QueryAudioEncoder(
    const AudioEncoderG722Config& config) {
  RTC_DCHECK(config.IsOk());
  return {16000, rtc::dchecked_cast<size_t>(config.num_channels),
          64000 * config.num_channels};
}

std::unique_ptr<AudioEncoder> AudioEncoderG722::MakeAudioEncoder(
    const AudioEncoderG722Config& config,
    int payload_type) {
  RTC_DCHECK(config.IsOk());
  return rtc::MakeUnique<AudioEncoderG722Impl>(config, payload_type);
}

}  // namespace webrtc
