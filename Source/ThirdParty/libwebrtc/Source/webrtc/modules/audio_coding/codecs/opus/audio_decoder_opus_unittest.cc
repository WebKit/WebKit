/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/codecs/opus/audio_decoder_opus.h"

#include <cmath>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include "api/array_view.h"
#include "api/audio/audio_frame.h"
#include "api/audio_codecs/audio_decoder.h"
#include "api/audio_codecs/opus/audio_encoder_opus_config.h"
#include "api/environment/environment.h"
#include "api/environment/environment_factory.h"
#include "modules/audio_coding/codecs/opus/audio_encoder_opus.h"
#include "modules/audio_coding/test/PCMFile.h"
#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/random.h"
#include "test/explicit_key_value_config.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {
namespace {

using test::ExplicitKeyValueConfig;
using testing::SizeIs;

using DecodeResult = ::webrtc::AudioDecoder::EncodedAudioFrame::DecodeResult;
using ParseResult = ::webrtc::AudioDecoder::ParseResult;

constexpr int kSampleRateHz = 48000;

constexpr int kInputFrameDurationMs = 10;
constexpr int kInputFrameLength = kInputFrameDurationMs * kSampleRateHz / 1000;

constexpr int kEncoderFrameDurationMs = 20;
constexpr int kEncoderFrameLength =
    kEncoderFrameDurationMs * kSampleRateHz / 1000;

constexpr int kPayloadType = 123;

AudioEncoderOpusConfig GetEncoderConfig(int num_channels, bool dtx_enabled) {
  AudioEncoderOpusConfig config;

  config.frame_size_ms = kEncoderFrameDurationMs;
  config.sample_rate_hz = kSampleRateHz;
  config.num_channels = num_channels;
  config.application = AudioEncoderOpusConfig::ApplicationMode::kVoip;
  config.bitrate_bps = 32000;
  config.fec_enabled = false;
  config.cbr_enabled = false;
  config.max_playback_rate_hz = kSampleRateHz;
  config.complexity = 10;
  config.dtx_enabled = dtx_enabled;

  return config;
}

class WhiteNoiseGenerator {
 public:
  explicit WhiteNoiseGenerator(double amplitude_dbfs)
      : amplitude_(
            rtc::saturated_cast<int16_t>(std::pow(10, amplitude_dbfs / 20) *
                                         std::numeric_limits<int16_t>::max())),
        random_generator_(42) {}

  void GenerateNextFrame(rtc::ArrayView<int16_t> frame) {
    for (size_t i = 0; i < frame.size(); ++i) {
      frame[i] = rtc::saturated_cast<int16_t>(
          random_generator_.Rand(-amplitude_, amplitude_));
    }
  }

