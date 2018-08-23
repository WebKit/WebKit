/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtcp_packet/fir.h"

#include "test/gmock.h"
#include "test/gtest.h"
#include "test/rtcp_packet_parser.h"

using testing::AllOf;
using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::Eq;
using testing::Field;
using testing::make_tuple;
using webrtc::rtcp::Fir;

namespace webrtc {
namespace {

constexpr uint32_t kSenderSsrc = 0x12345678;
constexpr uint32_t kRemoteSsrc = 0x23456789;
constexpr uint8_t kSeqNr = 13;
// Manually created Fir packet matching constants above.
constexpr uint8_t kPacket[] = {0x84, 206,  0x00, 0x04, 0x12, 0x34, 0x56,
                               0x78, 0x00, 0x00, 0x00, 0x00, 0x23, 0x45,
                               0x67, 0x89, 0x0d, 0x00, 0x00, 0x00};
}  // namespace

TEST(RtcpPacketFirTest, Parse) {
  Fir mutable_parsed;
  EXPECT_TRUE(test::ParseSinglePacket(kPacket, &mutable_parsed));
  const Fir& parsed = mutable_parsed;  // Read values from constant object.

  EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
  EXPECT_THAT(parsed.requests(),
              ElementsAre(AllOf(Field(&Fir::Request::ssrc, Eq(kRemoteSsrc)),
                                Field(&Fir::Request::seq_nr, Eq(kSeqNr)))));
}

TEST(RtcpPacketFirTest, Create) {
  Fir fir;
  fir.SetSenderSsrc(kSenderSsrc);
  fir.AddRequestTo(kRemoteSsrc, kSeqNr);

  rtc::Buffer packet = fir.Build();

  EXPECT_THAT(make_tuple(packet.data(), packet.size()),
              ElementsAreArray(kPacket));
}

TEST(RtcpPacketFirTest, TwoFciEntries) {
  Fir fir;
  fir.SetSenderSsrc(kSenderSsrc);
  fir.AddRequestTo(kRemoteSsrc, kSeqNr);
  fir.AddRequestTo(kRemoteSsrc + 1, kSeqNr + 1);

  rtc::Buffer packet = fir.Build();
  Fir parsed;
  EXPECT_TRUE(test::ParseSinglePacket(packet, &parsed));

  EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
  EXPECT_THAT(parsed.requests(),
              ElementsAre(AllOf(Field(&Fir::Request::ssrc, Eq(kRemoteSsrc)),
                                Field(&Fir::Request::seq_nr, Eq(kSeqNr))),
                          AllOf(Field(&Fir::Request::ssrc, Eq(kRemoteSsrc + 1)),
                                Field(&Fir::Request::seq_nr, Eq(kSeqNr + 1)))));
}

TEST(RtcpPacketFirTest, ParseFailsOnZeroFciEntries) {
  constexpr uint8_t kPacketWithoutFci[] = {0x84, 206,  0x00, 0x02, 0x12, 0x34,
                                           0x56, 0x78, 0x00, 0x00, 0x00, 0x00};
  Fir parsed;
  EXPECT_FALSE(test::ParseSinglePacket(kPacketWithoutFci, &parsed));
}

TEST(RtcpPacketFirTest, ParseFailsOnFractionalFciEntries) {
  constexpr uint8_t kPacketWithOneAndHalfFci[] = {
      0x84, 206,  0x00, 0x05, 0x12, 0x34, 0x56, 0x78, 0x00, 0x00, 0x00, 0x00,
      0x23, 0x45, 0x67, 0x89, 0x0d, 0x00, 0x00, 0x00, 'h',  'a',  'l',  'f'};

  Fir parsed;
  EXPECT_FALSE(test::ParseSinglePacket(kPacketWithOneAndHalfFci, &parsed));
}

}  // namespace webrtc
