/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/extended_reports.h"

#include "webrtc/base/random.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/rtcp_packet_parser.h"

using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::make_tuple;
using webrtc::rtcp::Dlrr;
using webrtc::rtcp::ExtendedReports;
using webrtc::rtcp::ReceiveTimeInfo;
using webrtc::rtcp::Rrtr;
using webrtc::rtcp::VoipMetric;

namespace webrtc {
// Define comparision operators that shouldn't be needed in production,
// but make testing matches more clear.
bool operator==(const RTCPVoIPMetric& metric1, const RTCPVoIPMetric& metric2) {
  return metric1.lossRate == metric2.lossRate &&
         metric1.discardRate == metric2.discardRate &&
         metric1.burstDensity == metric2.burstDensity &&
         metric1.gapDensity == metric2.gapDensity &&
         metric1.burstDuration == metric2.burstDuration &&
         metric1.gapDuration == metric2.gapDuration &&
         metric1.roundTripDelay == metric2.roundTripDelay &&
         metric1.endSystemDelay == metric2.endSystemDelay &&
         metric1.signalLevel == metric2.signalLevel &&
         metric1.noiseLevel == metric2.noiseLevel &&
         metric1.RERL == metric2.RERL &&
         metric1.Gmin == metric2.Gmin &&
         metric1.Rfactor == metric2.Rfactor &&
         metric1.extRfactor == metric2.extRfactor &&
         metric1.MOSLQ == metric2.MOSLQ &&
         metric1.MOSCQ == metric2.MOSCQ &&
         metric1.RXconfig == metric2.RXconfig &&
         metric1.JBnominal == metric2.JBnominal &&
         metric1.JBmax == metric2.JBmax &&
         metric1.JBabsMax == metric2.JBabsMax;
}

namespace rtcp {
bool operator==(const Rrtr& rrtr1, const Rrtr& rrtr2) {
  return rrtr1.ntp() == rrtr2.ntp();
}

bool operator==(const ReceiveTimeInfo& time1, const ReceiveTimeInfo& time2) {
  return time1.ssrc == time2.ssrc &&
         time1.last_rr == time2.last_rr &&
         time1.delay_since_last_rr == time2.delay_since_last_rr;
}

bool operator==(const VoipMetric& metric1, const VoipMetric& metric2) {
  return metric1.ssrc() == metric2.ssrc() &&
         metric1.voip_metric() == metric2.voip_metric();
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

template <>
RTCPVoIPMetric RtcpPacketExtendedReportsTest::Rand<RTCPVoIPMetric>() {
  RTCPVoIPMetric metric;
  metric.lossRate       = Rand<uint8_t>();
  metric.discardRate    = Rand<uint8_t>();
  metric.burstDensity   = Rand<uint8_t>();
  metric.gapDensity     = Rand<uint8_t>();
  metric.burstDuration  = Rand<uint16_t>();
  metric.gapDuration    = Rand<uint16_t>();
  metric.roundTripDelay = Rand<uint16_t>();
  metric.endSystemDelay = Rand<uint16_t>();
  metric.signalLevel    = Rand<uint8_t>();
  metric.noiseLevel     = Rand<uint8_t>();
  metric.RERL           = Rand<uint8_t>();
  metric.Gmin           = Rand<uint8_t>();
  metric.Rfactor        = Rand<uint8_t>();
  metric.extRfactor     = Rand<uint8_t>();
  metric.MOSLQ          = Rand<uint8_t>();
  metric.MOSCQ          = Rand<uint8_t>();
  metric.RXconfig       = Rand<uint8_t>();
  metric.JBnominal      = Rand<uint16_t>();
  metric.JBmax          = Rand<uint16_t>();
  metric.JBabsMax       = Rand<uint16_t>();
  return metric;
}

template <>
VoipMetric RtcpPacketExtendedReportsTest::Rand<VoipMetric>() {
  VoipMetric voip_metric;
  voip_metric.SetMediaSsrc(Rand<uint32_t>());
  voip_metric.SetVoipMetric(Rand<RTCPVoIPMetric>());
  return voip_metric;
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
  EXPECT_FALSE(parsed.voip_metric());
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

TEST_F(RtcpPacketExtendedReportsTest, CreateAndParseWithVoipMetric) {
  const VoipMetric kVoipMetric = Rand<VoipMetric>();

  ExtendedReports xr;
  xr.SetSenderSsrc(kSenderSsrc);
  xr.SetVoipMetric(kVoipMetric);

  rtc::Buffer packet = xr.Build();

  ExtendedReports mparsed;
  EXPECT_TRUE(test::ParseSinglePacket(packet, &mparsed));
  const ExtendedReports& parsed = mparsed;

  EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
  EXPECT_EQ(kVoipMetric, parsed.voip_metric());
}

TEST_F(RtcpPacketExtendedReportsTest, CreateAndParseWithMultipleReportBlocks) {
  const Rrtr kRrtr = Rand<Rrtr>();
  const ReceiveTimeInfo kTimeInfo = Rand<ReceiveTimeInfo>();
  const VoipMetric kVoipMetric = Rand<VoipMetric>();

  ExtendedReports xr;
  xr.SetSenderSsrc(kSenderSsrc);
  xr.SetRrtr(kRrtr);
  xr.AddDlrrItem(kTimeInfo);
  xr.SetVoipMetric(kVoipMetric);

  rtc::Buffer packet = xr.Build();

  ExtendedReports mparsed;
  EXPECT_TRUE(test::ParseSinglePacket(packet, &mparsed));
  const ExtendedReports& parsed = mparsed;

  EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
  EXPECT_EQ(kRrtr, parsed.rrtr());
  EXPECT_THAT(parsed.dlrr().sub_blocks(), ElementsAre(kTimeInfo));
  EXPECT_EQ(kVoipMetric, parsed.voip_metric());
}

}  // namespace webrtc
