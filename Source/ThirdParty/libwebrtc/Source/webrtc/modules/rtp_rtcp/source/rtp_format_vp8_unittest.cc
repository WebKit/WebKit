/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "modules/rtp_rtcp/source/rtp_format_vp8.h"
#include "modules/rtp_rtcp/source/rtp_format_vp8_test_helper.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAreArray;
using ::testing::make_tuple;

constexpr RtpPacketToSend::ExtensionManager* kNoExtensions = nullptr;
constexpr RtpPacketizer::PayloadSizeLimits kNoSizeLimits;
// Payload descriptor
//       0 1 2 3 4 5 6 7
//      +-+-+-+-+-+-+-+-+
//      |X|R|N|S|PartID | (REQUIRED)
//      +-+-+-+-+-+-+-+-+
// X:   |I|L|T|K|  RSV  | (OPTIONAL)
//      +-+-+-+-+-+-+-+-+
// I:   |   PictureID   | (OPTIONAL)
//      +-+-+-+-+-+-+-+-+
// L:   |   TL0PICIDX   | (OPTIONAL)
//      +-+-+-+-+-+-+-+-+
// T/K: |TID:Y| KEYIDX  | (OPTIONAL)
//      +-+-+-+-+-+-+-+-+
//
// Payload header
//       0 1 2 3 4 5 6 7
//      +-+-+-+-+-+-+-+-+
//      |Size0|H| VER |P|
//      +-+-+-+-+-+-+-+-+
//      |     Size1     |
//      +-+-+-+-+-+-+-+-+
//      |     Size2     |
//      +-+-+-+-+-+-+-+-+
//      | Bytes 4..N of |
//      | VP8 payload   |
//      :               :
//      +-+-+-+-+-+-+-+-+
//      | OPTIONAL RTP  |
//      | padding       |
//      :               :
//      +-+-+-+-+-+-+-+-+
void VerifyBasicHeader(RTPVideoHeader* header, bool N, bool S, int part_id) {
  ASSERT_TRUE(header != NULL);
  const auto& vp8_header =
      absl::get<RTPVideoHeaderVP8>(header->video_type_header);
  EXPECT_EQ(N, vp8_header.nonReference);
  EXPECT_EQ(S, vp8_header.beginningOfPartition);
  EXPECT_EQ(part_id, vp8_header.partitionId);
}

void VerifyExtensions(RTPVideoHeader* header,
                      int16_t picture_id,   /* I */
                      int16_t tl0_pic_idx,  /* L */
                      uint8_t temporal_idx, /* T */
                      int key_idx /* K */) {
  ASSERT_TRUE(header != NULL);
  const auto& vp8_header =
      absl::get<RTPVideoHeaderVP8>(header->video_type_header);
  EXPECT_EQ(picture_id, vp8_header.pictureId);
  EXPECT_EQ(tl0_pic_idx, vp8_header.tl0PicIdx);
  EXPECT_EQ(temporal_idx, vp8_header.temporalIdx);
  EXPECT_EQ(key_idx, vp8_header.keyIdx);
}

}  // namespace

TEST(RtpPacketizerVp8Test, ResultPacketsAreAlmostEqualSize) {
  RTPVideoHeaderVP8 hdr_info;
  hdr_info.InitRTPVideoHeaderVP8();
  hdr_info.pictureId = 200;
  RtpFormatVp8TestHelper helper(&hdr_info, /*payload_len=*/30);

  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 12;  // Small enough to produce 4 packets.
  RtpPacketizerVp8 packetizer(helper.payload(), limits, hdr_info);

  const size_t kExpectedSizes[] = {11, 11, 12, 12};
  helper.GetAllPacketsAndCheck(&packetizer, kExpectedSizes);
}

TEST(RtpPacketizerVp8Test, EqualSizeWithLastPacketReduction) {
  RTPVideoHeaderVP8 hdr_info;
  hdr_info.InitRTPVideoHeaderVP8();
  hdr_info.pictureId = 200;
  RtpFormatVp8TestHelper helper(&hdr_info, /*payload_len=*/43);

  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 15;  // Small enough to produce 5 packets.
  limits.last_packet_reduction_len = 5;
  RtpPacketizerVp8 packetizer(helper.payload(), limits, hdr_info);

  // Calculated by hand. VP8 payload descriptors are 4 byte each. 5 packets is
  // minimum possible to fit 43 payload bytes into packets with capacity of
  // 15 - 4 = 11 and leave 5 free bytes in the last packet. All packets are
  // almost equal in size, even last packet if counted with free space (which
  // will be filled up the stack by extra long RTP header).
  const size_t kExpectedSizes[] = {13, 13, 14, 14, 9};
  helper.GetAllPacketsAndCheck(&packetizer, kExpectedSizes);
}

