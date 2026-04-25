/*
 *  Copyright 2009 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/stun_port.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "api/candidate.h"
#include "api/environment/environment.h"
#include "api/environment/environment_factory.h"
#include "api/field_trials.h"
#include "api/field_trials_view.h"
#include "api/packet_socket_factory.h"
#include "api/test/mock_async_dns_resolver.h"
#include "api/test/rtc_error_matchers.h"
#include "api/transport/stun.h"
#include "api/units/time_delta.h"
#include "p2p/base/basic_packet_socket_factory.h"
#include "p2p/base/port.h"
#include "p2p/base/stun_request.h"
#include "p2p/test/mock_dns_resolving_packet_socket_factory.h"
#include "p2p/test/nat_server.h"
#include "p2p/test/nat_socket_factory.h"
#include "p2p/test/nat_types.h"
#include "p2p/test/test_stun_server.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/checks.h"
#include "rtc_base/crypto_random.h"
#include "rtc_base/dscp.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/gunit.h"
#include "rtc_base/ip_address.h"
#include "rtc_base/mdns_responder_interface.h"
#include "rtc_base/net_helpers.h"
#include "rtc_base/network.h"
#include "rtc_base/network/received_packet.h"
#include "rtc_base/network_constants.h"
#include "rtc_base/socket.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/socket_factory.h"
#include "rtc_base/socket_server.h"
#include "rtc_base/thread.h"
#include "rtc_base/virtual_socket_server.h"
#include "system_wrappers/include/metrics.h"
#include "test/create_test_environment.h"
#include "test/create_test_field_trials.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/wait_until.h"

namespace webrtc {
namespace {

using ::testing::_;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsTrue;
using ::testing::Return;
using ::testing::ReturnPointee;
using ::testing::SetArgPointee;

const SocketAddress kPrivateIP("192.168.1.12", 0);
const SocketAddress kMsdnAddress("unittest-mdns-host-name.local", 0);
const SocketAddress kPublicIP("212.116.91.133", 0);
const SocketAddress kNatAddr(kPublicIP.ipaddr(), webrtc::NAT_SERVER_UDP_PORT);
const SocketAddress kStunServerAddr1("34.38.54.120", 5000);
const SocketAddress kStunServerAddr2("34.38.54.120", 4000);

const SocketAddress kPrivateIPv6("2001:4860:4860::8844", 0);
const SocketAddress kPublicIPv6("2002:4860:4860::8844", 5000);
const SocketAddress kNatAddrIPv6(kPublicIPv6.ipaddr(),
                                 webrtc::NAT_SERVER_UDP_PORT);
const SocketAddress kStunServerAddrIPv6Addr("2003:4860:4860::8844", 5000);

const SocketAddress kBadAddr("0.0.0.1", 5000);
const SocketAddress kIPv6BadAddr("::ffff:0:1", 5000);
const SocketAddress kValidHostnameAddr("valid-hostname", 5000);
const SocketAddress kBadHostnameAddr("not-a-real-hostname", 5000);
// STUN timeout (with all retries) is webrtc::STUN_TOTAL_TIMEOUT.
// Add some margin of error for slow bots.
constexpr int kTimeoutMs = webrtc::STUN_TOTAL_TIMEOUT;
// stun prio = 100 (srflx) << 24 | 30 (IPv4) << 8 | 256 - 1 (component)
constexpr uint32_t kStunCandidatePriority = (100 << 24) | (30 << 8) | (256 - 1);
// stun prio = 100 (srflx) << 24 | 40 (IPv6) << 8 | 256 - 1 (component)
constexpr uint32_t kIPv6StunCandidatePriority =
    (100 << 24) | (40 << 8) | (256 - 1);
constexpr uint64_t kTiebreakerDefault = 44444;

struct IPAddressTypeTestConfig {
  absl::string_view address;
  webrtc::IPAddressType address_type;
};

// Used by the test framework to print the param value for parameterized tests.
std::string PrintToString(const IPAddressTypeTestConfig& param) {
  return std::string(param.address);
}

class FakeMdnsResponder : public webrtc::MdnsResponderInterface {
 public:
  void CreateNameForAddress(const webrtc::IPAddress& addr,
                            NameCreatedCallback callback) override {
    callback(addr, kMsdnAddress.HostAsSensitiveURIString());
  }

  void RemoveNameForAddress(const webrtc::IPAddress& addr,
                            NameRemovedCallback callback) override {}
};

class FakeMdnsResponderProvider : public webrtc::MdnsResponderProvider {
 public:
  FakeMdnsResponderProvider() : mdns_responder_(new FakeMdnsResponder()) {}

  webrtc::MdnsResponderInterface* GetMdnsResponder() const override {
    return mdns_responder_.get();
  }

 private:
  std::unique_ptr<webrtc::MdnsResponderInterface> mdns_responder_;
};

// Base class for tests connecting a StunPort to a fake STUN server
// (webrtc::StunServer).
class StunPortTestBase : public ::testing::Test {
 public:
  StunPortTestBase()
      : StunPortTestBase(kPrivateIP.ipaddr(),
                         {kStunServerAddr1, kStunServerAddr2},
                         kNatAddr) {}

  StunPortTestBase(const webrtc::IPAddress address,
                   const std::set<webrtc::SocketAddress>& stun_server_addresses,
                   const webrtc::SocketAddress& nat_server_address)
      : env_(CreateTestEnvironment()),
        ss_(new webrtc::VirtualSocketServer()),
        thread_(ss_.get()),
        nat_factory_(ss_.get(), nat_server_address, nat_server_address),
        nat_socket_factory_(&nat_factory_),
        mdns_responder_provider_(new FakeMdnsResponderProvider()),
        nat_server_(CreateNatServer(nat_server_address, webrtc::NAT_OPEN_CONE)),
        done_(false),
        error_(false),
        stun_keepalive_delay_(TimeDelta::Millis(1)) {
    network_ = MakeNetwork(address);
    RTC_CHECK(address.family() == nat_server_address.family());
    for (const auto& addr : stun_server_addresses) {
      RTC_CHECK(addr.family() == address.family());
      stun_servers_.push_back(
          webrtc::TestStunServer::Create(env_, addr, *ss_, thread_));
    }
  }

  std::unique_ptr<webrtc::NATServer> CreateNatServer(const SocketAddress& addr,
                                                     webrtc::NATType type) {
    return std::make_unique<webrtc::NATServer>(
        env_, type, thread_, ss_.get(), addr, addr, thread_, ss_.get(), addr);
  }

  virtual webrtc::PacketSocketFactory* socket_factory() {
    return &nat_socket_factory_;
  }

  webrtc::SocketServer* ss() const { return ss_.get(); }
  webrtc::UDPPort* port() const { return stun_port_.get(); }
  webrtc::AsyncPacketSocket* socket() const { return socket_.get(); }
  bool done() const { return done_; }
  bool error() const { return error_; }

  bool HasPendingRequest(int msg_type) {
    return stun_port_->request_manager().HasRequestForTest(msg_type);
  }

  void SetNetworkType(webrtc::AdapterType adapter_type) {
    network_->set_type(adapter_type);
  }

  void CreateStunPort(const webrtc::SocketAddress& server_addr,
                      const webrtc::FieldTrialsView* field_trials = nullptr) {
    ServerAddresses stun_servers;
    stun_servers.insert(server_addr);
    CreateStunPort(stun_servers, field_trials);
  }

  void CreateStunPort(const ServerAddresses& stun_servers,
                      const webrtc::FieldTrialsView* field_trials = nullptr) {
    // Overwrite field trials if provided.
    EnvironmentFactory env_factory(env_);
    env_factory.Set(field_trials);
    Environment env = env_factory.Create();

    stun_port_ = webrtc::StunPort::Create(
        {.env = env,
         .network_thread = &thread_,
         .socket_factory = socket_factory(),
         .network = network_,
         .ice_username_fragment = webrtc::CreateRandomString(16),
         .ice_password = webrtc::CreateRandomString(22)},
        0, 0, stun_servers, std::nullopt);
    stun_port_->SetIceTiebreaker(kTiebreakerDefault);
    stun_port_->set_stun_keepalive_delay(stun_keepalive_delay_);
    // If `stun_keepalive_lifetime_` is not set, let the stun port choose its
    // lifetime from the network type.
    if (stun_keepalive_lifetime_.has_value()) {
      stun_port_->set_stun_keepalive_lifetime(*stun_keepalive_lifetime_);
    }
    stun_port_->SubscribePortComplete(
        this, [this](Port* port) { OnPortComplete(port); });
    stun_port_->SubscribePortError(this,
                                   [this](Port* port) { OnPortError(port); });

    stun_port_->SubscribeCandidateError(
        this, [this](Port* port, const IceCandidateErrorEvent& event) {
          OnCandidateError(port, event);
        });
  }

  void CreateSharedUdpPort(const SocketAddress& server_addr,
                           std::unique_ptr<AsyncPacketSocket> socket) {
    // Destroy existing stun_port_, if any, before overwriting socket_.
    if (stun_port_) {
      stun_port_ = nullptr;
    }
    if (socket) {
      socket_ = std::move(socket);
    } else {
      socket_ = socket_factory()->CreateUdpSocket(
          env_, SocketAddress(kPrivateIP.ipaddr(), 0), 0, 0);
    }
    ASSERT_TRUE(socket_ != nullptr);
    socket_->RegisterReceivedPacketCallback(
        [&](webrtc::AsyncPacketSocket* socket,
            const webrtc::ReceivedIpPacket& packet) {
          OnReadPacket(socket, packet);
        });
    ServerAddresses stun_servers;
    stun_servers.insert(server_addr);
    stun_port_ = webrtc::UDPPort::Create(
        {.env = env_,
         .network_thread = &thread_,
         .socket_factory = socket_factory(),
         .network = network_,
         .ice_username_fragment = webrtc::CreateRandomString(16),
         .ice_password = webrtc::CreateRandomString(22)},
        socket_.get(), false, std::nullopt);
    stun_port_->set_server_addresses(stun_servers);
    ASSERT_TRUE(stun_port_ != nullptr);
    stun_port_->SetIceTiebreaker(kTiebreakerDefault);
    stun_port_->SubscribePortComplete(
        this, [this](Port* port) { OnPortComplete(port); });
    stun_port_->SubscribePortError(this,
                                   [this](Port* port) { OnPortError(port); });
  }

  void PrepareAddress() { stun_port_->PrepareAddress(); }

  void OnReadPacket(webrtc::AsyncPacketSocket* socket,
                    const webrtc::ReceivedIpPacket& packet) {
    stun_port_->HandleIncomingPacket(socket, packet);
  }

  void SendData(const char* data, size_t len) {
    stun_port_->HandleIncomingPacket(
        socket_.get(), webrtc::ReceivedIpPacket::CreateFromLegacy(
                           data, len, /* packet_time_us */ -1,
                           webrtc::SocketAddress("22.22.22.22", 0)));
  }

  void EnableMdnsObfuscation() {
    network_->set_mdns_responder_provider(mdns_responder_provider_.get());
  }

 protected:
  void OnPortComplete(webrtc::Port* /* port */) {
    ASSERT_FALSE(done_);
    done_ = true;
    error_ = false;
  }
  void OnPortError(webrtc::Port* /* port */) {
    done_ = true;
    error_ = true;
  }
  void OnCandidateError(webrtc::Port* /* port */,
                        const webrtc::IceCandidateErrorEvent& event) {
    error_event_ = event;
  }
  void SetKeepaliveDelay(TimeDelta delay) { stun_keepalive_delay_ = delay; }

  void SetKeepaliveLifetime(TimeDelta lifetime) {
    stun_keepalive_lifetime_ = lifetime;
  }

  webrtc::Network* MakeNetwork(const webrtc::IPAddress& addr) {
    networks_.emplace_back(
        std::make_unique<Network>("unittest", "unittest", addr, 32));
    networks_.back()->AddIP(addr);
    return networks_.back().get();
  }

  webrtc::TestStunServer* stun_server_1() { return stun_servers_[0].get(); }
  webrtc::TestStunServer* stun_server_2() { return stun_servers_[1].get(); }

  webrtc::AutoSocketServerThread& thread() { return thread_; }
  webrtc::SocketFactory* nat_factory() { return &nat_factory_; }

 private:
  const Environment env_;
  std::vector<std::unique_ptr<webrtc::Network>> networks_;
  webrtc::Network* network_;

  std::unique_ptr<webrtc::VirtualSocketServer> ss_;
  webrtc::AutoSocketServerThread thread_;
  webrtc::NATSocketFactory nat_factory_;
  webrtc::BasicPacketSocketFactory nat_socket_factory_;
  // Note that stun_port_ can refer to socket_, so must be destroyed
  // before it.
  std::unique_ptr<webrtc::AsyncPacketSocket> socket_;
  std::unique_ptr<webrtc::UDPPort> stun_port_;
  std::vector<webrtc::TestStunServer::StunServerPtr> stun_servers_;
  std::unique_ptr<webrtc::MdnsResponderProvider> mdns_responder_provider_;
  std::unique_ptr<webrtc::NATServer> nat_server_;
  bool done_;
  bool error_;
  TimeDelta stun_keepalive_delay_;
  std::optional<TimeDelta> stun_keepalive_lifetime_;

 protected:
  webrtc::IceCandidateErrorEvent error_event_;
};

