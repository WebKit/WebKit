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
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "api/test/simulated_network.h"
#include "call/simulated_network.h"
#include "rtc_base/event.h"
#include "rtc_base/logging.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {
namespace {

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
  EmulatedEndpoint endpoint{/*id=*/1, rtc::IPAddress(), /*is_enabled=*/true,
                            &task_queue_, &clock};
};

}  // namespace

TEST(CrossTrafficTest, TriggerPacketBurst) {
  TrafficCounterFixture fixture;
  TrafficRoute traffic(&fixture.clock, &fixture.counter, &fixture.endpoint);
  traffic.TriggerPacketBurst(100, 1000);

  EXPECT_EQ(fixture.counter.packets_count_, 100);
  EXPECT_EQ(fixture.counter.total_packets_size_, 100 * 1000ul);
}

TEST(CrossTrafficTest, PulsedPeaksCrossTraffic) {
  TrafficCounterFixture fixture;
  TrafficRoute traffic(&fixture.clock, &fixture.counter, &fixture.endpoint);

  PulsedPeaksConfig config;
  config.peak_rate = DataRate::kbps(1000);
  config.min_packet_size = DataSize::bytes(1);
  config.min_packet_interval = TimeDelta::ms(25);
  config.send_duration = TimeDelta::ms(500);
  config.hold_duration = TimeDelta::ms(250);
  PulsedPeaksCrossTraffic pulsed_peaks(config, &traffic);
  const auto kRunTime = TimeDelta::seconds(1);
  while (fixture.clock.TimeInMilliseconds() < kRunTime.ms()) {
    pulsed_peaks.Process(Timestamp::ms(fixture.clock.TimeInMilliseconds()));
    fixture.clock.AdvanceTimeMilliseconds(1);
  }

  RTC_LOG(INFO) << fixture.counter.packets_count_ << " packets; "
                << fixture.counter.total_packets_size_ << " bytes";
  // Using 50% duty cycle.
  const auto kExpectedDataSent = kRunTime * config.peak_rate * 0.5;
  EXPECT_NEAR(fixture.counter.total_packets_size_, kExpectedDataSent.bytes(),
              kExpectedDataSent.bytes() * 0.1);
}

TEST(CrossTrafficTest, RandomWalkCrossTraffic) {
  TrafficCounterFixture fixture;
  TrafficRoute traffic(&fixture.clock, &fixture.counter, &fixture.endpoint);

  RandomWalkConfig config;
  config.peak_rate = DataRate::kbps(1000);
  config.min_packet_size = DataSize::bytes(1);
  config.min_packet_interval = TimeDelta::ms(25);
  config.update_interval = TimeDelta::ms(500);
  config.variance = 0.0;
  config.bias = 1.0;

  RandomWalkCrossTraffic random_walk(config, &traffic);
  const auto kRunTime = TimeDelta::seconds(1);
  while (fixture.clock.TimeInMilliseconds() < kRunTime.ms()) {
    random_walk.Process(Timestamp::ms(fixture.clock.TimeInMilliseconds()));
    fixture.clock.AdvanceTimeMilliseconds(1);
  }

  RTC_LOG(INFO) << fixture.counter.packets_count_ << " packets; "
                << fixture.counter.total_packets_size_ << " bytes";
  // Sending at peak rate since bias = 1.
  const auto kExpectedDataSent = kRunTime * config.peak_rate;
  EXPECT_NEAR(fixture.counter.total_packets_size_, kExpectedDataSent.bytes(),
              kExpectedDataSent.bytes() * 0.1);
}

}  // namespace test
}  // namespace webrtc
