/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/system_wrappers/include/rtp_to_ntp.h"
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
  RtcpMeasurements rtcp;
  bool new_sr;
  uint32_t ntp_sec = 0;
  uint32_t ntp_frac = 1;
  uint32_t timestamp = 0;
  EXPECT_TRUE(UpdateRtcpList(ntp_sec, ntp_frac, timestamp, &rtcp, &new_sr));
  ntp_frac += kOneMsInNtpFrac;
  timestamp -= kTimestampTicksPerMs;
  // Expected to fail since the older RTCP has a smaller RTP timestamp than the
  // newer (old:0, new:4294967206).
  EXPECT_FALSE(UpdateRtcpList(ntp_sec, ntp_frac, timestamp, &rtcp, &new_sr));
}

TEST(WrapAroundTests, NewRtcpWrapped) {
  RtcpMeasurements rtcp;
  bool new_sr;
  uint32_t ntp_sec = 0;
  uint32_t ntp_frac = 1;
  uint32_t timestamp = 0xFFFFFFFF;
  EXPECT_TRUE(UpdateRtcpList(ntp_sec, ntp_frac, timestamp, &rtcp, &new_sr));
  ntp_frac += kOneMsInNtpFrac;
  timestamp += kTimestampTicksPerMs;
  EXPECT_TRUE(UpdateRtcpList(ntp_sec, ntp_frac, timestamp, &rtcp, &new_sr));
  int64_t timestamp_ms = -1;
  EXPECT_TRUE(RtpToNtpMs(rtcp.list.back().rtp_timestamp, rtcp, &timestamp_ms));
  // Since this RTP packet has the same timestamp as the RTCP packet constructed
  // at time 0 it should be mapped to 0 as well.
  EXPECT_EQ(0, timestamp_ms);
}

TEST(WrapAroundTests, RtpWrapped) {
  RtcpMeasurements rtcp;
  bool new_sr;
  uint32_t ntp_sec = 0;
  uint32_t ntp_frac = 1;
  uint32_t timestamp = 0xFFFFFFFF - 2 * kTimestampTicksPerMs;
  EXPECT_TRUE(UpdateRtcpList(ntp_sec, ntp_frac, timestamp, &rtcp, &new_sr));
  ntp_frac += kOneMsInNtpFrac;
  timestamp += kTimestampTicksPerMs;
  EXPECT_TRUE(UpdateRtcpList(ntp_sec, ntp_frac, timestamp, &rtcp, &new_sr));

  int64_t timestamp_ms = -1;
  EXPECT_TRUE(RtpToNtpMs(rtcp.list.back().rtp_timestamp, rtcp, &timestamp_ms));
  // Since this RTP packet has the same timestamp as the RTCP packet constructed
  // at time 0 it should be mapped to 0 as well.
  EXPECT_EQ(0, timestamp_ms);
  // Two kTimestampTicksPerMs advanced.
  timestamp += kTimestampTicksPerMs;
  EXPECT_TRUE(RtpToNtpMs(timestamp, rtcp, &timestamp_ms));
  EXPECT_EQ(2, timestamp_ms);
  // Wrapped rtp.
  timestamp += kTimestampTicksPerMs;
  EXPECT_TRUE(RtpToNtpMs(timestamp, rtcp, &timestamp_ms));
  EXPECT_EQ(3, timestamp_ms);
}

TEST(WrapAroundTests, OldRtp_RtcpsWrapped) {
  RtcpMeasurements rtcp;
  bool new_sr;
  uint32_t ntp_sec = 0;
  uint32_t ntp_frac = 1;
  uint32_t timestamp = 0;
  EXPECT_TRUE(UpdateRtcpList(ntp_sec, ntp_frac, timestamp, &rtcp, &new_sr));
  ntp_frac += kOneMsInNtpFrac;
  timestamp += kTimestampTicksPerMs;
  EXPECT_TRUE(UpdateRtcpList(ntp_sec, ntp_frac, timestamp, &rtcp, &new_sr));
  timestamp -= 2*kTimestampTicksPerMs;
  int64_t timestamp_ms = -1;
  EXPECT_FALSE(RtpToNtpMs(timestamp, rtcp, &timestamp_ms));
}

