/*
 *  Copyright 2011 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
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
#include "libyuv/scale.h"

#ifdef ENABLE_ROW_TESTS
// row.h defines SIMD_ALIGNED, overriding unit_test.h
// TODO(fbarchard): Remove row.h from unittests.  Test public functions.
#include "libyuv/row.h" /* For ScaleSumSamples_Neon */
#endif

#if defined(LIBYUV_BIT_EXACT)
#define EXPECTED_ATTENUATE_DIFF 0
#else
#define EXPECTED_ATTENUATE_DIFF 2
#endif

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
  EXPECT_NEAR(254, atten_pixels[255 * 4 + 0], EXPECTED_ATTENUATE_DIFF);
  EXPECT_NEAR(127, atten_pixels[255 * 4 + 1], EXPECTED_ATTENUATE_DIFF);
  EXPECT_NEAR(85, atten_pixels[255 * 4 + 2], EXPECTED_ATTENUATE_DIFF);
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
  int max_diff = TestAttenuateI(benchmark_width_ + 1, benchmark_height_,
                                benchmark_iterations_, disable_cpu_flags_,
                                benchmark_cpu_info_, +1, 0);

  EXPECT_LE(max_diff, EXPECTED_ATTENUATE_DIFF);
}

TEST_F(LibYUVPlanarTest, ARGBAttenuate_Unaligned) {
  int max_diff =
      TestAttenuateI(benchmark_width_, benchmark_height_, benchmark_iterations_,
                     disable_cpu_flags_, benchmark_cpu_info_, +1, 1);
  EXPECT_LE(max_diff, EXPECTED_ATTENUATE_DIFF);
}

TEST_F(LibYUVPlanarTest, ARGBAttenuate_Invert) {
  int max_diff =
      TestAttenuateI(benchmark_width_, benchmark_height_, benchmark_iterations_,
                     disable_cpu_flags_, benchmark_cpu_info_, -1, 0);
  EXPECT_LE(max_diff, EXPECTED_ATTENUATE_DIFF);
}

TEST_F(LibYUVPlanarTest, ARGBAttenuate_Opt) {
  int max_diff =
      TestAttenuateI(benchmark_width_, benchmark_height_, benchmark_iterations_,
                     disable_cpu_flags_, benchmark_cpu_info_, +1, 0);
  EXPECT_LE(max_diff, EXPECTED_ATTENUATE_DIFF);
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
  int max_diff = TestUnattenuateI(benchmark_width_ + 1, benchmark_height_,
                                  benchmark_iterations_, disable_cpu_flags_,
                                  benchmark_cpu_info_, +1, 0);
  EXPECT_LE(max_diff, EXPECTED_ATTENUATE_DIFF);
}

TEST_F(LibYUVPlanarTest, ARGBUnattenuate_Unaligned) {
  int max_diff = TestUnattenuateI(benchmark_width_, benchmark_height_,
                                  benchmark_iterations_, disable_cpu_flags_,
                                  benchmark_cpu_info_, +1, 1);
  EXPECT_LE(max_diff, EXPECTED_ATTENUATE_DIFF);
}

TEST_F(LibYUVPlanarTest, ARGBUnattenuate_Invert) {
  int max_diff = TestUnattenuateI(benchmark_width_, benchmark_height_,
                                  benchmark_iterations_, disable_cpu_flags_,
                                  benchmark_cpu_info_, -1, 0);
  EXPECT_LE(max_diff, EXPECTED_ATTENUATE_DIFF);
}

TEST_F(LibYUVPlanarTest, ARGBUnattenuate_Opt) {
  int max_diff = TestUnattenuateI(benchmark_width_, benchmark_height_,
                                  benchmark_iterations_, disable_cpu_flags_,
                                  benchmark_cpu_info_, +1, 0);
  EXPECT_LE(max_diff, EXPECTED_ATTENUATE_DIFF);
}

TEST_F(LibYUVPlanarTest, TestARGBComputeCumulativeSum) {
  SIMD_ALIGNED(uint8_t orig_pixels[16][16][4]);
  SIMD_ALIGNED(int32_t added_pixels[16][16][4]);

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

// near is for legacy platforms.
TEST_F(LibYUVPlanarTest, TestARGBGray) {
  SIMD_ALIGNED(uint8_t orig_pixels[1280][4]);
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
  EXPECT_NEAR(29u, orig_pixels[0][0], 1);
  EXPECT_NEAR(29u, orig_pixels[0][1], 1);
  EXPECT_NEAR(29u, orig_pixels[0][2], 1);
  EXPECT_EQ(128u, orig_pixels[0][3]);
  EXPECT_EQ(149u, orig_pixels[1][0]);
  EXPECT_EQ(149u, orig_pixels[1][1]);
  EXPECT_EQ(149u, orig_pixels[1][2]);
  EXPECT_EQ(0u, orig_pixels[1][3]);
  EXPECT_NEAR(77u, orig_pixels[2][0], 1);
  EXPECT_NEAR(77u, orig_pixels[2][1], 1);
  EXPECT_NEAR(77u, orig_pixels[2][2], 1);
  EXPECT_EQ(255u, orig_pixels[2][3]);
  EXPECT_EQ(0u, orig_pixels[3][0]);
  EXPECT_EQ(0u, orig_pixels[3][1]);
  EXPECT_EQ(0u, orig_pixels[3][2]);
  EXPECT_EQ(255u, orig_pixels[3][3]);
  EXPECT_EQ(255u, orig_pixels[4][0]);
  EXPECT_EQ(255u, orig_pixels[4][1]);
  EXPECT_EQ(255u, orig_pixels[4][2]);
  EXPECT_EQ(255u, orig_pixels[4][3]);
  EXPECT_NEAR(97u, orig_pixels[5][0], 1);
  EXPECT_NEAR(97u, orig_pixels[5][1], 1);
  EXPECT_NEAR(97u, orig_pixels[5][2], 1);
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
  SIMD_ALIGNED(uint8_t orig_pixels[1280][4]);
  SIMD_ALIGNED(uint8_t gray_pixels[1280][4]);
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
  EXPECT_NEAR(30u, gray_pixels[0][0], 1);
  EXPECT_NEAR(30u, gray_pixels[0][1], 1);
  EXPECT_NEAR(30u, gray_pixels[0][2], 1);
  EXPECT_NEAR(128u, gray_pixels[0][3], 1);
  EXPECT_NEAR(149u, gray_pixels[1][0], 1);
  EXPECT_NEAR(149u, gray_pixels[1][1], 1);
  EXPECT_NEAR(149u, gray_pixels[1][2], 1);
  EXPECT_NEAR(0u, gray_pixels[1][3], 1);
  EXPECT_NEAR(76u, gray_pixels[2][0], 1);
  EXPECT_NEAR(76u, gray_pixels[2][1], 1);
  EXPECT_NEAR(76u, gray_pixels[2][2], 1);
  EXPECT_NEAR(255u, gray_pixels[2][3], 1);
  EXPECT_NEAR(0u, gray_pixels[3][0], 1);
  EXPECT_NEAR(0u, gray_pixels[3][1], 1);
  EXPECT_NEAR(0u, gray_pixels[3][2], 1);
  EXPECT_NEAR(255u, gray_pixels[3][3], 1);
  EXPECT_NEAR(255u, gray_pixels[4][0], 1);
  EXPECT_NEAR(255u, gray_pixels[4][1], 1);
  EXPECT_NEAR(255u, gray_pixels[4][2], 1);
  EXPECT_NEAR(255u, gray_pixels[4][3], 1);
  EXPECT_NEAR(96u, gray_pixels[5][0], 1);
  EXPECT_NEAR(96u, gray_pixels[5][1], 1);
  EXPECT_NEAR(96u, gray_pixels[5][2], 1);
  EXPECT_NEAR(224u, gray_pixels[5][3], 1);
  for (int i = 0; i < 1280; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i / 2;
    orig_pixels[i][2] = i / 3;
    orig_pixels[i][3] = i;
  }
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    ARGBGrayTo(&orig_pixels[0][0], 0, &gray_pixels[0][0], 0, 1280, 1);
  }

  for (int i = 0; i < 256; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i;
    orig_pixels[i][2] = i;
    orig_pixels[i][3] = i;
  }
  ARGBGray(&orig_pixels[0][0], 0, 0, 0, 256, 1);
  for (int i = 0; i < 256; ++i) {
    EXPECT_EQ(i, orig_pixels[i][0]);
    EXPECT_EQ(i, orig_pixels[i][1]);
    EXPECT_EQ(i, orig_pixels[i][2]);
    EXPECT_EQ(i, orig_pixels[i][3]);
  }
}

