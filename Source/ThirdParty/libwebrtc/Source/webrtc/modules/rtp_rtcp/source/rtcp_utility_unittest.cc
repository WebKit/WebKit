/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/checks.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

using RTCPUtility::RtcpCommonHeader;

namespace rtcp {

TEST(RtcpUtilityTest, MidNtp) {
  const uint32_t kNtpSec = 0x12345678;
  const uint32_t kNtpFrac = 0x23456789;
  const uint32_t kNtpMid = 0x56782345;
  EXPECT_EQ(kNtpMid, RTCPUtility::MidNtp(kNtpSec, kNtpFrac));
}

TEST(RtcpUtilityTest, NackRequests) {
  RTCPUtility::NackStats stats;
  EXPECT_EQ(0U, stats.unique_requests());
  EXPECT_EQ(0U, stats.requests());
  stats.ReportRequest(10);
  EXPECT_EQ(1U, stats.unique_requests());
  EXPECT_EQ(1U, stats.requests());

  stats.ReportRequest(10);
  EXPECT_EQ(1U, stats.unique_requests());
  stats.ReportRequest(11);
  EXPECT_EQ(2U, stats.unique_requests());

  stats.ReportRequest(11);
  EXPECT_EQ(2U, stats.unique_requests());
  stats.ReportRequest(13);
  EXPECT_EQ(3U, stats.unique_requests());

  stats.ReportRequest(11);
  EXPECT_EQ(3U, stats.unique_requests());
  EXPECT_EQ(6U, stats.requests());
}

TEST(RtcpUtilityTest, NackRequestsWithWrap) {
  RTCPUtility::NackStats stats;
  stats.ReportRequest(65534);
  EXPECT_EQ(1U, stats.unique_requests());

  stats.ReportRequest(65534);
  EXPECT_EQ(1U, stats.unique_requests());
  stats.ReportRequest(65535);
  EXPECT_EQ(2U, stats.unique_requests());

  stats.ReportRequest(65535);
  EXPECT_EQ(2U, stats.unique_requests());
  stats.ReportRequest(0);
  EXPECT_EQ(3U, stats.unique_requests());

  stats.ReportRequest(65535);
  EXPECT_EQ(3U, stats.unique_requests());
  stats.ReportRequest(0);
  EXPECT_EQ(3U, stats.unique_requests());
  stats.ReportRequest(1);
  EXPECT_EQ(4U, stats.unique_requests());
  EXPECT_EQ(8U, stats.requests());
}

class RtcpParseCommonHeaderTest : public ::testing::Test {
 public:
  RtcpParseCommonHeaderTest() { memset(buffer, 0, kBufferCapacityBytes); }
  virtual ~RtcpParseCommonHeaderTest() {}

 protected:
  static const size_t kBufferCapacityBytes = 40;
  uint8_t buffer[kBufferCapacityBytes];
  RtcpCommonHeader header;
};

TEST_F(RtcpParseCommonHeaderTest, TooSmallBuffer) {
  // Buffer needs to be able to hold the header.
  for (size_t i = 0; i < RtcpCommonHeader::kHeaderSizeBytes; ++i)
    EXPECT_FALSE(RtcpParseCommonHeader(buffer, i, &header));
}

TEST_F(RtcpParseCommonHeaderTest, Version) {
  // Version 2 is the only allowed for now.
  for (int v = 0; v < 4; ++v) {
    buffer[0] = v << 6;
    EXPECT_EQ(v == 2, RtcpParseCommonHeader(
                          buffer, RtcpCommonHeader::kHeaderSizeBytes, &header));
  }
}

TEST_F(RtcpParseCommonHeaderTest, PacketSize) {
  // Set v = 2, leave p, fmt, pt as 0.
  buffer[0] = 2 << 6;

  const size_t kBlockSize = 3;
  ByteWriter<uint16_t>::WriteBigEndian(&buffer[2], kBlockSize);
  const size_t kSizeInBytes = (kBlockSize + 1) * 4;

  EXPECT_FALSE(RtcpParseCommonHeader(buffer, kSizeInBytes - 1, &header));
  EXPECT_TRUE(RtcpParseCommonHeader(buffer, kSizeInBytes, &header));
}

TEST_F(RtcpParseCommonHeaderTest, PayloadSize) {
  // Set v = 2, p = 1, but leave fmt, pt as 0.
  buffer[0] = (2 << 6) | (1 << 5);

  // Padding bit set, but no byte for padding (can't specify padding length).
  EXPECT_FALSE(RtcpParseCommonHeader(buffer, 4, &header));

  const size_t kBlockSize = 3;
  ByteWriter<uint16_t>::WriteBigEndian(&buffer[2], kBlockSize);
  const size_t kSizeInBytes = (kBlockSize + 1) * 4;
  const size_t kPayloadSizeBytes =
      kSizeInBytes - RtcpCommonHeader::kHeaderSizeBytes;

  // Padding one byte larger than possible.
  buffer[kSizeInBytes - 1] = kPayloadSizeBytes + 1;
  EXPECT_FALSE(RtcpParseCommonHeader(buffer, kSizeInBytes, &header));

  // Pure padding packet?
  buffer[kSizeInBytes - 1] = kPayloadSizeBytes;
  EXPECT_TRUE(RtcpParseCommonHeader(buffer, kSizeInBytes, &header));
  EXPECT_EQ(kPayloadSizeBytes, header.padding_bytes);
  EXPECT_EQ(0u, header.payload_size_bytes);

  // Single byte of actual data.
  buffer[kSizeInBytes - 1] = kPayloadSizeBytes - 1;
  EXPECT_TRUE(RtcpParseCommonHeader(buffer, kSizeInBytes, &header));
  EXPECT_EQ(kPayloadSizeBytes - 1, header.padding_bytes);
  EXPECT_EQ(1u, header.payload_size_bytes);
}

TEST_F(RtcpParseCommonHeaderTest, FormatAndPayloadType) {
  // Format/count and packet type both set to max values.
  const uint8_t kCountOrFormat = 0x1F;
  const uint8_t kPacketType = 0xFF;
  buffer[0] = 2 << 6;  // V = 2.
  buffer[0] |= kCountOrFormat;
  buffer[1] = kPacketType;

  EXPECT_TRUE(RtcpParseCommonHeader(buffer, RtcpCommonHeader::kHeaderSizeBytes,
                                    &header));
  EXPECT_EQ(kCountOrFormat, header.count_or_format);
  EXPECT_EQ(kPacketType, header.packet_type);
}

}  // namespace rtcp
}  // namespace webrtc

