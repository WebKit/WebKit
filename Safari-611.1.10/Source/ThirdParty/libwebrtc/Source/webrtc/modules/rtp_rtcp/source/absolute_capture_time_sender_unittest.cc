/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/absolute_capture_time_sender.h"

#include "system_wrappers/include/ntp_time.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

TEST(AbsoluteCaptureTimeSenderTest, GetSourceWithoutCsrcs) {
  constexpr uint32_t kSsrc = 12;

  EXPECT_EQ(AbsoluteCaptureTimeSender::GetSource(kSsrc, nullptr), kSsrc);
}

TEST(AbsoluteCaptureTimeSenderTest, GetSourceWithCsrcs) {
  constexpr uint32_t kSsrc = 12;
  constexpr uint32_t kCsrcs[] = {34, 56, 78, 90};

  EXPECT_EQ(AbsoluteCaptureTimeSender::GetSource(kSsrc, kCsrcs), kCsrcs[0]);
}

TEST(AbsoluteCaptureTimeSenderTest, InterpolateLaterPacketSentLater) {
  constexpr uint32_t kSource = 1337;
  constexpr uint32_t kRtpClockFrequency = 64000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 1280;
  constexpr uint32_t kRtpTimestamp2 = kRtpTimestamp0 + 2560;
  static const absl::optional<AbsoluteCaptureTime> kExtension0 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension1 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000 + 20), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension2 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000 + 40), Int64MsToQ32x32(-350)};
  SimulatedClock clock(0);
  AbsoluteCaptureTimeSender sender(&clock);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp0, kRtpClockFrequency,
                                kExtension0->absolute_capture_timestamp,
                                kExtension0->estimated_capture_clock_offset),
            kExtension0);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp1, kRtpClockFrequency,
                                kExtension1->absolute_capture_timestamp,
                                kExtension1->estimated_capture_clock_offset),
            absl::nullopt);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp2, kRtpClockFrequency,
                                kExtension2->absolute_capture_timestamp,
                                kExtension2->estimated_capture_clock_offset),
            absl::nullopt);
}

TEST(AbsoluteCaptureTimeSenderTest, InterpolateEarlierPacketSentLater) {
  constexpr uint32_t kSource = 1337;
  constexpr uint32_t kRtpClockFrequency = 64000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 - 1280;
  constexpr uint32_t kRtpTimestamp2 = kRtpTimestamp0 - 2560;
  static const absl::optional<AbsoluteCaptureTime> kExtension0 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension1 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000 - 20), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension2 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000 - 40), Int64MsToQ32x32(-350)};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeSender sender(&clock);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp0, kRtpClockFrequency,
                                kExtension0->absolute_capture_timestamp,
                                kExtension0->estimated_capture_clock_offset),
            kExtension0);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp1, kRtpClockFrequency,
                                kExtension1->absolute_capture_timestamp,
                                kExtension1->estimated_capture_clock_offset),
            absl::nullopt);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp2, kRtpClockFrequency,
                                kExtension2->absolute_capture_timestamp,
                                kExtension2->estimated_capture_clock_offset),
            absl::nullopt);
}

TEST(AbsoluteCaptureTimeSenderTest,
     InterpolateLaterPacketSentLaterWithRtpTimestampWrapAround) {
  constexpr uint32_t kSource = 1337;
  constexpr uint32_t kRtpClockFrequency = 64000;
  constexpr uint32_t kRtpTimestamp0 = ~uint32_t{0} - 79;
  constexpr uint32_t kRtpTimestamp1 = 1280 - 80;
  constexpr uint32_t kRtpTimestamp2 = 2560 - 80;
  static const absl::optional<AbsoluteCaptureTime> kExtension0 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension1 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000 + 20), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension2 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000 + 40), Int64MsToQ32x32(-350)};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeSender sender(&clock);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp0, kRtpClockFrequency,
                                kExtension0->absolute_capture_timestamp,
                                kExtension0->estimated_capture_clock_offset),
            kExtension0);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp1, kRtpClockFrequency,
                                kExtension1->absolute_capture_timestamp,
                                kExtension1->estimated_capture_clock_offset),
            absl::nullopt);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp2, kRtpClockFrequency,
                                kExtension2->absolute_capture_timestamp,
                                kExtension2->estimated_capture_clock_offset),
            absl::nullopt);
}

