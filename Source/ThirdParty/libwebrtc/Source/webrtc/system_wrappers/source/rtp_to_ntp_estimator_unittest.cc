/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/system_wrappers/include/rtp_to_ntp_estimator.h"
#include "webrtc/test/gtest.h"

namespace webrtc {
namespace {
const uint32_t kOneMsInNtpFrac = 4294967;
const uint32_t kTimestampTicksPerMs = 90;
}  // namespace

TEST(WrapAroundTests, NoWrap) {
  EXPECT_EQ(0, CheckForWrapArounds(0xFFFFFFFF, 0xFFFFFFFE));
  EXPECT_EQ(0, CheckForWrapArounds(1, 0));
  EXPECT_EQ(0, CheckForWrapArounds(0x00010000, 0x0000FFFF));
}

TEST(WrapAroundTests, ForwardWrap) {
  EXPECT_EQ(1, CheckForWrapArounds(0, 0xFFFFFFFF));
  EXPECT_EQ(1, CheckForWrapArounds(0, 0xFFFF0000));
  EXPECT_EQ(1, CheckForWrapArounds(0x0000FFFF, 0xFFFFFFFF));
  EXPECT_EQ(1, CheckForWrapArounds(0x0000FFFF, 0xFFFF0000));
}

TEST(WrapAroundTests, BackwardWrap) {
  EXPECT_EQ(-1, CheckForWrapArounds(0xFFFFFFFF, 0));
  EXPECT_EQ(-1, CheckForWrapArounds(0xFFFF0000, 0));
  EXPECT_EQ(-1, CheckForWrapArounds(0xFFFFFFFF, 0x0000FFFF));
  EXPECT_EQ(-1, CheckForWrapArounds(0xFFFF0000, 0x0000FFFF));
}

TEST(WrapAroundTests, OldRtcpWrapped_OldRtpTimestamp) {
  RtpToNtpEstimator estimator;
  bool new_sr;
  uint32_t ntp_sec = 0;
  uint32_t ntp_frac = 1;
  uint32_t timestamp = 0;
  EXPECT_TRUE(
      estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp, &new_sr));
  ntp_frac += kOneMsInNtpFrac;
  timestamp -= kTimestampTicksPerMs;
  // Expected to fail since the older RTCP has a smaller RTP timestamp than the
  // newer (old:0, new:4294967206).
  EXPECT_FALSE(
      estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp, &new_sr));
}

TEST(WrapAroundTests, NewRtcpWrapped) {
  RtpToNtpEstimator estimator;
  bool new_sr;
  uint32_t ntp_sec = 0;
  uint32_t ntp_frac = 1;
  uint32_t timestamp = 0xFFFFFFFF;
  EXPECT_TRUE(
      estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp, &new_sr));
  ntp_frac += kOneMsInNtpFrac;
  timestamp += kTimestampTicksPerMs;
  EXPECT_TRUE(
      estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp, &new_sr));
  int64_t timestamp_ms = -1;
  EXPECT_TRUE(estimator.Estimate(0xFFFFFFFF, &timestamp_ms));
  // Since this RTP packet has the same timestamp as the RTCP packet constructed
  // at time 0 it should be mapped to 0 as well.
  EXPECT_EQ(0, timestamp_ms);
}

TEST(WrapAroundTests, RtpWrapped) {
  RtpToNtpEstimator estimator;
  bool new_sr;
  uint32_t ntp_sec = 0;
  uint32_t ntp_frac = 1;
  uint32_t timestamp = 0xFFFFFFFF - 2 * kTimestampTicksPerMs;
  EXPECT_TRUE(
      estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp, &new_sr));
  ntp_frac += kOneMsInNtpFrac;
  timestamp += kTimestampTicksPerMs;
  EXPECT_TRUE(
      estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp, &new_sr));

  int64_t timestamp_ms = -1;
  EXPECT_TRUE(estimator.Estimate(0xFFFFFFFF - 2 * kTimestampTicksPerMs,
                                 &timestamp_ms));
  // Since this RTP packet has the same timestamp as the RTCP packet constructed
  // at time 0 it should be mapped to 0 as well.
  EXPECT_EQ(0, timestamp_ms);
  // Two kTimestampTicksPerMs advanced.
  timestamp += kTimestampTicksPerMs;
  EXPECT_TRUE(estimator.Estimate(timestamp, &timestamp_ms));
  EXPECT_EQ(2, timestamp_ms);
  // Wrapped rtp.
  timestamp += kTimestampTicksPerMs;
  EXPECT_TRUE(estimator.Estimate(timestamp, &timestamp_ms));
  EXPECT_EQ(3, timestamp_ms);
}

TEST(WrapAroundTests, OldRtp_RtcpsWrapped) {
  RtpToNtpEstimator estimator;
  bool new_sr;
  uint32_t ntp_sec = 0;
  uint32_t ntp_frac = 1;
  uint32_t timestamp = 0;
  EXPECT_TRUE(
      estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp, &new_sr));
  ntp_frac += kOneMsInNtpFrac;
  timestamp += kTimestampTicksPerMs;
  EXPECT_TRUE(
      estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp, &new_sr));
  timestamp -= 2 * kTimestampTicksPerMs;
  int64_t timestamp_ms = -1;
  EXPECT_FALSE(estimator.Estimate(timestamp, &timestamp_ms));
}

