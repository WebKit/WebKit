/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/rpsi.h"

#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/rtcp_packet_parser.h"

using testing::ElementsAreArray;
using testing::make_tuple;
using webrtc::rtcp::Rpsi;

namespace webrtc {
namespace {
const uint32_t kSenderSsrc = 0x12345678;
const uint32_t kRemoteSsrc = 0x23456789;
// 10000|01 100001|0 1000011 (7 bits = 1 byte in native string).
const uint64_t kPictureId = 0x106143;
const uint8_t kPayloadType = 100;
// Manually created Rpsi packet matching constants above.
const uint8_t kPacket[] = {0x83, 206,  0x00, 0x04,
                           0x12, 0x34, 0x56, 0x78,
                           0x23, 0x45, 0x67, 0x89,
                             24,  100, 0xc1, 0xc2,
                           0x43,    0,    0,    0};
}  // namespace

TEST(RtcpPacketRpsiTest, Parse) {
  Rpsi mutable_parsed;
  EXPECT_TRUE(test::ParseSinglePacket(kPacket, &mutable_parsed));
  const Rpsi& parsed = mutable_parsed;  // Read values from constant object.

  EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
  EXPECT_EQ(kRemoteSsrc, parsed.media_ssrc());
  EXPECT_EQ(kPayloadType, parsed.payload_type());
  EXPECT_EQ(kPictureId, parsed.picture_id());
}

TEST(RtcpPacketRpsiTest, Create) {
  Rpsi rpsi;
  rpsi.SetSenderSsrc(kSenderSsrc);
  rpsi.SetMediaSsrc(kRemoteSsrc);
  rpsi.SetPayloadType(kPayloadType);
  rpsi.SetPictureId(kPictureId);

  rtc::Buffer packet = rpsi.Build();

  EXPECT_THAT(make_tuple(packet.data(), packet.size()),
              ElementsAreArray(kPacket));
}

TEST(RtcpPacketRpsiTest, ParseFailsOnTooSmallPacket) {
  Rpsi rpsi;
  rpsi.SetSenderSsrc(kSenderSsrc);
  rpsi.SetMediaSsrc(kRemoteSsrc);

  rtc::Buffer packet = rpsi.Build();
  packet[3]--;  // Reduce size field by one word (4 bytes).
  packet.SetSize(packet.size() - 4);

  EXPECT_FALSE(test::ParseSinglePacket(packet, &rpsi));
}

TEST(RtcpPacketRpsiTest, ParseFailsOnFractionalPaddingBytes) {
  Rpsi rpsi;
  rpsi.SetSenderSsrc(kSenderSsrc);
  rpsi.SetMediaSsrc(kRemoteSsrc);
  rpsi.SetPictureId(kPictureId);
  rtc::Buffer packet = rpsi.Build();
  uint8_t* padding_bits = packet.data() + 12;
  uint8_t saved_padding_bits = *padding_bits;
  ASSERT_TRUE(test::ParseSinglePacket(packet, &rpsi));

  for (uint8_t i = 1; i < 8; ++i) {
    *padding_bits = saved_padding_bits + i;
    EXPECT_FALSE(test::ParseSinglePacket(packet, &rpsi));
  }
}

TEST(RtcpPacketRpsiTest, ParseFailsOnTooBigPadding) {
  Rpsi rpsi;
  rpsi.SetSenderSsrc(kSenderSsrc);
  rpsi.SetMediaSsrc(kRemoteSsrc);
  rpsi.SetPictureId(1);  // Small picture id that occupy just 1 byte.
  rtc::Buffer packet = rpsi.Build();
  uint8_t* padding_bits = packet.data() + 12;
  ASSERT_TRUE(test::ParseSinglePacket(packet, &rpsi));

  *padding_bits += 8;
  EXPECT_FALSE(test::ParseSinglePacket(packet, &rpsi));
}

// For raw rpsi packet extract how many bytes are used to store picture_id.
size_t UsedBytes(const rtc::Buffer& packet) {  // Works for small packets only.
  RTC_CHECK_EQ(packet.data()[2], 0);       // Assume packet is small.
  size_t total_rpsi_payload_bytes = 4 * (packet.data()[3] - 2) - 2;
  uint8_t padding_bits = packet.data()[12];
  RTC_CHECK_EQ(padding_bits % 8, 0);
  return total_rpsi_payload_bytes - (padding_bits / 8);
}

TEST(RtcpPacketRpsiTest, WithOneByteNativeString) {
  Rpsi rpsi;
  // 1000001 (7 bits = 1 byte in native string).
  const uint64_t kPictureId = 0x41;
  const uint16_t kNumberOfValidBytes = 1;
  rpsi.SetPictureId(kPictureId);

  rtc::Buffer packet = rpsi.Build();
  EXPECT_EQ(kNumberOfValidBytes, UsedBytes(packet));

  Rpsi parsed;
  EXPECT_TRUE(test::ParseSinglePacket(packet, &parsed));
  EXPECT_EQ(kPictureId, parsed.picture_id());
}

TEST(RtcpPacketRpsiTest, WithTwoByteNativeString) {
  Rpsi rpsi;
  // |1 0000001 (7 bits = 1 byte in native string).
  const uint64_t kPictureId = 0x81;
  const uint16_t kNumberOfValidBytes = 2;
  rpsi.SetPictureId(kPictureId);

  rtc::Buffer packet = rpsi.Build();
  EXPECT_EQ(kNumberOfValidBytes, UsedBytes(packet));

  Rpsi parsed;
  EXPECT_TRUE(test::ParseSinglePacket(packet, &parsed));
  EXPECT_EQ(kPictureId, parsed.picture_id());
}

TEST(RtcpPacketRpsiTest, WithThreeByteNativeString) {
  Rpsi rpsi;
  // 10000|00 100000|0 1000000 (7 bits = 1 byte in native string).
  const uint64_t kPictureId = 0x102040;
  const uint16_t kNumberOfValidBytes = 3;
  rpsi.SetPictureId(kPictureId);

  rtc::Buffer packet = rpsi.Build();
  EXPECT_EQ(kNumberOfValidBytes, UsedBytes(packet));

  Rpsi parsed;
  EXPECT_TRUE(test::ParseSinglePacket(packet, &parsed));
  EXPECT_EQ(kPictureId, parsed.picture_id());
}

TEST(RtcpPacketRpsiTest, WithFourByteNativeString) {
  Rpsi rpsi;
  // 1000|001 00001|01 100001|1 1000010 (7 bits = 1 byte in native string).
  const uint64_t kPictureId = 0x84161C2;
  const uint16_t kNumberOfValidBytes = 4;
  rpsi.SetPictureId(kPictureId);

  rtc::Buffer packet = rpsi.Build();
  EXPECT_EQ(kNumberOfValidBytes, UsedBytes(packet));

  Rpsi parsed;
  EXPECT_TRUE(test::ParseSinglePacket(packet, &parsed));
  EXPECT_EQ(kPictureId, parsed.picture_id());
}

TEST(RtcpPacketRpsiTest, WithMaxPictureId) {
  Rpsi rpsi;
  // 1 1111111| 1111111 1|111111 11|11111 111|1111 1111|111 11111|
  // 11 111111|1 1111111 (7 bits = 1 byte in native string).
  const uint64_t kPictureId = 0xffffffffffffffff;
  const uint16_t kNumberOfValidBytes = 10;
  rpsi.SetPictureId(kPictureId);

  rtc::Buffer packet = rpsi.Build();
  EXPECT_EQ(kNumberOfValidBytes, UsedBytes(packet));

  Rpsi parsed;
  EXPECT_TRUE(test::ParseSinglePacket(packet, &parsed));
  EXPECT_EQ(kPictureId, parsed.picture_id());
}
}  // namespace webrtc
