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
#include "libyuv/cpu_id.h"
#include "libyuv/scale_uv.h"

namespace libyuv {

#define STRINGIZE(line) #line
#define FILELINESTR(file, line) file ":" STRINGIZE(line)

// Test scaling with C vs Opt and return maximum pixel difference. 0 = exact.
static int UVTestFilter(int src_width,
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
  int64_t src_uv_plane_size =
      (Abs(src_width) + b * 2) * (Abs(src_height) + b * 2) * 2LL;
  int src_stride_uv = (b * 2 + Abs(src_width)) * 2;

  align_buffer_page_end(src_uv, src_uv_plane_size);
  if (!src_uv) {
    printf("Skipped.  Alloc failed " FILELINESTR(__FILE__, __LINE__) "\n");
    return 0;
  }
  MemRandomize(src_uv, src_uv_plane_size);

  int64_t dst_uv_plane_size = (dst_width + b * 2) * (dst_height + b * 2) * 2LL;
  int dst_stride_uv = (b * 2 + dst_width) * 2;

  align_buffer_page_end(dst_uv_c, dst_uv_plane_size);
  align_buffer_page_end(dst_uv_opt, dst_uv_plane_size);
  if (!dst_uv_c || !dst_uv_opt) {
    printf("Skipped.  Alloc failed " FILELINESTR(__FILE__, __LINE__) "\n");
    return 0;
  }
  memset(dst_uv_c, 2, dst_uv_plane_size);
  memset(dst_uv_opt, 3, dst_uv_plane_size);

  // Warm up both versions for consistent benchmarks.
  MaskCpuFlags(disable_cpu_flags);  // Disable all CPU optimization.
  UVScale(src_uv + (src_stride_uv * b) + b * 2, src_stride_uv, src_width,
          src_height, dst_uv_c + (dst_stride_uv * b) + b * 2, dst_stride_uv,
          dst_width, dst_height, f);
  MaskCpuFlags(benchmark_cpu_info);  // Enable all CPU optimization.
  UVScale(src_uv + (src_stride_uv * b) + b * 2, src_stride_uv, src_width,
          src_height, dst_uv_opt + (dst_stride_uv * b) + b * 2, dst_stride_uv,
          dst_width, dst_height, f);

  MaskCpuFlags(disable_cpu_flags);  // Disable all CPU optimization.
  double c_time = get_time();
  UVScale(src_uv + (src_stride_uv * b) + b * 2, src_stride_uv, src_width,
          src_height, dst_uv_c + (dst_stride_uv * b) + b * 2, dst_stride_uv,
          dst_width, dst_height, f);

  c_time = (get_time() - c_time);

  MaskCpuFlags(benchmark_cpu_info);  // Enable all CPU optimization.
  double opt_time = get_time();
  for (i = 0; i < benchmark_iterations; ++i) {
    UVScale(src_uv + (src_stride_uv * b) + b * 2, src_stride_uv, src_width,
            src_height, dst_uv_opt + (dst_stride_uv * b) + b * 2, dst_stride_uv,
            dst_width, dst_height, f);
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
    for (j = b * 2; j < (dst_width + b) * 2; ++j) {
      int abs_diff = Abs(dst_uv_c[(i * dst_stride_uv) + j] -
                         dst_uv_opt[(i * dst_stride_uv) + j]);
      if (abs_diff > max_diff) {
        max_diff = abs_diff;
      }
    }
  }

  free_aligned_buffer_page_end(dst_uv_c);
  free_aligned_buffer_page_end(dst_uv_opt);
  free_aligned_buffer_page_end(src_uv);
  return max_diff;
}

// The following adjustments in dimensions ensure the scale factor will be
// exactly achieved.
#define DX(x, nom, denom) static_cast<int>((Abs(x) / nom) * nom)
#define SX(x, nom, denom) static_cast<int>((x / nom) * denom)

#define TEST_FACTOR1(name, filter, nom, denom, max_diff)                     \
  TEST_F(LibYUVScaleTest, UVScaleDownBy##name##_##filter) {                  \
    int diff = UVTestFilter(                                                 \
        SX(benchmark_width_, nom, denom), SX(benchmark_height_, nom, denom), \
        DX(benchmark_width_, nom, denom), DX(benchmark_height_, nom, denom), \
        kFilter##filter, benchmark_iterations_, disable_cpu_flags_,          \
        benchmark_cpu_info_);                                                \
    EXPECT_LE(diff, max_diff);                                               \
  }

// Test a scale factor with all 4 filters.  Expect unfiltered to be exact, but
// filtering is different fixed point implementations for SSSE3, Neon and C.
#define TEST_FACTOR(name, nom, denom)         \
  TEST_FACTOR1(name, None, nom, denom, 0)     \
  TEST_FACTOR1(name, Linear, nom, denom, 3)   \
  TEST_FACTOR1(name, Bilinear, nom, denom, 3) \
  TEST_FACTOR1(name, Box, nom, denom, 3)

TEST_FACTOR(2, 1, 2)
TEST_FACTOR(4, 1, 4)
// TEST_FACTOR(8, 1, 8)  Disable for benchmark performance.
TEST_FACTOR(3by4, 3, 4)
TEST_FACTOR(3by8, 3, 8)
TEST_FACTOR(3, 1, 3)
#undef TEST_FACTOR1
#undef TEST_FACTOR
#undef SX
#undef DX

#define TEST_SCALETO1(name, width, height, filter, max_diff)                \
  TEST_F(LibYUVScaleTest, name##To##width##x##height##_##filter) {          \
    int diff = UVTestFilter(benchmark_width_, benchmark_height_, width,     \
                            height, kFilter##filter, benchmark_iterations_, \
                            disable_cpu_flags_, benchmark_cpu_info_);       \
    EXPECT_LE(diff, max_diff);                                              \
  }                                                                         \
  TEST_F(LibYUVScaleTest, name##From##width##x##height##_##filter) {        \
    int diff = UVTestFilter(width, height, Abs(benchmark_width_),           \
                            Abs(benchmark_height_), kFilter##filter,        \
                            benchmark_iterations_, disable_cpu_flags_,      \
                            benchmark_cpu_info_);                           \
    EXPECT_LE(diff, max_diff);                                              \
  }

/// Test scale to a specified size with all 4 filters.
#define TEST_SCALETO(name, width, height)       \
  TEST_SCALETO1(name, width, height, None, 0)   \
  TEST_SCALETO1(name, width, height, Linear, 3) \
  TEST_SCALETO1(name, width, height, Bilinear, 3)

TEST_SCALETO(UVScale, 1, 1)
TEST_SCALETO(UVScale, 256, 144) /* 128x72 * 2 */
TEST_SCALETO(UVScale, 320, 240)
TEST_SCALETO(UVScale, 569, 480)
TEST_SCALETO(UVScale, 640, 360)
#ifdef ENABLE_SLOW_TESTS
TEST_SCALETO(UVScale, 1280, 720)
TEST_SCALETO(UVScale, 1920, 1080)
#endif  // ENABLE_SLOW_TESTS
#undef TEST_SCALETO1
#undef TEST_SCALETO

#define TEST_SCALESWAPXY1(name, filter, max_diff)                              \
  TEST_F(LibYUVScaleTest, name##SwapXY_##filter) {                             \
    int diff =                                                                 \
        UVTestFilter(benchmark_width_, benchmark_height_, benchmark_height_,   \
                     benchmark_width_, kFilter##filter, benchmark_iterations_, \
                     disable_cpu_flags_, benchmark_cpu_info_);                 \
    EXPECT_LE(diff, max_diff);                                                 \
  }

// Test scale with swapped width and height with all 3 filters.
TEST_SCALESWAPXY1(UVScale, None, 0)
TEST_SCALESWAPXY1(UVScale, Linear, 0)
TEST_SCALESWAPXY1(UVScale, Bilinear, 0)
#undef TEST_SCALESWAPXY1

TEST_F(LibYUVScaleTest, UVTest3x) {
  const int kSrcStride = 48 * 2;
  const int kDstStride = 16 * 2;
  const int kSize = kSrcStride * 3;
  align_buffer_page_end(orig_pixels, kSize);
  for (int i = 0; i < 48 * 3; ++i) {
    orig_pixels[i * 2 + 0] = i;
    orig_pixels[i * 2 + 1] = 255 - i;
  }
  align_buffer_page_end(dest_pixels, kDstStride);

  int iterations16 =
      benchmark_width_ * benchmark_height_ / (16 * 1) * benchmark_iterations_;
  for (int i = 0; i < iterations16; ++i) {
    UVScale(orig_pixels, kSrcStride, 48, 3, dest_pixels, kDstStride, 16, 1,
            kFilterBilinear);
  }

  EXPECT_EQ(49, dest_pixels[0]);
  EXPECT_EQ(255 - 49, dest_pixels[1]);

  UVScale(orig_pixels, kSrcStride, 48, 3, dest_pixels, kDstStride, 16, 1,
          kFilterNone);

  EXPECT_EQ(49, dest_pixels[0]);
  EXPECT_EQ(255 - 49, dest_pixels[1]);

  free_aligned_buffer_page_end(dest_pixels);
  free_aligned_buffer_page_end(orig_pixels);
}

TEST_F(LibYUVScaleTest, UVTest4x) {
  const int kSrcStride = 64 * 2;
  const int kDstStride = 16 * 2;
  const int kSize = kSrcStride * 4;
  align_buffer_page_end(orig_pixels, kSize);
  for (int i = 0; i < 64 * 4; ++i) {
    orig_pixels[i * 2 + 0] = i;
    orig_pixels[i * 2 + 1] = 255 - i;
  }
  align_buffer_page_end(dest_pixels, kDstStride);

  int iterations16 =
      benchmark_width_ * benchmark_height_ / (16 * 1) * benchmark_iterations_;
  for (int i = 0; i < iterations16; ++i) {
    UVScale(orig_pixels, kSrcStride, 64, 4, dest_pixels, kDstStride, 16, 1,
            kFilterBilinear);
  }

  EXPECT_EQ((65 + 66 + 129 + 130 + 2) / 4, dest_pixels[0]);
  EXPECT_EQ((255 - 65 + 255 - 66 + 255 - 129 + 255 - 130 + 2) / 4,
            dest_pixels[1]);

  UVScale(orig_pixels, kSrcStride, 64, 4, dest_pixels, kDstStride, 16, 1,
          kFilterNone);

  EXPECT_EQ(130, dest_pixels[0]);  // expect the 3rd pixel of the 3rd row
  EXPECT_EQ(255 - 130, dest_pixels[1]);

  free_aligned_buffer_page_end(dest_pixels);
  free_aligned_buffer_page_end(orig_pixels);
}

}  // namespace libyuv
