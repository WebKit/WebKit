/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_ORTC_ORTCFACTORYINTERFACE_H_
#define WEBRTC_API_ORTC_ORTCFACTORYINTERFACE_H_

#include <memory>
#include <string>
#include <utility>  // For std::move.

#include "webrtc/api/mediaconstraintsinterface.h"
#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/api/mediatypes.h"
#include "webrtc/api/ortc/ortcrtpreceiverinterface.h"
#include "webrtc/api/ortc/ortcrtpsenderinterface.h"
#include "webrtc/api/ortc/packettransportinterface.h"
#include "webrtc/api/ortc/rtptransportcontrollerinterface.h"
#include "webrtc/api/ortc/rtptransportinterface.h"
#include "webrtc/api/ortc/srtptransportinterface.h"
#include "webrtc/api/ortc/udptransportinterface.h"
#include "webrtc/api/rtcerror.h"
#include "webrtc/api/rtpparameters.h"
#include "webrtc/base/network.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/base/thread.h"
#include "webrtc/p2p/base/packetsocketfactory.h"

namespace webrtc {

// TODO(deadbeef): This should be part of /api/, but currently it's not and
// including its header violates checkdeps rules.
class AudioDeviceModule;

// WARNING: This is experimental/under development, so use at your own risk; no
// guarantee about API stability is guaranteed here yet.
//
// This class is the ORTC analog of PeerConnectionFactory. It acts as a factory
// for ORTC objects that can be connected to each other.
//
// Some of these objects may not be represented by the ORTC specification, but
// follow the same general principles.
//
// If one of the factory methods takes another object as an argument, it MUST
// have been created by the same OrtcFactory.
//
// On object lifetimes: objects should be destroyed in this order:
// 1. Objects created by the factory.
// 2. The factory itself.
// 3. Objects passed into OrtcFactoryInterface::Create.
class OrtcFactoryInterface {
 public:
  // |network_thread| is the thread on which packets are sent and received.
  // If null, a new rtc::Thread with a default socket server is created.
  //
  // |signaling_thread| is used for callbacks to the consumer of the API. If
  // null, the current thread will be used, which assumes that the API consumer
  // is running a message loop on this thread (either using an existing
  // rtc::Thread, or by calling rtc::Thread::Current()->ProcessMessages).
  //
  // |network_manager| is used to determine which network interfaces are
  // available. This is used for ICE, for example. If null, a default
  // implementation will be used. Only accessed on |network_thread|.
  //
  // |socket_factory| is used (on the network thread) for creating sockets. If
  // it's null, a default implementation will be used, which assumes
  // |network_thread| is a normal rtc::Thread.
  //
  // |adm| is optional, and allows a different audio device implementation to
  // be injected; otherwise a platform-specific module will be used that will
  // use the default audio input.
  //
  // Note that the OrtcFactoryInterface does not take ownership of any of the
  // objects passed in, and as previously stated, these objects can't be
  // destroyed before the factory is.
  static RTCErrorOr<std::unique_ptr<OrtcFactoryInterface>> Create(
      rtc::Thread* network_thread,
      rtc::Thread* signaling_thread,
      rtc::NetworkManager* network_manager,
      rtc::PacketSocketFactory* socket_factory,
      AudioDeviceModule* adm);

  // Constructor for convenience which uses default implementations of
  // everything (though does still require that the current thread runs a
  // message loop; see above).
  static RTCErrorOr<std::unique_ptr<OrtcFactoryInterface>> Create() {
    return Create(nullptr, nullptr, nullptr, nullptr, nullptr);
  }

  virtual ~OrtcFactoryInterface() {}

  // Creates an RTP transport controller, which is used in calls to
  // CreateRtpTransport methods. If your application has some notion of a
  // "call", you should create one transport controller per call.
  //
  // However, if you only are using one RtpTransport object, this doesn't need
  // to be called explicitly; CreateRtpTransport will create one automatically
  // if |rtp_transport_controller| is null. See below.
  //
  // TODO(deadbeef): Add MediaConfig and RtcEventLog arguments?
  virtual RTCErrorOr<std::unique_ptr<RtpTransportControllerInterface>>
  CreateRtpTransportController() = 0;

  // Creates an RTP transport using the provided packet transports and
  // transport controller.
  //
  // |rtp| will be used for sending RTP packets, and |rtcp| for RTCP packets.
  //
  // |rtp| can't be null. |rtcp| must be non-null if and only if
  // |rtcp_parameters.mux| is false, indicating that RTCP muxing isn't used.
  // Note that if RTCP muxing isn't enabled initially, it can still enabled
  // later through SetRtcpParameters.
  //
  // If |transport_controller| is null, one will automatically be created, and
  // its lifetime managed by the returned RtpTransport. This should only be
  // done if a single RtpTransport is being used to communicate with the remote
  // endpoint.
  virtual RTCErrorOr<std::unique_ptr<RtpTransportInterface>> CreateRtpTransport(
      const RtcpParameters& rtcp_parameters,
      PacketTransportInterface* rtp,
      PacketTransportInterface* rtcp,
      RtpTransportControllerInterface* transport_controller) = 0;