TEST_F(LibYUVPlanarTest, TestARGBSepia) {
  SIMD_ALIGNED(uint8_t orig_pixels[1280][4]);
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
  SIMD_ALIGNED(uint8_t orig_pixels[1280][4]);
  SIMD_ALIGNED(uint8_t dst_pixels_opt[1280][4]);
  SIMD_ALIGNED(uint8_t dst_pixels_c[1280][4]);

  // Matrix for Sepia.
  SIMD_ALIGNED(static const int8_t kRGBToSepia[]) = {
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
  SIMD_ALIGNED(uint8_t orig_pixels[1280][4]);

  // Matrix for Sepia.
  SIMD_ALIGNED(static const int8_t kRGBToSepia[]) = {
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
  SIMD_ALIGNED(uint8_t orig_pixels[1280][4]);
  memset(orig_pixels, 0, sizeof(orig_pixels));

  // Matrix for Sepia.
  static const uint8_t kARGBTable[256 * 4] = {
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
  SIMD_ALIGNED(uint8_t orig_pixels[1280][4]);
  memset(orig_pixels, 0, sizeof(orig_pixels));

  // Matrix for Sepia.
  static const uint8_t kARGBTable[256 * 4] = {
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
  SIMD_ALIGNED(uint8_t orig_pixels[1280][4]);

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

TEST_F(LibYUVPlanarTest, ARGBMirror_Opt) {
  align_buffer_page_end(src_pixels, benchmark_width_ * benchmark_height_ * 4);
  align_buffer_page_end(dst_pixels_opt,
                        benchmark_width_ * benchmark_height_ * 4);
  align_buffer_page_end(dst_pixels_c, benchmark_width_ * benchmark_height_ * 4);

  MemRandomize(src_pixels, benchmark_width_ * benchmark_height_ * 4);
  MaskCpuFlags(disable_cpu_flags_);
  ARGBMirror(src_pixels, benchmark_width_ * 4, dst_pixels_c,
             benchmark_width_ * 4, benchmark_width_, benchmark_height_);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    ARGBMirror(src_pixels, benchmark_width_ * 4, dst_pixels_opt,
               benchmark_width_ * 4, benchmark_width_, benchmark_height_);
  }
  for (int i = 0; i < benchmark_width_ * benchmark_height_ * 4; ++i) {
    EXPECT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
  }
  free_aligned_buffer_page_end(src_pixels);
  free_aligned_buffer_page_end(dst_pixels_opt);
  free_aligned_buffer_page_end(dst_pixels_c);
}

TEST_F(LibYUVPlanarTest, MirrorPlane_Opt) {
  align_buffer_page_end(src_pixels, benchmark_width_ * benchmark_height_);
  align_buffer_page_end(dst_pixels_opt, benchmark_width_ * benchmark_height_);
  align_buffer_page_end(dst_pixels_c, benchmark_width_ * benchmark_height_);

  MemRandomize(src_pixels, benchmark_width_ * benchmark_height_);
  MaskCpuFlags(disable_cpu_flags_);
  MirrorPlane(src_pixels, benchmark_width_, dst_pixels_c, benchmark_width_,
              benchmark_width_, benchmark_height_);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    MirrorPlane(src_pixels, benchmark_width_, dst_pixels_opt, benchmark_width_,
                benchmark_width_, benchmark_height_);
  }
  for (int i = 0; i < benchmark_width_ * benchmark_height_; ++i) {
    EXPECT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
  }
  free_aligned_buffer_page_end(src_pixels);
  free_aligned_buffer_page_end(dst_pixels_opt);
  free_aligned_buffer_page_end(dst_pixels_c);
}

TEST_F(LibYUVPlanarTest, MirrorUVPlane_Opt) {
  align_buffer_page_end(src_pixels, benchmark_width_ * benchmark_height_ * 2);
  align_buffer_page_end(dst_pixels_opt,
                        benchmark_width_ * benchmark_height_ * 2);
  align_buffer_page_end(dst_pixels_c, benchmark_width_ * benchmark_height_ * 2);

  MemRandomize(src_pixels, benchmark_width_ * benchmark_height_ * 2);
  MaskCpuFlags(disable_cpu_flags_);
  MirrorUVPlane(src_pixels, benchmark_width_ * 2, dst_pixels_c,
                benchmark_width_ * 2, benchmark_width_, benchmark_height_);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    MirrorUVPlane(src_pixels, benchmark_width_ * 2, dst_pixels_opt,
                  benchmark_width_ * 2, benchmark_width_, benchmark_height_);
  }
  for (int i = 0; i < benchmark_width_ * benchmark_height_ * 2; ++i) {
    EXPECT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
  }
  free_aligned_buffer_page_end(src_pixels);
  free_aligned_buffer_page_end(dst_pixels_opt);
  free_aligned_buffer_page_end(dst_pixels_c);
}

TEST_F(LibYUVPlanarTest, TestShade) {
  SIMD_ALIGNED(uint8_t orig_pixels[1280][4]);
  SIMD_ALIGNED(uint8_t shade_pixels[1280][4]);
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
  SIMD_ALIGNED(uint8_t orig_pixels_0[1280][4]);
  SIMD_ALIGNED(uint8_t orig_pixels_1[1280][4]);
  SIMD_ALIGNED(uint8_t interpolate_pixels[1280][4]);
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
  SIMD_ALIGNED(uint8_t orig_pixels_0[1280]);
  SIMD_ALIGNED(uint8_t orig_pixels_1[1280]);
  SIMD_ALIGNED(uint8_t interpolate_pixels[1280]);
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

TEST_F(LibYUVPlanarTest, TestInterpolatePlane_16) {
  SIMD_ALIGNED(uint16_t orig_pixels_0[1280]);
  SIMD_ALIGNED(uint16_t orig_pixels_1[1280]);
  SIMD_ALIGNED(uint16_t interpolate_pixels[1280]);
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

  InterpolatePlane_16(&orig_pixels_0[0], 0, &orig_pixels_1[0], 0,
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

  InterpolatePlane_16(&orig_pixels_0[0], 0, &orig_pixels_1[0], 0,
                      &interpolate_pixels[0], 0, 16, 1, 0);
  EXPECT_EQ(16u, interpolate_pixels[0]);
  EXPECT_EQ(32u, interpolate_pixels[1]);
  EXPECT_EQ(64u, interpolate_pixels[2]);
  EXPECT_EQ(128u, interpolate_pixels[3]);

  InterpolatePlane_16(&orig_pixels_0[0], 0, &orig_pixels_1[0], 0,
                      &interpolate_pixels[0], 0, 16, 1, 192);

  EXPECT_EQ(4u, interpolate_pixels[0]);
  EXPECT_EQ(8u, interpolate_pixels[1]);
  EXPECT_EQ(16u, interpolate_pixels[2]);
  EXPECT_EQ(32u, interpolate_pixels[3]);

  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    InterpolatePlane_16(&orig_pixels_0[0], 0, &orig_pixels_1[0], 0,
                        &interpolate_pixels[0], 0, 1280, 1, 123);
  }
}

#define TESTTERP(FMT_A, BPP_A, STRIDE_A, FMT_B, BPP_B, STRIDE_B, W1280, TERP, \
                 N, NEG, OFF)                                                 \
  TEST_F(LibYUVPlanarTest, ARGBInterpolate##TERP##N) {                        \
    const int kWidth = W1280;                                                 \
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
  TESTTERP(ARGB, 4, 1, ARGB, 4, 1, benchmark_width_ + 1, TERP, _Any, +, 0)   \
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
                     int off,
                     int attenuate) {
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
  MemRandomize(src_argb_a, kStride * height + off);
  MemRandomize(src_argb_b, kStride * height + off);
  if (attenuate) {
    ARGBAttenuate(src_argb_a + off, kStride, src_argb_a + off, kStride, width,
                  height);
  }
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
      TestBlend(benchmark_width_ + 1, benchmark_height_, benchmark_iterations_,
                disable_cpu_flags_, benchmark_cpu_info_, +1, 0, 1);
  EXPECT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, ARGBBlend_Unaligned) {
  int max_diff =
      TestBlend(benchmark_width_, benchmark_height_, benchmark_iterations_,
                disable_cpu_flags_, benchmark_cpu_info_, +1, 1, 1);
  EXPECT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, ARGBBlend_Invert) {
  int max_diff =
      TestBlend(benchmark_width_, benchmark_height_, benchmark_iterations_,
                disable_cpu_flags_, benchmark_cpu_info_, -1, 0, 1);
  EXPECT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, ARGBBlend_Unattenuated) {
  int max_diff =
      TestBlend(benchmark_width_, benchmark_height_, benchmark_iterations_,
                disable_cpu_flags_, benchmark_cpu_info_, +1, 0, 0);
  EXPECT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, ARGBBlend_Opt) {
  int max_diff =
      TestBlend(benchmark_width_, benchmark_height_, benchmark_iterations_,
                disable_cpu_flags_, benchmark_cpu_info_, +1, 0, 1);
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
  TestBlendPlane(benchmark_width_ + 1, benchmark_height_, benchmark_iterations_,
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
  TestI420Blend(benchmark_width_ + 1, benchmark_height_, benchmark_iterations_,
                disable_cpu_flags_, benchmark_cpu_info_, +1, 0);
}
TEST_F(LibYUVPlanarTest, I420Blend_Invert) {
  TestI420Blend(benchmark_width_, benchmark_height_, benchmark_iterations_,
                disable_cpu_flags_, benchmark_cpu_info_, -1, 0);
}

TEST_F(LibYUVPlanarTest, TestAffine) {
  SIMD_ALIGNED(uint8_t orig_pixels_0[1280][4]);
  SIMD_ALIGNED(uint8_t interpolate_pixels_C[1280][4]);

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
  SIMD_ALIGNED(uint8_t interpolate_pixels_Opt[1280][4]);
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
    uint8_t random_number = fastrand() & 0x7f;
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
    if (dst_c[i] != dst_opt[i]) {
      ++err;
    }
  }

  free_aligned_buffer_page_end(orig_y);
  free_aligned_buffer_page_end(dst_c);
  free_aligned_buffer_page_end(dst_opt);

  EXPECT_EQ(0, err);
}

TEST_F(LibYUVPlanarTest, CopyPlane_Opt) {
  int i;
  int y_plane_size = benchmark_width_ * benchmark_height_;
  align_buffer_page_end(orig_y, y_plane_size);
  align_buffer_page_end(dst_c, y_plane_size);
  align_buffer_page_end(dst_opt, y_plane_size);

  MemRandomize(orig_y, y_plane_size);
  memset(dst_c, 1, y_plane_size);
  memset(dst_opt, 2, y_plane_size);

  // Disable all optimizations.
  MaskCpuFlags(disable_cpu_flags_);
  for (i = 0; i < benchmark_iterations_; i++) {
    CopyPlane(orig_y, benchmark_width_, dst_c, benchmark_width_,
              benchmark_width_, benchmark_height_);
  }

  // Enable optimizations.
  MaskCpuFlags(benchmark_cpu_info_);
  for (i = 0; i < benchmark_iterations_; i++) {
    CopyPlane(orig_y, benchmark_width_, dst_opt, benchmark_width_,
              benchmark_width_, benchmark_height_);
  }

  for (i = 0; i < y_plane_size; ++i) {
    EXPECT_EQ(dst_c[i], dst_opt[i]);
  }

  free_aligned_buffer_page_end(orig_y);
  free_aligned_buffer_page_end(dst_c);
  free_aligned_buffer_page_end(dst_opt);
}

TEST_F(LibYUVPlanarTest, TestCopyPlaneZero) {
  // Test to verify copying a rect with a zero height or width does
  // not touch destination memory.
  uint8_t src = 42;
  uint8_t dst = 0;

  // Disable all optimizations.
  MaskCpuFlags(disable_cpu_flags_);
  CopyPlane(&src, 0, &dst, 0, 0, 0);
  EXPECT_EQ(src, 42);
  EXPECT_EQ(dst, 0);

  CopyPlane(&src, 1, &dst, 1, 1, 0);
  EXPECT_EQ(src, 42);
  EXPECT_EQ(dst, 0);

  CopyPlane(&src, 1, &dst, 1, 0, 1);
  EXPECT_EQ(src, 42);
  EXPECT_EQ(dst, 0);

  // Enable optimizations.
  MaskCpuFlags(benchmark_cpu_info_);
  CopyPlane(&src, 0, &dst, 0, 0, 0);
  EXPECT_EQ(src, 42);
  EXPECT_EQ(dst, 0);

  CopyPlane(&src, 1, &dst, 1, 1, 0);
  EXPECT_EQ(src, 42);
  EXPECT_EQ(dst, 0);

  CopyPlane(&src, 1, &dst, 1, 0, 1);
  EXPECT_EQ(src, 42);
  EXPECT_EQ(dst, 0);
}

TEST_F(LibYUVPlanarTest, TestDetilePlane) {
  int i, j;

  // orig is tiled.  Allocate enough memory for tiles.
  int orig_width = (benchmark_width_ + 15) & ~15;
  int orig_height = (benchmark_height_ + 15) & ~15;
  int orig_plane_size = orig_width * orig_height;
  int y_plane_size = benchmark_width_ * benchmark_height_;
  align_buffer_page_end(orig_y, orig_plane_size);
  align_buffer_page_end(dst_c, y_plane_size);
  align_buffer_page_end(dst_opt, y_plane_size);

  MemRandomize(orig_y, orig_plane_size);
  memset(dst_c, 0, y_plane_size);
  memset(dst_opt, 0, y_plane_size);

  // Disable all optimizations.
  MaskCpuFlags(disable_cpu_flags_);
  for (j = 0; j < benchmark_iterations_; j++) {
    DetilePlane(orig_y, orig_width, dst_c, benchmark_width_, benchmark_width_,
                benchmark_height_, 16);
  }

  // Enable optimizations.
  MaskCpuFlags(benchmark_cpu_info_);
  for (j = 0; j < benchmark_iterations_; j++) {
    DetilePlane(orig_y, orig_width, dst_opt, benchmark_width_, benchmark_width_,
                benchmark_height_, 16);
  }

  for (i = 0; i < y_plane_size; ++i) {
    EXPECT_EQ(dst_c[i], dst_opt[i]);
  }

  free_aligned_buffer_page_end(orig_y);
  free_aligned_buffer_page_end(dst_c);
  free_aligned_buffer_page_end(dst_opt);
}

// Compares DetileSplitUV to 2 step Detile + SplitUV
TEST_F(LibYUVPlanarTest, TestDetileSplitUVPlane_Correctness) {
  int i, j;

  // orig is tiled.  Allocate enough memory for tiles.
  int orig_width = (benchmark_width_ + 15) & ~15;
  int orig_height = (benchmark_height_ + 15) & ~15;
  int orig_plane_size = orig_width * orig_height;
  int uv_plane_size = ((benchmark_width_ + 1) / 2) * benchmark_height_;
  align_buffer_page_end(orig_uv, orig_plane_size);
  align_buffer_page_end(detiled_uv, orig_plane_size);
  align_buffer_page_end(dst_u_two_stage, uv_plane_size);
  align_buffer_page_end(dst_u_opt, uv_plane_size);
  align_buffer_page_end(dst_v_two_stage, uv_plane_size);
  align_buffer_page_end(dst_v_opt, uv_plane_size);

  MemRandomize(orig_uv, orig_plane_size);
  memset(detiled_uv, 0, orig_plane_size);
  memset(dst_u_two_stage, 0, uv_plane_size);
  memset(dst_u_opt, 0, uv_plane_size);
  memset(dst_v_two_stage, 0, uv_plane_size);
  memset(dst_v_opt, 0, uv_plane_size);

  DetileSplitUVPlane(orig_uv, orig_width, dst_u_opt, (benchmark_width_ + 1) / 2,
                     dst_v_opt, (benchmark_width_ + 1) / 2, benchmark_width_,
                     benchmark_height_, 16);

  // Benchmark 2 step conversion for comparison.
  for (j = 0; j < benchmark_iterations_; j++) {
    DetilePlane(orig_uv, orig_width, detiled_uv, benchmark_width_,
                benchmark_width_, benchmark_height_, 16);
    SplitUVPlane(detiled_uv, orig_width, dst_u_two_stage,
                 (benchmark_width_ + 1) / 2, dst_v_two_stage,
                 (benchmark_width_ + 1) / 2, (benchmark_width_ + 1) / 2,
                 benchmark_height_);
  }

  for (i = 0; i < uv_plane_size; ++i) {
    EXPECT_EQ(dst_u_two_stage[i], dst_u_opt[i]);
    EXPECT_EQ(dst_v_two_stage[i], dst_v_opt[i]);
  }

  free_aligned_buffer_page_end(orig_uv);
  free_aligned_buffer_page_end(detiled_uv);
  free_aligned_buffer_page_end(dst_u_two_stage);
  free_aligned_buffer_page_end(dst_u_opt);
  free_aligned_buffer_page_end(dst_v_two_stage);
  free_aligned_buffer_page_end(dst_v_opt);
}

TEST_F(LibYUVPlanarTest, TestDetileSplitUVPlane_Benchmark) {
  int i, j;

  // orig is tiled.  Allocate enough memory for tiles.
  int orig_width = (benchmark_width_ + 15) & ~15;
  int orig_height = (benchmark_height_ + 15) & ~15;
  int orig_plane_size = orig_width * orig_height;
  int uv_plane_size = ((benchmark_width_ + 1) / 2) * benchmark_height_;
  align_buffer_page_end(orig_uv, orig_plane_size);
  align_buffer_page_end(dst_u_c, uv_plane_size);
  align_buffer_page_end(dst_u_opt, uv_plane_size);
  align_buffer_page_end(dst_v_c, uv_plane_size);
  align_buffer_page_end(dst_v_opt, uv_plane_size);

  MemRandomize(orig_uv, orig_plane_size);
  memset(dst_u_c, 0, uv_plane_size);
  memset(dst_u_opt, 0, uv_plane_size);
  memset(dst_v_c, 0, uv_plane_size);
  memset(dst_v_opt, 0, uv_plane_size);

  // Disable all optimizations.
  MaskCpuFlags(disable_cpu_flags_);

  DetileSplitUVPlane(orig_uv, orig_width, dst_u_c, (benchmark_width_ + 1) / 2,
                     dst_v_c, (benchmark_width_ + 1) / 2, benchmark_width_,
                     benchmark_height_, 16);

  // Enable optimizations.
  MaskCpuFlags(benchmark_cpu_info_);

  for (j = 0; j < benchmark_iterations_; j++) {
    DetileSplitUVPlane(
        orig_uv, orig_width, dst_u_opt, (benchmark_width_ + 1) / 2, dst_v_opt,
        (benchmark_width_ + 1) / 2, benchmark_width_, benchmark_height_, 16);
  }

  for (i = 0; i < uv_plane_size; ++i) {
    EXPECT_EQ(dst_u_c[i], dst_u_opt[i]);
    EXPECT_EQ(dst_v_c[i], dst_v_opt[i]);
  }

  free_aligned_buffer_page_end(orig_uv);
  free_aligned_buffer_page_end(dst_u_c);
  free_aligned_buffer_page_end(dst_u_opt);
  free_aligned_buffer_page_end(dst_v_c);
  free_aligned_buffer_page_end(dst_v_opt);
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
  int max_diff = TestMultiply(benchmark_width_ + 1, benchmark_height_,
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
      TestAdd(benchmark_width_ + 1, benchmark_height_, benchmark_iterations_,
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
  int max_diff = TestSubtract(benchmark_width_ + 1, benchmark_height_,
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
      TestSobel(benchmark_width_ + 1, benchmark_height_, benchmark_iterations_,
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
  int max_diff = TestSobelToPlane(benchmark_width_ + 1, benchmark_height_,
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
  int max_diff = TestSobelXY(benchmark_width_ + 1, benchmark_height_,
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
           reinterpret_cast<int32_t*>(dst_cumsum), width * 4, width,
           invert * height, radius);
  MaskCpuFlags(benchmark_cpu_info);
  for (int i = 0; i < benchmark_iterations; ++i) {
    ARGBBlur(src_argb_a + off, kStride, dst_argb_opt, kStride,
             reinterpret_cast<int32_t*>(dst_cumsum), width * 4, width,
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

#if !defined(DISABLE_SLOW_TESTS) || defined(__x86_64__) || defined(__i386__)
#define DISABLED_ARM(name) name
#else
#define DISABLED_ARM(name) DISABLED_##name
#endif

static const int kBlurSize = 55;
TEST_F(LibYUVPlanarTest, DISABLED_ARM(ARGBBlur_Any)) {
  int max_diff =
      TestBlur(benchmark_width_ + 1, benchmark_height_, benchmark_iterations_,
               disable_cpu_flags_, benchmark_cpu_info_, +1, 0, kBlurSize);
  EXPECT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, DISABLED_ARM(ARGBBlur_Unaligned)) {
  int max_diff =
      TestBlur(benchmark_width_, benchmark_height_, benchmark_iterations_,
               disable_cpu_flags_, benchmark_cpu_info_, +1, 1, kBlurSize);
  EXPECT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, DISABLED_ARM(ARGBBlur_Invert)) {
  int max_diff =
      TestBlur(benchmark_width_, benchmark_height_, benchmark_iterations_,
               disable_cpu_flags_, benchmark_cpu_info_, -1, 0, kBlurSize);
  EXPECT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, DISABLED_ARM(ARGBBlur_Opt)) {
  int max_diff =
      TestBlur(benchmark_width_, benchmark_height_, benchmark_iterations_,
               disable_cpu_flags_, benchmark_cpu_info_, +1, 0, kBlurSize);
  EXPECT_LE(max_diff, 1);
}

static const int kBlurSmallSize = 5;
TEST_F(LibYUVPlanarTest, DISABLED_ARM(ARGBBlurSmall_Any)) {
  int max_diff =
      TestBlur(benchmark_width_ + 1, benchmark_height_, benchmark_iterations_,
               disable_cpu_flags_, benchmark_cpu_info_, +1, 0, kBlurSmallSize);
  EXPECT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, DISABLED_ARM(ARGBBlurSmall_Unaligned)) {
  int max_diff =
      TestBlur(benchmark_width_, benchmark_height_, benchmark_iterations_,
               disable_cpu_flags_, benchmark_cpu_info_, +1, 1, kBlurSmallSize);
  EXPECT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, DISABLED_ARM(ARGBBlurSmall_Invert)) {
  int max_diff =
      TestBlur(benchmark_width_, benchmark_height_, benchmark_iterations_,
               disable_cpu_flags_, benchmark_cpu_info_, -1, 0, kBlurSmallSize);
  EXPECT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, DISABLED_ARM(ARGBBlurSmall_Opt)) {
  int max_diff =
      TestBlur(benchmark_width_, benchmark_height_, benchmark_iterations_,
               disable_cpu_flags_, benchmark_cpu_info_, +1, 0, kBlurSmallSize);
  EXPECT_LE(max_diff, 1);
}

TEST_F(LibYUVPlanarTest, DISABLED_ARM(TestARGBPolynomial)) {
  SIMD_ALIGNED(uint8_t orig_pixels[1280][4]);
  SIMD_ALIGNED(uint8_t dst_pixels_opt[1280][4]);
  SIMD_ALIGNED(uint8_t dst_pixels_c[1280][4]);
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
  uint8_t* dst_opt = orig_y + y_plane_size;
  uint8_t* dst_c = orig_y + y_plane_size * 2;

  MemRandomize(orig_y, y_plane_size);
  memset(dst_c, 0, y_plane_size);
  memset(dst_opt, 1, y_plane_size);

  for (i = 0; i < y_plane_size / 2; ++i) {
    reinterpret_cast<uint16_t*>(orig_y)[i] &= mask;
  }

  // Disable all optimizations.
  MaskCpuFlags(disable_cpu_flags);
  for (j = 0; j < benchmark_iterations; j++) {
    HalfFloatPlane(reinterpret_cast<uint16_t*>(orig_y), benchmark_width * 2,
                   reinterpret_cast<uint16_t*>(dst_c), benchmark_width * 2,
                   scale, benchmark_width, benchmark_height);
  }

  // Enable optimizations.
  MaskCpuFlags(benchmark_cpu_info);
  for (j = 0; j < benchmark_iterations; j++) {
    HalfFloatPlane(reinterpret_cast<uint16_t*>(orig_y), benchmark_width * 2,
                   reinterpret_cast<uint16_t*>(dst_opt), benchmark_width * 2,
                   scale, benchmark_width, benchmark_height);
  }

  int max_diff = 0;
  for (i = 0; i < y_plane_size / 2; ++i) {
    int abs_diff =
        abs(static_cast<int>(reinterpret_cast<uint16_t*>(dst_c)[i]) -
            static_cast<int>(reinterpret_cast<uint16_t*>(dst_opt)[i]));
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

float TestByteToFloat(int benchmark_width,
                      int benchmark_height,
                      int benchmark_iterations,
                      int disable_cpu_flags,
                      int benchmark_cpu_info,
                      float scale) {
  int i, j;
  const int y_plane_size = benchmark_width * benchmark_height;

  align_buffer_page_end(orig_y, y_plane_size * (1 + 4 + 4));
  float* dst_opt = reinterpret_cast<float*>(orig_y + y_plane_size);
  float* dst_c = reinterpret_cast<float*>(orig_y + y_plane_size * 5);

  MemRandomize(orig_y, y_plane_size);
  memset(dst_c, 0, y_plane_size * 4);
  memset(dst_opt, 1, y_plane_size * 4);

  // Disable all optimizations.
  MaskCpuFlags(disable_cpu_flags);
  ByteToFloat(orig_y, dst_c, scale, y_plane_size);

  // Enable optimizations.
  MaskCpuFlags(benchmark_cpu_info);
  for (j = 0; j < benchmark_iterations; j++) {
    ByteToFloat(orig_y, dst_opt, scale, y_plane_size);
  }

  float max_diff = 0;
  for (i = 0; i < y_plane_size; ++i) {
    float abs_diff = fabs(dst_c[i] - dst_opt[i]);
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }

  free_aligned_buffer_page_end(orig_y);
  return max_diff;
}

TEST_F(LibYUVPlanarTest, TestByteToFloat) {
  float diff = TestByteToFloat(benchmark_width_, benchmark_height_,
                               benchmark_iterations_, disable_cpu_flags_,
                               benchmark_cpu_info_, 1.0f);
  EXPECT_EQ(0.f, diff);
}

TEST_F(LibYUVPlanarTest, TestARGBLumaColorTable) {
  SIMD_ALIGNED(uint8_t orig_pixels[1280][4]);
  SIMD_ALIGNED(uint8_t dst_pixels_opt[1280][4]);
  SIMD_ALIGNED(uint8_t dst_pixels_c[1280][4]);
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
  const uint32_t v32 = fastrand() & (bpp == 4 ? 0xffffffff : 0xff);

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
  int max_diff = TestARGBRect(benchmark_width_ + 1, benchmark_height_,
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
  int max_diff = TestARGBRect(benchmark_width_ + 1, benchmark_height_,
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
  align_buffer_page_end(src_pixels_u, kPixels);
  align_buffer_page_end(src_pixels_v, kPixels);
  align_buffer_page_end(dst_pixels_opt, kPixels * 2);
  align_buffer_page_end(dst_pixels_c, kPixels * 2);

  MemRandomize(src_pixels_u, kPixels);
  MemRandomize(src_pixels_v, kPixels);
  MemRandomize(dst_pixels_opt, kPixels * 2);
  MemRandomize(dst_pixels_c, kPixels * 2);

  MaskCpuFlags(disable_cpu_flags_);
  MergeUVPlane(src_pixels_u, benchmark_width_, src_pixels_v, benchmark_width_,
               dst_pixels_c, benchmark_width_ * 2, benchmark_width_,
               benchmark_height_);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    MergeUVPlane(src_pixels_u, benchmark_width_, src_pixels_v, benchmark_width_,
                 dst_pixels_opt, benchmark_width_ * 2, benchmark_width_,
                 benchmark_height_);
  }

  for (int i = 0; i < kPixels * 2; ++i) {
    EXPECT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
  }

  free_aligned_buffer_page_end(src_pixels_u);
  free_aligned_buffer_page_end(src_pixels_v);
  free_aligned_buffer_page_end(dst_pixels_opt);
  free_aligned_buffer_page_end(dst_pixels_c);
}

// 16 bit channel split and merge
TEST_F(LibYUVPlanarTest, MergeUVPlane_16_Opt) {
  const int kPixels = benchmark_width_ * benchmark_height_;
  align_buffer_page_end(src_pixels_u, kPixels * 2);
  align_buffer_page_end(src_pixels_v, kPixels * 2);
  align_buffer_page_end(dst_pixels_opt, kPixels * 2 * 2);
  align_buffer_page_end(dst_pixels_c, kPixels * 2 * 2);
  MemRandomize(src_pixels_u, kPixels * 2);
  MemRandomize(src_pixels_v, kPixels * 2);
  MemRandomize(dst_pixels_opt, kPixels * 2 * 2);
  MemRandomize(dst_pixels_c, kPixels * 2 * 2);

  MaskCpuFlags(disable_cpu_flags_);
  MergeUVPlane_16((const uint16_t*)src_pixels_u, benchmark_width_,
                  (const uint16_t*)src_pixels_v, benchmark_width_,
                  (uint16_t*)dst_pixels_c, benchmark_width_ * 2,
                  benchmark_width_, benchmark_height_, 12);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    MergeUVPlane_16((const uint16_t*)src_pixels_u, benchmark_width_,
                    (const uint16_t*)src_pixels_v, benchmark_width_,
                    (uint16_t*)dst_pixels_opt, benchmark_width_ * 2,
                    benchmark_width_, benchmark_height_, 12);
  }

  for (int i = 0; i < kPixels * 2 * 2; ++i) {
    EXPECT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
  }
  free_aligned_buffer_page_end(src_pixels_u);
  free_aligned_buffer_page_end(src_pixels_v);
  free_aligned_buffer_page_end(dst_pixels_opt);
  free_aligned_buffer_page_end(dst_pixels_c);
}

TEST_F(LibYUVPlanarTest, SplitUVPlane_Opt) {
  const int kPixels = benchmark_width_ * benchmark_height_;
  align_buffer_page_end(src_pixels, kPixels * 2);
  align_buffer_page_end(dst_pixels_u_c, kPixels);
  align_buffer_page_end(dst_pixels_v_c, kPixels);
  align_buffer_page_end(dst_pixels_u_opt, kPixels);
  align_buffer_page_end(dst_pixels_v_opt, kPixels);

  MemRandomize(src_pixels, kPixels * 2);
  MemRandomize(dst_pixels_u_c, kPixels);
  MemRandomize(dst_pixels_v_c, kPixels);
  MemRandomize(dst_pixels_u_opt, kPixels);
  MemRandomize(dst_pixels_v_opt, kPixels);

  MaskCpuFlags(disable_cpu_flags_);
  SplitUVPlane(src_pixels, benchmark_width_ * 2, dst_pixels_u_c,
               benchmark_width_, dst_pixels_v_c, benchmark_width_,
               benchmark_width_, benchmark_height_);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    SplitUVPlane(src_pixels, benchmark_width_ * 2, dst_pixels_u_opt,
                 benchmark_width_, dst_pixels_v_opt, benchmark_width_,
                 benchmark_width_, benchmark_height_);
  }

  for (int i = 0; i < kPixels; ++i) {
    EXPECT_EQ(dst_pixels_u_c[i], dst_pixels_u_opt[i]);
    EXPECT_EQ(dst_pixels_v_c[i], dst_pixels_v_opt[i]);
  }

  free_aligned_buffer_page_end(src_pixels);
  free_aligned_buffer_page_end(dst_pixels_u_c);
  free_aligned_buffer_page_end(dst_pixels_v_c);
  free_aligned_buffer_page_end(dst_pixels_u_opt);
  free_aligned_buffer_page_end(dst_pixels_v_opt);
}

// 16 bit channel split
TEST_F(LibYUVPlanarTest, SplitUVPlane_16_Opt) {
  const int kPixels = benchmark_width_ * benchmark_height_;
  align_buffer_page_end(src_pixels, kPixels * 2 * 2);
  align_buffer_page_end(dst_pixels_u_c, kPixels * 2);
  align_buffer_page_end(dst_pixels_v_c, kPixels * 2);
  align_buffer_page_end(dst_pixels_u_opt, kPixels * 2);
  align_buffer_page_end(dst_pixels_v_opt, kPixels * 2);
  MemRandomize(src_pixels, kPixels * 2 * 2);
  MemRandomize(dst_pixels_u_c, kPixels * 2);
  MemRandomize(dst_pixels_v_c, kPixels * 2);
  MemRandomize(dst_pixels_u_opt, kPixels * 2);
  MemRandomize(dst_pixels_v_opt, kPixels * 2);

  MaskCpuFlags(disable_cpu_flags_);
  SplitUVPlane_16((const uint16_t*)src_pixels, benchmark_width_ * 2,
                  (uint16_t*)dst_pixels_u_c, benchmark_width_,
                  (uint16_t*)dst_pixels_v_c, benchmark_width_, benchmark_width_,
                  benchmark_height_, 10);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    SplitUVPlane_16((const uint16_t*)src_pixels, benchmark_width_ * 2,
                    (uint16_t*)dst_pixels_u_opt, benchmark_width_,
                    (uint16_t*)dst_pixels_v_opt, benchmark_width_,
                    benchmark_width_, benchmark_height_, 10);
  }

  for (int i = 0; i < kPixels * 2; ++i) {
    EXPECT_EQ(dst_pixels_u_c[i], dst_pixels_u_opt[i]);
    EXPECT_EQ(dst_pixels_v_c[i], dst_pixels_v_opt[i]);
  }
  free_aligned_buffer_page_end(src_pixels);
  free_aligned_buffer_page_end(dst_pixels_u_c);
  free_aligned_buffer_page_end(dst_pixels_v_c);
  free_aligned_buffer_page_end(dst_pixels_u_opt);
  free_aligned_buffer_page_end(dst_pixels_v_opt);
}

TEST_F(LibYUVPlanarTest, SwapUVPlane_Opt) {
  // Round count up to multiple of 16
  const int kPixels = benchmark_width_ * benchmark_height_;
  align_buffer_page_end(src_pixels, kPixels * 2);
  align_buffer_page_end(dst_pixels_opt, kPixels * 2);
  align_buffer_page_end(dst_pixels_c, kPixels * 2);

  MemRandomize(src_pixels, kPixels * 2);
  MemRandomize(dst_pixels_opt, kPixels * 2);
  MemRandomize(dst_pixels_c, kPixels * 2);

  MaskCpuFlags(disable_cpu_flags_);
  SwapUVPlane(src_pixels, benchmark_width_ * 2, dst_pixels_c,
              benchmark_width_ * 2, benchmark_width_, benchmark_height_);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    SwapUVPlane(src_pixels, benchmark_width_ * 2, dst_pixels_opt,
                benchmark_width_ * 2, benchmark_width_, benchmark_height_);
  }

  for (int i = 0; i < kPixels * 2; ++i) {
    EXPECT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
  }

  free_aligned_buffer_page_end(src_pixels);
  free_aligned_buffer_page_end(dst_pixels_opt);
  free_aligned_buffer_page_end(dst_pixels_c);
}

TEST_F(LibYUVPlanarTest, MergeRGBPlane_Opt) {
  // Round count up to multiple of 16
  const int kPixels = benchmark_width_ * benchmark_height_;
  align_buffer_page_end(src_pixels, kPixels * 3);
  align_buffer_page_end(tmp_pixels_r, kPixels);
  align_buffer_page_end(tmp_pixels_g, kPixels);
  align_buffer_page_end(tmp_pixels_b, kPixels);
  align_buffer_page_end(dst_pixels_opt, kPixels * 3);
  align_buffer_page_end(dst_pixels_c, kPixels * 3);

  MemRandomize(src_pixels, kPixels * 3);
  MemRandomize(tmp_pixels_r, kPixels);
  MemRandomize(tmp_pixels_g, kPixels);
  MemRandomize(tmp_pixels_b, kPixels);
  MemRandomize(dst_pixels_opt, kPixels * 3);
  MemRandomize(dst_pixels_c, kPixels * 3);

  MaskCpuFlags(disable_cpu_flags_);
  SplitRGBPlane(src_pixels, benchmark_width_ * 3, tmp_pixels_r,
                benchmark_width_, tmp_pixels_g, benchmark_width_, tmp_pixels_b,
                benchmark_width_, benchmark_width_, benchmark_height_);
  MergeRGBPlane(tmp_pixels_r, benchmark_width_, tmp_pixels_g, benchmark_width_,
                tmp_pixels_b, benchmark_width_, dst_pixels_c,
                benchmark_width_ * 3, benchmark_width_, benchmark_height_);
  MaskCpuFlags(benchmark_cpu_info_);

  SplitRGBPlane(src_pixels, benchmark_width_ * 3, tmp_pixels_r,
                benchmark_width_, tmp_pixels_g, benchmark_width_, tmp_pixels_b,
                benchmark_width_, benchmark_width_, benchmark_height_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    MergeRGBPlane(tmp_pixels_r, benchmark_width_, tmp_pixels_g,
                  benchmark_width_, tmp_pixels_b, benchmark_width_,
                  dst_pixels_opt, benchmark_width_ * 3, benchmark_width_,
                  benchmark_height_);
  }

  for (int i = 0; i < kPixels * 3; ++i) {
    EXPECT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
  }

  free_aligned_buffer_page_end(src_pixels);
  free_aligned_buffer_page_end(tmp_pixels_r);
  free_aligned_buffer_page_end(tmp_pixels_g);
  free_aligned_buffer_page_end(tmp_pixels_b);
  free_aligned_buffer_page_end(dst_pixels_opt);
  free_aligned_buffer_page_end(dst_pixels_c);
}

TEST_F(LibYUVPlanarTest, SplitRGBPlane_Opt) {
  // Round count up to multiple of 16
  const int kPixels = benchmark_width_ * benchmark_height_;
  align_buffer_page_end(src_pixels, kPixels * 3);
  align_buffer_page_end(tmp_pixels_r, kPixels);
  align_buffer_page_end(tmp_pixels_g, kPixels);
  align_buffer_page_end(tmp_pixels_b, kPixels);
  align_buffer_page_end(dst_pixels_opt, kPixels * 3);
  align_buffer_page_end(dst_pixels_c, kPixels * 3);

  MemRandomize(src_pixels, kPixels * 3);
  MemRandomize(tmp_pixels_r, kPixels);
  MemRandomize(tmp_pixels_g, kPixels);
  MemRandomize(tmp_pixels_b, kPixels);
  MemRandomize(dst_pixels_opt, kPixels * 3);
  MemRandomize(dst_pixels_c, kPixels * 3);

  MaskCpuFlags(disable_cpu_flags_);
  SplitRGBPlane(src_pixels, benchmark_width_ * 3, tmp_pixels_r,
                benchmark_width_, tmp_pixels_g, benchmark_width_, tmp_pixels_b,
                benchmark_width_, benchmark_width_, benchmark_height_);
  MergeRGBPlane(tmp_pixels_r, benchmark_width_, tmp_pixels_g, benchmark_width_,
                tmp_pixels_b, benchmark_width_, dst_pixels_c,
                benchmark_width_ * 3, benchmark_width_, benchmark_height_);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    SplitRGBPlane(src_pixels, benchmark_width_ * 3, tmp_pixels_r,
                  benchmark_width_, tmp_pixels_g, benchmark_width_,
                  tmp_pixels_b, benchmark_width_, benchmark_width_,
                  benchmark_height_);
  }
  MergeRGBPlane(tmp_pixels_r, benchmark_width_, tmp_pixels_g, benchmark_width_,
                tmp_pixels_b, benchmark_width_, dst_pixels_opt,
                benchmark_width_ * 3, benchmark_width_, benchmark_height_);

  for (int i = 0; i < kPixels * 3; ++i) {
    EXPECT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
  }

  free_aligned_buffer_page_end(src_pixels);
  free_aligned_buffer_page_end(tmp_pixels_r);
  free_aligned_buffer_page_end(tmp_pixels_g);
  free_aligned_buffer_page_end(tmp_pixels_b);
  free_aligned_buffer_page_end(dst_pixels_opt);
  free_aligned_buffer_page_end(dst_pixels_c);
}

TEST_F(LibYUVPlanarTest, MergeARGBPlane_Opt) {
  const int kPixels = benchmark_width_ * benchmark_height_;
  align_buffer_page_end(src_pixels, kPixels * 4);
  align_buffer_page_end(tmp_pixels_r, kPixels);
  align_buffer_page_end(tmp_pixels_g, kPixels);
  align_buffer_page_end(tmp_pixels_b, kPixels);
  align_buffer_page_end(tmp_pixels_a, kPixels);
  align_buffer_page_end(dst_pixels_opt, kPixels * 4);
  align_buffer_page_end(dst_pixels_c, kPixels * 4);

  MemRandomize(src_pixels, kPixels * 4);
  MemRandomize(tmp_pixels_r, kPixels);
  MemRandomize(tmp_pixels_g, kPixels);
  MemRandomize(tmp_pixels_b, kPixels);
  MemRandomize(tmp_pixels_a, kPixels);
  MemRandomize(dst_pixels_opt, kPixels * 4);
  MemRandomize(dst_pixels_c, kPixels * 4);

  MaskCpuFlags(disable_cpu_flags_);
  SplitARGBPlane(src_pixels, benchmark_width_ * 4, tmp_pixels_r,
                 benchmark_width_, tmp_pixels_g, benchmark_width_, tmp_pixels_b,
                 benchmark_width_, tmp_pixels_a, benchmark_width_,
                 benchmark_width_, benchmark_height_);
  MergeARGBPlane(tmp_pixels_r, benchmark_width_, tmp_pixels_g, benchmark_width_,
                 tmp_pixels_b, benchmark_width_, tmp_pixels_a, benchmark_width_,
                 dst_pixels_c, benchmark_width_ * 4, benchmark_width_,
                 benchmark_height_);

  MaskCpuFlags(benchmark_cpu_info_);
  SplitARGBPlane(src_pixels, benchmark_width_ * 4, tmp_pixels_r,
                 benchmark_width_, tmp_pixels_g, benchmark_width_, tmp_pixels_b,
                 benchmark_width_, tmp_pixels_a, benchmark_width_,
                 benchmark_width_, benchmark_height_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    MergeARGBPlane(tmp_pixels_r, benchmark_width_, tmp_pixels_g,
                   benchmark_width_, tmp_pixels_b, benchmark_width_,
                   tmp_pixels_a, benchmark_width_, dst_pixels_opt,
                   benchmark_width_ * 4, benchmark_width_, benchmark_height_);
  }

  for (int i = 0; i < kPixels * 4; ++i) {
    EXPECT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
  }

  free_aligned_buffer_page_end(src_pixels);
  free_aligned_buffer_page_end(tmp_pixels_r);
  free_aligned_buffer_page_end(tmp_pixels_g);
  free_aligned_buffer_page_end(tmp_pixels_b);
  free_aligned_buffer_page_end(tmp_pixels_a);
  free_aligned_buffer_page_end(dst_pixels_opt);
  free_aligned_buffer_page_end(dst_pixels_c);
}

TEST_F(LibYUVPlanarTest, SplitARGBPlane_Opt) {
  const int kPixels = benchmark_width_ * benchmark_height_;
  align_buffer_page_end(src_pixels, kPixels * 4);
  align_buffer_page_end(tmp_pixels_r, kPixels);
  align_buffer_page_end(tmp_pixels_g, kPixels);
  align_buffer_page_end(tmp_pixels_b, kPixels);
  align_buffer_page_end(tmp_pixels_a, kPixels);
  align_buffer_page_end(dst_pixels_opt, kPixels * 4);
  align_buffer_page_end(dst_pixels_c, kPixels * 4);

  MemRandomize(src_pixels, kPixels * 4);
  MemRandomize(tmp_pixels_r, kPixels);
  MemRandomize(tmp_pixels_g, kPixels);
  MemRandomize(tmp_pixels_b, kPixels);
  MemRandomize(tmp_pixels_a, kPixels);
  MemRandomize(dst_pixels_opt, kPixels * 4);
  MemRandomize(dst_pixels_c, kPixels * 4);

  MaskCpuFlags(disable_cpu_flags_);
  SplitARGBPlane(src_pixels, benchmark_width_ * 4, tmp_pixels_r,
                 benchmark_width_, tmp_pixels_g, benchmark_width_, tmp_pixels_b,
                 benchmark_width_, tmp_pixels_a, benchmark_width_,
                 benchmark_width_, benchmark_height_);
  MergeARGBPlane(tmp_pixels_r, benchmark_width_, tmp_pixels_g, benchmark_width_,
                 tmp_pixels_b, benchmark_width_, tmp_pixels_a, benchmark_width_,
                 dst_pixels_c, benchmark_width_ * 4, benchmark_width_,
                 benchmark_height_);

  MaskCpuFlags(benchmark_cpu_info_);
  for (int i = 0; i < benchmark_iterations_; ++i) {
    SplitARGBPlane(src_pixels, benchmark_width_ * 4, tmp_pixels_r,
                   benchmark_width_, tmp_pixels_g, benchmark_width_,
                   tmp_pixels_b, benchmark_width_, tmp_pixels_a,
                   benchmark_width_, benchmark_width_, benchmark_height_);
  }

  MergeARGBPlane(tmp_pixels_r, benchmark_width_, tmp_pixels_g, benchmark_width_,
                 tmp_pixels_b, benchmark_width_, tmp_pixels_a, benchmark_width_,
                 dst_pixels_opt, benchmark_width_ * 4, benchmark_width_,
                 benchmark_height_);

  for (int i = 0; i < kPixels * 4; ++i) {
    EXPECT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
  }

  free_aligned_buffer_page_end(src_pixels);
  free_aligned_buffer_page_end(tmp_pixels_r);
  free_aligned_buffer_page_end(tmp_pixels_g);
  free_aligned_buffer_page_end(tmp_pixels_b);
  free_aligned_buffer_page_end(tmp_pixels_a);
  free_aligned_buffer_page_end(dst_pixels_opt);
  free_aligned_buffer_page_end(dst_pixels_c);
}

TEST_F(LibYUVPlanarTest, MergeXRGBPlane_Opt) {
  const int kPixels = benchmark_width_ * benchmark_height_;
  align_buffer_page_end(src_pixels, kPixels * 4);
  align_buffer_page_end(tmp_pixels_r, kPixels);
  align_buffer_page_end(tmp_pixels_g, kPixels);
  align_buffer_page_end(tmp_pixels_b, kPixels);
  align_buffer_page_end(dst_pixels_opt, kPixels * 4);
  align_buffer_page_end(dst_pixels_c, kPixels * 4);

  MemRandomize(src_pixels, kPixels * 4);
  MemRandomize(tmp_pixels_r, kPixels);
  MemRandomize(tmp_pixels_g, kPixels);
  MemRandomize(tmp_pixels_b, kPixels);
  MemRandomize(dst_pixels_opt, kPixels * 4);
  MemRandomize(dst_pixels_c, kPixels * 4);

  MaskCpuFlags(disable_cpu_flags_);
  SplitARGBPlane(src_pixels, benchmark_width_ * 4, tmp_pixels_r,
                 benchmark_width_, tmp_pixels_g, benchmark_width_, tmp_pixels_b,
                 benchmark_width_, NULL, 0, benchmark_width_,
                 benchmark_height_);
  MergeARGBPlane(tmp_pixels_r, benchmark_width_, tmp_pixels_g, benchmark_width_,
                 tmp_pixels_b, benchmark_width_, NULL, 0, dst_pixels_c,
                 benchmark_width_ * 4, benchmark_width_, benchmark_height_);

  MaskCpuFlags(benchmark_cpu_info_);
  SplitARGBPlane(src_pixels, benchmark_width_ * 4, tmp_pixels_r,
                 benchmark_width_, tmp_pixels_g, benchmark_width_, tmp_pixels_b,
                 benchmark_width_, NULL, 0, benchmark_width_,
                 benchmark_height_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    MergeARGBPlane(tmp_pixels_r, benchmark_width_, tmp_pixels_g,
                   benchmark_width_, tmp_pixels_b, benchmark_width_, NULL, 0,
                   dst_pixels_opt, benchmark_width_ * 4, benchmark_width_,
                   benchmark_height_);
  }

  for (int i = 0; i < kPixels * 4; ++i) {
    EXPECT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
  }

  free_aligned_buffer_page_end(src_pixels);
  free_aligned_buffer_page_end(tmp_pixels_r);
  free_aligned_buffer_page_end(tmp_pixels_g);
  free_aligned_buffer_page_end(tmp_pixels_b);
  free_aligned_buffer_page_end(dst_pixels_opt);
  free_aligned_buffer_page_end(dst_pixels_c);
}

TEST_F(LibYUVPlanarTest, SplitXRGBPlane_Opt) {
  const int kPixels = benchmark_width_ * benchmark_height_;
  align_buffer_page_end(src_pixels, kPixels * 4);
  align_buffer_page_end(tmp_pixels_r, kPixels);
  align_buffer_page_end(tmp_pixels_g, kPixels);
  align_buffer_page_end(tmp_pixels_b, kPixels);
  align_buffer_page_end(dst_pixels_opt, kPixels * 4);
  align_buffer_page_end(dst_pixels_c, kPixels * 4);

  MemRandomize(src_pixels, kPixels * 4);
  MemRandomize(tmp_pixels_r, kPixels);
  MemRandomize(tmp_pixels_g, kPixels);
  MemRandomize(tmp_pixels_b, kPixels);
  MemRandomize(dst_pixels_opt, kPixels * 4);
  MemRandomize(dst_pixels_c, kPixels * 4);

  MaskCpuFlags(disable_cpu_flags_);
  SplitARGBPlane(src_pixels, benchmark_width_ * 4, tmp_pixels_r,
                 benchmark_width_, tmp_pixels_g, benchmark_width_, tmp_pixels_b,
                 benchmark_width_, NULL, 0, benchmark_width_,
                 benchmark_height_);
  MergeARGBPlane(tmp_pixels_r, benchmark_width_, tmp_pixels_g, benchmark_width_,
                 tmp_pixels_b, benchmark_width_, NULL, 0, dst_pixels_c,
                 benchmark_width_ * 4, benchmark_width_, benchmark_height_);

  MaskCpuFlags(benchmark_cpu_info_);
  for (int i = 0; i < benchmark_iterations_; ++i) {
    SplitARGBPlane(src_pixels, benchmark_width_ * 4, tmp_pixels_r,
                   benchmark_width_, tmp_pixels_g, benchmark_width_,
                   tmp_pixels_b, benchmark_width_, NULL, 0, benchmark_width_,
                   benchmark_height_);
  }

  MergeARGBPlane(tmp_pixels_r, benchmark_width_, tmp_pixels_g, benchmark_width_,
                 tmp_pixels_b, benchmark_width_, NULL, 0, dst_pixels_opt,
                 benchmark_width_ * 4, benchmark_width_, benchmark_height_);

  for (int i = 0; i < kPixels * 4; ++i) {
    EXPECT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
  }

  free_aligned_buffer_page_end(src_pixels);
  free_aligned_buffer_page_end(tmp_pixels_r);
  free_aligned_buffer_page_end(tmp_pixels_g);
  free_aligned_buffer_page_end(tmp_pixels_b);
  free_aligned_buffer_page_end(dst_pixels_opt);
  free_aligned_buffer_page_end(dst_pixels_c);
}

// Merge 4 channels
#define TESTQPLANARTOPI(FUNC, STYPE, DTYPE, DEPTH, W1280, N, NEG, OFF)      \
  TEST_F(LibYUVPlanarTest, FUNC##Plane_##DEPTH##N) {                        \
    const int kWidth = W1280;                                               \
    const int kPixels = kWidth * benchmark_height_;                         \
    align_buffer_page_end(src_memory_r, kPixels * sizeof(STYPE) + OFF);     \
    align_buffer_page_end(src_memory_g, kPixels * sizeof(STYPE) + OFF);     \
    align_buffer_page_end(src_memory_b, kPixels * sizeof(STYPE) + OFF);     \
    align_buffer_page_end(src_memory_a, kPixels * sizeof(STYPE) + OFF);     \
    align_buffer_page_end(dst_memory_c, kPixels * 4 * sizeof(DTYPE));       \
    align_buffer_page_end(dst_memory_opt, kPixels * 4 * sizeof(DTYPE));     \
    MemRandomize(src_memory_r, kPixels * sizeof(STYPE) + OFF);              \
    MemRandomize(src_memory_g, kPixels * sizeof(STYPE) + OFF);              \
    MemRandomize(src_memory_b, kPixels * sizeof(STYPE) + OFF);              \
    MemRandomize(src_memory_a, kPixels * sizeof(STYPE) + OFF);              \
    memset(dst_memory_c, 0, kPixels * 4 * sizeof(DTYPE));                   \
    memset(dst_memory_opt, 0, kPixels * 4 * sizeof(DTYPE));                 \
    STYPE* src_pixels_r = reinterpret_cast<STYPE*>(src_memory_r + OFF);     \
    STYPE* src_pixels_g = reinterpret_cast<STYPE*>(src_memory_g + OFF);     \
    STYPE* src_pixels_b = reinterpret_cast<STYPE*>(src_memory_b + OFF);     \
    STYPE* src_pixels_a = reinterpret_cast<STYPE*>(src_memory_a + OFF);     \
    DTYPE* dst_pixels_c = reinterpret_cast<DTYPE*>(dst_memory_c);           \
    DTYPE* dst_pixels_opt = reinterpret_cast<DTYPE*>(dst_memory_opt);       \
    MaskCpuFlags(disable_cpu_flags_);                                       \
    FUNC##Plane(src_pixels_r, kWidth, src_pixels_g, kWidth, src_pixels_b,   \
                kWidth, src_pixels_a, kWidth, dst_pixels_c, kWidth * 4,     \
                kWidth, NEG benchmark_height_, DEPTH);                      \
    MaskCpuFlags(benchmark_cpu_info_);                                      \
    for (int i = 0; i < benchmark_iterations_; ++i) {                       \
      FUNC##Plane(src_pixels_r, kWidth, src_pixels_g, kWidth, src_pixels_b, \
                  kWidth, src_pixels_a, kWidth, dst_pixels_opt, kWidth * 4, \
                  kWidth, NEG benchmark_height_, DEPTH);                    \
    }                                                                       \
    for (int i = 0; i < kPixels * 4; ++i) {                                 \
      EXPECT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);                        \
    }                                                                       \
    free_aligned_buffer_page_end(src_memory_r);                             \
    free_aligned_buffer_page_end(src_memory_g);                             \
    free_aligned_buffer_page_end(src_memory_b);                             \
    free_aligned_buffer_page_end(src_memory_a);                             \
    free_aligned_buffer_page_end(dst_memory_c);                             \
    free_aligned_buffer_page_end(dst_memory_opt);                           \
  }

// Merge 3 channel RGB into 4 channel XRGB with opaque alpha
#define TESTQPLANAROTOPI(FUNC, STYPE, DTYPE, DEPTH, W1280, N, NEG, OFF)     \
  TEST_F(LibYUVPlanarTest, FUNC##Plane_Opaque_##DEPTH##N) {                 \
    const int kWidth = W1280;                                               \
    const int kPixels = kWidth * benchmark_height_;                         \
    align_buffer_page_end(src_memory_r, kPixels * sizeof(STYPE) + OFF);     \
    align_buffer_page_end(src_memory_g, kPixels * sizeof(STYPE) + OFF);     \
    align_buffer_page_end(src_memory_b, kPixels * sizeof(STYPE) + OFF);     \
    align_buffer_page_end(dst_memory_c, kPixels * 4 * sizeof(DTYPE));       \
    align_buffer_page_end(dst_memory_opt, kPixels * 4 * sizeof(DTYPE));     \
    MemRandomize(src_memory_r, kPixels * sizeof(STYPE) + OFF);              \
    MemRandomize(src_memory_g, kPixels * sizeof(STYPE) + OFF);              \
    MemRandomize(src_memory_b, kPixels * sizeof(STYPE) + OFF);              \
    memset(dst_memory_c, 0, kPixels * 4 * sizeof(DTYPE));                   \
    memset(dst_memory_opt, 0, kPixels * 4 * sizeof(DTYPE));                 \
    STYPE* src_pixels_r = reinterpret_cast<STYPE*>(src_memory_r + OFF);     \
    STYPE* src_pixels_g = reinterpret_cast<STYPE*>(src_memory_g + OFF);     \
    STYPE* src_pixels_b = reinterpret_cast<STYPE*>(src_memory_b + OFF);     \
    DTYPE* dst_pixels_c = reinterpret_cast<DTYPE*>(dst_memory_c);           \
    DTYPE* dst_pixels_opt = reinterpret_cast<DTYPE*>(dst_memory_opt);       \
    MaskCpuFlags(disable_cpu_flags_);                                       \
    FUNC##Plane(src_pixels_r, kWidth, src_pixels_g, kWidth, src_pixels_b,   \
                kWidth, NULL, 0, dst_pixels_c, kWidth * 4, kWidth,          \
                NEG benchmark_height_, DEPTH);                              \
    MaskCpuFlags(benchmark_cpu_info_);                                      \
    for (int i = 0; i < benchmark_iterations_; ++i) {                       \
      FUNC##Plane(src_pixels_r, kWidth, src_pixels_g, kWidth, src_pixels_b, \
                  kWidth, NULL, 0, dst_pixels_opt, kWidth * 4, kWidth,      \
                  NEG benchmark_height_, DEPTH);                            \
    }                                                                       \
    for (int i = 0; i < kPixels * 4; ++i) {                                 \
      EXPECT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);                        \
    }                                                                       \
    free_aligned_buffer_page_end(src_memory_r);                             \
    free_aligned_buffer_page_end(src_memory_g);                             \
    free_aligned_buffer_page_end(src_memory_b);                             \
    free_aligned_buffer_page_end(dst_memory_c);                             \
    free_aligned_buffer_page_end(dst_memory_opt);                           \
  }

#define TESTQPLANARTOP(FUNC, STYPE, DTYPE, DEPTH)                              \
  TESTQPLANARTOPI(FUNC, STYPE, DTYPE, DEPTH, benchmark_width_ + 1, _Any, +, 0) \
  TESTQPLANARTOPI(FUNC, STYPE, DTYPE, DEPTH, benchmark_width_, _Unaligned, +,  \
                  2)                                                           \
  TESTQPLANARTOPI(FUNC, STYPE, DTYPE, DEPTH, benchmark_width_, _Invert, -, 0)  \
  TESTQPLANARTOPI(FUNC, STYPE, DTYPE, DEPTH, benchmark_width_, _Opt, +, 0)     \
  TESTQPLANAROTOPI(FUNC, STYPE, DTYPE, DEPTH, benchmark_width_ + 1, _Any, +,   \
                   0)                                                          \
  TESTQPLANAROTOPI(FUNC, STYPE, DTYPE, DEPTH, benchmark_width_, _Unaligned, +, \
                   2)                                                          \
  TESTQPLANAROTOPI(FUNC, STYPE, DTYPE, DEPTH, benchmark_width_, _Invert, -, 0) \
  TESTQPLANAROTOPI(FUNC, STYPE, DTYPE, DEPTH, benchmark_width_, _Opt, +, 0)

TESTQPLANARTOP(MergeAR64, uint16_t, uint16_t, 10)
TESTQPLANARTOP(MergeAR64, uint16_t, uint16_t, 12)
TESTQPLANARTOP(MergeAR64, uint16_t, uint16_t, 16)
TESTQPLANARTOP(MergeARGB16To8, uint16_t, uint8_t, 10)
TESTQPLANARTOP(MergeARGB16To8, uint16_t, uint8_t, 12)
TESTQPLANARTOP(MergeARGB16To8, uint16_t, uint8_t, 16)

#define TESTTPLANARTOPI(FUNC, STYPE, DTYPE, DEPTH, W1280, N, NEG, OFF)      \
  TEST_F(LibYUVPlanarTest, FUNC##Plane_##DEPTH##N) {                        \
    const int kWidth = W1280;                                               \
    const int kPixels = kWidth * benchmark_height_;                         \
    align_buffer_page_end(src_memory_r, kPixels * sizeof(STYPE) + OFF);     \
    align_buffer_page_end(src_memory_g, kPixels * sizeof(STYPE) + OFF);     \
    align_buffer_page_end(src_memory_b, kPixels * sizeof(STYPE) + OFF);     \
    align_buffer_page_end(dst_memory_c, kPixels * 4 * sizeof(DTYPE));       \
    align_buffer_page_end(dst_memory_opt, kPixels * 4 * sizeof(DTYPE));     \
    MemRandomize(src_memory_r, kPixels * sizeof(STYPE) + OFF);              \
    MemRandomize(src_memory_g, kPixels * sizeof(STYPE) + OFF);              \
    MemRandomize(src_memory_b, kPixels * sizeof(STYPE) + OFF);              \
    STYPE* src_pixels_r = reinterpret_cast<STYPE*>(src_memory_r + OFF);     \
    STYPE* src_pixels_g = reinterpret_cast<STYPE*>(src_memory_g + OFF);     \
    STYPE* src_pixels_b = reinterpret_cast<STYPE*>(src_memory_b + OFF);     \
    DTYPE* dst_pixels_c = reinterpret_cast<DTYPE*>(dst_memory_c);           \
    DTYPE* dst_pixels_opt = reinterpret_cast<DTYPE*>(dst_memory_opt);       \
    memset(dst_pixels_c, 1, kPixels * 4 * sizeof(DTYPE));                   \
    memset(dst_pixels_opt, 2, kPixels * 4 * sizeof(DTYPE));                 \
    MaskCpuFlags(disable_cpu_flags_);                                       \
    FUNC##Plane(src_pixels_r, kWidth, src_pixels_g, kWidth, src_pixels_b,   \
                kWidth, dst_pixels_c, kWidth * 4, kWidth,                   \
                NEG benchmark_height_, DEPTH);                              \
    MaskCpuFlags(benchmark_cpu_info_);                                      \
    for (int i = 0; i < benchmark_iterations_; ++i) {                       \
      FUNC##Plane(src_pixels_r, kWidth, src_pixels_g, kWidth, src_pixels_b, \
                  kWidth, dst_pixels_opt, kWidth * 4, kWidth,               \
                  NEG benchmark_height_, DEPTH);                            \
    }                                                                       \
    for (int i = 0; i < kPixels * 4; ++i) {                                 \
      EXPECT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);                        \
    }                                                                       \
    free_aligned_buffer_page_end(src_memory_r);                             \
    free_aligned_buffer_page_end(src_memory_g);                             \
    free_aligned_buffer_page_end(src_memory_b);                             \
    free_aligned_buffer_page_end(dst_memory_c);                             \
    free_aligned_buffer_page_end(dst_memory_opt);                           \
  }

