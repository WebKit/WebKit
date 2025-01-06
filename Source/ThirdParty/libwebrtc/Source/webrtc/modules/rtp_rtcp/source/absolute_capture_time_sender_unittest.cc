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

  EXPECT_EQ(AbsoluteCaptureTimeSender::GetSource(kSsrc, {}), kSsrc);
}

TEST(AbsoluteCaptureTimeSenderTest, GetSourceWithCsrcs) {
  constexpr uint32_t kSsrc = 12;
  constexpr uint32_t kCsrcs[] = {34, 56, 78, 90};

  EXPECT_EQ(AbsoluteCaptureTimeSender::GetSource(kSsrc, kCsrcs), kCsrcs[0]);
}

TEST(AbsoluteCaptureTimeSenderTest, InterpolateLaterPacketSentLater) {
  constexpr uint32_t kSource = 1337;
  constexpr int kRtpClockFrequency = 64'000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 1280;
  constexpr uint32_t kRtpTimestamp2 = kRtpTimestamp0 + 2560;
  const AbsoluteCaptureTime kExtension0 = {Int64MsToUQ32x32(9000),
                                           Int64MsToQ32x32(-350)};
  const AbsoluteCaptureTime kExtension1 = {Int64MsToUQ32x32(9000 + 20),
                                           Int64MsToQ32x32(-350)};
  const AbsoluteCaptureTime kExtension2 = {Int64MsToUQ32x32(9000 + 40),
                                           Int64MsToQ32x32(-350)};
  SimulatedClock clock(0);
  AbsoluteCaptureTimeSender sender(&clock);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp0, kRtpClockFrequency,
                                NtpTime(kExtension0.absolute_capture_timestamp),
                                kExtension0.estimated_capture_clock_offset),
            kExtension0);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp1, kRtpClockFrequency,
                                NtpTime(kExtension1.absolute_capture_timestamp),
                                kExtension1.estimated_capture_clock_offset),
            std::nullopt);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp2, kRtpClockFrequency,
                                NtpTime(kExtension2.absolute_capture_timestamp),
                                kExtension2.estimated_capture_clock_offset),
            std::nullopt);
}

TEST(AbsoluteCaptureTimeSenderTest, InterpolateEarlierPacketSentLater) {
  constexpr uint32_t kSource = 1337;
  constexpr int kRtpClockFrequency = 64'000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 - 1280;
  constexpr uint32_t kRtpTimestamp2 = kRtpTimestamp0 - 2560;
  const AbsoluteCaptureTime kExtension0 = {Int64MsToUQ32x32(9000),
                                           Int64MsToQ32x32(-350)};
  const AbsoluteCaptureTime kExtension1 = {Int64MsToUQ32x32(9000 - 20),
                                           Int64MsToQ32x32(-350)};
  const AbsoluteCaptureTime kExtension2 = {Int64MsToUQ32x32(9000 - 40),
                                           Int64MsToQ32x32(-350)};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeSender sender(&clock);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp0, kRtpClockFrequency,
                                NtpTime(kExtension0.absolute_capture_timestamp),
                                kExtension0.estimated_capture_clock_offset),
            kExtension0);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp1, kRtpClockFrequency,
                                NtpTime(kExtension1.absolute_capture_timestamp),
                                kExtension1.estimated_capture_clock_offset),
            std::nullopt);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp2, kRtpClockFrequency,
                                NtpTime(kExtension2.absolute_capture_timestamp),
                                kExtension2.estimated_capture_clock_offset),
            std::nullopt);
}

