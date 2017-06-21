/*
 *  Copyright 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/pc/peerconnectionfactory.h"

#include <utility>

#include "webrtc/api/mediaconstraintsinterface.h"
#include "webrtc/api/mediastreamproxy.h"
#include "webrtc/api/mediastreamtrackproxy.h"
#include "webrtc/api/peerconnectionfactoryproxy.h"
#include "webrtc/api/peerconnectionproxy.h"
#include "webrtc/api/videosourceproxy.h"
#include "webrtc/base/bind.h"
#include "webrtc/base/checks.h"
#include "webrtc/logging/rtc_event_log/rtc_event_log.h"
// Adding 'nogncheck' to disable the gn include headers check to support modular
// WebRTC build targets.
// TODO(zhihuang): This wouldn't be necessary if the interface and
// implementation of the media engine were in separate build targets.
#include "webrtc/media/engine/webrtcmediaengine.h"             // nogncheck
#include "webrtc/media/engine/webrtcvideodecoderfactory.h"     // nogncheck
#include "webrtc/media/engine/webrtcvideoencoderfactory.h"     // nogncheck
#include "webrtc/modules/audio_device/include/audio_device.h"  // nogncheck
#include "webrtc/p2p/base/basicpacketsocketfactory.h"
#include "webrtc/p2p/client/basicportallocator.h"
#include "webrtc/pc/audiotrack.h"
#include "webrtc/pc/localaudiosource.h"
#include "webrtc/pc/mediastream.h"
#include "webrtc/pc/peerconnection.h"
#include "webrtc/pc/videocapturertracksource.h"
#include "webrtc/pc/videotrack.h"

namespace webrtc {

rtc::scoped_refptr<PeerConnectionFactoryInterface>
CreateModularPeerConnectionFactory(
    rtc::Thread* network_thread,
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread,
    AudioDeviceModule* default_adm,
    rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
    rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory,
    cricket::WebRtcVideoEncoderFactory* video_encoder_factory,
    cricket::WebRtcVideoDecoderFactory* video_decoder_factory,
    rtc::scoped_refptr<AudioMixer> audio_mixer,
    std::unique_ptr<cricket::MediaEngineInterface> media_engine,
    std::unique_ptr<CallFactoryInterface> call_factory,
    std::unique_ptr<RtcEventLogFactoryInterface> event_log_factory) {
  rtc::scoped_refptr<PeerConnectionFactory> pc_factory(
      new rtc::RefCountedObject<PeerConnectionFactory>(
          network_thread, worker_thread, signaling_thread, default_adm,
          audio_encoder_factory, audio_decoder_factory, video_encoder_factory,
          video_decoder_factory, audio_mixer, std::move(media_engine),
          std::move(call_factory), std::move(event_log_factory)));

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
    AudioDeviceModule* default_adm,
    rtc::scoped_refptr<webrtc::AudioEncoderFactory> audio_encoder_factory,
    rtc::scoped_refptr<webrtc::AudioDecoderFactory> audio_decoder_factory,
    cricket::WebRtcVideoEncoderFactory* video_encoder_factory,
    cricket::WebRtcVideoDecoderFactory* video_decoder_factory,
    rtc::scoped_refptr<AudioMixer> audio_mixer,
    std::unique_ptr<cricket::MediaEngineInterface> media_engine,
    std::unique_ptr<webrtc::CallFactoryInterface> call_factory,
    std::unique_ptr<RtcEventLogFactoryInterface> event_log_factory)
    : wraps_current_thread_(false),
      network_thread_(network_thread),
      worker_thread_(worker_thread),
      signaling_thread_(signaling_thread),
      default_adm_(default_adm),
      audio_encoder_factory_(audio_encoder_factory),
      audio_decoder_factory_(audio_decoder_factory),
      video_encoder_factory_(video_encoder_factory),
      video_decoder_factory_(video_decoder_factory),
      external_audio_mixer_(audio_mixer),
      media_engine_(std::move(media_engine)),
      call_factory_(std::move(call_factory)),
      event_log_factory_(std::move(event_log_factory)) {
  if (!network_thread_) {
    owned_network_thread_ = rtc::Thread::CreateWithSocketServer();
    owned_network_thread_->Start();
    network_thread_ = owned_network_thread_.get();
  }

  if (!worker_thread_) {
    owned_worker_thread_ = rtc::Thread::Create();
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

  // TODO: Currently there is no way creating an external adm in
  // libjingle source tree. So we can 't currently assert if this is NULL.
  // RTC_DCHECK(default_adm != NULL);
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

  channel_manager_.reset(new cricket::ChannelManager(
      std::move(media_engine_), worker_thread_, network_thread_));

  channel_manager_->SetVideoRtxEnabled(true);
  if (!channel_manager_->Init()) {
    return false;
  }

  return true;
}

void PeerConnectionFactory::SetOptions(const Options& options) {
  options_ = options;
}

rtc::scoped_refptr<AudioSourceInterface>
PeerConnectionFactory::CreateAudioSource(
    const MediaConstraintsInterface* constraints) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  rtc::scoped_refptr<LocalAudioSource> source(
      LocalAudioSource::Create(constraints));
  return source;
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
    const PeerConnectionInterface::RTCConfiguration& configuration_in,
    const MediaConstraintsInterface* constraints,
    std::unique_ptr<cricket::PortAllocator> allocator,
    std::unique_ptr<rtc::RTCCertificateGeneratorInterface> cert_generator,
    PeerConnectionObserver* observer) {
  RTC_DCHECK(signaling_thread_->IsCurrent());

  // We merge constraints and configuration into a single configuration.
  PeerConnectionInterface::RTCConfiguration configuration = configuration_in;
  CopyConstraintsIntoRtcConfiguration(constraints, &configuration);

  return CreatePeerConnection(configuration, std::move(allocator),
                              std::move(cert_generator), observer);
}

rtc::scoped_refptr<PeerConnectionInterface>
PeerConnectionFactory::CreatePeerConnection(
    const PeerConnectionInterface::RTCConfiguration& configuration,
    std::unique_ptr<cricket::PortAllocator> allocator,
    std::unique_ptr<rtc::RTCCertificateGeneratorInterface> cert_generator,
    PeerConnectionObserver* observer) {
  RTC_DCHECK(signaling_thread_->IsCurrent());

  if (!cert_generator.get()) {
    // No certificate generator specified, use the default one.
    cert_generator.reset(
        new rtc::RTCCertificateGenerator(signaling_thread_, network_thread_));
  }

  if (!allocator) {
    allocator.reset(new cricket::BasicPortAllocator(
        default_network_manager_.get(), default_socket_factory_.get()));
  }
  network_thread_->Invoke<void>(
      RTC_FROM_HERE, rtc::Bind(&cricket::PortAllocator::SetNetworkIgnoreMask,
                               allocator.get(), options_.network_ignore_mask));

  std::unique_ptr<RtcEventLog> event_log(new RtcEventLogNullImpl());
  if (event_log_factory_) {
    event_log = event_log_factory_->CreateRtcEventLog();
  }

  std::unique_ptr<Call> call = worker_thread_->Invoke<std::unique_ptr<Call>>(
      RTC_FROM_HERE,
      rtc::Bind(&PeerConnectionFactory::CreateCall_w, this, event_log.get()));

  rtc::scoped_refptr<PeerConnection> pc(
      new rtc::RefCountedObject<PeerConnection>(this, std::move(event_log),
                                                std::move(call)));

  if (!pc->Initialize(configuration, std::move(allocator),
                      std::move(cert_generator), observer)) {
    return nullptr;
  }
  return PeerConnectionProxy::Create(signaling_thread(), pc);
}

rtc::scoped_refptr<MediaStreamInterface>
PeerConnectionFactory::CreateLocalMediaStream(const std::string& label) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  return MediaStreamProxy::Create(signaling_thread_,
                                  MediaStream::Create(label));
}

rtc::scoped_refptr<VideoTrackInterface> PeerConnectionFactory::CreateVideoTrack(
    const std::string& id,
    VideoTrackSourceInterface* source) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  rtc::scoped_refptr<VideoTrackInterface> track(
      VideoTrack::Create(id, source));
  return VideoTrackProxy::Create(signaling_thread_, worker_thread_, track);
}

rtc::scoped_refptr<AudioTrackInterface>
PeerConnectionFactory::CreateAudioTrack(const std::string& id,
                                        AudioSourceInterface* source) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  rtc::scoped_refptr<AudioTrackInterface> track(AudioTrack::Create(id, source));
  return AudioTrackProxy::Create(signaling_thread_, track);
}

cricket::TransportController* PeerConnectionFactory::CreateTransportController(
    cricket::PortAllocator* port_allocator,
    bool redetermine_role_on_ice_restart) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  return new cricket::TransportController(
      signaling_thread_, network_thread_, port_allocator,
      redetermine_role_on_ice_restart, options_.crypto_options);
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

std::unique_ptr<Call> PeerConnectionFactory::CreateCall_w(
    RtcEventLog* event_log) {
  const int kMinBandwidthBps = 30000;
  const int kStartBandwidthBps = 300000;
  const int kMaxBandwidthBps = 2000000;

  webrtc::Call::Config call_config(event_log);
  if (!channel_manager_->media_engine() || !call_factory_) {
    return nullptr;
  }
  call_config.audio_state = channel_manager_->media_engine()->GetAudioState();
  call_config.bitrate_config.min_bitrate_bps = kMinBandwidthBps;
  call_config.bitrate_config.start_bitrate_bps = kStartBandwidthBps;
  call_config.bitrate_config.max_bitrate_bps = kMaxBandwidthBps;

  return std::unique_ptr<Call>(call_factory_->CreateCall(call_config));
}

}  // namespace webrtc