class StunPortTestWithRealClock : public StunPortTestBase {};

class FakeClockBase {
 public:
  webrtc::ScopedFakeClock fake_clock;
};

class StunPortTest : public FakeClockBase, public StunPortTestBase {};

// Test that we can create a STUN port.
TEST_F(StunPortTest, TestCreateStunPort) {
  CreateStunPort(kStunServerAddr1);
  EXPECT_EQ(IceCandidateType::kSrflx, port()->Type());
  EXPECT_EQ(0U, port()->Candidates().size());
}

// Test that we can create a UDP port.
TEST_F(StunPortTest, TestCreateUdpPort) {
  CreateSharedUdpPort(kStunServerAddr1, nullptr);
  EXPECT_EQ(IceCandidateType::kHost, port()->Type());
  EXPECT_EQ(0U, port()->Candidates().size());
}

// Test that we can get an address from a STUN server.
TEST_F(StunPortTest, TestPrepareAddress) {
  CreateStunPort(kStunServerAddr1);
  PrepareAddress();
  EXPECT_THAT(
      webrtc::WaitUntil([&] { return done(); }, IsTrue(),
                        {.timeout = webrtc::TimeDelta::Millis(kTimeoutMs),
                         .clock = &fake_clock}),
      webrtc::IsRtcOk());
  ASSERT_EQ(1U, port()->Candidates().size());
  EXPECT_TRUE(kPublicIP.EqualIPs(port()->Candidates()[0].address()));
  std::string expected_server_url = "stun:" + kStunServerAddr1.ToString();
  EXPECT_EQ(port()->Candidates()[0].url(), expected_server_url);
}

