/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/remote_bitrate_estimator/packet_arrival_map.h"

#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(PacketArrivalMapTest, IsConsistentWhenEmpty) {
  PacketArrivalTimeMap map;

  EXPECT_EQ(map.begin_sequence_number(), map.end_sequence_number());
  EXPECT_FALSE(map.has_received(0));
  EXPECT_EQ(map.clamp(-5), 0);
  EXPECT_EQ(map.clamp(5), 0);
}

TEST(PacketArrivalMapTest, InsertsFirstItemIntoMap) {
  PacketArrivalTimeMap map;

  map.AddPacket(42, Timestamp::Millis(10));
  EXPECT_EQ(map.begin_sequence_number(), 42);
  EXPECT_EQ(map.end_sequence_number(), 43);

  EXPECT_FALSE(map.has_received(41));
  EXPECT_TRUE(map.has_received(42));
  EXPECT_FALSE(map.has_received(44));

  EXPECT_EQ(map.clamp(-100), 42);
  EXPECT_EQ(map.clamp(42), 42);
  EXPECT_EQ(map.clamp(100), 43);
}

TEST(PacketArrivalMapTest, InsertsWithGaps) {
  PacketArrivalTimeMap map;

  map.AddPacket(42, Timestamp::Zero());
  map.AddPacket(45, Timestamp::Millis(11));
  EXPECT_EQ(map.begin_sequence_number(), 42);
  EXPECT_EQ(map.end_sequence_number(), 46);

  EXPECT_FALSE(map.has_received(41));
  EXPECT_TRUE(map.has_received(42));
  EXPECT_FALSE(map.has_received(43));
  EXPECT_FALSE(map.has_received(44));
  EXPECT_TRUE(map.has_received(45));
  EXPECT_FALSE(map.has_received(46));

  EXPECT_EQ(map.get(42), Timestamp::Zero());
  EXPECT_LT(map.get(43), Timestamp::Zero());
  EXPECT_LT(map.get(44), Timestamp::Zero());
  EXPECT_EQ(map.get(45), Timestamp::Millis(11));

  EXPECT_EQ(map.clamp(-100), 42);
  EXPECT_EQ(map.clamp(44), 44);
  EXPECT_EQ(map.clamp(100), 46);
}

TEST(PacketArrivalMapTest, FindNextAtOrAfterWithGaps) {
  PacketArrivalTimeMap map;

  map.AddPacket(42, Timestamp::Zero());
  map.AddPacket(45, Timestamp::Millis(11));
  EXPECT_EQ(map.begin_sequence_number(), 42);
  EXPECT_EQ(map.end_sequence_number(), 46);

  PacketArrivalTimeMap::PacketArrivalTime packet = map.FindNextAtOrAfter(42);
  EXPECT_EQ(packet.arrival_time, Timestamp::Zero());
  EXPECT_EQ(packet.sequence_number, 42);

  packet = map.FindNextAtOrAfter(43);
  EXPECT_EQ(packet.arrival_time, Timestamp::Millis(11));
  EXPECT_EQ(packet.sequence_number, 45);
}

TEST(PacketArrivalMapTest, InsertsWithinBuffer) {
  PacketArrivalTimeMap map;

  map.AddPacket(42, Timestamp::Millis(10));
  map.AddPacket(45, Timestamp::Millis(11));

  map.AddPacket(43, Timestamp::Millis(12));
  map.AddPacket(44, Timestamp::Millis(13));

  EXPECT_EQ(map.begin_sequence_number(), 42);
  EXPECT_EQ(map.end_sequence_number(), 46);

  EXPECT_FALSE(map.has_received(41));
  EXPECT_TRUE(map.has_received(42));
  EXPECT_TRUE(map.has_received(43));
  EXPECT_TRUE(map.has_received(44));
  EXPECT_TRUE(map.has_received(45));
  EXPECT_FALSE(map.has_received(46));

  EXPECT_EQ(map.get(42), Timestamp::Millis(10));
  EXPECT_EQ(map.get(43), Timestamp::Millis(12));
  EXPECT_EQ(map.get(44), Timestamp::Millis(13));
  EXPECT_EQ(map.get(45), Timestamp::Millis(11));
}

TEST(PacketArrivalMapTest, GrowsBufferAndRemoveOld) {
  PacketArrivalTimeMap map;

  constexpr int64_t kLargeSeq = 42 + PacketArrivalTimeMap::kMaxNumberOfPackets;
  map.AddPacket(42, Timestamp::Millis(10));
  map.AddPacket(43, Timestamp::Millis(11));
  map.AddPacket(44, Timestamp::Millis(12));
  map.AddPacket(45, Timestamp::Millis(13));
  map.AddPacket(kLargeSeq, Timestamp::Millis(12));

  EXPECT_EQ(map.begin_sequence_number(), 43);
  EXPECT_EQ(map.end_sequence_number(), kLargeSeq + 1);
  EXPECT_EQ(map.end_sequence_number() - map.begin_sequence_number(),
            PacketArrivalTimeMap::kMaxNumberOfPackets);

  EXPECT_FALSE(map.has_received(41));
  EXPECT_FALSE(map.has_received(42));
  EXPECT_TRUE(map.has_received(43));
  EXPECT_TRUE(map.has_received(44));
  EXPECT_TRUE(map.has_received(45));
  EXPECT_FALSE(map.has_received(46));
  EXPECT_TRUE(map.has_received(kLargeSeq));
  EXPECT_FALSE(map.has_received(kLargeSeq + 1));
}