TEST(WrapAroundTests, OldRtp_NewRtcpWrapped) {
  RtcpMeasurements rtcp;
  bool new_sr;
  uint32_t ntp_sec = 0;
  uint32_t ntp_frac = 1;
  uint32_t timestamp = 0xFFFFFFFF;
  EXPECT_TRUE(UpdateRtcpList(ntp_sec, ntp_frac, timestamp, &rtcp, &new_sr));
  ntp_frac += kOneMsInNtpFrac;
  timestamp += kTimestampTicksPerMs;
  EXPECT_TRUE(UpdateRtcpList(ntp_sec, ntp_frac, timestamp, &rtcp, &new_sr));
  timestamp -= kTimestampTicksPerMs;
  int64_t timestamp_ms = -1;
  EXPECT_TRUE(RtpToNtpMs(timestamp, rtcp, &timestamp_ms));
  // Constructed at the same time as the first RTCP and should therefore be
  // mapped to zero.
  EXPECT_EQ(0, timestamp_ms);
}

TEST(UpdateRtcpListTests, InjectRtcpSr) {
  const uint32_t kNtpSec = 10;
  const uint32_t kNtpFrac = 12345;
  const uint32_t kTs = 0x12345678;
  bool new_sr;
  RtcpMeasurements rtcp;
  EXPECT_TRUE(UpdateRtcpList(kNtpSec, kNtpFrac, kTs, &rtcp, &new_sr));
  EXPECT_TRUE(new_sr);
  EXPECT_EQ(1u, rtcp.list.size());
  EXPECT_EQ(kNtpSec, rtcp.list.front().ntp_secs);
  EXPECT_EQ(kNtpFrac, rtcp.list.front().ntp_frac);
  EXPECT_EQ(kTs, rtcp.list.front().rtp_timestamp);
  // Add second report.
  EXPECT_TRUE(UpdateRtcpList(kNtpSec, kNtpFrac + kOneMsInNtpFrac, kTs + 1,
                             &rtcp, &new_sr));
  EXPECT_EQ(2u, rtcp.list.size());
  EXPECT_EQ(kTs + 1, rtcp.list.front().rtp_timestamp);
  EXPECT_EQ(kTs + 0, rtcp.list.back().rtp_timestamp);
  // List should contain last two reports.
  EXPECT_TRUE(UpdateRtcpList(kNtpSec, kNtpFrac + 2 * kOneMsInNtpFrac, kTs + 2,
                             &rtcp, &new_sr));
  EXPECT_EQ(2u, rtcp.list.size());
  EXPECT_EQ(kTs + 2, rtcp.list.front().rtp_timestamp);
  EXPECT_EQ(kTs + 1, rtcp.list.back().rtp_timestamp);
}

TEST(UpdateRtcpListTests, FailsForZeroNtp) {
  RtcpMeasurements rtcp;
  uint32_t ntp_sec = 0;
  uint32_t ntp_frac = 0;
  uint32_t timestamp = 0x12345678;
  bool new_sr;
  EXPECT_FALSE(UpdateRtcpList(ntp_sec, ntp_frac, timestamp, &rtcp, &new_sr));
  EXPECT_FALSE(new_sr);
  EXPECT_EQ(0u, rtcp.list.size());
}

TEST(UpdateRtcpListTests, FailsForEqualNtp) {
  RtcpMeasurements rtcp;
  uint32_t ntp_sec = 0;
  uint32_t ntp_frac = 699925050;
  uint32_t timestamp = 0x12345678;
  bool new_sr;
  EXPECT_TRUE(UpdateRtcpList(ntp_sec, ntp_frac, timestamp, &rtcp, &new_sr));
  EXPECT_TRUE(new_sr);
  EXPECT_EQ(1u, rtcp.list.size());
  // Ntp time already added, list not updated.
  ++timestamp;
  EXPECT_TRUE(UpdateRtcpList(ntp_sec, ntp_frac, timestamp, &rtcp, &new_sr));
  EXPECT_FALSE(new_sr);
  EXPECT_EQ(1u, rtcp.list.size());
}

TEST(UpdateRtcpListTests, FailsForOldNtp) {
  RtcpMeasurements rtcp;
  uint32_t ntp_sec = 1;
  uint32_t ntp_frac = 699925050;
  uint32_t timestamp = 0x12345678;
  bool new_sr;
  EXPECT_TRUE(UpdateRtcpList(ntp_sec, ntp_frac, timestamp, &rtcp, &new_sr));
  EXPECT_TRUE(new_sr);
  EXPECT_EQ(1u, rtcp.list.size());
  // Old ntp time, list not updated.
  ntp_frac -= kOneMsInNtpFrac;
  timestamp += kTimestampTicksPerMs;
  EXPECT_FALSE(UpdateRtcpList(ntp_sec, ntp_frac, timestamp, &rtcp, &new_sr));
  EXPECT_EQ(1u, rtcp.list.size());
}