TEST(AbsoluteCaptureTimeSenderTest,
     InterpolateLaterPacketSentLaterWithRtpTimestampWrapAround) {
  constexpr uint32_t kSource = 1337;
  constexpr int kRtpClockFrequency = 64'000;
  constexpr uint32_t kRtpTimestamp0 = uint32_t{0} - 80;
  constexpr uint32_t kRtpTimestamp1 = 1280 - 80;
  constexpr uint32_t kRtpTimestamp2 = 2560 - 80;
  const AbsoluteCaptureTime kExtension0 = {Int64MsToUQ32x32(9000),
                                           Int64MsToQ32x32(-350)};
  const AbsoluteCaptureTime kExtension1 = {Int64MsToUQ32x32(9000 + 20),
                                           Int64MsToQ32x32(-350)};
  const AbsoluteCaptureTime kExtension2 = {Int64MsToUQ32x32(9000 + 40),
                                           Int64MsToQ32x32(-350)};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeSender sender(&clock);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp0, kRtpClockFrequency,
                                NtpTime(kExtension0.absolute_capture_timestamp),
                                kExtension0.estimated_capture_clock_offset),
            kExtension0);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp1, kRtpClockFrequency,
                                NtpTime(kExtension1.absolute_capture_timestamp),
                                kExtension1.estimated_capture_clock_offset),
            std::nullopt);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp2, kRtpClockFrequency,
                                NtpTime(kExtension2.absolute_capture_timestamp),
                                kExtension2.estimated_capture_clock_offset),
            std::nullopt);
}

TEST(AbsoluteCaptureTimeSenderTest,
     InterpolateEarlierPacketSentLaterWithRtpTimestampWrapAround) {
  constexpr uint32_t kSource = 1337;
  constexpr int kRtpClockFrequency = 64'000;
  constexpr uint32_t kRtpTimestamp0 = 799;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 - 1280;
  constexpr uint32_t kRtpTimestamp2 = kRtpTimestamp0 - 2560;
  const AbsoluteCaptureTime kExtension0 = {Int64MsToUQ32x32(9000),
                                           Int64MsToQ32x32(-350)};
  const AbsoluteCaptureTime kExtension1 = {Int64MsToUQ32x32(9000 - 20),
                                           Int64MsToQ32x32(-350)};
  const AbsoluteCaptureTime kExtension2 = {Int64MsToUQ32x32(9000 - 40),
                                           Int64MsToQ32x32(-350)};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeSender sender(&clock);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp0, kRtpClockFrequency,
                                NtpTime(kExtension0.absolute_capture_timestamp),
                                kExtension0.estimated_capture_clock_offset),
            kExtension0);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp1, kRtpClockFrequency,
                                NtpTime(kExtension1.absolute_capture_timestamp),
                                kExtension1.estimated_capture_clock_offset),
            std::nullopt);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp2, kRtpClockFrequency,
                                NtpTime(kExtension2.absolute_capture_timestamp),
                                kExtension2.estimated_capture_clock_offset),
            std::nullopt);
}

TEST(AbsoluteCaptureTimeSenderTest, SkipInterpolateIfTooLate) {
  constexpr uint32_t kSource = 1337;
  constexpr int kRtpClockFrequency = 64'000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 1280;
  constexpr uint32_t kRtpTimestamp2 = kRtpTimestamp0 + 2560;
  const AbsoluteCaptureTime kExtension0 = {Int64MsToUQ32x32(9000),
                                           Int64MsToQ32x32(-350)};
  const AbsoluteCaptureTime kExtension1 = {Int64MsToUQ32x32(9000 + 20),
                                           Int64MsToQ32x32(-350)};
  const AbsoluteCaptureTime kExtension2 = {Int64MsToUQ32x32(9000 + 40),
                                           Int64MsToQ32x32(-350)};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeSender sender(&clock);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp0, kRtpClockFrequency,
                                NtpTime(kExtension0.absolute_capture_timestamp),
                                kExtension0.estimated_capture_clock_offset),
            kExtension0);

  clock.AdvanceTime(AbsoluteCaptureTimeSender::kInterpolationMaxInterval);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp1, kRtpClockFrequency,
                                NtpTime(kExtension1.absolute_capture_timestamp),
                                kExtension1.estimated_capture_clock_offset),
            std::nullopt);

  clock.AdvanceTime(TimeDelta::Millis(1));

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp2, kRtpClockFrequency,
                                NtpTime(kExtension2.absolute_capture_timestamp),
                                kExtension2.estimated_capture_clock_offset),
            kExtension2);
}

