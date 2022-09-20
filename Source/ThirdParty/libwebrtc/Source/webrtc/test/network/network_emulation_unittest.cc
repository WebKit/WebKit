/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/network/network_emulation.h"

#include <atomic>
#include <memory>
#include <set>

#include "api/test/simulated_network.h"
#include "api/units/time_delta.h"
#include "call/simulated_network.h"
#include "rtc_base/event.h"
#include "rtc_base/gunit.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/task_queue_for_test.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/network/network_emulation_manager.h"

namespace webrtc {
namespace test {
namespace {

using ::testing::ElementsAreArray;

constexpr TimeDelta kNetworkPacketWaitTimeout = TimeDelta::Millis(100);
constexpr TimeDelta kStatsWaitTimeout = TimeDelta::Seconds(1);
constexpr int kOverheadIpv4Udp = 20 + 8;

class SocketReader : public sigslot::has_slots<> {
 public:
  explicit SocketReader(rtc::AsyncSocket* socket, rtc::Thread* network_thread)
      : socket_(socket), network_thread_(network_thread) {
    socket_->SignalReadEvent.connect(this, &SocketReader::OnReadEvent);
    size_ = 128 * 1024;
    buf_ = new char[size_];
  }
  ~SocketReader() override { delete[] buf_; }

  void OnReadEvent(rtc::AsyncSocket* socket) {
    RTC_DCHECK(socket_ == socket);
    RTC_DCHECK(network_thread_->IsCurrent());
    int64_t timestamp;
    len_ = socket_->Recv(buf_, size_, &timestamp);

    MutexLock lock(&lock_);
    received_count_++;
  }

  int ReceivedCount() {
    MutexLock lock(&lock_);
    return received_count_;
  }

 private:
  rtc::AsyncSocket* const socket_;
  rtc::Thread* const network_thread_;
  char* buf_;
  size_t size_;
  int len_;

  Mutex lock_;
  int received_count_ RTC_GUARDED_BY(lock_) = 0;
};

class MockReceiver : public EmulatedNetworkReceiverInterface {
 public:
  MOCK_METHOD(void, OnPacketReceived, (EmulatedIpPacket packet), (override));
};

class NetworkEmulationManagerThreeNodesRoutingTest : public ::testing::Test {
 public:
  NetworkEmulationManagerThreeNodesRoutingTest() {
    e1_ = emulation_.CreateEndpoint(EmulatedEndpointConfig());
    e2_ = emulation_.CreateEndpoint(EmulatedEndpointConfig());
    e3_ = emulation_.CreateEndpoint(EmulatedEndpointConfig());
  }

  void SetupRouting(
      std::function<void(EmulatedEndpoint*,
                         EmulatedEndpoint*,
                         EmulatedEndpoint*,
                         NetworkEmulationManager*)> create_routing_func) {
    create_routing_func(e1_, e2_, e3_, &emulation_);
  }

  void SendPacketsAndValidateDelivery() {
    EXPECT_CALL(r_e1_e2_, OnPacketReceived(::testing::_)).Times(1);
    EXPECT_CALL(r_e2_e1_, OnPacketReceived(::testing::_)).Times(1);
    EXPECT_CALL(r_e1_e3_, OnPacketReceived(::testing::_)).Times(1);
    EXPECT_CALL(r_e3_e1_, OnPacketReceived(::testing::_)).Times(1);

    uint16_t common_send_port = 80;
    uint16_t r_e1_e2_port = e2_->BindReceiver(0, &r_e1_e2_).value();
    uint16_t r_e2_e1_port = e1_->BindReceiver(0, &r_e2_e1_).value();
    uint16_t r_e1_e3_port = e3_->BindReceiver(0, &r_e1_e3_).value();
    uint16_t r_e3_e1_port = e1_->BindReceiver(0, &r_e3_e1_).value();

    // Next code is using API of EmulatedEndpoint, that is visible only for
    // internals of network emulation layer. Don't use this API in other tests.
    // Send packet from e1 to e2.
    e1_->SendPacket(
        rtc::SocketAddress(e1_->GetPeerLocalAddress(), common_send_port),
        rtc::SocketAddress(e2_->GetPeerLocalAddress(), r_e1_e2_port),
        rtc::CopyOnWriteBuffer(10));

    // Send packet from e2 to e1.
    e2_->SendPacket(
        rtc::SocketAddress(e2_->GetPeerLocalAddress(), common_send_port),
        rtc::SocketAddress(e1_->GetPeerLocalAddress(), r_e2_e1_port),
        rtc::CopyOnWriteBuffer(10));

    // Send packet from e1 to e3.
    e1_->SendPacket(
        rtc::SocketAddress(e1_->GetPeerLocalAddress(), common_send_port),
        rtc::SocketAddress(e3_->GetPeerLocalAddress(), r_e1_e3_port),
        rtc::CopyOnWriteBuffer(10));

    // Send packet from e3 to e1.
    e3_->SendPacket(
        rtc::SocketAddress(e3_->GetPeerLocalAddress(), common_send_port),
        rtc::SocketAddress(e1_->GetPeerLocalAddress(), r_e3_e1_port),
        rtc::CopyOnWriteBuffer(10));

    // Sleep at the end to wait for async packets delivery.
    emulation_.time_controller()->AdvanceTime(kNetworkPacketWaitTimeout);
  }

