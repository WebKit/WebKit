/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/call_test.h"

#include <algorithm>
#include <memory>

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/task_queue/task_queue_base.h"
#include "api/test/create_frame_generator.h"
#include "api/video/builtin_video_bitrate_allocator_factory.h"
#include "api/video_codecs/video_encoder_config.h"
#include "call/fake_network_pipe.h"
#include "call/simulated_network.h"
#include "modules/audio_mixer/audio_mixer_impl.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/task_queue_for_test.h"
#include "test/fake_encoder.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {
namespace test {

CallTest::CallTest()
    : clock_(Clock::GetRealTimeClock()),
      task_queue_factory_(CreateDefaultTaskQueueFactory()),
      send_event_log_(std::make_unique<RtcEventLogNull>()),
      recv_event_log_(std::make_unique<RtcEventLogNull>()),
      audio_send_config_(/*send_transport=*/nullptr),
      audio_send_stream_(nullptr),
      frame_generator_capturer_(nullptr),
      fake_encoder_factory_([this]() {
        std::unique_ptr<FakeEncoder> fake_encoder;
        if (video_encoder_configs_[0].codec_type == kVideoCodecVP8) {
          fake_encoder = std::make_unique<FakeVp8Encoder>(clock_);
        } else {
          fake_encoder = std::make_unique<FakeEncoder>(clock_);
        }
        fake_encoder->SetMaxBitrate(fake_encoder_max_bitrate_);
        return fake_encoder;
      }),
      fake_decoder_factory_([]() { return std::make_unique<FakeDecoder>(); }),
      bitrate_allocator_factory_(CreateBuiltinVideoBitrateAllocatorFactory()),
      num_video_streams_(1),
      num_audio_streams_(0),
      num_flexfec_streams_(0),
      audio_decoder_factory_(CreateBuiltinAudioDecoderFactory()),
      audio_encoder_factory_(CreateBuiltinAudioEncoderFactory()),
      task_queue_(task_queue_factory_->CreateTaskQueue(
          "CallTestTaskQueue",
          TaskQueueFactory::Priority::NORMAL)) {}

CallTest::~CallTest() = default;

void CallTest::RegisterRtpExtension(const RtpExtension& extension) {
  for (const RtpExtension& registered_extension : rtp_extensions_) {
    if (registered_extension.id == extension.id) {
      ASSERT_EQ(registered_extension.uri, extension.uri)
          << "Different URIs associated with ID " << extension.id << ".";
      ASSERT_EQ(registered_extension.encrypt, extension.encrypt)
          << "Encryption mismatch associated with ID " << extension.id << ".";
      return;
    } else {  // Different IDs.
      // Different IDs referring to the same extension probably indicate
      // a mistake in the test.
      ASSERT_FALSE(registered_extension.uri == extension.uri &&
                   registered_extension.encrypt == extension.encrypt)
          << "URI " << extension.uri
          << (extension.encrypt ? " with " : " without ")
          << "encryption already registered with a different "
             "ID ("
          << extension.id << " vs. " << registered_extension.id << ").";
    }
  }
  rtp_extensions_.push_back(extension);
}

void CallTest::RunBaseTest(BaseTest* test) {
  SendTask(RTC_FROM_HERE, task_queue(), [this, test]() {
    num_video_streams_ = test->GetNumVideoStreams();
    num_audio_streams_ = test->GetNumAudioStreams();
    num_flexfec_streams_ = test->GetNumFlexfecStreams();
    RTC_DCHECK(num_video_streams_ > 0 || num_audio_streams_ > 0);
    Call::Config send_config(send_event_log_.get());
    test->ModifySenderBitrateConfig(&send_config.bitrate_config);
    if (num_audio_streams_ > 0) {
      CreateFakeAudioDevices(test->CreateCapturer(), test->CreateRenderer());
      test->OnFakeAudioDevicesCreated(fake_send_audio_device_.get(),
                                      fake_recv_audio_device_.get());
      apm_send_ = AudioProcessingBuilder().Create();
      apm_recv_ = AudioProcessingBuilder().Create();
      EXPECT_EQ(0, fake_send_audio_device_->Init());
      EXPECT_EQ(0, fake_recv_audio_device_->Init());
      AudioState::Config audio_state_config;
      audio_state_config.audio_mixer = AudioMixerImpl::Create();
      audio_state_config.audio_processing = apm_send_;
      audio_state_config.audio_device_module = fake_send_audio_device_;
      send_config.audio_state = AudioState::Create(audio_state_config);
      fake_send_audio_device_->RegisterAudioCallback(
          send_config.audio_state->audio_transport());
    }
    CreateSenderCall(send_config);
    if (test->ShouldCreateReceivers()) {
      Call::Config recv_config(recv_event_log_.get());
      test->ModifyReceiverBitrateConfig(&recv_config.bitrate_config);
      if (num_audio_streams_ > 0) {
        AudioState::Config audio_state_config;
        audio_state_config.audio_mixer = AudioMixerImpl::Create();
        audio_state_config.audio_processing = apm_recv_;
        audio_state_config.audio_device_module = fake_recv_audio_device_;
        recv_config.audio_state = AudioState::Create(audio_state_config);
        fake_recv_audio_device_->RegisterAudioCallback(
            recv_config.audio_state->audio_transport());
      }
      CreateReceiverCall(recv_config);
    }
    test->OnCallsCreated(sender_call_.get(), receiver_call_.get());
    receive_transport_ = test->CreateReceiveTransport(task_queue());
    send_transport_ =
        test->CreateSendTransport(task_queue(), sender_call_.get());

    if (test->ShouldCreateReceivers()) {
      send_transport_->SetReceiver(receiver_call_->Receiver());
      receive_transport_->SetReceiver(sender_call_->Receiver());
      if (num_video_streams_ > 0)
        receiver_call_->SignalChannelNetworkState(MediaType::VIDEO, kNetworkUp);
      if (num_audio_streams_ > 0)
        receiver_call_->SignalChannelNetworkState(MediaType::AUDIO, kNetworkUp);
    } else {
      // Sender-only call delivers to itself.
      send_transport_->SetReceiver(sender_call_->Receiver());
      receive_transport_->SetReceiver(nullptr);
    }

    CreateSendConfig(num_video_streams_, num_audio_streams_,
                     num_flexfec_streams_, send_transport_.get());
    if (test->ShouldCreateReceivers()) {
      CreateMatchingReceiveConfigs(receive_transport_.get());
    }
    if (num_video_streams_ > 0) {
      test->ModifyVideoConfigs(GetVideoSendConfig(), &video_receive_configs_,
                               GetVideoEncoderConfig());
    }
    if (num_audio_streams_ > 0) {
      test->ModifyAudioConfigs(&audio_send_config_, &audio_receive_configs_);
    }
    if (num_flexfec_streams_ > 0) {
      test->ModifyFlexfecConfigs(&flexfec_receive_configs_);
    }

    if (num_flexfec_streams_ > 0) {
      CreateFlexfecStreams();
      test->OnFlexfecStreamsCreated(flexfec_receive_streams_);
    }
    if (num_video_streams_ > 0) {
      CreateVideoStreams();
      test->OnVideoStreamsCreated(GetVideoSendStream(), video_receive_streams_);
    }
    if (num_audio_streams_ > 0) {
      CreateAudioStreams();
      test->OnAudioStreamsCreated(audio_send_stream_, audio_receive_streams_);
    }

    if (num_video_streams_ > 0) {
      int width = kDefaultWidth;
      int height = kDefaultHeight;
      int frame_rate = kDefaultFramerate;
      test->ModifyVideoCaptureStartResolution(&width, &height, &frame_rate);
      test->ModifyVideoDegradationPreference(&degradation_preference_);
      CreateFrameGeneratorCapturer(frame_rate, width, height);
      test->OnFrameGeneratorCapturerCreated(frame_generator_capturer_);
    }

    Start();
  });

  test->PerformTest();

  SendTask(RTC_FROM_HERE, task_queue(), [this, test]() {
    Stop();
    test->OnStreamsStopped();
    DestroyStreams();
    send_transport_.reset();
    receive_transport_.reset();

    frame_generator_capturer_ = nullptr;
    DestroyCalls();

    fake_send_audio_device_ = nullptr;
    fake_recv_audio_device_ = nullptr;
  });
}

void CallTest::CreateCalls() {
  CreateCalls(Call::Config(send_event_log_.get()),
              Call::Config(recv_event_log_.get()));
}

void CallTest::CreateCalls(const Call::Config& sender_config,
                           const Call::Config& receiver_config) {
  CreateSenderCall(sender_config);
  CreateReceiverCall(receiver_config);
}

void CallTest::CreateSenderCall() {
  CreateSenderCall(Call::Config(send_event_log_.get()));
}

void CallTest::CreateSenderCall(const Call::Config& config) {
  auto sender_config = config;
  sender_config.task_queue_factory = task_queue_factory_.get();
  sender_config.network_state_predictor_factory =
      network_state_predictor_factory_.get();
  sender_config.network_controller_factory = network_controller_factory_.get();
  sender_config.trials = &field_trials_;
  sender_call_.reset(Call::Create(sender_config));
}

void CallTest::CreateReceiverCall(const Call::Config& config) {
  auto receiver_config = config;
  receiver_config.task_queue_factory = task_queue_factory_.get();
  receiver_config.trials = &field_trials_;
  receiver_call_.reset(Call::Create(receiver_config));
}

void CallTest::DestroyCalls() {
  sender_call_.reset();
  receiver_call_.reset();
}

void CallTest::CreateVideoSendConfig(VideoSendStream::Config* video_config,
                                     size_t num_video_streams,
                                     size_t num_used_ssrcs,
                                     Transport* send_transport) {
  RTC_DCHECK_LE(num_video_streams + num_used_ssrcs, kNumSsrcs);
  *video_config = VideoSendStream::Config(send_transport);
  video_config->encoder_settings.encoder_factory = &fake_encoder_factory_;
  video_config->encoder_settings.bitrate_allocator_factory =
      bitrate_allocator_factory_.get();
  video_config->rtp.payload_name = "FAKE";
  video_config->rtp.payload_type = kFakeVideoSendPayloadType;
  video_config->rtp.extmap_allow_mixed = true;
  AddRtpExtensionByUri(RtpExtension::kTransportSequenceNumberUri,
                       &video_config->rtp.extensions);
  AddRtpExtensionByUri(RtpExtension::kVideoContentTypeUri,
                       &video_config->rtp.extensions);
  AddRtpExtensionByUri(RtpExtension::kGenericFrameDescriptorUri00,
                       &video_config->rtp.extensions);
  AddRtpExtensionByUri(RtpExtension::kDependencyDescriptorUri,
                       &video_config->rtp.extensions);
  if (video_encoder_configs_.empty()) {
    video_encoder_configs_.emplace_back();
    FillEncoderConfiguration(kVideoCodecGeneric, num_video_streams,
                             &video_encoder_configs_.back());
  }
  for (size_t i = 0; i < num_video_streams; ++i)
    video_config->rtp.ssrcs.push_back(kVideoSendSsrcs[num_used_ssrcs + i]);
  AddRtpExtensionByUri(RtpExtension::kVideoRotationUri,
                       &video_config->rtp.extensions);
  AddRtpExtensionByUri(RtpExtension::kColorSpaceUri,
                       &video_config->rtp.extensions);
}

void CallTest::CreateAudioAndFecSendConfigs(size_t num_audio_streams,
                                            size_t num_flexfec_streams,
                                            Transport* send_transport) {
  RTC_DCHECK_LE(num_audio_streams, 1);
  RTC_DCHECK_LE(num_flexfec_streams, 1);
  if (num_audio_streams > 0) {
    AudioSendStream::Config audio_send_config(send_transport);
    audio_send_config.rtp.ssrc = kAudioSendSsrc;
    audio_send_config.send_codec_spec = AudioSendStream::Config::SendCodecSpec(
        kAudioSendPayloadType, {"opus", 48000, 2, {{"stereo", "1"}}});
    audio_send_config.encoder_factory = audio_encoder_factory_;
    SetAudioConfig(audio_send_config);
  }

  // TODO(brandtr): Update this when we support multistream protection.
  if (num_flexfec_streams > 0) {
    SetSendFecConfig({kVideoSendSsrcs[0]});
  }
}

void CallTest::SetAudioConfig(const AudioSendStream::Config& config) {
  audio_send_config_ = config;
}

void CallTest::SetSendFecConfig(std::vector<uint32_t> video_send_ssrcs) {
  GetVideoSendConfig()->rtp.flexfec.payload_type = kFlexfecPayloadType;
  GetVideoSendConfig()->rtp.flexfec.ssrc = kFlexfecSendSsrc;
  GetVideoSendConfig()->rtp.flexfec.protected_media_ssrcs = video_send_ssrcs;
}

void CallTest::SetSendUlpFecConfig(VideoSendStream::Config* send_config) {
  send_config->rtp.ulpfec.red_payload_type = kRedPayloadType;
  send_config->rtp.ulpfec.ulpfec_payload_type = kUlpfecPayloadType;
  send_config->rtp.ulpfec.red_rtx_payload_type = kRtxRedPayloadType;
}

void CallTest::SetReceiveUlpFecConfig(
    VideoReceiveStream::Config* receive_config) {
  receive_config->rtp.red_payload_type = kRedPayloadType;
  receive_config->rtp.ulpfec_payload_type = kUlpfecPayloadType;
  receive_config->rtp.rtx_associated_payload_types[kRtxRedPayloadType] =
      kRedPayloadType;
}

void CallTest::CreateSendConfig(size_t num_video_streams,
                                size_t num_audio_streams,
                                size_t num_flexfec_streams,
                                Transport* send_transport) {
  if (num_video_streams > 0) {
    video_send_configs_.clear();
    video_send_configs_.emplace_back(nullptr);
    CreateVideoSendConfig(&video_send_configs_.back(), num_video_streams, 0,
                          send_transport);
  }
  CreateAudioAndFecSendConfigs(num_audio_streams, num_flexfec_streams,
                               send_transport);
}

void CallTest::CreateMatchingVideoReceiveConfigs(
    const VideoSendStream::Config& video_send_config,
    Transport* rtcp_send_transport) {
  CreateMatchingVideoReceiveConfigs(video_send_config, rtcp_send_transport,
                                    true, &fake_decoder_factory_, absl::nullopt,
                                    false, 0);
}

void CallTest::CreateMatchingVideoReceiveConfigs(
    const VideoSendStream::Config& video_send_config,
    Transport* rtcp_send_transport,
    bool send_side_bwe,
    VideoDecoderFactory* decoder_factory,
    absl::optional<size_t> decode_sub_stream,
    bool receiver_reference_time_report,
    int rtp_history_ms) {
  AddMatchingVideoReceiveConfigs(
      &video_receive_configs_, video_send_config, rtcp_send_transport,
      send_side_bwe, decoder_factory, decode_sub_stream,
      receiver_reference_time_report, rtp_history_ms);
}

void CallTest::AddMatchingVideoReceiveConfigs(
    std::vector<VideoReceiveStream::Config>* receive_configs,
    const VideoSendStream::Config& video_send_config,
    Transport* rtcp_send_transport,
    bool send_side_bwe,
    VideoDecoderFactory* decoder_factory,
    absl::optional<size_t> decode_sub_stream,
    bool receiver_reference_time_report,
    int rtp_history_ms) {
  RTC_DCHECK(!video_send_config.rtp.ssrcs.empty());
  VideoReceiveStream::Config default_config(rtcp_send_transport);
  default_config.rtp.transport_cc = send_side_bwe;
  default_config.rtp.local_ssrc = kReceiverLocalVideoSsrc;
  for (const RtpExtension& extension : video_send_config.rtp.extensions)
    default_config.rtp.extensions.push_back(extension);
  default_config.rtp.nack.rtp_history_ms = rtp_history_ms;
  // Enable RTT calculation so NTP time estimator will work.
  default_config.rtp.rtcp_xr.receiver_reference_time_report =
      receiver_reference_time_report;
  default_config.renderer = &fake_renderer_;

  for (size_t i = 0; i < video_send_config.rtp.ssrcs.size(); ++i) {
    VideoReceiveStream::Config video_recv_config(default_config.Copy());
    video_recv_config.decoders.clear();
    if (!video_send_config.rtp.rtx.ssrcs.empty()) {
      video_recv_config.rtp.rtx_ssrc = video_send_config.rtp.rtx.ssrcs[i];
      video_recv_config.rtp.rtx_associated_payload_types[kSendRtxPayloadType] =
          video_send_config.rtp.payload_type;
    }
    video_recv_config.rtp.remote_ssrc = video_send_config.rtp.ssrcs[i];
    VideoReceiveStream::Decoder decoder;

    decoder.payload_type = video_send_config.rtp.payload_type;
    decoder.video_format = SdpVideoFormat(video_send_config.rtp.payload_name);
    // Force fake decoders on non-selected simulcast streams.
    if (!decode_sub_stream || i == *decode_sub_stream) {
      video_recv_config.decoder_factory = decoder_factory;
    } else {
      video_recv_config.decoder_factory = &fake_decoder_factory_;
    }
    video_recv_config.decoders.push_back(decoder);
    receive_configs->emplace_back(std::move(video_recv_config));
  }
}

void CallTest::CreateMatchingAudioAndFecConfigs(
    Transport* rtcp_send_transport) {
  RTC_DCHECK_GE(1, num_audio_streams_);
  if (num_audio_streams_ == 1) {
    CreateMatchingAudioConfigs(rtcp_send_transport, "");
  }

  // TODO(brandtr): Update this when we support multistream protection.
  RTC_DCHECK(num_flexfec_streams_ <= 1);
  if (num_flexfec_streams_ == 1) {
    CreateMatchingFecConfig(rtcp_send_transport, *GetVideoSendConfig());
    for (const RtpExtension& extension : GetVideoSendConfig()->rtp.extensions)
      GetFlexFecConfig()->rtp_header_extensions.push_back(extension);
  }
}

void CallTest::CreateMatchingAudioConfigs(Transport* transport,
                                          std::string sync_group) {
  audio_receive_configs_.push_back(CreateMatchingAudioConfig(
      audio_send_config_, audio_decoder_factory_, transport, sync_group));
}

AudioReceiveStream::Config CallTest::CreateMatchingAudioConfig(
    const AudioSendStream::Config& send_config,
    rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory,
    Transport* transport,
    std::string sync_group) {
  AudioReceiveStream::Config audio_config;
  audio_config.rtp.local_ssrc = kReceiverLocalAudioSsrc;
  audio_config.rtcp_send_transport = transport;
  audio_config.rtp.remote_ssrc = send_config.rtp.ssrc;
  audio_config.rtp.transport_cc =
      send_config.send_codec_spec
          ? send_config.send_codec_spec->transport_cc_enabled
          : false;
  audio_config.rtp.extensions = send_config.rtp.extensions;
  audio_config.decoder_factory = audio_decoder_factory;
  audio_config.decoder_map = {{kAudioSendPayloadType, {"opus", 48000, 2}}};
  audio_config.sync_group = sync_group;
  return audio_config;
}

void CallTest::CreateMatchingFecConfig(
    Transport* transport,
    const VideoSendStream::Config& send_config) {
  FlexfecReceiveStream::Config config(transport);
  config.payload_type = send_config.rtp.flexfec.payload_type;
  config.remote_ssrc = send_config.rtp.flexfec.ssrc;
  config.protected_media_ssrcs = send_config.rtp.flexfec.protected_media_ssrcs;
  config.local_ssrc = kReceiverLocalVideoSsrc;
  if (!video_receive_configs_.empty()) {
    video_receive_configs_[0].rtp.protected_by_flexfec = true;
    video_receive_configs_[0].rtp.packet_sink_ = this;
  }
  flexfec_receive_configs_.push_back(config);
}

void CallTest::CreateMatchingReceiveConfigs(Transport* rtcp_send_transport) {
  video_receive_configs_.clear();
  for (VideoSendStream::Config& video_send_config : video_send_configs_) {
    CreateMatchingVideoReceiveConfigs(video_send_config, rtcp_send_transport);
  }
  CreateMatchingAudioAndFecConfigs(rtcp_send_transport);
}

void CallTest::CreateFrameGeneratorCapturerWithDrift(Clock* clock,
                                                     float speed,
                                                     int framerate,
                                                     int width,
                                                     int height) {
  video_sources_.clear();
  auto frame_generator_capturer =
      std::make_unique<test::FrameGeneratorCapturer>(
          clock,
          test::CreateSquareFrameGenerator(width, height, absl::nullopt,
                                           absl::nullopt),
          framerate * speed, *task_queue_factory_);
  frame_generator_capturer_ = frame_generator_capturer.get();
  frame_generator_capturer->Init();
  video_sources_.push_back(std::move(frame_generator_capturer));
  ConnectVideoSourcesToStreams();
}

void CallTest::CreateFrameGeneratorCapturer(int framerate,
                                            int width,
                                            int height) {
  video_sources_.clear();
  auto frame_generator_capturer =
      std::make_unique<test::FrameGeneratorCapturer>(
          clock_,
          test::CreateSquareFrameGenerator(width, height, absl::nullopt,
                                           absl::nullopt),
          framerate, *task_queue_factory_);
  frame_generator_capturer_ = frame_generator_capturer.get();
  frame_generator_capturer->Init();
  video_sources_.push_back(std::move(frame_generator_capturer));
  ConnectVideoSourcesToStreams();
}

void CallTest::CreateFakeAudioDevices(
    std::unique_ptr<TestAudioDeviceModule::Capturer> capturer,
    std::unique_ptr<TestAudioDeviceModule::Renderer> renderer) {
  fake_send_audio_device_ = TestAudioDeviceModule::Create(
      task_queue_factory_.get(), std::move(capturer), nullptr, 1.f);
  fake_recv_audio_device_ = TestAudioDeviceModule::Create(
      task_queue_factory_.get(), nullptr, std::move(renderer), 1.f);
}

void CallTest::CreateVideoStreams() {
  RTC_DCHECK(video_receive_streams_.empty());
  CreateVideoSendStreams();
  for (size_t i = 0; i < video_receive_configs_.size(); ++i) {
    video_receive_streams_.push_back(receiver_call_->CreateVideoReceiveStream(
        video_receive_configs_[i].Copy()));
  }
}

void CallTest::CreateVideoSendStreams() {
  RTC_DCHECK(video_send_streams_.empty());

  // We currently only support testing external fec controllers with a single
  // VideoSendStream.
  if (fec_controller_factory_.get()) {
    RTC_DCHECK_LE(video_send_configs_.size(), 1);
  }

  // TODO(http://crbug/818127):
  // Remove this workaround when ALR is not screenshare-specific.
  std::list<size_t> streams_creation_order;
  for (size_t i = 0; i < video_send_configs_.size(); ++i) {
    // If dual streams are created, add the screenshare stream last.
    if (video_encoder_configs_[i].content_type ==
        VideoEncoderConfig::ContentType::kScreen) {
      streams_creation_order.push_back(i);
    } else {
      streams_creation_order.push_front(i);
    }
  }

  video_send_streams_.resize(video_send_configs_.size(), nullptr);

  for (size_t i : streams_creation_order) {
    if (fec_controller_factory_.get()) {
      video_send_streams_[i] = sender_call_->CreateVideoSendStream(
          video_send_configs_[i].Copy(), video_encoder_configs_[i].Copy(),
          fec_controller_factory_->CreateFecController());
    } else {
      video_send_streams_[i] = sender_call_->CreateVideoSendStream(
          video_send_configs_[i].Copy(), video_encoder_configs_[i].Copy());
    }
  }
}

void CallTest::CreateVideoSendStream(const VideoEncoderConfig& encoder_config) {
  RTC_DCHECK(video_send_streams_.empty());
  video_send_streams_.push_back(sender_call_->CreateVideoSendStream(
      GetVideoSendConfig()->Copy(), encoder_config.Copy()));
}

void CallTest::CreateAudioStreams() {
  RTC_DCHECK(audio_send_stream_ == nullptr);
  RTC_DCHECK(audio_receive_streams_.empty());
  audio_send_stream_ = sender_call_->CreateAudioSendStream(audio_send_config_);
  for (size_t i = 0; i < audio_receive_configs_.size(); ++i) {
    audio_receive_streams_.push_back(
        receiver_call_->CreateAudioReceiveStream(audio_receive_configs_[i]));
  }
}

void CallTest::CreateFlexfecStreams() {
  for (size_t i = 0; i < flexfec_receive_configs_.size(); ++i) {
    flexfec_receive_streams_.push_back(
        receiver_call_->CreateFlexfecReceiveStream(
            flexfec_receive_configs_[i]));
  }
}

void CallTest::ConnectVideoSourcesToStreams() {
  for (size_t i = 0; i < video_sources_.size(); ++i)
    video_send_streams_[i]->SetSource(video_sources_[i].get(),
                                      degradation_preference_);
}

void CallTest::Start() {
  StartVideoStreams();
  if (audio_send_stream_) {
    audio_send_stream_->Start();
  }
  for (AudioReceiveStream* audio_recv_stream : audio_receive_streams_)
    audio_recv_stream->Start();
}

void CallTest::StartVideoStreams() {
  for (VideoSendStream* video_send_stream : video_send_streams_)
    video_send_stream->Start();
  for (VideoReceiveStream* video_recv_stream : video_receive_streams_)
    video_recv_stream->Start();
}

void CallTest::Stop() {
  for (AudioReceiveStream* audio_recv_stream : audio_receive_streams_)
    audio_recv_stream->Stop();
  if (audio_send_stream_) {
    audio_send_stream_->Stop();
  }
  StopVideoStreams();
}

void CallTest::StopVideoStreams() {
  for (VideoSendStream* video_send_stream : video_send_streams_)
    video_send_stream->Stop();
  for (VideoReceiveStream* video_recv_stream : video_receive_streams_)
    video_recv_stream->Stop();
}

void CallTest::DestroyStreams() {
  if (audio_send_stream_)
    sender_call_->DestroyAudioSendStream(audio_send_stream_);
  audio_send_stream_ = nullptr;
  for (AudioReceiveStream* audio_recv_stream : audio_receive_streams_)
    receiver_call_->DestroyAudioReceiveStream(audio_recv_stream);

  DestroyVideoSendStreams();

  for (VideoReceiveStream* video_recv_stream : video_receive_streams_)
    receiver_call_->DestroyVideoReceiveStream(video_recv_stream);

  for (FlexfecReceiveStream* flexfec_recv_stream : flexfec_receive_streams_)
    receiver_call_->DestroyFlexfecReceiveStream(flexfec_recv_stream);

  video_receive_streams_.clear();
  video_sources_.clear();
}

void CallTest::DestroyVideoSendStreams() {
  for (VideoSendStream* video_send_stream : video_send_streams_)
    sender_call_->DestroyVideoSendStream(video_send_stream);
  video_send_streams_.clear();
}

void CallTest::SetFakeVideoCaptureRotation(VideoRotation rotation) {
  frame_generator_capturer_->SetFakeRotation(rotation);
}

void CallTest::SetVideoDegradation(DegradationPreference preference) {
  GetVideoSendStream()->SetSource(frame_generator_capturer_, preference);
}

VideoSendStream::Config* CallTest::GetVideoSendConfig() {
  return &video_send_configs_[0];
}

void CallTest::SetVideoSendConfig(const VideoSendStream::Config& config) {
  video_send_configs_.clear();
  video_send_configs_.push_back(config.Copy());
}

VideoEncoderConfig* CallTest::GetVideoEncoderConfig() {
  return &video_encoder_configs_[0];
}

void CallTest::SetVideoEncoderConfig(const VideoEncoderConfig& config) {
  video_encoder_configs_.clear();
  video_encoder_configs_.push_back(config.Copy());
}

VideoSendStream* CallTest::GetVideoSendStream() {
  return video_send_streams_[0];
}
FlexfecReceiveStream::Config* CallTest::GetFlexFecConfig() {
  return &flexfec_receive_configs_[0];
}

void CallTest::OnRtpPacket(const RtpPacketReceived& packet) {
  // All FlexFEC streams protect all of the video streams.
  for (FlexfecReceiveStream* flexfec_recv_stream : flexfec_receive_streams_)
    flexfec_recv_stream->OnRtpPacket(packet);
}

absl::optional<RtpExtension> CallTest::GetRtpExtensionByUri(
    const std::string& uri) const {
  for (const auto& extension : rtp_extensions_) {
    if (extension.uri == uri) {
      return extension;
    }
  }
  return absl::nullopt;
}

void CallTest::AddRtpExtensionByUri(
    const std::string& uri,
    std::vector<RtpExtension>* extensions) const {
  const absl::optional<RtpExtension> extension = GetRtpExtensionByUri(uri);
  if (extension) {
    extensions->push_back(*extension);
  }
}

constexpr size_t CallTest::kNumSsrcs;
const int CallTest::kDefaultWidth;
const int CallTest::kDefaultHeight;
const int CallTest::kDefaultFramerate;
const int CallTest::kDefaultTimeoutMs = 30 * 1000;
const int CallTest::kLongTimeoutMs = 120 * 1000;
const uint32_t CallTest::kSendRtxSsrcs[kNumSsrcs] = {
    0xBADCAFD, 0xBADCAFE, 0xBADCAFF, 0xBADCB00, 0xBADCB01, 0xBADCB02};
const uint32_t CallTest::kVideoSendSsrcs[kNumSsrcs] = {
    0xC0FFED, 0xC0FFEE, 0xC0FFEF, 0xC0FFF0, 0xC0FFF1, 0xC0FFF2};
const uint32_t CallTest::kAudioSendSsrc = 0xDEADBEEF;
const uint32_t CallTest::kFlexfecSendSsrc = 0xBADBEEF;
const uint32_t CallTest::kReceiverLocalVideoSsrc = 0x123456;
const uint32_t CallTest::kReceiverLocalAudioSsrc = 0x1234567;
const int CallTest::kNackRtpHistoryMs = 1000;

const std::map<uint8_t, MediaType> CallTest::payload_type_map_ = {
    {CallTest::kVideoSendPayloadType, MediaType::VIDEO},
    {CallTest::kFakeVideoSendPayloadType, MediaType::VIDEO},
    {CallTest::kSendRtxPayloadType, MediaType::VIDEO},
    {CallTest::kRedPayloadType, MediaType::VIDEO},
    {CallTest::kRtxRedPayloadType, MediaType::VIDEO},
    {CallTest::kUlpfecPayloadType, MediaType::VIDEO},
    {CallTest::kFlexfecPayloadType, MediaType::VIDEO},
    {CallTest::kAudioSendPayloadType, MediaType::AUDIO}};

BaseTest::BaseTest() {}

BaseTest::BaseTest(int timeout_ms) : RtpRtcpObserver(timeout_ms) {}

BaseTest::~BaseTest() {}

std::unique_ptr<TestAudioDeviceModule::Capturer> BaseTest::CreateCapturer() {
  return TestAudioDeviceModule::CreatePulsedNoiseCapturer(256, 48000);
}

std::unique_ptr<TestAudioDeviceModule::Renderer> BaseTest::CreateRenderer() {
  return TestAudioDeviceModule::CreateDiscardRenderer(48000);
}

void BaseTest::OnFakeAudioDevicesCreated(
    TestAudioDeviceModule* send_audio_device,
    TestAudioDeviceModule* recv_audio_device) {}

void BaseTest::ModifySenderBitrateConfig(BitrateConstraints* bitrate_config) {}

void BaseTest::ModifyReceiverBitrateConfig(BitrateConstraints* bitrate_config) {
}

void BaseTest::OnCallsCreated(Call* sender_call, Call* receiver_call) {}

std::unique_ptr<PacketTransport> BaseTest::CreateSendTransport(
    TaskQueueBase* task_queue,
    Call* sender_call) {
  return std::make_unique<PacketTransport>(
      task_queue, sender_call, this, test::PacketTransport::kSender,
      CallTest::payload_type_map_,
      std::make_unique<FakeNetworkPipe>(
          Clock::GetRealTimeClock(),
          std::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig())));
}