// Verify that non-reference bit is set.
TEST(RtpPacketizerVp8Test, NonReferenceBit) {
  RTPVideoHeaderVP8 hdr_info;
  hdr_info.InitRTPVideoHeaderVP8();
  hdr_info.nonReference = true;
  RtpFormatVp8TestHelper helper(&hdr_info, /*payload_len=*/30);

  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 25;  // Small enough to produce two packets.
  RtpPacketizerVp8 packetizer(helper.payload(), limits, hdr_info);

  const size_t kExpectedSizes[] = {16, 16};
  helper.GetAllPacketsAndCheck(&packetizer, kExpectedSizes);
}

// Verify Tl0PicIdx and TID fields, and layerSync bit.
TEST(RtpPacketizerVp8Test, Tl0PicIdxAndTID) {
  RTPVideoHeaderVP8 hdr_info;
  hdr_info.InitRTPVideoHeaderVP8();
  hdr_info.tl0PicIdx = 117;
  hdr_info.temporalIdx = 2;
  hdr_info.layerSync = true;
  RtpFormatVp8TestHelper helper(&hdr_info, /*payload_len=*/30);

  RtpPacketizerVp8 packetizer(helper.payload(), kNoSizeLimits, hdr_info);

  const size_t kExpectedSizes[1] = {helper.payload_size() + 4};
  helper.GetAllPacketsAndCheck(&packetizer, kExpectedSizes);
}

TEST(RtpPacketizerVp8Test, KeyIdx) {
  RTPVideoHeaderVP8 hdr_info;
  hdr_info.InitRTPVideoHeaderVP8();
  hdr_info.keyIdx = 17;
  RtpFormatVp8TestHelper helper(&hdr_info, /*payload_len=*/30);

  RtpPacketizerVp8 packetizer(helper.payload(), kNoSizeLimits, hdr_info);

  const size_t kExpectedSizes[1] = {helper.payload_size() + 3};
  helper.GetAllPacketsAndCheck(&packetizer, kExpectedSizes);
}

// Verify TID field and KeyIdx field in combination.
TEST(RtpPacketizerVp8Test, TIDAndKeyIdx) {
  RTPVideoHeaderVP8 hdr_info;
  hdr_info.InitRTPVideoHeaderVP8();
  hdr_info.temporalIdx = 1;
  hdr_info.keyIdx = 5;
  RtpFormatVp8TestHelper helper(&hdr_info, /*payload_len=*/30);

  RtpPacketizerVp8 packetizer(helper.payload(), kNoSizeLimits, hdr_info);

  const size_t kExpectedSizes[1] = {helper.payload_size() + 3};
  helper.GetAllPacketsAndCheck(&packetizer, kExpectedSizes);
}

class RtpDepacketizerVp8Test : public ::testing::Test {
 protected:
  RtpDepacketizerVp8Test()
      : depacketizer_(RtpDepacketizer::Create(kVideoCodecVP8)) {}

  void ExpectPacket(RtpDepacketizer::ParsedPayload* parsed_payload,
                    const uint8_t* data,
                    size_t length) {
    ASSERT_TRUE(parsed_payload != NULL);
    EXPECT_THAT(
        make_tuple(parsed_payload->payload, parsed_payload->payload_length),
        ElementsAreArray(data, length));
  }

  std::unique_ptr<RtpDepacketizer> depacketizer_;
};

TEST_F(RtpDepacketizerVp8Test, BasicHeader) {
  const uint8_t kHeaderLength = 1;
  uint8_t packet[4] = {0};
  packet[0] = 0x14;  // Binary 0001 0100; S = 1, PartID = 4.
  packet[1] = 0x01;  // P frame.
  RtpDepacketizer::ParsedPayload payload;

  ASSERT_TRUE(depacketizer_->Parse(&payload, packet, sizeof(packet)));
  ExpectPacket(&payload, packet + kHeaderLength,
               sizeof(packet) - kHeaderLength);

  EXPECT_EQ(kVideoFrameDelta, payload.frame_type);
  EXPECT_EQ(kVideoCodecVP8, payload.video_header().codec);
  VerifyBasicHeader(&payload.video_header(), 0, 1, 4);
  VerifyExtensions(&payload.video_header(), kNoPictureId, kNoTl0PicIdx,
                   kNoTemporalIdx, kNoKeyIdx);
}

