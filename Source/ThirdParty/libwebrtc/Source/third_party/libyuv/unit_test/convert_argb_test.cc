/*
 *  Copyright 2023 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <stdlib.h>
#include <time.h>

#include "libyuv/basic_types.h"
#include "libyuv/compare.h"
#include "libyuv/convert.h"
#include "libyuv/convert_argb.h"
#include "libyuv/convert_from.h"
#include "libyuv/convert_from_argb.h"
#include "libyuv/cpu_id.h"
#ifdef HAVE_JPEG
#include "libyuv/mjpeg_decoder.h"
#endif
#include "../unit_test/unit_test.h"
#include "libyuv/planar_functions.h"
#include "libyuv/rotate.h"
#include "libyuv/video_common.h"

#ifdef ENABLE_ROW_TESTS
#include "libyuv/row.h" /* For ARGBToAR30Row_AVX2 */
#endif

#if defined(__riscv) && !defined(__clang__)
#define DISABLE_SLOW_TESTS
#undef ENABLE_FULL_TESTS
#undef ENABLE_ROW_TESTS
#define LEAN_TESTS
#endif

// Some functions fail on big endian. Enable these tests on all cpus except
// PowerPC, but they are not optimized so disabled by default.
#if !defined(DISABLE_SLOW_TESTS) && !defined(__powerpc__)
#define LITTLE_ENDIAN_ONLY_TEST 1
#endif
#if !defined(DISABLE_SLOW_TESTS) || defined(__x86_64__) || defined(__i386__)
// SLOW TESTS are those that are unoptimized C code.
// FULL TESTS are optimized but test many variations of the same code.
#define ENABLE_FULL_TESTS
#endif

namespace libyuv {

// Alias to copy pixels as is
#define AR30ToAR30 ARGBCopy
#define ABGRToABGR ARGBCopy

// subsample amount uses a divide.
#define SUBSAMPLE(v, a) ((((v) + (a)-1)) / (a))

#define ALIGNINT(V, ALIGN) (((V) + (ALIGN)-1) / (ALIGN) * (ALIGN))

#define TESTBPTOPI(SRC_FMT_PLANAR, SRC_T, SRC_BPC, SRC_SUBSAMP_X,              \
                   SRC_SUBSAMP_Y, FMT_PLANAR, DST_T, DST_BPC, DST_SUBSAMP_X,   \
                   DST_SUBSAMP_Y, W1280, N, NEG, OFF, SRC_DEPTH, TILE_WIDTH,   \
                   TILE_HEIGHT)                                                \
  TEST_F(LibYUVConvertTest, SRC_FMT_PLANAR##To##FMT_PLANAR##N) {               \
    static_assert(SRC_BPC == 1 || SRC_BPC == 2, "SRC BPC unsupported");        \
    static_assert(DST_BPC == 1 || DST_BPC == 2, "DST BPC unsupported");        \
    static_assert(SRC_SUBSAMP_X == 1 || SRC_SUBSAMP_X == 2,                    \
                  "SRC_SUBSAMP_X unsupported");                                \
    static_assert(SRC_SUBSAMP_Y == 1 || SRC_SUBSAMP_Y == 2,                    \
                  "SRC_SUBSAMP_Y unsupported");                                \
    static_assert(DST_SUBSAMP_X == 1 || DST_SUBSAMP_X == 2,                    \
                  "DST_SUBSAMP_X unsupported");                                \
    static_assert(DST_SUBSAMP_Y == 1 || DST_SUBSAMP_Y == 2,                    \
                  "DST_SUBSAMP_Y unsupported");                                \
    const int kWidth = W1280;                                                  \
    const int kHeight = benchmark_height_;                                     \
    const int kSrcHalfWidth = SUBSAMPLE(kWidth, SRC_SUBSAMP_X);                \
    const int kDstHalfWidth = SUBSAMPLE(kWidth, DST_SUBSAMP_X);                \
    const int kDstHalfHeight = SUBSAMPLE(kHeight, DST_SUBSAMP_Y);              \
    const int kPaddedWidth = (kWidth + (TILE_WIDTH - 1)) & ~(TILE_WIDTH - 1);  \
    const int kPaddedHeight =                                                  \
        (kHeight + (TILE_HEIGHT - 1)) & ~(TILE_HEIGHT - 1);                    \
    const int kSrcHalfPaddedWidth = SUBSAMPLE(kPaddedWidth, SRC_SUBSAMP_X);    \
    const int kSrcHalfPaddedHeight = SUBSAMPLE(kPaddedHeight, SRC_SUBSAMP_Y);  \
    align_buffer_page_end(src_y, kPaddedWidth* kPaddedHeight* SRC_BPC + OFF);  \
    align_buffer_page_end(                                                     \
        src_uv, kSrcHalfPaddedWidth* kSrcHalfPaddedHeight* SRC_BPC * 2 + OFF); \
    align_buffer_page_end(dst_y_c, kWidth* kHeight* DST_BPC);                  \
    align_buffer_page_end(dst_u_c, kDstHalfWidth* kDstHalfHeight* DST_BPC);    \
    align_buffer_page_end(dst_v_c, kDstHalfWidth* kDstHalfHeight* DST_BPC);    \
    align_buffer_page_end(dst_y_opt, kWidth* kHeight* DST_BPC);                \
    align_buffer_page_end(dst_u_opt, kDstHalfWidth* kDstHalfHeight* DST_BPC);  \
    align_buffer_page_end(dst_v_opt, kDstHalfWidth* kDstHalfHeight* DST_BPC);  \
    SRC_T* src_y_p = reinterpret_cast<SRC_T*>(src_y + OFF);                    \
    SRC_T* src_uv_p = reinterpret_cast<SRC_T*>(src_uv + OFF);                  \
    for (int i = 0; i < kPaddedWidth * kPaddedHeight; ++i) {                   \
      src_y_p[i] =                                                             \
          (fastrand() & (((SRC_T)(-1)) << ((8 * SRC_BPC) - SRC_DEPTH)));       \
    }                                                                          \
    for (int i = 0; i < kSrcHalfPaddedWidth * kSrcHalfPaddedHeight * 2; ++i) { \
      src_uv_p[i] =                                                            \
          (fastrand() & (((SRC_T)(-1)) << ((8 * SRC_BPC) - SRC_DEPTH)));       \
    }                                                                          \
    memset(dst_y_c, 1, kWidth* kHeight* DST_BPC);                              \
    memset(dst_u_c, 2, kDstHalfWidth* kDstHalfHeight* DST_BPC);                \
    memset(dst_v_c, 3, kDstHalfWidth* kDstHalfHeight* DST_BPC);                \
    memset(dst_y_opt, 101, kWidth* kHeight* DST_BPC);                          \
    memset(dst_u_opt, 102, kDstHalfWidth* kDstHalfHeight* DST_BPC);            \
    memset(dst_v_opt, 103, kDstHalfWidth* kDstHalfHeight* DST_BPC);            \
    MaskCpuFlags(disable_cpu_flags_);                                          \
    SRC_FMT_PLANAR##To##FMT_PLANAR(                                            \
        src_y_p, kWidth, src_uv_p, kSrcHalfWidth * 2,                          \
        reinterpret_cast<DST_T*>(dst_y_c), kWidth,                             \
        reinterpret_cast<DST_T*>(dst_u_c), kDstHalfWidth,                      \
        reinterpret_cast<DST_T*>(dst_v_c), kDstHalfWidth, kWidth,              \
        NEG kHeight);                                                          \
    MaskCpuFlags(benchmark_cpu_info_);                                         \
    for (int i = 0; i < benchmark_iterations_; ++i) {                          \
      SRC_FMT_PLANAR##To##FMT_PLANAR(                                          \
          src_y_p, kWidth, src_uv_p, kSrcHalfWidth * 2,                        \
          reinterpret_cast<DST_T*>(dst_y_opt), kWidth,                         \
          reinterpret_cast<DST_T*>(dst_u_opt), kDstHalfWidth,                  \
          reinterpret_cast<DST_T*>(dst_v_opt), kDstHalfWidth, kWidth,          \
          NEG kHeight);                                                        \
    }                                                                          \
    for (int i = 0; i < kHeight * kWidth * DST_BPC; ++i) {                     \
      EXPECT_EQ(dst_y_c[i], dst_y_opt[i]);                                     \
    }                                                                          \
    for (int i = 0; i < kDstHalfWidth * kDstHalfHeight * DST_BPC; ++i) {       \
      EXPECT_EQ(dst_u_c[i], dst_u_opt[i]);                                     \
      EXPECT_EQ(dst_v_c[i], dst_v_opt[i]);                                     \
    }                                                                          \
    free_aligned_buffer_page_end(dst_y_c);                                     \
    free_aligned_buffer_page_end(dst_u_c);                                     \
    free_aligned_buffer_page_end(dst_v_c);                                     \
    free_aligned_buffer_page_end(dst_y_opt);                                   \
    free_aligned_buffer_page_end(dst_u_opt);                                   \
    free_aligned_buffer_page_end(dst_v_opt);                                   \
    free_aligned_buffer_page_end(src_y);                                       \
    free_aligned_buffer_page_end(src_uv);                                      \
  }

#if defined(ENABLE_FULL_TESTS)
#define TESTBPTOP(SRC_FMT_PLANAR, SRC_T, SRC_BPC, SRC_SUBSAMP_X,            \
                  SRC_SUBSAMP_Y, FMT_PLANAR, DST_T, DST_BPC, DST_SUBSAMP_X, \
                  DST_SUBSAMP_Y, SRC_DEPTH, TILE_WIDTH, TILE_HEIGHT)        \
  TESTBPTOPI(SRC_FMT_PLANAR, SRC_T, SRC_BPC, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,  \
             FMT_PLANAR, DST_T, DST_BPC, DST_SUBSAMP_X, DST_SUBSAMP_Y,      \
             benchmark_width_ + 1, _Any, +, 0, SRC_DEPTH, TILE_WIDTH,       \
             TILE_HEIGHT)                                                   \
  TESTBPTOPI(SRC_FMT_PLANAR, SRC_T, SRC_BPC, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,  \
             FMT_PLANAR, DST_T, DST_BPC, DST_SUBSAMP_X, DST_SUBSAMP_Y,      \
             benchmark_width_, _Unaligned, +, 2, SRC_DEPTH, TILE_WIDTH,     \
             TILE_HEIGHT)                                                   \
  TESTBPTOPI(SRC_FMT_PLANAR, SRC_T, SRC_BPC, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,  \
             FMT_PLANAR, DST_T, DST_BPC, DST_SUBSAMP_X, DST_SUBSAMP_Y,      \
             benchmark_width_, _Invert, -, 0, SRC_DEPTH, TILE_WIDTH,        \
             TILE_HEIGHT)                                                   \
  TESTBPTOPI(SRC_FMT_PLANAR, SRC_T, SRC_BPC, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,  \
             FMT_PLANAR, DST_T, DST_BPC, DST_SUBSAMP_X, DST_SUBSAMP_Y,      \
             benchmark_width_, _Opt, +, 0, SRC_DEPTH, TILE_WIDTH, TILE_HEIGHT)
#else
#define TESTBPTOP(SRC_FMT_PLANAR, SRC_T, SRC_BPC, SRC_SUBSAMP_X,            \
                  SRC_SUBSAMP_Y, FMT_PLANAR, DST_T, DST_BPC, DST_SUBSAMP_X, \
                  DST_SUBSAMP_Y, SRC_DEPTH, TILE_WIDTH, TILE_HEIGHT)        \
  TESTBPTOPI(SRC_FMT_PLANAR, SRC_T, SRC_BPC, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,  \
             FMT_PLANAR, DST_T, DST_BPC, DST_SUBSAMP_X, DST_SUBSAMP_Y,      \
             benchmark_width_, _Opt, +, 0, SRC_DEPTH, TILE_WIDTH, TILE_HEIGHT)
#endif

TESTBPTOP(NV12, uint8_t, 1, 2, 2, I420, uint8_t, 1, 2, 2, 8, 1, 1)
TESTBPTOP(NV21, uint8_t, 1, 2, 2, I420, uint8_t, 1, 2, 2, 8, 1, 1)
TESTBPTOP(MM21, uint8_t, 1, 2, 2, I420, uint8_t, 1, 2, 2, 8, 16, 32)
TESTBPTOP(P010, uint16_t, 2, 2, 2, I010, uint16_t, 2, 2, 2, 10, 1, 1)
TESTBPTOP(P012, uint16_t, 2, 2, 2, I012, uint16_t, 2, 2, 2, 12, 1, 1)

// Provide matrix wrappers for full range bt.709
#define F420ToABGR(a, b, c, d, e, f, g, h, i, j) \
  I420ToARGBMatrix(a, b, e, f, c, d, g, h, &kYvuF709Constants, i, j)
#define F420ToARGB(a, b, c, d, e, f, g, h, i, j) \
  I420ToARGBMatrix(a, b, c, d, e, f, g, h, &kYuvF709Constants, i, j)
#define F422ToABGR(a, b, c, d, e, f, g, h, i, j) \
  I422ToARGBMatrix(a, b, e, f, c, d, g, h, &kYvuF709Constants, i, j)
#define F422ToARGB(a, b, c, d, e, f, g, h, i, j) \
  I422ToARGBMatrix(a, b, c, d, e, f, g, h, &kYuvF709Constants, i, j)
#define F444ToABGR(a, b, c, d, e, f, g, h, i, j) \
  I444ToARGBMatrix(a, b, e, f, c, d, g, h, &kYvuF709Constants, i, j)
#define F444ToARGB(a, b, c, d, e, f, g, h, i, j) \
  I444ToARGBMatrix(a, b, c, d, e, f, g, h, &kYuvF709Constants, i, j)

// Provide matrix wrappers for full range bt.2020
#define V420ToABGR(a, b, c, d, e, f, g, h, i, j) \
  I420ToARGBMatrix(a, b, e, f, c, d, g, h, &kYvuV2020Constants, i, j)
#define V420ToARGB(a, b, c, d, e, f, g, h, i, j) \
  I420ToARGBMatrix(a, b, c, d, e, f, g, h, &kYuvV2020Constants, i, j)
#define V422ToABGR(a, b, c, d, e, f, g, h, i, j) \
  I422ToARGBMatrix(a, b, e, f, c, d, g, h, &kYvuV2020Constants, i, j)
#define V422ToARGB(a, b, c, d, e, f, g, h, i, j) \
  I422ToARGBMatrix(a, b, c, d, e, f, g, h, &kYuvV2020Constants, i, j)
#define V444ToABGR(a, b, c, d, e, f, g, h, i, j) \
  I444ToARGBMatrix(a, b, e, f, c, d, g, h, &kYvuV2020Constants, i, j)
#define V444ToARGB(a, b, c, d, e, f, g, h, i, j) \
  I444ToARGBMatrix(a, b, c, d, e, f, g, h, &kYuvV2020Constants, i, j)

#define I420ToARGBFilter(a, b, c, d, e, f, g, h, i, j)                     \
  I420ToARGBMatrixFilter(a, b, c, d, e, f, g, h, &kYuvI601Constants, i, j, \
                         kFilterBilinear)
#define I422ToARGBFilter(a, b, c, d, e, f, g, h, i, j)                     \
  I422ToARGBMatrixFilter(a, b, c, d, e, f, g, h, &kYuvI601Constants, i, j, \
                         kFilterBilinear)
#define I420ToRGB24Filter(a, b, c, d, e, f, g, h, i, j)                     \
  I420ToRGB24MatrixFilter(a, b, c, d, e, f, g, h, &kYuvI601Constants, i, j, \
                          kFilterBilinear)
#define I422ToRGB24Filter(a, b, c, d, e, f, g, h, i, j)                     \
  I420ToRGB24MatrixFilter(a, b, c, d, e, f, g, h, &kYuvI601Constants, i, j, \
                          kFilterBilinear)

#define TESTPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN, \
                       YALIGN, W1280, N, NEG, OFF)                            \
  TEST_F(LibYUVConvertTest, FMT_PLANAR##To##FMT_B##N) {                       \
    const int kWidth = W1280;                                                 \
    const int kHeight = ALIGNINT(benchmark_height_, YALIGN);                  \
    const int kStrideB = ALIGNINT(kWidth * BPP_B, ALIGN);                     \
    const int kStrideUV = SUBSAMPLE(kWidth, SUBSAMP_X);                       \
    const int kSizeUV = kStrideUV * SUBSAMPLE(kHeight, SUBSAMP_Y);            \
    align_buffer_page_end(src_y, kWidth* kHeight + OFF);                      \
    align_buffer_page_end(src_u, kSizeUV + OFF);                              \
    align_buffer_page_end(src_v, kSizeUV + OFF);                              \
    align_buffer_page_end(dst_argb_c, kStrideB* kHeight + OFF);               \
    align_buffer_page_end(dst_argb_opt, kStrideB* kHeight + OFF);             \
    for (int i = 0; i < kWidth * kHeight; ++i) {                              \
      src_y[i + OFF] = (fastrand() & 0xff);                                   \
    }                                                                         \
    for (int i = 0; i < kSizeUV; ++i) {                                       \
      src_u[i + OFF] = (fastrand() & 0xff);                                   \
      src_v[i + OFF] = (fastrand() & 0xff);                                   \
    }                                                                         \
    memset(dst_argb_c + OFF, 1, kStrideB * kHeight);                          \
    memset(dst_argb_opt + OFF, 101, kStrideB * kHeight);                      \
    MaskCpuFlags(disable_cpu_flags_);                                         \
    double time0 = get_time();                                                \
    FMT_PLANAR##To##FMT_B(src_y + OFF, kWidth, src_u + OFF, kStrideUV,        \
                          src_v + OFF, kStrideUV, dst_argb_c + OFF, kStrideB, \
                          kWidth, NEG kHeight);                               \
    double time1 = get_time();                                                \
    MaskCpuFlags(benchmark_cpu_info_);                                        \
    for (int i = 0; i < benchmark_iterations_; ++i) {                         \
      FMT_PLANAR##To##FMT_B(src_y + OFF, kWidth, src_u + OFF, kStrideUV,      \
                            src_v + OFF, kStrideUV, dst_argb_opt + OFF,       \
                            kStrideB, kWidth, NEG kHeight);                   \
    }                                                                         \
    double time2 = get_time();                                                \
    printf(" %8d us C - %8d us OPT\n",                                        \
           static_cast<int>((time1 - time0) * 1e6),                           \
           static_cast<int>((time2 - time1) * 1e6 / benchmark_iterations_));  \
    for (int i = 0; i < kWidth * BPP_B * kHeight; ++i) {                      \
      EXPECT_EQ(dst_argb_c[i + OFF], dst_argb_opt[i + OFF]);                  \
    }                                                                         \
    free_aligned_buffer_page_end(src_y);                                      \
    free_aligned_buffer_page_end(src_u);                                      \
    free_aligned_buffer_page_end(src_v);                                      \
    free_aligned_buffer_page_end(dst_argb_c);                                 \
    free_aligned_buffer_page_end(dst_argb_opt);                               \
  }

#if defined(ENABLE_FULL_TESTS)
#define TESTPLANARTOB(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN, \
                      YALIGN)                                                \
  TESTPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,      \
                 YALIGN, benchmark_width_ + 1, _Any, +, 0)                   \
  TESTPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,      \
                 YALIGN, benchmark_width_, _Unaligned, +, 4)                 \
  TESTPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,      \
                 YALIGN, benchmark_width_, _Invert, -, 0)                    \
  TESTPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,      \
                 YALIGN, benchmark_width_, _Opt, +, 0)
#else
#define TESTPLANARTOB(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN, \
                      YALIGN)                                                \
  TESTPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,      \
                 YALIGN, benchmark_width_, _Opt, +, 0)
#endif

