/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtcp_packet/voip_metric.h"

#include "test/gtest.h"

namespace webrtc {
namespace rtcp {
namespace {

const uint32_t kRemoteSsrc = 0x23456789;
const uint8_t kBlock[] = {0x07, 0x00, 0x00, 0x08, 0x23, 0x45, 0x67, 0x89,
                          0x01, 0x02, 0x03, 0x04, 0x11, 0x12, 0x22, 0x23,
                          0x33, 0x34, 0x44, 0x45, 0x05, 0x06, 0x07, 0x08,
                          0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x00, 0x55, 0x56,
                          0x66, 0x67, 0x77, 0x78};
const size_t kBlockSizeBytes = sizeof(kBlock);
static_assert(
    kBlockSizeBytes == VoipMetric::kLength,
    "Size of manually created Voip Metric block should match class constant");

TEST(RtcpPacketVoipMetricTest, Create) {
  uint8_t buffer[VoipMetric::kLength];
  RTCPVoIPMetric metric;
  metric.lossRate = 1;
  metric.discardRate = 2;
  metric.burstDensity = 3;
  metric.gapDensity = 4;
  metric.burstDuration = 0x1112;
  metric.gapDuration = 0x2223;
  metric.roundTripDelay = 0x3334;
  metric.endSystemDelay = 0x4445;
  metric.signalLevel = 5;
  metric.noiseLevel = 6;
  metric.RERL = 7;
  metric.Gmin = 8;
  metric.Rfactor = 9;
  metric.extRfactor = 10;
  metric.MOSLQ = 11;
  metric.MOSCQ = 12;
  metric.RXconfig = 13;
  metric.JBnominal = 0x5556;
  metric.JBmax = 0x6667;
  metric.JBabsMax = 0x7778;
  VoipMetric metric_block;
  metric_block.SetMediaSsrc(kRemoteSsrc);
  metric_block.SetVoipMetric(metric);

  metric_block.Create(buffer);
  EXPECT_EQ(0, memcmp(buffer, kBlock, kBlockSizeBytes));
}

TEST(RtcpPacketVoipMetricTest, Parse) {
  VoipMetric read_metric;
  read_metric.Parse(kBlock);

  // Run checks on const object to ensure all accessors have const modifier.
  const VoipMetric& parsed = read_metric;

  EXPECT_EQ(kRemoteSsrc, parsed.ssrc());
  EXPECT_EQ(1, parsed.voip_metric().lossRate);
  EXPECT_EQ(2, parsed.voip_metric().discardRate);
  EXPECT_EQ(3, parsed.voip_metric().burstDensity);
  EXPECT_EQ(4, parsed.voip_metric().gapDensity);
  EXPECT_EQ(0x1112, parsed.voip_metric().burstDuration);
  EXPECT_EQ(0x2223, parsed.voip_metric().gapDuration);
  EXPECT_EQ(0x3334, parsed.voip_metric().roundTripDelay);
  EXPECT_EQ(0x4445, parsed.voip_metric().endSystemDelay);
  EXPECT_EQ(5, parsed.voip_metric().signalLevel);
  EXPECT_EQ(6, parsed.voip_metric().noiseLevel);
  EXPECT_EQ(7, parsed.voip_metric().RERL);
  EXPECT_EQ(8, parsed.voip_metric().Gmin);
  EXPECT_EQ(9, parsed.voip_metric().Rfactor);
  EXPECT_EQ(10, parsed.voip_metric().extRfactor);
  EXPECT_EQ(11, parsed.voip_metric().MOSLQ);
  EXPECT_EQ(12, parsed.voip_metric().MOSCQ);
  EXPECT_EQ(13, parsed.voip_metric().RXconfig);
  EXPECT_EQ(0x5556, parsed.voip_metric().JBnominal);
  EXPECT_EQ(0x6667, parsed.voip_metric().JBmax);
  EXPECT_EQ(0x7778, parsed.voip_metric().JBabsMax);
}

}  // namespace
}  // namespace rtcp
}  // namespace webrtc
