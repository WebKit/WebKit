/*
 *  Copyright 2022 The LibYuv Project Authors. All rights reserved.
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
#include "libyuv/cpu_id.h"
#include "libyuv/scale_rgb.h"

namespace libyuv {

#define STRINGIZE(line) #line
#define FILELINESTR(file, line) file ":" STRINGIZE(line)

#if !defined(DISABLE_SLOW_TESTS) || defined(__x86_64__) || defined(__i386__)
// SLOW TESTS are those that are unoptimized C code.
// FULL TESTS are optimized but test many variations of the same code.
#define ENABLE_FULL_TESTS
#endif

// Test scaling with C vs Opt and return maximum pixel difference. 0 = exact.
static int RGBTestFilter(int src_width,
                         int src_height,
                         int dst_width,
                         int dst_height,
                         FilterMode f,
                         int benchmark_iterations,
                         int disable_cpu_flags,
                         int benchmark_cpu_info) {
  if (!SizeValid(src_width, src_height, dst_width, dst_height)) {
    return 0;
  }

  int i, j;
  const int b = 0;  // 128 to test for padding/stride.
  int64_t src_rgb_plane_size =
      (Abs(src_width) + b * 3) * (Abs(src_height) + b * 3) * 3LL;
  int src_stride_rgb = (b * 3 + Abs(src_width)) * 3;

  align_buffer_page_end(src_rgb, src_rgb_plane_size);
  if (!src_rgb) {
    printf("Skipped.  Alloc failed " FILELINESTR(__FILE__, __LINE__) "\n");
    return 0;
  }
  MemRandomize(src_rgb, src_rgb_plane_size);

  int64_t dst_rgb_plane_size = (dst_width + b * 3) * (dst_height + b * 3) * 3LL;
  int dst_stride_rgb = (b * 3 + dst_width) * 3;

  align_buffer_page_end(dst_rgb_c, dst_rgb_plane_size);
  align_buffer_page_end(dst_rgb_opt, dst_rgb_plane_size);
  if (!dst_rgb_c || !dst_rgb_opt) {
    printf("Skipped.  Alloc failed " FILELINESTR(__FILE__, __LINE__) "\n");
    return 0;
  }
  memset(dst_rgb_c, 2, dst_rgb_plane_size);
  memset(dst_rgb_opt, 3, dst_rgb_plane_size);

  // Warm up both versions for consistent benchmarks.
  MaskCpuFlags(disable_cpu_flags);  // Disable all CPU optimization.
  RGBScale(src_rgb + (src_stride_rgb * b) + b * 3, src_stride_rgb, src_width,
           src_height, dst_rgb_c + (dst_stride_rgb * b) + b * 3, dst_stride_rgb,
           dst_width, dst_height, f);
  MaskCpuFlags(benchmark_cpu_info);  // Enable all CPU optimization.
  RGBScale(src_rgb + (src_stride_rgb * b) + b * 3, src_stride_rgb, src_width,
           src_height, dst_rgb_opt + (dst_stride_rgb * b) + b * 3,
           dst_stride_rgb, dst_width, dst_height, f);

  MaskCpuFlags(disable_cpu_flags);  // Disable all CPU optimization.
  double c_time = get_time();
  RGBScale(src_rgb + (src_stride_rgb * b) + b * 3, src_stride_rgb, src_width,
           src_height, dst_rgb_c + (dst_stride_rgb * b) + b * 3, dst_stride_rgb,
           dst_width, dst_height, f);

  c_time = (get_time() - c_time);

  MaskCpuFlags(benchmark_cpu_info);  // Enable all CPU optimization.
  double opt_time = get_time();
  for (i = 0; i < benchmark_iterations; ++i) {
    RGBScale(src_rgb + (src_stride_rgb * b) + b * 3, src_stride_rgb, src_width,
             src_height, dst_rgb_opt + (dst_stride_rgb * b) + b * 3,
             dst_stride_rgb, dst_width, dst_height, f);
  }
  opt_time = (get_time() - opt_time) / benchmark_iterations;

  // Report performance of C vs OPT
  printf("filter %d - %8d us C - %8d us OPT\n", f,
         static_cast<int>(c_time * 1e6), static_cast<int>(opt_time * 1e6));

  // C version may be a little off from the optimized. Order of
  //  operations may introduce rounding somewhere. So do a difference
  //  of the buffers and look to see that the max difference isn't
  //  over 2.
  int max_diff = 0;
  for (i = b; i < (dst_height + b); ++i) {
    for (j = b * 3; j < (dst_width + b) * 3; ++j) {
      int abs_diff = Abs(dst_rgb_c[(i * dst_stride_rgb) + j] -
                         dst_rgb_opt[(i * dst_stride_rgb) + j]);
      if (abs_diff > max_diff) {
        max_diff = abs_diff;
      }
    }
  }

  free_aligned_buffer_page_end(dst_rgb_c);
  free_aligned_buffer_page_end(dst_rgb_opt);
  free_aligned_buffer_page_end(src_rgb);
  return max_diff;
}

// The following adjustments in dimensions ensure the scale factor will be
// exactly achieved.
#define DX(x, nom, denom) static_cast<int>((Abs(x) / nom) * nom)
#define SX(x, nom, denom) static_cast<int>((x / nom) * denom)

#define TEST_FACTOR1(name, filter, nom, denom, max_diff)                     \
  TEST_F(LibYUVScaleTest, RGBScaleDownBy##name##_##filter) {                 \
    int diff = RGBTestFilter(                                                \
        SX(benchmark_width_, nom, denom), SX(benchmark_height_, nom, denom), \
        DX(benchmark_width_, nom, denom), DX(benchmark_height_, nom, denom), \
        kFilter##filter, benchmark_iterations_, disable_cpu_flags_,          \
        benchmark_cpu_info_);                                                \
    EXPECT_LE(diff, max_diff);                                               \
  }

#if defined(ENABLE_FULL_TESTS)
// Test a scale factor with all 4 filters.  Expect unfiltered to be exact, but
// filtering is different fixed point implementations for SSSE3, Neon and C.
#define TEST_FACTOR(name, nom, denom)         \
  TEST_FACTOR1(name, None, nom, denom, 0)     \
  TEST_FACTOR1(name, Linear, nom, denom, 3)   \
  TEST_FACTOR1(name, Bilinear, nom, denom, 3) \
  TEST_FACTOR1(name, Box, nom, denom, 3)
#else
// Test a scale factor with Bilinear.
#define TEST_FACTOR(name, nom, denom) \
  TEST_FACTOR1(name, Bilinear, nom, denom, 3)
#endif

TEST_FACTOR(2, 1, 2)
#ifndef DISABLE_SLOW_TESTS
TEST_FACTOR(4, 1, 4)
// TEST_FACTOR(8, 1, 8)  Disable for benchmark performance.
TEST_FACTOR(3by4, 3, 4)
TEST_FACTOR(3by8, 3, 8)
TEST_FACTOR(3, 1, 3)
#endif
#undef TEST_FACTOR1
#undef TEST_FACTOR
#undef SX
#undef DX

#define TEST_SCALETO1(name, width, height, filter, max_diff)                 \
  TEST_F(LibYUVScaleTest, name##To##width##x##height##_##filter) {           \
    int diff = RGBTestFilter(benchmark_width_, benchmark_height_, width,     \
                             height, kFilter##filter, benchmark_iterations_, \
                             disable_cpu_flags_, benchmark_cpu_info_);       \
    EXPECT_LE(diff, max_diff);                                               \
  }                                                                          \
  TEST_F(LibYUVScaleTest, name##From##width##x##height##_##filter) {         \
    int diff = RGBTestFilter(width, height, Abs(benchmark_width_),           \
                             Abs(benchmark_height_), kFilter##filter,        \
                             benchmark_iterations_, disable_cpu_flags_,      \
                             benchmark_cpu_info_);                           \
    EXPECT_LE(diff, max_diff);                                               \
  }

#if defined(ENABLE_FULL_TESTS)
/// Test scale to a specified size with all 4 filters.
#define TEST_SCALETO(name, width, height)       \
  TEST_SCALETO1(name, width, height, None, 0)   \
  TEST_SCALETO1(name, width, height, Linear, 3) \
  TEST_SCALETO1(name, width, height, Bilinear, 3)
#else
#define TEST_SCALETO(name, width, height) \
  TEST_SCALETO1(name, width, height, Bilinear, 3)
#endif

TEST_SCALETO(RGBScale, 640, 360)
#ifndef DISABLE_SLOW_TESTS
TEST_SCALETO(RGBScale, 1, 1)
TEST_SCALETO(RGBScale, 256, 144) /* 128x72 * 3 */
TEST_SCALETO(RGBScale, 320, 240)
TEST_SCALETO(RGBScale, 569, 480)
TEST_SCALETO(RGBScale, 1280, 720)
TEST_SCALETO(RGBScale, 1920, 1080)
#endif  // DISABLE_SLOW_TESTS
#undef TEST_SCALETO1
#undef TEST_SCALETO

