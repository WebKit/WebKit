/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <array>
#include <memory>
#include <utility>

#include "absl/memory/memory.h"
#include "api/audio_codecs/opus/audio_encoder_opus.h"
#include "common_audio/mocks/mock_smoothing_filter.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/audio_coding/audio_network_adaptor/mock/mock_audio_network_adaptor.h"
#include "modules/audio_coding/codecs/opus/audio_encoder_opus.h"
#include "modules/audio_coding/codecs/opus/opus_interface.h"
#include "modules/audio_coding/neteq/tools/audio_loop.h"
#include "rtc_base/checks.h"
#include "rtc_base/fakeclock.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
using ::testing::NiceMock;
using ::testing::Return;

namespace {

const CodecInst kDefaultOpusSettings = {105, "opus", 48000, 960, 1, 32000};
constexpr int64_t kInitialTimeUs = 12345678;

AudioEncoderOpusConfig CreateConfig(const CodecInst& codec_inst) {
  AudioEncoderOpusConfig config;
  config.frame_size_ms = rtc::CheckedDivExact(codec_inst.pacsize, 48);
  config.num_channels = codec_inst.channels;
  config.bitrate_bps = codec_inst.rate;
  config.application = config.num_channels == 1
                           ? AudioEncoderOpusConfig::ApplicationMode::kVoip
                           : AudioEncoderOpusConfig::ApplicationMode::kAudio;
  config.supported_frame_lengths_ms.push_back(config.frame_size_ms);
  return config;
}

AudioEncoderOpusConfig CreateConfigWithParameters(
    const SdpAudioFormat::Parameters& params) {
  const SdpAudioFormat format("opus", 48000, 2, params);
  return *AudioEncoderOpus::SdpToConfig(format);
}

struct AudioEncoderOpusStates {
  MockAudioNetworkAdaptor* mock_audio_network_adaptor;
  MockSmoothingFilter* mock_bitrate_smoother;
  std::unique_ptr<AudioEncoderOpusImpl> encoder;
  std::unique_ptr<rtc::ScopedFakeClock> fake_clock;
  AudioEncoderOpusConfig config;
};

std::unique_ptr<AudioEncoderOpusStates> CreateCodec(size_t num_channels) {
  std::unique_ptr<AudioEncoderOpusStates> states =
      absl::make_unique<AudioEncoderOpusStates>();
  states->mock_audio_network_adaptor = nullptr;
  states->fake_clock.reset(new rtc::ScopedFakeClock());
  states->fake_clock->SetTimeMicros(kInitialTimeUs);

  MockAudioNetworkAdaptor** mock_ptr = &states->mock_audio_network_adaptor;
  AudioEncoderOpusImpl::AudioNetworkAdaptorCreator creator =
      [mock_ptr](const std::string&, RtcEventLog* event_log) {
        std::unique_ptr<MockAudioNetworkAdaptor> adaptor(
            new NiceMock<MockAudioNetworkAdaptor>());
        EXPECT_CALL(*adaptor, Die());
        *mock_ptr = adaptor.get();
        return adaptor;
      };

  CodecInst codec_inst = kDefaultOpusSettings;
  codec_inst.channels = num_channels;
  states->config = CreateConfig(codec_inst);
  std::unique_ptr<MockSmoothingFilter> bitrate_smoother(
      new MockSmoothingFilter());
  states->mock_bitrate_smoother = bitrate_smoother.get();

  states->encoder.reset(new AudioEncoderOpusImpl(
      states->config, codec_inst.pltype, std::move(creator),
      std::move(bitrate_smoother)));
  return states;
}

AudioEncoderRuntimeConfig CreateEncoderRuntimeConfig() {
  constexpr int kBitrate = 40000;
  constexpr int kFrameLength = 60;
  constexpr bool kEnableFec = true;
  constexpr bool kEnableDtx = false;
  constexpr size_t kNumChannels = 1;
  constexpr float kPacketLossFraction = 0.1f;
  AudioEncoderRuntimeConfig config;
  config.bitrate_bps = kBitrate;
  config.frame_length_ms = kFrameLength;
  config.enable_fec = kEnableFec;
  config.enable_dtx = kEnableDtx;
  config.num_channels = kNumChannels;
  config.uplink_packet_loss_fraction = kPacketLossFraction;
  return config;
}

void CheckEncoderRuntimeConfig(const AudioEncoderOpusImpl* encoder,
                               const AudioEncoderRuntimeConfig& config) {
  EXPECT_EQ(*config.bitrate_bps, encoder->GetTargetBitrate());
  EXPECT_EQ(*config.frame_length_ms, encoder->next_frame_length_ms());
  EXPECT_EQ(*config.enable_fec, encoder->fec_enabled());
  EXPECT_EQ(*config.enable_dtx, encoder->GetDtx());
  EXPECT_EQ(*config.num_channels, encoder->num_channels_to_encode());
}

// Create 10ms audio data blocks for a total packet size of "packet_size_ms".
std::unique_ptr<test::AudioLoop> Create10msAudioBlocks(
    const std::unique_ptr<AudioEncoderOpusImpl>& encoder,
    int packet_size_ms) {
  const std::string file_name =
      test::ResourcePath("audio_coding/testfile32kHz", "pcm");

  std::unique_ptr<test::AudioLoop> speech_data(new test::AudioLoop());
  int audio_samples_per_ms =
      rtc::CheckedDivExact(encoder->SampleRateHz(), 1000);
  if (!speech_data->Init(
          file_name,
          packet_size_ms * audio_samples_per_ms *
              encoder->num_channels_to_encode(),
          10 * audio_samples_per_ms * encoder->num_channels_to_encode()))
    return nullptr;
  return speech_data;
}

}  // namespace

TEST(AudioEncoderOpusTest, DefaultApplicationModeMono) {
  auto states = CreateCodec(1);
  EXPECT_EQ(AudioEncoderOpusConfig::ApplicationMode::kVoip,
            states->encoder->application());
}

