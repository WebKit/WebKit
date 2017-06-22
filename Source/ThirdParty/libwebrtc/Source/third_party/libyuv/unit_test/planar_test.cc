/*
 *  Copyright 2011 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdlib.h>
#include <time.h>

#include "../unit_test/unit_test.h"
#include "libyuv/compare.h"
#include "libyuv/convert.h"
#include "libyuv/convert_argb.h"
#include "libyuv/convert_from.h"
#include "libyuv/convert_from_argb.h"
#include "libyuv/cpu_id.h"
#include "libyuv/planar_functions.h"
#include "libyuv/rotate.h"

namespace libyuv {

TEST_F(LibYUVPlanarTest, TestAttenuate) {
  const int kSize = 1280 * 4;
  align_buffer_page_end(orig_pixels, kSize);
  align_buffer_page_end(atten_pixels, kSize);
  align_buffer_page_end(unatten_pixels, kSize);
  align_buffer_page_end(atten2_pixels, kSize);

  // Test unattenuation clamps
  orig_pixels[0 * 4 + 0] = 200u;
  orig_pixels[0 * 4 + 1] = 129u;
  orig_pixels[0 * 4 + 2] = 127u;
  orig_pixels[0 * 4 + 3] = 128u;
  // Test unattenuation transparent and opaque are unaffected
  orig_pixels[1 * 4 + 0] = 16u;
  orig_pixels[1 * 4 + 1] = 64u;
  orig_pixels[1 * 4 + 2] = 192u;
  orig_pixels[1 * 4 + 3] = 0u;
  orig_pixels[2 * 4 + 0] = 16u;
  orig_pixels[2 * 4 + 1] = 64u;
  orig_pixels[2 * 4 + 2] = 192u;
  orig_pixels[2 * 4 + 3] = 255u;
  orig_pixels[3 * 4 + 0] = 16u;
  orig_pixels[3 * 4 + 1] = 64u;
  orig_pixels[3 * 4 + 2] = 192u;
  orig_pixels[3 * 4 + 3] = 128u;
  ARGBUnattenuate(orig_pixels, 0, unatten_pixels, 0, 4, 1);
  EXPECT_EQ(255u, unatten_pixels[0 * 4 + 0]);
  EXPECT_EQ(255u, unatten_pixels[0 * 4 + 1]);
  EXPECT_EQ(254u, unatten_pixels[0 * 4 + 2]);
  EXPECT_EQ(128u, unatten_pixels[0 * 4 + 3]);
  EXPECT_EQ(0u, unatten_pixels[1 * 4 + 0]);
  EXPECT_EQ(0u, unatten_pixels[1 * 4 + 1]);
  EXPECT_EQ(0u, unatten_pixels[1 * 4 + 2]);
  EXPECT_EQ(0u, unatten_pixels[1 * 4 + 3]);
  EXPECT_EQ(16u, unatten_pixels[2 * 4 + 0]);
  EXPECT_EQ(64u, unatten_pixels[2 * 4 + 1]);
  EXPECT_EQ(192u, unatten_pixels[2 * 4 + 2]);
  EXPECT_EQ(255u, unatten_pixels[2 * 4 + 3]);
  EXPECT_EQ(32u, unatten_pixels[3 * 4 + 0]);
  EXPECT_EQ(128u, unatten_pixels[3 * 4 + 1]);
  EXPECT_EQ(255u, unatten_pixels[3 * 4 + 2]);
  EXPECT_EQ(128u, unatten_pixels[3 * 4 + 3]);

  for (int i = 0; i < 1280; ++i) {
    orig_pixels[i * 4 + 0] = i;
    orig_pixels[i * 4 + 1] = i / 2;
    orig_pixels[i * 4 + 2] = i / 3;
    orig_pixels[i * 4 + 3] = i;
  }
  ARGBAttenuate(orig_pixels, 0, atten_pixels, 0, 1280, 1);
  ARGBUnattenuate(atten_pixels, 0, unatten_pixels, 0, 1280, 1);
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    ARGBAttenuate(unatten_pixels, 0, atten2_pixels, 0, 1280, 1);
  }
  for (int i = 0; i < 1280; ++i) {
    EXPECT_NEAR(atten_pixels[i * 4 + 0], atten2_pixels[i * 4 + 0], 2);
    EXPECT_NEAR(atten_pixels[i * 4 + 1], atten2_pixels[i * 4 + 1], 2);
    EXPECT_NEAR(atten_pixels[i * 4 + 2], atten2_pixels[i * 4 + 2], 2);
    EXPECT_NEAR(atten_pixels[i * 4 + 3], atten2_pixels[i * 4 + 3], 2);
  }
  // Make sure transparent, 50% and opaque are fully accurate.
  EXPECT_EQ(0, atten_pixels[0 * 4 + 0]);
  EXPECT_EQ(0, atten_pixels[0 * 4 + 1]);
  EXPECT_EQ(0, atten_pixels[0 * 4 + 2]);
  EXPECT_EQ(0, atten_pixels[0 * 4 + 3]);
  EXPECT_EQ(64, atten_pixels[128 * 4 + 0]);
  EXPECT_EQ(32, atten_pixels[128 * 4 + 1]);
  EXPECT_EQ(21, atten_pixels[128 * 4 + 2]);
  EXPECT_EQ(128, atten_pixels[128 * 4 + 3]);
  EXPECT_NEAR(255, atten_pixels[255 * 4 + 0], 1);
  EXPECT_NEAR(127, atten_pixels[255 * 4 + 1], 1);
  EXPECT_NEAR(85, atten_pixels[255 * 4 + 2], 1);
  EXPECT_EQ(255, atten_pixels[255 * 4 + 3]);

  free_aligned_buffer_page_end(atten2_pixels);
  free_aligned_buffer_page_end(unatten_pixels);
  free_aligned_buffer_page_end(atten_pixels);
  free_aligned_buffer_page_end(orig_pixels);
}

static int TestAttenuateI(int width,
                          int height,
                          int benchmark_iterations,
                          int disable_cpu_flags,
                          int benchmark_cpu_info,
                          int invert,
                          int off) {
  if (width < 1) {
    width = 1;
  }
  const int kBpp = 4;
  const int kStride = width * kBpp;
  align_buffer_page_end(src_argb, kStride * height + off);
  align_buffer_page_end(dst_argb_c, kStride * height);
  align_buffer_page_end(dst_argb_opt, kStride * height);
  for (int i = 0; i < kStride * height; ++i) {
    src_argb[i + off] = (fastrand() & 0xff);
  }
  memset(dst_argb_c, 0, kStride * height);
  memset(dst_argb_opt, 0, kStride * height);

  MaskCpuFlags(disable_cpu_flags);
  ARGBAttenuate(src_argb + off, kStride, dst_argb_c, kStride, width,
                invert * height);
  MaskCpuFlags(benchmark_cpu_info);
  for (int i = 0; i < benchmark_iterations; ++i) {
    ARGBAttenuate(src_argb + off, kStride, dst_argb_opt, kStride, width,
                  invert * height);
  }
  int max_diff = 0;
  for (int i = 0; i < kStride * height; ++i) {
    int abs_diff = abs(static_cast<int>(dst_argb_c[i]) -
                       static_cast<int>(dst_argb_opt[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }
  free_aligned_buffer_page_end(src_argb);
  free_aligned_buffer_page_end(dst_argb_c);
  free_aligned_buffer_page_end(dst_argb_opt);
  return max_diff;
}

TEST_F(LibYUVPlanarTest, ARGBAttenuate_Any) {
  int max_diff = TestAttenuateI(benchmark_width_ - 1, benchmark_height_,
                                benchmark_iterations_, disable_cpu_flags_,
                                benchmark_cpu_info_, +1, 0);
  EXPECT_LE(max_diff, 2);
}

TEST_F(LibYUVPlanarTest, ARGBAttenuate_Unaligned) {
  int max_diff =
      TestAttenuateI(benchmark_width_, benchmark_height_, benchmark_iterations_,
                     disable_cpu_flags_, benchmark_cpu_info_, +1, 1);
  EXPECT_LE(max_diff, 2);
}

TEST_F(LibYUVPlanarTest, ARGBAttenuate_Invert) {
  int max_diff =
      TestAttenuateI(benchmark_width_, benchmark_height_, benchmark_iterations_,
                     disable_cpu_flags_, benchmark_cpu_info_, -1, 0);
  EXPECT_LE(max_diff, 2);
}

TEST_F(LibYUVPlanarTest, ARGBAttenuate_Opt) {
  int max_diff =
      TestAttenuateI(benchmark_width_, benchmark_height_, benchmark_iterations_,
                     disable_cpu_flags_, benchmark_cpu_info_, +1, 0);
  EXPECT_LE(max_diff, 2);
}

static int TestUnattenuateI(int width,
                            int height,
                            int benchmark_iterations,
                            int disable_cpu_flags,
                            int benchmark_cpu_info,
                            int invert,
                            int off) {
  if (width < 1) {
    width = 1;
  }
  const int kBpp = 4;
  const int kStride = width * kBpp;
  align_buffer_page_end(src_argb, kStride * height + off);
  align_buffer_page_end(dst_argb_c, kStride * height);
  align_buffer_page_end(dst_argb_opt, kStride * height);
  for (int i = 0; i < kStride * height; ++i) {
    src_argb[i + off] = (fastrand() & 0xff);
  }
  ARGBAttenuate(src_argb + off, kStride, src_argb + off, kStride, width,
                height);
  memset(dst_argb_c, 0, kStride * height);
  memset(dst_argb_opt, 0, kStride * height);

  MaskCpuFlags(disable_cpu_flags);
  ARGBUnattenuate(src_argb + off, kStride, dst_argb_c, kStride, width,
                  invert * height);
  MaskCpuFlags(benchmark_cpu_info);
  for (int i = 0; i < benchmark_iterations; ++i) {
    ARGBUnattenuate(src_argb + off, kStride, dst_argb_opt, kStride, width,
                    invert * height);
  }
  int max_diff = 0;
  for (int i = 0; i < kStride * height; ++i) {
    int abs_diff = abs(static_cast<int>(dst_argb_c[i]) -
                       static_cast<int>(dst_argb_opt[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }
  free_aligned_buffer_page_end(src_argb);
  free_aligned_buffer_page_end(dst_argb_c);
  free_aligned_buffer_page_end(dst_argb_opt);
  return max_diff;
}

TEST_F(LibYUVPlanarTest, ARGBUnattenuate_Any) {
  int max_diff = TestUnattenuateI(benchmark_width_ - 1, benchmark_height_,
                                  benchmark_iterations_, disable_cpu_flags_,
                                  benchmark_cpu_info_, +1, 0);
  EXPECT_LE(max_diff, 2);
}

TEST_F(LibYUVPlanarTest, ARGBUnattenuate_Unaligned) {
  int max_diff = TestUnattenuateI(benchmark_width_, benchmark_height_,
                                  benchmark_iterations_, disable_cpu_flags_,
                                  benchmark_cpu_info_, +1, 1);
  EXPECT_LE(max_diff, 2);
}

TEST_F(LibYUVPlanarTest, ARGBUnattenuate_Invert) {
  int max_diff = TestUnattenuateI(benchmark_width_, benchmark_height_,
                                  benchmark_iterations_, disable_cpu_flags_,
                                  benchmark_cpu_info_, -1, 0);
  EXPECT_LE(max_diff, 2);
}

TEST_F(LibYUVPlanarTest, ARGBUnattenuate_Opt) {
  int max_diff = TestUnattenuateI(benchmark_width_, benchmark_height_,
                                  benchmark_iterations_, disable_cpu_flags_,
                                  benchmark_cpu_info_, +1, 0);
  EXPECT_LE(max_diff, 2);
}

TEST_F(LibYUVPlanarTest, TestARGBComputeCumulativeSum) {
  SIMD_ALIGNED(uint8 orig_pixels[16][16][4]);
  SIMD_ALIGNED(int32 added_pixels[16][16][4]);

  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 16; ++x) {
      orig_pixels[y][x][0] = 1u;
      orig_pixels[y][x][1] = 2u;
      orig_pixels[y][x][2] = 3u;
      orig_pixels[y][x][3] = 255u;
    }
  }

  ARGBComputeCumulativeSum(&orig_pixels[0][0][0], 16 * 4,
                           &added_pixels[0][0][0], 16 * 4, 16, 16);

  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 16; ++x) {
      EXPECT_EQ((x + 1) * (y + 1), added_pixels[y][x][0]);
      EXPECT_EQ((x + 1) * (y + 1) * 2, added_pixels[y][x][1]);
      EXPECT_EQ((x + 1) * (y + 1) * 3, added_pixels[y][x][2]);
      EXPECT_EQ((x + 1) * (y + 1) * 255, added_pixels[y][x][3]);
    }
  }
}

TEST_F(LibYUVPlanarTest, TestARGBGray) {
  SIMD_ALIGNED(uint8 orig_pixels[1280][4]);
  memset(orig_pixels, 0, sizeof(orig_pixels));

  // Test blue
  orig_pixels[0][0] = 255u;
  orig_pixels[0][1] = 0u;
  orig_pixels[0][2] = 0u;
  orig_pixels[0][3] = 128u;
  // Test green
  orig_pixels[1][0] = 0u;
  orig_pixels[1][1] = 255u;
  orig_pixels[1][2] = 0u;
  orig_pixels[1][3] = 0u;
  // Test red
  orig_pixels[2][0] = 0u;
  orig_pixels[2][1] = 0u;
  orig_pixels[2][2] = 255u;
  orig_pixels[2][3] = 255u;
  // Test black
  orig_pixels[3][0] = 0u;
  orig_pixels[3][1] = 0u;
  orig_pixels[3][2] = 0u;
  orig_pixels[3][3] = 255u;
  // Test white
  orig_pixels[4][0] = 255u;
  orig_pixels[4][1] = 255u;
  orig_pixels[4][2] = 255u;
  orig_pixels[4][3] = 255u;
  // Test color
  orig_pixels[5][0] = 16u;
  orig_pixels[5][1] = 64u;
  orig_pixels[5][2] = 192u;
  orig_pixels[5][3] = 224u;
  // Do 16 to test asm version.
  ARGBGray(&orig_pixels[0][0], 0, 0, 0, 16, 1);
  EXPECT_EQ(30u, orig_pixels[0][0]);
  EXPECT_EQ(30u, orig_pixels[0][1]);
  EXPECT_EQ(30u, orig_pixels[0][2]);
  EXPECT_EQ(128u, orig_pixels[0][3]);
  EXPECT_EQ(149u, orig_pixels[1][0]);
  EXPECT_EQ(149u, orig_pixels[1][1]);
  EXPECT_EQ(149u, orig_pixels[1][2]);
  EXPECT_EQ(0u, orig_pixels[1][3]);
  EXPECT_EQ(76u, orig_pixels[2][0]);
  EXPECT_EQ(76u, orig_pixels[2][1]);
  EXPECT_EQ(76u, orig_pixels[2][2]);
  EXPECT_EQ(255u, orig_pixels[2][3]);
  EXPECT_EQ(0u, orig_pixels[3][0]);
  EXPECT_EQ(0u, orig_pixels[3][1]);
  EXPECT_EQ(0u, orig_pixels[3][2]);
  EXPECT_EQ(255u, orig_pixels[3][3]);
  EXPECT_EQ(255u, orig_pixels[4][0]);
  EXPECT_EQ(255u, orig_pixels[4][1]);
  EXPECT_EQ(255u, orig_pixels[4][2]);
  EXPECT_EQ(255u, orig_pixels[4][3]);
  EXPECT_EQ(96u, orig_pixels[5][0]);
  EXPECT_EQ(96u, orig_pixels[5][1]);
  EXPECT_EQ(96u, orig_pixels[5][2]);
  EXPECT_EQ(224u, orig_pixels[5][3]);
  for (int i = 0; i < 1280; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i / 2;
    orig_pixels[i][2] = i / 3;
    orig_pixels[i][3] = i;
  }
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    ARGBGray(&orig_pixels[0][0], 0, 0, 0, 1280, 1);
  }
}

TEST_F(LibYUVPlanarTest, TestARGBGrayTo) {
  SIMD_ALIGNED(uint8 orig_pixels[1280][4]);
  SIMD_ALIGNED(uint8 gray_pixels[1280][4]);
  memset(orig_pixels, 0, sizeof(orig_pixels));

  // Test blue
  orig_pixels[0][0] = 255u;
  orig_pixels[0][1] = 0u;
  orig_pixels[0][2] = 0u;
  orig_pixels[0][3] = 128u;
  // Test green
  orig_pixels[1][0] = 0u;
  orig_pixels[1][1] = 255u;
  orig_pixels[1][2] = 0u;
  orig_pixels[1][3] = 0u;
  // Test red
  orig_pixels[2][0] = 0u;
  orig_pixels[2][1] = 0u;
  orig_pixels[2][2] = 255u;
  orig_pixels[2][3] = 255u;
  // Test black
  orig_pixels[3][0] = 0u;
  orig_pixels[3][1] = 0u;
  orig_pixels[3][2] = 0u;
  orig_pixels[3][3] = 255u;
  // Test white
  orig_pixels[4][0] = 255u;
  orig_pixels[4][1] = 255u;
  orig_pixels[4][2] = 255u;
  orig_pixels[4][3] = 255u;
  // Test color
  orig_pixels[5][0] = 16u;
  orig_pixels[5][1] = 64u;
  orig_pixels[5][2] = 192u;
  orig_pixels[5][3] = 224u;
  // Do 16 to test asm version.
  ARGBGrayTo(&orig_pixels[0][0], 0, &gray_pixels[0][0], 0, 16, 1);
  EXPECT_EQ(30u, gray_pixels[0][0]);
  EXPECT_EQ(30u, gray_pixels[0][1]);
  EXPECT_EQ(30u, gray_pixels[0][2]);
  EXPECT_EQ(128u, gray_pixels[0][3]);
  EXPECT_EQ(149u, gray_pixels[1][0]);
  EXPECT_EQ(149u, gray_pixels[1][1]);
  EXPECT_EQ(149u, gray_pixels[1][2]);
  EXPECT_EQ(0u, gray_pixels[1][3]);
  EXPECT_EQ(76u, gray_pixels[2][0]);
  EXPECT_EQ(76u, gray_pixels[2][1]);
  EXPECT_EQ(76u, gray_pixels[2][2]);
  EXPECT_EQ(255u, gray_pixels[2][3]);
  EXPECT_EQ(0u, gray_pixels[3][0]);
  EXPECT_EQ(0u, gray_pixels[3][1]);
  EXPECT_EQ(0u, gray_pixels[3][2]);
  EXPECT_EQ(255u, gray_pixels[3][3]);
  EXPECT_EQ(255u, gray_pixels[4][0]);
  EXPECT_EQ(255u, gray_pixels[4][1]);
  EXPECT_EQ(255u, gray_pixels[4][2]);
  EXPECT_EQ(255u, gray_pixels[4][3]);
  EXPECT_EQ(96u, gray_pixels[5][0]);
  EXPECT_EQ(96u, gray_pixels[5][1]);
  EXPECT_EQ(96u, gray_pixels[5][2]);
  EXPECT_EQ(224u, gray_pixels[5][3]);
  for (int i = 0; i < 1280; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i / 2;
    orig_pixels[i][2] = i / 3;
    orig_pixels[i][3] = i;
  }
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    ARGBGrayTo(&orig_pixels[0][0], 0, &gray_pixels[0][0], 0, 1280, 1);
  }
}

TEST_F(LibYUVPlanarTest, TestARGBSepia) {
  SIMD_ALIGNED(uint8 orig_pixels[1280][4]);
  memset(orig_pixels, 0, sizeof(orig_pixels));

  // Test blue
  orig_pixels[0][0] = 255u;
  orig_pixels[0][1] = 0u;
  orig_pixels[0][2] = 0u;
  orig_pixels[0][3] = 128u;
  // Test green
  orig_pixels[1][0] = 0u;
  orig_pixels[1][1] = 255u;
  orig_pixels[1][2] = 0u;
  orig_pixels[1][3] = 0u;
  // Test red
  orig_pixels[2][0] = 0u;
  orig_pixels[2][1] = 0u;
  orig_pixels[2][2] = 255u;
  orig_pixels[2][3] = 255u;
  // Test black
  orig_pixels[3][0] = 0u;
  orig_pixels[3][1] = 0u;
  orig_pixels[3][2] = 0u;
  orig_pixels[3][3] = 255u;
  // Test white
  orig_pixels[4][0] = 255u;
  orig_pixels[4][1] = 255u;
  orig_pixels[4][2] = 255u;
  orig_pixels[4][3] = 255u;
  // Test color
  orig_pixels[5][0] = 16u;
  orig_pixels[5][1] = 64u;
  orig_pixels[5][2] = 192u;
  orig_pixels[5][3] = 224u;
  // Do 16 to test asm version.
  ARGBSepia(&orig_pixels[0][0], 0, 0, 0, 16, 1);
  EXPECT_EQ(33u, orig_pixels[0][0]);
  EXPECT_EQ(43u, orig_pixels[0][1]);
  EXPECT_EQ(47u, orig_pixels[0][2]);
  EXPECT_EQ(128u, orig_pixels[0][3]);
  EXPECT_EQ(135u, orig_pixels[1][0]);
  EXPECT_EQ(175u, orig_pixels[1][1]);
  EXPECT_EQ(195u, orig_pixels[1][2]);
  EXPECT_EQ(0u, orig_pixels[1][3]);
  EXPECT_EQ(69u, orig_pixels[2][0]);
  EXPECT_EQ(89u, orig_pixels[2][1]);
  EXPECT_EQ(99u, orig_pixels[2][2]);
  EXPECT_EQ(255u, orig_pixels[2][3]);
  EXPECT_EQ(0u, orig_pixels[3][0]);
  EXPECT_EQ(0u, orig_pixels[3][1]);
  EXPECT_EQ(0u, orig_pixels[3][2]);
  EXPECT_EQ(255u, orig_pixels[3][3]);
  EXPECT_EQ(239u, orig_pixels[4][0]);
  EXPECT_EQ(255u, orig_pixels[4][1]);
  EXPECT_EQ(255u, orig_pixels[4][2]);
  EXPECT_EQ(255u, orig_pixels[4][3]);
  EXPECT_EQ(88u, orig_pixels[5][0]);
  EXPECT_EQ(114u, orig_pixels[5][1]);
  EXPECT_EQ(127u, orig_pixels[5][2]);
  EXPECT_EQ(224u, orig_pixels[5][3]);

  for (int i = 0; i < 1280; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i / 2;
    orig_pixels[i][2] = i / 3;
    orig_pixels[i][3] = i;
  }
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    ARGBSepia(&orig_pixels[0][0], 0, 0, 0, 1280, 1);
  }
}

TEST_F(LibYUVPlanarTest, TestARGBColorMatrix) {
  SIMD_ALIGNED(uint8 orig_pixels[1280][4]);
  SIMD_ALIGNED(uint8 dst_pixels_opt[1280][4]);
  SIMD_ALIGNED(uint8 dst_pixels_c[1280][4]);

  // Matrix for Sepia.
  SIMD_ALIGNED(static const int8 kRGBToSepia[]) = {
      17 / 2, 68 / 2, 35 / 2, 0, 22 / 2, 88 / 2, 45 / 2, 0,
      24 / 2, 98 / 2, 50 / 2, 0, 0,      0,      0,      64,  // Copy alpha.
  };
  memset(orig_pixels, 0, sizeof(orig_pixels));

  // Test blue
  orig_pixels[0][0] = 255u;
  orig_pixels[0][1] = 0u;
  orig_pixels[0][2] = 0u;
  orig_pixels[0][3] = 128u;
  // Test green
  orig_pixels[1][0] = 0u;
  orig_pixels[1][1] = 255u;
  orig_pixels[1][2] = 0u;
  orig_pixels[1][3] = 0u;
  // Test red
  orig_pixels[2][0] = 0u;
  orig_pixels[2][1] = 0u;
  orig_pixels[2][2] = 255u;
  orig_pixels[2][3] = 255u;
  // Test color
  orig_pixels[3][0] = 16u;
  orig_pixels[3][1] = 64u;
  orig_pixels[3][2] = 192u;
  orig_pixels[3][3] = 224u;
  // Do 16 to test asm version.
  ARGBColorMatrix(&orig_pixels[0][0], 0, &dst_pixels_opt[0][0], 0,
                  &kRGBToSepia[0], 16, 1);
  EXPECT_EQ(31u, dst_pixels_opt[0][0]);
  EXPECT_EQ(43u, dst_pixels_opt[0][1]);
  EXPECT_EQ(47u, dst_pixels_opt[0][2]);
  EXPECT_EQ(128u, dst_pixels_opt[0][3]);
  EXPECT_EQ(135u, dst_pixels_opt[1][0]);
  EXPECT_EQ(175u, dst_pixels_opt[1][1]);
  EXPECT_EQ(195u, dst_pixels_opt[1][2]);
  EXPECT_EQ(0u, dst_pixels_opt[1][3]);
  EXPECT_EQ(67u, dst_pixels_opt[2][0]);
  EXPECT_EQ(87u, dst_pixels_opt[2][1]);
  EXPECT_EQ(99u, dst_pixels_opt[2][2]);
  EXPECT_EQ(255u, dst_pixels_opt[2][3]);
  EXPECT_EQ(87u, dst_pixels_opt[3][0]);
  EXPECT_EQ(112u, dst_pixels_opt[3][1]);
  EXPECT_EQ(127u, dst_pixels_opt[3][2]);
  EXPECT_EQ(224u, dst_pixels_opt[3][3]);

  for (int i = 0; i < 1280; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i / 2;
    orig_pixels[i][2] = i / 3;
    orig_pixels[i][3] = i;
  }
  MaskCpuFlags(disable_cpu_flags_);
  ARGBColorMatrix(&orig_pixels[0][0], 0, &dst_pixels_c[0][0], 0,
                  &kRGBToSepia[0], 1280, 1);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    ARGBColorMatrix(&orig_pixels[0][0], 0, &dst_pixels_opt[0][0], 0,
                    &kRGBToSepia[0], 1280, 1);
  }

  for (int i = 0; i < 1280; ++i) {
    EXPECT_EQ(dst_pixels_c[i][0], dst_pixels_opt[i][0]);
    EXPECT_EQ(dst_pixels_c[i][1], dst_pixels_opt[i][1]);
    EXPECT_EQ(dst_pixels_c[i][2], dst_pixels_opt[i][2]);
    EXPECT_EQ(dst_pixels_c[i][3], dst_pixels_opt[i][3]);
  }
}

TEST_F(LibYUVPlanarTest, TestRGBColorMatrix) {
  SIMD_ALIGNED(uint8 orig_pixels[1280][4]);

  // Matrix for Sepia.
  SIMD_ALIGNED(static const int8 kRGBToSepia[]) = {
      17, 68, 35, 0, 22, 88, 45, 0,
      24, 98, 50, 0, 0,  0,  0,  0,  // Unused but makes matrix 16 bytes.
  };
  memset(orig_pixels, 0, sizeof(orig_pixels));

  // Test blue
  orig_pixels[0][0] = 255u;
  orig_pixels[0][1] = 0u;
  orig_pixels[0][2] = 0u;
  orig_pixels[0][3] = 128u;
  // Test green
  orig_pixels[1][0] = 0u;
  orig_pixels[1][1] = 255u;
  orig_pixels[1][2] = 0u;
  orig_pixels[1][3] = 0u;
  // Test red
  orig_pixels[2][0] = 0u;
  orig_pixels[2][1] = 0u;
  orig_pixels[2][2] = 255u;
  orig_pixels[2][3] = 255u;
  // Test color
  orig_pixels[3][0] = 16u;
  orig_pixels[3][1] = 64u;
  orig_pixels[3][2] = 192u;
  orig_pixels[3][3] = 224u;
  // Do 16 to test asm version.
  RGBColorMatrix(&orig_pixels[0][0], 0, &kRGBToSepia[0], 0, 0, 16, 1);
  EXPECT_EQ(31u, orig_pixels[0][0]);
  EXPECT_EQ(43u, orig_pixels[0][1]);
  EXPECT_EQ(47u, orig_pixels[0][2]);
  EXPECT_EQ(128u, orig_pixels[0][3]);
  EXPECT_EQ(135u, orig_pixels[1][0]);
  EXPECT_EQ(175u, orig_pixels[1][1]);
  EXPECT_EQ(195u, orig_pixels[1][2]);
  EXPECT_EQ(0u, orig_pixels[1][3]);
  EXPECT_EQ(67u, orig_pixels[2][0]);
  EXPECT_EQ(87u, orig_pixels[2][1]);
  EXPECT_EQ(99u, orig_pixels[2][2]);
  EXPECT_EQ(255u, orig_pixels[2][3]);
  EXPECT_EQ(87u, orig_pixels[3][0]);
  EXPECT_EQ(112u, orig_pixels[3][1]);
  EXPECT_EQ(127u, orig_pixels[3][2]);
  EXPECT_EQ(224u, orig_pixels[3][3]);

  for (int i = 0; i < 1280; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i / 2;
    orig_pixels[i][2] = i / 3;
    orig_pixels[i][3] = i;
  }
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    RGBColorMatrix(&orig_pixels[0][0], 0, &kRGBToSepia[0], 0, 0, 1280, 1);
  }
}

TEST_F(LibYUVPlanarTest, TestARGBColorTable) {
  SIMD_ALIGNED(uint8 orig_pixels[1280][4]);
  memset(orig_pixels, 0, sizeof(orig_pixels));

  // Matrix for Sepia.
  static const uint8 kARGBTable[256 * 4] = {
      1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u, 16u,
  };

  orig_pixels[0][0] = 0u;
  orig_pixels[0][1] = 0u;
  orig_pixels[0][2] = 0u;
  orig_pixels[0][3] = 0u;
  orig_pixels[1][0] = 1u;
  orig_pixels[1][1] = 1u;
  orig_pixels[1][2] = 1u;
  orig_pixels[1][3] = 1u;
  orig_pixels[2][0] = 2u;
  orig_pixels[2][1] = 2u;
  orig_pixels[2][2] = 2u;
  orig_pixels[2][3] = 2u;
  orig_pixels[3][0] = 0u;
  orig_pixels[3][1] = 1u;
  orig_pixels[3][2] = 2u;
  orig_pixels[3][3] = 3u;
  // Do 16 to test asm version.
  ARGBColorTable(&orig_pixels[0][0], 0, &kARGBTable[0], 0, 0, 16, 1);
  EXPECT_EQ(1u, orig_pixels[0][0]);
  EXPECT_EQ(2u, orig_pixels[0][1]);
  EXPECT_EQ(3u, orig_pixels[0][2]);
  EXPECT_EQ(4u, orig_pixels[0][3]);
  EXPECT_EQ(5u, orig_pixels[1][0]);
  EXPECT_EQ(6u, orig_pixels[1][1]);
  EXPECT_EQ(7u, orig_pixels[1][2]);
  EXPECT_EQ(8u, orig_pixels[1][3]);
  EXPECT_EQ(9u, orig_pixels[2][0]);
  EXPECT_EQ(10u, orig_pixels[2][1]);
  EXPECT_EQ(11u, orig_pixels[2][2]);
  EXPECT_EQ(12u, orig_pixels[2][3]);
  EXPECT_EQ(1u, orig_pixels[3][0]);
  EXPECT_EQ(6u, orig_pixels[3][1]);
  EXPECT_EQ(11u, orig_pixels[3][2]);
  EXPECT_EQ(16u, orig_pixels[3][3]);

  for (int i = 0; i < 1280; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i / 2;
    orig_pixels[i][2] = i / 3;
    orig_pixels[i][3] = i;
  }
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    ARGBColorTable(&orig_pixels[0][0], 0, &kARGBTable[0], 0, 0, 1280, 1);
  }
}

// Same as TestARGBColorTable except alpha does not change.
TEST_F(LibYUVPlanarTest, TestRGBColorTable) {
  SIMD_ALIGNED(uint8 orig_pixels[1280][4]);
  memset(orig_pixels, 0, sizeof(orig_pixels));

  // Matrix for Sepia.
  static const uint8 kARGBTable[256 * 4] = {
      1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u, 16u,
  };

  orig_pixels[0][0] = 0u;
  orig_pixels[0][1] = 0u;
  orig_pixels[0][2] = 0u;
  orig_pixels[0][3] = 0u;
  orig_pixels[1][0] = 1u;
  orig_pixels[1][1] = 1u;
  orig_pixels[1][2] = 1u;
  orig_pixels[1][3] = 1u;
  orig_pixels[2][0] = 2u;
  orig_pixels[2][1] = 2u;
  orig_pixels[2][2] = 2u;
  orig_pixels[2][3] = 2u;
  orig_pixels[3][0] = 0u;
  orig_pixels[3][1] = 1u;
  orig_pixels[3][2] = 2u;
  orig_pixels[3][3] = 3u;
  // Do 16 to test asm version.
  RGBColorTable(&orig_pixels[0][0], 0, &kARGBTable[0], 0, 0, 16, 1);
  EXPECT_EQ(1u, orig_pixels[0][0]);
  EXPECT_EQ(2u, orig_pixels[0][1]);
  EXPECT_EQ(3u, orig_pixels[0][2]);
  EXPECT_EQ(0u, orig_pixels[0][3]);  // Alpha unchanged.
  EXPECT_EQ(5u, orig_pixels[1][0]);
  EXPECT_EQ(6u, orig_pixels[1][1]);
  EXPECT_EQ(7u, orig_pixels[1][2]);
  EXPECT_EQ(1u, orig_pixels[1][3]);  // Alpha unchanged.
  EXPECT_EQ(9u, orig_pixels[2][0]);
  EXPECT_EQ(10u, orig_pixels[2][1]);
  EXPECT_EQ(11u, orig_pixels[2][2]);
  EXPECT_EQ(2u, orig_pixels[2][3]);  // Alpha unchanged.
  EXPECT_EQ(1u, orig_pixels[3][0]);
  EXPECT_EQ(6u, orig_pixels[3][1]);
  EXPECT_EQ(11u, orig_pixels[3][2]);
  EXPECT_EQ(3u, orig_pixels[3][3]);  // Alpha unchanged.

  for (int i = 0; i < 1280; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i / 2;
    orig_pixels[i][2] = i / 3;
    orig_pixels[i][3] = i;
  }
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    RGBColorTable(&orig_pixels[0][0], 0, &kARGBTable[0], 0, 0, 1280, 1);
  }
}

TEST_F(LibYUVPlanarTest, TestARGBQuantize) {
  SIMD_ALIGNED(uint8 orig_pixels[1280][4]);

  for (int i = 0; i < 1280; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i / 2;
    orig_pixels[i][2] = i / 3;
    orig_pixels[i][3] = i;
  }
  ARGBQuantize(&orig_pixels[0][0], 0, (65536 + (8 / 2)) / 8, 8, 8 / 2, 0, 0,
               1280, 1);

  for (int i = 0; i < 1280; ++i) {
    EXPECT_EQ((i / 8 * 8 + 8 / 2) & 255, orig_pixels[i][0]);
    EXPECT_EQ((i / 2 / 8 * 8 + 8 / 2) & 255, orig_pixels[i][1]);
    EXPECT_EQ((i / 3 / 8 * 8 + 8 / 2) & 255, orig_pixels[i][2]);
    EXPECT_EQ(i & 255, orig_pixels[i][3]);
  }
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    ARGBQuantize(&orig_pixels[0][0], 0, (65536 + (8 / 2)) / 8, 8, 8 / 2, 0, 0,
                 1280, 1);
  }
}

TEST_F(LibYUVPlanarTest, TestARGBMirror) {
  SIMD_ALIGNED(uint8 orig_pixels[1280][4]);
  SIMD_ALIGNED(uint8 dst_pixels[1280][4]);

  for (int i = 0; i < 1280; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i / 2;
    orig_pixels[i][2] = i / 3;
    orig_pixels[i][3] = i / 4;
  }
  ARGBMirror(&orig_pixels[0][0], 0, &dst_pixels[0][0], 0, 1280, 1);

  for (int i = 0; i < 1280; ++i) {
    EXPECT_EQ(i & 255, dst_pixels[1280 - 1 - i][0]);
    EXPECT_EQ((i / 2) & 255, dst_pixels[1280 - 1 - i][1]);
    EXPECT_EQ((i / 3) & 255, dst_pixels[1280 - 1 - i][2]);
    EXPECT_EQ((i / 4) & 255, dst_pixels[1280 - 1 - i][3]);
  }
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    ARGBMirror(&orig_pixels[0][0], 0, &dst_pixels[0][0], 0, 1280, 1);
  }
}

TEST_F(LibYUVPlanarTest, TestShade) {
  SIMD_ALIGNED(uint8 orig_pixels[1280][4]);
  SIMD_ALIGNED(uint8 shade_pixels[1280][4]);
  memset(orig_pixels, 0, sizeof(orig_pixels));

  orig_pixels[0][0] = 10u;
  orig_pixels[0][1] = 20u;
  orig_pixels[0][2] = 40u;
  orig_pixels[0][3] = 80u;
  orig_pixels[1][0] = 0u;
  orig_pixels[1][1] = 0u;
  orig_pixels[1][2] = 0u;
  orig_pixels[1][3] = 255u;
  orig_pixels[2][0] = 0u;
  orig_pixels[2][1] = 0u;
  orig_pixels[2][2] = 0u;
  orig_pixels[2][3] = 0u;
  orig_pixels[3][0] = 0u;
  orig_pixels[3][1] = 0u;
  orig_pixels[3][2] = 0u;
  orig_pixels[3][3] = 0u;
  // Do 8 pixels to allow opt version to be used.
  ARGBShade(&orig_pixels[0][0], 0, &shade_pixels[0][0], 0, 8, 1, 0x80ffffff);
  EXPECT_EQ(10u, shade_pixels[0][0]);
  EXPECT_EQ(20u, shade_pixels[0][1]);
  EXPECT_EQ(40u, shade_pixels[0][2]);
  EXPECT_EQ(40u, shade_pixels[0][3]);
  EXPECT_EQ(0u, shade_pixels[1][0]);
  EXPECT_EQ(0u, shade_pixels[1][1]);
  EXPECT_EQ(0u, shade_pixels[1][2]);
  EXPECT_EQ(128u, shade_pixels[1][3]);
  EXPECT_EQ(0u, shade_pixels[2][0]);
  EXPECT_EQ(0u, shade_pixels[2][1]);
  EXPECT_EQ(0u, shade_pixels[2][2]);
  EXPECT_EQ(0u, shade_pixels[2][3]);
  EXPECT_EQ(0u, shade_pixels[3][0]);
  EXPECT_EQ(0u, shade_pixels[3][1]);
  EXPECT_EQ(0u, shade_pixels[3][2]);
  EXPECT_EQ(0u, shade_pixels[3][3]);

  ARGBShade(&orig_pixels[0][0], 0, &shade_pixels[0][0], 0, 8, 1, 0x80808080);
  EXPECT_EQ(5u, shade_pixels[0][0]);
  EXPECT_EQ(10u, shade_pixels[0][1]);
  EXPECT_EQ(20u, shade_pixels[0][2]);
  EXPECT_EQ(40u, shade_pixels[0][3]);

  ARGBShade(&orig_pixels[0][0], 0, &shade_pixels[0][0], 0, 8, 1, 0x10204080);
  EXPECT_EQ(5u, shade_pixels[0][0]);
  EXPECT_EQ(5u, shade_pixels[0][1]);
  EXPECT_EQ(5u, shade_pixels[0][2]);
  EXPECT_EQ(5u, shade_pixels[0][3]);

  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    ARGBShade(&orig_pixels[0][0], 0, &shade_pixels[0][0], 0, 1280, 1,
              0x80808080);
  }
}

TEST_F(LibYUVPlanarTest, TestARGBInterpolate) {
  SIMD_ALIGNED(uint8 orig_pixels_0[1280][4]);
  SIMD_ALIGNED(uint8 orig_pixels_1[1280][4]);
  SIMD_ALIGNED(uint8 interpolate_pixels[1280][4]);
  memset(orig_pixels_0, 0, sizeof(orig_pixels_0));
  memset(orig_pixels_1, 0, sizeof(orig_pixels_1));

  orig_pixels_0[0][0] = 16u;
  orig_pixels_0[0][1] = 32u;
  orig_pixels_0[0][2] = 64u;
  orig_pixels_0[0][3] = 128u;
  orig_pixels_0[1][0] = 0u;
  orig_pixels_0[1][1] = 0u;
  orig_pixels_0[1][2] = 0u;
  orig_pixels_0[1][3] = 255u;
  orig_pixels_0[2][0] = 0u;
  orig_pixels_0[2][1] = 0u;
  orig_pixels_0[2][2] = 0u;
  orig_pixels_0[2][3] = 0u;
  orig_pixels_0[3][0] = 0u;
  orig_pixels_0[3][1] = 0u;
  orig_pixels_0[3][2] = 0u;
  orig_pixels_0[3][3] = 0u;

  orig_pixels_1[0][0] = 0u;
  orig_pixels_1[0][1] = 0u;
  orig_pixels_1[0][2] = 0u;
  orig_pixels_1[0][3] = 0u;
  orig_pixels_1[1][0] = 0u;
  orig_pixels_1[1][1] = 0u;
  orig_pixels_1[1][2] = 0u;
  orig_pixels_1[1][3] = 0u;
  orig_pixels_1[2][0] = 0u;
  orig_pixels_1[2][1] = 0u;
  orig_pixels_1[2][2] = 0u;
  orig_pixels_1[2][3] = 0u;
  orig_pixels_1[3][0] = 255u;
  orig_pixels_1[3][1] = 255u;
  orig_pixels_1[3][2] = 255u;
  orig_pixels_1[3][3] = 255u;

  ARGBInterpolate(&orig_pixels_0[0][0], 0, &orig_pixels_1[0][0], 0,
                  &interpolate_pixels[0][0], 0, 4, 1, 128);
  EXPECT_EQ(8u, interpolate_pixels[0][0]);
  EXPECT_EQ(16u, interpolate_pixels[0][1]);
  EXPECT_EQ(32u, interpolate_pixels[0][2]);
  EXPECT_EQ(64u, interpolate_pixels[0][3]);
  EXPECT_EQ(0u, interpolate_pixels[1][0]);
  EXPECT_EQ(0u, interpolate_pixels[1][1]);
  EXPECT_EQ(0u, interpolate_pixels[1][2]);
  EXPECT_EQ(128u, interpolate_pixels[1][3]);
  EXPECT_EQ(0u, interpolate_pixels[2][0]);
  EXPECT_EQ(0u, interpolate_pixels[2][1]);
  EXPECT_EQ(0u, interpolate_pixels[2][2]);
  EXPECT_EQ(0u, interpolate_pixels[2][3]);
  EXPECT_EQ(128u, interpolate_pixels[3][0]);
  EXPECT_EQ(128u, interpolate_pixels[3][1]);
  EXPECT_EQ(128u, interpolate_pixels[3][2]);
  EXPECT_EQ(128u, interpolate_pixels[3][3]);

  ARGBInterpolate(&orig_pixels_0[0][0], 0, &orig_pixels_1[0][0], 0,
                  &interpolate_pixels[0][0], 0, 4, 1, 0);
  EXPECT_EQ(16u, interpolate_pixels[0][0]);
  EXPECT_EQ(32u, interpolate_pixels[0][1]);
  EXPECT_EQ(64u, interpolate_pixels[0][2]);
  EXPECT_EQ(128u, interpolate_pixels[0][3]);

  ARGBInterpolate(&orig_pixels_0[0][0], 0, &orig_pixels_1[0][0], 0,
                  &interpolate_pixels[0][0], 0, 4, 1, 192);

  EXPECT_EQ(4u, interpolate_pixels[0][0]);
  EXPECT_EQ(8u, interpolate_pixels[0][1]);
  EXPECT_EQ(16u, interpolate_pixels[0][2]);
  EXPECT_EQ(32u, interpolate_pixels[0][3]);

  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    ARGBInterpolate(&orig_pixels_0[0][0], 0, &orig_pixels_1[0][0], 0,
                    &interpolate_pixels[0][0], 0, 1280, 1, 128);
  }
}

TEST_F(LibYUVPlanarTest, TestInterpolatePlane) {
  SIMD_ALIGNED(uint8 orig_pixels_0[1280]);
  SIMD_ALIGNED(uint8 orig_pixels_1[1280]);
  SIMD_ALIGNED(uint8 interpolate_pixels[1280]);
  memset(orig_pixels_0, 0, sizeof(orig_pixels_0));
  memset(orig_pixels_1, 0, sizeof(orig_pixels_1));

  orig_pixels_0[0] = 16u;
  orig_pixels_0[1] = 32u;
  orig_pixels_0[2] = 64u;
  orig_pixels_0[3] = 128u;
  orig_pixels_0[4] = 0u;
  orig_pixels_0[5] = 0u;
  orig_pixels_0[6] = 0u;
  orig_pixels_0[7] = 255u;
  orig_pixels_0[8] = 0u;
  orig_pixels_0[9] = 0u;
  orig_pixels_0[10] = 0u;
  orig_pixels_0[11] = 0u;
  orig_pixels_0[12] = 0u;
  orig_pixels_0[13] = 0u;
  orig_pixels_0[14] = 0u;
  orig_pixels_0[15] = 0u;

  orig_pixels_1[0] = 0u;
  orig_pixels_1[1] = 0u;
  orig_pixels_1[2] = 0u;
  orig_pixels_1[3] = 0u;
  orig_pixels_1[4] = 0u;
  orig_pixels_1[5] = 0u;
  orig_pixels_1[6] = 0u;
  orig_pixels_1[7] = 0u;
  orig_pixels_1[8] = 0u;
  orig_pixels_1[9] = 0u;
  orig_pixels_1[10] = 0u;
  orig_pixels_1[11] = 0u;
  orig_pixels_1[12] = 255u;
  orig_pixels_1[13] = 255u;
  orig_pixels_1[14] = 255u;
  orig_pixels_1[15] = 255u;

  InterpolatePlane(&orig_pixels_0[0], 0, &orig_pixels_1[0], 0,
                   &interpolate_pixels[0], 0, 16, 1, 128);
  EXPECT_EQ(8u, interpolate_pixels[0]);
  EXPECT_EQ(16u, interpolate_pixels[1]);
  EXPECT_EQ(32u, interpolate_pixels[2]);
  EXPECT_EQ(64u, interpolate_pixels[3]);
  EXPECT_EQ(0u, interpolate_pixels[4]);
  EXPECT_EQ(0u, interpolate_pixels[5]);
  EXPECT_EQ(0u, interpolate_pixels[6]);
  EXPECT_EQ(128u, interpolate_pixels[7]);
  EXPECT_EQ(0u, interpolate_pixels[8]);
  EXPECT_EQ(0u, interpolate_pixels[9]);
  EXPECT_EQ(0u, interpolate_pixels[10]);
  EXPECT_EQ(0u, interpolate_pixels[11]);
  EXPECT_EQ(128u, interpolate_pixels[12]);
  EXPECT_EQ(128u, interpolate_pixels[13]);
  EXPECT_EQ(128u, interpolate_pixels[14]);
  EXPECT_EQ(128u, interpolate_pixels[15]);

  InterpolatePlane(&orig_pixels_0[0], 0, &orig_pixels_1[0], 0,
                   &interpolate_pixels[0], 0, 16, 1, 0);
  EXPECT_EQ(16u, interpolate_pixels[0]);
  EXPECT_EQ(32u, interpolate_pixels[1]);
  EXPECT_EQ(64u, interpolate_pixels[2]);
  EXPECT_EQ(128u, interpolate_pixels[3]);

  InterpolatePlane(&orig_pixels_0[0], 0, &orig_pixels_1[0], 0,
                   &interpolate_pixels[0], 0, 16, 1, 192);

  EXPECT_EQ(4u, interpolate_pixels[0]);
  EXPECT_EQ(8u, interpolate_pixels[1]);
  EXPECT_EQ(16u, interpolate_pixels[2]);
  EXPECT_EQ(32u, interpolate_pixels[3]);

  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    InterpolatePlane(&orig_pixels_0[0], 0, &orig_pixels_1[0], 0,
                     &interpolate_pixels[0], 0, 1280, 1, 123);
  }
}

#define TESTTERP(FMT_A, BPP_A, STRIDE_A, FMT_B, BPP_B, STRIDE_B, W1280, TERP, \
                 N, NEG, OFF)                                                 \
  TEST_F(LibYUVPlanarTest, ARGBInterpolate##TERP##N) {                        \
    const int kWidth = ((W1280) > 0) ? (W1280) : 1;                           \
    const int kHeight = benchmark_height_;                                    \
    const int kStrideA =                                                      \
        (kWidth * BPP_A + STRIDE_A - 1) / STRIDE_A * STRIDE_A;                \
    const int kStrideB =                                                      \
        (kWidth * BPP_B + STRIDE_B - 1) / STRIDE_B * STRIDE_B;                \
    align_buffer_page_end(src_argb_a, kStrideA* kHeight + OFF);               \
    align_buffer_page_end(src_argb_b, kStrideA* kHeight + OFF);               \
    align_buffer_page_end(dst_argb_c, kStrideB* kHeight);                     \
    align_buffer_page_end(dst_argb_opt, kStrideB* kHeight);                   \
    for (int i = 0; i < kStrideA * kHeight; ++i) {                            \
      src_argb_a[i + OFF] = (fastrand() & 0xff);                              \
      src_argb_b[i + OFF] = (fastrand() & 0xff);                              \
    }                                                                         \
    MaskCpuFlags(disable_cpu_flags_);                                         \
    ARGBInterpolate(src_argb_a + OFF, kStrideA, src_argb_b + OFF, kStrideA,   \
                    dst_argb_c, kStrideB, kWidth, NEG kHeight, TERP);         \
    MaskCpuFlags(benchmark_cpu_info_);                                        \
    for (int i = 0; i < benchmark_iterations_; ++i) {                         \
      ARGBInterpolate(src_argb_a + OFF, kStrideA, src_argb_b + OFF, kStrideA, \
                      dst_argb_opt, kStrideB, kWidth, NEG kHeight, TERP);     \
    }                                                                         \
    for (int i = 0; i < kStrideB * kHeight; ++i) {                            \
      EXPECT_EQ(dst_argb_c[i], dst_argb_opt[i]);                              \
    }                                                                         \
    free_aligned_buffer_page_end(src_argb_a);                                 \
    free_aligned_buffer_page_end(src_argb_b);                                 \
    free_aligned_buffer_page_end(dst_argb_c);                                 \
    free_aligned_buffer_page_end(dst_argb_opt);                               \
  }

#define TESTINTERPOLATE(TERP)                                                \
  TESTTERP(ARGB, 4, 1, ARGB, 4, 1, benchmark_width_ - 1, TERP, _Any, +, 0)   \
  TESTTERP(ARGB, 4, 1, ARGB, 4, 1, benchmark_width_, TERP, _Unaligned, +, 1) \
  TESTTERP(ARGB, 4, 1, ARGB, 4, 1, benchmark_width_, TERP, _Invert, -, 0)    \
  TESTTERP(ARGB, 4, 1, ARGB, 4, 1, benchmark_width_, TERP, _Opt, +, 0)

TESTINTERPOLATE(0)
TESTINTERPOLATE(64)
TESTINTERPOLATE(128)
TESTINTERPOLATE(192)
TESTINTERPOLATE(255)

static int TestBlend(int width,
                     int height,
                     int benchmark_iterations,
                     int disable_cpu_flags,
                     int benchmark_cpu_info,
                     int invert,
                     int off) {
  if (width < 1) {
    width = 1;
  }
  const int kBpp = 4;
  const int kStride = width * kBpp;
  align_buffer_page_end(src_argb_a, kStride * height + off);
  align_buffer_page_end(src_argb_b, kStride * height + off);
  align_buffer_page_end(dst_argb_c, kStride * height);
  align_buffer_page_end(dst_argb_opt, kStride * height);
  for (int i = 0; i < kStride * height; ++i) {
    src_argb_a[i + off] = (fastrand() & 0xff);
    src_argb_b[i + off] = (fastrand() & 0xff);
  }
  ARGBAttenuate(src_argb_a + off, kStride, src_argb_a + off, kStride, width,
                height);
  ARGBAttenuate(src_argb_b + off, kStride, src_argb_b + off, kStride, width,
                height);
  memset(dst_argb_c, 255, kStride * height);
  memset(dst_argb_opt, 255, kStride * height);

  MaskCpuFlags(disable_cpu_flags);
  ARGBBlend(src_argb_a + off, kStride, src_argb_b + off, kStride, dst_argb_c,
            kStride, width, invert * height);
  MaskCpuFlags(benchmark_cpu_info);
  for (int i = 0; i < benchmark_iterations; ++i) {
    ARGBBlend(src_argb_a + off, kStride, src_argb_b + off, kStride,
              dst_argb_opt, kStride, width, invert * height);
  }
  int max_diff = 0;
  for (int i = 0; i < kStride * height; ++i) {
    int abs_diff = abs(static_cast<int>(dst_argb_c[i]) -
                       static_cast<int>(dst_argb_opt[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }
  free_aligned_buffer_page_end(src_argb_a);
  free_aligned_buffer_page_end(src_argb_b);
  free_aligned_buffer_page_end(dst_argb_c);
  free_aligned_buffer_page_end(dst_argb_opt);
  return max_diff;
}

TEST_F(LibYUVPlanarTest, ARGBBlend_Any) {
  int max_diff =
      TestBlend(benchmark_width_ - 4, benchmark_height_, benchmark_iterations_,
                disable_cpu_flags_, benchmark_cpu_info_, +1, 0);
  EXPECT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, ARGBBlend_Unaligned) {
  int max_diff =
      TestBlend(benchmark_width_, benchmark_height_, benchmark_iterations_,
                disable_cpu_flags_, benchmark_cpu_info_, +1, 1);
  EXPECT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, ARGBBlend_Invert) {
  int max_diff =
      TestBlend(benchmark_width_, benchmark_height_, benchmark_iterations_,
                disable_cpu_flags_, benchmark_cpu_info_, -1, 0);
  EXPECT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, ARGBBlend_Opt) {
  int max_diff =
      TestBlend(benchmark_width_, benchmark_height_, benchmark_iterations_,
                disable_cpu_flags_, benchmark_cpu_info_, +1, 0);
  EXPECT_LE(max_diff, 1);
}

static void TestBlendPlane(int width,
                           int height,
                           int benchmark_iterations,
                           int disable_cpu_flags,
                           int benchmark_cpu_info,
                           int invert,
                           int off) {
  if (width < 1) {
    width = 1;
  }
  const int kBpp = 1;
  const int kStride = width * kBpp;
  align_buffer_page_end(src_argb_a, kStride * height + off);
  align_buffer_page_end(src_argb_b, kStride * height + off);
  align_buffer_page_end(src_argb_alpha, kStride * height + off);
  align_buffer_page_end(dst_argb_c, kStride * height + off);
  align_buffer_page_end(dst_argb_opt, kStride * height + off);
  memset(dst_argb_c, 255, kStride * height + off);
  memset(dst_argb_opt, 255, kStride * height + off);

  // Test source is maintained exactly if alpha is 255.
  for (int i = 0; i < width; ++i) {
    src_argb_a[i + off] = i & 255;
    src_argb_b[i + off] = 255 - (i & 255);
  }
  memset(src_argb_alpha + off, 255, width);
  BlendPlane(src_argb_a + off, width, src_argb_b + off, width,
             src_argb_alpha + off, width, dst_argb_opt + off, width, width, 1);
  for (int i = 0; i < width; ++i) {
    EXPECT_EQ(src_argb_a[i + off], dst_argb_opt[i + off]);
  }
  // Test destination is maintained exactly if alpha is 0.
  memset(src_argb_alpha + off, 0, width);
  BlendPlane(src_argb_a + off, width, src_argb_b + off, width,
             src_argb_alpha + off, width, dst_argb_opt + off, width, width, 1);
  for (int i = 0; i < width; ++i) {
    EXPECT_EQ(src_argb_b[i + off], dst_argb_opt[i + off]);
  }
  for (int i = 0; i < kStride * height; ++i) {
    src_argb_a[i + off] = (fastrand() & 0xff);
    src_argb_b[i + off] = (fastrand() & 0xff);
    src_argb_alpha[i + off] = (fastrand() & 0xff);
  }

  MaskCpuFlags(disable_cpu_flags);
  BlendPlane(src_argb_a + off, width, src_argb_b + off, width,
             src_argb_alpha + off, width, dst_argb_c + off, width, width,
             invert * height);
  MaskCpuFlags(benchmark_cpu_info);
  for (int i = 0; i < benchmark_iterations; ++i) {
    BlendPlane(src_argb_a + off, width, src_argb_b + off, width,
               src_argb_alpha + off, width, dst_argb_opt + off, width, width,
               invert * height);
  }
  for (int i = 0; i < kStride * height; ++i) {
    EXPECT_EQ(dst_argb_c[i + off], dst_argb_opt[i + off]);
  }
  free_aligned_buffer_page_end(src_argb_a);
  free_aligned_buffer_page_end(src_argb_b);
  free_aligned_buffer_page_end(src_argb_alpha);
  free_aligned_buffer_page_end(dst_argb_c);
  free_aligned_buffer_page_end(dst_argb_opt);
  return;
}

TEST_F(LibYUVPlanarTest, BlendPlane_Opt) {
  TestBlendPlane(benchmark_width_, benchmark_height_, benchmark_iterations_,
                 disable_cpu_flags_, benchmark_cpu_info_, +1, 0);
}
TEST_F(LibYUVPlanarTest, BlendPlane_Unaligned) {
  TestBlendPlane(benchmark_width_, benchmark_height_, benchmark_iterations_,
                 disable_cpu_flags_, benchmark_cpu_info_, +1, 1);
}
TEST_F(LibYUVPlanarTest, BlendPlane_Any) {
  TestBlendPlane(benchmark_width_ - 4, benchmark_height_, benchmark_iterations_,
                 disable_cpu_flags_, benchmark_cpu_info_, +1, 1);
}
TEST_F(LibYUVPlanarTest, BlendPlane_Invert) {
  TestBlendPlane(benchmark_width_, benchmark_height_, benchmark_iterations_,
                 disable_cpu_flags_, benchmark_cpu_info_, -1, 1);
}

#define SUBSAMPLE(v, a) ((((v) + (a)-1)) / (a))

static void TestI420Blend(int width,
                          int height,
                          int benchmark_iterations,
                          int disable_cpu_flags,
                          int benchmark_cpu_info,
                          int invert,
                          int off) {
  width = ((width) > 0) ? (width) : 1;
  const int kStrideUV = SUBSAMPLE(width, 2);
  const int kSizeUV = kStrideUV * SUBSAMPLE(height, 2);
  align_buffer_page_end(src_y0, width * height + off);
  align_buffer_page_end(src_u0, kSizeUV + off);
  align_buffer_page_end(src_v0, kSizeUV + off);
  align_buffer_page_end(src_y1, width * height + off);
  align_buffer_page_end(src_u1, kSizeUV + off);
  align_buffer_page_end(src_v1, kSizeUV + off);
  align_buffer_page_end(src_a, width * height + off);
  align_buffer_page_end(dst_y_c, width * height + off);
  align_buffer_page_end(dst_u_c, kSizeUV + off);
  align_buffer_page_end(dst_v_c, kSizeUV + off);
  align_buffer_page_end(dst_y_opt, width * height + off);
  align_buffer_page_end(dst_u_opt, kSizeUV + off);
  align_buffer_page_end(dst_v_opt, kSizeUV + off);

  MemRandomize(src_y0, width * height + off);
  MemRandomize(src_u0, kSizeUV + off);
  MemRandomize(src_v0, kSizeUV + off);
  MemRandomize(src_y1, width * height + off);
  MemRandomize(src_u1, kSizeUV + off);
  MemRandomize(src_v1, kSizeUV + off);
  MemRandomize(src_a, width * height + off);
  memset(dst_y_c, 255, width * height + off);
  memset(dst_u_c, 255, kSizeUV + off);
  memset(dst_v_c, 255, kSizeUV + off);
  memset(dst_y_opt, 255, width * height + off);
  memset(dst_u_opt, 255, kSizeUV + off);
  memset(dst_v_opt, 255, kSizeUV + off);

  MaskCpuFlags(disable_cpu_flags);
  I420Blend(src_y0 + off, width, src_u0 + off, kStrideUV, src_v0 + off,
            kStrideUV, src_y1 + off, width, src_u1 + off, kStrideUV,
            src_v1 + off, kStrideUV, src_a + off, width, dst_y_c + off, width,
            dst_u_c + off, kStrideUV, dst_v_c + off, kStrideUV, width,
            invert * height);
  MaskCpuFlags(benchmark_cpu_info);
  for (int i = 0; i < benchmark_iterations; ++i) {
    I420Blend(src_y0 + off, width, src_u0 + off, kStrideUV, src_v0 + off,
              kStrideUV, src_y1 + off, width, src_u1 + off, kStrideUV,
              src_v1 + off, kStrideUV, src_a + off, width, dst_y_opt + off,
              width, dst_u_opt + off, kStrideUV, dst_v_opt + off, kStrideUV,
              width, invert * height);
  }
  for (int i = 0; i < width * height; ++i) {
    EXPECT_EQ(dst_y_c[i + off], dst_y_opt[i + off]);
  }
  for (int i = 0; i < kSizeUV; ++i) {
    EXPECT_EQ(dst_u_c[i + off], dst_u_opt[i + off]);
    EXPECT_EQ(dst_v_c[i + off], dst_v_opt[i + off]);
  }
  free_aligned_buffer_page_end(src_y0);
  free_aligned_buffer_page_end(src_u0);
  free_aligned_buffer_page_end(src_v0);
  free_aligned_buffer_page_end(src_y1);
  free_aligned_buffer_page_end(src_u1);
  free_aligned_buffer_page_end(src_v1);
  free_aligned_buffer_page_end(src_a);
  free_aligned_buffer_page_end(dst_y_c);
  free_aligned_buffer_page_end(dst_u_c);
  free_aligned_buffer_page_end(dst_v_c);
  free_aligned_buffer_page_end(dst_y_opt);
  free_aligned_buffer_page_end(dst_u_opt);
  free_aligned_buffer_page_end(dst_v_opt);
  return;
}

TEST_F(LibYUVPlanarTest, I420Blend_Opt) {
  TestI420Blend(benchmark_width_, benchmark_height_, benchmark_iterations_,
                disable_cpu_flags_, benchmark_cpu_info_, +1, 0);
}
TEST_F(LibYUVPlanarTest, I420Blend_Unaligned) {
  TestI420Blend(benchmark_width_, benchmark_height_, benchmark_iterations_,
                disable_cpu_flags_, benchmark_cpu_info_, +1, 1);
}

// TODO(fbarchard): DISABLED because _Any uses C.  Avoid C and re-enable.
TEST_F(LibYUVPlanarTest, DISABLED_I420Blend_Any) {
  TestI420Blend(benchmark_width_ - 4, benchmark_height_, benchmark_iterations_,
                disable_cpu_flags_, benchmark_cpu_info_, +1, 0);
}
TEST_F(LibYUVPlanarTest, I420Blend_Invert) {
  TestI420Blend(benchmark_width_, benchmark_height_, benchmark_iterations_,
                disable_cpu_flags_, benchmark_cpu_info_, -1, 0);
}

TEST_F(LibYUVPlanarTest, TestAffine) {
  SIMD_ALIGNED(uint8 orig_pixels_0[1280][4]);
  SIMD_ALIGNED(uint8 interpolate_pixels_C[1280][4]);

  for (int i = 0; i < 1280; ++i) {
    for (int j = 0; j < 4; ++j) {
      orig_pixels_0[i][j] = i;
    }
  }

  float uv_step[4] = {0.f, 0.f, 0.75f, 0.f};

  ARGBAffineRow_C(&orig_pixels_0[0][0], 0, &interpolate_pixels_C[0][0], uv_step,
                  1280);
  EXPECT_EQ(0u, interpolate_pixels_C[0][0]);
  EXPECT_EQ(96u, interpolate_pixels_C[128][0]);
  EXPECT_EQ(191u, interpolate_pixels_C[255][3]);

#if defined(HAS_ARGBAFFINEROW_SSE2)
  SIMD_ALIGNED(uint8 interpolate_pixels_Opt[1280][4]);
  ARGBAffineRow_SSE2(&orig_pixels_0[0][0], 0, &interpolate_pixels_Opt[0][0],
                     uv_step, 1280);
  EXPECT_EQ(0, memcmp(interpolate_pixels_Opt, interpolate_pixels_C, 1280 * 4));

  int has_sse2 = TestCpuFlag(kCpuHasSSE2);
  if (has_sse2) {
    for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
      ARGBAffineRow_SSE2(&orig_pixels_0[0][0], 0, &interpolate_pixels_Opt[0][0],
                         uv_step, 1280);
    }
  }
#endif
}

TEST_F(LibYUVPlanarTest, TestCopyPlane) {
  int err = 0;
  int yw = benchmark_width_;
  int yh = benchmark_height_;
  int b = 12;
  int i, j;

  int y_plane_size = (yw + b * 2) * (yh + b * 2);
  align_buffer_page_end(orig_y, y_plane_size);
  align_buffer_page_end(dst_c, y_plane_size);
  align_buffer_page_end(dst_opt, y_plane_size);

  memset(orig_y, 0, y_plane_size);
  memset(dst_c, 0, y_plane_size);
  memset(dst_opt, 0, y_plane_size);

  // Fill image buffers with random data.
  for (i = b; i < (yh + b); ++i) {
    for (j = b; j < (yw + b); ++j) {
      orig_y[i * (yw + b * 2) + j] = fastrand() & 0xff;
    }
  }

  // Fill destination buffers with random data.
  for (i = 0; i < y_plane_size; ++i) {
    uint8 random_number = fastrand() & 0x7f;
    dst_c[i] = random_number;
    dst_opt[i] = dst_c[i];
  }

  int y_off = b * (yw + b * 2) + b;

  int y_st = yw + b * 2;
  int stride = 8;

  // Disable all optimizations.
  MaskCpuFlags(disable_cpu_flags_);
  for (j = 0; j < benchmark_iterations_; j++) {
    CopyPlane(orig_y + y_off, y_st, dst_c + y_off, stride, yw, yh);
  }

  // Enable optimizations.
  MaskCpuFlags(benchmark_cpu_info_);
  for (j = 0; j < benchmark_iterations_; j++) {
    CopyPlane(orig_y + y_off, y_st, dst_opt + y_off, stride, yw, yh);
  }

  for (i = 0; i < y_plane_size; ++i) {
    if (dst_c[i] != dst_opt[i])
      ++err;
  }

  free_aligned_buffer_page_end(orig_y);
  free_aligned_buffer_page_end(dst_c);
  free_aligned_buffer_page_end(dst_opt);

  EXPECT_EQ(0, err);
}

static int TestMultiply(int width,
                        int height,
                        int benchmark_iterations,
                        int disable_cpu_flags,
                        int benchmark_cpu_info,
                        int invert,
                        int off) {
  if (width < 1) {
    width = 1;
  }
  const int kBpp = 4;
  const int kStride = width * kBpp;
  align_buffer_page_end(src_argb_a, kStride * height + off);
  align_buffer_page_end(src_argb_b, kStride * height + off);
  align_buffer_page_end(dst_argb_c, kStride * height);
  align_buffer_page_end(dst_argb_opt, kStride * height);
  for (int i = 0; i < kStride * height; ++i) {
    src_argb_a[i + off] = (fastrand() & 0xff);
    src_argb_b[i + off] = (fastrand() & 0xff);
  }
  memset(dst_argb_c, 0, kStride * height);
  memset(dst_argb_opt, 0, kStride * height);

  MaskCpuFlags(disable_cpu_flags);
  ARGBMultiply(src_argb_a + off, kStride, src_argb_b + off, kStride, dst_argb_c,
               kStride, width, invert * height);
  MaskCpuFlags(benchmark_cpu_info);
  for (int i = 0; i < benchmark_iterations; ++i) {
    ARGBMultiply(src_argb_a + off, kStride, src_argb_b + off, kStride,
                 dst_argb_opt, kStride, width, invert * height);
  }
  int max_diff = 0;
  for (int i = 0; i < kStride * height; ++i) {
    int abs_diff = abs(static_cast<int>(dst_argb_c[i]) -
                       static_cast<int>(dst_argb_opt[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }
  free_aligned_buffer_page_end(src_argb_a);
  free_aligned_buffer_page_end(src_argb_b);
  free_aligned_buffer_page_end(dst_argb_c);
  free_aligned_buffer_page_end(dst_argb_opt);
  return max_diff;
}

TEST_F(LibYUVPlanarTest, ARGBMultiply_Any) {
  int max_diff = TestMultiply(benchmark_width_ - 1, benchmark_height_,
                              benchmark_iterations_, disable_cpu_flags_,
                              benchmark_cpu_info_, +1, 0);
  EXPECT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, ARGBMultiply_Unaligned) {
  int max_diff =
      TestMultiply(benchmark_width_, benchmark_height_, benchmark_iterations_,
                   disable_cpu_flags_, benchmark_cpu_info_, +1, 1);
  EXPECT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, ARGBMultiply_Invert) {
  int max_diff =
      TestMultiply(benchmark_width_, benchmark_height_, benchmark_iterations_,
                   disable_cpu_flags_, benchmark_cpu_info_, -1, 0);
  EXPECT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, ARGBMultiply_Opt) {
  int max_diff =
      TestMultiply(benchmark_width_, benchmark_height_, benchmark_iterations_,
                   disable_cpu_flags_, benchmark_cpu_info_, +1, 0);
  EXPECT_LE(max_diff, 1);
}

static int TestAdd(int width,
                   int height,
                   int benchmark_iterations,
                   int disable_cpu_flags,
                   int benchmark_cpu_info,
                   int invert,
                   int off) {
  if (width < 1) {
    width = 1;
  }
  const int kBpp = 4;
  const int kStride = width * kBpp;
  align_buffer_page_end(src_argb_a, kStride * height + off);
  align_buffer_page_end(src_argb_b, kStride * height + off);
  align_buffer_page_end(dst_argb_c, kStride * height);
  align_buffer_page_end(dst_argb_opt, kStride * height);
  for (int i = 0; i < kStride * height; ++i) {
    src_argb_a[i + off] = (fastrand() & 0xff);
    src_argb_b[i + off] = (fastrand() & 0xff);
  }
  memset(dst_argb_c, 0, kStride * height);
  memset(dst_argb_opt, 0, kStride * height);

  MaskCpuFlags(disable_cpu_flags);
  ARGBAdd(src_argb_a + off, kStride, src_argb_b + off, kStride, dst_argb_c,
          kStride, width, invert * height);
  MaskCpuFlags(benchmark_cpu_info);
  for (int i = 0; i < benchmark_iterations; ++i) {
    ARGBAdd(src_argb_a + off, kStride, src_argb_b + off, kStride, dst_argb_opt,
            kStride, width, invert * height);
  }
  int max_diff = 0;
  for (int i = 0; i < kStride * height; ++i) {
    int abs_diff = abs(static_cast<int>(dst_argb_c[i]) -
                       static_cast<int>(dst_argb_opt[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }
  free_aligned_buffer_page_end(src_argb_a);
  free_aligned_buffer_page_end(src_argb_b);
  free_aligned_buffer_page_end(dst_argb_c);
  free_aligned_buffer_page_end(dst_argb_opt);
  return max_diff;
}

TEST_F(LibYUVPlanarTest, ARGBAdd_Any) {
  int max_diff =
      TestAdd(benchmark_width_ - 1, benchmark_height_, benchmark_iterations_,
              disable_cpu_flags_, benchmark_cpu_info_, +1, 0);
  EXPECT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, ARGBAdd_Unaligned) {
  int max_diff =
      TestAdd(benchmark_width_, benchmark_height_, benchmark_iterations_,
              disable_cpu_flags_, benchmark_cpu_info_, +1, 1);
  EXPECT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, ARGBAdd_Invert) {
  int max_diff =
      TestAdd(benchmark_width_, benchmark_height_, benchmark_iterations_,
              disable_cpu_flags_, benchmark_cpu_info_, -1, 0);
  EXPECT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, ARGBAdd_Opt) {
  int max_diff =
      TestAdd(benchmark_width_, benchmark_height_, benchmark_iterations_,
              disable_cpu_flags_, benchmark_cpu_info_, +1, 0);
  EXPECT_LE(max_diff, 1);
}

static int TestSubtract(int width,
                        int height,
                        int benchmark_iterations,
                        int disable_cpu_flags,
                        int benchmark_cpu_info,
                        int invert,
                        int off) {
  if (width < 1) {
    width = 1;
  }
  const int kBpp = 4;
  const int kStride = width * kBpp;
  align_buffer_page_end(src_argb_a, kStride * height + off);
  align_buffer_page_end(src_argb_b, kStride * height + off);
  align_buffer_page_end(dst_argb_c, kStride * height);
  align_buffer_page_end(dst_argb_opt, kStride * height);
  for (int i = 0; i < kStride * height; ++i) {
    src_argb_a[i + off] = (fastrand() & 0xff);
    src_argb_b[i + off] = (fastrand() & 0xff);
  }
  memset(dst_argb_c, 0, kStride * height);
  memset(dst_argb_opt, 0, kStride * height);

  MaskCpuFlags(disable_cpu_flags);
  ARGBSubtract(src_argb_a + off, kStride, src_argb_b + off, kStride, dst_argb_c,
               kStride, width, invert * height);
  MaskCpuFlags(benchmark_cpu_info);
  for (int i = 0; i < benchmark_iterations; ++i) {
    ARGBSubtract(src_argb_a + off, kStride, src_argb_b + off, kStride,
                 dst_argb_opt, kStride, width, invert * height);
  }
  int max_diff = 0;
  for (int i = 0; i < kStride * height; ++i) {
    int abs_diff = abs(static_cast<int>(dst_argb_c[i]) -
                       static_cast<int>(dst_argb_opt[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }
  free_aligned_buffer_page_end(src_argb_a);
  free_aligned_buffer_page_end(src_argb_b);
  free_aligned_buffer_page_end(dst_argb_c);
  free_aligned_buffer_page_end(dst_argb_opt);
  return max_diff;
}

TEST_F(LibYUVPlanarTest, ARGBSubtract_Any) {
  int max_diff = TestSubtract(benchmark_width_ - 1, benchmark_height_,
                              benchmark_iterations_, disable_cpu_flags_,
                              benchmark_cpu_info_, +1, 0);
  EXPECT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, ARGBSubtract_Unaligned) {
  int max_diff =
      TestSubtract(benchmark_width_, benchmark_height_, benchmark_iterations_,
                   disable_cpu_flags_, benchmark_cpu_info_, +1, 1);
  EXPECT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, ARGBSubtract_Invert) {
  int max_diff =
      TestSubtract(benchmark_width_, benchmark_height_, benchmark_iterations_,
                   disable_cpu_flags_, benchmark_cpu_info_, -1, 0);
  EXPECT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, ARGBSubtract_Opt) {
  int max_diff =
      TestSubtract(benchmark_width_, benchmark_height_, benchmark_iterations_,
                   disable_cpu_flags_, benchmark_cpu_info_, +1, 0);
  EXPECT_LE(max_diff, 1);
}

static int TestSobel(int width,
                     int height,
                     int benchmark_iterations,
                     int disable_cpu_flags,
                     int benchmark_cpu_info,
                     int invert,
                     int off) {
  if (width < 1) {
    width = 1;
  }
  const int kBpp = 4;
  const int kStride = width * kBpp;
  align_buffer_page_end(src_argb_a, kStride * height + off);
  align_buffer_page_end(dst_argb_c, kStride * height);
  align_buffer_page_end(dst_argb_opt, kStride * height);
  memset(src_argb_a, 0, kStride * height + off);
  for (int i = 0; i < kStride * height; ++i) {
    src_argb_a[i + off] = (fastrand() & 0xff);
  }
  memset(dst_argb_c, 0, kStride * height);
  memset(dst_argb_opt, 0, kStride * height);

  MaskCpuFlags(disable_cpu_flags);
  ARGBSobel(src_argb_a + off, kStride, dst_argb_c, kStride, width,
            invert * height);
  MaskCpuFlags(benchmark_cpu_info);
  for (int i = 0; i < benchmark_iterations; ++i) {
    ARGBSobel(src_argb_a + off, kStride, dst_argb_opt, kStride, width,
              invert * height);
  }
  int max_diff = 0;
  for (int i = 0; i < kStride * height; ++i) {
    int abs_diff = abs(static_cast<int>(dst_argb_c[i]) -
                       static_cast<int>(dst_argb_opt[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }
  free_aligned_buffer_page_end(src_argb_a);
  free_aligned_buffer_page_end(dst_argb_c);
  free_aligned_buffer_page_end(dst_argb_opt);
  return max_diff;
}

TEST_F(LibYUVPlanarTest, ARGBSobel_Any) {
  int max_diff =
      TestSobel(benchmark_width_ - 1, benchmark_height_, benchmark_iterations_,
                disable_cpu_flags_, benchmark_cpu_info_, +1, 0);
  EXPECT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, ARGBSobel_Unaligned) {
  int max_diff =
      TestSobel(benchmark_width_, benchmark_height_, benchmark_iterations_,
                disable_cpu_flags_, benchmark_cpu_info_, +1, 1);
  EXPECT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, ARGBSobel_Invert) {
  int max_diff =
      TestSobel(benchmark_width_, benchmark_height_, benchmark_iterations_,
                disable_cpu_flags_, benchmark_cpu_info_, -1, 0);
  EXPECT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, ARGBSobel_Opt) {
  int max_diff =
      TestSobel(benchmark_width_, benchmark_height_, benchmark_iterations_,
                disable_cpu_flags_, benchmark_cpu_info_, +1, 0);
  EXPECT_EQ(0, max_diff);
}

static int TestSobelToPlane(int width,
                            int height,
                            int benchmark_iterations,
                            int disable_cpu_flags,
                            int benchmark_cpu_info,
                            int invert,
                            int off) {
  if (width < 1) {
    width = 1;
  }
  const int kSrcBpp = 4;
  const int kDstBpp = 1;
  const int kSrcStride = (width * kSrcBpp + 15) & ~15;
  const int kDstStride = (width * kDstBpp + 15) & ~15;
  align_buffer_page_end(src_argb_a, kSrcStride * height + off);
  align_buffer_page_end(dst_argb_c, kDstStride * height);
  align_buffer_page_end(dst_argb_opt, kDstStride * height);
  memset(src_argb_a, 0, kSrcStride * height + off);
  for (int i = 0; i < kSrcStride * height; ++i) {
    src_argb_a[i + off] = (fastrand() & 0xff);
  }
  memset(dst_argb_c, 0, kDstStride * height);
  memset(dst_argb_opt, 0, kDstStride * height);

  MaskCpuFlags(disable_cpu_flags);
  ARGBSobelToPlane(src_argb_a + off, kSrcStride, dst_argb_c, kDstStride, width,
                   invert * height);
  MaskCpuFlags(benchmark_cpu_info);
  for (int i = 0; i < benchmark_iterations; ++i) {
    ARGBSobelToPlane(src_argb_a + off, kSrcStride, dst_argb_opt, kDstStride,
                     width, invert * height);
  }
  int max_diff = 0;
  for (int i = 0; i < kDstStride * height; ++i) {
    int abs_diff = abs(static_cast<int>(dst_argb_c[i]) -
                       static_cast<int>(dst_argb_opt[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }
  free_aligned_buffer_page_end(src_argb_a);
  free_aligned_buffer_page_end(dst_argb_c);
  free_aligned_buffer_page_end(dst_argb_opt);
  return max_diff;
}

TEST_F(LibYUVPlanarTest, ARGBSobelToPlane_Any) {
  int max_diff = TestSobelToPlane(benchmark_width_ - 1, benchmark_height_,
                                  benchmark_iterations_, disable_cpu_flags_,
                                  benchmark_cpu_info_, +1, 0);
  EXPECT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, ARGBSobelToPlane_Unaligned) {
  int max_diff = TestSobelToPlane(benchmark_width_, benchmark_height_,
                                  benchmark_iterations_, disable_cpu_flags_,
                                  benchmark_cpu_info_, +1, 1);
  EXPECT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, ARGBSobelToPlane_Invert) {
  int max_diff = TestSobelToPlane(benchmark_width_, benchmark_height_,
                                  benchmark_iterations_, disable_cpu_flags_,
                                  benchmark_cpu_info_, -1, 0);
  EXPECT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, ARGBSobelToPlane_Opt) {
  int max_diff = TestSobelToPlane(benchmark_width_, benchmark_height_,
                                  benchmark_iterations_, disable_cpu_flags_,
                                  benchmark_cpu_info_, +1, 0);
  EXPECT_EQ(0, max_diff);
}

static int TestSobelXY(int width,
                       int height,
                       int benchmark_iterations,
                       int disable_cpu_flags,
                       int benchmark_cpu_info,
                       int invert,
                       int off) {
  if (width < 1) {
    width = 1;
  }
  const int kBpp = 4;
  const int kStride = width * kBpp;
  align_buffer_page_end(src_argb_a, kStride * height + off);
  align_buffer_page_end(dst_argb_c, kStride * height);
  align_buffer_page_end(dst_argb_opt, kStride * height);
  memset(src_argb_a, 0, kStride * height + off);
  for (int i = 0; i < kStride * height; ++i) {
    src_argb_a[i + off] = (fastrand() & 0xff);
  }
  memset(dst_argb_c, 0, kStride * height);
  memset(dst_argb_opt, 0, kStride * height);

  MaskCpuFlags(disable_cpu_flags);
  ARGBSobelXY(src_argb_a + off, kStride, dst_argb_c, kStride, width,
              invert * height);
  MaskCpuFlags(benchmark_cpu_info);
  for (int i = 0; i < benchmark_iterations; ++i) {
    ARGBSobelXY(src_argb_a + off, kStride, dst_argb_opt, kStride, width,
                invert * height);
  }
  int max_diff = 0;
  for (int i = 0; i < kStride * height; ++i) {
    int abs_diff = abs(static_cast<int>(dst_argb_c[i]) -
                       static_cast<int>(dst_argb_opt[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }
  free_aligned_buffer_page_end(src_argb_a);
  free_aligned_buffer_page_end(dst_argb_c);
  free_aligned_buffer_page_end(dst_argb_opt);
  return max_diff;
}

TEST_F(LibYUVPlanarTest, ARGBSobelXY_Any) {
  int max_diff = TestSobelXY(benchmark_width_ - 1, benchmark_height_,
                             benchmark_iterations_, disable_cpu_flags_,
                             benchmark_cpu_info_, +1, 0);
  EXPECT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, ARGBSobelXY_Unaligned) {
  int max_diff =
      TestSobelXY(benchmark_width_, benchmark_height_, benchmark_iterations_,
                  disable_cpu_flags_, benchmark_cpu_info_, +1, 1);
  EXPECT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, ARGBSobelXY_Invert) {
  int max_diff =
      TestSobelXY(benchmark_width_, benchmark_height_, benchmark_iterations_,
                  disable_cpu_flags_, benchmark_cpu_info_, -1, 0);
  EXPECT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, ARGBSobelXY_Opt) {
  int max_diff =
      TestSobelXY(benchmark_width_, benchmark_height_, benchmark_iterations_,
                  disable_cpu_flags_, benchmark_cpu_info_, +1, 0);
  EXPECT_EQ(0, max_diff);
}

static int TestBlur(int width,
                    int height,
                    int benchmark_iterations,
                    int disable_cpu_flags,
                    int benchmark_cpu_info,
                    int invert,
                    int off,
                    int radius) {
  if (width < 1) {
    width = 1;
  }
  const int kBpp = 4;
  const int kStride = width * kBpp;
  align_buffer_page_end(src_argb_a, kStride * height + off);
  align_buffer_page_end(dst_cumsum, width * height * 16);
  align_buffer_page_end(dst_argb_c, kStride * height);
  align_buffer_page_end(dst_argb_opt, kStride * height);
  for (int i = 0; i < kStride * height; ++i) {
    src_argb_a[i + off] = (fastrand() & 0xff);
  }
  memset(dst_cumsum, 0, width * height * 16);
  memset(dst_argb_c, 0, kStride * height);
  memset(dst_argb_opt, 0, kStride * height);

  MaskCpuFlags(disable_cpu_flags);
  ARGBBlur(src_argb_a + off, kStride, dst_argb_c, kStride,
           reinterpret_cast<int32*>(dst_cumsum), width * 4, width,
           invert * height, radius);
  MaskCpuFlags(benchmark_cpu_info);
  for (int i = 0; i < benchmark_iterations; ++i) {
    ARGBBlur(src_argb_a + off, kStride, dst_argb_opt, kStride,
             reinterpret_cast<int32*>(dst_cumsum), width * 4, width,
             invert * height, radius);
  }
  int max_diff = 0;
  for (int i = 0; i < kStride * height; ++i) {
    int abs_diff = abs(static_cast<int>(dst_argb_c[i]) -
                       static_cast<int>(dst_argb_opt[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }
  free_aligned_buffer_page_end(src_argb_a);
  free_aligned_buffer_page_end(dst_cumsum);
  free_aligned_buffer_page_end(dst_argb_c);
  free_aligned_buffer_page_end(dst_argb_opt);
  return max_diff;
}

static const int kBlurSize = 55;
TEST_F(LibYUVPlanarTest, ARGBBlur_Any) {
  int max_diff =
      TestBlur(benchmark_width_ - 1, benchmark_height_, benchmark_iterations_,
               disable_cpu_flags_, benchmark_cpu_info_, +1, 0, kBlurSize);
  EXPECT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, ARGBBlur_Unaligned) {
  int max_diff =
      TestBlur(benchmark_width_, benchmark_height_, benchmark_iterations_,
               disable_cpu_flags_, benchmark_cpu_info_, +1, 1, kBlurSize);
  EXPECT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, ARGBBlur_Invert) {
  int max_diff =
      TestBlur(benchmark_width_, benchmark_height_, benchmark_iterations_,
               disable_cpu_flags_, benchmark_cpu_info_, -1, 0, kBlurSize);
  EXPECT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, ARGBBlur_Opt) {
  int max_diff =
      TestBlur(benchmark_width_, benchmark_height_, benchmark_iterations_,
               disable_cpu_flags_, benchmark_cpu_info_, +1, 0, kBlurSize);
  EXPECT_LE(max_diff, 1);
}

static const int kBlurSmallSize = 5;
TEST_F(LibYUVPlanarTest, ARGBBlurSmall_Any) {
  int max_diff =
      TestBlur(benchmark_width_ - 1, benchmark_height_, benchmark_iterations_,
               disable_cpu_flags_, benchmark_cpu_info_, +1, 0, kBlurSmallSize);
  EXPECT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, ARGBBlurSmall_Unaligned) {
  int max_diff =
      TestBlur(benchmark_width_, benchmark_height_, benchmark_iterations_,
               disable_cpu_flags_, benchmark_cpu_info_, +1, 1, kBlurSmallSize);
  EXPECT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, ARGBBlurSmall_Invert) {
  int max_diff =
      TestBlur(benchmark_width_, benchmark_height_, benchmark_iterations_,
               disable_cpu_flags_, benchmark_cpu_info_, -1, 0, kBlurSmallSize);
  EXPECT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, ARGBBlurSmall_Opt) {
  int max_diff =
      TestBlur(benchmark_width_, benchmark_height_, benchmark_iterations_,
               disable_cpu_flags_, benchmark_cpu_info_, +1, 0, kBlurSmallSize);
  EXPECT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, TestARGBPolynomial) {
  SIMD_ALIGNED(uint8 orig_pixels[1280][4]);
  SIMD_ALIGNED(uint8 dst_pixels_opt[1280][4]);
  SIMD_ALIGNED(uint8 dst_pixels_c[1280][4]);
  memset(orig_pixels, 0, sizeof(orig_pixels));

  SIMD_ALIGNED(static const float kWarmifyPolynomial[16]) = {
      0.94230f,  -3.03300f,    -2.92500f,    0.f,  // C0
      0.584500f, 1.112000f,    1.535000f,    1.f,  // C1 x
      0.001313f, -0.002503f,   -0.004496f,   0.f,  // C2 x * x
      0.0f,      0.000006965f, 0.000008781f, 0.f,  // C3 x * x * x
  };

  // Test blue
  orig_pixels[0][0] = 255u;
  orig_pixels[0][1] = 0u;
  orig_pixels[0][2] = 0u;
  orig_pixels[0][3] = 128u;
  // Test green
  orig_pixels[1][0] = 0u;
  orig_pixels[1][1] = 255u;
  orig_pixels[1][2] = 0u;
  orig_pixels[1][3] = 0u;
  // Test red
  orig_pixels[2][0] = 0u;
  orig_pixels[2][1] = 0u;
  orig_pixels[2][2] = 255u;
  orig_pixels[2][3] = 255u;
  // Test white
  orig_pixels[3][0] = 255u;
  orig_pixels[3][1] = 255u;
  orig_pixels[3][2] = 255u;
  orig_pixels[3][3] = 255u;
  // Test color
  orig_pixels[4][0] = 16u;
  orig_pixels[4][1] = 64u;
  orig_pixels[4][2] = 192u;
  orig_pixels[4][3] = 224u;
  // Do 16 to test asm version.
  ARGBPolynomial(&orig_pixels[0][0], 0, &dst_pixels_opt[0][0], 0,
                 &kWarmifyPolynomial[0], 16, 1);
  EXPECT_EQ(235u, dst_pixels_opt[0][0]);
  EXPECT_EQ(0u, dst_pixels_opt[0][1]);
  EXPECT_EQ(0u, dst_pixels_opt[0][2]);
  EXPECT_EQ(128u, dst_pixels_opt[0][3]);
  EXPECT_EQ(0u, dst_pixels_opt[1][0]);
  EXPECT_EQ(233u, dst_pixels_opt[1][1]);
  EXPECT_EQ(0u, dst_pixels_opt[1][2]);
  EXPECT_EQ(0u, dst_pixels_opt[1][3]);
  EXPECT_EQ(0u, dst_pixels_opt[2][0]);
  EXPECT_EQ(0u, dst_pixels_opt[2][1]);
  EXPECT_EQ(241u, dst_pixels_opt[2][2]);
  EXPECT_EQ(255u, dst_pixels_opt[2][3]);
  EXPECT_EQ(235u, dst_pixels_opt[3][0]);
  EXPECT_EQ(233u, dst_pixels_opt[3][1]);
  EXPECT_EQ(241u, dst_pixels_opt[3][2]);
  EXPECT_EQ(255u, dst_pixels_opt[3][3]);
  EXPECT_EQ(10u, dst_pixels_opt[4][0]);
  EXPECT_EQ(59u, dst_pixels_opt[4][1]);
  EXPECT_EQ(188u, dst_pixels_opt[4][2]);
  EXPECT_EQ(224u, dst_pixels_opt[4][3]);

  for (int i = 0; i < 1280; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i / 2;
    orig_pixels[i][2] = i / 3;
    orig_pixels[i][3] = i;
  }

  MaskCpuFlags(disable_cpu_flags_);
  ARGBPolynomial(&orig_pixels[0][0], 0, &dst_pixels_c[0][0], 0,
                 &kWarmifyPolynomial[0], 1280, 1);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    ARGBPolynomial(&orig_pixels[0][0], 0, &dst_pixels_opt[0][0], 0,
                   &kWarmifyPolynomial[0], 1280, 1);
  }

  for (int i = 0; i < 1280; ++i) {
    EXPECT_EQ(dst_pixels_c[i][0], dst_pixels_opt[i][0]);
    EXPECT_EQ(dst_pixels_c[i][1], dst_pixels_opt[i][1]);
    EXPECT_EQ(dst_pixels_c[i][2], dst_pixels_opt[i][2]);
    EXPECT_EQ(dst_pixels_c[i][3], dst_pixels_opt[i][3]);
  }
}

int TestHalfFloatPlane(int benchmark_width,
                       int benchmark_height,
                       int benchmark_iterations,
                       int disable_cpu_flags,
                       int benchmark_cpu_info,
                       float scale,
                       int mask) {
  int i, j;
  const int y_plane_size = benchmark_width * benchmark_height * 2;

  align_buffer_page_end(orig_y, y_plane_size * 3);
  uint8* dst_opt = orig_y + y_plane_size;
  uint8* dst_c = orig_y + y_plane_size * 2;

  MemRandomize(orig_y, y_plane_size);
  memset(dst_c, 0, y_plane_size);
  memset(dst_opt, 1, y_plane_size);

  for (i = 0; i < y_plane_size / 2; ++i) {
    reinterpret_cast<uint16*>(orig_y)[i] &= mask;
  }

  // Disable all optimizations.
  MaskCpuFlags(disable_cpu_flags);
  for (j = 0; j < benchmark_iterations; j++) {
    HalfFloatPlane(reinterpret_cast<uint16*>(orig_y), benchmark_width * 2,
                   reinterpret_cast<uint16*>(dst_c), benchmark_width * 2, scale,
                   benchmark_width, benchmark_height);
  }

  // Enable optimizations.
  MaskCpuFlags(benchmark_cpu_info);
  for (j = 0; j < benchmark_iterations; j++) {
    HalfFloatPlane(reinterpret_cast<uint16*>(orig_y), benchmark_width * 2,
                   reinterpret_cast<uint16*>(dst_opt), benchmark_width * 2,
                   scale, benchmark_width, benchmark_height);
  }

  int max_diff = 0;
  for (i = 0; i < y_plane_size / 2; ++i) {
    int abs_diff = abs(static_cast<int>(reinterpret_cast<uint16*>(dst_c)[i]) -
                       static_cast<int>(reinterpret_cast<uint16*>(dst_opt)[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }

  free_aligned_buffer_page_end(orig_y);
  return max_diff;
}

#if defined(__arm__)
static void EnableFlushDenormalToZero(void) {
  uint32_t cw;
  __asm__ __volatile__(
      "vmrs   %0, fpscr         \n"
      "orr    %0, %0, #0x1000000        \n"
      "vmsr   fpscr, %0         \n"
      : "=r"(cw)::"memory");
}
#endif

// 5 bit exponent with bias of 15 will underflow to a denormal if scale causes
// exponent to be less than 0.  15 - log2(65536) = -1/  This shouldnt normally
// happen since scale is 1/(1<<bits) where bits is 9, 10 or 12.

TEST_F(LibYUVPlanarTest, TestHalfFloatPlane_16bit_denormal) {
// 32 bit arm rounding on denormal case is off by 1 compared to C.
#if defined(__arm__)
  EnableFlushDenormalToZero();
#endif
  int diff = TestHalfFloatPlane(benchmark_width_, benchmark_height_,
                                benchmark_iterations_, disable_cpu_flags_,
                                benchmark_cpu_info_, 1.0f / 65536.0f, 65535);
  EXPECT_EQ(0, diff);
}

TEST_F(LibYUVPlanarTest, TestHalfFloatPlane_16bit_One) {
  int diff = TestHalfFloatPlane(benchmark_width_, benchmark_height_,
                                benchmark_iterations_, disable_cpu_flags_,
                                benchmark_cpu_info_, 1.0f, 65535);
  EXPECT_LE(diff, 1);
}

TEST_F(LibYUVPlanarTest, TestHalfFloatPlane_16bit_Opt) {
  int diff = TestHalfFloatPlane(benchmark_width_, benchmark_height_,
                                benchmark_iterations_, disable_cpu_flags_,
                                benchmark_cpu_info_, 1.0f / 4096.0f, 65535);
  EXPECT_EQ(0, diff);
}

TEST_F(LibYUVPlanarTest, TestHalfFloatPlane_10bit_Opt) {
  int diff = TestHalfFloatPlane(benchmark_width_, benchmark_height_,
                                benchmark_iterations_, disable_cpu_flags_,
                                benchmark_cpu_info_, 1.0f / 1024.0f, 1023);
  EXPECT_EQ(0, diff);
}

TEST_F(LibYUVPlanarTest, TestHalfFloatPlane_9bit_Opt) {
  int diff = TestHalfFloatPlane(benchmark_width_, benchmark_height_,
                                benchmark_iterations_, disable_cpu_flags_,
                                benchmark_cpu_info_, 1.0f / 512.0f, 511);
  EXPECT_EQ(0, diff);
}

TEST_F(LibYUVPlanarTest, TestHalfFloatPlane_Opt) {
  int diff = TestHalfFloatPlane(benchmark_width_, benchmark_height_,
                                benchmark_iterations_, disable_cpu_flags_,
                                benchmark_cpu_info_, 1.0f / 4096.0f, 4095);
  EXPECT_EQ(0, diff);
}

TEST_F(LibYUVPlanarTest, TestHalfFloatPlane_Offby1) {
  int diff = TestHalfFloatPlane(benchmark_width_, benchmark_height_,
                                benchmark_iterations_, disable_cpu_flags_,
                                benchmark_cpu_info_, 1.0f / 4095.0f, 4095);
  EXPECT_EQ(0, diff);
}

TEST_F(LibYUVPlanarTest, TestHalfFloatPlane_One) {
  int diff = TestHalfFloatPlane(benchmark_width_, benchmark_height_,
                                benchmark_iterations_, disable_cpu_flags_,
                                benchmark_cpu_info_, 1.0f, 2047);
  EXPECT_EQ(0, diff);
}

TEST_F(LibYUVPlanarTest, TestHalfFloatPlane_12bit_One) {
  int diff = TestHalfFloatPlane(benchmark_width_, benchmark_height_,
                                benchmark_iterations_, disable_cpu_flags_,
                                benchmark_cpu_info_, 1.0f, 4095);
  EXPECT_LE(diff, 1);
}

TEST_F(LibYUVPlanarTest, TestARGBLumaColorTable) {
  SIMD_ALIGNED(uint8 orig_pixels[1280][4]);
  SIMD_ALIGNED(uint8 dst_pixels_opt[1280][4]);
  SIMD_ALIGNED(uint8 dst_pixels_c[1280][4]);
  memset(orig_pixels, 0, sizeof(orig_pixels));

  align_buffer_page_end(lumacolortable, 32768);
  int v = 0;
  for (int i = 0; i < 32768; ++i) {
    lumacolortable[i] = v;
    v += 3;
  }
  // Test blue
  orig_pixels[0][0] = 255u;
  orig_pixels[0][1] = 0u;
  orig_pixels[0][2] = 0u;
  orig_pixels[0][3] = 128u;
  // Test green
  orig_pixels[1][0] = 0u;
  orig_pixels[1][1] = 255u;
  orig_pixels[1][2] = 0u;
  orig_pixels[1][3] = 0u;
  // Test red
  orig_pixels[2][0] = 0u;
  orig_pixels[2][1] = 0u;
  orig_pixels[2][2] = 255u;
  orig_pixels[2][3] = 255u;
  // Test color
  orig_pixels[3][0] = 16u;
  orig_pixels[3][1] = 64u;
  orig_pixels[3][2] = 192u;
  orig_pixels[3][3] = 224u;
  // Do 16 to test asm version.
  ARGBLumaColorTable(&orig_pixels[0][0], 0, &dst_pixels_opt[0][0], 0,
                     &lumacolortable[0], 16, 1);
  EXPECT_EQ(253u, dst_pixels_opt[0][0]);
  EXPECT_EQ(0u, dst_pixels_opt[0][1]);
  EXPECT_EQ(0u, dst_pixels_opt[0][2]);
  EXPECT_EQ(128u, dst_pixels_opt[0][3]);
  EXPECT_EQ(0u, dst_pixels_opt[1][0]);
  EXPECT_EQ(253u, dst_pixels_opt[1][1]);
  EXPECT_EQ(0u, dst_pixels_opt[1][2]);
  EXPECT_EQ(0u, dst_pixels_opt[1][3]);
  EXPECT_EQ(0u, dst_pixels_opt[2][0]);
  EXPECT_EQ(0u, dst_pixels_opt[2][1]);
  EXPECT_EQ(253u, dst_pixels_opt[2][2]);
  EXPECT_EQ(255u, dst_pixels_opt[2][3]);
  EXPECT_EQ(48u, dst_pixels_opt[3][0]);
  EXPECT_EQ(192u, dst_pixels_opt[3][1]);
  EXPECT_EQ(64u, dst_pixels_opt[3][2]);
  EXPECT_EQ(224u, dst_pixels_opt[3][3]);

  for (int i = 0; i < 1280; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i / 2;
    orig_pixels[i][2] = i / 3;
    orig_pixels[i][3] = i;
  }

  MaskCpuFlags(disable_cpu_flags_);
  ARGBLumaColorTable(&orig_pixels[0][0], 0, &dst_pixels_c[0][0], 0,
                     lumacolortable, 1280, 1);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    ARGBLumaColorTable(&orig_pixels[0][0], 0, &dst_pixels_opt[0][0], 0,
                       lumacolortable, 1280, 1);
  }
  for (int i = 0; i < 1280; ++i) {
    EXPECT_EQ(dst_pixels_c[i][0], dst_pixels_opt[i][0]);
    EXPECT_EQ(dst_pixels_c[i][1], dst_pixels_opt[i][1]);
    EXPECT_EQ(dst_pixels_c[i][2], dst_pixels_opt[i][2]);
    EXPECT_EQ(dst_pixels_c[i][3], dst_pixels_opt[i][3]);
  }

  free_aligned_buffer_page_end(lumacolortable);
}

TEST_F(LibYUVPlanarTest, TestARGBCopyAlpha) {
  const int kSize = benchmark_width_ * benchmark_height_ * 4;
  align_buffer_page_end(orig_pixels, kSize);
  align_buffer_page_end(dst_pixels_opt, kSize);
  align_buffer_page_end(dst_pixels_c, kSize);

  MemRandomize(orig_pixels, kSize);
  MemRandomize(dst_pixels_opt, kSize);
  memcpy(dst_pixels_c, dst_pixels_opt, kSize);

  MaskCpuFlags(disable_cpu_flags_);
  ARGBCopyAlpha(orig_pixels, benchmark_width_ * 4, dst_pixels_c,
                benchmark_width_ * 4, benchmark_width_, benchmark_height_);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    ARGBCopyAlpha(orig_pixels, benchmark_width_ * 4, dst_pixels_opt,
                  benchmark_width_ * 4, benchmark_width_, benchmark_height_);
  }
  for (int i = 0; i < kSize; ++i) {
    EXPECT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
  }

  free_aligned_buffer_page_end(dst_pixels_c);
  free_aligned_buffer_page_end(dst_pixels_opt);
  free_aligned_buffer_page_end(orig_pixels);
}

TEST_F(LibYUVPlanarTest, TestARGBExtractAlpha) {
  const int kPixels = benchmark_width_ * benchmark_height_;
  align_buffer_page_end(src_pixels, kPixels * 4);
  align_buffer_page_end(dst_pixels_opt, kPixels);
  align_buffer_page_end(dst_pixels_c, kPixels);

  MemRandomize(src_pixels, kPixels * 4);
  MemRandomize(dst_pixels_opt, kPixels);
  memcpy(dst_pixels_c, dst_pixels_opt, kPixels);

  MaskCpuFlags(disable_cpu_flags_);
  ARGBExtractAlpha(src_pixels, benchmark_width_ * 4, dst_pixels_c,
                   benchmark_width_, benchmark_width_, benchmark_height_);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    ARGBExtractAlpha(src_pixels, benchmark_width_ * 4, dst_pixels_opt,
                     benchmark_width_, benchmark_width_, benchmark_height_);
  }
  for (int i = 0; i < kPixels; ++i) {
    EXPECT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
  }

  free_aligned_buffer_page_end(dst_pixels_c);
  free_aligned_buffer_page_end(dst_pixels_opt);
  free_aligned_buffer_page_end(src_pixels);
}

TEST_F(LibYUVPlanarTest, TestARGBCopyYToAlpha) {
  const int kPixels = benchmark_width_ * benchmark_height_;
  align_buffer_page_end(orig_pixels, kPixels);
  align_buffer_page_end(dst_pixels_opt, kPixels * 4);
  align_buffer_page_end(dst_pixels_c, kPixels * 4);

  MemRandomize(orig_pixels, kPixels);
  MemRandomize(dst_pixels_opt, kPixels * 4);
  memcpy(dst_pixels_c, dst_pixels_opt, kPixels * 4);

  MaskCpuFlags(disable_cpu_flags_);
  ARGBCopyYToAlpha(orig_pixels, benchmark_width_, dst_pixels_c,
                   benchmark_width_ * 4, benchmark_width_, benchmark_height_);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    ARGBCopyYToAlpha(orig_pixels, benchmark_width_, dst_pixels_opt,
                     benchmark_width_ * 4, benchmark_width_, benchmark_height_);
  }
  for (int i = 0; i < kPixels * 4; ++i) {
    EXPECT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
  }

  free_aligned_buffer_page_end(dst_pixels_c);
  free_aligned_buffer_page_end(dst_pixels_opt);
  free_aligned_buffer_page_end(orig_pixels);
}

static int TestARGBRect(int width,
                        int height,
                        int benchmark_iterations,
                        int disable_cpu_flags,
                        int benchmark_cpu_info,
                        int invert,
                        int off,
                        int bpp) {
  if (width < 1) {
    width = 1;
  }
  const int kStride = width * bpp;
  const int kSize = kStride * height;
  const uint32 v32 = fastrand() & (bpp == 4 ? 0xffffffff : 0xff);

  align_buffer_page_end(dst_argb_c, kSize + off);
  align_buffer_page_end(dst_argb_opt, kSize + off);

  MemRandomize(dst_argb_c + off, kSize);
  memcpy(dst_argb_opt + off, dst_argb_c + off, kSize);

  MaskCpuFlags(disable_cpu_flags);
  if (bpp == 4) {
    ARGBRect(dst_argb_c + off, kStride, 0, 0, width, invert * height, v32);
  } else {
    SetPlane(dst_argb_c + off, kStride, width, invert * height, v32);
  }

  MaskCpuFlags(benchmark_cpu_info);
  for (int i = 0; i < benchmark_iterations; ++i) {
    if (bpp == 4) {
      ARGBRect(dst_argb_opt + off, kStride, 0, 0, width, invert * height, v32);
    } else {
      SetPlane(dst_argb_opt + off, kStride, width, invert * height, v32);
    }
  }
  int max_diff = 0;
  for (int i = 0; i < kStride * height; ++i) {
    int abs_diff = abs(static_cast<int>(dst_argb_c[i + off]) -
                       static_cast<int>(dst_argb_opt[i + off]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }
  free_aligned_buffer_page_end(dst_argb_c);
  free_aligned_buffer_page_end(dst_argb_opt);
  return max_diff;
}

TEST_F(LibYUVPlanarTest, ARGBRect_Any) {
  int max_diff = TestARGBRect(benchmark_width_ - 1, benchmark_height_,
                              benchmark_iterations_, disable_cpu_flags_,
                              benchmark_cpu_info_, +1, 0, 4);
  EXPECT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, ARGBRect_Unaligned) {
  int max_diff =
      TestARGBRect(benchmark_width_, benchmark_height_, benchmark_iterations_,
                   disable_cpu_flags_, benchmark_cpu_info_, +1, 1, 4);
  EXPECT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, ARGBRect_Invert) {
  int max_diff =
      TestARGBRect(benchmark_width_, benchmark_height_, benchmark_iterations_,
                   disable_cpu_flags_, benchmark_cpu_info_, -1, 0, 4);
  EXPECT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, ARGBRect_Opt) {
  int max_diff =
      TestARGBRect(benchmark_width_, benchmark_height_, benchmark_iterations_,
                   disable_cpu_flags_, benchmark_cpu_info_, +1, 0, 4);
  EXPECT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, SetPlane_Any) {
  int max_diff = TestARGBRect(benchmark_width_ - 1, benchmark_height_,
                              benchmark_iterations_, disable_cpu_flags_,
                              benchmark_cpu_info_, +1, 0, 1);
  EXPECT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, SetPlane_Unaligned) {
  int max_diff =
      TestARGBRect(benchmark_width_, benchmark_height_, benchmark_iterations_,
                   disable_cpu_flags_, benchmark_cpu_info_, +1, 1, 1);
  EXPECT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, SetPlane_Invert) {
  int max_diff =
      TestARGBRect(benchmark_width_, benchmark_height_, benchmark_iterations_,
                   disable_cpu_flags_, benchmark_cpu_info_, -1, 0, 1);
  EXPECT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, SetPlane_Opt) {
  int max_diff =
      TestARGBRect(benchmark_width_, benchmark_height_, benchmark_iterations_,
                   disable_cpu_flags_, benchmark_cpu_info_, +1, 0, 1);
  EXPECT_EQ(0, max_diff);
}

TEST_F(LibYUVPlanarTest, MergeUVPlane_Opt) {
  const int kPixels = benchmark_width_ * benchmark_height_;
  align_buffer_page_end(src_pixels, kPixels * 2);
  align_buffer_page_end(tmp_pixels_u, kPixels);
  align_buffer_page_end(tmp_pixels_v, kPixels);
  align_buffer_page_end(dst_pixels_opt, kPixels * 2);
  align_buffer_page_end(dst_pixels_c, kPixels * 2);

  MemRandomize(src_pixels, kPixels * 2);
  MemRandomize(tmp_pixels_u, kPixels);
  MemRandomize(tmp_pixels_v, kPixels);
  MemRandomize(dst_pixels_opt, kPixels * 2);
  MemRandomize(dst_pixels_c, kPixels * 2);

  MaskCpuFlags(disable_cpu_flags_);
  SplitUVPlane(src_pixels, benchmark_width_ * 2, tmp_pixels_u, benchmark_width_,
               tmp_pixels_v, benchmark_width_, benchmark_width_,
               benchmark_height_);
  MergeUVPlane(tmp_pixels_u, benchmark_width_, tmp_pixels_v, benchmark_width_,
               dst_pixels_c, benchmark_width_ * 2, benchmark_width_,
               benchmark_height_);
  MaskCpuFlags(benchmark_cpu_info_);

  SplitUVPlane(src_pixels, benchmark_width_ * 2, tmp_pixels_u, benchmark_width_,
               tmp_pixels_v, benchmark_width_, benchmark_width_,
               benchmark_height_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    MergeUVPlane(tmp_pixels_u, benchmark_width_, tmp_pixels_v, benchmark_width_,
                 dst_pixels_opt, benchmark_width_ * 2, benchmark_width_,
                 benchmark_height_);
  }

  for (int i = 0; i < kPixels * 2; ++i) {
    EXPECT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
  }

  free_aligned_buffer_page_end(src_pixels);
  free_aligned_buffer_page_end(tmp_pixels_u);
  free_aligned_buffer_page_end(tmp_pixels_v);
  free_aligned_buffer_page_end(dst_pixels_opt);
  free_aligned_buffer_page_end(dst_pixels_c);
}

TEST_F(LibYUVPlanarTest, SplitUVPlane_Opt) {
  const int kPixels = benchmark_width_ * benchmark_height_;
  align_buffer_page_end(src_pixels, kPixels * 2);
  align_buffer_page_end(tmp_pixels_u, kPixels);
  align_buffer_page_end(tmp_pixels_v, kPixels);
  align_buffer_page_end(dst_pixels_opt, kPixels * 2);
  align_buffer_page_end(dst_pixels_c, kPixels * 2);

  MemRandomize(src_pixels, kPixels * 2);
  MemRandomize(tmp_pixels_u, kPixels);
  MemRandomize(tmp_pixels_v, kPixels);
  MemRandomize(dst_pixels_opt, kPixels * 2);
  MemRandomize(dst_pixels_c, kPixels * 2);

  MaskCpuFlags(disable_cpu_flags_);
  SplitUVPlane(src_pixels, benchmark_width_ * 2, tmp_pixels_u, benchmark_width_,
               tmp_pixels_v, benchmark_width_, benchmark_width_,
               benchmark_height_);
  MergeUVPlane(tmp_pixels_u, benchmark_width_, tmp_pixels_v, benchmark_width_,
               dst_pixels_c, benchmark_width_ * 2, benchmark_width_,
               benchmark_height_);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    SplitUVPlane(src_pixels, benchmark_width_ * 2, tmp_pixels_u,
                 benchmark_width_, tmp_pixels_v, benchmark_width_,
                 benchmark_width_, benchmark_height_);
  }
  MergeUVPlane(tmp_pixels_u, benchmark_width_, tmp_pixels_v, benchmark_width_,
               dst_pixels_opt, benchmark_width_ * 2, benchmark_width_,
               benchmark_height_);

  for (int i = 0; i < kPixels * 2; ++i) {
    EXPECT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
  }

  free_aligned_buffer_page_end(src_pixels);
  free_aligned_buffer_page_end(tmp_pixels_u);
  free_aligned_buffer_page_end(tmp_pixels_v);
  free_aligned_buffer_page_end(dst_pixels_opt);
  free_aligned_buffer_page_end(dst_pixels_c);
}

}  // namespace libyuv
