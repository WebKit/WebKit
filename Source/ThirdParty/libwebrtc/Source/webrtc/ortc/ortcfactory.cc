/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/ortc/ortcfactory.h"

#include <sstream>
#include <utility>  // For std::move.
#include <vector>

#include "webrtc/api/audio_codecs/builtin_audio_decoder_factory.h"
#include "webrtc/api/audio_codecs/builtin_audio_encoder_factory.h"
#include "webrtc/api/mediastreamtrackproxy.h"
#include "webrtc/api/proxy.h"
#include "webrtc/api/rtcerror.h"
#include "webrtc/api/videosourceproxy.h"
#include "webrtc/base/asyncpacketsocket.h"
#include "webrtc/base/bind.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/helpers.h"
#include "webrtc/base/logging.h"
#include "webrtc/logging/rtc_event_log/rtc_event_log.h"
#include "webrtc/media/base/mediaconstants.h"
#include "webrtc/ortc/ortcrtpreceiveradapter.h"
#include "webrtc/ortc/ortcrtpsenderadapter.h"
#include "webrtc/ortc/rtpparametersconversion.h"
#include "webrtc/ortc/rtptransportadapter.h"
#include "webrtc/ortc/rtptransportcontrolleradapter.h"
#include "webrtc/p2p/base/basicpacketsocketfactory.h"
#include "webrtc/p2p/base/udptransport.h"
#include "webrtc/pc/audiotrack.h"
#include "webrtc/pc/channelmanager.h"
#include "webrtc/pc/localaudiosource.h"
#include "webrtc/pc/videocapturertracksource.h"
#include "webrtc/pc/videotrack.h"

namespace {

const int kDefaultRtcpCnameLength = 16;

// Asserts that all of the built-in capabilities can be converted to
// RtpCapabilities. If they can't, something's wrong (for example, maybe a new
// feedback mechanism is supported, but an enum value wasn't added to
// rtpparameters.h).
template <typename C>
webrtc::RtpCapabilities ToRtpCapabilitiesWithAsserts(
    const std::vector<C>& cricket_codecs,
    const cricket::RtpHeaderExtensions& cricket_extensions) {
  webrtc::RtpCapabilities capabilities =
      webrtc::ToRtpCapabilities(cricket_codecs, cricket_extensions);
  RTC_DCHECK_EQ(capabilities.codecs.size(), cricket_codecs.size());
  for (size_t i = 0; i < capabilities.codecs.size(); ++i) {
    RTC_DCHECK_EQ(capabilities.codecs[i].rtcp_feedback.size(),
                  cricket_codecs[i].feedback_params.params().size());
  }
  RTC_DCHECK_EQ(capabilities.header_extensions.size(),
                cricket_extensions.size());
  return capabilities;
}

}  // namespace