TEST(AudioEncoderOpusTest, DefaultApplicationModeStereo) {
  auto states = CreateCodec(2);
  EXPECT_EQ(AudioEncoderOpusConfig::ApplicationMode::kAudio,
            states->encoder->application());
}

TEST(AudioEncoderOpusTest, ChangeApplicationMode) {
  auto states = CreateCodec(2);
  EXPECT_TRUE(
      states->encoder->SetApplication(AudioEncoder::Application::kSpeech));
  EXPECT_EQ(AudioEncoderOpusConfig::ApplicationMode::kVoip,
            states->encoder->application());
}

TEST(AudioEncoderOpusTest, ResetWontChangeApplicationMode) {
  auto states = CreateCodec(2);

  // Trigger a reset.
  states->encoder->Reset();
  // Verify that the mode is still kAudio.
  EXPECT_EQ(AudioEncoderOpusConfig::ApplicationMode::kAudio,
            states->encoder->application());

  // Now change to kVoip.
  EXPECT_TRUE(
      states->encoder->SetApplication(AudioEncoder::Application::kSpeech));
  EXPECT_EQ(AudioEncoderOpusConfig::ApplicationMode::kVoip,
            states->encoder->application());

  // Trigger a reset again.
  states->encoder->Reset();
  // Verify that the mode is still kVoip.
  EXPECT_EQ(AudioEncoderOpusConfig::ApplicationMode::kVoip,
            states->encoder->application());
}

TEST(AudioEncoderOpusTest, ToggleDtx) {
  auto states = CreateCodec(2);
  // Enable DTX
  EXPECT_TRUE(states->encoder->SetDtx(true));
  EXPECT_TRUE(states->encoder->GetDtx());
  // Turn off DTX.
  EXPECT_TRUE(states->encoder->SetDtx(false));
  EXPECT_FALSE(states->encoder->GetDtx());
}

TEST(AudioEncoderOpusTest,
     OnReceivedUplinkBandwidthWithoutAudioNetworkAdaptor) {
  auto states = CreateCodec(1);
  // Constants are replicated from audio_states->encoderopus.cc.
  const int kMinBitrateBps = 6000;
  const int kMaxBitrateBps = 510000;
  // Set a too low bitrate.
  states->encoder->OnReceivedUplinkBandwidth(kMinBitrateBps - 1, absl::nullopt);
  EXPECT_EQ(kMinBitrateBps, states->encoder->GetTargetBitrate());
  // Set a too high bitrate.
  states->encoder->OnReceivedUplinkBandwidth(kMaxBitrateBps + 1, absl::nullopt);
  EXPECT_EQ(kMaxBitrateBps, states->encoder->GetTargetBitrate());
  // Set the minimum rate.
  states->encoder->OnReceivedUplinkBandwidth(kMinBitrateBps, absl::nullopt);
  EXPECT_EQ(kMinBitrateBps, states->encoder->GetTargetBitrate());
  // Set the maximum rate.
  states->encoder->OnReceivedUplinkBandwidth(kMaxBitrateBps, absl::nullopt);
  EXPECT_EQ(kMaxBitrateBps, states->encoder->GetTargetBitrate());
  // Set rates from kMaxBitrateBps up to 32000 bps.
  for (int rate = kMinBitrateBps; rate <= 32000; rate += 1000) {
    states->encoder->OnReceivedUplinkBandwidth(rate, absl::nullopt);
    EXPECT_EQ(rate, states->encoder->GetTargetBitrate());
  }
}

namespace {

// Returns a vector with the n evenly-spaced numbers a, a + (b - a)/(n - 1),
// ..., b.
std::vector<float> IntervalSteps(float a, float b, size_t n) {
  RTC_DCHECK_GT(n, 1u);
  const float step = (b - a) / (n - 1);
  std::vector<float> points;
  points.push_back(a);
  for (size_t i = 1; i < n - 1; ++i)
    points.push_back(a + i * step);
  points.push_back(b);
  return points;
}

// Sets the packet loss rate to each number in the vector in turn, and verifies
// that the loss rate as reported by the encoder is |expected_return| for all
// of them.
void TestSetPacketLossRate(const AudioEncoderOpusStates* states,
                           const std::vector<float>& losses,
                           float expected_return) {
  // |kSampleIntervalMs| is chosen to ease the calculation since
  // 0.9999 ^ 184198 = 1e-8. Which minimizes the effect of
  // PacketLossFractionSmoother used in AudioEncoderOpus.
  constexpr int64_t kSampleIntervalMs = 184198;
  for (float loss : losses) {
    states->encoder->OnReceivedUplinkPacketLossFraction(loss);
    states->fake_clock->AdvanceTime(TimeDelta::ms(kSampleIntervalMs));
    EXPECT_FLOAT_EQ(expected_return, states->encoder->packet_loss_rate());
  }
}

}  // namespace

TEST(AudioEncoderOpusTest, PacketLossRateOptimized) {
  auto states = CreateCodec(1);
  auto I = [](float a, float b) { return IntervalSteps(a, b, 10); };
  constexpr float eps = 1e-8f;

  // Note that the order of the following calls is critical.

  // clang-format off
  TestSetPacketLossRate(states.get(), I(0.00f      , 0.01f - eps), 0.00f);
  TestSetPacketLossRate(states.get(), I(0.01f + eps, 0.06f - eps), 0.01f);
  TestSetPacketLossRate(states.get(), I(0.06f + eps, 0.11f - eps), 0.05f);
  TestSetPacketLossRate(states.get(), I(0.11f + eps, 0.22f - eps), 0.10f);
  TestSetPacketLossRate(states.get(), I(0.22f + eps, 1.00f      ), 0.20f);

  TestSetPacketLossRate(states.get(), I(1.00f      , 0.18f + eps), 0.20f);
  TestSetPacketLossRate(states.get(), I(0.18f - eps, 0.09f + eps), 0.10f);
  TestSetPacketLossRate(states.get(), I(0.09f - eps, 0.04f + eps), 0.05f);
  TestSetPacketLossRate(states.get(), I(0.04f - eps, 0.01f + eps), 0.01f);
  TestSetPacketLossRate(states.get(), I(0.01f - eps, 0.00f      ), 0.00f);
  // clang-format on
}

