/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/test/test_stun_server.h"

#include <functional>
#include <memory>

#include "api/sequence_checker.h"
#include "api/transport/stun.h"
#include "p2p/test/stun_server.h"
#include "rtc_base/async_udp_socket.h"
#include "rtc_base/checks.h"
#include "rtc_base/socket.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/socket_server.h"

namespace webrtc {

std::unique_ptr<TestStunServer, std::function<void(TestStunServer*)>>
TestStunServer::Create(SocketServer* ss,
                       const SocketAddress& addr,
                       Thread& network_thread) {
  Socket* socket = ss->CreateSocket(addr.family(), SOCK_DGRAM);
  RTC_CHECK(socket != nullptr) << "Failed to create socket";
  AsyncUDPSocket* udp_socket = AsyncUDPSocket::Create(socket, addr);
  RTC_CHECK(udp_socket != nullptr) << "Failed to create AsyncUDPSocket";
  TestStunServer* server = nullptr;
  network_thread.BlockingCall(
      [&]() { server = new TestStunServer(udp_socket, network_thread); });
  std::unique_ptr<TestStunServer, std::function<void(TestStunServer*)>> result(
      server, [&](TestStunServer* server) {
        network_thread.BlockingCall([server]() { delete server; });
      });
  return result;
}

void TestStunServer::OnBindingRequest(StunMessage* msg,
                                      const SocketAddress& remote_addr) {
  RTC_DCHECK_RUN_ON(&network_thread_);
  if (fake_stun_addr_.IsNil()) {
    StunServer::OnBindingRequest(msg, remote_addr);
  } else {
    StunMessage response(STUN_BINDING_RESPONSE, msg->transaction_id());
    GetStunBindResponse(msg, fake_stun_addr_, &response);
    SendResponse(response, remote_addr);
  }
}

}  // namespace webrtc
