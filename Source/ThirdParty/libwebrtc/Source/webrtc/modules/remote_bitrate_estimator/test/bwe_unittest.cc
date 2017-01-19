/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/remote_bitrate_estimator/test/bwe.h"

#include <vector>

#include "webrtc/base/arraysize.h"
#include "webrtc/test/gtest.h"

namespace webrtc {
namespace testing {
namespace bwe {

const int kSetCapacity = 1000;

class LinkedSetTest : public ::testing::Test {
 public:
  LinkedSetTest() : linked_set_(kSetCapacity) {}

  ~LinkedSetTest() {}

 protected:
  LinkedSet linked_set_;
};

TEST_F(LinkedSetTest, EmptySet) {
  EXPECT_EQ(linked_set_.OldestSeqNumber(), 0);
  EXPECT_EQ(linked_set_.NewestSeqNumber(), 0);
}

TEST_F(LinkedSetTest, SinglePacket) {
  const uint16_t kSeqNumber = 1;  // Arbitrary.
  // Other parameters don't matter here.
  linked_set_.Insert(kSeqNumber, 0, 0, 0);

  EXPECT_EQ(linked_set_.OldestSeqNumber(), kSeqNumber);
  EXPECT_EQ(linked_set_.NewestSeqNumber(), kSeqNumber);
}

TEST_F(LinkedSetTest, MultiplePackets) {
  const uint16_t kNumberPackets = 100;

  std::vector<uint16_t> sequence_numbers;
  for (size_t i = 0; i < kNumberPackets; ++i) {
    sequence_numbers.push_back(static_cast<uint16_t>(i + 1));
  }
  random_shuffle(sequence_numbers.begin(), sequence_numbers.end());

  for (size_t i = 0; i < kNumberPackets; ++i) {
    // Other parameters don't matter here.
    linked_set_.Insert(static_cast<uint16_t>(i), 0, 0, 0);
  }

  // Packets arriving out of order should not affect the following values:
  EXPECT_EQ(linked_set_.OldestSeqNumber(), 0);
  EXPECT_EQ(linked_set_.NewestSeqNumber(), kNumberPackets - 1);
}

TEST_F(LinkedSetTest, Overflow) {
  const int kFirstSeqNumber = -100;
  const int kLastSeqNumber = 100;

  for (int i = kFirstSeqNumber; i <= kLastSeqNumber; ++i) {
    // Other parameters don't matter here.
    linked_set_.Insert(static_cast<uint16_t>(i), 0, 0, 0);
  }

  // Packets arriving out of order should not affect the following values:
  EXPECT_EQ(linked_set_.OldestSeqNumber(),
            static_cast<uint16_t>(kFirstSeqNumber));
  EXPECT_EQ(linked_set_.NewestSeqNumber(),
            static_cast<uint16_t>(kLastSeqNumber));
}

class SequenceNumberOlderThanTest : public ::testing::Test {
 public:
  SequenceNumberOlderThanTest() {}
  ~SequenceNumberOlderThanTest() {}

 protected:
  SequenceNumberOlderThan comparator_;
};

TEST_F(SequenceNumberOlderThanTest, Operator) {
  // Operator()(x, y) returns true <==> y is newer than x.
  EXPECT_TRUE(comparator_.operator()(0x0000, 0x0001));
  EXPECT_TRUE(comparator_.operator()(0x0001, 0x1000));
  EXPECT_FALSE(comparator_.operator()(0x0001, 0x0000));
  EXPECT_FALSE(comparator_.operator()(0x0002, 0x0002));
  EXPECT_TRUE(comparator_.operator()(0xFFF6, 0x000A));
  EXPECT_FALSE(comparator_.operator()(0x000A, 0xFFF6));
  EXPECT_TRUE(comparator_.operator()(0x0000, 0x8000));
  EXPECT_FALSE(comparator_.operator()(0x8000, 0x0000));
}

class LossAccountTest : public ::testing::Test {
 public:
  LossAccountTest() {}
  ~LossAccountTest() {}