TEST(AudioEncoderOpusTest, PacketLossRateLowerBounded) {
  test::ScopedFieldTrials override_field_trials(
      "WebRTC-Audio-OpusMinPacketLossRate/Enabled-5/");
  auto states = CreateCodec(1);
  auto I = [](float a, float b) { return IntervalSteps(a, b, 10); };
  constexpr float eps = 1e-8f;

  // clang-format off
  TestSetPacketLossRate(states.get(), I(0.00f      , 0.01f - eps), 0.05f);
  TestSetPacketLossRate(states.get(), I(0.01f + eps, 0.06f - eps), 0.05f);
  TestSetPacketLossRate(states.get(), I(0.06f + eps, 0.11f - eps), 0.05f);
  TestSetPacketLossRate(states.get(), I(0.11f + eps, 0.22f - eps), 0.10f);
  TestSetPacketLossRate(states.get(), I(0.22f + eps, 1.00f      ), 0.20f);

  TestSetPacketLossRate(states.get(), I(1.00f      , 0.18f + eps), 0.20f);
  TestSetPacketLossRate(states.get(), I(0.18f - eps, 0.09f + eps), 0.10f);
  TestSetPacketLossRate(states.get(), I(0.09f - eps, 0.04f + eps), 0.05f);
  TestSetPacketLossRate(states.get(), I(0.04f - eps, 0.01f + eps), 0.05f);
  TestSetPacketLossRate(states.get(), I(0.01f - eps, 0.00f      ), 0.05f);
  // clang-format on
}

TEST(AudioEncoderOpusTest, NewPacketLossRateOptimization) {
  {
    test::ScopedFieldTrials override_field_trials(
        "WebRTC-Audio-NewOpusPacketLossRateOptimization/Enabled-5-15-0.5/");
    auto states = CreateCodec(1);

    TestSetPacketLossRate(states.get(), {0.00f}, 0.05f);
    TestSetPacketLossRate(states.get(), {0.12f}, 0.06f);
    TestSetPacketLossRate(states.get(), {0.22f}, 0.11f);
    TestSetPacketLossRate(states.get(), {0.50f}, 0.15f);
  }
  {
    test::ScopedFieldTrials override_field_trials(
        "WebRTC-Audio-NewOpusPacketLossRateOptimization/Enabled/");
    auto states = CreateCodec(1);

    TestSetPacketLossRate(states.get(), {0.00f}, 0.01f);
    TestSetPacketLossRate(states.get(), {0.12f}, 0.12f);
    TestSetPacketLossRate(states.get(), {0.22f}, 0.20f);
    TestSetPacketLossRate(states.get(), {0.50f}, 0.20f);
  }
}

TEST(AudioEncoderOpusTest, SetReceiverFrameLengthRange) {
  auto states = CreateCodec(2);
  // Before calling to |SetReceiverFrameLengthRange|,
  // |supported_frame_lengths_ms| should contain only the frame length being
  // used.
  using ::testing::ElementsAre;
  EXPECT_THAT(states->encoder->supported_frame_lengths_ms(),
              ElementsAre(states->encoder->next_frame_length_ms()));
  states->encoder->SetReceiverFrameLengthRange(0, 12345);
  states->encoder->SetReceiverFrameLengthRange(21, 60);
  EXPECT_THAT(states->encoder->supported_frame_lengths_ms(), ElementsAre(60));
  states->encoder->SetReceiverFrameLengthRange(20, 59);
  EXPECT_THAT(states->encoder->supported_frame_lengths_ms(), ElementsAre(20));
}

TEST(AudioEncoderOpusTest,
     InvokeAudioNetworkAdaptorOnReceivedUplinkPacketLossFraction) {
  auto states = CreateCodec(2);
  states->encoder->EnableAudioNetworkAdaptor("", nullptr);

  auto config = CreateEncoderRuntimeConfig();
  EXPECT_CALL(*states->mock_audio_network_adaptor, GetEncoderRuntimeConfig())
      .WillOnce(Return(config));

  // Since using mock audio network adaptor, any packet loss fraction is fine.
  constexpr float kUplinkPacketLoss = 0.1f;
  EXPECT_CALL(*states->mock_audio_network_adaptor,
              SetUplinkPacketLossFraction(kUplinkPacketLoss));
  states->encoder->OnReceivedUplinkPacketLossFraction(kUplinkPacketLoss);

  CheckEncoderRuntimeConfig(states->encoder.get(), config);
}

TEST(AudioEncoderOpusTest, InvokeAudioNetworkAdaptorOnReceivedUplinkBandwidth) {
  auto states = CreateCodec(2);
  states->encoder->EnableAudioNetworkAdaptor("", nullptr);

  auto config = CreateEncoderRuntimeConfig();
  EXPECT_CALL(*states->mock_audio_network_adaptor, GetEncoderRuntimeConfig())
      .WillOnce(Return(config));

  // Since using mock audio network adaptor, any target audio bitrate is fine.
  constexpr int kTargetAudioBitrate = 30000;
  constexpr int64_t kProbingIntervalMs = 3000;
  EXPECT_CALL(*states->mock_audio_network_adaptor,
              SetTargetAudioBitrate(kTargetAudioBitrate));
  EXPECT_CALL(*states->mock_bitrate_smoother,
              SetTimeConstantMs(kProbingIntervalMs * 4));
  EXPECT_CALL(*states->mock_bitrate_smoother, AddSample(kTargetAudioBitrate));
  states->encoder->OnReceivedUplinkBandwidth(kTargetAudioBitrate,
                                             kProbingIntervalMs);

  CheckEncoderRuntimeConfig(states->encoder.get(), config);
}

