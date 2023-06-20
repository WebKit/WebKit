/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "system_wrappers/include/rtp_to_ntp_estimator.h"

#include <stddef.h>

#include "rtc_base/random.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
constexpr uint64_t kOneMsInNtp = 4294967;
constexpr uint64_t kOneHourInNtp = uint64_t{60 * 60} << 32;
constexpr uint32_t kTimestampTicksPerMs = 90;
}  // namespace

TEST(WrapAroundTests, OldRtcpWrapped_OldRtpTimestamp) {
  RtpToNtpEstimator estimator;
  EXPECT_EQ(estimator.UpdateMeasurements(NtpTime(kOneMsInNtp), 0),
            RtpToNtpEstimator::kNewMeasurement);
  // No wraparound will be detected, since we are not allowed to wrap below 0,
  // but there will be huge rtp timestamp jump, e.g. old_timestamp = 0,
  // new_timestamp = 4294967295, which should be detected.
  EXPECT_EQ(estimator.UpdateMeasurements(NtpTime(2 * kOneMsInNtp),
                                         -kTimestampTicksPerMs),
            RtpToNtpEstimator::kInvalidMeasurement);
}

TEST(WrapAroundTests, OldRtcpWrapped_OldRtpTimestamp_Wraparound_Detected) {
  RtpToNtpEstimator estimator;
  EXPECT_EQ(estimator.UpdateMeasurements(NtpTime(1), 0xFFFFFFFE),
            RtpToNtpEstimator::kNewMeasurement);
  EXPECT_EQ(estimator.UpdateMeasurements(NtpTime(1 + 2 * kOneMsInNtp),
                                         0xFFFFFFFE + 2 * kTimestampTicksPerMs),
            RtpToNtpEstimator::kNewMeasurement);
  // Expected to fail since the older RTCP has a smaller RTP timestamp than the
  // newer (old:10, new:4294967206).
  EXPECT_EQ(estimator.UpdateMeasurements(NtpTime(1 + 3 * kOneMsInNtp),
                                         0xFFFFFFFE + kTimestampTicksPerMs),
            RtpToNtpEstimator::kInvalidMeasurement);
}

TEST(WrapAroundTests, OldRtcpWrapped_OldRtpTimestamp_NegativeWraparound) {
  RtpToNtpEstimator estimator;
  EXPECT_EQ(estimator.UpdateMeasurements(NtpTime(1), 0),
            RtpToNtpEstimator::kNewMeasurement);
  // Expected to fail since the older RTCP has a smaller RTP timestamp than the
  // newer (old:0, new:-180).
  EXPECT_EQ(estimator.UpdateMeasurements(NtpTime(1 + 2 * kOneMsInNtp),
                                         0xFFFFFFFF - 2 * kTimestampTicksPerMs),
            RtpToNtpEstimator::kInvalidMeasurement);
}

TEST(WrapAroundTests, NewRtcpWrapped) {
  RtpToNtpEstimator estimator;
  EXPECT_EQ(estimator.UpdateMeasurements(NtpTime(1), 0xFFFFFFFF),
            RtpToNtpEstimator::kNewMeasurement);
  EXPECT_EQ(estimator.UpdateMeasurements(NtpTime(1 + kOneMsInNtp),
                                         0xFFFFFFFF + kTimestampTicksPerMs),
            RtpToNtpEstimator::kNewMeasurement);
  // Since this RTP packet has the same timestamp as the RTCP packet constructed
  // at time 0 it should be mapped to 0 as well.
  EXPECT_EQ(estimator.Estimate(0xFFFFFFFF), NtpTime(1));
}

TEST(WrapAroundTests, RtpWrapped) {
  RtpToNtpEstimator estimator;
  EXPECT_EQ(estimator.UpdateMeasurements(NtpTime(1),
                                         0xFFFFFFFF - 2 * kTimestampTicksPerMs),
            RtpToNtpEstimator::kNewMeasurement);
  EXPECT_EQ(estimator.UpdateMeasurements(NtpTime(1 + kOneMsInNtp),
                                         0xFFFFFFFF - kTimestampTicksPerMs),
            RtpToNtpEstimator::kNewMeasurement);

  // Since this RTP packet has the same timestamp as the RTCP packet constructed
  // at time 0 it should be mapped to 0 as well.
  EXPECT_EQ(estimator.Estimate(0xFFFFFFFF - 2 * kTimestampTicksPerMs),
            NtpTime(1));
  // Two kTimestampTicksPerMs advanced.
  EXPECT_EQ(estimator.Estimate(0xFFFFFFFF), NtpTime(1 + 2 * kOneMsInNtp));
  // Wrapped rtp.
  EXPECT_EQ(estimator.Estimate(0xFFFFFFFF + kTimestampTicksPerMs),
            NtpTime(1 + 3 * kOneMsInNtp));
}

