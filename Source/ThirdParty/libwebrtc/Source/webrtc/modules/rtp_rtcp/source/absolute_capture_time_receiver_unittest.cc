/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/absolute_capture_time_receiver.h"

#include "system_wrappers/include/ntp_time.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

TEST(AbsoluteCaptureTimeReceiverTest, GetSourceWithoutCsrcs) {
  constexpr uint32_t kSsrc = 12;

  EXPECT_EQ(AbsoluteCaptureTimeReceiver::GetSource(kSsrc, nullptr), kSsrc);
}

TEST(AbsoluteCaptureTimeReceiverTest, GetSourceWithCsrcs) {
  constexpr uint32_t kSsrc = 12;
  constexpr uint32_t kCsrcs[] = {34, 56, 78, 90};

  EXPECT_EQ(AbsoluteCaptureTimeReceiver::GetSource(kSsrc, kCsrcs), kCsrcs[0]);
}

TEST(AbsoluteCaptureTimeReceiverTest, ReceiveExtensionReturnsExtension) {
  constexpr uint32_t kSource = 1337;
  constexpr uint32_t kRtpClockFrequency = 64000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 1280;
  static const absl::optional<AbsoluteCaptureTime> kExtension0 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension1 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9020), absl::nullopt};

  SimulatedClock clock(0);
  AbsoluteCaptureTimeReceiver receiver(&clock);

  receiver.SetRemoteToLocalClockOffset(0);

  EXPECT_EQ(receiver.OnReceivePacket(kSource, kRtpTimestamp0,
                                     kRtpClockFrequency, kExtension0),
            kExtension0);

  EXPECT_EQ(receiver.OnReceivePacket(kSource, kRtpTimestamp1,
                                     kRtpClockFrequency, kExtension1),
            kExtension1);
}

TEST(AbsoluteCaptureTimeReceiverTest, ReceiveNoExtensionReturnsNoExtension) {
  constexpr uint32_t kSource = 1337;
  constexpr uint32_t kRtpClockFrequency = 64000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 1280;
  static const absl::optional<AbsoluteCaptureTime> kExtension0 = absl::nullopt;
  static const absl::optional<AbsoluteCaptureTime> kExtension1 = absl::nullopt;

  SimulatedClock clock(0);
  AbsoluteCaptureTimeReceiver receiver(&clock);

  receiver.SetRemoteToLocalClockOffset(0);

  EXPECT_EQ(receiver.OnReceivePacket(kSource, kRtpTimestamp0,
                                     kRtpClockFrequency, kExtension0),
            absl::nullopt);

  EXPECT_EQ(receiver.OnReceivePacket(kSource, kRtpTimestamp1,
                                     kRtpClockFrequency, kExtension1),
            absl::nullopt);
}

TEST(AbsoluteCaptureTimeReceiverTest, InterpolateLaterPacketArrivingLater) {
  constexpr uint32_t kSource = 1337;
  constexpr uint32_t kRtpClockFrequency = 64000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 1280;
  constexpr uint32_t kRtpTimestamp2 = kRtpTimestamp0 + 2560;
  static const absl::optional<AbsoluteCaptureTime> kExtension0 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension1 = absl::nullopt;
  static const absl::optional<AbsoluteCaptureTime> kExtension2 = absl::nullopt;

  SimulatedClock clock(0);
  AbsoluteCaptureTimeReceiver receiver(&clock);

  receiver.SetRemoteToLocalClockOffset(0);

  EXPECT_EQ(receiver.OnReceivePacket(kSource, kRtpTimestamp0,
                                     kRtpClockFrequency, kExtension0),
            kExtension0);

  absl::optional<AbsoluteCaptureTime> extension = receiver.OnReceivePacket(
      kSource, kRtpTimestamp1, kRtpClockFrequency, kExtension1);
  EXPECT_TRUE(extension.has_value());
  EXPECT_EQ(UQ32x32ToInt64Ms(extension->absolute_capture_timestamp),
            UQ32x32ToInt64Ms(kExtension0->absolute_capture_timestamp) + 20);
  EXPECT_EQ(extension->estimated_capture_clock_offset,
            kExtension0->estimated_capture_clock_offset);

  extension = receiver.OnReceivePacket(kSource, kRtpTimestamp2,
                                       kRtpClockFrequency, kExtension2);
  EXPECT_TRUE(extension.has_value());
  EXPECT_EQ(UQ32x32ToInt64Ms(extension->absolute_capture_timestamp),
            UQ32x32ToInt64Ms(kExtension0->absolute_capture_timestamp) + 40);
  EXPECT_EQ(extension->estimated_capture_clock_offset,
            kExtension0->estimated_capture_clock_offset);
}

