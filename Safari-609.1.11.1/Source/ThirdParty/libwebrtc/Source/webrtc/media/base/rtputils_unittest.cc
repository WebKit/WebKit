/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <vector>

#include "media/base/fakertp.h"
#include "media/base/rtputils.h"
#include "rtc_base/asyncpacketsocket.h"
#include "rtc_base/gunit.h"

namespace cricket {

static const uint8_t kRtpPacketWithMarker[] = {
    0x80, 0x80, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
// 3 CSRCs (0x01020304, 0x12345678, 0xAABBCCDD)
// Extension (0xBEDE, 0x1122334455667788)
static const uint8_t kRtpPacketWithMarkerAndCsrcAndExtension[] = {
    0x93, 0x80, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x01, 0x02, 0x03, 0x04, 0x12, 0x34, 0x56, 0x78, 0xAA, 0xBB, 0xCC, 0xDD,
    0xBE, 0xDE, 0x00, 0x02, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
static const uint8_t kInvalidPacket[] = {0x80, 0x00};
static const uint8_t kInvalidPacketWithCsrc[] = {
    0x83, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x01, 0x02, 0x03, 0x04, 0x12, 0x34, 0x56, 0x78, 0xAA, 0xBB, 0xCC};
static const uint8_t kInvalidPacketWithCsrcAndExtension1[] = {
    0x93, 0x80, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x01, 0x02, 0x03, 0x04, 0x12, 0x34,
    0x56, 0x78, 0xAA, 0xBB, 0xCC, 0xDD, 0xBE, 0xDE, 0x00};
static const uint8_t kInvalidPacketWithCsrcAndExtension2[] = {
    0x93, 0x80, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x01, 0x02, 0x03, 0x04, 0x12, 0x34, 0x56, 0x78, 0xAA, 0xBB, 0xCC, 0xDD,
    0xBE, 0xDE, 0x00, 0x02, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};

// PT = 206, FMT = 1, Sender SSRC  = 0x1111, Media SSRC = 0x1111
// No FCI information is needed for PLI.
static const uint8_t kNonCompoundRtcpPliFeedbackPacket[] = {
    0x81, 0xCE, 0x00, 0x0C, 0x00, 0x00, 0x11, 0x11, 0x00, 0x00, 0x11, 0x11};

// Packet has only mandatory fixed RTCP header
// PT = 204, SSRC = 0x1111
static const uint8_t kNonCompoundRtcpAppPacket[] = {0x81, 0xCC, 0x00, 0x0C,
                                                    0x00, 0x00, 0x11, 0x11};

// PT = 202, Source count = 0
static const uint8_t kNonCompoundRtcpSDESPacket[] = {0x80, 0xCA, 0x00, 0x00};

static uint8_t kFakeTag[4] = {0xba, 0xdd, 0xba, 0xdd};
static uint8_t kTestKey[] = "12345678901234567890";
static uint8_t kTestAstValue[3] = {0xaa, 0xbb, 0xcc};

// Valid rtp Message with 2 byte header extension.
static uint8_t kRtpMsgWith2ByteExtnHeader[] = {
    // clang-format off
    // clang formatting doesn't respect inline comments.
  0x90, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0xAA, 0xBB, 0xCC, 0XDD,  // SSRC
  0x10, 0x00, 0x00, 0x01,  // 2 Byte header extension
  0x01, 0x00, 0x00, 0x00
    // clang-format on
};

// RTP packet with single byte extension header of length 4 bytes.
// Extension id = 3 and length = 3
static uint8_t kRtpMsgWithAbsSendTimeExtension[] = {
    0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xBE, 0xDE, 0x00, 0x02, 0x22, 0x00, 0x02, 0x1c, 0x32, 0xaa, 0xbb, 0xcc,
};

// Index of AbsSendTimeExtn data in message |kRtpMsgWithAbsSendTimeExtension|.
static const int kAstIndexInRtpMsg = 21;

TEST(RtpUtilsTest, GetRtp) {
  EXPECT_TRUE(IsRtpPacket(kPcmuFrame, sizeof(kPcmuFrame)));

  int pt;
  EXPECT_TRUE(GetRtpPayloadType(kPcmuFrame, sizeof(kPcmuFrame), &pt));
  EXPECT_EQ(0, pt);
  EXPECT_TRUE(GetRtpPayloadType(kRtpPacketWithMarker,
                                sizeof(kRtpPacketWithMarker), &pt));
  EXPECT_EQ(0, pt);

  int seq_num;
  EXPECT_TRUE(GetRtpSeqNum(kPcmuFrame, sizeof(kPcmuFrame), &seq_num));
  EXPECT_EQ(1, seq_num);

  uint32_t ts;
  EXPECT_TRUE(GetRtpTimestamp(kPcmuFrame, sizeof(kPcmuFrame), &ts));
  EXPECT_EQ(0u, ts);

  uint32_t ssrc;
  EXPECT_TRUE(GetRtpSsrc(kPcmuFrame, sizeof(kPcmuFrame), &ssrc));
  EXPECT_EQ(1u, ssrc);

  RtpHeader header;
  EXPECT_TRUE(GetRtpHeader(kPcmuFrame, sizeof(kPcmuFrame), &header));
  EXPECT_EQ(0, header.payload_type);
  EXPECT_EQ(1, header.seq_num);
  EXPECT_EQ(0u, header.timestamp);
  EXPECT_EQ(1u, header.ssrc);

  EXPECT_FALSE(GetRtpPayloadType(kInvalidPacket, sizeof(kInvalidPacket), &pt));
  EXPECT_FALSE(GetRtpSeqNum(kInvalidPacket, sizeof(kInvalidPacket), &seq_num));
  EXPECT_FALSE(GetRtpTimestamp(kInvalidPacket, sizeof(kInvalidPacket), &ts));
  EXPECT_FALSE(GetRtpSsrc(kInvalidPacket, sizeof(kInvalidPacket), &ssrc));
}

TEST(RtpUtilsTest, SetRtpHeader) {
  uint8_t packet[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  RtpHeader header = {9, 1111, 2222u, 3333u};
  EXPECT_TRUE(SetRtpHeader(packet, sizeof(packet), header));

  // Bits: 10 0 0 0000
  EXPECT_EQ(128u, packet[0]);
  size_t len;
  EXPECT_TRUE(GetRtpHeaderLen(packet, sizeof(packet), &len));
  EXPECT_EQ(12U, len);
  EXPECT_TRUE(GetRtpHeader(packet, sizeof(packet), &header));
  EXPECT_EQ(9, header.payload_type);
  EXPECT_EQ(1111, header.seq_num);
  EXPECT_EQ(2222u, header.timestamp);
  EXPECT_EQ(3333u, header.ssrc);
}

TEST(RtpUtilsTest, GetRtpHeaderLen) {
  size_t len;
  EXPECT_TRUE(GetRtpHeaderLen(kPcmuFrame, sizeof(kPcmuFrame), &len));
  EXPECT_EQ(12U, len);

  EXPECT_TRUE(GetRtpHeaderLen(kRtpPacketWithMarkerAndCsrcAndExtension,
                              sizeof(kRtpPacketWithMarkerAndCsrcAndExtension),
                              &len));
  EXPECT_EQ(sizeof(kRtpPacketWithMarkerAndCsrcAndExtension), len);

  EXPECT_FALSE(GetRtpHeaderLen(kInvalidPacket, sizeof(kInvalidPacket), &len));
  EXPECT_FALSE(GetRtpHeaderLen(kInvalidPacketWithCsrc,
                               sizeof(kInvalidPacketWithCsrc), &len));
  EXPECT_FALSE(GetRtpHeaderLen(kInvalidPacketWithCsrcAndExtension1,
                               sizeof(kInvalidPacketWithCsrcAndExtension1),
                               &len));
  EXPECT_FALSE(GetRtpHeaderLen(kInvalidPacketWithCsrcAndExtension2,
                               sizeof(kInvalidPacketWithCsrcAndExtension2),
                               &len));
}

TEST(RtpUtilsTest, GetRtcp) {
  int pt;
  EXPECT_TRUE(GetRtcpType(kRtcpReport, sizeof(kRtcpReport), &pt));
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
  EXPECT_FALSE(ValidateRtpHeader(kRtpMsgWithInvalidLength,
                                 sizeof(kRtpMsgWithInvalidLength), nullptr));

  // Rtp message with single byte header extension, invalid extension length.
  const uint8_t kRtpMsgWithInvalidExtnLength[] = {
      0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0xBE, 0xDE, 0x0A, 0x00,  // Extn length - 0x0A00
  };
  EXPECT_FALSE(ValidateRtpHeader(kRtpMsgWithInvalidExtnLength,
                                 sizeof(kRtpMsgWithInvalidExtnLength),
                                 nullptr));
}

// Valid RTP packet with a 2byte header extension.
TEST(RtpUtilsTest, Valid2ByteExtnHdrRtpMessage) {
  EXPECT_TRUE(ValidateRtpHeader(kRtpMsgWith2ByteExtnHeader,
                                sizeof(kRtpMsgWith2ByteExtnHeader), nullptr));
}

// Valid RTP packet which has 1 byte header AbsSendTime extension in it.
TEST(RtpUtilsTest, ValidRtpPacketWithAbsSendTimeExtension) {
  EXPECT_TRUE(ValidateRtpHeader(kRtpMsgWithAbsSendTimeExtension,
                                sizeof(kRtpMsgWithAbsSendTimeExtension),
                                nullptr));
}

// Verify handling of a 2 byte extension header RTP messsage. Currently these
// messages are not supported.
TEST(RtpUtilsTest, UpdateAbsSendTimeExtensionIn2ByteHeaderExtn) {
  std::vector<uint8_t> data(
      kRtpMsgWith2ByteExtnHeader,
      kRtpMsgWith2ByteExtnHeader + sizeof(kRtpMsgWith2ByteExtnHeader));
  EXPECT_FALSE(UpdateRtpAbsSendTimeExtension(&data[0], data.size(), 3, 0));
}

// Verify finding an extension ID in the TURN send indication message.
TEST(RtpUtilsTest, UpdateAbsSendTimeExtensionInTurnSendIndication) {
  // A valid STUN indication message with a valid RTP header in data attribute
  // payload field and no extension bit set.
  uint8_t message_without_extension[] = {
      // clang-format off
      // clang formatting doesn't respect inline comments.
      0x00, 0x16, 0x00, 0x18,  // length of
      0x21, 0x12, 0xA4, 0x42,  // magic cookie
      '0',  '1',  '2',  '3',   // transaction id
      '4',  '5',  '6',  '7',
      '8',  '9',  'a',  'b',
      0x00, 0x20, 0x00, 0x04,  // Mapped address.
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x13, 0x00, 0x0C,  // Data attribute.
      0x80, 0x00, 0x00, 0x00,  // RTP packet.
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00,
      // clang-format on
  };
  EXPECT_TRUE(UpdateRtpAbsSendTimeExtension(
      message_without_extension, sizeof(message_without_extension), 3, 0));

  // A valid STUN indication message with a valid RTP header and a extension
  // header.
  uint8_t message[] = {
      // clang-format off
      // clang formatting doesn't respect inline comments.
      0x00, 0x16, 0x00, 0x24,  // length of
      0x21, 0x12, 0xA4, 0x42,  // magic cookie
      '0',  '1',  '2',  '3',   // transaction id
      '4',  '5',  '6',  '7',
      '8',  '9',  'a',  'b',
      0x00, 0x20, 0x00, 0x04,  // Mapped address.
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x13, 0x00, 0x18,  // Data attribute.
      0x90, 0x00, 0x00, 0x00,  // RTP packet.
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xBE, 0xDE,
      0x00, 0x02, 0x22, 0xaa, 0xbb, 0xcc, 0x32, 0xaa, 0xbb, 0xcc,
      // clang-format on
  };
  EXPECT_TRUE(UpdateRtpAbsSendTimeExtension(message, sizeof(message), 3, 0));
}

// Test without any packet options variables set. This method should return
// without HMAC value in the packet.
TEST(RtpUtilsTest, ApplyPacketOptionsWithDefaultValues) {
  rtc::PacketTimeUpdateParams packet_time_params;
  std::vector<uint8_t> rtp_packet(kRtpMsgWithAbsSendTimeExtension,
                                  kRtpMsgWithAbsSendTimeExtension +
                                      sizeof(kRtpMsgWithAbsSendTimeExtension));
  rtp_packet.insert(rtp_packet.end(), kFakeTag, kFakeTag + sizeof(kFakeTag));
  EXPECT_TRUE(ApplyPacketOptions(&rtp_packet[0], rtp_packet.size(),
                                 packet_time_params, 0));

  // Making sure HMAC wasn't updated..
  EXPECT_EQ(0, memcmp(&rtp_packet[sizeof(kRtpMsgWithAbsSendTimeExtension)],
                      kFakeTag, 4));

  // Verify AbsouluteSendTime extension field wasn't modified.
  EXPECT_EQ(0, memcmp(&rtp_packet[kAstIndexInRtpMsg], kTestAstValue,
                      sizeof(kTestAstValue)));
}

// Veirfy HMAC is updated when packet option parameters are set.
TEST(RtpUtilsTest, ApplyPacketOptionsWithAuthParams) {
  rtc::PacketTimeUpdateParams packet_time_params;
  packet_time_params.srtp_auth_key.assign(kTestKey,
                                          kTestKey + sizeof(kTestKey));
  packet_time_params.srtp_auth_tag_len = 4;

  std::vector<uint8_t> rtp_packet(kRtpMsgWithAbsSendTimeExtension,
                                  kRtpMsgWithAbsSendTimeExtension +
                                      sizeof(kRtpMsgWithAbsSendTimeExtension));
  rtp_packet.insert(rtp_packet.end(), kFakeTag, kFakeTag + sizeof(kFakeTag));
  EXPECT_TRUE(ApplyPacketOptions(&rtp_packet[0], rtp_packet.size(),
                                 packet_time_params, 0));

  uint8_t kExpectedTag[] = {0xc1, 0x7a, 0x8c, 0xa0};
  EXPECT_EQ(0, memcmp(&rtp_packet[sizeof(kRtpMsgWithAbsSendTimeExtension)],
                      kExpectedTag, sizeof(kExpectedTag)));

  // Verify AbsouluteSendTime extension field is not modified.
  EXPECT_EQ(0, memcmp(&rtp_packet[kAstIndexInRtpMsg], kTestAstValue,
                      sizeof(kTestAstValue)));
}

// Verify finding an extension ID in a raw rtp message.
TEST(RtpUtilsTest, UpdateAbsSendTimeExtensionInRtpPacket) {
  std::vector<uint8_t> rtp_packet(kRtpMsgWithAbsSendTimeExtension,
                                  kRtpMsgWithAbsSendTimeExtension +
                                      sizeof(kRtpMsgWithAbsSendTimeExtension));

  EXPECT_TRUE(UpdateRtpAbsSendTimeExtension(&rtp_packet[0], rtp_packet.size(),
                                            3, 51183266));

  // Verify that the timestamp was updated.
  const uint8_t kExpectedTimestamp[3] = {0xcc, 0xbb, 0xaa};
  EXPECT_EQ(0, memcmp(&rtp_packet[kAstIndexInRtpMsg], kExpectedTimestamp,
                      sizeof(kExpectedTimestamp)));
}

// Verify we update both AbsSendTime extension header and HMAC.
TEST(RtpUtilsTest, ApplyPacketOptionsWithAuthParamsAndAbsSendTime) {
  rtc::PacketTimeUpdateParams packet_time_params;
  packet_time_params.srtp_auth_key.assign(kTestKey,
                                          kTestKey + sizeof(kTestKey));
  packet_time_params.srtp_auth_tag_len = 4;
  packet_time_params.rtp_sendtime_extension_id = 3;
  // 3 is also present in the test message.

  std::vector<uint8_t> rtp_packet(kRtpMsgWithAbsSendTimeExtension,
                                  kRtpMsgWithAbsSendTimeExtension +
                                      sizeof(kRtpMsgWithAbsSendTimeExtension));
  rtp_packet.insert(rtp_packet.end(), kFakeTag, kFakeTag + sizeof(kFakeTag));
  EXPECT_TRUE(ApplyPacketOptions(&rtp_packet[0], rtp_packet.size(),
                                 packet_time_params, 51183266));

  const uint8_t kExpectedTag[] = {0x81, 0xd1, 0x2c, 0x0e};
  EXPECT_EQ(0, memcmp(&rtp_packet[sizeof(kRtpMsgWithAbsSendTimeExtension)],
                      kExpectedTag, sizeof(kExpectedTag)));

  // Verify that the timestamp was updated.
  const uint8_t kExpectedTimestamp[3] = {0xcc, 0xbb, 0xaa};
  EXPECT_EQ(0, memcmp(&rtp_packet[kAstIndexInRtpMsg], kExpectedTimestamp,
                      sizeof(kExpectedTimestamp)));
}

}  // namespace cricket
