/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_TEST_NETWORK_EMULATION_NETWORK_EMULATION_INTERFACES_H_
#define API_TEST_NETWORK_EMULATION_NETWORK_EMULATION_INTERFACES_H_

#include "absl/types/optional.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/timestamp.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/ip_address.h"
#include "rtc_base/socket_address.h"

namespace webrtc {

struct EmulatedIpPacket {
 public:
  EmulatedIpPacket(const rtc::SocketAddress& from,
                   const rtc::SocketAddress& to,
                   rtc::CopyOnWriteBuffer data,
                   Timestamp arrival_time,
                   uint16_t application_overhead = 0);
  ~EmulatedIpPacket() = default;
  // This object is not copyable or assignable.
  EmulatedIpPacket(const EmulatedIpPacket&) = delete;
  EmulatedIpPacket& operator=(const EmulatedIpPacket&) = delete;
  // This object is only moveable.
  EmulatedIpPacket(EmulatedIpPacket&&) = default;
  EmulatedIpPacket& operator=(EmulatedIpPacket&&) = default;

  size_t size() const { return data.size(); }
  const uint8_t* cdata() const { return data.cdata(); }

  size_t ip_packet_size() const { return size() + headers_size; }
  rtc::SocketAddress from;
  rtc::SocketAddress to;
  // Holds the UDP payload.
  rtc::CopyOnWriteBuffer data;
  uint16_t headers_size;
  Timestamp arrival_time;
};

// Interface for handling IP packets from an emulated network. This is used with
// EmulatedEndpoint to receive packets on a specific port.
class EmulatedNetworkReceiverInterface {
 public:
  virtual ~EmulatedNetworkReceiverInterface() = default;

  virtual void OnPacketReceived(EmulatedIpPacket packet) = 0;
};

struct EmulatedNetworkStats {
  int64_t packets_sent = 0;
  DataSize bytes_sent = DataSize::Zero();
  // Total amount of packets received with or without destination.
  int64_t packets_received = 0;
  // Total amount of bytes in received packets.
  DataSize bytes_received = DataSize::Zero();
  // Total amount of packets that were received, but no destination was found.
  int64_t packets_dropped = 0;
  // Total amount of bytes in dropped packets.
  DataSize bytes_dropped = DataSize::Zero();

  DataSize first_received_packet_size = DataSize::Zero();
  DataSize first_sent_packet_size = DataSize::Zero();

  Timestamp first_packet_sent_time = Timestamp::PlusInfinity();
  Timestamp last_packet_sent_time = Timestamp::PlusInfinity();
  Timestamp first_packet_received_time = Timestamp::PlusInfinity();
  Timestamp last_packet_received_time = Timestamp::PlusInfinity();

  DataRate AverageSendRate() const {
    RTC_DCHECK_GE(packets_sent, 2);
    return (bytes_sent - first_sent_packet_size) /
           (last_packet_sent_time - first_packet_sent_time);
  }
  DataRate AverageReceiveRate() const {
    RTC_DCHECK_GE(packets_received, 2);
    return (bytes_received - first_received_packet_size) /
           (last_packet_received_time - first_packet_received_time);
  }
};

// EmulatedEndpoint is an abstraction for network interface on device. Instances
// of this are created by NetworkEmulationManager::CreateEndpoint.
class EmulatedEndpoint : public EmulatedNetworkReceiverInterface {
 public:
  // Send packet into network.
  // |from| will be used to set source address for the packet in destination
  // socket.
  // |to| will be used for routing verification and picking right socket by port
  // on destination endpoint.
  virtual void SendPacket(const rtc::SocketAddress& from,
                          const rtc::SocketAddress& to,
                          rtc::CopyOnWriteBuffer packet_data,
                          uint16_t application_overhead = 0) = 0;

  // Binds receiver to this endpoint to send and receive data.
  // |desired_port| is a port that should be used. If it is equal to 0,
  // endpoint will pick the first available port starting from
  // |kFirstEphemeralPort|.
  //
  // Returns the port, that should be used (it will be equals to desired, if
  // |desired_port| != 0 and is free or will be the one, selected by endpoint)
  // or absl::nullopt if desired_port in used. Also fails if there are no more
  // free ports to bind to.
  virtual absl::optional<uint16_t> BindReceiver(
      uint16_t desired_port,
      EmulatedNetworkReceiverInterface* receiver) = 0;
  virtual void UnbindReceiver(uint16_t port) = 0;
  virtual rtc::IPAddress GetPeerLocalAddress() const = 0;

  virtual EmulatedNetworkStats stats() = 0;

 private:
  // Ensure that there can be no other subclass than EmulatedEndpointImpl. This
  // means that it's always safe to downcast EmulatedEndpoint instances to
  // EmulatedEndpointImpl.
  friend class EmulatedEndpointImpl;
  EmulatedEndpoint() = default;
};

// Simulates a TCP connection, this roughly implements the Reno algorithm. In
// difference from TCP this only support sending messages with a fixed length,
// no streaming. This is useful to simulate signaling and cross traffic using
// message based protocols such as HTTP. It differs from UDP messages in that
// they are guranteed to be delivered eventually, even on lossy networks.
class TcpMessageRoute {
 public:
  // Sends a TCP message of the given |size| over the route, |on_received| is
  // called when the message has been delivered. Note that the connection
  // parameters are reset iff there's no currently pending message on the route.
  virtual void SendMessage(size_t size, std::function<void()> on_received) = 0;

 protected:
  ~TcpMessageRoute() = default;
};
}  // namespace webrtc

#endif  // API_TEST_NETWORK_EMULATION_NETWORK_EMULATION_INTERFACES_H_