TEST(AudioEncoderOpusTest, InvokeAudioNetworkAdaptorOnReceivedRtt) {
  auto states = CreateCodec(2);
  states->encoder->EnableAudioNetworkAdaptor("", nullptr);

  auto config = CreateEncoderRuntimeConfig();
  EXPECT_CALL(*states->mock_audio_network_adaptor, GetEncoderRuntimeConfig())
      .WillOnce(Return(config));

  // Since using mock audio network adaptor, any rtt is fine.
  constexpr int kRtt = 30;
  EXPECT_CALL(*states->mock_audio_network_adaptor, SetRtt(kRtt));
  states->encoder->OnReceivedRtt(kRtt);

  CheckEncoderRuntimeConfig(states->encoder.get(), config);
}

TEST(AudioEncoderOpusTest, InvokeAudioNetworkAdaptorOnReceivedOverhead) {
  auto states = CreateCodec(2);
  states->encoder->EnableAudioNetworkAdaptor("", nullptr);

  auto config = CreateEncoderRuntimeConfig();
  EXPECT_CALL(*states->mock_audio_network_adaptor, GetEncoderRuntimeConfig())
      .WillOnce(Return(config));

  // Since using mock audio network adaptor, any overhead is fine.
  constexpr size_t kOverhead = 64;
  EXPECT_CALL(*states->mock_audio_network_adaptor, SetOverhead(kOverhead));
  states->encoder->OnReceivedOverhead(kOverhead);

  CheckEncoderRuntimeConfig(states->encoder.get(), config);
}

TEST(AudioEncoderOpusTest,
     PacketLossFractionSmoothedOnSetUplinkPacketLossFraction) {
  auto states = CreateCodec(2);

  // The values are carefully chosen so that if no smoothing is made, the test
  // will fail.
  constexpr float kPacketLossFraction_1 = 0.02f;
  constexpr float kPacketLossFraction_2 = 0.198f;
  // |kSecondSampleTimeMs| is chosen to ease the calculation since
  // 0.9999 ^ 6931 = 0.5.
  constexpr int64_t kSecondSampleTimeMs = 6931;

  // First time, no filtering.
  states->encoder->OnReceivedUplinkPacketLossFraction(kPacketLossFraction_1);
  EXPECT_FLOAT_EQ(0.01f, states->encoder->packet_loss_rate());

  states->fake_clock->AdvanceTime(TimeDelta::ms(kSecondSampleTimeMs));
  states->encoder->OnReceivedUplinkPacketLossFraction(kPacketLossFraction_2);

  // Now the output of packet loss fraction smoother should be
  // (0.02 + 0.198) / 2 = 0.109, which reach the threshold for the optimized
  // packet loss rate to increase to 0.05. If no smoothing has been made, the
  // optimized packet loss rate should have been increase to 0.1.
  EXPECT_FLOAT_EQ(0.05f, states->encoder->packet_loss_rate());
}

TEST(AudioEncoderOpusTest, DoNotInvokeSetTargetBitrateIfOverheadUnknown) {
  test::ScopedFieldTrials override_field_trials(
      "WebRTC-SendSideBwe-WithOverhead/Enabled/");

  auto states = CreateCodec(2);

  states->encoder->OnReceivedUplinkBandwidth(kDefaultOpusSettings.rate * 2,
                                             absl::nullopt);

  // Since |OnReceivedOverhead| has not been called, the codec bitrate should
  // not change.
  EXPECT_EQ(kDefaultOpusSettings.rate, states->encoder->GetTargetBitrate());
}

TEST(AudioEncoderOpusTest, OverheadRemovedFromTargetAudioBitrate) {
  test::ScopedFieldTrials override_field_trials(
      "WebRTC-SendSideBwe-WithOverhead/Enabled/");

  auto states = CreateCodec(2);

  constexpr size_t kOverheadBytesPerPacket = 64;
  states->encoder->OnReceivedOverhead(kOverheadBytesPerPacket);

  constexpr int kTargetBitrateBps = 40000;
  states->encoder->OnReceivedUplinkBandwidth(kTargetBitrateBps, absl::nullopt);

  int packet_rate = rtc::CheckedDivExact(48000, kDefaultOpusSettings.pacsize);
  EXPECT_EQ(kTargetBitrateBps -
                8 * static_cast<int>(kOverheadBytesPerPacket) * packet_rate,
            states->encoder->GetTargetBitrate());
}

TEST(AudioEncoderOpusTest, BitrateBounded) {
  test::ScopedFieldTrials override_field_trials(
      "WebRTC-SendSideBwe-WithOverhead/Enabled/");

  constexpr int kMinBitrateBps = 6000;
  constexpr int kMaxBitrateBps = 510000;

  auto states = CreateCodec(2);

  constexpr size_t kOverheadBytesPerPacket = 64;
  states->encoder->OnReceivedOverhead(kOverheadBytesPerPacket);

  int packet_rate = rtc::CheckedDivExact(48000, kDefaultOpusSettings.pacsize);

  // Set a target rate that is smaller than |kMinBitrateBps| when overhead is
  // subtracted. The eventual codec rate should be bounded by |kMinBitrateBps|.
  int target_bitrate =
      kOverheadBytesPerPacket * 8 * packet_rate + kMinBitrateBps - 1;
  states->encoder->OnReceivedUplinkBandwidth(target_bitrate, absl::nullopt);
  EXPECT_EQ(kMinBitrateBps, states->encoder->GetTargetBitrate());

  // Set a target rate that is greater than |kMaxBitrateBps| when overhead is
  // subtracted. The eventual codec rate should be bounded by |kMaxBitrateBps|.
  target_bitrate =
      kOverheadBytesPerPacket * 8 * packet_rate + kMaxBitrateBps + 1;
  states->encoder->OnReceivedUplinkBandwidth(target_bitrate, absl::nullopt);
  EXPECT_EQ(kMaxBitrateBps, states->encoder->GetTargetBitrate());
}

