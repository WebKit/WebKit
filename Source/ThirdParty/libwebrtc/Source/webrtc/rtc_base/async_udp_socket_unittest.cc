/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/async_udp_socket.h"

#include <cstdint>
#include <memory>

#include "absl/memory/memory.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/socket.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/virtual_socket_server.h"
#include "test/gtest.h"

namespace rtc {

static const SocketAddress kAddr("22.22.22.22", 0);

TEST(AsyncUDPSocketTest, SetSocketOptionIfEctChange) {
  VirtualSocketServer socket_server;
  Socket* socket = socket_server.CreateSocket(kAddr.family(), SOCK_DGRAM);
  std::unique_ptr<AsyncUDPSocket> udp__socket =
      absl::WrapUnique(AsyncUDPSocket::Create(socket, kAddr));

  int ect = 0;
  socket->GetOption(Socket::OPT_SEND_ECN, &ect);
  ASSERT_EQ(ect, 0);

  uint8_t buffer[] = "hello";
  rtc::PacketOptions packet_options;
  packet_options.ecn_1 = false;
  udp__socket->SendTo(buffer, 5, kAddr, packet_options);
  socket->GetOption(Socket::OPT_SEND_ECN, &ect);
  EXPECT_EQ(ect, 0);

  packet_options.ecn_1 = true;
  udp__socket->SendTo(buffer, 5, kAddr, packet_options);
  socket->GetOption(Socket::OPT_SEND_ECN, &ect);
  EXPECT_EQ(ect, 1);

  packet_options.ecn_1 = false;
  udp__socket->SendTo(buffer, 5, kAddr, packet_options);
  socket->GetOption(Socket::OPT_SEND_ECN, &ect);
  EXPECT_EQ(ect, 0);
}

}  // namespace rtc
