/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio_codecs/L16/audio_encoder_L16.h"

#include "common_types.h"  // NOLINT(build/include)
#include "modules/audio_coding/codecs/pcm16b/audio_encoder_pcm16b.h"
#include "modules/audio_coding/codecs/pcm16b/pcm16b_common.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/ptr_util.h"

namespace webrtc {

rtc::Optional<AudioEncoderL16::Config> AudioEncoderL16::SdpToConfig(
    const SdpAudioFormat& format) {
  if (!rtc::IsValueInRangeForNumericType<int>(format.num_channels)) {
    return rtc::nullopt;
  }
  Config config;
  config.sample_rate_hz = format.clockrate_hz;
  config.num_channels = rtc::dchecked_cast<int>(format.num_channels);
  return STR_CASE_CMP(format.name.c_str(), "L16") == 0 && config.IsOk()
             ? rtc::Optional<Config>(config)
             : rtc::nullopt;
}

void AudioEncoderL16::AppendSupportedEncoders(
    std::vector<AudioCodecSpec>* specs) {
  Pcm16BAppendSupportedCodecSpecs(specs);
}

AudioCodecInfo AudioEncoderL16::QueryAudioEncoder(
    const AudioEncoderL16::Config& config) {
  RTC_DCHECK(config.IsOk());
  return {config.sample_rate_hz,
          rtc::dchecked_cast<size_t>(config.num_channels),
          config.sample_rate_hz * config.num_channels * 16};
}

std::unique_ptr<AudioEncoder> AudioEncoderL16::MakeAudioEncoder(
    const AudioEncoderL16::Config& config,
    int payload_type) {
  RTC_DCHECK(config.IsOk());
  AudioEncoderPcm16B::Config c;
  c.sample_rate_hz = config.sample_rate_hz;
  c.num_channels = config.num_channels;
  c.frame_size_ms = config.frame_size_ms;
  c.payload_type = payload_type;
  return rtc::MakeUnique<AudioEncoderPcm16B>(c);
}

}  // namespace webrtc
