/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/voip/audio_channel.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/call/transport.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "modules/audio_mixer/audio_mixer_impl.h"
#include "modules/audio_mixer/sine_wave_generator.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/event.h"
#include "rtc_base/logging.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_transport.h"

namespace webrtc {
namespace {

using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Unused;

constexpr uint64_t kStartTime = 123456789;
constexpr uint32_t kLocalSsrc = 0xdeadc0de;
constexpr int16_t kAudioLevel = 3004;  // used for sine wave level
constexpr int kPcmuPayload = 0;

class AudioChannelTest : public ::testing::Test {
 public:
  const SdpAudioFormat kPcmuFormat = {"pcmu", 8000, 1};

  AudioChannelTest()
      : fake_clock_(kStartTime), wave_generator_(1000.0, kAudioLevel) {
    process_thread_ = ProcessThread::Create("ModuleProcessThread");
    audio_mixer_ = AudioMixerImpl::Create();
    task_queue_factory_ = CreateDefaultTaskQueueFactory();
    encoder_factory_ = CreateBuiltinAudioEncoderFactory();
    decoder_factory_ = CreateBuiltinAudioDecoderFactory();
  }

  void SetUp() override {
    audio_channel_ = new rtc::RefCountedObject<AudioChannel>(
        &transport_, kLocalSsrc, task_queue_factory_.get(),
        process_thread_.get(), audio_mixer_.get(), decoder_factory_);

    audio_channel_->SetEncoder(kPcmuPayload, kPcmuFormat,
                               encoder_factory_->MakeAudioEncoder(
                                   kPcmuPayload, kPcmuFormat, absl::nullopt));
    audio_channel_->SetReceiveCodecs({{kPcmuPayload, kPcmuFormat}});
    audio_channel_->StartSend();
    audio_channel_->StartPlay();
  }

  void TearDown() override {
    audio_channel_->StopSend();
    audio_channel_->StopPlay();
    audio_channel_ = nullptr;
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

  SimulatedClock fake_clock_;
  SineWaveGenerator wave_generator_;
  NiceMock<MockTransport> transport_;
  std::unique_ptr<TaskQueueFactory> task_queue_factory_;
  rtc::scoped_refptr<AudioMixer> audio_mixer_;
  rtc::scoped_refptr<AudioDecoderFactory> decoder_factory_;
  rtc::scoped_refptr<AudioEncoderFactory> encoder_factory_;
  std::unique_ptr<ProcessThread> process_thread_;
  rtc::scoped_refptr<AudioChannel> audio_channel_;
};

// Validate RTP packet generation by feeding audio frames with sine wave.
// Resulted RTP packet is looped back into AudioChannel and gets decoded into
// audio frame to see if it has some signal to indicate its validity.
TEST_F(AudioChannelTest, PlayRtpByLocalLoop) {
  rtc::Event event;
  auto loop_rtp = [&](const uint8_t* packet, size_t length, Unused) {
    audio_channel_->ReceivedRTPPacket(
        rtc::ArrayView<const uint8_t>(packet, length));
    event.Set();
    return true;
  };
  EXPECT_CALL(transport_, SendRtp).WillOnce(Invoke(loop_rtp));

  auto audio_sender = audio_channel_->GetAudioSender();
  audio_sender->SendAudioData(GetAudioFrame(0));
  audio_sender->SendAudioData(GetAudioFrame(1));

  event.Wait(/*ms=*/1000);

  AudioFrame empty_frame, audio_frame;
  empty_frame.Mute();
  empty_frame.mutable_data();  // This will zero out the data.
  audio_frame.CopyFrom(empty_frame);
  audio_mixer_->Mix(/*number_of_channels*/ 1, &audio_frame);

  // We expect now audio frame to pick up something.
  EXPECT_NE(memcmp(empty_frame.data(), audio_frame.data(),
                   AudioFrame::kMaxDataSizeBytes),
            0);
}

// Validate assigned local SSRC is resulted in RTP packet.
TEST_F(AudioChannelTest, VerifyLocalSsrcAsAssigned) {
  RtpPacketReceived rtp;
  rtc::Event event;
  auto loop_rtp = [&](const uint8_t* packet, size_t length, Unused) {
    rtp.Parse(packet, length);
    event.Set();
    return true;
  };
  EXPECT_CALL(transport_, SendRtp).WillOnce(Invoke(loop_rtp));

  auto audio_sender = audio_channel_->GetAudioSender();
  audio_sender->SendAudioData(GetAudioFrame(0));
  audio_sender->SendAudioData(GetAudioFrame(1));

  event.Wait(/*ms=*/1000);

  EXPECT_EQ(rtp.Ssrc(), kLocalSsrc);
}

}  // namespace
}  // namespace webrtc