#define TEST_SCALESWAPXY1(name, filter, max_diff)                      \
  TEST_F(LibYUVScaleTest, name##SwapXY_##filter) {                     \
    int diff = RGBTestFilter(benchmark_width_, benchmark_height_,      \
                             benchmark_height_, benchmark_width_,      \
                             kFilter##filter, benchmark_iterations_,   \
                             disable_cpu_flags_, benchmark_cpu_info_); \
    EXPECT_LE(diff, max_diff);                                         \
  }

#if defined(ENABLE_FULL_TESTS)
// Test scale with swapped width and height with all 3 filters.
TEST_SCALESWAPXY1(RGBScale, None, 0)
TEST_SCALESWAPXY1(RGBScale, Linear, 0)
TEST_SCALESWAPXY1(RGBScale, Bilinear, 0)
#else
TEST_SCALESWAPXY1(RGBScale, Bilinear, 0)
#endif
#undef TEST_SCALESWAPXY1

TEST_F(LibYUVScaleTest, RGBTest3x) {
  const int kSrcStride = 480 * 3;
  const int kDstStride = 160 * 3;
  const int kSize = kSrcStride * 3;
  align_buffer_page_end(orig_pixels, kSize);
  for (int i = 0; i < 480 * 3; ++i) {
    orig_pixels[i * 3 + 0] = i;
    orig_pixels[i * 3 + 1] = 255 - i;
  }
  align_buffer_page_end(dest_pixels, kDstStride);

  int iterations160 = (benchmark_width_ * benchmark_height_ + (160 - 1)) / 160 *
                      benchmark_iterations_;
  for (int i = 0; i < iterations160; ++i) {
    RGBScale(orig_pixels, kSrcStride, 480, 3, dest_pixels, kDstStride, 160, 1,
             kFilterBilinear);
  }

  EXPECT_EQ(225, dest_pixels[0]);
  EXPECT_EQ(255 - 225, dest_pixels[1]);

  RGBScale(orig_pixels, kSrcStride, 480, 3, dest_pixels, kDstStride, 160, 1,
           kFilterNone);

  EXPECT_EQ(225, dest_pixels[0]);
  EXPECT_EQ(255 - 225, dest_pixels[1]);

  free_aligned_buffer_page_end(dest_pixels);
  free_aligned_buffer_page_end(orig_pixels);
}

