/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/tcp_port.h"

#include <cstdint>
#include <list>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "api/candidate.h"
#include "api/environment/environment.h"
#include "api/test/rtc_error_matchers.h"
#include "api/units/time_delta.h"
#include "p2p/base/basic_packet_socket_factory.h"
#include "p2p/base/connection.h"
#include "p2p/base/p2p_constants.h"
#include "p2p/base/port.h"
#include "p2p/base/transport_description.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/checks.h"
#include "rtc_base/crypto_random.h"
#include "rtc_base/ip_address.h"
#include "rtc_base/network.h"
#include "rtc_base/network/sent_packet.h"
#include "rtc_base/socket.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/virtual_socket_server.h"
#include "test/create_test_environment.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/run_loop.h"
#include "test/wait_until.h"

namespace webrtc {
namespace {

using ::testing::Eq;
using ::testing::IsTrue;

int kTimeout = 1000;
const SocketAddress kLocalAddr("11.11.11.11", 0);
const SocketAddress kLocalIPv6Addr("2401:fa00:4:1000:be30:5bff:fee5:c3", 0);
const SocketAddress kAlternateLocalAddr("1.2.3.4", 0);
const SocketAddress kRemoteAddr("22.22.22.22", 0);
const SocketAddress kRemoteIPv6Addr("2401:fa00:4:1000:be30:5bff:fee5:c4", 0);

constexpr uint64_t kTiebreakerDefault = 44444;

class ConnectionObserver {
 public:
  explicit ConnectionObserver(Connection* conn) : conn_(conn) {
    conn->SubscribeDestroyed(
        this, [this](Connection* connection) { OnDestroyed(connection); });
  }

  ~ConnectionObserver() {
    if (!connection_destroyed_) {
      RTC_DCHECK(conn_);
      conn_->UnsubscribeDestroyed(this);
    }
  }

  bool connection_destroyed() { return connection_destroyed_; }

 private:
  void OnDestroyed(Connection*) { connection_destroyed_ = true; }

  Connection* const conn_;
  bool connection_destroyed_ = false;
};

class TCPPortTest : public ::testing::Test {
 public:
  TCPPortTest()
      : ss_(new VirtualSocketServer()),
        main_(ss_.get()),
        socket_factory_(ss_.get()),
        username_(CreateRandomString(ICE_UFRAG_LENGTH)),
        password_(CreateRandomString(ICE_PWD_LENGTH)) {}

  Network* MakeNetwork(const SocketAddress& addr) {
    networks_.emplace_back("unittest", "unittest", addr.ipaddr(), 32);
    networks_.back().AddIP(addr.ipaddr());
    return &networks_.back();
  }

  std::unique_ptr<TCPPort> CreateTCPPort(const SocketAddress& addr,
                                         bool allow_listen = true,
                                         int port_number = 0) {
    return std::unique_ptr<TCPPort>(
        TCPPort::Create({.env = env_,
                         .network_thread = main_.task_queue(),
                         .socket_factory = &socket_factory_,
                         .network = MakeNetwork(addr),
                         .ice_username_fragment = username_,
                         .ice_password = password_,
                         .ice_tiebreaker = kTiebreakerDefault},
                        port_number, port_number, allow_listen));
  }

  std::unique_ptr<TCPPort> CreateTCPPort(const Network* network) {
    return std::unique_ptr<TCPPort>(
        TCPPort::Create({.env = env_,
                         .network_thread = main_.task_queue(),
                         .socket_factory = &socket_factory_,
                         .network = network,
                         .ice_username_fragment = username_,
                         .ice_password = password_,
                         .ice_tiebreaker = kTiebreakerDefault},
                        0, 0, true));
  }

