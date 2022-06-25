/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtcp_packet/report_block.h"

#include <limits>

#include "rtc_base/random.h"
#include "test/gtest.h"

using webrtc::rtcp::ReportBlock;

namespace webrtc {
namespace {

const uint32_t kRemoteSsrc = 0x23456789;
const uint8_t kFractionLost = 55;
// Use values that are streamed differently LE and BE.
const int32_t kCumulativeLost = 0x111213;
const uint32_t kExtHighestSeqNum = 0x22232425;
const uint32_t kJitter = 0x33343536;
const uint32_t kLastSr = 0x44454647;
const uint32_t kDelayLastSr = 0x55565758;
const size_t kBufferLength = ReportBlock::kLength;

TEST(RtcpPacketReportBlockTest, ParseChecksLength) {
  uint8_t buffer[kBufferLength];
  memset(buffer, 0, sizeof(buffer));

  ReportBlock rb;
  EXPECT_FALSE(rb.Parse(buffer, kBufferLength - 1));
  EXPECT_TRUE(rb.Parse(buffer, kBufferLength));
}

TEST(RtcpPacketReportBlockTest, ParseAnyData) {
  uint8_t buffer[kBufferLength];
  // Fill buffer with semi-random data.
  Random generator(0x256F8A285EC829ull);
  for (size_t i = 0; i < kBufferLength; ++i)
    buffer[i] = static_cast<uint8_t>(generator.Rand(0, 0xff));

  ReportBlock rb;
  EXPECT_TRUE(rb.Parse(buffer, kBufferLength));
}

TEST(RtcpPacketReportBlockTest, ParseMatchCreate) {
  ReportBlock rb;
  rb.SetMediaSsrc(kRemoteSsrc);
  rb.SetFractionLost(kFractionLost);
  rb.SetCumulativeLost(kCumulativeLost);
  rb.SetExtHighestSeqNum(kExtHighestSeqNum);
  rb.SetJitter(kJitter);
  rb.SetLastSr(kLastSr);
  rb.SetDelayLastSr(kDelayLastSr);

  uint8_t buffer[kBufferLength];
  rb.Create(buffer);

  ReportBlock parsed;
  EXPECT_TRUE(parsed.Parse(buffer, kBufferLength));

  EXPECT_EQ(kRemoteSsrc, parsed.source_ssrc());
  EXPECT_EQ(kFractionLost, parsed.fraction_lost());
  EXPECT_EQ(kCumulativeLost, parsed.cumulative_lost_signed());
  EXPECT_EQ(kExtHighestSeqNum, parsed.extended_high_seq_num());
  EXPECT_EQ(kJitter, parsed.jitter());
  EXPECT_EQ(kLastSr, parsed.last_sr());
  EXPECT_EQ(kDelayLastSr, parsed.delay_since_last_sr());
}

TEST(RtcpPacketReportBlockTest, ValidateCumulativeLost) {
  // CumulativeLost is a signed 24-bit integer.
  // However, existing code expects it to be an unsigned integer.
  // The case of negative values should be unusual; we return 0
  // when caller wants an unsigned integer.
  const int32_t kMaxCumulativeLost = 0x7fffff;
  const int32_t kMinCumulativeLost = -0x800000;
  ReportBlock rb;
  EXPECT_FALSE(rb.SetCumulativeLost(kMaxCumulativeLost + 1));
  EXPECT_TRUE(rb.SetCumulativeLost(kMaxCumulativeLost));
  EXPECT_FALSE(rb.SetCumulativeLost(kMinCumulativeLost - 1));
  EXPECT_TRUE(rb.SetCumulativeLost(kMinCumulativeLost));
  EXPECT_EQ(kMinCumulativeLost, rb.cumulative_lost_signed());
  EXPECT_EQ(0u, rb.cumulative_lost());
}

TEST(RtcpPacketReportBlockTest, ParseNegativeCumulativeLost) {
  // CumulativeLost is a signed 24-bit integer.
  const int32_t kNegativeCumulativeLost = -123;
  ReportBlock rb;
  EXPECT_TRUE(rb.SetCumulativeLost(kNegativeCumulativeLost));

  uint8_t buffer[kBufferLength];
  rb.Create(buffer);

  ReportBlock parsed;
  EXPECT_TRUE(parsed.Parse(buffer, kBufferLength));

  EXPECT_EQ(kNegativeCumulativeLost, parsed.cumulative_lost_signed());
}

}  // namespace
}  // namespace webrtc
