/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_TEST_CALL_TEST_H_
#define WEBRTC_TEST_CALL_TEST_H_

#include <memory>
#include <vector>

#include "webrtc/call/call.h"
#include "webrtc/logging/rtc_event_log/rtc_event_log.h"
#include "webrtc/test/encoder_settings.h"
#include "webrtc/test/fake_audio_device.h"
#include "webrtc/test/fake_decoder.h"
#include "webrtc/test/fake_encoder.h"
#include "webrtc/test/fake_videorenderer.h"
#include "webrtc/test/frame_generator_capturer.h"
#include "webrtc/test/rtp_rtcp_observer.h"

namespace webrtc {

class VoEBase;

namespace test {

class BaseTest;

class CallTest : public ::testing::Test {
 public:
  CallTest();
  virtual ~CallTest();

  static const size_t kNumSsrcs = 3;
  static const int kDefaultWidth = 320;
  static const int kDefaultHeight = 180;
  static const int kDefaultFramerate = 30;
  static const int kDefaultTimeoutMs;
  static const int kLongTimeoutMs;
  static const uint8_t kVideoSendPayloadType;
  static const uint8_t kSendRtxPayloadType;
  static const uint8_t kFakeVideoSendPayloadType;
  static const uint8_t kRedPayloadType;
  static const uint8_t kRtxRedPayloadType;
  static const uint8_t kUlpfecPayloadType;
  static const uint8_t kFlexfecPayloadType;
  static const uint8_t kAudioSendPayloadType;
  static const uint32_t kSendRtxSsrcs[kNumSsrcs];
  static const uint32_t kVideoSendSsrcs[kNumSsrcs];
  static const uint32_t kAudioSendSsrc;
  static const uint32_t kFlexfecSendSsrc;
  static const uint32_t kReceiverLocalVideoSsrc;
  static const uint32_t kReceiverLocalAudioSsrc;
  static const int kNackRtpHistoryMs;
  static const std::map<uint8_t, MediaType> payload_type_map_;

 protected:
  // RunBaseTest overwrites the audio_state and the voice_engine of the send and
  // receive Call configs to simplify test code and avoid having old VoiceEngine
  // APIs in the tests.
  void RunBaseTest(BaseTest* test);

  void CreateCalls(const Call::Config& sender_config,
                   const Call::Config& receiver_config);
  void CreateSenderCall(const Call::Config& config);
  void CreateReceiverCall(const Call::Config& config);
  void DestroyCalls();

  void CreateSendConfig(size_t num_video_streams,
                        size_t num_audio_streams,
                        size_t num_flexfec_streams,
                        Transport* send_transport);

  void CreateMatchingReceiveConfigs(Transport* rtcp_send_transport);

  void CreateFrameGeneratorCapturerWithDrift(Clock* drift_clock,
                                             float speed,
                                             int framerate,
                                             int width,
                                             int height);
  void CreateFrameGeneratorCapturer(int framerate, int width, int height);
  void CreateFakeAudioDevices(
      std::unique_ptr<FakeAudioDevice::Capturer> capturer,
      std::unique_ptr<FakeAudioDevice::Renderer> renderer);

  void CreateVideoStreams();
  void CreateAudioStreams();
  void CreateFlexfecStreams();
  void Start();
  void Stop();
  void DestroyStreams();
  void SetFakeVideoCaptureRotation(VideoRotation rotation);

  Clock* const clock_;

  std::unique_ptr<webrtc::RtcEventLog> event_log_;
  std::unique_ptr<Call> sender_call_;
  std::unique_ptr<PacketTransport> send_transport_;
  VideoSendStream::Config video_send_config_;
  VideoEncoderConfig video_encoder_config_;
  VideoSendStream* video_send_stream_;
  AudioSendStream::Config audio_send_config_;
  AudioSendStream* audio_send_stream_;

  std::unique_ptr<Call> receiver_call_;
  std::unique_ptr<PacketTransport> receive_transport_;
  std::vector<VideoReceiveStream::Config> video_receive_configs_;
  std::vector<VideoReceiveStream*> video_receive_streams_;
  std::vector<AudioReceiveStream::Config> audio_receive_configs_;
  std::vector<AudioReceiveStream*> audio_receive_streams_;
  std::vector<FlexfecReceiveStream::Config> flexfec_receive_configs_;
  std::vector<FlexfecReceiveStream*> flexfec_receive_streams_;

