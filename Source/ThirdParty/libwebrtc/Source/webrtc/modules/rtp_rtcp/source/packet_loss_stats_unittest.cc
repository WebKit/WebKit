/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/packet_loss_stats.h"

#include "test/gtest.h"

namespace webrtc {

class PacketLossStatsTest : public ::testing::Test {
 protected:
  PacketLossStats stats_;
};

// Add a lost packet as every other packet, they should all count as single
// losses.
TEST_F(PacketLossStatsTest, EveryOtherPacket) {
  for (int i = 0; i < 1000; i += 2) {
    stats_.AddLostPacket(i);
  }
  EXPECT_EQ(500, stats_.GetSingleLossCount());
  EXPECT_EQ(0, stats_.GetMultipleLossEventCount());
  EXPECT_EQ(0, stats_.GetMultipleLossPacketCount());
}

// Add a lost packet as every other packet, but such that the sequence numbers
// will wrap around while they are being added.
TEST_F(PacketLossStatsTest, EveryOtherPacketWrapped) {
  for (int i = 65500; i < 66500; i += 2) {
    stats_.AddLostPacket(i & 0xFFFF);
  }
  EXPECT_EQ(500, stats_.GetSingleLossCount());
  EXPECT_EQ(0, stats_.GetMultipleLossEventCount());
  EXPECT_EQ(0, stats_.GetMultipleLossPacketCount());
}

// Add a lost packet as every other packet, but such that the sequence numbers
// will wrap around close to the very end, such that the buffer contains packets
// on either side of the wrapping.
TEST_F(PacketLossStatsTest, EveryOtherPacketWrappedAtEnd) {
  for (int i = 64600; i < 65600; i += 2) {
    stats_.AddLostPacket(i & 0xFFFF);
  }
  EXPECT_EQ(500, stats_.GetSingleLossCount());
  EXPECT_EQ(0, stats_.GetMultipleLossEventCount());
  EXPECT_EQ(0, stats_.GetMultipleLossPacketCount());
}

// Add a lost packet as the first three of every eight packets. Each set of
// three should count as a multiple loss event and three multiple loss packets.
TEST_F(PacketLossStatsTest, FirstThreeOfEight) {
  for (int i = 0; i < 1000; ++i) {
    if ((i & 7) < 3) {
      stats_.AddLostPacket(i);
    }
  }
  EXPECT_EQ(0, stats_.GetSingleLossCount());
  EXPECT_EQ(125, stats_.GetMultipleLossEventCount());
  EXPECT_EQ(375, stats_.GetMultipleLossPacketCount());
}

// Add a lost packet as the first three of every eight packets such that the
// sequence numbers wrap in the middle of adding them.
TEST_F(PacketLossStatsTest, FirstThreeOfEightWrapped) {
  for (int i = 65500; i < 66500; ++i) {
    if ((i & 7) < 3) {
      stats_.AddLostPacket(i & 0xFFFF);
    }
  }
  EXPECT_EQ(0, stats_.GetSingleLossCount());
  EXPECT_EQ(125, stats_.GetMultipleLossEventCount());
  EXPECT_EQ(375, stats_.GetMultipleLossPacketCount());
}

// Add a lost packet as the first three of every eight packets such that the
// sequence numbers wrap near the end of adding them and there are still numbers
// in the buffer from before the wrapping.
TEST_F(PacketLossStatsTest, FirstThreeOfEightWrappedAtEnd) {
  for (int i = 64600; i < 65600; ++i) {
    if ((i & 7) < 3) {
      stats_.AddLostPacket(i & 0xFFFF);
    }
  }
  EXPECT_EQ(0, stats_.GetSingleLossCount());
  EXPECT_EQ(125, stats_.GetMultipleLossEventCount());
  EXPECT_EQ(375, stats_.GetMultipleLossPacketCount());
}

// Add loss packets as the first three and the fifth of every eight packets. The
// set of three should be multiple loss and the fifth should be single loss.
TEST_F(PacketLossStatsTest, FirstThreeAndFifthOfEight) {
  for (int i = 0; i < 1000; ++i) {
    if ((i & 7) < 3 || (i & 7) == 4) {
      stats_.AddLostPacket(i);
    }
  }
  EXPECT_EQ(125, stats_.GetSingleLossCount());
  EXPECT_EQ(125, stats_.GetMultipleLossEventCount());
  EXPECT_EQ(375, stats_.GetMultipleLossPacketCount());
}

// Add loss packets as the first three and the fifth of every eight packets such
// that the sequence numbers wrap in the middle of adding them.
TEST_F(PacketLossStatsTest, FirstThreeAndFifthOfEightWrapped) {
  for (int i = 65500; i < 66500; ++i) {
    if ((i & 7) < 3 || (i & 7) == 4) {
      stats_.AddLostPacket(i & 0xFFFF);
    }
  }
  EXPECT_EQ(125, stats_.GetSingleLossCount());
  EXPECT_EQ(125, stats_.GetMultipleLossEventCount());
  EXPECT_EQ(375, stats_.GetMultipleLossPacketCount());
}

// Add loss packets as the first three and the fifth of every eight packets such
// that the sequence numbers wrap near the end of adding them and there are
// packets from before the wrapping still in the buffer.
TEST_F(PacketLossStatsTest, FirstThreeAndFifthOfEightWrappedAtEnd) {
  for (int i = 64600; i < 65600; ++i) {
    if ((i & 7) < 3 || (i & 7) == 4) {
      stats_.AddLostPacket(i & 0xFFFF);
    }
  }
  EXPECT_EQ(125, stats_.GetSingleLossCount());
  EXPECT_EQ(125, stats_.GetMultipleLossEventCount());
  EXPECT_EQ(375, stats_.GetMultipleLossPacketCount());
}

// Add loss packets such that there is a multiple loss event that continues
// around the wrapping of sequence numbers.
TEST_F(PacketLossStatsTest, MultipleLossEventWrapped) {
  for (int i = 60000; i < 60500; i += 2) {
    stats_.AddLostPacket(i);
  }
  for (int i = 65530; i < 65540; ++i) {
    stats_.AddLostPacket(i & 0xFFFF);
  }
  EXPECT_EQ(250, stats_.GetSingleLossCount());
  EXPECT_EQ(1, stats_.GetMultipleLossEventCount());
  EXPECT_EQ(10, stats_.GetMultipleLossPacketCount());
}

// Add loss packets such that there is a multiple loss event that continues
// around the wrapping of sequence numbers and then is pushed out of the buffer.
TEST_F(PacketLossStatsTest, MultipleLossEventWrappedPushedOut) {
  for (int i = 60000; i < 60500; i += 2) {
    stats_.AddLostPacket(i);
  }
  for (int i = 65530; i < 65540; ++i) {
    stats_.AddLostPacket(i & 0xFFFF);
  }
  for (int i = 1000; i < 1500; i += 2) {
    stats_.AddLostPacket(i);
  }
  EXPECT_EQ(500, stats_.GetSingleLossCount());
  EXPECT_EQ(1, stats_.GetMultipleLossEventCount());
  EXPECT_EQ(10, stats_.GetMultipleLossPacketCount());
}

// Add loss packets out of order and ensure that they still get counted
// correctly as single or multiple loss events.
TEST_F(PacketLossStatsTest, OutOfOrder) {
  for (int i = 0; i < 1000; i += 10) {
    stats_.AddLostPacket(i + 5);
    stats_.AddLostPacket(i + 7);
    stats_.AddLostPacket(i + 4);
    stats_.AddLostPacket(i + 1);
    stats_.AddLostPacket(i + 2);
  }
  EXPECT_EQ(100, stats_.GetSingleLossCount());
  EXPECT_EQ(200, stats_.GetMultipleLossEventCount());
  EXPECT_EQ(400, stats_.GetMultipleLossPacketCount());
}

// Add loss packets out of order and ensure that they still get counted
// correctly as single or multiple loss events, and wrap in the middle of
// adding.
TEST_F(PacketLossStatsTest, OutOfOrderWrapped) {
  for (int i = 65000; i < 66000; i += 10) {
    stats_.AddLostPacket((i + 5) & 0xFFFF);
    stats_.AddLostPacket((i + 7) & 0xFFFF);
    stats_.AddLostPacket((i + 4) & 0xFFFF);
    stats_.AddLostPacket((i + 1) & 0xFFFF);
    stats_.AddLostPacket((i + 2) & 0xFFFF);
  }
  EXPECT_EQ(100, stats_.GetSingleLossCount());
  EXPECT_EQ(200, stats_.GetMultipleLossEventCount());
  EXPECT_EQ(400, stats_.GetMultipleLossPacketCount());
}

}  // namespace webrtc
