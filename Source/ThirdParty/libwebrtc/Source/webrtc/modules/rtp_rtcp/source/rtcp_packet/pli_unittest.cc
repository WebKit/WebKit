/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtcp_packet/pli.h"

#include "test/gmock.h"
#include "test/gtest.h"
#include "test/rtcp_packet_parser.h"

using testing::ElementsAreArray;
using testing::make_tuple;
using webrtc::rtcp::Pli;

namespace webrtc {
namespace {
const uint32_t kSenderSsrc = 0x12345678;
const uint32_t kRemoteSsrc = 0x23456789;
// Manually created Pli packet matching constants above.
const uint8_t kPacket[] = {0x81, 206,  0x00, 0x02, 0x12, 0x34,
                           0x56, 0x78, 0x23, 0x45, 0x67, 0x89};
}  // namespace

TEST(RtcpPacketPliTest, Parse) {
  Pli mutable_parsed;
  EXPECT_TRUE(test::ParseSinglePacket(kPacket, &mutable_parsed));
  const Pli& parsed = mutable_parsed;  // Read values from constant object.

  EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
  EXPECT_EQ(kRemoteSsrc, parsed.media_ssrc());
}

TEST(RtcpPacketPliTest, Create) {
  Pli pli;
  pli.SetSenderSsrc(kSenderSsrc);
  pli.SetMediaSsrc(kRemoteSsrc);

  rtc::Buffer packet = pli.Build();

  EXPECT_THAT(make_tuple(packet.data(), packet.size()),
              ElementsAreArray(kPacket));
}

TEST(RtcpPacketPliTest, ParseFailsOnTooSmallPacket) {
  const uint8_t kTooSmallPacket[] = {0x81, 206,  0x00, 0x01,
                                     0x12, 0x34, 0x56, 0x78};

  Pli parsed;
  EXPECT_FALSE(test::ParseSinglePacket(kTooSmallPacket, &parsed));
}

}  // namespace webrtc