namespace webrtc {

// Note that this proxy class uses the network thread as the "worker" thread.
BEGIN_OWNED_PROXY_MAP(OrtcFactory)
PROXY_SIGNALING_THREAD_DESTRUCTOR()
PROXY_METHOD0(RTCErrorOr<std::unique_ptr<RtpTransportControllerInterface>>,
              CreateRtpTransportController)
PROXY_METHOD4(RTCErrorOr<std::unique_ptr<RtpTransportInterface>>,
              CreateRtpTransport,
              const RtcpParameters&,
              PacketTransportInterface*,
              PacketTransportInterface*,
              RtpTransportControllerInterface*)

PROXY_METHOD4(RTCErrorOr<std::unique_ptr<SrtpTransportInterface>>,
              CreateSrtpTransport,
              const RtcpParameters&,
              PacketTransportInterface*,
              PacketTransportInterface*,
              RtpTransportControllerInterface*)

PROXY_CONSTMETHOD1(RtpCapabilities,
                   GetRtpSenderCapabilities,
                   cricket::MediaType)
PROXY_METHOD2(RTCErrorOr<std::unique_ptr<OrtcRtpSenderInterface>>,
              CreateRtpSender,
              rtc::scoped_refptr<MediaStreamTrackInterface>,
              RtpTransportInterface*)
PROXY_METHOD2(RTCErrorOr<std::unique_ptr<OrtcRtpSenderInterface>>,
              CreateRtpSender,
              cricket::MediaType,
              RtpTransportInterface*)
PROXY_CONSTMETHOD1(RtpCapabilities,
                   GetRtpReceiverCapabilities,
                   cricket::MediaType)
PROXY_METHOD2(RTCErrorOr<std::unique_ptr<OrtcRtpReceiverInterface>>,
              CreateRtpReceiver,
              cricket::MediaType,
              RtpTransportInterface*)
PROXY_WORKER_METHOD3(RTCErrorOr<std::unique_ptr<UdpTransportInterface>>,
                     CreateUdpTransport,
                     int,
                     uint16_t,
                     uint16_t)
PROXY_METHOD1(rtc::scoped_refptr<AudioSourceInterface>,
              CreateAudioSource,
              const cricket::AudioOptions&)
PROXY_METHOD2(rtc::scoped_refptr<VideoTrackSourceInterface>,
              CreateVideoSource,
              std::unique_ptr<cricket::VideoCapturer>,
              const MediaConstraintsInterface*)
PROXY_METHOD2(rtc::scoped_refptr<VideoTrackInterface>,
              CreateVideoTrack,
              const std::string&,
              VideoTrackSourceInterface*)
PROXY_METHOD2(rtc::scoped_refptr<AudioTrackInterface>,
              CreateAudioTrack,
              const std::string&,
              AudioSourceInterface*)
END_PROXY_MAP()

// static
RTCErrorOr<std::unique_ptr<OrtcFactoryInterface>> OrtcFactory::Create(
    rtc::Thread* network_thread,
    rtc::Thread* signaling_thread,
    rtc::NetworkManager* network_manager,
    rtc::PacketSocketFactory* socket_factory,
    AudioDeviceModule* adm,
    std::unique_ptr<cricket::MediaEngineInterface> media_engine) {
  // Hop to signaling thread if needed.
  if (signaling_thread && !signaling_thread->IsCurrent()) {
    return signaling_thread
        ->Invoke<RTCErrorOr<std::unique_ptr<OrtcFactoryInterface>>>(
            RTC_FROM_HERE,
            rtc::Bind(&OrtcFactory::Create_s, network_thread, signaling_thread,
                      network_manager, socket_factory, adm,
                      media_engine.release()));
  }
  return Create_s(network_thread, signaling_thread, network_manager,
                  socket_factory, adm, media_engine.release());
}

RTCErrorOr<std::unique_ptr<OrtcFactoryInterface>> OrtcFactoryInterface::Create(
    rtc::Thread* network_thread,
    rtc::Thread* signaling_thread,
    rtc::NetworkManager* network_manager,
    rtc::PacketSocketFactory* socket_factory,
    AudioDeviceModule* adm) {
  return OrtcFactory::Create(network_thread, signaling_thread, network_manager,
                             socket_factory, adm, nullptr);
}

OrtcFactory::OrtcFactory(rtc::Thread* network_thread,
                         rtc::Thread* signaling_thread,
                         rtc::NetworkManager* network_manager,
                         rtc::PacketSocketFactory* socket_factory,
                         AudioDeviceModule* adm)
    : network_thread_(network_thread),
      signaling_thread_(signaling_thread),
      network_manager_(network_manager),
      socket_factory_(socket_factory),
      adm_(adm),
      null_event_log_(RtcEventLog::CreateNull()),
      audio_encoder_factory_(CreateBuiltinAudioEncoderFactory()),
      audio_decoder_factory_(CreateBuiltinAudioDecoderFactory()) {
  if (!rtc::CreateRandomString(kDefaultRtcpCnameLength, &default_cname_)) {
    LOG(LS_ERROR) << "Failed to generate CNAME?";
    RTC_NOTREACHED();
  }
  if (!network_thread_) {
    owned_network_thread_ = rtc::Thread::CreateWithSocketServer();
    owned_network_thread_->Start();
    network_thread_ = owned_network_thread_.get();
  }

  // The worker thread is created internally because it's an implementation
  // detail, and consumers of the API don't need to really know about it.
  worker_thread_ = rtc::Thread::Create();
  worker_thread_->Start();

  if (signaling_thread_) {
    RTC_DCHECK_RUN_ON(signaling_thread_);
  } else {
    signaling_thread_ = rtc::Thread::Current();
    if (!signaling_thread_) {
      // If this thread isn't already wrapped by an rtc::Thread, create a
      // wrapper and own it in this class.
      signaling_thread_ = rtc::ThreadManager::Instance()->WrapCurrentThread();
      wraps_signaling_thread_ = true;
    }
  }
  if (!network_manager_) {
    owned_network_manager_.reset(new rtc::BasicNetworkManager());
    network_manager_ = owned_network_manager_.get();
  }
  if (!socket_factory_) {
    owned_socket_factory_.reset(
        new rtc::BasicPacketSocketFactory(network_thread_));
    socket_factory_ = owned_socket_factory_.get();
  }
}

OrtcFactory::~OrtcFactory() {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  if (wraps_signaling_thread_) {
    rtc::ThreadManager::Instance()->UnwrapCurrentThread();
  }
}

RTCErrorOr<std::unique_ptr<RtpTransportControllerInterface>>
OrtcFactory::CreateRtpTransportController() {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  return RtpTransportControllerAdapter::CreateProxied(
      cricket::MediaConfig(), channel_manager_.get(), null_event_log_.get(),
      signaling_thread_, worker_thread_.get());
}

RTCErrorOr<std::unique_ptr<RtpTransportInterface>>
OrtcFactory::CreateRtpTransport(
    const RtcpParameters& rtcp_parameters,
    PacketTransportInterface* rtp,
    PacketTransportInterface* rtcp,
    RtpTransportControllerInterface* transport_controller) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  RtcpParameters copied_parameters = rtcp_parameters;
  if (copied_parameters.cname.empty()) {
    copied_parameters.cname = default_cname_;
  }
  if (transport_controller) {
    return transport_controller->GetInternal()->CreateProxiedRtpTransport(
        copied_parameters, rtp, rtcp);
  } else {
    // If |transport_controller| is null, create one automatically, which the
    // returned RtpTransport will own.
    auto controller_result = CreateRtpTransportController();
    if (!controller_result.ok()) {
      return controller_result.MoveError();
    }
    auto controller = controller_result.MoveValue();
    auto transport_result =
        controller->GetInternal()->CreateProxiedRtpTransport(copied_parameters,
                                                             rtp, rtcp);
    // If RtpTransport was successfully created, transfer ownership of
    // |rtp_transport_controller|. Otherwise it will go out of scope and be
    // deleted automatically.
    if (transport_result.ok()) {
      transport_result.value()
          ->GetInternal()
          ->TakeOwnershipOfRtpTransportController(std::move(controller));
    }
    return transport_result;
  }
}

