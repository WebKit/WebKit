/*
 *  Copyright 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/peerconnectionfactory.h"

#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "api/fec_controller.h"
#include "api/media_transport_interface.h"
#include "api/mediaconstraintsinterface.h"
#include "api/mediastreamproxy.h"
#include "api/mediastreamtrackproxy.h"
#include "api/peerconnectionfactoryproxy.h"
#include "api/peerconnectionproxy.h"
#include "api/turncustomizer.h"
#include "api/videosourceproxy.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "media/base/rtpdataengine.h"
#include "media/sctp/sctptransport.h"
#include "pc/rtpparametersconversion.h"
#include "rtc_base/bind.h"
#include "rtc_base/checks.h"
// Adding 'nogncheck' to disable the gn include headers check to support modular
// WebRTC build targets.
// TODO(zhihuang): This wouldn't be necessary if the interface and
// implementation of the media engine were in separate build targets.
#include "media/engine/webrtcmediaengine.h"             // nogncheck
#include "media/engine/webrtcvideodecoderfactory.h"     // nogncheck
#include "media/engine/webrtcvideoencoderfactory.h"     // nogncheck
#include "modules/audio_device/include/audio_device.h"  // nogncheck
#include "modules/congestion_controller/bbr/bbr_factory.h"
#include "p2p/base/basicpacketsocketfactory.h"
#include "p2p/client/basicportallocator.h"
#include "pc/audiotrack.h"
#include "pc/localaudiosource.h"
#include "pc/mediastream.h"
#include "pc/peerconnection.h"
#include "pc/videocapturertracksource.h"
#include "pc/videotrack.h"
#include "rtc_base/experiments/congestion_controller_experiment.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {

rtc::scoped_refptr<PeerConnectionFactoryInterface>
CreateModularPeerConnectionFactory(
    rtc::Thread* network_thread,
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread,
    std::unique_ptr<cricket::MediaEngineInterface> media_engine,
    std::unique_ptr<CallFactoryInterface> call_factory,
    std::unique_ptr<RtcEventLogFactoryInterface> event_log_factory) {
  PeerConnectionFactoryDependencies dependencies;
  dependencies.network_thread = network_thread;
  dependencies.worker_thread = worker_thread;
  dependencies.signaling_thread = signaling_thread;
  dependencies.media_engine = std::move(media_engine);
  dependencies.call_factory = std::move(call_factory);
  dependencies.event_log_factory = std::move(event_log_factory);
  return CreateModularPeerConnectionFactory(std::move(dependencies));
}

rtc::scoped_refptr<PeerConnectionFactoryInterface>
CreateModularPeerConnectionFactory(
    rtc::Thread* network_thread,
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread,
    std::unique_ptr<cricket::MediaEngineInterface> media_engine,
    std::unique_ptr<CallFactoryInterface> call_factory,
    std::unique_ptr<RtcEventLogFactoryInterface> event_log_factory,
    std::unique_ptr<FecControllerFactoryInterface> fec_controller_factory,
    std::unique_ptr<NetworkControllerFactoryInterface>
        network_controller_factory) {
  PeerConnectionFactoryDependencies dependencies;
  dependencies.network_thread = network_thread;
  dependencies.worker_thread = worker_thread;
  dependencies.signaling_thread = signaling_thread;
  dependencies.media_engine = std::move(media_engine);
  dependencies.call_factory = std::move(call_factory);
  dependencies.event_log_factory = std::move(event_log_factory);
  dependencies.fec_controller_factory = std::move(fec_controller_factory);
  dependencies.network_controller_factory =
      std::move(network_controller_factory);
  return CreateModularPeerConnectionFactory(std::move(dependencies));
}

rtc::scoped_refptr<PeerConnectionFactoryInterface>
CreateModularPeerConnectionFactory(
    PeerConnectionFactoryDependencies dependencies) {
  rtc::scoped_refptr<PeerConnectionFactory> pc_factory(
      new rtc::RefCountedObject<PeerConnectionFactory>(
          std::move(dependencies)));
  // Call Initialize synchronously but make sure it is executed on
  // |signaling_thread|.
  MethodCall0<PeerConnectionFactory, bool> call(
      pc_factory.get(), &PeerConnectionFactory::Initialize);
  bool result = call.Marshal(RTC_FROM_HERE, pc_factory->signaling_thread());

  if (!result) {
    return nullptr;
  }
  return PeerConnectionFactoryProxy::Create(pc_factory->signaling_thread(),
                                            pc_factory);
}

PeerConnectionFactory::PeerConnectionFactory(
    rtc::Thread* network_thread,
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread,
    std::unique_ptr<cricket::MediaEngineInterface> media_engine,
    std::unique_ptr<webrtc::CallFactoryInterface> call_factory,
    std::unique_ptr<RtcEventLogFactoryInterface> event_log_factory)
    : PeerConnectionFactory(network_thread,
                            worker_thread,
                            signaling_thread,
                            std::move(media_engine),
                            std::move(call_factory),
                            std::move(event_log_factory),
                            nullptr,
                            nullptr) {}

PeerConnectionFactory::PeerConnectionFactory(
    rtc::Thread* network_thread,
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread,
    std::unique_ptr<cricket::MediaEngineInterface> media_engine,
    std::unique_ptr<webrtc::CallFactoryInterface> call_factory,
    std::unique_ptr<RtcEventLogFactoryInterface> event_log_factory,
    std::unique_ptr<FecControllerFactoryInterface> fec_controller_factory,
    std::unique_ptr<NetworkControllerFactoryInterface>
        network_controller_factory)
    : wraps_current_thread_(false),
      network_thread_(network_thread),
      worker_thread_(worker_thread),
      signaling_thread_(signaling_thread),
      media_engine_(std::move(media_engine)),
      call_factory_(std::move(call_factory)),
      event_log_factory_(std::move(event_log_factory)),
      fec_controller_factory_(std::move(fec_controller_factory)),
      injected_network_controller_factory_(
          std::move(network_controller_factory)),
      bbr_network_controller_factory_(
          absl::make_unique<BbrNetworkControllerFactory>()) {
  if (!network_thread_) {
    owned_network_thread_ = rtc::Thread::CreateWithSocketServer();
    owned_network_thread_->SetName("pc_network_thread", nullptr);
    owned_network_thread_->Start();
    network_thread_ = owned_network_thread_.get();
  }

  if (!worker_thread_) {
    owned_worker_thread_ = rtc::Thread::Create();
    owned_worker_thread_->SetName("pc_worker_thread", nullptr);
    owned_worker_thread_->Start();
    worker_thread_ = owned_worker_thread_.get();
  }

  if (!signaling_thread_) {
    signaling_thread_ = rtc::Thread::Current();
    if (!signaling_thread_) {
      // If this thread isn't already wrapped by an rtc::Thread, create a
      // wrapper and own it in this class.
      signaling_thread_ = rtc::ThreadManager::Instance()->WrapCurrentThread();
      wraps_current_thread_ = true;
    }
  }

  // TODO(deadbeef): Currently there is no way to create an external adm in
  // libjingle source tree. So we can 't currently assert if this is NULL.
  // RTC_DCHECK(default_adm != NULL);
}

PeerConnectionFactory::PeerConnectionFactory(
    PeerConnectionFactoryDependencies dependencies)
    : PeerConnectionFactory(
          dependencies.network_thread,
          dependencies.worker_thread,
          dependencies.signaling_thread,
          std::move(dependencies.media_engine),
          std::move(dependencies.call_factory),
          std::move(dependencies.event_log_factory),
          std::move(dependencies.fec_controller_factory),
          std::move(dependencies.network_controller_factory)) {
  media_transport_factory_ = std::move(dependencies.media_transport_factory);
}

PeerConnectionFactory::~PeerConnectionFactory() {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  channel_manager_.reset(nullptr);

  // Make sure |worker_thread_| and |signaling_thread_| outlive
  // |default_socket_factory_| and |default_network_manager_|.
  default_socket_factory_ = nullptr;
  default_network_manager_ = nullptr;

  if (wraps_current_thread_)
    rtc::ThreadManager::Instance()->UnwrapCurrentThread();
}

bool PeerConnectionFactory::Initialize() {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  rtc::InitRandom(rtc::Time32());

  default_network_manager_.reset(new rtc::BasicNetworkManager());
  if (!default_network_manager_) {
    return false;
  }

  default_socket_factory_.reset(
      new rtc::BasicPacketSocketFactory(network_thread_));
  if (!default_socket_factory_) {
    return false;
  }

  channel_manager_ = absl::make_unique<cricket::ChannelManager>(
      std::move(media_engine_), absl::make_unique<cricket::RtpDataEngine>(),
      worker_thread_, network_thread_);

  channel_manager_->SetVideoRtxEnabled(true);
  if (!channel_manager_->Init()) {
    return false;
  }

  return true;
}

void PeerConnectionFactory::SetOptions(const Options& options) {
  options_ = options;
}

RtpCapabilities PeerConnectionFactory::GetRtpSenderCapabilities(
    cricket::MediaType kind) const {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  switch (kind) {
    case cricket::MEDIA_TYPE_AUDIO: {
      cricket::AudioCodecs cricket_codecs;
      cricket::RtpHeaderExtensions cricket_extensions;
      channel_manager_->GetSupportedAudioSendCodecs(&cricket_codecs);
      channel_manager_->GetSupportedAudioRtpHeaderExtensions(
          &cricket_extensions);
      return ToRtpCapabilities(cricket_codecs, cricket_extensions);
    }
    case cricket::MEDIA_TYPE_VIDEO: {
      cricket::VideoCodecs cricket_codecs;
      cricket::RtpHeaderExtensions cricket_extensions;
      channel_manager_->GetSupportedVideoCodecs(&cricket_codecs);
      channel_manager_->GetSupportedVideoRtpHeaderExtensions(
          &cricket_extensions);
      return ToRtpCapabilities(cricket_codecs, cricket_extensions);
    }
    case cricket::MEDIA_TYPE_DATA:
      return RtpCapabilities();
  }
  // Not reached; avoids compile warning.
  RTC_FATAL();
}

RtpCapabilities PeerConnectionFactory::GetRtpReceiverCapabilities(
    cricket::MediaType kind) const {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  switch (kind) {
    case cricket::MEDIA_TYPE_AUDIO: {
      cricket::AudioCodecs cricket_codecs;
      cricket::RtpHeaderExtensions cricket_extensions;
      channel_manager_->GetSupportedAudioReceiveCodecs(&cricket_codecs);
      channel_manager_->GetSupportedAudioRtpHeaderExtensions(
          &cricket_extensions);
      return ToRtpCapabilities(cricket_codecs, cricket_extensions);
    }
    case cricket::MEDIA_TYPE_VIDEO: {
      cricket::VideoCodecs cricket_codecs;
      cricket::RtpHeaderExtensions cricket_extensions;
      channel_manager_->GetSupportedVideoCodecs(&cricket_codecs);
      channel_manager_->GetSupportedVideoRtpHeaderExtensions(
          &cricket_extensions);
      return ToRtpCapabilities(cricket_codecs, cricket_extensions);
    }
    case cricket::MEDIA_TYPE_DATA:
      return RtpCapabilities();
  }
  // Not reached; avoids compile warning.
  RTC_FATAL();
}

rtc::scoped_refptr<AudioSourceInterface>
PeerConnectionFactory::CreateAudioSource(const cricket::AudioOptions& options) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  rtc::scoped_refptr<LocalAudioSource> source(
      LocalAudioSource::Create(&options));
  return source;
}

rtc::scoped_refptr<VideoTrackSourceInterface>
PeerConnectionFactory::CreateVideoSource(
    std::unique_ptr<cricket::VideoCapturer> capturer,
    const MediaConstraintsInterface* constraints) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  rtc::scoped_refptr<VideoTrackSourceInterface> source(
      VideoCapturerTrackSource::Create(worker_thread_, std::move(capturer),
                                       constraints, false));
  return VideoTrackSourceProxy::Create(signaling_thread_, worker_thread_,
                                       source);
}

rtc::scoped_refptr<VideoTrackSourceInterface>
PeerConnectionFactory::CreateVideoSource(
    std::unique_ptr<cricket::VideoCapturer> capturer) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  rtc::scoped_refptr<VideoTrackSourceInterface> source(
      VideoCapturerTrackSource::Create(worker_thread_, std::move(capturer),
                                       false));
  return VideoTrackSourceProxy::Create(signaling_thread_, worker_thread_,
                                       source);
}

bool PeerConnectionFactory::StartAecDump(rtc::PlatformFile file,
                                         int64_t max_size_bytes) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  return channel_manager_->StartAecDump(file, max_size_bytes);
}

void PeerConnectionFactory::StopAecDump() {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  channel_manager_->StopAecDump();
}

rtc::scoped_refptr<PeerConnectionInterface>
PeerConnectionFactory::CreatePeerConnection(
    const PeerConnectionInterface::RTCConfiguration& configuration,
    std::unique_ptr<cricket::PortAllocator> allocator,
    std::unique_ptr<rtc::RTCCertificateGeneratorInterface> cert_generator,
    PeerConnectionObserver* observer) {
  // Convert the legacy API into the new depnedency structure.
  PeerConnectionDependencies dependencies(observer);
  dependencies.allocator = std::move(allocator);
  dependencies.cert_generator = std::move(cert_generator);
  // Pass that into the new API.
  return CreatePeerConnection(configuration, std::move(dependencies));
}

rtc::scoped_refptr<PeerConnectionInterface>
PeerConnectionFactory::CreatePeerConnection(
    const PeerConnectionInterface::RTCConfiguration& configuration,
    PeerConnectionDependencies dependencies) {
  RTC_DCHECK(signaling_thread_->IsCurrent());

  // Set internal defaults if optional dependencies are not set.
  if (!dependencies.cert_generator) {
    dependencies.cert_generator =
        absl::make_unique<rtc::RTCCertificateGenerator>(signaling_thread_,
                                                        network_thread_);
  }
  if (!dependencies.allocator) {
    network_thread_->Invoke<void>(RTC_FROM_HERE, [this, &configuration,
                                                  &dependencies]() {
      dependencies.allocator = absl::make_unique<cricket::BasicPortAllocator>(
          default_network_manager_.get(), default_socket_factory_.get(),
          configuration.turn_customizer);
    });
  }

  // TODO(zstein): Once chromium injects its own AsyncResolverFactory, set
  // |dependencies.async_resolver_factory| to a new
  // |rtc::BasicAsyncResolverFactory| if no factory is provided.

  network_thread_->Invoke<void>(
      RTC_FROM_HERE,
      rtc::Bind(&cricket::PortAllocator::SetNetworkIgnoreMask,
                dependencies.allocator.get(), options_.network_ignore_mask));

  std::unique_ptr<RtcEventLog> event_log =
      worker_thread_->Invoke<std::unique_ptr<RtcEventLog>>(
          RTC_FROM_HERE,
          rtc::Bind(&PeerConnectionFactory::CreateRtcEventLog_w, this));

  std::unique_ptr<Call> call = worker_thread_->Invoke<std::unique_ptr<Call>>(
      RTC_FROM_HERE,
      rtc::Bind(&PeerConnectionFactory::CreateCall_w, this, event_log.get()));

  rtc::scoped_refptr<PeerConnection> pc(
      new rtc::RefCountedObject<PeerConnection>(this, std::move(event_log),
                                                std::move(call)));
  ActionsBeforeInitializeForTesting(pc);
  if (!pc->Initialize(configuration, std::move(dependencies))) {
    return nullptr;
  }
  return PeerConnectionProxy::Create(signaling_thread(), pc);
}

rtc::scoped_refptr<MediaStreamInterface>
PeerConnectionFactory::CreateLocalMediaStream(const std::string& stream_id) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  return MediaStreamProxy::Create(signaling_thread_,
                                  MediaStream::Create(stream_id));
}

rtc::scoped_refptr<VideoTrackInterface> PeerConnectionFactory::CreateVideoTrack(
    const std::string& id,
    VideoTrackSourceInterface* source) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  rtc::scoped_refptr<VideoTrackInterface> track(
      VideoTrack::Create(id, source, worker_thread_));
  return VideoTrackProxy::Create(signaling_thread_, worker_thread_, track);
}

rtc::scoped_refptr<AudioTrackInterface> PeerConnectionFactory::CreateAudioTrack(
    const std::string& id,
    AudioSourceInterface* source) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  rtc::scoped_refptr<AudioTrackInterface> track(AudioTrack::Create(id, source));
  return AudioTrackProxy::Create(signaling_thread_, track);
}

std::unique_ptr<cricket::SctpTransportInternalFactory>
PeerConnectionFactory::CreateSctpTransportInternalFactory() {
#ifdef HAVE_SCTP
  return absl::make_unique<cricket::SctpTransportFactory>(network_thread());
#else
  return nullptr;
#endif
}

cricket::ChannelManager* PeerConnectionFactory::channel_manager() {
  return channel_manager_.get();
}

rtc::Thread* PeerConnectionFactory::signaling_thread() {
  // This method can be called on a different thread when the factory is
  // created in CreatePeerConnectionFactory().
  return signaling_thread_;
}

rtc::Thread* PeerConnectionFactory::worker_thread() {
  return worker_thread_;
}

rtc::Thread* PeerConnectionFactory::network_thread() {
  return network_thread_;
}

std::unique_ptr<RtcEventLog> PeerConnectionFactory::CreateRtcEventLog_w() {
  RTC_DCHECK_RUN_ON(worker_thread_);

  auto encoding_type = RtcEventLog::EncodingType::Legacy;
  if (field_trial::IsEnabled("WebRTC-RtcEventLogNewFormat"))
    encoding_type = RtcEventLog::EncodingType::NewFormat;
  return event_log_factory_
             ? event_log_factory_->CreateRtcEventLog(encoding_type)
             : absl::make_unique<RtcEventLogNullImpl>();
}

std::unique_ptr<Call> PeerConnectionFactory::CreateCall_w(
    RtcEventLog* event_log) {
  RTC_DCHECK_RUN_ON(worker_thread_);

  const int kMinBandwidthBps = 30000;
  const int kStartBandwidthBps = 300000;
  const int kMaxBandwidthBps = 2000000;

  webrtc::Call::Config call_config(event_log);
  if (!channel_manager_->media_engine() || !call_factory_) {
    return nullptr;
  }
  call_config.audio_state =
      channel_manager_->media_engine()->voice().GetAudioState();
  call_config.bitrate_config.min_bitrate_bps = kMinBandwidthBps;
  call_config.bitrate_config.start_bitrate_bps = kStartBandwidthBps;
  call_config.bitrate_config.max_bitrate_bps = kMaxBandwidthBps;

  call_config.fec_controller_factory = fec_controller_factory_.get();

  if (CongestionControllerExperiment::BbrControllerEnabled()) {
    RTC_LOG(LS_INFO) << "Using BBR network controller factory";
    call_config.network_controller_factory =
        bbr_network_controller_factory_.get();
  } else if (CongestionControllerExperiment::InjectedControllerEnabled()) {
    RTC_LOG(LS_INFO) << "Using injected network controller factory";
    call_config.network_controller_factory =
        injected_network_controller_factory_.get();
  } else {
    RTC_LOG(LS_INFO) << "Using default network controller factory";
  }

  return std::unique_ptr<Call>(call_factory_->CreateCall(call_config));
}

}  // namespace webrtc