 private:
  const int32_t amplitude_;
  Random random_generator_;
};

bool IsZeroedFrame(rtc::ArrayView<const int16_t> audio) {
  for (const int16_t& v : audio) {
    if (v != 0)
      return false;
  }
  return true;
}

bool IsTrivialStereo(rtc::ArrayView<const int16_t> audio) {
  const int num_samples =
      rtc::CheckedDivExact(audio.size(), static_cast<size_t>(2));
  for (int i = 0, j = 0; i < num_samples; ++i, j += 2) {
    if (audio[j] != audio[j + 1]) {
      return false;
    }
  }
  return true;
}

void EncodeDecodeSpeech(AudioEncoderOpusImpl& encoder,
                        AudioDecoderOpusImpl& decoder,
                        uint32_t& rtp_timestamp,
                        uint32_t& timestamp,
                        int max_frames) {
  RTC_CHECK(encoder.NumChannels() == 1 || encoder.NumChannels() == 2);
  const bool stereo_encoding = encoder.NumChannels() == 2;
  const size_t decoder_num_channels = decoder.Channels();
  std::vector<int16_t> decoded_frame(kEncoderFrameLength *
                                     decoder_num_channels);

  PCMFile pcm_file;
  pcm_file.Open(test::ResourcePath(
                    stereo_encoding ? "near48_stereo" : "near48_mono", "pcm"),
                kSampleRateHz, "rb");
  pcm_file.ReadStereo(stereo_encoding);

  AudioFrame audio_frame;
  for (int i = 0; i < max_frames; ++i) {
    if (pcm_file.EndOfFile()) {
      break;
    }
    pcm_file.Read10MsData(audio_frame);
    rtc::Buffer payload;
    encoder.Encode(rtp_timestamp++, audio_frame.data_view().data(), &payload);

    // Ignore empty payloads: the encoder needs more audio to produce a packet.
    if (payload.size() == 0) {
      continue;
    }

    // Decode.
    std::vector<ParseResult> parse_results =
        decoder.ParsePayload(std::move(payload), timestamp++);
    RTC_CHECK_EQ(parse_results.size(), 1);
    std::optional<DecodeResult> decode_results =
        parse_results[0].frame->Decode(decoded_frame);
    RTC_CHECK(decode_results);
    RTC_CHECK_EQ(decode_results->num_decoded_samples, decoded_frame.size());
  }
}

void EncodeDecodeNoiseUntilDecoderInDtxMode(AudioEncoderOpusImpl& encoder,
                                            AudioDecoderOpusImpl& decoder,
                                            uint32_t& rtp_timestamp,
                                            uint32_t& timestamp) {
  WhiteNoiseGenerator generator(/*amplitude_dbfs=*/-70.0);
  std::vector<int16_t> input_frame(kInputFrameLength * encoder.NumChannels());
  const size_t decoder_num_channels = decoder.Channels();
  std::vector<int16_t> decoded_frame(kEncoderFrameLength *
                                     decoder_num_channels);

  for (int i = 0; i < 50; ++i) {
    generator.GenerateNextFrame(input_frame);
    rtc::Buffer payload;
    const AudioEncoder::EncodedInfo info =
        encoder.Encode(rtp_timestamp++, input_frame, &payload);

    // Ignore empty payloads: the encoder needs more audio to produce a packet.
    if (payload.size() == 0) {
      continue;
    }

    // Decode `payload`. If it encodes a DTX packet (i.e., 1 byte payload), the
    // decoder will switch to DTX mode. Otherwise, it may update the internal
    // decoder parameters for comfort noise generation.
    std::vector<ParseResult> parse_results =
        decoder.ParsePayload(std::move(payload), timestamp++);
    RTC_CHECK_EQ(parse_results.size(), 1);
    std::optional<DecodeResult> decode_results =
        parse_results[0].frame->Decode(decoded_frame);
    RTC_CHECK(decode_results);
    RTC_CHECK_EQ(decode_results->num_decoded_samples, decoded_frame.size());
    if (parse_results[0].frame->IsDtxPacket()) {
      return;
    }
  }
  RTC_CHECK_NOTREACHED();
}

// Generates packets by encoding speech frames and decodes them until a non-DTX
// packet is generated and, when that condition is met, returns the decoded
// audio samples.
std::vector<int16_t> EncodeDecodeSpeechUntilOneFrameIsDecoded(
    AudioEncoderOpusImpl& encoder,
    AudioDecoderOpusImpl& decoder,
    uint32_t& rtp_timestamp,
    uint32_t& timestamp) {
  RTC_CHECK(encoder.NumChannels() == 1 || encoder.NumChannels() == 2);
  const bool stereo_encoding = encoder.NumChannels() == 2;
  const size_t decoder_num_channels = decoder.Channels();
  std::vector<int16_t> decoded_frame(kEncoderFrameLength *
                                     decoder_num_channels);

  PCMFile pcm_file;
  pcm_file.Open(test::ResourcePath(
                    stereo_encoding ? "near48_stereo" : "near48_mono", "pcm"),
                kSampleRateHz, "rb");
  pcm_file.ReadStereo(stereo_encoding);

  AudioFrame audio_frame;
  while (true) {
    if (pcm_file.EndOfFile()) {
      break;
    }
    pcm_file.Read10MsData(audio_frame);
    rtc::Buffer payload;
    encoder.Encode(rtp_timestamp++, audio_frame.data_view().data(), &payload);

    // Ignore empty payloads: the encoder needs more audio to produce a packet.
    if (payload.size() == 0) {
      continue;
    }

    // Decode `payload`.
    std::vector<ParseResult> parse_results =
        decoder.ParsePayload(std::move(payload), timestamp++);
    RTC_CHECK_EQ(parse_results.size(), 1);
    std::optional<DecodeResult> decode_results =
        parse_results[0].frame->Decode(decoded_frame);
    RTC_CHECK(decode_results);

    if (parse_results[0].frame->IsDtxPacket()) {
      continue;
    }
    RTC_CHECK_EQ(decode_results->num_decoded_samples, decoded_frame.size());
    return decoded_frame;
  }
  RTC_CHECK_NOTREACHED();
}

}  // namespace

