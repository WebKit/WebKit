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

#include "webrtc/base/checks.h"
#include "webrtc/base/fakeclock.h"
#include "webrtc/common_audio/mocks/mock_smoothing_filter.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/audio_coding/audio_network_adaptor/mock/mock_audio_network_adaptor.h"
#include "webrtc/modules/audio_coding/codecs/opus/audio_encoder_opus.h"
#include "webrtc/modules/audio_coding/neteq/tools/audio_loop.h"
#include "webrtc/test/field_trial.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/system_wrappers/include/clock.h"

namespace webrtc {
using ::testing::NiceMock;
using ::testing::Return;

namespace {

const CodecInst kDefaultOpusSettings = {105, "opus", 48000, 960, 1, 32000};
constexpr int64_t kInitialTimeUs = 12345678;

AudioEncoderOpus::Config CreateConfig(const CodecInst& codec_inst) {
  AudioEncoderOpus::Config config;
  config.frame_size_ms = rtc::CheckedDivExact(codec_inst.pacsize, 48);
  config.num_channels = codec_inst.channels;
  config.bitrate_bps = rtc::Optional<int>(codec_inst.rate);
  config.payload_type = codec_inst.pltype;
  config.application = config.num_channels == 1 ? AudioEncoderOpus::kVoip
                                                : AudioEncoderOpus::kAudio;
  config.supported_frame_lengths_ms.push_back(config.frame_size_ms);
  return config;
}

struct AudioEncoderOpusStates {
  std::shared_ptr<MockAudioNetworkAdaptor*> mock_audio_network_adaptor;
  MockSmoothingFilter* mock_bitrate_smoother;
  std::unique_ptr<AudioEncoderOpus> encoder;
  std::unique_ptr<SimulatedClock> simulated_clock;
  AudioEncoderOpus::Config config;
};

AudioEncoderOpusStates CreateCodec(size_t num_channels) {
  AudioEncoderOpusStates states;
  states.mock_audio_network_adaptor =
      std::make_shared<MockAudioNetworkAdaptor*>(nullptr);

  std::weak_ptr<MockAudioNetworkAdaptor*> mock_ptr(
      states.mock_audio_network_adaptor);
  AudioEncoderOpus::AudioNetworkAdaptorCreator creator = [mock_ptr](
      const std::string&, RtcEventLog* event_log, const Clock*) {
    std::unique_ptr<MockAudioNetworkAdaptor> adaptor(
        new NiceMock<MockAudioNetworkAdaptor>());
    EXPECT_CALL(*adaptor, Die());
    if (auto sp = mock_ptr.lock()) {
      *sp = adaptor.get();
    } else {
      RTC_NOTREACHED();
    }
    return adaptor;
  };

  CodecInst codec_inst = kDefaultOpusSettings;
  codec_inst.channels = num_channels;
  states.config = CreateConfig(codec_inst);
  std::unique_ptr<MockSmoothingFilter> bitrate_smoother(
      new MockSmoothingFilter());
  states.mock_bitrate_smoother = bitrate_smoother.get();
  states.simulated_clock.reset(new SimulatedClock(kInitialTimeUs));
  states.config.clock = states.simulated_clock.get();

  states.encoder.reset(new AudioEncoderOpus(states.config, std::move(creator),
                                            std::move(bitrate_smoother)));
  return states;
}

AudioNetworkAdaptor::EncoderRuntimeConfig CreateEncoderRuntimeConfig() {
  constexpr int kBitrate = 40000;
  constexpr int kFrameLength = 60;
  constexpr bool kEnableFec = true;
  constexpr bool kEnableDtx = false;
  constexpr size_t kNumChannels = 1;
  constexpr float kPacketLossFraction = 0.1f;
  AudioNetworkAdaptor::EncoderRuntimeConfig config;
  config.bitrate_bps = rtc::Optional<int>(kBitrate);
  config.frame_length_ms = rtc::Optional<int>(kFrameLength);
  config.enable_fec = rtc::Optional<bool>(kEnableFec);
  config.enable_dtx = rtc::Optional<bool>(kEnableDtx);
  config.num_channels = rtc::Optional<size_t>(kNumChannels);
  config.uplink_packet_loss_fraction =
      rtc::Optional<float>(kPacketLossFraction);
  return config;
}

void CheckEncoderRuntimeConfig(
    const AudioEncoderOpus* encoder,
    const AudioNetworkAdaptor::EncoderRuntimeConfig& config) {
  EXPECT_EQ(*config.bitrate_bps, encoder->GetTargetBitrate());
  EXPECT_EQ(*config.frame_length_ms, encoder->next_frame_length_ms());
  EXPECT_EQ(*config.enable_fec, encoder->fec_enabled());
  EXPECT_EQ(*config.enable_dtx, encoder->GetDtx());
  EXPECT_EQ(*config.num_channels, encoder->num_channels_to_encode());
}

// Create 10ms audio data blocks for a total packet size of "packet_size_ms".
std::unique_ptr<test::AudioLoop> Create10msAudioBlocks(
    const std::unique_ptr<AudioEncoderOpus>& encoder,
    int packet_size_ms) {
  const std::string file_name =
      test::ResourcePath("audio_coding/testfile32kHz", "pcm");

  std::unique_ptr<test::AudioLoop> speech_data(new test::AudioLoop());
  int audio_samples_per_ms =
      rtc::CheckedDivExact(encoder->SampleRateHz(), 1000);
  RTC_DCHECK(speech_data->Init(
      file_name,
      packet_size_ms * audio_samples_per_ms * encoder->num_channels_to_encode(),
      10 * audio_samples_per_ms * encoder->num_channels_to_encode()));
  return speech_data;
}

}  // namespace

TEST(AudioEncoderOpusTest, DefaultApplicationModeMono) {
  auto states = CreateCodec(1);
  EXPECT_EQ(AudioEncoderOpus::kVoip, states.encoder->application());
}

TEST(AudioEncoderOpusTest, DefaultApplicationModeStereo) {
  auto states = CreateCodec(2);
  EXPECT_EQ(AudioEncoderOpus::kAudio, states.encoder->application());
}

TEST(AudioEncoderOpusTest, ChangeApplicationMode) {
  auto states = CreateCodec(2);
  EXPECT_TRUE(
      states.encoder->SetApplication(AudioEncoder::Application::kSpeech));
  EXPECT_EQ(AudioEncoderOpus::kVoip, states.encoder->application());
}

TEST(AudioEncoderOpusTest, ResetWontChangeApplicationMode) {
  auto states = CreateCodec(2);

  // Trigger a reset.
  states.encoder->Reset();
  // Verify that the mode is still kAudio.
  EXPECT_EQ(AudioEncoderOpus::kAudio, states.encoder->application());

  // Now change to kVoip.
  EXPECT_TRUE(
      states.encoder->SetApplication(AudioEncoder::Application::kSpeech));
  EXPECT_EQ(AudioEncoderOpus::kVoip, states.encoder->application());

  // Trigger a reset again.
  states.encoder->Reset();
  // Verify that the mode is still kVoip.
  EXPECT_EQ(AudioEncoderOpus::kVoip, states.encoder->application());
}

TEST(AudioEncoderOpusTest, ToggleDtx) {
  auto states = CreateCodec(2);
  // Enable DTX
  EXPECT_TRUE(states.encoder->SetDtx(true));
  // Verify that the mode is still kAudio.
  EXPECT_EQ(AudioEncoderOpus::kAudio, states.encoder->application());
  // Turn off DTX.
  EXPECT_TRUE(states.encoder->SetDtx(false));
}

TEST(AudioEncoderOpusTest,
     OnReceivedUplinkBandwidthWithoutAudioNetworkAdaptor) {
  auto states = CreateCodec(1);
  // Constants are replicated from audio_states.encoderopus.cc.
  const int kMinBitrateBps = 6000;
  const int kMaxBitrateBps = 512000;
  // Set a too low bitrate.
  states.encoder->OnReceivedUplinkBandwidth(kMinBitrateBps - 1,
                                            rtc::Optional<int64_t>());
  EXPECT_EQ(kMinBitrateBps, states.encoder->GetTargetBitrate());
  // Set a too high bitrate.
  states.encoder->OnReceivedUplinkBandwidth(kMaxBitrateBps + 1,
                                            rtc::Optional<int64_t>());
  EXPECT_EQ(kMaxBitrateBps, states.encoder->GetTargetBitrate());
  // Set the minimum rate.
  states.encoder->OnReceivedUplinkBandwidth(kMinBitrateBps,
                                            rtc::Optional<int64_t>());
  EXPECT_EQ(kMinBitrateBps, states.encoder->GetTargetBitrate());
  // Set the maximum rate.
  states.encoder->OnReceivedUplinkBandwidth(kMaxBitrateBps,
                                            rtc::Optional<int64_t>());
  EXPECT_EQ(kMaxBitrateBps, states.encoder->GetTargetBitrate());
  // Set rates from kMaxBitrateBps up to 32000 bps.
  for (int rate = kMinBitrateBps; rate <= 32000; rate += 1000) {
    states.encoder->OnReceivedUplinkBandwidth(rate, rtc::Optional<int64_t>());
    EXPECT_EQ(rate, states.encoder->GetTargetBitrate());
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
void TestSetPacketLossRate(AudioEncoderOpusStates* states,
                           const std::vector<float>& losses,
                           float expected_return) {
  // |kSampleIntervalMs| is chosen to ease the calculation since
  // 0.9999 ^ 184198 = 1e-8. Which minimizes the effect of
  // PacketLossFractionSmoother used in AudioEncoderOpus.
  constexpr int64_t kSampleIntervalMs = 184198;
  for (float loss : losses) {
    states->encoder->OnReceivedUplinkPacketLossFraction(loss);
    states->simulated_clock->AdvanceTimeMilliseconds(kSampleIntervalMs);
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
  TestSetPacketLossRate(&states, I(0.00f      , 0.01f - eps), 0.00f);
  TestSetPacketLossRate(&states, I(0.01f + eps, 0.06f - eps), 0.01f);
  TestSetPacketLossRate(&states, I(0.06f + eps, 0.11f - eps), 0.05f);
  TestSetPacketLossRate(&states, I(0.11f + eps, 0.22f - eps), 0.10f);
  TestSetPacketLossRate(&states, I(0.22f + eps, 1.00f      ), 0.20f);

  TestSetPacketLossRate(&states, I(1.00f      , 0.18f + eps), 0.20f);
  TestSetPacketLossRate(&states, I(0.18f - eps, 0.09f + eps), 0.10f);
  TestSetPacketLossRate(&states, I(0.09f - eps, 0.04f + eps), 0.05f);
  TestSetPacketLossRate(&states, I(0.04f - eps, 0.01f + eps), 0.01f);
  TestSetPacketLossRate(&states, I(0.01f - eps, 0.00f      ), 0.00f);
  // clang-format on
}

TEST(AudioEncoderOpusTest, SetReceiverFrameLengthRange) {
  auto states = CreateCodec(2);
  // Before calling to |SetReceiverFrameLengthRange|,
  // |supported_frame_lengths_ms| should contain only the frame length being
  // used.
  using ::testing::ElementsAre;
  EXPECT_THAT(states.encoder->supported_frame_lengths_ms(),
              ElementsAre(states.encoder->next_frame_length_ms()));
  states.encoder->SetReceiverFrameLengthRange(0, 12345);
  states.encoder->SetReceiverFrameLengthRange(21, 60);
  EXPECT_THAT(states.encoder->supported_frame_lengths_ms(), ElementsAre(60));
  states.encoder->SetReceiverFrameLengthRange(20, 59);
  EXPECT_THAT(states.encoder->supported_frame_lengths_ms(), ElementsAre(20));
}

TEST(AudioEncoderOpusTest,
     InvokeAudioNetworkAdaptorOnReceivedUplinkPacketLossFraction) {
  auto states = CreateCodec(2);
  states.encoder->EnableAudioNetworkAdaptor("", nullptr, nullptr);

  auto config = CreateEncoderRuntimeConfig();
  EXPECT_CALL(**states.mock_audio_network_adaptor, GetEncoderRuntimeConfig())
      .WillOnce(Return(config));

  // Since using mock audio network adaptor, any packet loss fraction is fine.
  constexpr float kUplinkPacketLoss = 0.1f;
  EXPECT_CALL(**states.mock_audio_network_adaptor,
              SetUplinkPacketLossFraction(kUplinkPacketLoss));
  states.encoder->OnReceivedUplinkPacketLossFraction(kUplinkPacketLoss);

  CheckEncoderRuntimeConfig(states.encoder.get(), config);
}

TEST(AudioEncoderOpusTest, InvokeAudioNetworkAdaptorOnReceivedUplinkBandwidth) {
  auto states = CreateCodec(2);
  states.encoder->EnableAudioNetworkAdaptor("", nullptr, nullptr);

  auto config = CreateEncoderRuntimeConfig();
  EXPECT_CALL(**states.mock_audio_network_adaptor, GetEncoderRuntimeConfig())
      .WillOnce(Return(config));

  // Since using mock audio network adaptor, any target audio bitrate is fine.
  constexpr int kTargetAudioBitrate = 30000;
  constexpr int64_t kProbingIntervalMs = 3000;
  EXPECT_CALL(**states.mock_audio_network_adaptor,
              SetTargetAudioBitrate(kTargetAudioBitrate));
  EXPECT_CALL(*states.mock_bitrate_smoother,
              SetTimeConstantMs(kProbingIntervalMs * 4));
  EXPECT_CALL(*states.mock_bitrate_smoother, AddSample(kTargetAudioBitrate));
  states.encoder->OnReceivedUplinkBandwidth(
      kTargetAudioBitrate, rtc::Optional<int64_t>(kProbingIntervalMs));

  CheckEncoderRuntimeConfig(states.encoder.get(), config);
}

TEST(AudioEncoderOpusTest, InvokeAudioNetworkAdaptorOnReceivedRtt) {
  auto states = CreateCodec(2);
  states.encoder->EnableAudioNetworkAdaptor("", nullptr, nullptr);

  auto config = CreateEncoderRuntimeConfig();
  EXPECT_CALL(**states.mock_audio_network_adaptor, GetEncoderRuntimeConfig())
      .WillOnce(Return(config));

  // Since using mock audio network adaptor, any rtt is fine.
  constexpr int kRtt = 30;
  EXPECT_CALL(**states.mock_audio_network_adaptor, SetRtt(kRtt));
  states.encoder->OnReceivedRtt(kRtt);

  CheckEncoderRuntimeConfig(states.encoder.get(), config);
}

TEST(AudioEncoderOpusTest, InvokeAudioNetworkAdaptorOnReceivedOverhead) {
  auto states = CreateCodec(2);
  states.encoder->EnableAudioNetworkAdaptor("", nullptr, nullptr);

  auto config = CreateEncoderRuntimeConfig();
  EXPECT_CALL(**states.mock_audio_network_adaptor, GetEncoderRuntimeConfig())
      .WillOnce(Return(config));

  // Since using mock audio network adaptor, any overhead is fine.
  constexpr size_t kOverhead = 64;
  EXPECT_CALL(**states.mock_audio_network_adaptor, SetOverhead(kOverhead));
  states.encoder->OnReceivedOverhead(kOverhead);

  CheckEncoderRuntimeConfig(states.encoder.get(), config);
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
  states.encoder->OnReceivedUplinkPacketLossFraction(kPacketLossFraction_1);
  EXPECT_FLOAT_EQ(0.01f, states.encoder->packet_loss_rate());

  states.simulated_clock->AdvanceTimeMilliseconds(kSecondSampleTimeMs);
  states.encoder->OnReceivedUplinkPacketLossFraction(kPacketLossFraction_2);

  // Now the output of packet loss fraction smoother should be
  // (0.02 + 0.198) / 2 = 0.109, which reach the threshold for the optimized
  // packet loss rate to increase to 0.05. If no smoothing has been made, the
  // optimized packet loss rate should have been increase to 0.1.
  EXPECT_FLOAT_EQ(0.05f, states.encoder->packet_loss_rate());
}

TEST(AudioEncoderOpusTest, DoNotInvokeSetTargetBitrateIfOverheadUnknown) {
  test::ScopedFieldTrials override_field_trials(
      "WebRTC-SendSideBwe-WithOverhead/Enabled/");

  auto states = CreateCodec(2);

  states.encoder->OnReceivedUplinkBandwidth(kDefaultOpusSettings.rate * 2,
                                            rtc::Optional<int64_t>());

  // Since |OnReceivedOverhead| has not been called, the codec bitrate should
  // not change.
  EXPECT_EQ(kDefaultOpusSettings.rate, states.encoder->GetTargetBitrate());
}

TEST(AudioEncoderOpusTest, OverheadRemovedFromTargetAudioBitrate) {
  test::ScopedFieldTrials override_field_trials(
      "WebRTC-SendSideBwe-WithOverhead/Enabled/");

  auto states = CreateCodec(2);

  constexpr size_t kOverheadBytesPerPacket = 64;
  states.encoder->OnReceivedOverhead(kOverheadBytesPerPacket);

  constexpr int kTargetBitrateBps = 40000;
  states.encoder->OnReceivedUplinkBandwidth(kTargetBitrateBps,
                                            rtc::Optional<int64_t>());

  int packet_rate = rtc::CheckedDivExact(48000, kDefaultOpusSettings.pacsize);
  EXPECT_EQ(kTargetBitrateBps -
                8 * static_cast<int>(kOverheadBytesPerPacket) * packet_rate,
            states.encoder->GetTargetBitrate());
}

TEST(AudioEncoderOpusTest, BitrateBounded) {
  test::ScopedFieldTrials override_field_trials(
      "WebRTC-SendSideBwe-WithOverhead/Enabled/");

  constexpr int kMinBitrateBps = 6000;
  constexpr int kMaxBitrateBps = 512000;

  auto states = CreateCodec(2);

  constexpr size_t kOverheadBytesPerPacket = 64;
  states.encoder->OnReceivedOverhead(kOverheadBytesPerPacket);

  int packet_rate = rtc::CheckedDivExact(48000, kDefaultOpusSettings.pacsize);

  // Set a target rate that is smaller than |kMinBitrateBps| when overhead is
  // subtracted. The eventual codec rate should be bounded by |kMinBitrateBps|.
  int target_bitrate =
      kOverheadBytesPerPacket * 8 * packet_rate + kMinBitrateBps - 1;
  states.encoder->OnReceivedUplinkBandwidth(target_bitrate,
                                            rtc::Optional<int64_t>());
  EXPECT_EQ(kMinBitrateBps, states.encoder->GetTargetBitrate());

  // Set a target rate that is greater than |kMaxBitrateBps| when overhead is
  // subtracted. The eventual codec rate should be bounded by |kMaxBitrateBps|.
  target_bitrate =
      kOverheadBytesPerPacket * 8 * packet_rate + kMaxBitrateBps + 1;
  states.encoder->OnReceivedUplinkBandwidth(target_bitrate,
                                            rtc::Optional<int64_t>());
  EXPECT_EQ(kMaxBitrateBps, states.encoder->GetTargetBitrate());
}

// Verifies that the complexity adaptation in the config works as intended.
TEST(AudioEncoderOpusTest, ConfigComplexityAdaptation) {
  AudioEncoderOpus::Config config;
  config.low_rate_complexity = 8;
  config.complexity = 6;

  // Bitrate within hysteresis window. Expect empty output.
  config.bitrate_bps = rtc::Optional<int>(12500);
  EXPECT_EQ(rtc::Optional<int>(), config.GetNewComplexity());

  // Bitrate below hysteresis window. Expect higher complexity.
  config.bitrate_bps = rtc::Optional<int>(10999);
  EXPECT_EQ(rtc::Optional<int>(8), config.GetNewComplexity());

  // Bitrate within hysteresis window. Expect empty output.
  config.bitrate_bps = rtc::Optional<int>(12500);
  EXPECT_EQ(rtc::Optional<int>(), config.GetNewComplexity());

  // Bitrate above hysteresis window. Expect lower complexity.
  config.bitrate_bps = rtc::Optional<int>(14001);
  EXPECT_EQ(rtc::Optional<int>(6), config.GetNewComplexity());
}

TEST(AudioEncoderOpusTest, EmptyConfigDoesNotAffectEncoderSettings) {
  auto states = CreateCodec(2);
  states.encoder->EnableAudioNetworkAdaptor("", nullptr, nullptr);

  auto config = CreateEncoderRuntimeConfig();
  AudioNetworkAdaptor::EncoderRuntimeConfig empty_config;

  EXPECT_CALL(**states.mock_audio_network_adaptor, GetEncoderRuntimeConfig())
      .WillOnce(Return(config))
      .WillOnce(Return(empty_config));

  constexpr size_t kOverhead = 64;
  EXPECT_CALL(**states.mock_audio_network_adaptor, SetOverhead(kOverhead))
      .Times(2);
  states.encoder->OnReceivedOverhead(kOverhead);
  states.encoder->OnReceivedOverhead(kOverhead);

  CheckEncoderRuntimeConfig(states.encoder.get(), config);
}

TEST(AudioEncoderOpusTest, UpdateUplinkBandwidthInAudioNetworkAdaptor) {
  rtc::ScopedFakeClock fake_clock;
  auto states = CreateCodec(2);
  states.encoder->EnableAudioNetworkAdaptor("", nullptr, nullptr);
  std::array<int16_t, 480 * 2> audio;
  audio.fill(0);
  rtc::Buffer encoded;
  EXPECT_CALL(*states.mock_bitrate_smoother, GetAverage())
      .WillOnce(Return(rtc::Optional<float>(50000)));
  EXPECT_CALL(**states.mock_audio_network_adaptor, SetUplinkBandwidth(50000));
  states.encoder->Encode(
      0, rtc::ArrayView<const int16_t>(audio.data(), audio.size()), &encoded);

  // Repeat update uplink bandwidth tests.
  for (int i = 0; i < 5; i++) {
    // Don't update till it is time to update again.
    fake_clock.AdvanceTime(rtc::TimeDelta::FromMilliseconds(
        states.config.uplink_bandwidth_update_interval_ms - 1));
    states.encoder->Encode(
        0, rtc::ArrayView<const int16_t>(audio.data(), audio.size()), &encoded);

    // Update when it is time to update.
    EXPECT_CALL(*states.mock_bitrate_smoother, GetAverage())
        .WillOnce(Return(rtc::Optional<float>(40000)));
    EXPECT_CALL(**states.mock_audio_network_adaptor, SetUplinkBandwidth(40000));
    fake_clock.AdvanceTime(rtc::TimeDelta::FromMilliseconds(1));
    states.encoder->Encode(
        0, rtc::ArrayView<const int16_t>(audio.data(), audio.size()), &encoded);
  }
}

TEST(AudioEncoderOpusTest, EncodeAtMinBitrate) {
  auto states = CreateCodec(1);
  constexpr int kNumPacketsToEncode = 2;
  auto audio_frames =
      Create10msAudioBlocks(states.encoder, kNumPacketsToEncode * 20);
  rtc::Buffer encoded;
  uint32_t rtp_timestamp = 12345;  // Just a number not important to this test.

  states.encoder->OnReceivedUplinkBandwidth(0, rtc::Optional<int64_t>());
  for (int packet_index = 0; packet_index < kNumPacketsToEncode;
       packet_index++) {
    // Make sure we are not encoding before we have enough data for
    // a 20ms packet.
    for (int index = 0; index < 1; index++) {
      states.encoder->Encode(rtp_timestamp, audio_frames->GetNextBlock(),
                             &encoded);
      EXPECT_EQ(0u, encoded.size());
    }

    // Should encode now.
    states.encoder->Encode(rtp_timestamp, audio_frames->GetNextBlock(),
                           &encoded);
    EXPECT_GT(encoded.size(), 0u);
    encoded.Clear();
  }
}

}  // namespace webrtc