TEST(AudioEncoderOpusTest, MinPacketLossRate) {
  constexpr float kDefaultMinPacketLossRate = 0.01;
  {
    test::ScopedFieldTrials override_field_trials(
        "WebRTC-Audio-OpusMinPacketLossRate/Enabled/");
    auto states = CreateCodec(1);
    EXPECT_EQ(kDefaultMinPacketLossRate, states->encoder->packet_loss_rate());
  }
  {
    test::ScopedFieldTrials override_field_trials(
        "WebRTC-Audio-OpusMinPacketLossRate/Enabled-200/");
    auto states = CreateCodec(1);
    EXPECT_EQ(kDefaultMinPacketLossRate, states->encoder->packet_loss_rate());
  }
  {
    test::ScopedFieldTrials override_field_trials(
        "WebRTC-Audio-OpusMinPacketLossRate/Enabled-50/");
    constexpr float kMinPacketLossRate = 0.5;
    auto states = CreateCodec(1);
    EXPECT_EQ(kMinPacketLossRate, states->encoder->packet_loss_rate());
  }
}

TEST(AudioEncoderOpusTest, NewPacketLossRateOptimizer) {
  {
    auto states = CreateCodec(1);
    auto optimizer = states->encoder->new_packet_loss_optimizer();
    EXPECT_EQ(nullptr, optimizer);
  }
  {
    test::ScopedFieldTrials override_field_trials(
        "WebRTC-Audio-NewOpusPacketLossRateOptimization/Enabled/");
    auto states = CreateCodec(1);
    auto optimizer = states->encoder->new_packet_loss_optimizer();
    ASSERT_NE(nullptr, optimizer);
    EXPECT_FLOAT_EQ(0.01, optimizer->min_packet_loss_rate());
    EXPECT_FLOAT_EQ(0.20, optimizer->max_packet_loss_rate());
    EXPECT_FLOAT_EQ(1.00, optimizer->slope());
  }
  {
    test::ScopedFieldTrials override_field_trials(
        "WebRTC-Audio-NewOpusPacketLossRateOptimization/Enabled-2-50-0.7/");
    auto states = CreateCodec(1);
    auto optimizer = states->encoder->new_packet_loss_optimizer();
    ASSERT_NE(nullptr, optimizer);
    EXPECT_FLOAT_EQ(0.02, optimizer->min_packet_loss_rate());
    EXPECT_FLOAT_EQ(0.50, optimizer->max_packet_loss_rate());
    EXPECT_FLOAT_EQ(0.70, optimizer->slope());
  }
}

// Verifies that the complexity adaptation in the config works as intended.
TEST(AudioEncoderOpusTest, ConfigComplexityAdaptation) {
  AudioEncoderOpusConfig config;
  config.low_rate_complexity = 8;
  config.complexity = 6;

  // Bitrate within hysteresis window. Expect empty output.
  config.bitrate_bps = 12500;
  EXPECT_EQ(absl::nullopt, AudioEncoderOpusImpl::GetNewComplexity(config));

  // Bitrate below hysteresis window. Expect higher complexity.
  config.bitrate_bps = 10999;
  EXPECT_EQ(8, AudioEncoderOpusImpl::GetNewComplexity(config));

  // Bitrate within hysteresis window. Expect empty output.
  config.bitrate_bps = 12500;
  EXPECT_EQ(absl::nullopt, AudioEncoderOpusImpl::GetNewComplexity(config));

  // Bitrate above hysteresis window. Expect lower complexity.
  config.bitrate_bps = 14001;
  EXPECT_EQ(6, AudioEncoderOpusImpl::GetNewComplexity(config));
}

// Verifies that the bandwidth adaptation in the config works as intended.
TEST(AudioEncoderOpusTest, ConfigBandwidthAdaptation) {
  AudioEncoderOpusConfig config;
  // Sample rate of Opus.
  constexpr size_t kOpusRateKhz = 48;
  std::vector<int16_t> silence(
      kOpusRateKhz * config.frame_size_ms * config.num_channels, 0);
  constexpr size_t kMaxBytes = 1000;
  uint8_t bitstream[kMaxBytes];

  OpusEncInst* inst;
  EXPECT_EQ(0, WebRtcOpus_EncoderCreate(
                   &inst, config.num_channels,
                   config.application ==
                           AudioEncoderOpusConfig::ApplicationMode::kVoip
                       ? 0
                       : 1));

  // Bitrate below minmum wideband. Expect narrowband.
  config.bitrate_bps = absl::optional<int>(7999);
  auto bandwidth = AudioEncoderOpusImpl::GetNewBandwidth(config, inst);
  EXPECT_EQ(absl::optional<int>(OPUS_BANDWIDTH_NARROWBAND), bandwidth);
  WebRtcOpus_SetBandwidth(inst, *bandwidth);
  // It is necessary to encode here because Opus has some logic in the encoder
  // that goes from the user-set bandwidth to the used and returned one.
  WebRtcOpus_Encode(inst, silence.data(),
                    rtc::CheckedDivExact(silence.size(), config.num_channels),
                    kMaxBytes, bitstream);

  // Bitrate not yet above maximum narrowband. Expect empty.
  config.bitrate_bps = absl::optional<int>(9000);
  bandwidth = AudioEncoderOpusImpl::GetNewBandwidth(config, inst);
  EXPECT_EQ(absl::optional<int>(), bandwidth);

  // Bitrate above maximum narrowband. Expect wideband.
  config.bitrate_bps = absl::optional<int>(9001);
  bandwidth = AudioEncoderOpusImpl::GetNewBandwidth(config, inst);
  EXPECT_EQ(absl::optional<int>(OPUS_BANDWIDTH_WIDEBAND), bandwidth);
  WebRtcOpus_SetBandwidth(inst, *bandwidth);
  // It is necessary to encode here because Opus has some logic in the encoder
  // that goes from the user-set bandwidth to the used and returned one.
  WebRtcOpus_Encode(inst, silence.data(),
                    rtc::CheckedDivExact(silence.size(), config.num_channels),
                    kMaxBytes, bitstream);

  // Bitrate not yet below minimum wideband. Expect empty.
  config.bitrate_bps = absl::optional<int>(8000);
  bandwidth = AudioEncoderOpusImpl::GetNewBandwidth(config, inst);
  EXPECT_EQ(absl::optional<int>(), bandwidth);

  // Bitrate above automatic threshold. Expect automatic.
  config.bitrate_bps = absl::optional<int>(12001);
  bandwidth = AudioEncoderOpusImpl::GetNewBandwidth(config, inst);
  EXPECT_EQ(absl::optional<int>(OPUS_AUTO), bandwidth);

  EXPECT_EQ(0, WebRtcOpus_EncoderFree(inst));
}

