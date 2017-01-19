/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/sli.h"

#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/rtcp_packet_parser.h"

using testing::ElementsAreArray;
using testing::make_tuple;
using webrtc::rtcp::Sli;

namespace webrtc {
namespace {
constexpr uint32_t kSenderSsrc = 0x12345678;
constexpr uint32_t kRemoteSsrc = 0x23456789;

constexpr uint8_t kPictureId = 0x3f;
constexpr uint16_t kFirstMb = 0x1e61;
constexpr uint16_t kNumberOfMb = 0x1a0a;
constexpr uint32_t kSliItem = (static_cast<uint32_t>(kFirstMb) << 19) |
                              (static_cast<uint32_t>(kNumberOfMb) << 6) |
                               static_cast<uint32_t>(kPictureId);

// Manually created Sli packet matching constants above.
constexpr uint8_t kPacket[] = {0x82,  206, 0x00, 0x03,
                               0x12, 0x34, 0x56, 0x78,
                               0x23, 0x45, 0x67, 0x89,
                               (kSliItem >> 24) & 0xff,
                               (kSliItem >> 16) & 0xff,
                               (kSliItem >> 8) & 0xff,
                                kSliItem & 0xff};
}  // namespace

TEST(RtcpPacketSliTest, Create) {
  Sli sli;
  sli.SetSenderSsrc(kSenderSsrc);
  sli.SetMediaSsrc(kRemoteSsrc);
  sli.AddPictureId(kPictureId, kFirstMb, kNumberOfMb);

  rtc::Buffer packet = sli.Build();

  EXPECT_THAT(make_tuple(packet.data(), packet.size()),
              ElementsAreArray(kPacket));
}

TEST(RtcpPacketSliTest, Parse) {
  Sli mutable_parsed;
  EXPECT_TRUE(test::ParseSinglePacket(kPacket, &mutable_parsed));
  const Sli& parsed = mutable_parsed;  // Read values from constant object.

  EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
  EXPECT_EQ(kRemoteSsrc, parsed.media_ssrc());
  EXPECT_EQ(1u, parsed.macroblocks().size());
  EXPECT_EQ(kFirstMb, parsed.macroblocks()[0].first());
  EXPECT_EQ(kNumberOfMb, parsed.macroblocks()[0].number());
  EXPECT_EQ(kPictureId, parsed.macroblocks()[0].picture_id());
}

TEST(RtcpPacketSliTest, ParseFailsOnTooSmallPacket) {
  Sli sli;
  sli.SetSenderSsrc(kSenderSsrc);
  sli.SetMediaSsrc(kRemoteSsrc);
  sli.AddPictureId(kPictureId, kFirstMb, kNumberOfMb);

  rtc::Buffer packet = sli.Build();
  packet[3]--;  // Decrease size by 1 word (4 bytes).
  packet.SetSize(packet.size() - 4);

  EXPECT_FALSE(test::ParseSinglePacket(packet, &sli));
}

}  // namespace webrtc
