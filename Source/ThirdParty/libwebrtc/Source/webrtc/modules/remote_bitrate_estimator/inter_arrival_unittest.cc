/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "common_types.h"  // NOLINT(build/include)
#include "modules/remote_bitrate_estimator/inter_arrival.h"
#include "test/gtest.h"

namespace webrtc {
namespace testing {

enum {
  kTimestampGroupLengthUs = 5000,
  kMinStep = 20,
  kTriggerNewGroupUs = kTimestampGroupLengthUs + kMinStep,
  kBurstThresholdMs = 5,
  kAbsSendTimeFraction = 18,
  kAbsSendTimeInterArrivalUpshift = 8,
  kInterArrivalShift = kAbsSendTimeFraction + kAbsSendTimeInterArrivalUpshift,
};

const double kRtpTimestampToMs = 1.0 / 90.0;
const double kAstToMs = 1000.0 / static_cast<double>(1 << kInterArrivalShift);

class InterArrivalTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    inter_arrival_.reset(
        new InterArrival(kTimestampGroupLengthUs / 1000, 1.0, true));
    inter_arrival_rtp_.reset(new InterArrival(
        MakeRtpTimestamp(kTimestampGroupLengthUs), kRtpTimestampToMs, true));
    inter_arrival_ast_.reset(new InterArrival(
        MakeAbsSendTime(kTimestampGroupLengthUs), kAstToMs, true));
  }

  // Test that neither inter_arrival instance complete the timestamp group from
  // the given data.
  void ExpectFalse(int64_t timestamp_us,
                   int64_t arrival_time_ms,
                   size_t packet_size) {
    InternalExpectFalse(inter_arrival_rtp_.get(),
                        MakeRtpTimestamp(timestamp_us), arrival_time_ms,
                        packet_size);
    InternalExpectFalse(inter_arrival_ast_.get(), MakeAbsSendTime(timestamp_us),
                        arrival_time_ms, packet_size);
  }

  // Test that both inter_arrival instances complete the timestamp group from
  // the given data and that all returned deltas are as expected (except
  // timestamp delta, which is rounded from us to different ranges and must
  // match within an interval, given in |timestamp_near].
  void ExpectTrue(int64_t timestamp_us,
                  int64_t arrival_time_ms,
                  size_t packet_size,
                  int64_t expected_timestamp_delta_us,
                  int64_t expected_arrival_time_delta_ms,
                  int expected_packet_size_delta,
                  uint32_t timestamp_near) {
    InternalExpectTrue(inter_arrival_rtp_.get(), MakeRtpTimestamp(timestamp_us),
                       arrival_time_ms, packet_size,
                       MakeRtpTimestamp(expected_timestamp_delta_us),
                       expected_arrival_time_delta_ms,
                       expected_packet_size_delta, timestamp_near);
    InternalExpectTrue(inter_arrival_ast_.get(), MakeAbsSendTime(timestamp_us),
                       arrival_time_ms, packet_size,
                       MakeAbsSendTime(expected_timestamp_delta_us),
                       expected_arrival_time_delta_ms,
                       expected_packet_size_delta, timestamp_near << 8);
  }

