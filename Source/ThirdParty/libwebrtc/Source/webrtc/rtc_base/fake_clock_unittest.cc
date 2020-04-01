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

namespace rtc {
TEST(ScopedFakeClockTest, OverridesGlobalClock) {
  const int64_t kFixedTimeUs = 100000;
  int64_t real_time_us = rtc::TimeMicros();
  EXPECT_NE(real_time_us, 0);
  {
    ScopedFakeClock scoped;
    EXPECT_EQ(rtc::TimeMicros(), 0);

    scoped.AdvanceTime(webrtc::TimeDelta::Millis(1));
    EXPECT_EQ(rtc::TimeMicros(), 1000);

    scoped.SetTime(webrtc::Timestamp::Micros(kFixedTimeUs));
    EXPECT_EQ(rtc::TimeMicros(), kFixedTimeUs);

    scoped.AdvanceTime(webrtc::TimeDelta::Millis(1));
    EXPECT_EQ(rtc::TimeMicros(), kFixedTimeUs + 1000);
  }

  EXPECT_NE(rtc::TimeMicros(), kFixedTimeUs + 1000);
  EXPECT_GE(rtc::TimeMicros(), real_time_us);
}
}  // namespace rtc
