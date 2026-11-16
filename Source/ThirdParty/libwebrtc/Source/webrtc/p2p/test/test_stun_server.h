/*
 *  Copyright 2008 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_TEST_TEST_STUN_SERVER_H_
#define P2P_TEST_TEST_STUN_SERVER_H_

#include <functional>
#include <memory>
#include <utility>

#include "absl/base/attributes.h"
#include "api/environment/environment.h"
#include "api/transport/stun.h"
#include "p2p/test/stun_server.h"
#include "rtc_base/async_udp_socket.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/socket_server.h"
#include "rtc_base/thread.h"

namespace webrtc {

// A test STUN server. Useful for unit tests.
class TestStunServer : StunServer {
 public:
  using StunServerPtr =
      std::unique_ptr<TestStunServer, std::function<void(TestStunServer*)>>;
  static StunServerPtr Create(const Environment& env,
                              const SocketAddress& addr,
                              SocketServer& ss,
                              Thread& network_thread
                                  ABSL_ATTRIBUTE_LIFETIME_BOUND);

  // Set a fake STUN address to return to the client.
  void set_fake_stun_addr(const SocketAddress& addr) { fake_stun_addr_ = addr; }

 private:
  static void DeleteOnNetworkThread(TestStunServer* server);

  TestStunServer(std::unique_ptr<AsyncUDPSocket> socket, Thread& network_thread)
      : StunServer(std::move(socket)), network_thread_(network_thread) {}

  void OnBindingRequest(StunMessage* msg,
                        const SocketAddress& remote_addr) override;

 private:
  SocketAddress fake_stun_addr_;
  Thread& network_thread_;
};

}  //  namespace webrtc


#endif  // P2P_TEST_TEST_STUN_SERVER_H_