TEST_F(RtpDepacketizerVp8Test, PictureID) {
  const uint8_t kHeaderLength1 = 3;
  const uint8_t kHeaderLength2 = 4;
  const uint8_t kPictureId = 17;
  uint8_t packet[10] = {0};
  packet[0] = 0xA0;
  packet[1] = 0x80;
  packet[2] = kPictureId;
  RtpDepacketizer::ParsedPayload payload;

  ASSERT_TRUE(depacketizer_->Parse(&payload, packet, sizeof(packet)));
  ExpectPacket(&payload, packet + kHeaderLength1,
               sizeof(packet) - kHeaderLength1);
  EXPECT_EQ(kVideoFrameDelta, payload.frame_type);
  EXPECT_EQ(kVideoCodecVP8, payload.video_header().codec);
  VerifyBasicHeader(&payload.video_header(), 1, 0, 0);
  VerifyExtensions(&payload.video_header(), kPictureId, kNoTl0PicIdx,
                   kNoTemporalIdx, kNoKeyIdx);

  // Re-use packet, but change to long PictureID.
  packet[2] = 0x80 | kPictureId;
  packet[3] = kPictureId;

  payload = RtpDepacketizer::ParsedPayload();
  ASSERT_TRUE(depacketizer_->Parse(&payload, packet, sizeof(packet)));
  ExpectPacket(&payload, packet + kHeaderLength2,
               sizeof(packet) - kHeaderLength2);
  VerifyBasicHeader(&payload.video_header(), 1, 0, 0);
  VerifyExtensions(&payload.video_header(), (kPictureId << 8) + kPictureId,
                   kNoTl0PicIdx, kNoTemporalIdx, kNoKeyIdx);
}

TEST_F(RtpDepacketizerVp8Test, Tl0PicIdx) {
  const uint8_t kHeaderLength = 3;
  const uint8_t kTl0PicIdx = 17;
  uint8_t packet[13] = {0};
  packet[0] = 0x90;
  packet[1] = 0x40;
  packet[2] = kTl0PicIdx;
  RtpDepacketizer::ParsedPayload payload;

  ASSERT_TRUE(depacketizer_->Parse(&payload, packet, sizeof(packet)));
  ExpectPacket(&payload, packet + kHeaderLength,
               sizeof(packet) - kHeaderLength);
  EXPECT_EQ(kVideoFrameKey, payload.frame_type);
  EXPECT_EQ(kVideoCodecVP8, payload.video_header().codec);
  VerifyBasicHeader(&payload.video_header(), 0, 1, 0);
  VerifyExtensions(&payload.video_header(), kNoPictureId, kTl0PicIdx,
                   kNoTemporalIdx, kNoKeyIdx);
}

TEST_F(RtpDepacketizerVp8Test, TIDAndLayerSync) {
  const uint8_t kHeaderLength = 3;
  uint8_t packet[10] = {0};
  packet[0] = 0x88;
  packet[1] = 0x20;
  packet[2] = 0x80;  // TID(2) + LayerSync(false)
  RtpDepacketizer::ParsedPayload payload;

  ASSERT_TRUE(depacketizer_->Parse(&payload, packet, sizeof(packet)));
  ExpectPacket(&payload, packet + kHeaderLength,
               sizeof(packet) - kHeaderLength);
  EXPECT_EQ(kVideoFrameDelta, payload.frame_type);
  EXPECT_EQ(kVideoCodecVP8, payload.video_header().codec);
  VerifyBasicHeader(&payload.video_header(), 0, 0, 8);
  VerifyExtensions(&payload.video_header(), kNoPictureId, kNoTl0PicIdx, 2,
                   kNoKeyIdx);
  EXPECT_FALSE(
      absl::get<RTPVideoHeaderVP8>(payload.video_header().video_type_header)
          .layerSync);
}

TEST_F(RtpDepacketizerVp8Test, KeyIdx) {
  const uint8_t kHeaderLength = 3;
  const uint8_t kKeyIdx = 17;
  uint8_t packet[10] = {0};
  packet[0] = 0x88;
  packet[1] = 0x10;  // K = 1.
  packet[2] = kKeyIdx;
  RtpDepacketizer::ParsedPayload payload;

  ASSERT_TRUE(depacketizer_->Parse(&payload, packet, sizeof(packet)));
  ExpectPacket(&payload, packet + kHeaderLength,
               sizeof(packet) - kHeaderLength);
  EXPECT_EQ(kVideoFrameDelta, payload.frame_type);
  EXPECT_EQ(kVideoCodecVP8, payload.video_header().codec);
  VerifyBasicHeader(&payload.video_header(), 0, 0, 8);
  VerifyExtensions(&payload.video_header(), kNoPictureId, kNoTl0PicIdx,
                   kNoTemporalIdx, kKeyIdx);
}