#define TESTTPLANARTOP(FUNC, STYPE, DTYPE, DEPTH)                              \
  TESTTPLANARTOPI(FUNC, STYPE, DTYPE, DEPTH, benchmark_width_ + 1, _Any, +, 0) \
  TESTTPLANARTOPI(FUNC, STYPE, DTYPE, DEPTH, benchmark_width_, _Unaligned, +,  \
                  2)                                                           \
  TESTTPLANARTOPI(FUNC, STYPE, DTYPE, DEPTH, benchmark_width_, _Invert, -, 0)  \
  TESTTPLANARTOPI(FUNC, STYPE, DTYPE, DEPTH, benchmark_width_, _Opt, +, 0)

TESTTPLANARTOP(MergeXR30, uint16_t, uint8_t, 10)
TESTTPLANARTOP(MergeXR30, uint16_t, uint8_t, 12)
TESTTPLANARTOP(MergeXR30, uint16_t, uint8_t, 16)

// TODO(fbarchard): improve test for platforms and cpu detect
#ifdef HAS_MERGEUVROW_16_AVX2
TEST_F(LibYUVPlanarTest, MergeUVRow_16_Opt) {
  // Round count up to multiple of 16
  const int kPixels = (benchmark_width_ * benchmark_height_ + 15) & ~15;

  align_buffer_page_end(src_pixels_u, kPixels * 2);
  align_buffer_page_end(src_pixels_v, kPixels * 2);
  align_buffer_page_end(dst_pixels_uv_opt, kPixels * 2 * 2);
  align_buffer_page_end(dst_pixels_uv_c, kPixels * 2 * 2);

  MemRandomize(src_pixels_u, kPixels * 2);
  MemRandomize(src_pixels_v, kPixels * 2);
  memset(dst_pixels_uv_opt, 0, kPixels * 2 * 2);
  memset(dst_pixels_uv_c, 1, kPixels * 2 * 2);

  MergeUVRow_16_C(reinterpret_cast<const uint16_t*>(src_pixels_u),
                  reinterpret_cast<const uint16_t*>(src_pixels_v),
                  reinterpret_cast<uint16_t*>(dst_pixels_uv_c), 16, kPixels);

  int has_avx2 = TestCpuFlag(kCpuHasAVX2);
  for (int i = 0; i < benchmark_iterations_; ++i) {
    if (has_avx2) {
      MergeUVRow_16_AVX2(reinterpret_cast<const uint16_t*>(src_pixels_u),
                         reinterpret_cast<const uint16_t*>(src_pixels_v),
                         reinterpret_cast<uint16_t*>(dst_pixels_uv_opt), 16,
                         kPixels);
    } else {
      MergeUVRow_16_C(reinterpret_cast<const uint16_t*>(src_pixels_u),
                      reinterpret_cast<const uint16_t*>(src_pixels_v),
                      reinterpret_cast<uint16_t*>(dst_pixels_uv_opt), 16,
                      kPixels);
    }
  }

  for (int i = 0; i < kPixels * 2 * 2; ++i) {
    EXPECT_EQ(dst_pixels_uv_opt[i], dst_pixels_uv_c[i]);
  }

  free_aligned_buffer_page_end(src_pixels_u);
  free_aligned_buffer_page_end(src_pixels_v);
  free_aligned_buffer_page_end(dst_pixels_uv_opt);
  free_aligned_buffer_page_end(dst_pixels_uv_c);
}
#endif