  std::unique_ptr<test::FrameGeneratorCapturer> frame_generator_capturer_;
  test::FakeEncoder fake_encoder_;
  std::vector<std::unique_ptr<VideoDecoder>> allocated_decoders_;
  size_t num_video_streams_;
  size_t num_audio_streams_;
  size_t num_flexfec_streams_;
  rtc::scoped_refptr<AudioDecoderFactory> decoder_factory_;
  rtc::scoped_refptr<AudioEncoderFactory> encoder_factory_;
  test::FakeVideoRenderer fake_renderer_;

 private:
  // TODO(holmer): Remove once VoiceEngine is fully refactored to the new API.
  // These methods are used to set up legacy voice engines and channels which is
  // necessary while voice engine is being refactored to the new stream API.
  struct VoiceEngineState {
    VoiceEngineState()
        : voice_engine(nullptr),
          base(nullptr),
          channel_id(-1) {}

    VoiceEngine* voice_engine;
    VoEBase* base;
    int channel_id;
  };

  void CreateVoiceEngines();
  void DestroyVoiceEngines();

  VoiceEngineState voe_send_;
  VoiceEngineState voe_recv_;

  // The audio devices must outlive the voice engines.
  std::unique_ptr<test::FakeAudioDevice> fake_send_audio_device_;
  std::unique_ptr<test::FakeAudioDevice> fake_recv_audio_device_;
};

class BaseTest : public RtpRtcpObserver {
 public:
  BaseTest();
  explicit BaseTest(unsigned int timeout_ms);
  virtual ~BaseTest();

  virtual void PerformTest() = 0;
  virtual bool ShouldCreateReceivers() const = 0;

  virtual size_t GetNumVideoStreams() const;
  virtual size_t GetNumAudioStreams() const;
  virtual size_t GetNumFlexfecStreams() const;

  virtual std::unique_ptr<FakeAudioDevice::Capturer> CreateCapturer();
  virtual std::unique_ptr<FakeAudioDevice::Renderer> CreateRenderer();
  virtual void OnFakeAudioDevicesCreated(FakeAudioDevice* send_audio_device,
                                         FakeAudioDevice* recv_audio_device);

  virtual Call::Config GetSenderCallConfig();
  virtual Call::Config GetReceiverCallConfig();
  virtual void OnCallsCreated(Call* sender_call, Call* receiver_call);

  virtual test::PacketTransport* CreateSendTransport(Call* sender_call);
  virtual test::PacketTransport* CreateReceiveTransport();

  virtual void ModifyVideoConfigs(
      VideoSendStream::Config* send_config,
      std::vector<VideoReceiveStream::Config>* receive_configs,
      VideoEncoderConfig* encoder_config);
  virtual void ModifyVideoCaptureStartResolution(int* width,
                                                 int* heigt,
                                                 int* frame_rate);
  virtual void OnVideoStreamsCreated(
      VideoSendStream* send_stream,
      const std::vector<VideoReceiveStream*>& receive_streams);

  virtual void ModifyAudioConfigs(
      AudioSendStream::Config* send_config,
      std::vector<AudioReceiveStream::Config>* receive_configs);
  virtual void OnAudioStreamsCreated(
      AudioSendStream* send_stream,
      const std::vector<AudioReceiveStream*>& receive_streams);

  virtual void ModifyFlexfecConfigs(
      std::vector<FlexfecReceiveStream::Config>* receive_configs);
  virtual void OnFlexfecStreamsCreated(
      const std::vector<FlexfecReceiveStream*>& receive_streams);

  virtual void OnFrameGeneratorCapturerCreated(
      FrameGeneratorCapturer* frame_generator_capturer);

  virtual void OnTestFinished();

  std::unique_ptr<webrtc::RtcEventLog> event_log_;
};

class SendTest : public BaseTest {
 public:
  explicit SendTest(unsigned int timeout_ms);

  bool ShouldCreateReceivers() const override;
};

class EndToEndTest : public BaseTest {
 public:
  EndToEndTest();
  explicit EndToEndTest(unsigned int timeout_ms);

  bool ShouldCreateReceivers() const override;
};

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_TEST_CALL_TEST_H_