TEST(AbsoluteCaptureTimeSenderTest, SkipInterpolateIfSourceChanged) {
  constexpr uint32_t kSource0 = 1337;
  constexpr uint32_t kSource1 = 1338;
  constexpr int kRtpClockFrequency = 64'000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 1280;
  constexpr uint32_t kRtpTimestamp2 = kRtpTimestamp0 + 2560;
  const AbsoluteCaptureTime kExtension0 = {Int64MsToUQ32x32(9000),
                                           Int64MsToQ32x32(-350)};
  const AbsoluteCaptureTime kExtension1 = {Int64MsToUQ32x32(9000 + 20),
                                           Int64MsToQ32x32(-350)};
  const AbsoluteCaptureTime kExtension2 = {Int64MsToUQ32x32(9000 + 40),
                                           Int64MsToQ32x32(-350)};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeSender sender(&clock);

  EXPECT_EQ(sender.OnSendPacket(kSource0, kRtpTimestamp0, kRtpClockFrequency,
                                NtpTime(kExtension0.absolute_capture_timestamp),
                                kExtension0.estimated_capture_clock_offset),
            kExtension0);

  EXPECT_EQ(sender.OnSendPacket(kSource1, kRtpTimestamp1, kRtpClockFrequency,
                                NtpTime(kExtension1.absolute_capture_timestamp),
                                kExtension1.estimated_capture_clock_offset),
            kExtension1);

  EXPECT_EQ(sender.OnSendPacket(kSource1, kRtpTimestamp2, kRtpClockFrequency,
                                NtpTime(kExtension2.absolute_capture_timestamp),
                                kExtension2.estimated_capture_clock_offset),
            std::nullopt);
}

TEST(AbsoluteCaptureTimeSenderTest, SkipInterpolateWhenForced) {
  constexpr uint32_t kSource = 1337;
  constexpr int kRtpClockFrequency = 64'000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 1280;
  constexpr uint32_t kRtpTimestamp2 = kRtpTimestamp0 + 2560;
  const AbsoluteCaptureTime kExtension0 = {Int64MsToUQ32x32(9000),
                                           Int64MsToQ32x32(-350)};
  const AbsoluteCaptureTime kExtension1 = {Int64MsToUQ32x32(9000 + 20),
                                           Int64MsToQ32x32(-350)};
  const AbsoluteCaptureTime kExtension2 = {Int64MsToUQ32x32(9000 + 40),
                                           Int64MsToQ32x32(-350)};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeSender sender(&clock);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp0, kRtpClockFrequency,
                                NtpTime(kExtension0.absolute_capture_timestamp),
                                kExtension0.estimated_capture_clock_offset),
            kExtension0);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp1, kRtpClockFrequency,
                                NtpTime(kExtension1.absolute_capture_timestamp),
                                kExtension1.estimated_capture_clock_offset,
                                /*force=*/true),
            kExtension1);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp2, kRtpClockFrequency,
                                NtpTime(kExtension2.absolute_capture_timestamp),
                                kExtension2.estimated_capture_clock_offset,
                                /*force=*/false),
            std::nullopt);
}

TEST(AbsoluteCaptureTimeSenderTest, SkipInterpolateIfRtpClockFrequencyChanged) {
  constexpr uint32_t kSource = 1337;
  constexpr int kRtpClockFrequency0 = 64'000;
  constexpr int kRtpClockFrequency1 = 32'000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 640;
  constexpr uint32_t kRtpTimestamp2 = kRtpTimestamp0 + 1280;
  const AbsoluteCaptureTime kExtension0 = {Int64MsToUQ32x32(9000),
                                           Int64MsToQ32x32(-350)};
  const AbsoluteCaptureTime kExtension1 = {Int64MsToUQ32x32(9000 + 20),
                                           Int64MsToQ32x32(-350)};
  const AbsoluteCaptureTime kExtension2 = {Int64MsToUQ32x32(9000 + 40),
                                           Int64MsToQ32x32(-350)};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeSender sender(&clock);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp0, kRtpClockFrequency0,
                                NtpTime(kExtension0.absolute_capture_timestamp),
                                kExtension0.estimated_capture_clock_offset),
            kExtension0);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp1, kRtpClockFrequency1,
                                NtpTime(kExtension1.absolute_capture_timestamp),
                                kExtension1.estimated_capture_clock_offset),
            kExtension1);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp2, kRtpClockFrequency1,
                                NtpTime(kExtension2.absolute_capture_timestamp),
                                kExtension2.estimated_capture_clock_offset),
            std::nullopt);
}

