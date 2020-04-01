/* Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This is EXPERIMENTAL interface for media transport.
//
// The goal is to refactor WebRTC code so that audio and video frames
// are sent / received through the media transport interface. This will
// enable different media transport implementations, including QUIC-based
// media transport.

#ifndef API_TRANSPORT_MEDIA_MEDIA_TRANSPORT_INTERFACE_H_
#define API_TRANSPORT_MEDIA_MEDIA_TRANSPORT_INTERFACE_H_

#include <memory>
#include <string>
#include <utility>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/rtc_error.h"
#include "api/transport/data_channel_transport_interface.h"
#include "api/transport/media/audio_transport.h"
#include "api/transport/media/video_transport.h"
#include "api/transport/network_control.h"
#include "api/units/data_rate.h"
#include "common_types.h"  // NOLINT(build/include)
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/network_route.h"

namespace rtc {
class PacketTransportInternal;
class Thread;
}  // namespace rtc

namespace webrtc {

class DatagramTransportInterface;
class RtcEventLog;

class AudioPacketReceivedObserver {
 public:
  virtual ~AudioPacketReceivedObserver() = default;

  // Invoked for the first received audio packet on a given channel id.
  // It will be invoked once for each channel id.
  virtual void OnFirstAudioPacketReceived(int64_t channel_id) = 0;
};

// Used to configure stream allocations.
struct MediaTransportAllocatedBitrateLimits {
  DataRate min_pacing_rate = DataRate::Zero();
  DataRate max_padding_bitrate = DataRate::Zero();
  DataRate max_total_allocated_bitrate = DataRate::Zero();
};

// Used to configure target bitrate constraints.
// If the value is provided, the constraint is updated.
// If the value is omitted, the value is left unchanged.
struct MediaTransportTargetRateConstraints {
  absl::optional<DataRate> min_bitrate;
  absl::optional<DataRate> max_bitrate;
  absl::optional<DataRate> starting_bitrate;
};

// A collection of settings for creation of media transport.
struct MediaTransportSettings final {
  MediaTransportSettings();
  MediaTransportSettings(const MediaTransportSettings&);
  MediaTransportSettings& operator=(const MediaTransportSettings&);
  ~MediaTransportSettings();

  // Group calls are not currently supported, in 1:1 call one side must set
  // is_caller = true and another is_caller = false.
  bool is_caller;

  // Must be set if a pre-shared key is used for the call.
  // TODO(bugs.webrtc.org/9944): This should become zero buffer in the distant
  // future.
  absl::optional<std::string> pre_shared_key;

  // If present, this is a config passed from the caller to the answerer in the
  // offer. Each media transport knows how to understand its own parameters.
  absl::optional<std::string> remote_transport_parameters;

  // If present, provides the event log that media transport should use.
  // Media transport does not own it. The lifetime of |event_log| will exceed
  // the lifetime of the instance of MediaTransportInterface instance.
  RtcEventLog* event_log = nullptr;
};

// Callback to notify about network route changes.
class MediaTransportNetworkChangeCallback {
 public:
  virtual ~MediaTransportNetworkChangeCallback() = default;

  // Called when the network route is changed, with the new network route.
  virtual void OnNetworkRouteChanged(
      const rtc::NetworkRoute& new_network_route) = 0;
};

// State of the media transport.  Media transport begins in the pending state.
// It transitions to writable when it is ready to send media.  It may transition
// back to pending if the connection is blocked.  It may transition to closed at
// any time.  Closed is terminal: a transport will never re-open once closed.
enum class MediaTransportState {
  kPending,
  kWritable,
  kClosed,
};

// Callback invoked whenever the state of the media transport changes.
class MediaTransportStateCallback {
 public:
  virtual ~MediaTransportStateCallback() = default;

  // Invoked whenever the state of the media transport changes.
  virtual void OnStateChanged(MediaTransportState state) = 0;
};

// Callback for RTT measurements on the receive side.
// TODO(nisse): Related interfaces: CallStatsObserver and RtcpRttStats. It's
// somewhat unclear what type of measurement is needed. It's used to configure
// NACK generation and playout buffer. Either raw measurement values or recent
// maximum would make sense for this use. Need consolidation of RTT signalling.
class MediaTransportRttObserver {
 public:
  virtual ~MediaTransportRttObserver() = default;

  // Invoked when a new RTT measurement is available, typically once per ACK.
  virtual void OnRttUpdated(int64_t rtt_ms) = 0;
};

// Media transport interface for sending / receiving encoded audio/video frames
// and receiving bandwidth estimate update from congestion control.
class MediaTransportInterface : public DataChannelTransportInterface {
 public:
  MediaTransportInterface();
  virtual ~MediaTransportInterface();

  // Retrieves callers config (i.e. media transport offer) that should be passed
  // to the callee, before the call is connected. Such config is opaque to SDP
  // (sdp just passes it through). The config is a binary blob, so SDP may
  // choose to use base64 to serialize it (or any other approach that guarantees
  // that the binary blob goes through). This should only be called for the
  // caller's perspective.
  //
  // This may return an unset optional, which means that the given media
  // transport is not supported / disabled and shouldn't be reported in SDP.
  //
  // It may also return an empty string, in which case the media transport is
  // supported, but without any extra settings.
  // TODO(psla): Make abstract.
  virtual absl::optional<std::string> GetTransportParametersOffer() const;

  // Connect the media transport to the ICE transport.
  // The implementation must be able to ignore incoming packets that don't
  // belong to it.
  // TODO(psla): Make abstract.
  virtual void Connect(rtc::PacketTransportInternal* packet_transport);

  // Start asynchronous send of audio frame. The status returned by this method
  // only pertains to the synchronous operations (e.g.
  // serialization/packetization), not to the asynchronous operation.

  virtual RTCError SendAudioFrame(uint64_t channel_id,
                                  MediaTransportEncodedAudioFrame frame) = 0;

  // Start asynchronous send of video frame. The status returned by this method
  // only pertains to the synchronous operations (e.g.
  // serialization/packetization), not to the asynchronous operation.
  virtual RTCError SendVideoFrame(
      uint64_t channel_id,
      const MediaTransportEncodedVideoFrame& frame) = 0;

  // Used by video sender to be notified on key frame requests.
  virtual void SetKeyFrameRequestCallback(
      MediaTransportKeyFrameRequestCallback* callback);

  // Requests a keyframe for the particular channel (stream). The caller should
  // check that the keyframe is not present in a jitter buffer already (i.e.
  // don't request a keyframe if there is one that you will get from the jitter
  // buffer in a moment).
  virtual RTCError RequestKeyFrame(uint64_t channel_id) = 0;

  // Sets audio sink. Sink must be unset by calling SetReceiveAudioSink(nullptr)
  // before the media transport is destroyed or before new sink is set.
  virtual void SetReceiveAudioSink(MediaTransportAudioSinkInterface* sink) = 0;

  // Registers a video sink. Before destruction of media transport, you must
  // pass a nullptr.
  virtual void SetReceiveVideoSink(MediaTransportVideoSinkInterface* sink) = 0;

  // Adds a target bitrate observer. Before media transport is destructed
  // the observer must be unregistered (by calling
  // RemoveTargetTransferRateObserver).
  // A newly registered observer will be called back with the latest recorded
  // target rate, if available.
  virtual void AddTargetTransferRateObserver(
      TargetTransferRateObserver* observer);

  // Removes an existing |observer| from observers. If observer was never
  // registered, an error is logged and method does nothing.
  virtual void RemoveTargetTransferRateObserver(
      TargetTransferRateObserver* observer);

  // Sets audio packets observer, which gets informed about incoming audio
  // packets. Before destruction, the observer must be unregistered by setting
  // nullptr.
  //
  // This method may be temporary, when the multiplexer is implemented (or
  // multiplexer may use it to demultiplex channel ids).
  virtual void SetFirstAudioPacketReceivedObserver(
      AudioPacketReceivedObserver* observer);

  // Intended for receive side. AddRttObserver registers an observer to be
  // called for each RTT measurement, typically once per ACK. Before media
  // transport is destructed the observer must be unregistered.
  virtual void AddRttObserver(MediaTransportRttObserver* observer);
  virtual void RemoveRttObserver(MediaTransportRttObserver* observer);

  // Returns the last known target transfer rate as reported to the above
  // observers.
  virtual absl::optional<TargetTransferRate> GetLatestTargetTransferRate();

  // Gets the audio packet overhead in bytes. Returned overhead does not include
  // transport overhead (ipv4/6, turn channeldata, tcp/udp, etc.).
  // If the transport is capable of fusing packets together, this overhead
  // might not be a very accurate number.
  // TODO(nisse): Deprecated.
  virtual size_t GetAudioPacketOverhead() const;

  // Corresponding observers for audio and video overhead. Before destruction,
  // the observers must be unregistered by setting nullptr.

  // TODO(nisse): Should move to per-stream objects, since packetization
  // overhead can vary per stream, e.g., depending on negotiated extensions. In
  // addition, we should move towards reporting total overhead including all
  // layers. Currently, overhead of the lower layers is reported elsewhere,
  // e.g., on route change between IPv4 and IPv6.
  virtual void SetAudioOverheadObserver(OverheadObserver* observer) {}

  // Registers an observer for network change events. If the network route is
  // already established when the callback is added, |callback| will be called
  // immediately with the current network route. Before media transport is
  // destroyed, the callback must be removed.
  virtual void AddNetworkChangeCallback(
      MediaTransportNetworkChangeCallback* callback);
  virtual void RemoveNetworkChangeCallback(
      MediaTransportNetworkChangeCallback* callback);

  // Sets a state observer callback. Before media transport is destroyed, the
  // callback must be unregistered by setting it to nullptr.
  // A newly registered callback will be called with the current state.
  // Media transport does not invoke this callback concurrently.
  virtual void SetMediaTransportStateCallback(
      MediaTransportStateCallback* callback) = 0;

  // Updates allocation limits.
  // TODO(psla): Make abstract when downstream implementation implement it.
  virtual void SetAllocatedBitrateLimits(
      const MediaTransportAllocatedBitrateLimits& limits);

  // Sets starting rate.
  // TODO(psla): Make abstract when downstream implementation implement it.
  virtual void SetTargetBitrateLimits(
      const MediaTransportTargetRateConstraints& target_rate_constraints) {}

  // TODO(sukhanov): RtcEventLogs.
};

// If media transport factory is set in peer connection factory, it will be
// used to create media transport for sending/receiving encoded frames and
// this transport will be used instead of default RTP/SRTP transport.
//
// Currently Media Transport negotiation is not supported in SDP.
// If application is using media transport, it must negotiate it before
// setting media transport factory in peer connection.
class MediaTransportFactory {
 public:
  virtual ~MediaTransportFactory() = default;

  // Creates media transport.
  // - Does not take ownership of packet_transport or network_thread.
  // - Does not support group calls, in 1:1 call one side must set
  //   is_caller = true and another is_caller = false.
  virtual RTCErrorOr<std::unique_ptr<MediaTransportInterface>>
  CreateMediaTransport(rtc::PacketTransportInternal* packet_transport,
                       rtc::Thread* network_thread,
                       const MediaTransportSettings& settings);

  // Creates a new Media Transport in a disconnected state. If the media
  // transport for the caller is created, one can then call
  // MediaTransportInterface::GetTransportParametersOffer on that new instance.
  // TODO(psla): Make abstract.
  virtual RTCErrorOr<std::unique_ptr<webrtc::MediaTransportInterface>>
  CreateMediaTransport(rtc::Thread* network_thread,
                       const MediaTransportSettings& settings);

  // Creates a new Datagram Transport in a disconnected state. If the datagram
  // transport for the caller is created, one can then call
  // DatagramTransportInterface::GetTransportParametersOffer on that new
  // instance.
  //
  // TODO(sukhanov): Consider separating media and datagram transport factories.
  // TODO(sukhanov): Move factory to a separate .h file.
  virtual RTCErrorOr<std::unique_ptr<DatagramTransportInterface>>
  CreateDatagramTransport(rtc::Thread* network_thread,
                          const MediaTransportSettings& settings);

  // Gets a transport name which is supported by the implementation.
  // Different factories should return different transport names, and at runtime
  // it will be checked that different names were used.
  // For example, "rtp" or "generic" may be returned by two different
  // implementations.
  // The value returned by this method must never change in the lifetime of the
  // factory.
  // TODO(psla): Make abstract.
  virtual std::string GetTransportName() const;
};

}  // namespace webrtc
#endif  // API_TRANSPORT_MEDIA_MEDIA_TRANSPORT_INTERFACE_H_
