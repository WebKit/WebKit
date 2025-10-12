/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_tools/data_channel_benchmark/peer_connection_client.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <utility>

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/create_peerconnection_factory.h"
#include "api/data_channel_interface.h"
#include "api/field_trials_view.h"
#include "api/jsep.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_error.h"
#include "api/scoped_refptr.h"
#include "api/set_local_description_observer_interface.h"
#include "api/set_remote_description_observer_interface.h"
#include "api/video_codecs/video_decoder_factory_template.h"
#include "api/video_codecs/video_decoder_factory_template_dav1d_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp8_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp9_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_open_h264_adapter.h"
#include "api/video_codecs/video_encoder_factory_template.h"
#include "api/video_codecs/video_encoder_factory_template_libaom_av1_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_libvpx_vp8_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_libvpx_vp9_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_open_h264_adapter.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/thread.h"
#include "rtc_tools/data_channel_benchmark/signaling_interface.h"

namespace {

constexpr char kStunServer[] = "stun:stun.l.google.com:19302";

class SetLocalDescriptionObserverAdapter
    : public webrtc::SetLocalDescriptionObserverInterface {
 public:
  using Callback = std::function<void(webrtc::RTCError)>;
  static webrtc::scoped_refptr<SetLocalDescriptionObserverAdapter> Create(
      Callback callback) {
    return webrtc::scoped_refptr<SetLocalDescriptionObserverAdapter>(
        new webrtc::RefCountedObject<SetLocalDescriptionObserverAdapter>(
            std::move(callback)));
  }

  explicit SetLocalDescriptionObserverAdapter(Callback callback)
      : callback_(std::move(callback)) {}
  ~SetLocalDescriptionObserverAdapter() override = default;

 private:
  void OnSetLocalDescriptionComplete(webrtc::RTCError error) override {
    callback_(std::move(error));
  }

  Callback callback_;
};

class SetRemoteDescriptionObserverAdapter
    : public webrtc::SetRemoteDescriptionObserverInterface {
 public:
  using Callback = std::function<void(webrtc::RTCError)>;
  static webrtc::scoped_refptr<SetRemoteDescriptionObserverAdapter> Create(
      Callback callback) {
    return webrtc::scoped_refptr<SetRemoteDescriptionObserverAdapter>(
        new webrtc::RefCountedObject<SetRemoteDescriptionObserverAdapter>(
            std::move(callback)));
  }

  explicit SetRemoteDescriptionObserverAdapter(Callback callback)
      : callback_(std::move(callback)) {}
  ~SetRemoteDescriptionObserverAdapter() override = default;

 private:
  void OnSetRemoteDescriptionComplete(webrtc::RTCError error) override {
    callback_(std::move(error));
  }

  Callback callback_;
};

class CreateSessionDescriptionObserverAdapter
    : public webrtc::CreateSessionDescriptionObserver {
 public:
  using Success = std::function<void(webrtc::SessionDescriptionInterface*)>;
  using Failure = std::function<void(webrtc::RTCError)>;

  static webrtc::scoped_refptr<CreateSessionDescriptionObserverAdapter> Create(
      Success success,
      Failure failure) {
    return webrtc::scoped_refptr<CreateSessionDescriptionObserverAdapter>(
        new webrtc::RefCountedObject<CreateSessionDescriptionObserverAdapter>(
            std::move(success), std::move(failure)));
  }

  CreateSessionDescriptionObserverAdapter(Success success, Failure failure)
      : success_(std::move(success)), failure_(std::move(failure)) {}
  ~CreateSessionDescriptionObserverAdapter() override = default;

 private:
  void OnSuccess(webrtc::SessionDescriptionInterface* desc) override {
    success_(desc);
  }

  void OnFailure(webrtc::RTCError error) override {
    failure_(std::move(error));
  }

  Success success_;
  Failure failure_;
};

}  // namespace