TEST(AudioDecoderOpusTest, MonoEncoderStereoDecoderOutputsTrivialStereo) {
  const Environment env = EnvironmentFactory().Create();
  WhiteNoiseGenerator generator(/*amplitude_dbfs=*/-70.0);
  std::array<int16_t, kInputFrameLength> input_frame;
  // Create a mono encoder.
  const AudioEncoderOpusConfig encoder_config =
      GetEncoderConfig(/*num_channels=*/1, /*dtx_enabled=*/false);
  AudioEncoderOpusImpl encoder(env, encoder_config, kPayloadType);
  // Create a stereo decoder.
  constexpr size_t kDecoderNumChannels = 2;
  AudioDecoderOpusImpl decoder(env.field_trials(), kDecoderNumChannels,
                               kSampleRateHz);
  std::array<int16_t, kEncoderFrameLength * kDecoderNumChannels> decoded_frame;

  uint32_t rtp_timestamp = 0xFFFu;
  uint32_t timestamp = 0;
  for (int i = 0; i < 30; ++i) {
    generator.GenerateNextFrame(input_frame);
    rtc::Buffer payload;
    encoder.Encode(rtp_timestamp++, input_frame, &payload);
    if (payload.size() == 0) {
      continue;
    }

    // Decode.
    std::vector<ParseResult> parse_results =
        decoder.ParsePayload(std::move(payload), timestamp++);
    RTC_CHECK_EQ(parse_results.size(), 1);
    std::optional<DecodeResult> decode_results =
        parse_results[0].frame->Decode(decoded_frame);
    RTC_CHECK(decode_results);
    RTC_CHECK_EQ(decode_results->num_decoded_samples, decoded_frame.size());

    EXPECT_TRUE(IsTrivialStereo(decoded_frame));
  }
}