 private:
  // Receivers: r_<source endpoint>_<destination endpoint>
  // They must be destroyed after emulation, so they should be declared before.
  MockReceiver r_e1_e2_;
  MockReceiver r_e2_e1_;
  MockReceiver r_e1_e3_;
  MockReceiver r_e3_e1_;

  NetworkEmulationManagerImpl emulation_{TimeMode::kRealTime};
  EmulatedEndpoint* e1_;
  EmulatedEndpoint* e2_;
  EmulatedEndpoint* e3_;
};

EmulatedNetworkNode* CreateEmulatedNodeWithDefaultBuiltInConfig(
    NetworkEmulationManager* emulation) {
  return emulation->CreateEmulatedNode(
      std::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig()));
}

}  // namespace

using ::testing::_;

TEST(NetworkEmulationManagerTest, GeneratedIpv4AddressDoesNotCollide) {
  NetworkEmulationManagerImpl network_manager(TimeMode::kRealTime);
  std::set<rtc::IPAddress> ips;
  EmulatedEndpointConfig config;
  config.generated_ip_family = EmulatedEndpointConfig::IpAddressFamily::kIpv4;
  for (int i = 0; i < 1000; i++) {
    EmulatedEndpoint* endpoint = network_manager.CreateEndpoint(config);
    ASSERT_EQ(endpoint->GetPeerLocalAddress().family(), AF_INET);
    bool result = ips.insert(endpoint->GetPeerLocalAddress()).second;
    ASSERT_TRUE(result);
  }
}

TEST(NetworkEmulationManagerTest, GeneratedIpv6AddressDoesNotCollide) {
  NetworkEmulationManagerImpl network_manager(TimeMode::kRealTime);
  std::set<rtc::IPAddress> ips;
  EmulatedEndpointConfig config;
  config.generated_ip_family = EmulatedEndpointConfig::IpAddressFamily::kIpv6;
  for (int i = 0; i < 1000; i++) {
    EmulatedEndpoint* endpoint = network_manager.CreateEndpoint(config);
    ASSERT_EQ(endpoint->GetPeerLocalAddress().family(), AF_INET6);
    bool result = ips.insert(endpoint->GetPeerLocalAddress()).second;
    ASSERT_TRUE(result);
  }
}

TEST(NetworkEmulationManagerTest, Run) {
  NetworkEmulationManagerImpl network_manager(TimeMode::kRealTime);

  EmulatedNetworkNode* alice_node = network_manager.CreateEmulatedNode(
      std::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig()));
  EmulatedNetworkNode* bob_node = network_manager.CreateEmulatedNode(
      std::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig()));
  EmulatedEndpoint* alice_endpoint =
      network_manager.CreateEndpoint(EmulatedEndpointConfig());
  EmulatedEndpoint* bob_endpoint =
      network_manager.CreateEndpoint(EmulatedEndpointConfig());
  network_manager.CreateRoute(alice_endpoint, {alice_node}, bob_endpoint);
  network_manager.CreateRoute(bob_endpoint, {bob_node}, alice_endpoint);

  EmulatedNetworkManagerInterface* nt1 =
      network_manager.CreateEmulatedNetworkManagerInterface({alice_endpoint});
  EmulatedNetworkManagerInterface* nt2 =
      network_manager.CreateEmulatedNetworkManagerInterface({bob_endpoint});

  rtc::Thread* t1 = nt1->network_thread();
  rtc::Thread* t2 = nt2->network_thread();

  rtc::CopyOnWriteBuffer data("Hello");
  for (uint64_t j = 0; j < 2; j++) {
<<<<<<< HEAD
    rtc::Socket* s1 = nullptr;
    rtc::Socket* s2 = nullptr;
    SendTask(t1, [&] {
      s1 = t1->socketserver()->CreateSocket(AF_INET, SOCK_DGRAM);
    });
    SendTask(t2, [&] {
      s2 = t2->socketserver()->CreateSocket(AF_INET, SOCK_DGRAM);
=======
    rtc::AsyncSocket* s1 = nullptr;
    rtc::AsyncSocket* s2 = nullptr;
    t1->Invoke<void>(RTC_FROM_HERE, [&] {
      s1 = t1->socketserver()->CreateAsyncSocket(AF_INET, SOCK_DGRAM);
    });
    t2->Invoke<void>(RTC_FROM_HERE, [&] {
      s2 = t2->socketserver()->CreateAsyncSocket(AF_INET, SOCK_DGRAM);
>>>>>>> parent of 8e32ad0e8387 (revert libwebrtc changes to help bump)
    });

    SocketReader r1(s1, t1);
    SocketReader r2(s2, t2);

    rtc::SocketAddress a1(alice_endpoint->GetPeerLocalAddress(), 0);
    rtc::SocketAddress a2(bob_endpoint->GetPeerLocalAddress(), 0);

    SendTask(t1, [&] {
      s1->Bind(a1);
      a1 = s1->GetLocalAddress();
    });
    SendTask(t2, [&] {
      s2->Bind(a2);
      a2 = s2->GetLocalAddress();
    });

    SendTask(t1, [&] { s1->Connect(a2); });
    SendTask(t2, [&] { s2->Connect(a1); });

    for (uint64_t i = 0; i < 1000; i++) {
      t1->PostTask([&]() { s1->Send(data.data(), data.size()); });
      t2->PostTask([&]() { s2->Send(data.data(), data.size()); });
    }

    network_manager.time_controller()->AdvanceTime(TimeDelta::Seconds(1));

    EXPECT_EQ(r1.ReceivedCount(), 1000);
    EXPECT_EQ(r2.ReceivedCount(), 1000);

    SendTask(t1, [&] { delete s1; });
    SendTask(t2, [&] { delete s2; });
  }

  const int64_t single_packet_size = data.size() + kOverheadIpv4Udp;
  std::atomic<int> received_stats_count{0};
  nt1->GetStats([&](std::unique_ptr<EmulatedNetworkStats> st) {
    EXPECT_EQ(st->PacketsSent(), 2000l);
    EXPECT_EQ(st->BytesSent().bytes(), single_packet_size * 2000l);
    EXPECT_THAT(st->LocalAddresses(),
                ElementsAreArray({alice_endpoint->GetPeerLocalAddress()}));
    EXPECT_EQ(st->PacketsReceived(), 2000l);
    EXPECT_EQ(st->BytesReceived().bytes(), single_packet_size * 2000l);
    EXPECT_EQ(st->PacketsDropped(), 0l);
    EXPECT_EQ(st->BytesDropped().bytes(), 0l);

    rtc::IPAddress bob_ip = bob_endpoint->GetPeerLocalAddress();
    std::map<rtc::IPAddress, std::unique_ptr<EmulatedNetworkIncomingStats>>
        source_st = st->IncomingStatsPerSource();
    ASSERT_EQ(source_st.size(), 1lu);
    EXPECT_EQ(source_st.at(bob_ip)->PacketsReceived(), 2000l);
    EXPECT_EQ(source_st.at(bob_ip)->BytesReceived().bytes(),
              single_packet_size * 2000l);
    EXPECT_EQ(source_st.at(bob_ip)->PacketsDropped(), 0l);
    EXPECT_EQ(source_st.at(bob_ip)->BytesDropped().bytes(), 0l);

    std::map<rtc::IPAddress, std::unique_ptr<EmulatedNetworkOutgoingStats>>
        dest_st = st->OutgoingStatsPerDestination();
    ASSERT_EQ(dest_st.size(), 1lu);
    EXPECT_EQ(dest_st.at(bob_ip)->PacketsSent(), 2000l);
    EXPECT_EQ(dest_st.at(bob_ip)->BytesSent().bytes(),
              single_packet_size * 2000l);

    // No debug stats are collected by default.
    EXPECT_TRUE(st->SentPacketsSizeCounter().IsEmpty());
    EXPECT_TRUE(st->SentPacketsQueueWaitTimeUs().IsEmpty());
    EXPECT_TRUE(st->ReceivedPacketsSizeCounter().IsEmpty());
    EXPECT_TRUE(st->DroppedPacketsSizeCounter().IsEmpty());
    EXPECT_TRUE(dest_st.at(bob_ip)->SentPacketsSizeCounter().IsEmpty());
    EXPECT_TRUE(source_st.at(bob_ip)->ReceivedPacketsSizeCounter().IsEmpty());
    EXPECT_TRUE(source_st.at(bob_ip)->DroppedPacketsSizeCounter().IsEmpty());

    received_stats_count++;
  });
  nt2->GetStats([&](std::unique_ptr<EmulatedNetworkStats> st) {
    EXPECT_EQ(st->PacketsSent(), 2000l);
    EXPECT_EQ(st->BytesSent().bytes(), single_packet_size * 2000l);
    EXPECT_THAT(st->LocalAddresses(),
                ElementsAreArray({bob_endpoint->GetPeerLocalAddress()}));
    EXPECT_EQ(st->PacketsReceived(), 2000l);
    EXPECT_EQ(st->BytesReceived().bytes(), single_packet_size * 2000l);
    EXPECT_EQ(st->PacketsDropped(), 0l);
    EXPECT_EQ(st->BytesDropped().bytes(), 0l);
    EXPECT_GT(st->FirstReceivedPacketSize(), DataSize::Zero());
    EXPECT_TRUE(st->FirstPacketReceivedTime().IsFinite());
    EXPECT_TRUE(st->LastPacketReceivedTime().IsFinite());

    rtc::IPAddress alice_ip = alice_endpoint->GetPeerLocalAddress();
    std::map<rtc::IPAddress, std::unique_ptr<EmulatedNetworkIncomingStats>>
        source_st = st->IncomingStatsPerSource();
    ASSERT_EQ(source_st.size(), 1lu);
    EXPECT_EQ(source_st.at(alice_ip)->PacketsReceived(), 2000l);
    EXPECT_EQ(source_st.at(alice_ip)->BytesReceived().bytes(),
              single_packet_size * 2000l);
    EXPECT_EQ(source_st.at(alice_ip)->PacketsDropped(), 0l);
    EXPECT_EQ(source_st.at(alice_ip)->BytesDropped().bytes(), 0l);

    std::map<rtc::IPAddress, std::unique_ptr<EmulatedNetworkOutgoingStats>>
        dest_st = st->OutgoingStatsPerDestination();
    ASSERT_EQ(dest_st.size(), 1lu);
    EXPECT_EQ(dest_st.at(alice_ip)->PacketsSent(), 2000l);
    EXPECT_EQ(dest_st.at(alice_ip)->BytesSent().bytes(),
              single_packet_size * 2000l);

    // No debug stats are collected by default.
    EXPECT_TRUE(st->SentPacketsSizeCounter().IsEmpty());
    EXPECT_TRUE(st->SentPacketsQueueWaitTimeUs().IsEmpty());
    EXPECT_TRUE(st->ReceivedPacketsSizeCounter().IsEmpty());
    EXPECT_TRUE(st->DroppedPacketsSizeCounter().IsEmpty());
    EXPECT_TRUE(dest_st.at(alice_ip)->SentPacketsSizeCounter().IsEmpty());
    EXPECT_TRUE(source_st.at(alice_ip)->ReceivedPacketsSizeCounter().IsEmpty());
    EXPECT_TRUE(source_st.at(alice_ip)->DroppedPacketsSizeCounter().IsEmpty());

    received_stats_count++;
  });
  ASSERT_EQ_SIMULATED_WAIT(received_stats_count.load(), 2,
                           kStatsWaitTimeout.ms(),
                           *network_manager.time_controller());
}

TEST(NetworkEmulationManagerTest, DebugStatsCollectedInDebugMode) {
  NetworkEmulationManagerImpl network_manager(TimeMode::kSimulated);

  EmulatedNetworkNode* alice_node = network_manager.CreateEmulatedNode(
      std::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig()));
  EmulatedNetworkNode* bob_node = network_manager.CreateEmulatedNode(
      std::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig()));
  EmulatedEndpointConfig debug_config;
  debug_config.stats_gathering_mode =
      EmulatedEndpointConfig::StatsGatheringMode::kDebug;
  EmulatedEndpoint* alice_endpoint =
      network_manager.CreateEndpoint(debug_config);
  EmulatedEndpoint* bob_endpoint =
      network_manager.CreateEndpoint(EmulatedEndpointConfig());
  network_manager.CreateRoute(alice_endpoint, {alice_node}, bob_endpoint);
  network_manager.CreateRoute(bob_endpoint, {bob_node}, alice_endpoint);

  EmulatedNetworkManagerInterface* nt1 =
      network_manager.CreateEmulatedNetworkManagerInterface({alice_endpoint});
  EmulatedNetworkManagerInterface* nt2 =
      network_manager.CreateEmulatedNetworkManagerInterface({bob_endpoint});

  rtc::Thread* t1 = nt1->network_thread();
  rtc::Thread* t2 = nt2->network_thread();

  rtc::CopyOnWriteBuffer data("Hello");
  for (uint64_t j = 0; j < 2; j++) {
<<<<<<< HEAD
    rtc::Socket* s1 = nullptr;
    rtc::Socket* s2 = nullptr;
    SendTask(t1, [&] {
      s1 = t1->socketserver()->CreateSocket(AF_INET, SOCK_DGRAM);
    });
    SendTask(t2, [&] {
      s2 = t2->socketserver()->CreateSocket(AF_INET, SOCK_DGRAM);
=======
    rtc::AsyncSocket* s1 = nullptr;
    rtc::AsyncSocket* s2 = nullptr;
    t1->Invoke<void>(RTC_FROM_HERE, [&] {
      s1 = t1->socketserver()->CreateAsyncSocket(AF_INET, SOCK_DGRAM);
    });
    t2->Invoke<void>(RTC_FROM_HERE, [&] {
      s2 = t2->socketserver()->CreateAsyncSocket(AF_INET, SOCK_DGRAM);
>>>>>>> parent of 8e32ad0e8387 (revert libwebrtc changes to help bump)
    });

    SocketReader r1(s1, t1);
    SocketReader r2(s2, t2);

    rtc::SocketAddress a1(alice_endpoint->GetPeerLocalAddress(), 0);
    rtc::SocketAddress a2(bob_endpoint->GetPeerLocalAddress(), 0);

    SendTask(t1, [&] {
      s1->Bind(a1);
      a1 = s1->GetLocalAddress();
    });
    SendTask(t2, [&] {
      s2->Bind(a2);
      a2 = s2->GetLocalAddress();
    });

    SendTask(t1, [&] { s1->Connect(a2); });
    SendTask(t2, [&] { s2->Connect(a1); });

    for (uint64_t i = 0; i < 1000; i++) {
      t1->PostTask([&]() { s1->Send(data.data(), data.size()); });
      t2->PostTask([&]() { s2->Send(data.data(), data.size()); });
    }

    network_manager.time_controller()->AdvanceTime(TimeDelta::Seconds(1));

    EXPECT_EQ(r1.ReceivedCount(), 1000);
    EXPECT_EQ(r2.ReceivedCount(), 1000);

    SendTask(t1, [&] { delete s1; });
    SendTask(t2, [&] { delete s2; });
  }

  const int64_t single_packet_size = data.size() + kOverheadIpv4Udp;
  std::atomic<int> received_stats_count{0};
  nt1->GetStats([&](std::unique_ptr<EmulatedNetworkStats> st) {
    rtc::IPAddress bob_ip = bob_endpoint->GetPeerLocalAddress();
    std::map<rtc::IPAddress, std::unique_ptr<EmulatedNetworkIncomingStats>>
        source_st = st->IncomingStatsPerSource();
    ASSERT_EQ(source_st.size(), 1lu);

    std::map<rtc::IPAddress, std::unique_ptr<EmulatedNetworkOutgoingStats>>
        dest_st = st->OutgoingStatsPerDestination();
    ASSERT_EQ(dest_st.size(), 1lu);

    // No debug stats are collected by default.
    EXPECT_EQ(st->SentPacketsSizeCounter().NumSamples(), 2000l);
    EXPECT_EQ(st->ReceivedPacketsSizeCounter().GetAverage(),
              single_packet_size);
    EXPECT_EQ(st->SentPacketsQueueWaitTimeUs().NumSamples(), 2000l);
    EXPECT_LT(st->SentPacketsQueueWaitTimeUs().GetMax(), 1);
    EXPECT_TRUE(st->DroppedPacketsSizeCounter().IsEmpty());
    EXPECT_EQ(dest_st.at(bob_ip)->SentPacketsSizeCounter().NumSamples(), 2000l);
    EXPECT_EQ(dest_st.at(bob_ip)->SentPacketsSizeCounter().GetAverage(),
              single_packet_size);
    EXPECT_EQ(source_st.at(bob_ip)->ReceivedPacketsSizeCounter().NumSamples(),
              2000l);
    EXPECT_EQ(source_st.at(bob_ip)->ReceivedPacketsSizeCounter().GetAverage(),
              single_packet_size);
    EXPECT_TRUE(source_st.at(bob_ip)->DroppedPacketsSizeCounter().IsEmpty());

    received_stats_count++;
  });
  ASSERT_EQ_SIMULATED_WAIT(received_stats_count.load(), 1,
                           kStatsWaitTimeout.ms(),
                           *network_manager.time_controller());
}

TEST(NetworkEmulationManagerTest, ThroughputStats) {
  NetworkEmulationManagerImpl network_manager(TimeMode::kRealTime);

  EmulatedNetworkNode* alice_node = network_manager.CreateEmulatedNode(
      std::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig()));
  EmulatedNetworkNode* bob_node = network_manager.CreateEmulatedNode(
      std::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig()));
  EmulatedEndpoint* alice_endpoint =
      network_manager.CreateEndpoint(EmulatedEndpointConfig());
  EmulatedEndpoint* bob_endpoint =
      network_manager.CreateEndpoint(EmulatedEndpointConfig());
  network_manager.CreateRoute(alice_endpoint, {alice_node}, bob_endpoint);
  network_manager.CreateRoute(bob_endpoint, {bob_node}, alice_endpoint);

  EmulatedNetworkManagerInterface* nt1 =
      network_manager.CreateEmulatedNetworkManagerInterface({alice_endpoint});
  EmulatedNetworkManagerInterface* nt2 =
      network_manager.CreateEmulatedNetworkManagerInterface({bob_endpoint});

  rtc::Thread* t1 = nt1->network_thread();
  rtc::Thread* t2 = nt2->network_thread();

  constexpr int64_t kUdpPayloadSize = 100;
  constexpr int64_t kSinglePacketSize = kUdpPayloadSize + kOverheadIpv4Udp;
  rtc::CopyOnWriteBuffer data(kUdpPayloadSize);