RTCErrorOr<std::unique_ptr<SrtpTransportInterface>>
OrtcFactory::CreateSrtpTransport(
    const RtcpParameters& rtcp_parameters,
    PacketTransportInterface* rtp,
    PacketTransportInterface* rtcp,
    RtpTransportControllerInterface* transport_controller) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  RtcpParameters copied_parameters = rtcp_parameters;
  if (copied_parameters.cname.empty()) {
    copied_parameters.cname = default_cname_;
  }
  if (transport_controller) {
    return transport_controller->GetInternal()->CreateProxiedSrtpTransport(
        copied_parameters, rtp, rtcp);
  } else {
    // If |transport_controller| is null, create one automatically, which the
    // returned SrtpTransport will own.
    auto controller_result = CreateRtpTransportController();
    if (!controller_result.ok()) {
      return controller_result.MoveError();
    }
    auto controller = controller_result.MoveValue();
    auto transport_result =
        controller->GetInternal()->CreateProxiedSrtpTransport(copied_parameters,
                                                              rtp, rtcp);
    // If SrtpTransport was successfully created, transfer ownership of
    // |rtp_transport_controller|. Otherwise it will go out of scope and be
    // deleted automatically.
    if (transport_result.ok()) {
      transport_result.value()
          ->GetInternal()
          ->TakeOwnershipOfRtpTransportController(std::move(controller));
    }
    return transport_result;
  }
}

RtpCapabilities OrtcFactory::GetRtpSenderCapabilities(
    cricket::MediaType kind) const {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  switch (kind) {
    case cricket::MEDIA_TYPE_AUDIO: {
      cricket::AudioCodecs cricket_codecs;
      cricket::RtpHeaderExtensions cricket_extensions;
      channel_manager_->GetSupportedAudioSendCodecs(&cricket_codecs);
      channel_manager_->GetSupportedAudioRtpHeaderExtensions(
          &cricket_extensions);
      return ToRtpCapabilitiesWithAsserts(cricket_codecs, cricket_extensions);
    }
    case cricket::MEDIA_TYPE_VIDEO: {
      cricket::VideoCodecs cricket_codecs;
      cricket::RtpHeaderExtensions cricket_extensions;
      channel_manager_->GetSupportedVideoCodecs(&cricket_codecs);
      channel_manager_->GetSupportedVideoRtpHeaderExtensions(
          &cricket_extensions);
      return ToRtpCapabilitiesWithAsserts(cricket_codecs, cricket_extensions);
    }
    case cricket::MEDIA_TYPE_DATA:
      return RtpCapabilities();
  }
  // Not reached; avoids compile warning.
  FATAL();
}