 protected:
  LossAccount loss_account_;
};

TEST_F(LossAccountTest, Operations) {
  const size_t kTotal = 100;  // Arbitrary values.
  const size_t kLost = 10;

  LossAccount rhs(kTotal, kLost);

  loss_account_.Add(rhs);
  EXPECT_EQ(loss_account_.num_total, kTotal);
  EXPECT_EQ(loss_account_.num_lost, kLost);
  EXPECT_NEAR(loss_account_.LossRatio(), static_cast<float>(kLost) / kTotal,
              0.001f);

  loss_account_.Subtract(rhs);
  EXPECT_EQ(loss_account_.num_total, 0UL);
  EXPECT_EQ(loss_account_.num_lost, 0UL);
  EXPECT_NEAR(loss_account_.LossRatio(), 0.0f, 0.001f);
}

class BweReceiverTest : public ::testing::Test {
 public:
  BweReceiverTest() : bwe_receiver_(kFlowId) {}
  ~BweReceiverTest() {}

 protected:
  const int kFlowId = 1;  // Arbitrary.
  BweReceiver bwe_receiver_;
};

TEST_F(BweReceiverTest, ReceivingRateNoPackets) {
  EXPECT_EQ(bwe_receiver_.RecentKbps(), static_cast<size_t>(0));
}

TEST_F(BweReceiverTest, ReceivingRateSinglePacket) {
  const size_t kPayloadSizeBytes = 500 * 1000;
  const int64_t kSendTimeUs = 300 * 1000;
  const int64_t kArrivalTimeMs = kSendTimeUs / 1000 + 100;
  const uint16_t kSequenceNumber = 1;
  const int64_t kTimeWindowMs = BweReceiver::kReceivingRateTimeWindowMs;

  const MediaPacket media_packet(kFlowId, kSendTimeUs, kPayloadSizeBytes,
                                 kSequenceNumber);
  bwe_receiver_.ReceivePacket(kArrivalTimeMs, media_packet);

  const size_t kReceivingRateKbps = 8 * kPayloadSizeBytes / kTimeWindowMs;

  EXPECT_NEAR(bwe_receiver_.RecentKbps(), kReceivingRateKbps,
              static_cast<float>(kReceivingRateKbps) / 100.0f);
}

TEST_F(BweReceiverTest, ReceivingRateSmallPackets) {
  const size_t kPayloadSizeBytes = 100 * 1000;
  const int64_t kTimeGapMs = 50;  // Between each packet.
  const int64_t kOneWayDelayMs = 50;

  for (int i = 1; i < 50; ++i) {
    int64_t send_time_us = i * kTimeGapMs * 1000;
    int64_t arrival_time_ms = send_time_us / 1000 + kOneWayDelayMs;
    uint16_t sequence_number = i;
    const MediaPacket media_packet(kFlowId, send_time_us, kPayloadSizeBytes,
                                   sequence_number);
    bwe_receiver_.ReceivePacket(arrival_time_ms, media_packet);
  }

  const size_t kReceivingRateKbps = 8 * kPayloadSizeBytes / kTimeGapMs;
  EXPECT_NEAR(bwe_receiver_.RecentKbps(), kReceivingRateKbps,
              static_cast<float>(kReceivingRateKbps) / 100.0f);
}

TEST_F(BweReceiverTest, PacketLossNoPackets) {
  EXPECT_EQ(bwe_receiver_.RecentPacketLossRatio(), 0.0f);
}

TEST_F(BweReceiverTest, PacketLossSinglePacket) {
  const MediaPacket media_packet(kFlowId, 0, 0, 0);
  bwe_receiver_.ReceivePacket(0, media_packet);
  EXPECT_EQ(bwe_receiver_.RecentPacketLossRatio(), 0.0f);
}

TEST_F(BweReceiverTest, PacketLossContiguousPackets) {
  const int64_t kTimeWindowMs = BweReceiver::kPacketLossTimeWindowMs;
  size_t set_capacity = bwe_receiver_.GetSetCapacity();

  for (int i = 0; i < 10; ++i) {
    uint16_t sequence_number = static_cast<uint16_t>(i);
    // Sequence_number and flow_id are the only members that matter here.
    const MediaPacket media_packet(kFlowId, 0, 0, sequence_number);
    // Arrival time = 0, all packets will be considered.
    bwe_receiver_.ReceivePacket(0, media_packet);
  }
  EXPECT_EQ(bwe_receiver_.RecentPacketLossRatio(), 0.0f);

  for (int i = 30; i > 20; i--) {
    uint16_t sequence_number = static_cast<uint16_t>(i);
    // Sequence_number and flow_id are the only members that matter here.
    const MediaPacket media_packet(kFlowId, 0, 0, sequence_number);
    // Only the packets sent in this for loop will be considered.
    bwe_receiver_.ReceivePacket(2 * kTimeWindowMs, media_packet);
  }
  EXPECT_EQ(bwe_receiver_.RecentPacketLossRatio(), 0.0f);

  // Should handle uint16_t overflow.
  for (int i = 0xFFFF - 10; i < 0xFFFF + 10; ++i) {
    uint16_t sequence_number = static_cast<uint16_t>(i);
    const MediaPacket media_packet(kFlowId, 0, 0, sequence_number);
    // Only the packets sent in this for loop will be considered.
    bwe_receiver_.ReceivePacket(4 * kTimeWindowMs, media_packet);
  }
  EXPECT_EQ(bwe_receiver_.RecentPacketLossRatio(), 0.0f);

  // Should handle set overflow.
  for (int i = 0; i < set_capacity * 1.5; ++i) {
    uint16_t sequence_number = static_cast<uint16_t>(i);
    const MediaPacket media_packet(kFlowId, 0, 0, sequence_number);
    // Only the packets sent in this for loop will be considered.
    bwe_receiver_.ReceivePacket(6 * kTimeWindowMs, media_packet);
  }
  EXPECT_EQ(bwe_receiver_.RecentPacketLossRatio(), 0.0f);
}

// Should handle duplicates.
TEST_F(BweReceiverTest, PacketLossDuplicatedPackets) {
  const int64_t kTimeWindowMs = BweReceiver::kPacketLossTimeWindowMs;

  for (int i = 0; i < 10; ++i) {
    const MediaPacket media_packet(kFlowId, 0, 0, 0);
    // Arrival time = 0, all packets will be considered.
    bwe_receiver_.ReceivePacket(0, media_packet);
  }
  EXPECT_EQ(bwe_receiver_.RecentPacketLossRatio(), 0.0f);

  // Missing the element 5.
  const uint16_t kSequenceNumbers[] = {1, 2, 3, 4, 6, 7, 8};
  const int kNumPackets = arraysize(kSequenceNumbers);

  // Insert each sequence number twice.
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < kNumPackets; j++) {
      const MediaPacket media_packet(kFlowId, 0, 0, kSequenceNumbers[j]);
      // Only the packets sent in this for loop will be considered.
      bwe_receiver_.ReceivePacket(2 * kTimeWindowMs, media_packet);
    }
  }

