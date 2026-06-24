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

#include <cstdint>
#include <optional>

#include "api/rtp_headers.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/metrics.h"
#include "system_wrappers/include/ntp_time.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

using testing::AllOf;
using testing::Ge;
using testing::Le;

TEST(AbsoluteCaptureTimeInterpolatorTest, GetSourceWithoutCsrcs) {
  constexpr uint32_t kSsrc = 12;

  EXPECT_EQ(AbsoluteCaptureTimeInterpolator::GetSource(kSsrc, {}), kSsrc);
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
  const AbsoluteCaptureTime kExtension0 = {
      .absolute_capture_timestamp = Int64MsToUQ32x32(9000),
      .estimated_capture_clock_offset = Int64MsToQ32x32(-350)};
  const AbsoluteCaptureTime kExtension1 = {
      .absolute_capture_timestamp = Int64MsToUQ32x32(9020),
      .estimated_capture_clock_offset = std::nullopt};

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
                                   /*received_extension=*/std::nullopt),
      std::nullopt);

  EXPECT_EQ(
      interpolator.OnReceivePacket(kSource, kRtpTimestamp1, kRtpClockFrequency,
                                   /*received_extension=*/std::nullopt),
      std::nullopt);
}

TEST(AbsoluteCaptureTimeInterpolatorTest, InterpolateLaterPacketArrivingLater) {
  constexpr uint32_t kSource = 1337;
  constexpr int kRtpClockFrequency = 64'000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 1280;
  constexpr uint32_t kRtpTimestamp2 = kRtpTimestamp0 + 2560;
  const AbsoluteCaptureTime kExtension = {
      .absolute_capture_timestamp = Int64MsToUQ32x32(9000),
      .estimated_capture_clock_offset = Int64MsToQ32x32(-350)};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeInterpolator interpolator(&clock);

  EXPECT_EQ(interpolator.OnReceivePacket(kSource, kRtpTimestamp0,
                                         kRtpClockFrequency, kExtension),
            kExtension);

  std::optional<AbsoluteCaptureTime> extension =
      interpolator.OnReceivePacket(kSource, kRtpTimestamp1, kRtpClockFrequency,
                                   /*received_extension=*/std::nullopt);
  ASSERT_TRUE(extension.has_value());
  EXPECT_EQ(UQ32x32ToInt64Ms(extension->absolute_capture_timestamp),
            UQ32x32ToInt64Ms(kExtension.absolute_capture_timestamp) + 20);
  EXPECT_EQ(extension->estimated_capture_clock_offset,
            kExtension.estimated_capture_clock_offset);

  extension =
      interpolator.OnReceivePacket(kSource, kRtpTimestamp2, kRtpClockFrequency,
                                   /*received_extension=*/std::nullopt);
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
  const AbsoluteCaptureTime kExtension = {
      .absolute_capture_timestamp = Int64MsToUQ32x32(9000),
      .estimated_capture_clock_offset = Int64MsToQ32x32(-350)};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeInterpolator interpolator(&clock);

  EXPECT_EQ(interpolator.OnReceivePacket(kSource, kRtpTimestamp0,
                                         kRtpClockFrequency, kExtension),
            kExtension);

  std::optional<AbsoluteCaptureTime> extension =
      interpolator.OnReceivePacket(kSource, kRtpTimestamp1, kRtpClockFrequency,
                                   /*received_extension=*/std::nullopt);
  ASSERT_TRUE(extension.has_value());
  EXPECT_EQ(UQ32x32ToInt64Ms(extension->absolute_capture_timestamp),
            UQ32x32ToInt64Ms(kExtension.absolute_capture_timestamp) - 20);
  EXPECT_EQ(extension->estimated_capture_clock_offset,
            kExtension.estimated_capture_clock_offset);

  extension =
      interpolator.OnReceivePacket(kSource, kRtpTimestamp2, kRtpClockFrequency,
                                   /*received_extension=*/std::nullopt);
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
  const AbsoluteCaptureTime kExtension = {
      .absolute_capture_timestamp = Int64MsToUQ32x32(9000),
      .estimated_capture_clock_offset = Int64MsToQ32x32(-350)};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeInterpolator interpolator(&clock);

  EXPECT_EQ(interpolator.OnReceivePacket(kSource, kRtpTimestamp0,
                                         kRtpClockFrequency, kExtension),
            kExtension);

  std::optional<AbsoluteCaptureTime> extension =
      interpolator.OnReceivePacket(kSource, kRtpTimestamp1, kRtpClockFrequency,
                                   /*received_extension=*/std::nullopt);
  ASSERT_TRUE(extension.has_value());
  EXPECT_EQ(UQ32x32ToInt64Ms(extension->absolute_capture_timestamp),
            UQ32x32ToInt64Ms(kExtension.absolute_capture_timestamp) + 20);
  EXPECT_EQ(extension->estimated_capture_clock_offset,
            kExtension.estimated_capture_clock_offset);

  extension =
      interpolator.OnReceivePacket(kSource, kRtpTimestamp2, kRtpClockFrequency,
                                   /*received_extension=*/std::nullopt);
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
  const AbsoluteCaptureTime kExtension = {
      .absolute_capture_timestamp = Int64MsToUQ32x32(9000),
      .estimated_capture_clock_offset = Int64MsToQ32x32(-350)};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeInterpolator interpolator(&clock);

  EXPECT_EQ(interpolator.OnReceivePacket(kSource, kRtpTimestamp0,
                                         kRtpClockFrequency, kExtension),
            kExtension);

  std::optional<AbsoluteCaptureTime> extension =
      interpolator.OnReceivePacket(kSource, kRtpTimestamp1, kRtpClockFrequency,
                                   /*received_extension=*/std::nullopt);
  ASSERT_TRUE(extension.has_value());
  EXPECT_EQ(UQ32x32ToInt64Ms(extension->absolute_capture_timestamp),
            UQ32x32ToInt64Ms(kExtension.absolute_capture_timestamp) - 20);
  EXPECT_EQ(extension->estimated_capture_clock_offset,
            kExtension.estimated_capture_clock_offset);

  extension =
      interpolator.OnReceivePacket(kSource, kRtpTimestamp2, kRtpClockFrequency,
                                   /*received_extension=*/std::nullopt);
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
  const AbsoluteCaptureTime kExtension = {
      .absolute_capture_timestamp = Int64MsToUQ32x32(9000),
      .estimated_capture_clock_offset = Int64MsToQ32x32(-350)};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeInterpolator interpolator(&clock);

  EXPECT_EQ(interpolator.OnReceivePacket(kSource, kRtpTimestamp0,
                                         kRtpClockFrequency, kExtension),
            kExtension);

  clock.AdvanceTime(AbsoluteCaptureTimeInterpolator::kInterpolationMaxInterval);

  EXPECT_NE(
      interpolator.OnReceivePacket(kSource, kRtpTimestamp1, kRtpClockFrequency,
                                   /*received_extension=*/std::nullopt),
      std::nullopt);

  clock.AdvanceTime(TimeDelta::Millis(1));

  EXPECT_EQ(
      interpolator.OnReceivePacket(kSource, kRtpTimestamp2, kRtpClockFrequency,
                                   /*received_extension=*/std::nullopt),
      std::nullopt);
}

