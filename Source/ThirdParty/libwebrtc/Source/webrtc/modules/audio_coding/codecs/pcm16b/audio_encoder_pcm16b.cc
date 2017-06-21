/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/codecs/pcm16b/audio_encoder_pcm16b.h"

#include <algorithm>

#include "webrtc/base/checks.h"
#include "webrtc/base/safe_conversions.h"
#include "webrtc/base/string_to_number.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/audio_coding/codecs/pcm16b/pcm16b.h"

namespace webrtc {

size_t AudioEncoderPcm16B::EncodeCall(const int16_t* audio,
                                      size_t input_len,
                                      uint8_t* encoded) {
  return WebRtcPcm16b_Encode(audio, input_len, encoded);
}

size_t AudioEncoderPcm16B::BytesPerSample() const {
  return 2;
}

AudioEncoder::CodecType AudioEncoderPcm16B::GetCodecType() const {
  return CodecType::kOther;
}

namespace {
AudioEncoderPcm16B::Config CreateConfig(const CodecInst& codec_inst) {
  AudioEncoderPcm16B::Config config;
  config.num_channels = codec_inst.channels;
  config.sample_rate_hz = codec_inst.plfreq;
  config.frame_size_ms = rtc::CheckedDivExact(
      codec_inst.pacsize, rtc::CheckedDivExact(config.sample_rate_hz, 1000));
  config.payload_type = codec_inst.pltype;
  return config;
}

AudioEncoderPcm16B::Config CreateConfig(int payload_type,
                                        const SdpAudioFormat& format) {
  AudioEncoderPcm16B::Config config;
  config.num_channels = format.num_channels;
  config.sample_rate_hz = format.clockrate_hz;
  config.frame_size_ms = 10;
  auto ptime_iter = format.parameters.find("ptime");
  if (ptime_iter != format.parameters.end()) {
    auto ptime = rtc::StringToNumber<int>(ptime_iter->second);
    if (ptime && *ptime > 0) {
      const int whole_packets = *ptime / 10;
      config.frame_size_ms = std::max(10, std::min(whole_packets * 10, 60));
    }
  }
  config.payload_type = payload_type;
  return config;
}
}  // namespace

bool AudioEncoderPcm16B::Config::IsOk() const {
  if ((sample_rate_hz != 8000) && (sample_rate_hz != 16000) &&
      (sample_rate_hz != 32000) && (sample_rate_hz != 48000))
    return false;
  return AudioEncoderPcm::Config::IsOk();
}

AudioEncoderPcm16B::AudioEncoderPcm16B(const CodecInst& codec_inst)
    : AudioEncoderPcm16B(CreateConfig(codec_inst)) {}

AudioEncoderPcm16B::AudioEncoderPcm16B(int payload_type,
                                       const SdpAudioFormat& format)
    : AudioEncoderPcm16B(CreateConfig(payload_type, format)) {}

rtc::Optional<AudioCodecInfo> AudioEncoderPcm16B::QueryAudioEncoder(
    const SdpAudioFormat& format) {
  if (STR_CASE_CMP(format.name.c_str(), GetPayloadName()) == 0 &&
      format.num_channels >= 1) {
    Config config = CreateConfig(0, format);
    if (config.IsOk()) {
      constexpr int bits_per_sample = 16;
      return rtc::Optional<AudioCodecInfo>(
          {config.sample_rate_hz, config.num_channels,
           config.sample_rate_hz * bits_per_sample *
               rtc::dchecked_cast<int>(config.num_channels)});
    }
  }
  return rtc::Optional<AudioCodecInfo>();
}

}  // namespace webrtc
