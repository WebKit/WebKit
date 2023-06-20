/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "examples/androidnativeapi/jni/android_call_client.h"

#include <memory>
#include <utility>

#include "api/peer_connection_interface.h"
#include "api/rtc_event_log/rtc_event_log_factory.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "examples/androidnativeapi/generated_jni/CallClient_jni.h"
#include "media/engine/internal_decoder_factory.h"
#include "media/engine/internal_encoder_factory.h"
#include "media/engine/webrtc_media_engine.h"
#include "media/engine/webrtc_media_engine_defaults.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/native_api/video/wrapper.h"

namespace webrtc_examples {

class AndroidCallClient::PCObserver : public webrtc::PeerConnectionObserver {
 public:
  explicit PCObserver(AndroidCallClient* client);

  void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state) override;
  void OnDataChannel(
      rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) override;
  void OnRenegotiationNeeded() override;
  void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state) override;
  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) override;
  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;

 private:
  AndroidCallClient* const client_;
};

namespace {

class CreateOfferObserver : public webrtc::CreateSessionDescriptionObserver {
 public:
  explicit CreateOfferObserver(
      rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc);

  void OnSuccess(webrtc::SessionDescriptionInterface* desc) override;
  void OnFailure(webrtc::RTCError error) override;

 private:
  const rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc_;
};

class SetRemoteSessionDescriptionObserver
    : public webrtc::SetRemoteDescriptionObserverInterface {
 public:
  void OnSetRemoteDescriptionComplete(webrtc::RTCError error) override;
};

class SetLocalSessionDescriptionObserver
    : public webrtc::SetSessionDescriptionObserver {
 public:
  void OnSuccess() override;
  void OnFailure(webrtc::RTCError error) override;
};

}  // namespace

AndroidCallClient::AndroidCallClient()
    : call_started_(false), pc_observer_(std::make_unique<PCObserver>(this)) {
  thread_checker_.Detach();
  CreatePeerConnectionFactory();
}

AndroidCallClient::~AndroidCallClient() = default;

void AndroidCallClient::Call(JNIEnv* env,
                             const webrtc::JavaRef<jobject>& local_sink,
                             const webrtc::JavaRef<jobject>& remote_sink) {
  RTC_DCHECK_RUN_ON(&thread_checker_);

  webrtc::MutexLock lock(&pc_mutex_);
  if (call_started_) {
    RTC_LOG(LS_WARNING) << "Call already started.";
    return;
  }
  call_started_ = true;

  local_sink_ = webrtc::JavaToNativeVideoSink(env, local_sink.obj());
  remote_sink_ = webrtc::JavaToNativeVideoSink(env, remote_sink.obj());

  video_source_ = webrtc::CreateJavaVideoSource(env, signaling_thread_.get(),
                                                /* is_screencast= */ false,
                                                /* align_timestamps= */ true);

  CreatePeerConnection();
  Connect();
}

void AndroidCallClient::Hangup(JNIEnv* env) {
  RTC_DCHECK_RUN_ON(&thread_checker_);

  call_started_ = false;

  {
    webrtc::MutexLock lock(&pc_mutex_);
    if (pc_ != nullptr) {
      pc_->Close();
      pc_ = nullptr;
    }
  }

  local_sink_ = nullptr;
  remote_sink_ = nullptr;
  video_source_ = nullptr;
}

void AndroidCallClient::Delete(JNIEnv* env) {
  RTC_DCHECK_RUN_ON(&thread_checker_);

  delete this;
}

webrtc::ScopedJavaLocalRef<jobject>
AndroidCallClient::GetJavaVideoCapturerObserver(JNIEnv* env) {
  RTC_DCHECK_RUN_ON(&thread_checker_);

  return video_source_->GetJavaVideoCapturerObserver(env);
}

void AndroidCallClient::CreatePeerConnectionFactory() {
  network_thread_ = rtc::Thread::CreateWithSocketServer();
  network_thread_->SetName("network_thread", nullptr);
  RTC_CHECK(network_thread_->Start()) << "Failed to start thread";

  worker_thread_ = rtc::Thread::Create();
  worker_thread_->SetName("worker_thread", nullptr);
  RTC_CHECK(worker_thread_->Start()) << "Failed to start thread";

  signaling_thread_ = rtc::Thread::Create();
  signaling_thread_->SetName("signaling_thread", nullptr);
  RTC_CHECK(signaling_thread_->Start()) << "Failed to start thread";

  webrtc::PeerConnectionFactoryDependencies pcf_deps;
  pcf_deps.network_thread = network_thread_.get();
  pcf_deps.worker_thread = worker_thread_.get();
  pcf_deps.signaling_thread = signaling_thread_.get();
  pcf_deps.task_queue_factory = webrtc::CreateDefaultTaskQueueFactory();
  pcf_deps.call_factory = webrtc::CreateCallFactory();
  pcf_deps.event_log_factory = std::make_unique<webrtc::RtcEventLogFactory>(
      pcf_deps.task_queue_factory.get());

  cricket::MediaEngineDependencies media_deps;
  media_deps.task_queue_factory = pcf_deps.task_queue_factory.get();
  media_deps.video_encoder_factory =
      std::make_unique<webrtc::InternalEncoderFactory>();
  media_deps.video_decoder_factory =
      std::make_unique<webrtc::InternalDecoderFactory>();
  webrtc::SetMediaEngineDefaults(&media_deps);
  pcf_deps.media_engine = cricket::CreateMediaEngine(std::move(media_deps));
  RTC_LOG(LS_INFO) << "Media engine created: " << pcf_deps.media_engine.get();

  pcf_ = CreateModularPeerConnectionFactory(std::move(pcf_deps));
  RTC_LOG(LS_INFO) << "PeerConnectionFactory created: " << pcf_.get();
}

