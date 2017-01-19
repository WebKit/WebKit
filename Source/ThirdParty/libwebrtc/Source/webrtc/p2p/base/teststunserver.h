/*
 *  Copyright 2008 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_TESTSTUNSERVER_H_
#define WEBRTC_P2P_BASE_TESTSTUNSERVER_H_

#include "webrtc/p2p/base/stunserver.h"
#include "webrtc/base/socketaddress.h"
#include "webrtc/base/thread.h"

namespace cricket {

// A test STUN server. Useful for unit tests.
class TestStunServer : StunServer {
 public:
  static TestStunServer* Create(rtc::Thread* thread,
                                const rtc::SocketAddress& addr) {
    rtc::AsyncSocket* socket =
        thread->socketserver()->CreateAsyncSocket(addr.family(), SOCK_DGRAM);
    rtc::AsyncUDPSocket* udp_socket =
        rtc::AsyncUDPSocket::Create(socket, addr);

    return new TestStunServer(udp_socket);
  }

  // Set a fake STUN address to return to the client.
  void set_fake_stun_addr(const rtc::SocketAddress& addr) {
    fake_stun_addr_ = addr;
  }

 private:
  explicit TestStunServer(rtc::AsyncUDPSocket* socket) : StunServer(socket) {}

  void OnBindingRequest(StunMessage* msg,
                        const rtc::SocketAddress& remote_addr) override {
    if (fake_stun_addr_.IsNil()) {
      StunServer::OnBindingRequest(msg, remote_addr);
    } else {
      StunMessage response;
      GetStunBindReqponse(msg, fake_stun_addr_, &response);
      SendResponse(response, remote_addr);
    }
  }

 private:
  rtc::SocketAddress fake_stun_addr_;
};

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_TESTSTUNSERVER_H_