TEST(WrapAroundTests, OldRtp_RtcpsWrapped) {
  RtpToNtpEstimator estimator;
  EXPECT_EQ(estimator.UpdateMeasurements(NtpTime(1), 0xFFFFFFFF),
            RtpToNtpEstimator::kNewMeasurement);
  EXPECT_EQ(estimator.UpdateMeasurements(NtpTime(1 + kOneMsInNtp),
                                         0xFFFFFFFF + kTimestampTicksPerMs),
            RtpToNtpEstimator::kNewMeasurement);

  EXPECT_FALSE(estimator.Estimate(0xFFFFFFFF - kTimestampTicksPerMs).Valid());
}

TEST(WrapAroundTests, OldRtp_NewRtcpWrapped) {
  RtpToNtpEstimator estimator;
  EXPECT_EQ(estimator.UpdateMeasurements(NtpTime(1), 0xFFFFFFFF),
            RtpToNtpEstimator::kNewMeasurement);
  EXPECT_EQ(estimator.UpdateMeasurements(NtpTime(1 + kOneMsInNtp),
                                         0xFFFFFFFF + kTimestampTicksPerMs),
            RtpToNtpEstimator::kNewMeasurement);

  // Constructed at the same time as the first RTCP and should therefore be
  // mapped to zero.
  EXPECT_EQ(estimator.Estimate(0xFFFFFFFF), NtpTime(1));
}

TEST(WrapAroundTests, GracefullyHandleRtpJump) {
  RtpToNtpEstimator estimator;
  EXPECT_EQ(estimator.UpdateMeasurements(NtpTime(1), 0xFFFFFFFF),
            RtpToNtpEstimator::kNewMeasurement);
  EXPECT_EQ(estimator.UpdateMeasurements(NtpTime(1 + kOneMsInNtp),
                                         0xFFFFFFFF + kTimestampTicksPerMs),
            RtpToNtpEstimator::kNewMeasurement);

  // Constructed at the same time as the first RTCP and should therefore be
  // mapped to zero.
  EXPECT_EQ(estimator.Estimate(0xFFFFFFFF), NtpTime(1));

  uint32_t timestamp = 0xFFFFFFFF - 0xFFFFF;
  uint64_t ntp_raw = 1 + 2 * kOneMsInNtp;
  for (int i = 0; i < RtpToNtpEstimator::kMaxInvalidSamples - 1; ++i) {
    EXPECT_EQ(estimator.UpdateMeasurements(NtpTime(ntp_raw), timestamp),
              RtpToNtpEstimator::kInvalidMeasurement);
    ntp_raw += kOneMsInNtp;
    timestamp += kTimestampTicksPerMs;
  }
  EXPECT_EQ(estimator.UpdateMeasurements(NtpTime(ntp_raw), timestamp),
            RtpToNtpEstimator::kNewMeasurement);
  ntp_raw += kOneMsInNtp;
  timestamp += kTimestampTicksPerMs;
  EXPECT_EQ(estimator.UpdateMeasurements(NtpTime(ntp_raw), timestamp),
            RtpToNtpEstimator::kNewMeasurement);

  EXPECT_EQ(estimator.Estimate(timestamp), NtpTime(ntp_raw));
}

TEST(UpdateRtcpMeasurementTests, FailsForZeroNtp) {
  RtpToNtpEstimator estimator;

  EXPECT_EQ(estimator.UpdateMeasurements(NtpTime(0), 0x12345678),
            RtpToNtpEstimator::kInvalidMeasurement);
}

TEST(UpdateRtcpMeasurementTests, FailsForEqualNtp) {
  RtpToNtpEstimator estimator;
  NtpTime ntp(0, 699925050);
  uint32_t timestamp = 0x12345678;

  EXPECT_EQ(estimator.UpdateMeasurements(ntp, timestamp),
            RtpToNtpEstimator::kNewMeasurement);
  // Ntp time already added, list not updated.
  EXPECT_EQ(estimator.UpdateMeasurements(ntp, timestamp + 1),
            RtpToNtpEstimator::kSameMeasurement);
}

TEST(UpdateRtcpMeasurementTests, FailsForOldNtp) {
  RtpToNtpEstimator estimator;
  uint64_t ntp_raw = 699925050;
  NtpTime ntp(ntp_raw);
  uint32_t timestamp = 0x12345678;
  EXPECT_EQ(estimator.UpdateMeasurements(ntp, timestamp),
            RtpToNtpEstimator::kNewMeasurement);

  // Old ntp time, list not updated.
  EXPECT_EQ(estimator.UpdateMeasurements(NtpTime(ntp_raw - kOneMsInNtp),
                                         timestamp + kTimestampTicksPerMs),
            RtpToNtpEstimator::kInvalidMeasurement);
}

