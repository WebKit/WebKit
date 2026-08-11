/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/network/schedulable_network_behavior.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "api/test/create_network_emulation_manager.h"
#include "api/test/network_emulation/leaky_bucket_network_queue.h"
#include "api/test/network_emulation/network_config_schedule.pb.h"
#include "api/test/network_emulation_manager.h"
#include "api/test/simulated_network.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::Mock;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::Sequence;
using ::testing::SizeIs;

constexpr uint64_t kRandomSeed = 1;

class SchedulableNetworkBehaviorTestFixture {
 public:
  SchedulableNetworkBehaviorTestFixture()
      : manager_(webrtc::CreateNetworkEmulationManager(
            {.time_mode = TimeMode::kSimulated})) {}

  webrtc::Clock& clock() const {
    return *manager_->time_controller()->GetClock();
  }
  void AdvanceTime(webrtc::TimeDelta delta) {
    manager_->time_controller()->AdvanceTime(delta);
  }
  void AdvanceTimeTo(int64_t timestamp_us) {
    TimeDelta delta = Timestamp::Micros(timestamp_us) - TimeNow();
    ASSERT_GE(delta, TimeDelta::Zero());
    manager_->time_controller()->AdvanceTime(delta);
  }

  webrtc::Timestamp TimeNow() const {
    return manager_->time_controller()->GetClock()->CurrentTime();
  }

 private:
  const std::unique_ptr<webrtc::NetworkEmulationManager> manager_;
};

TEST(SchedulableNetworkBehaviorTest, NoSchedule) {
  SchedulableNetworkBehaviorTestFixture fixture;

  network_behaviour::NetworkConfigSchedule schedule;
  SchedulableNetworkBehavior network_behaviour(
      schedule, kRandomSeed, fixture.clock(),
      [](webrtc::Timestamp) { return true; },
      std::make_unique<LeakyBucketNetworkQueue>());
  webrtc::Timestamp send_time = fixture.TimeNow();
  EXPECT_TRUE(network_behaviour.EnqueuePacket({/*size=*/1000 / 8,
                                               /*send_time_us=*/send_time.us(),
                                               /*packet_id=*/1}));
  ASSERT_TRUE(network_behaviour.NextDeliveryTimeUs().has_value());
  fixture.AdvanceTimeTo(*network_behaviour.NextDeliveryTimeUs());
  EXPECT_THAT(
      network_behaviour.DequeueDeliverablePackets(fixture.TimeNow().us()),
      SizeIs(1));
}

