/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/rtp_rtcp/source/ntp_time_util.h"

#include <cstdint>
#include <limits>

#include "api/units/time_delta.h"
#include "system_wrappers/include/ntp_time.h"
#include "test/gtest.h"

namespace webrtc {

TEST(NtpTimeUtilTest, CompactNtp) {
  const uint32_t kNtpSec = 0x12345678;
  const uint32_t kNtpFrac = 0x23456789;
  const NtpTime kNtp(kNtpSec, kNtpFrac);
  const uint32_t kNtpMid = 0x56782345;
  EXPECT_EQ(kNtpMid, CompactNtp(kNtp));
}

TEST(NtpTimeUtilTest, CompactNtpIntervalToTimeDelta) {
  const NtpTime ntp1(0x12345, 0x23456);
  const NtpTime ntp2(0x12654, 0x64335);
  int64_t ms_diff = ntp2.ToMs() - ntp1.ToMs();
  uint32_t ntp_diff = CompactNtp(ntp2) - CompactNtp(ntp1);

  EXPECT_NEAR(CompactNtpIntervalToTimeDelta(ntp_diff).ms(), ms_diff, 1);
}

TEST(NtpTimeUtilTest, CompactNtpIntervalToTimeDeltaWithWrap) {
  const NtpTime ntp1(0x1ffff, 0x23456);
  const NtpTime ntp2(0x20000, 0x64335);
  int64_t ms_diff = ntp2.ToMs() - ntp1.ToMs();

  // While ntp2 > ntp1, there compact ntp presentation happen to be opposite.
  // That shouldn't be a problem as long as unsigned arithmetic is used.
  ASSERT_GT(ntp2.ToMs(), ntp1.ToMs());
  ASSERT_LT(CompactNtp(ntp2), CompactNtp(ntp1));

  uint32_t ntp_diff = CompactNtp(ntp2) - CompactNtp(ntp1);
  EXPECT_NEAR(CompactNtpIntervalToTimeDelta(ntp_diff).ms(), ms_diff, 1);
}

TEST(NtpTimeUtilTest, CompactNtpIntervalToTimeDeltaLarge) {
  const NtpTime ntp1(0x10000, 0x00006);
  const NtpTime ntp2(0x17fff, 0xffff5);
  int64_t ms_diff = ntp2.ToMs() - ntp1.ToMs();
  // Ntp difference close to 2^15 seconds should convert correctly too.
  ASSERT_NEAR(ms_diff, ((1 << 15) - 1) * 1000, 1);
  uint32_t ntp_diff = CompactNtp(ntp2) - CompactNtp(ntp1);
  EXPECT_NEAR(CompactNtpRttToTimeDelta(ntp_diff).ms(), ms_diff, 1);
}

TEST(NtpTimeUtilTest, CompactNtpIntervalToTimeDeltaNegative) {
  const NtpTime ntp1(0x20000, 0x23456);
  const NtpTime ntp2(0x1ffff, 0x64335);
  int64_t ms_diff = ntp2.ToMs() - ntp1.ToMs();
  ASSERT_LT(ms_diff, 0);
  // Ntp difference close to 2^16 seconds should be treated as negative.
  uint32_t ntp_diff = CompactNtp(ntp2) - CompactNtp(ntp1);
  EXPECT_NEAR(CompactNtpIntervalToTimeDelta(ntp_diff).ms(), ms_diff, 1);
}

TEST(NtpTimeUtilTest, CompactNtpIntervalToTimeDeltaBorderToNegative) {
  // Both +0x8000 and -x0x8000 seconds can be valid result when converting value
  // exactly in the middle.
  EXPECT_EQ(CompactNtpIntervalToTimeDelta(0x8000'0000).Abs(),
            TimeDelta::Seconds(0x8000));
}

TEST(NtpTimeUtilTest, CompactNtpRttToTimeDeltaNegative) {
  const NtpTime ntp1(0x20000, 0x23456);
  const NtpTime ntp2(0x1ffff, 0x64335);
  int64_t ms_diff = ntp2.ToMs() - ntp1.ToMs();
  ASSERT_LT(ms_diff, 0);
  // Ntp difference close to 2^16 seconds should be treated as negative.
  uint32_t ntp_diff = CompactNtp(ntp2) - CompactNtp(ntp1);
  EXPECT_EQ(CompactNtpRttToTimeDelta(ntp_diff), TimeDelta::Millis(1));
}

TEST(NtpTimeUtilTest, SaturatedToCompactNtp) {
  // Converts negative to zero.
  EXPECT_EQ(SaturatedToCompactNtp(TimeDelta::Micros(-1)), 0u);
  EXPECT_EQ(SaturatedToCompactNtp(TimeDelta::Zero()), 0u);
  // Converts values just above and just below max uint32_t.
  EXPECT_EQ(SaturatedToCompactNtp(TimeDelta::Micros(65536000000)), 0xffffffff);
  EXPECT_EQ(SaturatedToCompactNtp(TimeDelta::Micros(65535999985)), 0xffffffff);
  EXPECT_EQ(SaturatedToCompactNtp(TimeDelta::Micros(65535999970)), 0xfffffffe);
  // Converts half-seconds.
  EXPECT_EQ(SaturatedToCompactNtp(TimeDelta::Millis(500)), 0x8000u);
  EXPECT_EQ(SaturatedToCompactNtp(TimeDelta::Seconds(1)), 0x10000u);
  EXPECT_EQ(SaturatedToCompactNtp(TimeDelta::Millis(1'500)), 0x18000u);
  // Convert us -> compact_ntp -> TimeDelta. Compact ntp precision is ~15us.
  EXPECT_NEAR(
      CompactNtpRttToTimeDelta(SaturatedToCompactNtp(TimeDelta::Micros(1'516)))
          .us(),
      1'516, 16);
  EXPECT_NEAR(
      CompactNtpRttToTimeDelta(SaturatedToCompactNtp(TimeDelta::Millis(15)))
          .us(),
      15'000, 16);
  EXPECT_NEAR(
      CompactNtpRttToTimeDelta(SaturatedToCompactNtp(TimeDelta::Micros(5'485)))
          .us(),
      5'485, 16);
  EXPECT_NEAR(
      CompactNtpRttToTimeDelta(SaturatedToCompactNtp(TimeDelta::Micros(5'515)))
          .us(),
      5'515, 16);
}

TEST(NtpTimeUtilTest, ToNtpUnits) {
  EXPECT_EQ(ToNtpUnits(TimeDelta::Zero()), 0);
  EXPECT_EQ(ToNtpUnits(TimeDelta::Seconds(1)), int64_t{1} << 32);
  EXPECT_EQ(ToNtpUnits(TimeDelta::Seconds(-1)), -(int64_t{1} << 32));

  EXPECT_EQ(ToNtpUnits(TimeDelta::Millis(500)), int64_t{1} << 31);
  EXPECT_EQ(ToNtpUnits(TimeDelta::Millis(-1'500)), -(int64_t{3} << 31));

  // Smallest TimeDelta that can be converted without precision loss.
  EXPECT_EQ(ToNtpUnits(TimeDelta::Micros(15'625)), int64_t{1} << 26);

  // 1 us ~= 4'294.97 NTP units. ToNtpUnits makes no rounding promises.
  EXPECT_GE(ToNtpUnits(TimeDelta::Micros(1)), 4'294);
  EXPECT_LE(ToNtpUnits(TimeDelta::Micros(1)), 4'295);

  // Test near maximum and minimum supported values.
  static constexpr int64_t k35MinutesInNtpUnits = int64_t{35 * 60} << 32;
  EXPECT_EQ(ToNtpUnits(TimeDelta::Seconds(35 * 60)), k35MinutesInNtpUnits);
  EXPECT_EQ(ToNtpUnits(TimeDelta::Seconds(-35 * 60)), -k35MinutesInNtpUnits);

  // The result for too large or too small values is unspecified, but
  // shouldn't cause integer overflow or other undefined behavior.
  ToNtpUnits(TimeDelta::Micros(std::numeric_limits<int64_t>::max() - 1));
  ToNtpUnits(TimeDelta::Micros(std::numeric_limits<int64_t>::min() + 1));
}

}  // namespace webrtc