// Test that we fail properly if we can't get an address.
TEST_F(StunPortTest, TestPrepareAddressFail) {
  CreateStunPort(kBadAddr);
  PrepareAddress();
  EXPECT_THAT(
      webrtc::WaitUntil([&] { return done(); }, IsTrue(),
                        {.timeout = webrtc::TimeDelta::Millis(kTimeoutMs),
                         .clock = &fake_clock}),
      webrtc::IsRtcOk());
  EXPECT_TRUE(error());
  EXPECT_EQ(0U, port()->Candidates().size());
  EXPECT_THAT(
      webrtc::WaitUntil([&] { return error_event_.error_code; },
                        Eq(webrtc::STUN_ERROR_SERVER_NOT_REACHABLE),
                        {.timeout = webrtc::TimeDelta::Millis(kTimeoutMs),
                         .clock = &fake_clock}),
      webrtc::IsRtcOk());
  EXPECT_NE(error_event_.error_text.find('.'), std::string::npos);
  EXPECT_NE(error_event_.address.find(kPrivateIP.HostAsSensitiveURIString()),
            std::string::npos);
  std::string server_url = "stun:" + kBadAddr.ToString();
  EXPECT_EQ(error_event_.url, server_url);
}

// Test that we fail without emitting an error if we try to get an address from
// a STUN server with a different address family. IPv4 local, IPv6 STUN.
TEST_F(StunPortTest, TestServerAddressFamilyMismatch) {
  CreateStunPort(kStunServerAddrIPv6Addr);
  PrepareAddress();
  EXPECT_THAT(
      webrtc::WaitUntil([&] { return done(); }, IsTrue(),
                        {.timeout = webrtc::TimeDelta::Millis(kTimeoutMs),
                         .clock = &fake_clock}),
      webrtc::IsRtcOk());
  EXPECT_TRUE(error());
  EXPECT_EQ(0U, port()->Candidates().size());
  EXPECT_EQ(0, error_event_.error_code);
}

class StunPortWithMockDnsResolverTest : public StunPortTest {
 public:
  StunPortWithMockDnsResolverTest()
      : StunPortTest(), socket_factory_(nat_factory()) {}

  webrtc::PacketSocketFactory* socket_factory() override {
    return &socket_factory_;
  }

  void SetDnsResolverExpectations(
      webrtc::MockDnsResolvingPacketSocketFactory::Expectations expectations) {
    socket_factory_.SetExpectations(expectations);
  }

 private:
  webrtc::MockDnsResolvingPacketSocketFactory socket_factory_;
};