  EXPECT_NEAR(bwe_receiver_.RecentPacketLossRatio(), 1.0f / (kNumPackets + 1),
              0.1f / (kNumPackets + 1));
}

TEST_F(BweReceiverTest, PacketLossLakingPackets) {
  size_t set_capacity = bwe_receiver_.GetSetCapacity();
  EXPECT_LT(set_capacity, static_cast<size_t>(0xFFFF));

  // Missing every other packet.
  for (size_t i = 0; i < set_capacity; ++i) {
    if ((i & 1) == 0) {  // Only even sequence numbers.
      uint16_t sequence_number = static_cast<uint16_t>(i);
      const MediaPacket media_packet(kFlowId, 0, 0, sequence_number);
      // Arrival time = 0, all packets will be considered.
      bwe_receiver_.ReceivePacket(0, media_packet);
    }
  }
  EXPECT_NEAR(bwe_receiver_.RecentPacketLossRatio(), 0.5f, 0.01f);
}

TEST_F(BweReceiverTest, PacketLossLakingFewPackets) {
  size_t set_capacity = bwe_receiver_.GetSetCapacity();
  EXPECT_LT(set_capacity, static_cast<size_t>(0xFFFF));

  const int kPeriod = 100;
  // Missing one for each kPeriod packets.
  for (size_t i = 0; i < set_capacity; ++i) {
    if ((i % kPeriod) != 0) {
      uint16_t sequence_number = static_cast<uint16_t>(i);
      const MediaPacket media_packet(kFlowId, 0, 0, sequence_number);
      // Arrival time = 0, all packets will be considered.
      bwe_receiver_.ReceivePacket(0, media_packet);
    }
  }
  EXPECT_NEAR(bwe_receiver_.RecentPacketLossRatio(), 1.0f / kPeriod,
              0.1f / kPeriod);
}

// Packet's sequence numbers greatly apart, expect high loss.
TEST_F(BweReceiverTest, PacketLossWideGap) {
  const int64_t kTimeWindowMs = BweReceiver::kPacketLossTimeWindowMs;

  const MediaPacket media_packet1(0, 0, 0, 1);
  const MediaPacket media_packet2(0, 0, 0, 1000);
  // Only these two packets will be considered.
  bwe_receiver_.ReceivePacket(0, media_packet1);
  bwe_receiver_.ReceivePacket(0, media_packet2);
  EXPECT_NEAR(bwe_receiver_.RecentPacketLossRatio(), 0.998f, 0.0001f);

  const MediaPacket media_packet3(0, 0, 0, 0);
  const MediaPacket media_packet4(0, 0, 0, 0x8000);
  // Only these two packets will be considered.
  bwe_receiver_.ReceivePacket(2 * kTimeWindowMs, media_packet3);
  bwe_receiver_.ReceivePacket(2 * kTimeWindowMs, media_packet4);
  EXPECT_NEAR(bwe_receiver_.RecentPacketLossRatio(), 0.99994f, 0.00001f);
}

// Packets arriving unordered should not be counted as losted.
TEST_F(BweReceiverTest, PacketLossUnorderedPackets) {
  size_t num_packets = bwe_receiver_.GetSetCapacity() / 2;
  std::vector<uint16_t> sequence_numbers;

  for (size_t i = 0; i < num_packets; ++i) {
    sequence_numbers.push_back(static_cast<uint16_t>(i + 1));
  }

  random_shuffle(sequence_numbers.begin(), sequence_numbers.end());

  for (size_t i = 0; i < num_packets; ++i) {
    const MediaPacket media_packet(kFlowId, 0, 0, sequence_numbers[i]);
    // Arrival time = 0, all packets will be considered.
    bwe_receiver_.ReceivePacket(0, media_packet);
  }

  EXPECT_EQ(bwe_receiver_.RecentPacketLossRatio(), 0.0f);
}

TEST_F(BweReceiverTest, RecentKbps) {
  EXPECT_EQ(bwe_receiver_.RecentKbps(), 0U);

  const size_t kPacketSizeBytes = 1200;
  const int kNumPackets = 100;

  double window_size_s = bwe_receiver_.BitrateWindowS();

  // Receive packets at the same time.
  for (int i = 0; i < kNumPackets; ++i) {
    MediaPacket packet(kFlowId, 0L, kPacketSizeBytes, static_cast<uint16_t>(i));
    bwe_receiver_.ReceivePacket(0, packet);
  }

  EXPECT_NEAR(bwe_receiver_.RecentKbps(),
              (8 * kNumPackets * kPacketSizeBytes) / (1000 * window_size_s),
              10);

  int64_t time_gap_ms =
      2 * 1000 * window_size_s;  // Larger than rate_counter time window.

  MediaPacket packet(kFlowId, time_gap_ms * 1000, kPacketSizeBytes,
                     static_cast<uint16_t>(kNumPackets));
  bwe_receiver_.ReceivePacket(time_gap_ms, packet);

  EXPECT_NEAR(bwe_receiver_.RecentKbps(),
              (8 * kPacketSizeBytes) / (1000 * window_size_s), 10);
}

TEST_F(BweReceiverTest, Loss) {
  EXPECT_NEAR(bwe_receiver_.GlobalReceiverPacketLossRatio(), 0.0f, 0.001f);

  LossAccount loss_account = bwe_receiver_.LinkedSetPacketLossRatio();
  EXPECT_NEAR(loss_account.LossRatio(), 0.0f, 0.001f);

  // Insert packets 1-50 and 151-200;
  for (int i = 1; i <= 200; ++i) {
    // Packet size and timestamp do not matter here.
    MediaPacket packet(kFlowId, 0L, 0UL, static_cast<uint16_t>(i));
    bwe_receiver_.ReceivePacket(0, packet);
    if (i == 50) {
      i += 100;
    }
  }

  loss_account = bwe_receiver_.LinkedSetPacketLossRatio();
  EXPECT_NEAR(loss_account.LossRatio(), 0.5f, 0.001f);

  bwe_receiver_.RelieveSetAndUpdateLoss();
  EXPECT_EQ(bwe_receiver_.received_packets_.size(), 100U / 10);

  // No packet loss within the preserved packets.
  loss_account = bwe_receiver_.LinkedSetPacketLossRatio();
  EXPECT_NEAR(loss_account.LossRatio(), 0.0f, 0.001f);

  // RelieveSetAndUpdateLoss automatically updates loss account.
  EXPECT_NEAR(bwe_receiver_.GlobalReceiverPacketLossRatio(), 0.5f, 0.001f);
}

}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