  void WrapTestHelper(int64_t wrap_start_us,
                      uint32_t timestamp_near,
                      bool unorderly_within_group) {
    // Step through the range of a 32 bit int, 1/4 at a time to not cause
    // packets close to wraparound to be judged as out of order.

    // G1
    int64_t arrival_time = 17;
    ExpectFalse(0, arrival_time, 1);

    // G2
    arrival_time += kBurstThresholdMs + 1;
    ExpectFalse(wrap_start_us / 4, arrival_time, 1);

    // G3
    arrival_time += kBurstThresholdMs + 1;
    ExpectTrue(wrap_start_us / 2, arrival_time, 1, wrap_start_us / 4, 6,
               0,  // Delta G2-G1
               0);

    // G4
    arrival_time += kBurstThresholdMs + 1;
    int64_t g4_arrival_time = arrival_time;
    ExpectTrue(wrap_start_us / 2 + wrap_start_us / 4, arrival_time, 1,
               wrap_start_us / 4, 6, 0,  // Delta G3-G2
               timestamp_near);

    // G5
    arrival_time += kBurstThresholdMs + 1;
    ExpectTrue(wrap_start_us, arrival_time, 2, wrap_start_us / 4, 6,
               0,  // Delta G4-G3
               timestamp_near);
    for (int i = 0; i < 10; ++i) {
      // Slowly step across the wrap point.
      arrival_time += kBurstThresholdMs + 1;
      if (unorderly_within_group) {
        // These packets arrive with timestamps in decreasing order but are
        // nevertheless accumulated to group because their timestamps are higher
        // than the initial timestamp of the group.
        ExpectFalse(wrap_start_us + kMinStep * (9 - i), arrival_time, 1);
      } else {
        ExpectFalse(wrap_start_us + kMinStep * i, arrival_time, 1);
      }
    }
    int64_t g5_arrival_time = arrival_time;

    // This packet is out of order and should be dropped.
    arrival_time += kBurstThresholdMs + 1;
    ExpectFalse(wrap_start_us - 100, arrival_time, 100);

    // G6
    arrival_time += kBurstThresholdMs + 1;
    int64_t g6_arrival_time = arrival_time;
    ExpectTrue(wrap_start_us + kTriggerNewGroupUs, arrival_time, 10,
               wrap_start_us / 4 + 9 * kMinStep,
               g5_arrival_time - g4_arrival_time,
               (2 + 10) - 1,  // Delta G5-G4
               timestamp_near);

    // This packet is out of order and should be dropped.
    arrival_time += kBurstThresholdMs + 1;
    ExpectFalse(wrap_start_us + kTimestampGroupLengthUs, arrival_time, 100);

    // G7
    arrival_time += kBurstThresholdMs + 1;
    ExpectTrue(wrap_start_us + 2 * kTriggerNewGroupUs, arrival_time, 100,
               // Delta G6-G5
               kTriggerNewGroupUs - 9 * kMinStep,
               g6_arrival_time - g5_arrival_time, 10 - (2 + 10),
               timestamp_near);
  }

  std::unique_ptr<InterArrival> inter_arrival_;

 private:
  static uint32_t MakeRtpTimestamp(int64_t us) {
    return static_cast<uint32_t>(static_cast<uint64_t>(us * 90 + 500) / 1000);
  }

  static uint32_t MakeAbsSendTime(int64_t us) {
    uint32_t absolute_send_time =
        static_cast<uint32_t>(((static_cast<uint64_t>(us) << 18) + 500000) /
                              1000000) &
        0x00FFFFFFul;
    return absolute_send_time << 8;
  }

  static void InternalExpectFalse(InterArrival* inter_arrival,
                                  uint32_t timestamp,
                                  int64_t arrival_time_ms,
                                  size_t packet_size) {
    uint32_t dummy_timestamp = 101;
    int64_t dummy_arrival_time_ms = 303;
    int dummy_packet_size = 909;
    bool computed = inter_arrival->ComputeDeltas(
        timestamp, arrival_time_ms, arrival_time_ms, packet_size,
        &dummy_timestamp, &dummy_arrival_time_ms, &dummy_packet_size);
    EXPECT_EQ(computed, false);
    EXPECT_EQ(101ul, dummy_timestamp);
    EXPECT_EQ(303, dummy_arrival_time_ms);
    EXPECT_EQ(909, dummy_packet_size);
  }

  static void InternalExpectTrue(InterArrival* inter_arrival,
                                 uint32_t timestamp,
                                 int64_t arrival_time_ms,
                                 size_t packet_size,
                                 uint32_t expected_timestamp_delta,
                                 int64_t expected_arrival_time_delta_ms,
                                 int expected_packet_size_delta,
                                 uint32_t timestamp_near) {
    uint32_t delta_timestamp = 101;
    int64_t delta_arrival_time_ms = 303;
    int delta_packet_size = 909;
    bool computed = inter_arrival->ComputeDeltas(
        timestamp, arrival_time_ms, arrival_time_ms, packet_size,
        &delta_timestamp, &delta_arrival_time_ms, &delta_packet_size);
    EXPECT_EQ(true, computed);
    EXPECT_NEAR(expected_timestamp_delta, delta_timestamp, timestamp_near);
    EXPECT_EQ(expected_arrival_time_delta_ms, delta_arrival_time_ms);
    EXPECT_EQ(expected_packet_size_delta, delta_packet_size);
  }

  std::unique_ptr<InterArrival> inter_arrival_rtp_;
  std::unique_ptr<InterArrival> inter_arrival_ast_;
};

TEST_F(InterArrivalTest, FirstPacket) {
  ExpectFalse(0, 17, 1);
}