// TODO(fbarchard): Improve test for more platforms.
#ifdef HAS_MULTIPLYROW_16_AVX2
TEST_F(LibYUVPlanarTest, MultiplyRow_16_Opt) {
  // Round count up to multiple of 32
  const int kPixels = (benchmark_width_ * benchmark_height_ + 31) & ~31;

  align_buffer_page_end(src_pixels_y, kPixels * 2);
  align_buffer_page_end(dst_pixels_y_opt, kPixels * 2);
  align_buffer_page_end(dst_pixels_y_c, kPixels * 2);

  MemRandomize(src_pixels_y, kPixels * 2);
  memset(dst_pixels_y_opt, 0, kPixels * 2);
  memset(dst_pixels_y_c, 1, kPixels * 2);

  MultiplyRow_16_C(reinterpret_cast<const uint16_t*>(src_pixels_y),
                   reinterpret_cast<uint16_t*>(dst_pixels_y_c), 64, kPixels);

  int has_avx2 = TestCpuFlag(kCpuHasAVX2);
  for (int i = 0; i < benchmark_iterations_; ++i) {
    if (has_avx2) {
      MultiplyRow_16_AVX2(reinterpret_cast<const uint16_t*>(src_pixels_y),
                          reinterpret_cast<uint16_t*>(dst_pixels_y_opt), 64,
                          kPixels);
    } else {
      MultiplyRow_16_C(reinterpret_cast<const uint16_t*>(src_pixels_y),
                       reinterpret_cast<uint16_t*>(dst_pixels_y_opt), 64,
                       kPixels);
    }
  }

  for (int i = 0; i < kPixels * 2; ++i) {
    EXPECT_EQ(dst_pixels_y_opt[i], dst_pixels_y_c[i]);
  }

  free_aligned_buffer_page_end(src_pixels_y);
  free_aligned_buffer_page_end(dst_pixels_y_opt);
  free_aligned_buffer_page_end(dst_pixels_y_c);
}
#endif  // HAS_MULTIPLYROW_16_AVX2