#if defined(ENABLE_FULL_TESTS)
TESTPLANARTOB(I420, 2, 2, ARGB, 4, 4, 1)
TESTPLANARTOB(I420, 2, 2, ABGR, 4, 4, 1)
TESTPLANARTOB(J420, 2, 2, ARGB, 4, 4, 1)
TESTPLANARTOB(J420, 2, 2, ABGR, 4, 4, 1)
TESTPLANARTOB(F420, 2, 2, ARGB, 4, 4, 1)
TESTPLANARTOB(F420, 2, 2, ABGR, 4, 4, 1)
TESTPLANARTOB(H420, 2, 2, ARGB, 4, 4, 1)
TESTPLANARTOB(H420, 2, 2, ABGR, 4, 4, 1)
TESTPLANARTOB(U420, 2, 2, ARGB, 4, 4, 1)
TESTPLANARTOB(U420, 2, 2, ABGR, 4, 4, 1)
TESTPLANARTOB(V420, 2, 2, ARGB, 4, 4, 1)
TESTPLANARTOB(V420, 2, 2, ABGR, 4, 4, 1)
TESTPLANARTOB(I420, 2, 2, BGRA, 4, 4, 1)
TESTPLANARTOB(I420, 2, 2, RGBA, 4, 4, 1)
TESTPLANARTOB(I420, 2, 2, RAW, 3, 3, 1)
TESTPLANARTOB(I420, 2, 2, RGB24, 3, 3, 1)
TESTPLANARTOB(J420, 2, 2, RAW, 3, 3, 1)
TESTPLANARTOB(J420, 2, 2, RGB24, 3, 3, 1)
TESTPLANARTOB(H420, 2, 2, RAW, 3, 3, 1)
TESTPLANARTOB(H420, 2, 2, RGB24, 3, 3, 1)
#ifdef LITTLE_ENDIAN_ONLY_TEST
TESTPLANARTOB(I420, 2, 2, RGB565, 2, 2, 1)
TESTPLANARTOB(J420, 2, 2, RGB565, 2, 2, 1)
TESTPLANARTOB(H420, 2, 2, RGB565, 2, 2, 1)
TESTPLANARTOB(I420, 2, 2, ARGB1555, 2, 2, 1)
TESTPLANARTOB(I420, 2, 2, ARGB4444, 2, 2, 1)
TESTPLANARTOB(I422, 2, 1, RGB565, 2, 2, 1)
#endif
TESTPLANARTOB(I422, 2, 1, ARGB, 4, 4, 1)
TESTPLANARTOB(I422, 2, 1, ABGR, 4, 4, 1)
TESTPLANARTOB(J422, 2, 1, ARGB, 4, 4, 1)
TESTPLANARTOB(J422, 2, 1, ABGR, 4, 4, 1)
TESTPLANARTOB(H422, 2, 1, ARGB, 4, 4, 1)
TESTPLANARTOB(H422, 2, 1, ABGR, 4, 4, 1)
TESTPLANARTOB(U422, 2, 1, ARGB, 4, 4, 1)
TESTPLANARTOB(U422, 2, 1, ABGR, 4, 4, 1)
TESTPLANARTOB(V422, 2, 1, ARGB, 4, 4, 1)
TESTPLANARTOB(V422, 2, 1, ABGR, 4, 4, 1)
TESTPLANARTOB(I422, 2, 1, BGRA, 4, 4, 1)
TESTPLANARTOB(I422, 2, 1, RGBA, 4, 4, 1)
TESTPLANARTOB(I422, 1, 1, RGB24, 3, 3, 1)
TESTPLANARTOB(I422, 1, 1, RAW, 3, 3, 1)
TESTPLANARTOB(I444, 1, 1, ARGB, 4, 4, 1)
TESTPLANARTOB(I444, 1, 1, ABGR, 4, 4, 1)
TESTPLANARTOB(I444, 1, 1, RGB24, 3, 3, 1)
TESTPLANARTOB(I444, 1, 1, RAW, 3, 3, 1)
TESTPLANARTOB(J444, 1, 1, ARGB, 4, 4, 1)
TESTPLANARTOB(J444, 1, 1, ABGR, 4, 4, 1)
TESTPLANARTOB(H444, 1, 1, ARGB, 4, 4, 1)
TESTPLANARTOB(H444, 1, 1, ABGR, 4, 4, 1)
TESTPLANARTOB(U444, 1, 1, ARGB, 4, 4, 1)
TESTPLANARTOB(U444, 1, 1, ABGR, 4, 4, 1)
TESTPLANARTOB(V444, 1, 1, ARGB, 4, 4, 1)
TESTPLANARTOB(V444, 1, 1, ABGR, 4, 4, 1)
TESTPLANARTOB(I420, 2, 2, YUY2, 2, 4, 1)
TESTPLANARTOB(I420, 2, 2, UYVY, 2, 4, 1)
TESTPLANARTOB(I422, 2, 1, YUY2, 2, 4, 1)
TESTPLANARTOB(I422, 2, 1, UYVY, 2, 4, 1)
TESTPLANARTOB(I420, 2, 2, I400, 1, 1, 1)
TESTPLANARTOB(J420, 2, 2, J400, 1, 1, 1)
#ifdef LITTLE_ENDIAN_ONLY_TEST
TESTPLANARTOB(I420, 2, 2, AR30, 4, 4, 1)
TESTPLANARTOB(H420, 2, 2, AR30, 4, 4, 1)
TESTPLANARTOB(I420, 2, 2, AB30, 4, 4, 1)
TESTPLANARTOB(H420, 2, 2, AB30, 4, 4, 1)
#endif
TESTPLANARTOB(I420, 2, 2, ARGBFilter, 4, 4, 1)
TESTPLANARTOB(I422, 2, 1, ARGBFilter, 4, 4, 1)
TESTPLANARTOB(I420, 2, 2, RGB24Filter, 3, 3, 1)
TESTPLANARTOB(I422, 2, 2, RGB24Filter, 3, 3, 1)
#else  // FULL_TESTS
TESTPLANARTOB(I420, 2, 2, ABGR, 4, 4, 1)
TESTPLANARTOB(I420, 2, 2, ARGB, 4, 4, 1)
TESTPLANARTOB(I420, 2, 2, BGRA, 4, 4, 1)
TESTPLANARTOB(I420, 2, 2, RAW, 3, 3, 1)
TESTPLANARTOB(I420, 2, 2, RGB24, 3, 3, 1)
TESTPLANARTOB(I420, 2, 2, RGBA, 4, 4, 1)
#ifdef LITTLE_ENDIAN_ONLY_TEST
TESTPLANARTOB(I420, 2, 2, RGB565, 2, 2, 1)
TESTPLANARTOB(I420, 2, 2, ARGB1555, 2, 2, 1)
TESTPLANARTOB(I420, 2, 2, ARGB4444, 2, 2, 1)
TESTPLANARTOB(I422, 2, 1, RGB565, 2, 2, 1)
#endif
TESTPLANARTOB(I420, 2, 2, I400, 1, 1, 1)
TESTPLANARTOB(I420, 2, 2, UYVY, 2, 4, 1)
TESTPLANARTOB(I420, 2, 2, YUY2, 2, 4, 1)
TESTPLANARTOB(I422, 2, 1, ABGR, 4, 4, 1)
TESTPLANARTOB(I422, 2, 1, ARGB, 4, 4, 1)
TESTPLANARTOB(I422, 2, 1, BGRA, 4, 4, 1)
TESTPLANARTOB(I422, 2, 1, RGBA, 4, 4, 1)
TESTPLANARTOB(I422, 2, 1, UYVY, 2, 4, 1)
TESTPLANARTOB(I422, 2, 1, YUY2, 2, 4, 1)
TESTPLANARTOB(I420, 2, 2, ARGBFilter, 4, 4, 1)
TESTPLANARTOB(I422, 2, 1, ARGBFilter, 4, 4, 1)
TESTPLANARTOB(I420, 2, 2, RGB24Filter, 3, 3, 1)
TESTPLANARTOB(I444, 1, 1, ABGR, 4, 4, 1)
TESTPLANARTOB(I444, 1, 1, ARGB, 4, 4, 1)
#endif

#define TESTBPTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, FMT_C, BPP_B,      \
                   W1280, N, NEG, OFF)                                         \
  TEST_F(LibYUVConvertTest, FMT_PLANAR##To##FMT_B##N) {                        \
    const int kWidth = W1280;                                                  \
    const int kHeight = benchmark_height_;                                     \
    const int kStrideB = kWidth * BPP_B;                                       \
    const int kStrideUV = SUBSAMPLE(kWidth, SUBSAMP_X);                        \
    align_buffer_page_end(src_y, kWidth* kHeight + OFF);                       \
    align_buffer_page_end(src_uv,                                              \
                          kStrideUV* SUBSAMPLE(kHeight, SUBSAMP_Y) * 2 + OFF); \
    align_buffer_page_end(dst_argb_c, kStrideB* kHeight);                      \
    align_buffer_page_end(dst_argb_opt, kStrideB* kHeight);                    \
    for (int i = 0; i < kHeight; ++i)                                          \
      for (int j = 0; j < kWidth; ++j)                                         \
        src_y[i * kWidth + j + OFF] = (fastrand() & 0xff);                     \
    for (int i = 0; i < SUBSAMPLE(kHeight, SUBSAMP_Y); ++i) {                  \
      for (int j = 0; j < kStrideUV * 2; ++j) {                                \
        src_uv[i * kStrideUV * 2 + j + OFF] = (fastrand() & 0xff);             \
      }                                                                        \
    }                                                                          \
    memset(dst_argb_c, 1, kStrideB* kHeight);                                  \
    memset(dst_argb_opt, 101, kStrideB* kHeight);                              \
    MaskCpuFlags(disable_cpu_flags_);                                          \
    FMT_PLANAR##To##FMT_B(src_y + OFF, kWidth, src_uv + OFF, kStrideUV * 2,    \
                          dst_argb_c, kWidth * BPP_B, kWidth, NEG kHeight);    \
    MaskCpuFlags(benchmark_cpu_info_);                                         \
    for (int i = 0; i < benchmark_iterations_; ++i) {                          \
      FMT_PLANAR##To##FMT_B(src_y + OFF, kWidth, src_uv + OFF, kStrideUV * 2,  \
                            dst_argb_opt, kWidth * BPP_B, kWidth,              \
                            NEG kHeight);                                      \
    }                                                                          \
    /* Convert to ARGB so 565 is expanded to bytes that can be compared. */    \
    align_buffer_page_end(dst_argb32_c, kWidth * 4 * kHeight);                 \
    align_buffer_page_end(dst_argb32_opt, kWidth * 4 * kHeight);               \
    memset(dst_argb32_c, 2, kWidth * 4 * kHeight);                             \
    memset(dst_argb32_opt, 102, kWidth * 4 * kHeight);                         \
    FMT_C##ToARGB(dst_argb_c, kStrideB, dst_argb32_c, kWidth * 4, kWidth,      \
                  kHeight);                                                    \
    FMT_C##ToARGB(dst_argb_opt, kStrideB, dst_argb32_opt, kWidth * 4, kWidth,  \
                  kHeight);                                                    \
    for (int i = 0; i < kHeight; ++i) {                                        \
      for (int j = 0; j < kWidth * 4; ++j) {                                   \
        EXPECT_EQ(dst_argb32_c[i * kWidth * 4 + j],                            \
                  dst_argb32_opt[i * kWidth * 4 + j]);                         \
      }                                                                        \
    }                                                                          \
    free_aligned_buffer_page_end(src_y);                                       \
    free_aligned_buffer_page_end(src_uv);                                      \
    free_aligned_buffer_page_end(dst_argb_c);                                  \
    free_aligned_buffer_page_end(dst_argb_opt);                                \
    free_aligned_buffer_page_end(dst_argb32_c);                                \
    free_aligned_buffer_page_end(dst_argb32_opt);                              \
  }

#if defined(ENABLE_FULL_TESTS)
#define TESTBPTOB(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, FMT_C, BPP_B) \
  TESTBPTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, FMT_C, BPP_B,      \
             benchmark_width_ + 1, _Any, +, 0)                           \
  TESTBPTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, FMT_C, BPP_B,      \
             benchmark_width_, _Unaligned, +, 2)                         \
  TESTBPTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, FMT_C, BPP_B,      \
             benchmark_width_, _Invert, -, 0)                            \
  TESTBPTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, FMT_C, BPP_B,      \
             benchmark_width_, _Opt, +, 0)
#else
#define TESTBPTOB(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, FMT_C, BPP_B) \
  TESTBPTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, FMT_C, BPP_B,      \
             benchmark_width_, _Opt, +, 0)
#endif

#define JNV12ToARGB(a, b, c, d, e, f, g, h) \
  NV12ToARGBMatrix(a, b, c, d, e, f, &kYuvJPEGConstants, g, h)
#define JNV21ToARGB(a, b, c, d, e, f, g, h) \
  NV21ToARGBMatrix(a, b, c, d, e, f, &kYuvJPEGConstants, g, h)
#define JNV12ToABGR(a, b, c, d, e, f, g, h) \
  NV21ToARGBMatrix(a, b, c, d, e, f, &kYvuJPEGConstants, g, h)
#define JNV21ToABGR(a, b, c, d, e, f, g, h) \
  NV12ToARGBMatrix(a, b, c, d, e, f, &kYvuJPEGConstants, g, h)
#define JNV12ToRGB24(a, b, c, d, e, f, g, h) \
  NV12ToRGB24Matrix(a, b, c, d, e, f, &kYuvJPEGConstants, g, h)
#define JNV21ToRGB24(a, b, c, d, e, f, g, h) \
  NV21ToRGB24Matrix(a, b, c, d, e, f, &kYuvJPEGConstants, g, h)
#define JNV12ToRAW(a, b, c, d, e, f, g, h) \
  NV21ToRGB24Matrix(a, b, c, d, e, f, &kYvuJPEGConstants, g, h)
#define JNV21ToRAW(a, b, c, d, e, f, g, h) \
  NV12ToRGB24Matrix(a, b, c, d, e, f, &kYvuJPEGConstants, g, h)
#define JNV12ToRGB565(a, b, c, d, e, f, g, h) \
  NV12ToRGB565Matrix(a, b, c, d, e, f, &kYuvJPEGConstants, g, h)

TESTBPTOB(JNV12, 2, 2, ARGB, ARGB, 4)
TESTBPTOB(JNV21, 2, 2, ARGB, ARGB, 4)
TESTBPTOB(JNV12, 2, 2, ABGR, ABGR, 4)
TESTBPTOB(JNV21, 2, 2, ABGR, ABGR, 4)
TESTBPTOB(JNV12, 2, 2, RGB24, RGB24, 3)
TESTBPTOB(JNV21, 2, 2, RGB24, RGB24, 3)
TESTBPTOB(JNV12, 2, 2, RAW, RAW, 3)
TESTBPTOB(JNV21, 2, 2, RAW, RAW, 3)
#ifdef LITTLE_ENDIAN_ONLY_TEST
TESTBPTOB(JNV12, 2, 2, RGB565, RGB565, 2)
#endif

TESTBPTOB(NV12, 2, 2, ARGB, ARGB, 4)
TESTBPTOB(NV21, 2, 2, ARGB, ARGB, 4)
TESTBPTOB(NV12, 2, 2, ABGR, ABGR, 4)
TESTBPTOB(NV21, 2, 2, ABGR, ABGR, 4)
TESTBPTOB(NV12, 2, 2, RGB24, RGB24, 3)
TESTBPTOB(NV21, 2, 2, RGB24, RGB24, 3)
TESTBPTOB(NV12, 2, 2, RAW, RAW, 3)
TESTBPTOB(NV21, 2, 2, RAW, RAW, 3)
TESTBPTOB(NV21, 2, 2, YUV24, RAW, 3)
#ifdef LITTLE_ENDIAN_ONLY_TEST
TESTBPTOB(NV12, 2, 2, RGB565, RGB565, 2)
#endif

#define TESTATOBI(FMT_A, TYPE_A, EPP_A, STRIDE_A, HEIGHT_A, FMT_B, TYPE_B,     \
                  EPP_B, STRIDE_B, HEIGHT_B, W1280, N, NEG, OFF)               \
  TEST_F(LibYUVConvertTest, FMT_A##To##FMT_B##N) {                             \
    const int kWidth = W1280;                                                  \
    const int kHeight = benchmark_height_;                                     \
    const int kHeightA = (kHeight + HEIGHT_A - 1) / HEIGHT_A * HEIGHT_A;       \
    const int kHeightB = (kHeight + HEIGHT_B - 1) / HEIGHT_B * HEIGHT_B;       \
    const int kStrideA =                                                       \
        (kWidth * EPP_A + STRIDE_A - 1) / STRIDE_A * STRIDE_A;                 \
    const int kStrideB =                                                       \
        (kWidth * EPP_B + STRIDE_B - 1) / STRIDE_B * STRIDE_B;                 \
    align_buffer_page_end(src_argb,                                            \
                          kStrideA* kHeightA*(int)sizeof(TYPE_A) + OFF);       \
    align_buffer_page_end(dst_argb_c, kStrideB* kHeightB*(int)sizeof(TYPE_B)); \
    align_buffer_page_end(dst_argb_opt,                                        \
                          kStrideB* kHeightB*(int)sizeof(TYPE_B));             \
    for (int i = 0; i < kStrideA * kHeightA * (int)sizeof(TYPE_A); ++i) {      \
      src_argb[i + OFF] = (fastrand() & 0xff);                                 \
    }                                                                          \
    memset(dst_argb_c, 1, kStrideB* kHeightB);                                 \
    memset(dst_argb_opt, 101, kStrideB* kHeightB);                             \
    MaskCpuFlags(disable_cpu_flags_);                                          \
    FMT_A##To##FMT_B((TYPE_A*)(src_argb + OFF), kStrideA, (TYPE_B*)dst_argb_c, \
                     kStrideB, kWidth, NEG kHeight);                           \
    MaskCpuFlags(benchmark_cpu_info_);                                         \
    for (int i = 0; i < benchmark_iterations_; ++i) {                          \
      FMT_A##To##FMT_B((TYPE_A*)(src_argb + OFF), kStrideA,                    \
                       (TYPE_B*)dst_argb_opt, kStrideB, kWidth, NEG kHeight);  \
    }                                                                          \
    for (int i = 0; i < kStrideB * kHeightB * (int)sizeof(TYPE_B); ++i) {      \
      EXPECT_EQ(dst_argb_c[i], dst_argb_opt[i]);                               \
    }                                                                          \
    free_aligned_buffer_page_end(src_argb);                                    \
    free_aligned_buffer_page_end(dst_argb_c);                                  \
    free_aligned_buffer_page_end(dst_argb_opt);                                \
  }

#define TESTATOBRANDOM(FMT_A, TYPE_A, EPP_A, STRIDE_A, HEIGHT_A, FMT_B,        \
                       TYPE_B, EPP_B, STRIDE_B, HEIGHT_B)                      \
  TEST_F(LibYUVConvertTest, FMT_A##To##FMT_B##_Random) {                       \
    for (int times = 0; times < benchmark_iterations_; ++times) {              \
      const int kWidth = (fastrand() & 63) + 1;                                \
      const int kHeight = (fastrand() & 31) + 1;                               \
      const int kHeightA = (kHeight + HEIGHT_A - 1) / HEIGHT_A * HEIGHT_A;     \
      const int kHeightB = (kHeight + HEIGHT_B - 1) / HEIGHT_B * HEIGHT_B;     \
      const int kStrideA =                                                     \
          (kWidth * EPP_A + STRIDE_A - 1) / STRIDE_A * STRIDE_A;               \
      const int kStrideB =                                                     \
          (kWidth * EPP_B + STRIDE_B - 1) / STRIDE_B * STRIDE_B;               \
      align_buffer_page_end(src_argb, kStrideA* kHeightA*(int)sizeof(TYPE_A)); \
      align_buffer_page_end(dst_argb_c,                                        \
                            kStrideB* kHeightB*(int)sizeof(TYPE_B));           \
      align_buffer_page_end(dst_argb_opt,                                      \
                            kStrideB* kHeightB*(int)sizeof(TYPE_B));           \
      for (int i = 0; i < kStrideA * kHeightA * (int)sizeof(TYPE_A); ++i) {    \
        src_argb[i] = 0xfe;                                                    \
      }                                                                        \
      memset(dst_argb_c, 123, kStrideB* kHeightB);                             \
      memset(dst_argb_opt, 123, kStrideB* kHeightB);                           \
      MaskCpuFlags(disable_cpu_flags_);                                        \
      FMT_A##To##FMT_B((TYPE_A*)src_argb, kStrideA, (TYPE_B*)dst_argb_c,       \
                       kStrideB, kWidth, kHeight);                             \
      MaskCpuFlags(benchmark_cpu_info_);                                       \
      FMT_A##To##FMT_B((TYPE_A*)src_argb, kStrideA, (TYPE_B*)dst_argb_opt,     \
                       kStrideB, kWidth, kHeight);                             \
      for (int i = 0; i < kStrideB * kHeightB * (int)sizeof(TYPE_B); ++i) {    \
        EXPECT_EQ(dst_argb_c[i], dst_argb_opt[i]);                             \
      }                                                                        \
      free_aligned_buffer_page_end(src_argb);                                  \
      free_aligned_buffer_page_end(dst_argb_c);                                \
      free_aligned_buffer_page_end(dst_argb_opt);                              \
    }                                                                          \
  }

#if defined(ENABLE_FULL_TESTS)
#define TESTATOB(FMT_A, TYPE_A, EPP_A, STRIDE_A, HEIGHT_A, FMT_B, TYPE_B,   \
                 EPP_B, STRIDE_B, HEIGHT_B)                                 \
  TESTATOBI(FMT_A, TYPE_A, EPP_A, STRIDE_A, HEIGHT_A, FMT_B, TYPE_B, EPP_B, \
            STRIDE_B, HEIGHT_B, benchmark_width_ + 1, _Any, +, 0)           \
  TESTATOBI(FMT_A, TYPE_A, EPP_A, STRIDE_A, HEIGHT_A, FMT_B, TYPE_B, EPP_B, \
            STRIDE_B, HEIGHT_B, benchmark_width_, _Unaligned, +, 4)         \
  TESTATOBI(FMT_A, TYPE_A, EPP_A, STRIDE_A, HEIGHT_A, FMT_B, TYPE_B, EPP_B, \
            STRIDE_B, HEIGHT_B, benchmark_width_, _Invert, -, 0)            \
  TESTATOBI(FMT_A, TYPE_A, EPP_A, STRIDE_A, HEIGHT_A, FMT_B, TYPE_B, EPP_B, \
            STRIDE_B, HEIGHT_B, benchmark_width_, _Opt, +, 0)               \
  TESTATOBRANDOM(FMT_A, TYPE_A, EPP_A, STRIDE_A, HEIGHT_A, FMT_B, TYPE_B,   \
                 EPP_B, STRIDE_B, HEIGHT_B)
#else
#define TESTATOB(FMT_A, TYPE_A, EPP_A, STRIDE_A, HEIGHT_A, FMT_B, TYPE_B,   \
                 EPP_B, STRIDE_B, HEIGHT_B)                                 \
  TESTATOBI(FMT_A, TYPE_A, EPP_A, STRIDE_A, HEIGHT_A, FMT_B, TYPE_B, EPP_B, \
            STRIDE_B, HEIGHT_B, benchmark_width_, _Opt, +, 0)
#endif

