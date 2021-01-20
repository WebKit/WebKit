/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <array>
#include <map>
#include <memory>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "api/audio_codecs/isac/audio_decoder_isac_fix.h"
#include "api/audio_codecs/isac/audio_decoder_isac_float.h"
#include "api/audio_codecs/isac/audio_encoder_isac_fix.h"
#include "api/audio_codecs/isac/audio_encoder_isac_float.h"
#include "modules/audio_coding/test/PCMFile.h"
#include "rtc_base/checks.h"
#include "rtc_base/strings/string_builder.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {
namespace {

constexpr int kPayloadType = 42;

enum class IsacImpl { kFixed, kFloat };

absl::string_view IsacImplToString(IsacImpl impl) {
  switch (impl) {
    case IsacImpl::kFixed:
      return "fixed";
    case IsacImpl::kFloat:
      return "float";
  }
}

std::unique_ptr<PCMFile> GetPcmTestFileReader(int sample_rate_hz) {
  std::string filename;
  switch (sample_rate_hz) {
    case 16000:
      filename = test::ResourcePath("audio_coding/testfile16kHz", "pcm");
      break;
    case 32000:
      filename = test::ResourcePath("audio_coding/testfile32kHz", "pcm");
      break;
    default:
      RTC_NOTREACHED() << "No test file available for " << sample_rate_hz
                       << " Hz.";
  }
  auto pcm_file = std::make_unique<PCMFile>();
  pcm_file->ReadStereo(false);
  pcm_file->Open(filename, sample_rate_hz, "rb", /*auto_rewind=*/true);
  pcm_file->FastForward(/*num_10ms_blocks=*/100);  // Skip initial silence.
  RTC_CHECK(!pcm_file->EndOfFile());
  return pcm_file;
}

// Returns a view to the interleaved samples of an AudioFrame object.
rtc::ArrayView<const int16_t> AudioFrameToView(const AudioFrame& audio_frame) {
  return {audio_frame.data(),
          audio_frame.samples_per_channel() * audio_frame.num_channels()};
}

std::unique_ptr<AudioEncoder> CreateEncoder(IsacImpl impl,
                                            int sample_rate_hz,
                                            int frame_size_ms,
                                            int bitrate_bps) {
  RTC_CHECK(sample_rate_hz == 16000 || sample_rate_hz == 32000);
  RTC_CHECK(frame_size_ms == 30 || frame_size_ms == 60);
  RTC_CHECK_GT(bitrate_bps, 0);
  switch (impl) {
    case IsacImpl::kFixed: {
      AudioEncoderIsacFix::Config config;
      config.bit_rate = bitrate_bps;
      config.frame_size_ms = frame_size_ms;
      RTC_CHECK_EQ(16000, sample_rate_hz);
      return AudioEncoderIsacFix::MakeAudioEncoder(config, kPayloadType);
    }
    case IsacImpl::kFloat: {
      AudioEncoderIsacFloat::Config config;
      config.bit_rate = bitrate_bps;
      config.frame_size_ms = frame_size_ms;
      config.sample_rate_hz = sample_rate_hz;
      return AudioEncoderIsacFloat::MakeAudioEncoder(config, kPayloadType);
    }
  }
}

std::unique_ptr<AudioDecoder> CreateDecoder(IsacImpl impl, int sample_rate_hz) {
  RTC_CHECK(sample_rate_hz == 16000 || sample_rate_hz == 32000);
  switch (impl) {
    case IsacImpl::kFixed: {
      webrtc::AudioDecoderIsacFix::Config config;
      RTC_CHECK_EQ(16000, sample_rate_hz);
      return webrtc::AudioDecoderIsacFix::MakeAudioDecoder(config);
    }
    case IsacImpl::kFloat: {
      webrtc::AudioDecoderIsacFloat::Config config;
      config.sample_rate_hz = sample_rate_hz;
      return webrtc::AudioDecoderIsacFloat::MakeAudioDecoder(config);
    }
  }
}

struct EncoderTestParams {
  IsacImpl impl;
  int sample_rate_hz;
  int frame_size_ms;
};

class EncoderTest : public testing::TestWithParam<EncoderTestParams> {
 protected:
  EncoderTest() = default;
  IsacImpl GetIsacImpl() const { return GetParam().impl; }
  int GetSampleRateHz() const { return GetParam().sample_rate_hz; }
  int GetFrameSizeMs() const { return GetParam().frame_size_ms; }
};

TEST_P(EncoderTest, TestConfig) {
  for (int bitrate_bps : {10000, 21000, 32000}) {
    SCOPED_TRACE(bitrate_bps);
    auto encoder = CreateEncoder(GetIsacImpl(), GetSampleRateHz(),
                                 GetFrameSizeMs(), bitrate_bps);
    EXPECT_EQ(GetSampleRateHz(), encoder->SampleRateHz());
    EXPECT_EQ(size_t{1}, encoder->NumChannels());
    EXPECT_EQ(bitrate_bps, encoder->GetTargetBitrate());
  }
}

// Encodes an input audio sequence with a low and a high target bitrate and
// checks that the number of produces bytes in the first case is less than that
// of the second case.
TEST_P(EncoderTest, TestDifferentBitrates) {
  auto pcm_file = GetPcmTestFileReader(GetSampleRateHz());
  constexpr int kLowBps = 20000;
  constexpr int kHighBps = 25000;
  auto encoder_low = CreateEncoder(GetIsacImpl(), GetSampleRateHz(),
                                   GetFrameSizeMs(), kLowBps);
  auto encoder_high = CreateEncoder(GetIsacImpl(), GetSampleRateHz(),
                                    GetFrameSizeMs(), kHighBps);
  int num_bytes_low = 0;
  int num_bytes_high = 0;
  constexpr int kNumFrames = 12;
  for (int i = 0; i < kNumFrames; ++i) {
    AudioFrame in;
    pcm_file->Read10MsData(in);
    rtc::Buffer low, high;
    encoder_low->Encode(/*rtp_timestamp=*/0, AudioFrameToView(in), &low);
    encoder_high->Encode(/*rtp_timestamp=*/0, AudioFrameToView(in), &high);
    num_bytes_low += low.size();
    num_bytes_high += high.size();
  }
  EXPECT_LT(num_bytes_low, num_bytes_high);
}

// Encodes an input audio sequence first with a low, then with a high target
// bitrate *using the same encoder* and checks that the number of emitted bytes
// in the first case is less than in the second case.
TEST_P(EncoderTest, TestDynamicBitrateChange) {
  constexpr int kLowBps = 20000;
  constexpr int kHighBps = 25000;
  constexpr int kStartBps = 30000;
  auto encoder = CreateEncoder(GetIsacImpl(), GetSampleRateHz(),
                               GetFrameSizeMs(), kStartBps);
  std::map<int, int> num_bytes;
  constexpr int kNumFrames = 200;  // 2 seconds.
  for (int bitrate_bps : {kLowBps, kHighBps}) {
    auto pcm_file = GetPcmTestFileReader(GetSampleRateHz());
    encoder->OnReceivedTargetAudioBitrate(bitrate_bps);
    for (int i = 0; i < kNumFrames; ++i) {
      AudioFrame in;
      pcm_file->Read10MsData(in);
      rtc::Buffer buf;
      encoder->Encode(/*rtp_timestamp=*/0, AudioFrameToView(in), &buf);
      num_bytes[bitrate_bps] += buf.size();
    }
  }
  // kHighBps / kLowBps == 1.25, so require the high-bitrate run to produce at
  // least 1.2 times the number of bytes.
  EXPECT_LT(1.2 * num_bytes[kLowBps], num_bytes[kHighBps]);
}

// Checks that, given a target bitrate, the encoder does not overshoot too much.
TEST_P(EncoderTest, DoNotOvershootTargetBitrate) {
  for (int bitrate_bps : {10000, 15000, 20000, 26000, 32000}) {
    SCOPED_TRACE(bitrate_bps);
    auto pcm_file = GetPcmTestFileReader(GetSampleRateHz());
    auto e = CreateEncoder(GetIsacImpl(), GetSampleRateHz(), GetFrameSizeMs(),
                           bitrate_bps);
    int num_bytes = 0;
    constexpr int kNumFrames = 200;  // 2 seconds.
    for (int i = 0; i < kNumFrames; ++i) {
      AudioFrame in;
      pcm_file->Read10MsData(in);
      rtc::Buffer encoded;
      e->Encode(/*rtp_timestamp=*/0, AudioFrameToView(in), &encoded);
      num_bytes += encoded.size();
    }
    // Inverse of the duration of |kNumFrames| 10 ms frames (unit: seconds^-1).
    constexpr float kAudioDurationInv = 100.f / kNumFrames;
    const int measured_bitrate_bps = 8 * num_bytes * kAudioDurationInv;
    EXPECT_LT(measured_bitrate_bps, bitrate_bps + 2000);  // Max 2 kbps extra.
  }
}

// Creates tests for different encoder configurations and implementations.
INSTANTIATE_TEST_SUITE_P(
    IsacApiTest,
    EncoderTest,
    ::testing::ValuesIn([] {
      std::vector<EncoderTestParams> cases;
      for (IsacImpl impl : {IsacImpl::kFloat, IsacImpl::kFixed}) {
        for (int frame_size_ms : {30, 60}) {
          cases.push_back({impl, 16000, frame_size_ms});
        }
      }
      cases.push_back({IsacImpl::kFloat, 32000, 30});
      return cases;
    }()),
    [](const ::testing::TestParamInfo<EncoderTestParams>& info) {
      rtc::StringBuilder b;
      const auto& p = info.param;
      b << IsacImplToString(p.impl) << "_" << p.sample_rate_hz << "_"
        << p.frame_size_ms;
      return b.Release();
    });

struct DecoderTestParams {
  IsacImpl impl;
  int sample_rate_hz;
};

class DecoderTest : public testing::TestWithParam<DecoderTestParams> {
 protected:
  DecoderTest() = default;
  IsacImpl GetIsacImpl() const { return GetParam().impl; }
  int GetSampleRateHz() const { return GetParam().sample_rate_hz; }
};

TEST_P(DecoderTest, TestConfig) {
  auto decoder = CreateDecoder(GetIsacImpl(), GetSampleRateHz());
  EXPECT_EQ(GetSampleRateHz(), decoder->SampleRateHz());
  EXPECT_EQ(size_t{1}, decoder->Channels());
}

// Creates tests for different decoder configurations and implementations.
INSTANTIATE_TEST_SUITE_P(
    IsacApiTest,
    DecoderTest,
    ::testing::ValuesIn({DecoderTestParams{IsacImpl::kFixed, 16000},
                         DecoderTestParams{IsacImpl::kFloat, 16000},
                         DecoderTestParams{IsacImpl::kFloat, 32000}}),
    [](const ::testing::TestParamInfo<DecoderTestParams>& info) {
      const auto& p = info.param;
      return (rtc::StringBuilder()
              << IsacImplToString(p.impl) << "_" << p.sample_rate_hz)
          .Release();
    });

struct EncoderDecoderPairTestParams {
  int sample_rate_hz;
  int frame_size_ms;
  IsacImpl encoder_impl;
  IsacImpl decoder_impl;
};

class EncoderDecoderPairTest
    : public testing::TestWithParam<EncoderDecoderPairTestParams> {
 protected:
  EncoderDecoderPairTest() = default;
  int GetSampleRateHz() const { return GetParam().sample_rate_hz; }
  int GetEncoderFrameSizeMs() const { return GetParam().frame_size_ms; }
  IsacImpl GetEncoderIsacImpl() const { return GetParam().encoder_impl; }
  IsacImpl GetDecoderIsacImpl() const { return GetParam().decoder_impl; }
  int GetEncoderFrameSize() const {
    return GetEncoderFrameSizeMs() * GetSampleRateHz() / 1000;
  }
};

// Checks that the number of encoded and decoded samples match.
TEST_P(EncoderDecoderPairTest, EncodeDecode) {
  auto pcm_file = GetPcmTestFileReader(GetSampleRateHz());
  auto encoder = CreateEncoder(GetEncoderIsacImpl(), GetSampleRateHz(),
                               GetEncoderFrameSizeMs(), /*bitrate_bps=*/20000);
  auto decoder = CreateDecoder(GetDecoderIsacImpl(), GetSampleRateHz());
  const int encoder_frame_size = GetEncoderFrameSize();
  std::vector<int16_t> out(encoder_frame_size);
  size_t num_encoded_samples = 0;
  size_t num_decoded_samples = 0;
  constexpr int kNumFrames = 12;
  for (int i = 0; i < kNumFrames; ++i) {
    AudioFrame in;
    pcm_file->Read10MsData(in);
    rtc::Buffer encoded;
    encoder->Encode(/*rtp_timestamp=*/0, AudioFrameToView(in), &encoded);
    num_encoded_samples += in.samples_per_channel();
    if (encoded.empty()) {
      continue;
    }
    // Decode.
    const std::vector<AudioDecoder::ParseResult> parse_result =
        decoder->ParsePayload(std::move(encoded), /*timestamp=*/0);
    EXPECT_EQ(parse_result.size(), size_t{1});
    auto decode_result = parse_result[0].frame->Decode(out);
    EXPECT_TRUE(decode_result.has_value());
    EXPECT_EQ(out.size(), decode_result->num_decoded_samples);
    num_decoded_samples += decode_result->num_decoded_samples;
  }
  EXPECT_EQ(num_encoded_samples, num_decoded_samples);
}

// Creates tests for different encoder frame sizes and different
// encoder/decoder implementations.
INSTANTIATE_TEST_SUITE_P(
    IsacApiTest,
    EncoderDecoderPairTest,
    ::testing::ValuesIn([] {
      std::vector<EncoderDecoderPairTestParams> cases;
      for (int frame_size_ms : {30, 60}) {
        for (IsacImpl enc : {IsacImpl::kFloat, IsacImpl::kFixed}) {
          for (IsacImpl dec : {IsacImpl::kFloat, IsacImpl::kFixed}) {
            cases.push_back({16000, frame_size_ms, enc, dec});
          }
        }
      }
      cases.push_back({32000, 30, IsacImpl::kFloat, IsacImpl::kFloat});
      return cases;
    }()),
    [](const ::testing::TestParamInfo<EncoderDecoderPairTestParams>& info) {
      rtc::StringBuilder b;
      const auto& p = info.param;
      b << p.sample_rate_hz << "_" << p.frame_size_ms << "_"
        << IsacImplToString(p.encoder_impl) << "_"
        << IsacImplToString(p.decoder_impl);
      return b.Release();
    });

}  // namespace
}  // namespace webrtc
