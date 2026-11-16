/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/h264/h264_common.h"

#include <stdint.h>

#include <limits>

#include "test/gtest.h"

namespace webrtc {
namespace H264 {

TEST(H264CommonTest, AcceptsStandardResolutions) {
  EXPECT_TRUE(IsValidResolution(640, 480));
  EXPECT_TRUE(IsValidResolution(1920, 1080));
}

TEST(H264CommonTest, AcceptsMaxLevel52Resolution) {
  // 4096x2304 = 256x144 MBs = 36864 MBs (exact MaxFS for Level 5.2)
  EXPECT_TRUE(IsValidResolution(4096, 2304));
}

TEST(H264CommonTest, AcceptsMaxLevel52AspectWidth) {
  // 543 MBs = 8688 pixels (max macroblock dimension for Level 5.2)
  EXPECT_TRUE(IsValidResolution(8688, 16));
}

TEST(H264CommonTest, AcceptsMaxLevel52AspectHeight) {
  // 543 MBs = 8688 pixels (max macroblock dimension for Level 5.2)
  EXPECT_TRUE(IsValidResolution(16, 8688));
}

TEST(H264CommonTest, RejectsNonPositiveWidth) {
  EXPECT_FALSE(IsValidResolution(0, 480));
  EXPECT_FALSE(IsValidResolution(-1, 480));
}

TEST(H264CommonTest, RejectsNonPositiveHeight) {
  EXPECT_FALSE(IsValidResolution(640, 0));
  EXPECT_FALSE(IsValidResolution(640, -1));
}

TEST(H264CommonTest, RejectsResolutionExceedingLevel52MaxFS) {
  // 4096x2320 = 256x145 MBs = 37120 MBs > 36864
  EXPECT_FALSE(IsValidResolution(4096, 2320));
}

TEST(H264CommonTest, RejectsResolutionExceedingLevel52AspectWidth) {
  // 8689 px = 544 MBs > 543 MBs
  EXPECT_FALSE(IsValidResolution(8689, 16));
}

TEST(H264CommonTest, RejectsResolutionExceedingLevel52AspectHeight) {
  // 8689 px = 544 MBs > 543 MBs
  EXPECT_FALSE(IsValidResolution(16, 8689));
}

TEST(H264CommonTest, RejectsExtremeWidthWithoutOverflow) {
  EXPECT_FALSE(IsValidResolution(std::numeric_limits<int64_t>::max(), 1080));
}

TEST(H264CommonTest, RejectsExtremeHeightWithoutOverflow) {
  EXPECT_FALSE(IsValidResolution(1920, std::numeric_limits<int64_t>::max()));
}

}  // namespace H264
}  // namespace webrtc
