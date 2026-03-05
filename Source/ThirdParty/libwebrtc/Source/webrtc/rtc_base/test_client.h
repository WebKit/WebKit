/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_TEST_CLIENT_H_
#define RTC_BASE_TEST_CLIENT_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include "api/transport/ecn_marking.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/buffer.h"
#include "rtc_base/network/received_packet.h"
#include "rtc_base/socket.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "test/wait_until.h"

namespace webrtc {

// A simple client that can send TCP or UDP data and check that it receives
// what it expects to receive. Useful for testing server functionality.
class TestClient : public sigslot::has_slots<> {
 public:
  // Records the contents of a packet that was received.
  struct Packet {
    Packet(const ReceivedIpPacket& received_packet);
    Packet(const Packet& p);

    SocketAddress addr;
    Buffer buf;
    EcnMarking ecn;
    std::optional<Timestamp> packet_time;
  };

  // Default timeout for NextPacket reads.
  static constexpr int kTimeoutMs = 5000;

  // Creates a client that will send and receive with the given socket and
  // will post itself messages with the given thread.
  explicit TestClient(std::unique_ptr<AsyncPacketSocket> socket);
  // Create a test client that will use a fake clock. NextPacket needs to wait
  // for a packet to be received, and thus it needs to advance the fake clock
  // if the test is using one, rather than just sleeping.
  TestClient(std::unique_ptr<AsyncPacketSocket> socket, ClockVariant clock);
  ~TestClient() override;

  TestClient(const TestClient&) = delete;
  TestClient& operator=(const TestClient&) = delete;

  SocketAddress address() const { return socket_->GetLocalAddress(); }
  SocketAddress remote_address() const { return socket_->GetRemoteAddress(); }

  // Checks that the socket moves to the specified connect state.
  bool CheckConnState(AsyncPacketSocket::State state);

  // Checks that the socket is connected to the remote side.
  bool CheckConnected() {
    return CheckConnState(AsyncPacketSocket::STATE_CONNECTED);
  }

  // Sends using the clients socket.
  int Send(const char* buf, size_t size);

  // Sends using the clients socket to the given destination.
  int SendTo(const char* buf, size_t size, const SocketAddress& dest);

  // Returns the next packet received by the client or null if none is received
  // within the specified timeout.
  std::unique_ptr<Packet> NextPacket(int timeout_ms = kTimeoutMs);

  // Checks that the next packet has the given contents. Returns the remote
  // address that the packet was sent from.
  bool CheckNextPacket(const char* buf, size_t size, SocketAddress* addr);

  // Checks that no packets have arrived or will arrive in the next second.
  bool CheckNoPacket();

  int GetError();
  int SetOption(Socket::Option opt, int value);

  bool ready_to_send() const { return ready_to_send_count() > 0; }

  // How many times SignalReadyToSend has been fired.
  int ready_to_send_count() const { return ready_to_send_count_; }

 private:
  // Timeout for reads when no packet is expected.
  static constexpr TimeDelta kNoPacketTimeout = TimeDelta::Seconds(1);
  // Workaround for the fact that AsyncPacketSocket::GetConnState doesn't exist.
  Socket::ConnState GetState();

  void OnPacket(AsyncPacketSocket* socket,
                const ReceivedIpPacket& received_packet);
  void OnReadyToSend(AsyncPacketSocket* socket);
  bool CheckTimestamp(std::optional<Timestamp> packet_timestamp);

  ClockVariant clock_;
  Mutex mutex_;
  std::unique_ptr<AsyncPacketSocket> socket_;
  std::vector<std::unique_ptr<Packet>> packets_;
  int ready_to_send_count_ = 0;
  std::optional<Timestamp> prev_packet_timestamp_;
};

}  //  namespace webrtc

#endif  // RTC_BASE_TEST_CLIENT_H_