<<<<<<< HEAD
  rtc::Socket* s1 = nullptr;
  rtc::Socket* s2 = nullptr;
  SendTask(t1,
           [&] { s1 = t1->socketserver()->CreateSocket(AF_INET, SOCK_DGRAM); });
  SendTask(t2,
           [&] { s2 = t2->socketserver()->CreateSocket(AF_INET, SOCK_DGRAM); });
=======
  rtc::AsyncSocket* s1 = nullptr;
  rtc::AsyncSocket* s2 = nullptr;
  t1->Invoke<void>(RTC_FROM_HERE, [&] {
    s1 = t1->socketserver()->CreateAsyncSocket(AF_INET, SOCK_DGRAM);
  });
  t2->Invoke<void>(RTC_FROM_HERE, [&] {
    s2 = t2->socketserver()->CreateAsyncSocket(AF_INET, SOCK_DGRAM);
  });
>>>>>>> parent of 8e32ad0e8387 (revert libwebrtc changes to help bump)

  SocketReader r1(s1, t1);
  SocketReader r2(s2, t2);

  rtc::SocketAddress a1(alice_endpoint->GetPeerLocalAddress(), 0);
  rtc::SocketAddress a2(bob_endpoint->GetPeerLocalAddress(), 0);

  SendTask(t1, [&] {
    s1->Bind(a1);
    a1 = s1->GetLocalAddress();
  });
  SendTask(t2, [&] {
    s2->Bind(a2);
    a2 = s2->GetLocalAddress();
  });

  SendTask(t1, [&] { s1->Connect(a2); });
  SendTask(t2, [&] { s2->Connect(a1); });

  // Send 11 packets, totalizing 1 second between the first and the last.
  const int kNumPacketsSent = 11;
  const TimeDelta kDelay = TimeDelta::Millis(100);
  for (int i = 0; i < kNumPacketsSent; i++) {
    t1->PostTask([&]() { s1->Send(data.data(), data.size()); });
    t2->PostTask([&]() { s2->Send(data.data(), data.size()); });
    network_manager.time_controller()->AdvanceTime(kDelay);
  }

  std::atomic<int> received_stats_count{0};
  nt1->GetStats([&](std::unique_ptr<EmulatedNetworkStats> st) {
    EXPECT_EQ(st->PacketsSent(), kNumPacketsSent);
    EXPECT_EQ(st->BytesSent().bytes(), kSinglePacketSize * kNumPacketsSent);

    const double tolerance = 0.95;  // Accept 5% tolerance for timing.
    EXPECT_GE(st->LastPacketSentTime() - st->FirstPacketSentTime(),
              (kNumPacketsSent - 1) * kDelay * tolerance);
    EXPECT_GT(st->AverageSendRate().bps(), 0);
    received_stats_count++;
  });

  ASSERT_EQ_SIMULATED_WAIT(received_stats_count.load(), 1,
                           kStatsWaitTimeout.ms(),
                           *network_manager.time_controller());

  EXPECT_EQ(r1.ReceivedCount(), 11);
  EXPECT_EQ(r2.ReceivedCount(), 11);

  SendTask(t1, [&] { delete s1; });
  SendTask(t2, [&] { delete s2; });
}

