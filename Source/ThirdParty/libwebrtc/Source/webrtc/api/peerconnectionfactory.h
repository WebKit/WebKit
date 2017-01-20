/*
 *  Copyright 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_PEERCONNECTIONFACTORY_H_
#define WEBRTC_API_PEERCONNECTIONFACTORY_H_

#include <memory>
#include <string>

#include "webrtc/api/mediacontroller.h"
#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/api/peerconnectioninterface.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/base/thread.h"
#include "webrtc/base/rtccertificategenerator.h"
#include "webrtc/pc/channelmanager.h"

namespace rtc {
class BasicNetworkManager;
class BasicPacketSocketFactory;
}

namespace webrtc {

class RtcEventLog;

class PeerConnectionFactory : public PeerConnectionFactoryInterface {
 public:
  void SetOptions(const Options& options) override;

  // Deprecated, use version without constraints.
  rtc::scoped_refptr<PeerConnectionInterface> CreatePeerConnection(
      const PeerConnectionInterface::RTCConfiguration& configuration,
      const MediaConstraintsInterface* constraints,
      std::unique_ptr<cricket::PortAllocator> allocator,
      std::unique_ptr<rtc::RTCCertificateGeneratorInterface> cert_generator,
      PeerConnectionObserver* observer) override;

  virtual rtc::scoped_refptr<PeerConnectionInterface> CreatePeerConnection(
      const PeerConnectionInterface::RTCConfiguration& configuration,
      std::unique_ptr<cricket::PortAllocator> allocator,
      std::unique_ptr<rtc::RTCCertificateGeneratorInterface> cert_generator,
      PeerConnectionObserver* observer) override;

  bool Initialize();

  rtc::scoped_refptr<MediaStreamInterface>
      CreateLocalMediaStream(const std::string& label) override;

  virtual rtc::scoped_refptr<AudioSourceInterface> CreateAudioSource(
      const cricket::AudioOptions& options) override;
  // Deprecated, use version without constraints.
  rtc::scoped_refptr<AudioSourceInterface> CreateAudioSource(
      const MediaConstraintsInterface* constraints) override;

  virtual rtc::scoped_refptr<VideoTrackSourceInterface> CreateVideoSource(
      cricket::VideoCapturer* capturer) override;
  // This version supports filtering on width, height and frame rate.
  // For the "constraints=null" case, use the version without constraints.
  // TODO(hta): Design a version without MediaConstraintsInterface.
  // https://bugs.chromium.org/p/webrtc/issues/detail?id=5617
  rtc::scoped_refptr<VideoTrackSourceInterface> CreateVideoSource(
      cricket::VideoCapturer* capturer,
      const MediaConstraintsInterface* constraints) override;

  rtc::scoped_refptr<VideoTrackInterface> CreateVideoTrack(
      const std::string& id,
      VideoTrackSourceInterface* video_source) override;

  rtc::scoped_refptr<AudioTrackInterface>
      CreateAudioTrack(const std::string& id,
                       AudioSourceInterface* audio_source) override;

  bool StartAecDump(rtc::PlatformFile file, int64_t max_size_bytes) override;
  void StopAecDump() override;
  // TODO(ivoc) Remove after Chrome is updated.
  bool StartRtcEventLog(rtc::PlatformFile) override { return false; }
  // TODO(ivoc) Remove after Chrome is updated.
  bool StartRtcEventLog(rtc::PlatformFile,
                        int64_t) override {
    return false;
  }
  // TODO(ivoc) Remove after Chrome is updated.
  void StopRtcEventLog() override {}

  virtual webrtc::MediaControllerInterface* CreateMediaController(
      const cricket::MediaConfig& config,
      RtcEventLog* event_log) const;
  virtual cricket::TransportController* CreateTransportController(
      cricket::PortAllocator* port_allocator,
      bool redetermine_role_on_ice_restart);
  virtual rtc::Thread* signaling_thread();
  virtual rtc::Thread* worker_thread();
  virtual rtc::Thread* network_thread();
  const Options& options() const { return options_; }

 protected:
  PeerConnectionFactory();
  PeerConnectionFactory(
      rtc::Thread* network_thread,
      rtc::Thread* worker_thread,
      rtc::Thread* signaling_thread,
      AudioDeviceModule* default_adm,
      const rtc::scoped_refptr<webrtc::AudioDecoderFactory>&
          audio_decoder_factory,
      cricket::WebRtcVideoEncoderFactory* video_encoder_factory,
      cricket::WebRtcVideoDecoderFactory* video_decoder_factory);
  virtual ~PeerConnectionFactory();

 private:
  cricket::MediaEngineInterface* CreateMediaEngine_w();

  bool owns_ptrs_;
  bool wraps_current_thread_;
  rtc::Thread* network_thread_;
  rtc::Thread* worker_thread_;
  rtc::Thread* signaling_thread_;
  Options options_;
  // External Audio device used for audio playback.
  rtc::scoped_refptr<AudioDeviceModule> default_adm_;
  rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory_;
  std::unique_ptr<cricket::ChannelManager> channel_manager_;
  // External Video encoder factory. This can be NULL if the client has not
  // injected any. In that case, video engine will use the internal SW encoder.
  std::unique_ptr<cricket::WebRtcVideoEncoderFactory> video_encoder_factory_;
  // External Video decoder factory. This can be NULL if the client has not
  // injected any. In that case, video engine will use the internal SW decoder.
  std::unique_ptr<cricket::WebRtcVideoDecoderFactory> video_decoder_factory_;
  std::unique_ptr<rtc::BasicNetworkManager> default_network_manager_;
  std::unique_ptr<rtc::BasicPacketSocketFactory> default_socket_factory_;
};

}  // namespace webrtc

#endif  // WEBRTC_API_PEERCONNECTIONFACTORY_H_
