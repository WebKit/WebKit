/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ORTC_ORTCFACTORY_H_
#define ORTC_ORTCFACTORY_H_

#include <memory>
#include <string>

#include "api/ortc/ortcfactoryinterface.h"
#include "media/base/mediaengine.h"
#include "media/engine/webrtcmediaengine.h"
#include "pc/channelmanager.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/scoped_ref_ptr.h"

namespace webrtc {

// Implementation of OrtcFactoryInterface.
//
// See ortcfactoryinterface.h for documentation.
class OrtcFactory : public OrtcFactoryInterface {
 public:
  ~OrtcFactory() override;

  // Internal-only Create method that allows passing in a fake media engine,
  // for testing.
  static RTCErrorOr<std::unique_ptr<OrtcFactoryInterface>> Create(
      rtc::Thread* network_thread,
      rtc::Thread* signaling_thread,
      rtc::NetworkManager* network_manager,
      rtc::PacketSocketFactory* socket_factory,
      AudioDeviceModule* adm,
      std::unique_ptr<cricket::MediaEngineInterface> media_engine,
      rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
      rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory);

  RTCErrorOr<std::unique_ptr<RtpTransportControllerInterface>>
  CreateRtpTransportController() override;

  RTCErrorOr<std::unique_ptr<RtpTransportInterface>> CreateRtpTransport(
      const RtpTransportParameters& parameters,
      PacketTransportInterface* rtp,
      PacketTransportInterface* rtcp,
      RtpTransportControllerInterface* transport_controller) override;

  RTCErrorOr<std::unique_ptr<SrtpTransportInterface>> CreateSrtpTransport(
      const RtpTransportParameters& parameters,
      PacketTransportInterface* rtp,
      PacketTransportInterface* rtcp,
      RtpTransportControllerInterface* transport_controller) override;

  RtpCapabilities GetRtpSenderCapabilities(
      cricket::MediaType kind) const override;

  RTCErrorOr<std::unique_ptr<OrtcRtpSenderInterface>> CreateRtpSender(
      rtc::scoped_refptr<MediaStreamTrackInterface> track,
      RtpTransportInterface* transport) override;

  RTCErrorOr<std::unique_ptr<OrtcRtpSenderInterface>> CreateRtpSender(
      cricket::MediaType kind,
      RtpTransportInterface* transport) override;

  RtpCapabilities GetRtpReceiverCapabilities(
      cricket::MediaType kind) const override;

  RTCErrorOr<std::unique_ptr<OrtcRtpReceiverInterface>> CreateRtpReceiver(
      cricket::MediaType kind,
      RtpTransportInterface* transport) override;

  RTCErrorOr<std::unique_ptr<UdpTransportInterface>>
  CreateUdpTransport(int family, uint16_t min_port, uint16_t max_port) override;

  rtc::scoped_refptr<AudioSourceInterface> CreateAudioSource(
      const cricket::AudioOptions& options) override;

  rtc::scoped_refptr<VideoTrackInterface> CreateVideoTrack(
      const std::string& id,
      VideoTrackSourceInterface* source) override;

  rtc::scoped_refptr<AudioTrackInterface> CreateAudioTrack(
      const std::string& id,
      AudioSourceInterface* source) override;

  rtc::Thread* network_thread() { return network_thread_; }
  rtc::Thread* worker_thread() { return worker_thread_.get(); }
  rtc::Thread* signaling_thread() { return signaling_thread_; }

 private:
  // Should only be called by OrtcFactoryInterface::Create.
  OrtcFactory(rtc::Thread* network_thread,
              rtc::Thread* signaling_thread,
              rtc::NetworkManager* network_manager,
              rtc::PacketSocketFactory* socket_factory,
              AudioDeviceModule* adm,
              rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
              rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory);

  RTCErrorOr<std::unique_ptr<RtpTransportControllerInterface>>
  CreateRtpTransportController(const RtpTransportParameters& parameters);

  // Thread::Invoke doesn't support move-only arguments, so we need to remove
  // the unique_ptr wrapper from media_engine. TODO(deadbeef): Fix this.
  static RTCErrorOr<std::unique_ptr<OrtcFactoryInterface>> Create_s(
      rtc::Thread* network_thread,
      rtc::Thread* signaling_thread,
      rtc::NetworkManager* network_manager,
      rtc::PacketSocketFactory* socket_factory,
      AudioDeviceModule* adm,
      cricket::MediaEngineInterface* media_engine,
      rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
      rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory);

  // Performs initialization that can fail. Called by factory method after
  // construction, and if it fails, no object is returned.
  RTCError Initialize(
      std::unique_ptr<cricket::MediaEngineInterface> media_engine);
  std::unique_ptr<cricket::MediaEngineInterface> CreateMediaEngine_w();

  // Threads and networking objects.
  rtc::Thread* network_thread_;
  rtc::Thread* signaling_thread_;
  rtc::NetworkManager* network_manager_;
  rtc::PacketSocketFactory* socket_factory_;
  AudioDeviceModule* adm_;
  // If we created/own the objects above, these will be non-null and thus will
  // be released automatically upon destruction.
  std::unique_ptr<rtc::Thread> owned_network_thread_;
  bool wraps_signaling_thread_ = false;
  std::unique_ptr<rtc::NetworkManager> owned_network_manager_;
  std::unique_ptr<rtc::PacketSocketFactory> owned_socket_factory_;
  // We always own the worker thread.
  std::unique_ptr<rtc::Thread> worker_thread_;
  // Media-releated objects.
  std::unique_ptr<RtcEventLog> null_event_log_;
  rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory_;
  rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory_;
  std::unique_ptr<cricket::ChannelManager> channel_manager_;
  // Default CNAME to use for RtpTransports if none is passed in.
  std::string default_cname_;

  friend class OrtcFactoryInterface;

  RTC_DISALLOW_COPY_AND_ASSIGN(OrtcFactory);
};

}  // namespace webrtc

#endif  // ORTC_ORTCFACTORY_H_
