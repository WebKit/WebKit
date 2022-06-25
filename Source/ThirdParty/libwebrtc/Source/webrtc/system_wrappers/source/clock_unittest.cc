/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "system_wrappers/include/clock.h"

#include "test/gtest.h"

namespace webrtc {

TEST(ClockTest, NtpTime) {
  Clock* clock = Clock::GetRealTimeClock();

  // To ensure the test runs correctly even on a heavily loaded system, do not
  // compare the seconds/fractions and millisecond values directly. Instead,
  // we check that the NTP time is between the "milliseconds" values returned
  // right before and right after the call.
  // The comparison includes 1 ms of margin to account for the rounding error in
  // the conversion.
  int64_t milliseconds_lower_bound = clock->CurrentNtpInMilliseconds();
  NtpTime ntp_time = clock->CurrentNtpTime();
  int64_t milliseconds_upper_bound = clock->CurrentNtpInMilliseconds();
  EXPECT_GT(milliseconds_lower_bound / 1000, kNtpJan1970);
  EXPECT_LE(milliseconds_lower_bound - 1, ntp_time.ToMs());
  EXPECT_GE(milliseconds_upper_bound + 1, ntp_time.ToMs());
}

}  // namespace webrtc