TEST_F(LibYUVPlanarTest, Convert16To8Plane) {
  const int kPixels = benchmark_width_ * benchmark_height_;
  align_buffer_page_end(src_pixels_y, kPixels * 2);
  align_buffer_page_end(dst_pixels_y_opt, kPixels);
  align_buffer_page_end(dst_pixels_y_c, kPixels);

  MemRandomize(src_pixels_y, kPixels * 2);
  memset(dst_pixels_y_opt, 0, kPixels);
  memset(dst_pixels_y_c, 1, kPixels);

  MaskCpuFlags(disable_cpu_flags_);
  Convert16To8Plane(reinterpret_cast<const uint16_t*>(src_pixels_y),
                    benchmark_width_, dst_pixels_y_c, benchmark_width_, 16384,
                    benchmark_width_, benchmark_height_);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    Convert16To8Plane(reinterpret_cast<const uint16_t*>(src_pixels_y),
                      benchmark_width_, dst_pixels_y_opt, benchmark_width_,
                      16384, benchmark_width_, benchmark_height_);
  }

  for (int i = 0; i < kPixels; ++i) {
    EXPECT_EQ(dst_pixels_y_opt[i], dst_pixels_y_c[i]);
  }

  free_aligned_buffer_page_end(src_pixels_y);
  free_aligned_buffer_page_end(dst_pixels_y_opt);
  free_aligned_buffer_page_end(dst_pixels_y_c);
}