TESTATOB(AB30, uint8_t, 4, 4, 1, ABGR, uint8_t, 4, 4, 1)
TESTATOB(AB30, uint8_t, 4, 4, 1, ARGB, uint8_t, 4, 4, 1)
#ifdef LITTLE_ENDIAN_ONLY_TEST
TESTATOB(ABGR, uint8_t, 4, 4, 1, AR30, uint8_t, 4, 4, 1)
#endif
TESTATOB(ABGR, uint8_t, 4, 4, 1, ARGB, uint8_t, 4, 4, 1)
#ifdef LITTLE_ENDIAN_ONLY_TEST
TESTATOB(AR30, uint8_t, 4, 4, 1, AB30, uint8_t, 4, 4, 1)
#endif
TESTATOB(AR30, uint8_t, 4, 4, 1, ABGR, uint8_t, 4, 4, 1)
#ifdef LITTLE_ENDIAN_ONLY_TEST
TESTATOB(AR30, uint8_t, 4, 4, 1, AR30, uint8_t, 4, 4, 1)
TESTATOB(AR30, uint8_t, 4, 4, 1, ARGB, uint8_t, 4, 4, 1)
#endif
TESTATOB(ARGB, uint8_t, 4, 4, 1, ABGR, uint8_t, 4, 4, 1)
#ifdef LITTLE_ENDIAN_ONLY_TEST
TESTATOB(ARGB, uint8_t, 4, 4, 1, AR30, uint8_t, 4, 4, 1)
#endif
TESTATOB(ARGB, uint8_t, 4, 4, 1, ARGB, uint8_t, 4, 4, 1)
TESTATOB(ARGB, uint8_t, 4, 4, 1, ARGB1555, uint8_t, 2, 2, 1)
TESTATOB(ARGB, uint8_t, 4, 4, 1, ARGB4444, uint8_t, 2, 2, 1)
TESTATOB(ARGB, uint8_t, 4, 4, 1, ARGBMirror, uint8_t, 4, 4, 1)
TESTATOB(ARGB, uint8_t, 4, 4, 1, BGRA, uint8_t, 4, 4, 1)
TESTATOB(ARGB, uint8_t, 4, 4, 1, I400, uint8_t, 1, 1, 1)
TESTATOB(ARGB, uint8_t, 4, 4, 1, J400, uint8_t, 1, 1, 1)
TESTATOB(ABGR, uint8_t, 4, 4, 1, J400, uint8_t, 1, 1, 1)
TESTATOB(RGBA, uint8_t, 4, 4, 1, J400, uint8_t, 1, 1, 1)
TESTATOB(ARGB, uint8_t, 4, 4, 1, RAW, uint8_t, 3, 3, 1)
TESTATOB(ARGB, uint8_t, 4, 4, 1, RGB24, uint8_t, 3, 3, 1)
TESTATOB(ABGR, uint8_t, 4, 4, 1, RAW, uint8_t, 3, 3, 1)
TESTATOB(ABGR, uint8_t, 4, 4, 1, RGB24, uint8_t, 3, 3, 1)
#ifdef LITTLE_ENDIAN_ONLY_TEST
TESTATOB(ARGB, uint8_t, 4, 4, 1, RGB565, uint8_t, 2, 2, 1)
#endif
TESTATOB(ARGB, uint8_t, 4, 4, 1, RGBA, uint8_t, 4, 4, 1)
TESTATOB(ARGB, uint8_t, 4, 4, 1, UYVY, uint8_t, 2, 4, 1)
TESTATOB(ARGB, uint8_t, 4, 4, 1, YUY2, uint8_t, 2, 4, 1)
TESTATOB(ARGB1555, uint8_t, 2, 2, 1, ARGB, uint8_t, 4, 4, 1)
TESTATOB(ARGB4444, uint8_t, 2, 2, 1, ARGB, uint8_t, 4, 4, 1)
TESTATOB(BGRA, uint8_t, 4, 4, 1, ARGB, uint8_t, 4, 4, 1)
TESTATOB(I400, uint8_t, 1, 1, 1, ARGB, uint8_t, 4, 4, 1)
TESTATOB(I400, uint8_t, 1, 1, 1, I400, uint8_t, 1, 1, 1)
TESTATOB(I400, uint8_t, 1, 1, 1, I400Mirror, uint8_t, 1, 1, 1)
TESTATOB(J400, uint8_t, 1, 1, 1, ARGB, uint8_t, 4, 4, 1)
TESTATOB(J400, uint8_t, 1, 1, 1, J400, uint8_t, 1, 1, 1)
TESTATOB(RAW, uint8_t, 3, 3, 1, ARGB, uint8_t, 4, 4, 1)
TESTATOB(RAW, uint8_t, 3, 3, 1, RGBA, uint8_t, 4, 4, 1)
TESTATOB(RAW, uint8_t, 3, 3, 1, RGB24, uint8_t, 3, 3, 1)
TESTATOB(RGB24, uint8_t, 3, 3, 1, ARGB, uint8_t, 4, 4, 1)
TESTATOB(RGB24, uint8_t, 3, 3, 1, J400, uint8_t, 1, 1, 1)
TESTATOB(RGB24, uint8_t, 3, 3, 1, RGB24Mirror, uint8_t, 3, 3, 1)
TESTATOB(RAW, uint8_t, 3, 3, 1, J400, uint8_t, 1, 1, 1)
#ifdef LITTLE_ENDIAN_ONLY_TEST
TESTATOB(RGB565, uint8_t, 2, 2, 1, ARGB, uint8_t, 4, 4, 1)
#endif
TESTATOB(RGBA, uint8_t, 4, 4, 1, ARGB, uint8_t, 4, 4, 1)
TESTATOB(UYVY, uint8_t, 2, 4, 1, ARGB, uint8_t, 4, 4, 1)
TESTATOB(YUY2, uint8_t, 2, 4, 1, ARGB, uint8_t, 4, 4, 1)
TESTATOB(YUY2, uint8_t, 2, 4, 1, Y, uint8_t, 1, 1, 1)
TESTATOB(ARGB, uint8_t, 4, 4, 1, AR64, uint16_t, 4, 4, 1)
TESTATOB(ARGB, uint8_t, 4, 4, 1, AB64, uint16_t, 4, 4, 1)
TESTATOB(ABGR, uint8_t, 4, 4, 1, AR64, uint16_t, 4, 4, 1)
TESTATOB(ABGR, uint8_t, 4, 4, 1, AB64, uint16_t, 4, 4, 1)
TESTATOB(AR64, uint16_t, 4, 4, 1, ARGB, uint8_t, 4, 4, 1)
TESTATOB(AB64, uint16_t, 4, 4, 1, ARGB, uint8_t, 4, 4, 1)
TESTATOB(AR64, uint16_t, 4, 4, 1, ABGR, uint8_t, 4, 4, 1)
TESTATOB(AB64, uint16_t, 4, 4, 1, ABGR, uint8_t, 4, 4, 1)
TESTATOB(AR64, uint16_t, 4, 4, 1, AB64, uint16_t, 4, 4, 1)
TESTATOB(AB64, uint16_t, 4, 4, 1, AR64, uint16_t, 4, 4, 1)

// in place test
#define TESTATOAI(FMT_A, TYPE_A, EPP_A, STRIDE_A, HEIGHT_A, FMT_B, TYPE_B,    \
                  EPP_B, STRIDE_B, HEIGHT_B, W1280, N, NEG, OFF)              \
  TEST_F(LibYUVConvertTest, FMT_A##To##FMT_B##N) {                            \
    const int kWidth = W1280;                                                 \
    const int kHeight = benchmark_height_;                                    \
    const int kHeightA = (kHeight + HEIGHT_A - 1) / HEIGHT_A * HEIGHT_A;      \
    const int kHeightB = (kHeight + HEIGHT_B - 1) / HEIGHT_B * HEIGHT_B;      \
    const int kStrideA =                                                      \
        (kWidth * EPP_A + STRIDE_A - 1) / STRIDE_A * STRIDE_A;                \
    const int kStrideB =                                                      \
        (kWidth * EPP_B + STRIDE_B - 1) / STRIDE_B * STRIDE_B;                \
    align_buffer_page_end(src_argb,                                           \
                          kStrideA* kHeightA*(int)sizeof(TYPE_A) + OFF);      \
    align_buffer_page_end(dst_argb_c,                                         \
                          kStrideA* kHeightA*(int)sizeof(TYPE_A) + OFF);      \
    align_buffer_page_end(dst_argb_opt,                                       \
                          kStrideA* kHeightA*(int)sizeof(TYPE_A) + OFF);      \
    for (int i = 0; i < kStrideA * kHeightA * (int)sizeof(TYPE_A); ++i) {     \
      src_argb[i + OFF] = (fastrand() & 0xff);                                \
    }                                                                         \
    memcpy(dst_argb_c + OFF, src_argb,                                        \
           kStrideA * kHeightA * (int)sizeof(TYPE_A));                        \
    memcpy(dst_argb_opt + OFF, src_argb,                                      \
           kStrideA * kHeightA * (int)sizeof(TYPE_A));                        \
    MaskCpuFlags(disable_cpu_flags_);                                         \
    FMT_A##To##FMT_B((TYPE_A*)(dst_argb_c /* src */ + OFF), kStrideA,         \
                     (TYPE_B*)dst_argb_c, kStrideB, kWidth, NEG kHeight);     \
    MaskCpuFlags(benchmark_cpu_info_);                                        \
    for (int i = 0; i < benchmark_iterations_; ++i) {                         \
      FMT_A##To##FMT_B((TYPE_A*)(dst_argb_opt /* src */ + OFF), kStrideA,     \
                       (TYPE_B*)dst_argb_opt, kStrideB, kWidth, NEG kHeight); \
    }                                                                         \
    memcpy(dst_argb_opt + OFF, src_argb,                                      \
           kStrideA * kHeightA * (int)sizeof(TYPE_A));                        \
    FMT_A##To##FMT_B((TYPE_A*)(dst_argb_opt /* src */ + OFF), kStrideA,       \
                     (TYPE_B*)dst_argb_opt, kStrideB, kWidth, NEG kHeight);   \
    for (int i = 0; i < kStrideB * kHeightB * (int)sizeof(TYPE_B); ++i) {     \
      EXPECT_EQ(dst_argb_c[i], dst_argb_opt[i]);                              \
    }                                                                         \
    free_aligned_buffer_page_end(src_argb);                                   \
    free_aligned_buffer_page_end(dst_argb_c);                                 \
    free_aligned_buffer_page_end(dst_argb_opt);                               \
  }

#define TESTATOA(FMT_A, TYPE_A, EPP_A, STRIDE_A, HEIGHT_A, FMT_B, TYPE_B,   \
                 EPP_B, STRIDE_B, HEIGHT_B)                                 \
  TESTATOAI(FMT_A, TYPE_A, EPP_A, STRIDE_A, HEIGHT_A, FMT_B, TYPE_B, EPP_B, \
            STRIDE_B, HEIGHT_B, benchmark_width_, _Inplace, +, 0)

TESTATOA(AB30, uint8_t, 4, 4, 1, ABGR, uint8_t, 4, 4, 1)
TESTATOA(AB30, uint8_t, 4, 4, 1, ARGB, uint8_t, 4, 4, 1)
#ifdef LITTLE_ENDIAN_ONLY_TEST
TESTATOA(ABGR, uint8_t, 4, 4, 1, AR30, uint8_t, 4, 4, 1)
#endif
TESTATOA(ABGR, uint8_t, 4, 4, 1, ARGB, uint8_t, 4, 4, 1)
#ifdef LITTLE_ENDIAN_ONLY_TEST
TESTATOA(AR30, uint8_t, 4, 4, 1, AB30, uint8_t, 4, 4, 1)
#endif
TESTATOA(AR30, uint8_t, 4, 4, 1, ABGR, uint8_t, 4, 4, 1)
#ifdef LITTLE_ENDIAN_ONLY_TEST
TESTATOA(AR30, uint8_t, 4, 4, 1, AR30, uint8_t, 4, 4, 1)
TESTATOA(AR30, uint8_t, 4, 4, 1, ARGB, uint8_t, 4, 4, 1)
#endif
TESTATOA(ARGB, uint8_t, 4, 4, 1, ABGR, uint8_t, 4, 4, 1)
#ifdef LITTLE_ENDIAN_ONLY_TEST
TESTATOA(ARGB, uint8_t, 4, 4, 1, AR30, uint8_t, 4, 4, 1)
#endif
TESTATOA(ARGB, uint8_t, 4, 4, 1, ARGB, uint8_t, 4, 4, 1)
TESTATOA(ARGB, uint8_t, 4, 4, 1, ARGB1555, uint8_t, 2, 2, 1)
TESTATOA(ARGB, uint8_t, 4, 4, 1, ARGB4444, uint8_t, 2, 2, 1)
// TODO(fbarchard): Support in place for mirror.
// TESTATOA(ARGB, uint8_t, 4, 4, 1, ARGBMirror, uint8_t, 4, 4, 1)
TESTATOA(ARGB, uint8_t, 4, 4, 1, BGRA, uint8_t, 4, 4, 1)
TESTATOA(ARGB, uint8_t, 4, 4, 1, I400, uint8_t, 1, 1, 1)
TESTATOA(ARGB, uint8_t, 4, 4, 1, J400, uint8_t, 1, 1, 1)
TESTATOA(RGBA, uint8_t, 4, 4, 1, J400, uint8_t, 1, 1, 1)
TESTATOA(ARGB, uint8_t, 4, 4, 1, RAW, uint8_t, 3, 3, 1)
TESTATOA(ARGB, uint8_t, 4, 4, 1, RGB24, uint8_t, 3, 3, 1)
TESTATOA(ABGR, uint8_t, 4, 4, 1, RAW, uint8_t, 3, 3, 1)
TESTATOA(ABGR, uint8_t, 4, 4, 1, RGB24, uint8_t, 3, 3, 1)
#ifdef LITTLE_ENDIAN_ONLY_TEST
TESTATOA(ARGB, uint8_t, 4, 4, 1, RGB565, uint8_t, 2, 2, 1)
#endif
TESTATOA(ARGB, uint8_t, 4, 4, 1, RGBA, uint8_t, 4, 4, 1)
TESTATOA(ARGB, uint8_t, 4, 4, 1, UYVY, uint8_t, 2, 4, 1)
TESTATOA(ARGB, uint8_t, 4, 4, 1, YUY2, uint8_t, 2, 4, 1)
// TODO(fbarchard): Support in place for conversions that increase bpp.
// TESTATOA(ARGB1555, uint8_t, 2, 2, 1, ARGB, uint8_t, 4, 4, 1)
// TESTATOA(ARGB4444, uint8_t, 2, 2, 1, ARGB, uint8_t, 4, 4, 1)
TESTATOA(BGRA, uint8_t, 4, 4, 1, ARGB, uint8_t, 4, 4, 1)
// TESTATOA(I400, uint8_t, 1, 1, 1, ARGB, uint8_t, 4, 4, 1)
TESTATOA(I400, uint8_t, 1, 1, 1, I400, uint8_t, 1, 1, 1)
// TESTATOA(I400, uint8_t, 1, 1, 1, I400Mirror, uint8_t, 1, 1, 1)
// TESTATOA(J400, uint8_t, 1, 1, 1, ARGB, uint8_t, 4, 4, 1)
TESTATOA(J400, uint8_t, 1, 1, 1, J400, uint8_t, 1, 1, 1)
// TESTATOA(RAW, uint8_t, 3, 3, 1, ARGB, uint8_t, 4, 4, 1)
// TESTATOA(RAW, uint8_t, 3, 3, 1, RGBA, uint8_t, 4, 4, 1)
TESTATOA(RAW, uint8_t, 3, 3, 1, RGB24, uint8_t, 3, 3, 1)
// TESTATOA(RGB24, uint8_t, 3, 3, 1, ARGB, uint8_t, 4, 4, 1)
TESTATOA(RGB24, uint8_t, 3, 3, 1, J400, uint8_t, 1, 1, 1)
// TESTATOA(RGB24, uint8_t, 3, 3, 1, RGB24Mirror, uint8_t, 3, 3, 1)
TESTATOA(RAW, uint8_t, 3, 3, 1, J400, uint8_t, 1, 1, 1)
#ifdef LITTLE_ENDIAN_ONLY_TEST
// TESTATOA(RGB565, uint8_t, 2, 2, 1, ARGB, uint8_t, 4, 4, 1)
#endif
TESTATOA(RGBA, uint8_t, 4, 4, 1, ARGB, uint8_t, 4, 4, 1)
// TESTATOA(UYVY, uint8_t, 2, 4, 1, ARGB, uint8_t, 4, 4, 1)
// TESTATOA(YUY2, uint8_t, 2, 4, 1, ARGB, uint8_t, 4, 4, 1)
TESTATOA(YUY2, uint8_t, 2, 4, 1, Y, uint8_t, 1, 1, 1)
// TESTATOA(ARGB, uint8_t, 4, 4, 1, AR64, uint16_t, 4, 4, 1)
// TESTATOA(ARGB, uint8_t, 4, 4, 1, AB64, uint16_t, 4, 4, 1)
// TESTATOA(ABGR, uint8_t, 4, 4, 1, AR64, uint16_t, 4, 4, 1)
// TESTATOA(ABGR, uint8_t, 4, 4, 1, AB64, uint16_t, 4, 4, 1)
TESTATOA(AR64, uint16_t, 4, 4, 1, ARGB, uint8_t, 4, 4, 1)
TESTATOA(AB64, uint16_t, 4, 4, 1, ARGB, uint8_t, 4, 4, 1)
TESTATOA(AR64, uint16_t, 4, 4, 1, ABGR, uint8_t, 4, 4, 1)
TESTATOA(AB64, uint16_t, 4, 4, 1, ABGR, uint8_t, 4, 4, 1)
TESTATOA(AR64, uint16_t, 4, 4, 1, AB64, uint16_t, 4, 4, 1)
TESTATOA(AB64, uint16_t, 4, 4, 1, AR64, uint16_t, 4, 4, 1)

#define TESTATOBDI(FMT_A, BPP_A, STRIDE_A, HEIGHT_A, FMT_B, BPP_B, STRIDE_B, \
                   HEIGHT_B, W1280, N, NEG, OFF)                             \
  TEST_F(LibYUVConvertTest, FMT_A##To##FMT_B##Dither##N) {                   \
    const int kWidth = W1280;                                                \
    const int kHeight = benchmark_height_;                                   \
    const int kHeightA = (kHeight + HEIGHT_A - 1) / HEIGHT_A * HEIGHT_A;     \
    const int kHeightB = (kHeight + HEIGHT_B - 1) / HEIGHT_B * HEIGHT_B;     \
    const int kStrideA =                                                     \
        (kWidth * BPP_A + STRIDE_A - 1) / STRIDE_A * STRIDE_A;               \
    const int kStrideB =                                                     \
        (kWidth * BPP_B + STRIDE_B - 1) / STRIDE_B * STRIDE_B;               \
    align_buffer_page_end(src_argb, kStrideA* kHeightA + OFF);               \
    align_buffer_page_end(dst_argb_c, kStrideB* kHeightB);                   \
    align_buffer_page_end(dst_argb_opt, kStrideB* kHeightB);                 \
    for (int i = 0; i < kStrideA * kHeightA; ++i) {                          \
      src_argb[i + OFF] = (fastrand() & 0xff);                               \
    }                                                                        \
    memset(dst_argb_c, 1, kStrideB* kHeightB);                               \
    memset(dst_argb_opt, 101, kStrideB* kHeightB);                           \
    MaskCpuFlags(disable_cpu_flags_);                                        \
    FMT_A##To##FMT_B##Dither(src_argb + OFF, kStrideA, dst_argb_c, kStrideB, \
                             NULL, kWidth, NEG kHeight);                     \
    MaskCpuFlags(benchmark_cpu_info_);                                       \
    for (int i = 0; i < benchmark_iterations_; ++i) {                        \
      FMT_A##To##FMT_B##Dither(src_argb + OFF, kStrideA, dst_argb_opt,       \
                               kStrideB, NULL, kWidth, NEG kHeight);         \
    }                                                                        \
    for (int i = 0; i < kStrideB * kHeightB; ++i) {                          \
      EXPECT_EQ(dst_argb_c[i], dst_argb_opt[i]);                             \
    }                                                                        \
    free_aligned_buffer_page_end(src_argb);                                  \
    free_aligned_buffer_page_end(dst_argb_c);                                \
    free_aligned_buffer_page_end(dst_argb_opt);                              \
  }

#define TESTATOBDRANDOM(FMT_A, BPP_A, STRIDE_A, HEIGHT_A, FMT_B, BPP_B,        \
                        STRIDE_B, HEIGHT_B)                                    \
  TEST_F(LibYUVConvertTest, FMT_A##To##FMT_B##Dither_Random) {                 \
    for (int times = 0; times < benchmark_iterations_; ++times) {              \
      const int kWidth = (fastrand() & 63) + 1;                                \
      const int kHeight = (fastrand() & 31) + 1;                               \
      const int kHeightA = (kHeight + HEIGHT_A - 1) / HEIGHT_A * HEIGHT_A;     \
      const int kHeightB = (kHeight + HEIGHT_B - 1) / HEIGHT_B * HEIGHT_B;     \
      const int kStrideA =                                                     \
          (kWidth * BPP_A + STRIDE_A - 1) / STRIDE_A * STRIDE_A;               \
      const int kStrideB =                                                     \
          (kWidth * BPP_B + STRIDE_B - 1) / STRIDE_B * STRIDE_B;               \
      align_buffer_page_end(src_argb, kStrideA* kHeightA);                     \
      align_buffer_page_end(dst_argb_c, kStrideB* kHeightB);                   \
      align_buffer_page_end(dst_argb_opt, kStrideB* kHeightB);                 \
      for (int i = 0; i < kStrideA * kHeightA; ++i) {                          \
        src_argb[i] = (fastrand() & 0xff);                                     \
      }                                                                        \
      memset(dst_argb_c, 123, kStrideB* kHeightB);                             \
      memset(dst_argb_opt, 123, kStrideB* kHeightB);                           \
      MaskCpuFlags(disable_cpu_flags_);                                        \
      FMT_A##To##FMT_B##Dither(src_argb, kStrideA, dst_argb_c, kStrideB, NULL, \
                               kWidth, kHeight);                               \
      MaskCpuFlags(benchmark_cpu_info_);                                       \
      FMT_A##To##FMT_B##Dither(src_argb, kStrideA, dst_argb_opt, kStrideB,     \
                               NULL, kWidth, kHeight);                         \
      for (int i = 0; i < kStrideB * kHeightB; ++i) {                          \
        EXPECT_EQ(dst_argb_c[i], dst_argb_opt[i]);                             \
      }                                                                        \
      free_aligned_buffer_page_end(src_argb);                                  \
      free_aligned_buffer_page_end(dst_argb_c);                                \
      free_aligned_buffer_page_end(dst_argb_opt);                              \
    }                                                                          \
  }

#if defined(ENABLE_FULL_TESTS)
#define TESTATOBD(FMT_A, BPP_A, STRIDE_A, HEIGHT_A, FMT_B, BPP_B, STRIDE_B, \
                  HEIGHT_B)                                                 \
  TESTATOBDI(FMT_A, BPP_A, STRIDE_A, HEIGHT_A, FMT_B, BPP_B, STRIDE_B,      \
             HEIGHT_B, benchmark_width_ + 1, _Any, +, 0)                    \
  TESTATOBDI(FMT_A, BPP_A, STRIDE_A, HEIGHT_A, FMT_B, BPP_B, STRIDE_B,      \
             HEIGHT_B, benchmark_width_, _Unaligned, +, 2)                  \
  TESTATOBDI(FMT_A, BPP_A, STRIDE_A, HEIGHT_A, FMT_B, BPP_B, STRIDE_B,      \
             HEIGHT_B, benchmark_width_, _Invert, -, 0)                     \
  TESTATOBDI(FMT_A, BPP_A, STRIDE_A, HEIGHT_A, FMT_B, BPP_B, STRIDE_B,      \
             HEIGHT_B, benchmark_width_, _Opt, +, 0)                        \
  TESTATOBDRANDOM(FMT_A, BPP_A, STRIDE_A, HEIGHT_A, FMT_B, BPP_B, STRIDE_B, \
                  HEIGHT_B)
#else
#define TESTATOBD(FMT_A, BPP_A, STRIDE_A, HEIGHT_A, FMT_B, BPP_B, STRIDE_B, \
                  HEIGHT_B)                                                 \
  TESTATOBDRANDOM(FMT_A, BPP_A, STRIDE_A, HEIGHT_A, FMT_B, BPP_B, STRIDE_B, \
                  HEIGHT_B)
#endif

#ifdef LITTLE_ENDIAN_ONLY_TEST
TESTATOBD(ARGB, 4, 4, 1, RGB565, 2, 2, 1)
#endif