TEST_F(InterArrivalTest, FirstGroup) {
  // G1
  int64_t arrival_time = 17;
  int64_t g1_arrival_time = arrival_time;
  ExpectFalse(0, arrival_time, 1);

  // G2
  arrival_time += kBurstThresholdMs + 1;
  int64_t g2_arrival_time = arrival_time;
  ExpectFalse(kTriggerNewGroupUs, arrival_time, 2);

  // G3
  // Only once the first packet of the third group arrives, do we see the deltas
  // between the first two.
  arrival_time += kBurstThresholdMs + 1;
  ExpectTrue(2 * kTriggerNewGroupUs, arrival_time, 1,
             // Delta G2-G1
             kTriggerNewGroupUs, g2_arrival_time - g1_arrival_time, 1, 0);
}

TEST_F(InterArrivalTest, SecondGroup) {
  // G1
  int64_t arrival_time = 17;
  int64_t g1_arrival_time = arrival_time;
  ExpectFalse(0, arrival_time, 1);

  // G2
  arrival_time += kBurstThresholdMs + 1;
  int64_t g2_arrival_time = arrival_time;
  ExpectFalse(kTriggerNewGroupUs, arrival_time, 2);

  // G3
  arrival_time += kBurstThresholdMs + 1;
  int64_t g3_arrival_time = arrival_time;
  ExpectTrue(2 * kTriggerNewGroupUs, arrival_time, 1,
             // Delta G2-G1
             kTriggerNewGroupUs, g2_arrival_time - g1_arrival_time, 1, 0);

  // G4
  // First packet of 4th group yields deltas between group 2 and 3.
  arrival_time += kBurstThresholdMs + 1;
  ExpectTrue(3 * kTriggerNewGroupUs, arrival_time, 2,
             // Delta G3-G2
             kTriggerNewGroupUs, g3_arrival_time - g2_arrival_time, -1, 0);
}

TEST_F(InterArrivalTest, AccumulatedGroup) {
  // G1
  int64_t arrival_time = 17;
  int64_t g1_arrival_time = arrival_time;
  ExpectFalse(0, arrival_time, 1);

  // G2
  arrival_time += kBurstThresholdMs + 1;
  ExpectFalse(kTriggerNewGroupUs, 28, 2);
  int64_t timestamp = kTriggerNewGroupUs;
  for (int i = 0; i < 10; ++i) {
    // A bunch of packets arriving within the same group.
    arrival_time += kBurstThresholdMs + 1;
    timestamp += kMinStep;
    ExpectFalse(timestamp, arrival_time, 1);
  }
  int64_t g2_arrival_time = arrival_time;
  int64_t g2_timestamp = timestamp;

  // G3
  arrival_time = 500;
  ExpectTrue(2 * kTriggerNewGroupUs, arrival_time, 100, g2_timestamp,
             g2_arrival_time - g1_arrival_time,
             (2 + 10) - 1,  // Delta G2-G1
             0);
}

TEST_F(InterArrivalTest, OutOfOrderPacket) {
  // G1
  int64_t arrival_time = 17;
  int64_t timestamp = 0;
  ExpectFalse(timestamp, arrival_time, 1);
  int64_t g1_timestamp = timestamp;
  int64_t g1_arrival_time = arrival_time;

  // G2
  arrival_time += 11;
  timestamp += kTriggerNewGroupUs;
  ExpectFalse(timestamp, 28, 2);
  for (int i = 0; i < 10; ++i) {
    arrival_time += kBurstThresholdMs + 1;
    timestamp += kMinStep;
    ExpectFalse(timestamp, arrival_time, 1);
  }
  int64_t g2_timestamp = timestamp;
  int64_t g2_arrival_time = arrival_time;

  // This packet is out of order and should be dropped.
  arrival_time = 281;
  ExpectFalse(g1_timestamp, arrival_time, 100);

  // G3
  arrival_time = 500;
  timestamp = 2 * kTriggerNewGroupUs;
  ExpectTrue(timestamp, arrival_time, 100,
             // Delta G2-G1
             g2_timestamp - g1_timestamp, g2_arrival_time - g1_arrival_time,
             (2 + 10) - 1, 0);
}

TEST_F(InterArrivalTest, OutOfOrderWithinGroup) {
  // G1
  int64_t arrival_time = 17;
  int64_t timestamp = 0;
  ExpectFalse(timestamp, arrival_time, 1);
  int64_t g1_timestamp = timestamp;
  int64_t g1_arrival_time = arrival_time;

  // G2
  timestamp += kTriggerNewGroupUs;
  arrival_time += 11;
  ExpectFalse(kTriggerNewGroupUs, 28, 2);
  timestamp += 10 * kMinStep;
  int64_t g2_timestamp = timestamp;
  for (int i = 0; i < 10; ++i) {
    // These packets arrive with timestamps in decreasing order but are
    // nevertheless accumulated to group because their timestamps are higher
    // than the initial timestamp of the group.
    arrival_time += kBurstThresholdMs + 1;
    ExpectFalse(timestamp, arrival_time, 1);
    timestamp -= kMinStep;
  }
  int64_t g2_arrival_time = arrival_time;

  // However, this packet is deemed out of order and should be dropped.
  arrival_time = 281;
  timestamp = g1_timestamp;
  ExpectFalse(timestamp, arrival_time, 100);

  // G3
  timestamp = 2 * kTriggerNewGroupUs;
  arrival_time = 500;
  ExpectTrue(timestamp, arrival_time, 100, g2_timestamp - g1_timestamp,
             g2_arrival_time - g1_arrival_time, (2 + 10) - 1, 0);
}

