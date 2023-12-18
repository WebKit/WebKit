/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/voip/audio_egress.h"

#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/call/transport.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/audio_mixer/sine_wave_generator.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_rtcp_impl2.h"
#include "rtc_base/event.h"
#include "rtc_base/logging.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_transport.h"
#include "test/run_loop.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {
namespace {

using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Unused;

std::unique_ptr<ModuleRtpRtcpImpl2> CreateRtpStack(Clock* clock,
                                                   Transport* transport,
                                                   uint32_t remote_ssrc) {
  RtpRtcpInterface::Configuration rtp_config;
  rtp_config.clock = clock;
  rtp_config.audio = true;
  rtp_config.rtcp_report_interval_ms = 5000;
  rtp_config.outgoing_transport = transport;
  rtp_config.local_media_ssrc = remote_ssrc;
  auto rtp_rtcp = ModuleRtpRtcpImpl2::Create(rtp_config);
  rtp_rtcp->SetSendingMediaStatus(false);
  rtp_rtcp->SetRTCPStatus(RtcpMode::kCompound);
  return rtp_rtcp;
}

constexpr int16_t kAudioLevel = 3004;  // Used for sine wave level.

// AudioEgressTest configures audio egress by using Rtp Stack, fake clock,
// and task queue factory.  Encoder factory is needed to create codec and
// configure the RTP stack in audio egress.
class AudioEgressTest : public ::testing::Test {
 public:
  static constexpr uint16_t kSeqNum = 12345;
  static constexpr uint64_t kStartTime = 123456789;
  static constexpr uint32_t kRemoteSsrc = 0xDEADBEEF;
  const SdpAudioFormat kPcmuFormat = {"pcmu", 8000, 1};

  AudioEgressTest() : wave_generator_(1000.0, kAudioLevel) {
    encoder_factory_ = CreateBuiltinAudioEncoderFactory();
  }

  // Prepare test on audio egress by using PCMu codec with specific
  // sequence number and its status to be running.
  void SetUp() override {
    rtp_rtcp_ =
        CreateRtpStack(time_controller_.GetClock(), &transport_, kRemoteSsrc);
    egress_ = std::make_unique<AudioEgress>(
        rtp_rtcp_.get(), time_controller_.GetClock(),
        time_controller_.GetTaskQueueFactory());
    constexpr int kPcmuPayload = 0;
    egress_->SetEncoder(kPcmuPayload, kPcmuFormat,
                        encoder_factory_->MakeAudioEncoder(
                            kPcmuPayload, kPcmuFormat, absl::nullopt));
    egress_->StartSend();
    rtp_rtcp_->SetSequenceNumber(kSeqNum);
    rtp_rtcp_->SetSendingStatus(true);
  }

  // Make sure we have shut down rtp stack and reset egress for each test.
  void TearDown() override {
    egress_->StopSend();
    rtp_rtcp_->SetSendingStatus(false);
    egress_.reset();
    rtp_rtcp_.reset();
  }

  // Create an audio frame prepared for pcmu encoding. Timestamp is
  // increased per RTP specification which is the number of samples it contains.
  // Wave generator writes sine wave which has expected high level set
  // by kAudioLevel.
  std::unique_ptr<AudioFrame> GetAudioFrame(int order) {
    auto frame = std::make_unique<AudioFrame>();
    frame->sample_rate_hz_ = kPcmuFormat.clockrate_hz;
    frame->samples_per_channel_ = kPcmuFormat.clockrate_hz / 100;  // 10 ms.
    frame->num_channels_ = kPcmuFormat.num_channels;
    frame->timestamp_ = frame->samples_per_channel_ * order;
    wave_generator_.GenerateNextFrame(frame.get());
    return frame;
  }

