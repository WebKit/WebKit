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

#include <memory>
#include <string>

#include "rtc_base/gunit.h"
#include "rtc_base/physical_socket_server.h"
#include "rtc_base/virtual_socket_server.h"

namespace rtc {

class AsyncUdpSocketTest : public ::testing::Test, public sigslot::has_slots<> {
 public:
  AsyncUdpSocketTest()
      : pss_(new rtc::PhysicalSocketServer),
        vss_(new rtc::VirtualSocketServer(pss_.get())),
        socket_(vss_->CreateSocket(SOCK_DGRAM)),
        udp_socket_(new AsyncUDPSocket(socket_)),
        ready_to_send_(false) {
    udp_socket_->SignalReadyToSend.connect(this,
                                           &AsyncUdpSocketTest::OnReadyToSend);
  }

  void OnReadyToSend(rtc::AsyncPacketSocket* socket) { ready_to_send_ = true; }

 protected:
  std::unique_ptr<PhysicalSocketServer> pss_;
  std::unique_ptr<VirtualSocketServer> vss_;
  Socket* socket_;
  std::unique_ptr<AsyncUDPSocket> udp_socket_;
  bool ready_to_send_;
};

TEST_F(AsyncUdpSocketTest, OnWriteEvent) {
  EXPECT_FALSE(ready_to_send_);
  socket_->SignalWriteEvent(socket_);
  EXPECT_TRUE(ready_to_send_);
}

}  // namespace rtc
