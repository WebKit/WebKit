/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/rtp_utils.h"

#include <cstdint>
#include <cstring>

#include "media/base/fake_rtp.h"
#include "test/gtest.h"

namespace webrtc {

namespace {
const uint8_t kInvalidPacket[] = {0x80, 0x00};

// PT = 206, FMT = 1, Sender SSRC  = 0x1111, Media SSRC = 0x1111
// No FCI information is needed for PLI.
const uint8_t kNonCompoundRtcpPliFeedbackPacket[] = {
    0x81, 0xCE, 0x00, 0x0C, 0x00, 0x00, 0x11, 0x11, 0x00, 0x00, 0x11, 0x11};

// Packet has only mandatory fixed RTCP header
// PT = 204, SSRC = 0x1111
const uint8_t kNonCompoundRtcpAppPacket[] = {0x81, 0xCC, 0x00, 0x0C,
                                             0x00, 0x00, 0x11, 0x11};

// PT = 202, Source count = 0
const uint8_t kNonCompoundRtcpSDESPacket[] = {0x80, 0xCA, 0x00, 0x00};

// Valid rtp Message with 2 byte header extension.
uint8_t kRtpMsgWith2ByteExtnHeader[] = {
    // clang-format off
    // clang formatting doesn't respect inline comments.
  0x90, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0xAA, 0xBB, 0xCC, 0XDD,  // SSRC
  0x10, 0x00, 0x00, 0x01,  // 2 Byte header extension
  0x01, 0x00, 0x00, 0x00
    // clang-format on
};

// RTP packet with two one-byte header extensions. The last 4 bytes consist of
// abs-send-time with extension id = 3 and length = 3.
uint8_t kRtpMsgWithOneByteAbsSendTimeExtension[] = {
    0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xBE, 0xDE, 0x00, 0x02, 0x22, 0x00, 0x02, 0x1c, 0x32, 0xaa, 0xbb, 0xcc,
};

// RTP packet with two two-byte header extensions. The last 5 bytes consist of
// abs-send-time with extension id = 3 and length = 3.
uint8_t kRtpMsgWithTwoByteAbsSendTimeExtension[] = {
    0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x10, 0x00, 0x00, 0x02, 0x02, 0x01, 0x02, 0x03, 0x03, 0xaa, 0xbb, 0xcc,
};

}  // namespace

TEST(RtpUtilsTest, GetRtcp) {
  int pt;
  EXPECT_TRUE(GetRtcpType(kFakeRtcpReport, sizeof(kFakeRtcpReport), &pt));
  EXPECT_EQ(0xc9, pt);

  EXPECT_FALSE(GetRtcpType(kInvalidPacket, sizeof(kInvalidPacket), &pt));

  uint32_t ssrc;
  EXPECT_TRUE(GetRtcpSsrc(kNonCompoundRtcpPliFeedbackPacket,
                          sizeof(kNonCompoundRtcpPliFeedbackPacket), &ssrc));
  EXPECT_TRUE(GetRtcpSsrc(kNonCompoundRtcpAppPacket,
                          sizeof(kNonCompoundRtcpAppPacket), &ssrc));
  EXPECT_FALSE(GetRtcpSsrc(kNonCompoundRtcpSDESPacket,
                           sizeof(kNonCompoundRtcpSDESPacket), &ssrc));
}

TEST(RtpUtilsTest, InferRtpPacketType) {
  EXPECT_EQ(RtpPacketType::kRtp, InferRtpPacketType(kPcmuFrame));
  EXPECT_EQ(RtpPacketType::kRtcp, InferRtpPacketType(kFakeRtcpReport));
  EXPECT_EQ(RtpPacketType::kUnknown, InferRtpPacketType(kInvalidPacket));
}

// Invalid RTP packets.
TEST(RtpUtilsTest, InvalidRtpHeader) {
  // Rtp message with invalid length.
  const uint8_t kRtpMsgWithInvalidLength[] = {
      // clang-format off
      // clang formatting doesn't respect inline comments.
      0x94, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0xAA, 0xBB, 0xCC, 0XDD,  // SSRC
      0xDD, 0xCC, 0xBB, 0xAA,  // Only 1 CSRC, but CC count is 4.
      // clang-format on
  };
  EXPECT_FALSE(ValidateRtpHeader(kRtpMsgWithInvalidLength, nullptr));

  // Rtp message with single byte header extension, invalid extension length.
  const uint8_t kRtpMsgWithInvalidExtnLength[] = {
      0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0xBE, 0xDE, 0x0A, 0x00,  // Extn length - 0x0A00
  };
  EXPECT_FALSE(ValidateRtpHeader(kRtpMsgWithInvalidExtnLength,
                                 nullptr));
}

// Valid RTP packet with a 2byte header extension.
TEST(RtpUtilsTest, Valid2ByteExtnHdrRtpMessage) {
  EXPECT_TRUE(ValidateRtpHeader(kRtpMsgWith2ByteExtnHeader, nullptr));
}

// Valid RTP packet which has 1 byte header AbsSendTime extension in it.
TEST(RtpUtilsTest, ValidRtpPacketWithOneByteAbsSendTimeExtension) {
  EXPECT_TRUE(ValidateRtpHeader(kRtpMsgWithOneByteAbsSendTimeExtension,
                                nullptr));
}

// Valid RTP packet which has 2 byte header AbsSendTime extension in it.
TEST(RtpUtilsTest, ValidRtpPacketWithTwoByteAbsSendTimeExtension) {
  EXPECT_TRUE(ValidateRtpHeader(kRtpMsgWithTwoByteAbsSendTimeExtension,
                                nullptr));
}
}  // namespace webrtc
