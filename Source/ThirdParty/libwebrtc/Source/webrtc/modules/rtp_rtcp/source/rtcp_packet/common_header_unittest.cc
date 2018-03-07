/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtcp_packet/common_header.h"

#include "test/gtest.h"

using webrtc::rtcp::CommonHeader;

namespace webrtc {

TEST(RtcpCommonHeaderTest, TooSmallBuffer) {
  uint8_t buffer[] = {0x80, 0x00, 0x00, 0x00};
  CommonHeader header;
  // Buffer needs to be able to hold the header.
  EXPECT_FALSE(header.Parse(buffer, 0));
  EXPECT_FALSE(header.Parse(buffer, 1));
  EXPECT_FALSE(header.Parse(buffer, 2));
  EXPECT_FALSE(header.Parse(buffer, 3));
  EXPECT_TRUE(header.Parse(buffer, 4));
}

TEST(RtcpCommonHeaderTest, Version) {
  uint8_t buffer[] = {0x00, 0x00, 0x00, 0x00};
  CommonHeader header;
  // Version 2 is the only allowed.
  buffer[0] = 0 << 6;
  EXPECT_FALSE(header.Parse(buffer, sizeof(buffer)));
  buffer[0] = 1 << 6;
  EXPECT_FALSE(header.Parse(buffer, sizeof(buffer)));
  buffer[0] = 2 << 6;
  EXPECT_TRUE(header.Parse(buffer, sizeof(buffer)));
  buffer[0] = 3 << 6;
  EXPECT_FALSE(header.Parse(buffer, sizeof(buffer)));
}

TEST(RtcpCommonHeaderTest, PacketSize) {
  uint8_t buffer[] = {0x80, 0x00, 0x00, 0x02,
                      0x00, 0x00, 0x00, 0x00,
                      0x00, 0x00, 0x00, 0x00};
  CommonHeader header;
  EXPECT_FALSE(header.Parse(buffer, sizeof(buffer) - 1));
  EXPECT_TRUE(header.Parse(buffer, sizeof(buffer)));
  EXPECT_EQ(8u, header.payload_size_bytes());
  EXPECT_EQ(buffer + sizeof(buffer), header.NextPacket());
  EXPECT_EQ(sizeof(buffer), header.packet_size());
}

TEST(RtcpCommonHeaderTest, PaddingAndPayloadSize) {
  // Set v = 2, p = 1, but leave fmt, pt as 0.
  uint8_t buffer[] = {0xa0, 0x00, 0x00, 0x00,
                      0x00, 0x00, 0x00, 0x00,
                      0x00, 0x00, 0x00, 0x00};
  CommonHeader header;
  // Padding bit set, but no byte for padding (can't specify padding length).
  EXPECT_FALSE(header.Parse(buffer, 4));

  buffer[3] = 2;  //  Set payload size to 2x32bit.
  const size_t kPayloadSizeBytes = buffer[3] * 4;
  const size_t kPaddingAddress =
      CommonHeader::kHeaderSizeBytes + kPayloadSizeBytes - 1;

  // Padding one byte larger than possible.
  buffer[kPaddingAddress] = kPayloadSizeBytes + 1;
  EXPECT_FALSE(header.Parse(buffer, sizeof(buffer)));

  // Invalid zero padding size.
  buffer[kPaddingAddress] = 0;
  EXPECT_FALSE(header.Parse(buffer, sizeof(buffer)));

  // Pure padding packet.
  buffer[kPaddingAddress] = kPayloadSizeBytes;
  EXPECT_TRUE(header.Parse(buffer, sizeof(buffer)));
  EXPECT_EQ(0u, header.payload_size_bytes());
  EXPECT_EQ(buffer + sizeof(buffer), header.NextPacket());
  EXPECT_EQ(header.payload(), buffer + CommonHeader::kHeaderSizeBytes);
  EXPECT_EQ(header.packet_size(), sizeof(buffer));

  // Single byte of actual data.
  buffer[kPaddingAddress] = kPayloadSizeBytes - 1;
  EXPECT_TRUE(header.Parse(buffer, sizeof(buffer)));
  EXPECT_EQ(1u, header.payload_size_bytes());
  EXPECT_EQ(buffer + sizeof(buffer), header.NextPacket());
  EXPECT_EQ(header.packet_size(), sizeof(buffer));
}

TEST(RtcpCommonHeaderTest, FormatAndPayloadType) {
  uint8_t buffer[] = {0x9e, 0xab, 0x00, 0x00};
  CommonHeader header;
  EXPECT_TRUE(header.Parse(buffer, sizeof(buffer)));

  EXPECT_EQ(header.count(), 0x1e);
  EXPECT_EQ(header.fmt(), 0x1e);
  EXPECT_EQ(header.type(), 0xab);
  EXPECT_EQ(header.payload_size_bytes(), 0u);
  EXPECT_EQ(header.payload(), buffer + CommonHeader::kHeaderSizeBytes);
}
}  // namespace webrtc