TEST(AbsoluteCaptureTimeReceiverTest, InterpolateEarlierPacketArrivingLater) {
  constexpr uint32_t kSource = 1337;
  constexpr uint32_t kRtpClockFrequency = 64000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 - 1280;
  constexpr uint32_t kRtpTimestamp2 = kRtpTimestamp0 - 2560;
  static const absl::optional<AbsoluteCaptureTime> kExtension0 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension1 = absl::nullopt;
  static const absl::optional<AbsoluteCaptureTime> kExtension2 = absl::nullopt;

  SimulatedClock clock(0);
  AbsoluteCaptureTimeReceiver receiver(&clock);

  receiver.SetRemoteToLocalClockOffset(0);

  EXPECT_EQ(receiver.OnReceivePacket(kSource, kRtpTimestamp0,
                                     kRtpClockFrequency, kExtension0),
            kExtension0);

  absl::optional<AbsoluteCaptureTime> extension = receiver.OnReceivePacket(
      kSource, kRtpTimestamp1, kRtpClockFrequency, kExtension1);
  EXPECT_TRUE(extension.has_value());
  EXPECT_EQ(UQ32x32ToInt64Ms(extension->absolute_capture_timestamp),
            UQ32x32ToInt64Ms(kExtension0->absolute_capture_timestamp) - 20);
  EXPECT_EQ(extension->estimated_capture_clock_offset,
            kExtension0->estimated_capture_clock_offset);

  extension = receiver.OnReceivePacket(kSource, kRtpTimestamp2,
                                       kRtpClockFrequency, kExtension2);
  EXPECT_TRUE(extension.has_value());
  EXPECT_EQ(UQ32x32ToInt64Ms(extension->absolute_capture_timestamp),
            UQ32x32ToInt64Ms(kExtension0->absolute_capture_timestamp) - 40);
  EXPECT_EQ(extension->estimated_capture_clock_offset,
            kExtension0->estimated_capture_clock_offset);
}

TEST(AbsoluteCaptureTimeReceiverTest,
     InterpolateLaterPacketArrivingLaterWithRtpTimestampWrapAround) {
  constexpr uint32_t kSource = 1337;
  constexpr uint32_t kRtpClockFrequency = 64000;
  constexpr uint32_t kRtpTimestamp0 = ~uint32_t{0} - 79;
  constexpr uint32_t kRtpTimestamp1 = 1280 - 80;
  constexpr uint32_t kRtpTimestamp2 = 2560 - 80;
  static const absl::optional<AbsoluteCaptureTime> kExtension0 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension1 = absl::nullopt;
  static const absl::optional<AbsoluteCaptureTime> kExtension2 = absl::nullopt;

  SimulatedClock clock(0);
  AbsoluteCaptureTimeReceiver receiver(&clock);

  receiver.SetRemoteToLocalClockOffset(0);

  EXPECT_EQ(receiver.OnReceivePacket(kSource, kRtpTimestamp0,
                                     kRtpClockFrequency, kExtension0),
            kExtension0);

  absl::optional<AbsoluteCaptureTime> extension = receiver.OnReceivePacket(
      kSource, kRtpTimestamp1, kRtpClockFrequency, kExtension1);
  EXPECT_TRUE(extension.has_value());
  EXPECT_EQ(UQ32x32ToInt64Ms(extension->absolute_capture_timestamp),
            UQ32x32ToInt64Ms(kExtension0->absolute_capture_timestamp) + 20);
  EXPECT_EQ(extension->estimated_capture_clock_offset,
            kExtension0->estimated_capture_clock_offset);

  extension = receiver.OnReceivePacket(kSource, kRtpTimestamp2,
                                       kRtpClockFrequency, kExtension2);
  EXPECT_TRUE(extension.has_value());
  EXPECT_EQ(UQ32x32ToInt64Ms(extension->absolute_capture_timestamp),
            UQ32x32ToInt64Ms(kExtension0->absolute_capture_timestamp) + 40);
  EXPECT_EQ(extension->estimated_capture_clock_offset,
            kExtension0->estimated_capture_clock_offset);
}

