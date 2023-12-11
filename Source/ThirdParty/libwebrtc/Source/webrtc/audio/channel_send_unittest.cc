/*
 *  Copyright 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/channel_send.h"

#include <utility>

#include "api/audio/audio_frame.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "api/scoped_refptr.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "call/rtp_transport_controller_send.h"
#include "rtc_base/gunit.h"
#include "test/gtest.h"
#include "test/mock_frame_transformer.h"
#include "test/mock_transport.h"
#include "test/scoped_key_value_config.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {
namespace voe {
namespace {

using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;

constexpr int kRtcpIntervalMs = 1000;
constexpr int kSsrc = 333;
constexpr int kPayloadType = 1;
constexpr int kSampleRateHz = 48000;
constexpr int kRtpRateHz = 48000;

BitrateConstraints GetBitrateConfig() {
  BitrateConstraints bitrate_config;
  bitrate_config.min_bitrate_bps = 10000;
  bitrate_config.start_bitrate_bps = 100000;
  bitrate_config.max_bitrate_bps = 1000000;
  return bitrate_config;
}

class ChannelSendTest : public ::testing::Test {
 protected:
  ChannelSendTest()
      : time_controller_(Timestamp::Seconds(1)),
        transport_controller_(
            time_controller_.GetClock(),
            RtpTransportConfig{
                .bitrate_config = GetBitrateConfig(),
                .event_log = &event_log_,
                .task_queue_factory = time_controller_.GetTaskQueueFactory(),
                .trials = &field_trials_,
            }) {
    channel_ = voe::CreateChannelSend(
        time_controller_.GetClock(), time_controller_.GetTaskQueueFactory(),
        &transport_, nullptr, &event_log_, nullptr, crypto_options_, false,
        kRtcpIntervalMs, kSsrc, nullptr, &transport_controller_, field_trials_);
    encoder_factory_ = CreateBuiltinAudioEncoderFactory();
    std::unique_ptr<AudioEncoder> encoder = encoder_factory_->MakeAudioEncoder(
        kPayloadType, SdpAudioFormat("opus", kRtpRateHz, 2), {});
    channel_->SetEncoder(kPayloadType, std::move(encoder));
    transport_controller_.EnsureStarted();
    channel_->RegisterSenderCongestionControlObjects(&transport_controller_);
    ON_CALL(transport_, SendRtcp).WillByDefault(Return(true));
    ON_CALL(transport_, SendRtp).WillByDefault(Return(true));
  }

  std::unique_ptr<AudioFrame> CreateAudioFrame() {
    auto frame = std::make_unique<AudioFrame>();
    frame->sample_rate_hz_ = kSampleRateHz;
    frame->samples_per_channel_ = kSampleRateHz / 100;
    frame->num_channels_ = 1;
    frame->set_absolute_capture_timestamp_ms(
        time_controller_.GetClock()->TimeInMilliseconds());
    return frame;
  }

  void ProcessNextFrame() {
    channel_->ProcessAndEncodeAudio(CreateAudioFrame());
    // Advance time to process the task queue.
    time_controller_.AdvanceTime(TimeDelta::Millis(10));
  }

  GlobalSimulatedTimeController time_controller_;
  webrtc::test::ScopedKeyValueConfig field_trials_;
  RtcEventLogNull event_log_;
  NiceMock<MockTransport> transport_;
  CryptoOptions crypto_options_;
  RtpTransportControllerSend transport_controller_;
  std::unique_ptr<ChannelSendInterface> channel_;
  rtc::scoped_refptr<AudioEncoderFactory> encoder_factory_;
};

TEST_F(ChannelSendTest, StopSendShouldResetEncoder) {
  channel_->StartSend();
  // Insert two frames which should trigger a new packet.
  EXPECT_CALL(transport_, SendRtp).Times(1);
  ProcessNextFrame();
  ProcessNextFrame();

  EXPECT_CALL(transport_, SendRtp).Times(0);
  ProcessNextFrame();
  // StopSend should clear the previous audio frame stored in the encoder.
  channel_->StopSend();
  channel_->StartSend();
  // The following frame should not trigger a new packet since the encoder
  // needs 20 ms audio.
  EXPECT_CALL(transport_, SendRtp).Times(0);
  ProcessNextFrame();
}

TEST_F(ChannelSendTest, IncreaseRtpTimestampByPauseDuration) {
  channel_->StartSend();
  uint32_t timestamp;
  int sent_packets = 0;
  auto send_rtp = [&](rtc::ArrayView<const uint8_t> data,
                      const PacketOptions& options) {
    ++sent_packets;
    RtpPacketReceived packet;
    packet.Parse(data);
    timestamp = packet.Timestamp();
    return true;
  };
  EXPECT_CALL(transport_, SendRtp).WillRepeatedly(Invoke(send_rtp));
  ProcessNextFrame();
  ProcessNextFrame();
  EXPECT_EQ(sent_packets, 1);
  uint32_t first_timestamp = timestamp;
  channel_->StopSend();
  time_controller_.AdvanceTime(TimeDelta::Seconds(10));
  channel_->StartSend();

  ProcessNextFrame();
  ProcessNextFrame();
  EXPECT_EQ(sent_packets, 2);
  int64_t timestamp_gap_ms =
      static_cast<int64_t>(timestamp - first_timestamp) * 1000 / kRtpRateHz;
  EXPECT_EQ(timestamp_gap_ms, 10020);
}

TEST_F(ChannelSendTest, FrameTransformerGetsCorrectTimestamp) {
  rtc::scoped_refptr<MockFrameTransformer> mock_frame_transformer =
      rtc::make_ref_counted<MockFrameTransformer>();
  channel_->SetEncoderToPacketizerFrameTransformer(mock_frame_transformer);
  rtc::scoped_refptr<TransformedFrameCallback> callback;
  EXPECT_CALL(*mock_frame_transformer, RegisterTransformedFrameCallback)
      .WillOnce(SaveArg<0>(&callback));
  EXPECT_CALL(*mock_frame_transformer, UnregisterTransformedFrameCallback);

  absl::optional<uint32_t> sent_timestamp;
  auto send_rtp = [&](rtc::ArrayView<const uint8_t> data,
                      const PacketOptions& options) {
    RtpPacketReceived packet;
    packet.Parse(data);
    if (!sent_timestamp) {
      sent_timestamp = packet.Timestamp();
    }
    return true;
  };
  EXPECT_CALL(transport_, SendRtp).WillRepeatedly(Invoke(send_rtp));

  channel_->StartSend();
  int64_t transformable_frame_timestamp = -1;
  EXPECT_CALL(*mock_frame_transformer, Transform)
      .WillOnce([&](std::unique_ptr<TransformableFrameInterface> frame) {
        transformable_frame_timestamp = frame->GetTimestamp();
        callback->OnTransformedFrame(std::move(frame));
      });
  // Insert two frames which should trigger a new packet.
  ProcessNextFrame();
  ProcessNextFrame();

  // Ensure the RTP timestamp on the frame passed to the transformer
  // includes the RTP offset and matches the actual RTP timestamp on the sent
  // packet.
  EXPECT_EQ_WAIT(transformable_frame_timestamp,
                 0 + channel_->GetRtpRtcp()->StartTimestamp(), 1000);
  EXPECT_TRUE_WAIT(sent_timestamp, 1000);
  EXPECT_EQ(*sent_timestamp, transformable_frame_timestamp);
}
}  // namespace
}  // namespace voe
}  // namespace webrtc
