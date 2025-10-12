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

#include "rtc_base/async_packet_socket.h"
#include "rtc_base/net_helpers.h"
#include "rtc_base/socket.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "rtc_base/virtual_socket_server.h"
#include "test/gtest.h"

namespace webrtc {

class AsyncTCPSocketTest : public ::testing::Test, public sigslot::has_slots<> {
 public:
  AsyncTCPSocketTest()
      : vss_(new VirtualSocketServer()),
        socket_(vss_->CreateSocket(AF_INET, SOCK_STREAM)),
        tcp_socket_(new AsyncTCPSocket(socket_)),
        ready_to_send_(false) {
    tcp_socket_->SignalReadyToSend.connect(this,
                                           &AsyncTCPSocketTest::OnReadyToSend);
  }

  void OnReadyToSend(AsyncPacketSocket* socket) { ready_to_send_ = true; }

 protected:
  std::unique_ptr<VirtualSocketServer> vss_;
  Socket* socket_;
  std::unique_ptr<AsyncTCPSocket> tcp_socket_;
  bool ready_to_send_;
};

TEST_F(AsyncTCPSocketTest, OnWriteEvent) {
  EXPECT_FALSE(ready_to_send_);
  socket_->SignalWriteEvent(socket_);
  EXPECT_TRUE(ready_to_send_);
}

}  // namespace webrtc