TEST(UpdateRtcpListTests, FailsForEqualTimestamp) {
  RtcpMeasurements rtcp;
  uint32_t ntp_sec = 0;
  uint32_t ntp_frac = 2;
  uint32_t timestamp = 0x12345678;
  bool new_sr;
  EXPECT_TRUE(UpdateRtcpList(ntp_sec, ntp_frac, timestamp, &rtcp, &new_sr));
  EXPECT_TRUE(new_sr);
  EXPECT_EQ(1u, rtcp.list.size());
  // Timestamp already added, list not updated.
  ++ntp_frac;
  EXPECT_TRUE(UpdateRtcpList(ntp_sec, ntp_frac, timestamp, &rtcp, &new_sr));
  EXPECT_FALSE(new_sr);
  EXPECT_EQ(1u, rtcp.list.size());
}

TEST(UpdateRtcpListTests, FailsForOldRtpTimestamp) {
  RtcpMeasurements rtcp;
  uint32_t ntp_sec = 0;
  uint32_t ntp_frac = 2;
  uint32_t timestamp = 0x12345678;
  bool new_sr;
  EXPECT_TRUE(UpdateRtcpList(ntp_sec, ntp_frac, timestamp, &rtcp, &new_sr));
  EXPECT_TRUE(new_sr);
  EXPECT_EQ(1u, rtcp.list.size());
  // Old timestamp, list not updated.
  ntp_frac += kOneMsInNtpFrac;
  timestamp -= kTimestampTicksPerMs;
  EXPECT_FALSE(UpdateRtcpList(ntp_sec, ntp_frac, timestamp, &rtcp, &new_sr));
  EXPECT_FALSE(new_sr);
  EXPECT_EQ(1u, rtcp.list.size());
}

TEST(UpdateRtcpListTests, VerifyParameters) {
  RtcpMeasurements rtcp;
  uint32_t ntp_sec = 1;
  uint32_t ntp_frac = 2;
  uint32_t timestamp = 0x12345678;
  bool new_sr;
  EXPECT_TRUE(UpdateRtcpList(ntp_sec, ntp_frac, timestamp, &rtcp, &new_sr));
  EXPECT_TRUE(new_sr);
  EXPECT_FALSE(rtcp.params.calculated);
  // Add second report, parameters should be calculated.
  ntp_frac += kOneMsInNtpFrac;
  timestamp += kTimestampTicksPerMs;
  EXPECT_TRUE(UpdateRtcpList(ntp_sec, ntp_frac, timestamp, &rtcp, &new_sr));
  EXPECT_TRUE(rtcp.params.calculated);
  EXPECT_DOUBLE_EQ(90.0, rtcp.params.frequency_khz);
  EXPECT_NE(0.0, rtcp.params.offset_ms);
}

TEST(RtpToNtpTests, FailsForEmptyList) {
  RtcpMeasurements rtcp;
  rtcp.params.calculated = true;
  // List is empty, conversion of RTP to NTP time should fail.
  EXPECT_EQ(0u, rtcp.list.size());
  int64_t timestamp_ms = -1;
  EXPECT_FALSE(RtpToNtpMs(0, rtcp, &timestamp_ms));
}

TEST(RtpToNtpTests, FailsForNoParameters) {
  RtcpMeasurements rtcp;
  uint32_t ntp_sec = 1;
  uint32_t ntp_frac = 2;
  uint32_t timestamp = 0x12345678;
  bool new_sr;
  EXPECT_TRUE(UpdateRtcpList(ntp_sec, ntp_frac, timestamp, &rtcp, &new_sr));
  EXPECT_EQ(1u, rtcp.list.size());
  // Parameters are not calculated, conversion of RTP to NTP time should fail.
  EXPECT_FALSE(rtcp.params.calculated);
  int64_t timestamp_ms = -1;
  EXPECT_FALSE(RtpToNtpMs(timestamp, rtcp, &timestamp_ms));
}

};  // namespace webrtc
