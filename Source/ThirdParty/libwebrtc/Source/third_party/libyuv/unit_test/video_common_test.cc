/*
 *  Copyright 2012 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdlib.h>
#include <string.h>

#include "../unit_test/unit_test.h"
#include "libyuv/video_common.h"

namespace libyuv {

// Tests FourCC codes in video common, which are used for ConvertToI420().

static bool TestValidChar(uint32_t onecc) {
  return (onecc >= '0' && onecc <= '9') || (onecc >= 'A' && onecc <= 'Z') ||
         (onecc >= 'a' && onecc <= 'z') || (onecc == ' ') || (onecc == 0xff);
}

static bool TestValidFourCC(uint32_t fourcc, int bpp) {
  if (!TestValidChar(fourcc & 0xff) || !TestValidChar((fourcc >> 8) & 0xff) ||
      !TestValidChar((fourcc >> 16) & 0xff) ||
      !TestValidChar((fourcc >> 24) & 0xff)) {
    return false;
  }
  if (bpp < 0 || bpp > 64) {
    return false;
  }
  return true;
}

TEST_F(LibYUVBaseTest, TestCanonicalFourCC) {
  EXPECT_EQ(static_cast<uint32_t>(FOURCC_I420), CanonicalFourCC(FOURCC_IYUV));
  EXPECT_EQ(static_cast<uint32_t>(FOURCC_I420), CanonicalFourCC(FOURCC_YU12));
  EXPECT_EQ(static_cast<uint32_t>(FOURCC_I422), CanonicalFourCC(FOURCC_YU16));
  EXPECT_EQ(static_cast<uint32_t>(FOURCC_I444), CanonicalFourCC(FOURCC_YU24));
  EXPECT_EQ(static_cast<uint32_t>(FOURCC_YUY2), CanonicalFourCC(FOURCC_YUYV));
  EXPECT_EQ(static_cast<uint32_t>(FOURCC_YUY2), CanonicalFourCC(FOURCC_YUVS));
  EXPECT_EQ(static_cast<uint32_t>(FOURCC_UYVY), CanonicalFourCC(FOURCC_HDYC));
  EXPECT_EQ(static_cast<uint32_t>(FOURCC_UYVY), CanonicalFourCC(FOURCC_2VUY));
  EXPECT_EQ(static_cast<uint32_t>(FOURCC_MJPG), CanonicalFourCC(FOURCC_JPEG));
  EXPECT_EQ(static_cast<uint32_t>(FOURCC_MJPG), CanonicalFourCC(FOURCC_DMB1));
  EXPECT_EQ(static_cast<uint32_t>(FOURCC_RAW), CanonicalFourCC(FOURCC_RGB3));
  EXPECT_EQ(static_cast<uint32_t>(FOURCC_24BG), CanonicalFourCC(FOURCC_BGR3));
  EXPECT_EQ(static_cast<uint32_t>(FOURCC_BGRA), CanonicalFourCC(FOURCC_CM32));
  EXPECT_EQ(static_cast<uint32_t>(FOURCC_RAW), CanonicalFourCC(FOURCC_CM24));
  EXPECT_EQ(static_cast<uint32_t>(FOURCC_RGBO), CanonicalFourCC(FOURCC_L555));
  EXPECT_EQ(static_cast<uint32_t>(FOURCC_RGBP), CanonicalFourCC(FOURCC_L565));
  EXPECT_EQ(static_cast<uint32_t>(FOURCC_RGBO), CanonicalFourCC(FOURCC_5551));
}

TEST_F(LibYUVBaseTest, TestFourCC) {
  EXPECT_TRUE(TestValidFourCC(FOURCC_I420, FOURCC_BPP_I420));
  EXPECT_TRUE(TestValidFourCC(FOURCC_I420, FOURCC_BPP_I420));
  EXPECT_TRUE(TestValidFourCC(FOURCC_I422, FOURCC_BPP_I422));
  EXPECT_TRUE(TestValidFourCC(FOURCC_I444, FOURCC_BPP_I444));
  EXPECT_TRUE(TestValidFourCC(FOURCC_I400, FOURCC_BPP_I400));
  EXPECT_TRUE(TestValidFourCC(FOURCC_NV21, FOURCC_BPP_NV21));
  EXPECT_TRUE(TestValidFourCC(FOURCC_NV12, FOURCC_BPP_NV12));
  EXPECT_TRUE(TestValidFourCC(FOURCC_YUY2, FOURCC_BPP_YUY2));
  EXPECT_TRUE(TestValidFourCC(FOURCC_UYVY, FOURCC_BPP_UYVY));
  EXPECT_TRUE(TestValidFourCC(FOURCC_M420, FOURCC_BPP_M420));  // deprecated.
  EXPECT_TRUE(TestValidFourCC(FOURCC_Q420, FOURCC_BPP_Q420));  // deprecated.
  EXPECT_TRUE(TestValidFourCC(FOURCC_ARGB, FOURCC_BPP_ARGB));
  EXPECT_TRUE(TestValidFourCC(FOURCC_BGRA, FOURCC_BPP_BGRA));
  EXPECT_TRUE(TestValidFourCC(FOURCC_ABGR, FOURCC_BPP_ABGR));
  EXPECT_TRUE(TestValidFourCC(FOURCC_AR30, FOURCC_BPP_AR30));
  EXPECT_TRUE(TestValidFourCC(FOURCC_AB30, FOURCC_BPP_AB30));
  EXPECT_TRUE(TestValidFourCC(FOURCC_AR64, FOURCC_BPP_AR64));
  EXPECT_TRUE(TestValidFourCC(FOURCC_AB64, FOURCC_BPP_AB64));
  EXPECT_TRUE(TestValidFourCC(FOURCC_24BG, FOURCC_BPP_24BG));
  EXPECT_TRUE(TestValidFourCC(FOURCC_RAW, FOURCC_BPP_RAW));
  EXPECT_TRUE(TestValidFourCC(FOURCC_RGBA, FOURCC_BPP_RGBA));
  EXPECT_TRUE(TestValidFourCC(FOURCC_RGBP, FOURCC_BPP_RGBP));
  EXPECT_TRUE(TestValidFourCC(FOURCC_RGBO, FOURCC_BPP_RGBO));
  EXPECT_TRUE(TestValidFourCC(FOURCC_R444, FOURCC_BPP_R444));
  EXPECT_TRUE(TestValidFourCC(FOURCC_H420, FOURCC_BPP_H420));
  EXPECT_TRUE(TestValidFourCC(FOURCC_H422, FOURCC_BPP_H422));
  EXPECT_TRUE(TestValidFourCC(FOURCC_H010, FOURCC_BPP_H010));
  EXPECT_TRUE(TestValidFourCC(FOURCC_H210, FOURCC_BPP_H210));
  EXPECT_TRUE(TestValidFourCC(FOURCC_I010, FOURCC_BPP_I010));
  EXPECT_TRUE(TestValidFourCC(FOURCC_I210, FOURCC_BPP_I210));
  EXPECT_TRUE(TestValidFourCC(FOURCC_P010, FOURCC_BPP_P010));
  EXPECT_TRUE(TestValidFourCC(FOURCC_P210, FOURCC_BPP_P210));
  EXPECT_TRUE(TestValidFourCC(FOURCC_MJPG, FOURCC_BPP_MJPG));
  EXPECT_TRUE(TestValidFourCC(FOURCC_YV12, FOURCC_BPP_YV12));
  EXPECT_TRUE(TestValidFourCC(FOURCC_YV16, FOURCC_BPP_YV16));
  EXPECT_TRUE(TestValidFourCC(FOURCC_YV24, FOURCC_BPP_YV24));
  EXPECT_TRUE(TestValidFourCC(FOURCC_YU12, FOURCC_BPP_YU12));
  EXPECT_TRUE(TestValidFourCC(FOURCC_IYUV, FOURCC_BPP_IYUV));
  EXPECT_TRUE(TestValidFourCC(FOURCC_YU16, FOURCC_BPP_YU16));
  EXPECT_TRUE(TestValidFourCC(FOURCC_YU24, FOURCC_BPP_YU24));
  EXPECT_TRUE(TestValidFourCC(FOURCC_YUYV, FOURCC_BPP_YUYV));
  EXPECT_TRUE(TestValidFourCC(FOURCC_YUVS, FOURCC_BPP_YUVS));
  EXPECT_TRUE(TestValidFourCC(FOURCC_HDYC, FOURCC_BPP_HDYC));
  EXPECT_TRUE(TestValidFourCC(FOURCC_2VUY, FOURCC_BPP_2VUY));
  EXPECT_TRUE(TestValidFourCC(FOURCC_JPEG, FOURCC_BPP_JPEG));
  EXPECT_TRUE(TestValidFourCC(FOURCC_DMB1, FOURCC_BPP_DMB1));
  EXPECT_TRUE(TestValidFourCC(FOURCC_BA81, FOURCC_BPP_BA81));
  EXPECT_TRUE(TestValidFourCC(FOURCC_RGB3, FOURCC_BPP_RGB3));
  EXPECT_TRUE(TestValidFourCC(FOURCC_BGR3, FOURCC_BPP_BGR3));
  EXPECT_TRUE(TestValidFourCC(FOURCC_H264, FOURCC_BPP_H264));
  EXPECT_TRUE(TestValidFourCC(FOURCC_ANY, FOURCC_BPP_ANY));
}

}  // namespace libyuv
