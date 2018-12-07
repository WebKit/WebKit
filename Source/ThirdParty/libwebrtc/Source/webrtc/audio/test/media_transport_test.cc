/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio_codecs/audio_decoder_factory_template.h"
#include "api/audio_codecs/audio_encoder_factory_template.h"
#include "api/audio_codecs/opus/audio_decoder_opus.h"
#include "api/audio_codecs/opus/audio_encoder_opus.h"
#include "api/test/loopback_media_transport.h"
#include "api/test/mock_audio_mixer.h"
#include "audio/audio_receive_stream.h"
#include "audio/audio_send_stream.h"
#include "call/test/mock_bitrate_allocator.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "modules/audio_device/include/test_audio_device.h"
#include "modules/audio_mixer/audio_mixer_impl.h"
#include "modules/audio_processing/include/mock_audio_processing.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/timeutils.h"
#include "test/gtest.h"
#include "test/mock_transport.h"

namespace webrtc {
namespace test {

namespace {
constexpr int kPayloadTypeOpus = 17;
constexpr int kSamplingFrequency = 48000;
constexpr int kNumChannels = 2;
constexpr int kWantedSamples = 3000;
constexpr int kTestTimeoutMs = 2 * rtc::kNumMillisecsPerSec;

class TestRenderer : public TestAudioDeviceModule::Renderer {
 public:
  TestRenderer(int sampling_frequency, int num_channels, size_t wanted_samples)
      : sampling_frequency_(sampling_frequency),
        num_channels_(num_channels),
        wanted_samples_(wanted_samples) {}
  ~TestRenderer() override = default;

  int SamplingFrequency() const override { return sampling_frequency_; }
  int NumChannels() const override { return num_channels_; }

  bool Render(rtc::ArrayView<const int16_t> data) override {
    if (data.size() >= wanted_samples_) {
      return false;
    }
    wanted_samples_ -= data.size();
    return true;
  }

 private:
  const int sampling_frequency_;
  const int num_channels_;
  size_t wanted_samples_;
};

}  // namespace

TEST(AudioWithMediaTransport, DeliversAudio) {
  std::unique_ptr<rtc::Thread> transport_thread = rtc::Thread::Create();
  transport_thread->Start();
  MediaTransportPair transport_pair(transport_thread.get());
  MockTransport rtcp_send_transport;
  MockTransport send_transport;
  std::unique_ptr<RtcEventLog> null_event_log = RtcEventLog::CreateNull();
  MockBitrateAllocator bitrate_allocator;

  rtc::scoped_refptr<TestAudioDeviceModule> audio_device =
      TestAudioDeviceModule::CreateTestAudioDeviceModule(
          TestAudioDeviceModule::CreatePulsedNoiseCapturer(
              /* max_amplitude= */ 10000, kSamplingFrequency, kNumChannels),
          absl::make_unique<TestRenderer>(kSamplingFrequency, kNumChannels,
                                          kWantedSamples));

  AudioState::Config audio_config;
  audio_config.audio_mixer = AudioMixerImpl::Create();
  // TODO(nisse): Is a mock AudioProcessing enough?
  audio_config.audio_processing =
      new rtc::RefCountedObject<MockAudioProcessing>();
  audio_config.audio_device_module = audio_device;
  rtc::scoped_refptr<AudioState> audio_state = AudioState::Create(audio_config);

  // TODO(nisse): Use some lossless codec?
  const SdpAudioFormat audio_format("opus", kSamplingFrequency, kNumChannels);

  // Setup receive stream;
  webrtc::AudioReceiveStream::Config receive_config;
  // TODO(nisse): Update AudioReceiveStream to not require rtcp_send_transport
  // when a MediaTransport is provided.
  receive_config.rtcp_send_transport = &rtcp_send_transport;
  receive_config.media_transport = transport_pair.first();
  receive_config.decoder_map.emplace(kPayloadTypeOpus, audio_format);
  receive_config.decoder_factory =
      CreateAudioDecoderFactory<AudioDecoderOpus>();

  std::unique_ptr<ProcessThread> receive_process_thread =
      ProcessThread::Create("audio recv thread");

  webrtc::internal::AudioReceiveStream receive_stream(
      /*rtp_stream_receiver_controller=*/nullptr,
      /*packet_router=*/nullptr, receive_process_thread.get(), receive_config,
      audio_state, null_event_log.get());

  // TODO(nisse): Update AudioSendStream to not require send_transport when a
  // MediaTransport is provided.
  AudioSendStream::Config send_config(&send_transport, transport_pair.second());
  send_config.send_codec_spec =
      AudioSendStream::Config::SendCodecSpec(kPayloadTypeOpus, audio_format);
  send_config.encoder_factory = CreateAudioEncoderFactory<AudioEncoderOpus>();
  rtc::TaskQueue send_tq("audio send queue");
  std::unique_ptr<ProcessThread> send_process_thread =
      ProcessThread::Create("audio send thread");
  webrtc::internal::AudioSendStream send_stream(
      send_config, audio_state, &send_tq, send_process_thread.get(),
      /*transport=*/nullptr, &bitrate_allocator, null_event_log.get(),
      /*rtcp_rtt_stats=*/nullptr, absl::optional<RtpState>());

  audio_device->Init();  // Starts thread.
  audio_device->RegisterAudioCallback(audio_state->audio_transport());

  receive_stream.Start();
  send_stream.Start();
  audio_device->StartPlayout();
  audio_device->StartRecording();

  EXPECT_TRUE(audio_device->WaitForPlayoutEnd(kTestTimeoutMs));

  audio_device->StopRecording();
  audio_device->StopPlayout();
  receive_stream.Stop();
  send_stream.Stop();
}

}  // namespace test
}  // namespace webrtc
