/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/relative_arrival_delay_tracker.h"

#include "test/gtest.h"

namespace webrtc {

namespace {
constexpr int kMaxHistoryMs = 2000;
constexpr int kFs = 8000;
constexpr int kFrameSizeMs = 20;
constexpr int kTsIncrement = kFrameSizeMs * kFs / 1000;
constexpr uint32_t kTs = 0x12345678;
}  // namespace

TEST(RelativeArrivalDelayTrackerTest, RelativeArrivalDelay) {
  TickTimer tick_timer;
  RelativeArrivalDelayTracker tracker(&tick_timer, kMaxHistoryMs);

  EXPECT_FALSE(tracker.Update(kTs, kFs));

  tick_timer.Increment(kFrameSizeMs / tick_timer.ms_per_tick());
  EXPECT_EQ(tracker.Update(kTs + kTsIncrement, kFs), 0);

  tick_timer.Increment(2 * kFrameSizeMs / tick_timer.ms_per_tick());
  EXPECT_EQ(tracker.Update(kTs + 2 * kTsIncrement, kFs), 20);

  EXPECT_EQ(tracker.Update(kTs, kFs), 60);  // Reordered, 60ms delayed.

  tick_timer.Increment(2 * kFrameSizeMs / tick_timer.ms_per_tick());
  EXPECT_EQ(tracker.Update(kTs + 3 * kTsIncrement, kFs), 40);
}

TEST(RelativeArrivalDelayTrackerTest, ReorderedPackets) {
  TickTimer tick_timer;
  RelativeArrivalDelayTracker tracker(&tick_timer, kMaxHistoryMs);

  // Insert first packet.
  EXPECT_FALSE(tracker.Update(kTs, kFs));

  // Insert reordered packet.
  EXPECT_EQ(tracker.Update(kTs - 4 * kTsIncrement, kFs), 80);
  EXPECT_EQ(tracker.newest_timestamp(), kTs);

  // Insert another reordered packet.
  EXPECT_EQ(tracker.Update(kTs - kTsIncrement, kFs), 20);
  EXPECT_EQ(tracker.newest_timestamp(), kTs);

  // Insert the next packet in order and verify that the relative delay is
  // estimated based on the first inserted packet.
  tick_timer.Increment(4 * kFrameSizeMs / tick_timer.ms_per_tick());
  EXPECT_EQ(tracker.Update(kTs + kTsIncrement, kFs), 60);
  EXPECT_EQ(tracker.newest_timestamp(), kTs + kTsIncrement);
}

TEST(RelativeArrivalDelayTrackerTest, MaxDelayHistory) {
  TickTimer tick_timer;
  RelativeArrivalDelayTracker tracker(&tick_timer, kMaxHistoryMs);

  EXPECT_FALSE(tracker.Update(kTs, kFs));

  // Insert 20 ms iat delay in the delay history.
  tick_timer.Increment(2 * kFrameSizeMs / tick_timer.ms_per_tick());
  EXPECT_EQ(tracker.Update(kTs + kTsIncrement, kFs), 20);

  // Insert next packet with a timestamp difference larger than maximum history
  // size. This removes the previously inserted iat delay from the history.
  tick_timer.Increment((kMaxHistoryMs + kFrameSizeMs) /
                       tick_timer.ms_per_tick());
  EXPECT_EQ(
      tracker.Update(kTs + 2 * kTsIncrement + kFs * kMaxHistoryMs / 1000, kFs),
      0);
}

}  // namespace webrtc
