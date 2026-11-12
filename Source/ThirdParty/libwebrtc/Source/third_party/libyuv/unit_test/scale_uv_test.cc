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
#include "libyuv/scale.h"
#include "libyuv/scale_uv.h"

namespace libyuv {

#define STRINGIZE(line) #line
#define FILELINESTR(file, line) file ":" STRINGIZE(line)

#if !defined(DISABLE_SLOW_TESTS) || defined(__x86_64__) || defined(__i386__)
// SLOW TESTS are those that are unoptimized C code.
// FULL TESTS are optimized but test many variations of the same code.
#define ENABLE_FULL_TESTS
#endif

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

  int i;
  int64_t src_uv_plane_size = Abs(src_width) * Abs(src_height) * 2LL;
  int src_stride_uv = Abs(src_width) * 2;
  int64_t dst_uv_plane_size = dst_width * dst_height * 2LL;
  int dst_stride_uv = dst_width * 2;

  align_buffer_page_end(src_uv, src_uv_plane_size);
  align_buffer_page_end(dst_uv_c, dst_uv_plane_size);
  align_buffer_page_end(dst_uv_opt, dst_uv_plane_size);

  if (!src_uv || !dst_uv_c || !dst_uv_opt) {
    printf("Skipped.  Alloc failed " FILELINESTR(__FILE__, __LINE__) "\n");
    return 0;
  }
  MemRandomize(src_uv, src_uv_plane_size);
  memset(dst_uv_c, 2, dst_uv_plane_size);
  memset(dst_uv_opt, 123, dst_uv_plane_size);

  MaskCpuFlags(disable_cpu_flags);  // Disable all CPU optimization.
  double c_time = get_time();
  UVScale(src_uv, src_stride_uv, src_width, src_height, dst_uv_c, dst_stride_uv,
          dst_width, dst_height, f);
  c_time = (get_time() - c_time);

  MaskCpuFlags(benchmark_cpu_info);  // Enable all CPU optimization.
  double opt_time = get_time();
  for (i = 0; i < benchmark_iterations; ++i) {
    UVScale(src_uv, src_stride_uv, src_width, src_height, dst_uv_opt,
            dst_stride_uv, dst_width, dst_height, f);
  }
  opt_time = (get_time() - opt_time) / benchmark_iterations;

  // Report performance of C vs OPT
  printf("filter %d - %8d us C - %8d us OPT\n", f,
         static_cast<int>(c_time * 1e6), static_cast<int>(opt_time * 1e6));

