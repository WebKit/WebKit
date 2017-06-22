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

#define STRINGIZE(line) #line
#define FILELINESTR(file, line) file ":" STRINGIZE(line)

namespace libyuv {

// Test scaling with C vs Opt and return maximum pixel difference. 0 = exact.
static int TestFilter(int src_width,
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
  int src_width_uv = (Abs(src_width) + 1) >> 1;
  int src_height_uv = (Abs(src_height) + 1) >> 1;

  int64 src_y_plane_size = (Abs(src_width) + b * 2) * (Abs(src_height) + b * 2);
  int64 src_uv_plane_size = (src_width_uv + b * 2) * (src_height_uv + b * 2);

  int src_stride_y = b * 2 + Abs(src_width);
  int src_stride_uv = b * 2 + src_width_uv;

  align_buffer_page_end(src_y, src_y_plane_size)
      align_buffer_page_end(src_u, src_uv_plane_size) align_buffer_page_end(
          src_v, src_uv_plane_size) if (!src_y || !src_u || !src_v) {
    printf("Skipped.  Alloc failed " FILELINESTR(__FILE__, __LINE__) "\n");
    return 0;
  }
  MemRandomize(src_y, src_y_plane_size);
  MemRandomize(src_u, src_uv_plane_size);
  MemRandomize(src_v, src_uv_plane_size);

  int dst_width_uv = (dst_width + 1) >> 1;
  int dst_height_uv = (dst_height + 1) >> 1;

  int64 dst_y_plane_size = (dst_width + b * 2) * (dst_height + b * 2);
  int64 dst_uv_plane_size = (dst_width_uv + b * 2) * (dst_height_uv + b * 2);

  int dst_stride_y = b * 2 + dst_width;
  int dst_stride_uv = b * 2 + dst_width_uv;

  align_buffer_page_end(dst_y_c, dst_y_plane_size)
      align_buffer_page_end(dst_u_c, dst_uv_plane_size)
          align_buffer_page_end(dst_v_c, dst_uv_plane_size)
              align_buffer_page_end(dst_y_opt, dst_y_plane_size)
                  align_buffer_page_end(dst_u_opt, dst_uv_plane_size)
                      align_buffer_page_end(
                          dst_v_opt,
                          dst_uv_plane_size) if (!dst_y_c || !dst_u_c ||
                                                 !dst_v_c || !dst_y_opt ||
                                                 !dst_u_opt || !dst_v_opt) {
    printf("Skipped.  Alloc failed " FILELINESTR(__FILE__, __LINE__) "\n");
    return 0;
  }

  MaskCpuFlags(disable_cpu_flags);  // Disable all CPU optimization.
  double c_time = get_time();
  I420Scale(src_y + (src_stride_y * b) + b, src_stride_y,
            src_u + (src_stride_uv * b) + b, src_stride_uv,
            src_v + (src_stride_uv * b) + b, src_stride_uv, src_width,
            src_height, dst_y_c + (dst_stride_y * b) + b, dst_stride_y,
            dst_u_c + (dst_stride_uv * b) + b, dst_stride_uv,
            dst_v_c + (dst_stride_uv * b) + b, dst_stride_uv, dst_width,
            dst_height, f);
  c_time = (get_time() - c_time);

  MaskCpuFlags(benchmark_cpu_info);  // Enable all CPU optimization.
  double opt_time = get_time();
  for (i = 0; i < benchmark_iterations; ++i) {
    I420Scale(src_y + (src_stride_y * b) + b, src_stride_y,
              src_u + (src_stride_uv * b) + b, src_stride_uv,
              src_v + (src_stride_uv * b) + b, src_stride_uv, src_width,
              src_height, dst_y_opt + (dst_stride_y * b) + b, dst_stride_y,
              dst_u_opt + (dst_stride_uv * b) + b, dst_stride_uv,
              dst_v_opt + (dst_stride_uv * b) + b, dst_stride_uv, dst_width,
              dst_height, f);
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
    for (j = b; j < (dst_width + b); ++j) {
      int abs_diff = Abs(dst_y_c[(i * dst_stride_y) + j] -
                         dst_y_opt[(i * dst_stride_y) + j]);
      if (abs_diff > max_diff) {
        max_diff = abs_diff;
      }
    }
  }

  for (i = b; i < (dst_height_uv + b); ++i) {
    for (j = b; j < (dst_width_uv + b); ++j) {
      int abs_diff = Abs(dst_u_c[(i * dst_stride_uv) + j] -
                         dst_u_opt[(i * dst_stride_uv) + j]);
      if (abs_diff > max_diff) {
        max_diff = abs_diff;
      }
      abs_diff = Abs(dst_v_c[(i * dst_stride_uv) + j] -
                     dst_v_opt[(i * dst_stride_uv) + j]);
      if (abs_diff > max_diff) {
        max_diff = abs_diff;
      }
    }
  }

  free_aligned_buffer_page_end(dst_y_c) free_aligned_buffer_page_end(dst_u_c)
      free_aligned_buffer_page_end(dst_v_c)
          free_aligned_buffer_page_end(dst_y_opt)
              free_aligned_buffer_page_end(dst_u_opt)
                  free_aligned_buffer_page_end(dst_v_opt)

                      free_aligned_buffer_page_end(src_y)
                          free_aligned_buffer_page_end(src_u)
                              free_aligned_buffer_page_end(src_v)

                                  return max_diff;
}

// Test scaling with 8 bit C vs 16 bit C and return maximum pixel difference.
// 0 = exact.
static int TestFilter_16(int src_width,
                         int src_height,
                         int dst_width,
                         int dst_height,
                         FilterMode f,
                         int benchmark_iterations) {
  if (!SizeValid(src_width, src_height, dst_width, dst_height)) {
    return 0;
  }

  int i, j;
  const int b = 0;  // 128 to test for padding/stride.
  int src_width_uv = (Abs(src_width) + 1) >> 1;
  int src_height_uv = (Abs(src_height) + 1) >> 1;

  int64 src_y_plane_size = (Abs(src_width) + b * 2) * (Abs(src_height) + b * 2);
  int64 src_uv_plane_size = (src_width_uv + b * 2) * (src_height_uv + b * 2);

  int src_stride_y = b * 2 + Abs(src_width);
  int src_stride_uv = b * 2 + src_width_uv;

  align_buffer_page_end(src_y, src_y_plane_size) align_buffer_page_end(
      src_u, src_uv_plane_size) align_buffer_page_end(src_v, src_uv_plane_size)
      align_buffer_page_end(src_y_16, src_y_plane_size * 2)
          align_buffer_page_end(src_u_16, src_uv_plane_size * 2)
              align_buffer_page_end(src_v_16, src_uv_plane_size * 2)
                  uint16* p_src_y_16 = reinterpret_cast<uint16*>(src_y_16);
  uint16* p_src_u_16 = reinterpret_cast<uint16*>(src_u_16);
  uint16* p_src_v_16 = reinterpret_cast<uint16*>(src_v_16);

  MemRandomize(src_y, src_y_plane_size);
  MemRandomize(src_u, src_uv_plane_size);
  MemRandomize(src_v, src_uv_plane_size);

  for (i = b; i < src_height + b; ++i) {
    for (j = b; j < src_width + b; ++j) {
      p_src_y_16[(i * src_stride_y) + j] = src_y[(i * src_stride_y) + j];
    }
  }

  for (i = b; i < (src_height_uv + b); ++i) {
    for (j = b; j < (src_width_uv + b); ++j) {
      p_src_u_16[(i * src_stride_uv) + j] = src_u[(i * src_stride_uv) + j];
      p_src_v_16[(i * src_stride_uv) + j] = src_v[(i * src_stride_uv) + j];
    }
  }

  int dst_width_uv = (dst_width + 1) >> 1;
  int dst_height_uv = (dst_height + 1) >> 1;

  int dst_y_plane_size = (dst_width + b * 2) * (dst_height + b * 2);
  int dst_uv_plane_size = (dst_width_uv + b * 2) * (dst_height_uv + b * 2);

  int dst_stride_y = b * 2 + dst_width;
  int dst_stride_uv = b * 2 + dst_width_uv;

  align_buffer_page_end(dst_y_8, dst_y_plane_size)
      align_buffer_page_end(dst_u_8, dst_uv_plane_size)
          align_buffer_page_end(dst_v_8, dst_uv_plane_size)
              align_buffer_page_end(dst_y_16, dst_y_plane_size * 2)
                  align_buffer_page_end(dst_u_16, dst_uv_plane_size * 2)
                      align_buffer_page_end(dst_v_16, dst_uv_plane_size * 2)

                          uint16* p_dst_y_16 =
                              reinterpret_cast<uint16*>(dst_y_16);
  uint16* p_dst_u_16 = reinterpret_cast<uint16*>(dst_u_16);
  uint16* p_dst_v_16 = reinterpret_cast<uint16*>(dst_v_16);

  I420Scale(src_y + (src_stride_y * b) + b, src_stride_y,
            src_u + (src_stride_uv * b) + b, src_stride_uv,
            src_v + (src_stride_uv * b) + b, src_stride_uv, src_width,
            src_height, dst_y_8 + (dst_stride_y * b) + b, dst_stride_y,
            dst_u_8 + (dst_stride_uv * b) + b, dst_stride_uv,
            dst_v_8 + (dst_stride_uv * b) + b, dst_stride_uv, dst_width,
            dst_height, f);

  for (i = 0; i < benchmark_iterations; ++i) {
    I420Scale_16(p_src_y_16 + (src_stride_y * b) + b, src_stride_y,
                 p_src_u_16 + (src_stride_uv * b) + b, src_stride_uv,
                 p_src_v_16 + (src_stride_uv * b) + b, src_stride_uv, src_width,
                 src_height, p_dst_y_16 + (dst_stride_y * b) + b, dst_stride_y,
                 p_dst_u_16 + (dst_stride_uv * b) + b, dst_stride_uv,
                 p_dst_v_16 + (dst_stride_uv * b) + b, dst_stride_uv, dst_width,
                 dst_height, f);
  }

  // Expect an exact match
  int max_diff = 0;
  for (i = b; i < (dst_height + b); ++i) {
    for (j = b; j < (dst_width + b); ++j) {
      int abs_diff = Abs(dst_y_8[(i * dst_stride_y) + j] -
                         p_dst_y_16[(i * dst_stride_y) + j]);
      if (abs_diff > max_diff) {
        max_diff = abs_diff;
      }
    }
  }

  for (i = b; i < (dst_height_uv + b); ++i) {
    for (j = b; j < (dst_width_uv + b); ++j) {
      int abs_diff = Abs(dst_u_8[(i * dst_stride_uv) + j] -
                         p_dst_u_16[(i * dst_stride_uv) + j]);
      if (abs_diff > max_diff) {
        max_diff = abs_diff;
      }
      abs_diff = Abs(dst_v_8[(i * dst_stride_uv) + j] -
                     p_dst_v_16[(i * dst_stride_uv) + j]);
      if (abs_diff > max_diff) {
        max_diff = abs_diff;
      }
    }
  }

  free_aligned_buffer_page_end(dst_y_8) free_aligned_buffer_page_end(dst_u_8)
      free_aligned_buffer_page_end(dst_v_8)
          free_aligned_buffer_page_end(dst_y_16)
              free_aligned_buffer_page_end(dst_u_16)
                  free_aligned_buffer_page_end(dst_v_16)

                      free_aligned_buffer_page_end(src_y)
                          free_aligned_buffer_page_end(src_u)
                              free_aligned_buffer_page_end(src_v)
                                  free_aligned_buffer_page_end(src_y_16)
                                      free_aligned_buffer_page_end(src_u_16)
                                          free_aligned_buffer_page_end(src_v_16)

                                              return max_diff;
}

// The following adjustments in dimensions ensure the scale factor will be
// exactly achieved.
// 2 is chroma subsample
#define DX(x, nom, denom) static_cast<int>(((Abs(x) / nom + 1) / 2) * nom * 2)
#define SX(x, nom, denom) static_cast<int>(((x / nom + 1) / 2) * denom * 2)

#define TEST_FACTOR1(name, filter, nom, denom, max_diff)                     \
  TEST_F(LibYUVScaleTest, ScaleDownBy##name##_##filter) {                    \
    int diff = TestFilter(                                                   \
        SX(benchmark_width_, nom, denom), SX(benchmark_height_, nom, denom), \
        DX(benchmark_width_, nom, denom), DX(benchmark_height_, nom, denom), \
        kFilter##filter, benchmark_iterations_, disable_cpu_flags_,          \
        benchmark_cpu_info_);                                                \
    EXPECT_LE(diff, max_diff);                                               \
  }                                                                          \
  TEST_F(LibYUVScaleTest, DISABLED_ScaleDownBy##name##_##filter##_16) {      \
    int diff = TestFilter_16(                                                \
        SX(benchmark_width_, nom, denom), SX(benchmark_height_, nom, denom), \
        DX(benchmark_width_, nom, denom), DX(benchmark_height_, nom, denom), \
        kFilter##filter, benchmark_iterations_);                             \
    EXPECT_LE(diff, max_diff);                                               \
  }

// Test a scale factor with all 4 filters.  Expect unfiltered to be exact, but
// filtering is different fixed point implementations for SSSE3, Neon and C.
#define TEST_FACTOR(name, nom, denom, boxdiff) \
  TEST_FACTOR1(name, None, nom, denom, 0)      \
  TEST_FACTOR1(name, Linear, nom, denom, 3)    \
  TEST_FACTOR1(name, Bilinear, nom, denom, 3)  \
  TEST_FACTOR1(name, Box, nom, denom, boxdiff)

TEST_FACTOR(2, 1, 2, 0)
TEST_FACTOR(4, 1, 4, 0)
TEST_FACTOR(8, 1, 8, 0)
TEST_FACTOR(3by4, 3, 4, 1)
TEST_FACTOR(3by8, 3, 8, 1)
TEST_FACTOR(3, 1, 3, 0)
#undef TEST_FACTOR1
#undef TEST_FACTOR
#undef SX
#undef DX

#define TEST_SCALETO1(name, width, height, filter, max_diff)                  \
  TEST_F(LibYUVScaleTest, name##To##width##x##height##_##filter) {            \
    int diff = TestFilter(benchmark_width_, benchmark_height_, width, height, \
                          kFilter##filter, benchmark_iterations_,             \
                          disable_cpu_flags_, benchmark_cpu_info_);           \
    EXPECT_LE(diff, max_diff);                                                \
  }                                                                           \
  TEST_F(LibYUVScaleTest, name##From##width##x##height##_##filter) {          \
    int diff = TestFilter(width, height, Abs(benchmark_width_),               \
                          Abs(benchmark_height_), kFilter##filter,            \
                          benchmark_iterations_, disable_cpu_flags_,          \
                          benchmark_cpu_info_);                               \
    EXPECT_LE(diff, max_diff);                                                \
  }                                                                           \
  TEST_F(LibYUVScaleTest,                                                     \
         DISABLED_##name##To##width##x##height##_##filter##_16) {             \
    int diff = TestFilter_16(benchmark_width_, benchmark_height_, width,      \
                             height, kFilter##filter, benchmark_iterations_); \
    EXPECT_LE(diff, max_diff);                                                \
  }                                                                           \
  TEST_F(LibYUVScaleTest,                                                     \
         DISABLED_##name##From##width##x##height##_##filter##_16) {           \
    int diff = TestFilter_16(width, height, Abs(benchmark_width_),            \
                             Abs(benchmark_height_), kFilter##filter,         \
                             benchmark_iterations_);                          \
    EXPECT_LE(diff, max_diff);                                                \
  }

// Test scale to a specified size with all 4 filters.
#define TEST_SCALETO(name, width, height)         \
  TEST_SCALETO1(name, width, height, None, 0)     \
  TEST_SCALETO1(name, width, height, Linear, 0)   \
  TEST_SCALETO1(name, width, height, Bilinear, 0) \
  TEST_SCALETO1(name, width, height, Box, 0)

TEST_SCALETO(Scale, 1, 1)
TEST_SCALETO(Scale, 320, 240)
TEST_SCALETO(Scale, 352, 288)
TEST_SCALETO(Scale, 569, 480)
TEST_SCALETO(Scale, 640, 360)
TEST_SCALETO(Scale, 1280, 720)
#undef TEST_SCALETO1
#undef TEST_SCALETO

}  // namespace libyuv
