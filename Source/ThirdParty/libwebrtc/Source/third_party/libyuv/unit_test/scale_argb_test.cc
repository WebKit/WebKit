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
#include "libyuv/convert_argb.h"
#include "libyuv/cpu_id.h"
#include "libyuv/scale_argb.h"
#include "libyuv/video_common.h"

namespace libyuv {

#define STRINGIZE(line) #line
#define FILELINESTR(file, line) file ":" STRINGIZE(line)

// Test scaling with C vs Opt and return maximum pixel difference. 0 = exact.
static int ARGBTestFilter(int src_width,
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
  int64_t src_argb_plane_size =
      (Abs(src_width) + b * 2) * (Abs(src_height) + b * 2) * 4LL;
  int src_stride_argb = (b * 2 + Abs(src_width)) * 4;

  align_buffer_page_end(src_argb, src_argb_plane_size);
  if (!src_argb) {
    printf("Skipped.  Alloc failed " FILELINESTR(__FILE__, __LINE__) "\n");
    return 0;
  }
  MemRandomize(src_argb, src_argb_plane_size);

  int64_t dst_argb_plane_size =
      (dst_width + b * 2) * (dst_height + b * 2) * 4LL;
  int dst_stride_argb = (b * 2 + dst_width) * 4;

  align_buffer_page_end(dst_argb_c, dst_argb_plane_size);
  align_buffer_page_end(dst_argb_opt, dst_argb_plane_size);
  if (!dst_argb_c || !dst_argb_opt) {
    printf("Skipped.  Alloc failed " FILELINESTR(__FILE__, __LINE__) "\n");
    return 0;
  }
  memset(dst_argb_c, 2, dst_argb_plane_size);
  memset(dst_argb_opt, 3, dst_argb_plane_size);

  // Warm up both versions for consistent benchmarks.
  MaskCpuFlags(disable_cpu_flags);  // Disable all CPU optimization.
  ARGBScale(src_argb + (src_stride_argb * b) + b * 4, src_stride_argb,
            src_width, src_height, dst_argb_c + (dst_stride_argb * b) + b * 4,
            dst_stride_argb, dst_width, dst_height, f);
  MaskCpuFlags(benchmark_cpu_info);  // Enable all CPU optimization.
  ARGBScale(src_argb + (src_stride_argb * b) + b * 4, src_stride_argb,
            src_width, src_height, dst_argb_opt + (dst_stride_argb * b) + b * 4,
            dst_stride_argb, dst_width, dst_height, f);

  MaskCpuFlags(disable_cpu_flags);  // Disable all CPU optimization.
  double c_time = get_time();
  ARGBScale(src_argb + (src_stride_argb * b) + b * 4, src_stride_argb,
            src_width, src_height, dst_argb_c + (dst_stride_argb * b) + b * 4,
            dst_stride_argb, dst_width, dst_height, f);

  c_time = (get_time() - c_time);

  MaskCpuFlags(benchmark_cpu_info);  // Enable all CPU optimization.
  double opt_time = get_time();
  for (i = 0; i < benchmark_iterations; ++i) {
    ARGBScale(src_argb + (src_stride_argb * b) + b * 4, src_stride_argb,
              src_width, src_height,
              dst_argb_opt + (dst_stride_argb * b) + b * 4, dst_stride_argb,
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
    for (j = b * 4; j < (dst_width + b) * 4; ++j) {
      int abs_diff = Abs(dst_argb_c[(i * dst_stride_argb) + j] -
                         dst_argb_opt[(i * dst_stride_argb) + j]);
      if (abs_diff > max_diff) {
        max_diff = abs_diff;
      }
    }
  }

  free_aligned_buffer_page_end(dst_argb_c);
  free_aligned_buffer_page_end(dst_argb_opt);
  free_aligned_buffer_page_end(src_argb);
  return max_diff;
}

static const int kTileX = 64;
static const int kTileY = 64;

static int TileARGBScale(const uint8_t* src_argb,
                         int src_stride_argb,
                         int src_width,
                         int src_height,
                         uint8_t* dst_argb,
                         int dst_stride_argb,
                         int dst_width,
                         int dst_height,
                         FilterMode filtering) {
  for (int y = 0; y < dst_height; y += kTileY) {
    for (int x = 0; x < dst_width; x += kTileX) {
      int clip_width = kTileX;
      if (x + clip_width > dst_width) {
        clip_width = dst_width - x;
      }
      int clip_height = kTileY;
      if (y + clip_height > dst_height) {
        clip_height = dst_height - y;
      }
      int r = ARGBScaleClip(src_argb, src_stride_argb, src_width, src_height,
                            dst_argb, dst_stride_argb, dst_width, dst_height, x,
                            y, clip_width, clip_height, filtering);
      if (r) {
        return r;
      }
    }
  }
  return 0;
}

static int ARGBClipTestFilter(int src_width,
                              int src_height,
                              int dst_width,
                              int dst_height,
                              FilterMode f,
                              int benchmark_iterations) {
  if (!SizeValid(src_width, src_height, dst_width, dst_height)) {
    return 0;
  }

  const int b = 128;
  int64_t src_argb_plane_size =
      (Abs(src_width) + b * 2) * (Abs(src_height) + b * 2) * 4;
  int src_stride_argb = (b * 2 + Abs(src_width)) * 4;

  align_buffer_page_end(src_argb, src_argb_plane_size);
  if (!src_argb) {
    printf("Skipped.  Alloc failed " FILELINESTR(__FILE__, __LINE__) "\n");
    return 0;
  }
  memset(src_argb, 1, src_argb_plane_size);

  int64_t dst_argb_plane_size = (dst_width + b * 2) * (dst_height + b * 2) * 4;
  int dst_stride_argb = (b * 2 + dst_width) * 4;

  int i, j;
  for (i = b; i < (Abs(src_height) + b); ++i) {
    for (j = b; j < (Abs(src_width) + b) * 4; ++j) {
      src_argb[(i * src_stride_argb) + j] = (fastrand() & 0xff);
    }
  }

  align_buffer_page_end(dst_argb_c, dst_argb_plane_size);
  align_buffer_page_end(dst_argb_opt, dst_argb_plane_size);
  if (!dst_argb_c || !dst_argb_opt) {
    printf("Skipped.  Alloc failed " FILELINESTR(__FILE__, __LINE__) "\n");
    return 0;
  }
  memset(dst_argb_c, 2, dst_argb_plane_size);
  memset(dst_argb_opt, 3, dst_argb_plane_size);

  // Do full image, no clipping.
  double c_time = get_time();
  ARGBScale(src_argb + (src_stride_argb * b) + b * 4, src_stride_argb,
            src_width, src_height, dst_argb_c + (dst_stride_argb * b) + b * 4,
            dst_stride_argb, dst_width, dst_height, f);
  c_time = (get_time() - c_time);

  // Do tiled image, clipping scale to a tile at a time.
  double opt_time = get_time();
  for (i = 0; i < benchmark_iterations; ++i) {
    TileARGBScale(src_argb + (src_stride_argb * b) + b * 4, src_stride_argb,
                  src_width, src_height,
                  dst_argb_opt + (dst_stride_argb * b) + b * 4, dst_stride_argb,
                  dst_width, dst_height, f);
  }
  opt_time = (get_time() - opt_time) / benchmark_iterations;

  // Report performance of Full vs Tiled.
  printf("filter %d - %8d us Full - %8d us Tiled\n", f,
         static_cast<int>(c_time * 1e6), static_cast<int>(opt_time * 1e6));

  // Compare full scaled image vs tiled image.
  int max_diff = 0;
  for (i = b; i < (dst_height + b); ++i) {
    for (j = b * 4; j < (dst_width + b) * 4; ++j) {
      int abs_diff = Abs(dst_argb_c[(i * dst_stride_argb) + j] -
                         dst_argb_opt[(i * dst_stride_argb) + j]);
      if (abs_diff > max_diff) {
        max_diff = abs_diff;
      }
    }
  }

  free_aligned_buffer_page_end(dst_argb_c);
  free_aligned_buffer_page_end(dst_argb_opt);
  free_aligned_buffer_page_end(src_argb);
  return max_diff;
}

// The following adjustments in dimensions ensure the scale factor will be
// exactly achieved.
#define DX(x, nom, denom) static_cast<int>((Abs(x) / nom) * nom)
#define SX(x, nom, denom) static_cast<int>((x / nom) * denom)

#define TEST_FACTOR1(DISABLED_, name, filter, nom, denom, max_diff)          \
  TEST_F(LibYUVScaleTest, ARGBScaleDownBy##name##_##filter) {                \
    int diff = ARGBTestFilter(                                               \
        SX(benchmark_width_, nom, denom), SX(benchmark_height_, nom, denom), \
        DX(benchmark_width_, nom, denom), DX(benchmark_height_, nom, denom), \
        kFilter##filter, benchmark_iterations_, disable_cpu_flags_,          \
        benchmark_cpu_info_);                                                \
    EXPECT_LE(diff, max_diff);                                               \
  }                                                                          \
  TEST_F(LibYUVScaleTest, DISABLED_##ARGBScaleDownClipBy##name##_##filter) { \
    int diff = ARGBClipTestFilter(                                           \
        SX(benchmark_width_, nom, denom), SX(benchmark_height_, nom, denom), \
        DX(benchmark_width_, nom, denom), DX(benchmark_height_, nom, denom), \
        kFilter##filter, benchmark_iterations_);                             \
    EXPECT_LE(diff, max_diff);                                               \
  }

// Test a scale factor with all 4 filters.  Expect unfiltered to be exact, but
// filtering is different fixed point implementations for SSSE3, Neon and C.
#ifdef ENABLE_SLOW_TESTS
#define TEST_FACTOR(name, nom, denom)           \
  TEST_FACTOR1(, name, None, nom, denom, 0)     \
  TEST_FACTOR1(, name, Linear, nom, denom, 3)   \
  TEST_FACTOR1(, name, Bilinear, nom, denom, 3) \
  TEST_FACTOR1(, name, Box, nom, denom, 3)
#else
#define TEST_FACTOR(name, nom, denom)                    \
  TEST_FACTOR1(DISABLED_, name, None, nom, denom, 0)     \
  TEST_FACTOR1(DISABLED_, name, Linear, nom, denom, 3)   \
  TEST_FACTOR1(DISABLED_, name, Bilinear, nom, denom, 3) \
  TEST_FACTOR1(DISABLED_, name, Box, nom, denom, 3)
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

#define TEST_SCALETO1(DISABLED_, name, width, height, filter, max_diff)        \
  TEST_F(LibYUVScaleTest, name##To##width##x##height##_##filter) {             \
    int diff = ARGBTestFilter(benchmark_width_, benchmark_height_, width,      \
                              height, kFilter##filter, benchmark_iterations_,  \
                              disable_cpu_flags_, benchmark_cpu_info_);        \
    EXPECT_LE(diff, max_diff);                                                 \
  }                                                                            \
  TEST_F(LibYUVScaleTest, name##From##width##x##height##_##filter) {           \
    int diff = ARGBTestFilter(width, height, Abs(benchmark_width_),            \
                              Abs(benchmark_height_), kFilter##filter,         \
                              benchmark_iterations_, disable_cpu_flags_,       \
                              benchmark_cpu_info_);                            \
    EXPECT_LE(diff, max_diff);                                                 \
  }                                                                            \
  TEST_F(LibYUVScaleTest,                                                      \
         DISABLED_##name##ClipTo##width##x##height##_##filter) {               \
    int diff =                                                                 \
        ARGBClipTestFilter(benchmark_width_, benchmark_height_, width, height, \
                           kFilter##filter, benchmark_iterations_);            \
    EXPECT_LE(diff, max_diff);                                                 \
  }                                                                            \
  TEST_F(LibYUVScaleTest,                                                      \
         DISABLED_##name##ClipFrom##width##x##height##_##filter) {             \
    int diff = ARGBClipTestFilter(width, height, Abs(benchmark_width_),        \
                                  Abs(benchmark_height_), kFilter##filter,     \
                                  benchmark_iterations_);                      \
    EXPECT_LE(diff, max_diff);                                                 \
  }

/// Test scale to a specified size with all 4 filters.
#ifdef ENABLE_SLOW_TESTS
#define TEST_SCALETO(name, width, height)         \
  TEST_SCALETO1(, name, width, height, None, 0)   \
  TEST_SCALETO1(, name, width, height, Linear, 3) \
  TEST_SCALETO1(, name, width, height, Bilinear, 3)
#else
#define TEST_SCALETO(name, width, height)                  \
  TEST_SCALETO1(DISABLED_, name, width, height, None, 0)   \
  TEST_SCALETO1(DISABLED_, name, width, height, Linear, 3) \
  TEST_SCALETO1(DISABLED_, name, width, height, Bilinear, 3)
#endif

TEST_SCALETO(ARGBScale, 1, 1)
TEST_SCALETO(ARGBScale, 256, 144) /* 128x72 * 2 */
TEST_SCALETO(ARGBScale, 320, 240)
TEST_SCALETO(ARGBScale, 569, 480)
TEST_SCALETO(ARGBScale, 640, 360)
#ifdef ENABLE_SLOW_TESTS
TEST_SCALETO(ARGBScale, 1280, 720)
TEST_SCALETO(ARGBScale, 1920, 1080)
#endif  // ENABLE_SLOW_TESTS
#undef TEST_SCALETO1
#undef TEST_SCALETO

#define TEST_SCALESWAPXY1(name, filter, max_diff)                       \
  TEST_F(LibYUVScaleTest, name##SwapXY_##filter) {                      \
    int diff = ARGBTestFilter(benchmark_width_, benchmark_height_,      \
                              benchmark_height_, benchmark_width_,      \
                              kFilter##filter, benchmark_iterations_,   \
                              disable_cpu_flags_, benchmark_cpu_info_); \
    EXPECT_LE(diff, max_diff);                                          \
  }

// Test scale with swapped width and height with all 3 filters.
TEST_SCALESWAPXY1(ARGBScale, None, 0)
TEST_SCALESWAPXY1(ARGBScale, Linear, 0)
TEST_SCALESWAPXY1(ARGBScale, Bilinear, 0)
#undef TEST_SCALESWAPXY1

// Scale with YUV conversion to ARGB and clipping.
// TODO(fbarchard): Add fourcc support.  All 4 ARGB formats is easy to support.
LIBYUV_API
int YUVToARGBScaleReference2(const uint8_t* src_y,
                             int src_stride_y,
                             const uint8_t* src_u,
                             int src_stride_u,
                             const uint8_t* src_v,
                             int src_stride_v,
                             uint32_t /* src_fourcc */,
                             int src_width,
                             int src_height,
                             uint8_t* dst_argb,
                             int dst_stride_argb,
                             uint32_t /* dst_fourcc */,
                             int dst_width,
                             int dst_height,
                             int clip_x,
                             int clip_y,
                             int clip_width,
                             int clip_height,
                             enum FilterMode filtering) {
  uint8_t* argb_buffer =
      static_cast<uint8_t*>(malloc(src_width * src_height * 4));
  int r;
  I420ToARGB(src_y, src_stride_y, src_u, src_stride_u, src_v, src_stride_v,
             argb_buffer, src_width * 4, src_width, src_height);

  r = ARGBScaleClip(argb_buffer, src_width * 4, src_width, src_height, dst_argb,
                    dst_stride_argb, dst_width, dst_height, clip_x, clip_y,
                    clip_width, clip_height, filtering);
  free(argb_buffer);
  return r;
}

static void FillRamp(uint8_t* buf,
                     int width,
                     int height,
                     int v,
                     int dx,
                     int dy) {
  int rv = v;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      *buf++ = v;
      v += dx;
      if (v < 0 || v > 255) {
        dx = -dx;
        v += dx;
      }
    }
    v = rv + dy;
    if (v < 0 || v > 255) {
      dy = -dy;
      v += dy;
    }
    rv = v;
  }
}

// Test scaling with C vs Opt and return maximum pixel difference. 0 = exact.
static int YUVToARGBTestFilter(int src_width,
                               int src_height,
                               int dst_width,
                               int dst_height,
                               FilterMode f,
                               int benchmark_iterations) {
  int64_t src_y_plane_size = Abs(src_width) * Abs(src_height);
  int64_t src_uv_plane_size =
      ((Abs(src_width) + 1) / 2) * ((Abs(src_height) + 1) / 2);
  int src_stride_y = Abs(src_width);
  int src_stride_uv = (Abs(src_width) + 1) / 2;

  align_buffer_page_end(src_y, src_y_plane_size);
  align_buffer_page_end(src_u, src_uv_plane_size);
  align_buffer_page_end(src_v, src_uv_plane_size);

  int64_t dst_argb_plane_size = (dst_width) * (dst_height)*4LL;
  int dst_stride_argb = (dst_width)*4;
  align_buffer_page_end(dst_argb_c, dst_argb_plane_size);
  align_buffer_page_end(dst_argb_opt, dst_argb_plane_size);
  if (!dst_argb_c || !dst_argb_opt || !src_y || !src_u || !src_v) {
    printf("Skipped.  Alloc failed " FILELINESTR(__FILE__, __LINE__) "\n");
    return 0;
  }
  // Fill YUV image with continuous ramp, which is less sensitive to
  // subsampling and filtering differences for test purposes.
  FillRamp(src_y, Abs(src_width), Abs(src_height), 128, 1, 1);
  FillRamp(src_u, (Abs(src_width) + 1) / 2, (Abs(src_height) + 1) / 2, 3, 1, 1);
  FillRamp(src_v, (Abs(src_width) + 1) / 2, (Abs(src_height) + 1) / 2, 4, 1, 1);
  memset(dst_argb_c, 2, dst_argb_plane_size);
  memset(dst_argb_opt, 3, dst_argb_plane_size);

  YUVToARGBScaleReference2(src_y, src_stride_y, src_u, src_stride_uv, src_v,
                           src_stride_uv, libyuv::FOURCC_I420, src_width,
                           src_height, dst_argb_c, dst_stride_argb,
                           libyuv::FOURCC_I420, dst_width, dst_height, 0, 0,
                           dst_width, dst_height, f);

  for (int i = 0; i < benchmark_iterations; ++i) {
    YUVToARGBScaleClip(src_y, src_stride_y, src_u, src_stride_uv, src_v,
                       src_stride_uv, libyuv::FOURCC_I420, src_width,
                       src_height, dst_argb_opt, dst_stride_argb,
                       libyuv::FOURCC_I420, dst_width, dst_height, 0, 0,
                       dst_width, dst_height, f);
  }
  int max_diff = 0;
  for (int i = 0; i < dst_height; ++i) {
    for (int j = 0; j < dst_width * 4; ++j) {
      int abs_diff = Abs(dst_argb_c[(i * dst_stride_argb) + j] -
                         dst_argb_opt[(i * dst_stride_argb) + j]);
      if (abs_diff > max_diff) {
        printf("error %d at %d,%d c %d opt %d", abs_diff, j, i,
               dst_argb_c[(i * dst_stride_argb) + j],
               dst_argb_opt[(i * dst_stride_argb) + j]);
        EXPECT_LE(abs_diff, 40);
        max_diff = abs_diff;
      }
    }
  }

  free_aligned_buffer_page_end(dst_argb_c);
  free_aligned_buffer_page_end(dst_argb_opt);
  free_aligned_buffer_page_end(src_y);
  free_aligned_buffer_page_end(src_u);
  free_aligned_buffer_page_end(src_v);
  return max_diff;
}

TEST_F(LibYUVScaleTest, YUVToRGBScaleUp) {
  int diff =
      YUVToARGBTestFilter(benchmark_width_, benchmark_height_,
                          benchmark_width_ * 3 / 2, benchmark_height_ * 3 / 2,
                          libyuv::kFilterBilinear, benchmark_iterations_);
  EXPECT_LE(diff, 10);
}

TEST_F(LibYUVScaleTest, YUVToRGBScaleDown) {
  int diff = YUVToARGBTestFilter(
      benchmark_width_ * 3 / 2, benchmark_height_ * 3 / 2, benchmark_width_,
      benchmark_height_, libyuv::kFilterBilinear, benchmark_iterations_);
  EXPECT_LE(diff, 10);
}

TEST_F(LibYUVScaleTest, ARGBTest3x) {
  const int kSrcStride = 48 * 4;
  const int kDstStride = 16 * 4;
  const int kSize = kSrcStride * 3;
  align_buffer_page_end(orig_pixels, kSize);
  for (int i = 0; i < 48 * 3; ++i) {
    orig_pixels[i * 4 + 0] = i;
    orig_pixels[i * 4 + 1] = 255 - i;
    orig_pixels[i * 4 + 2] = i + 1;
    orig_pixels[i * 4 + 3] = i + 10;
  }
  align_buffer_page_end(dest_pixels, kDstStride);

  int iterations16 =
      benchmark_width_ * benchmark_height_ / (16 * 1) * benchmark_iterations_;
  for (int i = 0; i < iterations16; ++i) {
    ARGBScale(orig_pixels, kSrcStride, 48, 3, dest_pixels, kDstStride, 16, 1,
              kFilterBilinear);
  }

  EXPECT_EQ(49, dest_pixels[0]);
  EXPECT_EQ(255 - 49, dest_pixels[1]);
  EXPECT_EQ(50, dest_pixels[2]);
  EXPECT_EQ(59, dest_pixels[3]);

  ARGBScale(orig_pixels, kSrcStride, 48, 3, dest_pixels, kDstStride, 16, 1,
            kFilterNone);

  EXPECT_EQ(49, dest_pixels[0]);
  EXPECT_EQ(255 - 49, dest_pixels[1]);
  EXPECT_EQ(50, dest_pixels[2]);
  EXPECT_EQ(59, dest_pixels[3]);

  free_aligned_buffer_page_end(dest_pixels);
  free_aligned_buffer_page_end(orig_pixels);
}

TEST_F(LibYUVScaleTest, ARGBTest4x) {
  const int kSrcStride = 64 * 4;
  const int kDstStride = 16 * 4;
  const int kSize = kSrcStride * 4;
  align_buffer_page_end(orig_pixels, kSize);
  for (int i = 0; i < 64 * 4; ++i) {
    orig_pixels[i * 4 + 0] = i;
    orig_pixels[i * 4 + 1] = 255 - i;
    orig_pixels[i * 4 + 2] = i + 1;
    orig_pixels[i * 4 + 3] = i + 10;
  }
  align_buffer_page_end(dest_pixels, kDstStride);

  int iterations16 =
      benchmark_width_ * benchmark_height_ / (16 * 1) * benchmark_iterations_;
  for (int i = 0; i < iterations16; ++i) {
    ARGBScale(orig_pixels, kSrcStride, 64, 4, dest_pixels, kDstStride, 16, 1,
              kFilterBilinear);
  }

  EXPECT_NEAR((65 + 66 + 129 + 130 + 2) / 4, dest_pixels[0], 4);
  EXPECT_NEAR((255 - 65 + 255 - 66 + 255 - 129 + 255 - 130 + 2) / 4,
              dest_pixels[1], 4);
  EXPECT_NEAR((1 * 4 + 65 + 66 + 129 + 130 + 2) / 4, dest_pixels[2], 4);
  EXPECT_NEAR((10 * 4 + 65 + 66 + 129 + 130 + 2) / 4, dest_pixels[3], 4);

  ARGBScale(orig_pixels, kSrcStride, 64, 4, dest_pixels, kDstStride, 16, 1,
            kFilterNone);

  EXPECT_EQ(130, dest_pixels[0]);
  EXPECT_EQ(255 - 130, dest_pixels[1]);
  EXPECT_EQ(130 + 1, dest_pixels[2]);
  EXPECT_EQ(130 + 10, dest_pixels[3]);

  free_aligned_buffer_page_end(dest_pixels);
  free_aligned_buffer_page_end(orig_pixels);
}

}  // namespace libyuv
