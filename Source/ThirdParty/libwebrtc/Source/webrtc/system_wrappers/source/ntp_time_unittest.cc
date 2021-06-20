/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "system_wrappers/include/ntp_time.h"

#include <random>

#include "system_wrappers/include/clock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

constexpr uint32_t kNtpSec = 0x12345678;
constexpr uint32_t kNtpFrac = 0x23456789;

constexpr int64_t kOneSecQ32x32 = uint64_t{1} << 32;
constexpr int64_t kOneMsQ32x32 = 4294967;

TEST(NtpTimeTest, NoValueMeansInvalid) {
  NtpTime ntp;
  EXPECT_FALSE(ntp.Valid());
}

TEST(NtpTimeTest, CanResetValue) {
  NtpTime ntp(kNtpSec, kNtpFrac);
  EXPECT_TRUE(ntp.Valid());
  ntp.Reset();
  EXPECT_FALSE(ntp.Valid());
}

TEST(NtpTimeTest, CanGetWhatIsSet) {
  NtpTime ntp;
  ntp.Set(kNtpSec, kNtpFrac);
  EXPECT_EQ(kNtpSec, ntp.seconds());
  EXPECT_EQ(kNtpFrac, ntp.fractions());
}

TEST(NtpTimeTest, SetIsSameAs2ParameterConstructor) {
  NtpTime ntp1(kNtpSec, kNtpFrac);
  NtpTime ntp2;
  EXPECT_NE(ntp1, ntp2);

  ntp2.Set(kNtpSec, kNtpFrac);
  EXPECT_EQ(ntp1, ntp2);
}

TEST(NtpTimeTest, ToMsMeansToNtpMilliseconds) {
  SimulatedClock clock(0x123456789abc);

  NtpTime ntp = clock.CurrentNtpTime();
  EXPECT_EQ(ntp.ToMs(), clock.CurrentNtpInMilliseconds());
}

TEST(NtpTimeTest, CanExplicitlyConvertToAndFromUint64) {
  uint64_t untyped_time = 0x123456789;
  NtpTime time(untyped_time);
  EXPECT_EQ(untyped_time, static_cast<uint64_t>(time));
  EXPECT_EQ(NtpTime(0x12345678, 0x90abcdef), NtpTime(0x1234567890abcdef));
}

TEST(NtpTimeTest, VerifyInt64MsToQ32x32NearZero) {
  // Zero
  EXPECT_EQ(Int64MsToQ32x32(0), 0);

  // Zero + 1 millisecond
  EXPECT_EQ(Int64MsToQ32x32(1), kOneMsQ32x32);

  // Zero - 1 millisecond
  EXPECT_EQ(Int64MsToQ32x32(-1), -kOneMsQ32x32);

  // Zero + 1 second
  EXPECT_EQ(Int64MsToQ32x32(1000), kOneSecQ32x32);

  // Zero - 1 second
  EXPECT_EQ(Int64MsToQ32x32(-1000), -kOneSecQ32x32);
}

TEST(NtpTimeTest, VerifyInt64MsToUQ32x32NearZero) {
  // Zero
  EXPECT_EQ(Int64MsToUQ32x32(0), uint64_t{0});

  // Zero + 1 millisecond
  EXPECT_EQ(Int64MsToUQ32x32(1), uint64_t{kOneMsQ32x32});

  // Zero - 1 millisecond
  EXPECT_EQ(Int64MsToUQ32x32(-1), uint64_t{0});  // Clamped

  // Zero + 1 second
  EXPECT_EQ(Int64MsToUQ32x32(1000), uint64_t{kOneSecQ32x32});

  // Zero - 1 second
  EXPECT_EQ(Int64MsToUQ32x32(-1000), uint64_t{0});  // Clamped
}

TEST(NtpTimeTest, VerifyQ32x32ToInt64MsNearZero) {
  // Zero
  EXPECT_EQ(Q32x32ToInt64Ms(0), 0);

  // Zero + 1 millisecond
  EXPECT_EQ(Q32x32ToInt64Ms(kOneMsQ32x32), 1);

  // Zero - 1 millisecond
  EXPECT_EQ(Q32x32ToInt64Ms(-kOneMsQ32x32), -1);

  // Zero + 1 second
  EXPECT_EQ(Q32x32ToInt64Ms(kOneSecQ32x32), 1000);

  // Zero - 1 second
  EXPECT_EQ(Q32x32ToInt64Ms(-kOneSecQ32x32), -1000);
}

