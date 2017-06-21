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
      : ss_(new rtc::VirtualSocketServer()),
        main_(ss_.get()),
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
    return TCPPort::Create(&main_, &socket_factory_, &network_, addr.ipaddr(),
                           0, 0, username_, password_, true);
  }

 protected:
  std::unique_ptr<rtc::VirtualSocketServer> ss_;
  rtc::AutoSocketServerThread main_;
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

class SentPacketCounter : public sigslot::has_slots<> {
 public:
  SentPacketCounter(TCPPort* p) {
    p->SignalSentPacket.connect(this, &SentPacketCounter::OnSentPacket);
  }

  int sent_packets() const { return sent_packets_; }

 private:
  void OnSentPacket(const rtc::SentPacket&) { ++sent_packets_; }

  int sent_packets_ = 0;
};

// Test that SignalSentPacket is fired when a packet is successfully sent, for
// both TCP client and server sockets.
TEST_F(TCPPortTest, SignalSentPacket) {
  std::unique_ptr<TCPPort> client(CreateTCPPort(kLocalAddr));
  std::unique_ptr<TCPPort> server(CreateTCPPort(kRemoteAddr));
  client->SetIceRole(cricket::ICEROLE_CONTROLLING);
  server->SetIceRole(cricket::ICEROLE_CONTROLLED);
  client->PrepareAddress();
  server->PrepareAddress();

  Connection* client_conn =
      client->CreateConnection(server->Candidates()[0], Port::ORIGIN_MESSAGE);
  ASSERT_NE(nullptr, client_conn);
  ASSERT_TRUE_WAIT(client_conn->connected(), kTimeout);

  // Need to get the port of the actual outgoing socket, not the server socket..
  cricket::Candidate client_candidate = client->Candidates()[0];
  client_candidate.set_address(static_cast<cricket::TCPConnection*>(client_conn)
                                   ->socket()
                                   ->GetLocalAddress());
  Connection* server_conn =
      server->CreateConnection(client_candidate, Port::ORIGIN_THIS_PORT);
  ASSERT_NE(nullptr, server_conn);
  ASSERT_TRUE_WAIT(server_conn->connected(), kTimeout);

  client_conn->Ping(rtc::TimeMillis());
  server_conn->Ping(rtc::TimeMillis());
  ASSERT_TRUE_WAIT(client_conn->writable(), kTimeout);
  ASSERT_TRUE_WAIT(server_conn->writable(), kTimeout);

  SentPacketCounter client_counter(client.get());
  SentPacketCounter server_counter(server.get());
  static const char kData[] = "hello";
  for (int i = 0; i < 10; ++i) {
    client_conn->Send(&kData, sizeof(kData), rtc::PacketOptions());
    server_conn->Send(&kData, sizeof(kData), rtc::PacketOptions());
  }
  EXPECT_EQ_WAIT(10, client_counter.sent_packets(), kTimeout);
  EXPECT_EQ_WAIT(10, server_counter.sent_packets(), kTimeout);
}