// Testing that packets are delivered via all routes using a routing scheme as
// follows:
//  * e1 -> n1 -> e2
//  * e2 -> n2 -> e1
//  * e1 -> n3 -> e3
//  * e3 -> n4 -> e1
TEST_F(NetworkEmulationManagerThreeNodesRoutingTest,
       PacketsAreDeliveredInBothWaysWhenConnectedToTwoPeers) {
  SetupRouting([](EmulatedEndpoint* e1, EmulatedEndpoint* e2,
                  EmulatedEndpoint* e3, NetworkEmulationManager* emulation) {
    auto* node1 = CreateEmulatedNodeWithDefaultBuiltInConfig(emulation);
    auto* node2 = CreateEmulatedNodeWithDefaultBuiltInConfig(emulation);
    auto* node3 = CreateEmulatedNodeWithDefaultBuiltInConfig(emulation);
    auto* node4 = CreateEmulatedNodeWithDefaultBuiltInConfig(emulation);

    emulation->CreateRoute(e1, {node1}, e2);
    emulation->CreateRoute(e2, {node2}, e1);

    emulation->CreateRoute(e1, {node3}, e3);
    emulation->CreateRoute(e3, {node4}, e1);
  });
  SendPacketsAndValidateDelivery();
}

// Testing that packets are delivered via all routes using a routing scheme as
// follows:
//  * e1 -> n1 -> e2
//  * e2 -> n2 -> e1
//  * e1 -> n1 -> e3
//  * e3 -> n4 -> e1
TEST_F(NetworkEmulationManagerThreeNodesRoutingTest,
       PacketsAreDeliveredInBothWaysWhenConnectedToTwoPeersOverSameSendLink) {
  SetupRouting([](EmulatedEndpoint* e1, EmulatedEndpoint* e2,
                  EmulatedEndpoint* e3, NetworkEmulationManager* emulation) {
    auto* node1 = CreateEmulatedNodeWithDefaultBuiltInConfig(emulation);
    auto* node2 = CreateEmulatedNodeWithDefaultBuiltInConfig(emulation);
    auto* node3 = CreateEmulatedNodeWithDefaultBuiltInConfig(emulation);

    emulation->CreateRoute(e1, {node1}, e2);
    emulation->CreateRoute(e2, {node2}, e1);

    emulation->CreateRoute(e1, {node1}, e3);
    emulation->CreateRoute(e3, {node3}, e1);
  });
  SendPacketsAndValidateDelivery();
}

