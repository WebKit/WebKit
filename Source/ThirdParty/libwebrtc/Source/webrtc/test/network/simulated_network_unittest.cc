/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/network/simulated_network.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "api/test/network_emulation/leaky_bucket_network_queue.h"
#include "api/test/simulated_network.h"
#include "api/transport/ecn_marking.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::MockFunction;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

PacketInFlightInfo PacketWithSize(size_t size) {
  return PacketInFlightInfo(/*size=*/size, /*send_time_us=*/0, /*packet_id=*/1);
}

TEST(SimulatedNetworkTest, NextDeliveryTimeIsUnknownOnEmptyNetwork) {
  SimulatedNetwork network = SimulatedNetwork({});
  EXPECT_EQ(network.NextDeliveryTimeUs(), std::nullopt);
}

TEST(SimulatedNetworkTest, EnqueueFirstPacketOnNetworkWithInfiniteCapacity) {
  // A packet of 1 kB that gets enqueued on a network with infinite capacity
  // should be ready to exit the network immediately.
  SimulatedNetwork network = SimulatedNetwork({});
  ASSERT_TRUE(network.EnqueuePacket(PacketWithSize(1'000)));

  EXPECT_EQ(network.NextDeliveryTimeUs(), 0);
}

TEST(SimulatedNetworkTest, EnqueueFirstPacketOnNetworkWithLimitedCapacity) {
  // A packet of 125 bytes that gets enqueued on a network with 1 kbps capacity
  // should be ready to exit the network in 1 second.
  SimulatedNetwork network =
      SimulatedNetwork({.link_capacity = DataRate::KilobitsPerSec(1)});
  ASSERT_TRUE(network.EnqueuePacket(PacketWithSize(125)));

  EXPECT_EQ(network.NextDeliveryTimeUs(), TimeDelta::Seconds(1).us());
}

TEST(SimulatedNetworkTest,
     EnqueuePacketsButNextDeliveryIsBasedOnFirstEnqueuedPacket) {
  // A packet of 125 bytes that gets enqueued on a network with 1 kbps capacity
  // should be ready to exit the network in 1 second.
  SimulatedNetwork network =
      SimulatedNetwork({.link_capacity = DataRate::KilobitsPerSec(1)});
  ASSERT_TRUE(network.EnqueuePacket(
      PacketInFlightInfo(/*size=*/125, /*send_time_us=*/0, /*packet_id=*/1)));
  EXPECT_EQ(network.NextDeliveryTimeUs(), TimeDelta::Seconds(1).us());

  // Enqueuing another packet after 100 us doesn't change the next delivery
  // time.
  ASSERT_TRUE(network.EnqueuePacket(
      PacketInFlightInfo(/*size=*/125, /*send_time_us=*/100, /*packet_id=*/2)));
  EXPECT_EQ(network.NextDeliveryTimeUs(), TimeDelta::Seconds(1).us());

  // Enqueuing another packet after 2 seconds doesn't change the next delivery
  // time since the first packet has not left the network yet.
  ASSERT_TRUE(network.EnqueuePacket(PacketInFlightInfo(
      /*size=*/125, /*send_time_us=*/TimeDelta::Seconds(2).us(),
      /*packet_id=*/3)));
  EXPECT_EQ(network.NextDeliveryTimeUs(), TimeDelta::Seconds(1).us());
}

TEST(SimulatedNetworkTest, EnqueueFailsWhenQueueLengthIsReached) {
  SimulatedNetwork network =
      SimulatedNetwork({.queue_length_packets = 1,
                        .link_capacity = DataRate::KilobitsPerSec(1)});
  ASSERT_TRUE(network.EnqueuePacket(
      PacketInFlightInfo(/*size=*/125, /*send_time_us=*/0, /*packet_id=*/1)));

  // Until there is 1 packet in the queue, no other packets can be enqueued,
  // the only way to make space for new packets is calling
  // DequeueDeliverablePackets at a time greater than or equal to
  // NextDeliveryTimeUs.
  EXPECT_FALSE(network.EnqueuePacket(
      PacketInFlightInfo(/*size=*/125,
                         /*send_time_us=*/TimeDelta::Seconds(0.5).us(),
                         /*packet_id=*/2)));

  // Even if the send_time_us is after NextDeliveryTimeUs, it is still not
  // possible to enqueue a new packet since the client didn't deque any packet
  // from the queue (in this case the client is introducing unbounded delay but
  // the network cannot do anything about it).
  EXPECT_FALSE(network.EnqueuePacket(
      PacketInFlightInfo(/*size=*/125,
                         /*send_time_us=*/TimeDelta::Seconds(2).us(),
                         /*packet_id=*/3)));
}

TEST(SimulatedNetworkTest, CanChangeQueueLength) {
  SimulatedNetwork network =
      SimulatedNetwork({.link_capacity = DataRate::KilobitsPerSec(1)});
  ASSERT_TRUE(network.EnqueuePacket(
      PacketInFlightInfo(/*size=*/125, /*send_time_us=*/0, /*packet_id=*/1)));
  ASSERT_TRUE(network.EnqueuePacket(
      PacketInFlightInfo(/*size=*/125, /*send_time_us=*/0, /*packet_id=*/2)));

  network.SetConfig({.queue_length_packets = 1,
                     .link_capacity = DataRate::KilobitsPerSec(1)});
  EXPECT_FALSE(network.EnqueuePacket(
      PacketInFlightInfo(/*size=*/125, /*send_time_us=*/0, /*packet_id=*/3)));
}

TEST(SimulatedNetworkTest, PacketOverhead) {
  // A packet of 125 bytes that gets enqueued on a network with 1 kbps capacity
  // should be ready to exit the network in 1 second, but since there is an
  // overhead per packet of 125 bytes, it will exit the network after 2 seconds.
  SimulatedNetwork network = SimulatedNetwork(
      {.link_capacity = DataRate::KilobitsPerSec(1), .packet_overhead = 125});
  ASSERT_TRUE(network.EnqueuePacket(PacketWithSize(125)));

  EXPECT_EQ(network.NextDeliveryTimeUs(), TimeDelta::Seconds(2).us());
}

TEST(SimulatedNetworkTest,
     DequeueDeliverablePacketsLeavesPacketsInCapacityLink) {
  // A packet of 125 bytes that gets enqueued on a network with 1 kbps capacity
  // should be ready to exit the network in 1 second.
  SimulatedNetwork network =
      SimulatedNetwork({.link_capacity = DataRate::KilobitsPerSec(1)});
  ASSERT_TRUE(network.EnqueuePacket(
      PacketInFlightInfo(/*size=*/125, /*send_time_us=*/0, /*packet_id=*/1)));
  // Enqueue another packet of 125 bytes (this one should exit after 2 seconds).
  ASSERT_TRUE(network.EnqueuePacket(
      PacketInFlightInfo(/*size=*/125,
                         /*send_time_us=*/TimeDelta::Seconds(1).us(),
                         /*packet_id=*/2)));

  // The first packet will exit after 1 second, so that is the next delivery
  // time.
  EXPECT_EQ(network.NextDeliveryTimeUs(), TimeDelta::Seconds(1).us());

  // After 1 seconds, we collect the delivered packets...
  std::vector<PacketDeliveryInfo> delivered_packets =
      network.DequeueDeliverablePackets(
          /*receive_time_us=*/TimeDelta::Seconds(1).us());
  ASSERT_EQ(delivered_packets.size(), 1ul);
  EXPECT_EQ(delivered_packets[0].packet_id, 1ul);
  EXPECT_EQ(delivered_packets[0].receive_time_us, TimeDelta::Seconds(1).us());

  // ... And after the first enqueued packet has left the network, the next
  // delivery time reflects the delivery time of the next packet.
  EXPECT_EQ(network.NextDeliveryTimeUs(), TimeDelta::Seconds(2).us());
}

TEST(SimulatedNetworkTest,
     DequeueDeliverablePacketsAppliesConfigChangesToCapacityLink) {
  // A packet of 125 bytes that gets enqueued on a network with 1 kbps capacity
  // should be ready to exit the network in 1 second.
  SimulatedNetwork network =
      SimulatedNetwork({.link_capacity = DataRate::KilobitsPerSec(1)});
  const PacketInFlightInfo packet_1 =
      PacketInFlightInfo(/*size=*/125, /*send_time_us=*/0, /*packet_id=*/1);
  ASSERT_TRUE(network.EnqueuePacket(packet_1));

  // Enqueue another packet of 125 bytes with send time 1 second so this should
  // exit after 2 seconds.
  PacketInFlightInfo packet_2 =
      PacketInFlightInfo(/*size=*/125,
                         /*send_time_us=*/TimeDelta::Seconds(1).us(),
                         /*packet_id=*/2);
  ASSERT_TRUE(network.EnqueuePacket(packet_2));

  // The first packet will exit after 1 second, so that is the next delivery
  // time.
  EXPECT_EQ(network.NextDeliveryTimeUs(), TimeDelta::Seconds(1).us());

  // Since the link capacity changes from 1 kbps to 10 kbps, packets will take
  // 100 ms each to leave the network.
  network.SetConfig({.link_capacity = DataRate::KilobitsPerSec(10)});

  // The next delivery time doesn't change (it will be updated, if needed at
  // DequeueDeliverablePackets time).
  EXPECT_EQ(network.NextDeliveryTimeUs(), TimeDelta::Seconds(1).us());

  // Getting the first enqueued packet after 100 ms.
  std::vector<PacketDeliveryInfo> delivered_packets =
      network.DequeueDeliverablePackets(
          /*receive_time_us=*/TimeDelta::Millis(100).us());
  ASSERT_EQ(delivered_packets.size(), 1ul);
  EXPECT_THAT(delivered_packets,
              ElementsAre(PacketDeliveryInfo(
                  /*source=*/packet_1,
                  /*receive_time_us=*/TimeDelta::Millis(100).us())));

  // Getting the second enqueued packet that cannot be delivered before its send
  // time, hence it will be delivered after 1.1 seconds.
  EXPECT_EQ(network.NextDeliveryTimeUs(), TimeDelta::Millis(1100).us());
  delivered_packets = network.DequeueDeliverablePackets(
      /*receive_time_us=*/TimeDelta::Millis(1100).us());
  ASSERT_EQ(delivered_packets.size(), 1ul);
  EXPECT_THAT(delivered_packets,
              ElementsAre(PacketDeliveryInfo(
                  /*source=*/packet_2,
                  /*receive_time_us=*/TimeDelta::Millis(1100).us())));
}

TEST(SimulatedNetworkTest,
     SetConfigUpdateNextDeliveryTimeIfLinkCapacityChange) {
  // A packet of 125 bytes that gets enqueued on a network with 1 kbps capacity
  // should be ready to exit the network in 1 second.
  SimulatedNetwork network =
      SimulatedNetwork({.link_capacity = DataRate::KilobitsPerSec(1)});
  MockFunction<void()> delivery_time_changed_callback;
  network.RegisterDeliveryTimeChangedCallback(
      delivery_time_changed_callback.AsStdFunction());
  const PacketInFlightInfo packet_1 =
      PacketInFlightInfo(/*size=*/125, /*send_time_us=*/0, /*packet_id=*/1);
  ASSERT_TRUE(network.EnqueuePacket(packet_1));
  EXPECT_EQ(network.NextDeliveryTimeUs(), TimeDelta::Seconds(1).us());

  // Since the link capacity changes from 1 kbps to 10 kbps, packets will take
  // 100 ms each to leave the network. After 500ms, half the packet should have
  // gone through.
  EXPECT_CALL(delivery_time_changed_callback, Call).WillOnce([&]() {
    EXPECT_EQ(network.NextDeliveryTimeUs(), TimeDelta::Millis(500 + 50).us());
  });
  network.SetConfig({.link_capacity = DataRate::KilobitsPerSec(10)},
                    /*config_update_time*/ Timestamp::Millis(500));
}

TEST(SimulatedNetworkTest,
     SetConfigUpdateNextDeliveryTimeIfLinkCapacityChangeFromZero) {
  SimulatedNetwork network =
      SimulatedNetwork({.link_capacity = DataRate::Zero()});
  MockFunction<void()> delivery_time_changed_callback;
  network.RegisterDeliveryTimeChangedCallback(
      delivery_time_changed_callback.AsStdFunction());
  const PacketInFlightInfo packet_1 =
      PacketInFlightInfo(/*size=*/125, /*send_time_us=*/0, /*packet_id=*/1);
  const PacketInFlightInfo packet_2 =
      PacketInFlightInfo(/*size=*/125, /*send_time_us=*/0, /*packet_id=*/2);
  ASSERT_TRUE(network.EnqueuePacket(packet_1));
  ASSERT_TRUE(network.EnqueuePacket(packet_2));
  EXPECT_FALSE(network.NextDeliveryTimeUs().has_value());

  // The link capacity changes from 0 kbps to 10 kbps during 10ms 1/10th of the
  // packet will be transmitted. (The packet would take 100ms to go through the
  // network at 10kbps.)
  ::testing::Sequence s;
  EXPECT_CALL(delivery_time_changed_callback, Call)
      .InSequence(s)
      .WillOnce([&]() {
        EXPECT_EQ(network.NextDeliveryTimeUs(),
                  TimeDelta::Millis(500 + 100).us());
      });
  EXPECT_CALL(delivery_time_changed_callback, Call)
      .InSequence(s)
      .WillOnce(
          [&]() { EXPECT_FALSE(network.NextDeliveryTimeUs().has_value()); });
  EXPECT_CALL(delivery_time_changed_callback, Call)
      .InSequence(s)
      .WillOnce([&]() {
        EXPECT_EQ(network.NextDeliveryTimeUs(),
                  TimeDelta::Millis(610 + 90).us());
      });
  network.SetConfig({.link_capacity = DataRate::KilobitsPerSec(10)},
                    /*config_update_time*/ Timestamp::Millis(500));
  network.SetConfig({.link_capacity = DataRate::Zero()},
                    /*config_update_time*/ Timestamp::Millis(510));
  network.SetConfig({.link_capacity = DataRate::KilobitsPerSec(10)},
                    /*config_update_time*/ Timestamp::Millis(610));
}

TEST(SimulatedNetworkTest, SetConfigUpdateQueueDelayAfterDelivery) {
  // A packet of 125 bytes that gets enqueued on a network with 1000 kbps
  // capacity should be ready to exit the narrow section in 1 ms.
  SimulatedNetwork network =
      SimulatedNetwork({.queue_delay_ms = 1000,
                        .link_capacity = DataRate::KilobitsPerSec(1000)});
  MockFunction<void()> delivery_time_changed_callback;
  network.RegisterDeliveryTimeChangedCallback(
      delivery_time_changed_callback.AsStdFunction());
  EXPECT_CALL(delivery_time_changed_callback, Call).Times(0);
  const PacketInFlightInfo packet_1 =
      PacketInFlightInfo(/*size=*/125, /*send_time_us=*/0, /*packet_id=*/1);
  ASSERT_TRUE(network.EnqueuePacket(packet_1));
  EXPECT_EQ(network.NextDeliveryTimeUs(), TimeDelta::Millis(1).us());
  // But no packets is actually delivered. Only moved to the delay link.
  EXPECT_TRUE(network
                  .DequeueDeliverablePackets(
                      /*receive_time_us=*/TimeDelta::Millis(1).us())
                  .empty());
  EXPECT_EQ(network.NextDeliveryTimeUs(), TimeDelta::Millis(1000 + 1).us());

  // Changing the queue time does not change the next delivery time.
  network.SetConfig(
      {.queue_delay_ms = 1, .link_capacity = DataRate::KilobitsPerSec(100)},
      /*config_update_time*/ Timestamp::Millis(500));
  EXPECT_EQ(network.NextDeliveryTimeUs(), TimeDelta::Millis(1000 + 1).us());

  // A new packet require NextDeliveryTimeUs to change since the capacity
  // change. But does not affect the delivery time of packet_1.
  const PacketInFlightInfo packet_2 = PacketInFlightInfo(
      /*size=*/125, /*send_time_us=*/TimeDelta::Millis(500).us(),
      /*packet_id=*/2);
  ASSERT_TRUE(network.EnqueuePacket(packet_2));
  EXPECT_EQ(network.NextDeliveryTimeUs(), TimeDelta::Millis(1000 + 1).us());
  // At 100kbps, it will take packet 2 10ms to pass through the narrow section.
  // Since delay is lower for packet_2, but reordering is not allowed, both
  // packets are delivered at the same time.
  std::vector<PacketDeliveryInfo> delivered_packets =
      network.DequeueDeliverablePackets(
          /*receive_time_us=*/TimeDelta::Millis(1000 + 1).us());
  ASSERT_THAT(delivered_packets, SizeIs(2));
  EXPECT_EQ(delivered_packets[0].receive_time_us,
            delivered_packets[1].receive_time_us);
}

TEST(SimulatedNetworkTest, NetworkEmptyAfterLastPacketDequeued) {
  // A packet of 125 bytes that gets enqueued on a network with 1 kbps
  // capacity should be ready to exit the network in 1 second.
  SimulatedNetwork network =
      SimulatedNetwork({.link_capacity = DataRate::KilobitsPerSec(1)});
  ASSERT_TRUE(network.EnqueuePacket(PacketWithSize(125)));

  // Collecting all the delivered packets ...
  std::vector<PacketDeliveryInfo> delivered_packets =
      network.DequeueDeliverablePackets(
          /*receive_time_us=*/TimeDelta::Seconds(1).us());
  EXPECT_EQ(delivered_packets.size(), 1ul);

  // ... leaves the network empty.
  EXPECT_EQ(network.NextDeliveryTimeUs(), std::nullopt);
}

TEST(SimulatedNetworkTest, DequeueDeliverablePacketsOnLateCall) {
  // A packet of 125 bytes that gets enqueued on a network with 1 kbps
  // capacity should be ready to exit the network in 1 second.
  SimulatedNetwork network =
      SimulatedNetwork({.link_capacity = DataRate::KilobitsPerSec(1)});
  ASSERT_TRUE(network.EnqueuePacket(
      PacketInFlightInfo(/*size=*/125, /*send_time_us=*/0, /*packet_id=*/1)));

  // Enqueue another packet of 125 bytes with send time 1 second so this
  // should exit after 2 seconds.
  ASSERT_TRUE(network.EnqueuePacket(
      PacketInFlightInfo(/*size=*/125,
                         /*send_time_us=*/TimeDelta::Seconds(1).us(),
                         /*packet_id=*/2)));

  // Collecting delivered packets after 3 seconds will result in the delivery
  // of both the enqueued packets.
  std::vector<PacketDeliveryInfo> delivered_packets =
      network.DequeueDeliverablePackets(
          /*receive_time_us=*/TimeDelta::Seconds(3).us());
  EXPECT_EQ(delivered_packets.size(), 2ul);
}

TEST(SimulatedNetworkTest,
     DequeueDeliverablePacketsOnEarlyCallReturnsNoPackets) {
  // A packet of 125 bytes that gets enqueued on a network with 1 kbps
  // capacity should be ready to exit the network in 1 second.
  SimulatedNetwork network =
      SimulatedNetwork({.link_capacity = DataRate::KilobitsPerSec(1)});
  ASSERT_TRUE(network.EnqueuePacket(PacketWithSize(125)));

  // Collecting delivered packets after 0.5 seconds will result in the
  // delivery of 0 packets.
  std::vector<PacketDeliveryInfo> delivered_packets =
      network.DequeueDeliverablePackets(
          /*receive_time_us=*/TimeDelta::Seconds(0.5).us());
  EXPECT_EQ(delivered_packets.size(), 0ul);

  // Since the first enqueued packet was supposed to exit after 1 second.
  EXPECT_EQ(network.NextDeliveryTimeUs(), TimeDelta::Seconds(1).us());
}

TEST(SimulatedNetworkTest, QueueDelayMsWithoutStandardDeviation) {
  // A packet of 125 bytes that gets enqueued on a network with 1 kbps
  // capacity should be ready to exit the network in 1 second.
  SimulatedNetwork network = SimulatedNetwork(
      {.queue_delay_ms = 100, .link_capacity = DataRate::KilobitsPerSec(1)});
  ASSERT_TRUE(network.EnqueuePacket(PacketWithSize(125)));
  // The next delivery time is still 1 second even if there are 100 ms of
  // extra delay but this will be applied at DequeueDeliverablePackets time.
  ASSERT_EQ(network.NextDeliveryTimeUs(), TimeDelta::Seconds(1).us());

  // Since all packets are delayed by 100 ms, after 1 second, no packets will
  // exit the network.
  std::vector<PacketDeliveryInfo> delivered_packets =
      network.DequeueDeliverablePackets(
          /*receive_time_us=*/TimeDelta::Seconds(1).us());
  EXPECT_EQ(delivered_packets.size(), 0ul);

  // And the updated next delivery time takes into account the extra delay of
  // 100 ms so the first packet in the network will be delivered after 1.1
  // seconds.
  EXPECT_EQ(network.NextDeliveryTimeUs(), TimeDelta::Millis(1100).us());
  delivered_packets = network.DequeueDeliverablePackets(
      /*receive_time_us=*/TimeDelta::Millis(1100).us());
  EXPECT_EQ(delivered_packets.size(), 1ul);
}

TEST(SimulatedNetworkTest,
     QueueDelayMsWithStandardDeviationAndReorderNotAllowed) {
  SimulatedNetwork network =
      SimulatedNetwork({.queue_delay_ms = 100,
                        .delay_standard_deviation_ms = 90,
                        .link_capacity = DataRate::KilobitsPerSec(1),
                        .allow_reordering = false});
  // A packet of 125 bytes that gets enqueued on a network with 1 kbps
  // capacity should be ready to exit the network in 1 second.
  ASSERT_TRUE(network.EnqueuePacket(
      PacketInFlightInfo(/*size=*/125, /*send_time_us=*/0, /*packet_id=*/1)));

  // But 3 more packets of size 1 byte are enqueued at the same time.
  ASSERT_TRUE(network.EnqueuePacket(
      PacketInFlightInfo(/*size=*/1, /*send_time_us=*/0, /*packet_id=*/2)));
  ASSERT_TRUE(network.EnqueuePacket(
      PacketInFlightInfo(/*size=*/1, /*send_time_us=*/0, /*packet_id=*/3)));
  ASSERT_TRUE(network.EnqueuePacket(
      PacketInFlightInfo(/*size=*/1, /*send_time_us=*/0, /*packet_id=*/4)));

  // After 5 seconds all of them exit the network.
  std::vector<PacketDeliveryInfo> delivered_packets =
      network.DequeueDeliverablePackets(
          /*receive_time_us=*/TimeDelta::Seconds(5).us());
  ASSERT_EQ(delivered_packets.size(), 4ul);

  // And they are still in order even if the delay was applied.
  EXPECT_EQ(delivered_packets[0].packet_id, 1ul);
  EXPECT_EQ(delivered_packets[1].packet_id, 2ul);
  EXPECT_GE(delivered_packets[1].receive_time_us,
            delivered_packets[0].receive_time_us);
  EXPECT_EQ(delivered_packets[2].packet_id, 3ul);
  EXPECT_GE(delivered_packets[2].receive_time_us,
            delivered_packets[1].receive_time_us);
  EXPECT_EQ(delivered_packets[3].packet_id, 4ul);
  EXPECT_GE(delivered_packets[3].receive_time_us,
            delivered_packets[2].receive_time_us);
}

TEST(SimulatedNetworkTest, QueueDelayMsWithStandardDeviationAndReorderAllowed) {
  SimulatedNetwork network =
      SimulatedNetwork({.queue_delay_ms = 100,
                        .delay_standard_deviation_ms = 90,
                        .link_capacity = DataRate::KilobitsPerSec(1),
                        .allow_reordering = true},
                       /*random_seed=*/1);
  // A packet of 125 bytes that gets enqueued on a network with 1 kbps
  // capacity should be ready to exit the network in 1 second.
  ASSERT_TRUE(network.EnqueuePacket(
      PacketInFlightInfo(/*size=*/125, /*send_time_us=*/0, /*packet_id=*/1)));

  // But 3 more packets of size 1 byte are enqueued at the same time.
  ASSERT_TRUE(network.EnqueuePacket(
      PacketInFlightInfo(/*size=*/1, /*send_time_us=*/0, /*packet_id=*/2)));
  ASSERT_TRUE(network.EnqueuePacket(
      PacketInFlightInfo(/*size=*/1, /*send_time_us=*/0, /*packet_id=*/3)));
  ASSERT_TRUE(network.EnqueuePacket(
      PacketInFlightInfo(/*size=*/1, /*send_time_us=*/0, /*packet_id=*/4)));

  // After 5 seconds all of them exit the network.
  std::vector<PacketDeliveryInfo> delivered_packets =
      network.DequeueDeliverablePackets(
          /*receive_time_us=*/TimeDelta::Seconds(5).us());
  ASSERT_EQ(delivered_packets.size(), 4ul);

  // And they have been reordered according to the applied extra delay.
  EXPECT_EQ(delivered_packets[0].packet_id, 3ul);
  EXPECT_EQ(delivered_packets[1].packet_id, 1ul);
  EXPECT_GE(delivered_packets[1].receive_time_us,
            delivered_packets[0].receive_time_us);
  EXPECT_EQ(delivered_packets[2].packet_id, 2ul);
  EXPECT_GE(delivered_packets[2].receive_time_us,
            delivered_packets[1].receive_time_us);
  EXPECT_EQ(delivered_packets[3].packet_id, 4ul);
  EXPECT_GE(delivered_packets[3].receive_time_us,
            delivered_packets[2].receive_time_us);
}

TEST(SimulatedNetworkTest, PacketLoss) {
  // On a network with 50% probability of packet loss ...
  SimulatedNetwork network =
      SimulatedNetwork({.loss_percent = 50}, /*random_seed =*/1);

  // Enqueueing 8 packets ...
  for (int i = 0; i < 8; i++) {
    ASSERT_TRUE(network.EnqueuePacket(PacketInFlightInfo(
        /*size=*/1, /*send_time_us=*/0, /*packet_id=*/i + 1)));
  }

  std::vector<PacketDeliveryInfo> delivered_packets =
      network.DequeueDeliverablePackets(
          /*receive_time_us=*/TimeDelta::Seconds(5).us());
  EXPECT_EQ(delivered_packets.size(), 8ul);

  // Results in the loss of 4 of them.
  int lost_packets = 0;
  for (const auto& packet : delivered_packets) {
    if (packet.receive_time_us == PacketDeliveryInfo::kNotReceived) {
      lost_packets++;
    }
  }
  EXPECT_EQ(lost_packets, 4);
}

TEST(SimulatedNetworkTest, NextDeliveryTimeSetAfterLostPackets) {
  // On a network with 50% probablility of packet loss ...
  SimulatedNetwork network =
      SimulatedNetwork({.queue_delay_ms = 10,
                        .link_capacity = DataRate::KilobitsPerSec(1000),
                        .loss_percent = 50},
                       /*random_seed =*/1);
  // Enqueueing 8 packets at the same time. It should take 1ms to pass through
  // the capacity limited section per packet, it total adding 8ms delay to the
  // last packet. Since queue delay is 10ms, multiple packets will be in the
  // delay queue at the same time.
  for (int i = 0; i < 8; i++) {
    ASSERT_TRUE(network.EnqueuePacket(PacketInFlightInfo(
        /*size=*/125, /*send_time_us=*/0, /*packet_id=*/i + 1)));
  }
  int64_t time_us = 0;
  std::vector<PacketDeliveryInfo> delivered_packets;
  // This assumes first packet is lost and last packet is delivered....
  while (delivered_packets.size() != 8) {
    ASSERT_TRUE(network.NextDeliveryTimeUs().has_value());
    time_us = *network.NextDeliveryTimeUs();
    std::vector<PacketDeliveryInfo> packets =
        network.DequeueDeliverablePackets(time_us);
    delivered_packets.insert(delivered_packets.end(), packets.begin(),
                             packets.end());
  }
  // Results in the loss of 4 of them.
  int lost_packets = 0;
  int received_packets = 0;
  for (const auto& packet : delivered_packets) {
    if (packet.receive_time_us == PacketDeliveryInfo::kNotReceived) {
      lost_packets++;
    } else {
      received_packets++;
    }
  }
  EXPECT_EQ(delivered_packets.back().receive_time_us,
            Timestamp::Millis(10 + 8).us());
  EXPECT_EQ(lost_packets, 4);
  EXPECT_EQ(received_packets, 4);
}

TEST(SimulatedNetworkTest, PacketLossBurst) {
  // On a network with 50% probability of packet loss and an average burst
  // loss length of 100 ...
  SimulatedNetwork network = SimulatedNetwork(
      {.loss_percent = 50, .avg_burst_loss_length = 100}, /*random_seed=*/1);

  // Enqueueing 20 packets ...
  for (int i = 0; i < 20; i++) {
    ASSERT_TRUE(network.EnqueuePacket(PacketInFlightInfo(
        /*size=*/1, /*send_time_us=*/0, /*packet_id=*/i + 1)));
  }

  std::vector<PacketDeliveryInfo> delivered_packets =
      network.DequeueDeliverablePackets(
          /*receive_time_us=*/TimeDelta::Seconds(5).us());
  EXPECT_EQ(delivered_packets.size(), 20ul);

  // Results in a burst of lost packets after the first packet lost.
  // With the current random seed, at least 5 packets are lost.
  int num_lost_packets = 0;
  for (const auto& packet : delivered_packets) {
    if (packet.receive_time_us == PacketDeliveryInfo::kNotReceived) {
      num_lost_packets++;
    }
    if (num_lost_packets > 0) {
      EXPECT_EQ(packet.receive_time_us, PacketDeliveryInfo::kNotReceived);
    }
  }
  EXPECT_GT(num_lost_packets, 5);
}

TEST(SimulatedNetworkTest, PauseTransmissionUntil) {
  // 3 packets of 125 bytes that gets enqueued on a network with 1 kbps
  // capacity should be ready to exit the network after 1, 2 and 3 seconds
  // respectively.
  SimulatedNetwork network =
      SimulatedNetwork({.link_capacity = DataRate::KilobitsPerSec(1)});
  ASSERT_TRUE(network.EnqueuePacket(
      PacketInFlightInfo(/*size=*/125, /*send_time_us=*/0, /*packet_id=*/1)));
  ASSERT_TRUE(network.EnqueuePacket(
      PacketInFlightInfo(/*size=*/125, /*send_time_us=*/0, /*packet_id=*/2)));
  ASSERT_TRUE(network.EnqueuePacket(
      PacketInFlightInfo(/*size=*/125, /*send_time_us=*/0, /*packet_id=*/3)));
  ASSERT_EQ(network.NextDeliveryTimeUs(), TimeDelta::Seconds(1).us());

  // The network gets paused for 5 seconds, which means that the first packet
  // can exit after 5 seconds instead of 1 second.
  network.PauseTransmissionUntil(TimeDelta::Seconds(5).us());

  // No packets after 1 second.
  std::vector<PacketDeliveryInfo> delivered_packets =
      network.DequeueDeliverablePackets(
          /*receive_time_us=*/TimeDelta::Seconds(1).us());
  EXPECT_EQ(delivered_packets.size(), 0ul);
  EXPECT_EQ(network.NextDeliveryTimeUs(), TimeDelta::Seconds(5).us());

  // The first packet exits after 5 seconds.
  delivered_packets = network.DequeueDeliverablePackets(
      /*receive_time_us=*/TimeDelta::Seconds(5).us());
  EXPECT_EQ(delivered_packets.size(), 1ul);

  // After the first packet is exited, the next delivery time reflects the
  // delivery time of the next packet which accounts for the network pause.
  EXPECT_EQ(network.NextDeliveryTimeUs(), TimeDelta::Seconds(6).us());

  // And 2 seconds after the exit of the first enqueued packet, the following
  // 2 packets are also delivered.
  delivered_packets = network.DequeueDeliverablePackets(
      /*receive_time_us=*/TimeDelta::Seconds(7).us());
  EXPECT_EQ(delivered_packets.size(), 2ul);
}

TEST(SimulatedNetworkTest, CongestedNetworkRespectsLinkCapacity) {
  SimulatedNetwork network =
      SimulatedNetwork({.link_capacity = DataRate::KilobitsPerSec(1)});
  for (size_t i = 0; i < 1'000; ++i) {
    ASSERT_TRUE(network.EnqueuePacket(PacketInFlightInfo(
        /*size=*/125, /*send_time_us=*/0, /*packet_id=*/i)));
  }
  PacketDeliveryInfo last_delivered_packet{
      PacketInFlightInfo(/*size=*/0, /*send_time_us=*/0, /*packet_id=*/0), 0};
  while (network.NextDeliveryTimeUs().has_value()) {
    std::vector<PacketDeliveryInfo> delivered_packets =
        network.DequeueDeliverablePackets(
            /*receive_time_us=*/network.NextDeliveryTimeUs().value());
    if (!delivered_packets.empty()) {
      last_delivered_packet = delivered_packets.back();
    }
  }
  // 1000 packets of 1000 bits each will take 1000 seconds to exit a 1 kpbs
  // network.
  EXPECT_EQ(last_delivered_packet.receive_time_us,
            TimeDelta::Seconds(1000).us());
  EXPECT_EQ(last_delivered_packet.packet_id, 999ul);
}

TEST(SimulatedNetworkTest, EnqueuePacketWithSubSecondNonMonotonicBehaviour) {
  // On multi-core systems, different threads can experience sub-millisecond
  // non monothonic behaviour when running on different cores. This test
  // checks that when a non monotonic packet enqueue, the network continues to
  // work and the out of order packet is sent anyway.
  SimulatedNetwork network =
      SimulatedNetwork({.link_capacity = DataRate::KilobitsPerSec(1)});
  ASSERT_TRUE(network.EnqueuePacket(PacketInFlightInfo(
      /*size=*/125, /*send_time_us=*/TimeDelta::Seconds(1).us(),
      /*packet_id=*/0)));
  ASSERT_TRUE(network.EnqueuePacket(PacketInFlightInfo(
      /*size=*/125, /*send_time_us=*/TimeDelta::Seconds(1).us() - 1,
      /*packet_id=*/1)));

  std::vector<PacketDeliveryInfo> delivered_packets =
      network.DequeueDeliverablePackets(
          /*receive_time_us=*/TimeDelta::Seconds(2).us());
  ASSERT_EQ(delivered_packets.size(), 1ul);
  EXPECT_EQ(delivered_packets[0].packet_id, 0ul);
  EXPECT_EQ(delivered_packets[0].receive_time_us, TimeDelta::Seconds(2).us());

  delivered_packets = network.DequeueDeliverablePackets(
      /*receive_time_us=*/TimeDelta::Seconds(3).us());
  ASSERT_EQ(delivered_packets.size(), 1ul);
  EXPECT_EQ(delivered_packets[0].packet_id, 1ul);
  EXPECT_EQ(delivered_packets[0].receive_time_us, TimeDelta::Seconds(3).us());
}

TEST(SimulatedNetworkTest, CanUseInjectedQueueAndDropPacketsAtQueueHead) {
  auto queue = std::make_unique<LeakyBucketNetworkQueue>();
  LeakyBucketNetworkQueue* queue_ptr = queue.get();
  SimulatedNetwork network =
      SimulatedNetwork({.link_capacity = DataRate::KilobitsPerSec(1)},
                       /*random_seed=*/1, std::move(queue));
  ASSERT_TRUE(network.EnqueuePacket(PacketInFlightInfo(
      DataSize::Bytes(125), Timestamp::Seconds(1), /*packet_id=*/0)));
  ASSERT_TRUE(network.EnqueuePacket(PacketInFlightInfo(
      DataSize::Bytes(125), Timestamp::Seconds(1), /*packet_id=*/1)));

  // packet 0 is already sent, packet 1 is in the queue and will be dropped.
  queue_ptr->DropOldestPacket();

  std::vector<PacketDeliveryInfo> delivered_packets =
      network.DequeueDeliverablePackets(network.NextDeliveryTimeUs().value());
  ASSERT_EQ(delivered_packets.size(), 2ul);
  EXPECT_THAT(
      delivered_packets,
      UnorderedElementsAre(Field(&PacketDeliveryInfo::packet_id, 0),
                           AllOf(Field(&PacketDeliveryInfo::packet_id, 1),
                                 Field(&PacketDeliveryInfo::receive_time_us,
                                       PacketDeliveryInfo::kNotReceived))));
}

TEST(SimulatedNetworkTest, DefaultPropagateEcn) {
  SimulatedNetwork network = SimulatedNetwork({});
  ASSERT_TRUE(network.EnqueuePacket(
      PacketInFlightInfo(DataSize::Bytes(125), Timestamp::Seconds(1),
                         /*packet_id=*/0, EcnMarking::kCe)));
  EXPECT_THAT(
      network.DequeueDeliverablePackets(network.NextDeliveryTimeUs().value()),
      ElementsAre(Field(&PacketDeliveryInfo::ecn, EcnMarking::kCe)));
}

TEST(SimulatedNetworkTest, CanBleachEcn) {
  SimulatedNetwork network = SimulatedNetwork({.forward_ecn = false});
  ASSERT_TRUE(network.EnqueuePacket(
      PacketInFlightInfo(DataSize::Bytes(125), Timestamp::Seconds(1),
                         /*packet_id=*/0, EcnMarking::kEct1)));
  ASSERT_TRUE(network.EnqueuePacket(
      PacketInFlightInfo(DataSize::Bytes(125), Timestamp::Seconds(1),
                         /*packet_id=*/0, EcnMarking::kCe)));
  EXPECT_THAT(
      network.DequeueDeliverablePackets(network.NextDeliveryTimeUs().value()),
      ElementsAre(Field(&PacketDeliveryInfo::ecn, EcnMarking::kNotEct),
                  Field(&PacketDeliveryInfo::ecn, EcnMarking::kNotEct)));
}

// TODO(bugs.webrtc.org/14525): Re-enable when the DCHECK will be uncommented
// and the non-monotonic events on real time clock tests is solved/understood.
// TEST(SimulatedNetworkDeathTest, EnqueuePacketExpectMonotonicSendTime) {
//   SimulatedNetwork network = SimulatedNetwork({.link_capacity =
//   DataRate::KilobitsPerSec(1)});
//   ASSERT_TRUE(network.EnqueuePacket(PacketInFlightInfo(
//       /*size=*/125, /*send_time_us=*/2'000'000, /*packet_id=*/0)));
//   EXPECT_DEATH_IF_SUPPORTED(network.EnqueuePacket(PacketInFlightInfo(
//       /*size=*/125, /*send_time_us=*/900'000, /*packet_id=*/1)), "");
// }
}  // namespace
}  // namespace webrtc
