/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/codecs/g711/audio_encoder_pcm.h"

#include <algorithm>
#include <limits>

#include "webrtc/base/checks.h"
#include "webrtc/base/string_to_number.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/audio_coding/codecs/g711/g711_interface.h"

namespace webrtc {

namespace {

template <typename T>
typename T::Config CreateConfig(const CodecInst& codec_inst) {
  typename T::Config config;
  config.frame_size_ms = codec_inst.pacsize / 8;
  config.num_channels = codec_inst.channels;
  config.payload_type = codec_inst.pltype;
  return config;
}

template <typename T>
typename T::Config CreateConfig(int payload_type,
                                const SdpAudioFormat& format) {
  typename T::Config config;
  config.frame_size_ms = 20;
  auto ptime_iter = format.parameters.find("ptime");
  if (ptime_iter != format.parameters.end()) {
    auto ptime = rtc::StringToNumber<int>(ptime_iter->second);
    if (ptime && *ptime > 0) {
      const int whole_packets = *ptime / 10;
      config.frame_size_ms = std::max(10, std::min(whole_packets * 10, 60));
    }
  }
  config.num_channels = format.num_channels;
  config.payload_type = payload_type;
  return config;
}

template <typename T>
rtc::Optional<AudioCodecInfo> QueryAudioEncoderImpl(
    const SdpAudioFormat& format) {
  if (STR_CASE_CMP(format.name.c_str(), T::GetPayloadName()) == 0 &&
      format.clockrate_hz == 8000 && format.num_channels >= 1 &&
      CreateConfig<T>(0, format).IsOk()) {
    return rtc::Optional<AudioCodecInfo>({8000, format.num_channels, 64000});
  }
  return rtc::Optional<AudioCodecInfo>();
}

}  // namespace

bool AudioEncoderPcm::Config::IsOk() const {
  return (frame_size_ms % 10 == 0) && (num_channels >= 1);
}

AudioEncoderPcm::AudioEncoderPcm(const Config& config, int sample_rate_hz)
    : sample_rate_hz_(sample_rate_hz),
      num_channels_(config.num_channels),
      payload_type_(config.payload_type),
      num_10ms_frames_per_packet_(
          static_cast<size_t>(config.frame_size_ms / 10)),
      full_frame_samples_(
          config.num_channels * config.frame_size_ms * sample_rate_hz / 1000),
      first_timestamp_in_buffer_(0) {
  RTC_CHECK_GT(sample_rate_hz, 0) << "Sample rate must be larger than 0 Hz";
  RTC_CHECK_EQ(config.frame_size_ms % 10, 0)
      << "Frame size must be an integer multiple of 10 ms.";
  speech_buffer_.reserve(full_frame_samples_);
}

AudioEncoderPcm::~AudioEncoderPcm() = default;

int AudioEncoderPcm::SampleRateHz() const {
  return sample_rate_hz_;
}

size_t AudioEncoderPcm::NumChannels() const {
  return num_channels_;
}

size_t AudioEncoderPcm::Num10MsFramesInNextPacket() const {
  return num_10ms_frames_per_packet_;
}

size_t AudioEncoderPcm::Max10MsFramesInAPacket() const {
  return num_10ms_frames_per_packet_;
}

int AudioEncoderPcm::GetTargetBitrate() const {
  return static_cast<int>(
      8 * BytesPerSample() * SampleRateHz() * NumChannels());
}

AudioEncoder::EncodedInfo AudioEncoderPcm::EncodeImpl(
    uint32_t rtp_timestamp,
    rtc::ArrayView<const int16_t> audio,
    rtc::Buffer* encoded) {
  if (speech_buffer_.empty()) {
    first_timestamp_in_buffer_ = rtp_timestamp;
  }
  speech_buffer_.insert(speech_buffer_.end(), audio.begin(), audio.end());
  if (speech_buffer_.size() < full_frame_samples_) {
    return EncodedInfo();
  }
  RTC_CHECK_EQ(speech_buffer_.size(), full_frame_samples_);
  EncodedInfo info;
  info.encoded_timestamp = first_timestamp_in_buffer_;
  info.payload_type = payload_type_;
  info.encoded_bytes =
      encoded->AppendData(full_frame_samples_ * BytesPerSample(),
                          [&] (rtc::ArrayView<uint8_t> encoded) {
                            return EncodeCall(&speech_buffer_[0],
                                              full_frame_samples_,
                                              encoded.data());
                          });
  speech_buffer_.clear();
  info.encoder_type = GetCodecType();
  return info;
}

void AudioEncoderPcm::Reset() {
  speech_buffer_.clear();
}

AudioEncoderPcmA::AudioEncoderPcmA(const CodecInst& codec_inst)
    : AudioEncoderPcmA(CreateConfig<AudioEncoderPcmA>(codec_inst)) {}

AudioEncoderPcmA::AudioEncoderPcmA(int payload_type,
                                   const SdpAudioFormat& format)
    : AudioEncoderPcmA(CreateConfig<AudioEncoderPcmA>(payload_type, format)) {}

rtc::Optional<AudioCodecInfo> AudioEncoderPcmA::QueryAudioEncoder(
    const SdpAudioFormat& format) {
  return QueryAudioEncoderImpl<AudioEncoderPcmA>(format);
}

size_t AudioEncoderPcmA::EncodeCall(const int16_t* audio,
                                    size_t input_len,
                                    uint8_t* encoded) {
  return WebRtcG711_EncodeA(audio, input_len, encoded);
}

size_t AudioEncoderPcmA::BytesPerSample() const {
  return 1;
}

AudioEncoder::CodecType AudioEncoderPcmA::GetCodecType() const {
  return AudioEncoder::CodecType::kPcmA;
}

AudioEncoderPcmU::AudioEncoderPcmU(const CodecInst& codec_inst)
    : AudioEncoderPcmU(CreateConfig<AudioEncoderPcmU>(codec_inst)) {}

AudioEncoderPcmU::AudioEncoderPcmU(int payload_type,
                                   const SdpAudioFormat& format)
    : AudioEncoderPcmU(CreateConfig<AudioEncoderPcmU>(payload_type, format)) {}

rtc::Optional<AudioCodecInfo> AudioEncoderPcmU::QueryAudioEncoder(
    const SdpAudioFormat& format) {
  return QueryAudioEncoderImpl<AudioEncoderPcmU>(format);
}

size_t AudioEncoderPcmU::EncodeCall(const int16_t* audio,
                                    size_t input_len,
                                    uint8_t* encoded) {
  return WebRtcG711_EncodeU(audio, input_len, encoded);
}

size_t AudioEncoderPcmU::BytesPerSample() const {
  return 1;
}

AudioEncoder::CodecType AudioEncoderPcmU::GetCodecType() const {
  return AudioEncoder::CodecType::kPcmU;
}

}  // namespace webrtc
