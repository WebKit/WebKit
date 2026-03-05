/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/voip/audio_ingress.h"

#include <cstddef>
#include <cstdint>
#include <memory>

#include "api/array_view.h"
#include "api/audio/audio_frame.h"
#include "api/audio/audio_mixer.h"
#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/audio_codecs/audio_format.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/environment/environment.h"
#include "api/environment/environment_factory.h"
#include "api/rtp_headers.h"
#include "api/scoped_refptr.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "audio/voip/audio_egress.h"
#include "modules/audio_mixer/sine_wave_generator.h"
#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "modules/rtp_rtcp/source/rtp_rtcp_impl2.h"
#include "modules/rtp_rtcp/source/rtp_rtcp_interface.h"
#include "rtc_base/event.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_transport.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {
namespace {

using ::testing::NiceMock;
using ::testing::Unused;

constexpr int16_t kAudioLevel = 3004;  // Used for sine wave level.

class AudioIngressTest : public ::testing::Test {
 public:
  const SdpAudioFormat kPcmuFormat = {"pcmu", 8000, 1};

  AudioIngressTest() : wave_generator_(1000.0, kAudioLevel) {
    receive_statistics_ =
        ReceiveStatistics::Create(time_controller_.GetClock());

    RtpRtcpInterface::Configuration rtp_config;
    rtp_config.audio = true;
    rtp_config.receive_statistics = receive_statistics_.get();
    rtp_config.rtcp_report_interval_ms = 5000;
    rtp_config.outgoing_transport = &transport_;
    rtp_config.local_media_ssrc = 0xdeadc0de;
    rtp_rtcp_ = std::make_unique<ModuleRtpRtcpImpl2>(env_, rtp_config);

    rtp_rtcp_->SetSendingMediaStatus(false);
    rtp_rtcp_->SetRTCPStatus(RtcpMode::kCompound);

    encoder_factory_ = CreateBuiltinAudioEncoderFactory();
    decoder_factory_ = CreateBuiltinAudioDecoderFactory();
  }

  void SetUp() override {
    constexpr int kPcmuPayload = 0;
    ingress_ = std::make_unique<AudioIngress>(
        env_, rtp_rtcp_.get(), receive_statistics_.get(), decoder_factory_);
    ingress_->SetReceiveCodecs({{kPcmuPayload, kPcmuFormat}});

    egress_ = std::make_unique<AudioEgress>(env_, rtp_rtcp_.get());
    egress_->SetEncoder(kPcmuPayload, kPcmuFormat,
                        encoder_factory_->Create(
                            env_, kPcmuFormat, {.payload_type = kPcmuPayload}));
    egress_->StartSend();
    ingress_->StartPlay();
    rtp_rtcp_->SetSendingStatus(true);
  }

  void TearDown() override {
    rtp_rtcp_->SetSendingStatus(false);
    ingress_->StopPlay();
    egress_->StopSend();
    egress_.reset();
    ingress_.reset();
  }

  std::unique_ptr<AudioFrame> GetAudioFrame(int order) {
    auto frame = std::make_unique<AudioFrame>();
    frame->sample_rate_hz_ = kPcmuFormat.clockrate_hz;
    frame->samples_per_channel_ = kPcmuFormat.clockrate_hz / 100;  // 10 ms.
    frame->num_channels_ = kPcmuFormat.num_channels;
    frame->timestamp_ = frame->samples_per_channel_ * order;
    wave_generator_.GenerateNextFrame(frame.get());
    return frame;
  }