TEST(UpdateRtcpMeasurementTests, FailsForTooNewNtp) {
  RtpToNtpEstimator estimator;

  uint64_t ntp_raw = 699925050;
  uint32_t timestamp = 0x12345678;
  EXPECT_EQ(estimator.UpdateMeasurements(NtpTime(ntp_raw), timestamp),
            RtpToNtpEstimator::kNewMeasurement);

  // Ntp time from far future, list not updated.
  EXPECT_EQ(estimator.UpdateMeasurements(NtpTime(ntp_raw + 2 * kOneHourInNtp),
                                         timestamp + 10 * kTimestampTicksPerMs),
            RtpToNtpEstimator::kInvalidMeasurement);
}

TEST(UpdateRtcpMeasurementTests, FailsForEqualTimestamp) {
  RtpToNtpEstimator estimator;

  uint32_t timestamp = 0x12345678;
  EXPECT_EQ(estimator.UpdateMeasurements(NtpTime(2), timestamp),
            RtpToNtpEstimator::kNewMeasurement);
  // Timestamp already added, list not updated.
  EXPECT_EQ(estimator.UpdateMeasurements(NtpTime(3), timestamp),
            RtpToNtpEstimator::kSameMeasurement);
}

TEST(UpdateRtcpMeasurementTests, FailsForOldRtpTimestamp) {
  RtpToNtpEstimator estimator;
  uint32_t timestamp = 0x12345678;

  EXPECT_EQ(estimator.UpdateMeasurements(NtpTime(2), timestamp),
            RtpToNtpEstimator::kNewMeasurement);
  // Old timestamp, list not updated.
  EXPECT_EQ(estimator.UpdateMeasurements(NtpTime(2 + kOneMsInNtp),
                                         timestamp - kTimestampTicksPerMs),
            RtpToNtpEstimator::kInvalidMeasurement);
}

TEST(UpdateRtcpMeasurementTests, VerifyParameters) {
  RtpToNtpEstimator estimator;
  uint32_t timestamp = 0x12345678;

  EXPECT_EQ(estimator.UpdateMeasurements(NtpTime(kOneMsInNtp), timestamp),
            RtpToNtpEstimator::kNewMeasurement);

  EXPECT_DOUBLE_EQ(estimator.EstimatedFrequencyKhz(), 0.0);

  // Add second report, parameters should be calculated.
  EXPECT_EQ(estimator.UpdateMeasurements(NtpTime(2 * kOneMsInNtp),
                                         timestamp + kTimestampTicksPerMs),
            RtpToNtpEstimator::kNewMeasurement);

  EXPECT_NEAR(estimator.EstimatedFrequencyKhz(), kTimestampTicksPerMs, 0.01);
}

TEST(RtpToNtpTests, FailsForNoParameters) {
  RtpToNtpEstimator estimator;
  uint32_t timestamp = 0x12345678;

  EXPECT_EQ(estimator.UpdateMeasurements(NtpTime(1), timestamp),
            RtpToNtpEstimator::kNewMeasurement);
  // Parameters are not calculated, conversion of RTP to NTP time should fail.
  EXPECT_DOUBLE_EQ(estimator.EstimatedFrequencyKhz(), 0.0);
  EXPECT_FALSE(estimator.Estimate(timestamp).Valid());
}

TEST(RtpToNtpTests, AveragesErrorOut) {
  RtpToNtpEstimator estimator;
  uint64_t ntp_raw = 90000000;  // More than 1 ms.
  ASSERT_GT(ntp_raw, kOneMsInNtp);
  uint32_t timestamp = 0x12345678;
  constexpr uint64_t kNtpSecStep = uint64_t{1} << 32;  // 1 second.
  constexpr int kRtpTicksPerMs = 90;
  constexpr int kRtpStep = kRtpTicksPerMs * 1000;

  EXPECT_EQ(estimator.UpdateMeasurements(NtpTime(ntp_raw), timestamp),
            RtpToNtpEstimator::kNewMeasurement);

  Random rand(1123536L);
  for (size_t i = 0; i < 1000; i++) {
    // Advance both timestamps by exactly 1 second.
    ntp_raw += kNtpSecStep;
    timestamp += kRtpStep;
    // Add upto 1ms of errors to NTP and RTP timestamps passed to estimator.
    EXPECT_EQ(
        estimator.UpdateMeasurements(
            NtpTime(ntp_raw + rand.Rand(-int{kOneMsInNtp}, int{kOneMsInNtp})),
            timestamp + rand.Rand(-kRtpTicksPerMs, kRtpTicksPerMs)),
        RtpToNtpEstimator::kNewMeasurement);

    NtpTime estimated_ntp = estimator.Estimate(timestamp);
    EXPECT_TRUE(estimated_ntp.Valid());
    // Allow upto 2 ms of error.
    EXPECT_NEAR(ntp_raw, static_cast<uint64_t>(estimated_ntp), 2 * kOneMsInNtp);
  }
}

}  // namespace webrtc