TEST(AbsoluteCaptureTimeReceiverTest,
     InterpolateEarlierPacketArrivingLaterWithRtpTimestampWrapAround) {
  constexpr uint32_t kSource = 1337;
  constexpr uint32_t kRtpClockFrequency = 64000;
  constexpr uint32_t kRtpTimestamp0 = 799;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 - 1280;
  constexpr uint32_t kRtpTimestamp2 = kRtpTimestamp0 - 2560;
  static const absl::optional<AbsoluteCaptureTime> kExtension0 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension1 = absl::nullopt;
  static const absl::optional<AbsoluteCaptureTime> kExtension2 = absl::nullopt;

  SimulatedClock clock(0);
  AbsoluteCaptureTimeReceiver receiver(&clock);

  receiver.SetRemoteToLocalClockOffset(0);

  EXPECT_EQ(receiver.OnReceivePacket(kSource, kRtpTimestamp0,
                                     kRtpClockFrequency, kExtension0),
            kExtension0);

  absl::optional<AbsoluteCaptureTime> extension = receiver.OnReceivePacket(
      kSource, kRtpTimestamp1, kRtpClockFrequency, kExtension1);
  EXPECT_TRUE(extension.has_value());
  EXPECT_EQ(UQ32x32ToInt64Ms(extension->absolute_capture_timestamp),
            UQ32x32ToInt64Ms(kExtension0->absolute_capture_timestamp) - 20);
  EXPECT_EQ(extension->estimated_capture_clock_offset,
            kExtension0->estimated_capture_clock_offset);

  extension = receiver.OnReceivePacket(kSource, kRtpTimestamp2,
                                       kRtpClockFrequency, kExtension2);
  EXPECT_TRUE(extension.has_value());
  EXPECT_EQ(UQ32x32ToInt64Ms(extension->absolute_capture_timestamp),
            UQ32x32ToInt64Ms(kExtension0->absolute_capture_timestamp) - 40);
  EXPECT_EQ(extension->estimated_capture_clock_offset,
            kExtension0->estimated_capture_clock_offset);
}

TEST(AbsoluteCaptureTimeReceiverTest,
     SkipEstimatedCaptureClockOffsetIfRemoteToLocalClockOffsetIsUnknown) {
  constexpr uint32_t kSource = 1337;
  constexpr uint32_t kRtpClockFrequency = 64000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 1280;
  constexpr uint32_t kRtpTimestamp2 = kRtpTimestamp0 + 2560;
  static const absl::optional<AbsoluteCaptureTime> kExtension0 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension1 = absl::nullopt;
  static const absl::optional<AbsoluteCaptureTime> kExtension2 = absl::nullopt;
  static const absl::optional<int64_t> kRemoteToLocalClockOffset2 =
      Int64MsToQ32x32(-7000007);

  SimulatedClock clock(0);
  AbsoluteCaptureTimeReceiver receiver(&clock);

  receiver.SetRemoteToLocalClockOffset(0);

  EXPECT_EQ(receiver.OnReceivePacket(kSource, kRtpTimestamp0,
                                     kRtpClockFrequency, kExtension0),
            kExtension0);

  receiver.SetRemoteToLocalClockOffset(absl::nullopt);

  absl::optional<AbsoluteCaptureTime> extension = receiver.OnReceivePacket(
      kSource, kRtpTimestamp1, kRtpClockFrequency, kExtension1);
  EXPECT_TRUE(extension.has_value());
  EXPECT_EQ(UQ32x32ToInt64Ms(extension->absolute_capture_timestamp),
            UQ32x32ToInt64Ms(kExtension0->absolute_capture_timestamp) + 20);
  EXPECT_EQ(extension->estimated_capture_clock_offset, absl::nullopt);

  receiver.SetRemoteToLocalClockOffset(kRemoteToLocalClockOffset2);

  extension = receiver.OnReceivePacket(kSource, kRtpTimestamp2,
                                       kRtpClockFrequency, kExtension2);
  EXPECT_TRUE(extension.has_value());
  EXPECT_EQ(UQ32x32ToInt64Ms(extension->absolute_capture_timestamp),
            UQ32x32ToInt64Ms(kExtension0->absolute_capture_timestamp) + 40);
  EXPECT_EQ(extension->estimated_capture_clock_offset,
            *kExtension0->estimated_capture_clock_offset +
                *kRemoteToLocalClockOffset2);
}

TEST(AbsoluteCaptureTimeReceiverTest, SkipInterpolateIfTooLate) {
  constexpr uint32_t kSource = 1337;
  constexpr uint32_t kRtpClockFrequency = 64000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 1280;
  constexpr uint32_t kRtpTimestamp2 = kRtpTimestamp1 + 1280;
  static const absl::optional<AbsoluteCaptureTime> kExtension0 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension1 = absl::nullopt;
  static const absl::optional<AbsoluteCaptureTime> kExtension2 = absl::nullopt;

  SimulatedClock clock(0);
  AbsoluteCaptureTimeReceiver receiver(&clock);

  receiver.SetRemoteToLocalClockOffset(0);

  EXPECT_EQ(receiver.OnReceivePacket(kSource, kRtpTimestamp0,
                                     kRtpClockFrequency, kExtension0),
            kExtension0);

  clock.AdvanceTime(AbsoluteCaptureTimeReceiver::kInterpolationMaxInterval);

  EXPECT_TRUE(receiver
                  .OnReceivePacket(kSource, kRtpTimestamp1, kRtpClockFrequency,
                                   kExtension1)
                  .has_value());

  clock.AdvanceTimeMilliseconds(1);

  EXPECT_FALSE(receiver
                   .OnReceivePacket(kSource, kRtpTimestamp2, kRtpClockFrequency,
                                    kExtension2)
                   .has_value());
}

