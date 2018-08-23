/*
 *  Copyright 2009 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <map>
#include <memory>

#include "p2p/base/basicpacketsocketfactory.h"
#include "p2p/base/relayport.h"
#include "p2p/base/relayserver.h"
#include "rtc_base/gunit.h"
#include "rtc_base/helpers.h"
#include "rtc_base/logging.h"
#include "rtc_base/socketadapters.h"
#include "rtc_base/socketaddress.h"
#include "rtc_base/ssladapter.h"
#include "rtc_base/thread.h"
#include "rtc_base/virtualsocketserver.h"

using rtc::SocketAddress;

static const SocketAddress kLocalAddress = SocketAddress("192.168.1.2", 0);
static const SocketAddress kRelayUdpAddr = SocketAddress("99.99.99.1", 5000);
static const SocketAddress kRelayTcpAddr = SocketAddress("99.99.99.2", 5001);
static const SocketAddress kRelaySslAddr = SocketAddress("99.99.99.3", 443);
static const SocketAddress kRelayExtAddr = SocketAddress("99.99.99.3", 5002);

static const int kTimeoutMs = 1000;
static const int kMaxTimeoutMs = 5000;

// Tests connecting a RelayPort to a fake relay server
// (cricket::RelayServer) using all currently available protocols. The
// network layer is faked out by using a VirtualSocketServer for
// creating sockets. The test will monitor the current state of the
// RelayPort and created sockets by listening for signals such as,
// SignalConnectFailure, SignalConnectTimeout, SignalSocketClosed and
// SignalReadPacket.
class RelayPortTest : public testing::Test, public sigslot::has_slots<> {
 public:
  RelayPortTest()
      : virtual_socket_server_(new rtc::VirtualSocketServer()),
        main_(virtual_socket_server_.get()),
        network_("unittest", "unittest", kLocalAddress.ipaddr(), 32),
        socket_factory_(rtc::Thread::Current()),
        username_(rtc::CreateRandomString(16)),
        password_(rtc::CreateRandomString(16)),
        relay_port_(cricket::RelayPort::Create(&main_,
                                               &socket_factory_,
                                               &network_,
                                               0,
                                               0,
                                               username_,
                                               password_)),
        relay_server_(new cricket::RelayServer(&main_)) {
    network_.AddIP(kLocalAddress.ipaddr());
  }

  void OnReadPacket(rtc::AsyncPacketSocket* socket,
                    const char* data,
                    size_t size,
                    const rtc::SocketAddress& remote_addr,
                    const rtc::PacketTime& packet_time) {
    received_packet_count_[socket]++;
  }

  void OnConnectFailure(const cricket::ProtocolAddress* addr) {
    failed_connections_.push_back(*addr);
  }

  void OnSoftTimeout(const cricket::ProtocolAddress* addr) {
    soft_timedout_connections_.push_back(*addr);
  }

 protected:
  virtual void SetUp() {
    // The relay server needs an external socket to work properly.
    rtc::AsyncUDPSocket* ext_socket = CreateAsyncUdpSocket(kRelayExtAddr);
    relay_server_->AddExternalSocket(ext_socket);

    // Listen for failures.
    relay_port_->SignalConnectFailure.connect(this,
                                              &RelayPortTest::OnConnectFailure);

    // Listen for soft timeouts.
    relay_port_->SignalSoftTimeout.connect(this, &RelayPortTest::OnSoftTimeout);
  }

  // Udp has the highest 'goodness' value of the three different
  // protocols used for connecting to the relay server. As soon as
  // PrepareAddress is called, the RelayPort will start trying to
  // connect to the given UDP address. As soon as a response to the
  // sent STUN allocate request message has been received, the
  // RelayPort will consider the connection to be complete and will
  // abort any other connection attempts.
  void TestConnectUdp() {
    // Add a UDP socket to the relay server.
    rtc::AsyncUDPSocket* internal_udp_socket =
        CreateAsyncUdpSocket(kRelayUdpAddr);
    rtc::AsyncSocket* server_socket = CreateServerSocket(kRelayTcpAddr);

    relay_server_->AddInternalSocket(internal_udp_socket);
    relay_server_->AddInternalServerSocket(server_socket, cricket::PROTO_TCP);

    // Now add our relay addresses to the relay port and let it start.
    relay_port_->AddServerAddress(
        cricket::ProtocolAddress(kRelayUdpAddr, cricket::PROTO_UDP));
    relay_port_->AddServerAddress(
        cricket::ProtocolAddress(kRelayTcpAddr, cricket::PROTO_TCP));
    relay_port_->PrepareAddress();

    // Should be connected.
    EXPECT_TRUE_WAIT(relay_port_->IsReady(), kTimeoutMs);

    // Make sure that we are happy with UDP, ie. not continuing with
    // TCP, SSLTCP, etc.
    WAIT(relay_server_->HasConnection(kRelayTcpAddr), kTimeoutMs);

    // Should have only one connection.
    EXPECT_EQ(1, relay_server_->GetConnectionCount());

    // Should be the UDP address.
    EXPECT_TRUE(relay_server_->HasConnection(kRelayUdpAddr));
  }

  // TCP has the second best 'goodness' value, and as soon as UDP
  // connection has failed, the RelayPort will attempt to connect via
  // TCP. Here we add a fake UDP address together with a real TCP
  // address to simulate an UDP failure. As soon as UDP has failed the
  // RelayPort will try the TCP adress and succed.
  void TestConnectTcp() {
    // Create a fake UDP address for relay port to simulate a failure.
    cricket::ProtocolAddress fake_protocol_address =
        cricket::ProtocolAddress(kRelayUdpAddr, cricket::PROTO_UDP);

    // Create a server socket for the RelayServer.
    rtc::AsyncSocket* server_socket = CreateServerSocket(kRelayTcpAddr);
    relay_server_->AddInternalServerSocket(server_socket, cricket::PROTO_TCP);

    // Add server addresses to the relay port and let it start.
    relay_port_->AddServerAddress(
        cricket::ProtocolAddress(fake_protocol_address));
    relay_port_->AddServerAddress(
        cricket::ProtocolAddress(kRelayTcpAddr, cricket::PROTO_TCP));
    relay_port_->PrepareAddress();

    EXPECT_FALSE(relay_port_->IsReady());

    // Should have timed out in 200 + 200 + 400 + 800 + 1600 ms = 3200ms.
    // Add some margin of error for slow bots.
    // TODO(deadbeef): Use simulated clock instead of just increasing timeouts
    // to fix flaky tests.
    EXPECT_TRUE_WAIT(HasFailed(&fake_protocol_address), 5000);

    // Wait until relayport is ready.
    EXPECT_TRUE_WAIT(relay_port_->IsReady(), kMaxTimeoutMs);

    // Should have only one connection.
    EXPECT_EQ(1, relay_server_->GetConnectionCount());

    // Should be the TCP address.
    EXPECT_TRUE(relay_server_->HasConnection(kRelayTcpAddr));
  }

  void TestConnectSslTcp() {
    // Create a fake TCP address for relay port to simulate a failure.
    // We skip UDP here since transition from UDP to TCP has been
    // tested above.
    cricket::ProtocolAddress fake_protocol_address =
        cricket::ProtocolAddress(kRelayTcpAddr, cricket::PROTO_TCP);

    // Create a ssl server socket for the RelayServer.
    rtc::AsyncSocket* ssl_server_socket = CreateServerSocket(kRelaySslAddr);
    relay_server_->AddInternalServerSocket(ssl_server_socket,
                                           cricket::PROTO_SSLTCP);

    // Create a tcp server socket that listens on the fake address so
    // the relay port can attempt to connect to it.
    std::unique_ptr<rtc::AsyncSocket> tcp_server_socket(
        CreateServerSocket(kRelayTcpAddr));

    // Add server addresses to the relay port and let it start.
    relay_port_->AddServerAddress(fake_protocol_address);
    relay_port_->AddServerAddress(
        cricket::ProtocolAddress(kRelaySslAddr, cricket::PROTO_SSLTCP));
    relay_port_->PrepareAddress();
    EXPECT_FALSE(relay_port_->IsReady());

    // Should have timed out in 3000 ms(relayport.cc, kSoftConnectTimeoutMs).
    EXPECT_TRUE_WAIT_MARGIN(HasTimedOut(&fake_protocol_address), 3000, 100);

    // Wait until relayport is ready.
    EXPECT_TRUE_WAIT(relay_port_->IsReady(), kMaxTimeoutMs);

    // Should have only one connection.
    EXPECT_EQ(1, relay_server_->GetConnectionCount());

    // Should be the SSLTCP address.
    EXPECT_TRUE(relay_server_->HasConnection(kRelaySslAddr));
  }

 private:
  rtc::AsyncUDPSocket* CreateAsyncUdpSocket(const SocketAddress addr) {
    rtc::AsyncSocket* socket =
        virtual_socket_server_->CreateAsyncSocket(AF_INET, SOCK_DGRAM);
    rtc::AsyncUDPSocket* packet_socket =
        rtc::AsyncUDPSocket::Create(socket, addr);
    EXPECT_TRUE(packet_socket != NULL);
    packet_socket->SignalReadPacket.connect(this, &RelayPortTest::OnReadPacket);
    return packet_socket;
  }

  rtc::AsyncSocket* CreateServerSocket(const SocketAddress addr) {
    rtc::AsyncSocket* socket =
        virtual_socket_server_->CreateAsyncSocket(AF_INET, SOCK_STREAM);
    EXPECT_GE(socket->Bind(addr), 0);
    EXPECT_GE(socket->Listen(5), 0);
    return socket;
  }

  bool HasFailed(cricket::ProtocolAddress* addr) {
    for (size_t i = 0; i < failed_connections_.size(); i++) {
      if (failed_connections_[i].address == addr->address &&
          failed_connections_[i].proto == addr->proto) {
        return true;
      }
    }
    return false;
  }

  bool HasTimedOut(cricket::ProtocolAddress* addr) {
    for (size_t i = 0; i < soft_timedout_connections_.size(); i++) {
      if (soft_timedout_connections_[i].address == addr->address &&
          soft_timedout_connections_[i].proto == addr->proto) {
        return true;
      }
    }
    return false;
  }

  typedef std::map<rtc::AsyncPacketSocket*, int> PacketMap;

  std::unique_ptr<rtc::VirtualSocketServer> virtual_socket_server_;
  rtc::AutoSocketServerThread main_;
  rtc::Network network_;
  rtc::BasicPacketSocketFactory socket_factory_;
  std::string username_;
  std::string password_;
  std::unique_ptr<cricket::RelayPort> relay_port_;
  std::unique_ptr<cricket::RelayServer> relay_server_;
  std::vector<cricket::ProtocolAddress> failed_connections_;
  std::vector<cricket::ProtocolAddress> soft_timedout_connections_;
  PacketMap received_packet_count_;
};

TEST_F(RelayPortTest, ConnectUdp) {
  TestConnectUdp();
}

TEST_F(RelayPortTest, ConnectTcp) {
  TestConnectTcp();
}

TEST_F(RelayPortTest, ConnectSslTcp) {
  TestConnectSslTcp();
}
