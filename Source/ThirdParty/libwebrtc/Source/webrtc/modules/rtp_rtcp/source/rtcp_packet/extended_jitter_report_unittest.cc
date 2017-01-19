/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/extended_jitter_report.h"

#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/rtcp_packet_parser.h"

using testing::ElementsAre;
using testing::IsEmpty;
using webrtc::rtcp::ExtendedJitterReport;

namespace webrtc {
namespace {
constexpr uint32_t kJitter1 = 0x11121314;
constexpr uint32_t kJitter2 = 0x22242628;
}  // namespace

TEST(RtcpPacketExtendedJitterReportTest, CreateAndParseWithoutItems) {
  ExtendedJitterReport ij;
  rtc::Buffer raw = ij.Build();

  ExtendedJitterReport parsed;
  EXPECT_TRUE(test::ParseSinglePacket(raw, &parsed));

  EXPECT_THAT(parsed.jitter_values(), IsEmpty());
}

TEST(RtcpPacketExtendedJitterReportTest, CreateAndParseWithOneItem) {
  ExtendedJitterReport ij;
  EXPECT_TRUE(ij.SetJitterValues({kJitter1}));
  rtc::Buffer raw = ij.Build();

  ExtendedJitterReport parsed;
  EXPECT_TRUE(test::ParseSinglePacket(raw, &parsed));

  EXPECT_THAT(parsed.jitter_values(), ElementsAre(kJitter1));
}

TEST(RtcpPacketExtendedJitterReportTest, CreateAndParseWithTwoItems) {
  ExtendedJitterReport ij;
  EXPECT_TRUE(ij.SetJitterValues({kJitter1, kJitter2}));
  rtc::Buffer raw = ij.Build();

  ExtendedJitterReport parsed;
  EXPECT_TRUE(test::ParseSinglePacket(raw, &parsed));

  EXPECT_THAT(parsed.jitter_values(), ElementsAre(kJitter1, kJitter2));
}

TEST(RtcpPacketExtendedJitterReportTest, CreateWithTooManyItems) {
  ExtendedJitterReport ij;
  const int kMaxItems = ExtendedJitterReport::kMaxNumberOfJitterValues;
  EXPECT_FALSE(
      ij.SetJitterValues(std::vector<uint32_t>(kMaxItems + 1, kJitter1)));
  EXPECT_TRUE(ij.SetJitterValues(std::vector<uint32_t>(kMaxItems, kJitter1)));
}

TEST(RtcpPacketExtendedJitterReportTest, ParseFailsWithTooManyItems) {
  ExtendedJitterReport ij;
  ij.SetJitterValues({kJitter1});
  rtc::Buffer raw = ij.Build();
  raw[0]++;  // Damage packet: increase jitter count by 1.
  ExtendedJitterReport parsed;
  EXPECT_FALSE(test::ParseSinglePacket(raw, &parsed));
}

}  // namespace webrtc