TEST(SchedulableNetworkBehaviorTest, ScheduleWithoutUpdates) {
  SchedulableNetworkBehaviorTestFixture fixture;

  network_behaviour::NetworkConfigSchedule schedule;
  auto initial_config = schedule.add_item();
  initial_config->set_link_capacity_kbps(10);
  initial_config->set_queue_delay_ms(70);

  SchedulableNetworkBehavior network_behaviour(
      schedule, kRandomSeed, fixture.clock(),
      [](webrtc::Timestamp) { return true; },
      std::make_unique<LeakyBucketNetworkQueue>());
  webrtc::Timestamp send_time = fixture.TimeNow();
  EXPECT_TRUE(network_behaviour.EnqueuePacket({/*size=*/1000 / 8,
                                               /*send_time_us=*/send_time.us(),
                                               /*packet_id=*/1}));

  // 1000 bits, on a 10kbps link should take 100ms +  70 extra.
  // The network_behaviour at the time of writing this test needs two calls
  // to NextDeliveryTimeUs to before the packet is delivered (one for the link
  // capacity queue and one for the queue delay).
  std::vector<webrtc::PacketDeliveryInfo> packet_delivery_infos;
  while (packet_delivery_infos.empty()) {
    ASSERT_TRUE(network_behaviour.NextDeliveryTimeUs().has_value());
    fixture.AdvanceTimeTo(*network_behaviour.NextDeliveryTimeUs());
    packet_delivery_infos =
        network_behaviour.DequeueDeliverablePackets(fixture.TimeNow().us());
  }
  EXPECT_EQ(fixture.TimeNow(), send_time + TimeDelta::Millis(170));
  ASSERT_THAT(packet_delivery_infos, SizeIs(1));
  EXPECT_EQ(packet_delivery_infos[0].packet_id, 1u);
  EXPECT_EQ(packet_delivery_infos[0].receive_time_us, send_time.us() + 170'000);
}

TEST(SchedulableNetworkBehaviorTest,
     TriggersDeliveryTimeChangedCallbackOnScheduleIfPacketInLinkCapacityQueue) {
  SchedulableNetworkBehaviorTestFixture fixture;
  network_behaviour::NetworkConfigSchedule schedule;
  auto initial_config = schedule.add_item();
  // A packet of size 1000 bits should take 100ms to send.
  initial_config->set_link_capacity_kbps(10);
  initial_config->set_queue_delay_ms(10);
  auto updated_capacity = schedule.add_item();
  updated_capacity->set_time_since_first_sent_packet_ms(50);
  // A packet of size 1000 bits should take 10ms to send. But since "half" the
  // first packet has passed the narrow section, it should take 50ms + 500/100 =
  // 55ms.
  updated_capacity->set_link_capacity_kbps(100);

  SchedulableNetworkBehavior network_behaviour(
      schedule, kRandomSeed, fixture.clock(),
      [](webrtc::Timestamp) { return true; },
      std::make_unique<LeakyBucketNetworkQueue>());
  MockFunction<void()> delivery_time_changed_callback;
  network_behaviour.RegisterDeliveryTimeChangedCallback(
      delivery_time_changed_callback.AsStdFunction());

  webrtc::Timestamp first_packet_send_time = fixture.TimeNow();
  EXPECT_CALL(delivery_time_changed_callback, Call).WillOnce([&]() {
    EXPECT_EQ(fixture.TimeNow(),
              first_packet_send_time + TimeDelta::Millis(50));
    ASSERT_TRUE(network_behaviour.NextDeliveryTimeUs().has_value());
  });
  EXPECT_TRUE(network_behaviour.EnqueuePacket(
      {/*size=*/1000 / 8,
       /*send_time_us=*/first_packet_send_time.us(),
       /*packet_id=*/1}));
  fixture.AdvanceTime(
      TimeDelta::Millis(updated_capacity->time_since_first_sent_packet_ms()));
  Mock::VerifyAndClearExpectations(&delivery_time_changed_callback);
  ASSERT_TRUE(network_behaviour.NextDeliveryTimeUs().has_value());
  fixture.AdvanceTime(
      TimeDelta::Micros(*network_behaviour.NextDeliveryTimeUs()));
  std::vector<PacketDeliveryInfo> dequeued_packets =
      network_behaviour.DequeueDeliverablePackets(fixture.TimeNow().us());
  ASSERT_FALSE(dequeued_packets.empty());
  EXPECT_EQ(dequeued_packets[0].receive_time_us,
            (first_packet_send_time + TimeDelta::Millis(55) +
             /*queue_delay=*/TimeDelta::Millis(10))
                .us());
}

TEST(SchedulableNetworkBehaviorTest, ScheduleStartedWhenStartConditionTrue) {
  SchedulableNetworkBehaviorTestFixture fixture;
  network_behaviour::NetworkConfigSchedule schedule;
  auto initial_config = schedule.add_item();
  initial_config->set_link_capacity_kbps(0);
  auto item = schedule.add_item();
  item->set_time_since_first_sent_packet_ms(1);
  item->set_link_capacity_kbps(1000000);

  MockFunction<bool(Timestamp)> start_condition;
  webrtc::Timestamp first_packet_send_time = fixture.TimeNow();
  webrtc::Timestamp second_packet_send_time =
      fixture.TimeNow() + TimeDelta::Millis(100);
  Sequence s;
  EXPECT_CALL(start_condition, Call(first_packet_send_time))
      .InSequence(s)
      .WillOnce(Return(false));
  // Expect schedule to start when the second packet is sent.
  EXPECT_CALL(start_condition, Call(second_packet_send_time))
      .InSequence(s)
      .WillOnce(Return(true));
  SchedulableNetworkBehavior network_behaviour(
      schedule, kRandomSeed, fixture.clock(), start_condition.AsStdFunction(),
      std::make_unique<LeakyBucketNetworkQueue>());

  EXPECT_TRUE(network_behaviour.EnqueuePacket(
      {/*size=*/1000 / 8,
       /*send_time_us=*/first_packet_send_time.us(),
       /*packet_id=*/1}));
  EXPECT_FALSE(network_behaviour.NextDeliveryTimeUs().has_value());
  // Move passed the normal schedule change time. Still dont expect a delivery
  // time.
  fixture.AdvanceTime(TimeDelta::Millis(100));
  EXPECT_FALSE(network_behaviour.NextDeliveryTimeUs().has_value());

  EXPECT_TRUE(network_behaviour.EnqueuePacket(
      {/*size=*/1000 / 8,
       /*send_time_us=*/second_packet_send_time.us(),
       /*packet_id=*/2}));

  EXPECT_FALSE(network_behaviour.NextDeliveryTimeUs().has_value());
  fixture.AdvanceTime(TimeDelta::Millis(1));
  EXPECT_TRUE(network_behaviour.NextDeliveryTimeUs().has_value());
}

TEST(SchedulableNetworkBehaviorTest, ScheduleWithRepeat) {
  SchedulableNetworkBehaviorTestFixture fixture;
  network_behaviour::NetworkConfigSchedule schedule;
  auto initial_config = schedule.add_item();
  // A packet of size 1000 bits should take 100ms to send.
  initial_config->set_link_capacity_kbps(10);
  auto updated_capacity = schedule.add_item();
  updated_capacity->set_time_since_first_sent_packet_ms(150);
  // A packet of size 1000 bits should take 10ms to send.
  updated_capacity->set_link_capacity_kbps(100);
  // A packet of size 1000 bits, scheduled 200ms after the last update to the
  // config should again take 100ms to send.
  schedule.set_repeat_schedule_after_last_ms(200);

  SchedulableNetworkBehavior network_behaviour(
      schedule, kRandomSeed, fixture.clock(),
      [](webrtc::Timestamp) { return true; },
      std::make_unique<LeakyBucketNetworkQueue>());

  webrtc::Timestamp first_packet_send_time = fixture.TimeNow();
  EXPECT_TRUE(network_behaviour.EnqueuePacket(
      {/*size=*/1000 / 8,
       /*send_time_us=*/first_packet_send_time.us(),
       /*packet_id=*/1}));
  ASSERT_TRUE(network_behaviour.NextDeliveryTimeUs().has_value());
  EXPECT_EQ(*network_behaviour.NextDeliveryTimeUs(),
            fixture.TimeNow().us() + TimeDelta::Millis(100).us());
  fixture.AdvanceTimeTo(*network_behaviour.NextDeliveryTimeUs());
  EXPECT_THAT(
      network_behaviour.DequeueDeliverablePackets(fixture.TimeNow().us()),
      SizeIs(1));
  fixture.AdvanceTime(
      TimeDelta::Millis(updated_capacity->time_since_first_sent_packet_ms() +
                        schedule.repeat_schedule_after_last_ms() -
                        /*time already advanced*/ 100));
  // Schedule should be repeated.
  // A packet of size 1000 bits should take 100ms to send.
  EXPECT_TRUE(
      network_behaviour.EnqueuePacket({/*size=*/1000 / 8,
                                       /*send_time_us=*/fixture.TimeNow().us(),
                                       /*packet_id=*/2}));
  ASSERT_TRUE(network_behaviour.NextDeliveryTimeUs().has_value());
  EXPECT_EQ(*network_behaviour.NextDeliveryTimeUs(),
            fixture.TimeNow().us() + TimeDelta::Millis(100).us());
}

TEST(SchedulableNetworkBehaviorTest, ScheduleWithoutRepeat) {
  SchedulableNetworkBehaviorTestFixture fixture;
  network_behaviour::NetworkConfigSchedule schedule;
  auto initial_config = schedule.add_item();
  // A packet of size 1000 bits should take 100ms to send.
  initial_config->set_link_capacity_kbps(10);
  auto updated_capacity = schedule.add_item();
  updated_capacity->set_time_since_first_sent_packet_ms(150);
  // A packet of size 1000 bits should take 10ms to send.
  updated_capacity->set_link_capacity_kbps(100);

  SchedulableNetworkBehavior network_behaviour(
      schedule, kRandomSeed, fixture.clock(),
      [](webrtc::Timestamp) { return true; },
      std::make_unique<LeakyBucketNetworkQueue>());

  webrtc::Timestamp first_packet_send_time = fixture.TimeNow();
  EXPECT_TRUE(network_behaviour.EnqueuePacket(
      {/*size=*/1000 / 8,
       /*send_time_us=*/first_packet_send_time.us(),
       /*packet_id=*/1}));
  ASSERT_TRUE(network_behaviour.NextDeliveryTimeUs().has_value());
  EXPECT_EQ(*network_behaviour.NextDeliveryTimeUs(),
            fixture.TimeNow().us() + TimeDelta::Millis(100).us());
  fixture.AdvanceTimeTo(*network_behaviour.NextDeliveryTimeUs());
  EXPECT_THAT(
      network_behaviour.DequeueDeliverablePackets(fixture.TimeNow().us()),
      SizeIs(1));
  // Advance time to when the updated capacity should be in effect and add one
  // minute. The updated capacity should still be in affect.
  fixture.AdvanceTime(
      TimeDelta::Millis(updated_capacity->time_since_first_sent_packet_ms() -
                        /*time already advanced*/ 100) +
      TimeDelta::Minutes(1));

  // Schedule should not be repeated.
  // A packet of size 1000 bits should take 10ms to send.
  EXPECT_TRUE(
      network_behaviour.EnqueuePacket({/*size=*/1000 / 8,
                                       /*send_time_us=*/fixture.TimeNow().us(),
                                       /*packet_id=*/2}));
  ASSERT_TRUE(network_behaviour.NextDeliveryTimeUs().has_value());
  EXPECT_EQ(*network_behaviour.NextDeliveryTimeUs(),
            fixture.TimeNow().us() + TimeDelta::Millis(10).us());
  fixture.AdvanceTimeTo(*network_behaviour.NextDeliveryTimeUs());
  EXPECT_THAT(
      network_behaviour.DequeueDeliverablePackets(fixture.TimeNow().us()),
      SizeIs(1));
}

}  // namespace
}  // namespace webrtc
