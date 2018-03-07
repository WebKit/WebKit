/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/h264_sprop_parameter_sets.h"

#include <stdint.h>

#include <vector>

#include "test/gtest.h"

namespace webrtc {

class H264SpropParameterSetsTest : public testing::Test {
 public:
  H264SpropParameterSets h264_sprop;
};

TEST_F(H264SpropParameterSetsTest, Base64DecodeSprop) {
  // Example sprop string from https://tools.ietf.org/html/rfc3984 .
  EXPECT_TRUE(h264_sprop.DecodeSprop("Z0IACpZTBYmI,aMljiA=="));
  static const std::vector<uint8_t> raw_sps{0x67, 0x42, 0x00, 0x0A, 0x96,
                                            0x53, 0x05, 0x89, 0x88};
  static const std::vector<uint8_t> raw_pps{0x68, 0xC9, 0x63, 0x88};
  EXPECT_EQ(raw_sps, h264_sprop.sps_nalu());
  EXPECT_EQ(raw_pps, h264_sprop.pps_nalu());
}

TEST_F(H264SpropParameterSetsTest, InvalidData) {
  EXPECT_FALSE(h264_sprop.DecodeSprop(","));
  EXPECT_FALSE(h264_sprop.DecodeSprop(""));
  EXPECT_FALSE(h264_sprop.DecodeSprop(",iA=="));
  EXPECT_FALSE(h264_sprop.DecodeSprop("iA==,"));
  EXPECT_TRUE(h264_sprop.DecodeSprop("iA==,iA=="));
  EXPECT_FALSE(h264_sprop.DecodeSprop("--,--"));
  EXPECT_FALSE(h264_sprop.DecodeSprop(",,"));
  EXPECT_FALSE(h264_sprop.DecodeSprop("iA=="));
}

}  // namespace webrtc