TEST(AudioEncoderOpusTest, EmptyConfigDoesNotAffectEncoderSettings) {
  auto states = CreateCodec(2);
  states->encoder->EnableAudioNetworkAdaptor("", nullptr);

  auto config = CreateEncoderRuntimeConfig();
  AudioEncoderRuntimeConfig empty_config;

  EXPECT_CALL(*states->mock_audio_network_adaptor, GetEncoderRuntimeConfig())
      .WillOnce(Return(config))
      .WillOnce(Return(empty_config));

  constexpr size_t kOverhead = 64;
  EXPECT_CALL(*states->mock_audio_network_adaptor, SetOverhead(kOverhead))
      .Times(2);
  states->encoder->OnReceivedOverhead(kOverhead);
  states->encoder->OnReceivedOverhead(kOverhead);

  CheckEncoderRuntimeConfig(states->encoder.get(), config);
}

TEST(AudioEncoderOpusTest, UpdateUplinkBandwidthInAudioNetworkAdaptor) {
  auto states = CreateCodec(2);
  states->encoder->EnableAudioNetworkAdaptor("", nullptr);
  std::array<int16_t, 480 * 2> audio;
  audio.fill(0);
  rtc::Buffer encoded;
  EXPECT_CALL(*states->mock_bitrate_smoother, GetAverage())
      .WillOnce(Return(50000));
  EXPECT_CALL(*states->mock_audio_network_adaptor, SetUplinkBandwidth(50000));
  states->encoder->Encode(
      0, rtc::ArrayView<const int16_t>(audio.data(), audio.size()), &encoded);

  // Repeat update uplink bandwidth tests.
  for (int i = 0; i < 5; i++) {
    // Don't update till it is time to update again.
    states->fake_clock->AdvanceTime(
        TimeDelta::ms(states->config.uplink_bandwidth_update_interval_ms - 1));
    states->encoder->Encode(
        0, rtc::ArrayView<const int16_t>(audio.data(), audio.size()), &encoded);

    // Update when it is time to update.
    EXPECT_CALL(*states->mock_bitrate_smoother, GetAverage())
        .WillOnce(Return(40000));
    EXPECT_CALL(*states->mock_audio_network_adaptor, SetUplinkBandwidth(40000));
    states->fake_clock->AdvanceTime(TimeDelta::ms(1));
    states->encoder->Encode(
        0, rtc::ArrayView<const int16_t>(audio.data(), audio.size()), &encoded);
  }
}

TEST(AudioEncoderOpusTest, EncodeAtMinBitrate) {
  auto states = CreateCodec(1);
  constexpr int kNumPacketsToEncode = 2;
  auto audio_frames =
      Create10msAudioBlocks(states->encoder, kNumPacketsToEncode * 20);
  ASSERT_TRUE(audio_frames) << "Create10msAudioBlocks failed";
  rtc::Buffer encoded;
  uint32_t rtp_timestamp = 12345;  // Just a number not important to this test.

  states->encoder->OnReceivedUplinkBandwidth(0, absl::nullopt);
  for (int packet_index = 0; packet_index < kNumPacketsToEncode;
       packet_index++) {
    // Make sure we are not encoding before we have enough data for
    // a 20ms packet.
    for (int index = 0; index < 1; index++) {
      states->encoder->Encode(rtp_timestamp, audio_frames->GetNextBlock(),
                              &encoded);
      EXPECT_EQ(0u, encoded.size());
    }

    // Should encode now.
    states->encoder->Encode(rtp_timestamp, audio_frames->GetNextBlock(),
                            &encoded);
    EXPECT_GT(encoded.size(), 0u);
    encoded.Clear();
  }
}

TEST(AudioEncoderOpusTest, TestConfigDefaults) {
  const auto config_opt = AudioEncoderOpus::SdpToConfig({"opus", 48000, 2});
  ASSERT_TRUE(config_opt);
  EXPECT_EQ(48000, config_opt->max_playback_rate_hz);
  EXPECT_EQ(1u, config_opt->num_channels);
  EXPECT_FALSE(config_opt->fec_enabled);
  EXPECT_FALSE(config_opt->dtx_enabled);
  EXPECT_EQ(20, config_opt->frame_size_ms);
}