TEST(AudioDecoderOpusTest,
     MonoEncoderStereoDecoderOutputsTrivialStereoComfortNoise) {
  const Environment env = EnvironmentFactory().Create();
  // Create a mono encoder.
  const AudioEncoderOpusConfig encoder_config =
      GetEncoderConfig(/*num_channels=*/1, /*dtx_enabled=*/true);
  AudioEncoderOpusImpl encoder(env, encoder_config, kPayloadType);
  // Create a stereo decoder.
  constexpr size_t kDecoderNumChannels = 2;
  AudioDecoderOpusImpl decoder(env.field_trials(), kDecoderNumChannels,
                               kSampleRateHz);
  std::vector<int16_t> decoded_frame;

  uint32_t rtp_timestamp = 0xFFFu;
  uint32_t timestamp = 0;
  // Feed the encoder with speech, otherwise DTX will never kick in.
  EncodeDecodeSpeech(encoder, decoder, rtp_timestamp, timestamp,
                     /*max_frames=*/100);
  // Feed the encoder with noise until the decoder is in DTX mode.
  EncodeDecodeNoiseUntilDecoderInDtxMode(encoder, decoder, rtp_timestamp,
                                         timestamp);

  // Decode an empty packet so that Opus generates comfort noise.
  decoded_frame.resize(kEncoderFrameLength * kDecoderNumChannels);
  AudioDecoder::SpeechType speech_type;
  const int num_decoded_samples =
      decoder.Decode(/*encoded=*/nullptr, /*encoded_len=*/0, kSampleRateHz,
                     decoded_frame.size(), decoded_frame.data(), &speech_type);
  ASSERT_EQ(speech_type, AudioDecoder::SpeechType::kComfortNoise);
  RTC_CHECK_GT(num_decoded_samples, 0);
  RTC_CHECK_LE(num_decoded_samples, decoded_frame.size());
  rtc::ArrayView<const int16_t> decoded_view(decoded_frame.data(),
                                             num_decoded_samples);
  // Make sure that comfort noise is not a muted frame.
  ASSERT_FALSE(IsZeroedFrame(decoded_view));
  EXPECT_TRUE(IsTrivialStereo(decoded_view));

  // Also check the first decoded audio frame after comfort noise.
  decoded_frame = EncodeDecodeSpeechUntilOneFrameIsDecoded(
      encoder, decoder, rtp_timestamp, timestamp);
  ASSERT_THAT(decoded_frame, SizeIs(kDecoderNumChannels * kEncoderFrameLength));
  ASSERT_FALSE(IsZeroedFrame(decoded_frame));
  EXPECT_TRUE(IsTrivialStereo(decoded_frame));
}

TEST(AudioDecoderOpusTest, MonoEncoderStereoDecoderOutputsTrivialStereoPlc) {
  const ExplicitKeyValueConfig trials("WebRTC-Audio-OpusGeneratePlc/Enabled/");
  EnvironmentFactory env_factory;
  env_factory.Set(&trials);
  const Environment env = env_factory.Create();
  // Create a mono encoder.
  const AudioEncoderOpusConfig encoder_config =
      GetEncoderConfig(/*num_channels=*/1, /*dtx_enabled=*/false);
  AudioEncoderOpusImpl encoder(env, encoder_config, kPayloadType);
  // Create a stereo decoder.
  constexpr size_t kDecoderNumChannels = 2;
  AudioDecoderOpusImpl decoder(env.field_trials(), kDecoderNumChannels,
                               kSampleRateHz);

  uint32_t rtp_timestamp = 0xFFFu;
  uint32_t timestamp = 0;
  // Feed the encoder with speech.
  EncodeDecodeSpeech(encoder, decoder, rtp_timestamp, timestamp,
                     /*max_frames=*/100);

  // Generate packet loss concealment.
  rtc::BufferT<int16_t> concealment_audio;
  constexpr int kIgnored = 123;
  decoder.GeneratePlc(/*requested_samples_per_channel=*/kIgnored,
                      &concealment_audio);
  RTC_CHECK_GT(concealment_audio.size(), 0);
  rtc::ArrayView<const int16_t> decoded_view(concealment_audio.data(),
                                             concealment_audio.size());
  // Make sure that packet loss concealment is not a muted frame.
  ASSERT_FALSE(IsZeroedFrame(decoded_view));
  EXPECT_TRUE(IsTrivialStereo(decoded_view));

  // Also check the first decoded audio frame after packet loss concealment.
  std::vector<int16_t> decoded_frame = EncodeDecodeSpeechUntilOneFrameIsDecoded(
      encoder, decoder, rtp_timestamp, timestamp);
  ASSERT_THAT(decoded_frame, SizeIs(kDecoderNumChannels * kEncoderFrameLength));
  ASSERT_FALSE(IsZeroedFrame(decoded_frame));
  EXPECT_TRUE(IsTrivialStereo(decoded_frame));
}