RTCErrorOr<std::unique_ptr<OrtcRtpSenderInterface>>
OrtcFactory::CreateRtpSender(
    rtc::scoped_refptr<MediaStreamTrackInterface> track,
    RtpTransportInterface* transport) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  if (!track) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                         "Cannot pass null track into CreateRtpSender.");
  }
  auto result =
      CreateRtpSender(cricket::MediaTypeFromString(track->kind()), transport);
  if (!result.ok()) {
    return result;
  }
  auto err = result.value()->SetTrack(track);
  if (!err.ok()) {
    return std::move(err);
  }
  return result;
}

RTCErrorOr<std::unique_ptr<OrtcRtpSenderInterface>>
OrtcFactory::CreateRtpSender(cricket::MediaType kind,
                             RtpTransportInterface* transport) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  if (kind == cricket::MEDIA_TYPE_DATA) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                         "Cannot create data RtpSender.");
  }
  if (!transport) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                         "Cannot pass null transport into CreateRtpSender.");
  }
  return transport->GetInternal()
      ->rtp_transport_controller()
      ->CreateProxiedRtpSender(kind, transport);
}

RtpCapabilities OrtcFactory::GetRtpReceiverCapabilities(
    cricket::MediaType kind) const {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  switch (kind) {
    case cricket::MEDIA_TYPE_AUDIO: {
      cricket::AudioCodecs cricket_codecs;
      cricket::RtpHeaderExtensions cricket_extensions;
      channel_manager_->GetSupportedAudioReceiveCodecs(&cricket_codecs);
      channel_manager_->GetSupportedAudioRtpHeaderExtensions(
          &cricket_extensions);
      return ToRtpCapabilitiesWithAsserts(cricket_codecs, cricket_extensions);
    }
    case cricket::MEDIA_TYPE_VIDEO: {
      cricket::VideoCodecs cricket_codecs;
      cricket::RtpHeaderExtensions cricket_extensions;
      channel_manager_->GetSupportedVideoCodecs(&cricket_codecs);
      channel_manager_->GetSupportedVideoRtpHeaderExtensions(
          &cricket_extensions);
      return ToRtpCapabilitiesWithAsserts(cricket_codecs, cricket_extensions);
    }
    case cricket::MEDIA_TYPE_DATA:
      return RtpCapabilities();
  }
  // Not reached; avoids compile warning.
  FATAL();
}

RTCErrorOr<std::unique_ptr<OrtcRtpReceiverInterface>>
OrtcFactory::CreateRtpReceiver(cricket::MediaType kind,
                               RtpTransportInterface* transport) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  if (kind == cricket::MEDIA_TYPE_DATA) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                         "Cannot create data RtpReceiver.");
  }
  if (!transport) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                         "Cannot pass null transport into CreateRtpReceiver.");
  }
  return transport->GetInternal()
      ->rtp_transport_controller()
      ->CreateProxiedRtpReceiver(kind, transport);
}

// UdpTransport expects all methods to be called on one thread, which needs to
// be the network thread, since that's where its socket can safely be used. So
// return a proxy to the created UdpTransport.
BEGIN_OWNED_PROXY_MAP(UdpTransport)
PROXY_WORKER_THREAD_DESTRUCTOR()
PROXY_WORKER_CONSTMETHOD0(rtc::SocketAddress, GetLocalAddress)
PROXY_WORKER_METHOD1(bool, SetRemoteAddress, const rtc::SocketAddress&)
PROXY_WORKER_CONSTMETHOD0(rtc::SocketAddress, GetRemoteAddress)
protected:
rtc::PacketTransportInternal* GetInternal() override {
  return internal();
}
END_PROXY_MAP()

RTCErrorOr<std::unique_ptr<UdpTransportInterface>>
OrtcFactory::CreateUdpTransport(int family,
                                uint16_t min_port,
                                uint16_t max_port) {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (family != AF_INET && family != AF_INET6) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                         "Address family must be AF_INET or AF_INET6.");
  }
  if (min_port > max_port) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_RANGE,
                         "Port range invalid; minimum port must be less than "
                         "or equal to max port.");
  }
  std::unique_ptr<rtc::AsyncPacketSocket> socket(
      socket_factory_->CreateUdpSocket(
          rtc::SocketAddress(rtc::GetAnyIP(family), 0), min_port, max_port));
  if (!socket) {
    // Only log at warning level, because this method may be called with
    // specific port ranges to determine if a port is available, expecting the
    // possibility of an error.
    LOG_AND_RETURN_ERROR_EX(RTCErrorType::RESOURCE_EXHAUSTED,
                            "Local socket allocation failure.", LS_WARNING);
  }
  LOG(LS_INFO) << "Created UDP socket with address "
               << socket->GetLocalAddress().ToSensitiveString() << ".";
  // Make a unique debug name (for logging/diagnostics only).
  std::ostringstream oss;
  static int udp_id = 0;
  oss << "udp" << udp_id++;
  return UdpTransportProxyWithInternal<cricket::UdpTransport>::Create(
      signaling_thread_, network_thread_,
      std::unique_ptr<cricket::UdpTransport>(
          new cricket::UdpTransport(oss.str(), std::move(socket))));
}