  // Creates an SrtpTransport which is an RTP transport that uses SRTP.
  virtual RTCErrorOr<std::unique_ptr<SrtpTransportInterface>>
  CreateSrtpTransport(
      const RtcpParameters& rtcp_parameters,
      PacketTransportInterface* rtp,
      PacketTransportInterface* rtcp,
      RtpTransportControllerInterface* transport_controller) = 0;

  // Returns the capabilities of an RTP sender of type |kind|. These
  // capabilities can be used to determine what RtpParameters to use to create
  // an RtpSender.
  //
  // If for some reason you pass in MEDIA_TYPE_DATA, returns an empty structure.
  virtual RtpCapabilities GetRtpSenderCapabilities(
      cricket::MediaType kind) const = 0;

  // Creates an RTP sender with |track|. Will not start sending until Send is
  // called. This is provided as a convenience; it's equivalent to calling
  // CreateRtpSender with a kind (see below), followed by SetTrack.
  //
  // |track| and |transport| must not be null.
  virtual RTCErrorOr<std::unique_ptr<OrtcRtpSenderInterface>> CreateRtpSender(
      rtc::scoped_refptr<MediaStreamTrackInterface> track,
      RtpTransportInterface* transport) = 0;

  // Overload of CreateRtpSender allows creating the sender without a track.
  //
  // |kind| must be MEDIA_TYPE_AUDIO or MEDIA_TYPE_VIDEO.
  virtual RTCErrorOr<std::unique_ptr<OrtcRtpSenderInterface>> CreateRtpSender(
      cricket::MediaType kind,
      RtpTransportInterface* transport) = 0;

  // Returns the capabilities of an RTP receiver of type |kind|. These
  // capabilities can be used to determine what RtpParameters to use to create
  // an RtpReceiver.
  //
  // If for some reason you pass in MEDIA_TYPE_DATA, returns an empty structure.
  virtual RtpCapabilities GetRtpReceiverCapabilities(
      cricket::MediaType kind) const = 0;

  // Creates an RTP receiver of type |kind|. Will not start receiving media
  // until Receive is called.
  //
  // |kind| must be MEDIA_TYPE_AUDIO or MEDIA_TYPE_VIDEO.
  //
  // |transport| must not be null.
  virtual RTCErrorOr<std::unique_ptr<OrtcRtpReceiverInterface>>
  CreateRtpReceiver(cricket::MediaType kind,
                    RtpTransportInterface* transport) = 0;

  // Create a UDP transport with IP address family |family|, using a port
  // within the specified range.
  //
  // |family| must be AF_INET or AF_INET6.
  //
  // |min_port|/|max_port| values of 0 indicate no range restriction.
  //
  // Returns an error if the transport wasn't successfully created.
  virtual RTCErrorOr<std::unique_ptr<UdpTransportInterface>>
  CreateUdpTransport(int family, uint16_t min_port, uint16_t max_port) = 0;

  // Method for convenience that has no port range restrictions.
  RTCErrorOr<std::unique_ptr<UdpTransportInterface>> CreateUdpTransport(
      int family) {
    return CreateUdpTransport(family, 0, 0);
  }

  // NOTE: The methods below to create tracks/sources return scoped_refptrs
  // rather than unique_ptrs, because these interfaces are also used with
  // PeerConnection, where everything is ref-counted.

  // Creates a audio source representing the default microphone input.
  // |options| decides audio processing settings.
  virtual rtc::scoped_refptr<AudioSourceInterface> CreateAudioSource(
      const cricket::AudioOptions& options) = 0;

  // Version of the above method that uses default options.
  rtc::scoped_refptr<AudioSourceInterface> CreateAudioSource() {
    return CreateAudioSource(cricket::AudioOptions());
  }

  // Creates a video source object wrapping and taking ownership of |capturer|.
  //
  // |constraints| can be used for selection of resolution and frame rate, and
  // may be null if no constraints are desired.
  virtual rtc::scoped_refptr<VideoTrackSourceInterface> CreateVideoSource(
      std::unique_ptr<cricket::VideoCapturer> capturer,
      const MediaConstraintsInterface* constraints) = 0;

  // Version of the above method that omits |constraints|.
  rtc::scoped_refptr<VideoTrackSourceInterface> CreateVideoSource(
      std::unique_ptr<cricket::VideoCapturer> capturer) {
    return CreateVideoSource(std::move(capturer), nullptr);
  }

  // Creates a new local video track wrapping |source|. The same |source| can
  // be used in several tracks.
  virtual rtc::scoped_refptr<VideoTrackInterface> CreateVideoTrack(
      const std::string& id,
      VideoTrackSourceInterface* source) = 0;

  // Creates an new local audio track wrapping |source|.
  virtual rtc::scoped_refptr<AudioTrackInterface> CreateAudioTrack(
      const std::string& id,
      AudioSourceInterface* source) = 0;
};

}  // namespace webrtc

#endif  // WEBRTC_API_ORTC_ORTCFACTORYINTERFACE_H_
