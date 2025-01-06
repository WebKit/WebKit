/*
 *  Copyright 2023 The LibYuv Project Authors. All rights reserved.
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

#ifdef ENABLE_ROW_TESTS
#include "libyuv/scale_row.h"  // For ScaleRowDown2Box_Odd_C
#endif

#define STRINGIZE(line) #line
#define FILELINESTR(file, line) file ":" STRINGIZE(line)

#if defined(__riscv) && !defined(__clang__)
#define DISABLE_SLOW_TESTS
#undef ENABLE_FULL_TESTS
#undef ENABLE_ROW_TESTS
#define LEAN_TESTS
#endif

#if !defined(DISABLE_SLOW_TESTS) || defined(__x86_64__) || defined(__i386__)
// SLOW TESTS are those that are unoptimized C code.
// FULL TESTS are optimized but test many variations of the same code.
#define ENABLE_FULL_TESTS
#endif

namespace libyuv {

#ifdef ENABLE_ROW_TESTS
#ifdef HAS_SCALEROWDOWN2_SSSE3
TEST_F(LibYUVScaleTest, TestScaleRowDown2Box_Odd_SSSE3) {
  SIMD_ALIGNED(uint8_t orig_pixels[128 * 2]);
  SIMD_ALIGNED(uint8_t dst_pixels_opt[64]);
  SIMD_ALIGNED(uint8_t dst_pixels_c[64]);
  memset(orig_pixels, 0, sizeof(orig_pixels));
  memset(dst_pixels_opt, 0, sizeof(dst_pixels_opt));
  memset(dst_pixels_c, 0, sizeof(dst_pixels_c));

  int has_ssse3 = TestCpuFlag(kCpuHasSSSE3);
  if (!has_ssse3) {
    printf("Warning SSSE3 not detected; Skipping test.\n");
  } else {
    // TL.
    orig_pixels[0] = 255u;
    orig_pixels[1] = 0u;
    orig_pixels[128 + 0] = 0u;
    orig_pixels[128 + 1] = 0u;
    // TR.
    orig_pixels[2] = 0u;
    orig_pixels[3] = 100u;
    orig_pixels[128 + 2] = 0u;
    orig_pixels[128 + 3] = 0u;
    // BL.
    orig_pixels[4] = 0u;
    orig_pixels[5] = 0u;
    orig_pixels[128 + 4] = 50u;
    orig_pixels[128 + 5] = 0u;
    // BR.
    orig_pixels[6] = 0u;
    orig_pixels[7] = 0u;
    orig_pixels[128 + 6] = 0u;
    orig_pixels[128 + 7] = 20u;
    // Odd.
    orig_pixels[126] = 4u;
    orig_pixels[127] = 255u;
    orig_pixels[128 + 126] = 16u;
    orig_pixels[128 + 127] = 255u;

    // Test regular half size.
    ScaleRowDown2Box_C(orig_pixels, 128, dst_pixels_c, 64);

    EXPECT_EQ(64u, dst_pixels_c[0]);
    EXPECT_EQ(25u, dst_pixels_c[1]);
    EXPECT_EQ(13u, dst_pixels_c[2]);
    EXPECT_EQ(5u, dst_pixels_c[3]);
    EXPECT_EQ(0u, dst_pixels_c[4]);
    EXPECT_EQ(133u, dst_pixels_c[63]);

    // Test Odd width version - Last pixel is just 1 horizontal pixel.
    ScaleRowDown2Box_Odd_C(orig_pixels, 128, dst_pixels_c, 64);

    EXPECT_EQ(64u, dst_pixels_c[0]);
    EXPECT_EQ(25u, dst_pixels_c[1]);
    EXPECT_EQ(13u, dst_pixels_c[2]);
    EXPECT_EQ(5u, dst_pixels_c[3]);
    EXPECT_EQ(0u, dst_pixels_c[4]);
    EXPECT_EQ(10u, dst_pixels_c[63]);

    // Test one pixel less, should skip the last pixel.
    memset(dst_pixels_c, 0, sizeof(dst_pixels_c));
    ScaleRowDown2Box_Odd_C(orig_pixels, 128, dst_pixels_c, 63);

    EXPECT_EQ(64u, dst_pixels_c[0]);
    EXPECT_EQ(25u, dst_pixels_c[1]);
    EXPECT_EQ(13u, dst_pixels_c[2]);
    EXPECT_EQ(5u, dst_pixels_c[3]);
    EXPECT_EQ(0u, dst_pixels_c[4]);
    EXPECT_EQ(0u, dst_pixels_c[63]);

    // Test regular half size SSSE3.
    ScaleRowDown2Box_SSSE3(orig_pixels, 128, dst_pixels_opt, 64);

    EXPECT_EQ(64u, dst_pixels_opt[0]);
    EXPECT_EQ(25u, dst_pixels_opt[1]);
    EXPECT_EQ(13u, dst_pixels_opt[2]);
    EXPECT_EQ(5u, dst_pixels_opt[3]);
    EXPECT_EQ(0u, dst_pixels_opt[4]);
    EXPECT_EQ(133u, dst_pixels_opt[63]);

    // Compare C and SSSE3 match.
    ScaleRowDown2Box_Odd_C(orig_pixels, 128, dst_pixels_c, 64);
    ScaleRowDown2Box_Odd_SSSE3(orig_pixels, 128, dst_pixels_opt, 64);
    for (int i = 0; i < 64; ++i) {
      EXPECT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
    }
  }
}
#endif  // HAS_SCALEROWDOWN2_SSSE3

TEST_F(LibYUVScaleTest, TestScaleRowDown2Box_16) {
  SIMD_ALIGNED(uint16_t orig_pixels[2560 * 2]);
  SIMD_ALIGNED(uint16_t dst_pixels_c[1280]);
  SIMD_ALIGNED(uint16_t dst_pixels_opt[1280]);

  memset(orig_pixels, 0, sizeof(orig_pixels));
  memset(dst_pixels_c, 1, sizeof(dst_pixels_c));
  memset(dst_pixels_opt, 2, sizeof(dst_pixels_opt));

  for (int i = 0; i < 2560 * 2; ++i) {
    orig_pixels[i] = i;
  }
  ScaleRowDown2Box_16_C(&orig_pixels[0], 2560, &dst_pixels_c[0], 1280);
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
#if !defined(LIBYUV_DISABLE_NEON) && defined(__aarch64__)
    int has_neon = TestCpuFlag(kCpuHasNEON);
    if (has_neon) {
      ScaleRowDown2Box_16_NEON(&orig_pixels[0], 2560, &dst_pixels_opt[0], 1280);
    } else {
      ScaleRowDown2Box_16_C(&orig_pixels[0], 2560, &dst_pixels_opt[0], 1280);
    }
#else
    ScaleRowDown2Box_16_C(&orig_pixels[0], 2560, &dst_pixels_opt[0], 1280);
#endif
  }

  for (int i = 0; i < 1280; ++i) {
    EXPECT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
  }

  EXPECT_EQ(dst_pixels_c[0], (0 + 1 + 2560 + 2561 + 2) / 4);
  EXPECT_EQ(dst_pixels_c[1279], 3839);
}
#endif  // ENABLE_ROW_TESTS

// Test scaling plane with 8 bit C vs 12 bit C and return maximum pixel
// difference.
// 0 = exact.
static int TestPlaneFilter_16(int src_width,
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
  int64_t src_y_plane_size = (Abs(src_width)) * (Abs(src_height));
  int src_stride_y = Abs(src_width);
  int dst_y_plane_size = dst_width * dst_height;
  int dst_stride_y = dst_width;

  align_buffer_page_end(src_y, src_y_plane_size);
  align_buffer_page_end(src_y_16, src_y_plane_size * 2);
  align_buffer_page_end(dst_y_8, dst_y_plane_size);
  align_buffer_page_end(dst_y_16, dst_y_plane_size * 2);
  uint16_t* p_src_y_16 = reinterpret_cast<uint16_t*>(src_y_16);
  uint16_t* p_dst_y_16 = reinterpret_cast<uint16_t*>(dst_y_16);

  MemRandomize(src_y, src_y_plane_size);
  memset(dst_y_8, 0, dst_y_plane_size);
  memset(dst_y_16, 1, dst_y_plane_size * 2);

  for (i = 0; i < src_y_plane_size; ++i) {
    p_src_y_16[i] = src_y[i] & 255;
  }

  MaskCpuFlags(disable_cpu_flags);  // Disable all CPU optimization.
  ScalePlane(src_y, src_stride_y, src_width, src_height, dst_y_8, dst_stride_y,
             dst_width, dst_height, f);
  MaskCpuFlags(benchmark_cpu_info);  // Enable all CPU optimization.

  for (i = 0; i < benchmark_iterations; ++i) {
    ScalePlane_16(p_src_y_16, src_stride_y, src_width, src_height, p_dst_y_16,
                  dst_stride_y, dst_width, dst_height, f);
  }

  // Expect an exact match.
  int max_diff = 0;
  for (i = 0; i < dst_y_plane_size; ++i) {
    int abs_diff = Abs(dst_y_8[i] - p_dst_y_16[i]);
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }

  free_aligned_buffer_page_end(dst_y_8);
  free_aligned_buffer_page_end(dst_y_16);
  free_aligned_buffer_page_end(src_y);
  free_aligned_buffer_page_end(src_y_16);

  return max_diff;
}

// The following adjustments in dimensions ensure the scale factor will be
// exactly achieved.
// 2 is chroma subsample.
#define DX(x, nom, denom) static_cast<int>(((Abs(x) / nom + 1) / 2) * nom * 2)
#define SX(x, nom, denom) static_cast<int>(((x / nom + 1) / 2) * denom * 2)

#define TEST_FACTOR1(name, filter, nom, denom, max_diff)                       \
  TEST_F(LibYUVScaleTest, DISABLED_##ScalePlaneDownBy##name##_##filter##_16) { \
    int diff = TestPlaneFilter_16(                                             \
        SX(benchmark_width_, nom, denom), SX(benchmark_height_, nom, denom),   \
        DX(benchmark_width_, nom, denom), DX(benchmark_height_, nom, denom),   \
        kFilter##filter, benchmark_iterations_, disable_cpu_flags_,            \
        benchmark_cpu_info_);                                                  \
    EXPECT_LE(diff, max_diff);                                                 \
  }

// Test a scale factor with all 4 filters.  Expect unfiltered to be exact, but
// filtering is different fixed point implementations for SSSE3, Neon and C.
#define TEST_FACTOR(name, nom, denom, boxdiff)      \
  TEST_FACTOR1(name, None, nom, denom, 0)           \
  TEST_FACTOR1(name, Linear, nom, denom, boxdiff)   \
  TEST_FACTOR1(name, Bilinear, nom, denom, boxdiff) \
  TEST_FACTOR1(name, Box, nom, denom, boxdiff)

TEST_FACTOR(2, 1, 2, 0)
TEST_FACTOR(4, 1, 4, 0)
// TEST_FACTOR(8, 1, 8, 0) Disable for benchmark performance.  Takes 90 seconds.
TEST_FACTOR(3by4, 3, 4, 1)
TEST_FACTOR(3by8, 3, 8, 1)
TEST_FACTOR(3, 1, 3, 0)
#undef TEST_FACTOR1
#undef TEST_FACTOR
#undef SX
#undef DX

TEST_F(LibYUVScaleTest, PlaneTest3x) {
  const int kSrcStride = 480;
  const int kDstStride = 160;
  const int kSize = kSrcStride * 3;
  align_buffer_page_end(orig_pixels, kSize);
  for (int i = 0; i < 480 * 3; ++i) {
    orig_pixels[i] = i;
  }
  align_buffer_page_end(dest_pixels, kDstStride);

  int iterations160 = (benchmark_width_ * benchmark_height_ + (160 - 1)) / 160 *
                      benchmark_iterations_;
  for (int i = 0; i < iterations160; ++i) {
    ScalePlane(orig_pixels, kSrcStride, 480, 3, dest_pixels, kDstStride, 160, 1,
               kFilterBilinear);
  }

  EXPECT_EQ(225, dest_pixels[0]);

  ScalePlane(orig_pixels, kSrcStride, 480, 3, dest_pixels, kDstStride, 160, 1,
             kFilterNone);

  EXPECT_EQ(225, dest_pixels[0]);

  free_aligned_buffer_page_end(dest_pixels);
  free_aligned_buffer_page_end(orig_pixels);
}

TEST_F(LibYUVScaleTest, PlaneTest4x) {
  const int kSrcStride = 640;
  const int kDstStride = 160;
  const int kSize = kSrcStride * 4;
  align_buffer_page_end(orig_pixels, kSize);
  for (int i = 0; i < 640 * 4; ++i) {
    orig_pixels[i] = i;
  }
  align_buffer_page_end(dest_pixels, kDstStride);

  int iterations160 = (benchmark_width_ * benchmark_height_ + (160 - 1)) / 160 *
                      benchmark_iterations_;
  for (int i = 0; i < iterations160; ++i) {
    ScalePlane(orig_pixels, kSrcStride, 640, 4, dest_pixels, kDstStride, 160, 1,
               kFilterBilinear);
  }

  EXPECT_EQ(66, dest_pixels[0]);

  ScalePlane(orig_pixels, kSrcStride, 640, 4, dest_pixels, kDstStride, 160, 1,
             kFilterNone);

  EXPECT_EQ(2, dest_pixels[0]);  // expect the 3rd pixel of the 3rd row

  free_aligned_buffer_page_end(dest_pixels);
  free_aligned_buffer_page_end(orig_pixels);
}

// Intent is to test 200x50 to 50x200 but width and height can be parameters.
TEST_F(LibYUVScaleTest, PlaneTestRotate_None) {
  const int kSize = benchmark_width_ * benchmark_height_;
  align_buffer_page_end(orig_pixels, kSize);
  for (int i = 0; i < kSize; ++i) {
    orig_pixels[i] = i;
  }
  align_buffer_page_end(dest_opt_pixels, kSize);
  align_buffer_page_end(dest_c_pixels, kSize);

  MaskCpuFlags(disable_cpu_flags_);  // Disable all CPU optimization.
  ScalePlane(orig_pixels, benchmark_width_, benchmark_width_, benchmark_height_,
             dest_c_pixels, benchmark_height_, benchmark_height_,
             benchmark_width_, kFilterNone);
  MaskCpuFlags(benchmark_cpu_info_);  // Enable all CPU optimization.

  for (int i = 0; i < benchmark_iterations_; ++i) {
    ScalePlane(orig_pixels, benchmark_width_, benchmark_width_,
               benchmark_height_, dest_opt_pixels, benchmark_height_,
               benchmark_height_, benchmark_width_, kFilterNone);
  }

  for (int i = 0; i < kSize; ++i) {
    EXPECT_EQ(dest_c_pixels[i], dest_opt_pixels[i]);
  }

  free_aligned_buffer_page_end(dest_c_pixels);
  free_aligned_buffer_page_end(dest_opt_pixels);
  free_aligned_buffer_page_end(orig_pixels);
}

TEST_F(LibYUVScaleTest, PlaneTestRotate_Bilinear) {
  const int kSize = benchmark_width_ * benchmark_height_;
  align_buffer_page_end(orig_pixels, kSize);
  for (int i = 0; i < kSize; ++i) {
    orig_pixels[i] = i;
  }
  align_buffer_page_end(dest_opt_pixels, kSize);
  align_buffer_page_end(dest_c_pixels, kSize);

  MaskCpuFlags(disable_cpu_flags_);  // Disable all CPU optimization.
  ScalePlane(orig_pixels, benchmark_width_, benchmark_width_, benchmark_height_,
             dest_c_pixels, benchmark_height_, benchmark_height_,
             benchmark_width_, kFilterBilinear);
  MaskCpuFlags(benchmark_cpu_info_);  // Enable all CPU optimization.

  for (int i = 0; i < benchmark_iterations_; ++i) {
    ScalePlane(orig_pixels, benchmark_width_, benchmark_width_,
               benchmark_height_, dest_opt_pixels, benchmark_height_,
               benchmark_height_, benchmark_width_, kFilterBilinear);
  }

  for (int i = 0; i < kSize; ++i) {
    EXPECT_EQ(dest_c_pixels[i], dest_opt_pixels[i]);
  }

  free_aligned_buffer_page_end(dest_c_pixels);
  free_aligned_buffer_page_end(dest_opt_pixels);
  free_aligned_buffer_page_end(orig_pixels);
}

// Intent is to test 200x50 to 50x200 but width and height can be parameters.
TEST_F(LibYUVScaleTest, PlaneTestRotate_Box) {
  const int kSize = benchmark_width_ * benchmark_height_;
  align_buffer_page_end(orig_pixels, kSize);
  for (int i = 0; i < kSize; ++i) {
    orig_pixels[i] = i;
  }
  align_buffer_page_end(dest_opt_pixels, kSize);
  align_buffer_page_end(dest_c_pixels, kSize);

  MaskCpuFlags(disable_cpu_flags_);  // Disable all CPU optimization.
  ScalePlane(orig_pixels, benchmark_width_, benchmark_width_, benchmark_height_,
             dest_c_pixels, benchmark_height_, benchmark_height_,
             benchmark_width_, kFilterBox);
  MaskCpuFlags(benchmark_cpu_info_);  // Enable all CPU optimization.

  for (int i = 0; i < benchmark_iterations_; ++i) {
    ScalePlane(orig_pixels, benchmark_width_, benchmark_width_,
               benchmark_height_, dest_opt_pixels, benchmark_height_,
               benchmark_height_, benchmark_width_, kFilterBox);
  }

  for (int i = 0; i < kSize; ++i) {
    EXPECT_EQ(dest_c_pixels[i], dest_opt_pixels[i]);
  }

  free_aligned_buffer_page_end(dest_c_pixels);
  free_aligned_buffer_page_end(dest_opt_pixels);
  free_aligned_buffer_page_end(orig_pixels);
}

TEST_F(LibYUVScaleTest, PlaneTest1_Box) {
  align_buffer_page_end(orig_pixels, 3);
  align_buffer_page_end(dst_pixels, 3);

  // Pad the 1x1 byte image with invalid values before and after in case libyuv
  // reads outside the memory boundaries.
  orig_pixels[0] = 0;
  orig_pixels[1] = 1;  // scale this pixel
  orig_pixels[2] = 2;
  dst_pixels[0] = 3;
  dst_pixels[1] = 3;
  dst_pixels[2] = 3;

  libyuv::ScalePlane(orig_pixels + 1, /* src_stride= */ 1, /* src_width= */ 1,
                     /* src_height= */ 1, dst_pixels, /* dst_stride= */ 1,
                     /* dst_width= */ 1, /* dst_height= */ 2,
                     libyuv::kFilterBox);

  EXPECT_EQ(dst_pixels[0], 1);
  EXPECT_EQ(dst_pixels[1], 1);
  EXPECT_EQ(dst_pixels[2], 3);

  free_aligned_buffer_page_end(dst_pixels);
  free_aligned_buffer_page_end(orig_pixels);
}