// These conversions called twice, produce the original result.
// e.g. endian swap twice.
#define TESTENDI(FMT_ATOB, TYPE_A, EPP_A, STRIDE_A, HEIGHT_A, W1280, N, NEG,   \
                 OFF)                                                          \
  TEST_F(LibYUVConvertTest, FMT_ATOB##_Endswap##N) {                           \
    const int kWidth = W1280;                                                  \
    const int kHeight = benchmark_height_;                                     \
    const int kHeightA = (kHeight + HEIGHT_A - 1) / HEIGHT_A * HEIGHT_A;       \
    const int kStrideA =                                                       \
        (kWidth * EPP_A + STRIDE_A - 1) / STRIDE_A * STRIDE_A;                 \
    align_buffer_page_end(src_argb,                                            \
                          kStrideA* kHeightA*(int)sizeof(TYPE_A) + OFF);       \
    align_buffer_page_end(dst_argb_c, kStrideA* kHeightA*(int)sizeof(TYPE_A)); \
    align_buffer_page_end(dst_argb_opt,                                        \
                          kStrideA* kHeightA*(int)sizeof(TYPE_A));             \
    for (int i = 0; i < kStrideA * kHeightA * (int)sizeof(TYPE_A); ++i) {      \
      src_argb[i + OFF] = (fastrand() & 0xff);                                 \
    }                                                                          \
    memset(dst_argb_c, 1, kStrideA* kHeightA);                                 \
    memset(dst_argb_opt, 101, kStrideA* kHeightA);                             \
    MaskCpuFlags(disable_cpu_flags_);                                          \
    FMT_ATOB((TYPE_A*)(src_argb + OFF), kStrideA, (TYPE_A*)dst_argb_c,         \
             kStrideA, kWidth, NEG kHeight);                                   \
    MaskCpuFlags(benchmark_cpu_info_);                                         \
    for (int i = 0; i < benchmark_iterations_; ++i) {                          \
      FMT_ATOB((TYPE_A*)(src_argb + OFF), kStrideA, (TYPE_A*)dst_argb_opt,     \
               kStrideA, kWidth, NEG kHeight);                                 \
    }                                                                          \
    MaskCpuFlags(disable_cpu_flags_);                                          \
    FMT_ATOB((TYPE_A*)dst_argb_c, kStrideA, (TYPE_A*)dst_argb_c, kStrideA,     \
             kWidth, NEG kHeight);                                             \
    MaskCpuFlags(benchmark_cpu_info_);                                         \
    FMT_ATOB((TYPE_A*)dst_argb_opt, kStrideA, (TYPE_A*)dst_argb_opt, kStrideA, \
             kWidth, NEG kHeight);                                             \
    for (int i = 0; i < kStrideA * kHeightA * (int)sizeof(TYPE_A); ++i) {      \
      EXPECT_EQ(src_argb[i + OFF], dst_argb_opt[i]);                           \
      EXPECT_EQ(dst_argb_c[i], dst_argb_opt[i]);                               \
    }                                                                          \
    free_aligned_buffer_page_end(src_argb);                                    \
    free_aligned_buffer_page_end(dst_argb_c);                                  \
    free_aligned_buffer_page_end(dst_argb_opt);                                \
  }

#if defined(ENABLE_FULL_TESTS)
#define TESTEND(FMT_ATOB, TYPE_A, EPP_A, STRIDE_A, HEIGHT_A)                  \
  TESTENDI(FMT_ATOB, TYPE_A, EPP_A, STRIDE_A, HEIGHT_A, benchmark_width_ + 1, \
           _Any, +, 0)                                                        \
  TESTENDI(FMT_ATOB, TYPE_A, EPP_A, STRIDE_A, HEIGHT_A, benchmark_width_,     \
           _Unaligned, +, 2)                                                  \
  TESTENDI(FMT_ATOB, TYPE_A, EPP_A, STRIDE_A, HEIGHT_A, benchmark_width_,     \
           _Opt, +, 0)
#else
#define TESTEND(FMT_ATOB, TYPE_A, EPP_A, STRIDE_A, HEIGHT_A)              \
  TESTENDI(FMT_ATOB, TYPE_A, EPP_A, STRIDE_A, HEIGHT_A, benchmark_width_, \
           _Opt, +, 0)
#endif

TESTEND(ARGBToBGRA, uint8_t, 4, 4, 1)
TESTEND(ARGBToABGR, uint8_t, 4, 4, 1)
TESTEND(BGRAToARGB, uint8_t, 4, 4, 1)
TESTEND(ABGRToARGB, uint8_t, 4, 4, 1)
TESTEND(AB64ToAR64, uint16_t, 4, 4, 1)

#define TESTQPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN, \
                        YALIGN, W1280, N, NEG, OFF, ATTEN)                     \
  TEST_F(LibYUVConvertTest, FMT_PLANAR##To##FMT_B##N) {                        \
    const int kWidth = W1280;                                                  \
    const int kHeight = ALIGNINT(benchmark_height_, YALIGN);                   \
    const int kStrideB = ALIGNINT(kWidth * BPP_B, ALIGN);                      \
    const int kStrideUV = SUBSAMPLE(kWidth, SUBSAMP_X);                        \
    const int kSizeUV = kStrideUV * SUBSAMPLE(kHeight, SUBSAMP_Y);             \
    align_buffer_page_end(src_y, kWidth* kHeight + OFF);                       \
    align_buffer_page_end(src_u, kSizeUV + OFF);                               \
    align_buffer_page_end(src_v, kSizeUV + OFF);                               \
    align_buffer_page_end(src_a, kWidth* kHeight + OFF);                       \
    align_buffer_page_end(dst_argb_c, kStrideB* kHeight + OFF);                \
    align_buffer_page_end(dst_argb_opt, kStrideB* kHeight + OFF);              \
    for (int i = 0; i < kWidth * kHeight; ++i) {                               \
      src_y[i + OFF] = (fastrand() & 0xff);                                    \
      src_a[i + OFF] = (fastrand() & 0xff);                                    \
    }                                                                          \
    for (int i = 0; i < kSizeUV; ++i) {                                        \
      src_u[i + OFF] = (fastrand() & 0xff);                                    \
      src_v[i + OFF] = (fastrand() & 0xff);                                    \
    }                                                                          \
    memset(dst_argb_c + OFF, 1, kStrideB * kHeight);                           \
    memset(dst_argb_opt + OFF, 101, kStrideB * kHeight);                       \
    MaskCpuFlags(disable_cpu_flags_);                                          \
    FMT_PLANAR##To##FMT_B(src_y + OFF, kWidth, src_u + OFF, kStrideUV,         \
                          src_v + OFF, kStrideUV, src_a + OFF, kWidth,         \
                          dst_argb_c + OFF, kStrideB, kWidth, NEG kHeight,     \
                          ATTEN);                                              \
    MaskCpuFlags(benchmark_cpu_info_);                                         \
    for (int i = 0; i < benchmark_iterations_; ++i) {                          \
      FMT_PLANAR##To##FMT_B(src_y + OFF, kWidth, src_u + OFF, kStrideUV,       \
                            src_v + OFF, kStrideUV, src_a + OFF, kWidth,       \
                            dst_argb_opt + OFF, kStrideB, kWidth, NEG kHeight, \
                            ATTEN);                                            \
    }                                                                          \
    for (int i = 0; i < kWidth * BPP_B * kHeight; ++i) {                       \
      EXPECT_EQ(dst_argb_c[i + OFF], dst_argb_opt[i + OFF]);                   \
    }                                                                          \
    free_aligned_buffer_page_end(src_y);                                       \
    free_aligned_buffer_page_end(src_u);                                       \
    free_aligned_buffer_page_end(src_v);                                       \
    free_aligned_buffer_page_end(src_a);                                       \
    free_aligned_buffer_page_end(dst_argb_c);                                  \
    free_aligned_buffer_page_end(dst_argb_opt);                                \
  }

#if defined(ENABLE_FULL_TESTS)
#define TESTQPLANARTOB(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN, \
                       YALIGN)                                                \
  TESTQPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,      \
                  YALIGN, benchmark_width_ + 1, _Any, +, 0, 0)                \
  TESTQPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,      \
                  YALIGN, benchmark_width_, _Unaligned, +, 2, 0)              \
  TESTQPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,      \
                  YALIGN, benchmark_width_, _Invert, -, 0, 0)                 \
  TESTQPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,      \
                  YALIGN, benchmark_width_, _Opt, +, 0, 0)                    \
  TESTQPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,      \
                  YALIGN, benchmark_width_, _Premult, +, 0, 1)
#else
#define TESTQPLANARTOB(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN, \
                       YALIGN)                                                \
  TESTQPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,      \
                  YALIGN, benchmark_width_, _Opt, +, 0, 0)
#endif

#define J420AlphaToARGB(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I420AlphaToARGBMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvJPEGConstants, k, \
                        l, m)
#define J420AlphaToABGR(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I420AlphaToABGRMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvJPEGConstants, k, \
                        l, m)
#define F420AlphaToARGB(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I420AlphaToARGBMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvF709Constants, k, \
                        l, m)
#define F420AlphaToABGR(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I420AlphaToABGRMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvF709Constants, k, \
                        l, m)
#define H420AlphaToARGB(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I420AlphaToARGBMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvH709Constants, k, \
                        l, m)
#define H420AlphaToABGR(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I420AlphaToABGRMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvH709Constants, k, \
                        l, m)
#define U420AlphaToARGB(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I420AlphaToARGBMatrix(a, b, c, d, e, f, g, h, i, j, &kYuv2020Constants, k, \
                        l, m)
#define U420AlphaToABGR(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I420AlphaToABGRMatrix(a, b, c, d, e, f, g, h, i, j, &kYuv2020Constants, k, \
                        l, m)
#define V420AlphaToARGB(a, b, c, d, e, f, g, h, i, j, k, l, m)                \
  I420AlphaToARGBMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvV2020Constants, k, \
                        l, m)
#define V420AlphaToABGR(a, b, c, d, e, f, g, h, i, j, k, l, m)                \
  I420AlphaToABGRMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvV2020Constants, k, \
                        l, m)
#define J422AlphaToARGB(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I422AlphaToARGBMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvJPEGConstants, k, \
                        l, m)
#define J422AlphaToABGR(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I422AlphaToABGRMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvJPEGConstants, k, \
                        l, m)
#define F422AlphaToARGB(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I422AlphaToARGBMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvF709Constants, k, \
                        l, m)
#define F422AlphaToABGR(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I422AlphaToABGRMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvF709Constants, k, \
                        l, m)
#define H422AlphaToARGB(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I422AlphaToARGBMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvH709Constants, k, \
                        l, m)
#define H422AlphaToABGR(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I422AlphaToABGRMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvH709Constants, k, \
                        l, m)
#define U422AlphaToARGB(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I422AlphaToARGBMatrix(a, b, c, d, e, f, g, h, i, j, &kYuv2020Constants, k, \
                        l, m)
#define U422AlphaToABGR(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I422AlphaToABGRMatrix(a, b, c, d, e, f, g, h, i, j, &kYuv2020Constants, k, \
                        l, m)
#define V422AlphaToARGB(a, b, c, d, e, f, g, h, i, j, k, l, m)                \
  I422AlphaToARGBMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvV2020Constants, k, \
                        l, m)
#define V422AlphaToABGR(a, b, c, d, e, f, g, h, i, j, k, l, m)                \
  I422AlphaToABGRMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvV2020Constants, k, \
                        l, m)
#define J444AlphaToARGB(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I444AlphaToARGBMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvJPEGConstants, k, \
                        l, m)
#define J444AlphaToABGR(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I444AlphaToABGRMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvJPEGConstants, k, \
                        l, m)
#define F444AlphaToARGB(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I444AlphaToARGBMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvF709Constants, k, \
                        l, m)
#define F444AlphaToABGR(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I444AlphaToABGRMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvF709Constants, k, \
                        l, m)
#define H444AlphaToARGB(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I444AlphaToARGBMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvH709Constants, k, \
                        l, m)
#define H444AlphaToABGR(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I444AlphaToABGRMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvH709Constants, k, \
                        l, m)
#define U444AlphaToARGB(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I444AlphaToARGBMatrix(a, b, c, d, e, f, g, h, i, j, &kYuv2020Constants, k, \
                        l, m)
#define U444AlphaToABGR(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I444AlphaToABGRMatrix(a, b, c, d, e, f, g, h, i, j, &kYuv2020Constants, k, \
                        l, m)
#define V444AlphaToARGB(a, b, c, d, e, f, g, h, i, j, k, l, m)                \
  I444AlphaToARGBMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvV2020Constants, k, \
                        l, m)
#define V444AlphaToABGR(a, b, c, d, e, f, g, h, i, j, k, l, m)                \
  I444AlphaToABGRMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvV2020Constants, k, \
                        l, m)

#define I420AlphaToARGBFilter(a, b, c, d, e, f, g, h, i, j, k, l, m) \
  I420AlphaToARGBMatrixFilter(a, b, c, d, e, f, g, h, i, j,          \
                              &kYuvI601Constants, k, l, m, kFilterBilinear)
#define I422AlphaToARGBFilter(a, b, c, d, e, f, g, h, i, j, k, l, m) \
  I422AlphaToARGBMatrixFilter(a, b, c, d, e, f, g, h, i, j,          \
                              &kYuvI601Constants, k, l, m, kFilterBilinear)

#if defined(ENABLE_FULL_TESTS)
TESTQPLANARTOB(I420Alpha, 2, 2, ARGB, 4, 4, 1)
TESTQPLANARTOB(I420Alpha, 2, 2, ABGR, 4, 4, 1)
TESTQPLANARTOB(J420Alpha, 2, 2, ARGB, 4, 4, 1)
TESTQPLANARTOB(J420Alpha, 2, 2, ABGR, 4, 4, 1)
TESTQPLANARTOB(H420Alpha, 2, 2, ARGB, 4, 4, 1)
TESTQPLANARTOB(H420Alpha, 2, 2, ABGR, 4, 4, 1)
TESTQPLANARTOB(F420Alpha, 2, 2, ARGB, 4, 4, 1)
TESTQPLANARTOB(F420Alpha, 2, 2, ABGR, 4, 4, 1)
TESTQPLANARTOB(U420Alpha, 2, 2, ARGB, 4, 4, 1)
TESTQPLANARTOB(U420Alpha, 2, 2, ABGR, 4, 4, 1)
TESTQPLANARTOB(V420Alpha, 2, 2, ARGB, 4, 4, 1)
TESTQPLANARTOB(V420Alpha, 2, 2, ABGR, 4, 4, 1)
TESTQPLANARTOB(I422Alpha, 2, 1, ARGB, 4, 4, 1)
TESTQPLANARTOB(I422Alpha, 2, 1, ABGR, 4, 4, 1)
TESTQPLANARTOB(J422Alpha, 2, 1, ARGB, 4, 4, 1)
TESTQPLANARTOB(J422Alpha, 2, 1, ABGR, 4, 4, 1)
TESTQPLANARTOB(H422Alpha, 2, 1, ARGB, 4, 4, 1)
TESTQPLANARTOB(H422Alpha, 2, 1, ABGR, 4, 4, 1)
TESTQPLANARTOB(F422Alpha, 2, 1, ARGB, 4, 4, 1)
TESTQPLANARTOB(F422Alpha, 2, 1, ABGR, 4, 4, 1)
TESTQPLANARTOB(U422Alpha, 2, 1, ARGB, 4, 4, 1)
TESTQPLANARTOB(U422Alpha, 2, 1, ABGR, 4, 4, 1)
TESTQPLANARTOB(V422Alpha, 2, 1, ARGB, 4, 4, 1)
TESTQPLANARTOB(V422Alpha, 2, 1, ABGR, 4, 4, 1)
TESTQPLANARTOB(I444Alpha, 1, 1, ARGB, 4, 4, 1)
TESTQPLANARTOB(I444Alpha, 1, 1, ABGR, 4, 4, 1)
TESTQPLANARTOB(J444Alpha, 1, 1, ARGB, 4, 4, 1)
TESTQPLANARTOB(J444Alpha, 1, 1, ABGR, 4, 4, 1)
TESTQPLANARTOB(H444Alpha, 1, 1, ARGB, 4, 4, 1)
TESTQPLANARTOB(H444Alpha, 1, 1, ABGR, 4, 4, 1)
TESTQPLANARTOB(F444Alpha, 1, 1, ARGB, 4, 4, 1)
TESTQPLANARTOB(F444Alpha, 1, 1, ABGR, 4, 4, 1)
TESTQPLANARTOB(U444Alpha, 1, 1, ARGB, 4, 4, 1)
TESTQPLANARTOB(U444Alpha, 1, 1, ABGR, 4, 4, 1)
TESTQPLANARTOB(V444Alpha, 1, 1, ARGB, 4, 4, 1)
TESTQPLANARTOB(V444Alpha, 1, 1, ABGR, 4, 4, 1)
TESTQPLANARTOB(I420Alpha, 2, 2, ARGBFilter, 4, 4, 1)
TESTQPLANARTOB(I422Alpha, 2, 1, ARGBFilter, 4, 4, 1)
#else
TESTQPLANARTOB(I420Alpha, 2, 2, ARGB, 4, 4, 1)
TESTQPLANARTOB(I422Alpha, 2, 1, ARGB, 4, 4, 1)
TESTQPLANARTOB(I444Alpha, 1, 1, ARGB, 4, 4, 1)
TESTQPLANARTOB(I420Alpha, 2, 2, ARGBFilter, 4, 4, 1)
TESTQPLANARTOB(I422Alpha, 2, 1, ARGBFilter, 4, 4, 1)
#endif

TEST_F(LibYUVConvertTest, TestYToARGB) {
  uint8_t y[32];
  uint8_t expectedg[32];
  for (int i = 0; i < 32; ++i) {
    y[i] = i * 5 + 17;
    expectedg[i] = static_cast<int>((y[i] - 16) * 1.164f + 0.5f);
  }
  uint8_t argb[32 * 4];
  YToARGB(y, 0, argb, 0, 32, 1);

  for (int i = 0; i < 32; ++i) {
    printf("%2d %d: %d <-> %d,%d,%d,%d\n", i, y[i], expectedg[i],
           argb[i * 4 + 0], argb[i * 4 + 1], argb[i * 4 + 2], argb[i * 4 + 3]);
  }
  for (int i = 0; i < 32; ++i) {
    EXPECT_EQ(expectedg[i], argb[i * 4 + 0]);
  }
}

static const uint8_t kNoDither4x4[16] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

TEST_F(LibYUVConvertTest, TestNoDither) {
  align_buffer_page_end(src_argb, benchmark_width_ * benchmark_height_ * 4);
  align_buffer_page_end(dst_rgb565, benchmark_width_ * benchmark_height_ * 2);
  align_buffer_page_end(dst_rgb565dither,
                        benchmark_width_ * benchmark_height_ * 2);
  MemRandomize(src_argb, benchmark_width_ * benchmark_height_ * 4);
  MemRandomize(dst_rgb565, benchmark_width_ * benchmark_height_ * 2);
  MemRandomize(dst_rgb565dither, benchmark_width_ * benchmark_height_ * 2);
  ARGBToRGB565(src_argb, benchmark_width_ * 4, dst_rgb565, benchmark_width_ * 2,
               benchmark_width_, benchmark_height_);
  ARGBToRGB565Dither(src_argb, benchmark_width_ * 4, dst_rgb565dither,
                     benchmark_width_ * 2, kNoDither4x4, benchmark_width_,
                     benchmark_height_);
  for (int i = 0; i < benchmark_width_ * benchmark_height_ * 2; ++i) {
    EXPECT_EQ(dst_rgb565[i], dst_rgb565dither[i]);
  }

  free_aligned_buffer_page_end(src_argb);
  free_aligned_buffer_page_end(dst_rgb565);
  free_aligned_buffer_page_end(dst_rgb565dither);
}

// Ordered 4x4 dither for 888 to 565.  Values from 0 to 7.
static const uint8_t kDither565_4x4[16] = {
    0, 4, 1, 5, 6, 2, 7, 3, 1, 5, 0, 4, 7, 3, 6, 2,
};

TEST_F(LibYUVConvertTest, TestDither) {
  align_buffer_page_end(src_argb, benchmark_width_ * benchmark_height_ * 4);
  align_buffer_page_end(dst_rgb565, benchmark_width_ * benchmark_height_ * 2);
  align_buffer_page_end(dst_rgb565dither,
                        benchmark_width_ * benchmark_height_ * 2);
  align_buffer_page_end(dst_argb, benchmark_width_ * benchmark_height_ * 4);
  align_buffer_page_end(dst_argbdither,
                        benchmark_width_ * benchmark_height_ * 4);
  MemRandomize(src_argb, benchmark_width_ * benchmark_height_ * 4);
  MemRandomize(dst_rgb565, benchmark_width_ * benchmark_height_ * 2);
  MemRandomize(dst_rgb565dither, benchmark_width_ * benchmark_height_ * 2);
  MemRandomize(dst_argb, benchmark_width_ * benchmark_height_ * 4);
  MemRandomize(dst_argbdither, benchmark_width_ * benchmark_height_ * 4);
  ARGBToRGB565(src_argb, benchmark_width_ * 4, dst_rgb565, benchmark_width_ * 2,
               benchmark_width_, benchmark_height_);
  ARGBToRGB565Dither(src_argb, benchmark_width_ * 4, dst_rgb565dither,
                     benchmark_width_ * 2, kDither565_4x4, benchmark_width_,
                     benchmark_height_);
  RGB565ToARGB(dst_rgb565, benchmark_width_ * 2, dst_argb, benchmark_width_ * 4,
               benchmark_width_, benchmark_height_);
  RGB565ToARGB(dst_rgb565dither, benchmark_width_ * 2, dst_argbdither,
               benchmark_width_ * 4, benchmark_width_, benchmark_height_);

  for (int i = 0; i < benchmark_width_ * benchmark_height_ * 4; ++i) {
    EXPECT_NEAR(dst_argb[i], dst_argbdither[i], 9);
  }
  free_aligned_buffer_page_end(src_argb);
  free_aligned_buffer_page_end(dst_rgb565);
  free_aligned_buffer_page_end(dst_rgb565dither);
  free_aligned_buffer_page_end(dst_argb);
  free_aligned_buffer_page_end(dst_argbdither);
}