// Test that we can get an address from a STUN server specified by a hostname.
TEST_F(StunPortWithMockDnsResolverTest, TestPrepareAddressHostname) {
  SetDnsResolverExpectations(
      [](webrtc::MockAsyncDnsResolver* resolver,
         webrtc::MockAsyncDnsResolverResult* resolver_result) {
        EXPECT_CALL(*resolver, Start(kValidHostnameAddr, /*family=*/AF_INET, _))
            .WillOnce([](const webrtc::SocketAddress& /* addr */,
                         int /* family */,
                         absl::AnyInvocable<void()> callback) { callback(); });

        EXPECT_CALL(*resolver, result)
            .WillRepeatedly(ReturnPointee(resolver_result));
        EXPECT_CALL(*resolver_result, GetError).WillOnce(Return(0));
        EXPECT_CALL(*resolver_result, GetResolvedAddress(AF_INET, _))
            .WillOnce(DoAll(SetArgPointee<1>(kStunServerAddr1), Return(true)));
      });
  CreateStunPort(kValidHostnameAddr);
  PrepareAddress();
  EXPECT_THAT(
      webrtc::WaitUntil([&] { return done(); }, IsTrue(),
                        {.timeout = webrtc::TimeDelta::Millis(kTimeoutMs),
                         .clock = &fake_clock}),
      webrtc::IsRtcOk());
  ASSERT_EQ(1U, port()->Candidates().size());
  EXPECT_TRUE(kPublicIP.EqualIPs(port()->Candidates()[0].address()));
  EXPECT_EQ(kStunCandidatePriority, port()->Candidates()[0].priority());
}

TEST_F(StunPortWithMockDnsResolverTest,
       TestPrepareAddressHostnameWithPriorityAdjustment) {
  FieldTrials field_trials = CreateTestFieldTrials(
      "WebRTC-IncreaseIceCandidatePriorityHostSrflx/Enabled/");
  SetDnsResolverExpectations(
      [](webrtc::MockAsyncDnsResolver* resolver,
         webrtc::MockAsyncDnsResolverResult* resolver_result) {
        EXPECT_CALL(*resolver, Start(kValidHostnameAddr, /*family=*/AF_INET, _))
            .WillOnce([](const webrtc::SocketAddress& /* addr */,
                         int /* family */,
                         absl::AnyInvocable<void()> callback) { callback(); });
        EXPECT_CALL(*resolver, result)
            .WillRepeatedly(ReturnPointee(resolver_result));
        EXPECT_CALL(*resolver_result, GetError).WillOnce(Return(0));
        EXPECT_CALL(*resolver_result, GetResolvedAddress(AF_INET, _))
            .WillOnce(DoAll(SetArgPointee<1>(kStunServerAddr1), Return(true)));
      });
  CreateStunPort(kValidHostnameAddr, &field_trials);
  PrepareAddress();
  EXPECT_THAT(
      webrtc::WaitUntil([&] { return done(); }, IsTrue(),
                        {.timeout = webrtc::TimeDelta::Millis(kTimeoutMs),
                         .clock = &fake_clock}),
      webrtc::IsRtcOk());
  ASSERT_EQ(1U, port()->Candidates().size());
  EXPECT_TRUE(kPublicIP.EqualIPs(port()->Candidates()[0].address()));
  EXPECT_EQ(kStunCandidatePriority + (webrtc::kMaxTurnServers << 8),
            port()->Candidates()[0].priority());
}

// Test that we handle hostname lookup failures properly.
TEST_F(StunPortTestWithRealClock, TestPrepareAddressHostnameFail) {
  CreateStunPort(kBadHostnameAddr);
  PrepareAddress();
  EXPECT_THAT(
      webrtc::WaitUntil([&] { return done(); }, IsTrue(),
                        {.timeout = webrtc::TimeDelta::Millis(kTimeoutMs)}),
      webrtc::IsRtcOk());
  EXPECT_TRUE(error());
  EXPECT_EQ(0U, port()->Candidates().size());
  EXPECT_THAT(
      webrtc::WaitUntil([&] { return error_event_.error_code; },
                        Eq(webrtc::STUN_ERROR_SERVER_NOT_REACHABLE),
                        {.timeout = webrtc::TimeDelta::Millis(kTimeoutMs)}),
      webrtc::IsRtcOk());
}

// This test verifies keepalive response messages don't result in
// additional candidate generation.
TEST_F(StunPortTest, TestKeepAliveResponse) {
  SetKeepaliveDelay(TimeDelta::Millis(500));
  CreateStunPort(kStunServerAddr1);
  PrepareAddress();
  EXPECT_THAT(
      webrtc::WaitUntil([&] { return done(); }, IsTrue(),
                        {.timeout = webrtc::TimeDelta::Millis(kTimeoutMs),
                         .clock = &fake_clock}),
      webrtc::IsRtcOk());
  ASSERT_EQ(1U, port()->Candidates().size());
  EXPECT_TRUE(kPublicIP.EqualIPs(port()->Candidates()[0].address()));
  SIMULATED_WAIT(false, 1000, fake_clock);
  EXPECT_EQ(1U, port()->Candidates().size());
}

// Test that a local candidate can be generated using a shared socket.
TEST_F(StunPortTest, TestSharedSocketPrepareAddress) {
  CreateSharedUdpPort(kStunServerAddr1, nullptr);
  PrepareAddress();
  EXPECT_THAT(
      webrtc::WaitUntil([&] { return done(); }, IsTrue(),
                        {.timeout = webrtc::TimeDelta::Millis(kTimeoutMs),
                         .clock = &fake_clock}),
      webrtc::IsRtcOk());
  ASSERT_EQ(2U, port()->Candidates().size());
  EXPECT_EQ(port()->Candidates()[0].type(), IceCandidateType::kHost);
  EXPECT_TRUE(kPrivateIP.EqualIPs(port()->Candidates()[0].address()));
  EXPECT_EQ(port()->Candidates()[1].type(), IceCandidateType::kSrflx);
  EXPECT_TRUE(kPublicIP.EqualIPs(port()->Candidates()[1].address()));
}

// Test that we still get a local candidate with invalid stun server hostname.
// Also verifing that UDPPort can receive packets when stun address can't be
// resolved.
TEST_F(StunPortTestWithRealClock,
       TestSharedSocketPrepareAddressInvalidHostname) {
  CreateSharedUdpPort(kBadHostnameAddr, nullptr);
  PrepareAddress();
  EXPECT_THAT(
      webrtc::WaitUntil([&] { return done(); }, IsTrue(),
                        {.timeout = webrtc::TimeDelta::Millis(kTimeoutMs)}),
      webrtc::IsRtcOk());
  ASSERT_EQ(1U, port()->Candidates().size());
  EXPECT_TRUE(kPrivateIP.EqualIPs(port()->Candidates()[0].address()));

  // Send data to port after it's ready. This is to make sure, UDP port can
  // handle data with unresolved stun server address.
  std::string data = "some random data, sending to webrtc::Port.";
  SendData(data.c_str(), data.length());
  // No crash is success.
}