TEST(AbsoluteCaptureTimeSenderTest,
     InterpolateEarlierPacketSentLaterWithRtpTimestampWrapAround) {
  constexpr uint32_t kSource = 1337;
  constexpr uint32_t kRtpClockFrequency = 64000;
  constexpr uint32_t kRtpTimestamp0 = 799;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 - 1280;
  constexpr uint32_t kRtpTimestamp2 = kRtpTimestamp0 - 2560;
  static const absl::optional<AbsoluteCaptureTime> kExtension0 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension1 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000 - 20), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension2 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000 - 40), Int64MsToQ32x32(-350)};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeSender sender(&clock);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp0, kRtpClockFrequency,
                                kExtension0->absolute_capture_timestamp,
                                kExtension0->estimated_capture_clock_offset),
            kExtension0);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp1, kRtpClockFrequency,
                                kExtension1->absolute_capture_timestamp,
                                kExtension1->estimated_capture_clock_offset),
            absl::nullopt);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp2, kRtpClockFrequency,
                                kExtension2->absolute_capture_timestamp,
                                kExtension2->estimated_capture_clock_offset),
            absl::nullopt);
}

TEST(AbsoluteCaptureTimeSenderTest, SkipInterpolateIfTooLate) {
  constexpr uint32_t kSource = 1337;
  constexpr uint32_t kRtpClockFrequency = 64000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 1280;
  constexpr uint32_t kRtpTimestamp2 = kRtpTimestamp0 + 2560;
  static const absl::optional<AbsoluteCaptureTime> kExtension0 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension1 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000 + 20), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension2 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000 + 40), Int64MsToQ32x32(-350)};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeSender sender(&clock);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp0, kRtpClockFrequency,
                                kExtension0->absolute_capture_timestamp,
                                kExtension0->estimated_capture_clock_offset),
            kExtension0);

  clock.AdvanceTime(AbsoluteCaptureTimeSender::kInterpolationMaxInterval);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp1, kRtpClockFrequency,
                                kExtension1->absolute_capture_timestamp,
                                kExtension1->estimated_capture_clock_offset),
            absl::nullopt);

  clock.AdvanceTimeMicroseconds(1);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp2, kRtpClockFrequency,
                                kExtension2->absolute_capture_timestamp,
                                kExtension2->estimated_capture_clock_offset),
            kExtension2);
}

TEST(AbsoluteCaptureTimeSenderTest, SkipInterpolateIfSourceChanged) {
  constexpr uint32_t kSource0 = 1337;
  constexpr uint32_t kSource1 = 1338;
  constexpr uint32_t kSource2 = 1338;
  constexpr uint32_t kRtpClockFrequency = 64000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 1280;
  constexpr uint32_t kRtpTimestamp2 = kRtpTimestamp0 + 2560;
  static const absl::optional<AbsoluteCaptureTime> kExtension0 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension1 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000 + 20), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension2 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000 + 40), Int64MsToQ32x32(-350)};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeSender sender(&clock);

  EXPECT_EQ(sender.OnSendPacket(kSource0, kRtpTimestamp0, kRtpClockFrequency,
                                kExtension0->absolute_capture_timestamp,
                                kExtension0->estimated_capture_clock_offset),
            kExtension0);

  EXPECT_EQ(sender.OnSendPacket(kSource1, kRtpTimestamp1, kRtpClockFrequency,
                                kExtension1->absolute_capture_timestamp,
                                kExtension1->estimated_capture_clock_offset),
            kExtension1);

  EXPECT_EQ(sender.OnSendPacket(kSource2, kRtpTimestamp2, kRtpClockFrequency,
                                kExtension2->absolute_capture_timestamp,
                                kExtension2->estimated_capture_clock_offset),
            absl::nullopt);
}

TEST(AbsoluteCaptureTimeSenderTest, SkipInterpolateIfRtpClockFrequencyChanged) {
  constexpr uint32_t kSource = 1337;
  constexpr uint32_t kRtpClockFrequency0 = 64000;
  constexpr uint32_t kRtpClockFrequency1 = 32000;
  constexpr uint32_t kRtpClockFrequency2 = 32000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 640;
  constexpr uint32_t kRtpTimestamp2 = kRtpTimestamp0 + 1280;
  static const absl::optional<AbsoluteCaptureTime> kExtension0 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension1 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000 + 20), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension2 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000 + 40), Int64MsToQ32x32(-350)};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeSender sender(&clock);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp0, kRtpClockFrequency0,
                                kExtension0->absolute_capture_timestamp,
                                kExtension0->estimated_capture_clock_offset),
            kExtension0);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp1, kRtpClockFrequency1,
                                kExtension1->absolute_capture_timestamp,
                                kExtension1->estimated_capture_clock_offset),
            kExtension1);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp2, kRtpClockFrequency2,
                                kExtension2->absolute_capture_timestamp,
                                kExtension2->estimated_capture_clock_offset),
            absl::nullopt);
}

