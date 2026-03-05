/*
 *  Copyright 2025 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/test/network_emulation/leaky_bucket_network_queue.h"

#include <optional>

#include "api/test/simulated_network.h"
#include "api/transport/ecn_marking.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::Field;
using ::testing::Optional;
using ::testing::Property;

TEST(LeakyBucketNetworkQueueTest, EnqueuePacketReturnsFalseIfQueueIsFull) {
  LeakyBucketNetworkQueue queue;
  queue.SetMaxPacketCapacity(1);
  PacketInFlightInfo packet_info(DataSize::Bytes(123), Timestamp::Zero(),
                                 /*packet_id=*/1);
  EXPECT_TRUE(queue.EnqueuePacket(packet_info));
  EXPECT_FALSE(queue.EnqueuePacket(packet_info));
}

TEST(LeakyBucketNetworkQueueTest,
     EnqueuePacketReturnsFalseIfQueueIsFullAfterCapacityChange) {
  LeakyBucketNetworkQueue queue;
  PacketInFlightInfo packet_info(DataSize::Bytes(123), Timestamp::Zero(),
                                 /*packet_id=*/1);
  ASSERT_TRUE(queue.EnqueuePacket(packet_info));
  ASSERT_TRUE(queue.EnqueuePacket(packet_info));

  queue.SetMaxPacketCapacity(2);
  EXPECT_FALSE(queue.EnqueuePacket(packet_info));
  EXPECT_NE(queue.DequeuePacket(Timestamp::Seconds(125)), std::nullopt);
  EXPECT_TRUE(queue.EnqueuePacket(packet_info));
}

TEST(LeakyBucketNetworkQueueTest, ReturnsNullOptWhenEmtpy) {
  LeakyBucketNetworkQueue queue;
  EXPECT_TRUE(queue.empty());
  EXPECT_EQ(queue.DequeuePacket(Timestamp::Zero()), std::nullopt);
  EXPECT_EQ(queue.PeekNextPacket(), std::nullopt);
}

TEST(LeakyBucketNetworkQueueTest, DequeueDoesNotChangePacketInfo) {
  LeakyBucketNetworkQueue queue;
  EXPECT_TRUE(queue.empty());
  PacketInFlightInfo packet_info(DataSize::Bytes(123), Timestamp::Seconds(123),
                                 /*packet_id=*/1);
  queue.EnqueuePacket(packet_info);

  EXPECT_THAT(
      queue.DequeuePacket(Timestamp::Seconds(125)),
      Optional(AllOf(
          Field(&PacketInFlightInfo::packet_id, packet_info.packet_id),
          Property(&PacketInFlightInfo::packet_size, packet_info.packet_size()),
          Property(&PacketInFlightInfo::send_time, packet_info.send_time()))));
}

TEST(LeakyBucketNetworkQueueTest,
     Ect1PacketMarkedCeIfSoujournEqualOrGreaterThanMax) {
  LeakyBucketNetworkQueue queue(
      {.max_ect1_sojourn_time = TimeDelta::Millis(10),
       .target_ect1_sojourn_time = TimeDelta::Millis(5)});

  PacketInFlightInfo packet_info(DataSize::Bytes(123), Timestamp::Seconds(123),
                                 /*packet_id=*/1, EcnMarking::kEct1);
  queue.EnqueuePacket(packet_info);

  EXPECT_THAT(
      queue.DequeuePacket(Timestamp::Seconds(123) + TimeDelta::Millis(10)),
      Optional(Field(&PacketInFlightInfo::ecn, EcnMarking::kCe)));

  // Sojourn time greater than max.
  queue.EnqueuePacket(packet_info);
  EXPECT_THAT(
      queue.DequeuePacket(Timestamp::Seconds(123) + TimeDelta::Millis(11)),
      Optional(Field(&PacketInFlightInfo::ecn, EcnMarking::kCe)));
}

TEST(LeakyBucketNetworkQueueTest, Ect0PacketNeverMarkedCe) {
  LeakyBucketNetworkQueue queue(
      {.max_ect1_sojourn_time = TimeDelta::Millis(10),
       .target_ect1_sojourn_time = TimeDelta::Millis(5)});

  PacketInFlightInfo packet_info(DataSize::Bytes(123), Timestamp::Seconds(123),
                                 /*packet_id=*/1, EcnMarking::kEct0);
  queue.EnqueuePacket(packet_info);

  EXPECT_THAT(
      queue.DequeuePacket(Timestamp::Seconds(123) + TimeDelta::Millis(10)),
      Optional(Field(&PacketInFlightInfo::ecn, EcnMarking::kEct0)));
}

TEST(LeakyBucketNetworkQueueTest,
     Ect1PacketNotMarkedAsCeIfSoujournTimeLessOrEqualTarget) {
  LeakyBucketNetworkQueue queue(
      {.max_ect1_sojourn_time = TimeDelta::Millis(10),
       .target_ect1_sojourn_time = TimeDelta::Millis(5)});

  PacketInFlightInfo packet_info(DataSize::Bytes(123), Timestamp::Seconds(123),
                                 /*packet_id=*/1, EcnMarking::kEct1);
  queue.EnqueuePacket(packet_info);

  EXPECT_THAT(
      queue.DequeuePacket(Timestamp::Seconds(123) + TimeDelta::Millis(5)),
      Optional(Field(&PacketInFlightInfo::ecn, EcnMarking::kEct1)));

  queue.EnqueuePacket(packet_info);
  EXPECT_THAT(
      queue.DequeuePacket(Timestamp::Seconds(123) + TimeDelta::Millis(3)),
      Optional(Field(&PacketInFlightInfo::ecn, EcnMarking::kEct1)));
}

}  // namespace
}  // namespace webrtc