// Test that a stun candidate (srflx candidate) is generated whose address is
// equal to that of a local candidate if mDNS obfuscation is enabled.
TEST_F(StunPortTest, TestStunCandidateGeneratedWithMdnsObfuscationEnabled) {
  EnableMdnsObfuscation();
  CreateSharedUdpPort(kStunServerAddr1, nullptr);
  PrepareAddress();
  EXPECT_THAT(
      webrtc::WaitUntil([&] { return done(); }, IsTrue(),
                        {.timeout = webrtc::TimeDelta::Millis(kTimeoutMs),
                         .clock = &fake_clock}),
      webrtc::IsRtcOk());
  ASSERT_EQ(2U, port()->Candidates().size());

  // One of the generated candidates is a local candidate and the other is a
  // stun candidate.
  EXPECT_NE(port()->Candidates()[0].type(), port()->Candidates()[1].type());
  if (port()->Candidates()[0].is_local()) {
    EXPECT_EQ(kMsdnAddress.HostAsSensitiveURIString(),
              port()->Candidates()[0].address().HostAsSensitiveURIString());
    EXPECT_TRUE(port()->Candidates()[1].is_stun());
    EXPECT_TRUE(kPublicIP.EqualIPs(port()->Candidates()[1].address()));
  } else {
    EXPECT_TRUE(port()->Candidates()[0].is_stun());
    EXPECT_TRUE(kPublicIP.EqualIPs(port()->Candidates()[0].address()));
    EXPECT_TRUE(port()->Candidates()[1].is_local());
    EXPECT_EQ(kMsdnAddress.HostAsSensitiveURIString(),
              port()->Candidates()[1].address().HostAsSensitiveURIString());
  }
}

// Test that the same address is added only once if two STUN servers are in
// use.
TEST_F(StunPortTest, TestNoDuplicatedAddressWithTwoStunServers) {
  ServerAddresses stun_servers;
  stun_servers.insert(kStunServerAddr1);
  stun_servers.insert(kStunServerAddr2);
  CreateStunPort(stun_servers);
  EXPECT_EQ(IceCandidateType::kSrflx, port()->Type());
  PrepareAddress();
  EXPECT_THAT(
      webrtc::WaitUntil([&] { return done(); }, IsTrue(),
                        {.timeout = webrtc::TimeDelta::Millis(kTimeoutMs),
                         .clock = &fake_clock}),
      webrtc::IsRtcOk());
  EXPECT_EQ(1U, port()->Candidates().size());
  EXPECT_EQ(port()->Candidates()[0].relay_protocol(), "");
}

// Test that candidates can be allocated for multiple STUN servers, one of
// which is not reachable.
TEST_F(StunPortTest, TestMultipleStunServersWithBadServer) {
  ServerAddresses stun_servers;
  stun_servers.insert(kStunServerAddr1);
  stun_servers.insert(kBadAddr);
  CreateStunPort(stun_servers);
  EXPECT_EQ(IceCandidateType::kSrflx, port()->Type());
  PrepareAddress();
  EXPECT_THAT(
      webrtc::WaitUntil([&] { return done(); }, IsTrue(),
                        {.timeout = webrtc::TimeDelta::Millis(kTimeoutMs),
                         .clock = &fake_clock}),
      webrtc::IsRtcOk());
  EXPECT_EQ(1U, port()->Candidates().size());
  std::string server_url = "stun:" + kBadAddr.ToString();
  ASSERT_THAT(
      webrtc::WaitUntil([&] { return error_event_.url; }, Eq(server_url),
                        {.timeout = webrtc::TimeDelta::Millis(kTimeoutMs),
                         .clock = &fake_clock}),
      webrtc::IsRtcOk());
}

// Test that two candidates are allocated if the two STUN servers return
// different mapped addresses.
TEST_F(StunPortTest, TestTwoCandidatesWithTwoStunServersAcrossNat) {
  const SocketAddress kStunMappedAddr1("77.77.77.77", 0);
  const SocketAddress kStunMappedAddr2("88.77.77.77", 0);
  stun_server_1()->set_fake_stun_addr(kStunMappedAddr1);
  stun_server_2()->set_fake_stun_addr(kStunMappedAddr2);

  ServerAddresses stun_servers;
  stun_servers.insert(kStunServerAddr1);
  stun_servers.insert(kStunServerAddr2);
  CreateStunPort(stun_servers);
  EXPECT_EQ(IceCandidateType::kSrflx, port()->Type());
  PrepareAddress();
  EXPECT_THAT(
      webrtc::WaitUntil([&] { return done(); }, IsTrue(),
                        {.timeout = webrtc::TimeDelta::Millis(kTimeoutMs),
                         .clock = &fake_clock}),
      webrtc::IsRtcOk());
  EXPECT_EQ(2U, port()->Candidates().size());
  EXPECT_EQ(port()->Candidates()[0].relay_protocol(), "");
  EXPECT_EQ(port()->Candidates()[1].relay_protocol(), "");
}

// Test that the stun_keepalive_lifetime is set correctly based on the network
// type on a STUN port. Also test that it will be updated if the network type
// changes.
TEST_F(StunPortTest, TestStunPortGetStunKeepaliveLifetime) {
  // Lifetime for the default (unknown) network type is infinite.
  CreateStunPort(kStunServerAddr1);
  EXPECT_EQ(port()->stun_keepalive_lifetime(), TimeDelta::PlusInfinity());
  // Lifetime for the cellular network is `kHighCostPortKeepaliveLifetime`
  SetNetworkType(webrtc::ADAPTER_TYPE_CELLULAR);
  EXPECT_EQ(port()->stun_keepalive_lifetime(), kHighCostPortKeepaliveLifetime);

  // Lifetime for the wifi network is infinite.
  SetNetworkType(webrtc::ADAPTER_TYPE_WIFI);
  CreateStunPort(kStunServerAddr2);
  EXPECT_EQ(port()->stun_keepalive_lifetime(), TimeDelta::PlusInfinity());
}