TEST(AbsoluteCaptureTimeSenderTest,
     SkipInterpolateIfRtpClockFrequencyIsInvalid) {
  constexpr uint32_t kSource = 1337;
  constexpr uint32_t kRtpClockFrequency0 = 0;
  constexpr uint32_t kRtpClockFrequency1 = 0;
  constexpr uint32_t kRtpClockFrequency2 = 0;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0;
  constexpr uint32_t kRtpTimestamp2 = kRtpTimestamp0;
  static const absl::optional<AbsoluteCaptureTime> kExtension0 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension1 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000 + 20), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension2 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000 + 40), Int64MsToQ32x32(-350)};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeSender sender(&clock);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp0, kRtpClockFrequency0,
                                kExtension0->absolute_capture_timestamp,
                                kExtension0->estimated_capture_clock_offset),
            kExtension0);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp1, kRtpClockFrequency1,
                                kExtension1->absolute_capture_timestamp,
                                kExtension1->estimated_capture_clock_offset),
            kExtension1);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp2, kRtpClockFrequency2,
                                kExtension2->absolute_capture_timestamp,
                                kExtension2->estimated_capture_clock_offset),
            kExtension2);
}

TEST(AbsoluteCaptureTimeSenderTest,
     SkipInterpolateIfEstimatedCaptureClockOffsetChanged) {
  constexpr uint32_t kSource = 1337;
  constexpr uint32_t kRtpClockFrequency = 64000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 1280;
  constexpr uint32_t kRtpTimestamp2 = kRtpTimestamp0 + 2560;
  static const absl::optional<AbsoluteCaptureTime> kExtension0 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension1 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000 + 20), Int64MsToQ32x32(370)};
  static const absl::optional<AbsoluteCaptureTime> kExtension2 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000 + 40), absl::nullopt};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeSender sender(&clock);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp0, kRtpClockFrequency,
                                kExtension0->absolute_capture_timestamp,
                                kExtension0->estimated_capture_clock_offset),
            kExtension0);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp1, kRtpClockFrequency,
                                kExtension1->absolute_capture_timestamp,
                                kExtension1->estimated_capture_clock_offset),
            kExtension1);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp2, kRtpClockFrequency,
                                kExtension2->absolute_capture_timestamp,
                                kExtension2->estimated_capture_clock_offset),
            kExtension2);
}

TEST(AbsoluteCaptureTimeSenderTest,
     SkipInterpolateIfTooMuchInterpolationError) {
  constexpr uint32_t kSource = 1337;
  constexpr uint32_t kRtpClockFrequency = 64000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 1280;
  constexpr uint32_t kRtpTimestamp2 = kRtpTimestamp0 + 2560;
  static const absl::optional<AbsoluteCaptureTime> kExtension0 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension1 =
      AbsoluteCaptureTime{
          Int64MsToUQ32x32(
              9000 + 20 +
              AbsoluteCaptureTimeSender::kInterpolationMaxError.ms()),
          Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension2 =
      AbsoluteCaptureTime{
          Int64MsToUQ32x32(
              9000 + 40 +
              AbsoluteCaptureTimeSender::kInterpolationMaxError.ms() + 1),
          Int64MsToQ32x32(-350)};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeSender sender(&clock);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp0, kRtpClockFrequency,
                                kExtension0->absolute_capture_timestamp,
                                kExtension0->estimated_capture_clock_offset),
            kExtension0);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp1, kRtpClockFrequency,
                                kExtension1->absolute_capture_timestamp,
                                kExtension1->estimated_capture_clock_offset),
            absl::nullopt);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp2, kRtpClockFrequency,
                                kExtension2->absolute_capture_timestamp,
                                kExtension2->estimated_capture_clock_offset),
            kExtension2);
}

}  // namespace webrtc