  GlobalSimulatedTimeController time_controller_{Timestamp::Micros(kStartTime)};
  NiceMock<MockTransport> transport_;
  SineWaveGenerator wave_generator_;
  std::unique_ptr<ModuleRtpRtcpImpl2> rtp_rtcp_;
  rtc::scoped_refptr<AudioEncoderFactory> encoder_factory_;
  std::unique_ptr<AudioEgress> egress_;
};

TEST_F(AudioEgressTest, SendingStatusAfterStartAndStop) {
  EXPECT_TRUE(egress_->IsSending());
  egress_->StopSend();
  EXPECT_FALSE(egress_->IsSending());
}

TEST_F(AudioEgressTest, ProcessAudioWithMute) {
  constexpr int kExpected = 10;
  rtc::Event event;
  int rtp_count = 0;
  RtpPacketReceived rtp;
  auto rtp_sent = [&](rtc::ArrayView<const uint8_t> packet, Unused) {
    rtp.Parse(packet);
    if (++rtp_count == kExpected) {
      event.Set();
    }
    return true;
  };

  EXPECT_CALL(transport_, SendRtp).WillRepeatedly(Invoke(rtp_sent));

  egress_->SetMute(true);

  // Two 10 ms audio frames will result in rtp packet with ptime 20.
  for (size_t i = 0; i < kExpected * 2; i++) {
    egress_->SendAudioData(GetAudioFrame(i));
    time_controller_.AdvanceTime(TimeDelta::Millis(10));
  }

  event.Wait(TimeDelta::Seconds(1));
  EXPECT_EQ(rtp_count, kExpected);

  // we expect on pcmu payload to result in 255 for silenced payload
  RTPHeader header;
  rtp.GetHeader(&header);
  size_t packet_length = rtp.size();
  size_t payload_length = packet_length - header.headerLength;
  size_t payload_data_length = payload_length - header.paddingLength;
  const uint8_t* payload = rtp.data() + header.headerLength;
  for (size_t i = 0; i < payload_data_length; ++i) {
    EXPECT_EQ(*payload++, 255);
  }
}

TEST_F(AudioEgressTest, ProcessAudioWithSineWave) {
  constexpr int kExpected = 10;
  rtc::Event event;
  int rtp_count = 0;
  RtpPacketReceived rtp;
  auto rtp_sent = [&](rtc::ArrayView<const uint8_t> packet, Unused) {
    rtp.Parse(packet);
    if (++rtp_count == kExpected) {
      event.Set();
    }
    return true;
  };

  EXPECT_CALL(transport_, SendRtp).WillRepeatedly(Invoke(rtp_sent));

  // Two 10 ms audio frames will result in rtp packet with ptime 20.
  for (size_t i = 0; i < kExpected * 2; i++) {
    egress_->SendAudioData(GetAudioFrame(i));
    time_controller_.AdvanceTime(TimeDelta::Millis(10));
  }

  event.Wait(TimeDelta::Seconds(1));
  EXPECT_EQ(rtp_count, kExpected);

  // we expect on pcmu to result in < 255 for payload with sine wave
  RTPHeader header;
  rtp.GetHeader(&header);
  size_t packet_length = rtp.size();
  size_t payload_length = packet_length - header.headerLength;
  size_t payload_data_length = payload_length - header.paddingLength;
  const uint8_t* payload = rtp.data() + header.headerLength;
  for (size_t i = 0; i < payload_data_length; ++i) {
    EXPECT_NE(*payload++, 255);
  }
}

TEST_F(AudioEgressTest, SkipAudioEncodingAfterStopSend) {
  constexpr int kExpected = 10;
  rtc::Event event;
  int rtp_count = 0;
  auto rtp_sent = [&](rtc::ArrayView<const uint8_t> packet, Unused) {
    if (++rtp_count == kExpected) {
      event.Set();
    }
    return true;
  };

  EXPECT_CALL(transport_, SendRtp).WillRepeatedly(Invoke(rtp_sent));

  // Two 10 ms audio frames will result in rtp packet with ptime 20.
  for (size_t i = 0; i < kExpected * 2; i++) {
    egress_->SendAudioData(GetAudioFrame(i));
    time_controller_.AdvanceTime(TimeDelta::Millis(10));
  }

  event.Wait(TimeDelta::Seconds(1));
  EXPECT_EQ(rtp_count, kExpected);

  // Now stop send and yet feed more data.
  egress_->StopSend();

  // It should be safe to exit the test case while encoder_queue_ has
  // outstanding data to process. We are making sure that this doesn't
  // result in crashes or sanitizer errors due to remaining data.
  for (size_t i = 0; i < kExpected * 2; i++) {
    egress_->SendAudioData(GetAudioFrame(i));
    time_controller_.AdvanceTime(TimeDelta::Millis(10));
  }
}

TEST_F(AudioEgressTest, ChangeEncoderFromPcmuToOpus) {
  absl::optional<SdpAudioFormat> pcmu = egress_->GetEncoderFormat();
  EXPECT_TRUE(pcmu);
  EXPECT_EQ(pcmu->clockrate_hz, kPcmuFormat.clockrate_hz);
  EXPECT_EQ(pcmu->num_channels, kPcmuFormat.num_channels);

  constexpr int kOpusPayload = 120;
  const SdpAudioFormat kOpusFormat = {"opus", 48000, 2};

  egress_->SetEncoder(kOpusPayload, kOpusFormat,
                      encoder_factory_->MakeAudioEncoder(
                          kOpusPayload, kOpusFormat, absl::nullopt));

  absl::optional<SdpAudioFormat> opus = egress_->GetEncoderFormat();
  EXPECT_TRUE(opus);
  EXPECT_EQ(opus->clockrate_hz, kOpusFormat.clockrate_hz);
  EXPECT_EQ(opus->num_channels, kOpusFormat.num_channels);
}

TEST_F(AudioEgressTest, SendDTMF) {
  constexpr int kExpected = 7;
  constexpr int kPayloadType = 100;
  constexpr int kDurationMs = 100;
  constexpr int kSampleRate = 8000;
  constexpr int kEvent = 3;

  egress_->RegisterTelephoneEventType(kPayloadType, kSampleRate);
  // 100 ms duration will produce total 7 DTMF
  // 1 @ 20 ms, 2 @ 40 ms, 3 @ 60 ms, 4 @ 80 ms
  // 5, 6, 7 @ 100 ms (last one sends 3 dtmf)
  egress_->SendTelephoneEvent(kEvent, kDurationMs);

  rtc::Event event;
  int dtmf_count = 0;
  auto is_dtmf = [&](RtpPacketReceived& rtp) {
    return (rtp.PayloadType() == kPayloadType &&
            rtp.SequenceNumber() == kSeqNum + dtmf_count &&
            rtp.padding_size() == 0 && rtp.Marker() == (dtmf_count == 0) &&
            rtp.Ssrc() == kRemoteSsrc);
  };

  // It's possible that we may have actual audio RTP packets along with
  // DTMF packtets.  We are only interested in the exact number of DTMF
  // packets rtp stack is emitting.
  auto rtp_sent = [&](rtc::ArrayView<const uint8_t> packet, Unused) {
    RtpPacketReceived rtp;
    rtp.Parse(packet);
    if (is_dtmf(rtp) && ++dtmf_count == kExpected) {
      event.Set();
    }
    return true;
  };

  EXPECT_CALL(transport_, SendRtp).WillRepeatedly(Invoke(rtp_sent));

  // Two 10 ms audio frames will result in rtp packet with ptime 20.
  for (size_t i = 0; i < kExpected * 2; i++) {
    egress_->SendAudioData(GetAudioFrame(i));
    time_controller_.AdvanceTime(TimeDelta::Millis(10));
  }

  event.Wait(TimeDelta::Seconds(1));
  EXPECT_EQ(dtmf_count, kExpected);
}

TEST_F(AudioEgressTest, TestAudioInputLevelAndEnergyDuration) {
  // Per audio_level's kUpdateFrequency, we need more than 10 audio samples to
  // get audio level from input source.
  constexpr int kExpected = 6;
  rtc::Event event;
  int rtp_count = 0;
  auto rtp_sent = [&](rtc::ArrayView<const uint8_t> packet, Unused) {
    if (++rtp_count == kExpected) {
      event.Set();
    }
    return true;
  };

  EXPECT_CALL(transport_, SendRtp).WillRepeatedly(Invoke(rtp_sent));

  // Two 10 ms audio frames will result in rtp packet with ptime 20.
  for (size_t i = 0; i < kExpected * 2; i++) {
    egress_->SendAudioData(GetAudioFrame(i));
    time_controller_.AdvanceTime(TimeDelta::Millis(10));
  }

  event.Wait(/*give_up_after=*/TimeDelta::Seconds(1));
  EXPECT_EQ(rtp_count, kExpected);

  constexpr double kExpectedEnergy = 0.00016809565587789564;
  constexpr double kExpectedDuration = 0.11999999999999998;

  EXPECT_EQ(egress_->GetInputAudioLevel(), kAudioLevel);
  EXPECT_DOUBLE_EQ(egress_->GetInputTotalEnergy(), kExpectedEnergy);
  EXPECT_DOUBLE_EQ(egress_->GetInputTotalDuration(), kExpectedDuration);
}

}  // namespace
}  // namespace webrtc