// Test that the stun_keepalive_lifetime is set correctly based on the network
// type on a shared STUN port (UDPPort). Also test that it will be updated
// if the network type changes.
TEST_F(StunPortTest, TestUdpPortGetStunKeepaliveLifetime) {
  // Lifetime for the default (unknown) network type is infinite.
  CreateSharedUdpPort(kStunServerAddr1, nullptr);
  EXPECT_EQ(port()->stun_keepalive_lifetime(), TimeDelta::PlusInfinity());
  // Lifetime for the cellular network is `kHighCostPortKeepaliveLifetime`.
  SetNetworkType(webrtc::ADAPTER_TYPE_CELLULAR);
  EXPECT_EQ(port()->stun_keepalive_lifetime(), kHighCostPortKeepaliveLifetime);

  // Lifetime for the wifi network type is infinite.
  SetNetworkType(webrtc::ADAPTER_TYPE_WIFI);
  CreateSharedUdpPort(kStunServerAddr2, nullptr);
  EXPECT_EQ(port()->stun_keepalive_lifetime(), TimeDelta::PlusInfinity());
}

// Test that STUN binding requests will be stopped shortly if the keep-alive
// lifetime is short.
TEST_F(StunPortTest, TestStunBindingRequestShortLifetime) {
  SetKeepaliveDelay(TimeDelta::Millis(101));
  SetKeepaliveLifetime(TimeDelta::Millis(100));
  CreateStunPort(kStunServerAddr1);
  PrepareAddress();
  EXPECT_THAT(
      webrtc::WaitUntil([&] { return done(); }, IsTrue(),
                        {.timeout = webrtc::TimeDelta::Millis(kTimeoutMs),
                         .clock = &fake_clock}),
      webrtc::IsRtcOk());
  EXPECT_THAT(
      webrtc::WaitUntil(
          [&] { return !HasPendingRequest(webrtc::STUN_BINDING_REQUEST); },
          IsTrue(), {.clock = &fake_clock}),
      webrtc::IsRtcOk());
}

// Test that by default, the STUN binding requests will last for a long time.
TEST_F(StunPortTest, TestStunBindingRequestLongLifetime) {
  SetKeepaliveDelay(TimeDelta::Millis(101));
  CreateStunPort(kStunServerAddr1);
  PrepareAddress();
  EXPECT_THAT(
      webrtc::WaitUntil([&] { return done(); }, IsTrue(),
                        {.timeout = webrtc::TimeDelta::Millis(kTimeoutMs),
                         .clock = &fake_clock}),
      webrtc::IsRtcOk());
  EXPECT_THAT(
      webrtc::WaitUntil(
          [&] { return HasPendingRequest(webrtc::STUN_BINDING_REQUEST); },
          IsTrue(), {.clock = &fake_clock}),
      webrtc::IsRtcOk());
}

class StunPortIPAddressTypeMetricsTest
    : public StunPortWithMockDnsResolverTest,
      public ::testing::WithParamInterface<IPAddressTypeTestConfig> {};

TEST_P(StunPortIPAddressTypeMetricsTest, TestIPAddressTypeMetrics) {
  SetDnsResolverExpectations(
      [](webrtc::MockAsyncDnsResolver* resolver,
         webrtc::MockAsyncDnsResolverResult* resolver_result) {
        EXPECT_CALL(*resolver, Start(SocketAddress("localhost", 5000),
                                     /*family=*/AF_INET, _))
            .WillOnce([](const webrtc::SocketAddress& /* addr */,
                         int /* family */,
                         absl::AnyInvocable<void()> callback) { callback(); });

        EXPECT_CALL(*resolver, result)
            .WillRepeatedly(ReturnPointee(resolver_result));
        EXPECT_CALL(*resolver_result, GetError).WillOnce(Return(0));
        EXPECT_CALL(*resolver_result, GetResolvedAddress(AF_INET, _))
            .WillOnce(DoAll(SetArgPointee<1>(SocketAddress("127.0.0.1", 5000)),
                            Return(true)));
      });

  webrtc::metrics::Reset();

  CreateStunPort({GetParam().address, 5000});
  PrepareAddress();
  EXPECT_THAT(
      webrtc::WaitUntil([&] { return done(); }, IsTrue(),
                        {.timeout = webrtc::TimeDelta::Millis(kTimeoutMs),
                         .clock = &fake_clock}),
      webrtc::IsRtcOk());

  auto samples =
      webrtc::metrics::Samples("WebRTC.PeerConnection.Stun.ServerAddressType");
  ASSERT_EQ(samples.size(), 1u);
  EXPECT_EQ(samples[static_cast<int>(GetParam().address_type)], 1);
}

const IPAddressTypeTestConfig kAllIPAddressTypeTestConfigs[] = {
    {.address = "127.0.0.1", .address_type = webrtc::IPAddressType::kLoopback},
    {.address = "localhost", .address_type = webrtc::IPAddressType::kLoopback},
    {.address = "10.0.0.3", .address_type = webrtc::IPAddressType::kPrivate},
    {.address = "1.1.1.1", .address_type = webrtc::IPAddressType::kPublic},
};

INSTANTIATE_TEST_SUITE_P(All,
                         StunPortIPAddressTypeMetricsTest,
                         ::testing::ValuesIn(kAllIPAddressTypeTestConfigs));

class MockAsyncPacketSocket : public webrtc::AsyncPacketSocket {
 public:
  ~MockAsyncPacketSocket() override = default;

  MOCK_METHOD(SocketAddress, GetLocalAddress, (), (const, override));
  MOCK_METHOD(SocketAddress, GetRemoteAddress, (), (const, override));
  MOCK_METHOD(int,
              Send,
              (const void* pv,
               size_t cb,
               const webrtc::AsyncSocketPacketOptions& options),
              (override));