TEST(AudioEncoderOpusTest, TestConfigFromParams) {
  const auto config1 = CreateConfigWithParameters({{"stereo", "0"}});
  EXPECT_EQ(1U, config1.num_channels);

  const auto config2 = CreateConfigWithParameters({{"stereo", "1"}});
  EXPECT_EQ(2U, config2.num_channels);

  const auto config3 = CreateConfigWithParameters({{"useinbandfec", "0"}});
  EXPECT_FALSE(config3.fec_enabled);

  const auto config4 = CreateConfigWithParameters({{"useinbandfec", "1"}});
  EXPECT_TRUE(config4.fec_enabled);

  const auto config5 = CreateConfigWithParameters({{"usedtx", "0"}});
  EXPECT_FALSE(config5.dtx_enabled);

  const auto config6 = CreateConfigWithParameters({{"usedtx", "1"}});
  EXPECT_TRUE(config6.dtx_enabled);

  const auto config7 = CreateConfigWithParameters({{"cbr", "0"}});
  EXPECT_FALSE(config7.cbr_enabled);

  const auto config8 = CreateConfigWithParameters({{"cbr", "1"}});
  EXPECT_TRUE(config8.cbr_enabled);

  const auto config9 =
      CreateConfigWithParameters({{"maxplaybackrate", "12345"}});
  EXPECT_EQ(12345, config9.max_playback_rate_hz);

  const auto config10 =
      CreateConfigWithParameters({{"maxaveragebitrate", "96000"}});
  EXPECT_EQ(96000, config10.bitrate_bps);

  const auto config11 = CreateConfigWithParameters({{"maxptime", "40"}});
  for (int frame_length : config11.supported_frame_lengths_ms) {
    EXPECT_LE(frame_length, 40);
  }

  const auto config12 = CreateConfigWithParameters({{"minptime", "40"}});
  for (int frame_length : config12.supported_frame_lengths_ms) {
    EXPECT_GE(frame_length, 40);
  }

  const auto config13 = CreateConfigWithParameters({{"ptime", "40"}});
  EXPECT_EQ(40, config13.frame_size_ms);

  constexpr int kMinSupportedFrameLength = 10;
  constexpr int kMaxSupportedFrameLength =
      WEBRTC_OPUS_SUPPORT_120MS_PTIME ? 120 : 60;

  const auto config14 = CreateConfigWithParameters({{"ptime", "1"}});
  EXPECT_EQ(kMinSupportedFrameLength, config14.frame_size_ms);

  const auto config15 = CreateConfigWithParameters({{"ptime", "2000"}});
  EXPECT_EQ(kMaxSupportedFrameLength, config15.frame_size_ms);
}

TEST(AudioEncoderOpusTest, TestConfigFromInvalidParams) {
  const webrtc::SdpAudioFormat format("opus", 48000, 2);
  const auto default_config = *AudioEncoderOpus::SdpToConfig(format);
#if WEBRTC_OPUS_SUPPORT_120MS_PTIME
  const std::vector<int> default_supported_frame_lengths_ms({20, 60, 120});
#else
  const std::vector<int> default_supported_frame_lengths_ms({20, 60});
#endif

  AudioEncoderOpusConfig config;
  config = CreateConfigWithParameters({{"stereo", "invalid"}});
  EXPECT_EQ(default_config.num_channels, config.num_channels);

  config = CreateConfigWithParameters({{"useinbandfec", "invalid"}});
  EXPECT_EQ(default_config.fec_enabled, config.fec_enabled);

  config = CreateConfigWithParameters({{"usedtx", "invalid"}});
  EXPECT_EQ(default_config.dtx_enabled, config.dtx_enabled);

  config = CreateConfigWithParameters({{"cbr", "invalid"}});
  EXPECT_EQ(default_config.dtx_enabled, config.dtx_enabled);

  config = CreateConfigWithParameters({{"maxplaybackrate", "0"}});
  EXPECT_EQ(default_config.max_playback_rate_hz, config.max_playback_rate_hz);

  config = CreateConfigWithParameters({{"maxplaybackrate", "-23"}});
  EXPECT_EQ(default_config.max_playback_rate_hz, config.max_playback_rate_hz);

  config = CreateConfigWithParameters({{"maxplaybackrate", "not a number!"}});
  EXPECT_EQ(default_config.max_playback_rate_hz, config.max_playback_rate_hz);

  config = CreateConfigWithParameters({{"maxaveragebitrate", "0"}});
  EXPECT_EQ(6000, config.bitrate_bps);

  config = CreateConfigWithParameters({{"maxaveragebitrate", "-1000"}});
  EXPECT_EQ(6000, config.bitrate_bps);

  config = CreateConfigWithParameters({{"maxaveragebitrate", "1024000"}});
  EXPECT_EQ(510000, config.bitrate_bps);

  config = CreateConfigWithParameters({{"maxaveragebitrate", "not a number!"}});
  EXPECT_EQ(default_config.bitrate_bps, config.bitrate_bps);

  config = CreateConfigWithParameters({{"maxptime", "invalid"}});
  EXPECT_EQ(default_supported_frame_lengths_ms,
            config.supported_frame_lengths_ms);

  config = CreateConfigWithParameters({{"minptime", "invalid"}});
  EXPECT_EQ(default_supported_frame_lengths_ms,
            config.supported_frame_lengths_ms);

  config = CreateConfigWithParameters({{"ptime", "invalid"}});
  EXPECT_EQ(default_supported_frame_lengths_ms,
            config.supported_frame_lengths_ms);
}

// Test that bitrate will be overridden by the "maxaveragebitrate" parameter.
// Also test that the "maxaveragebitrate" can't be set to values outside the
// range of 6000 and 510000
TEST(AudioEncoderOpusTest, SetSendCodecOpusMaxAverageBitrate) {
  // Ignore if less than 6000.
  const auto config1 = AudioEncoderOpus::SdpToConfig(
      {"opus", 48000, 2, {{"maxaveragebitrate", "5999"}}});
  EXPECT_EQ(6000, config1->bitrate_bps);

  // Ignore if larger than 510000.
  const auto config2 = AudioEncoderOpus::SdpToConfig(
      {"opus", 48000, 2, {{"maxaveragebitrate", "510001"}}});
  EXPECT_EQ(510000, config2->bitrate_bps);

  const auto config3 = AudioEncoderOpus::SdpToConfig(
      {"opus", 48000, 2, {{"maxaveragebitrate", "200000"}}});
  EXPECT_EQ(200000, config3->bitrate_bps);
}

