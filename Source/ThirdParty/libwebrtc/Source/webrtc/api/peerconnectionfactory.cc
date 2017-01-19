/*
 *  Copyright 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/peerconnectionfactory.h"

#include <utility>

#include "webrtc/api/audiotrack.h"
#include "webrtc/api/localaudiosource.h"
#include "webrtc/api/mediaconstraintsinterface.h"
#include "webrtc/api/mediastream.h"
#include "webrtc/api/mediastreamproxy.h"
#include "webrtc/api/mediastreamtrackproxy.h"
#include "webrtc/api/peerconnection.h"
#include "webrtc/api/peerconnectionfactoryproxy.h"
#include "webrtc/api/peerconnectionproxy.h"
#include "webrtc/api/videocapturertracksource.h"
#include "webrtc/api/videosourceproxy.h"
#include "webrtc/api/videotrack.h"
#include "webrtc/base/bind.h"
#include "webrtc/media/engine/webrtcmediaengine.h"
#include "webrtc/media/engine/webrtcvideodecoderfactory.h"
#include "webrtc/media/engine/webrtcvideoencoderfactory.h"
#include "webrtc/modules/audio_coding/codecs/builtin_audio_decoder_factory.h"
#include "webrtc/modules/audio_device/include/audio_device.h"
#include "webrtc/p2p/base/basicpacketsocketfactory.h"
#include "webrtc/p2p/client/basicportallocator.h"

namespace webrtc {

rtc::scoped_refptr<PeerConnectionFactoryInterface>
CreatePeerConnectionFactory() {
  rtc::scoped_refptr<PeerConnectionFactory> pc_factory(
      new rtc::RefCountedObject<PeerConnectionFactory>());

  RTC_CHECK(rtc::Thread::Current() == pc_factory->signaling_thread());
  // The signaling thread is the current thread so we can
  // safely call Initialize directly.
  if (!pc_factory->Initialize()) {
    return nullptr;
  }
  return PeerConnectionFactoryProxy::Create(pc_factory->signaling_thread(),
                                            pc_factory);
}

rtc::scoped_refptr<PeerConnectionFactoryInterface> CreatePeerConnectionFactory(
    rtc::Thread* network_thread,
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread,
    AudioDeviceModule* default_adm,
    cricket::WebRtcVideoEncoderFactory* encoder_factory,
    cricket::WebRtcVideoDecoderFactory* decoder_factory) {
  rtc::scoped_refptr<PeerConnectionFactory> pc_factory(
      new rtc::RefCountedObject<PeerConnectionFactory>(
          network_thread,
          worker_thread,
          signaling_thread,
          default_adm,
          CreateBuiltinAudioDecoderFactory(),
          encoder_factory,
          decoder_factory));

  // Call Initialize synchronously but make sure its executed on
  // |signaling_thread|.
  MethodCall0<PeerConnectionFactory, bool> call(
      pc_factory.get(),
      &PeerConnectionFactory::Initialize);
  bool result = call.Marshal(RTC_FROM_HERE, signaling_thread);

  if (!result) {
    return nullptr;
  }
  return PeerConnectionFactoryProxy::Create(signaling_thread, pc_factory);
}

PeerConnectionFactory::PeerConnectionFactory()
    : owns_ptrs_(true),
      wraps_current_thread_(false),
      network_thread_(rtc::Thread::CreateWithSocketServer().release()),
      worker_thread_(rtc::Thread::Create().release()),
      signaling_thread_(rtc::Thread::Current()),
      audio_decoder_factory_(CreateBuiltinAudioDecoderFactory()) {
  if (!signaling_thread_) {
    signaling_thread_ = rtc::ThreadManager::Instance()->WrapCurrentThread();
    wraps_current_thread_ = true;
  }
  network_thread_->Start();
  worker_thread_->Start();
}

PeerConnectionFactory::PeerConnectionFactory(
    rtc::Thread* network_thread,
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread,
    AudioDeviceModule* default_adm,
    const rtc::scoped_refptr<webrtc::AudioDecoderFactory>&
        audio_decoder_factory,
    cricket::WebRtcVideoEncoderFactory* video_encoder_factory,
    cricket::WebRtcVideoDecoderFactory* video_decoder_factory)
    : owns_ptrs_(false),
      wraps_current_thread_(false),
      network_thread_(network_thread),
      worker_thread_(worker_thread),
      signaling_thread_(signaling_thread),
      default_adm_(default_adm),
      audio_decoder_factory_(audio_decoder_factory),
      video_encoder_factory_(video_encoder_factory),
      video_decoder_factory_(video_decoder_factory) {
  RTC_DCHECK(network_thread);
  RTC_DCHECK(worker_thread);
  RTC_DCHECK(signaling_thread);
  // TODO: Currently there is no way creating an external adm in
  // libjingle source tree. So we can 't currently assert if this is NULL.
  // ASSERT(default_adm != NULL);
}

PeerConnectionFactory::~PeerConnectionFactory() {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  channel_manager_.reset(nullptr);

  // Make sure |worker_thread_| and |signaling_thread_| outlive
  // |default_socket_factory_| and |default_network_manager_|.
  default_socket_factory_ = nullptr;
  default_network_manager_ = nullptr;

  if (owns_ptrs_) {
    if (wraps_current_thread_)
      rtc::ThreadManager::Instance()->UnwrapCurrentThread();
    delete worker_thread_;
    delete network_thread_;
  }
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

  // TODO:  Need to make sure only one VoE is created inside
  // WebRtcMediaEngine.
  cricket::MediaEngineInterface* media_engine =
      worker_thread_->Invoke<cricket::MediaEngineInterface*>(
          RTC_FROM_HERE,
          rtc::Bind(&PeerConnectionFactory::CreateMediaEngine_w, this));

  channel_manager_.reset(new cricket::ChannelManager(
      media_engine, worker_thread_, network_thread_));

  channel_manager_->SetVideoRtxEnabled(true);
  channel_manager_->SetCryptoOptions(options_.crypto_options);
  if (!channel_manager_->Init()) {
    return false;
  }

  return true;
}

void PeerConnectionFactory::SetOptions(const Options& options) {
  options_ = options;
  if (channel_manager_) {
    channel_manager_->SetCryptoOptions(options.crypto_options);
  }
}

rtc::scoped_refptr<AudioSourceInterface>
PeerConnectionFactory::CreateAudioSource(
    const MediaConstraintsInterface* constraints) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  rtc::scoped_refptr<LocalAudioSource> source(
      LocalAudioSource::Create(options_, constraints));
  return source;
}

rtc::scoped_refptr<AudioSourceInterface>
PeerConnectionFactory::CreateAudioSource(const cricket::AudioOptions& options) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  rtc::scoped_refptr<LocalAudioSource> source(
      LocalAudioSource::Create(options_, &options));
  return source;
}

rtc::scoped_refptr<VideoTrackSourceInterface>
PeerConnectionFactory::CreateVideoSource(
    cricket::VideoCapturer* capturer,
    const MediaConstraintsInterface* constraints) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  rtc::scoped_refptr<VideoTrackSourceInterface> source(
      VideoCapturerTrackSource::Create(worker_thread_, capturer, constraints,
                                       false));
  return VideoTrackSourceProxy::Create(signaling_thread_, worker_thread_,
                                       source);
}

rtc::scoped_refptr<VideoTrackSourceInterface>
PeerConnectionFactory::CreateVideoSource(cricket::VideoCapturer* capturer) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  rtc::scoped_refptr<VideoTrackSourceInterface> source(
      VideoCapturerTrackSource::Create(worker_thread_, capturer, false));
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

  rtc::scoped_refptr<PeerConnection> pc(
      new rtc::RefCountedObject<PeerConnection>(this));

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

webrtc::MediaControllerInterface* PeerConnectionFactory::CreateMediaController(
    const cricket::MediaConfig& config,
    webrtc::RtcEventLog* event_log) const {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  return MediaControllerInterface::Create(config, worker_thread_,
                                          channel_manager_.get(), event_log);
}

cricket::TransportController* PeerConnectionFactory::CreateTransportController(
    cricket::PortAllocator* port_allocator,
    bool redetermine_role_on_ice_restart) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  return new cricket::TransportController(signaling_thread_, network_thread_,
                                          port_allocator,
                                          redetermine_role_on_ice_restart);
}

rtc::Thread* PeerConnectionFactory::signaling_thread() {
  // This method can be called on a different thread when the factory is
  // created in CreatePeerConnectionFactory().
  return signaling_thread_;
}

rtc::Thread* PeerConnectionFactory::worker_thread() {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  return worker_thread_;
}

rtc::Thread* PeerConnectionFactory::network_thread() {
  return network_thread_;
}

cricket::MediaEngineInterface* PeerConnectionFactory::CreateMediaEngine_w() {
  ASSERT(worker_thread_ == rtc::Thread::Current());
  return cricket::WebRtcMediaEngineFactory::Create(
      default_adm_.get(),
      audio_decoder_factory_,
      video_encoder_factory_.get(),
      video_decoder_factory_.get());
}

}  // namespace webrtc