#define TESTPLANARTOBID(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN, \
                        YALIGN, W1280, N, NEG, OFF, FMT_C, BPP_C)              \
  TEST_F(LibYUVConvertTest, FMT_PLANAR##To##FMT_B##Dither##N) {                \
    const int kWidth = W1280;                                                  \
    const int kHeight = ALIGNINT(benchmark_height_, YALIGN);                   \
    const int kStrideB = ALIGNINT(kWidth * BPP_B, ALIGN);                      \
    const int kStrideUV = SUBSAMPLE(kWidth, SUBSAMP_X);                        \
    const int kSizeUV = kStrideUV * SUBSAMPLE(kHeight, SUBSAMP_Y);             \
    align_buffer_page_end(src_y, kWidth* kHeight + OFF);                       \
    align_buffer_page_end(src_u, kSizeUV + OFF);                               \
    align_buffer_page_end(src_v, kSizeUV + OFF);                               \
    align_buffer_page_end(dst_argb_c, kStrideB* kHeight + OFF);                \
    align_buffer_page_end(dst_argb_opt, kStrideB* kHeight + OFF);              \
    for (int i = 0; i < kWidth * kHeight; ++i) {                               \
      src_y[i + OFF] = (fastrand() & 0xff);                                    \
    }                                                                          \
    for (int i = 0; i < kSizeUV; ++i) {                                        \
      src_u[i + OFF] = (fastrand() & 0xff);                                    \
      src_v[i + OFF] = (fastrand() & 0xff);                                    \
    }                                                                          \
    memset(dst_argb_c + OFF, 1, kStrideB * kHeight);                           \
    memset(dst_argb_opt + OFF, 101, kStrideB * kHeight);                       \
    MaskCpuFlags(disable_cpu_flags_);                                          \
    FMT_PLANAR##To##FMT_B##Dither(src_y + OFF, kWidth, src_u + OFF, kStrideUV, \
                                  src_v + OFF, kStrideUV, dst_argb_c + OFF,    \
                                  kStrideB, NULL, kWidth, NEG kHeight);        \
    MaskCpuFlags(benchmark_cpu_info_);                                         \
    for (int i = 0; i < benchmark_iterations_; ++i) {                          \
      FMT_PLANAR##To##FMT_B##Dither(                                           \
          src_y + OFF, kWidth, src_u + OFF, kStrideUV, src_v + OFF, kStrideUV, \
          dst_argb_opt + OFF, kStrideB, NULL, kWidth, NEG kHeight);            \
    }                                                                          \
    /* Convert to ARGB so 565 is expanded to bytes that can be compared. */    \
    align_buffer_page_end(dst_argb32_c, kWidth* BPP_C* kHeight);               \
    align_buffer_page_end(dst_argb32_opt, kWidth* BPP_C* kHeight);             \
    memset(dst_argb32_c, 2, kWidth* BPP_C* kHeight);                           \
    memset(dst_argb32_opt, 102, kWidth* BPP_C* kHeight);                       \
    FMT_B##To##FMT_C(dst_argb_c + OFF, kStrideB, dst_argb32_c, kWidth * BPP_C, \
                     kWidth, kHeight);                                         \
    FMT_B##To##FMT_C(dst_argb_opt + OFF, kStrideB, dst_argb32_opt,             \
                     kWidth * BPP_C, kWidth, kHeight);                         \
    for (int i = 0; i < kWidth * BPP_C * kHeight; ++i) {                       \
      EXPECT_EQ(dst_argb32_c[i], dst_argb32_opt[i]);                           \
    }                                                                          \
    free_aligned_buffer_page_end(src_y);                                       \
    free_aligned_buffer_page_end(src_u);                                       \
    free_aligned_buffer_page_end(src_v);                                       \
    free_aligned_buffer_page_end(dst_argb_c);                                  \
    free_aligned_buffer_page_end(dst_argb_opt);                                \
    free_aligned_buffer_page_end(dst_argb32_c);                                \
    free_aligned_buffer_page_end(dst_argb32_opt);                              \
  }

#if defined(ENABLE_FULL_TESTS)
#define TESTPLANARTOBD(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN, \
                       YALIGN, FMT_C, BPP_C)                                  \
  TESTPLANARTOBID(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,      \
                  YALIGN, benchmark_width_ + 1, _Any, +, 0, FMT_C, BPP_C)     \
  TESTPLANARTOBID(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,      \
                  YALIGN, benchmark_width_, _Unaligned, +, 2, FMT_C, BPP_C)   \
  TESTPLANARTOBID(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,      \
                  YALIGN, benchmark_width_, _Invert, -, 0, FMT_C, BPP_C)      \
  TESTPLANARTOBID(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,      \
                  YALIGN, benchmark_width_, _Opt, +, 0, FMT_C, BPP_C)
#else
#define TESTPLANARTOBD(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN, \
                       YALIGN, FMT_C, BPP_C)                                  \
  TESTPLANARTOBID(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,      \
                  YALIGN, benchmark_width_, _Opt, +, 0, FMT_C, BPP_C)
#endif

#ifdef LITTLE_ENDIAN_ONLY_TEST
TESTPLANARTOBD(I420, 2, 2, RGB565, 2, 2, 1, ARGB, 4)
#endif

// Transitive test.  A to B to C is same as A to C.
// Benchmarks A To B to C for comparison to 1 step, benchmarked elsewhere.
#define TESTPLANARTOEI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, SUB_B, BPP_B, \
                       W1280, N, NEG, OFF, FMT_C, BPP_C)                      \
  TEST_F(LibYUVConvertTest, FMT_PLANAR##To##FMT_B##To##FMT_C##N) {            \
    const int kWidth = W1280;                                                 \
    const int kHeight = benchmark_height_;                                    \
    const int kStrideB = SUBSAMPLE(kWidth, SUB_B) * BPP_B;                    \
    const int kStrideUV = SUBSAMPLE(kWidth, SUBSAMP_X);                       \
    const int kSizeUV = kStrideUV * SUBSAMPLE(kHeight, SUBSAMP_Y);            \
    align_buffer_page_end(src_y, kWidth* kHeight + OFF);                      \
    align_buffer_page_end(src_u, kSizeUV + OFF);                              \
    align_buffer_page_end(src_v, kSizeUV + OFF);                              \
    align_buffer_page_end(dst_argb_b, kStrideB* kHeight + OFF);               \
    for (int i = 0; i < kWidth * kHeight; ++i) {                              \
      src_y[i + OFF] = (fastrand() & 0xff);                                   \
    }                                                                         \
    for (int i = 0; i < kSizeUV; ++i) {                                       \
      src_u[i + OFF] = (fastrand() & 0xff);                                   \
      src_v[i + OFF] = (fastrand() & 0xff);                                   \
    }                                                                         \
    memset(dst_argb_b + OFF, 1, kStrideB * kHeight);                          \
    FMT_PLANAR##To##FMT_B(src_y + OFF, kWidth, src_u + OFF, kStrideUV,        \
                          src_v + OFF, kStrideUV, dst_argb_b + OFF, kStrideB, \
                          kWidth, NEG kHeight);                               \
    /* Convert to a 3rd format in 1 step and 2 steps and compare  */          \
    const int kStrideC = kWidth * BPP_C;                                      \
    align_buffer_page_end(dst_argb_c, kStrideC* kHeight + OFF);               \
    align_buffer_page_end(dst_argb_bc, kStrideC* kHeight + OFF);              \
    memset(dst_argb_c + OFF, 2, kStrideC * kHeight);                          \
    memset(dst_argb_bc + OFF, 3, kStrideC * kHeight);                         \
    for (int i = 0; i < benchmark_iterations_; ++i) {                         \
      FMT_PLANAR##To##FMT_C(src_y + OFF, kWidth, src_u + OFF, kStrideUV,      \
                            src_v + OFF, kStrideUV, dst_argb_c + OFF,         \
                            kStrideC, kWidth, NEG kHeight);                   \
      /* Convert B to C */                                                    \
      FMT_B##To##FMT_C(dst_argb_b + OFF, kStrideB, dst_argb_bc + OFF,         \
                       kStrideC, kWidth, kHeight);                            \
    }                                                                         \
    for (int i = 0; i < kStrideC * kHeight; ++i) {                            \
      EXPECT_EQ(dst_argb_c[i + OFF], dst_argb_bc[i + OFF]);                   \
    }                                                                         \
    free_aligned_buffer_page_end(src_y);                                      \
    free_aligned_buffer_page_end(src_u);                                      \
    free_aligned_buffer_page_end(src_v);                                      \
    free_aligned_buffer_page_end(dst_argb_b);                                 \
    free_aligned_buffer_page_end(dst_argb_c);                                 \
    free_aligned_buffer_page_end(dst_argb_bc);                                \
  }

#if defined(ENABLE_FULL_TESTS)
#define TESTPLANARTOE(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, SUB_B, BPP_B, \
                      FMT_C, BPP_C)                                          \
  TESTPLANARTOEI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, SUB_B, BPP_B,      \
                 benchmark_width_ + 1, _Any, +, 0, FMT_C, BPP_C)             \
  TESTPLANARTOEI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, SUB_B, BPP_B,      \
                 benchmark_width_, _Unaligned, +, 2, FMT_C, BPP_C)           \
  TESTPLANARTOEI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, SUB_B, BPP_B,      \
                 benchmark_width_, _Invert, -, 0, FMT_C, BPP_C)              \
  TESTPLANARTOEI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, SUB_B, BPP_B,      \
                 benchmark_width_, _Opt, +, 0, FMT_C, BPP_C)
#else
#define TESTPLANARTOE(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, SUB_B, BPP_B, \
                      FMT_C, BPP_C)                                          \
  TESTPLANARTOEI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, SUB_B, BPP_B,      \
                 benchmark_width_, _Opt, +, 0, FMT_C, BPP_C)
#endif

#if defined(ENABLE_FULL_TESTS)
TESTPLANARTOE(I420, 2, 2, ABGR, 1, 4, ARGB, 4)
TESTPLANARTOE(I420, 2, 2, ARGB, 1, 4, ABGR, 4)
TESTPLANARTOE(I420, 2, 2, ARGB, 1, 4, RAW, 3)
TESTPLANARTOE(I420, 2, 2, ARGB, 1, 4, RGB24, 3)
TESTPLANARTOE(I420, 2, 2, BGRA, 1, 4, ARGB, 4)
TESTPLANARTOE(I420, 2, 2, RAW, 1, 3, ARGB, 4)
TESTPLANARTOE(I420, 2, 2, RAW, 1, 3, RGB24, 3)
TESTPLANARTOE(I420, 2, 2, RGB24, 1, 3, ARGB, 4)
TESTPLANARTOE(I420, 2, 2, RGB24, 1, 3, RAW, 3)
TESTPLANARTOE(I420, 2, 2, RGBA, 1, 4, ARGB, 4)
TESTPLANARTOE(H420, 2, 2, ABGR, 1, 4, ARGB, 4)
TESTPLANARTOE(H420, 2, 2, ARGB, 1, 4, ABGR, 4)
TESTPLANARTOE(H420, 2, 2, ARGB, 1, 4, RAW, 3)
TESTPLANARTOE(H420, 2, 2, ARGB, 1, 4, RGB24, 3)
TESTPLANARTOE(H420, 2, 2, RAW, 1, 3, ARGB, 4)
TESTPLANARTOE(H420, 2, 2, RAW, 1, 3, RGB24, 3)
TESTPLANARTOE(H420, 2, 2, RGB24, 1, 3, ARGB, 4)
TESTPLANARTOE(H420, 2, 2, RGB24, 1, 3, RAW, 3)
TESTPLANARTOE(J420, 2, 2, ABGR, 1, 4, ARGB, 4)
TESTPLANARTOE(J420, 2, 2, ARGB, 1, 4, ARGB, 4)
TESTPLANARTOE(U420, 2, 2, ABGR, 1, 4, ARGB, 4)
TESTPLANARTOE(U420, 2, 2, ARGB, 1, 4, ARGB, 4)
#ifdef LITTLE_ENDIAN_ONLY_TEST
TESTPLANARTOE(I420, 2, 2, ARGB, 1, 4, RGB565, 2)
TESTPLANARTOE(I420, 2, 2, ARGB, 1, 4, ARGB1555, 2)
TESTPLANARTOE(I420, 2, 2, ARGB, 1, 4, ARGB4444, 2)
TESTPLANARTOE(I422, 2, 1, ARGB, 1, 4, RGB565, 2)
#endif
TESTPLANARTOE(I422, 2, 1, ARGB, 1, 4, ABGR, 4)
TESTPLANARTOE(I422, 2, 1, ABGR, 1, 4, ARGB, 4)
TESTPLANARTOE(J422, 2, 1, ARGB, 1, 4, ARGB, 4)
TESTPLANARTOE(J422, 2, 1, ABGR, 1, 4, ARGB, 4)
TESTPLANARTOE(H422, 2, 1, ARGB, 1, 4, ARGB, 4)
TESTPLANARTOE(H422, 2, 1, ABGR, 1, 4, ARGB, 4)
TESTPLANARTOE(U422, 2, 1, ARGB, 1, 4, ARGB, 4)
TESTPLANARTOE(U422, 2, 1, ABGR, 1, 4, ARGB, 4)
TESTPLANARTOE(V422, 2, 1, ARGB, 1, 4, ARGB, 4)
TESTPLANARTOE(V422, 2, 1, ABGR, 1, 4, ARGB, 4)
TESTPLANARTOE(I422, 2, 1, BGRA, 1, 4, ARGB, 4)
TESTPLANARTOE(I422, 2, 1, RGBA, 1, 4, ARGB, 4)
TESTPLANARTOE(I444, 1, 1, ARGB, 1, 4, ABGR, 4)
TESTPLANARTOE(I444, 1, 1, ABGR, 1, 4, ARGB, 4)
TESTPLANARTOE(J444, 1, 1, ARGB, 1, 4, ARGB, 4)
TESTPLANARTOE(J444, 1, 1, ABGR, 1, 4, ARGB, 4)
TESTPLANARTOE(H444, 1, 1, ARGB, 1, 4, ARGB, 4)
TESTPLANARTOE(H444, 1, 1, ABGR, 1, 4, ARGB, 4)
TESTPLANARTOE(U444, 1, 1, ARGB, 1, 4, ARGB, 4)
TESTPLANARTOE(U444, 1, 1, ABGR, 1, 4, ARGB, 4)
TESTPLANARTOE(V444, 1, 1, ARGB, 1, 4, ARGB, 4)
TESTPLANARTOE(V444, 1, 1, ABGR, 1, 4, ARGB, 4)
TESTPLANARTOE(I420, 2, 2, YUY2, 2, 4, ARGB, 4)
TESTPLANARTOE(I420, 2, 2, UYVY, 2, 4, ARGB, 4)
TESTPLANARTOE(I422, 2, 1, YUY2, 2, 4, ARGB, 4)
TESTPLANARTOE(I422, 2, 1, UYVY, 2, 4, ARGB, 4)
#else
TESTPLANARTOE(I420, 2, 2, ABGR, 1, 4, ARGB, 4)
TESTPLANARTOE(I420, 2, 2, ARGB, 1, 4, ARGB1555, 2)
TESTPLANARTOE(I420, 2, 2, ARGB, 1, 4, ARGB4444, 2)
TESTPLANARTOE(I420, 2, 2, ARGB, 1, 4, RAW, 3)
TESTPLANARTOE(I420, 2, 2, ARGB, 1, 4, RGB24, 3)
TESTPLANARTOE(I420, 2, 2, ARGB, 1, 4, RGB565, 2)
TESTPLANARTOE(I420, 2, 2, BGRA, 1, 4, ARGB, 4)
TESTPLANARTOE(I420, 2, 2, RAW, 1, 3, ARGB, 4)
TESTPLANARTOE(I420, 2, 2, RAW, 1, 3, RGB24, 3)
TESTPLANARTOE(I420, 2, 2, RGB24, 1, 3, ARGB, 4)
TESTPLANARTOE(I420, 2, 2, RGB24, 1, 3, RAW, 3)
TESTPLANARTOE(I420, 2, 2, RGBA, 1, 4, ARGB, 4)
TESTPLANARTOE(I420, 2, 2, UYVY, 2, 4, ARGB, 4)
TESTPLANARTOE(I420, 2, 2, YUY2, 2, 4, ARGB, 4)
TESTPLANARTOE(I422, 2, 1, ABGR, 1, 4, ARGB, 4)
TESTPLANARTOE(I422, 2, 1, ARGB, 1, 4, RGB565, 2)
TESTPLANARTOE(I422, 2, 1, BGRA, 1, 4, ARGB, 4)
TESTPLANARTOE(I422, 2, 1, RGBA, 1, 4, ARGB, 4)
TESTPLANARTOE(I422, 2, 1, UYVY, 2, 4, ARGB, 4)
TESTPLANARTOE(I422, 2, 1, YUY2, 2, 4, ARGB, 4)
TESTPLANARTOE(I444, 1, 1, ABGR, 1, 4, ARGB, 4)
#endif

// Transitive test: Compare 1 step vs 2 step conversion for YUVA to ARGB.
// Benchmark 2 step conversion for comparison to 1 step conversion.
#define TESTQPLANARTOEI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, SUB_B, BPP_B, \
                        W1280, N, NEG, OFF, FMT_C, BPP_C, ATTEN)               \
  TEST_F(LibYUVConvertTest, FMT_PLANAR##To##FMT_B##To##FMT_C##N) {             \
    const int kWidth = W1280;                                                  \
    const int kHeight = benchmark_height_;                                     \
    const int kStrideB = SUBSAMPLE(kWidth, SUB_B) * BPP_B;                     \
    const int kSizeUV =                                                        \
        SUBSAMPLE(kWidth, SUBSAMP_X) * SUBSAMPLE(kHeight, SUBSAMP_Y);          \
    align_buffer_page_end(src_y, kWidth* kHeight + OFF);                       \
    align_buffer_page_end(src_u, kSizeUV + OFF);                               \
    align_buffer_page_end(src_v, kSizeUV + OFF);                               \
    align_buffer_page_end(src_a, kWidth* kHeight + OFF);                       \
    align_buffer_page_end(dst_argb_b, kStrideB* kHeight + OFF);                \
    const int kStrideC = kWidth * BPP_C;                                       \
    align_buffer_page_end(dst_argb_c, kStrideC* kHeight + OFF);                \
    align_buffer_page_end(dst_argb_bc, kStrideC* kHeight + OFF);               \
    memset(dst_argb_c + OFF, 2, kStrideC * kHeight);                           \
    memset(dst_argb_b + OFF, 1, kStrideB * kHeight);                           \
    memset(dst_argb_bc + OFF, 3, kStrideC * kHeight);                          \
    for (int i = 0; i < kWidth * kHeight; ++i) {                               \
      src_y[i + OFF] = (fastrand() & 0xff);                                    \
      src_a[i + OFF] = (fastrand() & 0xff);                                    \
    }                                                                          \
    for (int i = 0; i < kSizeUV; ++i) {                                        \
      src_u[i + OFF] = (fastrand() & 0xff);                                    \
      src_v[i + OFF] = (fastrand() & 0xff);                                    \
    }                                                                          \
    for (int i = 0; i < benchmark_iterations_; ++i) {                          \
      /* Convert A to B */                                                     \
      FMT_PLANAR##To##FMT_B(                                                   \
          src_y + OFF, kWidth, src_u + OFF, SUBSAMPLE(kWidth, SUBSAMP_X),      \
          src_v + OFF, SUBSAMPLE(kWidth, SUBSAMP_X), src_a + OFF, kWidth,      \
          dst_argb_b + OFF, kStrideB, kWidth, NEG kHeight, ATTEN);             \
      /* Convert B to C */                                                     \
      FMT_B##To##FMT_C(dst_argb_b + OFF, kStrideB, dst_argb_bc + OFF,          \
                       kStrideC, kWidth, kHeight);                             \
    }                                                                          \
    /* Convert A to C */                                                       \
    FMT_PLANAR##To##FMT_C(                                                     \
        src_y + OFF, kWidth, src_u + OFF, SUBSAMPLE(kWidth, SUBSAMP_X),        \
        src_v + OFF, SUBSAMPLE(kWidth, SUBSAMP_X), src_a + OFF, kWidth,        \
        dst_argb_c + OFF, kStrideC, kWidth, NEG kHeight, ATTEN);               \
    for (int i = 0; i < kStrideC * kHeight; ++i) {                             \
      EXPECT_EQ(dst_argb_c[i + OFF], dst_argb_bc[i + OFF]);                    \
    }                                                                          \
    free_aligned_buffer_page_end(src_y);                                       \
    free_aligned_buffer_page_end(src_u);                                       \
    free_aligned_buffer_page_end(src_v);                                       \
    free_aligned_buffer_page_end(src_a);                                       \
    free_aligned_buffer_page_end(dst_argb_b);                                  \
    free_aligned_buffer_page_end(dst_argb_c);                                  \
    free_aligned_buffer_page_end(dst_argb_bc);                                 \
  }

#if defined(ENABLE_FULL_TESTS)
#define TESTQPLANARTOE(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, SUB_B, BPP_B, \
                       FMT_C, BPP_C)                                          \
  TESTQPLANARTOEI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, SUB_B, BPP_B,      \
                  benchmark_width_ + 1, _Any, +, 0, FMT_C, BPP_C, 0)          \
  TESTQPLANARTOEI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, SUB_B, BPP_B,      \
                  benchmark_width_, _Unaligned, +, 2, FMT_C, BPP_C, 0)        \
  TESTQPLANARTOEI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, SUB_B, BPP_B,      \
                  benchmark_width_, _Invert, -, 0, FMT_C, BPP_C, 0)           \
  TESTQPLANARTOEI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, SUB_B, BPP_B,      \
                  benchmark_width_, _Opt, +, 0, FMT_C, BPP_C, 0)              \
  TESTQPLANARTOEI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, SUB_B, BPP_B,      \
                  benchmark_width_, _Premult, +, 0, FMT_C, BPP_C, 1)
#else
#define TESTQPLANARTOE(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, SUB_B, BPP_B, \
                       FMT_C, BPP_C)                                          \
  TESTQPLANARTOEI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, SUB_B, BPP_B,      \
                  benchmark_width_, _Opt, +, 0, FMT_C, BPP_C, 0)
#endif