TEST_F(LibYUVPlanarTest, YUY2ToY) {
  const int kPixels = benchmark_width_ * benchmark_height_;
  align_buffer_page_end(src_pixels_y, kPixels * 2);
  align_buffer_page_end(dst_pixels_y_opt, kPixels);
  align_buffer_page_end(dst_pixels_y_c, kPixels);

  MemRandomize(src_pixels_y, kPixels * 2);
  memset(dst_pixels_y_opt, 0, kPixels);
  memset(dst_pixels_y_c, 1, kPixels);

  MaskCpuFlags(disable_cpu_flags_);
  YUY2ToY(src_pixels_y, benchmark_width_ * 2, dst_pixels_y_c, benchmark_width_,
          benchmark_width_, benchmark_height_);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    YUY2ToY(src_pixels_y, benchmark_width_ * 2, dst_pixels_y_opt,
            benchmark_width_, benchmark_width_, benchmark_height_);
  }

  for (int i = 0; i < kPixels; ++i) {
    EXPECT_EQ(dst_pixels_y_opt[i], dst_pixels_y_c[i]);
  }

  free_aligned_buffer_page_end(src_pixels_y);
  free_aligned_buffer_page_end(dst_pixels_y_opt);
  free_aligned_buffer_page_end(dst_pixels_y_c);
}

TEST_F(LibYUVPlanarTest, UYVYToY) {
  const int kPixels = benchmark_width_ * benchmark_height_;
  align_buffer_page_end(src_pixels_y, kPixels * 2);
  align_buffer_page_end(dst_pixels_y_opt, kPixels);
  align_buffer_page_end(dst_pixels_y_c, kPixels);

  MemRandomize(src_pixels_y, kPixels * 2);
  memset(dst_pixels_y_opt, 0, kPixels);
  memset(dst_pixels_y_c, 1, kPixels);

  MaskCpuFlags(disable_cpu_flags_);
  UYVYToY(src_pixels_y, benchmark_width_ * 2, dst_pixels_y_c, benchmark_width_,
          benchmark_width_, benchmark_height_);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    UYVYToY(src_pixels_y, benchmark_width_ * 2, dst_pixels_y_opt,
            benchmark_width_, benchmark_width_, benchmark_height_);
  }

  for (int i = 0; i < kPixels; ++i) {
    EXPECT_EQ(dst_pixels_y_opt[i], dst_pixels_y_c[i]);
  }

  free_aligned_buffer_page_end(src_pixels_y);
  free_aligned_buffer_page_end(dst_pixels_y_opt);
  free_aligned_buffer_page_end(dst_pixels_y_c);
}

#ifdef ENABLE_ROW_TESTS
// TODO(fbarchard): Improve test for more platforms.
#ifdef HAS_CONVERT16TO8ROW_AVX2
TEST_F(LibYUVPlanarTest, Convert16To8Row_Opt) {
  // AVX2 does multiple of 32, so round count up
  const int kPixels = (benchmark_width_ * benchmark_height_ + 31) & ~31;
  align_buffer_page_end(src_pixels_y, kPixels * 2);
  align_buffer_page_end(dst_pixels_y_opt, kPixels);
  align_buffer_page_end(dst_pixels_y_c, kPixels);

  MemRandomize(src_pixels_y, kPixels * 2);
  // clamp source range to 10 bits.
  for (int i = 0; i < kPixels; ++i) {
    reinterpret_cast<uint16_t*>(src_pixels_y)[i] &= 1023;
  }

  memset(dst_pixels_y_opt, 0, kPixels);
  memset(dst_pixels_y_c, 1, kPixels);

  Convert16To8Row_C(reinterpret_cast<const uint16_t*>(src_pixels_y),
                    dst_pixels_y_c, 16384, kPixels);

  int has_avx2 = TestCpuFlag(kCpuHasAVX2);
  int has_ssse3 = TestCpuFlag(kCpuHasSSSE3);
  for (int i = 0; i < benchmark_iterations_; ++i) {
    if (has_avx2) {
      Convert16To8Row_AVX2(reinterpret_cast<const uint16_t*>(src_pixels_y),
                           dst_pixels_y_opt, 16384, kPixels);
    } else if (has_ssse3) {
      Convert16To8Row_SSSE3(reinterpret_cast<const uint16_t*>(src_pixels_y),
                            dst_pixels_y_opt, 16384, kPixels);
    } else {
      Convert16To8Row_C(reinterpret_cast<const uint16_t*>(src_pixels_y),
                        dst_pixels_y_opt, 16384, kPixels);
    }
  }

  for (int i = 0; i < kPixels; ++i) {
    EXPECT_EQ(dst_pixels_y_opt[i], dst_pixels_y_c[i]);
  }

  free_aligned_buffer_page_end(src_pixels_y);
  free_aligned_buffer_page_end(dst_pixels_y_opt);
  free_aligned_buffer_page_end(dst_pixels_y_c);
}
#endif  // HAS_CONVERT16TO8ROW_AVX2

#ifdef HAS_UYVYTOYROW_NEON
TEST_F(LibYUVPlanarTest, UYVYToYRow_Opt) {
  // NEON does multiple of 16, so round count up
  const int kPixels = (benchmark_width_ * benchmark_height_ + 15) & ~15;
  align_buffer_page_end(src_pixels_y, kPixels * 2);
  align_buffer_page_end(dst_pixels_y_opt, kPixels);
  align_buffer_page_end(dst_pixels_y_c, kPixels);

  MemRandomize(src_pixels_y, kPixels * 2);
  memset(dst_pixels_y_opt, 0, kPixels);
  memset(dst_pixels_y_c, 1, kPixels);

  UYVYToYRow_C(src_pixels_y, dst_pixels_y_c, kPixels);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    UYVYToYRow_NEON(src_pixels_y, dst_pixels_y_opt, kPixels);
  }

  for (int i = 0; i < kPixels; ++i) {
    EXPECT_EQ(dst_pixels_y_opt[i], dst_pixels_y_c[i]);
  }

  free_aligned_buffer_page_end(src_pixels_y);
  free_aligned_buffer_page_end(dst_pixels_y_opt);
  free_aligned_buffer_page_end(dst_pixels_y_c);
}
#endif  // HAS_UYVYTOYROW_NEON

#endif  // ENABLE_ROW_TESTS

TEST_F(LibYUVPlanarTest, Convert8To16Plane) {
  const int kPixels = benchmark_width_ * benchmark_height_;
  align_buffer_page_end(src_pixels_y, kPixels);
  align_buffer_page_end(dst_pixels_y_opt, kPixels * 2);
  align_buffer_page_end(dst_pixels_y_c, kPixels * 2);

  MemRandomize(src_pixels_y, kPixels);
  memset(dst_pixels_y_opt, 0, kPixels * 2);
  memset(dst_pixels_y_c, 1, kPixels * 2);

  MaskCpuFlags(disable_cpu_flags_);
  Convert8To16Plane(src_pixels_y, benchmark_width_,
                    reinterpret_cast<uint16_t*>(dst_pixels_y_c),
                    benchmark_width_, 1024, benchmark_width_,
                    benchmark_height_);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    Convert8To16Plane(src_pixels_y, benchmark_width_,
                      reinterpret_cast<uint16_t*>(dst_pixels_y_opt),
                      benchmark_width_, 1024, benchmark_width_,
                      benchmark_height_);
  }

  for (int i = 0; i < kPixels * 2; ++i) {
    EXPECT_EQ(dst_pixels_y_opt[i], dst_pixels_y_c[i]);
  }

  free_aligned_buffer_page_end(src_pixels_y);
  free_aligned_buffer_page_end(dst_pixels_y_opt);
  free_aligned_buffer_page_end(dst_pixels_y_c);
}