TEST_F(InterArrivalTest, TwoBursts) {
  // G1
  int64_t g1_arrival_time = 17;
  ExpectFalse(0, g1_arrival_time, 1);

  // G2
  int64_t timestamp = kTriggerNewGroupUs;
  int64_t arrival_time = 100;  // Simulate no packets arriving for 100 ms.
  for (int i = 0; i < 10; ++i) {
    // A bunch of packets arriving in one burst (within 5 ms apart).
    timestamp += 30000;
    arrival_time += kBurstThresholdMs;
    ExpectFalse(timestamp, arrival_time, 1);
  }
  int64_t g2_arrival_time = arrival_time;
  int64_t g2_timestamp = timestamp;

  // G3
  timestamp += 30000;
  arrival_time += kBurstThresholdMs + 1;
  ExpectTrue(timestamp, arrival_time, 100, g2_timestamp,
             g2_arrival_time - g1_arrival_time,
             10 - 1,  // Delta G2-G1
             0);
}

TEST_F(InterArrivalTest, NoBursts) {
  // G1
  ExpectFalse(0, 17, 1);

  // G2
  int64_t timestamp = kTriggerNewGroupUs;
  int64_t arrival_time = 28;
  ExpectFalse(timestamp, arrival_time, 2);

  // G3
  ExpectTrue(kTriggerNewGroupUs + 30000, arrival_time + kBurstThresholdMs + 1,
             100, timestamp - 0, arrival_time - 17,
             2 - 1,  // Delta G2-G1
             0);
}

// Yields 0xfffffffe when converted to internal representation in
// inter_arrival_rtp_ and inter_arrival_ast_ respectively.
static const int64_t kStartRtpTimestampWrapUs = 47721858827;
static const int64_t kStartAbsSendTimeWrapUs = 63999995;

TEST_F(InterArrivalTest, RtpTimestampWrap) {
  WrapTestHelper(kStartRtpTimestampWrapUs, 1, false);
}

TEST_F(InterArrivalTest, AbsSendTimeWrap) {
  WrapTestHelper(kStartAbsSendTimeWrapUs, 1, false);
}

TEST_F(InterArrivalTest, RtpTimestampWrapOutOfOrderWithinGroup) {
  WrapTestHelper(kStartRtpTimestampWrapUs, 1, true);
}

TEST_F(InterArrivalTest, AbsSendTimeWrapOutOfOrderWithinGroup) {
  WrapTestHelper(kStartAbsSendTimeWrapUs, 1, true);
}