#if defined(ENABLE_FULL_TESTS)
TESTQPLANARTOE(I420Alpha, 2, 2, ARGB, 1, 4, ABGR, 4)
TESTQPLANARTOE(I420Alpha, 2, 2, ABGR, 1, 4, ARGB, 4)
TESTQPLANARTOE(J420Alpha, 2, 2, ARGB, 1, 4, ABGR, 4)
TESTQPLANARTOE(J420Alpha, 2, 2, ABGR, 1, 4, ARGB, 4)
TESTQPLANARTOE(H420Alpha, 2, 2, ARGB, 1, 4, ABGR, 4)
TESTQPLANARTOE(H420Alpha, 2, 2, ABGR, 1, 4, ARGB, 4)
TESTQPLANARTOE(F420Alpha, 2, 2, ARGB, 1, 4, ABGR, 4)
TESTQPLANARTOE(F420Alpha, 2, 2, ABGR, 1, 4, ARGB, 4)
TESTQPLANARTOE(U420Alpha, 2, 2, ARGB, 1, 4, ABGR, 4)
TESTQPLANARTOE(U420Alpha, 2, 2, ABGR, 1, 4, ARGB, 4)
TESTQPLANARTOE(V420Alpha, 2, 2, ARGB, 1, 4, ABGR, 4)
TESTQPLANARTOE(V420Alpha, 2, 2, ABGR, 1, 4, ARGB, 4)
TESTQPLANARTOE(I422Alpha, 2, 1, ARGB, 1, 4, ABGR, 4)
TESTQPLANARTOE(I422Alpha, 2, 1, ABGR, 1, 4, ARGB, 4)
TESTQPLANARTOE(J422Alpha, 2, 1, ARGB, 1, 4, ABGR, 4)
TESTQPLANARTOE(J422Alpha, 2, 1, ABGR, 1, 4, ARGB, 4)
TESTQPLANARTOE(F422Alpha, 2, 1, ARGB, 1, 4, ABGR, 4)
TESTQPLANARTOE(F422Alpha, 2, 1, ABGR, 1, 4, ARGB, 4)
TESTQPLANARTOE(H422Alpha, 2, 1, ARGB, 1, 4, ABGR, 4)
TESTQPLANARTOE(H422Alpha, 2, 1, ABGR, 1, 4, ARGB, 4)
TESTQPLANARTOE(U422Alpha, 2, 1, ARGB, 1, 4, ABGR, 4)
TESTQPLANARTOE(U422Alpha, 2, 1, ABGR, 1, 4, ARGB, 4)
TESTQPLANARTOE(V422Alpha, 2, 1, ARGB, 1, 4, ABGR, 4)
TESTQPLANARTOE(V422Alpha, 2, 1, ABGR, 1, 4, ARGB, 4)
TESTQPLANARTOE(I444Alpha, 1, 1, ARGB, 1, 4, ABGR, 4)
TESTQPLANARTOE(I444Alpha, 1, 1, ABGR, 1, 4, ARGB, 4)
TESTQPLANARTOE(J444Alpha, 1, 1, ARGB, 1, 4, ABGR, 4)
TESTQPLANARTOE(J444Alpha, 1, 1, ABGR, 1, 4, ARGB, 4)
TESTQPLANARTOE(H444Alpha, 1, 1, ARGB, 1, 4, ABGR, 4)
TESTQPLANARTOE(H444Alpha, 1, 1, ABGR, 1, 4, ARGB, 4)
TESTQPLANARTOE(U444Alpha, 1, 1, ARGB, 1, 4, ABGR, 4)
TESTQPLANARTOE(U444Alpha, 1, 1, ABGR, 1, 4, ARGB, 4)
TESTQPLANARTOE(V444Alpha, 1, 1, ARGB, 1, 4, ABGR, 4)
TESTQPLANARTOE(V444Alpha, 1, 1, ABGR, 1, 4, ARGB, 4)
#else
TESTQPLANARTOE(I420Alpha, 2, 2, ABGR, 1, 4, ARGB, 4)
TESTQPLANARTOE(I422Alpha, 2, 1, ABGR, 1, 4, ARGB, 4)
TESTQPLANARTOE(I444Alpha, 1, 1, ABGR, 1, 4, ARGB, 4)
#endif

#define TESTPLANETOEI(FMT_A, SUB_A, BPP_A, FMT_B, SUB_B, BPP_B, W1280, N, NEG, \
                      OFF, FMT_C, BPP_C)                                       \
  TEST_F(LibYUVConvertTest, FMT_A##To##FMT_B##To##FMT_C##N) {                  \
    const int kWidth = W1280;                                                  \
    const int kHeight = benchmark_height_;                                     \
    const int kStrideA = SUBSAMPLE(kWidth, SUB_A) * BPP_A;                     \
    const int kStrideB = SUBSAMPLE(kWidth, SUB_B) * BPP_B;                     \
    align_buffer_page_end(src_argb_a, kStrideA* kHeight + OFF);                \
    align_buffer_page_end(dst_argb_b, kStrideB* kHeight + OFF);                \
    MemRandomize(src_argb_a + OFF, kStrideA * kHeight);                        \
    memset(dst_argb_b + OFF, 1, kStrideB * kHeight);                           \
    FMT_A##To##FMT_B(src_argb_a + OFF, kStrideA, dst_argb_b + OFF, kStrideB,   \
                     kWidth, NEG kHeight);                                     \
    /* Convert to a 3rd format in 1 step and 2 steps and compare  */           \
    const int kStrideC = kWidth * BPP_C;                                       \
    align_buffer_page_end(dst_argb_c, kStrideC* kHeight + OFF);                \
    align_buffer_page_end(dst_argb_bc, kStrideC* kHeight + OFF);               \
    memset(dst_argb_c + OFF, 2, kStrideC * kHeight);                           \
    memset(dst_argb_bc + OFF, 3, kStrideC * kHeight);                          \
    for (int i = 0; i < benchmark_iterations_; ++i) {                          \
      FMT_A##To##FMT_C(src_argb_a + OFF, kStrideA, dst_argb_c + OFF, kStrideC, \
                       kWidth, NEG kHeight);                                   \
      /* Convert B to C */                                                     \
      FMT_B##To##FMT_C(dst_argb_b + OFF, kStrideB, dst_argb_bc + OFF,          \
                       kStrideC, kWidth, kHeight);                             \
    }                                                                          \
    for (int i = 0; i < kStrideC * kHeight; i += 4) {                          \
      EXPECT_EQ(dst_argb_c[i + OFF + 0], dst_argb_bc[i + OFF + 0]);            \
      EXPECT_EQ(dst_argb_c[i + OFF + 1], dst_argb_bc[i + OFF + 1]);            \
      EXPECT_EQ(dst_argb_c[i + OFF + 2], dst_argb_bc[i + OFF + 2]);            \
      EXPECT_NEAR(dst_argb_c[i + OFF + 3], dst_argb_bc[i + OFF + 3], 64);      \
    }                                                                          \
    free_aligned_buffer_page_end(src_argb_a);                                  \
    free_aligned_buffer_page_end(dst_argb_b);                                  \
    free_aligned_buffer_page_end(dst_argb_c);                                  \
    free_aligned_buffer_page_end(dst_argb_bc);                                 \
  }

#if defined(ENABLE_FULL_TESTS)
#define TESTPLANETOE(FMT_A, SUB_A, BPP_A, FMT_B, SUB_B, BPP_B, FMT_C, BPP_C) \
  TESTPLANETOEI(FMT_A, SUB_A, BPP_A, FMT_B, SUB_B, BPP_B,                    \
                benchmark_width_ + 1, _Any, +, 0, FMT_C, BPP_C)              \
  TESTPLANETOEI(FMT_A, SUB_A, BPP_A, FMT_B, SUB_B, BPP_B, benchmark_width_,  \
                _Unaligned, +, 4, FMT_C, BPP_C)                              \
  TESTPLANETOEI(FMT_A, SUB_A, BPP_A, FMT_B, SUB_B, BPP_B, benchmark_width_,  \
                _Invert, -, 0, FMT_C, BPP_C)                                 \
  TESTPLANETOEI(FMT_A, SUB_A, BPP_A, FMT_B, SUB_B, BPP_B, benchmark_width_,  \
                _Opt, +, 0, FMT_C, BPP_C)
#else
#define TESTPLANETOE(FMT_A, SUB_A, BPP_A, FMT_B, SUB_B, BPP_B, FMT_C, BPP_C) \
  TESTPLANETOEI(FMT_A, SUB_A, BPP_A, FMT_B, SUB_B, BPP_B, benchmark_width_,  \
                _Opt, +, 0, FMT_C, BPP_C)
#endif

// Caveat: Destination needs to be 4 bytes
#ifdef LITTLE_ENDIAN_ONLY_TEST
TESTPLANETOE(ARGB, 1, 4, AR30, 1, 4, ARGB, 4)
TESTPLANETOE(ABGR, 1, 4, AR30, 1, 4, ABGR, 4)
TESTPLANETOE(AR30, 1, 4, ARGB, 1, 4, ABGR, 4)
TESTPLANETOE(AR30, 1, 4, ABGR, 1, 4, ARGB, 4)
TESTPLANETOE(ARGB, 1, 4, AB30, 1, 4, ARGB, 4)
TESTPLANETOE(ABGR, 1, 4, AB30, 1, 4, ABGR, 4)
TESTPLANETOE(AB30, 1, 4, ARGB, 1, 4, ABGR, 4)
TESTPLANETOE(AB30, 1, 4, ABGR, 1, 4, ARGB, 4)
#endif

TEST_F(LibYUVConvertTest, RotateWithARGBSource) {
  // 2x2 frames
  uint32_t src[4];
  uint32_t dst[4];
  // some random input
  src[0] = 0x11000000;
  src[1] = 0x00450000;
  src[2] = 0x00009f00;
  src[3] = 0x000000ff;
  // zeros on destination
  dst[0] = 0x00000000;
  dst[1] = 0x00000000;
  dst[2] = 0x00000000;
  dst[3] = 0x00000000;

  int r = ConvertToARGB(reinterpret_cast<uint8_t*>(src),
                        16,  // input size
                        reinterpret_cast<uint8_t*>(dst),
                        8,  // destination stride
                        0,  // crop_x
                        0,  // crop_y
                        2,  // width
                        2,  // height
                        2,  // crop width
                        2,  // crop height
                        kRotate90, FOURCC_ARGB);

  EXPECT_EQ(r, 0);
  // 90 degrees rotation, no conversion
  EXPECT_EQ(dst[0], src[2]);
  EXPECT_EQ(dst[1], src[0]);
  EXPECT_EQ(dst[2], src[3]);
  EXPECT_EQ(dst[3], src[1]);
}

#ifdef HAS_ARGBTOAR30ROW_AVX2
TEST_F(LibYUVConvertTest, ARGBToAR30Row_Opt) {
  // ARGBToAR30Row_AVX2 expects a multiple of 8 pixels.
  const int kPixels = (benchmark_width_ * benchmark_height_ + 7) & ~7;
  align_buffer_page_end(src, kPixels * 4);
  align_buffer_page_end(dst_opt, kPixels * 4);
  align_buffer_page_end(dst_c, kPixels * 4);
  MemRandomize(src, kPixels * 4);
  memset(dst_opt, 0, kPixels * 4);
  memset(dst_c, 1, kPixels * 4);

  ARGBToAR30Row_C(src, dst_c, kPixels);

  int has_avx2 = TestCpuFlag(kCpuHasAVX2);
  int has_ssse3 = TestCpuFlag(kCpuHasSSSE3);
  for (int i = 0; i < benchmark_iterations_; ++i) {
    if (has_avx2) {
      ARGBToAR30Row_AVX2(src, dst_opt, kPixels);
    } else if (has_ssse3) {
      ARGBToAR30Row_SSSE3(src, dst_opt, kPixels);
    } else {
      ARGBToAR30Row_C(src, dst_opt, kPixels);
    }
  }
  for (int i = 0; i < kPixels * 4; ++i) {
    EXPECT_EQ(dst_opt[i], dst_c[i]);
  }

  free_aligned_buffer_page_end(src);
  free_aligned_buffer_page_end(dst_opt);
  free_aligned_buffer_page_end(dst_c);
}
#endif  // HAS_ARGBTOAR30ROW_AVX2

#ifdef HAS_ABGRTOAR30ROW_AVX2
TEST_F(LibYUVConvertTest, ABGRToAR30Row_Opt) {
  // ABGRToAR30Row_AVX2 expects a multiple of 8 pixels.
  const int kPixels = (benchmark_width_ * benchmark_height_ + 7) & ~7;
  align_buffer_page_end(src, kPixels * 4);
  align_buffer_page_end(dst_opt, kPixels * 4);
  align_buffer_page_end(dst_c, kPixels * 4);
  MemRandomize(src, kPixels * 4);
  memset(dst_opt, 0, kPixels * 4);
  memset(dst_c, 1, kPixels * 4);

  ABGRToAR30Row_C(src, dst_c, kPixels);

  int has_avx2 = TestCpuFlag(kCpuHasAVX2);
  int has_ssse3 = TestCpuFlag(kCpuHasSSSE3);
  for (int i = 0; i < benchmark_iterations_; ++i) {
    if (has_avx2) {
      ABGRToAR30Row_AVX2(src, dst_opt, kPixels);
    } else if (has_ssse3) {
      ABGRToAR30Row_SSSE3(src, dst_opt, kPixels);
    } else {
      ABGRToAR30Row_C(src, dst_opt, kPixels);
    }
  }
  for (int i = 0; i < kPixels * 4; ++i) {
    EXPECT_EQ(dst_opt[i], dst_c[i]);
  }

  free_aligned_buffer_page_end(src);
  free_aligned_buffer_page_end(dst_opt);
  free_aligned_buffer_page_end(dst_c);
}
#endif  // HAS_ABGRTOAR30ROW_AVX2

#if !defined(LEAN_TESTS)

// Provide matrix wrappers for 12 bit YUV
#define I012ToARGB(a, b, c, d, e, f, g, h, i, j) \
  I012ToARGBMatrix(a, b, c, d, e, f, g, h, &kYuvI601Constants, i, j)
#define I012ToAR30(a, b, c, d, e, f, g, h, i, j) \
  I012ToAR30Matrix(a, b, c, d, e, f, g, h, &kYuvI601Constants, i, j)
#define I012ToAB30(a, b, c, d, e, f, g, h, i, j) \
  I012ToAB30Matrix(a, b, c, d, e, f, g, h, &kYuvI601Constants, i, j)

#define I410ToARGB(a, b, c, d, e, f, g, h, i, j) \
  I410ToARGBMatrix(a, b, c, d, e, f, g, h, &kYuvI601Constants, i, j)
#define I410ToABGR(a, b, c, d, e, f, g, h, i, j) \
  I410ToABGRMatrix(a, b, c, d, e, f, g, h, &kYuvI601Constants, i, j)
#define H410ToARGB(a, b, c, d, e, f, g, h, i, j) \
  I410ToARGBMatrix(a, b, c, d, e, f, g, h, &kYuvH709Constants, i, j)
#define H410ToABGR(a, b, c, d, e, f, g, h, i, j) \
  I410ToABGRMatrix(a, b, c, d, e, f, g, h, &kYuvH709Constants, i, j)
#define U410ToARGB(a, b, c, d, e, f, g, h, i, j) \
  I410ToARGBMatrix(a, b, c, d, e, f, g, h, &kYuv2020Constants, i, j)
#define U410ToABGR(a, b, c, d, e, f, g, h, i, j) \
  I410ToABGRMatrix(a, b, c, d, e, f, g, h, &kYuv2020Constants, i, j)
#define I410ToAR30(a, b, c, d, e, f, g, h, i, j) \
  I410ToAR30Matrix(a, b, c, d, e, f, g, h, &kYuvI601Constants, i, j)
#define I410ToAB30(a, b, c, d, e, f, g, h, i, j) \
  I410ToAB30Matrix(a, b, c, d, e, f, g, h, &kYuvI601Constants, i, j)
#define H410ToAR30(a, b, c, d, e, f, g, h, i, j) \
  I410ToAR30Matrix(a, b, c, d, e, f, g, h, &kYuvH709Constants, i, j)
#define H410ToAB30(a, b, c, d, e, f, g, h, i, j) \
  I410ToAB30Matrix(a, b, c, d, e, f, g, h, &kYuvH709Constants, i, j)
#define U410ToAR30(a, b, c, d, e, f, g, h, i, j) \
  I410ToAR30Matrix(a, b, c, d, e, f, g, h, &kYuv2020Constants, i, j)
#define U410ToAB30(a, b, c, d, e, f, g, h, i, j) \
  I410ToAB30Matrix(a, b, c, d, e, f, g, h, &kYuv2020Constants, i, j)

#define I010ToARGBFilter(a, b, c, d, e, f, g, h, i, j)                     \
  I010ToARGBMatrixFilter(a, b, c, d, e, f, g, h, &kYuvI601Constants, i, j, \
                         kFilterBilinear)
#define I010ToAR30Filter(a, b, c, d, e, f, g, h, i, j)                     \
  I010ToAR30MatrixFilter(a, b, c, d, e, f, g, h, &kYuvI601Constants, i, j, \
                         kFilterBilinear)
#define I210ToARGBFilter(a, b, c, d, e, f, g, h, i, j)                     \
  I210ToARGBMatrixFilter(a, b, c, d, e, f, g, h, &kYuvI601Constants, i, j, \
                         kFilterBilinear)
#define I210ToAR30Filter(a, b, c, d, e, f, g, h, i, j)                     \
  I210ToAR30MatrixFilter(a, b, c, d, e, f, g, h, &kYuvI601Constants, i, j, \
                         kFilterBilinear)

// TODO(fbarchard): Fix clamping issue affected by U channel.
#define TESTPLANAR16TOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_MASK, FMT_B,   \
                         BPP_B, ALIGN, YALIGN, W1280, N, NEG, SOFF, DOFF)     \
  TEST_F(LibYUVConvertTest, FMT_PLANAR##To##FMT_B##N) {                       \
    const int kWidth = W1280;                                                 \
    const int kHeight = ALIGNINT(benchmark_height_, YALIGN);                  \
    const int kStrideB = ALIGNINT(kWidth * BPP_B, ALIGN);                     \
    const int kStrideUV = SUBSAMPLE(kWidth, SUBSAMP_X);                       \
    const int kSizeUV = kStrideUV * SUBSAMPLE(kHeight, SUBSAMP_Y);            \
    const int kBpc = 2;                                                       \
    align_buffer_page_end(src_y, kWidth* kHeight* kBpc + SOFF);               \
    align_buffer_page_end(src_u, kSizeUV* kBpc + SOFF);                       \
    align_buffer_page_end(src_v, kSizeUV* kBpc + SOFF);                       \
    align_buffer_page_end(dst_argb_c, kStrideB* kHeight + DOFF);              \
    align_buffer_page_end(dst_argb_opt, kStrideB* kHeight + DOFF);            \
    for (int i = 0; i < kWidth * kHeight; ++i) {                              \
      reinterpret_cast<uint16_t*>(src_y + SOFF)[i] = (fastrand() & FMT_MASK); \
    }                                                                         \
    for (int i = 0; i < kSizeUV; ++i) {                                       \
      reinterpret_cast<uint16_t*>(src_u + SOFF)[i] = (fastrand() & FMT_MASK); \
      reinterpret_cast<uint16_t*>(src_v + SOFF)[i] = (fastrand() & FMT_MASK); \
    }                                                                         \
    memset(dst_argb_c + DOFF, 1, kStrideB * kHeight);                         \
    memset(dst_argb_opt + DOFF, 101, kStrideB * kHeight);                     \
    MaskCpuFlags(disable_cpu_flags_);                                         \
    FMT_PLANAR##To##FMT_B(                                                    \
        reinterpret_cast<uint16_t*>(src_y + SOFF), kWidth,                    \
        reinterpret_cast<uint16_t*>(src_u + SOFF), kStrideUV,                 \
        reinterpret_cast<uint16_t*>(src_v + SOFF), kStrideUV,                 \
        dst_argb_c + DOFF, kStrideB, kWidth, NEG kHeight);                    \
    MaskCpuFlags(benchmark_cpu_info_);                                        \
    for (int i = 0; i < benchmark_iterations_; ++i) {                         \
      FMT_PLANAR##To##FMT_B(                                                  \
          reinterpret_cast<uint16_t*>(src_y + SOFF), kWidth,                  \
          reinterpret_cast<uint16_t*>(src_u + SOFF), kStrideUV,               \
          reinterpret_cast<uint16_t*>(src_v + SOFF), kStrideUV,               \
          dst_argb_opt + DOFF, kStrideB, kWidth, NEG kHeight);                \
    }                                                                         \
    for (int i = 0; i < kWidth * BPP_B * kHeight; ++i) {                      \
      EXPECT_EQ(dst_argb_c[i + DOFF], dst_argb_opt[i + DOFF]);                \
    }                                                                         \
    free_aligned_buffer_page_end(src_y);                                      \
    free_aligned_buffer_page_end(src_u);                                      \
    free_aligned_buffer_page_end(src_v);                                      \
    free_aligned_buffer_page_end(dst_argb_c);                                 \
    free_aligned_buffer_page_end(dst_argb_opt);                               \
  }

#if defined(ENABLE_FULL_TESTS)
#define TESTPLANAR16TOB(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_MASK, FMT_B,   \
                        BPP_B, ALIGN, YALIGN)                                \
  TESTPLANAR16TOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_MASK, FMT_B, BPP_B, \
                   ALIGN, YALIGN, benchmark_width_ + 1, _Any, +, 0, 0)       \
  TESTPLANAR16TOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_MASK, FMT_B, BPP_B, \
                   ALIGN, YALIGN, benchmark_width_, _Unaligned, +, 4, 4)     \
  TESTPLANAR16TOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_MASK, FMT_B, BPP_B, \
                   ALIGN, YALIGN, benchmark_width_, _Invert, -, 0, 0)        \
  TESTPLANAR16TOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_MASK, FMT_B, BPP_B, \
                   ALIGN, YALIGN, benchmark_width_, _Opt, +, 0, 0)
#else
#define TESTPLANAR16TOB(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_MASK, FMT_B,   \
                        BPP_B, ALIGN, YALIGN)                                \
  TESTPLANAR16TOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_MASK, FMT_B, BPP_B, \
                   ALIGN, YALIGN, benchmark_width_, _Opt, +, 0, 0)
#endif

// These conversions are only optimized for x86
#if !defined(DISABLE_SLOW_TESTS) || defined(__x86_64__) || defined(__i386__)
TESTPLANAR16TOB(I010, 2, 2, 0x3ff, ARGB, 4, 4, 1)
TESTPLANAR16TOB(I010, 2, 2, 0x3ff, ABGR, 4, 4, 1)
TESTPLANAR16TOB(H010, 2, 2, 0x3ff, ARGB, 4, 4, 1)
TESTPLANAR16TOB(H010, 2, 2, 0x3ff, ABGR, 4, 4, 1)
TESTPLANAR16TOB(U010, 2, 2, 0x3ff, ARGB, 4, 4, 1)
TESTPLANAR16TOB(U010, 2, 2, 0x3ff, ABGR, 4, 4, 1)
TESTPLANAR16TOB(I210, 2, 1, 0x3ff, ARGB, 4, 4, 1)
TESTPLANAR16TOB(I210, 2, 1, 0x3ff, ABGR, 4, 4, 1)
TESTPLANAR16TOB(H210, 2, 1, 0x3ff, ARGB, 4, 4, 1)
TESTPLANAR16TOB(H210, 2, 1, 0x3ff, ABGR, 4, 4, 1)
TESTPLANAR16TOB(U210, 2, 1, 0x3ff, ARGB, 4, 4, 1)
TESTPLANAR16TOB(U210, 2, 1, 0x3ff, ABGR, 4, 4, 1)
TESTPLANAR16TOB(I410, 1, 1, 0x3ff, ARGB, 4, 4, 1)
TESTPLANAR16TOB(I410, 1, 1, 0x3ff, ABGR, 4, 4, 1)
TESTPLANAR16TOB(H410, 1, 1, 0x3ff, ARGB, 4, 4, 1)
TESTPLANAR16TOB(H410, 1, 1, 0x3ff, ABGR, 4, 4, 1)
TESTPLANAR16TOB(U410, 1, 1, 0x3ff, ARGB, 4, 4, 1)
TESTPLANAR16TOB(U410, 1, 1, 0x3ff, ABGR, 4, 4, 1)
TESTPLANAR16TOB(I012, 2, 2, 0xfff, ARGB, 4, 4, 1)
TESTPLANAR16TOB(I010, 2, 2, 0x3ff, ARGBFilter, 4, 4, 1)
TESTPLANAR16TOB(I210, 2, 1, 0x3ff, ARGBFilter, 4, 4, 1)

