/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/codecs/opus/audio_decoder_opus.h"

#include <memory>
#include <utility>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "rtc_base/checks.h"

namespace webrtc {

namespace {
class OpusFrame : public AudioDecoder::EncodedAudioFrame {
 public:
  OpusFrame(AudioDecoderOpusImpl* decoder,
            rtc::Buffer&& payload,
            bool is_primary_payload)
      : decoder_(decoder),
        payload_(std::move(payload)),
        is_primary_payload_(is_primary_payload) {}

  size_t Duration() const override {
    int ret;
    if (is_primary_payload_) {
      ret = decoder_->PacketDuration(payload_.data(), payload_.size());
    } else {
      ret = decoder_->PacketDurationRedundant(payload_.data(), payload_.size());
    }
    return (ret < 0) ? 0 : static_cast<size_t>(ret);
  }

  bool IsDtxPacket() const override { return payload_.size() <= 2; }

  absl::optional<DecodeResult> Decode(
      rtc::ArrayView<int16_t> decoded) const override {
    AudioDecoder::SpeechType speech_type = AudioDecoder::kSpeech;
    int ret;
    if (is_primary_payload_) {
      ret = decoder_->Decode(
          payload_.data(), payload_.size(), decoder_->SampleRateHz(),
          decoded.size() * sizeof(int16_t), decoded.data(), &speech_type);
    } else {
      ret = decoder_->DecodeRedundant(
          payload_.data(), payload_.size(), decoder_->SampleRateHz(),
          decoded.size() * sizeof(int16_t), decoded.data(), &speech_type);
    }

    if (ret < 0)
      return absl::nullopt;

    return DecodeResult{static_cast<size_t>(ret), speech_type};
  }

 private:
  AudioDecoderOpusImpl* const decoder_;
  const rtc::Buffer payload_;
  const bool is_primary_payload_;
};

}  // namespace

AudioDecoderOpusImpl::AudioDecoderOpusImpl(size_t num_channels)
    : channels_(num_channels) {
  RTC_DCHECK(num_channels == 1 || num_channels == 2);
  WebRtcOpus_DecoderCreate(&dec_state_, channels_);
  WebRtcOpus_DecoderInit(dec_state_);
}

AudioDecoderOpusImpl::~AudioDecoderOpusImpl() {
  WebRtcOpus_DecoderFree(dec_state_);
}

std::vector<AudioDecoder::ParseResult> AudioDecoderOpusImpl::ParsePayload(
    rtc::Buffer&& payload,
    uint32_t timestamp) {
  std::vector<ParseResult> results;

  if (PacketHasFec(payload.data(), payload.size())) {
    const int duration =
        PacketDurationRedundant(payload.data(), payload.size());
    RTC_DCHECK_GE(duration, 0);
    rtc::Buffer payload_copy(payload.data(), payload.size());
    std::unique_ptr<EncodedAudioFrame> fec_frame(
        new OpusFrame(this, std::move(payload_copy), false));
    results.emplace_back(timestamp - duration, 1, std::move(fec_frame));
  }
  std::unique_ptr<EncodedAudioFrame> frame(
      new OpusFrame(this, std::move(payload), true));
  results.emplace_back(timestamp, 0, std::move(frame));
  return results;
}

int AudioDecoderOpusImpl::DecodeInternal(const uint8_t* encoded,
                                         size_t encoded_len,
                                         int sample_rate_hz,
                                         int16_t* decoded,
                                         SpeechType* speech_type) {
  RTC_DCHECK_EQ(sample_rate_hz, 48000);
  int16_t temp_type = 1;  // Default is speech.
  int ret =
      WebRtcOpus_Decode(dec_state_, encoded, encoded_len, decoded, &temp_type);
  if (ret > 0)
    ret *= static_cast<int>(channels_);  // Return total number of samples.
  *speech_type = ConvertSpeechType(temp_type);
  return ret;
}

int AudioDecoderOpusImpl::DecodeRedundantInternal(const uint8_t* encoded,
                                                  size_t encoded_len,
                                                  int sample_rate_hz,
                                                  int16_t* decoded,
                                                  SpeechType* speech_type) {
  if (!PacketHasFec(encoded, encoded_len)) {
    // This packet is a RED packet.
    return DecodeInternal(encoded, encoded_len, sample_rate_hz, decoded,
                          speech_type);
  }

  RTC_DCHECK_EQ(sample_rate_hz, 48000);
  int16_t temp_type = 1;  // Default is speech.
  int ret = WebRtcOpus_DecodeFec(dec_state_, encoded, encoded_len, decoded,
                                 &temp_type);
  if (ret > 0)
    ret *= static_cast<int>(channels_);  // Return total number of samples.
  *speech_type = ConvertSpeechType(temp_type);
  return ret;
}

void AudioDecoderOpusImpl::Reset() {
  WebRtcOpus_DecoderInit(dec_state_);
}

int AudioDecoderOpusImpl::PacketDuration(const uint8_t* encoded,
                                         size_t encoded_len) const {
  return WebRtcOpus_DurationEst(dec_state_, encoded, encoded_len);
}

int AudioDecoderOpusImpl::PacketDurationRedundant(const uint8_t* encoded,
                                                  size_t encoded_len) const {
  if (!PacketHasFec(encoded, encoded_len)) {
    // This packet is a RED packet.
    return PacketDuration(encoded, encoded_len);
  }

  return WebRtcOpus_FecDurationEst(encoded, encoded_len);
}

bool AudioDecoderOpusImpl::PacketHasFec(const uint8_t* encoded,
                                        size_t encoded_len) const {
  int fec;
  fec = WebRtcOpus_PacketHasFec(encoded, encoded_len);
  return (fec == 1);
}

int AudioDecoderOpusImpl::SampleRateHz() const {
  return 48000;
}

size_t AudioDecoderOpusImpl::Channels() const {
  return channels_;
}

}  // namespace webrtc
