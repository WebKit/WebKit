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
  int64 src_argb_plane_size =
      (Abs(src_width) + b * 2) * (Abs(src_height) + b * 2) * 4LL;
  int src_stride_argb = (b * 2 + Abs(src_width)) * 4;

  align_buffer_page_end(src_argb, src_argb_plane_size);
  if (!src_argb) {
    printf("Skipped.  Alloc failed " FILELINESTR(__FILE__, __LINE__) "\n");
    return 0;
  }
  MemRandomize(src_argb, src_argb_plane_size);

  int64 dst_argb_plane_size = (dst_width + b * 2) * (dst_height + b * 2) * 4LL;
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

static const int kTileX = 8;
static const int kTileY = 8;

static int TileARGBScale(const uint8* src_argb,
                         int src_stride_argb,
                         int src_width,
                         int src_height,
                         uint8* dst_argb,
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
  int64 src_argb_plane_size =
      (Abs(src_width) + b * 2) * (Abs(src_height) + b * 2) * 4;
  int src_stride_argb = (b * 2 + Abs(src_width)) * 4;

  align_buffer_page_end(src_argb, src_argb_plane_size);
  if (!src_argb) {
    printf("Skipped.  Alloc failed " FILELINESTR(__FILE__, __LINE__) "\n");
    return 0;
  }
  memset(src_argb, 1, src_argb_plane_size);

  int64 dst_argb_plane_size = (dst_width + b * 2) * (dst_height + b * 2) * 4;
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

#define TEST_FACTOR1(name, filter, nom, denom, max_diff)                     \
  TEST_F(LibYUVScaleTest, ARGBScaleDownBy##name##_##filter) {                \
    int diff = ARGBTestFilter(                                               \
        SX(benchmark_width_, nom, denom), SX(benchmark_height_, nom, denom), \
        DX(benchmark_width_, nom, denom), DX(benchmark_height_, nom, denom), \
        kFilter##filter, benchmark_iterations_, disable_cpu_flags_,          \
        benchmark_cpu_info_);                                                \
    EXPECT_LE(diff, max_diff);                                               \
  }                                                                          \
  TEST_F(LibYUVScaleTest, ARGBScaleDownClipBy##name##_##filter) {            \
    int diff = ARGBClipTestFilter(                                           \
        SX(benchmark_width_, nom, denom), SX(benchmark_height_, nom, denom), \
        DX(benchmark_width_, nom, denom), DX(benchmark_height_, nom, denom), \
        kFilter##filter, benchmark_iterations_);                             \
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
TEST_FACTOR(8, 1, 8)
TEST_FACTOR(3by4, 3, 4)
TEST_FACTOR(3by8, 3, 8)
TEST_FACTOR(3, 1, 3)
#undef TEST_FACTOR1
#undef TEST_FACTOR
#undef SX
#undef DX

#define TEST_SCALETO1(name, width, height, filter, max_diff)                   \
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
  TEST_F(LibYUVScaleTest, name##ClipTo##width##x##height##_##filter) {         \
    int diff =                                                                 \
        ARGBClipTestFilter(benchmark_width_, benchmark_height_, width, height, \
                           kFilter##filter, benchmark_iterations_);            \
    EXPECT_LE(diff, max_diff);                                                 \
  }                                                                            \
  TEST_F(LibYUVScaleTest, name##ClipFrom##width##x##height##_##filter) {       \
    int diff = ARGBClipTestFilter(width, height, Abs(benchmark_width_),        \
                                  Abs(benchmark_height_), kFilter##filter,     \
                                  benchmark_iterations_);                      \
    EXPECT_LE(diff, max_diff);                                                 \
  }

/// Test scale to a specified size with all 4 filters.
#define TEST_SCALETO(name, width, height)       \
  TEST_SCALETO1(name, width, height, None, 0)   \
  TEST_SCALETO1(name, width, height, Linear, 3) \
  TEST_SCALETO1(name, width, height, Bilinear, 3)

TEST_SCALETO(ARGBScale, 1, 1)
TEST_SCALETO(ARGBScale, 320, 240)
TEST_SCALETO(ARGBScale, 352, 288)
TEST_SCALETO(ARGBScale, 569, 480)
TEST_SCALETO(ARGBScale, 640, 360)
TEST_SCALETO(ARGBScale, 1280, 720)
#undef TEST_SCALETO1
#undef TEST_SCALETO

// Scale with YUV conversion to ARGB and clipping.
LIBYUV_API
int YUVToARGBScaleReference2(const uint8* src_y,
                             int src_stride_y,
                             const uint8* src_u,
                             int src_stride_u,
                             const uint8* src_v,
                             int src_stride_v,
                             uint32 /* src_fourcc */,  // TODO: Add support.
                             int src_width,
                             int src_height,
                             uint8* dst_argb,
                             int dst_stride_argb,
                             uint32 /* dst_fourcc */,  // TODO: Add support.
                             int dst_width,
                             int dst_height,
                             int clip_x,
                             int clip_y,
                             int clip_width,
                             int clip_height,
                             enum FilterMode filtering) {
  uint8* argb_buffer = static_cast<uint8*>(malloc(src_width * src_height * 4));
  int r;
  I420ToARGB(src_y, src_stride_y, src_u, src_stride_u, src_v, src_stride_v,
             argb_buffer, src_width * 4, src_width, src_height);

  r = ARGBScaleClip(argb_buffer, src_width * 4, src_width, src_height, dst_argb,
                    dst_stride_argb, dst_width, dst_height, clip_x, clip_y,
                    clip_width, clip_height, filtering);
  free(argb_buffer);
  return r;
}

static void FillRamp(uint8* buf, int width, int height, int v, int dx, int dy) {
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
  int64 src_y_plane_size = Abs(src_width) * Abs(src_height);
  int64 src_uv_plane_size =
      ((Abs(src_width) + 1) / 2) * ((Abs(src_height) + 1) / 2);
  int src_stride_y = Abs(src_width);
  int src_stride_uv = (Abs(src_width) + 1) / 2;

  align_buffer_page_end(src_y, src_y_plane_size);
  align_buffer_page_end(src_u, src_uv_plane_size);
  align_buffer_page_end(src_v, src_uv_plane_size);

  int64 dst_argb_plane_size = (dst_width) * (dst_height)*4LL;
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

}  // namespace libyuv
