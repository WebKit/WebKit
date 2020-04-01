/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/bbr/rtt_stats.h"

#include <stdlib.h>

#include <cmath>
#include <vector>

#include "test/gtest.h"

namespace webrtc {
namespace bbr {
namespace test {

class RttStatsTest : public ::testing::Test {
 protected:
  RttStats rtt_stats_;
};

TEST_F(RttStatsTest, DefaultsBeforeUpdate) {
  EXPECT_LT(0u, rtt_stats_.initial_rtt_us());
  EXPECT_EQ(TimeDelta::Zero(), rtt_stats_.min_rtt());
  EXPECT_EQ(TimeDelta::Zero(), rtt_stats_.smoothed_rtt());
}

TEST_F(RttStatsTest, SmoothedRtt) {
  // Verify that ack_delay is corrected for in Smoothed RTT.
  rtt_stats_.UpdateRtt(TimeDelta::Millis(300), TimeDelta::Millis(100),
                       Timestamp::Millis(0));
  EXPECT_EQ(TimeDelta::Millis(200), rtt_stats_.latest_rtt());
  EXPECT_EQ(TimeDelta::Millis(200), rtt_stats_.smoothed_rtt());
  // Verify that effective RTT of zero does not change Smoothed RTT.
  rtt_stats_.UpdateRtt(TimeDelta::Millis(200), TimeDelta::Millis(200),
                       Timestamp::Millis(0));
  EXPECT_EQ(TimeDelta::Millis(200), rtt_stats_.latest_rtt());
  EXPECT_EQ(TimeDelta::Millis(200), rtt_stats_.smoothed_rtt());
  // Verify that large erroneous ack_delay does not change Smoothed RTT.
  rtt_stats_.UpdateRtt(TimeDelta::Millis(200), TimeDelta::Millis(300),
                       Timestamp::Millis(0));
  EXPECT_EQ(TimeDelta::Millis(200), rtt_stats_.latest_rtt());
  EXPECT_EQ(TimeDelta::Millis(200), rtt_stats_.smoothed_rtt());
}

// Ensure that the potential rounding artifacts in EWMA calculation do not cause
// the SRTT to drift too far from the exact value.
TEST_F(RttStatsTest, SmoothedRttStability) {
  for (int64_t time = 3; time < 20000; time++) {
    RttStats stats;
    for (int64_t i = 0; i < 100; i++) {
      stats.UpdateRtt(TimeDelta::Micros(time), TimeDelta::Millis(0),
                      Timestamp::Millis(0));
      int64_t time_delta_us = stats.smoothed_rtt().us() - time;
      ASSERT_LE(std::abs(time_delta_us), 1);
    }
  }
}

TEST_F(RttStatsTest, PreviousSmoothedRtt) {
  // Verify that ack_delay is corrected for in Smoothed RTT.
  rtt_stats_.UpdateRtt(TimeDelta::Millis(300), TimeDelta::Millis(100),
                       Timestamp::Millis(0));
  EXPECT_EQ(TimeDelta::Millis(200), rtt_stats_.latest_rtt());
  EXPECT_EQ(TimeDelta::Millis(200), rtt_stats_.smoothed_rtt());
  EXPECT_EQ(TimeDelta::Zero(), rtt_stats_.previous_srtt());
  // Ensure the previous SRTT is 200ms after a 100ms sample.
  rtt_stats_.UpdateRtt(TimeDelta::Millis(100), TimeDelta::Zero(),
                       Timestamp::Millis(0));
  EXPECT_EQ(TimeDelta::Millis(100), rtt_stats_.latest_rtt());
  EXPECT_EQ(TimeDelta::Micros(187500).us(), rtt_stats_.smoothed_rtt().us());
  EXPECT_EQ(TimeDelta::Millis(200), rtt_stats_.previous_srtt());
}

TEST_F(RttStatsTest, MinRtt) {
  rtt_stats_.UpdateRtt(TimeDelta::Millis(200), TimeDelta::Zero(),
                       Timestamp::Millis(0));
  EXPECT_EQ(TimeDelta::Millis(200), rtt_stats_.min_rtt());
  rtt_stats_.UpdateRtt(TimeDelta::Millis(10), TimeDelta::Zero(),
                       Timestamp::Millis(0) + TimeDelta::Millis(10));
  EXPECT_EQ(TimeDelta::Millis(10), rtt_stats_.min_rtt());
  rtt_stats_.UpdateRtt(TimeDelta::Millis(50), TimeDelta::Zero(),
                       Timestamp::Millis(0) + TimeDelta::Millis(20));
  EXPECT_EQ(TimeDelta::Millis(10), rtt_stats_.min_rtt());
  rtt_stats_.UpdateRtt(TimeDelta::Millis(50), TimeDelta::Zero(),
                       Timestamp::Millis(0) + TimeDelta::Millis(30));
  EXPECT_EQ(TimeDelta::Millis(10), rtt_stats_.min_rtt());
  rtt_stats_.UpdateRtt(TimeDelta::Millis(50), TimeDelta::Zero(),
                       Timestamp::Millis(0) + TimeDelta::Millis(40));
  EXPECT_EQ(TimeDelta::Millis(10), rtt_stats_.min_rtt());
  // Verify that ack_delay does not go into recording of min_rtt_.
  rtt_stats_.UpdateRtt(TimeDelta::Millis(7), TimeDelta::Millis(2),
                       Timestamp::Millis(0) + TimeDelta::Millis(50));
  EXPECT_EQ(TimeDelta::Millis(7), rtt_stats_.min_rtt());
}

TEST_F(RttStatsTest, ExpireSmoothedMetrics) {
  TimeDelta initial_rtt = TimeDelta::Millis(10);
  rtt_stats_.UpdateRtt(initial_rtt, TimeDelta::Zero(), Timestamp::Millis(0));
  EXPECT_EQ(initial_rtt, rtt_stats_.min_rtt());
  EXPECT_EQ(initial_rtt, rtt_stats_.smoothed_rtt());

  EXPECT_EQ(0.5 * initial_rtt, rtt_stats_.mean_deviation());

  // Update once with a 20ms RTT.
  TimeDelta doubled_rtt = 2 * initial_rtt;
  rtt_stats_.UpdateRtt(doubled_rtt, TimeDelta::Zero(), Timestamp::Millis(0));
  EXPECT_EQ(1.125 * initial_rtt, rtt_stats_.smoothed_rtt());

  // Expire the smoothed metrics, increasing smoothed rtt and mean deviation.
  rtt_stats_.ExpireSmoothedMetrics();
  EXPECT_EQ(doubled_rtt, rtt_stats_.smoothed_rtt());
  EXPECT_EQ(0.875 * initial_rtt, rtt_stats_.mean_deviation());

  // Now go back down to 5ms and expire the smoothed metrics, and ensure the
  // mean deviation increases to 15ms.
  TimeDelta half_rtt = 0.5 * initial_rtt;
  rtt_stats_.UpdateRtt(half_rtt, TimeDelta::Zero(), Timestamp::Millis(0));
  EXPECT_GT(doubled_rtt, rtt_stats_.smoothed_rtt());
  EXPECT_LT(initial_rtt, rtt_stats_.mean_deviation());
}

TEST_F(RttStatsTest, UpdateRttWithBadSendDeltas) {
  // Make sure we ignore bad RTTs.

  TimeDelta initial_rtt = TimeDelta::Millis(10);
  rtt_stats_.UpdateRtt(initial_rtt, TimeDelta::Zero(), Timestamp::Millis(0));
  EXPECT_EQ(initial_rtt, rtt_stats_.min_rtt());
  EXPECT_EQ(initial_rtt, rtt_stats_.smoothed_rtt());

  std::vector<TimeDelta> bad_send_deltas;
  bad_send_deltas.push_back(TimeDelta::Zero());
  bad_send_deltas.push_back(TimeDelta::PlusInfinity());
  bad_send_deltas.push_back(TimeDelta::Micros(-1000));

  for (TimeDelta bad_send_delta : bad_send_deltas) {
    rtt_stats_.UpdateRtt(bad_send_delta, TimeDelta::Zero(),
                         Timestamp::Millis(0));
    EXPECT_EQ(initial_rtt, rtt_stats_.min_rtt());
    EXPECT_EQ(initial_rtt, rtt_stats_.smoothed_rtt());
  }
}

TEST_F(RttStatsTest, ResetAfterConnectionMigrations) {
  rtt_stats_.UpdateRtt(TimeDelta::Millis(300), TimeDelta::Millis(100),
                       Timestamp::Millis(0));
  EXPECT_EQ(TimeDelta::Millis(200), rtt_stats_.latest_rtt());
  EXPECT_EQ(TimeDelta::Millis(200), rtt_stats_.smoothed_rtt());
  EXPECT_EQ(TimeDelta::Millis(300), rtt_stats_.min_rtt());

  // Reset rtt stats on connection migrations.
  rtt_stats_.OnConnectionMigration();
  EXPECT_EQ(TimeDelta::Zero(), rtt_stats_.latest_rtt());
  EXPECT_EQ(TimeDelta::Zero(), rtt_stats_.smoothed_rtt());
  EXPECT_EQ(TimeDelta::Zero(), rtt_stats_.min_rtt());
}

}  // namespace test
}  // namespace bbr
}  // namespace webrtc
