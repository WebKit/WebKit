/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/sender_report.h"

#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/rtcp_packet_parser.h"

using testing::ElementsAreArray;
using testing::make_tuple;
using webrtc::rtcp::ReportBlock;
using webrtc::rtcp::SenderReport;

namespace webrtc {
namespace {
const uint32_t kSenderSsrc = 0x12345678;
const uint32_t kRemoteSsrc = 0x23456789;
const NtpTime kNtp(0x11121418, 0x22242628);
const uint32_t kRtpTimestamp = 0x33343536;
const uint32_t kPacketCount = 0x44454647;
const uint32_t kOctetCount = 0x55565758;
const uint8_t kPacket[] = {0x80,  200, 0x00, 0x06,
                           0x12, 0x34, 0x56, 0x78,
                           0x11, 0x12, 0x14, 0x18,
                           0x22, 0x24, 0x26, 0x28,
                           0x33, 0x34, 0x35, 0x36,
                           0x44, 0x45, 0x46, 0x47,
                           0x55, 0x56, 0x57, 0x58};
}  // namespace

TEST(RtcpPacketSenderReportTest, CreateWithoutReportBlocks) {
  SenderReport sr;
  sr.SetSenderSsrc(kSenderSsrc);
  sr.SetNtp(kNtp);
  sr.SetRtpTimestamp(kRtpTimestamp);
  sr.SetPacketCount(kPacketCount);
  sr.SetOctetCount(kOctetCount);

  rtc::Buffer raw = sr.Build();
  EXPECT_THAT(make_tuple(raw.data(), raw.size()), ElementsAreArray(kPacket));
}

TEST(RtcpPacketSenderReportTest, ParseWithoutReportBlocks) {
  SenderReport parsed;
  EXPECT_TRUE(test::ParseSinglePacket(kPacket, &parsed));

  EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
  EXPECT_EQ(kNtp, parsed.ntp());
  EXPECT_EQ(kRtpTimestamp, parsed.rtp_timestamp());
  EXPECT_EQ(kPacketCount, parsed.sender_packet_count());
  EXPECT_EQ(kOctetCount, parsed.sender_octet_count());
  EXPECT_TRUE(parsed.report_blocks().empty());
}

TEST(RtcpPacketSenderReportTest, CreateAndParseWithOneReportBlock) {
  ReportBlock rb;
  rb.SetMediaSsrc(kRemoteSsrc);

  SenderReport sr;
  sr.SetSenderSsrc(kSenderSsrc);
  EXPECT_TRUE(sr.AddReportBlock(rb));

  rtc::Buffer raw = sr.Build();
  SenderReport parsed;
  EXPECT_TRUE(test::ParseSinglePacket(raw, &parsed));

  EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
  EXPECT_EQ(1u, parsed.report_blocks().size());
  EXPECT_EQ(kRemoteSsrc, parsed.report_blocks()[0].source_ssrc());
}

TEST(RtcpPacketSenderReportTest, CreateAndParseWithTwoReportBlocks) {
  ReportBlock rb1;
  rb1.SetMediaSsrc(kRemoteSsrc);
  ReportBlock rb2;
  rb2.SetMediaSsrc(kRemoteSsrc + 1);

  SenderReport sr;
  sr.SetSenderSsrc(kSenderSsrc);
  EXPECT_TRUE(sr.AddReportBlock(rb1));
  EXPECT_TRUE(sr.AddReportBlock(rb2));

  rtc::Buffer raw = sr.Build();
  SenderReport parsed;
  EXPECT_TRUE(test::ParseSinglePacket(raw, &parsed));

  EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
  EXPECT_EQ(2u, parsed.report_blocks().size());
  EXPECT_EQ(kRemoteSsrc, parsed.report_blocks()[0].source_ssrc());
  EXPECT_EQ(kRemoteSsrc + 1, parsed.report_blocks()[1].source_ssrc());
}

TEST(RtcpPacketSenderReportTest, CreateWithTooManyReportBlocks) {
  SenderReport sr;
  sr.SetSenderSsrc(kSenderSsrc);
  const size_t kMaxReportBlocks = (1 << 5) - 1;
  ReportBlock rb;
  for (size_t i = 0; i < kMaxReportBlocks; ++i) {
    rb.SetMediaSsrc(kRemoteSsrc + i);
    EXPECT_TRUE(sr.AddReportBlock(rb));
  }
  rb.SetMediaSsrc(kRemoteSsrc + kMaxReportBlocks);
  EXPECT_FALSE(sr.AddReportBlock(rb));
}

}  // namespace webrtc