TEST(NtpTimeTest, VerifyUQ32x32ToInt64MsNearZero) {
  // Zero
  EXPECT_EQ(UQ32x32ToInt64Ms(0), 0);

  // Zero + 1 millisecond
  EXPECT_EQ(UQ32x32ToInt64Ms(kOneMsQ32x32), 1);

  // Zero + 1 second
  EXPECT_EQ(UQ32x32ToInt64Ms(kOneSecQ32x32), 1000);
}

TEST(NtpTimeTest, VerifyInt64MsToQ32x32NearMax) {
  constexpr int64_t kMaxQ32x32 = std::numeric_limits<int64_t>::max();
  constexpr int64_t kBoundaryMs = (kMaxQ32x32 >> 32) * 1000 + 999;

  // Max
  const int64_t boundary_q32x32 = Int64MsToQ32x32(kBoundaryMs);
  EXPECT_LE(boundary_q32x32, kMaxQ32x32);
  EXPECT_GT(boundary_q32x32, kMaxQ32x32 - kOneMsQ32x32);

  // Max + 1 millisecond
  EXPECT_EQ(Int64MsToQ32x32(kBoundaryMs + 1), kMaxQ32x32);  // Clamped

  // Max - 1 millisecond
  EXPECT_LE(Int64MsToQ32x32(kBoundaryMs - 1), kMaxQ32x32 - kOneMsQ32x32);

  // Max + 1 second
  EXPECT_EQ(Int64MsToQ32x32(kBoundaryMs + 1000), kMaxQ32x32);  // Clamped

  // Max - 1 second
  EXPECT_LE(Int64MsToQ32x32(kBoundaryMs - 1000), kMaxQ32x32 - kOneSecQ32x32);
}

TEST(NtpTimeTest, VerifyInt64MsToUQ32x32NearMax) {
  constexpr uint64_t kMaxUQ32x32 = std::numeric_limits<uint64_t>::max();
  constexpr int64_t kBoundaryMs = (kMaxUQ32x32 >> 32) * 1000 + 999;

  // Max
  const uint64_t boundary_uq32x32 = Int64MsToUQ32x32(kBoundaryMs);
  EXPECT_LE(boundary_uq32x32, kMaxUQ32x32);
  EXPECT_GT(boundary_uq32x32, kMaxUQ32x32 - kOneMsQ32x32);

  // Max + 1 millisecond
  EXPECT_EQ(Int64MsToUQ32x32(kBoundaryMs + 1), kMaxUQ32x32);  // Clamped

  // Max - 1 millisecond
  EXPECT_LE(Int64MsToUQ32x32(kBoundaryMs - 1), kMaxUQ32x32 - kOneMsQ32x32);

  // Max + 1 second
  EXPECT_EQ(Int64MsToUQ32x32(kBoundaryMs + 1000), kMaxUQ32x32);  // Clamped

  // Max - 1 second
  EXPECT_LE(Int64MsToUQ32x32(kBoundaryMs - 1000), kMaxUQ32x32 - kOneSecQ32x32);
}

TEST(NtpTimeTest, VerifyQ32x32ToInt64MsNearMax) {
  constexpr int64_t kMaxQ32x32 = std::numeric_limits<int64_t>::max();
  constexpr int64_t kBoundaryMs = (kMaxQ32x32 >> 32) * 1000 + 1000;

  // Max
  EXPECT_EQ(Q32x32ToInt64Ms(kMaxQ32x32), kBoundaryMs);

  // Max - 1 millisecond
  EXPECT_EQ(Q32x32ToInt64Ms(kMaxQ32x32 - kOneMsQ32x32), kBoundaryMs - 1);

  // Max - 1 second
  EXPECT_EQ(Q32x32ToInt64Ms(kMaxQ32x32 - kOneSecQ32x32), kBoundaryMs - 1000);
}

