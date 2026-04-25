/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/codecs/legacy_encoded_audio_frame.h"

#include <cstddef>
#include <cstdint>

#include "api/array_view.h"
#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "test/gtest.h"

namespace webrtc {

enum class NetEqDecoder {
  kDecoderPCMu,
  kDecoderPCMa,
  kDecoderPCMu_2ch,
  kDecoderPCMa_2ch,
  kDecoderPCM16B,
  kDecoderPCM16Bwb,
  kDecoderPCM16Bswb32kHz,
  kDecoderPCM16Bswb48kHz,
  kDecoderPCM16B_2ch,
  kDecoderPCM16Bwb_2ch,
  kDecoderPCM16Bswb32kHz_2ch,
  kDecoderPCM16Bswb48kHz_2ch,
  kDecoderPCM16B_5ch,
  kDecoderG722,
};

class SplitBySamplesTest : public ::testing::TestWithParam<NetEqDecoder> {
 protected:
  void SetUp() override {
    decoder_type_ = GetParam();
    switch (decoder_type_) {
      case NetEqDecoder::kDecoderPCMu:
      case NetEqDecoder::kDecoderPCMa:
        bytes_per_ms_ = 8;
        samples_per_ms_ = 8;
        break;
      case NetEqDecoder::kDecoderPCMu_2ch:
      case NetEqDecoder::kDecoderPCMa_2ch:
        bytes_per_ms_ = 2 * 8;
        samples_per_ms_ = 8;
        break;
      case NetEqDecoder::kDecoderG722:
        bytes_per_ms_ = 8;
        samples_per_ms_ = 16;
        break;
      case NetEqDecoder::kDecoderPCM16B:
        bytes_per_ms_ = 16;
        samples_per_ms_ = 8;
        break;
      case NetEqDecoder::kDecoderPCM16Bwb:
        bytes_per_ms_ = 32;
        samples_per_ms_ = 16;
        break;
      case NetEqDecoder::kDecoderPCM16Bswb32kHz:
        bytes_per_ms_ = 64;
        samples_per_ms_ = 32;
        break;
      case NetEqDecoder::kDecoderPCM16Bswb48kHz:
        bytes_per_ms_ = 96;
        samples_per_ms_ = 48;
        break;
      case NetEqDecoder::kDecoderPCM16B_2ch:
        bytes_per_ms_ = 2 * 16;
        samples_per_ms_ = 8;
        break;
      case NetEqDecoder::kDecoderPCM16Bwb_2ch:
        bytes_per_ms_ = 2 * 32;
        samples_per_ms_ = 16;
        break;
      case NetEqDecoder::kDecoderPCM16Bswb32kHz_2ch:
        bytes_per_ms_ = 2 * 64;
        samples_per_ms_ = 32;
        break;
      case NetEqDecoder::kDecoderPCM16Bswb48kHz_2ch:
        bytes_per_ms_ = 2 * 96;
        samples_per_ms_ = 48;
        break;
      case NetEqDecoder::kDecoderPCM16B_5ch:
        bytes_per_ms_ = 5 * 16;
        samples_per_ms_ = 8;
        break;
      default:
        RTC_DCHECK_NOTREACHED();
        break;
    }
  }
  size_t bytes_per_ms_;
  int samples_per_ms_;
  NetEqDecoder decoder_type_;
};

// Test splitting sample-based payloads.
TEST_P(SplitBySamplesTest, PayloadSizes) {
  constexpr uint32_t kBaseTimestamp = 0x12345678;
  struct ExpectedSplit {
    size_t payload_size_ms;
    size_t num_frames;
    // For simplicity. We only expect up to two packets per split.
    size_t frame_sizes[2];
  };
  // The payloads are expected to be split as follows:
  // 10 ms -> 10 ms
  // 20 ms -> 20 ms
  // 30 ms -> 30 ms
  // 40 ms -> 20 + 20 ms
  // 50 ms -> 25 + 25 ms
  // 60 ms -> 30 + 30 ms
  ExpectedSplit expected_splits[] = {
      {.payload_size_ms = 10, .num_frames = 1, .frame_sizes = {10}},
      {.payload_size_ms = 20, .num_frames = 1, .frame_sizes = {20}},
      {.payload_size_ms = 30, .num_frames = 1, .frame_sizes = {30}},
      {.payload_size_ms = 40, .num_frames = 2, .frame_sizes = {20, 20}},
      {.payload_size_ms = 50, .num_frames = 2, .frame_sizes = {25, 25}},
      {.payload_size_ms = 60, .num_frames = 2, .frame_sizes = {30, 30}}};

  for (const auto& expected_split : expected_splits) {
    // The payload values are set to steadily increase (modulo 256), so that the
    // resulting frames can be checked and we can be reasonably certain no
    // sample was missed or repeated.
    const auto generate_payload = [](size_t num_bytes) {
      Buffer payload = Buffer::CreateWithCapacity(num_bytes);
      payload.AppendData(num_bytes, [](ArrayView<uint8_t> payload_view) {
        uint8_t value = 0;
        // Allow wrap-around of value in counter below.
        for (size_t i = 0; i != payload_view.size(); ++i, ++value) {
          payload_view[i] = value;
        }
        return payload_view.size();
      });
      return payload;
    };

    const auto results = LegacyEncodedAudioFrame::SplitBySamples(
        nullptr,
        generate_payload(expected_split.payload_size_ms * bytes_per_ms_),
        kBaseTimestamp, bytes_per_ms_, samples_per_ms_);

    EXPECT_EQ(expected_split.num_frames, results.size());
    uint32_t expected_timestamp = kBaseTimestamp;
    uint8_t value = 0;
    for (size_t i = 0; i != expected_split.num_frames; ++i) {
      const auto& result = results[i];
      const LegacyEncodedAudioFrame* frame =
          static_cast<const LegacyEncodedAudioFrame*>(result.frame.get());
      const size_t length_bytes = expected_split.frame_sizes[i] * bytes_per_ms_;
      EXPECT_EQ(length_bytes, frame->payload().size());
      EXPECT_EQ(expected_timestamp, result.timestamp);
      const Buffer& payload = frame->payload();
      // Allow wrap-around of value in counter below.
      for (size_t j = 0; j != payload.size(); ++j, ++value) {
        ASSERT_EQ(value, payload[j]);
      }

      expected_timestamp += checked_cast<uint32_t>(
          expected_split.frame_sizes[i] * samples_per_ms_);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    LegacyEncodedAudioFrame,
    SplitBySamplesTest,
    ::testing::Values(NetEqDecoder::kDecoderPCMu,
                      NetEqDecoder::kDecoderPCMa,
                      NetEqDecoder::kDecoderPCMu_2ch,
                      NetEqDecoder::kDecoderPCMa_2ch,
                      NetEqDecoder::kDecoderG722,
                      NetEqDecoder::kDecoderPCM16B,
                      NetEqDecoder::kDecoderPCM16Bwb,
                      NetEqDecoder::kDecoderPCM16Bswb32kHz,
                      NetEqDecoder::kDecoderPCM16Bswb48kHz,
                      NetEqDecoder::kDecoderPCM16B_2ch,
                      NetEqDecoder::kDecoderPCM16Bwb_2ch,
                      NetEqDecoder::kDecoderPCM16Bswb32kHz_2ch,
                      NetEqDecoder::kDecoderPCM16Bswb48kHz_2ch,
                      NetEqDecoder::kDecoderPCM16B_5ch));

}  // namespace webrtc