TEST_F(InterArrivalTest, PositiveArrivalTimeJump) {
  const size_t kPacketSize = 1000;
  uint32_t send_time_ms = 10000;
  int64_t arrival_time_ms = 20000;
  int64_t system_time_ms = 30000;

  uint32_t send_delta;
  int64_t arrival_delta;
  int size_delta;
  EXPECT_FALSE(inter_arrival_->ComputeDeltas(
      send_time_ms, arrival_time_ms, system_time_ms, kPacketSize, &send_delta,
      &arrival_delta, &size_delta));

  const int kTimeDeltaMs = 30;
  send_time_ms += kTimeDeltaMs;
  arrival_time_ms += kTimeDeltaMs;
  system_time_ms += kTimeDeltaMs;
  EXPECT_FALSE(inter_arrival_->ComputeDeltas(
      send_time_ms, arrival_time_ms, system_time_ms, kPacketSize, &send_delta,
      &arrival_delta, &size_delta));

  send_time_ms += kTimeDeltaMs;
  arrival_time_ms += kTimeDeltaMs + InterArrival::kArrivalTimeOffsetThresholdMs;
  system_time_ms += kTimeDeltaMs;
  EXPECT_TRUE(inter_arrival_->ComputeDeltas(
      send_time_ms, arrival_time_ms, system_time_ms, kPacketSize, &send_delta,
      &arrival_delta, &size_delta));
  EXPECT_EQ(kTimeDeltaMs, static_cast<int>(send_delta));
  EXPECT_EQ(kTimeDeltaMs, arrival_delta);
  EXPECT_EQ(size_delta, 0);

  send_time_ms += kTimeDeltaMs;
  arrival_time_ms += kTimeDeltaMs;
  system_time_ms += kTimeDeltaMs;
  // The previous arrival time jump should now be detected and cause a reset.
  EXPECT_FALSE(inter_arrival_->ComputeDeltas(
      send_time_ms, arrival_time_ms, system_time_ms, kPacketSize, &send_delta,
      &arrival_delta, &size_delta));

  // The two next packets will not give a valid delta since we're in the initial
  // state.
  for (int i = 0; i < 2; ++i) {
    send_time_ms += kTimeDeltaMs;
    arrival_time_ms += kTimeDeltaMs;
    system_time_ms += kTimeDeltaMs;
    EXPECT_FALSE(inter_arrival_->ComputeDeltas(
        send_time_ms, arrival_time_ms, system_time_ms, kPacketSize, &send_delta,
        &arrival_delta, &size_delta));
  }

  send_time_ms += kTimeDeltaMs;
  arrival_time_ms += kTimeDeltaMs;
  system_time_ms += kTimeDeltaMs;
  EXPECT_TRUE(inter_arrival_->ComputeDeltas(
      send_time_ms, arrival_time_ms, system_time_ms, kPacketSize, &send_delta,
      &arrival_delta, &size_delta));
  EXPECT_EQ(kTimeDeltaMs, static_cast<int>(send_delta));
  EXPECT_EQ(kTimeDeltaMs, arrival_delta);
  EXPECT_EQ(size_delta, 0);
}

TEST_F(InterArrivalTest, NegativeArrivalTimeJump) {
  const size_t kPacketSize = 1000;
  uint32_t send_time_ms = 10000;
  int64_t arrival_time_ms = 20000;
  int64_t system_time_ms = 30000;

  uint32_t send_delta;
  int64_t arrival_delta;
  int size_delta;
  EXPECT_FALSE(inter_arrival_->ComputeDeltas(
      send_time_ms, arrival_time_ms, system_time_ms, kPacketSize, &send_delta,
      &arrival_delta, &size_delta));

  const int kTimeDeltaMs = 30;
  send_time_ms += kTimeDeltaMs;
  arrival_time_ms += kTimeDeltaMs;
  system_time_ms += kTimeDeltaMs;
  EXPECT_FALSE(inter_arrival_->ComputeDeltas(
      send_time_ms, arrival_time_ms, system_time_ms, kPacketSize, &send_delta,
      &arrival_delta, &size_delta));

  send_time_ms += kTimeDeltaMs;
  arrival_time_ms += kTimeDeltaMs;
  system_time_ms += kTimeDeltaMs;
  EXPECT_TRUE(inter_arrival_->ComputeDeltas(
      send_time_ms, arrival_time_ms, system_time_ms, kPacketSize, &send_delta,
      &arrival_delta, &size_delta));
  EXPECT_EQ(kTimeDeltaMs, static_cast<int>(send_delta));
  EXPECT_EQ(kTimeDeltaMs, arrival_delta);
  EXPECT_EQ(size_delta, 0);

  // Three out of order will fail, after that we will be reset and two more will
  // fail before we get our first valid delta after the reset.
  arrival_time_ms -= 1000;
  for (int i = 0; i < InterArrival::kReorderedResetThreshold + 3; ++i) {
    send_time_ms += kTimeDeltaMs;
    arrival_time_ms += kTimeDeltaMs;
    system_time_ms += kTimeDeltaMs;
    // The previous arrival time jump should now be detected and cause a reset.
    EXPECT_FALSE(inter_arrival_->ComputeDeltas(
        send_time_ms, arrival_time_ms, system_time_ms, kPacketSize, &send_delta,
        &arrival_delta, &size_delta));
  }

  send_time_ms += kTimeDeltaMs;
  arrival_time_ms += kTimeDeltaMs;
  system_time_ms += kTimeDeltaMs;
  EXPECT_TRUE(inter_arrival_->ComputeDeltas(
      send_time_ms, arrival_time_ms, system_time_ms, kPacketSize, &send_delta,
      &arrival_delta, &size_delta));
  EXPECT_EQ(kTimeDeltaMs, static_cast<int>(send_delta));
  EXPECT_EQ(kTimeDeltaMs, arrival_delta);
  EXPECT_EQ(size_delta, 0);
}
}  // namespace testing
}  // namespace webrtc