#ifdef ENABLE_ROW_TESTS
// TODO(fbarchard): Improve test for more platforms.
#ifdef HAS_CONVERT8TO16ROW_AVX2
TEST_F(LibYUVPlanarTest, Convert8To16Row_Opt) {
  const int kPixels = (benchmark_width_ * benchmark_height_ + 31) & ~31;
  align_buffer_page_end(src_pixels_y, kPixels);
  align_buffer_page_end(dst_pixels_y_opt, kPixels * 2);
  align_buffer_page_end(dst_pixels_y_c, kPixels * 2);

  MemRandomize(src_pixels_y, kPixels);
  memset(dst_pixels_y_opt, 0, kPixels * 2);
  memset(dst_pixels_y_c, 1, kPixels * 2);

  Convert8To16Row_C(src_pixels_y, reinterpret_cast<uint16_t*>(dst_pixels_y_c),
                    1024, kPixels);

  int has_avx2 = TestCpuFlag(kCpuHasAVX2);
  int has_sse2 = TestCpuFlag(kCpuHasSSE2);
  for (int i = 0; i < benchmark_iterations_; ++i) {
    if (has_avx2) {
      Convert8To16Row_AVX2(src_pixels_y,
                           reinterpret_cast<uint16_t*>(dst_pixels_y_opt), 1024,
                           kPixels);
    } else if (has_sse2) {
      Convert8To16Row_SSE2(src_pixels_y,
                           reinterpret_cast<uint16_t*>(dst_pixels_y_opt), 1024,
                           kPixels);
    } else {
      Convert8To16Row_C(src_pixels_y,
                        reinterpret_cast<uint16_t*>(dst_pixels_y_opt), 1024,
                        kPixels);
    }
  }

  for (int i = 0; i < kPixels * 2; ++i) {
    EXPECT_EQ(dst_pixels_y_opt[i], dst_pixels_y_c[i]);
  }

  free_aligned_buffer_page_end(src_pixels_y);
  free_aligned_buffer_page_end(dst_pixels_y_opt);
  free_aligned_buffer_page_end(dst_pixels_y_c);
}
#endif  // HAS_CONVERT8TO16ROW_AVX2