TEST_F(LibYUVScaleTest, RGBTest4x) {
  const int kSrcStride = 640 * 3;
  const int kDstStride = 160 * 3;
  const int kSize = kSrcStride * 4;
  align_buffer_page_end(orig_pixels, kSize);
  for (int i = 0; i < 640 * 4; ++i) {
    orig_pixels[i * 3 + 0] = i;
    orig_pixels[i * 3 + 1] = 255 - i;
  }
  align_buffer_page_end(dest_pixels, kDstStride);

  int iterations160 = (benchmark_width_ * benchmark_height_ + (160 - 1)) / 160 *
                      benchmark_iterations_;
  for (int i = 0; i < iterations160; ++i) {
    RGBScale(orig_pixels, kSrcStride, 640, 4, dest_pixels, kDstStride, 160, 1,
             kFilterBilinear);
  }

  EXPECT_EQ(66, dest_pixels[0]);
  EXPECT_EQ(190, dest_pixels[1]);

  RGBScale(orig_pixels, kSrcStride, 64, 4, dest_pixels, kDstStride, 16, 1,
           kFilterNone);

  EXPECT_EQ(2, dest_pixels[0]);  // expect the 3rd pixel of the 3rd row
  EXPECT_EQ(255 - 2, dest_pixels[1]);

  free_aligned_buffer_page_end(dest_pixels);
  free_aligned_buffer_page_end(orig_pixels);
}

}  // namespace libyuv
