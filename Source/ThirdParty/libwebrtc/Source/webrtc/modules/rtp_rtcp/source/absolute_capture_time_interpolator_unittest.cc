/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/absolute_capture_time_interpolator.h"

#include "system_wrappers/include/ntp_time.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

TEST(AbsoluteCaptureTimeInterpolatorTest, GetSourceWithoutCsrcs) {
  constexpr uint32_t kSsrc = 12;

  EXPECT_EQ(AbsoluteCaptureTimeInterpolator::GetSource(kSsrc, nullptr), kSsrc);
}

TEST(AbsoluteCaptureTimeInterpolatorTest, GetSourceWithCsrcs) {
  constexpr uint32_t kSsrc = 12;
  constexpr uint32_t kCsrcs[] = {34, 56, 78, 90};

  EXPECT_EQ(AbsoluteCaptureTimeInterpolator::GetSource(kSsrc, kCsrcs),
            kCsrcs[0]);
}

TEST(AbsoluteCaptureTimeInterpolatorTest, ReceiveExtensionReturnsExtension) {
  constexpr uint32_t kSource = 1337;
  constexpr int kRtpClockFrequency = 64'000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 1280;
  const AbsoluteCaptureTime kExtension0 = {Int64MsToUQ32x32(9000),
                                           Int64MsToQ32x32(-350)};
  const AbsoluteCaptureTime kExtension1 = {Int64MsToUQ32x32(9020),
                                           absl::nullopt};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeInterpolator interpolator(&clock);

  EXPECT_EQ(interpolator.OnReceivePacket(kSource, kRtpTimestamp0,
                                         kRtpClockFrequency, kExtension0),
            kExtension0);

  EXPECT_EQ(interpolator.OnReceivePacket(kSource, kRtpTimestamp1,
                                         kRtpClockFrequency, kExtension1),
            kExtension1);
}

TEST(AbsoluteCaptureTimeInterpolatorTest,
     ReceiveNoExtensionReturnsNoExtension) {
  constexpr uint32_t kSource = 1337;
  constexpr int kRtpClockFrequency = 64'000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 1280;

  SimulatedClock clock(0);
  AbsoluteCaptureTimeInterpolator interpolator(&clock);

  EXPECT_EQ(
      interpolator.OnReceivePacket(kSource, kRtpTimestamp0, kRtpClockFrequency,
                                   /*received_extension=*/absl::nullopt),
      absl::nullopt);

  EXPECT_EQ(
      interpolator.OnReceivePacket(kSource, kRtpTimestamp1, kRtpClockFrequency,
                                   /*received_extension=*/absl::nullopt),
      absl::nullopt);
}

TEST(AbsoluteCaptureTimeInterpolatorTest, InterpolateLaterPacketArrivingLater) {
  constexpr uint32_t kSource = 1337;
  constexpr int kRtpClockFrequency = 64'000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 1280;
  constexpr uint32_t kRtpTimestamp2 = kRtpTimestamp0 + 2560;
  const AbsoluteCaptureTime kExtension = {Int64MsToUQ32x32(9000),
                                          Int64MsToQ32x32(-350)};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeInterpolator interpolator(&clock);

  EXPECT_EQ(interpolator.OnReceivePacket(kSource, kRtpTimestamp0,
                                         kRtpClockFrequency, kExtension),
            kExtension);

  absl::optional<AbsoluteCaptureTime> extension =
      interpolator.OnReceivePacket(kSource, kRtpTimestamp1, kRtpClockFrequency,
                                   /*received_extension=*/absl::nullopt);
  ASSERT_TRUE(extension.has_value());
  EXPECT_EQ(UQ32x32ToInt64Ms(extension->absolute_capture_timestamp),
            UQ32x32ToInt64Ms(kExtension.absolute_capture_timestamp) + 20);
  EXPECT_EQ(extension->estimated_capture_clock_offset,
            kExtension.estimated_capture_clock_offset);

  extension =
      interpolator.OnReceivePacket(kSource, kRtpTimestamp2, kRtpClockFrequency,
                                   /*received_extension=*/absl::nullopt);
  ASSERT_TRUE(extension.has_value());
  EXPECT_EQ(UQ32x32ToInt64Ms(extension->absolute_capture_timestamp),
            UQ32x32ToInt64Ms(kExtension.absolute_capture_timestamp) + 40);
  EXPECT_EQ(extension->estimated_capture_clock_offset,
            kExtension.estimated_capture_clock_offset);
}

