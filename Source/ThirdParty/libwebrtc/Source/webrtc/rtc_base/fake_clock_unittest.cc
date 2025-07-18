/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/fake_clock.h"

#include "test/gtest.h"

namespace webrtc {
TEST(ScopedFakeClockTest, OverridesGlobalClock) {
  const int64_t kFixedTimeUs = 100000;
  int64_t real_time_us = TimeMicros();
  EXPECT_NE(real_time_us, 0);
  {
    ScopedFakeClock scoped;
    EXPECT_EQ(TimeMicros(), 0);

    scoped.AdvanceTime(TimeDelta::Millis(1));
    EXPECT_EQ(TimeMicros(), 1000);

    scoped.SetTime(Timestamp::Micros(kFixedTimeUs));
    EXPECT_EQ(TimeMicros(), kFixedTimeUs);

    scoped.AdvanceTime(TimeDelta::Millis(1));
    EXPECT_EQ(TimeMicros(), kFixedTimeUs + 1000);
  }

  EXPECT_NE(TimeMicros(), kFixedTimeUs + 1000);
  EXPECT_GE(TimeMicros(), real_time_us);
}
}  // namespace webrtc