TEST(WrapAroundTests, OldRtp_NewRtcpWrapped) {
  RtpToNtpEstimator estimator;
  bool new_sr;
  uint32_t ntp_sec = 0;
  uint32_t ntp_frac = 1;
  uint32_t timestamp = 0xFFFFFFFF;
  EXPECT_TRUE(
      estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp, &new_sr));
  ntp_frac += kOneMsInNtpFrac;
  timestamp += kTimestampTicksPerMs;
  EXPECT_TRUE(
      estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp, &new_sr));
  timestamp -= kTimestampTicksPerMs;
  int64_t timestamp_ms = -1;
  EXPECT_TRUE(estimator.Estimate(timestamp, &timestamp_ms));
  // Constructed at the same time as the first RTCP and should therefore be
  // mapped to zero.
  EXPECT_EQ(0, timestamp_ms);
}

TEST(UpdateRtcpMeasurementTests, FailsForZeroNtp) {
  RtpToNtpEstimator estimator;
  uint32_t ntp_sec = 0;
  uint32_t ntp_frac = 0;
  uint32_t timestamp = 0x12345678;
  bool new_sr;
  EXPECT_FALSE(
      estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp, &new_sr));
  EXPECT_FALSE(new_sr);
}

TEST(UpdateRtcpMeasurementTests, FailsForEqualNtp) {
  RtpToNtpEstimator estimator;
  uint32_t ntp_sec = 0;
  uint32_t ntp_frac = 699925050;
  uint32_t timestamp = 0x12345678;
  bool new_sr;
  EXPECT_TRUE(
      estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp, &new_sr));
  EXPECT_TRUE(new_sr);
  // Ntp time already added, list not updated.
  ++timestamp;
  EXPECT_TRUE(
      estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp, &new_sr));
  EXPECT_FALSE(new_sr);
}

TEST(UpdateRtcpMeasurementTests, FailsForOldNtp) {
  RtpToNtpEstimator estimator;
  uint32_t ntp_sec = 1;
  uint32_t ntp_frac = 699925050;
  uint32_t timestamp = 0x12345678;
  bool new_sr;
  EXPECT_TRUE(
      estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp, &new_sr));
  EXPECT_TRUE(new_sr);
  // Old ntp time, list not updated.
  ntp_frac -= kOneMsInNtpFrac;
  timestamp += kTimestampTicksPerMs;
  EXPECT_FALSE(
      estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp, &new_sr));
}

TEST(UpdateRtcpMeasurementTests, FailsForEqualTimestamp) {
  RtpToNtpEstimator estimator;
  uint32_t ntp_sec = 0;
  uint32_t ntp_frac = 2;
  uint32_t timestamp = 0x12345678;
  bool new_sr;
  EXPECT_TRUE(
      estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp, &new_sr));
  EXPECT_TRUE(new_sr);
  // Timestamp already added, list not updated.
  ++ntp_frac;
  EXPECT_TRUE(
      estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp, &new_sr));
  EXPECT_FALSE(new_sr);
}

TEST(UpdateRtcpMeasurementTests, FailsForOldRtpTimestamp) {
  RtpToNtpEstimator estimator;
  uint32_t ntp_sec = 0;
  uint32_t ntp_frac = 2;
  uint32_t timestamp = 0x12345678;
  bool new_sr;
  EXPECT_TRUE(
      estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp, &new_sr));
  EXPECT_TRUE(new_sr);
  // Old timestamp, list not updated.
  ntp_frac += kOneMsInNtpFrac;
  timestamp -= kTimestampTicksPerMs;
  EXPECT_FALSE(
      estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp, &new_sr));
  EXPECT_FALSE(new_sr);
}

TEST(UpdateRtcpMeasurementTests, VerifyParameters) {
  RtpToNtpEstimator estimator;
  uint32_t ntp_sec = 1;
  uint32_t ntp_frac = 2;
  uint32_t timestamp = 0x12345678;
  bool new_sr;
  EXPECT_TRUE(
      estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp, &new_sr));
  EXPECT_TRUE(new_sr);
  EXPECT_FALSE(estimator.params().calculated);
  // Add second report, parameters should be calculated.
  ntp_frac += kOneMsInNtpFrac;
  timestamp += kTimestampTicksPerMs;
  EXPECT_TRUE(
      estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp, &new_sr));
  EXPECT_TRUE(estimator.params().calculated);
  EXPECT_DOUBLE_EQ(90.0, estimator.params().frequency_khz);
  EXPECT_NE(0.0, estimator.params().offset_ms);
}

TEST(RtpToNtpTests, FailsForNoParameters) {
  RtpToNtpEstimator estimator;
  uint32_t ntp_sec = 1;
  uint32_t ntp_frac = 2;
  uint32_t timestamp = 0x12345678;
  bool new_sr;
  EXPECT_TRUE(
      estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp, &new_sr));
  EXPECT_TRUE(new_sr);
  // Parameters are not calculated, conversion of RTP to NTP time should fail.
  EXPECT_FALSE(estimator.params().calculated);
  int64_t timestamp_ms = -1;
  EXPECT_FALSE(estimator.Estimate(timestamp, &timestamp_ms));
}

};  // namespace webrtc