  MOCK_METHOD(int,
              SendTo,
              (const void* pv,
               size_t cb,
               const SocketAddress& addr,
               const webrtc::AsyncSocketPacketOptions& options),
              (override));
  MOCK_METHOD(int, Close, (), (override));
  MOCK_METHOD(State, GetState, (), (const, override));
  MOCK_METHOD(int,
              GetOption,
              (webrtc::Socket::Option opt, int* value),
              (override));
  MOCK_METHOD(int,
              SetOption,
              (webrtc::Socket::Option opt, int value),
              (override));
  MOCK_METHOD(int, GetError, (), (const, override));
  MOCK_METHOD(void, SetError, (int error), (override));
};

// Test that outbound packets inherit the dscp value assigned to the socket.
TEST_F(StunPortTest, TestStunPacketsHaveDscpPacketOption) {
  auto mocked_socket = std::make_unique<MockAsyncPacketSocket>();
  MockAsyncPacketSocket& socket = *mocked_socket;
  CreateSharedUdpPort(kStunServerAddr1, std::move(mocked_socket));

  EXPECT_CALL(socket, GetLocalAddress).WillRepeatedly(Return(kPrivateIP));
  EXPECT_CALL(socket, GetState)
      .WillRepeatedly(Return(AsyncPacketSocket::STATE_BOUND));
  EXPECT_CALL(socket, SetOption).WillRepeatedly(Return(0));

  // If DSCP is not set on the socket, stun packets should have no value.
  EXPECT_CALL(socket, SendTo(_, _, _,
                             Field(&AsyncSocketPacketOptions::dscp,
                                   Eq(DSCP_NO_CHANGE))))
      .WillOnce(Return(100));
  PrepareAddress();

  // Once it is set transport wide, they should inherit that value.
  port()->SetOption(Socket::OPT_DSCP, DSCP_AF41);
  EXPECT_CALL(
      socket,
      SendTo(_, _, _, Field(&AsyncSocketPacketOptions::dscp, Eq(DSCP_AF41))))
      .WillRepeatedly(Return(100));

  EXPECT_TRUE(WaitUntil(
      [&] { return done(); },
      {.timeout = TimeDelta::Millis(kTimeoutMs), .clock = &fake_clock}));
}

class StunIPv6PortTestBase : public StunPortTestBase {
 public:
  StunIPv6PortTestBase()
      : StunPortTestBase(kPrivateIPv6.ipaddr(),
                         {kStunServerAddrIPv6Addr},
                         kNatAddrIPv6) {}
};

class StunIPv6PortTestWithRealClock : public StunIPv6PortTestBase {};

class StunIPv6PortTest : public FakeClockBase, public StunIPv6PortTestBase {};

// Test that we can get an address from a STUN server.
TEST_F(StunIPv6PortTest, TestPrepareAddress) {
  CreateStunPort(kStunServerAddrIPv6Addr);
  PrepareAddress();
  EXPECT_THAT(
      webrtc::WaitUntil([&] { return done(); }, IsTrue(),
                        {.timeout = webrtc::TimeDelta::Millis(kTimeoutMs),
                         .clock = &fake_clock}),
      webrtc::IsRtcOk());
  ASSERT_EQ(1U, port()->Candidates().size());
  EXPECT_TRUE(kPublicIPv6.EqualIPs(port()->Candidates()[0].address()));
  std::string expected_server_url = "stun:2003:4860:4860::8844:5000";
  EXPECT_EQ(port()->Candidates()[0].url(), expected_server_url);
}

// Test that we fail properly if we can't get an address.
TEST_F(StunIPv6PortTest, TestPrepareAddressFail) {
  CreateStunPort(kIPv6BadAddr);
  PrepareAddress();
  EXPECT_THAT(
      webrtc::WaitUntil([&] { return done(); }, IsTrue(),
                        {.timeout = webrtc::TimeDelta::Millis(kTimeoutMs),
                         .clock = &fake_clock}),
      webrtc::IsRtcOk());
  EXPECT_TRUE(error());
  EXPECT_EQ(0U, port()->Candidates().size());
  EXPECT_THAT(
      webrtc::WaitUntil([&] { return error_event_.error_code; },
                        Eq(webrtc::STUN_ERROR_SERVER_NOT_REACHABLE),
                        {.timeout = webrtc::TimeDelta::Millis(kTimeoutMs),
                         .clock = &fake_clock}),
      webrtc::IsRtcOk());
  EXPECT_NE(error_event_.error_text.find('.'), std::string::npos);
  EXPECT_NE(error_event_.address.find(kPrivateIPv6.HostAsSensitiveURIString()),
            std::string::npos);
  std::string server_url = "stun:" + kIPv6BadAddr.ToString();
  EXPECT_EQ(error_event_.url, server_url);
}

// Test that we fail without emitting an error if we try to get an address from
// a STUN server with a different address family. IPv6 local, IPv4 STUN.
TEST_F(StunIPv6PortTest, TestServerAddressFamilyMismatch) {
  CreateStunPort(kStunServerAddr1);
  PrepareAddress();
  EXPECT_THAT(
      webrtc::WaitUntil([&] { return done(); }, IsTrue(),
                        {.timeout = webrtc::TimeDelta::Millis(kTimeoutMs),
                         .clock = &fake_clock}),
      webrtc::IsRtcOk());
  EXPECT_TRUE(error());
  EXPECT_EQ(0U, port()->Candidates().size());
  EXPECT_EQ(0, error_event_.error_code);
}

// Test that we handle hostname lookup failures properly with a real clock.
TEST_F(StunIPv6PortTestWithRealClock, TestPrepareAddressHostnameFail) {
  CreateStunPort(kBadHostnameAddr);
  PrepareAddress();
  EXPECT_THAT(
      webrtc::WaitUntil([&] { return done(); }, IsTrue(),
                        {.timeout = webrtc::TimeDelta::Millis(kTimeoutMs)}),
      webrtc::IsRtcOk());
  EXPECT_TRUE(error());
  EXPECT_EQ(0U, port()->Candidates().size());
  EXPECT_THAT(
      webrtc::WaitUntil([&] { return error_event_.error_code; },
                        Eq(webrtc::STUN_ERROR_SERVER_NOT_REACHABLE),
                        {.timeout = webrtc::TimeDelta::Millis(kTimeoutMs)}),
      webrtc::IsRtcOk());
}

