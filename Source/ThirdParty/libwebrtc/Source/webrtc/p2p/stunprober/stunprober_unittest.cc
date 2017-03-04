/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdint.h>

#include <memory>

#include "webrtc/base/asyncresolverinterface.h"
#include "webrtc/base/bind.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/physicalsocketserver.h"
#include "webrtc/base/ssladapter.h"
#include "webrtc/base/virtualsocketserver.h"
#include "webrtc/p2p/base/basicpacketsocketfactory.h"
#include "webrtc/p2p/base/teststunserver.h"
#include "webrtc/p2p/stunprober/stunprober.h"

using stunprober::StunProber;
using stunprober::AsyncCallback;

namespace stunprober {

namespace {

const rtc::SocketAddress kLocalAddr("192.168.0.1", 0);
const rtc::SocketAddress kStunAddr1("1.1.1.1", 3478);
const rtc::SocketAddress kStunAddr2("1.1.1.2", 3478);
const rtc::SocketAddress kFailedStunAddr("1.1.1.3", 3478);
const rtc::SocketAddress kStunMappedAddr("77.77.77.77", 0);

}  // namespace

class StunProberTest : public testing::Test {
 public:
  StunProberTest()
      : main_(rtc::Thread::Current()),
        pss_(new rtc::PhysicalSocketServer),
        ss_(new rtc::VirtualSocketServer(pss_.get())),
        ss_scope_(ss_.get()),
        result_(StunProber::SUCCESS),
        stun_server_1_(cricket::TestStunServer::Create(rtc::Thread::Current(),
                                                       kStunAddr1)),
        stun_server_2_(cricket::TestStunServer::Create(rtc::Thread::Current(),
                                                       kStunAddr2)) {
    stun_server_1_->set_fake_stun_addr(kStunMappedAddr);
    stun_server_2_->set_fake_stun_addr(kStunMappedAddr);
    rtc::InitializeSSL();
  }

  void set_expected_result(int result) { result_ = result; }

  void StartProbing(rtc::PacketSocketFactory* socket_factory,
                    const std::vector<rtc::SocketAddress>& addrs,
                    const rtc::NetworkManager::NetworkList& networks,
                    bool shared_socket,
                    uint16_t interval,
                    uint16_t pings_per_ip) {
    prober.reset(
        new StunProber(socket_factory, rtc::Thread::Current(), networks));
    prober->Start(addrs, shared_socket, interval, pings_per_ip,
                  100 /* timeout_ms */, [this](StunProber* prober, int result) {
                    this->StopCallback(prober, result);
                  });
  }

  void RunProber(bool shared_mode) {
    const int pings_per_ip = 3;
    std::vector<rtc::SocketAddress> addrs;
    addrs.push_back(kStunAddr1);
    addrs.push_back(kStunAddr2);
    // Add a non-existing server. This shouldn't pollute the result.
    addrs.push_back(kFailedStunAddr);

    rtc::Network ipv4_network1("test_eth0", "Test Network Adapter 1",
                               rtc::IPAddress(0x12345600U), 24);
    ipv4_network1.AddIP(rtc::IPAddress(0x12345678));
    rtc::NetworkManager::NetworkList networks;
    networks.push_back(&ipv4_network1);

    std::unique_ptr<rtc::BasicPacketSocketFactory> socket_factory(
        new rtc::BasicPacketSocketFactory());

    // Set up the expected results for verification.
    std::set<std::string> srflx_addresses;
    srflx_addresses.insert(kStunMappedAddr.ToString());
    const uint32_t total_pings_tried =
        static_cast<uint32_t>(pings_per_ip * addrs.size());

    // The reported total_pings should not count for pings sent to the
    // kFailedStunAddr.
    const uint32_t total_pings_reported = total_pings_tried - pings_per_ip;

    StartProbing(socket_factory.get(), addrs, networks, shared_mode, 3,
                 pings_per_ip);

    WAIT(stopped_, 1000);

    StunProber::Stats stats;
    EXPECT_TRUE(prober->GetStats(&stats));
    EXPECT_EQ(stats.success_percent, 100);
    EXPECT_TRUE(stats.nat_type > stunprober::NATTYPE_NONE);
    EXPECT_EQ(stats.srflx_addrs, srflx_addresses);
    EXPECT_EQ(static_cast<uint32_t>(stats.num_request_sent),
              total_pings_reported);
    EXPECT_EQ(static_cast<uint32_t>(stats.num_response_received),
              total_pings_reported);
  }

 private:
  void StopCallback(StunProber* prober, int result) {
    EXPECT_EQ(result, result_);
    stopped_ = true;
  }

  rtc::Thread* main_;
  std::unique_ptr<rtc::PhysicalSocketServer> pss_;
  std::unique_ptr<rtc::VirtualSocketServer> ss_;
  rtc::SocketServerScope ss_scope_;
  std::unique_ptr<StunProber> prober;
  int result_ = 0;
  bool stopped_ = false;
  std::unique_ptr<cricket::TestStunServer> stun_server_1_;
  std::unique_ptr<cricket::TestStunServer> stun_server_2_;
};

TEST_F(StunProberTest, NonSharedMode) {
  RunProber(false);
}

TEST_F(StunProberTest, SharedMode) {
  RunProber(true);
}

}  // namespace stunprober