TEST(AbsoluteCaptureTimeReceiverTest, SkipInterpolateIfSourceChanged) {
  constexpr uint32_t kSource0 = 1337;
  constexpr uint32_t kSource1 = 1338;
  constexpr uint32_t kRtpClockFrequency = 64000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 1280;
  static const absl::optional<AbsoluteCaptureTime> kExtension0 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension1 = absl::nullopt;

  SimulatedClock clock(0);
  AbsoluteCaptureTimeReceiver receiver(&clock);

  receiver.SetRemoteToLocalClockOffset(0);

  EXPECT_EQ(receiver.OnReceivePacket(kSource0, kRtpTimestamp0,
                                     kRtpClockFrequency, kExtension0),
            kExtension0);

  EXPECT_FALSE(receiver
                   .OnReceivePacket(kSource1, kRtpTimestamp1,
                                    kRtpClockFrequency, kExtension1)
                   .has_value());
}

TEST(AbsoluteCaptureTimeReceiverTest,
     SkipInterpolateIfRtpClockFrequencyChanged) {
  constexpr uint32_t kSource = 1337;
  constexpr uint32_t kRtpClockFrequency0 = 64000;
  constexpr uint32_t kRtpClockFrequency1 = 32000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 640;
  static const absl::optional<AbsoluteCaptureTime> kExtension0 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension1 = absl::nullopt;

  SimulatedClock clock(0);
  AbsoluteCaptureTimeReceiver receiver(&clock);

  receiver.SetRemoteToLocalClockOffset(0);

  EXPECT_EQ(receiver.OnReceivePacket(kSource, kRtpTimestamp0,
                                     kRtpClockFrequency0, kExtension0),
            kExtension0);

  EXPECT_FALSE(receiver
                   .OnReceivePacket(kSource, kRtpTimestamp1,
                                    kRtpClockFrequency1, kExtension1)
                   .has_value());
}

TEST(AbsoluteCaptureTimeReceiverTest,
     SkipInterpolateIfRtpClockFrequencyIsInvalid) {
  constexpr uint32_t kSource = 1337;
  constexpr uint32_t kRtpClockFrequency = 0;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 640;
  static const absl::optional<AbsoluteCaptureTime> kExtension0 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension1 = absl::nullopt;

  SimulatedClock clock(0);
  AbsoluteCaptureTimeReceiver receiver(&clock);

  receiver.SetRemoteToLocalClockOffset(0);

  EXPECT_EQ(receiver.OnReceivePacket(kSource, kRtpTimestamp0,
                                     kRtpClockFrequency, kExtension0),
            kExtension0);

  EXPECT_FALSE(receiver
                   .OnReceivePacket(kSource, kRtpTimestamp1, kRtpClockFrequency,
                                    kExtension1)
                   .has_value());
}

TEST(AbsoluteCaptureTimeReceiverTest, SkipInterpolateIsSticky) {
  constexpr uint32_t kSource0 = 1337;
  constexpr uint32_t kSource1 = 1338;
  constexpr uint32_t kSource2 = 1337;
  constexpr uint32_t kRtpClockFrequency = 64000;
  constexpr uint32_t kRtpTimestamp0 = 1020300000;
  constexpr uint32_t kRtpTimestamp1 = kRtpTimestamp0 + 1280;
  constexpr uint32_t kRtpTimestamp2 = kRtpTimestamp1 + 1280;
  static const absl::optional<AbsoluteCaptureTime> kExtension0 =
      AbsoluteCaptureTime{Int64MsToUQ32x32(9000), Int64MsToQ32x32(-350)};
  static const absl::optional<AbsoluteCaptureTime> kExtension1 = absl::nullopt;
  static const absl::optional<AbsoluteCaptureTime> kExtension2 = absl::nullopt;

  SimulatedClock clock(0);
  AbsoluteCaptureTimeReceiver receiver(&clock);

  receiver.SetRemoteToLocalClockOffset(0);

  EXPECT_EQ(receiver.OnReceivePacket(kSource0, kRtpTimestamp0,
                                     kRtpClockFrequency, kExtension0),
            kExtension0);

  EXPECT_FALSE(receiver
                   .OnReceivePacket(kSource1, kRtpTimestamp1,
                                    kRtpClockFrequency, kExtension1)
                   .has_value());

  EXPECT_FALSE(receiver
                   .OnReceivePacket(kSource2, kRtpTimestamp2,
                                    kRtpClockFrequency, kExtension2)
                   .has_value());
}

}  // namespace webrtc