namespace webrtc {

PeerConnectionClient::PeerConnectionClient(
    PeerConnectionFactoryInterface* factory,
    SignalingInterface* signaling)
    : signaling_(signaling) {
  signaling_->OnIceCandidate([&](std::unique_ptr<IceCandidate> candidate) {
    AddIceCandidate(std::move(candidate));
  });
  signaling_->OnRemoteDescription(
      [&](std::unique_ptr<SessionDescriptionInterface> sdp) {
        SetRemoteDescription(std::move(sdp));
      });
  InitializePeerConnection(factory);
}

PeerConnectionClient::~PeerConnectionClient() {
  Disconnect();
}

scoped_refptr<PeerConnectionFactoryInterface>
PeerConnectionClient::CreateDefaultFactory(
    Thread* signaling_thread,
    std::unique_ptr<FieldTrialsView> field_trials) {
  auto factory = CreatePeerConnectionFactory(
      /*network_thread=*/nullptr, /*worker_thread=*/nullptr,
      /*signaling_thread*/ signaling_thread,
      /*default_adm=*/nullptr, CreateBuiltinAudioEncoderFactory(),
      CreateBuiltinAudioDecoderFactory(),
      std::make_unique<VideoEncoderFactoryTemplate<
          LibvpxVp8EncoderTemplateAdapter, LibvpxVp9EncoderTemplateAdapter,
          OpenH264EncoderTemplateAdapter, LibaomAv1EncoderTemplateAdapter>>(),
      std::make_unique<VideoDecoderFactoryTemplate<
          LibvpxVp8DecoderTemplateAdapter, LibvpxVp9DecoderTemplateAdapter,
          OpenH264DecoderTemplateAdapter, Dav1dDecoderTemplateAdapter>>(),
      /*audio_mixer=*/nullptr, /*audio_processing=*/nullptr,
      /*audio_frame_processor=*/nullptr, std::move(field_trials));

  if (!factory) {
    RTC_LOG(LS_ERROR) << "Failed to initialize PeerConnectionFactory";
    return nullptr;
  }

  return factory;
}

bool PeerConnectionClient::InitializePeerConnection(
    PeerConnectionFactoryInterface* factory) {
  RTC_CHECK(factory)
      << "Must call InitializeFactory before InitializePeerConnection";

  PeerConnectionInterface::RTCConfiguration config;
  PeerConnectionInterface::IceServer server;
  server.urls.push_back(kStunServer);
  config.servers.push_back(server);
  config.sdp_semantics = SdpSemantics::kUnifiedPlan;

  PeerConnectionDependencies dependencies(this);
  auto result =
      factory->CreatePeerConnectionOrError(config, std::move(dependencies));

  if (!result.ok()) {
    RTC_LOG(LS_ERROR) << "Failed to create PeerConnection: "
                      << result.error().message();
    DeletePeerConnection();
    return false;
  }
  peer_connection_ = result.MoveValue();
  RTC_LOG(LS_INFO) << "PeerConnection created successfully";
  return true;
}

bool PeerConnectionClient::StartPeerConnection() {
  RTC_LOG(LS_INFO) << "Creating offer";

  peer_connection_->SetLocalDescription(
      SetLocalDescriptionObserverAdapter::Create([this](RTCError error) {
        if (error.ok())
          signaling_->SendDescription(peer_connection_->local_description());
      }));

  return true;
}

bool PeerConnectionClient::IsConnected() {
  return peer_connection_->peer_connection_state() ==
         PeerConnectionInterface::PeerConnectionState::kConnected;
}

// Disconnect from the call.
void PeerConnectionClient::Disconnect() {
  for (auto& data_channel : data_channels_) {
    data_channel->Close();
    data_channel.release();
  }
  data_channels_.clear();
  DeletePeerConnection();
}

// Delete the WebRTC PeerConnection.
void PeerConnectionClient::DeletePeerConnection() {
  RTC_LOG(LS_INFO);

  if (peer_connection_) {
    peer_connection_->Close();
  }
  peer_connection_.release();
}

void PeerConnectionClient::OnIceConnectionChange(
    PeerConnectionInterface::IceConnectionState new_state) {
  if (new_state ==
      PeerConnectionInterface::IceConnectionState::kIceConnectionCompleted) {
    RTC_LOG(LS_INFO) << "State is updating to connected";
  } else if (new_state == PeerConnectionInterface::IceConnectionState::
                              kIceConnectionDisconnected) {
    RTC_LOG(LS_INFO) << "Disconnecting from peer";
    Disconnect();
  }
}

void PeerConnectionClient::OnIceGatheringChange(
    PeerConnectionInterface::IceGatheringState new_state) {
  if (new_state == PeerConnectionInterface::kIceGatheringComplete) {
    RTC_LOG(LS_INFO) << "Client is ready to receive remote SDP";
  }
}

void PeerConnectionClient::OnIceCandidate(const IceCandidate* candidate) {
  signaling_->SendIceCandidate(candidate);
}

void PeerConnectionClient::OnDataChannel(
    scoped_refptr<DataChannelInterface> channel) {
  RTC_LOG(LS_INFO) << __FUNCTION__ << " remote datachannel created";
  if (on_data_channel_callback_)
    on_data_channel_callback_(channel);
  data_channels_.push_back(channel);
}

void PeerConnectionClient::SetOnDataChannel(
    std::function<void(scoped_refptr<DataChannelInterface>)> callback) {
  on_data_channel_callback_ = callback;
}

void PeerConnectionClient::OnNegotiationNeededEvent(uint32_t event_id) {
  RTC_LOG(LS_INFO) << "OnNegotiationNeededEvent";

  peer_connection_->SetLocalDescription(
      SetLocalDescriptionObserverAdapter::Create([this](RTCError error) {
        if (error.ok())
          signaling_->SendDescription(peer_connection_->local_description());
      }));
}

bool PeerConnectionClient::SetRemoteDescription(
    std::unique_ptr<SessionDescriptionInterface> desc) {
  RTC_LOG(LS_INFO) << "SetRemoteDescription";
  auto type = desc->GetType();

  peer_connection_->SetRemoteDescription(
      std::move(desc),
      SetRemoteDescriptionObserverAdapter::Create([&](RTCError) {
        RTC_LOG(LS_INFO) << "SetRemoteDescription done";

        if (type == SdpType::kOffer) {
          // Got an offer from the remote, need to set an answer and send it.
          peer_connection_->SetLocalDescription(
              SetLocalDescriptionObserverAdapter::Create(
                  [this](RTCError error) {
                    if (error.ok())
                      signaling_->SendDescription(
                          peer_connection_->local_description());
                  }));
        }
      }));

  return true;
}

void PeerConnectionClient::AddIceCandidate(
    std::unique_ptr<IceCandidate> candidate) {
  RTC_LOG(LS_INFO) << "AddIceCandidate";

  peer_connection_->AddIceCandidate(
      std::move(candidate), [](const RTCError& error) {
        RTC_LOG(LS_INFO) << "Failed to add candidate: " << error.message();
      });
}

}  // namespace webrtc