TEST(AbsoluteCaptureTimeInterpolatorTest, SkipInterpolateIfSourceChanged) {
  constexpr uint32_t kSource0 = 1337;
  constexpr uint32_t kSource1 = 1338;
  constexpr int kRtpClockFrequency = 64'000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 1280;
  const AbsoluteCaptureTime kExtension = {
      .absolute_capture_timestamp = Int64MsToUQ32x32(9000),
      .estimated_capture_clock_offset = Int64MsToQ32x32(-350)};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeInterpolator interpolator(&clock);

  EXPECT_EQ(interpolator.OnReceivePacket(kSource0, kRtpTimestamp0,
                                         kRtpClockFrequency, kExtension),
            kExtension);

  EXPECT_EQ(
      interpolator.OnReceivePacket(kSource1, kRtpTimestamp1, kRtpClockFrequency,
                                   /*received_extension=*/std::nullopt),
      std::nullopt);
}

TEST(AbsoluteCaptureTimeInterpolatorTest,
     SkipInterpolateIfRtpClockFrequencyChanged) {
  constexpr uint32_t kSource = 1337;
  constexpr int kRtpClockFrequency0 = 64'000;
  constexpr int kRtpClockFrequency1 = 32'000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 640;
  const AbsoluteCaptureTime kExtension = {
      .absolute_capture_timestamp = Int64MsToUQ32x32(9000),
      .estimated_capture_clock_offset = Int64MsToQ32x32(-350)};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeInterpolator interpolator(&clock);

  EXPECT_EQ(interpolator.OnReceivePacket(kSource, kRtpTimestamp0,
                                         kRtpClockFrequency0, kExtension),
            kExtension);

  EXPECT_EQ(
      interpolator.OnReceivePacket(kSource, kRtpTimestamp1, kRtpClockFrequency1,
                                   /*received_extension=*/std::nullopt),
      std::nullopt);
}

TEST(AbsoluteCaptureTimeInterpolatorTest,
     SkipInterpolateIfRtpClockFrequencyIsInvalid) {
  constexpr uint32_t kSource = 1337;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 640;
  const AbsoluteCaptureTime kExtension = {
      .absolute_capture_timestamp = Int64MsToUQ32x32(9000),
      .estimated_capture_clock_offset = Int64MsToQ32x32(-350)};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeInterpolator interpolator(&clock);

  EXPECT_EQ(
      interpolator.OnReceivePacket(kSource, kRtpTimestamp0,
                                   /*rtp_clock_frequency_hz=*/0, kExtension),
      kExtension);

  EXPECT_EQ(interpolator.OnReceivePacket(kSource, kRtpTimestamp1,
                                         /*rtp_clock_frequency_hz=*/0,
                                         /*received_extension=*/std::nullopt),
            std::nullopt);
}

