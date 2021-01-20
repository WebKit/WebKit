/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtcp_packet/extended_reports.h"

#include "rtc_base/random.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/rtcp_packet_parser.h"

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::make_tuple;
using ::testing::SizeIs;
using webrtc::rtcp::ExtendedReports;
using webrtc::rtcp::ReceiveTimeInfo;
using webrtc::rtcp::Rrtr;

namespace webrtc {
// Define comparision operators that shouldn't be needed in production,
// but make testing matches more clear.
namespace rtcp {
bool operator==(const Rrtr& rrtr1, const Rrtr& rrtr2) {
  return rrtr1.ntp() == rrtr2.ntp();
}

bool operator==(const ReceiveTimeInfo& time1, const ReceiveTimeInfo& time2) {
  return time1.ssrc == time2.ssrc && time1.last_rr == time2.last_rr &&
         time1.delay_since_last_rr == time2.delay_since_last_rr;
}
}  // namespace rtcp

namespace {
constexpr uint32_t kSenderSsrc = 0x12345678;
constexpr uint8_t kEmptyPacket[] = {0x80, 207,  0x00, 0x01,
                                    0x12, 0x34, 0x56, 0x78};
}  // namespace

class RtcpPacketExtendedReportsTest : public ::testing::Test {
 public:
  RtcpPacketExtendedReportsTest() : random_(0x123456789) {}

 protected:
  template <typename T>
  T Rand() {
    return random_.Rand<T>();
  }

 private:
  Random random_;
};

template <>
ReceiveTimeInfo RtcpPacketExtendedReportsTest::Rand<ReceiveTimeInfo>() {
  uint32_t ssrc = Rand<uint32_t>();
  uint32_t last_rr = Rand<uint32_t>();
  uint32_t delay_since_last_rr = Rand<uint32_t>();
  return ReceiveTimeInfo(ssrc, last_rr, delay_since_last_rr);
}

template <>
NtpTime RtcpPacketExtendedReportsTest::Rand<NtpTime>() {
  uint32_t secs = Rand<uint32_t>();
  uint32_t frac = Rand<uint32_t>();
  return NtpTime(secs, frac);
}

template <>
Rrtr RtcpPacketExtendedReportsTest::Rand<Rrtr>() {
  Rrtr rrtr;
  rrtr.SetNtp(Rand<NtpTime>());
  return rrtr;
}

TEST_F(RtcpPacketExtendedReportsTest, CreateWithoutReportBlocks) {
  ExtendedReports xr;
  xr.SetSenderSsrc(kSenderSsrc);

  rtc::Buffer packet = xr.Build();

  EXPECT_THAT(make_tuple(packet.data(), packet.size()),
              ElementsAreArray(kEmptyPacket));
}

TEST_F(RtcpPacketExtendedReportsTest, ParseWithoutReportBlocks) {
  ExtendedReports parsed;
  EXPECT_TRUE(test::ParseSinglePacket(kEmptyPacket, &parsed));
  EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
  EXPECT_FALSE(parsed.rrtr());
  EXPECT_FALSE(parsed.dlrr());
}

TEST_F(RtcpPacketExtendedReportsTest, CreateAndParseWithRrtrBlock) {
  const Rrtr kRrtr = Rand<Rrtr>();
  ExtendedReports xr;
  xr.SetSenderSsrc(kSenderSsrc);
  xr.SetRrtr(kRrtr);
  rtc::Buffer packet = xr.Build();

  ExtendedReports mparsed;
  EXPECT_TRUE(test::ParseSinglePacket(packet, &mparsed));
  const ExtendedReports& parsed = mparsed;

  EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
  EXPECT_EQ(kRrtr, parsed.rrtr());
}

TEST_F(RtcpPacketExtendedReportsTest, CreateAndParseWithDlrrWithOneSubBlock) {
  const ReceiveTimeInfo kTimeInfo = Rand<ReceiveTimeInfo>();
  ExtendedReports xr;
  xr.SetSenderSsrc(kSenderSsrc);
  xr.AddDlrrItem(kTimeInfo);

  rtc::Buffer packet = xr.Build();

  ExtendedReports mparsed;
  EXPECT_TRUE(test::ParseSinglePacket(packet, &mparsed));
  const ExtendedReports& parsed = mparsed;

  EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
  EXPECT_THAT(parsed.dlrr().sub_blocks(), ElementsAre(kTimeInfo));
}

TEST_F(RtcpPacketExtendedReportsTest, CreateAndParseWithDlrrWithTwoSubBlocks) {
  const ReceiveTimeInfo kTimeInfo1 = Rand<ReceiveTimeInfo>();
  const ReceiveTimeInfo kTimeInfo2 = Rand<ReceiveTimeInfo>();
  ExtendedReports xr;
  xr.SetSenderSsrc(kSenderSsrc);
  xr.AddDlrrItem(kTimeInfo1);
  xr.AddDlrrItem(kTimeInfo2);

  rtc::Buffer packet = xr.Build();

  ExtendedReports mparsed;
  EXPECT_TRUE(test::ParseSinglePacket(packet, &mparsed));
  const ExtendedReports& parsed = mparsed;

  EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
  EXPECT_THAT(parsed.dlrr().sub_blocks(), ElementsAre(kTimeInfo1, kTimeInfo2));
}

TEST_F(RtcpPacketExtendedReportsTest, CreateLimitsTheNumberOfDlrrSubBlocks) {
  const ReceiveTimeInfo kTimeInfo = Rand<ReceiveTimeInfo>();
  ExtendedReports xr;

  for (size_t i = 0; i < ExtendedReports::kMaxNumberOfDlrrItems; ++i)
    EXPECT_TRUE(xr.AddDlrrItem(kTimeInfo));
  EXPECT_FALSE(xr.AddDlrrItem(kTimeInfo));

  EXPECT_THAT(xr.dlrr().sub_blocks(),
              SizeIs(ExtendedReports::kMaxNumberOfDlrrItems));
}

TEST_F(RtcpPacketExtendedReportsTest, CreateAndParseWithMaximumReportBlocks) {
  const Rrtr kRrtr = Rand<Rrtr>();

  ExtendedReports xr;
  xr.SetSenderSsrc(kSenderSsrc);
  xr.SetRrtr(kRrtr);
  for (size_t i = 0; i < ExtendedReports::kMaxNumberOfDlrrItems; ++i)
    xr.AddDlrrItem(Rand<ReceiveTimeInfo>());

  rtc::Buffer packet = xr.Build();

  ExtendedReports mparsed;
  EXPECT_TRUE(test::ParseSinglePacket(packet, &mparsed));
  const ExtendedReports& parsed = mparsed;

  EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
  EXPECT_EQ(kRrtr, parsed.rrtr());
  EXPECT_THAT(parsed.dlrr().sub_blocks(),
              ElementsAreArray(xr.dlrr().sub_blocks()));
}

}  // namespace webrtc