// Test maxplaybackrate <= 8000 triggers Opus narrow band mode.
TEST(AudioEncoderOpusTest, SetMaxPlaybackRateNb) {
  auto config = CreateConfigWithParameters({{"maxplaybackrate", "8000"}});
  EXPECT_EQ(8000, config.max_playback_rate_hz);
  EXPECT_EQ(12000, config.bitrate_bps);

  config = CreateConfigWithParameters(
      {{"maxplaybackrate", "8000"}, {"stereo", "1"}});
  EXPECT_EQ(8000, config.max_playback_rate_hz);
  EXPECT_EQ(24000, config.bitrate_bps);
}

// Test 8000 < maxplaybackrate <= 12000 triggers Opus medium band mode.
TEST(AudioEncoderOpusTest, SetMaxPlaybackRateMb) {
  auto config = CreateConfigWithParameters({{"maxplaybackrate", "8001"}});
  EXPECT_EQ(8001, config.max_playback_rate_hz);
  EXPECT_EQ(20000, config.bitrate_bps);

  config = CreateConfigWithParameters(
      {{"maxplaybackrate", "8001"}, {"stereo", "1"}});
  EXPECT_EQ(8001, config.max_playback_rate_hz);
  EXPECT_EQ(40000, config.bitrate_bps);
}

// Test 12000 < maxplaybackrate <= 16000 triggers Opus wide band mode.
TEST(AudioEncoderOpusTest, SetMaxPlaybackRateWb) {
  auto config = CreateConfigWithParameters({{"maxplaybackrate", "12001"}});
  EXPECT_EQ(12001, config.max_playback_rate_hz);
  EXPECT_EQ(20000, config.bitrate_bps);

  config = CreateConfigWithParameters(
      {{"maxplaybackrate", "12001"}, {"stereo", "1"}});
  EXPECT_EQ(12001, config.max_playback_rate_hz);
  EXPECT_EQ(40000, config.bitrate_bps);
}

// Test 16000 < maxplaybackrate <= 24000 triggers Opus super wide band mode.
TEST(AudioEncoderOpusTest, SetMaxPlaybackRateSwb) {
  auto config = CreateConfigWithParameters({{"maxplaybackrate", "16001"}});
  EXPECT_EQ(16001, config.max_playback_rate_hz);
  EXPECT_EQ(32000, config.bitrate_bps);

  config = CreateConfigWithParameters(
      {{"maxplaybackrate", "16001"}, {"stereo", "1"}});
  EXPECT_EQ(16001, config.max_playback_rate_hz);
  EXPECT_EQ(64000, config.bitrate_bps);
}

// Test 24000 < maxplaybackrate triggers Opus full band mode.
TEST(AudioEncoderOpusTest, SetMaxPlaybackRateFb) {
  auto config = CreateConfigWithParameters({{"maxplaybackrate", "24001"}});
  EXPECT_EQ(24001, config.max_playback_rate_hz);
  EXPECT_EQ(32000, config.bitrate_bps);

  config = CreateConfigWithParameters(
      {{"maxplaybackrate", "24001"}, {"stereo", "1"}});
  EXPECT_EQ(24001, config.max_playback_rate_hz);
  EXPECT_EQ(64000, config.bitrate_bps);
}

TEST(AudioEncoderOpusTest, OpusFlagDtxAsNonSpeech) {
  // Create encoder with DTX enabled.
  AudioEncoderOpusConfig config;
  config.dtx_enabled = true;
  constexpr int payload_type = 17;
  const auto encoder = AudioEncoderOpus::MakeAudioEncoder(config, payload_type);

  // Open file containing speech and silence.
  const std::string kInputFileName =
      webrtc::test::ResourcePath("audio_coding/testfile32kHz", "pcm");
  test::AudioLoop audio_loop;
  // Use the file as if it were sampled at 48 kHz.
  constexpr int kSampleRateHz = 48000;
  EXPECT_EQ(kSampleRateHz, encoder->SampleRateHz());
  constexpr size_t kMaxLoopLengthSamples =
      kSampleRateHz * 10;  // Max 10 second loop.
  constexpr size_t kInputBlockSizeSamples =
      10 * kSampleRateHz / 1000;  // 10 ms.
  EXPECT_TRUE(audio_loop.Init(kInputFileName, kMaxLoopLengthSamples,
                              kInputBlockSizeSamples));

  // Encode.
  AudioEncoder::EncodedInfo info;
  rtc::Buffer encoded(500);
  int nonspeech_frames = 0;
  int max_nonspeech_frames = 0;
  int dtx_frames = 0;
  int max_dtx_frames = 0;
  uint32_t rtp_timestamp = 0u;
  for (size_t i = 0; i < 500; ++i) {
    encoded.Clear();

    // Every second call to the encoder will generate an Opus packet.
    for (int j = 0; j < 2; j++) {
      info =
          encoder->Encode(rtp_timestamp, audio_loop.GetNextBlock(), &encoded);
      rtp_timestamp += kInputBlockSizeSamples;
    }

    // Bookkeeping of number of DTX frames.
    if (info.encoded_bytes <= 2) {
      ++dtx_frames;
    } else {
      if (dtx_frames > max_dtx_frames)
        max_dtx_frames = dtx_frames;
      dtx_frames = 0;
    }

    // Bookkeeping of number of non-speech frames.
    if (info.speech == 0) {
      ++nonspeech_frames;
    } else {
      if (nonspeech_frames > max_nonspeech_frames)
        max_nonspeech_frames = nonspeech_frames;
      nonspeech_frames = 0;
    }
  }

  // Maximum number of consecutive non-speech packets should exceed 20.
  EXPECT_GT(max_nonspeech_frames, 20);
}

}  // namespace webrtc