TEST(AbsoluteCaptureTimeInterpolatorTest,
     InterpolateEarlierPacketArrivingLater) {
  constexpr uint32_t kSource = 1337;
  constexpr int kRtpClockFrequency = 64'000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 - 1280;
  constexpr uint32_t kRtpTimestamp2 = kRtpTimestamp0 - 2560;
  const AbsoluteCaptureTime kExtension = {Int64MsToUQ32x32(9000),
                                          Int64MsToQ32x32(-350)};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeInterpolator interpolator(&clock);

  EXPECT_EQ(interpolator.OnReceivePacket(kSource, kRtpTimestamp0,
                                         kRtpClockFrequency, kExtension),
            kExtension);

  absl::optional<AbsoluteCaptureTime> extension =
      interpolator.OnReceivePacket(kSource, kRtpTimestamp1, kRtpClockFrequency,
                                   /*received_extension=*/absl::nullopt);
  ASSERT_TRUE(extension.has_value());
  EXPECT_EQ(UQ32x32ToInt64Ms(extension->absolute_capture_timestamp),
            UQ32x32ToInt64Ms(kExtension.absolute_capture_timestamp) - 20);
  EXPECT_EQ(extension->estimated_capture_clock_offset,
            kExtension.estimated_capture_clock_offset);

  extension =
      interpolator.OnReceivePacket(kSource, kRtpTimestamp2, kRtpClockFrequency,
                                   /*received_extension=*/absl::nullopt);
  ASSERT_TRUE(extension.has_value());
  EXPECT_EQ(UQ32x32ToInt64Ms(extension->absolute_capture_timestamp),
            UQ32x32ToInt64Ms(kExtension.absolute_capture_timestamp) - 40);
  EXPECT_EQ(extension->estimated_capture_clock_offset,
            kExtension.estimated_capture_clock_offset);
}

TEST(AbsoluteCaptureTimeInterpolatorTest,
     InterpolateLaterPacketArrivingLaterWithRtpTimestampWrapAround) {
  constexpr uint32_t kSource = 1337;
  constexpr int kRtpClockFrequency = 64'000;
  constexpr uint32_t kRtpTimestamp0 = uint32_t{0} - 80;
  constexpr uint32_t kRtpTimestamp1 = 1280 - 80;
  constexpr uint32_t kRtpTimestamp2 = 2560 - 80;
  const AbsoluteCaptureTime kExtension = {Int64MsToUQ32x32(9000),
                                          Int64MsToQ32x32(-350)};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeInterpolator interpolator(&clock);

  EXPECT_EQ(interpolator.OnReceivePacket(kSource, kRtpTimestamp0,
                                         kRtpClockFrequency, kExtension),
            kExtension);

  absl::optional<AbsoluteCaptureTime> extension =
      interpolator.OnReceivePacket(kSource, kRtpTimestamp1, kRtpClockFrequency,
                                   /*received_extension=*/absl::nullopt);
  ASSERT_TRUE(extension.has_value());
  EXPECT_EQ(UQ32x32ToInt64Ms(extension->absolute_capture_timestamp),
            UQ32x32ToInt64Ms(kExtension.absolute_capture_timestamp) + 20);
  EXPECT_EQ(extension->estimated_capture_clock_offset,
            kExtension.estimated_capture_clock_offset);

  extension =
      interpolator.OnReceivePacket(kSource, kRtpTimestamp2, kRtpClockFrequency,
                                   /*received_extension=*/absl::nullopt);
  ASSERT_TRUE(extension.has_value());
  EXPECT_EQ(UQ32x32ToInt64Ms(extension->absolute_capture_timestamp),
            UQ32x32ToInt64Ms(kExtension.absolute_capture_timestamp) + 40);
  EXPECT_EQ(extension->estimated_capture_clock_offset,
            kExtension.estimated_capture_clock_offset);
}

TEST(AbsoluteCaptureTimeInterpolatorTest,
     InterpolateEarlierPacketArrivingLaterWithRtpTimestampWrapAround) {
  constexpr uint32_t kSource = 1337;
  constexpr int kRtpClockFrequency = 64'000;
  constexpr uint32_t kRtpTimestamp0 = 799;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 - 1280;
  constexpr uint32_t kRtpTimestamp2 = kRtpTimestamp0 - 2560;
  const AbsoluteCaptureTime kExtension = {Int64MsToUQ32x32(9000),
                                          Int64MsToQ32x32(-350)};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeInterpolator interpolator(&clock);

  EXPECT_EQ(interpolator.OnReceivePacket(kSource, kRtpTimestamp0,
                                         kRtpClockFrequency, kExtension),
            kExtension);

  absl::optional<AbsoluteCaptureTime> extension =
      interpolator.OnReceivePacket(kSource, kRtpTimestamp1, kRtpClockFrequency,
                                   /*received_extension=*/absl::nullopt);
  ASSERT_TRUE(extension.has_value());
  EXPECT_EQ(UQ32x32ToInt64Ms(extension->absolute_capture_timestamp),
            UQ32x32ToInt64Ms(kExtension.absolute_capture_timestamp) - 20);
  EXPECT_EQ(extension->estimated_capture_clock_offset,
            kExtension.estimated_capture_clock_offset);

  extension =
      interpolator.OnReceivePacket(kSource, kRtpTimestamp2, kRtpClockFrequency,
                                   /*received_extension=*/absl::nullopt);
  ASSERT_TRUE(extension.has_value());
  EXPECT_EQ(UQ32x32ToInt64Ms(extension->absolute_capture_timestamp),
            UQ32x32ToInt64Ms(kExtension.absolute_capture_timestamp) - 40);
  EXPECT_EQ(extension->estimated_capture_clock_offset,
            kExtension.estimated_capture_clock_offset);
}

