/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "examples/objcnativeapi/objc/objc_call_client.h"

#include <memory>
#include <utility>

#import "sdk/objc/base/RTCVideoRenderer.h"
#import "sdk/objc/components/video_codec/RTCDefaultVideoDecoderFactory.h"
#import "sdk/objc/components/video_codec/RTCDefaultVideoEncoderFactory.h"
#import "sdk/objc/helpers/RTCCameraPreviewView.h"

#include "api/audio/audio_processing.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/enable_media.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_event_log/rtc_event_log_factory.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "sdk/objc/native/api/video_capturer.h"
#include "sdk/objc/native/api/video_decoder_factory.h"
#include "sdk/objc/native/api/video_encoder_factory.h"
#include "sdk/objc/native/api/video_renderer.h"

namespace webrtc_examples {

namespace {

class CreateOfferObserver : public webrtc::CreateSessionDescriptionObserver {
 public:
  explicit CreateOfferObserver(rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc);

  void OnSuccess(webrtc::SessionDescriptionInterface* desc) override;
  void OnFailure(webrtc::RTCError error) override;

 private:
  const rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc_;
};

class SetRemoteSessionDescriptionObserver : public webrtc::SetRemoteDescriptionObserverInterface {
 public:
  void OnSetRemoteDescriptionComplete(webrtc::RTCError error) override;
};

class SetLocalSessionDescriptionObserver : public webrtc::SetLocalDescriptionObserverInterface {
 public:
  void OnSetLocalDescriptionComplete(webrtc::RTCError error) override;
};

}  // namespace

ObjCCallClient::ObjCCallClient()
    : call_started_(false), pc_observer_(std::make_unique<PCObserver>(this)) {
  thread_checker_.Detach();
  CreatePeerConnectionFactory();
}

void ObjCCallClient::Call(RTC_OBJC_TYPE(RTCVideoCapturer) * capturer,
                          id<RTC_OBJC_TYPE(RTCVideoRenderer)> remote_renderer) {
  RTC_DCHECK_RUN_ON(&thread_checker_);

  webrtc::MutexLock lock(&pc_mutex_);
  if (call_started_) {
    RTC_LOG(LS_WARNING) << "Call already started.";
    return;
  }
  call_started_ = true;

  remote_sink_ = webrtc::ObjCToNativeVideoRenderer(remote_renderer);

  video_source_ =
      webrtc::ObjCToNativeVideoCapturer(capturer, signaling_thread_.get(), worker_thread_.get());

  CreatePeerConnection();
  Connect();
}

void ObjCCallClient::Hangup() {
  RTC_DCHECK_RUN_ON(&thread_checker_);

  call_started_ = false;

  {
    webrtc::MutexLock lock(&pc_mutex_);
    if (pc_ != nullptr) {
      pc_->Close();
      pc_ = nullptr;
    }
  }

  remote_sink_ = nullptr;
  video_source_ = nullptr;
}

void ObjCCallClient::CreatePeerConnectionFactory() {
  network_thread_ = rtc::Thread::CreateWithSocketServer();
  network_thread_->SetName("network_thread", nullptr);
  RTC_CHECK(network_thread_->Start()) << "Failed to start thread";

  worker_thread_ = rtc::Thread::Create();
  worker_thread_->SetName("worker_thread", nullptr);
  RTC_CHECK(worker_thread_->Start()) << "Failed to start thread";

  signaling_thread_ = rtc::Thread::Create();
  signaling_thread_->SetName("signaling_thread", nullptr);
  RTC_CHECK(signaling_thread_->Start()) << "Failed to start thread";

  webrtc::PeerConnectionFactoryDependencies dependencies;
  dependencies.network_thread = network_thread_.get();
  dependencies.worker_thread = worker_thread_.get();
  dependencies.signaling_thread = signaling_thread_.get();
  dependencies.task_queue_factory = webrtc::CreateDefaultTaskQueueFactory();
  dependencies.audio_encoder_factory = webrtc::CreateBuiltinAudioEncoderFactory();
  dependencies.audio_decoder_factory = webrtc::CreateBuiltinAudioDecoderFactory();
  dependencies.video_encoder_factory = webrtc::ObjCToNativeVideoEncoderFactory(
      [[RTC_OBJC_TYPE(RTCDefaultVideoEncoderFactory) alloc] init]);
  dependencies.video_decoder_factory = webrtc::ObjCToNativeVideoDecoderFactory(
      [[RTC_OBJC_TYPE(RTCDefaultVideoDecoderFactory) alloc] init]);
  dependencies.audio_processing = webrtc::AudioProcessingBuilder().Create();
  webrtc::EnableMedia(dependencies);
  dependencies.event_log_factory = std::make_unique<webrtc::RtcEventLogFactory>();
  pcf_ = webrtc::CreateModularPeerConnectionFactory(std::move(dependencies));
  RTC_LOG(LS_INFO) << "PeerConnectionFactory created: " << pcf_.get();
}

void ObjCCallClient::CreatePeerConnection() {
  webrtc::MutexLock lock(&pc_mutex_);
  webrtc::PeerConnectionInterface::RTCConfiguration config;
  config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
  // Encryption has to be disabled for loopback to work.
  webrtc::PeerConnectionFactoryInterface::Options options;
  options.disable_encryption = true;
  pcf_->SetOptions(options);
  webrtc::PeerConnectionDependencies pc_dependencies(pc_observer_.get());
  pc_ = pcf_->CreatePeerConnectionOrError(config, std::move(pc_dependencies)).MoveValue();
  RTC_LOG(LS_INFO) << "PeerConnection created: " << pc_.get();

  rtc::scoped_refptr<webrtc::VideoTrackInterface> local_video_track =
      pcf_->CreateVideoTrack(video_source_, "video");
  pc_->AddTransceiver(local_video_track);
  RTC_LOG(LS_INFO) << "Local video sink set up: " << local_video_track.get();

  for (const rtc::scoped_refptr<webrtc::RtpTransceiverInterface>& tranceiver :
       pc_->GetTransceivers()) {
    rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track = tranceiver->receiver()->track();
    if (track && track->kind() == webrtc::MediaStreamTrackInterface::kVideoKind) {
      static_cast<webrtc::VideoTrackInterface*>(track.get())
          ->AddOrUpdateSink(remote_sink_.get(), rtc::VideoSinkWants());
      RTC_LOG(LS_INFO) << "Remote video sink set up: " << track.get();
      break;
    }
  }
}

void ObjCCallClient::Connect() {
  webrtc::MutexLock lock(&pc_mutex_);
  pc_->CreateOffer(rtc::make_ref_counted<CreateOfferObserver>(pc_).get(),
                   webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
}

ObjCCallClient::PCObserver::PCObserver(ObjCCallClient* client) : client_(client) {}

void ObjCCallClient::PCObserver::OnSignalingChange(
    webrtc::PeerConnectionInterface::SignalingState new_state) {
  RTC_LOG(LS_INFO) << "OnSignalingChange: " << new_state;
}

void ObjCCallClient::PCObserver::OnDataChannel(
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) {
  RTC_LOG(LS_INFO) << "OnDataChannel";
}

void ObjCCallClient::PCObserver::OnRenegotiationNeeded() {
  RTC_LOG(LS_INFO) << "OnRenegotiationNeeded";
}

void ObjCCallClient::PCObserver::OnIceConnectionChange(
    webrtc::PeerConnectionInterface::IceConnectionState new_state) {
  RTC_LOG(LS_INFO) << "OnIceConnectionChange: " << new_state;
}

void ObjCCallClient::PCObserver::OnIceGatheringChange(
    webrtc::PeerConnectionInterface::IceGatheringState new_state) {
  RTC_LOG(LS_INFO) << "OnIceGatheringChange: " << new_state;
}

void ObjCCallClient::PCObserver::OnIceCandidate(const webrtc::IceCandidateInterface* candidate) {
  RTC_LOG(LS_INFO) << "OnIceCandidate: " << candidate->server_url();
  webrtc::MutexLock lock(&client_->pc_mutex_);
  RTC_DCHECK(client_->pc_ != nullptr);
  client_->pc_->AddIceCandidate(candidate);
}

CreateOfferObserver::CreateOfferObserver(rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc)
    : pc_(pc) {}

void CreateOfferObserver::OnSuccess(webrtc::SessionDescriptionInterface* desc) {
  std::string sdp;
  desc->ToString(&sdp);
  RTC_LOG(LS_INFO) << "Created offer: " << sdp;

  // Ownership of desc was transferred to us, now we transfer it forward.
  pc_->SetLocalDescription(absl::WrapUnique(desc),
                           rtc::make_ref_counted<SetLocalSessionDescriptionObserver>());

  // Generate a fake answer.
  std::unique_ptr<webrtc::SessionDescriptionInterface> answer(
      webrtc::CreateSessionDescription(webrtc::SdpType::kAnswer, sdp));
  pc_->SetRemoteDescription(std::move(answer),
                            rtc::make_ref_counted<SetRemoteSessionDescriptionObserver>());
}

void CreateOfferObserver::OnFailure(webrtc::RTCError error) {
  RTC_LOG(LS_INFO) << "Failed to create offer: " << error.message();
}

void SetRemoteSessionDescriptionObserver::OnSetRemoteDescriptionComplete(webrtc::RTCError error) {
  RTC_LOG(LS_INFO) << "Set remote description: " << error.message();
}

void SetLocalSessionDescriptionObserver::OnSetLocalDescriptionComplete(webrtc::RTCError error) {
  RTC_LOG(LS_INFO) << "Set local description: " << error.message();
}

}  // namespace webrtc_examples