rtc::scoped_refptr<AudioSourceInterface> OrtcFactory::CreateAudioSource(
    const cricket::AudioOptions& options) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  return rtc::scoped_refptr<LocalAudioSource>(
      LocalAudioSource::Create(&options));
}

rtc::scoped_refptr<VideoTrackSourceInterface> OrtcFactory::CreateVideoSource(
    std::unique_ptr<cricket::VideoCapturer> capturer,
    const MediaConstraintsInterface* constraints) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  rtc::scoped_refptr<VideoTrackSourceInterface> source(
      VideoCapturerTrackSource::Create(
          worker_thread_.get(), std::move(capturer), constraints, false));
  return VideoTrackSourceProxy::Create(signaling_thread_, worker_thread_.get(),
                                       source);
}

rtc::scoped_refptr<VideoTrackInterface> OrtcFactory::CreateVideoTrack(
    const std::string& id,
    VideoTrackSourceInterface* source) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  rtc::scoped_refptr<VideoTrackInterface> track(VideoTrack::Create(id, source));
  return VideoTrackProxy::Create(signaling_thread_, worker_thread_.get(),
                                 track);
}

rtc::scoped_refptr<AudioTrackInterface> OrtcFactory::CreateAudioTrack(
    const std::string& id,
    AudioSourceInterface* source) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  rtc::scoped_refptr<AudioTrackInterface> track(AudioTrack::Create(id, source));
  return AudioTrackProxy::Create(signaling_thread_, track);
}

// static
RTCErrorOr<std::unique_ptr<OrtcFactoryInterface>> OrtcFactory::Create_s(
    rtc::Thread* network_thread,
    rtc::Thread* signaling_thread,
    rtc::NetworkManager* network_manager,
    rtc::PacketSocketFactory* socket_factory,
    AudioDeviceModule* adm,
    cricket::MediaEngineInterface* media_engine) {
  // Add the unique_ptr wrapper back.
  std::unique_ptr<cricket::MediaEngineInterface> owned_media_engine(
      media_engine);
  std::unique_ptr<OrtcFactory> new_factory(new OrtcFactory(
      network_thread, signaling_thread, network_manager, socket_factory, adm));
  RTCError err = new_factory->Initialize(std::move(owned_media_engine));
  if (!err.ok()) {
    return std::move(err);
  }
  // Return a proxy so that any calls on the returned object (including
  // destructor) happen on the signaling thread.
  rtc::Thread* signaling = new_factory->signaling_thread();
  rtc::Thread* network = new_factory->network_thread();
  return OrtcFactoryProxy::Create(signaling, network, std::move(new_factory));
}

RTCError OrtcFactory::Initialize(
    std::unique_ptr<cricket::MediaEngineInterface> media_engine) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  // TODO(deadbeef): Get rid of requirement to hop to worker thread here.
  if (!media_engine) {
    media_engine =
        worker_thread_->Invoke<std::unique_ptr<cricket::MediaEngineInterface>>(
            RTC_FROM_HERE, rtc::Bind(&OrtcFactory::CreateMediaEngine_w, this));
  }

  channel_manager_.reset(new cricket::ChannelManager(
      std::move(media_engine), worker_thread_.get(), network_thread_));
  channel_manager_->SetVideoRtxEnabled(true);
  if (!channel_manager_->Init()) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR,
                         "Failed to initialize ChannelManager.");
  }
  return RTCError::OK();
}

std::unique_ptr<cricket::MediaEngineInterface>
OrtcFactory::CreateMediaEngine_w() {
  RTC_DCHECK_RUN_ON(worker_thread_.get());
  // The null arguments are optional factories that could be passed into the
  // OrtcFactory, but aren't yet.
  //
  // Note that |adm_| may be null, in which case the platform-specific default
  // AudioDeviceModule will be used.
  return std::unique_ptr<cricket::MediaEngineInterface>(
      cricket::WebRtcMediaEngineFactory::Create(adm_, audio_encoder_factory_,
                                                audio_decoder_factory_, nullptr,
                                                nullptr, nullptr));
}

}  // namespace webrtc
