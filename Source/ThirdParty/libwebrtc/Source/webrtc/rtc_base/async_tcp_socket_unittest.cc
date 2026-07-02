/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/async_tcp_socket.h"

#include <memory>
#include <utility>

#include "rtc_base/async_packet_socket.h"
#include "rtc_base/net_helpers.h"
#include "rtc_base/socket.h"
#include "rtc_base/virtual_socket_server.h"
#include "test/create_test_environment.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::NotNull;

struct AsyncTCPSocketObserver {
  void OnReadyToSend(AsyncPacketSocket* socket) { ready_to_send = true; }

  bool ready_to_send = false;
};

TEST(AsyncTCPSocketTest, OnWriteEvent) {
  VirtualSocketServer vss;
  std::unique_ptr<Socket> socket = vss.Create(AF_INET, SOCK_STREAM);
  ASSERT_THAT(socket, NotNull());
  Socket& socket_ref = *socket;
  AsyncTCPSocketObserver observer;
  AsyncTCPSocket tcp_socket(webrtc::CreateTestEnvironment(), std::move(socket));
  tcp_socket.SubscribeReadyToSend(&observer,
                                  [&observer](AsyncPacketSocket* socket) {
                                    observer.OnReadyToSend(socket);
                                  });

  EXPECT_FALSE(observer.ready_to_send);
  socket_ref.NotifyWriteEvent(&socket_ref);
  EXPECT_TRUE(observer.ready_to_send);
}

}  // namespace
}  // namespace webrtc