TEST_F(RtpDepacketizerVp8Test, MultipleExtensions) {
  const uint8_t kHeaderLength = 6;
  uint8_t packet[10] = {0};
  packet[0] = 0x88;
  packet[1] = 0x80 | 0x40 | 0x20 | 0x10;
  packet[2] = 0x80 | 17;           // PictureID, high 7 bits.
  packet[3] = 17;                  // PictureID, low 8 bits.
  packet[4] = 42;                  // Tl0PicIdx.
  packet[5] = 0x40 | 0x20 | 0x11;  // TID(1) + LayerSync(true) + KEYIDX(17).
  RtpDepacketizer::ParsedPayload payload;

  ASSERT_TRUE(depacketizer_->Parse(&payload, packet, sizeof(packet)));
  ExpectPacket(&payload, packet + kHeaderLength,
               sizeof(packet) - kHeaderLength);
  EXPECT_EQ(kVideoFrameDelta, payload.frame_type);
  EXPECT_EQ(kVideoCodecVP8, payload.video_header().codec);
  VerifyBasicHeader(&payload.video_header(), 0, 0, 8);
  VerifyExtensions(&payload.video_header(), (17 << 8) + 17, 42, 1, 17);
}

TEST_F(RtpDepacketizerVp8Test, TooShortHeader) {
  uint8_t packet[4] = {0};
  packet[0] = 0x88;
  packet[1] = 0x80 | 0x40 | 0x20 | 0x10;  // All extensions are enabled...
  packet[2] = 0x80 | 17;  // ... but only 2 bytes PictureID is provided.
  packet[3] = 17;         // PictureID, low 8 bits.
  RtpDepacketizer::ParsedPayload payload;

  EXPECT_FALSE(depacketizer_->Parse(&payload, packet, sizeof(packet)));
}

TEST_F(RtpDepacketizerVp8Test, TestWithPacketizer) {
  const uint8_t kHeaderLength = 5;
  uint8_t data[10] = {0};
  RtpPacketToSend packet(kNoExtensions);
  RTPVideoHeaderVP8 input_header;
  input_header.nonReference = true;
  input_header.pictureId = 300;
  input_header.temporalIdx = 1;
  input_header.layerSync = false;
  input_header.tl0PicIdx = kNoTl0PicIdx;  // Disable.
  input_header.keyIdx = 31;
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 20;
  RtpPacketizerVp8 packetizer(data, limits, input_header);
  EXPECT_EQ(packetizer.NumPackets(), 1u);
  ASSERT_TRUE(packetizer.NextPacket(&packet));
  EXPECT_TRUE(packet.Marker());

  auto rtp_payload = packet.payload();
  RtpDepacketizer::ParsedPayload payload;
  ASSERT_TRUE(
      depacketizer_->Parse(&payload, rtp_payload.data(), rtp_payload.size()));
  auto vp8_payload = rtp_payload.subview(kHeaderLength);
  ExpectPacket(&payload, vp8_payload.data(), vp8_payload.size());
  EXPECT_EQ(kVideoFrameKey, payload.frame_type);
  EXPECT_EQ(kVideoCodecVP8, payload.video_header().codec);
  VerifyBasicHeader(&payload.video_header(), 1, 1, 0);
  VerifyExtensions(&payload.video_header(), input_header.pictureId,
                   input_header.tl0PicIdx, input_header.temporalIdx,
                   input_header.keyIdx);
  EXPECT_EQ(
      absl::get<RTPVideoHeaderVP8>(payload.video_header().video_type_header)
          .layerSync,
      input_header.layerSync);
}

TEST_F(RtpDepacketizerVp8Test, TestEmptyPayload) {
  // Using a wild pointer to crash on accesses from inside the depacketizer.
  uint8_t* garbage_ptr = reinterpret_cast<uint8_t*>(0x4711);
  RtpDepacketizer::ParsedPayload payload;
  EXPECT_FALSE(depacketizer_->Parse(&payload, garbage_ptr, 0));
}
}  // namespace webrtc
