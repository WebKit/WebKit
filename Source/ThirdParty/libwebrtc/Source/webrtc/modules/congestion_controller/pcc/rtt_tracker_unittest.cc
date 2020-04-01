/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/pcc/rtt_tracker.h"

#include "test/gtest.h"

namespace webrtc {
namespace pcc {
namespace test {
namespace {
const TimeDelta kInitialRtt = TimeDelta::Micros(10);
constexpr double kAlpha = 0.9;
const Timestamp kStartTime = Timestamp::Seconds(0);

PacketResult GetPacketWithRtt(TimeDelta rtt) {
  SentPacket packet;
  packet.send_time = kStartTime;
  PacketResult packet_result;
  packet_result.sent_packet = packet;
  if (rtt.IsFinite()) {
    packet_result.receive_time = kStartTime + rtt;
  } else {
    packet_result.receive_time = Timestamp::PlusInfinity();
  }
  return packet_result;
}
}  // namespace

TEST(PccRttTrackerTest, InitialValue) {
  RttTracker tracker{kInitialRtt, kAlpha};
  EXPECT_EQ(kInitialRtt, tracker.GetRtt());
  for (int i = 0; i < 100; ++i) {
    tracker.OnPacketsFeedback({GetPacketWithRtt(kInitialRtt)},
                              kStartTime + kInitialRtt);
  }
  EXPECT_EQ(kInitialRtt, tracker.GetRtt());
}

TEST(PccRttTrackerTest, DoNothingWhenPacketIsLost) {
  RttTracker tracker{kInitialRtt, kAlpha};
  tracker.OnPacketsFeedback({GetPacketWithRtt(TimeDelta::PlusInfinity())},
                            kStartTime + kInitialRtt);
  EXPECT_EQ(tracker.GetRtt(), kInitialRtt);
}

TEST(PccRttTrackerTest, ChangeInRtt) {
  RttTracker tracker{kInitialRtt, kAlpha};
  const TimeDelta kNewRtt = TimeDelta::Micros(100);
  tracker.OnPacketsFeedback({GetPacketWithRtt(kNewRtt)}, kStartTime + kNewRtt);
  EXPECT_GT(tracker.GetRtt(), kInitialRtt);
  EXPECT_LE(tracker.GetRtt(), kNewRtt);
  for (int i = 0; i < 100; ++i) {
    tracker.OnPacketsFeedback({GetPacketWithRtt(kNewRtt)},
                              kStartTime + kNewRtt);
  }
  const TimeDelta absolute_error = TimeDelta::Micros(1);
  EXPECT_NEAR(tracker.GetRtt().us(), kNewRtt.us(), absolute_error.us());
  EXPECT_LE(tracker.GetRtt(), kNewRtt);
}

}  // namespace test
}  // namespace pcc
}  // namespace webrtc
