/*
 *  Copyright 2025 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/network/token_bucket_network_behavior.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

#include "api/test/create_network_emulation_manager.h"
#include "api/test/network_emulation/leaky_bucket_network_queue.h"
#include "api/test/network_emulation/token_bucket_network_behavior_builder.h"
#include "api/test/network_emulation/token_bucket_network_behavior_config.h"
#include "api/test/network_emulation_manager.h"
#include "api/test/simulated_network.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::MockFunction;
using ::testing::SizeIs;

constexpr DataSize kPacketSize = DataSize::Bytes(1500);

TEST(TokenBucketNetworkBehaviorTest, PacketBurstIsAllowedThrough) {
  TokenBucketNetworkBehaviorConfig config = {
      .burst = 12 * kPacketSize, .rate = DataRate::KilobitsPerSec(512)};
  TokenBucketNetworkBehavior policer(config);

  int64_t send_time_us = 0;
  for (int i = 0; i < 12; ++i) {
    EXPECT_TRUE(policer.EnqueuePacket(
        PacketInFlightInfo(kPacketSize.bytes(), send_time_us, i)));
  }
  EXPECT_EQ(policer.NextDeliveryTimeUs(), send_time_us);
  EXPECT_EQ(policer.DequeueDeliverablePackets(send_time_us).size(), 12u);
  EXPECT_FALSE(policer.EnqueuePacket(
      PacketInFlightInfo(kPacketSize.bytes(), send_time_us, 12)));
  EXPECT_EQ(policer.NextDeliveryTimeUs(), std::nullopt);
  EXPECT_TRUE(policer.DequeueDeliverablePackets(send_time_us).empty());
}

TEST(TokenBucketNetworkBehaviorTest, BucketIsRefilledAtConfiguredRate) {
  TokenBucketNetworkBehaviorConfig config = {
      .burst = kPacketSize, .rate = DataRate::KilobitsPerSec(512)};
  TokenBucketNetworkBehavior policer(config);
  int64_t send_time_us = 0;
  int id = 0;
  EXPECT_TRUE(policer.EnqueuePacket(
      PacketInFlightInfo(kPacketSize.bytes(), send_time_us, id++)));
  EXPECT_FALSE(policer.EnqueuePacket(
      PacketInFlightInfo(kPacketSize.bytes(), send_time_us, id++)));

  send_time_us += (kPacketSize / 2 / config.rate).us();
  EXPECT_FALSE(policer.EnqueuePacket(
      PacketInFlightInfo(kPacketSize.bytes(), send_time_us, id++)));
  send_time_us += (kPacketSize / 2 / config.rate).us();
  EXPECT_TRUE(policer.EnqueuePacket(
      PacketInFlightInfo(kPacketSize.bytes(), send_time_us, id++)));
}

TEST(TokenBucketNetworkBehaviorTest, BucketDoesNotGrowAboveBurstSize) {
  TokenBucketNetworkBehaviorConfig config = {
      .burst = kPacketSize, .rate = DataRate::KilobitsPerSec(512)};
  TokenBucketNetworkBehavior policer(config);
  int64_t send_time_us = 0;
  int id = 0;
  EXPECT_TRUE(policer.EnqueuePacket(
      PacketInFlightInfo(kPacketSize.bytes(), send_time_us, id++)));
  // Increase time enough to fill the burst size twice.
  send_time_us += (2 * config.burst / config.rate).us();
  EXPECT_TRUE(policer.EnqueuePacket(
      PacketInFlightInfo(kPacketSize.bytes(), send_time_us, id++)));
  EXPECT_FALSE(policer.EnqueuePacket(
      PacketInFlightInfo(kPacketSize.bytes(), send_time_us, id++)));
}

TEST(TokenBucketNetworkBehaviorTest, DeliversPacketsFromQueue) {
  TokenBucketNetworkBehaviorConfig config = {
      .burst = kPacketSize, .rate = DataRate::KilobitsPerSec(512)};
  std::unique_ptr<LeakyBucketNetworkQueue> queue =
      std::make_unique<LeakyBucketNetworkQueue>();
  LeakyBucketNetworkQueue* queue_ptr = queue.get();

  TokenBucketNetworkBehavior policer(config, std::move(queue));
  int64_t time_us = 0;
  int id = 0;
  // One packet can be dequeued immediately.
  ASSERT_TRUE(policer.EnqueuePacket(
      PacketInFlightInfo(kPacketSize.bytes(), time_us, id++)));
  EXPECT_EQ(policer.NextDeliveryTimeUs(), time_us);
  ASSERT_TRUE(policer.EnqueuePacket(
      PacketInFlightInfo(kPacketSize.bytes(), time_us, id++)));
  EXPECT_EQ(policer.NextDeliveryTimeUs(), time_us);
  EXPECT_FALSE(queue_ptr->empty());
  // Dequeue the packet that is sent immediately.
  EXPECT_THAT(policer.DequeueDeliverablePackets(time_us), SizeIs(1));
  EXPECT_THAT(policer.DequeueDeliverablePackets(time_us), IsEmpty());
  // The other packet is still in the queue but the next delivery time is
  // known.
  EXPECT_FALSE(queue_ptr->empty());
  EXPECT_EQ(policer.NextDeliveryTimeUs(),
            time_us + (config.burst / config.rate).us());

  // Increase time to trigger the delivery of the packet in the queue.
  time_us += (config.burst / config.rate).us();
  EXPECT_THAT(
      policer.DequeueDeliverablePackets(time_us),
      AllOf(SizeIs(1),
            ElementsAre(Field(&PacketDeliveryInfo::receive_time_us, time_us))));
  EXPECT_TRUE(queue_ptr->empty());
}

TEST(TokenBucketNetworkBehaviorTest, DeliverTimeNotIncreasedIfQueueDropPacket) {
  TokenBucketNetworkBehaviorConfig config = {
      .burst = kPacketSize, .rate = DataRate::KilobitsPerSec(512)};
  std::unique_ptr<LeakyBucketNetworkQueue> queue =
      std::make_unique<LeakyBucketNetworkQueue>();
  LeakyBucketNetworkQueue* queue_ptr = queue.get();

  TokenBucketNetworkBehavior policer(config, std::move(queue));
  int64_t time_us = 0;
  int id = 0;
  // One packet can be dequeued immediately.
  ASSERT_TRUE(policer.EnqueuePacket(
      PacketInFlightInfo(kPacketSize.bytes(), time_us, id++)));
  EXPECT_THAT(policer.DequeueDeliverablePackets(time_us), SizeIs(1));
  ASSERT_TRUE(policer.EnqueuePacket(
      PacketInFlightInfo(kPacketSize.bytes(), time_us, id++)));
  EXPECT_EQ(policer.NextDeliveryTimeUs(),
            time_us + (config.burst / config.rate).us());
  EXPECT_FALSE(queue_ptr->empty());
  // Next time a packet is dequeued, the packet receive time should be
  // kNotReceived and no tokens should be used for the packet.
  queue_ptr->DropOldestPacket();
  // Add a new packet to the queue. It should be delivered (config.burst /
  // config.rate) later since the dropped packet should not reduce available
  // tokens.
  ASSERT_TRUE(policer.EnqueuePacket(
      PacketInFlightInfo(kPacketSize.bytes(), time_us, id++)));
  EXPECT_FALSE(queue_ptr->empty());
  EXPECT_EQ(policer.NextDeliveryTimeUs(),
            time_us + (config.burst / config.rate).us());
  time_us += (config.burst / config.rate).us();
  EXPECT_THAT(policer.DequeueDeliverablePackets(time_us),
              AllOf(SizeIs(2),
                    UnorderedElementsAre(
                        Field(&PacketDeliveryInfo::receive_time_us,
                              PacketDeliveryInfo::kNotReceived),
                        Field(&PacketDeliveryInfo::receive_time_us, time_us))));
  EXPECT_TRUE(queue_ptr->empty());
}

TEST(TokenBucketNetworkBehaviorTest,
     EnqueuePacketReturnsFalseIfBufferIsFullAndNoToken) {
  TokenBucketNetworkBehaviorConfig config = {
      .burst = kPacketSize, .rate = DataRate::KilobitsPerSec(512)};
  std::unique_ptr<LeakyBucketNetworkQueue> queue =
      std::make_unique<LeakyBucketNetworkQueue>();
  queue->SetMaxPacketCapacity(1);
  LeakyBucketNetworkQueue* queue_ptr = queue.get();

  TokenBucketNetworkBehavior policer(config, std::move(queue));
  int64_t time_us = 0;
  int id = 0;
  // One packet can be dequeued immediately.
  EXPECT_TRUE(policer.EnqueuePacket(
      PacketInFlightInfo(kPacketSize.bytes(), time_us, id++)));
  policer.DequeueDeliverablePackets(time_us);
  EXPECT_TRUE(queue_ptr->empty());
  // One packet can be enqueued.
  EXPECT_TRUE(policer.EnqueuePacket(
      PacketInFlightInfo(kPacketSize.bytes(), time_us, id++)));
  EXPECT_FALSE(queue_ptr->empty());
  EXPECT_FALSE(policer.EnqueuePacket(
      PacketInFlightInfo(kPacketSize.bytes(), time_us, id++)));
  EXPECT_FALSE(queue_ptr->empty());
}

TEST(TokenBucketNetworkBehaviorTest, CanUpdateConfig) {
  TokenBucketNetworkBehaviorConfig config = {
      .burst = DataSize::Bytes(0), .rate = DataRate::KilobitsPerSec(0)};
  TokenBucketNetworkBehavior policer(config);
  int64_t send_time_us = 0;
  int id = 0;
  EXPECT_FALSE(policer.EnqueuePacket(
      PacketInFlightInfo(kPacketSize.bytes(), send_time_us, id++)));

  policer.UpdateConfig([](TokenBucketNetworkBehaviorConfig& config) {
    config.burst = kPacketSize;
    config.rate = DataRate::KilobitsPerSec(512);
  });

  send_time_us += (kPacketSize / DataRate::KilobitsPerSec(512)).us();
  EXPECT_TRUE(policer.EnqueuePacket(
      PacketInFlightInfo(kPacketSize.bytes(), send_time_us, id++)));
  EXPECT_FALSE(policer.EnqueuePacket(
      PacketInFlightInfo(kPacketSize.bytes(), send_time_us, id++)));
}

TEST(TokenBucketNetworkBehaviorBuilderTest, BuildWithUpdateFunction) {
  std::unique_ptr<NetworkEmulationManager> network_emulation =
      CreateNetworkEmulationManager({.time_mode = TimeMode::kSimulated});

  const TokenBucketNetworkBehaviorConfig kConfig = {
      .burst = DataSize::Bytes(1000), .rate = DataRate::KilobitsPerSec(512)};
  TokenBucketNetworkBehaviorNodeBuilder builder(network_emulation.get());
  auto [policerlink, updatefunction] =
      builder.burst(kConfig.burst).rate(kConfig.rate).BuildWithUpdateFunction();
  EXPECT_NE(policerlink, nullptr);
  MockFunction<void(TokenBucketNetworkBehaviorConfig&)> updater;

  EXPECT_CALL(updater, Call)
      .WillOnce([&](TokenBucketNetworkBehaviorConfig& config) {
        EXPECT_EQ(config.burst, kConfig.burst);
        EXPECT_EQ(config.rate, kConfig.rate);
        config.burst = kConfig.burst * 2;
        config.rate = kConfig.rate * 2;
      });
  updatefunction(updater.AsStdFunction());

  EXPECT_CALL(updater, Call)
      .WillOnce([&](TokenBucketNetworkBehaviorConfig& config) {
        EXPECT_EQ(config.burst, kConfig.burst * 2);
        EXPECT_EQ(config.rate, kConfig.rate * 2);
      });
  updatefunction(updater.AsStdFunction());
}

}  // namespace

}  // namespace webrtc
