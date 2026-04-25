/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_TEST_ECHO_SERVER_H_
#define RTC_BASE_TEST_ECHO_SERVER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <utility>

#include "absl/memory/memory.h"
#include "api/environment/environment.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/async_tcp_socket.h"
#include "rtc_base/checks.h"
#include "rtc_base/memory/less_unique_ptr.h"
#include "rtc_base/network/received_packet.h"
#include "rtc_base/socket.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/thread.h"

namespace webrtc {

// A test echo server, echoes back any packets sent to it.
// Useful for unit tests.
class TestEchoServer {
 public:
  TestEchoServer(const Environment& env,
                 Thread* thread,
                 const SocketAddress& addr);
  virtual ~TestEchoServer();

  TestEchoServer(const TestEchoServer&) = delete;
  TestEchoServer& operator=(const TestEchoServer&) = delete;

  SocketAddress address() const { return server_socket_->GetLocalAddress(); }

 private:
  void OnAccept(Socket* socket) {
    std::unique_ptr<Socket> raw_socket =
        absl::WrapUnique(socket->Accept(nullptr));
    if (raw_socket) {
      auto packet_socket =
          std::make_unique<AsyncTCPSocket>(env_, std::move(raw_socket));
      packet_socket->RegisterReceivedPacketCallback(
          [&](AsyncPacketSocket* socket, const ReceivedIpPacket& packet) {
            OnPacket(socket, packet);
          });
      packet_socket->SubscribeCloseEvent(
          this, [this](AsyncPacketSocket* s, int err) { OnClose(s, err); });
      client_sockets_.insert(std::move(packet_socket));
    }
  }
  void OnPacket(AsyncPacketSocket* socket, const ReceivedIpPacket& packet) {
    AsyncSocketPacketOptions options;
    socket->Send(packet.payload().data(), packet.payload().size(), options);
  }
  void OnClose(AsyncPacketSocket* socket, int err) {
    // Use `find` instead of `extract` directly because `find` allows
    // heterogeneous lookup while `extract` requires key (i.e., unique_ptr) as
    // the input parameter until c++23.
    auto iter = client_sockets_.find(socket);
    RTC_CHECK(iter != client_sockets_.end());
    // `OnClose` is triggered by socket Close callback, deleting `socket` while
    // processing that callback might be unsafe.
    auto node = client_sockets_.extract(iter);
    Thread::Current()->PostTask([node = std::move(node)] {});
  }

  const Environment env_;
  std::unique_ptr<Socket> server_socket_;
  std::set<std::unique_ptr<AsyncTCPSocket>, less_unique_ptr> client_sockets_;
};

}  //  namespace webrtc

#endif  // RTC_BASE_TEST_ECHO_SERVER_H_