TEST(AbsoluteCaptureTimeSenderTest,
     SkipInterpolateIfRtpClockFrequencyIsInvalid) {
  constexpr uint32_t kSource = 1337;
  constexpr int kRtpClockFrequency = 0;
  constexpr uint32_t kRtpTimestamp = 1020300000;
  const AbsoluteCaptureTime kExtension0 = {Int64MsToUQ32x32(9000),
                                           Int64MsToQ32x32(-350)};
  const AbsoluteCaptureTime kExtension1 = {Int64MsToUQ32x32(9000 + 20),
                                           Int64MsToQ32x32(-350)};
  const AbsoluteCaptureTime kExtension2 = {Int64MsToUQ32x32(9000 + 40),
                                           Int64MsToQ32x32(-350)};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeSender sender(&clock);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp, kRtpClockFrequency,
                                NtpTime(kExtension0.absolute_capture_timestamp),
                                kExtension0.estimated_capture_clock_offset),
            kExtension0);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp, kRtpClockFrequency,
                                NtpTime(kExtension1.absolute_capture_timestamp),
                                kExtension1.estimated_capture_clock_offset),
            kExtension1);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp, kRtpClockFrequency,
                                NtpTime(kExtension2.absolute_capture_timestamp),
                                kExtension2.estimated_capture_clock_offset),
            kExtension2);
}

TEST(AbsoluteCaptureTimeSenderTest,
     SkipInterpolateIfEstimatedCaptureClockOffsetChanged) {
  constexpr uint32_t kSource = 1337;
  constexpr int kRtpClockFrequency = 64'000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 1280;
  constexpr uint32_t kRtpTimestamp2 = kRtpTimestamp0 + 2560;
  const AbsoluteCaptureTime kExtension0 = {Int64MsToUQ32x32(9000),
                                           Int64MsToQ32x32(-350)};
  const AbsoluteCaptureTime kExtension1 = {Int64MsToUQ32x32(9000 + 20),
                                           Int64MsToQ32x32(370)};
  const AbsoluteCaptureTime kExtension2 = {Int64MsToUQ32x32(9000 + 40),
                                           std::nullopt};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeSender sender(&clock);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp0, kRtpClockFrequency,
                                NtpTime(kExtension0.absolute_capture_timestamp),
                                kExtension0.estimated_capture_clock_offset),
            kExtension0);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp1, kRtpClockFrequency,
                                NtpTime(kExtension1.absolute_capture_timestamp),
                                kExtension1.estimated_capture_clock_offset),
            kExtension1);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp2, kRtpClockFrequency,
                                NtpTime(kExtension2.absolute_capture_timestamp),
                                kExtension2.estimated_capture_clock_offset),
            kExtension2);
}

TEST(AbsoluteCaptureTimeSenderTest,
     SkipInterpolateIfTooMuchInterpolationError) {
  constexpr uint32_t kSource = 1337;
  constexpr int kRtpClockFrequency = 64'000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 1280;
  constexpr uint32_t kRtpTimestamp2 = kRtpTimestamp0 + 2560;
  const AbsoluteCaptureTime kExtension0 = {Int64MsToUQ32x32(9000),
                                           Int64MsToQ32x32(-350)};
  const AbsoluteCaptureTime kExtension1 = {
      Int64MsToUQ32x32(9000 + 20 +
                       AbsoluteCaptureTimeSender::kInterpolationMaxError.ms()),
      Int64MsToQ32x32(-350)};
  const AbsoluteCaptureTime kExtension2 = {
      Int64MsToUQ32x32(9000 + 40 +
                       AbsoluteCaptureTimeSender::kInterpolationMaxError.ms() +
                       1),
      Int64MsToQ32x32(-350)};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeSender sender(&clock);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp0, kRtpClockFrequency,
                                NtpTime(kExtension0.absolute_capture_timestamp),
                                kExtension0.estimated_capture_clock_offset),
            kExtension0);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp1, kRtpClockFrequency,
                                NtpTime(kExtension1.absolute_capture_timestamp),
                                kExtension1.estimated_capture_clock_offset),
            std::nullopt);

  EXPECT_EQ(sender.OnSendPacket(kSource, kRtpTimestamp2, kRtpClockFrequency,
                                NtpTime(kExtension2.absolute_capture_timestamp),
                                kExtension2.estimated_capture_clock_offset),
            kExtension2);
}

}  // namespace webrtc