#ifdef LITTLE_ENDIAN_ONLY_TEST
TESTPLANAR16TOB(I010, 2, 2, 0x3ff, AR30, 4, 4, 1)
TESTPLANAR16TOB(I010, 2, 2, 0x3ff, AB30, 4, 4, 1)
TESTPLANAR16TOB(H010, 2, 2, 0x3ff, AR30, 4, 4, 1)
TESTPLANAR16TOB(H010, 2, 2, 0x3ff, AB30, 4, 4, 1)
TESTPLANAR16TOB(U010, 2, 2, 0x3ff, AR30, 4, 4, 1)
TESTPLANAR16TOB(U010, 2, 2, 0x3ff, AB30, 4, 4, 1)
TESTPLANAR16TOB(I210, 2, 1, 0x3ff, AR30, 4, 4, 1)
TESTPLANAR16TOB(I210, 2, 1, 0x3ff, AB30, 4, 4, 1)
TESTPLANAR16TOB(H210, 2, 1, 0x3ff, AR30, 4, 4, 1)
TESTPLANAR16TOB(H210, 2, 1, 0x3ff, AB30, 4, 4, 1)
TESTPLANAR16TOB(U210, 2, 1, 0x3ff, AR30, 4, 4, 1)
TESTPLANAR16TOB(U210, 2, 1, 0x3ff, AB30, 4, 4, 1)
TESTPLANAR16TOB(I410, 1, 1, 0x3ff, AR30, 4, 4, 1)
TESTPLANAR16TOB(I410, 1, 1, 0x3ff, AB30, 4, 4, 1)
TESTPLANAR16TOB(H410, 1, 1, 0x3ff, AR30, 4, 4, 1)
TESTPLANAR16TOB(H410, 1, 1, 0x3ff, AB30, 4, 4, 1)
TESTPLANAR16TOB(U410, 1, 1, 0x3ff, AR30, 4, 4, 1)
TESTPLANAR16TOB(U410, 1, 1, 0x3ff, AB30, 4, 4, 1)
TESTPLANAR16TOB(I012, 2, 2, 0xfff, AR30, 4, 4, 1)
TESTPLANAR16TOB(I012, 2, 2, 0xfff, AB30, 4, 4, 1)
TESTPLANAR16TOB(I010, 2, 2, 0x3ff, AR30Filter, 4, 4, 1)
TESTPLANAR16TOB(I210, 2, 1, 0x3ff, AR30Filter, 4, 4, 1)
#endif  // LITTLE_ENDIAN_ONLY_TEST
#endif  // DISABLE_SLOW_TESTS

#define TESTQPLANAR16TOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B,      \
                          ALIGN, YALIGN, W1280, N, NEG, OFF, ATTEN, S_DEPTH)   \
  TEST_F(LibYUVConvertTest, FMT_PLANAR##To##FMT_B##N) {                        \
    const int kWidth = W1280;                                                  \
    const int kHeight = ALIGNINT(benchmark_height_, YALIGN);                   \
    const int kStrideB = ALIGNINT(kWidth * BPP_B, ALIGN);                      \
    const int kStrideUV = SUBSAMPLE(kWidth, SUBSAMP_X);                        \
    const int kSizeUV = kStrideUV * SUBSAMPLE(kHeight, SUBSAMP_Y);             \
    const int kBpc = 2;                                                        \
    align_buffer_page_end(src_y, kWidth* kHeight* kBpc + OFF);                 \
    align_buffer_page_end(src_u, kSizeUV* kBpc + OFF);                         \
    align_buffer_page_end(src_v, kSizeUV* kBpc + OFF);                         \
    align_buffer_page_end(src_a, kWidth* kHeight* kBpc + OFF);                 \
    align_buffer_page_end(dst_argb_c, kStrideB* kHeight + OFF);                \
    align_buffer_page_end(dst_argb_opt, kStrideB* kHeight + OFF);              \
    for (int i = 0; i < kWidth * kHeight; ++i) {                               \
      reinterpret_cast<uint16_t*>(src_y + OFF)[i] =                            \
          (fastrand() & ((1 << S_DEPTH) - 1));                                 \
      reinterpret_cast<uint16_t*>(src_a + OFF)[i] =                            \
          (fastrand() & ((1 << S_DEPTH) - 1));                                 \
    }                                                                          \
    for (int i = 0; i < kSizeUV; ++i) {                                        \
      reinterpret_cast<uint16_t*>(src_u + OFF)[i] =                            \
          (fastrand() & ((1 << S_DEPTH) - 1));                                 \
      reinterpret_cast<uint16_t*>(src_v + OFF)[i] =                            \
          (fastrand() & ((1 << S_DEPTH) - 1));                                 \
    }                                                                          \
    memset(dst_argb_c + OFF, 1, kStrideB * kHeight);                           \
    memset(dst_argb_opt + OFF, 101, kStrideB * kHeight);                       \
    MaskCpuFlags(disable_cpu_flags_);                                          \
    FMT_PLANAR##To##FMT_B(reinterpret_cast<uint16_t*>(src_y + OFF), kWidth,    \
                          reinterpret_cast<uint16_t*>(src_u + OFF), kStrideUV, \
                          reinterpret_cast<uint16_t*>(src_v + OFF), kStrideUV, \
                          reinterpret_cast<uint16_t*>(src_a + OFF), kWidth,    \
                          dst_argb_c + OFF, kStrideB, kWidth, NEG kHeight,     \
                          ATTEN);                                              \
    MaskCpuFlags(benchmark_cpu_info_);                                         \
    for (int i = 0; i < benchmark_iterations_; ++i) {                          \
      FMT_PLANAR##To##FMT_B(                                                   \
          reinterpret_cast<uint16_t*>(src_y + OFF), kWidth,                    \
          reinterpret_cast<uint16_t*>(src_u + OFF), kStrideUV,                 \
          reinterpret_cast<uint16_t*>(src_v + OFF), kStrideUV,                 \
          reinterpret_cast<uint16_t*>(src_a + OFF), kWidth,                    \
          dst_argb_opt + OFF, kStrideB, kWidth, NEG kHeight, ATTEN);           \
    }                                                                          \
    for (int i = 0; i < kWidth * BPP_B * kHeight; ++i) {                       \
      EXPECT_EQ(dst_argb_c[i + OFF], dst_argb_opt[i + OFF]);                   \
    }                                                                          \
    free_aligned_buffer_page_end(src_y);                                       \
    free_aligned_buffer_page_end(src_u);                                       \
    free_aligned_buffer_page_end(src_v);                                       \
    free_aligned_buffer_page_end(src_a);                                       \
    free_aligned_buffer_page_end(dst_argb_c);                                  \
    free_aligned_buffer_page_end(dst_argb_opt);                                \
  }

#if defined(ENABLE_FULL_TESTS)
#define TESTQPLANAR16TOB(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B,    \
                         ALIGN, YALIGN, S_DEPTH)                            \
  TESTQPLANAR16TOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,  \
                    YALIGN, benchmark_width_ + 1, _Any, +, 0, 0, S_DEPTH)   \
  TESTQPLANAR16TOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,  \
                    YALIGN, benchmark_width_, _Unaligned, +, 2, 0, S_DEPTH) \
  TESTQPLANAR16TOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,  \
                    YALIGN, benchmark_width_, _Invert, -, 0, 0, S_DEPTH)    \
  TESTQPLANAR16TOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,  \
                    YALIGN, benchmark_width_, _Opt, +, 0, 0, S_DEPTH)       \
  TESTQPLANAR16TOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,  \
                    YALIGN, benchmark_width_, _Premult, +, 0, 1, S_DEPTH)
#else
#define TESTQPLANAR16TOB(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B,   \
                         ALIGN, YALIGN, S_DEPTH)                           \
  TESTQPLANAR16TOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN, \
                    YALIGN, benchmark_width_, _Opt, +, 0, 0, S_DEPTH)
#endif

#define I010AlphaToARGB(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I010AlphaToARGBMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvI601Constants, k, \
                        l, m)
#define I010AlphaToABGR(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I010AlphaToABGRMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvI601Constants, k, \
                        l, m)
#define J010AlphaToARGB(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I010AlphaToARGBMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvJPEGConstants, k, \
                        l, m)
#define J010AlphaToABGR(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I010AlphaToABGRMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvJPEGConstants, k, \
                        l, m)
#define F010AlphaToARGB(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I010AlphaToARGBMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvF709Constants, k, \
                        l, m)
#define F010AlphaToABGR(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I010AlphaToABGRMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvF709Constants, k, \
                        l, m)
#define H010AlphaToARGB(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I010AlphaToARGBMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvH709Constants, k, \
                        l, m)
#define H010AlphaToABGR(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I010AlphaToABGRMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvH709Constants, k, \
                        l, m)
#define U010AlphaToARGB(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I010AlphaToARGBMatrix(a, b, c, d, e, f, g, h, i, j, &kYuv2020Constants, k, \
                        l, m)
#define U010AlphaToABGR(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I010AlphaToABGRMatrix(a, b, c, d, e, f, g, h, i, j, &kYuv2020Constants, k, \
                        l, m)
#define V010AlphaToARGB(a, b, c, d, e, f, g, h, i, j, k, l, m)                \
  I010AlphaToARGBMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvV2020Constants, k, \
                        l, m)
#define V010AlphaToABGR(a, b, c, d, e, f, g, h, i, j, k, l, m)                \
  I010AlphaToABGRMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvV2020Constants, k, \
                        l, m)
#define I210AlphaToARGB(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I210AlphaToARGBMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvI601Constants, k, \
                        l, m)
#define I210AlphaToABGR(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I210AlphaToABGRMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvI601Constants, k, \
                        l, m)
#define J210AlphaToARGB(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I210AlphaToARGBMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvJPEGConstants, k, \
                        l, m)
#define J210AlphaToABGR(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I210AlphaToABGRMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvJPEGConstants, k, \
                        l, m)
#define F210AlphaToARGB(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I210AlphaToARGBMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvF709Constants, k, \
                        l, m)
#define F210AlphaToABGR(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I210AlphaToABGRMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvF709Constants, k, \
                        l, m)
#define H210AlphaToARGB(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I210AlphaToARGBMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvH709Constants, k, \
                        l, m)
#define H210AlphaToABGR(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I210AlphaToABGRMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvH709Constants, k, \
                        l, m)
#define U210AlphaToARGB(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I210AlphaToARGBMatrix(a, b, c, d, e, f, g, h, i, j, &kYuv2020Constants, k, \
                        l, m)
#define U210AlphaToABGR(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I210AlphaToABGRMatrix(a, b, c, d, e, f, g, h, i, j, &kYuv2020Constants, k, \
                        l, m)
#define V210AlphaToARGB(a, b, c, d, e, f, g, h, i, j, k, l, m)                \
  I210AlphaToARGBMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvV2020Constants, k, \
                        l, m)
#define V210AlphaToABGR(a, b, c, d, e, f, g, h, i, j, k, l, m)                \
  I210AlphaToABGRMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvV2020Constants, k, \
                        l, m)
#define I410AlphaToARGB(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I410AlphaToARGBMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvI601Constants, k, \
                        l, m)
#define I410AlphaToABGR(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I410AlphaToABGRMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvI601Constants, k, \
                        l, m)
#define J410AlphaToARGB(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I410AlphaToARGBMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvJPEGConstants, k, \
                        l, m)
#define J410AlphaToABGR(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I410AlphaToABGRMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvJPEGConstants, k, \
                        l, m)
#define F410AlphaToARGB(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I410AlphaToARGBMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvF709Constants, k, \
                        l, m)
#define F410AlphaToABGR(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I410AlphaToABGRMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvF709Constants, k, \
                        l, m)
#define H410AlphaToARGB(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I410AlphaToARGBMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvH709Constants, k, \
                        l, m)
#define H410AlphaToABGR(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I410AlphaToABGRMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvH709Constants, k, \
                        l, m)
#define U410AlphaToARGB(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I410AlphaToARGBMatrix(a, b, c, d, e, f, g, h, i, j, &kYuv2020Constants, k, \
                        l, m)
#define U410AlphaToABGR(a, b, c, d, e, f, g, h, i, j, k, l, m)               \
  I410AlphaToABGRMatrix(a, b, c, d, e, f, g, h, i, j, &kYuv2020Constants, k, \
                        l, m)
#define V410AlphaToARGB(a, b, c, d, e, f, g, h, i, j, k, l, m)                \
  I410AlphaToARGBMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvV2020Constants, k, \
                        l, m)
#define V410AlphaToABGR(a, b, c, d, e, f, g, h, i, j, k, l, m)                \
  I410AlphaToABGRMatrix(a, b, c, d, e, f, g, h, i, j, &kYuvV2020Constants, k, \
                        l, m)
#define I010AlphaToARGBFilter(a, b, c, d, e, f, g, h, i, j, k, l, m) \
  I010AlphaToARGBMatrixFilter(a, b, c, d, e, f, g, h, i, j,          \
                              &kYuvI601Constants, k, l, m, kFilterBilinear)
#define I210AlphaToARGBFilter(a, b, c, d, e, f, g, h, i, j, k, l, m) \
  I010AlphaToARGBMatrixFilter(a, b, c, d, e, f, g, h, i, j,          \
                              &kYuvI601Constants, k, l, m, kFilterBilinear)

// These conversions are only optimized for x86
#if !defined(DISABLE_SLOW_TESTS) || defined(__x86_64__) || defined(__i386__)
TESTQPLANAR16TOB(I010Alpha, 2, 2, ARGB, 4, 4, 1, 10)
TESTQPLANAR16TOB(I010Alpha, 2, 2, ABGR, 4, 4, 1, 10)
TESTQPLANAR16TOB(J010Alpha, 2, 2, ARGB, 4, 4, 1, 10)
TESTQPLANAR16TOB(J010Alpha, 2, 2, ABGR, 4, 4, 1, 10)
TESTQPLANAR16TOB(H010Alpha, 2, 2, ARGB, 4, 4, 1, 10)
TESTQPLANAR16TOB(H010Alpha, 2, 2, ABGR, 4, 4, 1, 10)
TESTQPLANAR16TOB(F010Alpha, 2, 2, ARGB, 4, 4, 1, 10)
TESTQPLANAR16TOB(F010Alpha, 2, 2, ABGR, 4, 4, 1, 10)
TESTQPLANAR16TOB(U010Alpha, 2, 2, ARGB, 4, 4, 1, 10)
TESTQPLANAR16TOB(U010Alpha, 2, 2, ABGR, 4, 4, 1, 10)
TESTQPLANAR16TOB(V010Alpha, 2, 2, ARGB, 4, 4, 1, 10)
TESTQPLANAR16TOB(V010Alpha, 2, 2, ABGR, 4, 4, 1, 10)
TESTQPLANAR16TOB(I210Alpha, 2, 1, ARGB, 4, 4, 1, 10)
TESTQPLANAR16TOB(I210Alpha, 2, 1, ABGR, 4, 4, 1, 10)
TESTQPLANAR16TOB(J210Alpha, 2, 1, ARGB, 4, 4, 1, 10)
TESTQPLANAR16TOB(J210Alpha, 2, 1, ABGR, 4, 4, 1, 10)
TESTQPLANAR16TOB(H210Alpha, 2, 1, ARGB, 4, 4, 1, 10)
TESTQPLANAR16TOB(H210Alpha, 2, 1, ABGR, 4, 4, 1, 10)
TESTQPLANAR16TOB(F210Alpha, 2, 1, ARGB, 4, 4, 1, 10)
TESTQPLANAR16TOB(F210Alpha, 2, 1, ABGR, 4, 4, 1, 10)
TESTQPLANAR16TOB(U210Alpha, 2, 1, ARGB, 4, 4, 1, 10)
TESTQPLANAR16TOB(U210Alpha, 2, 1, ABGR, 4, 4, 1, 10)
TESTQPLANAR16TOB(V210Alpha, 2, 1, ARGB, 4, 4, 1, 10)
TESTQPLANAR16TOB(V210Alpha, 2, 1, ABGR, 4, 4, 1, 10)
TESTQPLANAR16TOB(I410Alpha, 1, 1, ARGB, 4, 4, 1, 10)
TESTQPLANAR16TOB(I410Alpha, 1, 1, ABGR, 4, 4, 1, 10)
TESTQPLANAR16TOB(J410Alpha, 1, 1, ARGB, 4, 4, 1, 10)
TESTQPLANAR16TOB(J410Alpha, 1, 1, ABGR, 4, 4, 1, 10)
TESTQPLANAR16TOB(H410Alpha, 1, 1, ARGB, 4, 4, 1, 10)
TESTQPLANAR16TOB(H410Alpha, 1, 1, ABGR, 4, 4, 1, 10)
TESTQPLANAR16TOB(F410Alpha, 1, 1, ARGB, 4, 4, 1, 10)
TESTQPLANAR16TOB(F410Alpha, 1, 1, ABGR, 4, 4, 1, 10)
TESTQPLANAR16TOB(U410Alpha, 1, 1, ARGB, 4, 4, 1, 10)
TESTQPLANAR16TOB(U410Alpha, 1, 1, ABGR, 4, 4, 1, 10)
TESTQPLANAR16TOB(V410Alpha, 1, 1, ARGB, 4, 4, 1, 10)
TESTQPLANAR16TOB(V410Alpha, 1, 1, ABGR, 4, 4, 1, 10)
TESTQPLANAR16TOB(I010Alpha, 2, 2, ARGBFilter, 4, 4, 1, 10)
TESTQPLANAR16TOB(I210Alpha, 2, 1, ARGBFilter, 4, 4, 1, 10)
#endif  // DISABLE_SLOW_TESTS

#define TESTBP16TOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,    \
                     YALIGN, W1280, N, NEG, SOFF, DOFF, S_DEPTH)               \
  TEST_F(LibYUVConvertTest, FMT_PLANAR##To##FMT_B##N) {                        \
    const int kWidth = W1280;                                                  \
    const int kHeight = ALIGNINT(benchmark_height_, YALIGN);                   \
    const int kStrideB = ALIGNINT(kWidth * BPP_B, ALIGN);                      \
    const int kStrideUV = SUBSAMPLE(kWidth, SUBSAMP_X) * 2;                    \
    const int kSizeUV = kStrideUV * SUBSAMPLE(kHeight, SUBSAMP_Y) * 2;         \
    const int kBpc = 2;                                                        \
    align_buffer_page_end(src_y, kWidth* kHeight* kBpc + SOFF);                \
    align_buffer_page_end(src_uv, kSizeUV* kBpc + SOFF);                       \
    align_buffer_page_end(dst_argb_c, kStrideB* kHeight + DOFF);               \
    align_buffer_page_end(dst_argb_opt, kStrideB* kHeight + DOFF);             \
    for (int i = 0; i < kWidth * kHeight; ++i) {                               \
      reinterpret_cast<uint16_t*>(src_y + SOFF)[i] =                           \
          (fastrand() & (((uint16_t)(-1)) << (16 - S_DEPTH)));                 \
    }                                                                          \
    for (int i = 0; i < kSizeUV; ++i) {                                        \
      reinterpret_cast<uint16_t*>(src_uv + SOFF)[i] =                          \
          (fastrand() & (((uint16_t)(-1)) << (16 - S_DEPTH)));                 \
    }                                                                          \
    memset(dst_argb_c + DOFF, 1, kStrideB * kHeight);                          \
    memset(dst_argb_opt + DOFF, 101, kStrideB * kHeight);                      \
    MaskCpuFlags(disable_cpu_flags_);                                          \
    FMT_PLANAR##To##FMT_B(reinterpret_cast<uint16_t*>(src_y + SOFF), kWidth,   \
                          reinterpret_cast<uint16_t*>(src_uv + SOFF),          \
                          kStrideUV, dst_argb_c + DOFF, kStrideB, kWidth,      \
                          NEG kHeight);                                        \
    MaskCpuFlags(benchmark_cpu_info_);                                         \
    for (int i = 0; i < benchmark_iterations_; ++i) {                          \
      FMT_PLANAR##To##FMT_B(reinterpret_cast<uint16_t*>(src_y + SOFF), kWidth, \
                            reinterpret_cast<uint16_t*>(src_uv + SOFF),        \
                            kStrideUV, dst_argb_opt + DOFF, kStrideB, kWidth,  \
                            NEG kHeight);                                      \
    }                                                                          \
    for (int i = 0; i < kWidth * BPP_B * kHeight; ++i) {                       \
      EXPECT_EQ(dst_argb_c[i + DOFF], dst_argb_opt[i + DOFF]);                 \
    }                                                                          \
    free_aligned_buffer_page_end(src_y);                                       \
    free_aligned_buffer_page_end(src_uv);                                      \
    free_aligned_buffer_page_end(dst_argb_c);                                  \
    free_aligned_buffer_page_end(dst_argb_opt);                                \
  }

#if defined(ENABLE_FULL_TESTS)
#define TESTBP16TOB(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,    \
                    YALIGN, S_DEPTH)                                          \
  TESTBP16TOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN, YALIGN, \
               benchmark_width_ + 1, _Any, +, 0, 0, S_DEPTH)                  \
  TESTBP16TOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN, YALIGN, \
               benchmark_width_, _Unaligned, +, 4, 4, S_DEPTH)                \
  TESTBP16TOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN, YALIGN, \
               benchmark_width_, _Invert, -, 0, 0, S_DEPTH)                   \
  TESTBP16TOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN, YALIGN, \
               benchmark_width_, _Opt, +, 0, 0, S_DEPTH)
#else
#define TESTBP16TOB(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,    \
                    YALIGN, S_DEPTH)                                          \
  TESTBP16TOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN, YALIGN, \
               benchmark_width_, _Opt, +, 0, 0, S_DEPTH)
#endif

#define P010ToARGB(a, b, c, d, e, f, g, h) \
  P010ToARGBMatrix(a, b, c, d, e, f, &kYuvH709Constants, g, h)
#define P210ToARGB(a, b, c, d, e, f, g, h) \
  P210ToARGBMatrix(a, b, c, d, e, f, &kYuvH709Constants, g, h)
#define P010ToAR30(a, b, c, d, e, f, g, h) \
  P010ToAR30Matrix(a, b, c, d, e, f, &kYuvH709Constants, g, h)
#define P210ToAR30(a, b, c, d, e, f, g, h) \
  P210ToAR30Matrix(a, b, c, d, e, f, &kYuvH709Constants, g, h)

#define P012ToARGB(a, b, c, d, e, f, g, h) \
  P012ToARGBMatrix(a, b, c, d, e, f, &kYuvH709Constants, g, h)
#define P212ToARGB(a, b, c, d, e, f, g, h) \
  P212ToARGBMatrix(a, b, c, d, e, f, &kYuvH709Constants, g, h)
#define P012ToAR30(a, b, c, d, e, f, g, h) \
  P012ToAR30Matrix(a, b, c, d, e, f, &kYuvH709Constants, g, h)
#define P212ToAR30(a, b, c, d, e, f, g, h) \
  P212ToAR30Matrix(a, b, c, d, e, f, &kYuvH709Constants, g, h)

