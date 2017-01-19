/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_video/h264/h264_bitstream_parser.h"

#include "webrtc/test/gtest.h"

namespace webrtc {

// SPS/PPS part of below chunk.
uint8_t kH264SpsPps[] = {0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x80, 0x20, 0xda,
                         0x01, 0x40, 0x16, 0xe8, 0x06, 0xd0, 0xa1, 0x35, 0x00,
                         0x00, 0x00, 0x01, 0x68, 0xce, 0x06, 0xe2};

// Contains enough of the image slice to contain slice QP.
uint8_t kH264BitstreamChunk[] = {
    0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x80, 0x20, 0xda, 0x01, 0x40, 0x16,
    0xe8, 0x06, 0xd0, 0xa1, 0x35, 0x00, 0x00, 0x00, 0x01, 0x68, 0xce, 0x06,
    0xe2, 0x00, 0x00, 0x00, 0x01, 0x65, 0xb8, 0x40, 0xf0, 0x8c, 0x03, 0xf2,
    0x75, 0x67, 0xad, 0x41, 0x64, 0x24, 0x0e, 0xa0, 0xb2, 0x12, 0x1e, 0xf8,
};

// Contains enough of the image slice to contain slice QP.
uint8_t kH264BitstreamNextImageSliceChunk[] = {
    0x00, 0x00, 0x00, 0x01, 0x41, 0xe2, 0x01, 0x16, 0x0e, 0x3e, 0x2b, 0x86,
};

TEST(H264BitstreamParserTest, ReportsNoQpWithoutParsedSlices) {
  H264BitstreamParser h264_parser;
  int qp;
  EXPECT_FALSE(h264_parser.GetLastSliceQp(&qp));
}

TEST(H264BitstreamParserTest, ReportsNoQpWithOnlyParsedPpsAndSpsSlices) {
  H264BitstreamParser h264_parser;
  h264_parser.ParseBitstream(kH264SpsPps, sizeof(kH264SpsPps));
  int qp;
  EXPECT_FALSE(h264_parser.GetLastSliceQp(&qp));
}

TEST(H264BitstreamParserTest, ReportsLastSliceQpForImageSlices) {
  H264BitstreamParser h264_parser;
  h264_parser.ParseBitstream(kH264BitstreamChunk, sizeof(kH264BitstreamChunk));
  int qp;
  ASSERT_TRUE(h264_parser.GetLastSliceQp(&qp));
  EXPECT_EQ(35, qp);

  // Parse an additional image slice.
  h264_parser.ParseBitstream(kH264BitstreamNextImageSliceChunk,
                             sizeof(kH264BitstreamNextImageSliceChunk));
  ASSERT_TRUE(h264_parser.GetLastSliceQp(&qp));
  EXPECT_EQ(37, qp);
}

}  // namespace webrtc
