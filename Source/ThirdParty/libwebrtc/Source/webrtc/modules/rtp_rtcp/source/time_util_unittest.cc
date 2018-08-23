/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/rtp_rtcp/source/time_util.h"

#include "rtc_base/fakeclock.h"
#include "rtc_base/timeutils.h"
#include "system_wrappers/include/clock.h"
#include "test/gtest.h"

namespace webrtc {

TEST(TimeUtilTest, TimeMicrosToNtpDoesntChangeBetweenRuns) {
  rtc::ScopedFakeClock clock;
  // TimeMicrosToNtp is not pure: it behave differently between different
  // execution of the program, but should behave same during same execution.
  const int64_t time_us = 12345;
  clock.SetTimeMicros(2);
  NtpTime time_ntp = TimeMicrosToNtp(time_us);
  clock.SetTimeMicros(time_us);
  EXPECT_EQ(TimeMicrosToNtp(time_us), time_ntp);
  clock.SetTimeMicros(1000000);
  EXPECT_EQ(TimeMicrosToNtp(time_us), time_ntp);
}

TEST(TimeUtilTest, TimeMicrosToNtpKeepsIntervals) {
  rtc::ScopedFakeClock clock;
  NtpTime time_ntp1 = TimeMicrosToNtp(rtc::TimeMicros());
  clock.AdvanceTimeMicros(20000);
  NtpTime time_ntp2 = TimeMicrosToNtp(rtc::TimeMicros());
  EXPECT_EQ(time_ntp2.ToMs() - time_ntp1.ToMs(), 20);
}

TEST(TimeUtilTest, CompactNtp) {
  const uint32_t kNtpSec = 0x12345678;
  const uint32_t kNtpFrac = 0x23456789;
  const NtpTime kNtp(kNtpSec, kNtpFrac);
  const uint32_t kNtpMid = 0x56782345;
  EXPECT_EQ(kNtpMid, CompactNtp(kNtp));
}

TEST(TimeUtilTest, CompactNtpRttToMs) {
  const NtpTime ntp1(0x12345, 0x23456);
  const NtpTime ntp2(0x12654, 0x64335);
  int64_t ms_diff = ntp2.ToMs() - ntp1.ToMs();
  uint32_t ntp_diff = CompactNtp(ntp2) - CompactNtp(ntp1);

  int64_t ntp_to_ms_diff = CompactNtpRttToMs(ntp_diff);

  EXPECT_NEAR(ms_diff, ntp_to_ms_diff, 1);
}

TEST(TimeUtilTest, CompactNtpRttToMsWithWrap) {
  const NtpTime ntp1(0x1ffff, 0x23456);
  const NtpTime ntp2(0x20000, 0x64335);
  int64_t ms_diff = ntp2.ToMs() - ntp1.ToMs();

  // While ntp2 > ntp1, there compact ntp presentation happen to be opposite.
  // That shouldn't be a problem as long as unsigned arithmetic is used.
  ASSERT_GT(ntp2.ToMs(), ntp1.ToMs());
  ASSERT_LT(CompactNtp(ntp2), CompactNtp(ntp1));

  uint32_t ntp_diff = CompactNtp(ntp2) - CompactNtp(ntp1);
  int64_t ntp_to_ms_diff = CompactNtpRttToMs(ntp_diff);

  EXPECT_NEAR(ms_diff, ntp_to_ms_diff, 1);
}

TEST(TimeUtilTest, CompactNtpRttToMsLarge) {
  const NtpTime ntp1(0x10000, 0x00006);
  const NtpTime ntp2(0x17fff, 0xffff5);
  int64_t ms_diff = ntp2.ToMs() - ntp1.ToMs();
  // Ntp difference close to 2^15 seconds should convert correctly too.
  ASSERT_NEAR(ms_diff, ((1 << 15) - 1) * 1000, 1);
  uint32_t ntp_diff = CompactNtp(ntp2) - CompactNtp(ntp1);
  int64_t ntp_to_ms_diff = CompactNtpRttToMs(ntp_diff);

  EXPECT_NEAR(ms_diff, ntp_to_ms_diff, 1);
}

TEST(TimeUtilTest, CompactNtpRttToMsNegative) {
  const NtpTime ntp1(0x20000, 0x23456);
  const NtpTime ntp2(0x1ffff, 0x64335);
  int64_t ms_diff = ntp2.ToMs() - ntp1.ToMs();
  ASSERT_GT(0, ms_diff);
  // Ntp difference close to 2^16 seconds should be treated as negative.
  uint32_t ntp_diff = CompactNtp(ntp2) - CompactNtp(ntp1);
  int64_t ntp_to_ms_diff = CompactNtpRttToMs(ntp_diff);
  EXPECT_EQ(1, ntp_to_ms_diff);
}

TEST(TimeUtilTest, SaturatedUsToCompactNtp) {
  // Converts negative to zero.
  EXPECT_EQ(SaturatedUsToCompactNtp(-1), 0u);
  EXPECT_EQ(SaturatedUsToCompactNtp(0), 0u);
  // Converts values just above and just below max uint32_t.
  EXPECT_EQ(SaturatedUsToCompactNtp(65536000000), 0xffffffff);
  EXPECT_EQ(SaturatedUsToCompactNtp(65535999985), 0xffffffff);
  EXPECT_EQ(SaturatedUsToCompactNtp(65535999970), 0xfffffffe);
  // Converts half-seconds.
  EXPECT_EQ(SaturatedUsToCompactNtp(500000), 0x8000u);
  EXPECT_EQ(SaturatedUsToCompactNtp(1000000), 0x10000u);
  EXPECT_EQ(SaturatedUsToCompactNtp(1500000), 0x18000u);
  // Convert us -> compact_ntp -> ms. Compact ntp precision is ~15us.
  EXPECT_EQ(CompactNtpRttToMs(SaturatedUsToCompactNtp(1516)), 2);
  EXPECT_EQ(CompactNtpRttToMs(SaturatedUsToCompactNtp(15000)), 15);
  EXPECT_EQ(CompactNtpRttToMs(SaturatedUsToCompactNtp(5485)), 5);
  EXPECT_EQ(CompactNtpRttToMs(SaturatedUsToCompactNtp(5515)), 6);
}

}  // namespace webrtc
