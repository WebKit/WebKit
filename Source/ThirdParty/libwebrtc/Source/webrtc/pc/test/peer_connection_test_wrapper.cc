/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/test/peer_connection_test_wrapper.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "api/audio/audio_device.h"
#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/audio_options.h"
#include "api/create_peerconnection_factory.h"
#include "api/data_channel_interface.h"
#include "api/environment/environment.h"
#include "api/environment/environment_factory.h"
#include "api/field_trials_view.h"
#include "api/jsep.h"
#include "api/make_ref_counted.h"
#include "api/media_stream_interface.h"
#include "api/media_types.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_error.h"
#include "api/rtp_parameters.h"
#include "api/rtp_receiver_interface.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/test/rtc_error_matchers.h"
#include "api/units/time_delta.h"
#include "api/video/resolution.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_decoder_factory_template.h"
#include "api/video_codecs/video_decoder_factory_template_dav1d_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp8_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp9_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_open_h264_adapter.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "api/video_codecs/video_encoder_factory_template.h"
#include "api/video_codecs/video_encoder_factory_template_libaom_av1_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_libvpx_vp8_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_libvpx_vp9_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_open_h264_adapter.h"
#include "media/engine/simulcast_encoder_adapter.h"
#include "p2p/test/fake_port_allocator.h"
#include "pc/test/fake_audio_capture_module.h"
#include "pc/test/fake_periodic_video_source.h"
#include "pc/test/fake_periodic_video_track_source.h"
#include "pc/test/fake_rtc_certificate_generator.h"
#include "pc/test/fake_video_track_renderer.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "rtc_base/logging.h"
#include "rtc_base/rtc_certificate_generator.h"
#include "rtc_base/socket_server.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/wait_until.h"

namespace webrtc {
namespace {

const char kStreamIdBase[] = "stream_id";
const char kVideoTrackLabelBase[] = "video_track";
const char kAudioTrackLabelBase[] = "audio_track";
constexpr int kMaxWait = 10000;
constexpr int kTestAudioFrameCount = 3;
constexpr int kTestVideoFrameCount = 3;

class FuzzyMatchedVideoEncoderFactory : public VideoEncoderFactory {
 public:
  std::vector<SdpVideoFormat> GetSupportedFormats() const override {
    return factory_.GetSupportedFormats();
  }

  std::unique_ptr<VideoEncoder> Create(const Environment& env,
                                       const SdpVideoFormat& format) override {
    if (std::optional<SdpVideoFormat> original_format =
            FuzzyMatchSdpVideoFormat(factory_.GetSupportedFormats(), format)) {
      return std::make_unique<SimulcastEncoderAdapter>(env, &factory_, nullptr,
                                                       *original_format);
    }

    return nullptr;
  }

  CodecSupport QueryCodecSupport(
      const SdpVideoFormat& format,
      std::optional<std::string> scalability_mode) const override {
    return factory_.QueryCodecSupport(format, scalability_mode);
  }