 protected:
  const Environment env_ = CreateTestEnvironment();
  // When a "create port" helper method is called with an IP, we create a
  // Network with that IP and add it to this list. Using a list instead of a
  // vector so that when it grows, pointers aren't invalidated.
  std::list<Network> networks_;
  std::unique_ptr<VirtualSocketServer> ss_;
  test::RunLoop main_;
  BasicPacketSocketFactory socket_factory_;
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
  EXPECT_TRUE(WaitUntil([&] { return conn->connected(); },
                        {.timeout = TimeDelta::Millis(kTimeout)}));
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
  EXPECT_TRUE(WaitUntil([&] { return observer.connection_destroyed(); },
                        {.timeout = TimeDelta::Millis(kTimeout)}));
}

// A caveat for the above logic: if the socket ends up bound to one of the IPs
// associated with the Network, just not the "best" one, this is ok.
TEST_F(TCPPortTest, TCPPortNotDiscardedIfNotBoundToBestIP) {
  // Sockets bound to kLocalAddr will actually end up with kAlternateLocalAddr.
  ss_->SetAlternativeLocalAddress(kLocalAddr.ipaddr(),
                                  kAlternateLocalAddr.ipaddr());

  // Set up a network with kLocalAddr1 as the "best" IP, and kAlternateLocalAddr
  // as an alternate.
  Network* network = MakeNetwork(kLocalAddr);
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
  EXPECT_TRUE(WaitUntil([&] { return conn->connected(); },
                        {.timeout = TimeDelta::Millis(kTimeout)}));

  // Verify that the socket actually used the alternate address, otherwise this
  // test isn't doing what it meant to.
  ASSERT_EQ(kAlternateLocalAddr.ipaddr(),
            local_port->Candidates()[0].address().ipaddr());
}

// Regression test for crbug.com/webrtc/8972, caused by buggy comparison
// between IPAddress and InterfaceAddress.
TEST_F(TCPPortTest, TCPPortNotDiscardedIfBoundToTemporaryIP) {
  networks_.emplace_back("unittest", "unittest", kLocalIPv6Addr.ipaddr(), 32);
  networks_.back().AddIP(
      InterfaceAddress(kLocalIPv6Addr.ipaddr(), IPV6_ADDRESS_FLAG_TEMPORARY));

  auto local_port = CreateTCPPort(&networks_.back());
  auto remote_port = CreateTCPPort(kRemoteIPv6Addr);
  local_port->PrepareAddress();
  remote_port->PrepareAddress();

  // Connection should succeed if the port isn't discarded.
  Connection* conn = local_port->CreateConnection(remote_port->Candidates()[0],
                                                  Port::ORIGIN_MESSAGE);
  ASSERT_NE(nullptr, conn);
  EXPECT_TRUE(WaitUntil([&] { return conn->connected(); },
                        {.timeout = TimeDelta::Millis(kTimeout)}));
}

class SentPacketCounter {
 public:
  explicit SentPacketCounter(TCPPort* p) {
    p->SubscribeSentPacket(
        this, [this](const SentPacketInfo& info) { OnSentPacket(info); });
  }

  int sent_packets() const { return sent_packets_; }

 private:
  void OnSentPacket(const SentPacketInfo&) { ++sent_packets_; }

