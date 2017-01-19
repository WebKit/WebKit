/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/remb.h"

#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/rtcp_packet_parser.h"

using testing::ElementsAreArray;
using testing::IsEmpty;
using testing::make_tuple;
using webrtc::rtcp::Remb;

namespace webrtc {
namespace {
const uint32_t kSenderSsrc = 0x12345678;
const uint32_t kRemoteSsrcs[] = {0x23456789, 0x2345678a, 0x2345678b};
const uint32_t kBitrateBps = 0x3fb93 * 2;  // 522022;
const uint64_t kBitrateBps64bit = 0x3fb93ULL << 30;
const uint8_t kPacket[] = {0x8f, 206,  0x00, 0x07, 0x12, 0x34, 0x56, 0x78,
                           0x00, 0x00, 0x00, 0x00, 'R',  'E',  'M',  'B',
                           0x03, 0x07, 0xfb, 0x93, 0x23, 0x45, 0x67, 0x89,
                           0x23, 0x45, 0x67, 0x8a, 0x23, 0x45, 0x67, 0x8b};
const size_t kPacketLength = sizeof(kPacket);
}  // namespace

TEST(RtcpPacketRembTest, Create) {
  Remb remb;
  remb.SetSenderSsrc(kSenderSsrc);
  remb.SetSsrcs(
      std::vector<uint32_t>(std::begin(kRemoteSsrcs), std::end(kRemoteSsrcs)));
  remb.SetBitrateBps(kBitrateBps);

  rtc::Buffer packet = remb.Build();

  EXPECT_THAT(make_tuple(packet.data(), packet.size()),
              ElementsAreArray(kPacket));
}

TEST(RtcpPacketRembTest, Parse) {
  Remb remb;
  EXPECT_TRUE(test::ParseSinglePacket(kPacket, &remb));
  const Remb& parsed = remb;

  EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
  EXPECT_EQ(kBitrateBps, parsed.bitrate_bps());
  EXPECT_THAT(parsed.ssrcs(), ElementsAreArray(kRemoteSsrcs));
}

TEST(RtcpPacketRembTest, CreateAndParseWithoutSsrcs) {
  Remb remb;
  remb.SetSenderSsrc(kSenderSsrc);
  remb.SetBitrateBps(kBitrateBps);
  rtc::Buffer packet = remb.Build();

  Remb parsed;
  EXPECT_TRUE(test::ParseSinglePacket(packet, &parsed));
  EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
  EXPECT_EQ(kBitrateBps, parsed.bitrate_bps());
  EXPECT_THAT(parsed.ssrcs(), IsEmpty());
}

TEST(RtcpPacketRembTest, CreateAndParse64bitBitrate) {
  Remb remb;
  remb.SetBitrateBps(kBitrateBps64bit);
  rtc::Buffer packet = remb.Build();

  Remb parsed;
  EXPECT_TRUE(test::ParseSinglePacket(packet, &parsed));
  EXPECT_EQ(kBitrateBps64bit, parsed.bitrate_bps());
}

TEST(RtcpPacketRembTest, ParseFailsOnTooSmallPacketToBeRemb) {
  // Make it too small.
  constexpr size_t kTooSmallSize = (1 + 3) * 4;
  uint8_t packet[kTooSmallSize];
  memcpy(packet, kPacket, kTooSmallSize);
  packet[3] = 3;

  Remb remb;
  EXPECT_FALSE(test::ParseSinglePacket(packet, &remb));
}

TEST(RtcpPacketRembTest, ParseFailsWhenUniqueIdentifierIsNotRemb) {
  uint8_t packet[kPacketLength];
  memcpy(packet, kPacket, kPacketLength);
  packet[12] = 'N';  // Swap 'R' -> 'N' in the 'REMB' unique identifier.

  Remb remb;
  EXPECT_FALSE(test::ParseSinglePacket(packet, &remb));
}

TEST(RtcpPacketRembTest, ParseFailsWhenBitrateDoNotFitIn64bits) {
  uint8_t packet[kPacketLength];
  memcpy(packet, kPacket, kPacketLength);
  packet[17] |= 0xfc;  // Set exponenta component to maximum of 63.
  packet[19] |= 0x02;  // Ensure mantissa is at least 2.

  Remb remb;
  EXPECT_FALSE(test::ParseSinglePacket(packet, &remb));
}

TEST(RtcpPacketRembTest, ParseFailsWhenSsrcCountMismatchLength) {
  uint8_t packet[kPacketLength];
  memcpy(packet, kPacket, kPacketLength);
  packet[16]++;  // Swap 3 -> 4 in the ssrcs count.

  Remb remb;
  EXPECT_FALSE(test::ParseSinglePacket(packet, &remb));
}

TEST(RtcpPacketRembTest, TooManySsrcs) {
  Remb remb;
  EXPECT_FALSE(remb.SetSsrcs(
      std::vector<uint32_t>(Remb::kMaxNumberOfSsrcs + 1, kRemoteSsrcs[0])));
  EXPECT_TRUE(remb.SetSsrcs(
      std::vector<uint32_t>(Remb::kMaxNumberOfSsrcs, kRemoteSsrcs[0])));
}

}  // namespace webrtc
