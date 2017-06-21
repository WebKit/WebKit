/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <utility>

#include "webrtc/base/gunit.h"
#include "webrtc/p2p/base/packetlossestimator.h"

using cricket::PacketLossEstimator;

class PacketLossEstimatorTest : public testing::Test {};

TEST_F(PacketLossEstimatorTest, InitialResponseRate) {
  PacketLossEstimator ple(5, 100);
  EXPECT_EQ(1.0, ple.get_response_rate());
}

TEST_F(PacketLossEstimatorTest, InitialUpdateResponseRate) {
  PacketLossEstimator ple(5, 100);
  ple.UpdateResponseRate(10);
  EXPECT_EQ(1.0, ple.get_response_rate());
}

TEST_F(PacketLossEstimatorTest, ResponseReceived) {
  PacketLossEstimator ple(5, 100);

  ple.ExpectResponse("a", 0);
  ple.ReceivedResponse("a", 1);
  ple.UpdateResponseRate(2);

  EXPECT_EQ(1.0, ple.get_response_rate());
}

TEST_F(PacketLossEstimatorTest, ResponseNotConsideredLostYet) {
  PacketLossEstimator ple(5, 100);

  ple.ExpectResponse("a", 0);
  ple.UpdateResponseRate(2);

  EXPECT_EQ(1.0, ple.get_response_rate());
}

TEST_F(PacketLossEstimatorTest, ResponseConsideredLost) {
  PacketLossEstimator ple(5, 100);

  ple.ExpectResponse("a", 0);
  ple.UpdateResponseRate(10);

  EXPECT_EQ(0.0, ple.get_response_rate());
}

TEST_F(PacketLossEstimatorTest, ResponseLate) {
  PacketLossEstimator ple(5, 100);

  ple.ExpectResponse("a", 0);
  ple.ReceivedResponse("a", 6);
  ple.UpdateResponseRate(10);

  EXPECT_EQ(1.0, ple.get_response_rate());
}

TEST_F(PacketLossEstimatorTest, ResponseForgotten) {
  PacketLossEstimator ple(5, 100);
  ple.ExpectResponse("a", 0);
  ple.UpdateResponseRate(101);

  EXPECT_EQ(1.0, ple.get_response_rate());
}

TEST_F(PacketLossEstimatorTest, Lost1_Received1) {
  PacketLossEstimator ple(5, 100);

  ple.ExpectResponse("a", 0);
  ple.ExpectResponse("b", 2);
  ple.ReceivedResponse("b", 6);
  ple.UpdateResponseRate(7);

  EXPECT_EQ(0.5, ple.get_response_rate());
}

static const std::pair<std::string, int64_t> kFivePackets[5] = {{"a", 0},
                                                                {"b", 2},
                                                                {"c", 4},
                                                                {"d", 6},
                                                                {"e", 8}};

// Time: 0 1 2 3 4 5 6 7 8 9 10
// Sent: a   b   c   d   e   |
// Recv:       b             |
// Wind: -------------->     |
TEST_F(PacketLossEstimatorTest, Lost3_Received1_Waiting1) {
  PacketLossEstimator ple(3, 100);

  for (const auto& p : kFivePackets) {
    ple.ExpectResponse(p.first, p.second);
  }
  ple.ReceivedResponse("b", 3);
  ple.UpdateResponseRate(10);
  EXPECT_EQ(0.25, ple.get_response_rate());
}

// Time: 0 1 2 3 4 5 6 7 8 9 10
// Sent: a   b   c   d   e   |
// Recv:                   e |
// Wind: -------------->     |
TEST_F(PacketLossEstimatorTest, Lost4_Early1) {
  PacketLossEstimator ple(3, 100);

  for (const auto& p : kFivePackets) {
    ple.ExpectResponse(p.first, p.second);
  }
  ple.ReceivedResponse("e", 9);
  ple.UpdateResponseRate(10);
  // e should be considered, even though its response was received less than
  // |consider_lost_after_ms| ago.
  EXPECT_EQ(0.2, ple.get_response_rate());
}

// Time: 0 1 2 3 4 5 6 7 8 9 10
// Sent: a   b   c   d   e   |
// Recv:           c         |
// Wind:       <------->     |
TEST_F(PacketLossEstimatorTest, Forgot2_Received1_Lost1_Waiting1) {
  PacketLossEstimator ple(3, 7);

  for (const auto& p : kFivePackets) {
    ple.ExpectResponse(p.first, p.second);
  }
  ple.ReceivedResponse("c", 5);
  ple.UpdateResponseRate(10);
  EXPECT_EQ(0.5, ple.get_response_rate());
}

// Tests that old messages are forgotten if ExpectResponse is called
// |forget_after_ms| after |last_forgot_at|. It is important that ExpectResponse
// and ReceivedResponse forget old messages to avoid |tracked_packets_| growing
// without bound if UpdateResponseRate is never called (or rarely called).
//
// Time: 0 1 2 3 4 5 6
// Sent: a           b
// Wind:   <------->
TEST_F(PacketLossEstimatorTest, ExpectResponseForgetsOldPackets) {
  PacketLossEstimator ple(1, 5);
  ple.ExpectResponse("a", 0);  // This message will be forgotten.
  ple.ExpectResponse("b", 6);  // This call should trigger clean up.
  // a should be forgotten, b should be tracked.
  EXPECT_EQ(1u, ple.tracked_packet_count_for_testing());
}

// Tests that old messages are forgotten if ExpectResponse is called
// |forget_after_ms| after |last_forgot_at|. It is important that ExpectResponse
// and ReceivedResponse forget old messages to avoid |tracked_packets_| growing
// without bound if UpdateResponseRate is never called (or rarely called).
//
// Time: 0 1 2 3 4 5 6
// Sent: a
// Recv:             b
// Wind:   <------->
TEST_F(PacketLossEstimatorTest, ReceivedResponseForgetsOldPackets) {
  PacketLossEstimator ple(1, 5);
  ple.ExpectResponse("a", 0);    // This message will be forgotten.
  ple.ReceivedResponse("b", 6);  // This call should trigger clean up.
  // a should be forgoten, b should not be tracked (received but not sent).
  EXPECT_EQ(0u, ple.tracked_packet_count_for_testing());
}