  int sent_packets_ = 0;
};

// Test that SignalSentPacket is fired when a packet is successfully sent, for
// both TCP client and server sockets.
TEST_F(TCPPortTest, SignalSentPacket) {
  std::unique_ptr<TCPPort> client(CreateTCPPort(kLocalAddr));
  std::unique_ptr<TCPPort> server(CreateTCPPort(kRemoteAddr));
  client->SetIceRole(ICEROLE_CONTROLLING);
  server->SetIceRole(ICEROLE_CONTROLLED);
  client->PrepareAddress();
  server->PrepareAddress();

  Connection* client_conn =
      client->CreateConnection(server->Candidates()[0], Port::ORIGIN_MESSAGE);
  ASSERT_NE(nullptr, client_conn);
  ASSERT_TRUE(WaitUntil([&] { return client_conn->connected(); },
                        {.timeout = TimeDelta::Millis(kTimeout)}));

  // Need to get the port of the actual outgoing socket, not the server socket..
  Candidate client_candidate = client->Candidates()[0];
  client_candidate.set_address(
      static_cast<TCPConnection*>(client_conn)->socket()->GetLocalAddress());
  Connection* server_conn =
      server->CreateConnection(client_candidate, Port::ORIGIN_THIS_PORT);
  ASSERT_NE(nullptr, server_conn);
  ASSERT_TRUE(WaitUntil([&] { return server_conn->connected(); },
                        {.timeout = TimeDelta::Millis(kTimeout)}));

  client_conn->Ping(env_.clock().CurrentTime());
  server_conn->Ping(env_.clock().CurrentTime());
  ASSERT_TRUE(WaitUntil([&] { return client_conn->writable(); },
                        {.timeout = TimeDelta::Millis(kTimeout)}));
  ASSERT_TRUE(WaitUntil([&] { return server_conn->writable(); },
                        {.timeout = TimeDelta::Millis(kTimeout)}));

  SentPacketCounter client_counter(client.get());
  SentPacketCounter server_counter(server.get());
  static constexpr uint8_t kData[] = {'h', 'e', 'l', 'l', 'o', '\0'};
  for (int i = 0; i < 10; ++i) {
    client_conn->Send(kData, AsyncSocketPacketOptions());
    server_conn->Send(kData, AsyncSocketPacketOptions());
  }
  EXPECT_THAT(WaitUntil([&] { return client_counter.sent_packets(); }, Eq(10),
                        {.timeout = TimeDelta::Millis(kTimeout)}),
              IsRtcOk());
  EXPECT_THAT(WaitUntil([&] { return server_counter.sent_packets(); }, Eq(10),
                        {.timeout = TimeDelta::Millis(kTimeout)}),
              IsRtcOk());
}

// Test that SignalSentPacket is fired when a packet is successfully sent, even
// after a remote server has been restarted.
TEST_F(TCPPortTest, SignalSentPacketAfterReconnect) {
  std::unique_ptr<TCPPort> client(
      CreateTCPPort(kLocalAddr, /*allow_listen=*/false));
  constexpr int kServerPort = 123;
  std::unique_ptr<TCPPort> server(
      CreateTCPPort(kRemoteAddr, /*allow_listen=*/true, kServerPort));
  client->SetIceRole(ICEROLE_CONTROLLING);
  server->SetIceRole(ICEROLE_CONTROLLED);
  client->PrepareAddress();
  server->PrepareAddress();

  Connection* client_conn =
      client->CreateConnection(server->Candidates()[0], Port::ORIGIN_MESSAGE);
  ASSERT_NE(nullptr, client_conn);
  ASSERT_TRUE(WaitUntil([&] { return client_conn->connected(); },
                        {.timeout = TimeDelta::Millis(kTimeout)}));

  // Need to get the port of the actual outgoing socket.
  Candidate client_candidate = client->Candidates()[0];
  client_candidate.set_address(
      static_cast<TCPConnection*>(client_conn)->socket()->GetLocalAddress());
  client_candidate.set_tcptype("");
  Connection* server_conn =
      server->CreateConnection(client_candidate, Port::ORIGIN_THIS_PORT);
  ASSERT_TRUE(WaitUntil([&] { return server_conn->connected(); },
                        {.timeout = TimeDelta::Millis(kTimeout)}));
  EXPECT_FALSE(client_conn->writable());
  client_conn->Ping(env_.clock().CurrentTime());
  ASSERT_TRUE(WaitUntil([&] { return client_conn->writable(); },
                        {.timeout = TimeDelta::Millis(kTimeout)}));

  SentPacketCounter client_counter(client.get());
  static constexpr uint8_t kData[] = {'h', 'e', 'l', 'l', 'o', '\0'};
  int result = client_conn->Send(kData, AsyncSocketPacketOptions());
  EXPECT_EQ(result, 6);

  // Deleting the server port should break the current connection.
  server = nullptr;
  server_conn = nullptr;
  ASSERT_TRUE(WaitUntil([&] { return !client_conn->connected(); },
                        {.timeout = TimeDelta::Millis(kTimeout)}));

  // Recreate the server port with the same port number.
  server = CreateTCPPort(kRemoteAddr, /*allow_listen=*/true, kServerPort);
  server->SetIceRole(ICEROLE_CONTROLLED);
  server->PrepareAddress();

  // Sending a packet from the client will trigger a reconnect attempt but the
  // packet will be discarded.
  result = client_conn->Send(kData, AsyncSocketPacketOptions());
  EXPECT_EQ(result, SOCKET_ERROR);
  ASSERT_TRUE(WaitUntil([&] { return client_conn->connected(); },
                        {.timeout = TimeDelta::Millis(kTimeout)}));
  // For unknown reasons, connection is still supposed to be writable....
  EXPECT_TRUE(client_conn->writable());
  for (int i = 0; i < 10; ++i) {
    // All sent packets still fail to send.
    EXPECT_EQ(client_conn->Send(kData, AsyncSocketPacketOptions()),
              SOCKET_ERROR);
  }
  // And are not reported as sent.
  EXPECT_THAT(WaitUntil([&] { return client_counter.sent_packets(); }, Eq(1),
                        {.timeout = TimeDelta::Millis(kTimeout)}),
              IsRtcOk());

  // Create the server connection again so server can reply to STUN pings.
  // Client outgoing socket port will have changed since the client create a new
  // socket when it reconnect.
  client_candidate = client->Candidates()[0];
  client_candidate.set_address(
      static_cast<TCPConnection*>(client_conn)->socket()->GetLocalAddress());
  client_candidate.set_tcptype("");
  server_conn =
      server->CreateConnection(client_candidate, Port::ORIGIN_THIS_PORT);
  ASSERT_TRUE(WaitUntil([&] { return server_conn->connected(); },
                        {.timeout = TimeDelta::Millis(kTimeout)}));
  EXPECT_THAT(WaitUntil([&] { return client_counter.sent_packets(); }, Eq(1),
                        {.timeout = TimeDelta::Millis(kTimeout)}),
              IsRtcOk());

  // Send Stun Binding request.
  client_conn->Ping(env_.clock().CurrentTime());
  // The Stun Binding request is reported as sent.
  EXPECT_THAT(WaitUntil([&] { return client_counter.sent_packets(); }, Eq(2),
                        {.timeout = TimeDelta::Millis(kTimeout)}),
              IsRtcOk());
  // Wait a bit for the Stun response to be received.
  main_.RunFor(TimeDelta::Millis(100));

  // After the Stun Ping response has been received, packets can be sent again
  // and SignalSentPacket should be invoked.
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(client_conn->Send(kData, AsyncSocketPacketOptions()), 6);
  }
  EXPECT_THAT(WaitUntil([&] { return client_counter.sent_packets(); },
                        Eq(2 + 5), {.timeout = TimeDelta::Millis(kTimeout)}),
              IsRtcOk());
}

}  // namespace
}  // namespace webrtc