TEST(NtpTimeTest, VerifyUQ32x32ToInt64MsNearMax) {
  constexpr uint64_t kMaxUQ32x32 = std::numeric_limits<uint64_t>::max();
  constexpr int64_t kBoundaryMs = (kMaxUQ32x32 >> 32) * 1000 + 1000;

  // Max
  EXPECT_EQ(UQ32x32ToInt64Ms(kMaxUQ32x32), kBoundaryMs);

  // Max - 1 millisecond
  EXPECT_EQ(UQ32x32ToInt64Ms(kMaxUQ32x32 - kOneMsQ32x32), kBoundaryMs - 1);

  // Max - 1 second
  EXPECT_EQ(UQ32x32ToInt64Ms(kMaxUQ32x32 - kOneSecQ32x32), kBoundaryMs - 1000);
}

TEST(NtpTimeTest, VerifyInt64MsToQ32x32NearMin) {
  constexpr int64_t kBoundaryQ32x32 = 0x8000000000000000;
  constexpr int64_t kBoundaryMs = -int64_t{0x80000000} * 1000;

  // Min
  EXPECT_EQ(Int64MsToQ32x32(kBoundaryMs), kBoundaryQ32x32);

  // Min + 1 millisecond
  EXPECT_EQ(Q32x32ToInt64Ms(Int64MsToQ32x32(kBoundaryMs + 1)), kBoundaryMs + 1);

  // Min - 1 millisecond
  EXPECT_EQ(Int64MsToQ32x32(kBoundaryMs - 1), kBoundaryQ32x32);  // Clamped

  // Min + 1 second
  EXPECT_EQ(Int64MsToQ32x32(kBoundaryMs + 1000),
            kBoundaryQ32x32 + kOneSecQ32x32);

  // Min - 1 second
  EXPECT_EQ(Int64MsToQ32x32(kBoundaryMs - 1000), kBoundaryQ32x32);  // Clamped
}

TEST(NtpTimeTest, VerifyQ32x32ToInt64MsNearMin) {
  constexpr int64_t kBoundaryQ32x32 = 0x8000000000000000;
  constexpr int64_t kBoundaryMs = -int64_t{0x80000000} * 1000;

  // Min
  EXPECT_EQ(Q32x32ToInt64Ms(kBoundaryQ32x32), kBoundaryMs);

  // Min + 1 millisecond
  EXPECT_EQ(Q32x32ToInt64Ms(kBoundaryQ32x32 + kOneMsQ32x32), kBoundaryMs + 1);

  // Min + 1 second
  EXPECT_EQ(Q32x32ToInt64Ms(kBoundaryQ32x32 + kOneSecQ32x32),
            kBoundaryMs + 1000);
}

TEST(NtpTimeTest, VerifyInt64MsToQ32x32RoundTrip) {
  constexpr int kIterations = 50000;

  std::mt19937 generator(123456789);
  std::uniform_int_distribution<int64_t> distribution(
      Q32x32ToInt64Ms(std::numeric_limits<int64_t>::min()),
      Q32x32ToInt64Ms(std::numeric_limits<int64_t>::max()));

  for (int iteration = 0; iteration < kIterations; ++iteration) {
    int64_t input_ms = distribution(generator);
    int64_t transit_q32x32 = Int64MsToQ32x32(input_ms);
    int64_t output_ms = Q32x32ToInt64Ms(transit_q32x32);

    ASSERT_EQ(input_ms, output_ms)
        << "iteration = " << iteration << ", input_ms = " << input_ms
        << ", transit_q32x32 = " << transit_q32x32
        << ", output_ms = " << output_ms;
  }
}

TEST(NtpTimeTest, VerifyInt64MsToUQ32x32RoundTrip) {
  constexpr int kIterations = 50000;

  std::mt19937 generator(123456789);
  std::uniform_int_distribution<uint64_t> distribution(
      UQ32x32ToInt64Ms(std::numeric_limits<uint64_t>::min()),
      UQ32x32ToInt64Ms(std::numeric_limits<uint64_t>::max()));

  for (int iteration = 0; iteration < kIterations; ++iteration) {
    uint64_t input_ms = distribution(generator);
    uint64_t transit_uq32x32 = Int64MsToUQ32x32(input_ms);
    uint64_t output_ms = UQ32x32ToInt64Ms(transit_uq32x32);

    ASSERT_EQ(input_ms, output_ms)
        << "iteration = " << iteration << ", input_ms = " << input_ms
        << ", transit_uq32x32 = " << transit_uq32x32
        << ", output_ms = " << output_ms;
  }
}

}  // namespace
}  // namespace webrtc