TEST(AbsoluteCaptureTimeInterpolatorTest, SkipInterpolateIfTooLate) {
  constexpr uint32_t kSource = 1337;
  constexpr int kRtpClockFrequency = 64'000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 1280;
  constexpr uint32_t kRtpTimestamp2 = kRtpTimestamp1 + 1280;
  const AbsoluteCaptureTime kExtension = {Int64MsToUQ32x32(9000),
                                          Int64MsToQ32x32(-350)};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeInterpolator interpolator(&clock);

  EXPECT_EQ(interpolator.OnReceivePacket(kSource, kRtpTimestamp0,
                                         kRtpClockFrequency, kExtension),
            kExtension);

  clock.AdvanceTime(AbsoluteCaptureTimeInterpolator::kInterpolationMaxInterval);

  EXPECT_NE(
      interpolator.OnReceivePacket(kSource, kRtpTimestamp1, kRtpClockFrequency,
                                   /*received_extension=*/absl::nullopt),
      absl::nullopt);

  clock.AdvanceTime(TimeDelta::Millis(1));

  EXPECT_EQ(
      interpolator.OnReceivePacket(kSource, kRtpTimestamp2, kRtpClockFrequency,
                                   /*received_extension=*/absl::nullopt),
      absl::nullopt);
}

TEST(AbsoluteCaptureTimeInterpolatorTest, SkipInterpolateIfSourceChanged) {
  constexpr uint32_t kSource0 = 1337;
  constexpr uint32_t kSource1 = 1338;
  constexpr int kRtpClockFrequency = 64'000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 1280;
  const AbsoluteCaptureTime kExtension = {Int64MsToUQ32x32(9000),
                                          Int64MsToQ32x32(-350)};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeInterpolator interpolator(&clock);

  EXPECT_EQ(interpolator.OnReceivePacket(kSource0, kRtpTimestamp0,
                                         kRtpClockFrequency, kExtension),
            kExtension);

  EXPECT_EQ(
      interpolator.OnReceivePacket(kSource1, kRtpTimestamp1, kRtpClockFrequency,
                                   /*received_extension=*/absl::nullopt),
      absl::nullopt);
}

TEST(AbsoluteCaptureTimeInterpolatorTest,
     SkipInterpolateIfRtpClockFrequencyChanged) {
  constexpr uint32_t kSource = 1337;
  constexpr int kRtpClockFrequency0 = 64'000;
  constexpr int kRtpClockFrequency1 = 32'000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 640;
  const AbsoluteCaptureTime kExtension = {Int64MsToUQ32x32(9000),
                                          Int64MsToQ32x32(-350)};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeInterpolator interpolator(&clock);

  EXPECT_EQ(interpolator.OnReceivePacket(kSource, kRtpTimestamp0,
                                         kRtpClockFrequency0, kExtension),
            kExtension);

  EXPECT_EQ(
      interpolator.OnReceivePacket(kSource, kRtpTimestamp1, kRtpClockFrequency1,
                                   /*received_extension=*/absl::nullopt),
      absl::nullopt);
}

TEST(AbsoluteCaptureTimeInterpolatorTest,
     SkipInterpolateIfRtpClockFrequencyIsInvalid) {
  constexpr uint32_t kSource = 1337;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 640;
  const AbsoluteCaptureTime kExtension = {Int64MsToUQ32x32(9000),
                                          Int64MsToQ32x32(-350)};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeInterpolator interpolator(&clock);

  EXPECT_EQ(
      interpolator.OnReceivePacket(kSource, kRtpTimestamp0,
                                   /*rtp_clock_frequency_hz=*/0, kExtension),
      kExtension);

  EXPECT_EQ(interpolator.OnReceivePacket(kSource, kRtpTimestamp1,
                                         /*rtp_clock_frequency_hz=*/0,
                                         /*received_extension=*/absl::nullopt),
            absl::nullopt);
}

TEST(AbsoluteCaptureTimeInterpolatorTest, SkipInterpolateIsSticky) {
  constexpr uint32_t kSource0 = 1337;
  constexpr uint32_t kSource1 = 1338;
  constexpr int kRtpClockFrequency = 64'000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 1280;
  constexpr uint32_t kRtpTimestamp2 = kRtpTimestamp1 + 1280;
  const AbsoluteCaptureTime kExtension = {Int64MsToUQ32x32(9000),
                                          Int64MsToQ32x32(-350)};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeInterpolator interpolator(&clock);

  EXPECT_EQ(interpolator.OnReceivePacket(kSource0, kRtpTimestamp0,
                                         kRtpClockFrequency, kExtension),
            kExtension);

  EXPECT_EQ(
      interpolator.OnReceivePacket(kSource1, kRtpTimestamp1, kRtpClockFrequency,
                                   /*received_extension=*/absl::nullopt),
      absl::nullopt);

  EXPECT_EQ(
      interpolator.OnReceivePacket(kSource0, kRtpTimestamp2, kRtpClockFrequency,
                                   /*received_extension=*/absl::nullopt),
      absl::nullopt);
}

}  // namespace webrtc