  GlobalSimulatedTimeController time_controller_{Timestamp::Micros(123456789)};
  const Environment env_ =
      CreateEnvironment(time_controller_.GetClock(),
                        time_controller_.GetTaskQueueFactory());
  SineWaveGenerator wave_generator_;
  NiceMock<MockTransport> transport_;
  std::unique_ptr<ReceiveStatistics> receive_statistics_;
  std::unique_ptr<ModuleRtpRtcpImpl2> rtp_rtcp_;
  scoped_refptr<AudioEncoderFactory> encoder_factory_;
  scoped_refptr<AudioDecoderFactory> decoder_factory_;
  std::unique_ptr<AudioIngress> ingress_;
  std::unique_ptr<AudioEgress> egress_;
};

TEST_F(AudioIngressTest, PlayingAfterStartAndStop) {
  EXPECT_EQ(ingress_->IsPlaying(), true);
  ingress_->StopPlay();
  EXPECT_EQ(ingress_->IsPlaying(), false);
}

TEST_F(AudioIngressTest, GetAudioFrameAfterRtpReceived) {
  Event event;
  auto handle_rtp = [&](ArrayView<const uint8_t> packet, Unused) {
    ingress_->ReceivedRTPPacket(packet);
    event.Set();
    return true;
  };
  EXPECT_CALL(transport_, SendRtp).WillRepeatedly(handle_rtp);
  egress_->SendAudioData(GetAudioFrame(0));
  egress_->SendAudioData(GetAudioFrame(1));
  time_controller_.AdvanceTime(TimeDelta::Zero());
  ASSERT_TRUE(event.Wait(TimeDelta::Seconds(1)));

  AudioFrame audio_frame;
  EXPECT_EQ(
      ingress_->GetAudioFrameWithInfo(kPcmuFormat.clockrate_hz, &audio_frame),
      AudioMixer::Source::AudioFrameInfo::kNormal);
  EXPECT_FALSE(audio_frame.muted());
  EXPECT_EQ(audio_frame.num_channels_, 1u);
  EXPECT_EQ(audio_frame.samples_per_channel_,
            static_cast<size_t>(kPcmuFormat.clockrate_hz / 100));
  EXPECT_EQ(audio_frame.sample_rate_hz_, kPcmuFormat.clockrate_hz);
  EXPECT_NE(audio_frame.timestamp_, 0u);
  EXPECT_EQ(audio_frame.elapsed_time_ms_, 0);
}

TEST_F(AudioIngressTest, TestSpeechOutputLevelAndEnergyDuration) {
  // Per audio_level's kUpdateFrequency, we need more than 10 audio samples to
  // get audio level from output source.
  constexpr int kNumRtp = 6;
  int rtp_count = 0;
  Event event;
  auto handle_rtp = [&](ArrayView<const uint8_t> packet, Unused) {
    ingress_->ReceivedRTPPacket(packet);
    if (++rtp_count == kNumRtp) {
      event.Set();
    }
    return true;
  };
  EXPECT_CALL(transport_, SendRtp).WillRepeatedly(handle_rtp);
  for (int i = 0; i < kNumRtp * 2; i++) {
    egress_->SendAudioData(GetAudioFrame(i));
    time_controller_.AdvanceTime(TimeDelta::Millis(10));
  }
  event.Wait(/*give_up_after=*/TimeDelta::Seconds(1));

  for (int i = 0; i < kNumRtp * 2; ++i) {
    AudioFrame audio_frame;
    EXPECT_EQ(
        ingress_->GetAudioFrameWithInfo(kPcmuFormat.clockrate_hz, &audio_frame),
        AudioMixer::Source::AudioFrameInfo::kNormal);
  }
  EXPECT_EQ(ingress_->GetOutputAudioLevel(), kAudioLevel);

  constexpr double kExpectedEnergy = 0.00016809565587789564;
  constexpr double kExpectedDuration = 0.11999999999999998;

  EXPECT_DOUBLE_EQ(ingress_->GetOutputTotalEnergy(), kExpectedEnergy);
  EXPECT_DOUBLE_EQ(ingress_->GetOutputTotalDuration(), kExpectedDuration);
}

TEST_F(AudioIngressTest, PreferredSampleRate) {
  Event event;
  auto handle_rtp = [&](ArrayView<const uint8_t> packet, Unused) {
    ingress_->ReceivedRTPPacket(packet);
    event.Set();
    return true;
  };
  EXPECT_CALL(transport_, SendRtp).WillRepeatedly(handle_rtp);
  egress_->SendAudioData(GetAudioFrame(0));
  egress_->SendAudioData(GetAudioFrame(1));
  time_controller_.AdvanceTime(TimeDelta::Zero());
  ASSERT_TRUE(event.Wait(TimeDelta::Seconds(1)));

  AudioFrame audio_frame;
  EXPECT_EQ(
      ingress_->GetAudioFrameWithInfo(kPcmuFormat.clockrate_hz, &audio_frame),
      AudioMixer::Source::AudioFrameInfo::kNormal);
  EXPECT_EQ(ingress_->PreferredSampleRate(), kPcmuFormat.clockrate_hz);
}

// This test highlights the case where caller invokes StopPlay() which then
// AudioIngress should play silence frame afterwards.
TEST_F(AudioIngressTest, GetMutedAudioFrameAfterRtpReceivedAndStopPlay) {
  // StopPlay before we start sending RTP packet with sine wave.
  ingress_->StopPlay();

  // Send 6 RTP packets to generate more than 100 ms audio sample to get
  // valid speech level.
  constexpr int kNumRtp = 6;
  int rtp_count = 0;
  Event event;
  auto handle_rtp = [&](ArrayView<const uint8_t> packet, Unused) {
    ingress_->ReceivedRTPPacket(packet);
    if (++rtp_count == kNumRtp) {
      event.Set();
    }
    return true;
  };
  EXPECT_CALL(transport_, SendRtp).WillRepeatedly(handle_rtp);
  for (int i = 0; i < kNumRtp * 2; i++) {
    egress_->SendAudioData(GetAudioFrame(i));
    time_controller_.AdvanceTime(TimeDelta::Millis(10));
  }
  event.Wait(/*give_up_after=*/TimeDelta::Seconds(1));

  for (int i = 0; i < kNumRtp * 2; ++i) {
    AudioFrame audio_frame;
    EXPECT_EQ(
        ingress_->GetAudioFrameWithInfo(kPcmuFormat.clockrate_hz, &audio_frame),
        AudioMixer::Source::AudioFrameInfo::kMuted);
    const int16_t* audio_data = audio_frame.data();
    size_t length =
        audio_frame.samples_per_channel_ * audio_frame.num_channels_;
    for (size_t j = 0; j < length; ++j) {
      EXPECT_EQ(audio_data[j], 0);
    }
  }

  // Now we should still see valid speech output level as StopPlay won't affect
  // the measurement.
  EXPECT_EQ(ingress_->GetOutputAudioLevel(), kAudioLevel);
}

}  // namespace
}  // namespace webrtc
