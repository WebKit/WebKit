/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "webrtc/base/gunit.h"
#include "webrtc/base/physicalsocketserver.h"
#include "webrtc/base/thread.h"
#include "webrtc/base/virtualsocketserver.h"
#include "webrtc/p2p/base/basicpacketsocketfactory.h"
#include "webrtc/p2p/base/tcpport.h"

using rtc::SocketAddress;
using cricket::Connection;
using cricket::Port;
using cricket::TCPPort;
using cricket::ICE_UFRAG_LENGTH;
using cricket::ICE_PWD_LENGTH;

static int kTimeout = 1000;
static const SocketAddress kLocalAddr("11.11.11.11", 1);
static const SocketAddress kRemoteAddr("22.22.22.22", 2);

class TCPPortTest : public testing::Test, public sigslot::has_slots<> {
 public:
  TCPPortTest()
      : main_(rtc::Thread::Current()),
        pss_(new rtc::PhysicalSocketServer),
        ss_(new rtc::VirtualSocketServer(pss_.get())),
        ss_scope_(ss_.get()),
        network_("unittest", "unittest", rtc::IPAddress(INADDR_ANY), 32),
        socket_factory_(rtc::Thread::Current()),
        username_(rtc::CreateRandomString(ICE_UFRAG_LENGTH)),
        password_(rtc::CreateRandomString(ICE_PWD_LENGTH)) {
    network_.AddIP(rtc::IPAddress(INADDR_ANY));
  }

  void ConnectSignalSocketCreated() {
    ss_->SignalSocketCreated.connect(this, &TCPPortTest::OnSocketCreated);
  }

  void OnSocketCreated(rtc::VirtualSocket* socket) {
    LOG(LS_INFO) << "socket created ";
    socket->SignalAddressReady.connect(
        this, &TCPPortTest::SetLocalhostAsAlternativeLocalAddress);
  }

  void SetLocalhostAsAlternativeLocalAddress(rtc::VirtualSocket* socket,
                                             const SocketAddress& address) {
    SocketAddress local_address("127.0.0.1", 2000);
    socket->SetAlternativeLocalAddress(local_address);
  }

  TCPPort* CreateTCPPort(const SocketAddress& addr) {
    return TCPPort::Create(main_, &socket_factory_, &network_, addr.ipaddr(), 0,
                           0, username_, password_, true);
  }

 protected:
  rtc::Thread* main_;
  std::unique_ptr<rtc::PhysicalSocketServer> pss_;
  std::unique_ptr<rtc::VirtualSocketServer> ss_;
  rtc::SocketServerScope ss_scope_;
  rtc::Network network_;
  rtc::BasicPacketSocketFactory socket_factory_;
  std::string username_;
  std::string password_;
};

TEST_F(TCPPortTest, TestTCPPortWithLocalhostAddress) {
  std::unique_ptr<TCPPort> lport(CreateTCPPort(kLocalAddr));
  std::unique_ptr<TCPPort> rport(CreateTCPPort(kRemoteAddr));
  lport->PrepareAddress();
  rport->PrepareAddress();
  // Start to listen to new socket creation event.
  ConnectSignalSocketCreated();
  Connection* conn =
      lport->CreateConnection(rport->Candidates()[0], Port::ORIGIN_MESSAGE);
  EXPECT_TRUE_WAIT(conn->connected(), kTimeout);
}