#define P016ToARGB(a, b, c, d, e, f, g, h) \
  P016ToARGBMatrix(a, b, c, d, e, f, &kYuvH709Constants, g, h)
#define P216ToARGB(a, b, c, d, e, f, g, h) \
  P216ToARGBMatrix(a, b, c, d, e, f, &kYuvH709Constants, g, h)
#define P016ToAR30(a, b, c, d, e, f, g, h) \
  P016ToAR30Matrix(a, b, c, d, e, f, &kYuvH709Constants, g, h)
#define P216ToAR30(a, b, c, d, e, f, g, h) \
  P216ToAR30Matrix(a, b, c, d, e, f, &kYuvH709Constants, g, h)

#define P010ToARGBFilter(a, b, c, d, e, f, g, h)                     \
  P010ToARGBMatrixFilter(a, b, c, d, e, f, &kYuvH709Constants, g, h, \
                         kFilterBilinear)
#define P210ToARGBFilter(a, b, c, d, e, f, g, h)                     \
  P210ToARGBMatrixFilter(a, b, c, d, e, f, &kYuvH709Constants, g, h, \
                         kFilterBilinear)
#define P010ToAR30Filter(a, b, c, d, e, f, g, h)                     \
  P010ToAR30MatrixFilter(a, b, c, d, e, f, &kYuvH709Constants, g, h, \
                         kFilterBilinear)
#define P210ToAR30Filter(a, b, c, d, e, f, g, h)                     \
  P210ToAR30MatrixFilter(a, b, c, d, e, f, &kYuvH709Constants, g, h, \
                         kFilterBilinear)

#if !defined(DISABLE_SLOW_TESTS) || defined(__x86_64__) || defined(__i386__)
TESTBP16TOB(P010, 2, 2, ARGB, 4, 4, 1, 10)
TESTBP16TOB(P210, 2, 1, ARGB, 4, 4, 1, 10)
TESTBP16TOB(P012, 2, 2, ARGB, 4, 4, 1, 12)
TESTBP16TOB(P212, 2, 1, ARGB, 4, 4, 1, 12)
TESTBP16TOB(P016, 2, 2, ARGB, 4, 4, 1, 16)
TESTBP16TOB(P216, 2, 1, ARGB, 4, 4, 1, 16)
TESTBP16TOB(P010, 2, 2, ARGBFilter, 4, 4, 1, 10)
TESTBP16TOB(P210, 2, 1, ARGBFilter, 4, 4, 1, 10)
#ifdef LITTLE_ENDIAN_ONLY_TEST
TESTBP16TOB(P010, 2, 2, AR30, 4, 4, 1, 10)
TESTBP16TOB(P210, 2, 1, AR30, 4, 4, 1, 10)
TESTBP16TOB(P012, 2, 2, AR30, 4, 4, 1, 12)
TESTBP16TOB(P212, 2, 1, AR30, 4, 4, 1, 12)
TESTBP16TOB(P016, 2, 2, AR30, 4, 4, 1, 16)
TESTBP16TOB(P216, 2, 1, AR30, 4, 4, 1, 16)
TESTBP16TOB(P010, 2, 2, AR30Filter, 4, 4, 1, 10)
TESTBP16TOB(P210, 2, 1, AR30Filter, 4, 4, 1, 10)
#endif  // LITTLE_ENDIAN_ONLY_TEST
#endif  // DISABLE_SLOW_TESTS

static int Clamp(int y) {
  if (y < 0) {
    y = 0;
  }
  if (y > 255) {
    y = 255;
  }
  return y;
}

static int Clamp10(int y) {
  if (y < 0) {
    y = 0;
  }
  if (y > 1023) {
    y = 1023;
  }
  return y;
}

// Test 8 bit YUV to 8 bit RGB
TEST_F(LibYUVConvertTest, TestH420ToARGB) {
  const int kSize = 256;
  int histogram_b[256];
  int histogram_g[256];
  int histogram_r[256];
  memset(histogram_b, 0, sizeof(histogram_b));
  memset(histogram_g, 0, sizeof(histogram_g));
  memset(histogram_r, 0, sizeof(histogram_r));
  align_buffer_page_end(orig_yuv, kSize + kSize / 2 * 2);
  align_buffer_page_end(argb_pixels, kSize * 4);
  uint8_t* orig_y = orig_yuv;
  uint8_t* orig_u = orig_y + kSize;
  uint8_t* orig_v = orig_u + kSize / 2;

  // Test grey scale
  for (int i = 0; i < kSize; ++i) {
    orig_y[i] = i;
  }
  for (int i = 0; i < kSize / 2; ++i) {
    orig_u[i] = 128;  // 128 is 0.
    orig_v[i] = 128;
  }

  H420ToARGB(orig_y, 0, orig_u, 0, orig_v, 0, argb_pixels, 0, kSize, 1);

  for (int i = 0; i < kSize; ++i) {
    int b = argb_pixels[i * 4 + 0];
    int g = argb_pixels[i * 4 + 1];
    int r = argb_pixels[i * 4 + 2];
    int a = argb_pixels[i * 4 + 3];
    ++histogram_b[b];
    ++histogram_g[g];
    ++histogram_r[r];
    // Reference formula for Y channel contribution in YUV to RGB conversions:
    int expected_y = Clamp(static_cast<int>((i - 16) * 1.164f + 0.5f));
    EXPECT_EQ(b, expected_y);
    EXPECT_EQ(g, expected_y);
    EXPECT_EQ(r, expected_y);
    EXPECT_EQ(a, 255);
  }

  int count_b = 0;
  int count_g = 0;
  int count_r = 0;
  for (int i = 0; i < kSize; ++i) {
    if (histogram_b[i]) {
      ++count_b;
    }
    if (histogram_g[i]) {
      ++count_g;
    }
    if (histogram_r[i]) {
      ++count_r;
    }
  }
  printf("uniques: B %d, G, %d, R %d\n", count_b, count_g, count_r);

  free_aligned_buffer_page_end(orig_yuv);
  free_aligned_buffer_page_end(argb_pixels);
}

// Test 10 bit YUV to 8 bit RGB
TEST_F(LibYUVConvertTest, TestH010ToARGB) {
  const int kSize = 1024;
  int histogram_b[1024];
  int histogram_g[1024];
  int histogram_r[1024];
  memset(histogram_b, 0, sizeof(histogram_b));
  memset(histogram_g, 0, sizeof(histogram_g));
  memset(histogram_r, 0, sizeof(histogram_r));
  align_buffer_page_end(orig_yuv, kSize * 2 + kSize / 2 * 2 * 2);
  align_buffer_page_end(argb_pixels, kSize * 4);
  uint16_t* orig_y = reinterpret_cast<uint16_t*>(orig_yuv);
  uint16_t* orig_u = orig_y + kSize;
  uint16_t* orig_v = orig_u + kSize / 2;

  // Test grey scale
  for (int i = 0; i < kSize; ++i) {
    orig_y[i] = i;
  }
  for (int i = 0; i < kSize / 2; ++i) {
    orig_u[i] = 512;  // 512 is 0.
    orig_v[i] = 512;
  }

  H010ToARGB(orig_y, 0, orig_u, 0, orig_v, 0, argb_pixels, 0, kSize, 1);

  for (int i = 0; i < kSize; ++i) {
    int b = argb_pixels[i * 4 + 0];
    int g = argb_pixels[i * 4 + 1];
    int r = argb_pixels[i * 4 + 2];
    int a = argb_pixels[i * 4 + 3];
    ++histogram_b[b];
    ++histogram_g[g];
    ++histogram_r[r];
    int expected_y = Clamp(static_cast<int>((i - 64) * 1.164f / 4));
    EXPECT_NEAR(b, expected_y, 1);
    EXPECT_NEAR(g, expected_y, 1);
    EXPECT_NEAR(r, expected_y, 1);
    EXPECT_EQ(a, 255);
  }

  int count_b = 0;
  int count_g = 0;
  int count_r = 0;
  for (int i = 0; i < kSize; ++i) {
    if (histogram_b[i]) {
      ++count_b;
    }
    if (histogram_g[i]) {
      ++count_g;
    }
    if (histogram_r[i]) {
      ++count_r;
    }
  }
  printf("uniques: B %d, G, %d, R %d\n", count_b, count_g, count_r);

  free_aligned_buffer_page_end(orig_yuv);
  free_aligned_buffer_page_end(argb_pixels);
}

// Test 10 bit YUV to 10 bit RGB
// Caveat: Result is near due to float rounding in expected
// result.
TEST_F(LibYUVConvertTest, TestH010ToAR30) {
  const int kSize = 1024;
  int histogram_b[1024];
  int histogram_g[1024];
  int histogram_r[1024];
  memset(histogram_b, 0, sizeof(histogram_b));
  memset(histogram_g, 0, sizeof(histogram_g));
  memset(histogram_r, 0, sizeof(histogram_r));

  align_buffer_page_end(orig_yuv, kSize * 2 + kSize / 2 * 2 * 2);
  align_buffer_page_end(ar30_pixels, kSize * 4);
  uint16_t* orig_y = reinterpret_cast<uint16_t*>(orig_yuv);
  uint16_t* orig_u = orig_y + kSize;
  uint16_t* orig_v = orig_u + kSize / 2;

  // Test grey scale
  for (int i = 0; i < kSize; ++i) {
    orig_y[i] = i;
  }
  for (int i = 0; i < kSize / 2; ++i) {
    orig_u[i] = 512;  // 512 is 0.
    orig_v[i] = 512;
  }

  H010ToAR30(orig_y, 0, orig_u, 0, orig_v, 0, ar30_pixels, 0, kSize, 1);

  for (int i = 0; i < kSize; ++i) {
    int b10 = reinterpret_cast<uint32_t*>(ar30_pixels)[i] & 1023;
    int g10 = (reinterpret_cast<uint32_t*>(ar30_pixels)[i] >> 10) & 1023;
    int r10 = (reinterpret_cast<uint32_t*>(ar30_pixels)[i] >> 20) & 1023;
    int a2 = (reinterpret_cast<uint32_t*>(ar30_pixels)[i] >> 30) & 3;
    ++histogram_b[b10];
    ++histogram_g[g10];
    ++histogram_r[r10];
    int expected_y = Clamp10(static_cast<int>((i - 64) * 1.164f + 0.5));
    EXPECT_NEAR(b10, expected_y, 4);
    EXPECT_NEAR(g10, expected_y, 4);
    EXPECT_NEAR(r10, expected_y, 4);
    EXPECT_EQ(a2, 3);
  }

  int count_b = 0;
  int count_g = 0;
  int count_r = 0;
  for (int i = 0; i < kSize; ++i) {
    if (histogram_b[i]) {
      ++count_b;
    }
    if (histogram_g[i]) {
      ++count_g;
    }
    if (histogram_r[i]) {
      ++count_r;
    }
  }
  printf("uniques: B %d, G, %d, R %d\n", count_b, count_g, count_r);

  free_aligned_buffer_page_end(orig_yuv);
  free_aligned_buffer_page_end(ar30_pixels);
}

// Test 10 bit YUV to 10 bit RGB
// Caveat: Result is near due to float rounding in expected
// result.
TEST_F(LibYUVConvertTest, TestH010ToAB30) {
  const int kSize = 1024;
  int histogram_b[1024];
  int histogram_g[1024];
  int histogram_r[1024];
  memset(histogram_b, 0, sizeof(histogram_b));
  memset(histogram_g, 0, sizeof(histogram_g));
  memset(histogram_r, 0, sizeof(histogram_r));

  align_buffer_page_end(orig_yuv, kSize * 2 + kSize / 2 * 2 * 2);
  align_buffer_page_end(ab30_pixels, kSize * 4);
  uint16_t* orig_y = reinterpret_cast<uint16_t*>(orig_yuv);
  uint16_t* orig_u = orig_y + kSize;
  uint16_t* orig_v = orig_u + kSize / 2;

  // Test grey scale
  for (int i = 0; i < kSize; ++i) {
    orig_y[i] = i;
  }
  for (int i = 0; i < kSize / 2; ++i) {
    orig_u[i] = 512;  // 512 is 0.
    orig_v[i] = 512;
  }

  H010ToAB30(orig_y, 0, orig_u, 0, orig_v, 0, ab30_pixels, 0, kSize, 1);

  for (int i = 0; i < kSize; ++i) {
    int r10 = reinterpret_cast<uint32_t*>(ab30_pixels)[i] & 1023;
    int g10 = (reinterpret_cast<uint32_t*>(ab30_pixels)[i] >> 10) & 1023;
    int b10 = (reinterpret_cast<uint32_t*>(ab30_pixels)[i] >> 20) & 1023;
    int a2 = (reinterpret_cast<uint32_t*>(ab30_pixels)[i] >> 30) & 3;
    ++histogram_b[b10];
    ++histogram_g[g10];
    ++histogram_r[r10];
    int expected_y = Clamp10(static_cast<int>((i - 64) * 1.164f));
    EXPECT_NEAR(b10, expected_y, 4);
    EXPECT_NEAR(g10, expected_y, 4);
    EXPECT_NEAR(r10, expected_y, 4);
    EXPECT_EQ(a2, 3);
  }

  int count_b = 0;
  int count_g = 0;
  int count_r = 0;
  for (int i = 0; i < kSize; ++i) {
    if (histogram_b[i]) {
      ++count_b;
    }
    if (histogram_g[i]) {
      ++count_g;
    }
    if (histogram_r[i]) {
      ++count_r;
    }
  }
  printf("uniques: B %d, G, %d, R %d\n", count_b, count_g, count_r);

  free_aligned_buffer_page_end(orig_yuv);
  free_aligned_buffer_page_end(ab30_pixels);
}

// Test 8 bit YUV to 10 bit RGB
TEST_F(LibYUVConvertTest, TestH420ToAR30) {
  const int kSize = 256;
  const int kHistSize = 1024;
  int histogram_b[kHistSize];
  int histogram_g[kHistSize];
  int histogram_r[kHistSize];
  memset(histogram_b, 0, sizeof(histogram_b));
  memset(histogram_g, 0, sizeof(histogram_g));
  memset(histogram_r, 0, sizeof(histogram_r));
  align_buffer_page_end(orig_yuv, kSize + kSize / 2 * 2);
  align_buffer_page_end(ar30_pixels, kSize * 4);
  uint8_t* orig_y = orig_yuv;
  uint8_t* orig_u = orig_y + kSize;
  uint8_t* orig_v = orig_u + kSize / 2;

  // Test grey scale
  for (int i = 0; i < kSize; ++i) {
    orig_y[i] = i;
  }
  for (int i = 0; i < kSize / 2; ++i) {
    orig_u[i] = 128;  // 128 is 0.
    orig_v[i] = 128;
  }

  H420ToAR30(orig_y, 0, orig_u, 0, orig_v, 0, ar30_pixels, 0, kSize, 1);

  for (int i = 0; i < kSize; ++i) {
    int b10 = reinterpret_cast<uint32_t*>(ar30_pixels)[i] & 1023;
    int g10 = (reinterpret_cast<uint32_t*>(ar30_pixels)[i] >> 10) & 1023;
    int r10 = (reinterpret_cast<uint32_t*>(ar30_pixels)[i] >> 20) & 1023;
    int a2 = (reinterpret_cast<uint32_t*>(ar30_pixels)[i] >> 30) & 3;
    ++histogram_b[b10];
    ++histogram_g[g10];
    ++histogram_r[r10];
    int expected_y = Clamp10(static_cast<int>((i - 16) * 1.164f * 4.f));
    EXPECT_NEAR(b10, expected_y, 4);
    EXPECT_NEAR(g10, expected_y, 4);
    EXPECT_NEAR(r10, expected_y, 4);
    EXPECT_EQ(a2, 3);
  }

  int count_b = 0;
  int count_g = 0;
  int count_r = 0;
  for (int i = 0; i < kHistSize; ++i) {
    if (histogram_b[i]) {
      ++count_b;
    }
    if (histogram_g[i]) {
      ++count_g;
    }
    if (histogram_r[i]) {
      ++count_r;
    }
  }
  printf("uniques: B %d, G, %d, R %d\n", count_b, count_g, count_r);

  free_aligned_buffer_page_end(orig_yuv);
  free_aligned_buffer_page_end(ar30_pixels);
}

// Test I400 with jpeg matrix is same as J400
TEST_F(LibYUVConvertTest, TestI400) {
  const int kSize = 256;
  align_buffer_page_end(orig_i400, kSize);
  align_buffer_page_end(argb_pixels_i400, kSize * 4);
  align_buffer_page_end(argb_pixels_j400, kSize * 4);
  align_buffer_page_end(argb_pixels_jpeg_i400, kSize * 4);
  align_buffer_page_end(argb_pixels_h709_i400, kSize * 4);
  align_buffer_page_end(argb_pixels_2020_i400, kSize * 4);

  // Test grey scale
  for (int i = 0; i < kSize; ++i) {
    orig_i400[i] = i;
  }

  J400ToARGB(orig_i400, 0, argb_pixels_j400, 0, kSize, 1);
  I400ToARGB(orig_i400, 0, argb_pixels_i400, 0, kSize, 1);
  I400ToARGBMatrix(orig_i400, 0, argb_pixels_jpeg_i400, 0, &kYuvJPEGConstants,
                   kSize, 1);
  I400ToARGBMatrix(orig_i400, 0, argb_pixels_h709_i400, 0, &kYuvH709Constants,
                   kSize, 1);
  I400ToARGBMatrix(orig_i400, 0, argb_pixels_2020_i400, 0, &kYuv2020Constants,
                   kSize, 1);

  EXPECT_EQ(0, argb_pixels_i400[0]);
  EXPECT_EQ(0, argb_pixels_j400[0]);
  EXPECT_EQ(0, argb_pixels_jpeg_i400[0]);
  EXPECT_EQ(0, argb_pixels_h709_i400[0]);
  EXPECT_EQ(0, argb_pixels_2020_i400[0]);
  EXPECT_EQ(0, argb_pixels_i400[16 * 4]);
  EXPECT_EQ(16, argb_pixels_j400[16 * 4]);
  EXPECT_EQ(16, argb_pixels_jpeg_i400[16 * 4]);
  EXPECT_EQ(0, argb_pixels_h709_i400[16 * 4]);
  EXPECT_EQ(0, argb_pixels_2020_i400[16 * 4]);
  EXPECT_EQ(130, argb_pixels_i400[128 * 4]);
  EXPECT_EQ(128, argb_pixels_j400[128 * 4]);
  EXPECT_EQ(128, argb_pixels_jpeg_i400[128 * 4]);
  EXPECT_EQ(130, argb_pixels_h709_i400[128 * 4]);
  EXPECT_EQ(130, argb_pixels_2020_i400[128 * 4]);
  EXPECT_EQ(255, argb_pixels_i400[255 * 4]);
  EXPECT_EQ(255, argb_pixels_j400[255 * 4]);
  EXPECT_EQ(255, argb_pixels_jpeg_i400[255 * 4]);
  EXPECT_EQ(255, argb_pixels_h709_i400[255 * 4]);
  EXPECT_EQ(255, argb_pixels_2020_i400[255 * 4]);

  for (int i = 0; i < kSize * 4; ++i) {
    if ((i & 3) == 3) {
      EXPECT_EQ(255, argb_pixels_j400[i]);
    } else {
      EXPECT_EQ(i / 4, argb_pixels_j400[i]);
    }
    EXPECT_EQ(argb_pixels_jpeg_i400[i], argb_pixels_j400[i]);
  }

  free_aligned_buffer_page_end(orig_i400);
  free_aligned_buffer_page_end(argb_pixels_i400);
  free_aligned_buffer_page_end(argb_pixels_j400);
  free_aligned_buffer_page_end(argb_pixels_jpeg_i400);
  free_aligned_buffer_page_end(argb_pixels_h709_i400);
  free_aligned_buffer_page_end(argb_pixels_2020_i400);
}

// Test RGB24 to ARGB and back to RGB24
TEST_F(LibYUVConvertTest, TestARGBToRGB24) {
  const int kSize = 256;
  align_buffer_page_end(orig_rgb24, kSize * 3);
  align_buffer_page_end(argb_pixels, kSize * 4);
  align_buffer_page_end(dest_rgb24, kSize * 3);

  // Test grey scale
  for (int i = 0; i < kSize * 3; ++i) {
    orig_rgb24[i] = i;
  }

  RGB24ToARGB(orig_rgb24, 0, argb_pixels, 0, kSize, 1);
  ARGBToRGB24(argb_pixels, 0, dest_rgb24, 0, kSize, 1);

  for (int i = 0; i < kSize * 3; ++i) {
    EXPECT_EQ(orig_rgb24[i], dest_rgb24[i]);
  }

  free_aligned_buffer_page_end(orig_rgb24);
  free_aligned_buffer_page_end(argb_pixels);
  free_aligned_buffer_page_end(dest_rgb24);
}

TEST_F(LibYUVConvertTest, TestARGBToRGB565) {
  SIMD_ALIGNED(uint8_t orig_pixels[256][4]);
  SIMD_ALIGNED(uint8_t dest_rgb565[256][2]);

  for (int i = 0; i < 256; ++i) {
    for (int j = 0; j < 4; ++j) {
      orig_pixels[i][j] = i;
    }
  }
  ARGBToRGB565(&orig_pixels[0][0], 0, &dest_rgb565[0][0], 0, 256, 1);
  uint32_t checksum = HashDjb2(&dest_rgb565[0][0], sizeof(dest_rgb565), 5381);
  EXPECT_EQ(610919429u, checksum);
}

TEST_F(LibYUVConvertTest, TestYUY2ToARGB) {
  SIMD_ALIGNED(uint8_t orig_pixels[256][2]);
  SIMD_ALIGNED(uint8_t dest_argb[256][4]);

  for (int i = 0; i < 256; ++i) {
    for (int j = 0; j < 2; ++j) {
      orig_pixels[i][j] = i;
    }
  }
  YUY2ToARGB(&orig_pixels[0][0], 0, &dest_argb[0][0], 0, 256, 1);
  uint32_t checksum = HashDjb2(&dest_argb[0][0], sizeof(dest_argb), 5381);
  EXPECT_EQ(3486643515u, checksum);
}

TEST_F(LibYUVConvertTest, TestUYVYToARGB) {
  SIMD_ALIGNED(uint8_t orig_pixels[256][2]);
  SIMD_ALIGNED(uint8_t dest_argb[256][4]);

  for (int i = 0; i < 256; ++i) {
    for (int j = 0; j < 2; ++j) {
      orig_pixels[i][j] = i;
    }
  }
  UYVYToARGB(&orig_pixels[0][0], 0, &dest_argb[0][0], 0, 256, 1);
  uint32_t checksum = HashDjb2(&dest_argb[0][0], sizeof(dest_argb), 5381);
  EXPECT_EQ(3486643515u, checksum);
}
#endif  // !defined(LEAN_TESTS)

}  // namespace libyuv