TEST(NetworkEmulationManagerTest, EndpointLoopback) {
  NetworkEmulationManagerImpl network_manager(TimeMode::kSimulated);
  auto endpoint = network_manager.CreateEndpoint(EmulatedEndpointConfig());

  MockReceiver receiver;
  EXPECT_CALL(receiver, OnPacketReceived(::testing::_)).Times(1);
  ASSERT_EQ(endpoint->BindReceiver(80, &receiver), 80);

  endpoint->SendPacket(rtc::SocketAddress(endpoint->GetPeerLocalAddress(), 80),
                       rtc::SocketAddress(endpoint->GetPeerLocalAddress(), 80),
                       "Hello");
  network_manager.time_controller()->AdvanceTime(TimeDelta::Seconds(1));
}

TEST(NetworkEmulationManagerTest, EndpointCanSendWithDifferentSourceIp) {
  constexpr uint32_t kEndpointIp = 0xC0A80011;  // 192.168.0.17
  constexpr uint32_t kSourceIp = 0xC0A80012;    // 192.168.0.18
  NetworkEmulationManagerImpl network_manager(TimeMode::kSimulated);
  EmulatedEndpointConfig endpoint_config;
  endpoint_config.ip = rtc::IPAddress(kEndpointIp);
  endpoint_config.allow_send_packet_with_different_source_ip = true;
  auto endpoint = network_manager.CreateEndpoint(endpoint_config);

  MockReceiver receiver;
  EXPECT_CALL(receiver, OnPacketReceived(::testing::_)).Times(1);
  ASSERT_EQ(endpoint->BindReceiver(80, &receiver), 80);

  endpoint->SendPacket(rtc::SocketAddress(kSourceIp, 80),
                       rtc::SocketAddress(endpoint->GetPeerLocalAddress(), 80),
                       "Hello");
  network_manager.time_controller()->AdvanceTime(TimeDelta::Seconds(1));
}