TEST_F(LibYUVScaleTest, PlaneTest1_16_Box) {
  align_buffer_page_end(orig_pixels_alloc, 3 * 2);
  align_buffer_page_end(dst_pixels_alloc, 3 * 2);
  uint16_t* orig_pixels = (uint16_t*)orig_pixels_alloc;
  uint16_t* dst_pixels = (uint16_t*)dst_pixels_alloc;

  // Pad the 1x1 byte image with invalid values before and after in case libyuv
  // reads outside the memory boundaries.
  orig_pixels[0] = 0;
  orig_pixels[1] = 1;  // scale this pixel
  orig_pixels[2] = 2;
  dst_pixels[0] = 3;
  dst_pixels[1] = 3;
  dst_pixels[2] = 3;

  libyuv::ScalePlane_16(
      orig_pixels + 1, /* src_stride= */ 1, /* src_width= */ 1,
      /* src_height= */ 1, dst_pixels, /* dst_stride= */ 1,
      /* dst_width= */ 1, /* dst_height= */ 2, libyuv::kFilterNone);

  EXPECT_EQ(dst_pixels[0], 1);
  EXPECT_EQ(dst_pixels[1], 1);
  EXPECT_EQ(dst_pixels[2], 3);

  free_aligned_buffer_page_end(dst_pixels_alloc);
  free_aligned_buffer_page_end(orig_pixels_alloc);
}
}  // namespace libyuv