void AndroidCallClient::CreatePeerConnection() {
  webrtc::MutexLock lock(&pc_mutex_);
  webrtc::PeerConnectionInterface::RTCConfiguration config;
  config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
  // Encryption has to be disabled for loopback to work.
  webrtc::PeerConnectionFactoryInterface::Options options;
  options.disable_encryption = true;
  pcf_->SetOptions(options);
  webrtc::PeerConnectionDependencies deps(pc_observer_.get());
  pc_ = pcf_->CreatePeerConnectionOrError(config, std::move(deps)).MoveValue();

  RTC_LOG(LS_INFO) << "PeerConnection created: " << pc_.get();

  rtc::scoped_refptr<webrtc::VideoTrackInterface> local_video_track =
      pcf_->CreateVideoTrack(video_source_, "video");
  local_video_track->AddOrUpdateSink(local_sink_.get(), rtc::VideoSinkWants());
  pc_->AddTransceiver(local_video_track);
  RTC_LOG(LS_INFO) << "Local video sink set up: " << local_video_track.get();

  for (const rtc::scoped_refptr<webrtc::RtpTransceiverInterface>& tranceiver :
       pc_->GetTransceivers()) {
    rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track =
        tranceiver->receiver()->track();
    if (track &&
        track->kind() == webrtc::MediaStreamTrackInterface::kVideoKind) {
      static_cast<webrtc::VideoTrackInterface*>(track.get())
          ->AddOrUpdateSink(remote_sink_.get(), rtc::VideoSinkWants());
      RTC_LOG(LS_INFO) << "Remote video sink set up: " << track.get();
      break;
    }
  }
}

void AndroidCallClient::Connect() {
  webrtc::MutexLock lock(&pc_mutex_);
  pc_->CreateOffer(rtc::make_ref_counted<CreateOfferObserver>(pc_).get(),
                   webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
}

AndroidCallClient::PCObserver::PCObserver(AndroidCallClient* client)
    : client_(client) {}

void AndroidCallClient::PCObserver::OnSignalingChange(
    webrtc::PeerConnectionInterface::SignalingState new_state) {
  RTC_LOG(LS_INFO) << "OnSignalingChange: " << new_state;
}

void AndroidCallClient::PCObserver::OnDataChannel(
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) {
  RTC_LOG(LS_INFO) << "OnDataChannel";
}

void AndroidCallClient::PCObserver::OnRenegotiationNeeded() {
  RTC_LOG(LS_INFO) << "OnRenegotiationNeeded";
}

void AndroidCallClient::PCObserver::OnIceConnectionChange(
    webrtc::PeerConnectionInterface::IceConnectionState new_state) {
  RTC_LOG(LS_INFO) << "OnIceConnectionChange: " << new_state;
}

void AndroidCallClient::PCObserver::OnIceGatheringChange(
    webrtc::PeerConnectionInterface::IceGatheringState new_state) {
  RTC_LOG(LS_INFO) << "OnIceGatheringChange: " << new_state;
}

void AndroidCallClient::PCObserver::OnIceCandidate(
    const webrtc::IceCandidateInterface* candidate) {
  RTC_LOG(LS_INFO) << "OnIceCandidate: " << candidate->server_url();
  webrtc::MutexLock lock(&client_->pc_mutex_);
  RTC_DCHECK(client_->pc_ != nullptr);
  client_->pc_->AddIceCandidate(candidate);
}

CreateOfferObserver::CreateOfferObserver(
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc)
    : pc_(pc) {}

void CreateOfferObserver::OnSuccess(webrtc::SessionDescriptionInterface* desc) {
  std::string sdp;
  desc->ToString(&sdp);
  RTC_LOG(LS_INFO) << "Created offer: " << sdp;

  // Ownership of desc was transferred to us, now we transfer it forward.
  pc_->SetLocalDescription(
      rtc::make_ref_counted<SetLocalSessionDescriptionObserver>().get(), desc);

  // Generate a fake answer.
  std::unique_ptr<webrtc::SessionDescriptionInterface> answer(
      webrtc::CreateSessionDescription(webrtc::SdpType::kAnswer, sdp));
  pc_->SetRemoteDescription(
      std::move(answer),
      rtc::make_ref_counted<SetRemoteSessionDescriptionObserver>());
}

void CreateOfferObserver::OnFailure(webrtc::RTCError error) {
  RTC_LOG(LS_INFO) << "Failed to create offer: " << ToString(error.type())
                   << ": " << error.message();
}

void SetRemoteSessionDescriptionObserver::OnSetRemoteDescriptionComplete(
    webrtc::RTCError error) {
  RTC_LOG(LS_INFO) << "Set remote description: " << error.message();
}

void SetLocalSessionDescriptionObserver::OnSuccess() {
  RTC_LOG(LS_INFO) << "Set local description success!";
}

void SetLocalSessionDescriptionObserver::OnFailure(webrtc::RTCError error) {
  RTC_LOG(LS_INFO) << "Set local description failure: "
                   << ToString(error.type()) << ": " << error.message();
}

static jlong JNI_CallClient_CreateClient(JNIEnv* env) {
  return webrtc::NativeToJavaPointer(new webrtc_examples::AndroidCallClient());
}

}  // namespace webrtc_examples