TEST(NetworkEmulationManagerTest,
     EndpointCanReceiveWithDifferentDestIpThroughDefaultRoute) {
  constexpr uint32_t kDestEndpointIp = 0xC0A80011;  // 192.168.0.17
  constexpr uint32_t kDestIp = 0xC0A80012;          // 192.168.0.18
  NetworkEmulationManagerImpl network_manager(TimeMode::kSimulated);
  auto sender_endpoint =
      network_manager.CreateEndpoint(EmulatedEndpointConfig());
  EmulatedEndpointConfig endpoint_config;
  endpoint_config.ip = rtc::IPAddress(kDestEndpointIp);
  endpoint_config.allow_receive_packets_with_different_dest_ip = true;
  auto receiver_endpoint = network_manager.CreateEndpoint(endpoint_config);

  MockReceiver receiver;
  EXPECT_CALL(receiver, OnPacketReceived(::testing::_)).Times(1);
  ASSERT_EQ(receiver_endpoint->BindReceiver(80, &receiver), 80);

  network_manager.CreateDefaultRoute(
      sender_endpoint, {network_manager.NodeBuilder().Build().node},
      receiver_endpoint);

  sender_endpoint->SendPacket(
      rtc::SocketAddress(sender_endpoint->GetPeerLocalAddress(), 80),
      rtc::SocketAddress(kDestIp, 80), "Hello");
  network_manager.time_controller()->AdvanceTime(TimeDelta::Seconds(1));
}