TEST(PacketArrivalMapTest, SequenceNumberJumpsDeletesAll) {
  PacketArrivalTimeMap map;

  constexpr int64_t kLargeSeq =
      42 + 2 * PacketArrivalTimeMap::kMaxNumberOfPackets;
  map.AddPacket(42, Timestamp::Millis(10));
  map.AddPacket(kLargeSeq, Timestamp::Millis(12));

  EXPECT_EQ(map.begin_sequence_number(), kLargeSeq);
  EXPECT_EQ(map.end_sequence_number(), kLargeSeq + 1);

  EXPECT_FALSE(map.has_received(42));
  EXPECT_TRUE(map.has_received(kLargeSeq));
  EXPECT_FALSE(map.has_received(kLargeSeq + 1));
}

TEST(PacketArrivalMapTest, ExpandsBeforeBeginning) {
  PacketArrivalTimeMap map;

  map.AddPacket(42, Timestamp::Millis(10));
  map.AddPacket(-1000, Timestamp::Millis(13));

  EXPECT_EQ(map.begin_sequence_number(), -1000);
  EXPECT_EQ(map.end_sequence_number(), 43);

  EXPECT_FALSE(map.has_received(-1001));
  EXPECT_TRUE(map.has_received(-1000));
  EXPECT_FALSE(map.has_received(-999));
  EXPECT_TRUE(map.has_received(42));
  EXPECT_FALSE(map.has_received(43));
}

TEST(PacketArrivalMapTest, ExpandingBeforeBeginningKeepsReceived) {
  PacketArrivalTimeMap map;

  map.AddPacket(42, Timestamp::Millis(10));
  constexpr int64_t kSmallSeq =
      static_cast<int64_t>(42) - 2 * PacketArrivalTimeMap::kMaxNumberOfPackets;
  map.AddPacket(kSmallSeq, Timestamp::Millis(13));

  EXPECT_EQ(map.begin_sequence_number(), 42);
  EXPECT_EQ(map.end_sequence_number(), 43);
}

TEST(PacketArrivalMapTest, ErasesToRemoveElements) {
  PacketArrivalTimeMap map;

  map.AddPacket(42, Timestamp::Millis(10));
  map.AddPacket(43, Timestamp::Millis(11));
  map.AddPacket(44, Timestamp::Millis(12));
  map.AddPacket(45, Timestamp::Millis(13));

  map.EraseTo(44);

  EXPECT_EQ(map.begin_sequence_number(), 44);
  EXPECT_EQ(map.end_sequence_number(), 46);

  EXPECT_FALSE(map.has_received(43));
  EXPECT_TRUE(map.has_received(44));
  EXPECT_TRUE(map.has_received(45));
  EXPECT_FALSE(map.has_received(46));
}

TEST(PacketArrivalMapTest, ErasesInEmptyMap) {
  PacketArrivalTimeMap map;

  EXPECT_EQ(map.begin_sequence_number(), map.end_sequence_number());

  map.EraseTo(map.end_sequence_number());
  EXPECT_EQ(map.begin_sequence_number(), map.end_sequence_number());
}

TEST(PacketArrivalMapTest, IsTolerantToWrongArgumentsForErase) {
  PacketArrivalTimeMap map;

  map.AddPacket(42, Timestamp::Millis(10));
  map.AddPacket(43, Timestamp::Millis(11));

  map.EraseTo(1);

  EXPECT_EQ(map.begin_sequence_number(), 42);
  EXPECT_EQ(map.end_sequence_number(), 44);

  map.EraseTo(100);

  EXPECT_EQ(map.begin_sequence_number(), 44);
  EXPECT_EQ(map.end_sequence_number(), 44);
}

TEST(PacketArrivalMapTest, EraseAllRemembersBeginningSeqNbr) {
  PacketArrivalTimeMap map;

  map.AddPacket(42, Timestamp::Millis(10));
  map.AddPacket(43, Timestamp::Millis(11));
  map.AddPacket(44, Timestamp::Millis(12));
  map.AddPacket(45, Timestamp::Millis(13));

  map.EraseTo(46);

  map.AddPacket(50, Timestamp::Millis(10));

  EXPECT_EQ(map.begin_sequence_number(), 46);
  EXPECT_EQ(map.end_sequence_number(), 51);

  EXPECT_FALSE(map.has_received(45));
  EXPECT_FALSE(map.has_received(46));
  EXPECT_FALSE(map.has_received(47));
  EXPECT_FALSE(map.has_received(48));
  EXPECT_FALSE(map.has_received(49));
  EXPECT_TRUE(map.has_received(50));
  EXPECT_FALSE(map.has_received(51));
}

TEST(PacketArrivalMapTest, EraseToMissingSequenceNumber) {
  PacketArrivalTimeMap map;

  map.AddPacket(37, Timestamp::Millis(10));
  map.AddPacket(39, Timestamp::Millis(11));
  map.AddPacket(40, Timestamp::Millis(12));
  map.AddPacket(41, Timestamp::Millis(13));

  map.EraseTo(38);

  map.AddPacket(42, Timestamp::Millis(40));

  EXPECT_EQ(map.begin_sequence_number(), 38);
  EXPECT_EQ(map.end_sequence_number(), 43);

  EXPECT_FALSE(map.has_received(37));
  EXPECT_FALSE(map.has_received(38));
  EXPECT_TRUE(map.has_received(39));
  EXPECT_TRUE(map.has_received(40));
  EXPECT_TRUE(map.has_received(41));
  EXPECT_TRUE(map.has_received(42));
}

}  // namespace
}  // namespace webrtc