 private:
  VideoEncoderFactoryTemplate<LibvpxVp8EncoderTemplateAdapter,
                              LibvpxVp9EncoderTemplateAdapter,
                              OpenH264EncoderTemplateAdapter,
                              LibaomAv1EncoderTemplateAdapter>
      factory_;
};
}  // namespace

void PeerConnectionTestWrapper::Connect(PeerConnectionTestWrapper* caller,
                                        PeerConnectionTestWrapper* callee) {
  caller->SignalOnIceCandidateReady.connect(
      callee, &PeerConnectionTestWrapper::AddIceCandidate);
  callee->SignalOnIceCandidateReady.connect(
      caller, &PeerConnectionTestWrapper::AddIceCandidate);

  caller->SignalOnSdpReady.connect(callee,
                                   &PeerConnectionTestWrapper::ReceiveOfferSdp);
  callee->SignalOnSdpReady.connect(
      caller, &PeerConnectionTestWrapper::ReceiveAnswerSdp);
}

void PeerConnectionTestWrapper::AwaitNegotiation(
    PeerConnectionTestWrapper* caller,
    PeerConnectionTestWrapper* callee) {
  auto offer = caller->AwaitCreateOffer();
  caller->AwaitSetLocalDescription(offer.get());
  callee->AwaitSetRemoteDescription(offer.get());
  auto answer = callee->AwaitCreateAnswer();
  callee->AwaitSetLocalDescription(answer.get());
  caller->AwaitSetRemoteDescription(answer.get());
}

PeerConnectionTestWrapper::PeerConnectionTestWrapper(
    const std::string& name,
    const Environment& env,
    SocketServer* socket_server,
    Thread* network_thread,
    Thread* worker_thread)
    : name_(name),
      env_(env),
      socket_server_(socket_server),
      network_thread_(network_thread),
      worker_thread_(worker_thread),
      pending_negotiation_(false) {
  pc_thread_checker_.Detach();
}

PeerConnectionTestWrapper::~PeerConnectionTestWrapper() {
  RTC_DCHECK_RUN_ON(&pc_thread_checker_);
  // To avoid flaky bot failures, make sure fake sources are stopped prior to
  // closing the peer connections. See https://crbug.com/webrtc/15018.
  StopFakeVideoSources();
  // Either network_thread or worker_thread might be active at this point.
  // Relying on ~PeerConnection to properly wait for them doesn't work,
  // as a vptr race might occur (before we enter the destruction body).
  // See: bugs.webrtc.org/9847
  if (pc()) {
    pc()->Close();
  }
}

bool PeerConnectionTestWrapper::CreatePc(
    const PeerConnectionInterface::RTCConfiguration& config,
    scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
    scoped_refptr<AudioDecoderFactory> audio_decoder_factory,
    std::unique_ptr<VideoEncoderFactory> video_encoder_factory,
    std::unique_ptr<VideoDecoderFactory> video_decoder_factory,
    std::unique_ptr<FieldTrialsView> field_trials) {
  EnvironmentFactory env_factory(env_);
  env_factory.Set(field_trials.get());
  Environment env = env_factory.Create();
  auto port_allocator =
      std::make_unique<FakePortAllocator>(env, socket_server_, network_thread_);

  RTC_DCHECK_RUN_ON(&pc_thread_checker_);

  fake_audio_capture_module_ = FakeAudioCaptureModule::Create();
  if (fake_audio_capture_module_ == nullptr) {
    return false;
  }

  peer_connection_factory_ = CreatePeerConnectionFactory(
      network_thread_, worker_thread_, Thread::Current(),
      scoped_refptr<AudioDeviceModule>(fake_audio_capture_module_),
      audio_encoder_factory, audio_decoder_factory,
      std::move(video_encoder_factory), std::move(video_decoder_factory),
      nullptr /* audio_mixer */, nullptr /* audio_processing */, nullptr,
      std::move(field_trials));
  if (!peer_connection_factory_) {
    return false;
  }

  std::unique_ptr<RTCCertificateGeneratorInterface> cert_generator(
      new FakeRTCCertificateGenerator());
  PeerConnectionDependencies deps(this);
  deps.allocator = std::move(port_allocator);
  deps.cert_generator = std::move(cert_generator);
  auto result = peer_connection_factory_->CreatePeerConnectionOrError(
      config, std::move(deps));
  if (result.ok()) {
    peer_connection_ = result.MoveValue();
    return true;
  } else {
    return false;
  }
}

bool PeerConnectionTestWrapper::CreatePc(
    const PeerConnectionInterface::RTCConfiguration& config,
    scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
    scoped_refptr<AudioDecoderFactory> audio_decoder_factory,
    std::unique_ptr<FieldTrialsView> field_trials) {
  return CreatePc(
      config, std::move(audio_encoder_factory),
      std::move(audio_decoder_factory),
      std::make_unique<FuzzyMatchedVideoEncoderFactory>(),
      std::make_unique<VideoDecoderFactoryTemplate<
          LibvpxVp8DecoderTemplateAdapter, LibvpxVp9DecoderTemplateAdapter,
          OpenH264DecoderTemplateAdapter, Dav1dDecoderTemplateAdapter>>(),
      std::move(field_trials));
}

scoped_refptr<DataChannelInterface>
PeerConnectionTestWrapper::CreateDataChannel(const std::string& label,
                                             const DataChannelInit& init) {
  auto result = peer_connection_->CreateDataChannelOrError(label, &init);
  if (!result.ok()) {
    RTC_LOG(LS_ERROR) << "CreateDataChannel failed: "
                      << ToString(result.error().type()) << " "
                      << result.error().message();
    return nullptr;
  }
  return result.MoveValue();
}

std::optional<RtpCodecCapability>
PeerConnectionTestWrapper::FindFirstSendCodecWithName(
    MediaType media_type,
    const std::string& name) const {
  std::vector<RtpCodecCapability> codecs =
      peer_connection_factory_->GetRtpSenderCapabilities(media_type).codecs;
  for (const auto& codec : codecs) {
    if (absl::EqualsIgnoreCase(codec.name, name)) {
      return codec;
    }
  }
  return std::nullopt;
}

void PeerConnectionTestWrapper::WaitForNegotiation() {
  EXPECT_THAT(
      WaitUntil([&] { return !pending_negotiation_; }, ::testing::IsTrue(),
                {.timeout = TimeDelta::Millis(kMaxWait)}),
      IsRtcOk());
}

std::unique_ptr<SessionDescriptionInterface>
PeerConnectionTestWrapper::AwaitCreateOffer() {
  auto observer = make_ref_counted<MockCreateSessionDescriptionObserver>();
  peer_connection_->CreateOffer(observer.get(), {});
  EXPECT_THAT(
      WaitUntil([&] { return observer->called(); }, ::testing::IsTrue()),
      IsRtcOk());
  return observer->MoveDescription();
}

std::unique_ptr<SessionDescriptionInterface>
PeerConnectionTestWrapper::AwaitCreateAnswer() {
  auto observer = make_ref_counted<MockCreateSessionDescriptionObserver>();
  peer_connection_->CreateAnswer(observer.get(), {});
  EXPECT_THAT(
      WaitUntil([&] { return observer->called(); }, ::testing::IsTrue()),
      IsRtcOk());
  return observer->MoveDescription();
}

void PeerConnectionTestWrapper::AwaitSetLocalDescription(
    webrtc::SessionDescriptionInterface* sdp) {
  auto observer = make_ref_counted<MockSetSessionDescriptionObserver>();
  peer_connection_->SetLocalDescription(observer.get(), sdp->Clone().release());
  EXPECT_THAT(
      WaitUntil([&] { return observer->called(); }, ::testing::IsTrue()),
      IsRtcOk());
}

void PeerConnectionTestWrapper::AwaitSetRemoteDescription(
    SessionDescriptionInterface* sdp) {
  auto observer = make_ref_counted<MockSetSessionDescriptionObserver>();
  peer_connection_->SetRemoteDescription(observer.get(),
                                         sdp->Clone().release());
  EXPECT_THAT(
      WaitUntil([&] { return observer->called(); }, ::testing::IsTrue()),
      IsRtcOk());
}

void PeerConnectionTestWrapper::ListenForRemoteIceCandidates(
    scoped_refptr<PeerConnectionTestWrapper> remote_wrapper) {
  remote_wrapper_ = remote_wrapper;
  remote_wrapper_->SignalOnIceCandidateReady.connect(
      this, &PeerConnectionTestWrapper::OnRemoteIceCandidate);
}

void PeerConnectionTestWrapper::AwaitAddRemoteIceCandidates() {
  EXPECT_TRUE(remote_wrapper_);
  EXPECT_THAT(
      WaitUntil(
          [&] {
            return remote_wrapper_->pc()->ice_gathering_state() ==
                   PeerConnectionInterface::kIceGatheringComplete;
          },
          ::testing::IsTrue(), {.timeout = TimeDelta::Millis(kMaxWait)}),
      IsRtcOk());
  for (const auto& remote_ice_candidate : remote_ice_candidates_) {
    peer_connection_->AddIceCandidate(remote_ice_candidate.get());
  }
  remote_wrapper_ = nullptr;
  remote_ice_candidates_.clear();
}

void PeerConnectionTestWrapper::OnRemoteIceCandidate(
    const std::string& sdp_mid,
    int sdp_mline_index,
    const std::string& candidate) {
  remote_ice_candidates_.emplace_back(
      CreateIceCandidate(sdp_mid, sdp_mline_index, candidate, nullptr));
}

void PeerConnectionTestWrapper::OnSignalingChange(
    PeerConnectionInterface::SignalingState new_state) {
  if (new_state == PeerConnectionInterface::SignalingState::kStable) {
    pending_negotiation_ = false;
  }
}

void PeerConnectionTestWrapper::OnAddTrack(
    scoped_refptr<RtpReceiverInterface> receiver,
    const std::vector<scoped_refptr<MediaStreamInterface>>& streams) {
  RTC_LOG(LS_INFO) << "PeerConnectionTestWrapper " << name_ << ": OnAddTrack";
  if (receiver->track()->kind() == MediaStreamTrackInterface::kVideoKind) {
    auto* video_track =
        static_cast<VideoTrackInterface*>(receiver->track().get());
    renderer_ = std::make_unique<FakeVideoTrackRenderer>(video_track);
  }
}

void PeerConnectionTestWrapper::OnIceCandidate(const IceCandidate* candidate) {
  std::string sdp = candidate->ToString();
  SignalOnIceCandidateReady(candidate->sdp_mid(), candidate->sdp_mline_index(),
                            sdp);
}

void PeerConnectionTestWrapper::OnDataChannel(
    scoped_refptr<DataChannelInterface> data_channel) {
  SignalOnDataChannel(data_channel.get());
}

void PeerConnectionTestWrapper::OnSuccess(SessionDescriptionInterface* desc) {
  // This callback should take the ownership of `desc`.
  std::unique_ptr<SessionDescriptionInterface> owned_desc(desc);
  std::string sdp;
  EXPECT_TRUE(desc->ToString(&sdp));

  RTC_LOG(LS_INFO) << "PeerConnectionTestWrapper " << name_ << ": "
                   << desc->GetType() << " sdp created: " << sdp;

  SetLocalDescription(desc->GetType(), sdp);

  SignalOnSdpReady(sdp);
}

void PeerConnectionTestWrapper::CreateOffer(
    const PeerConnectionInterface::RTCOfferAnswerOptions& options) {
  RTC_LOG(LS_INFO) << "PeerConnectionTestWrapper " << name_ << ": CreateOffer.";
  pending_negotiation_ = true;
  peer_connection_->CreateOffer(this, options);
}

void PeerConnectionTestWrapper::CreateAnswer(
    const PeerConnectionInterface::RTCOfferAnswerOptions& options) {
  RTC_LOG(LS_INFO) << "PeerConnectionTestWrapper " << name_
                   << ": CreateAnswer.";
  pending_negotiation_ = true;
  peer_connection_->CreateAnswer(this, options);
}

void PeerConnectionTestWrapper::ReceiveOfferSdp(const std::string& sdp) {
  SetRemoteDescription(SdpType::kOffer, sdp);
  CreateAnswer(PeerConnectionInterface::RTCOfferAnswerOptions());
}

void PeerConnectionTestWrapper::ReceiveAnswerSdp(const std::string& sdp) {
  SetRemoteDescription(SdpType::kAnswer, sdp);
}

void PeerConnectionTestWrapper::SetLocalDescription(SdpType type,
                                                    const std::string& sdp) {
  RTC_LOG(LS_INFO) << "PeerConnectionTestWrapper " << name_
                   << ": SetLocalDescription " << type << " " << sdp;

  auto observer = make_ref_counted<MockSetSessionDescriptionObserver>();
  peer_connection_->SetLocalDescription(
      observer.get(), CreateSessionDescription(type, sdp).release());
}

void PeerConnectionTestWrapper::SetRemoteDescription(SdpType type,
                                                     const std::string& sdp) {
  RTC_LOG(LS_INFO) << "PeerConnectionTestWrapper " << name_
                   << ": SetRemoteDescription " << type << " " << sdp;

  auto observer = make_ref_counted<MockSetSessionDescriptionObserver>();
  peer_connection_->SetRemoteDescription(
      observer.get(), CreateSessionDescription(type, sdp).release());
}

void PeerConnectionTestWrapper::AddIceCandidate(const std::string& sdp_mid,
                                                int sdp_mline_index,
                                                const std::string& candidate) {
  std::unique_ptr<IceCandidate> owned_candidate(
      CreateIceCandidate(sdp_mid, sdp_mline_index, candidate, nullptr));
  EXPECT_TRUE(peer_connection_->AddIceCandidate(owned_candidate.get()));
}

bool PeerConnectionTestWrapper::WaitForCallEstablished() {
  if (!WaitForConnection())
    return false;
  if (!WaitForAudio())
    return false;
  if (!WaitForVideo())
    return false;
  return true;
}

bool PeerConnectionTestWrapper::WaitForConnection() {
  EXPECT_THAT(
      WaitUntil([&] { return CheckForConnection(); }, ::testing::IsTrue(),
                {.timeout = TimeDelta::Millis(kMaxWait)}),
      IsRtcOk());
  if (testing::Test::HasFailure()) {
    return false;
  }
  RTC_LOG(LS_INFO) << "PeerConnectionTestWrapper " << name_ << ": Connected.";
  return true;
}

bool PeerConnectionTestWrapper::CheckForConnection() {
  return (peer_connection_->ice_connection_state() ==
          PeerConnectionInterface::kIceConnectionConnected) ||
         (peer_connection_->ice_connection_state() ==
          PeerConnectionInterface::kIceConnectionCompleted);
}

bool PeerConnectionTestWrapper::WaitForAudio() {
  EXPECT_THAT(WaitUntil([&] { return CheckForAudio(); }, ::testing::IsTrue(),
                        {.timeout = TimeDelta::Millis(kMaxWait)}),
              IsRtcOk());
  if (testing::Test::HasFailure()) {
    return false;
  }
  RTC_LOG(LS_INFO) << "PeerConnectionTestWrapper " << name_
                   << ": Got enough audio frames.";
  return true;
}

bool PeerConnectionTestWrapper::CheckForAudio() {
  return (fake_audio_capture_module_->frames_received() >=
          kTestAudioFrameCount);
}

bool PeerConnectionTestWrapper::WaitForVideo() {
  EXPECT_THAT(WaitUntil([&] { return CheckForVideo(); }, ::testing::IsTrue(),
                        {.timeout = TimeDelta::Millis(kMaxWait)}),
              IsRtcOk());
  if (testing::Test::HasFailure()) {
    return false;
  }
  RTC_LOG(LS_INFO) << "PeerConnectionTestWrapper " << name_
                   << ": Got enough video frames.";
  return true;
}

bool PeerConnectionTestWrapper::CheckForVideo() {
  if (!renderer_) {
    return false;
  }
  return (renderer_->num_rendered_frames() >= kTestVideoFrameCount);
}

void PeerConnectionTestWrapper::GetAndAddUserMedia(
    bool audio,
    const AudioOptions& audio_options,
    bool video) {
  scoped_refptr<MediaStreamInterface> stream =
      GetUserMedia(audio, audio_options, video);
  for (const auto& audio_track : stream->GetAudioTracks()) {
    EXPECT_TRUE(peer_connection_->AddTrack(audio_track, {stream->id()}).ok());
  }
  for (const auto& video_track : stream->GetVideoTracks()) {
    EXPECT_TRUE(peer_connection_->AddTrack(video_track, {stream->id()}).ok());
  }
}

scoped_refptr<MediaStreamInterface> PeerConnectionTestWrapper::GetUserMedia(
    bool audio,
    const AudioOptions& audio_options,
    bool video,
    Resolution resolution) {
  std::string stream_id =
      kStreamIdBase + absl::StrCat(num_get_user_media_calls_++);
  scoped_refptr<MediaStreamInterface> stream =
      peer_connection_factory_->CreateLocalMediaStream(stream_id);

  if (audio) {
    AudioOptions options = audio_options;
    // Disable highpass filter so that we can get all the test audio frames.
    options.highpass_filter = false;
    scoped_refptr<AudioSourceInterface> source =
        peer_connection_factory_->CreateAudioSource(options);
    scoped_refptr<AudioTrackInterface> audio_track(
        peer_connection_factory_->CreateAudioTrack(kAudioTrackLabelBase,
                                                   source.get()));
    stream->AddTrack(audio_track);
  }

  if (video) {
    // Set max frame rate to 10fps to reduce the risk of the tests to be flaky.
    FakePeriodicVideoSource::Config config;
    config.frame_interval_ms = 100;
    config.timestamp_offset_ms = env_.clock().TimeInMilliseconds();
    config.width = resolution.width;
    config.height = resolution.height;

    auto source = make_ref_counted<FakePeriodicVideoTrackSource>(
        config, /* remote */ false);
    fake_video_sources_.push_back(source);

    std::string videotrack_label = stream_id + kVideoTrackLabelBase;
    scoped_refptr<VideoTrackInterface> video_track(
        peer_connection_factory_->CreateVideoTrack(source, videotrack_label));

    stream->AddTrack(video_track);
  }
  return stream;
}

void PeerConnectionTestWrapper::StopFakeVideoSources() {
  for (const auto& fake_video_source : fake_video_sources_) {
    fake_video_source->fake_periodic_source().Stop();
  }
  fake_video_sources_.clear();
}

}  // namespace webrtc
