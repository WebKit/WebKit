/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/tools/fake_decode_from_file.h"

#include "modules/rtp_rtcp/source/byte_io.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_conversions.h"

namespace webrtc {
namespace test {

namespace {

class FakeEncodedFrame : public AudioDecoder::EncodedAudioFrame {
 public:
  FakeEncodedFrame(AudioDecoder* decoder, rtc::Buffer&& payload)
      : decoder_(decoder), payload_(std::move(payload)) {}

  size_t Duration() const override {
    const int ret = decoder_->PacketDuration(payload_.data(), payload_.size());
    return ret < 0 ? 0 : static_cast<size_t>(ret);
  }

  absl::optional<DecodeResult> Decode(
      rtc::ArrayView<int16_t> decoded) const override {
    auto speech_type = AudioDecoder::kSpeech;
    const int ret = decoder_->Decode(
        payload_.data(), payload_.size(), decoder_->SampleRateHz(),
        decoded.size() * sizeof(int16_t), decoded.data(), &speech_type);
    return ret < 0 ? absl::nullopt
                   : absl::optional<DecodeResult>(
                         {static_cast<size_t>(ret), speech_type});
  }

  // This is to mimic OpusFrame.
  bool IsDtxPacket() const override {
    uint32_t original_payload_size_bytes =
        ByteReader<uint32_t>::ReadLittleEndian(&payload_.data()[8]);
    return original_payload_size_bytes <= 2;
  }

 private:
  AudioDecoder* const decoder_;
  const rtc::Buffer payload_;
};

}  // namespace

std::vector<AudioDecoder::ParseResult> FakeDecodeFromFile::ParsePayload(
    rtc::Buffer&& payload,
    uint32_t timestamp) {
  std::vector<ParseResult> results;
  std::unique_ptr<EncodedAudioFrame> frame(
      new FakeEncodedFrame(this, std::move(payload)));
  results.emplace_back(timestamp, 0, std::move(frame));
  return results;
}

int FakeDecodeFromFile::DecodeInternal(const uint8_t* encoded,
                                       size_t encoded_len,
                                       int sample_rate_hz,
                                       int16_t* decoded,
                                       SpeechType* speech_type) {
  RTC_DCHECK_EQ(sample_rate_hz, SampleRateHz());

  const int samples_to_decode = PacketDuration(encoded, encoded_len);
  const int total_samples_to_decode = samples_to_decode * (stereo_ ? 2 : 1);

  if (encoded_len == 0) {
    // Decoder is asked to produce codec-internal comfort noise.
    RTC_DCHECK(!encoded);  // NetEq always sends nullptr in this case.
    RTC_DCHECK(cng_mode_);
    RTC_DCHECK_GT(total_samples_to_decode, 0);
    std::fill_n(decoded, total_samples_to_decode, 0);
    *speech_type = kComfortNoise;
    return rtc::dchecked_cast<int>(total_samples_to_decode);
  }

  RTC_CHECK_GE(encoded_len, 12);
  uint32_t timestamp_to_decode =
      ByteReader<uint32_t>::ReadLittleEndian(encoded);

  if (next_timestamp_from_input_ &&
      timestamp_to_decode != *next_timestamp_from_input_) {
    // A gap in the timestamp sequence is detected. Skip the same number of
    // samples from the file.
    uint32_t jump = timestamp_to_decode - *next_timestamp_from_input_;
    RTC_CHECK(input_->Seek(jump));
  }

  next_timestamp_from_input_ = timestamp_to_decode + samples_to_decode;

  uint32_t original_payload_size_bytes =
      ByteReader<uint32_t>::ReadLittleEndian(&encoded[8]);
  if (original_payload_size_bytes <= 2) {
    // This is a comfort noise payload.
    RTC_DCHECK_GT(total_samples_to_decode, 0);
    std::fill_n(decoded, total_samples_to_decode, 0);
    *speech_type = kComfortNoise;
    cng_mode_ = true;
    return rtc::dchecked_cast<int>(total_samples_to_decode);
  }

  cng_mode_ = false;
  RTC_CHECK(input_->Read(static_cast<size_t>(samples_to_decode), decoded));

  if (stereo_) {
    InputAudioFile::DuplicateInterleaved(decoded, samples_to_decode, 2,
                                         decoded);
  }

  *speech_type = kSpeech;
  last_decoded_length_ = samples_to_decode;
  return rtc::dchecked_cast<int>(total_samples_to_decode);
}

int FakeDecodeFromFile::PacketDuration(const uint8_t* encoded,
                                       size_t encoded_len) const {
  const uint32_t original_payload_size_bytes =
      encoded_len < 8 + sizeof(uint32_t)
          ? 0
          : ByteReader<uint32_t>::ReadLittleEndian(&encoded[8]);
  const uint32_t samples_to_decode =
      encoded_len < 4 + sizeof(uint32_t)
          ? 0
          : ByteReader<uint32_t>::ReadLittleEndian(&encoded[4]);
  if (encoded_len == 0) {
    // Decoder is asked to produce codec-internal comfort noise.
    return rtc::CheckedDivExact(SampleRateHz(), 100);
  }
  bool is_dtx_payload =
      original_payload_size_bytes <= 2 || samples_to_decode == 0;
  bool has_error_duration =
      samples_to_decode % rtc::CheckedDivExact(SampleRateHz(), 100) != 0;
  if (is_dtx_payload || has_error_duration) {
    if (last_decoded_length_ > 0) {
      // Use length of last decoded packet.
      return rtc::dchecked_cast<int>(last_decoded_length_);
    } else {
      // This is the first packet to decode, and we do not know the length of
      // it. Set it to 10 ms.
      return rtc::CheckedDivExact(SampleRateHz(), 100);
    }
  }
  return samples_to_decode;
}

void FakeDecodeFromFile::PrepareEncoded(uint32_t timestamp,
                                        size_t samples,
                                        size_t original_payload_size_bytes,
                                        rtc::ArrayView<uint8_t> encoded) {
  RTC_CHECK_GE(encoded.size(), 12);
  ByteWriter<uint32_t>::WriteLittleEndian(&encoded[0], timestamp);
  ByteWriter<uint32_t>::WriteLittleEndian(&encoded[4],
                                          rtc::checked_cast<uint32_t>(samples));
  ByteWriter<uint32_t>::WriteLittleEndian(
      &encoded[8], rtc::checked_cast<uint32_t>(original_payload_size_bytes));
}

}  // namespace test
}  // namespace webrtc