  int max_diff = 0;
  for (i = 0; i < dst_uv_plane_size; ++i) {
    int abs_diff = Abs(dst_uv_c[i] - dst_uv_opt[i]);
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
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

#define TEST_FACTOR1(name, filter, nom, denom)                               \
  TEST_F(LibYUVScaleTest, UVScaleDownBy##name##_##filter) {                  \
    int diff = UVTestFilter(                                                 \
        SX(benchmark_width_, nom, denom), SX(benchmark_height_, nom, denom), \
        DX(benchmark_width_, nom, denom), DX(benchmark_height_, nom, denom), \
        kFilter##filter, benchmark_iterations_, disable_cpu_flags_,          \
        benchmark_cpu_info_);                                                \
    ASSERT_EQ(0, diff);                                                      \
  }

#if defined(ENABLE_FULL_TESTS)
// Test a scale factor with all 4 filters.  Expect exact for SIMD vs C.
#define TEST_FACTOR(name, nom, denom)      \
  TEST_FACTOR1(name, None, nom, denom)     \
  TEST_FACTOR1(name, Linear, nom, denom)   \
  TEST_FACTOR1(name, Bilinear, nom, denom) \
  TEST_FACTOR1(name, Box, nom, denom)
#else
// Test a scale factor with Bilinear.
#define TEST_FACTOR(name, nom, denom) TEST_FACTOR1(name, Bilinear, nom, denom)
#endif

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
    ASSERT_LE(diff, max_diff);                                              \
  }                                                                         \
  TEST_F(LibYUVScaleTest, name##From##width##x##height##_##filter) {        \
    int diff = UVTestFilter(width, height, Abs(benchmark_width_),           \
                            Abs(benchmark_height_), kFilter##filter,        \
                            benchmark_iterations_, disable_cpu_flags_,      \
                            benchmark_cpu_info_);                           \
    ASSERT_LE(diff, max_diff);                                              \
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

TEST_SCALETO(UVScale, 1, 1)
TEST_SCALETO(UVScale, 569, 480)
TEST_SCALETO(UVScale, 640, 360)
#ifndef DISABLE_SLOW_TESTS
TEST_SCALETO(UVScale, 256, 144) /* 128x72 * 2 */
TEST_SCALETO(UVScale, 320, 240)
TEST_SCALETO(UVScale, 1280, 720)
TEST_SCALETO(UVScale, 1920, 1080)
#endif  // DISABLE_SLOW_TESTS
#undef TEST_SCALETO1
#undef TEST_SCALETO

#define TEST_SCALESWAPXY1(name, filter, max_diff)                              \
  TEST_F(LibYUVScaleTest, name##SwapXY_##filter) {                             \
    int diff =                                                                 \
        UVTestFilter(benchmark_width_, benchmark_height_, benchmark_height_,   \
                     benchmark_width_, kFilter##filter, benchmark_iterations_, \
                     disable_cpu_flags_, benchmark_cpu_info_);                 \
    ASSERT_LE(diff, max_diff);                                                 \
  }

#if defined(ENABLE_FULL_TESTS)
// Test scale with swapped width and height with all 3 filters.
TEST_SCALESWAPXY1(UVScale, None, 0)
TEST_SCALESWAPXY1(UVScale, Linear, 0)
TEST_SCALESWAPXY1(UVScale, Bilinear, 0)
#else
TEST_SCALESWAPXY1(UVScale, Bilinear, 0)
#endif
#undef TEST_SCALESWAPXY1

TEST_F(LibYUVScaleTest, UVTest3x) {
  const int kSrcStride = 480 * 2;
  const int kDstStride = 160 * 2;
  const int kSize = kSrcStride * 3;
  align_buffer_page_end(orig_pixels, kSize);
  for (int i = 0; i < 480 * 3; ++i) {
    orig_pixels[i * 2 + 0] = i;
    orig_pixels[i * 2 + 1] = 255 - i;
  }
  align_buffer_page_end(dest_pixels, kDstStride);

  int iterations160 = (benchmark_width_ * benchmark_height_ + (160 - 1)) / 160 *
                      benchmark_iterations_;
  for (int i = 0; i < iterations160; ++i) {
    UVScale(orig_pixels, kSrcStride, 480, 3, dest_pixels, kDstStride, 160, 1,
            kFilterBilinear);
  }

  ASSERT_EQ(225, dest_pixels[0]);
  ASSERT_EQ(255 - 225, dest_pixels[1]);

  UVScale(orig_pixels, kSrcStride, 480, 3, dest_pixels, kDstStride, 160, 1,
          kFilterNone);

  ASSERT_EQ(225, dest_pixels[0]);
  ASSERT_EQ(255 - 225, dest_pixels[1]);

  free_aligned_buffer_page_end(dest_pixels);
  free_aligned_buffer_page_end(orig_pixels);
}

TEST_F(LibYUVScaleTest, UVTest4x) {
  const int kSrcStride = 640 * 2;
  const int kDstStride = 160 * 2;
  const int kSize = kSrcStride * 4;
  align_buffer_page_end(orig_pixels, kSize);
  for (int i = 0; i < 640 * 4; ++i) {
    orig_pixels[i * 2 + 0] = i;
    orig_pixels[i * 2 + 1] = 255 - i;
  }
  align_buffer_page_end(dest_pixels, kDstStride);

  int iterations160 = (benchmark_width_ * benchmark_height_ + (160 - 1)) / 160 *
                      benchmark_iterations_;
  for (int i = 0; i < iterations160; ++i) {
    UVScale(orig_pixels, kSrcStride, 640, 4, dest_pixels, kDstStride, 160, 1,
            kFilterBilinear);
  }

  ASSERT_EQ(66, dest_pixels[0]);
  ASSERT_EQ(190, dest_pixels[1]);

  UVScale(orig_pixels, kSrcStride, 64, 4, dest_pixels, kDstStride, 16, 1,
          kFilterNone);

  ASSERT_EQ(2, dest_pixels[0]);  // expect the 3rd pixel of the 3rd row
  ASSERT_EQ(255 - 2, dest_pixels[1]);

  free_aligned_buffer_page_end(dest_pixels);
  free_aligned_buffer_page_end(orig_pixels);
}

// Box filtering an interleaved UV plane has to produce the same bytes as box
// filtering the U and V planes separately, which is the path I420 takes.
// C and SIMD agreeing is not enough here: before ScaleUVBox existed both
// point sampled, so this compares against an independent implementation.
static int UVTestBoxAgainstPlanes(int src_width,
                                  int src_height,
                                  int dst_width,
                                  int dst_height,
                                  int cpu_info) {
  if (!SizeValid(src_width, src_height, dst_width, dst_height)) {
    return 0;
  }
  int64_t src_uv_plane_size = src_width * src_height * 2LL;
  int64_t src_plane_size = src_width * src_height * 1LL;
  int64_t dst_uv_plane_size = dst_width * dst_height * 2LL;
  int64_t dst_plane_size = dst_width * dst_height * 1LL;

  align_buffer_page_end(src_uv, src_uv_plane_size);
  align_buffer_page_end(src_u, src_plane_size);
  align_buffer_page_end(src_v, src_plane_size);
  align_buffer_page_end(dst_uv, dst_uv_plane_size);
  align_buffer_page_end(dst_u, dst_plane_size);
  align_buffer_page_end(dst_v, dst_plane_size);
  if (!src_uv || !src_u || !src_v || !dst_uv || !dst_u || !dst_v) {
    printf("Skipped.  Alloc failed " FILELINESTR(__FILE__, __LINE__) "\n");
    return 0;
  }
  MemRandomize(src_uv, src_uv_plane_size);
  memset(dst_uv, 123, dst_uv_plane_size);
  for (int i = 0; i < src_width * src_height; ++i) {
    src_u[i] = src_uv[i * 2 + 0];
    src_v[i] = src_uv[i * 2 + 1];
  }

  MaskCpuFlags(cpu_info);
  UVScale(src_uv, src_width * 2, src_width, src_height, dst_uv, dst_width * 2,
          dst_width, dst_height, kFilterBox);
  ScalePlane(src_u, src_width, src_width, src_height, dst_u, dst_width,
             dst_width, dst_height, kFilterBox);
  ScalePlane(src_v, src_width, src_width, src_height, dst_v, dst_width,
             dst_width, dst_height, kFilterBox);

  int max_diff = 0;
  for (int i = 0; i < dst_width * dst_height; ++i) {
    int du = Abs(dst_uv[i * 2 + 0] - dst_u[i]);
    int dv = Abs(dst_uv[i * 2 + 1] - dst_v[i]);
    if (du > max_diff) {
      max_diff = du;
    }
    if (dv > max_diff) {
      max_diff = dv;
    }
  }

  free_aligned_buffer_page_end(dst_v);
  free_aligned_buffer_page_end(dst_u);
  free_aligned_buffer_page_end(dst_uv);
  free_aligned_buffer_page_end(src_v);
  free_aligned_buffer_page_end(src_u);
  free_aligned_buffer_page_end(src_uv);
  return max_diff;
}

// Chroma dimensions, ie half the frame sizes named in the comments.
// max_diff is 0 for everything ScaleUVBox handles. The 1/2 and 1/4 fast paths
// predate it and keep their own rounding, so 1/4 is allowed to drift by one.
#define TEST_BOXPLANE(name, swidth, sheight, dwidth, dheight, max_diff)     \
  TEST_F(LibYUVScaleTest, UVScaleBoxMatchesPlane##name) {                   \
    int diff_c = UVTestBoxAgainstPlanes(swidth, sheight, dwidth, dheight,   \
                                        disable_cpu_flags_);                \
    ASSERT_LE(diff_c, max_diff);                                            \
    int diff_opt = UVTestBoxAgainstPlanes(swidth, sheight, dwidth, dheight, \
                                          benchmark_cpu_info_);             \
    ASSERT_LE(diff_opt, max_diff);                                          \
  }

TEST_BOXPLANE(By2, 960, 540, 480, 270, 0)         /* 1080p to 960x540 */
TEST_BOXPLANE(By3, 960, 540, 320, 180, 0)         /* 1080p to 640x360 */
TEST_BOXPLANE(By3Rounded, 640, 360, 214, 120, 0)  /* 720p to 427x240 */
TEST_BOXPLANE(By4, 960, 540, 240, 135, 1)         /* ScaleUVDown4Box */
TEST_BOXPLANE(By3_75, 960, 540, 256, 144, 0)      /* 1080p to 512x288 */
TEST_BOXPLANE(By6, 640, 360, 106, 60, 0)          /* 720p to 213x120 */
TEST_BOXPLANE(By8, 640, 360, 80, 45, 0)           /* 720p to 160x90 */
TEST_BOXPLANE(OddTail, 33, 17, 7, 5, 0)           /* not a multiple of 8 wide */
TEST_BOXPLANE(Tiny, 80, 60, 3, 2, 0)              /* dst below one SIMD group */
#undef TEST_BOXPLANE

// A box filter has to read the whole box. Every 3x3 block here is 255 except
// for its top left pixel, so point sampling the block origin yields 0 while
// the box average is 8 * 255 * (65536 / 9) >> 16 = 226.
TEST_F(LibYUVScaleTest, UVScaleBoxAverages) {
  const int kSrcWidth = 48;
  const int kSrcHeight = 24;
  const int kDstWidth = kSrcWidth / 3;
  const int kDstHeight = kSrcHeight / 3;
  align_buffer_page_end(src_uv, kSrcWidth * kSrcHeight * 2);
  align_buffer_page_end(dst_uv, kDstWidth * kDstHeight * 2);

  memset(src_uv, 255, kSrcWidth * kSrcHeight * 2);
  for (int y = 0; y < kSrcHeight; y += 3) {
    for (int x = 0; x < kSrcWidth; x += 3) {
      src_uv[(y * kSrcWidth + x) * 2 + 0] = 0;
      src_uv[(y * kSrcWidth + x) * 2 + 1] = 0;
    }
  }
  memset(dst_uv, 123, kDstWidth * kDstHeight * 2);

  UVScale(src_uv, kSrcWidth * 2, kSrcWidth, kSrcHeight, dst_uv, kDstWidth * 2,
          kDstWidth, kDstHeight, kFilterBox);

  for (int i = 0; i < kDstWidth * kDstHeight * 2; ++i) {
    ASSERT_EQ(226, dst_uv[i]) << "at " << i;
  }

  free_aligned_buffer_page_end(dst_uv);
  free_aligned_buffer_page_end(src_uv);
}

}  // namespace libyuv