TEST(AbsoluteCaptureTimeInterpolatorTest, SkipInterpolateIsSticky) {
  constexpr uint32_t kSource0 = 1337;
  constexpr uint32_t kSource1 = 1338;
  constexpr int kRtpClockFrequency = 64'000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 1280;
  constexpr uint32_t kRtpTimestamp2 = kRtpTimestamp1 + 1280;
  const AbsoluteCaptureTime kExtension = {
      .absolute_capture_timestamp = Int64MsToUQ32x32(9000),
      .estimated_capture_clock_offset = Int64MsToQ32x32(-350)};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeInterpolator interpolator(&clock);

  EXPECT_EQ(interpolator.OnReceivePacket(kSource0, kRtpTimestamp0,
                                         kRtpClockFrequency, kExtension),
            kExtension);

  EXPECT_EQ(
      interpolator.OnReceivePacket(kSource1, kRtpTimestamp1, kRtpClockFrequency,
                                   /*received_extension=*/std::nullopt),
      std::nullopt);

  EXPECT_EQ(
      interpolator.OnReceivePacket(kSource0, kRtpTimestamp2, kRtpClockFrequency,
                                   /*received_extension=*/std::nullopt),
      std::nullopt);
}

TEST(AbsoluteCaptureTimeInterpolatorTest, MetricsAreUpdated) {
  constexpr uint32_t kRtpTimestamp0 = 102030000;
  constexpr uint32_t kSource = 1234;
  constexpr uint32_t kFrequency = 1000;
  SimulatedClock clock(0);
  AbsoluteCaptureTimeInterpolator interpolator(&clock);

  metrics::Reset();
  // First packet has no extension.
  interpolator.OnReceivePacket(kSource, kRtpTimestamp0, kFrequency,
                               std::nullopt);
  EXPECT_METRIC_EQ(metrics::NumSamples("WebRTC.Call.AbsCapture.ExtensionWait"),
                   0);

  // Second packet has extension, but no offset.
  clock.AdvanceTimeMilliseconds(10);
  interpolator.OnReceivePacket(
      kSource, kRtpTimestamp0 + 10, kFrequency,
      AbsoluteCaptureTime{.absolute_capture_timestamp = Int64MsToUQ32x32(5000),
                          .estimated_capture_clock_offset = std::nullopt});
  EXPECT_METRIC_EQ(metrics::NumSamples("WebRTC.Call.AbsCapture.ExtensionWait"),
                   1);

  // Third packet has extension with offset, value zero.
  clock.AdvanceTimeMilliseconds(10);
  interpolator.OnReceivePacket(
      kSource, kRtpTimestamp0 + 20, kFrequency,
      AbsoluteCaptureTime{
          .absolute_capture_timestamp = Int64MsToUQ32x32(20),
          .estimated_capture_clock_offset = Int64MsToUQ32x32(0)});
  EXPECT_METRIC_EQ(metrics::NumSamples("WebRTC.Call.AbsCapture.Delta"), 2);
  EXPECT_METRIC_EQ(metrics::NumSamples("WebRTC.Call.AbsCapture.DeltaDeviation"),
                   1);
}

TEST(AbsoluteCaptureTimeInterpolatorTest, DeltaRecordedCorrectly) {
  constexpr uint32_t kRtpTimestamp0 = 102030000;
  constexpr uint32_t kSource = 1234;
  constexpr uint32_t kFrequency = 1000;
  SimulatedClock clock(0);
  AbsoluteCaptureTimeInterpolator interpolator(&clock);

  metrics::Reset();
  clock.AdvanceTimeMilliseconds(10);
  // Packet has extension, with delta 5 ms in the past.
  interpolator.OnReceivePacket(
      kSource, kRtpTimestamp0 + 10, kFrequency,
      AbsoluteCaptureTime{
          .absolute_capture_timestamp =
              uint64_t{clock.ConvertTimestampToNtpTime(Timestamp::Millis(5))},
          .estimated_capture_clock_offset = std::nullopt});

  EXPECT_METRIC_EQ(metrics::NumSamples("WebRTC.Call.AbsCapture.ExtensionWait"),
                   1);
  int sample = metrics::MinSample("WebRTC.Call.AbsCapture.Delta");
  EXPECT_THAT(sample, AllOf(Ge(5000), Le(5000)));

  metrics::Reset();
  // Packet has extension, with timestamp 6 ms in the future.
  interpolator.OnReceivePacket(
      kSource, kRtpTimestamp0 + 15, kFrequency,
      AbsoluteCaptureTime{
          .absolute_capture_timestamp =
              uint64_t{clock.ConvertTimestampToNtpTime(Timestamp::Millis(16))},
          .estimated_capture_clock_offset = std::nullopt});

  sample = metrics::MinSample("WebRTC.Call.AbsCapture.Delta");
  // Since we capture with abs(), this should also be recorded as 6 ms
  EXPECT_THAT(sample, AllOf(Ge(6000), Le(6000)));
}

}  // namespace webrtc
