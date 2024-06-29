/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/stunprober/stun_prober.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "p2p/base/basic_packet_socket_factory.h"
#include "p2p/base/test_stun_server.h"
#include "rtc_base/gunit.h"
#include "rtc_base/ip_address.h"
#include "rtc_base/ssl_adapter.h"
#include "rtc_base/virtual_socket_server.h"
#include "test/gtest.h"

using stunprober::AsyncCallback;
using stunprober::StunProber;

namespace stunprober {

namespace {

const rtc::SocketAddress kLocalAddr("192.168.0.1", 0);
const rtc::SocketAddress kStunAddr1("1.1.1.1", 3478);
const rtc::SocketAddress kStunAddr2("1.1.1.2", 3478);
const rtc::SocketAddress kFailedStunAddr("1.1.1.3", 3478);
const rtc::SocketAddress kStunMappedAddr("77.77.77.77", 0);

}  // namespace

class StunProberTest : public ::testing::Test {
 public:
  StunProberTest()
      : ss_(std::make_unique<rtc::VirtualSocketServer>()),
        main_(ss_.get()),
        result_(StunProber::SUCCESS),
        stun_server_1_(
            cricket::TestStunServer::Create(ss_.get(), kStunAddr1, main_)),
        stun_server_2_(
            cricket::TestStunServer::Create(ss_.get(), kStunAddr2, main_)) {
    stun_server_1_->set_fake_stun_addr(kStunMappedAddr);
    stun_server_2_->set_fake_stun_addr(kStunMappedAddr);
    rtc::InitializeSSL();
  }

  static constexpr int pings_per_ip = 3;

  void set_expected_result(int result) { result_ = result; }

  void CreateProber(rtc::PacketSocketFactory* socket_factory,
                    std::vector<const rtc::Network*> networks) {
    prober_ = std::make_unique<StunProber>(socket_factory, &main_,
                                           std::move(networks));
  }

  void StartProbing(rtc::PacketSocketFactory* socket_factory,
                    const std::vector<rtc::SocketAddress>& addrs,
                    std::vector<const rtc::Network*> networks,
                    bool shared_socket,
                    uint16_t interval,
                    uint16_t pings_per_ip) {
    CreateProber(socket_factory, networks);
    prober_->Start(addrs, shared_socket, interval, pings_per_ip,
                   100 /* timeout_ms */,
                   [this](StunProber* prober, int result) {
                     StopCallback(prober, result);
                   });
  }

  void RunProber(bool shared_mode) {
    std::vector<rtc::SocketAddress> addrs;
    addrs.push_back(kStunAddr1);
    addrs.push_back(kStunAddr2);
    // Add a non-existing server. This shouldn't pollute the result.
    addrs.push_back(kFailedStunAddr);
    RunProber(shared_mode, addrs, /* check_results= */ true);
  }

  void RunProber(bool shared_mode,
                 const std::vector<rtc::SocketAddress>& addrs,
                 bool check_results) {
    rtc::Network ipv4_network1("test_eth0", "Test Network Adapter 1",
                               rtc::IPAddress(0x12345600U), 24);
    ipv4_network1.AddIP(rtc::IPAddress(0x12345678));
    std::vector<const rtc::Network*> networks;
    networks.push_back(&ipv4_network1);

    auto socket_factory =
        std::make_unique<rtc::BasicPacketSocketFactory>(ss_.get());

    // Set up the expected results for verification.
    std::set<std::string> srflx_addresses;
    srflx_addresses.insert(kStunMappedAddr.ToString());
    const uint32_t total_pings_tried =
        static_cast<uint32_t>(pings_per_ip * addrs.size());

    // The reported total_pings should not count for pings sent to the
    // kFailedStunAddr.
    const uint32_t total_pings_reported = total_pings_tried - pings_per_ip;

    StartProbing(socket_factory.get(), addrs, std::move(networks), shared_mode,
                 3, pings_per_ip);

    WAIT(stopped_, 1000);

    EXPECT_TRUE(prober_->GetStats(&stats_));
    if (check_results) {
      EXPECT_EQ(stats_.success_percent, 100);
      EXPECT_TRUE(stats_.nat_type > stunprober::NATTYPE_NONE);
      EXPECT_EQ(stats_.srflx_addrs, srflx_addresses);
      EXPECT_EQ(static_cast<uint32_t>(stats_.num_request_sent),
                total_pings_reported);
      EXPECT_EQ(static_cast<uint32_t>(stats_.num_response_received),
                total_pings_reported);
    }
  }

  StunProber* prober() { return prober_.get(); }
  StunProber::Stats& stats() { return stats_; }

 private:
  void StopCallback(StunProber* prober, int result) {
    EXPECT_EQ(result, result_);
    stopped_ = true;
  }

  std::unique_ptr<rtc::VirtualSocketServer> ss_;
  rtc::AutoSocketServerThread main_;
  std::unique_ptr<StunProber> prober_;
  int result_ = 0;
  bool stopped_ = false;
  cricket::TestStunServer::StunServerPtr stun_server_1_;
  cricket::TestStunServer::StunServerPtr stun_server_2_;
  StunProber::Stats stats_;
};

TEST_F(StunProberTest, NonSharedMode) {
  RunProber(false);
}

TEST_F(StunProberTest, SharedMode) {
  RunProber(true);
}

TEST_F(StunProberTest, ResolveNonexistentHostname) {
  std::vector<rtc::SocketAddress> addrs;
  addrs.push_back(kStunAddr1);
  // Add a non-existing server by name. This should cause a failed lookup.
  addrs.push_back(rtc::SocketAddress("nonexistent.test", 3478));
  RunProber(false, addrs, false);
  // One server is pinged
  EXPECT_EQ(stats().raw_num_request_sent, pings_per_ip);
}

TEST_F(StunProberTest, ResolveExistingHostname) {
  std::vector<rtc::SocketAddress> addrs;
  addrs.push_back(kStunAddr1);
  // Add a non-existing server by name. This should cause a failed lookup.
  addrs.push_back(rtc::SocketAddress("localhost", 3478));
  RunProber(false, addrs, false);
  // Two servers are pinged, only one responds.
  // TODO(bugs.webrtc.org/15559): Figure out why this doesn't always work
  // EXPECT_EQ(stats().raw_num_request_sent, pings_per_ip * 2);
  EXPECT_EQ(stats().num_request_sent, pings_per_ip);
}

}  // namespace stunprober