std::unique_ptr<PacketTransport> BaseTest::CreateReceiveTransport(
    TaskQueueBase* task_queue) {
  return std::make_unique<PacketTransport>(
      task_queue, nullptr, this, test::PacketTransport::kReceiver,
      CallTest::payload_type_map_,
      std::make_unique<FakeNetworkPipe>(
          Clock::GetRealTimeClock(),
          std::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig())));
}

size_t BaseTest::GetNumVideoStreams() const {
  return 1;
}

size_t BaseTest::GetNumAudioStreams() const {
  return 0;
}

size_t BaseTest::GetNumFlexfecStreams() const {
  return 0;
}

void BaseTest::ModifyVideoConfigs(
    VideoSendStream::Config* send_config,
    std::vector<VideoReceiveStream::Config>* receive_configs,
    VideoEncoderConfig* encoder_config) {}

void BaseTest::ModifyVideoCaptureStartResolution(int* width,
                                                 int* heigt,
                                                 int* frame_rate) {}

void BaseTest::ModifyVideoDegradationPreference(
    DegradationPreference* degradation_preference) {}

void BaseTest::OnVideoStreamsCreated(
    VideoSendStream* send_stream,
    const std::vector<VideoReceiveStream*>& receive_streams) {}

void BaseTest::ModifyAudioConfigs(
    AudioSendStream::Config* send_config,
    std::vector<AudioReceiveStream::Config>* receive_configs) {}

void BaseTest::OnAudioStreamsCreated(
    AudioSendStream* send_stream,
    const std::vector<AudioReceiveStream*>& receive_streams) {}

void BaseTest::ModifyFlexfecConfigs(
    std::vector<FlexfecReceiveStream::Config>* receive_configs) {}

void BaseTest::OnFlexfecStreamsCreated(
    const std::vector<FlexfecReceiveStream*>& receive_streams) {}

void BaseTest::OnFrameGeneratorCapturerCreated(
    FrameGeneratorCapturer* frame_generator_capturer) {}

void BaseTest::OnStreamsStopped() {}

SendTest::SendTest(int timeout_ms) : BaseTest(timeout_ms) {}

bool SendTest::ShouldCreateReceivers() const {
  return false;
}

EndToEndTest::EndToEndTest() {}

EndToEndTest::EndToEndTest(int timeout_ms) : BaseTest(timeout_ms) {}

bool EndToEndTest::ShouldCreateReceivers() const {
  return true;
}

}  // namespace test
}  // namespace webrtc
