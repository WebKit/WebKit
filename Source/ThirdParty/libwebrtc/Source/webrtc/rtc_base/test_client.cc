/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/test_client.h"

#include <cstring>
#include <memory>
#include <optional>
#include <utility>
#include <variant>

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/network/received_packet.h"
#include "rtc_base/socket.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/synchronization/mutex.h"
#include "test/wait_until.h"

namespace webrtc {

// DESIGN: Each packet received is put it into a list of packets.
//         Callers can retrieve received packets from any thread by calling
//         NextPacket.

TestClient::TestClient(std::unique_ptr<AsyncPacketSocket> socket)
    : TestClient(std::move(socket), std::monostate()) {}

TestClient::TestClient(std::unique_ptr<AsyncPacketSocket> socket,
                       ClockVariant clock)
    : clock_(clock), socket_(std::move(socket)) {
  socket_->RegisterReceivedPacketCallback(
      [&](AsyncPacketSocket* socket, const ReceivedIpPacket& packet) {
        OnPacket(socket, packet);
      });
  socket_->SignalReadyToSend.connect(this, &TestClient::OnReadyToSend);
}

TestClient::~TestClient() {}

bool TestClient::CheckConnState(AsyncPacketSocket::State state) {
  // Wait for our timeout value until the socket reaches the desired state.
  return WaitUntil([&]() { return socket_->GetState() == state; },
                   {.clock = clock_});
}

int TestClient::Send(const char* buf, size_t size) {
  AsyncSocketPacketOptions options;
  return socket_->Send(buf, size, options);
}

int TestClient::SendTo(const char* buf,
                       size_t size,
                       const SocketAddress& dest) {
  AsyncSocketPacketOptions options;
  return socket_->SendTo(buf, size, dest, options);
}

std::unique_ptr<TestClient::Packet> TestClient::NextPacket(int timeout_ms) {
  // If no packets are currently available, we go into a get/dispatch loop for
  // at most timeout_ms.  If, during the loop, a packet arrives, then we can
  // stop early and return it.

  // Note that the case where no packet arrives is important.  We often want to
  // test that a packet does not arrive.

  // Note also that we only try to pump our current thread's message queue.
  // Pumping another thread's queue could lead to messages being dispatched from
  // the wrong thread to non-thread-safe objects.
  TimeDelta timeout = TimeDelta::Millis(timeout_ms);

  bool packets_available = WaitUntil(
      [&] {
        MutexLock lock(&mutex_);
        return !packets_.empty();
      },
      {
          .timeout = timeout,
          .clock = clock_,
      });

  // Return the first packet placed in the queue.
  std::unique_ptr<Packet> packet;
  MutexLock lock(&mutex_);
  if (packets_available) {
    packet = std::move(packets_.front());
    packets_.erase(packets_.begin());
  }

  return packet;
}

bool TestClient::CheckNextPacket(const char* buf,
                                 size_t size,
                                 SocketAddress* addr) {
  bool res = false;
  std::unique_ptr<Packet> packet = NextPacket(kTimeoutMs);
  if (packet) {
    res = (packet->buf.size() == size &&
           memcmp(packet->buf.data(), buf, size) == 0 &&
           CheckTimestamp(packet->packet_time));
    if (addr)
      *addr = packet->addr;
  }
  return res;
}

bool TestClient::CheckTimestamp(std::optional<Timestamp> packet_timestamp) {
  bool res = true;
  if (!packet_timestamp) {
    res = false;
  }
  if (prev_packet_timestamp_) {
    if (packet_timestamp < prev_packet_timestamp_) {
      res = false;
    }
  }
  prev_packet_timestamp_ = packet_timestamp;
  return res;
}

bool TestClient::CheckNoPacket() {
  return NextPacket(kNoPacketTimeout.ms()) == nullptr;
}

int TestClient::GetError() {
  return socket_->GetError();
}

int TestClient::SetOption(Socket::Option opt, int value) {
  return socket_->SetOption(opt, value);
}

void TestClient::OnPacket(AsyncPacketSocket* socket,
                          const ReceivedIpPacket& received_packet) {
  MutexLock lock(&mutex_);
  packets_.push_back(std::make_unique<Packet>(received_packet));
}

void TestClient::OnReadyToSend(AsyncPacketSocket* socket) {
  ++ready_to_send_count_;
}

TestClient::Packet::Packet(const ReceivedIpPacket& received_packet)
    : addr(received_packet.source_address()),
      // Copy received_packet payload to a buffer owned by Packet.
      buf(received_packet.payload().data(), received_packet.payload().size()),
      ecn(received_packet.ecn()),
      packet_time(received_packet.arrival_time()) {}

TestClient::Packet::Packet(const Packet& p)
    : addr(p.addr),
      buf(p.buf.data(), p.buf.size()),
      ecn(p.ecn),
      packet_time(p.packet_time) {}

}  // namespace webrtc