float TestScaleMaxSamples(int benchmark_width,
                          int benchmark_height,
                          int benchmark_iterations,
                          float scale,
                          bool opt) {
  int i, j;
  float max_c, max_opt = 0.f;
  // NEON does multiple of 8, so round count up
  const int kPixels = (benchmark_width * benchmark_height + 7) & ~7;
  align_buffer_page_end(orig_y, kPixels * 4 * 3 + 48);
  uint8_t* dst_c = orig_y + kPixels * 4 + 16;
  uint8_t* dst_opt = orig_y + kPixels * 4 * 2 + 32;

  // Randomize works but may contain some denormals affecting performance.
  // MemRandomize(orig_y, kPixels * 4);
  // large values are problematic.  audio is really -1 to 1.
  for (i = 0; i < kPixels; ++i) {
    (reinterpret_cast<float*>(orig_y))[i] = sinf(static_cast<float>(i) * 0.1f);
  }
  memset(dst_c, 0, kPixels * 4);
  memset(dst_opt, 1, kPixels * 4);

  max_c = ScaleMaxSamples_C(reinterpret_cast<float*>(orig_y),
                            reinterpret_cast<float*>(dst_c), scale, kPixels);

  for (j = 0; j < benchmark_iterations; j++) {
    if (opt) {
#ifdef HAS_SCALESUMSAMPLES_NEON
      max_opt = ScaleMaxSamples_NEON(reinterpret_cast<float*>(orig_y),
                                     reinterpret_cast<float*>(dst_opt), scale,
                                     kPixels);
#else
      max_opt =
          ScaleMaxSamples_C(reinterpret_cast<float*>(orig_y),
                            reinterpret_cast<float*>(dst_opt), scale, kPixels);
#endif
    } else {
      max_opt =
          ScaleMaxSamples_C(reinterpret_cast<float*>(orig_y),
                            reinterpret_cast<float*>(dst_opt), scale, kPixels);
    }
  }

  float max_diff = FAbs(max_opt - max_c);
  for (i = 0; i < kPixels; ++i) {
    float abs_diff = FAbs((reinterpret_cast<float*>(dst_c)[i]) -
                          (reinterpret_cast<float*>(dst_opt)[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }

  free_aligned_buffer_page_end(orig_y);
  return max_diff;
}

TEST_F(LibYUVPlanarTest, TestScaleMaxSamples_C) {
  float diff = TestScaleMaxSamples(benchmark_width_, benchmark_height_,
                                   benchmark_iterations_, 1.2f, false);
  EXPECT_EQ(0, diff);
}

TEST_F(LibYUVPlanarTest, TestScaleMaxSamples_Opt) {
  float diff = TestScaleMaxSamples(benchmark_width_, benchmark_height_,
                                   benchmark_iterations_, 1.2f, true);
  EXPECT_EQ(0, diff);
}

float TestScaleSumSamples(int benchmark_width,
                          int benchmark_height,
                          int benchmark_iterations,
                          float scale,
                          bool opt) {
  int i, j;
  float sum_c, sum_opt = 0.f;
  // NEON does multiple of 8, so round count up
  const int kPixels = (benchmark_width * benchmark_height + 7) & ~7;
  align_buffer_page_end(orig_y, kPixels * 4 * 3);
  uint8_t* dst_c = orig_y + kPixels * 4;
  uint8_t* dst_opt = orig_y + kPixels * 4 * 2;

  // Randomize works but may contain some denormals affecting performance.
  // MemRandomize(orig_y, kPixels * 4);
  // large values are problematic.  audio is really -1 to 1.
  for (i = 0; i < kPixels; ++i) {
    (reinterpret_cast<float*>(orig_y))[i] = sinf(static_cast<float>(i) * 0.1f);
  }
  memset(dst_c, 0, kPixels * 4);
  memset(dst_opt, 1, kPixels * 4);

  sum_c = ScaleSumSamples_C(reinterpret_cast<float*>(orig_y),
                            reinterpret_cast<float*>(dst_c), scale, kPixels);

  for (j = 0; j < benchmark_iterations; j++) {
    if (opt) {
#ifdef HAS_SCALESUMSAMPLES_NEON
      sum_opt = ScaleSumSamples_NEON(reinterpret_cast<float*>(orig_y),
                                     reinterpret_cast<float*>(dst_opt), scale,
                                     kPixels);
#else
      sum_opt =
          ScaleSumSamples_C(reinterpret_cast<float*>(orig_y),
                            reinterpret_cast<float*>(dst_opt), scale, kPixels);
#endif
    } else {
      sum_opt =
          ScaleSumSamples_C(reinterpret_cast<float*>(orig_y),
                            reinterpret_cast<float*>(dst_opt), scale, kPixels);
    }
  }

  float mse_opt = sum_opt / kPixels * 4;
  float mse_c = sum_c / kPixels * 4;
  float mse_error = FAbs(mse_opt - mse_c) / mse_c;

  // If the sum of a float is more than 4 million, small adds are round down on
  // float and produce different results with vectorized sum vs scalar sum.
  // Ignore the difference if the sum is large.
  float max_diff = 0.f;
  if (mse_error > 0.0001 && sum_c < 4000000) {  // allow .01% difference of mse
    max_diff = mse_error;
  }

  for (i = 0; i < kPixels; ++i) {
    float abs_diff = FAbs((reinterpret_cast<float*>(dst_c)[i]) -
                          (reinterpret_cast<float*>(dst_opt)[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }

  free_aligned_buffer_page_end(orig_y);
  return max_diff;
}

TEST_F(LibYUVPlanarTest, TestScaleSumSamples_C) {
  float diff = TestScaleSumSamples(benchmark_width_, benchmark_height_,
                                   benchmark_iterations_, 1.2f, false);
  EXPECT_EQ(0, diff);
}

TEST_F(LibYUVPlanarTest, TestScaleSumSamples_Opt) {
  float diff = TestScaleSumSamples(benchmark_width_, benchmark_height_,
                                   benchmark_iterations_, 1.2f, true);
  EXPECT_EQ(0, diff);
}

float TestScaleSamples(int benchmark_width,
                       int benchmark_height,
                       int benchmark_iterations,
                       float scale,
                       bool opt) {
  int i, j;
  // NEON does multiple of 8, so round count up
  const int kPixels = (benchmark_width * benchmark_height + 7) & ~7;
  align_buffer_page_end(orig_y, kPixels * 4 * 3);
  uint8_t* dst_c = orig_y + kPixels * 4;
  uint8_t* dst_opt = orig_y + kPixels * 4 * 2;

  // Randomize works but may contain some denormals affecting performance.
  // MemRandomize(orig_y, kPixels * 4);
  // large values are problematic.  audio is really -1 to 1.
  for (i = 0; i < kPixels; ++i) {
    (reinterpret_cast<float*>(orig_y))[i] = sinf(static_cast<float>(i) * 0.1f);
  }
  memset(dst_c, 0, kPixels * 4);
  memset(dst_opt, 1, kPixels * 4);

  ScaleSamples_C(reinterpret_cast<float*>(orig_y),
                 reinterpret_cast<float*>(dst_c), scale, kPixels);

  for (j = 0; j < benchmark_iterations; j++) {
    if (opt) {
#ifdef HAS_SCALESUMSAMPLES_NEON
      ScaleSamples_NEON(reinterpret_cast<float*>(orig_y),
                        reinterpret_cast<float*>(dst_opt), scale, kPixels);
#else
      ScaleSamples_C(reinterpret_cast<float*>(orig_y),
                     reinterpret_cast<float*>(dst_opt), scale, kPixels);
#endif
    } else {
      ScaleSamples_C(reinterpret_cast<float*>(orig_y),
                     reinterpret_cast<float*>(dst_opt), scale, kPixels);
    }
  }

  float max_diff = 0.f;
  for (i = 0; i < kPixels; ++i) {
    float abs_diff = FAbs((reinterpret_cast<float*>(dst_c)[i]) -
                          (reinterpret_cast<float*>(dst_opt)[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }

  free_aligned_buffer_page_end(orig_y);
  return max_diff;
}

TEST_F(LibYUVPlanarTest, TestScaleSamples_C) {
  float diff = TestScaleSamples(benchmark_width_, benchmark_height_,
                                benchmark_iterations_, 1.2f, false);
  EXPECT_EQ(0, diff);
}

TEST_F(LibYUVPlanarTest, TestScaleSamples_Opt) {
  float diff = TestScaleSamples(benchmark_width_, benchmark_height_,
                                benchmark_iterations_, 1.2f, true);
  EXPECT_EQ(0, diff);
}

float TestCopySamples(int benchmark_width,
                      int benchmark_height,
                      int benchmark_iterations,
                      bool opt) {
  int i, j;
  // NEON does multiple of 16 floats, so round count up
  const int kPixels = (benchmark_width * benchmark_height + 15) & ~15;
  align_buffer_page_end(orig_y, kPixels * 4 * 3);
  uint8_t* dst_c = orig_y + kPixels * 4;
  uint8_t* dst_opt = orig_y + kPixels * 4 * 2;

  // Randomize works but may contain some denormals affecting performance.
  // MemRandomize(orig_y, kPixels * 4);
  // large values are problematic.  audio is really -1 to 1.
  for (i = 0; i < kPixels; ++i) {
    (reinterpret_cast<float*>(orig_y))[i] = sinf(static_cast<float>(i) * 0.1f);
  }
  memset(dst_c, 0, kPixels * 4);
  memset(dst_opt, 1, kPixels * 4);

  memcpy(reinterpret_cast<void*>(dst_c), reinterpret_cast<void*>(orig_y),
         kPixels * 4);

  for (j = 0; j < benchmark_iterations; j++) {
    if (opt) {
#ifdef HAS_COPYROW_NEON
      CopyRow_NEON(orig_y, dst_opt, kPixels * 4);
#else
      CopyRow_C(orig_y, dst_opt, kPixels * 4);
#endif
    } else {
      CopyRow_C(orig_y, dst_opt, kPixels * 4);
    }
  }

  float max_diff = 0.f;
  for (i = 0; i < kPixels; ++i) {
    float abs_diff = FAbs((reinterpret_cast<float*>(dst_c)[i]) -
                          (reinterpret_cast<float*>(dst_opt)[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }

  free_aligned_buffer_page_end(orig_y);
  return max_diff;
}

TEST_F(LibYUVPlanarTest, TestCopySamples_C) {
  float diff = TestCopySamples(benchmark_width_, benchmark_height_,
                               benchmark_iterations_, false);
  EXPECT_EQ(0, diff);
}

TEST_F(LibYUVPlanarTest, TestCopySamples_Opt) {
  float diff = TestCopySamples(benchmark_width_, benchmark_height_,
                               benchmark_iterations_, true);
  EXPECT_EQ(0, diff);
}

extern "C" void GaussRow_NEON(const uint32_t* src, uint16_t* dst, int width);
extern "C" void GaussRow_C(const uint32_t* src, uint16_t* dst, int width);

TEST_F(LibYUVPlanarTest, TestGaussRow_Opt) {
  SIMD_ALIGNED(uint32_t orig_pixels[1280 + 8]);
  SIMD_ALIGNED(uint16_t dst_pixels_c[1280]);
  SIMD_ALIGNED(uint16_t dst_pixels_opt[1280]);

  memset(orig_pixels, 0, sizeof(orig_pixels));
  memset(dst_pixels_c, 1, sizeof(dst_pixels_c));
  memset(dst_pixels_opt, 2, sizeof(dst_pixels_opt));

  for (int i = 0; i < 1280 + 8; ++i) {
    orig_pixels[i] = i * 256;
  }
  GaussRow_C(&orig_pixels[0], &dst_pixels_c[0], 1280);
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
#if !defined(LIBYUV_DISABLE_NEON) && \
    (defined(__aarch64__) || defined(__ARM_NEON__) || defined(LIBYUV_NEON))
    int has_neon = TestCpuFlag(kCpuHasNEON);
    if (has_neon) {
      GaussRow_NEON(&orig_pixels[0], &dst_pixels_opt[0], 1280);
    } else {
      GaussRow_C(&orig_pixels[0], &dst_pixels_opt[0], 1280);
    }
#else
    GaussRow_C(&orig_pixels[0], &dst_pixels_opt[0], 1280);
#endif
  }

  for (int i = 0; i < 1280; ++i) {
    EXPECT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
  }

  EXPECT_EQ(dst_pixels_c[0],
            static_cast<uint16_t>(0 * 1 + 1 * 4 + 2 * 6 + 3 * 4 + 4 * 1));
  EXPECT_EQ(dst_pixels_c[639], static_cast<uint16_t>(10256));
}

extern "C" void GaussCol_NEON(const uint16_t* src0,
                              const uint16_t* src1,
                              const uint16_t* src2,
                              const uint16_t* src3,
                              const uint16_t* src4,
                              uint32_t* dst,
                              int width);

extern "C" void GaussCol_C(const uint16_t* src0,
                           const uint16_t* src1,
                           const uint16_t* src2,
                           const uint16_t* src3,
                           const uint16_t* src4,
                           uint32_t* dst,
                           int width);

TEST_F(LibYUVPlanarTest, TestGaussCol_Opt) {
  SIMD_ALIGNED(uint16_t orig_pixels[1280 * 5]);
  SIMD_ALIGNED(uint32_t dst_pixels_c[1280]);
  SIMD_ALIGNED(uint32_t dst_pixels_opt[1280]);

  memset(orig_pixels, 0, sizeof(orig_pixels));
  memset(dst_pixels_c, 1, sizeof(dst_pixels_c));
  memset(dst_pixels_opt, 2, sizeof(dst_pixels_opt));

  for (int i = 0; i < 1280 * 5; ++i) {
    orig_pixels[i] = static_cast<float>(i);
  }
  GaussCol_C(&orig_pixels[0], &orig_pixels[1280], &orig_pixels[1280 * 2],
             &orig_pixels[1280 * 3], &orig_pixels[1280 * 4], &dst_pixels_c[0],
             1280);
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
#if !defined(LIBYUV_DISABLE_NEON) && \
    (defined(__aarch64__) || defined(__ARM_NEON__) || defined(LIBYUV_NEON))
    int has_neon = TestCpuFlag(kCpuHasNEON);
    if (has_neon) {
      GaussCol_NEON(&orig_pixels[0], &orig_pixels[1280], &orig_pixels[1280 * 2],
                    &orig_pixels[1280 * 3], &orig_pixels[1280 * 4],
                    &dst_pixels_opt[0], 1280);
    } else {
      GaussCol_C(&orig_pixels[0], &orig_pixels[1280], &orig_pixels[1280 * 2],
                 &orig_pixels[1280 * 3], &orig_pixels[1280 * 4],
                 &dst_pixels_opt[0], 1280);
    }
#else
    GaussCol_C(&orig_pixels[0], &orig_pixels[1280], &orig_pixels[1280 * 2],
               &orig_pixels[1280 * 3], &orig_pixels[1280 * 4],
               &dst_pixels_opt[0], 1280);
#endif
  }

  for (int i = 0; i < 1280; ++i) {
    EXPECT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
  }
}

TEST_F(LibYUVPlanarTest, TestGaussRow_F32_Opt) {
  SIMD_ALIGNED(float orig_pixels[1280 + 4]);
  SIMD_ALIGNED(float dst_pixels_c[1280]);
  SIMD_ALIGNED(float dst_pixels_opt[1280]);

  memset(orig_pixels, 0, sizeof(orig_pixels));
  memset(dst_pixels_c, 1, sizeof(dst_pixels_c));
  memset(dst_pixels_opt, 2, sizeof(dst_pixels_opt));

  for (int i = 0; i < 1280 + 4; ++i) {
    orig_pixels[i] = static_cast<float>(i);
  }
  GaussRow_F32_C(&orig_pixels[0], &dst_pixels_c[0], 1280);
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
#if !defined(LIBYUV_DISABLE_NEON) && defined(__aarch64__)
    int has_neon = TestCpuFlag(kCpuHasNEON);
    if (has_neon) {
      GaussRow_F32_NEON(&orig_pixels[0], &dst_pixels_opt[0], 1280);
    } else {
      GaussRow_F32_C(&orig_pixels[0], &dst_pixels_opt[0], 1280);
    }
#else
    GaussRow_F32_C(&orig_pixels[0], &dst_pixels_opt[0], 1280);
#endif
  }

  for (int i = 0; i < 1280; ++i) {
    EXPECT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
  }
}

TEST_F(LibYUVPlanarTest, TestGaussCol_F32_Opt) {
  SIMD_ALIGNED(float dst_pixels_c[1280]);
  SIMD_ALIGNED(float dst_pixels_opt[1280]);
  align_buffer_page_end(orig_pixels_buf, 1280 * 5 * 4);  // 5 rows
  float* orig_pixels = reinterpret_cast<float*>(orig_pixels_buf);

  memset(orig_pixels, 0, 1280 * 5 * 4);
  memset(dst_pixels_c, 1, sizeof(dst_pixels_c));
  memset(dst_pixels_opt, 2, sizeof(dst_pixels_opt));

  for (int i = 0; i < 1280 * 5; ++i) {
    orig_pixels[i] = static_cast<float>(i);
  }
  GaussCol_F32_C(&orig_pixels[0], &orig_pixels[1280], &orig_pixels[1280 * 2],
                 &orig_pixels[1280 * 3], &orig_pixels[1280 * 4],
                 &dst_pixels_c[0], 1280);
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
#if !defined(LIBYUV_DISABLE_NEON) && defined(__aarch64__)
    int has_neon = TestCpuFlag(kCpuHasNEON);
    if (has_neon) {
      GaussCol_F32_NEON(&orig_pixels[0], &orig_pixels[1280],
                        &orig_pixels[1280 * 2], &orig_pixels[1280 * 3],
                        &orig_pixels[1280 * 4], &dst_pixels_opt[0], 1280);
    } else {
      GaussCol_F32_C(&orig_pixels[0], &orig_pixels[1280],
                     &orig_pixels[1280 * 2], &orig_pixels[1280 * 3],
                     &orig_pixels[1280 * 4], &dst_pixels_opt[0], 1280);
    }
#else
    GaussCol_F32_C(&orig_pixels[0], &orig_pixels[1280], &orig_pixels[1280 * 2],
                   &orig_pixels[1280 * 3], &orig_pixels[1280 * 4],
                   &dst_pixels_opt[0], 1280);
#endif
  }

  for (int i = 0; i < 1280; ++i) {
    EXPECT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
  }
  free_aligned_buffer_page_end(orig_pixels_buf);
}

TEST_F(LibYUVPlanarTest, SwapUVRow) {
  const int kPixels = benchmark_width_ * benchmark_height_;
  void (*SwapUVRow)(const uint8_t* src_uv, uint8_t* dst_vu, int width) =
      SwapUVRow_C;

  align_buffer_page_end(src_pixels_vu, kPixels * 2);
  align_buffer_page_end(dst_pixels_uv, kPixels * 2);
  MemRandomize(src_pixels_vu, kPixels * 2);
  memset(dst_pixels_uv, 1, kPixels * 2);

#if defined(HAS_SWAPUVROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    SwapUVRow = SwapUVRow_Any_NEON;
    if (IS_ALIGNED(kPixels, 16)) {
      SwapUVRow = SwapUVRow_NEON;
    }
  }
#endif

  for (int j = 0; j < benchmark_iterations_; j++) {
    SwapUVRow(src_pixels_vu, dst_pixels_uv, kPixels);
  }
  for (int i = 0; i < kPixels; ++i) {
    EXPECT_EQ(dst_pixels_uv[i * 2 + 0], src_pixels_vu[i * 2 + 1]);
    EXPECT_EQ(dst_pixels_uv[i * 2 + 1], src_pixels_vu[i * 2 + 0]);
  }

  free_aligned_buffer_page_end(src_pixels_vu);
  free_aligned_buffer_page_end(dst_pixels_uv);
}
#endif  // ENABLE_ROW_TESTS

TEST_F(LibYUVPlanarTest, TestGaussPlane_F32) {
  const int kSize = benchmark_width_ * benchmark_height_ * 4;
  align_buffer_page_end(orig_pixels, kSize);
  align_buffer_page_end(dst_pixels_opt, kSize);
  align_buffer_page_end(dst_pixels_c, kSize);

  for (int i = 0; i < benchmark_width_ * benchmark_height_; ++i) {
    ((float*)(orig_pixels))[i] = (i & 1023) * 3.14f;
  }
  memset(dst_pixels_opt, 1, kSize);
  memset(dst_pixels_c, 2, kSize);

  MaskCpuFlags(disable_cpu_flags_);
  GaussPlane_F32((const float*)(orig_pixels), benchmark_width_,
                 (float*)(dst_pixels_c), benchmark_width_, benchmark_width_,
                 benchmark_height_);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    GaussPlane_F32((const float*)(orig_pixels), benchmark_width_,
                   (float*)(dst_pixels_opt), benchmark_width_, benchmark_width_,
                   benchmark_height_);
  }
  for (int i = 0; i < benchmark_width_ * benchmark_height_; ++i) {
    EXPECT_NEAR(((float*)(dst_pixels_c))[i], ((float*)(dst_pixels_opt))[i], 1.f)
        << i;
  }

  free_aligned_buffer_page_end(dst_pixels_c);
  free_aligned_buffer_page_end(dst_pixels_opt);
  free_aligned_buffer_page_end(orig_pixels);
}

TEST_F(LibYUVPlanarTest, HalfMergeUVPlane_Opt) {
  int dst_width = (benchmark_width_ + 1) / 2;
  int dst_height = (benchmark_height_ + 1) / 2;
  align_buffer_page_end(src_pixels_u, benchmark_width_ * benchmark_height_);
  align_buffer_page_end(src_pixels_v, benchmark_width_ * benchmark_height_);
  align_buffer_page_end(tmp_pixels_u, dst_width * dst_height);
  align_buffer_page_end(tmp_pixels_v, dst_width * dst_height);
  align_buffer_page_end(dst_pixels_uv_opt, dst_width * 2 * dst_height);
  align_buffer_page_end(dst_pixels_uv_c, dst_width * 2 * dst_height);

  MemRandomize(src_pixels_u, benchmark_width_ * benchmark_height_);
  MemRandomize(src_pixels_v, benchmark_width_ * benchmark_height_);
  MemRandomize(tmp_pixels_u, dst_width * dst_height);
  MemRandomize(tmp_pixels_v, dst_width * dst_height);
  MemRandomize(dst_pixels_uv_opt, dst_width * 2 * dst_height);
  MemRandomize(dst_pixels_uv_c, dst_width * 2 * dst_height);

  MaskCpuFlags(disable_cpu_flags_);
  HalfMergeUVPlane(src_pixels_u, benchmark_width_, src_pixels_v,
                   benchmark_width_, dst_pixels_uv_c, dst_width * 2,
                   benchmark_width_, benchmark_height_);
  MaskCpuFlags(benchmark_cpu_info_);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    HalfMergeUVPlane(src_pixels_u, benchmark_width_, src_pixels_v,
                     benchmark_width_, dst_pixels_uv_opt, dst_width * 2,
                     benchmark_width_, benchmark_height_);
  }

  for (int i = 0; i < dst_width * 2 * dst_height; ++i) {
    EXPECT_EQ(dst_pixels_uv_c[i], dst_pixels_uv_opt[i]);
  }

  free_aligned_buffer_page_end(src_pixels_u);
  free_aligned_buffer_page_end(src_pixels_v);
  free_aligned_buffer_page_end(tmp_pixels_u);
  free_aligned_buffer_page_end(tmp_pixels_v);
  free_aligned_buffer_page_end(dst_pixels_uv_opt);
  free_aligned_buffer_page_end(dst_pixels_uv_c);
}

TEST_F(LibYUVPlanarTest, NV12Copy) {
  const int halfwidth = (benchmark_width_ + 1) >> 1;
  const int halfheight = (benchmark_height_ + 1) >> 1;
  align_buffer_page_end(src_y, benchmark_width_ * benchmark_height_);
  align_buffer_page_end(src_uv, halfwidth * 2 * halfheight);
  align_buffer_page_end(dst_y, benchmark_width_ * benchmark_height_);
  align_buffer_page_end(dst_uv, halfwidth * 2 * halfheight);

  MemRandomize(src_y, benchmark_width_ * benchmark_height_);
  MemRandomize(src_uv, halfwidth * 2 * halfheight);
  MemRandomize(dst_y, benchmark_width_ * benchmark_height_);
  MemRandomize(dst_uv, halfwidth * 2 * halfheight);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    NV12Copy(src_y, benchmark_width_, src_uv, halfwidth * 2, dst_y,
             benchmark_width_, dst_uv, halfwidth * 2, benchmark_width_,
             benchmark_height_);
  }

  for (int i = 0; i < benchmark_width_ * benchmark_height_; ++i) {
    EXPECT_EQ(src_y[i], dst_y[i]);
  }
  for (int i = 0; i < halfwidth * 2 * halfheight; ++i) {
    EXPECT_EQ(src_uv[i], dst_uv[i]);
  }

  free_aligned_buffer_page_end(src_y);
  free_aligned_buffer_page_end(src_uv);
  free_aligned_buffer_page_end(dst_y);
  free_aligned_buffer_page_end(dst_uv);
}

TEST_F(LibYUVPlanarTest, NV21Copy) {
  const int halfwidth = (benchmark_width_ + 1) >> 1;
  const int halfheight = (benchmark_height_ + 1) >> 1;
  align_buffer_page_end(src_y, benchmark_width_ * benchmark_height_);
  align_buffer_page_end(src_vu, halfwidth * 2 * halfheight);
  align_buffer_page_end(dst_y, benchmark_width_ * benchmark_height_);
  align_buffer_page_end(dst_vu, halfwidth * 2 * halfheight);

  MemRandomize(src_y, benchmark_width_ * benchmark_height_);
  MemRandomize(src_vu, halfwidth * 2 * halfheight);
  MemRandomize(dst_y, benchmark_width_ * benchmark_height_);
  MemRandomize(dst_vu, halfwidth * 2 * halfheight);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    NV21Copy(src_y, benchmark_width_, src_vu, halfwidth * 2, dst_y,
             benchmark_width_, dst_vu, halfwidth * 2, benchmark_width_,
             benchmark_height_);
  }

  for (int i = 0; i < benchmark_width_ * benchmark_height_; ++i) {
    EXPECT_EQ(src_y[i], dst_y[i]);
  }
  for (int i = 0; i < halfwidth * 2 * halfheight; ++i) {
    EXPECT_EQ(src_vu[i], dst_vu[i]);
  }

  free_aligned_buffer_page_end(src_y);
  free_aligned_buffer_page_end(src_vu);
  free_aligned_buffer_page_end(dst_y);
  free_aligned_buffer_page_end(dst_vu);
}

}  // namespace libyuv