TEST(AudioDecoderOpusTest,
     StereoEncoderStereoDecoderOutputsNonTrivialStereoComfortNoise) {
  const Environment env = EnvironmentFactory().Create();
  // Create a stereo encoder.
  const AudioEncoderOpusConfig encoder_config =
      GetEncoderConfig(/*num_channels=*/2, /*dtx_enabled=*/true);
  AudioEncoderOpusImpl encoder(env, encoder_config, kPayloadType);
  // Create a stereo decoder.
  constexpr size_t kDecoderNumChannels = 2;
  AudioDecoderOpusImpl decoder(env.field_trials(), kDecoderNumChannels,
                               kSampleRateHz);

  uint32_t rtp_timestamp = 0xFFFu;
  uint32_t timestamp = 0;
  // Feed the encoder with speech, otherwise DTX will never kick in.
  EncodeDecodeSpeech(encoder, decoder, rtp_timestamp, timestamp,
                     /*max_frames=*/100);
  // Feed the encoder with noise and decode until the decoder is in DTX mode.
  EncodeDecodeNoiseUntilDecoderInDtxMode(encoder, decoder, rtp_timestamp,
                                         timestamp);

  // Decode an empty packet so that Opus generates comfort noise.
  std::array<int16_t, kEncoderFrameLength * kDecoderNumChannels> decoded_frame;
  AudioDecoder::SpeechType speech_type;
  const int num_decoded_samples =
      decoder.Decode(/*encoded=*/nullptr, /*encoded_len=*/0, kSampleRateHz,
                     decoded_frame.size(), decoded_frame.data(), &speech_type);
  ASSERT_EQ(speech_type, AudioDecoder::SpeechType::kComfortNoise);
  RTC_CHECK_GT(num_decoded_samples, 0);
  RTC_CHECK_LE(num_decoded_samples, decoded_frame.size());
  rtc::ArrayView<const int16_t> decoded_view(decoded_frame.data(),
                                             num_decoded_samples);
  // Make sure that comfort noise is not a muted frame.
  ASSERT_FALSE(IsZeroedFrame(decoded_view));

  EXPECT_FALSE(IsTrivialStereo(decoded_view));
}

TEST(AudioDecoderOpusTest,
     StereoEncoderStereoDecoderOutputsNonTrivialStereoPlc) {
  const ExplicitKeyValueConfig trials("WebRTC-Audio-OpusGeneratePlc/Enabled/");
  EnvironmentFactory env_factory;
  env_factory.Set(&trials);
  const Environment env = env_factory.Create();
  // Create a stereo encoder.
  const AudioEncoderOpusConfig encoder_config =
      GetEncoderConfig(/*num_channels=*/2, /*dtx_enabled=*/false);
  AudioEncoderOpusImpl encoder(env, encoder_config, kPayloadType);
  // Create a stereo decoder.
  constexpr size_t kDecoderNumChannels = 2;
  AudioDecoderOpusImpl decoder(env.field_trials(), kDecoderNumChannels,
                               kSampleRateHz);

  uint32_t rtp_timestamp = 0xFFFu;
  uint32_t timestamp = 0;
  // Feed the encoder with speech.
  EncodeDecodeSpeech(encoder, decoder, rtp_timestamp, timestamp,
                     /*max_frames=*/100);

  // Generate packet loss concealment.
  rtc::BufferT<int16_t> concealment_audio;
  constexpr int kIgnored = 123;
  decoder.GeneratePlc(/*requested_samples_per_channel=*/kIgnored,
                      &concealment_audio);
  RTC_CHECK_GT(concealment_audio.size(), 0);
  rtc::ArrayView<const int16_t> decoded_view(concealment_audio.data(),
                                             concealment_audio.size());
  // Make sure that packet loss concealment is not a muted frame.
  ASSERT_FALSE(IsZeroedFrame(decoded_view));

  EXPECT_FALSE(IsTrivialStereo(decoded_view));
}

}  // namespace webrtc