TEST(NetworkEmulationManagerTURNTest, GetIceServerConfig) {
  NetworkEmulationManagerImpl network_manager(TimeMode::kRealTime);
  auto turn = network_manager.CreateTURNServer(EmulatedTURNServerConfig());

  EXPECT_GT(turn->GetIceServerConfig().username.size(), 0u);
  EXPECT_GT(turn->GetIceServerConfig().password.size(), 0u);
  EXPECT_NE(turn->GetIceServerConfig().url.find(
                turn->GetClientEndpoint()->GetPeerLocalAddress().ToString()),
            std::string::npos);
}

TEST(NetworkEmulationManagerTURNTest, ClientTraffic) {
  NetworkEmulationManagerImpl emulation(TimeMode::kSimulated);
  auto* ep = emulation.CreateEndpoint(EmulatedEndpointConfig());
  auto* turn = emulation.CreateTURNServer(EmulatedTURNServerConfig());
  auto* node = CreateEmulatedNodeWithDefaultBuiltInConfig(&emulation);
  emulation.CreateRoute(ep, {node}, turn->GetClientEndpoint());
  emulation.CreateRoute(turn->GetClientEndpoint(), {node}, ep);

  MockReceiver recv;
  int port = ep->BindReceiver(0, &recv).value();

  // Construct a STUN BINDING.
  cricket::StunMessage ping(cricket::STUN_BINDING_REQUEST);
  rtc::ByteBufferWriter buf;
  ping.Write(&buf);
  rtc::CopyOnWriteBuffer packet(buf.Data(), buf.Length());

  // We expect to get a ping reply.
  EXPECT_CALL(recv, OnPacketReceived(::testing::_)).Times(1);

  ep->SendPacket(rtc::SocketAddress(ep->GetPeerLocalAddress(), port),
                 turn->GetClientEndpointAddress(), packet);
  emulation.time_controller()->AdvanceTime(TimeDelta::Seconds(1));
}

}  // namespace test
}  // namespace webrtc
