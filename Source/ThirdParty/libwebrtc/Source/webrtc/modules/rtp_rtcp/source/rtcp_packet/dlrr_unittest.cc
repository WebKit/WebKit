/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/dlrr.h"

#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/test/gtest.h"

using webrtc::rtcp::Dlrr;
using webrtc::rtcp::ReceiveTimeInfo;

namespace webrtc {
namespace {
const uint32_t kSsrc = 0x12345678;
const uint32_t kLastRR = 0x23344556;
const uint32_t kDelay = 0x33343536;
const uint8_t kBlock[] = {0x05, 0x00, 0x00, 0x03, 0x12, 0x34, 0x56, 0x78,
                          0x23, 0x34, 0x45, 0x56, 0x33, 0x34, 0x35, 0x36};
const size_t kBlockSizeBytes = sizeof(kBlock);
}  // namespace

TEST(RtcpPacketDlrrTest, Empty) {
  Dlrr dlrr;

  EXPECT_EQ(0u, dlrr.BlockLength());
}

TEST(RtcpPacketDlrrTest, Create) {
  Dlrr dlrr;
  dlrr.AddDlrrItem(ReceiveTimeInfo(kSsrc, kLastRR, kDelay));

  ASSERT_EQ(kBlockSizeBytes, dlrr.BlockLength());
  uint8_t buffer[kBlockSizeBytes];

  dlrr.Create(buffer);
  EXPECT_EQ(0, memcmp(buffer, kBlock, kBlockSizeBytes));
}

TEST(RtcpPacketDlrrTest, Parse) {
  Dlrr dlrr;
  uint16_t block_length = ByteReader<uint16_t>::ReadBigEndian(&kBlock[2]);
  EXPECT_TRUE(dlrr.Parse(kBlock, block_length));

  EXPECT_EQ(1u, dlrr.sub_blocks().size());
  const ReceiveTimeInfo& block = dlrr.sub_blocks().front();
  EXPECT_EQ(kSsrc, block.ssrc);
  EXPECT_EQ(kLastRR, block.last_rr);
  EXPECT_EQ(kDelay, block.delay_since_last_rr);
}

TEST(RtcpPacketDlrrTest, ParseFailsOnBadSize) {
  const size_t kBigBufferSize = 0x100;  // More than enough.
  uint8_t buffer[kBigBufferSize];
  buffer[0] = Dlrr::kBlockType;
  buffer[1] = 0;  // Reserved.
  buffer[2] = 0;  // Most significant size byte.
  for (uint8_t size = 3; size < 6; ++size) {
    buffer[3] = size;
    Dlrr dlrr;
    // Parse should be successful only when size is multiple of 3.
    EXPECT_EQ(size % 3 == 0, dlrr.Parse(buffer, static_cast<uint16_t>(size)));
  }
}

TEST(RtcpPacketDlrrTest, CreateAndParseManySubBlocks) {
  const size_t kBufferSize = 0x1000;  // More than enough.
  const size_t kManyDlrrItems = 50;
  uint8_t buffer[kBufferSize];

  // Create.
  Dlrr dlrr;
  for (size_t i = 1; i <= kManyDlrrItems; ++i)
    dlrr.AddDlrrItem(ReceiveTimeInfo(kSsrc + i, kLastRR + i, kDelay + i));
  size_t used_buffer_size = dlrr.BlockLength();
  ASSERT_LE(used_buffer_size, kBufferSize);
  dlrr.Create(buffer);

  // Parse.
  Dlrr parsed;
  uint16_t block_length = ByteReader<uint16_t>::ReadBigEndian(&buffer[2]);
  EXPECT_EQ(used_buffer_size, (block_length + 1) * 4u);
  EXPECT_TRUE(parsed.Parse(buffer, block_length));
  EXPECT_EQ(kManyDlrrItems, parsed.sub_blocks().size());
}
}  // namespace webrtc
