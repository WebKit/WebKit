/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/time_interval.h"
#include "rtc_base/fakeclock.h"
#include "rtc_base/timedelta.h"
#include "test/gtest.h"

namespace webrtc {

TEST(TimeIntervalTest, TimeInMs) {
  rtc::ScopedFakeClock fake_clock;
  TimeInterval interval;
  interval.Extend();
  fake_clock.AdvanceTime(rtc::TimeDelta::FromMilliseconds(100));
  interval.Extend();
  EXPECT_EQ(interval.Length(), 100);
}

TEST(TimeIntervalTest, Empty) {
  TimeInterval interval;
  EXPECT_TRUE(interval.Empty());
  interval.Extend();
  EXPECT_FALSE(interval.Empty());
  interval.Extend(200);
  EXPECT_FALSE(interval.Empty());
}

TEST(TimeIntervalTest, MonotoneIncreasing) {
  const size_t point_count = 7;
  const int64_t interval_points[] = {3, 2, 5, 0, 4, 1, 6};
  const int64_t interval_differences[] = {0, 1, 3, 5, 5, 5, 6};
  TimeInterval interval;
  EXPECT_TRUE(interval.Empty());
  for (size_t i = 0; i < point_count; ++i) {
    interval.Extend(interval_points[i]);
    EXPECT_EQ(interval_differences[i], interval.Length());
  }
}

}  // namespace webrtc
