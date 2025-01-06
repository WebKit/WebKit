/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/network/cross_traffic.h"

#include <atomic>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "api/test/network_emulation_manager.h"
#include "api/test/simulated_network.h"
#include "api/units/data_rate.h"
#include "rtc_base/event.h"
#include "rtc_base/logging.h"
#include "rtc_base/network_constants.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/network/network_emulation_manager.h"
#include "test/network/simulated_network.h"
#include "test/network/traffic_route.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {
namespace test {
namespace {

constexpr uint32_t kTestIpAddress = 0xC0A80011;  // 192.168.0.17

class CountingReceiver : public EmulatedNetworkReceiverInterface {
 public:
  void OnPacketReceived(EmulatedIpPacket packet) override {
    packets_count_++;
    total_packets_size_ += packet.size();
  }

  std::atomic<int> packets_count_{0};
  std::atomic<uint64_t> total_packets_size_{0};
};
struct TrafficCounterFixture {
  SimulatedClock clock{0};
  CountingReceiver counter;
  TaskQueueForTest task_queue_;
  EmulatedEndpointImpl endpoint{EmulatedEndpointImpl::Options{
                                    /*id=*/1,
                                    rtc::IPAddress(kTestIpAddress),
                                    EmulatedEndpointConfig(),
                                    EmulatedNetworkStatsGatheringMode::kDefault,
                                },
                                /*is_enabled=*/true, task_queue_.Get(), &clock};
};

}  // namespace

TEST(CrossTrafficTest, TriggerPacketBurst) {
  TrafficCounterFixture fixture;
  CrossTrafficRouteImpl traffic(&fixture.clock, &fixture.counter,
                                &fixture.endpoint);
  traffic.TriggerPacketBurst(100, 1000);

  EXPECT_EQ(fixture.counter.packets_count_, 100);
  EXPECT_EQ(fixture.counter.total_packets_size_, 100 * 1000ul);
}

TEST(CrossTrafficTest, PulsedPeaksCrossTraffic) {
  TrafficCounterFixture fixture;
  CrossTrafficRouteImpl traffic(&fixture.clock, &fixture.counter,
                                &fixture.endpoint);

  PulsedPeaksConfig config;
  config.peak_rate = DataRate::KilobitsPerSec(1000);
  config.min_packet_size = DataSize::Bytes(1);
  config.min_packet_interval = TimeDelta::Millis(25);
  config.send_duration = TimeDelta::Millis(500);
  config.hold_duration = TimeDelta::Millis(250);
  PulsedPeaksCrossTraffic pulsed_peaks(config, &traffic);
  const auto kRunTime = TimeDelta::Seconds(1);
  while (fixture.clock.TimeInMilliseconds() < kRunTime.ms()) {
    pulsed_peaks.Process(Timestamp::Millis(fixture.clock.TimeInMilliseconds()));
    fixture.clock.AdvanceTimeMilliseconds(1);
  }

  RTC_LOG(LS_INFO) << fixture.counter.packets_count_ << " packets; "
                   << fixture.counter.total_packets_size_ << " bytes";
  // Using 50% duty cycle.
  const auto kExpectedDataSent = kRunTime * config.peak_rate * 0.5;
  EXPECT_NEAR(fixture.counter.total_packets_size_, kExpectedDataSent.bytes(),
              kExpectedDataSent.bytes() * 0.1);
}

TEST(CrossTrafficTest, RandomWalkCrossTraffic) {
  TrafficCounterFixture fixture;
  CrossTrafficRouteImpl traffic(&fixture.clock, &fixture.counter,
                                &fixture.endpoint);

  RandomWalkConfig config;
  config.peak_rate = DataRate::KilobitsPerSec(1000);
  config.min_packet_size = DataSize::Bytes(1);
  config.min_packet_interval = TimeDelta::Millis(25);
  config.update_interval = TimeDelta::Millis(500);
  config.variance = 0.0;
  config.bias = 1.0;

  RandomWalkCrossTraffic random_walk(config, &traffic);
  const auto kRunTime = TimeDelta::Seconds(1);
  while (fixture.clock.TimeInMilliseconds() < kRunTime.ms()) {
    random_walk.Process(Timestamp::Millis(fixture.clock.TimeInMilliseconds()));
    fixture.clock.AdvanceTimeMilliseconds(1);
  }

  RTC_LOG(LS_INFO) << fixture.counter.packets_count_ << " packets; "
                   << fixture.counter.total_packets_size_ << " bytes";
  // Sending at peak rate since bias = 1.
  const auto kExpectedDataSent = kRunTime * config.peak_rate;
  EXPECT_NEAR(fixture.counter.total_packets_size_, kExpectedDataSent.bytes(),
              kExpectedDataSent.bytes() * 0.1);
}

TEST(TcpMessageRouteTest, DeliveredOnLossyNetwork) {
  NetworkEmulationManagerImpl net({.time_mode = TimeMode::kSimulated});
  BuiltInNetworkBehaviorConfig send;
  // 800 kbps means that the 100 kB message would be delivered in ca 1 second
  // under ideal conditions and no overhead.
  send.link_capacity = DataRate::KilobitsPerSec(100 * 8);
  send.loss_percent = 50;
  send.queue_delay_ms = 100;
  send.delay_standard_deviation_ms = 20;
  send.allow_reordering = true;
  auto ret = send;
  ret.loss_percent = 10;

  auto* tcp_route =
      net.CreateTcpRoute(net.CreateRoute({net.CreateEmulatedNode(send)}),
                         net.CreateRoute({net.CreateEmulatedNode(ret)}));
  int deliver_count = 0;
  // 100 kB is more than what fits into a single packet.
  constexpr size_t kMessageSize = 100000;

  tcp_route->SendMessage(kMessageSize, [&] {
    RTC_LOG(LS_INFO) << "Received at " << ToString(net.Now());
    deliver_count++;
  });

  // If there was no loss, we would have delivered the message in ca 1 second,
  // with 50% it should take much longer.
  net.time_controller()->AdvanceTime(TimeDelta::Seconds(5));
  ASSERT_EQ(deliver_count, 0);
  // But given enough time the messsage will be delivered, but only once.
  net.time_controller()->AdvanceTime(TimeDelta::Seconds(60));
  EXPECT_EQ(deliver_count, 1);
}

}  // namespace test
}  // namespace webrtc
