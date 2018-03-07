/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <list>
#include <memory>

#include "p2p/base/basicpacketsocketfactory.h"
#include "p2p/base/tcpport.h"
#include "rtc_base/gunit.h"
#include "rtc_base/thread.h"
#include "rtc_base/virtualsocketserver.h"

using rtc::SocketAddress;
using cricket::Connection;
using cricket::Port;
using cricket::TCPPort;
using cricket::ICE_UFRAG_LENGTH;
using cricket::ICE_PWD_LENGTH;

static int kTimeout = 1000;
static const SocketAddress kLocalAddr("11.11.11.11", 0);
static const SocketAddress kAlternateLocalAddr("1.2.3.4", 0);
static const SocketAddress kRemoteAddr("22.22.22.22", 0);

class ConnectionObserver : public sigslot::has_slots<> {
 public:
  ConnectionObserver(Connection* conn) {
    conn->SignalDestroyed.connect(this, &ConnectionObserver::OnDestroyed);
  }

  bool connection_destroyed() { return connection_destroyed_; }

 private:
  void OnDestroyed(Connection*) { connection_destroyed_ = true; }

  bool connection_destroyed_ = false;
};

class TCPPortTest : public testing::Test, public sigslot::has_slots<> {
 public:
  TCPPortTest()
      : ss_(new rtc::VirtualSocketServer()),
        main_(ss_.get()),
        socket_factory_(rtc::Thread::Current()),
        username_(rtc::CreateRandomString(ICE_UFRAG_LENGTH)),
        password_(rtc::CreateRandomString(ICE_PWD_LENGTH)) {
  }

  rtc::Network* MakeNetwork(const SocketAddress& addr) {
    networks_.emplace_back("unittest", "unittest", addr.ipaddr(), 32);
    networks_.back().AddIP(addr.ipaddr());
    return &networks_.back();
  }

  std::unique_ptr<TCPPort> CreateTCPPort(const SocketAddress& addr) {
    return std::unique_ptr<TCPPort>(
        TCPPort::Create(&main_, &socket_factory_, MakeNetwork(addr), 0, 0,
                        username_, password_, true));
  }

  std::unique_ptr<TCPPort> CreateTCPPort(rtc::Network* network) {
    return std::unique_ptr<TCPPort>(TCPPort::Create(
        &main_, &socket_factory_, network, 0, 0, username_, password_, true));
  }

 protected:
  // When a "create port" helper method is called with an IP, we create a
  // Network with that IP and add it to this list. Using a list instead of a
  // vector so that when it grows, pointers aren't invalidated.
  std::list<rtc::Network> networks_;
  std::unique_ptr<rtc::VirtualSocketServer> ss_;
  rtc::AutoSocketServerThread main_;
  rtc::BasicPacketSocketFactory socket_factory_;
  std::string username_;
  std::string password_;
};

TEST_F(TCPPortTest, TestTCPPortWithLocalhostAddress) {
  SocketAddress local_address("127.0.0.1", 0);
  // After calling this, when TCPPort attempts to get a socket bound to
  // kLocalAddr, it will end up using localhost instead.
  ss_->SetAlternativeLocalAddress(kLocalAddr.ipaddr(), local_address.ipaddr());
  auto local_port = CreateTCPPort(kLocalAddr);
  auto remote_port = CreateTCPPort(kRemoteAddr);
  local_port->PrepareAddress();
  remote_port->PrepareAddress();
  Connection* conn = local_port->CreateConnection(remote_port->Candidates()[0],
                                                  Port::ORIGIN_MESSAGE);
  EXPECT_TRUE_WAIT(conn->connected(), kTimeout);
  // Verify that the socket actually used localhost, otherwise this test isn't
  // doing what it meant to.
  ASSERT_EQ(local_address.ipaddr(),
            local_port->Candidates()[0].address().ipaddr());
}

// If the address the socket ends up bound to does not match any address of the
// TCPPort's Network, then the socket should be discarded and no candidates
// should be signaled. In the context of ICE, where one TCPPort is created for
// each Network, when this happens it's likely that the unexpected address is
// associated with some other Network, which another TCPPort is already
// covering.
TEST_F(TCPPortTest, TCPPortDiscardedIfBoundAddressDoesNotMatchNetwork) {
  // Sockets bound to kLocalAddr will actually end up with kAlternateLocalAddr.
  ss_->SetAlternativeLocalAddress(kLocalAddr.ipaddr(),
                                  kAlternateLocalAddr.ipaddr());

  // Create ports (local_port is the one whose IP will end up reassigned).
  auto local_port = CreateTCPPort(kLocalAddr);
  auto remote_port = CreateTCPPort(kRemoteAddr);
  local_port->PrepareAddress();
  remote_port->PrepareAddress();

  // Tell port to create a connection; it should be destroyed when it's
  // realized that it's using an unexpected address.
  Connection* conn = local_port->CreateConnection(remote_port->Candidates()[0],
                                                  Port::ORIGIN_MESSAGE);
  ConnectionObserver observer(conn);
  EXPECT_TRUE_WAIT(observer.connection_destroyed(), kTimeout);
}

// A caveat for the above logic: if the socket ends up bound to one of the IPs
// associated with the Network, just not the "best" one, this is ok.
TEST_F(TCPPortTest, TCPPortNotDiscardedIfNotBoundToBestIP) {
  // Sockets bound to kLocalAddr will actually end up with kAlternateLocalAddr.
  ss_->SetAlternativeLocalAddress(kLocalAddr.ipaddr(),
                                  kAlternateLocalAddr.ipaddr());

  // Set up a network with kLocalAddr1 as the "best" IP, and kAlternateLocalAddr
  // as an alternate.
  rtc::Network* network = MakeNetwork(kLocalAddr);
  network->AddIP(kAlternateLocalAddr.ipaddr());
  ASSERT_EQ(kLocalAddr.ipaddr(), network->GetBestIP());

  // Create ports (using our special 2-IP Network for local_port).
  auto local_port = CreateTCPPort(network);
  auto remote_port = CreateTCPPort(kRemoteAddr);
  local_port->PrepareAddress();
  remote_port->PrepareAddress();

  // Expect connection to succeed.
  Connection* conn = local_port->CreateConnection(remote_port->Candidates()[0],
                                                  Port::ORIGIN_MESSAGE);
  EXPECT_TRUE_WAIT(conn->connected(), kTimeout);

  // Verify that the socket actually used the alternate address, otherwise this
  // test isn't doing what it meant to.
  ASSERT_EQ(kAlternateLocalAddr.ipaddr(),
            local_port->Candidates()[0].address().ipaddr());
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