class StunIPv6PortTestWithMockDnsResolver : public StunIPv6PortTest {
 public:
  StunIPv6PortTestWithMockDnsResolver()
      : StunIPv6PortTest(), socket_factory_(ss()) {}

  webrtc::PacketSocketFactory* socket_factory() override {
    return &socket_factory_;
  }

  void SetDnsResolverExpectations(
      webrtc::MockDnsResolvingPacketSocketFactory::Expectations expectations) {
    socket_factory_.SetExpectations(expectations);
  }

 private:
  webrtc::MockDnsResolvingPacketSocketFactory socket_factory_;
};

// Test that we can get an address from a STUN server specified by a hostname.
TEST_F(StunIPv6PortTestWithMockDnsResolver, TestPrepareAddressHostname) {
  SetDnsResolverExpectations(
      [](webrtc::MockAsyncDnsResolver* resolver,
         webrtc::MockAsyncDnsResolverResult* resolver_result) {
        EXPECT_CALL(*resolver,
                    Start(kValidHostnameAddr, /*family=*/AF_INET6, _))
            .WillOnce([](const webrtc::SocketAddress& addr, int family,
                         absl::AnyInvocable<void()> callback) { callback(); });

        EXPECT_CALL(*resolver, result)
            .WillRepeatedly(ReturnPointee(resolver_result));
        EXPECT_CALL(*resolver_result, GetError).WillOnce(Return(0));
        EXPECT_CALL(*resolver_result, GetResolvedAddress(AF_INET6, _))
            .WillOnce(
                DoAll(SetArgPointee<1>(kStunServerAddrIPv6Addr), Return(true)));
      });
  CreateStunPort(kValidHostnameAddr);
  PrepareAddress();
  EXPECT_THAT(
      webrtc::WaitUntil([&] { return done(); }, IsTrue(),
                        {.timeout = webrtc::TimeDelta::Millis(kTimeoutMs),
                         .clock = &fake_clock}),
      webrtc::IsRtcOk());
  ASSERT_EQ(1U, port()->Candidates().size());
  EXPECT_TRUE(kPrivateIPv6.EqualIPs(port()->Candidates()[0].address()));
  EXPECT_EQ(kIPv6StunCandidatePriority, port()->Candidates()[0].priority());
}

// Same as before but with a field trial that changes the priority.
TEST_F(StunIPv6PortTestWithMockDnsResolver,
       TestPrepareAddressHostnameWithPriorityAdjustment) {
  FieldTrials field_trials = CreateTestFieldTrials(
      "WebRTC-IncreaseIceCandidatePriorityHostSrflx/Enabled/");
  SetDnsResolverExpectations(
      [](webrtc::MockAsyncDnsResolver* resolver,
         webrtc::MockAsyncDnsResolverResult* resolver_result) {
        EXPECT_CALL(*resolver,
                    Start(kValidHostnameAddr, /*family=*/AF_INET6, _))
            .WillOnce([](const webrtc::SocketAddress& addr, int family,
                         absl::AnyInvocable<void()> callback) { callback(); });
        EXPECT_CALL(*resolver, result)
            .WillRepeatedly(ReturnPointee(resolver_result));
        EXPECT_CALL(*resolver_result, GetError).WillOnce(Return(0));
        EXPECT_CALL(*resolver_result, GetResolvedAddress(AF_INET6, _))
            .WillOnce(
                DoAll(SetArgPointee<1>(kStunServerAddrIPv6Addr), Return(true)));
      });
  CreateStunPort(kValidHostnameAddr, &field_trials);
  PrepareAddress();
  EXPECT_THAT(
      webrtc::WaitUntil([&] { return done(); }, IsTrue(),
                        {.timeout = webrtc::TimeDelta::Millis(kTimeoutMs),
                         .clock = &fake_clock}),
      webrtc::IsRtcOk());
  ASSERT_EQ(1U, port()->Candidates().size());
  EXPECT_TRUE(kPrivateIPv6.EqualIPs(port()->Candidates()[0].address()));
  EXPECT_EQ(kIPv6StunCandidatePriority + (webrtc::kMaxTurnServers << 8),
            port()->Candidates()[0].priority());
}

class StunIPv6PortIPAddressTypeMetricsTest
    : public StunIPv6PortTestWithMockDnsResolver,
      public ::testing::WithParamInterface<IPAddressTypeTestConfig> {};

TEST_P(StunIPv6PortIPAddressTypeMetricsTest, TestIPAddressTypeMetrics) {
  webrtc::metrics::Reset();

  CreateStunPort({GetParam().address, 5000});
  PrepareAddress();
  EXPECT_THAT(
      webrtc::WaitUntil([&] { return done(); }, IsTrue(),
                        {.timeout = webrtc::TimeDelta::Millis(kTimeoutMs),
                         .clock = &fake_clock}),
      webrtc::IsRtcOk());

  auto samples =
      webrtc::metrics::Samples("WebRTC.PeerConnection.Stun.ServerAddressType");
  ASSERT_EQ(samples.size(), 1u);
  EXPECT_EQ(samples[static_cast<int>(GetParam().address_type)], 1);
}

const IPAddressTypeTestConfig kAllIPv6AddressTypeTestConfigs[] = {
    {.address = "::1", .address_type = webrtc::IPAddressType::kLoopback},
    {.address = "fd00:4860:4860::8844",
     .address_type = webrtc::IPAddressType::kPrivate},
    {.address = "2001:4860:4860::8888",
     .address_type = webrtc::IPAddressType::kPublic},
};

INSTANTIATE_TEST_SUITE_P(All,
                         StunIPv6PortIPAddressTypeMetricsTest,
                         ::testing::ValuesIn(kAllIPv6AddressTypeTestConfigs));

}  // namespace
}  // namespace webrtc
