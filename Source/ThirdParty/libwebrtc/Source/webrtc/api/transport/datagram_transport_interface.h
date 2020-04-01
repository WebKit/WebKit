/* Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This is EXPERIMENTAL interface for media and datagram transports.

#ifndef API_TRANSPORT_DATAGRAM_TRANSPORT_INTERFACE_H_
#define API_TRANSPORT_DATAGRAM_TRANSPORT_INTERFACE_H_

#include <memory>
#include <string>
#include <utility>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/rtc_error.h"
#include "api/transport/congestion_control_interface.h"
#include "api/transport/data_channel_transport_interface.h"
#include "api/units/data_rate.h"
#include "api/units/timestamp.h"

namespace rtc {
class PacketTransportInternal;
}  // namespace rtc

namespace webrtc {

class MediaTransportStateCallback;

typedef int64_t DatagramId;

struct DatagramAck {
  // |datagram_id| is same as passed in
  // DatagramTransportInterface::SendDatagram.
  DatagramId datagram_id;

  // The timestamp at which the remote peer received the identified datagram,
  // according to that peer's clock.
  Timestamp receive_timestamp = Timestamp::MinusInfinity();
};

// All sink methods are called on network thread.
class DatagramSinkInterface {
 public:
  virtual ~DatagramSinkInterface() {}

  // Called when new packet is received.
  virtual void OnDatagramReceived(rtc::ArrayView<const uint8_t> data) = 0;

  // Called when datagram is actually sent (datragram can be delayed due
  // to congestion control or fusing). |datagram_id| is same as passed in
  // DatagramTransportInterface::SendDatagram.
  virtual void OnDatagramSent(DatagramId datagram_id) = 0;

  // Called when datagram is ACKed.
  virtual void OnDatagramAcked(const DatagramAck& datagram_ack) = 0;

  // Called when a datagram is lost.
  virtual void OnDatagramLost(DatagramId datagram_id) = 0;
};

// Datagram transport allows to send and receive unreliable packets (datagrams)
// and receive feedback from congestion control (via
// CongestionControlInterface). The idea is to send RTP packets as datagrams and
// have underlying implementation of datagram transport to use QUIC datagram
// protocol.
class DatagramTransportInterface : public DataChannelTransportInterface {
 public:
  virtual ~DatagramTransportInterface() = default;

  // Connect the datagram transport to the ICE transport.
  // The implementation must be able to ignore incoming packets that don't
  // belong to it.
  virtual void Connect(rtc::PacketTransportInternal* packet_transport) = 0;

  // Returns congestion control feedback interface or nullptr if datagram
  // transport does not implement congestion control.
  //
  // Note that right now datagram transport is used without congestion control,
  // but we plan to use it in the future.
  virtual CongestionControlInterface* congestion_control() = 0;

  // Sets a state observer callback. Before datagram transport is destroyed, the
  // callback must be unregistered by setting it to nullptr.
  // A newly registered callback will be called with the current state.
  // Datagram transport does not invoke this callback concurrently.
  virtual void SetTransportStateCallback(
      MediaTransportStateCallback* callback) = 0;

  // Start asynchronous send of datagram. The status returned by this method
  // only pertains to the synchronous operations (e.g. serialization /
  // packetization), not to the asynchronous operation.
  //
  // Datagrams larger than GetLargestDatagramSize() will fail and return error.
  //
  // Datagrams are sent in FIFO order.
  //
  // |datagram_id| is only used in ACK/LOST notifications in
  // DatagramSinkInterface and does not need to be unique.
  virtual RTCError SendDatagram(rtc::ArrayView<const uint8_t> data,
                                DatagramId datagram_id) = 0;

  // Returns maximum size of datagram message, does not change.
  // TODO(sukhanov): Because value may be undefined before connection setup
  // is complete, consider returning error when called before connection is
  // established. Currently returns hardcoded const, because integration
  // prototype may call before connection is established.
  virtual size_t GetLargestDatagramSize() const = 0;

  // Sets packet sink. Sink must be unset by calling
  // SetDataTransportSink(nullptr) before the data transport is destroyed or
  // before new sink is set.
  virtual void SetDatagramSink(DatagramSinkInterface* sink) = 0;

  // Retrieves transport parameters for this datagram transport.  May be called
  // on either client- or server-perspective transports.
  //
  // For servers, the parameters represent what kind of connections and data the
  // server is prepared to accept.  This is generally a superset of acceptable
  // parameters.
  //
  // For clients, the parameters echo the server configuration used to create
  // the client, possibly removing any fields or parameters which the client
  // does not understand.
  virtual std::string GetTransportParameters() const = 0;

  // Sets remote transport parameters.  |remote_params| is a serialized string
  // of opaque parameters, understood by the datagram transport implementation.
  // Returns an error if |remote_params| are not compatible with this transport.
  //
  // TODO(mellem): Make pure virtual.  The default implementation maintains
  // original negotiation behavior (negotiation falls back to RTP if the
  // remote datagram transport fails to echo exactly the local parameters).
  virtual RTCError SetRemoteTransportParameters(
      absl::string_view remote_params) {
    if (remote_params == GetTransportParameters()) {
      return RTCError::OK();
    }
    return RTCError(RTCErrorType::UNSUPPORTED_PARAMETER,
                    "Local and remote transport parameters do not match");
  }
};

}  // namespace webrtc

#endif  // API_TRANSPORT_DATAGRAM_TRANSPORT_INTERFACE_H_
