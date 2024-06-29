/*
 *  Copyright 2011 The LibYuv Project Authors. All rights reserved.
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

// Planar test

#define TESTPLANARTOPI(SRC_FMT_PLANAR, SRC_T, SRC_BPC, SRC_SUBSAMP_X,         \
                       SRC_SUBSAMP_Y, FMT_PLANAR, DST_T, DST_BPC,             \
                       DST_SUBSAMP_X, DST_SUBSAMP_Y, W1280, N, NEG, OFF,      \
                       SRC_DEPTH)                                             \
  TEST_F(LibYUVConvertTest, SRC_FMT_PLANAR##To##FMT_PLANAR##N) {              \
    static_assert(SRC_BPC == 1 || SRC_BPC == 2, "SRC BPC unsupported");       \
    static_assert(DST_BPC == 1 || DST_BPC == 2, "DST BPC unsupported");       \
    static_assert(SRC_SUBSAMP_X == 1 || SRC_SUBSAMP_X == 2,                   \
                  "SRC_SUBSAMP_X unsupported");                               \
    static_assert(SRC_SUBSAMP_Y == 1 || SRC_SUBSAMP_Y == 2,                   \
                  "SRC_SUBSAMP_Y unsupported");                               \
    static_assert(DST_SUBSAMP_X == 1 || DST_SUBSAMP_X == 2,                   \
                  "DST_SUBSAMP_X unsupported");                               \
    static_assert(DST_SUBSAMP_Y == 1 || DST_SUBSAMP_Y == 2,                   \
                  "DST_SUBSAMP_Y unsupported");                               \
    const int kWidth = W1280;                                                 \
    const int kHeight = benchmark_height_;                                    \
    const int kSrcHalfWidth = SUBSAMPLE(kWidth, SRC_SUBSAMP_X);               \
    const int kSrcHalfHeight = SUBSAMPLE(kHeight, SRC_SUBSAMP_Y);             \
    const int kDstHalfWidth = SUBSAMPLE(kWidth, DST_SUBSAMP_X);               \
    const int kDstHalfHeight = SUBSAMPLE(kHeight, DST_SUBSAMP_Y);             \
    align_buffer_page_end(src_y, kWidth* kHeight* SRC_BPC + OFF);             \
    align_buffer_page_end(src_u,                                              \
                          kSrcHalfWidth* kSrcHalfHeight* SRC_BPC + OFF);      \
    align_buffer_page_end(src_v,                                              \
                          kSrcHalfWidth* kSrcHalfHeight* SRC_BPC + OFF);      \
    align_buffer_page_end(dst_y_c, kWidth* kHeight* DST_BPC);                 \
    align_buffer_page_end(dst_u_c, kDstHalfWidth* kDstHalfHeight* DST_BPC);   \
    align_buffer_page_end(dst_v_c, kDstHalfWidth* kDstHalfHeight* DST_BPC);   \
    align_buffer_page_end(dst_y_opt, kWidth* kHeight* DST_BPC);               \
    align_buffer_page_end(dst_u_opt, kDstHalfWidth* kDstHalfHeight* DST_BPC); \
    align_buffer_page_end(dst_v_opt, kDstHalfWidth* kDstHalfHeight* DST_BPC); \
    MemRandomize(src_y + OFF, kWidth * kHeight * SRC_BPC);                    \
    MemRandomize(src_u + OFF, kSrcHalfWidth * kSrcHalfHeight * SRC_BPC);      \
    MemRandomize(src_v + OFF, kSrcHalfWidth * kSrcHalfHeight * SRC_BPC);      \
    SRC_T* src_y_p = reinterpret_cast<SRC_T*>(src_y + OFF);                   \
    SRC_T* src_u_p = reinterpret_cast<SRC_T*>(src_u + OFF);                   \
    SRC_T* src_v_p = reinterpret_cast<SRC_T*>(src_v + OFF);                   \
    for (int i = 0; i < kWidth * kHeight; ++i) {                              \
      src_y_p[i] = src_y_p[i] & ((1 << SRC_DEPTH) - 1);                       \
    }                                                                         \
    for (int i = 0; i < kSrcHalfWidth * kSrcHalfHeight; ++i) {                \
      src_u_p[i] = src_u_p[i] & ((1 << SRC_DEPTH) - 1);                       \
      src_v_p[i] = src_v_p[i] & ((1 << SRC_DEPTH) - 1);                       \
    }                                                                         \
    memset(dst_y_c, 1, kWidth* kHeight* DST_BPC);                             \
    memset(dst_u_c, 2, kDstHalfWidth* kDstHalfHeight* DST_BPC);               \
    memset(dst_v_c, 3, kDstHalfWidth* kDstHalfHeight* DST_BPC);               \
    memset(dst_y_opt, 101, kWidth* kHeight* DST_BPC);                         \
    memset(dst_u_opt, 102, kDstHalfWidth* kDstHalfHeight* DST_BPC);           \
    memset(dst_v_opt, 103, kDstHalfWidth* kDstHalfHeight* DST_BPC);           \
    MaskCpuFlags(disable_cpu_flags_);                                         \
    SRC_FMT_PLANAR##To##FMT_PLANAR(                                           \
        src_y_p, kWidth, src_u_p, kSrcHalfWidth, src_v_p, kSrcHalfWidth,      \
        reinterpret_cast<DST_T*>(dst_y_c), kWidth,                            \
        reinterpret_cast<DST_T*>(dst_u_c), kDstHalfWidth,                     \
        reinterpret_cast<DST_T*>(dst_v_c), kDstHalfWidth, kWidth,             \
        NEG kHeight);                                                         \
    MaskCpuFlags(benchmark_cpu_info_);                                        \
    for (int i = 0; i < benchmark_iterations_; ++i) {                         \
      SRC_FMT_PLANAR##To##FMT_PLANAR(                                         \
          src_y_p, kWidth, src_u_p, kSrcHalfWidth, src_v_p, kSrcHalfWidth,    \
          reinterpret_cast<DST_T*>(dst_y_opt), kWidth,                        \
          reinterpret_cast<DST_T*>(dst_u_opt), kDstHalfWidth,                 \
          reinterpret_cast<DST_T*>(dst_v_opt), kDstHalfWidth, kWidth,         \
          NEG kHeight);                                                       \
    }                                                                         \
    for (int i = 0; i < kHeight * kWidth * DST_BPC; ++i) {                    \
      EXPECT_EQ(dst_y_c[i], dst_y_opt[i]);                                    \
    }                                                                         \
    for (int i = 0; i < kDstHalfWidth * kDstHalfHeight * DST_BPC; ++i) {      \
      EXPECT_EQ(dst_u_c[i], dst_u_opt[i]);                                    \
      EXPECT_EQ(dst_v_c[i], dst_v_opt[i]);                                    \
    }                                                                         \
    free_aligned_buffer_page_end(dst_y_c);                                    \
    free_aligned_buffer_page_end(dst_u_c);                                    \
    free_aligned_buffer_page_end(dst_v_c);                                    \
    free_aligned_buffer_page_end(dst_y_opt);                                  \
    free_aligned_buffer_page_end(dst_u_opt);                                  \
    free_aligned_buffer_page_end(dst_v_opt);                                  \
    free_aligned_buffer_page_end(src_y);                                      \
    free_aligned_buffer_page_end(src_u);                                      \
    free_aligned_buffer_page_end(src_v);                                      \
  }

#if defined(ENABLE_FULL_TESTS)
#define TESTPLANARTOP(SRC_FMT_PLANAR, SRC_T, SRC_BPC, SRC_SUBSAMP_X,           \
                      SRC_SUBSAMP_Y, FMT_PLANAR, DST_T, DST_BPC,               \
                      DST_SUBSAMP_X, DST_SUBSAMP_Y, SRC_DEPTH)                 \
  TESTPLANARTOPI(SRC_FMT_PLANAR, SRC_T, SRC_BPC, SRC_SUBSAMP_X, SRC_SUBSAMP_Y, \
                 FMT_PLANAR, DST_T, DST_BPC, DST_SUBSAMP_X, DST_SUBSAMP_Y,     \
                 benchmark_width_ + 1, _Any, +, 0, SRC_DEPTH)                  \
  TESTPLANARTOPI(SRC_FMT_PLANAR, SRC_T, SRC_BPC, SRC_SUBSAMP_X, SRC_SUBSAMP_Y, \
                 FMT_PLANAR, DST_T, DST_BPC, DST_SUBSAMP_X, DST_SUBSAMP_Y,     \
                 benchmark_width_, _Unaligned, +, 2, SRC_DEPTH)                \
  TESTPLANARTOPI(SRC_FMT_PLANAR, SRC_T, SRC_BPC, SRC_SUBSAMP_X, SRC_SUBSAMP_Y, \
                 FMT_PLANAR, DST_T, DST_BPC, DST_SUBSAMP_X, DST_SUBSAMP_Y,     \
                 benchmark_width_, _Invert, -, 0, SRC_DEPTH)                   \
  TESTPLANARTOPI(SRC_FMT_PLANAR, SRC_T, SRC_BPC, SRC_SUBSAMP_X, SRC_SUBSAMP_Y, \
                 FMT_PLANAR, DST_T, DST_BPC, DST_SUBSAMP_X, DST_SUBSAMP_Y,     \
                 benchmark_width_, _Opt, +, 0, SRC_DEPTH)
#else
#define TESTPLANARTOP(SRC_FMT_PLANAR, SRC_T, SRC_BPC, SRC_SUBSAMP_X,           \
                      SRC_SUBSAMP_Y, FMT_PLANAR, DST_T, DST_BPC,               \
                      DST_SUBSAMP_X, DST_SUBSAMP_Y, SRC_DEPTH)                 \
  TESTPLANARTOPI(SRC_FMT_PLANAR, SRC_T, SRC_BPC, SRC_SUBSAMP_X, SRC_SUBSAMP_Y, \
                 FMT_PLANAR, DST_T, DST_BPC, DST_SUBSAMP_X, DST_SUBSAMP_Y,     \
                 benchmark_width_, _Opt, +, 0, SRC_DEPTH)
#endif

TESTPLANARTOP(I420, uint8_t, 1, 2, 2, I420, uint8_t, 1, 2, 2, 8)
TESTPLANARTOP(I422, uint8_t, 1, 2, 1, I420, uint8_t, 1, 2, 2, 8)
TESTPLANARTOP(I444, uint8_t, 1, 1, 1, I420, uint8_t, 1, 2, 2, 8)
TESTPLANARTOP(I420, uint8_t, 1, 2, 2, I422, uint8_t, 1, 2, 1, 8)
TESTPLANARTOP(I420, uint8_t, 1, 2, 2, I444, uint8_t, 1, 1, 1, 8)
TESTPLANARTOP(I420, uint8_t, 1, 2, 2, I420Mirror, uint8_t, 1, 2, 2, 8)
TESTPLANARTOP(I422, uint8_t, 1, 2, 1, I422, uint8_t, 1, 2, 1, 8)
TESTPLANARTOP(I422, uint8_t, 1, 2, 1, I444, uint8_t, 1, 1, 1, 8)
TESTPLANARTOP(I444, uint8_t, 1, 1, 1, I444, uint8_t, 1, 1, 1, 8)
TESTPLANARTOP(I010, uint16_t, 2, 2, 2, I010, uint16_t, 2, 2, 2, 10)
TESTPLANARTOP(I420, uint8_t, 1, 2, 2, I010, uint16_t, 2, 2, 2, 8)
TESTPLANARTOP(I420, uint8_t, 1, 2, 2, I012, uint16_t, 2, 2, 2, 8)
TESTPLANARTOP(H010, uint16_t, 2, 2, 2, H010, uint16_t, 2, 2, 2, 10)
TESTPLANARTOP(H010, uint16_t, 2, 2, 2, H420, uint8_t, 1, 2, 2, 10)
TESTPLANARTOP(H420, uint8_t, 1, 2, 2, H010, uint16_t, 2, 2, 2, 8)
TESTPLANARTOP(H420, uint8_t, 1, 2, 2, H012, uint16_t, 2, 2, 2, 8)
TESTPLANARTOP(I010, uint16_t, 2, 2, 2, I410, uint16_t, 2, 1, 1, 10)
TESTPLANARTOP(I210, uint16_t, 2, 2, 1, I410, uint16_t, 2, 1, 1, 10)
TESTPLANARTOP(I012, uint16_t, 2, 2, 2, I412, uint16_t, 2, 1, 1, 12)
TESTPLANARTOP(I212, uint16_t, 2, 2, 1, I412, uint16_t, 2, 1, 1, 12)
TESTPLANARTOP(I410, uint16_t, 2, 1, 1, I010, uint16_t, 2, 2, 2, 10)
TESTPLANARTOP(I210, uint16_t, 2, 2, 1, I010, uint16_t, 2, 2, 2, 10)
TESTPLANARTOP(I412, uint16_t, 2, 1, 1, I012, uint16_t, 2, 2, 2, 12)
TESTPLANARTOP(I212, uint16_t, 2, 2, 1, I012, uint16_t, 2, 2, 2, 12)
TESTPLANARTOP(I010, uint16_t, 2, 2, 2, I420, uint8_t, 1, 2, 2, 10)
TESTPLANARTOP(I210, uint16_t, 2, 2, 1, I420, uint8_t, 1, 2, 2, 10)
TESTPLANARTOP(I210, uint16_t, 2, 2, 1, I422, uint8_t, 1, 2, 1, 10)
TESTPLANARTOP(I410, uint16_t, 2, 1, 1, I420, uint8_t, 1, 2, 2, 10)
TESTPLANARTOP(I410, uint16_t, 2, 1, 1, I444, uint8_t, 1, 1, 1, 10)
TESTPLANARTOP(I012, uint16_t, 2, 2, 2, I420, uint8_t, 1, 2, 2, 12)
TESTPLANARTOP(I212, uint16_t, 2, 2, 1, I420, uint8_t, 1, 2, 2, 12)
TESTPLANARTOP(I212, uint16_t, 2, 2, 1, I422, uint8_t, 1, 2, 1, 12)
TESTPLANARTOP(I412, uint16_t, 2, 1, 1, I420, uint8_t, 1, 2, 2, 12)
TESTPLANARTOP(I412, uint16_t, 2, 1, 1, I444, uint8_t, 1, 1, 1, 12)

// Test Android 420 to I420
#define TESTAPLANARTOPI(SRC_FMT_PLANAR, PIXEL_STRIDE, SRC_SUBSAMP_X,          \
                        SRC_SUBSAMP_Y, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,      \
                        W1280, N, NEG, OFF, PN, OFF_U, OFF_V)                 \
  TEST_F(LibYUVConvertTest, SRC_FMT_PLANAR##To##FMT_PLANAR##To##PN##N) {      \
    const int kWidth = W1280;                                                 \
    const int kHeight = benchmark_height_;                                    \
    const int kSizeUV =                                                       \
        SUBSAMPLE(kWidth, SRC_SUBSAMP_X) * SUBSAMPLE(kHeight, SRC_SUBSAMP_Y); \
    align_buffer_page_end(src_y, kWidth* kHeight + OFF);                      \
    align_buffer_page_end(src_uv,                                             \
                          kSizeUV*((PIXEL_STRIDE == 3) ? 3 : 2) + OFF);       \
    align_buffer_page_end(dst_y_c, kWidth* kHeight);                          \
    align_buffer_page_end(dst_u_c, SUBSAMPLE(kWidth, SUBSAMP_X) *             \
                                       SUBSAMPLE(kHeight, SUBSAMP_Y));        \
    align_buffer_page_end(dst_v_c, SUBSAMPLE(kWidth, SUBSAMP_X) *             \
                                       SUBSAMPLE(kHeight, SUBSAMP_Y));        \
    align_buffer_page_end(dst_y_opt, kWidth* kHeight);                        \
    align_buffer_page_end(dst_u_opt, SUBSAMPLE(kWidth, SUBSAMP_X) *           \
                                         SUBSAMPLE(kHeight, SUBSAMP_Y));      \
    align_buffer_page_end(dst_v_opt, SUBSAMPLE(kWidth, SUBSAMP_X) *           \
                                         SUBSAMPLE(kHeight, SUBSAMP_Y));      \
    uint8_t* src_u = src_uv + OFF_U;                                          \
    uint8_t* src_v = src_uv + (PIXEL_STRIDE == 1 ? kSizeUV : OFF_V);          \
    int src_stride_uv = SUBSAMPLE(kWidth, SUBSAMP_X) * PIXEL_STRIDE;          \
    for (int i = 0; i < kHeight; ++i)                                         \
      for (int j = 0; j < kWidth; ++j)                                        \
        src_y[i * kWidth + j + OFF] = (fastrand() & 0xff);                    \
    for (int i = 0; i < SUBSAMPLE(kHeight, SRC_SUBSAMP_Y); ++i) {             \
      for (int j = 0; j < SUBSAMPLE(kWidth, SRC_SUBSAMP_X); ++j) {            \
        src_u[(i * src_stride_uv) + j * PIXEL_STRIDE + OFF] =                 \
            (fastrand() & 0xff);                                              \
        src_v[(i * src_stride_uv) + j * PIXEL_STRIDE + OFF] =                 \
            (fastrand() & 0xff);                                              \
      }                                                                       \
    }                                                                         \
    memset(dst_y_c, 1, kWidth* kHeight);                                      \
    memset(dst_u_c, 2,                                                        \
           SUBSAMPLE(kWidth, SUBSAMP_X) * SUBSAMPLE(kHeight, SUBSAMP_Y));     \
    memset(dst_v_c, 3,                                                        \
           SUBSAMPLE(kWidth, SUBSAMP_X) * SUBSAMPLE(kHeight, SUBSAMP_Y));     \
    memset(dst_y_opt, 101, kWidth* kHeight);                                  \
    memset(dst_u_opt, 102,                                                    \
           SUBSAMPLE(kWidth, SUBSAMP_X) * SUBSAMPLE(kHeight, SUBSAMP_Y));     \
    memset(dst_v_opt, 103,                                                    \
           SUBSAMPLE(kWidth, SUBSAMP_X) * SUBSAMPLE(kHeight, SUBSAMP_Y));     \
    MaskCpuFlags(disable_cpu_flags_);                                         \
    SRC_FMT_PLANAR##To##FMT_PLANAR(                                           \
        src_y + OFF, kWidth, src_u + OFF, SUBSAMPLE(kWidth, SRC_SUBSAMP_X),   \
        src_v + OFF, SUBSAMPLE(kWidth, SRC_SUBSAMP_X), PIXEL_STRIDE, dst_y_c, \
        kWidth, dst_u_c, SUBSAMPLE(kWidth, SUBSAMP_X), dst_v_c,               \
        SUBSAMPLE(kWidth, SUBSAMP_X), kWidth, NEG kHeight);                   \
    MaskCpuFlags(benchmark_cpu_info_);                                        \
    for (int i = 0; i < benchmark_iterations_; ++i) {                         \
      SRC_FMT_PLANAR##To##FMT_PLANAR(                                         \
          src_y + OFF, kWidth, src_u + OFF, SUBSAMPLE(kWidth, SRC_SUBSAMP_X), \
          src_v + OFF, SUBSAMPLE(kWidth, SRC_SUBSAMP_X), PIXEL_STRIDE,        \
          dst_y_opt, kWidth, dst_u_opt, SUBSAMPLE(kWidth, SUBSAMP_X),         \
          dst_v_opt, SUBSAMPLE(kWidth, SUBSAMP_X), kWidth, NEG kHeight);      \
    }                                                                         \
    for (int i = 0; i < kHeight; ++i) {                                       \
      for (int j = 0; j < kWidth; ++j) {                                      \
        EXPECT_EQ(dst_y_c[i * kWidth + j], dst_y_opt[i * kWidth + j]);        \
      }                                                                       \
    }                                                                         \
    for (int i = 0; i < SUBSAMPLE(kHeight, SUBSAMP_Y); ++i) {                 \
      for (int j = 0; j < SUBSAMPLE(kWidth, SUBSAMP_X); ++j) {                \
        EXPECT_EQ(dst_u_c[i * SUBSAMPLE(kWidth, SUBSAMP_X) + j],              \
                  dst_u_opt[i * SUBSAMPLE(kWidth, SUBSAMP_X) + j]);           \
      }                                                                       \
    }                                                                         \
    for (int i = 0; i < SUBSAMPLE(kHeight, SUBSAMP_Y); ++i) {                 \
      for (int j = 0; j < SUBSAMPLE(kWidth, SUBSAMP_X); ++j) {                \
        EXPECT_EQ(dst_v_c[i * SUBSAMPLE(kWidth, SUBSAMP_X) + j],              \
                  dst_v_opt[i * SUBSAMPLE(kWidth, SUBSAMP_X) + j]);           \
      }                                                                       \
    }                                                                         \
    free_aligned_buffer_page_end(dst_y_c);                                    \
    free_aligned_buffer_page_end(dst_u_c);                                    \
    free_aligned_buffer_page_end(dst_v_c);                                    \
    free_aligned_buffer_page_end(dst_y_opt);                                  \
    free_aligned_buffer_page_end(dst_u_opt);                                  \
    free_aligned_buffer_page_end(dst_v_opt);                                  \
    free_aligned_buffer_page_end(src_y);                                      \
    free_aligned_buffer_page_end(src_uv);                                     \
  }

#if defined(ENABLE_FULL_TESTS)
#define TESTAPLANARTOP(SRC_FMT_PLANAR, PN, PIXEL_STRIDE, OFF_U, OFF_V,         \
                       SRC_SUBSAMP_X, SRC_SUBSAMP_Y, FMT_PLANAR, SUBSAMP_X,    \
                       SUBSAMP_Y)                                              \
  TESTAPLANARTOPI(SRC_FMT_PLANAR, PIXEL_STRIDE, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,  \
                  FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, benchmark_width_ + 1,      \
                  _Any, +, 0, PN, OFF_U, OFF_V)                                \
  TESTAPLANARTOPI(SRC_FMT_PLANAR, PIXEL_STRIDE, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,  \
                  FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, benchmark_width_,          \
                  _Unaligned, +, 2, PN, OFF_U, OFF_V)                          \
  TESTAPLANARTOPI(SRC_FMT_PLANAR, PIXEL_STRIDE, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,  \
                  FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, benchmark_width_, _Invert, \
                  -, 0, PN, OFF_U, OFF_V)                                      \
  TESTAPLANARTOPI(SRC_FMT_PLANAR, PIXEL_STRIDE, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,  \
                  FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, benchmark_width_, _Opt, +, \
                  0, PN, OFF_U, OFF_V)
#else
#define TESTAPLANARTOP(SRC_FMT_PLANAR, PN, PIXEL_STRIDE, OFF_U, OFF_V,         \
                       SRC_SUBSAMP_X, SRC_SUBSAMP_Y, FMT_PLANAR, SUBSAMP_X,    \
                       SUBSAMP_Y)                                              \
  TESTAPLANARTOPI(SRC_FMT_PLANAR, PIXEL_STRIDE, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,  \
                  FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, benchmark_width_, _Opt, +, \
                  0, PN, OFF_U, OFF_V)
#endif

TESTAPLANARTOP(Android420, I420, 1, 0, 0, 2, 2, I420, 2, 2)
TESTAPLANARTOP(Android420, NV12, 2, 0, 1, 2, 2, I420, 2, 2)
TESTAPLANARTOP(Android420, NV21, 2, 1, 0, 2, 2, I420, 2, 2)
#undef TESTAPLANARTOP
#undef TESTAPLANARTOPI

// wrapper to keep API the same
int I400ToNV21(const uint8_t* src_y,
               int src_stride_y,
               const uint8_t* /* src_u */,
               int /* src_stride_u */,
               const uint8_t* /* src_v */,
               int /* src_stride_v */,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_vu,
               int dst_stride_vu,
               int width,
               int height) {
  return I400ToNV21(src_y, src_stride_y, dst_y, dst_stride_y, dst_vu,
                    dst_stride_vu, width, height);
}

#define TESTPLANARTOBPI(SRC_FMT_PLANAR, SRC_T, SRC_BPC, SRC_SUBSAMP_X,        \
                        SRC_SUBSAMP_Y, FMT_PLANAR, DST_T, DST_BPC,            \
                        DST_SUBSAMP_X, DST_SUBSAMP_Y, W1280, N, NEG, OFF,     \
                        SRC_DEPTH)                                            \
  TEST_F(LibYUVConvertTest, SRC_FMT_PLANAR##To##FMT_PLANAR##N) {              \
    static_assert(SRC_BPC == 1 || SRC_BPC == 2, "SRC BPC unsupported");       \
    static_assert(DST_BPC == 1 || DST_BPC == 2, "DST BPC unsupported");       \
    static_assert(SRC_SUBSAMP_X == 1 || SRC_SUBSAMP_X == 2,                   \
                  "SRC_SUBSAMP_X unsupported");                               \
    static_assert(SRC_SUBSAMP_Y == 1 || SRC_SUBSAMP_Y == 2,                   \
                  "SRC_SUBSAMP_Y unsupported");                               \
    static_assert(DST_SUBSAMP_X == 1 || DST_SUBSAMP_X == 2,                   \
                  "DST_SUBSAMP_X unsupported");                               \
    static_assert(DST_SUBSAMP_Y == 1 || DST_SUBSAMP_Y == 2,                   \
                  "DST_SUBSAMP_Y unsupported");                               \
    const int kWidth = W1280;                                                 \
    const int kHeight = benchmark_height_;                                    \
    const int kSrcHalfWidth = SUBSAMPLE(kWidth, SRC_SUBSAMP_X);               \
    const int kSrcHalfHeight = SUBSAMPLE(kHeight, SRC_SUBSAMP_Y);             \
    const int kDstHalfWidth = SUBSAMPLE(kWidth, DST_SUBSAMP_X);               \
    const int kDstHalfHeight = SUBSAMPLE(kHeight, DST_SUBSAMP_Y);             \
    align_buffer_page_end(src_y, kWidth* kHeight* SRC_BPC + OFF);             \
    align_buffer_page_end(src_u,                                              \
                          kSrcHalfWidth* kSrcHalfHeight* SRC_BPC + OFF);      \
    align_buffer_page_end(src_v,                                              \
                          kSrcHalfWidth* kSrcHalfHeight* SRC_BPC + OFF);      \
    align_buffer_page_end(dst_y_c, kWidth* kHeight* DST_BPC);                 \
    align_buffer_page_end(dst_uv_c,                                           \
                          kDstHalfWidth* kDstHalfHeight* DST_BPC * 2);        \
    align_buffer_page_end(dst_y_opt, kWidth* kHeight* DST_BPC);               \
    align_buffer_page_end(dst_uv_opt,                                         \
                          kDstHalfWidth* kDstHalfHeight* DST_BPC * 2);        \
    MemRandomize(src_y + OFF, kWidth * kHeight * SRC_BPC);                    \
    MemRandomize(src_u + OFF, kSrcHalfWidth * kSrcHalfHeight * SRC_BPC);      \
    MemRandomize(src_v + OFF, kSrcHalfWidth * kSrcHalfHeight * SRC_BPC);      \
    SRC_T* src_y_p = reinterpret_cast<SRC_T*>(src_y + OFF);                   \
    SRC_T* src_u_p = reinterpret_cast<SRC_T*>(src_u + OFF);                   \
    SRC_T* src_v_p = reinterpret_cast<SRC_T*>(src_v + OFF);                   \
    for (int i = 0; i < kWidth * kHeight; ++i) {                              \
      src_y_p[i] = src_y_p[i] & ((1 << SRC_DEPTH) - 1);                       \
    }                                                                         \
    for (int i = 0; i < kSrcHalfWidth * kSrcHalfHeight; ++i) {                \
      src_u_p[i] = src_u_p[i] & ((1 << SRC_DEPTH) - 1);                       \
      src_v_p[i] = src_v_p[i] & ((1 << SRC_DEPTH) - 1);                       \
    }                                                                         \
    memset(dst_y_c, 1, kWidth* kHeight* DST_BPC);                             \
    memset(dst_uv_c, 2, kDstHalfWidth* kDstHalfHeight* DST_BPC * 2);          \
    memset(dst_y_opt, 101, kWidth* kHeight* DST_BPC);                         \
    memset(dst_uv_opt, 102, kDstHalfWidth* kDstHalfHeight* DST_BPC * 2);      \
    MaskCpuFlags(disable_cpu_flags_);                                         \
    SRC_FMT_PLANAR##To##FMT_PLANAR(src_y_p, kWidth, src_u_p, kSrcHalfWidth,   \
                                   src_v_p, kSrcHalfWidth,                    \
                                   reinterpret_cast<DST_T*>(dst_y_c), kWidth, \
                                   reinterpret_cast<DST_T*>(dst_uv_c),        \
                                   kDstHalfWidth * 2, kWidth, NEG kHeight);   \
    MaskCpuFlags(benchmark_cpu_info_);                                        \
    for (int i = 0; i < benchmark_iterations_; ++i) {                         \
      SRC_FMT_PLANAR##To##FMT_PLANAR(                                         \
          src_y_p, kWidth, src_u_p, kSrcHalfWidth, src_v_p, kSrcHalfWidth,    \
          reinterpret_cast<DST_T*>(dst_y_opt), kWidth,                        \
          reinterpret_cast<DST_T*>(dst_uv_opt), kDstHalfWidth * 2, kWidth,    \
          NEG kHeight);                                                       \
    }                                                                         \
    for (int i = 0; i < kHeight * kWidth * DST_BPC; ++i) {                    \
      EXPECT_EQ(dst_y_c[i], dst_y_opt[i]);                                    \
    }                                                                         \
    for (int i = 0; i < kDstHalfWidth * kDstHalfHeight * DST_BPC * 2; ++i) {  \
      EXPECT_EQ(dst_uv_c[i], dst_uv_opt[i]);                                  \
    }                                                                         \
    free_aligned_buffer_page_end(dst_y_c);                                    \
    free_aligned_buffer_page_end(dst_uv_c);                                   \
    free_aligned_buffer_page_end(dst_y_opt);                                  \
    free_aligned_buffer_page_end(dst_uv_opt);                                 \
    free_aligned_buffer_page_end(src_y);                                      \
    free_aligned_buffer_page_end(src_u);                                      \
    free_aligned_buffer_page_end(src_v);                                      \
  }

#if defined(ENABLE_FULL_TESTS)
#define TESTPLANARTOBP(SRC_FMT_PLANAR, SRC_T, SRC_BPC, SRC_SUBSAMP_X,         \
                       SRC_SUBSAMP_Y, FMT_PLANAR, DST_T, DST_BPC,             \
                       DST_SUBSAMP_X, DST_SUBSAMP_Y, SRC_DEPTH)               \
  TESTPLANARTOBPI(SRC_FMT_PLANAR, SRC_T, SRC_BPC, SRC_SUBSAMP_X,              \
                  SRC_SUBSAMP_Y, FMT_PLANAR, DST_T, DST_BPC, DST_SUBSAMP_X,   \
                  DST_SUBSAMP_Y, benchmark_width_ + 1, _Any, +, 0, SRC_DEPTH) \
  TESTPLANARTOBPI(SRC_FMT_PLANAR, SRC_T, SRC_BPC, SRC_SUBSAMP_X,              \
                  SRC_SUBSAMP_Y, FMT_PLANAR, DST_T, DST_BPC, DST_SUBSAMP_X,   \
                  DST_SUBSAMP_Y, benchmark_width_, _Unaligned, +, 2,          \
                  SRC_DEPTH)                                                  \
  TESTPLANARTOBPI(SRC_FMT_PLANAR, SRC_T, SRC_BPC, SRC_SUBSAMP_X,              \
                  SRC_SUBSAMP_Y, FMT_PLANAR, DST_T, DST_BPC, DST_SUBSAMP_X,   \
                  DST_SUBSAMP_Y, benchmark_width_, _Invert, -, 0, SRC_DEPTH)  \
  TESTPLANARTOBPI(SRC_FMT_PLANAR, SRC_T, SRC_BPC, SRC_SUBSAMP_X,              \
                  SRC_SUBSAMP_Y, FMT_PLANAR, DST_T, DST_BPC, DST_SUBSAMP_X,   \
                  DST_SUBSAMP_Y, benchmark_width_, _Opt, +, 0, SRC_DEPTH)
#else
#define TESTPLANARTOBP(SRC_FMT_PLANAR, SRC_T, SRC_BPC, SRC_SUBSAMP_X,       \
                       SRC_SUBSAMP_Y, FMT_PLANAR, DST_T, DST_BPC,           \
                       DST_SUBSAMP_X, DST_SUBSAMP_Y, SRC_DEPTH)             \
  TESTPLANARTOBPI(SRC_FMT_PLANAR, SRC_T, SRC_BPC, SRC_SUBSAMP_X,            \
                  SRC_SUBSAMP_Y, FMT_PLANAR, DST_T, DST_BPC, DST_SUBSAMP_X, \
                  DST_SUBSAMP_Y, benchmark_width_, _Opt, +, 0, SRC_DEPTH)
#endif

TESTPLANARTOBP(I420, uint8_t, 1, 2, 2, NV12, uint8_t, 1, 2, 2, 8)
TESTPLANARTOBP(I420, uint8_t, 1, 2, 2, NV21, uint8_t, 1, 2, 2, 8)
TESTPLANARTOBP(I422, uint8_t, 1, 2, 1, NV21, uint8_t, 1, 2, 2, 8)
TESTPLANARTOBP(I444, uint8_t, 1, 1, 1, NV12, uint8_t, 1, 2, 2, 8)
TESTPLANARTOBP(I444, uint8_t, 1, 1, 1, NV21, uint8_t, 1, 2, 2, 8)
TESTPLANARTOBP(I400, uint8_t, 1, 2, 2, NV21, uint8_t, 1, 2, 2, 8)
TESTPLANARTOBP(I010, uint16_t, 2, 2, 2, P010, uint16_t, 2, 2, 2, 10)
TESTPLANARTOBP(I210, uint16_t, 2, 2, 1, P210, uint16_t, 2, 2, 1, 10)
TESTPLANARTOBP(I012, uint16_t, 2, 2, 2, P012, uint16_t, 2, 2, 2, 12)
TESTPLANARTOBP(I212, uint16_t, 2, 2, 1, P212, uint16_t, 2, 2, 1, 12)

#define TESTBPTOBPI(SRC_FMT_PLANAR, SRC_T, SRC_BPC, SRC_SUBSAMP_X,            \
                    SRC_SUBSAMP_Y, FMT_PLANAR, DST_T, DST_BPC, DST_SUBSAMP_X, \
                    DST_SUBSAMP_Y, W1280, N, NEG, OFF, DOY, SRC_DEPTH,        \
                    TILE_WIDTH, TILE_HEIGHT)                                  \
  TEST_F(LibYUVConvertTest, SRC_FMT_PLANAR##To##FMT_PLANAR##N) {              \
    static_assert(DST_BPC == 1 || DST_BPC == 2, "DST BPC unsupported");       \
    static_assert(SRC_SUBSAMP_X == 1 || SRC_SUBSAMP_X == 2,                   \
                  "SRC_SUBSAMP_X unsupported");                               \
    static_assert(SRC_SUBSAMP_Y == 1 || SRC_SUBSAMP_Y == 2,                   \
                  "SRC_SUBSAMP_Y unsupported");                               \
    static_assert(DST_SUBSAMP_X == 1 || DST_SUBSAMP_X == 2,                   \
                  "DST_SUBSAMP_X unsupported");                               \
    static_assert(DST_SUBSAMP_Y == 1 || DST_SUBSAMP_Y == 2,                   \
                  "DST_SUBSAMP_Y unsupported");                               \
    const int kWidth = W1280;                                                 \
    const int kHeight = benchmark_height_;                                    \
    const int kSrcHalfWidth = SUBSAMPLE(kWidth, SRC_SUBSAMP_X);               \
    const int kDstHalfWidth = SUBSAMPLE(kWidth, DST_SUBSAMP_X);               \
    const int kDstHalfHeight = SUBSAMPLE(kHeight, DST_SUBSAMP_Y);             \
    const int kPaddedWidth = (kWidth + (TILE_WIDTH - 1)) & ~(TILE_WIDTH - 1); \
    const int kPaddedHeight =                                                 \
        (kHeight + (TILE_HEIGHT - 1)) & ~(TILE_HEIGHT - 1);                   \
    const int kSrcHalfPaddedWidth = SUBSAMPLE(kPaddedWidth, SRC_SUBSAMP_X);   \
    const int kSrcHalfPaddedHeight = SUBSAMPLE(kPaddedHeight, SRC_SUBSAMP_Y); \
    align_buffer_page_end(src_y, kPaddedWidth* kPaddedHeight* SRC_BPC + OFF); \
    align_buffer_page_end(                                                    \
        src_uv,                                                               \
        2 * kSrcHalfPaddedWidth * kSrcHalfPaddedHeight * SRC_BPC + OFF);      \
    align_buffer_page_end(dst_y_c, kWidth* kHeight* DST_BPC);                 \
    align_buffer_page_end(dst_uv_c,                                           \
                          2 * kDstHalfWidth * kDstHalfHeight * DST_BPC);      \
    align_buffer_page_end(dst_y_opt, kWidth* kHeight* DST_BPC);               \
    align_buffer_page_end(dst_uv_opt,                                         \
                          2 * kDstHalfWidth * kDstHalfHeight * DST_BPC);      \
    SRC_T* src_y_p = reinterpret_cast<SRC_T*>(src_y + OFF);                   \
    SRC_T* src_uv_p = reinterpret_cast<SRC_T*>(src_uv + OFF);                 \
    for (int i = 0;                                                           \
         i < kPaddedWidth * kPaddedHeight * SRC_BPC / (int)sizeof(SRC_T);     \
         ++i) {                                                               \
      src_y_p[i] =                                                            \
          (fastrand() & (((SRC_T)(-1)) << ((8 * SRC_BPC) - SRC_DEPTH)));      \
    }                                                                         \
    for (int i = 0; i < kSrcHalfPaddedWidth * kSrcHalfPaddedHeight * 2 *      \
                            SRC_BPC / (int)sizeof(SRC_T);                     \
         ++i) {                                                               \
      src_uv_p[i] =                                                           \
          (fastrand() & (((SRC_T)(-1)) << ((8 * SRC_BPC) - SRC_DEPTH)));      \
    }                                                                         \
    memset(dst_y_c, 1, kWidth* kHeight* DST_BPC);                             \
    memset(dst_uv_c, 2, 2 * kDstHalfWidth * kDstHalfHeight * DST_BPC);        \
    memset(dst_y_opt, 101, kWidth* kHeight* DST_BPC);                         \
    memset(dst_uv_opt, 102, 2 * kDstHalfWidth * kDstHalfHeight * DST_BPC);    \
    MaskCpuFlags(disable_cpu_flags_);                                         \
    SRC_FMT_PLANAR##To##FMT_PLANAR(                                           \
        src_y_p, kWidth* SRC_BPC / (int)sizeof(SRC_T), src_uv_p,              \
        2 * kSrcHalfWidth * SRC_BPC / (int)sizeof(SRC_T),                     \
        DOY ? reinterpret_cast<DST_T*>(dst_y_c) : NULL, kWidth,               \
        reinterpret_cast<DST_T*>(dst_uv_c), 2 * kDstHalfWidth, kWidth,        \
        NEG kHeight);                                                         \
    MaskCpuFlags(benchmark_cpu_info_);                                        \
    for (int i = 0; i < benchmark_iterations_; ++i) {                         \
      SRC_FMT_PLANAR##To##FMT_PLANAR(                                         \
          src_y_p, kWidth* SRC_BPC / (int)sizeof(SRC_T), src_uv_p,            \
          2 * kSrcHalfWidth * SRC_BPC / (int)sizeof(SRC_T),                   \
          DOY ? reinterpret_cast<DST_T*>(dst_y_opt) : NULL, kWidth,           \
          reinterpret_cast<DST_T*>(dst_uv_opt), 2 * kDstHalfWidth, kWidth,    \
          NEG kHeight);                                                       \
    }                                                                         \
    if (DOY) {                                                                \
      for (int i = 0; i < kHeight; ++i) {                                     \
        for (int j = 0; j < kWidth; ++j) {                                    \
          EXPECT_EQ(dst_y_c[i * kWidth + j], dst_y_opt[i * kWidth + j]);      \
        }                                                                     \
      }                                                                       \
    }                                                                         \
    for (int i = 0; i < kDstHalfHeight; ++i) {                                \
      for (int j = 0; j < 2 * kDstHalfWidth; ++j) {                           \
        EXPECT_EQ(dst_uv_c[i * 2 * kDstHalfWidth + j],                        \
                  dst_uv_opt[i * 2 * kDstHalfWidth + j]);                     \
      }                                                                       \
    }                                                                         \
    free_aligned_buffer_page_end(dst_y_c);                                    \
    free_aligned_buffer_page_end(dst_uv_c);                                   \
    free_aligned_buffer_page_end(dst_y_opt);                                  \
    free_aligned_buffer_page_end(dst_uv_opt);                                 \
    free_aligned_buffer_page_end(src_y);                                      \
    free_aligned_buffer_page_end(src_uv);                                     \
  }

#if defined(ENABLE_FULL_TESTS)
#define TESTBPTOBP(SRC_FMT_PLANAR, SRC_T, SRC_BPC, SRC_SUBSAMP_X,            \
                   SRC_SUBSAMP_Y, FMT_PLANAR, DST_T, DST_BPC, DST_SUBSAMP_X, \
                   DST_SUBSAMP_Y, SRC_DEPTH, TILE_WIDTH, TILE_HEIGHT)        \
  TESTBPTOBPI(SRC_FMT_PLANAR, SRC_T, SRC_BPC, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,  \
              FMT_PLANAR, DST_T, DST_BPC, DST_SUBSAMP_X, DST_SUBSAMP_Y,      \
              benchmark_width_ + 1, _Any, +, 0, 1, SRC_DEPTH, TILE_WIDTH,    \
              TILE_HEIGHT)                                                   \
  TESTBPTOBPI(SRC_FMT_PLANAR, SRC_T, SRC_BPC, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,  \
              FMT_PLANAR, DST_T, DST_BPC, DST_SUBSAMP_X, DST_SUBSAMP_Y,      \
              benchmark_width_, _Unaligned, +, 2, 1, SRC_DEPTH, TILE_WIDTH,  \
              TILE_HEIGHT)                                                   \
  TESTBPTOBPI(SRC_FMT_PLANAR, SRC_T, SRC_BPC, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,  \
              FMT_PLANAR, DST_T, DST_BPC, DST_SUBSAMP_X, DST_SUBSAMP_Y,      \
              benchmark_width_, _Invert, -, 0, 1, SRC_DEPTH, TILE_WIDTH,     \
              TILE_HEIGHT)                                                   \
  TESTBPTOBPI(SRC_FMT_PLANAR, SRC_T, SRC_BPC, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,  \
              FMT_PLANAR, DST_T, DST_BPC, DST_SUBSAMP_X, DST_SUBSAMP_Y,      \
              benchmark_width_, _Opt, +, 0, 1, SRC_DEPTH, TILE_WIDTH,        \
              TILE_HEIGHT)                                                   \
  TESTBPTOBPI(SRC_FMT_PLANAR, SRC_T, SRC_BPC, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,  \
              FMT_PLANAR, DST_T, DST_BPC, DST_SUBSAMP_X, DST_SUBSAMP_Y,      \
              benchmark_width_, _NullY, +, 0, 0, SRC_DEPTH, TILE_WIDTH,      \
              TILE_HEIGHT)
#else
#define TESTBPTOBP(SRC_FMT_PLANAR, SRC_T, SRC_BPC, SRC_SUBSAMP_X,            \
                   SRC_SUBSAMP_Y, FMT_PLANAR, DST_T, DST_BPC, DST_SUBSAMP_X, \
                   DST_SUBSAMP_Y, SRC_DEPTH, TILE_WIDTH, TILE_HEIGHT)        \
  TESTBPTOBPI(SRC_FMT_PLANAR, SRC_T, SRC_BPC, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,  \
              FMT_PLANAR, DST_T, DST_BPC, DST_SUBSAMP_X, DST_SUBSAMP_Y,      \
              benchmark_width_, _NullY, +, 0, 0, SRC_DEPTH, TILE_WIDTH,      \
              TILE_HEIGHT)
#endif

TESTBPTOBP(NV21, uint8_t, 1, 2, 2, NV12, uint8_t, 1, 2, 2, 8, 1, 1)
TESTBPTOBP(NV12, uint8_t, 1, 2, 2, NV12Mirror, uint8_t, 1, 2, 2, 8, 1, 1)
TESTBPTOBP(NV12, uint8_t, 1, 2, 2, NV24, uint8_t, 1, 1, 1, 8, 1, 1)
TESTBPTOBP(NV16, uint8_t, 1, 2, 1, NV24, uint8_t, 1, 1, 1, 8, 1, 1)
TESTBPTOBP(P010, uint16_t, 2, 2, 2, P410, uint16_t, 2, 1, 1, 10, 1, 1)
TESTBPTOBP(P210, uint16_t, 2, 2, 1, P410, uint16_t, 2, 1, 1, 10, 1, 1)
TESTBPTOBP(P012, uint16_t, 2, 2, 2, P412, uint16_t, 2, 1, 1, 10, 1, 1)
TESTBPTOBP(P212, uint16_t, 2, 2, 1, P412, uint16_t, 2, 1, 1, 12, 1, 1)
TESTBPTOBP(P016, uint16_t, 2, 2, 2, P416, uint16_t, 2, 1, 1, 12, 1, 1)
TESTBPTOBP(P216, uint16_t, 2, 2, 1, P416, uint16_t, 2, 1, 1, 12, 1, 1)
TESTBPTOBP(MM21, uint8_t, 1, 2, 2, NV12, uint8_t, 1, 2, 2, 8, 16, 32)
TESTBPTOBP(MT2T, uint8_t, 10 / 8, 2, 2, P010, uint16_t, 2, 2, 2, 10, 16, 32)

#define TESTATOPLANARI(FMT_A, BPP_A, YALIGN, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, \
                       W1280, N, NEG, OFF)                                     \
  TEST_F(LibYUVConvertTest, FMT_A##To##FMT_PLANAR##N) {                        \
    const int kWidth = W1280;                                                  \
    const int kHeight = ALIGNINT(benchmark_height_, YALIGN);                   \
    const int kStrideUV = SUBSAMPLE(kWidth, SUBSAMP_X);                        \
    const int kStride = (kStrideUV * SUBSAMP_X * 8 * BPP_A + 7) / 8;           \
    align_buffer_page_end(src_argb, kStride* kHeight + OFF);                   \
    align_buffer_page_end(dst_y_c, kWidth* kHeight);                           \
    align_buffer_page_end(dst_uv_c,                                            \
                          kStrideUV * 2 * SUBSAMPLE(kHeight, SUBSAMP_Y));      \
    align_buffer_page_end(dst_y_opt, kWidth* kHeight);                         \
    align_buffer_page_end(dst_uv_opt,                                          \
                          kStrideUV * 2 * SUBSAMPLE(kHeight, SUBSAMP_Y));      \
    memset(dst_y_c, 1, kWidth* kHeight);                                       \
    memset(dst_uv_c, 2, kStrideUV * 2 * SUBSAMPLE(kHeight, SUBSAMP_Y));        \
    memset(dst_y_opt, 101, kWidth* kHeight);                                   \
    memset(dst_uv_opt, 102, kStrideUV * 2 * SUBSAMPLE(kHeight, SUBSAMP_Y));    \
    for (int i = 0; i < kHeight; ++i)                                          \
      for (int j = 0; j < kStride; ++j)                                        \
        src_argb[(i * kStride) + j + OFF] = (fastrand() & 0xff);               \
    MaskCpuFlags(disable_cpu_flags_);                                          \
    FMT_A##To##FMT_PLANAR(src_argb + OFF, kStride, dst_y_c, kWidth, dst_uv_c,  \
                          kStrideUV * 2, dst_uv_c + kStrideUV, kStrideUV * 2,  \
                          kWidth, NEG kHeight);                                \
    MaskCpuFlags(benchmark_cpu_info_);                                         \
    for (int i = 0; i < benchmark_iterations_; ++i) {                          \
      FMT_A##To##FMT_PLANAR(src_argb + OFF, kStride, dst_y_opt, kWidth,        \
                            dst_uv_opt, kStrideUV * 2, dst_uv_opt + kStrideUV, \
                            kStrideUV * 2, kWidth, NEG kHeight);               \
    }                                                                          \
    for (int i = 0; i < kHeight; ++i) {                                        \
      for (int j = 0; j < kWidth; ++j) {                                       \
        EXPECT_EQ(dst_y_c[i * kWidth + j], dst_y_opt[i * kWidth + j]);         \
      }                                                                        \
    }                                                                          \
    for (int i = 0; i < SUBSAMPLE(kHeight, SUBSAMP_Y) * 2; ++i) {              \
      for (int j = 0; j < kStrideUV; ++j) {                                    \
        EXPECT_EQ(dst_uv_c[i * kStrideUV + j], dst_uv_opt[i * kStrideUV + j]); \
      }                                                                        \
    }                                                                          \
    free_aligned_buffer_page_end(dst_y_c);                                     \
    free_aligned_buffer_page_end(dst_uv_c);                                    \
    free_aligned_buffer_page_end(dst_y_opt);                                   \
    free_aligned_buffer_page_end(dst_uv_opt);                                  \
    free_aligned_buffer_page_end(src_argb);                                    \
  }

#if defined(ENABLE_FULL_TESTS)
#define TESTATOPLANAR(FMT_A, BPP_A, YALIGN, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y) \
  TESTATOPLANARI(FMT_A, BPP_A, YALIGN, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,      \
                 benchmark_width_ + 1, _Any, +, 0)                            \
  TESTATOPLANARI(FMT_A, BPP_A, YALIGN, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,      \
                 benchmark_width_, _Unaligned, +, 2)                          \
  TESTATOPLANARI(FMT_A, BPP_A, YALIGN, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,      \
                 benchmark_width_, _Invert, -, 0)                             \
  TESTATOPLANARI(FMT_A, BPP_A, YALIGN, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,      \
                 benchmark_width_, _Opt, +, 0)
#else
#define TESTATOPLANAR(FMT_A, BPP_A, YALIGN, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y) \
  TESTATOPLANARI(FMT_A, BPP_A, YALIGN, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,      \
                 benchmark_width_, _Opt, +, 0)
#endif

TESTATOPLANAR(ABGR, 4, 1, I420, 2, 2)
TESTATOPLANAR(ARGB, 4, 1, I420, 2, 2)
TESTATOPLANAR(ARGB, 4, 1, I422, 2, 1)
TESTATOPLANAR(ARGB, 4, 1, I444, 1, 1)
TESTATOPLANAR(ARGB, 4, 1, J420, 2, 2)
TESTATOPLANAR(ARGB, 4, 1, J422, 2, 1)
TESTATOPLANAR(ABGR, 4, 1, J420, 2, 2)
TESTATOPLANAR(ABGR, 4, 1, J422, 2, 1)
#ifdef LITTLE_ENDIAN_ONLY_TEST
TESTATOPLANAR(ARGB4444, 2, 1, I420, 2, 2)
TESTATOPLANAR(RGB565, 2, 1, I420, 2, 2)
TESTATOPLANAR(ARGB1555, 2, 1, I420, 2, 2)
#endif
TESTATOPLANAR(BGRA, 4, 1, I420, 2, 2)
TESTATOPLANAR(I400, 1, 1, I420, 2, 2)
TESTATOPLANAR(J400, 1, 1, J420, 2, 2)
TESTATOPLANAR(RAW, 3, 1, I420, 2, 2)
TESTATOPLANAR(RAW, 3, 1, J420, 2, 2)
TESTATOPLANAR(RGB24, 3, 1, I420, 2, 2)
TESTATOPLANAR(RGB24, 3, 1, J420, 2, 2)
TESTATOPLANAR(RGBA, 4, 1, I420, 2, 2)
TESTATOPLANAR(UYVY, 2, 1, I420, 2, 2)
TESTATOPLANAR(UYVY, 2, 1, I422, 2, 1)
TESTATOPLANAR(YUY2, 2, 1, I420, 2, 2)
TESTATOPLANAR(YUY2, 2, 1, I422, 2, 1)

#define TESTATOPLANARAI(FMT_A, BPP_A, YALIGN, FMT_PLANAR, SUBSAMP_X,           \
                        SUBSAMP_Y, W1280, N, NEG, OFF)                         \
  TEST_F(LibYUVConvertTest, FMT_A##To##FMT_PLANAR##N) {                        \
    const int kWidth = W1280;                                                  \
    const int kHeight = ALIGNINT(benchmark_height_, YALIGN);                   \
    const int kStrideUV = SUBSAMPLE(kWidth, SUBSAMP_X);                        \
    const int kStride = (kStrideUV * SUBSAMP_X * 8 * BPP_A + 7) / 8;           \
    align_buffer_page_end(src_argb, kStride* kHeight + OFF);                   \
    align_buffer_page_end(dst_a_c, kWidth* kHeight);                           \
    align_buffer_page_end(dst_y_c, kWidth* kHeight);                           \
    align_buffer_page_end(dst_uv_c,                                            \
                          kStrideUV * 2 * SUBSAMPLE(kHeight, SUBSAMP_Y));      \
    align_buffer_page_end(dst_a_opt, kWidth* kHeight);                         \
    align_buffer_page_end(dst_y_opt, kWidth* kHeight);                         \
    align_buffer_page_end(dst_uv_opt,                                          \
                          kStrideUV * 2 * SUBSAMPLE(kHeight, SUBSAMP_Y));      \
    memset(dst_a_c, 1, kWidth* kHeight);                                       \
    memset(dst_y_c, 2, kWidth* kHeight);                                       \
    memset(dst_uv_c, 3, kStrideUV * 2 * SUBSAMPLE(kHeight, SUBSAMP_Y));        \
    memset(dst_a_opt, 101, kWidth* kHeight);                                   \
    memset(dst_y_opt, 102, kWidth* kHeight);                                   \
    memset(dst_uv_opt, 103, kStrideUV * 2 * SUBSAMPLE(kHeight, SUBSAMP_Y));    \
    for (int i = 0; i < kHeight; ++i)                                          \
      for (int j = 0; j < kStride; ++j)                                        \
        src_argb[(i * kStride) + j + OFF] = (fastrand() & 0xff);               \
    MaskCpuFlags(disable_cpu_flags_);                                          \
    FMT_A##To##FMT_PLANAR(src_argb + OFF, kStride, dst_y_c, kWidth, dst_uv_c,  \
                          kStrideUV * 2, dst_uv_c + kStrideUV, kStrideUV * 2,  \
                          dst_a_c, kWidth, kWidth, NEG kHeight);               \
    MaskCpuFlags(benchmark_cpu_info_);                                         \
    for (int i = 0; i < benchmark_iterations_; ++i) {                          \
      FMT_A##To##FMT_PLANAR(src_argb + OFF, kStride, dst_y_opt, kWidth,        \
                            dst_uv_opt, kStrideUV * 2, dst_uv_opt + kStrideUV, \
                            kStrideUV * 2, dst_a_opt, kWidth, kWidth,          \
                            NEG kHeight);                                      \
    }                                                                          \
    for (int i = 0; i < kHeight; ++i) {                                        \
      for (int j = 0; j < kWidth; ++j) {                                       \
        EXPECT_EQ(dst_y_c[i * kWidth + j], dst_y_opt[i * kWidth + j]);         \
        EXPECT_EQ(dst_a_c[i * kWidth + j], dst_a_opt[i * kWidth + j]);         \
      }                                                                        \
    }                                                                          \
    for (int i = 0; i < SUBSAMPLE(kHeight, SUBSAMP_Y) * 2; ++i) {              \
      for (int j = 0; j < kStrideUV; ++j) {                                    \
        EXPECT_EQ(dst_uv_c[i * kStrideUV + j], dst_uv_opt[i * kStrideUV + j]); \
      }                                                                        \
    }                                                                          \
    free_aligned_buffer_page_end(dst_a_c);                                     \
    free_aligned_buffer_page_end(dst_y_c);                                     \
    free_aligned_buffer_page_end(dst_uv_c);                                    \
    free_aligned_buffer_page_end(dst_a_opt);                                   \
    free_aligned_buffer_page_end(dst_y_opt);                                   \
    free_aligned_buffer_page_end(dst_uv_opt);                                  \
    free_aligned_buffer_page_end(src_argb);                                    \
  }

#if defined(ENABLE_FULL_TESTS)
#define TESTATOPLANARA(FMT_A, BPP_A, YALIGN, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y) \
  TESTATOPLANARAI(FMT_A, BPP_A, YALIGN, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,      \
                  benchmark_width_ + 1, _Any, +, 0)                            \
  TESTATOPLANARAI(FMT_A, BPP_A, YALIGN, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,      \
                  benchmark_width_, _Unaligned, +, 2)                          \
  TESTATOPLANARAI(FMT_A, BPP_A, YALIGN, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,      \
                  benchmark_width_, _Invert, -, 0)                             \
  TESTATOPLANARAI(FMT_A, BPP_A, YALIGN, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,      \
                  benchmark_width_, _Opt, +, 0)
#else
#define TESTATOPLANARA(FMT_A, BPP_A, YALIGN, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y) \
  TESTATOPLANARAI(FMT_A, BPP_A, YALIGN, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,      \
                  benchmark_width_, _Opt, +, 0)
#endif

TESTATOPLANARA(ARGB, 4, 1, I420Alpha, 2, 2)

#define TESTATOBPI(FMT_A, SUB_A, BPP_A, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,     \
                   W1280, N, NEG, OFF)                                        \
  TEST_F(LibYUVConvertTest, FMT_A##To##FMT_PLANAR##N) {                       \
    const int kWidth = W1280;                                                 \
    const int kHeight = benchmark_height_;                                    \
    const int kStride = SUBSAMPLE(kWidth, SUB_A) * BPP_A;                     \
    const int kStrideUV = SUBSAMPLE(kWidth, SUBSAMP_X);                       \
    align_buffer_page_end(src_argb, kStride* kHeight + OFF);                  \
    align_buffer_page_end(dst_y_c, kWidth* kHeight);                          \
    align_buffer_page_end(dst_uv_c,                                           \
                          kStrideUV * 2 * SUBSAMPLE(kHeight, SUBSAMP_Y));     \
    align_buffer_page_end(dst_y_opt, kWidth* kHeight);                        \
    align_buffer_page_end(dst_uv_opt,                                         \
                          kStrideUV * 2 * SUBSAMPLE(kHeight, SUBSAMP_Y));     \
    for (int i = 0; i < kHeight; ++i)                                         \
      for (int j = 0; j < kStride; ++j)                                       \
        src_argb[(i * kStride) + j + OFF] = (fastrand() & 0xff);              \
    memset(dst_y_c, 1, kWidth* kHeight);                                      \
    memset(dst_uv_c, 2, kStrideUV * 2 * SUBSAMPLE(kHeight, SUBSAMP_Y));       \
    memset(dst_y_opt, 101, kWidth* kHeight);                                  \
    memset(dst_uv_opt, 102, kStrideUV * 2 * SUBSAMPLE(kHeight, SUBSAMP_Y));   \
    MaskCpuFlags(disable_cpu_flags_);                                         \
    FMT_A##To##FMT_PLANAR(src_argb + OFF, kStride, dst_y_c, kWidth, dst_uv_c, \
                          kStrideUV * 2, kWidth, NEG kHeight);                \
    MaskCpuFlags(benchmark_cpu_info_);                                        \
    for (int i = 0; i < benchmark_iterations_; ++i) {                         \
      FMT_A##To##FMT_PLANAR(src_argb + OFF, kStride, dst_y_opt, kWidth,       \
                            dst_uv_opt, kStrideUV * 2, kWidth, NEG kHeight);  \
    }                                                                         \
    for (int i = 0; i < kHeight; ++i) {                                       \
      for (int j = 0; j < kWidth; ++j) {                                      \
        EXPECT_EQ(dst_y_c[i * kWidth + j], dst_y_opt[i * kWidth + j]);        \
      }                                                                       \
    }                                                                         \
    for (int i = 0; i < SUBSAMPLE(kHeight, SUBSAMP_Y); ++i) {                 \
      for (int j = 0; j < kStrideUV * 2; ++j) {                               \
        EXPECT_EQ(dst_uv_c[i * kStrideUV * 2 + j],                            \
                  dst_uv_opt[i * kStrideUV * 2 + j]);                         \
      }                                                                       \
    }                                                                         \
    free_aligned_buffer_page_end(dst_y_c);                                    \
    free_aligned_buffer_page_end(dst_uv_c);                                   \
    free_aligned_buffer_page_end(dst_y_opt);                                  \
    free_aligned_buffer_page_end(dst_uv_opt);                                 \
    free_aligned_buffer_page_end(src_argb);                                   \
  }

#if defined(ENABLE_FULL_TESTS)
#define TESTATOBP(FMT_A, SUB_A, BPP_A, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y) \
  TESTATOBPI(FMT_A, SUB_A, BPP_A, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,      \
             benchmark_width_ + 1, _Any, +, 0)                           \
  TESTATOBPI(FMT_A, SUB_A, BPP_A, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,      \
             benchmark_width_, _Unaligned, +, 2)                         \
  TESTATOBPI(FMT_A, SUB_A, BPP_A, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,      \
             benchmark_width_, _Invert, -, 0)                            \
  TESTATOBPI(FMT_A, SUB_A, BPP_A, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,      \
             benchmark_width_, _Opt, +, 0)
#else
#define TESTATOBP(FMT_A, SUB_A, BPP_A, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y) \
  TESTATOBPI(FMT_A, SUB_A, BPP_A, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,      \
             benchmark_width_, _Opt, +, 0)
#endif

TESTATOBP(ARGB, 1, 4, NV12, 2, 2)
TESTATOBP(ARGB, 1, 4, NV21, 2, 2)
TESTATOBP(ABGR, 1, 4, NV12, 2, 2)
TESTATOBP(ABGR, 1, 4, NV21, 2, 2)
TESTATOBP(RAW, 1, 3, JNV21, 2, 2)
TESTATOBP(YUY2, 2, 4, NV12, 2, 2)
TESTATOBP(UYVY, 2, 4, NV12, 2, 2)
TESTATOBP(AYUV, 1, 4, NV12, 2, 2)
TESTATOBP(AYUV, 1, 4, NV21, 2, 2)

#if !defined(LEAN_TESTS)

#ifdef HAVE_JPEG
TEST_F(LibYUVConvertTest, ValidateJpeg) {
  const int kOff = 10;
  const int kMinJpeg = 64;
  const int kImageSize = benchmark_width_ * benchmark_height_ >= kMinJpeg
                             ? benchmark_width_ * benchmark_height_
                             : kMinJpeg;
  const int kSize = kImageSize + kOff;
  align_buffer_page_end(orig_pixels, kSize);

  // No SOI or EOI. Expect fail.
  memset(orig_pixels, 0, kSize);
  EXPECT_FALSE(ValidateJpeg(orig_pixels, kSize));

  // Test special value that matches marker start.
  memset(orig_pixels, 0xff, kSize);
  EXPECT_FALSE(ValidateJpeg(orig_pixels, kSize));

  // EOI, SOI. Expect pass.
  orig_pixels[0] = 0xff;
  orig_pixels[1] = 0xd8;  // SOI.
  orig_pixels[2] = 0xff;
  orig_pixels[kSize - kOff + 0] = 0xff;
  orig_pixels[kSize - kOff + 1] = 0xd9;  // EOI.
  for (int times = 0; times < benchmark_iterations_; ++times) {
    EXPECT_TRUE(ValidateJpeg(orig_pixels, kSize));
  }
  free_aligned_buffer_page_end(orig_pixels);
}

TEST_F(LibYUVConvertTest, ValidateJpegLarge) {
  const int kOff = 10;
  const int kMinJpeg = 64;
  const int kImageSize = benchmark_width_ * benchmark_height_ >= kMinJpeg
                             ? benchmark_width_ * benchmark_height_
                             : kMinJpeg;
  const int kSize = kImageSize + kOff;
  const int kMultiple = 10;
  const int kBufSize = kImageSize * kMultiple + kOff;
  align_buffer_page_end(orig_pixels, kBufSize);

  // No SOI or EOI. Expect fail.
  memset(orig_pixels, 0, kBufSize);
  EXPECT_FALSE(ValidateJpeg(orig_pixels, kBufSize));

  // EOI, SOI. Expect pass.
  orig_pixels[0] = 0xff;
  orig_pixels[1] = 0xd8;  // SOI.
  orig_pixels[2] = 0xff;
  orig_pixels[kSize - kOff + 0] = 0xff;
  orig_pixels[kSize - kOff + 1] = 0xd9;  // EOI.
  for (int times = 0; times < benchmark_iterations_; ++times) {
    EXPECT_TRUE(ValidateJpeg(orig_pixels, kBufSize));
  }
  free_aligned_buffer_page_end(orig_pixels);
}

TEST_F(LibYUVConvertTest, InvalidateJpeg) {
  const int kOff = 10;
  const int kMinJpeg = 64;
  const int kImageSize = benchmark_width_ * benchmark_height_ >= kMinJpeg
                             ? benchmark_width_ * benchmark_height_
                             : kMinJpeg;
  const int kSize = kImageSize + kOff;
  align_buffer_page_end(orig_pixels, kSize);

  // NULL pointer. Expect fail.
  EXPECT_FALSE(ValidateJpeg(NULL, kSize));

  // Negative size. Expect fail.
  EXPECT_FALSE(ValidateJpeg(orig_pixels, -1));

  // Too large size. Expect fail.
  EXPECT_FALSE(ValidateJpeg(orig_pixels, 0xfb000000ull));

  // No SOI or EOI. Expect fail.
  memset(orig_pixels, 0, kSize);
  EXPECT_FALSE(ValidateJpeg(orig_pixels, kSize));

  // SOI but no EOI. Expect fail.
  orig_pixels[0] = 0xff;
  orig_pixels[1] = 0xd8;  // SOI.
  orig_pixels[2] = 0xff;
  for (int times = 0; times < benchmark_iterations_; ++times) {
    EXPECT_FALSE(ValidateJpeg(orig_pixels, kSize));
  }

  // EOI but no SOI. Expect fail.
  orig_pixels[0] = 0;
  orig_pixels[1] = 0;
  orig_pixels[kSize - kOff + 0] = 0xff;
  orig_pixels[kSize - kOff + 1] = 0xd9;  // EOI.
  EXPECT_FALSE(ValidateJpeg(orig_pixels, kSize));

  free_aligned_buffer_page_end(orig_pixels);
}

TEST_F(LibYUVConvertTest, FuzzJpeg) {
  // SOI but no EOI. Expect fail.
  for (int times = 0; times < benchmark_iterations_; ++times) {
    const int kSize = fastrand() % 5000 + 3;
    align_buffer_page_end(orig_pixels, kSize);
    MemRandomize(orig_pixels, kSize);

    // Add SOI so frame will be scanned.
    orig_pixels[0] = 0xff;
    orig_pixels[1] = 0xd8;  // SOI.
    orig_pixels[2] = 0xff;
    orig_pixels[kSize - 1] = 0xff;
    ValidateJpeg(orig_pixels,
                 kSize);  // Failure normally expected.
    free_aligned_buffer_page_end(orig_pixels);
  }
}

// Test data created in GIMP.  In export jpeg, disable
// thumbnails etc, choose a subsampling, and use low quality
// (50) to keep size small. Generated with xxd -i test.jpg
// test 0 is J400
static const uint8_t kTest0Jpg[] = {
    0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01,
    0x01, 0x01, 0x00, 0x48, 0x00, 0x48, 0x00, 0x00, 0xff, 0xdb, 0x00, 0x43,
    0x00, 0x10, 0x0b, 0x0c, 0x0e, 0x0c, 0x0a, 0x10, 0x0e, 0x0d, 0x0e, 0x12,
    0x11, 0x10, 0x13, 0x18, 0x28, 0x1a, 0x18, 0x16, 0x16, 0x18, 0x31, 0x23,
    0x25, 0x1d, 0x28, 0x3a, 0x33, 0x3d, 0x3c, 0x39, 0x33, 0x38, 0x37, 0x40,
    0x48, 0x5c, 0x4e, 0x40, 0x44, 0x57, 0x45, 0x37, 0x38, 0x50, 0x6d, 0x51,
    0x57, 0x5f, 0x62, 0x67, 0x68, 0x67, 0x3e, 0x4d, 0x71, 0x79, 0x70, 0x64,
    0x78, 0x5c, 0x65, 0x67, 0x63, 0xff, 0xc2, 0x00, 0x0b, 0x08, 0x00, 0x10,
    0x00, 0x20, 0x01, 0x01, 0x11, 0x00, 0xff, 0xc4, 0x00, 0x17, 0x00, 0x01,
    0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x03, 0x04, 0x01, 0x02, 0xff, 0xda, 0x00, 0x08, 0x01,
    0x01, 0x00, 0x00, 0x00, 0x01, 0x43, 0x7e, 0xa7, 0x97, 0x57, 0xff, 0xc4,
    0x00, 0x1b, 0x10, 0x00, 0x03, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x11, 0x00, 0x03,
    0x10, 0x12, 0x13, 0xff, 0xda, 0x00, 0x08, 0x01, 0x01, 0x00, 0x01, 0x05,
    0x02, 0x3b, 0xc0, 0x6f, 0x66, 0x76, 0x56, 0x23, 0x87, 0x99, 0x0d, 0x26,
    0x62, 0xf6, 0xbf, 0xff, 0xc4, 0x00, 0x1e, 0x10, 0x00, 0x02, 0x01, 0x03,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x11, 0x21, 0x02, 0x12, 0x32, 0x10, 0x31, 0x71, 0x81, 0xa1, 0xff,
    0xda, 0x00, 0x08, 0x01, 0x01, 0x00, 0x06, 0x3f, 0x02, 0x4b, 0xb3, 0x28,
    0x32, 0xd2, 0xed, 0xf9, 0x1d, 0x3e, 0x13, 0x51, 0x73, 0x83, 0xff, 0xc4,
    0x00, 0x1c, 0x10, 0x01, 0x01, 0x01, 0x00, 0x02, 0x03, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x11, 0x00, 0x21, 0x51,
    0x31, 0x61, 0x81, 0xf0, 0xff, 0xda, 0x00, 0x08, 0x01, 0x01, 0x00, 0x01,
    0x3f, 0x21, 0x65, 0x6e, 0x31, 0x86, 0x28, 0xf9, 0x30, 0xdc, 0x27, 0xdb,
    0xa9, 0x01, 0xf3, 0xde, 0x02, 0xa0, 0xed, 0x1e, 0x34, 0x68, 0x23, 0xf9,
    0xc6, 0x48, 0x5d, 0x7a, 0x35, 0x02, 0xf5, 0x6f, 0xff, 0xda, 0x00, 0x08,
    0x01, 0x01, 0x00, 0x00, 0x00, 0x10, 0x35, 0xff, 0xc4, 0x00, 0x1f, 0x10,
    0x01, 0x00, 0x02, 0x01, 0x04, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x11, 0x31, 0x41, 0x61, 0x71, 0x91,
    0x21, 0x81, 0xd1, 0xb1, 0xff, 0xda, 0x00, 0x08, 0x01, 0x01, 0x00, 0x01,
    0x3f, 0x10, 0x0b, 0x30, 0xe9, 0x58, 0xbe, 0x1a, 0xfd, 0x88, 0xab, 0x8b,
    0x34, 0x74, 0x80, 0x4b, 0xb5, 0xd5, 0xab, 0xcd, 0x46, 0x96, 0x2e, 0xec,
    0xbd, 0xaa, 0x78, 0x47, 0x5c, 0x47, 0xa7, 0x30, 0x49, 0xad, 0x88, 0x7c,
    0x40, 0x74, 0x30, 0xff, 0x00, 0x23, 0x1d, 0x03, 0x0b, 0xb7, 0xd4, 0xff,
    0xd9};
static const size_t kTest0JpgLen = 421;

// test 1 is J444
static const uint8_t kTest1Jpg[] = {
    0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01,
    0x01, 0x01, 0x00, 0x48, 0x00, 0x48, 0x00, 0x00, 0xff, 0xdb, 0x00, 0x43,
    0x00, 0x10, 0x0b, 0x0c, 0x0e, 0x0c, 0x0a, 0x10, 0x0e, 0x0d, 0x0e, 0x12,
    0x11, 0x10, 0x13, 0x18, 0x28, 0x1a, 0x18, 0x16, 0x16, 0x18, 0x31, 0x23,
    0x25, 0x1d, 0x28, 0x3a, 0x33, 0x3d, 0x3c, 0x39, 0x33, 0x38, 0x37, 0x40,
    0x48, 0x5c, 0x4e, 0x40, 0x44, 0x57, 0x45, 0x37, 0x38, 0x50, 0x6d, 0x51,
    0x57, 0x5f, 0x62, 0x67, 0x68, 0x67, 0x3e, 0x4d, 0x71, 0x79, 0x70, 0x64,
    0x78, 0x5c, 0x65, 0x67, 0x63, 0xff, 0xdb, 0x00, 0x43, 0x01, 0x11, 0x12,
    0x12, 0x18, 0x15, 0x18, 0x2f, 0x1a, 0x1a, 0x2f, 0x63, 0x42, 0x38, 0x42,
    0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
    0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
    0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
    0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
    0x63, 0x63, 0xff, 0xc2, 0x00, 0x11, 0x08, 0x00, 0x10, 0x00, 0x20, 0x03,
    0x01, 0x11, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01, 0xff, 0xc4, 0x00,
    0x17, 0x00, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x04, 0x01, 0x02, 0xff, 0xc4,
    0x00, 0x16, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x01, 0x03, 0xff, 0xda,
    0x00, 0x0c, 0x03, 0x01, 0x00, 0x02, 0x10, 0x03, 0x10, 0x00, 0x00, 0x01,
    0x40, 0x8f, 0x26, 0xe8, 0xf4, 0xcc, 0xf9, 0x69, 0x2b, 0x1b, 0x2a, 0xcb,
    0xff, 0xc4, 0x00, 0x1b, 0x10, 0x00, 0x03, 0x00, 0x02, 0x03, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x11,
    0x00, 0x03, 0x10, 0x12, 0x13, 0xff, 0xda, 0x00, 0x08, 0x01, 0x01, 0x00,
    0x01, 0x05, 0x02, 0x3b, 0x80, 0x6f, 0x56, 0x76, 0x56, 0x23, 0x87, 0x99,
    0x0d, 0x26, 0x62, 0xf6, 0xbf, 0xff, 0xc4, 0x00, 0x19, 0x11, 0x01, 0x00,
    0x03, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x10, 0x11, 0x02, 0x12, 0xff, 0xda, 0x00, 0x08,
    0x01, 0x03, 0x01, 0x01, 0x3f, 0x01, 0xf1, 0x00, 0x27, 0x45, 0xbb, 0x31,
    0xaf, 0xff, 0xc4, 0x00, 0x1a, 0x11, 0x00, 0x02, 0x03, 0x01, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x02, 0x10, 0x11, 0x41, 0x12, 0xff, 0xda, 0x00, 0x08, 0x01, 0x02, 0x01,
    0x01, 0x3f, 0x01, 0xf6, 0x4b, 0x5f, 0x48, 0xb3, 0x69, 0x63, 0x35, 0x72,
    0xbf, 0xff, 0xc4, 0x00, 0x1e, 0x10, 0x00, 0x02, 0x01, 0x03, 0x05, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11,
    0x21, 0x02, 0x12, 0x32, 0x10, 0x31, 0x71, 0x81, 0xa1, 0xff, 0xda, 0x00,
    0x08, 0x01, 0x01, 0x00, 0x06, 0x3f, 0x02, 0x4b, 0xb3, 0x28, 0x32, 0xd2,
    0xed, 0xf9, 0x1d, 0x3e, 0x13, 0x51, 0x73, 0x83, 0xff, 0xc4, 0x00, 0x1c,
    0x10, 0x01, 0x01, 0x01, 0x00, 0x02, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x11, 0x00, 0x21, 0x51, 0x31, 0x61,
    0x81, 0xf0, 0xff, 0xda, 0x00, 0x08, 0x01, 0x01, 0x00, 0x01, 0x3f, 0x21,
    0x75, 0x6e, 0x31, 0x94, 0x28, 0xf9, 0x30, 0xdc, 0x27, 0xdb, 0xa9, 0x01,
    0xf3, 0xde, 0x02, 0xa0, 0xed, 0x1e, 0x34, 0x68, 0x23, 0xf9, 0xc6, 0x48,
    0x5d, 0x7a, 0x35, 0x02, 0xf5, 0x6f, 0xff, 0xda, 0x00, 0x0c, 0x03, 0x01,
    0x00, 0x02, 0x00, 0x03, 0x00, 0x00, 0x00, 0x10, 0x26, 0x61, 0xd4, 0xff,
    0xc4, 0x00, 0x1a, 0x11, 0x00, 0x03, 0x01, 0x00, 0x03, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x11, 0x21,
    0x31, 0x41, 0x51, 0xff, 0xda, 0x00, 0x08, 0x01, 0x03, 0x01, 0x01, 0x3f,
    0x10, 0x54, 0xa8, 0xbf, 0x50, 0x87, 0xb0, 0x9d, 0x8b, 0xc4, 0x6a, 0x26,
    0x6b, 0x2a, 0x9c, 0x1f, 0xff, 0xc4, 0x00, 0x18, 0x11, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x11, 0x21, 0x51, 0xff, 0xda, 0x00, 0x08, 0x01, 0x02,
    0x01, 0x01, 0x3f, 0x10, 0x70, 0xe1, 0x3e, 0xd1, 0x8e, 0x0d, 0xe1, 0xb5,
    0xd5, 0x91, 0x76, 0x43, 0x82, 0x45, 0x4c, 0x7b, 0x7f, 0xff, 0xc4, 0x00,
    0x1f, 0x10, 0x01, 0x00, 0x02, 0x01, 0x04, 0x03, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x11, 0x31, 0x41, 0x61,
    0x71, 0x91, 0x21, 0x81, 0xd1, 0xb1, 0xff, 0xda, 0x00, 0x08, 0x01, 0x01,
    0x00, 0x01, 0x3f, 0x10, 0x1b, 0x30, 0xe9, 0x58, 0xbe, 0x1a, 0xfd, 0x8a,
    0xeb, 0x8b, 0x34, 0x74, 0x80, 0x4b, 0xb5, 0xd5, 0xab, 0xcd, 0x46, 0x96,
    0x2e, 0xec, 0xbd, 0xaa, 0x78, 0x47, 0x5c, 0x47, 0xa7, 0x30, 0x49, 0xad,
    0x88, 0x7c, 0x40, 0x74, 0x30, 0xff, 0x00, 0x23, 0x1d, 0x03, 0x0b, 0xb7,
    0xd4, 0xff, 0xd9};
static const size_t kTest1JpgLen = 735;

// test 2 is J420
static const uint8_t kTest2Jpg[] = {
    0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01,
    0x01, 0x01, 0x00, 0x48, 0x00, 0x48, 0x00, 0x00, 0xff, 0xdb, 0x00, 0x43,
    0x00, 0x10, 0x0b, 0x0c, 0x0e, 0x0c, 0x0a, 0x10, 0x0e, 0x0d, 0x0e, 0x12,
    0x11, 0x10, 0x13, 0x18, 0x28, 0x1a, 0x18, 0x16, 0x16, 0x18, 0x31, 0x23,
    0x25, 0x1d, 0x28, 0x3a, 0x33, 0x3d, 0x3c, 0x39, 0x33, 0x38, 0x37, 0x40,
    0x48, 0x5c, 0x4e, 0x40, 0x44, 0x57, 0x45, 0x37, 0x38, 0x50, 0x6d, 0x51,
    0x57, 0x5f, 0x62, 0x67, 0x68, 0x67, 0x3e, 0x4d, 0x71, 0x79, 0x70, 0x64,
    0x78, 0x5c, 0x65, 0x67, 0x63, 0xff, 0xdb, 0x00, 0x43, 0x01, 0x11, 0x12,
    0x12, 0x18, 0x15, 0x18, 0x2f, 0x1a, 0x1a, 0x2f, 0x63, 0x42, 0x38, 0x42,
    0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
    0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
    0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
    0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
    0x63, 0x63, 0xff, 0xc2, 0x00, 0x11, 0x08, 0x00, 0x10, 0x00, 0x20, 0x03,
    0x01, 0x22, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01, 0xff, 0xc4, 0x00,
    0x18, 0x00, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x05, 0x01, 0x02, 0x04, 0xff,
    0xc4, 0x00, 0x16, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x01, 0x02, 0xff,
    0xda, 0x00, 0x0c, 0x03, 0x01, 0x00, 0x02, 0x10, 0x03, 0x10, 0x00, 0x00,
    0x01, 0x20, 0xe7, 0x28, 0xa3, 0x0b, 0x2e, 0x2d, 0xcf, 0xff, 0xc4, 0x00,
    0x1b, 0x10, 0x00, 0x03, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x11, 0x00, 0x03, 0x10,
    0x12, 0x13, 0xff, 0xda, 0x00, 0x08, 0x01, 0x01, 0x00, 0x01, 0x05, 0x02,
    0x3b, 0x80, 0x6f, 0x56, 0x76, 0x56, 0x23, 0x87, 0x99, 0x0d, 0x26, 0x62,
    0xf6, 0xbf, 0xff, 0xc4, 0x00, 0x17, 0x11, 0x01, 0x00, 0x03, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x11, 0x21, 0xff, 0xda, 0x00, 0x08, 0x01, 0x03, 0x01, 0x01, 0x3f,
    0x01, 0xc8, 0x53, 0xff, 0xc4, 0x00, 0x16, 0x11, 0x01, 0x01, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x11, 0x32, 0xff, 0xda, 0x00, 0x08, 0x01, 0x02, 0x01, 0x01, 0x3f,
    0x01, 0xd2, 0xc7, 0xff, 0xc4, 0x00, 0x1e, 0x10, 0x00, 0x02, 0x01, 0x03,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x11, 0x21, 0x02, 0x12, 0x32, 0x10, 0x31, 0x71, 0x81, 0xa1, 0xff,
    0xda, 0x00, 0x08, 0x01, 0x01, 0x00, 0x06, 0x3f, 0x02, 0x4b, 0xb3, 0x28,
    0x32, 0xd2, 0xed, 0xf9, 0x1d, 0x3e, 0x13, 0x51, 0x73, 0x83, 0xff, 0xc4,
    0x00, 0x1c, 0x10, 0x01, 0x01, 0x01, 0x00, 0x02, 0x03, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x11, 0x00, 0x21, 0x51,
    0x31, 0x61, 0x81, 0xf0, 0xff, 0xda, 0x00, 0x08, 0x01, 0x01, 0x00, 0x01,
    0x3f, 0x21, 0x75, 0x6e, 0x31, 0x94, 0x28, 0xf9, 0x30, 0xdc, 0x27, 0xdb,
    0xa9, 0x01, 0xf3, 0xde, 0x02, 0xa0, 0xed, 0x1e, 0x34, 0x68, 0x23, 0xf9,
    0xc6, 0x48, 0x5d, 0x7a, 0x35, 0x02, 0xf5, 0x6f, 0xff, 0xda, 0x00, 0x0c,
    0x03, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x00, 0x00, 0x10, 0x13, 0x5f,
    0xff, 0xc4, 0x00, 0x17, 0x11, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x11,
    0x21, 0xff, 0xda, 0x00, 0x08, 0x01, 0x03, 0x01, 0x01, 0x3f, 0x10, 0x0e,
    0xa1, 0x3a, 0x76, 0xff, 0xc4, 0x00, 0x17, 0x11, 0x01, 0x01, 0x01, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x21, 0x11, 0xff, 0xda, 0x00, 0x08, 0x01, 0x02, 0x01, 0x01,
    0x3f, 0x10, 0x57, 0x0b, 0x08, 0x70, 0xdb, 0xff, 0xc4, 0x00, 0x1f, 0x10,
    0x01, 0x00, 0x02, 0x01, 0x04, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x11, 0x31, 0x41, 0x61, 0x71, 0x91,
    0x21, 0x81, 0xd1, 0xb1, 0xff, 0xda, 0x00, 0x08, 0x01, 0x01, 0x00, 0x01,
    0x3f, 0x10, 0x1b, 0x30, 0xe9, 0x58, 0xbe, 0x1a, 0xfd, 0x8a, 0xeb, 0x8b,
    0x34, 0x74, 0x80, 0x4b, 0xb5, 0xd5, 0xab, 0xcd, 0x46, 0x96, 0x2e, 0xec,
    0xbd, 0xaa, 0x78, 0x47, 0x5c, 0x47, 0xa7, 0x30, 0x49, 0xad, 0x88, 0x7c,
    0x40, 0x74, 0x30, 0xff, 0x00, 0x23, 0x1d, 0x03, 0x0b, 0xb7, 0xd4, 0xff,
    0xd9};
static const size_t kTest2JpgLen = 685;

// test 3 is J422
static const uint8_t kTest3Jpg[] = {
    0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01,
    0x01, 0x01, 0x00, 0x48, 0x00, 0x48, 0x00, 0x00, 0xff, 0xdb, 0x00, 0x43,
    0x00, 0x10, 0x0b, 0x0c, 0x0e, 0x0c, 0x0a, 0x10, 0x0e, 0x0d, 0x0e, 0x12,
    0x11, 0x10, 0x13, 0x18, 0x28, 0x1a, 0x18, 0x16, 0x16, 0x18, 0x31, 0x23,
    0x25, 0x1d, 0x28, 0x3a, 0x33, 0x3d, 0x3c, 0x39, 0x33, 0x38, 0x37, 0x40,
    0x48, 0x5c, 0x4e, 0x40, 0x44, 0x57, 0x45, 0x37, 0x38, 0x50, 0x6d, 0x51,
    0x57, 0x5f, 0x62, 0x67, 0x68, 0x67, 0x3e, 0x4d, 0x71, 0x79, 0x70, 0x64,
    0x78, 0x5c, 0x65, 0x67, 0x63, 0xff, 0xdb, 0x00, 0x43, 0x01, 0x11, 0x12,
    0x12, 0x18, 0x15, 0x18, 0x2f, 0x1a, 0x1a, 0x2f, 0x63, 0x42, 0x38, 0x42,
    0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
    0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
    0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
    0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
    0x63, 0x63, 0xff, 0xc2, 0x00, 0x11, 0x08, 0x00, 0x10, 0x00, 0x20, 0x03,
    0x01, 0x21, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01, 0xff, 0xc4, 0x00,
    0x17, 0x00, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x04, 0x01, 0x02, 0xff, 0xc4,
    0x00, 0x17, 0x01, 0x00, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x00, 0xff,
    0xda, 0x00, 0x0c, 0x03, 0x01, 0x00, 0x02, 0x10, 0x03, 0x10, 0x00, 0x00,
    0x01, 0x43, 0x8d, 0x1f, 0xa2, 0xb3, 0xca, 0x1b, 0x57, 0x0f, 0xff, 0xc4,
    0x00, 0x1b, 0x10, 0x00, 0x03, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x11, 0x00, 0x03,
    0x10, 0x12, 0x13, 0xff, 0xda, 0x00, 0x08, 0x01, 0x01, 0x00, 0x01, 0x05,
    0x02, 0x3b, 0x80, 0x6f, 0x56, 0x76, 0x56, 0x23, 0x87, 0x99, 0x0d, 0x26,
    0x62, 0xf6, 0xbf, 0xff, 0xc4, 0x00, 0x19, 0x11, 0x00, 0x02, 0x03, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x02, 0x10, 0x11, 0x21, 0xff, 0xda, 0x00, 0x08, 0x01, 0x03,
    0x01, 0x01, 0x3f, 0x01, 0x51, 0xce, 0x8c, 0x75, 0xff, 0xc4, 0x00, 0x18,
    0x11, 0x00, 0x03, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x61, 0x21, 0xff, 0xda,
    0x00, 0x08, 0x01, 0x02, 0x01, 0x01, 0x3f, 0x01, 0xa6, 0xd9, 0x2f, 0x84,
    0xe8, 0xf0, 0xff, 0xc4, 0x00, 0x1e, 0x10, 0x00, 0x02, 0x01, 0x03, 0x05,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x11, 0x21, 0x02, 0x12, 0x32, 0x10, 0x31, 0x71, 0x81, 0xa1, 0xff, 0xda,
    0x00, 0x08, 0x01, 0x01, 0x00, 0x06, 0x3f, 0x02, 0x4b, 0xb3, 0x28, 0x32,
    0xd2, 0xed, 0xf9, 0x1d, 0x3e, 0x13, 0x51, 0x73, 0x83, 0xff, 0xc4, 0x00,
    0x1c, 0x10, 0x01, 0x01, 0x01, 0x00, 0x02, 0x03, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x11, 0x00, 0x21, 0x51, 0x31,
    0x61, 0x81, 0xf0, 0xff, 0xda, 0x00, 0x08, 0x01, 0x01, 0x00, 0x01, 0x3f,
    0x21, 0x75, 0x6e, 0x31, 0x94, 0x28, 0xf9, 0x30, 0xdc, 0x27, 0xdb, 0xa9,
    0x01, 0xf3, 0xde, 0x02, 0xa0, 0xed, 0x1e, 0x34, 0x68, 0x23, 0xf9, 0xc6,
    0x48, 0x5d, 0x7a, 0x35, 0x02, 0xf5, 0x6f, 0xff, 0xda, 0x00, 0x0c, 0x03,
    0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x00, 0x00, 0x10, 0x2e, 0x45, 0xff,
    0xc4, 0x00, 0x18, 0x11, 0x00, 0x03, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x11, 0x21,
    0x31, 0xff, 0xda, 0x00, 0x08, 0x01, 0x03, 0x01, 0x01, 0x3f, 0x10, 0x53,
    0x50, 0xba, 0x54, 0xc1, 0x67, 0x4f, 0xff, 0xc4, 0x00, 0x18, 0x11, 0x00,
    0x03, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x11, 0x21, 0x00, 0x10, 0xff, 0xda, 0x00, 0x08,
    0x01, 0x02, 0x01, 0x01, 0x3f, 0x10, 0x18, 0x81, 0x5c, 0x04, 0x1a, 0xca,
    0x91, 0xbf, 0xff, 0xc4, 0x00, 0x1f, 0x10, 0x01, 0x00, 0x02, 0x01, 0x04,
    0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x11, 0x31, 0x41, 0x61, 0x71, 0x91, 0x21, 0x81, 0xd1, 0xb1, 0xff,
    0xda, 0x00, 0x08, 0x01, 0x01, 0x00, 0x01, 0x3f, 0x10, 0x1b, 0x30, 0xe9,
    0x58, 0xbe, 0x1a, 0xfd, 0x8a, 0xeb, 0x8b, 0x34, 0x74, 0x80, 0x4b, 0xb5,
    0xd5, 0xab, 0xcd, 0x46, 0x96, 0x2e, 0xec, 0xbd, 0xaa, 0x78, 0x47, 0x5c,
    0x47, 0xa7, 0x30, 0x49, 0xad, 0x88, 0x7c, 0x40, 0x74, 0x30, 0xff, 0x00,
    0x23, 0x1d, 0x03, 0x0b, 0xb7, 0xd4, 0xff, 0xd9};
static const size_t kTest3JpgLen = 704;

// test 4 is J422 vertical - not supported
static const uint8_t kTest4Jpg[] = {
    0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01,
    0x01, 0x01, 0x00, 0x48, 0x00, 0x48, 0x00, 0x00, 0xff, 0xdb, 0x00, 0x43,
    0x00, 0x10, 0x0b, 0x0c, 0x0e, 0x0c, 0x0a, 0x10, 0x0e, 0x0d, 0x0e, 0x12,
    0x11, 0x10, 0x13, 0x18, 0x28, 0x1a, 0x18, 0x16, 0x16, 0x18, 0x31, 0x23,
    0x25, 0x1d, 0x28, 0x3a, 0x33, 0x3d, 0x3c, 0x39, 0x33, 0x38, 0x37, 0x40,
    0x48, 0x5c, 0x4e, 0x40, 0x44, 0x57, 0x45, 0x37, 0x38, 0x50, 0x6d, 0x51,
    0x57, 0x5f, 0x62, 0x67, 0x68, 0x67, 0x3e, 0x4d, 0x71, 0x79, 0x70, 0x64,
    0x78, 0x5c, 0x65, 0x67, 0x63, 0xff, 0xdb, 0x00, 0x43, 0x01, 0x11, 0x12,
    0x12, 0x18, 0x15, 0x18, 0x2f, 0x1a, 0x1a, 0x2f, 0x63, 0x42, 0x38, 0x42,
    0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
    0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
    0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
    0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
    0x63, 0x63, 0xff, 0xc2, 0x00, 0x11, 0x08, 0x00, 0x10, 0x00, 0x20, 0x03,
    0x01, 0x12, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01, 0xff, 0xc4, 0x00,
    0x18, 0x00, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x05, 0x01, 0x02, 0x03, 0xff,
    0xc4, 0x00, 0x16, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0xff,
    0xda, 0x00, 0x0c, 0x03, 0x01, 0x00, 0x02, 0x10, 0x03, 0x10, 0x00, 0x00,
    0x01, 0xd2, 0x98, 0xe9, 0x03, 0x0c, 0x00, 0x46, 0x21, 0xd9, 0xff, 0xc4,
    0x00, 0x1b, 0x10, 0x00, 0x03, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x11, 0x00, 0x03,
    0x10, 0x12, 0x13, 0xff, 0xda, 0x00, 0x08, 0x01, 0x01, 0x00, 0x01, 0x05,
    0x02, 0x3b, 0x80, 0x6f, 0x56, 0x76, 0x56, 0x23, 0x87, 0x99, 0x0d, 0x26,
    0x62, 0xf6, 0xbf, 0xff, 0xc4, 0x00, 0x17, 0x11, 0x01, 0x01, 0x01, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x11, 0x01, 0x21, 0xff, 0xda, 0x00, 0x08, 0x01, 0x03, 0x01, 0x01,
    0x3f, 0x01, 0x98, 0xb1, 0xbd, 0x47, 0xff, 0xc4, 0x00, 0x18, 0x11, 0x00,
    0x03, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x12, 0x11, 0x21, 0xff, 0xda, 0x00, 0x08,
    0x01, 0x02, 0x01, 0x01, 0x3f, 0x01, 0xb6, 0x35, 0xa2, 0xe1, 0x47, 0xff,
    0xc4, 0x00, 0x1e, 0x10, 0x00, 0x02, 0x01, 0x03, 0x05, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x21, 0x02,
    0x12, 0x32, 0x10, 0x31, 0x71, 0x81, 0xa1, 0xff, 0xda, 0x00, 0x08, 0x01,
    0x01, 0x00, 0x06, 0x3f, 0x02, 0x4b, 0xb3, 0x28, 0x32, 0xd2, 0xed, 0xf9,
    0x1d, 0x3e, 0x13, 0x51, 0x73, 0x83, 0xff, 0xc4, 0x00, 0x1c, 0x10, 0x01,
    0x01, 0x01, 0x00, 0x02, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x11, 0x00, 0x21, 0x51, 0x31, 0x61, 0x81, 0xf0,
    0xff, 0xda, 0x00, 0x08, 0x01, 0x01, 0x00, 0x01, 0x3f, 0x21, 0x75, 0x6e,
    0x31, 0x94, 0x28, 0xf9, 0x30, 0xdc, 0x27, 0xdb, 0xa9, 0x01, 0xf3, 0xde,
    0x02, 0xa0, 0xed, 0x1e, 0x34, 0x68, 0x23, 0xf9, 0xc6, 0x48, 0x5d, 0x7a,
    0x35, 0x02, 0xf5, 0x6f, 0xff, 0xda, 0x00, 0x0c, 0x03, 0x01, 0x00, 0x02,
    0x00, 0x03, 0x00, 0x00, 0x00, 0x10, 0x24, 0xaf, 0xff, 0xc4, 0x00, 0x19,
    0x11, 0x00, 0x03, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x11, 0x51, 0x21, 0x31, 0xff,
    0xda, 0x00, 0x08, 0x01, 0x03, 0x01, 0x01, 0x3f, 0x10, 0x59, 0x11, 0xca,
    0x42, 0x60, 0x9f, 0x69, 0xff, 0xc4, 0x00, 0x19, 0x11, 0x00, 0x02, 0x03,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x11, 0x21, 0x31, 0x61, 0xff, 0xda, 0x00, 0x08, 0x01,
    0x02, 0x01, 0x01, 0x3f, 0x10, 0xb0, 0xd7, 0x27, 0x51, 0xb6, 0x41, 0xff,
    0xc4, 0x00, 0x1f, 0x10, 0x01, 0x00, 0x02, 0x01, 0x04, 0x03, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x11, 0x31,
    0x41, 0x61, 0x71, 0x91, 0x21, 0x81, 0xd1, 0xb1, 0xff, 0xda, 0x00, 0x08,
    0x01, 0x01, 0x00, 0x01, 0x3f, 0x10, 0x1b, 0x30, 0xe9, 0x58, 0xbe, 0x1a,
    0xfd, 0x8a, 0xeb, 0x8b, 0x34, 0x74, 0x80, 0x4b, 0xb5, 0xd5, 0xab, 0xcd,
    0x46, 0x96, 0x2e, 0xec, 0xbd, 0xaa, 0x78, 0x47, 0x5c, 0x47, 0xa7, 0x30,
    0x49, 0xad, 0x88, 0x7c, 0x40, 0x74, 0x30, 0xff, 0x00, 0x23, 0x1d, 0x03,
    0x0b, 0xb7, 0xd4, 0xff, 0xd9};
static const size_t kTest4JpgLen = 701;

TEST_F(LibYUVConvertTest, TestMJPGSize) {
  int width = 0;
  int height = 0;
  int ret = MJPGSize(kTest2Jpg, kTest2JpgLen, &width, &height);
  EXPECT_EQ(0, ret);

  printf("test jpeg size %d x %d\n", width, height);
}

TEST_F(LibYUVConvertTest, TestMJPGToI420) {
  int width = 0;
  int height = 0;
  int ret = MJPGSize(kTest2Jpg, kTest2JpgLen, &width, &height);
  EXPECT_EQ(0, ret);

  int half_width = (width + 1) / 2;
  int half_height = (height + 1) / 2;
  int benchmark_iterations = benchmark_iterations_ * benchmark_width_ *
                             benchmark_height_ / (width * height);
  if (benchmark_iterations < 1) {
    benchmark_iterations = 1;
  }

  align_buffer_page_end(dst_y, width * height);
  align_buffer_page_end(dst_u, half_width * half_height);
  align_buffer_page_end(dst_v, half_width * half_height);
  for (int times = 0; times < benchmark_iterations; ++times) {
    ret = MJPGToI420(kTest2Jpg, kTest2JpgLen, dst_y, width, dst_u, half_width,
                     dst_v, half_width, width, height, width, height);
  }
  // Expect sucesss
  EXPECT_EQ(0, ret);

  // Test result matches known hash value.
  uint32_t dst_y_hash = HashDjb2(dst_y, width * height, 5381);
  uint32_t dst_u_hash = HashDjb2(dst_u, half_width * half_height, 5381);
  uint32_t dst_v_hash = HashDjb2(dst_v, half_width * half_height, 5381);
  EXPECT_EQ(dst_y_hash, 2682851208u);
  EXPECT_EQ(dst_u_hash, 2501859930u);
  EXPECT_EQ(dst_v_hash, 2126459123u);

  free_aligned_buffer_page_end(dst_y);
  free_aligned_buffer_page_end(dst_u);
  free_aligned_buffer_page_end(dst_v);
}

TEST_F(LibYUVConvertTest, TestMJPGToI420_NV21) {
  int width = 0;
  int height = 0;
  int ret = MJPGSize(kTest2Jpg, kTest2JpgLen, &width, &height);
  EXPECT_EQ(0, ret);

  int half_width = (width + 1) / 2;
  int half_height = (height + 1) / 2;
  int benchmark_iterations = benchmark_iterations_ * benchmark_width_ *
                             benchmark_height_ / (width * height);
  if (benchmark_iterations < 1) {
    benchmark_iterations = 1;
  }

  // Convert to NV21
  align_buffer_page_end(dst_y, width * height);
  align_buffer_page_end(dst_vu, half_width * half_height * 2);

  for (int times = 0; times < benchmark_iterations; ++times) {
    ret = MJPGToNV21(kTest2Jpg, kTest2JpgLen, dst_y, width, dst_vu,
                     half_width * 2, width, height, width, height);
  }
  // Expect sucesss
  EXPECT_EQ(0, ret);

  // Convert to I420
  align_buffer_page_end(dst2_y, width * height);
  align_buffer_page_end(dst2_u, half_width * half_height);
  align_buffer_page_end(dst2_v, half_width * half_height);
  for (int times = 0; times < benchmark_iterations; ++times) {
    ret = MJPGToI420(kTest2Jpg, kTest2JpgLen, dst2_y, width, dst2_u, half_width,
                     dst2_v, half_width, width, height, width, height);
  }
  // Expect sucesss
  EXPECT_EQ(0, ret);

  // Convert I420 to NV21
  align_buffer_page_end(dst3_y, width * height);
  align_buffer_page_end(dst3_vu, half_width * half_height * 2);

  I420ToNV21(dst2_y, width, dst2_u, half_width, dst2_v, half_width, dst3_y,
             width, dst3_vu, half_width * 2, width, height);

  for (int i = 0; i < width * height; ++i) {
    EXPECT_EQ(dst_y[i], dst3_y[i]);
  }
  for (int i = 0; i < half_width * half_height * 2; ++i) {
    EXPECT_EQ(dst_vu[i], dst3_vu[i]);
    EXPECT_EQ(dst_vu[i], dst3_vu[i]);
  }

  free_aligned_buffer_page_end(dst3_y);
  free_aligned_buffer_page_end(dst3_vu);

  free_aligned_buffer_page_end(dst2_y);
  free_aligned_buffer_page_end(dst2_u);
  free_aligned_buffer_page_end(dst2_v);

  free_aligned_buffer_page_end(dst_y);
  free_aligned_buffer_page_end(dst_vu);
}

TEST_F(LibYUVConvertTest, TestMJPGToI420_NV12) {
  int width = 0;
  int height = 0;
  int ret = MJPGSize(kTest2Jpg, kTest2JpgLen, &width, &height);
  EXPECT_EQ(0, ret);

  int half_width = (width + 1) / 2;
  int half_height = (height + 1) / 2;
  int benchmark_iterations = benchmark_iterations_ * benchmark_width_ *
                             benchmark_height_ / (width * height);
  if (benchmark_iterations < 1) {
    benchmark_iterations = 1;
  }

  // Convert to NV12
  align_buffer_page_end(dst_y, width * height);
  align_buffer_page_end(dst_uv, half_width * half_height * 2);

  for (int times = 0; times < benchmark_iterations; ++times) {
    ret = MJPGToNV12(kTest2Jpg, kTest2JpgLen, dst_y, width, dst_uv,
                     half_width * 2, width, height, width, height);
  }
  // Expect sucesss
  EXPECT_EQ(0, ret);

  // Convert to I420
  align_buffer_page_end(dst2_y, width * height);
  align_buffer_page_end(dst2_u, half_width * half_height);
  align_buffer_page_end(dst2_v, half_width * half_height);
  for (int times = 0; times < benchmark_iterations; ++times) {
    ret = MJPGToI420(kTest2Jpg, kTest2JpgLen, dst2_y, width, dst2_u, half_width,
                     dst2_v, half_width, width, height, width, height);
  }
  // Expect sucesss
  EXPECT_EQ(0, ret);

  // Convert I420 to NV12
  align_buffer_page_end(dst3_y, width * height);
  align_buffer_page_end(dst3_uv, half_width * half_height * 2);

  I420ToNV12(dst2_y, width, dst2_u, half_width, dst2_v, half_width, dst3_y,
             width, dst3_uv, half_width * 2, width, height);

  for (int i = 0; i < width * height; ++i) {
    EXPECT_EQ(dst_y[i], dst3_y[i]);
  }
  for (int i = 0; i < half_width * half_height * 2; ++i) {
    EXPECT_EQ(dst_uv[i], dst3_uv[i]);
    EXPECT_EQ(dst_uv[i], dst3_uv[i]);
  }

  free_aligned_buffer_page_end(dst3_y);
  free_aligned_buffer_page_end(dst3_uv);

  free_aligned_buffer_page_end(dst2_y);
  free_aligned_buffer_page_end(dst2_u);
  free_aligned_buffer_page_end(dst2_v);

  free_aligned_buffer_page_end(dst_y);
  free_aligned_buffer_page_end(dst_uv);
}

TEST_F(LibYUVConvertTest, TestMJPGToNV21_420) {
  int width = 0;
  int height = 0;
  int ret = MJPGSize(kTest2Jpg, kTest2JpgLen, &width, &height);
  EXPECT_EQ(0, ret);

  int half_width = (width + 1) / 2;
  int half_height = (height + 1) / 2;
  int benchmark_iterations = benchmark_iterations_ * benchmark_width_ *
                             benchmark_height_ / (width * height);
  if (benchmark_iterations < 1) {
    benchmark_iterations = 1;
  }

  align_buffer_page_end(dst_y, width * height);
  align_buffer_page_end(dst_uv, half_width * half_height * 2);
  for (int times = 0; times < benchmark_iterations; ++times) {
    ret = MJPGToNV21(kTest2Jpg, kTest2JpgLen, dst_y, width, dst_uv,
                     half_width * 2, width, height, width, height);
  }
  // Expect sucesss
  EXPECT_EQ(0, ret);

  // Test result matches known hash value.
  uint32_t dst_y_hash = HashDjb2(dst_y, width * height, 5381);
  uint32_t dst_uv_hash = HashDjb2(dst_uv, half_width * half_height * 2, 5381);
  EXPECT_EQ(dst_y_hash, 2682851208u);
  EXPECT_EQ(dst_uv_hash, 1069662856u);

  free_aligned_buffer_page_end(dst_y);
  free_aligned_buffer_page_end(dst_uv);
}

TEST_F(LibYUVConvertTest, TestMJPGToNV12_420) {
  int width = 0;
  int height = 0;
  int ret = MJPGSize(kTest2Jpg, kTest2JpgLen, &width, &height);
  EXPECT_EQ(0, ret);

  int half_width = (width + 1) / 2;
  int half_height = (height + 1) / 2;
  int benchmark_iterations = benchmark_iterations_ * benchmark_width_ *
                             benchmark_height_ / (width * height);
  if (benchmark_iterations < 1) {
    benchmark_iterations = 1;
  }

  align_buffer_page_end(dst_y, width * height);
  align_buffer_page_end(dst_uv, half_width * half_height * 2);
  for (int times = 0; times < benchmark_iterations; ++times) {
    ret = MJPGToNV12(kTest2Jpg, kTest2JpgLen, dst_y, width, dst_uv,
                     half_width * 2, width, height, width, height);
  }
  // Expect sucesss
  EXPECT_EQ(0, ret);

  // Test result matches known hash value. Hashes are for VU so flip the plane.
  uint32_t dst_y_hash = HashDjb2(dst_y, width * height, 5381);
  align_buffer_page_end(dst_vu, half_width * half_height * 2);
  SwapUVPlane(dst_uv, half_width * 2, dst_vu, half_width * 2, half_width,
              half_height);
  uint32_t dst_vu_hash = HashDjb2(dst_vu, half_width * half_height * 2, 5381);
  EXPECT_EQ(dst_y_hash, 2682851208u);
  EXPECT_EQ(dst_vu_hash, 1069662856u);

  free_aligned_buffer_page_end(dst_y);
  free_aligned_buffer_page_end(dst_uv);
  free_aligned_buffer_page_end(dst_vu);
}

// TODO(fbarchard): Improve test to compare against I422, not checksum
TEST_F(LibYUVConvertTest, DISABLED_TestMJPGToNV21_422) {
  int width = 0;
  int height = 0;
  int ret = MJPGSize(kTest3Jpg, kTest3JpgLen, &width, &height);
  EXPECT_EQ(0, ret);

  int half_width = (width + 1) / 2;
  int half_height = (height + 1) / 2;
  int benchmark_iterations = benchmark_iterations_ * benchmark_width_ *
                             benchmark_height_ / (width * height);
  if (benchmark_iterations < 1) {
    benchmark_iterations = 1;
  }

  align_buffer_page_end(dst_y, width * height);
  align_buffer_page_end(dst_uv, half_width * half_height * 2);
  for (int times = 0; times < benchmark_iterations; ++times) {
    ret = MJPGToNV21(kTest3Jpg, kTest3JpgLen, dst_y, width, dst_uv,
                     half_width * 2, width, height, width, height);
  }
  // Expect sucesss
  EXPECT_EQ(0, ret);

  // Test result matches known hash value.
  uint32_t dst_y_hash = HashDjb2(dst_y, width * height, 5381);
  uint32_t dst_uv_hash = HashDjb2(dst_uv, half_width * half_height * 2, 5381);
  EXPECT_EQ(dst_y_hash, 2682851208u);
  EXPECT_EQ(dst_uv_hash, 493520167u);

  free_aligned_buffer_page_end(dst_y);
  free_aligned_buffer_page_end(dst_uv);
}

TEST_F(LibYUVConvertTest, DISABLED_TestMJPGToNV12_422) {
  int width = 0;
  int height = 0;
  int ret = MJPGSize(kTest3Jpg, kTest3JpgLen, &width, &height);
  EXPECT_EQ(0, ret);

  int half_width = (width + 1) / 2;
  int half_height = (height + 1) / 2;
  int benchmark_iterations = benchmark_iterations_ * benchmark_width_ *
                             benchmark_height_ / (width * height);
  if (benchmark_iterations < 1) {
    benchmark_iterations = 1;
  }

  align_buffer_page_end(dst_y, width * height);
  align_buffer_page_end(dst_uv, half_width * half_height * 2);
  for (int times = 0; times < benchmark_iterations; ++times) {
    ret = MJPGToNV12(kTest3Jpg, kTest3JpgLen, dst_y, width, dst_uv,
                     half_width * 2, width, height, width, height);
  }
  // Expect sucesss
  EXPECT_EQ(0, ret);

  // Test result matches known hash value. Hashes are for VU so flip the plane.
  uint32_t dst_y_hash = HashDjb2(dst_y, width * height, 5381);
  align_buffer_page_end(dst_vu, half_width * half_height * 2);
  SwapUVPlane(dst_uv, half_width * 2, dst_vu, half_width * 2, half_width,
              half_height);
  uint32_t dst_vu_hash = HashDjb2(dst_vu, half_width * half_height * 2, 5381);
  EXPECT_EQ(dst_y_hash, 2682851208u);
  EXPECT_EQ(dst_vu_hash, 493520167u);

  free_aligned_buffer_page_end(dst_y);
  free_aligned_buffer_page_end(dst_uv);
  free_aligned_buffer_page_end(dst_vu);
}

TEST_F(LibYUVConvertTest, TestMJPGToNV21_400) {
  int width = 0;
  int height = 0;
  int ret = MJPGSize(kTest0Jpg, kTest0JpgLen, &width, &height);
  EXPECT_EQ(0, ret);

  int half_width = (width + 1) / 2;
  int half_height = (height + 1) / 2;
  int benchmark_iterations = benchmark_iterations_ * benchmark_width_ *
                             benchmark_height_ / (width * height);
  if (benchmark_iterations < 1) {
    benchmark_iterations = 1;
  }

  align_buffer_page_end(dst_y, width * height);
  align_buffer_page_end(dst_uv, half_width * half_height * 2);
  for (int times = 0; times < benchmark_iterations; ++times) {
    ret = MJPGToNV21(kTest0Jpg, kTest0JpgLen, dst_y, width, dst_uv,
                     half_width * 2, width, height, width, height);
  }
  // Expect sucesss
  EXPECT_EQ(0, ret);

  // Test result matches known hash value.
  uint32_t dst_y_hash = HashDjb2(dst_y, width * height, 5381);
  uint32_t dst_uv_hash = HashDjb2(dst_uv, half_width * half_height * 2, 5381);
  EXPECT_EQ(dst_y_hash, 330644005u);
  EXPECT_EQ(dst_uv_hash, 135214341u);

  free_aligned_buffer_page_end(dst_y);
  free_aligned_buffer_page_end(dst_uv);
}

TEST_F(LibYUVConvertTest, TestMJPGToNV12_400) {
  int width = 0;
  int height = 0;
  int ret = MJPGSize(kTest0Jpg, kTest0JpgLen, &width, &height);
  EXPECT_EQ(0, ret);

  int half_width = (width + 1) / 2;
  int half_height = (height + 1) / 2;
  int benchmark_iterations = benchmark_iterations_ * benchmark_width_ *
                             benchmark_height_ / (width * height);
  if (benchmark_iterations < 1) {
    benchmark_iterations = 1;
  }

  align_buffer_page_end(dst_y, width * height);
  align_buffer_page_end(dst_uv, half_width * half_height * 2);
  for (int times = 0; times < benchmark_iterations; ++times) {
    ret = MJPGToNV12(kTest0Jpg, kTest0JpgLen, dst_y, width, dst_uv,
                     half_width * 2, width, height, width, height);
  }
  // Expect sucesss
  EXPECT_EQ(0, ret);

  // Test result matches known hash value. Hashes are for VU so flip the plane.
  uint32_t dst_y_hash = HashDjb2(dst_y, width * height, 5381);
  align_buffer_page_end(dst_vu, half_width * half_height * 2);
  SwapUVPlane(dst_uv, half_width * 2, dst_vu, half_width * 2, half_width,
              half_height);
  uint32_t dst_vu_hash = HashDjb2(dst_vu, half_width * half_height * 2, 5381);
  EXPECT_EQ(dst_y_hash, 330644005u);
  EXPECT_EQ(dst_vu_hash, 135214341u);

  free_aligned_buffer_page_end(dst_y);
  free_aligned_buffer_page_end(dst_uv);
  free_aligned_buffer_page_end(dst_vu);
}

TEST_F(LibYUVConvertTest, TestMJPGToNV21_444) {
  int width = 0;
  int height = 0;
  int ret = MJPGSize(kTest1Jpg, kTest1JpgLen, &width, &height);
  EXPECT_EQ(0, ret);

  int half_width = (width + 1) / 2;
  int half_height = (height + 1) / 2;
  int benchmark_iterations = benchmark_iterations_ * benchmark_width_ *
                             benchmark_height_ / (width * height);
  if (benchmark_iterations < 1) {
    benchmark_iterations = 1;
  }

  align_buffer_page_end(dst_y, width * height);
  align_buffer_page_end(dst_uv, half_width * half_height * 2);
  for (int times = 0; times < benchmark_iterations; ++times) {
    ret = MJPGToNV21(kTest1Jpg, kTest1JpgLen, dst_y, width, dst_uv,
                     half_width * 2, width, height, width, height);
  }
  // Expect sucesss
  EXPECT_EQ(0, ret);

  // Test result matches known hash value.
  uint32_t dst_y_hash = HashDjb2(dst_y, width * height, 5381);
  uint32_t dst_uv_hash = HashDjb2(dst_uv, half_width * half_height * 2, 5381);
  EXPECT_EQ(dst_y_hash, 2682851208u);
  EXPECT_EQ(dst_uv_hash, 506143297u);

  free_aligned_buffer_page_end(dst_y);
  free_aligned_buffer_page_end(dst_uv);
}

TEST_F(LibYUVConvertTest, TestMJPGToNV12_444) {
  int width = 0;
  int height = 0;
  int ret = MJPGSize(kTest1Jpg, kTest1JpgLen, &width, &height);
  EXPECT_EQ(0, ret);

  int half_width = (width + 1) / 2;
  int half_height = (height + 1) / 2;
  int benchmark_iterations = benchmark_iterations_ * benchmark_width_ *
                             benchmark_height_ / (width * height);
  if (benchmark_iterations < 1) {
    benchmark_iterations = 1;
  }

  align_buffer_page_end(dst_y, width * height);
  align_buffer_page_end(dst_uv, half_width * half_height * 2);
  for (int times = 0; times < benchmark_iterations; ++times) {
    ret = MJPGToNV12(kTest1Jpg, kTest1JpgLen, dst_y, width, dst_uv,
                     half_width * 2, width, height, width, height);
  }
  // Expect sucesss
  EXPECT_EQ(0, ret);

  // Test result matches known hash value. Hashes are for VU so flip the plane.
  uint32_t dst_y_hash = HashDjb2(dst_y, width * height, 5381);
  align_buffer_page_end(dst_vu, half_width * half_height * 2);
  SwapUVPlane(dst_uv, half_width * 2, dst_vu, half_width * 2, half_width,
              half_height);
  uint32_t dst_vu_hash = HashDjb2(dst_vu, half_width * half_height * 2, 5381);
  EXPECT_EQ(dst_y_hash, 2682851208u);
  EXPECT_EQ(dst_vu_hash, 506143297u);

  free_aligned_buffer_page_end(dst_y);
  free_aligned_buffer_page_end(dst_uv);
  free_aligned_buffer_page_end(dst_vu);
}

TEST_F(LibYUVConvertTest, TestMJPGToARGB) {
  int width = 0;
  int height = 0;
  int ret = MJPGSize(kTest3Jpg, kTest3JpgLen, &width, &height);
  EXPECT_EQ(0, ret);

  int benchmark_iterations = benchmark_iterations_ * benchmark_width_ *
                             benchmark_height_ / (width * height);
  if (benchmark_iterations < 1) {
    benchmark_iterations = 1;
  }

  align_buffer_page_end(dst_argb, width * height * 4);
  for (int times = 0; times < benchmark_iterations; ++times) {
    ret = MJPGToARGB(kTest3Jpg, kTest3JpgLen, dst_argb, width * 4, width,
                     height, width, height);
  }
  // Expect sucesss
  EXPECT_EQ(0, ret);

  // Test result matches known hash value.
  uint32_t dst_argb_hash = HashDjb2(dst_argb, width * height, 5381);
#ifdef LIBYUV_UNLIMITED_DATA
  EXPECT_EQ(dst_argb_hash, 3900633302u);
#else
  EXPECT_EQ(dst_argb_hash, 2355976473u);
#endif

  free_aligned_buffer_page_end(dst_argb);
}

static int ShowJPegInfo(const uint8_t* sample, size_t sample_size) {
  MJpegDecoder mjpeg_decoder;
  LIBYUV_BOOL ret = mjpeg_decoder.LoadFrame(sample, sample_size);

  int width = mjpeg_decoder.GetWidth();
  int height = mjpeg_decoder.GetHeight();

  // YUV420
  if (mjpeg_decoder.GetColorSpace() == MJpegDecoder::kColorSpaceYCbCr &&
      mjpeg_decoder.GetNumComponents() == 3 &&
      mjpeg_decoder.GetVertSampFactor(0) == 2 &&
      mjpeg_decoder.GetHorizSampFactor(0) == 2 &&
      mjpeg_decoder.GetVertSampFactor(1) == 1 &&
      mjpeg_decoder.GetHorizSampFactor(1) == 1 &&
      mjpeg_decoder.GetVertSampFactor(2) == 1 &&
      mjpeg_decoder.GetHorizSampFactor(2) == 1) {
    printf("JPeg is J420, %dx%d %d bytes\n", width, height,
           static_cast<int>(sample_size));
    // YUV422
  } else if (mjpeg_decoder.GetColorSpace() == MJpegDecoder::kColorSpaceYCbCr &&
             mjpeg_decoder.GetNumComponents() == 3 &&
             mjpeg_decoder.GetVertSampFactor(0) == 1 &&
             mjpeg_decoder.GetHorizSampFactor(0) == 2 &&
             mjpeg_decoder.GetVertSampFactor(1) == 1 &&
             mjpeg_decoder.GetHorizSampFactor(1) == 1 &&
             mjpeg_decoder.GetVertSampFactor(2) == 1 &&
             mjpeg_decoder.GetHorizSampFactor(2) == 1) {
    printf("JPeg is J422, %dx%d %d bytes\n", width, height,
           static_cast<int>(sample_size));
    // YUV444
  } else if (mjpeg_decoder.GetColorSpace() == MJpegDecoder::kColorSpaceYCbCr &&
             mjpeg_decoder.GetNumComponents() == 3 &&
             mjpeg_decoder.GetVertSampFactor(0) == 1 &&
             mjpeg_decoder.GetHorizSampFactor(0) == 1 &&
             mjpeg_decoder.GetVertSampFactor(1) == 1 &&
             mjpeg_decoder.GetHorizSampFactor(1) == 1 &&
             mjpeg_decoder.GetVertSampFactor(2) == 1 &&
             mjpeg_decoder.GetHorizSampFactor(2) == 1) {
    printf("JPeg is J444, %dx%d %d bytes\n", width, height,
           static_cast<int>(sample_size));
    // YUV400
  } else if (mjpeg_decoder.GetColorSpace() ==
                 MJpegDecoder::kColorSpaceGrayscale &&
             mjpeg_decoder.GetNumComponents() == 1 &&
             mjpeg_decoder.GetVertSampFactor(0) == 1 &&
             mjpeg_decoder.GetHorizSampFactor(0) == 1) {
    printf("JPeg is J400, %dx%d %d bytes\n", width, height,
           static_cast<int>(sample_size));
  } else {
    // Unknown colorspace.
    printf("JPeg is Unknown colorspace.\n");
  }
  mjpeg_decoder.UnloadFrame();
  return ret;
}

TEST_F(LibYUVConvertTest, TestMJPGInfo) {
  EXPECT_EQ(1, ShowJPegInfo(kTest0Jpg, kTest0JpgLen));
  EXPECT_EQ(1, ShowJPegInfo(kTest1Jpg, kTest1JpgLen));
  EXPECT_EQ(1, ShowJPegInfo(kTest2Jpg, kTest2JpgLen));
  EXPECT_EQ(1, ShowJPegInfo(kTest3Jpg, kTest3JpgLen));
  EXPECT_EQ(1, ShowJPegInfo(kTest4Jpg,
                            kTest4JpgLen));  // Valid but unsupported.
}
#endif  // HAVE_JPEG

TEST_F(LibYUVConvertTest, NV12Crop) {
  const int SUBSAMP_X = 2;
  const int SUBSAMP_Y = 2;
  const int kWidth = benchmark_width_;
  const int kHeight = benchmark_height_;
  const int crop_y =
      ((benchmark_height_ - (benchmark_height_ * 360 / 480)) / 2 + 1) & ~1;
  const int kDestWidth = benchmark_width_;
  const int kDestHeight = benchmark_height_ - crop_y * 2;
  const int kStrideUV = SUBSAMPLE(kWidth, SUBSAMP_X);
  const int sample_size =
      kWidth * kHeight + kStrideUV * SUBSAMPLE(kHeight, SUBSAMP_Y) * 2;
  align_buffer_page_end(src_y, sample_size);
  uint8_t* src_uv = src_y + kWidth * kHeight;

  align_buffer_page_end(dst_y, kDestWidth * kDestHeight);
  align_buffer_page_end(dst_u, SUBSAMPLE(kDestWidth, SUBSAMP_X) *
                                   SUBSAMPLE(kDestHeight, SUBSAMP_Y));
  align_buffer_page_end(dst_v, SUBSAMPLE(kDestWidth, SUBSAMP_X) *
                                   SUBSAMPLE(kDestHeight, SUBSAMP_Y));

  align_buffer_page_end(dst_y_2, kDestWidth * kDestHeight);
  align_buffer_page_end(dst_u_2, SUBSAMPLE(kDestWidth, SUBSAMP_X) *
                                     SUBSAMPLE(kDestHeight, SUBSAMP_Y));
  align_buffer_page_end(dst_v_2, SUBSAMPLE(kDestWidth, SUBSAMP_X) *
                                     SUBSAMPLE(kDestHeight, SUBSAMP_Y));

  for (int i = 0; i < kHeight * kWidth; ++i) {
    src_y[i] = (fastrand() & 0xff);
  }
  for (int i = 0; i < (SUBSAMPLE(kHeight, SUBSAMP_Y) * kStrideUV) * 2; ++i) {
    src_uv[i] = (fastrand() & 0xff);
  }
  memset(dst_y, 1, kDestWidth * kDestHeight);
  memset(dst_u, 2,
         SUBSAMPLE(kDestWidth, SUBSAMP_X) * SUBSAMPLE(kDestHeight, SUBSAMP_Y));
  memset(dst_v, 3,
         SUBSAMPLE(kDestWidth, SUBSAMP_X) * SUBSAMPLE(kDestHeight, SUBSAMP_Y));
  memset(dst_y_2, 1, kDestWidth * kDestHeight);
  memset(dst_u_2, 2,
         SUBSAMPLE(kDestWidth, SUBSAMP_X) * SUBSAMPLE(kDestHeight, SUBSAMP_Y));
  memset(dst_v_2, 3,
         SUBSAMPLE(kDestWidth, SUBSAMP_X) * SUBSAMPLE(kDestHeight, SUBSAMP_Y));

  ConvertToI420(src_y, sample_size, dst_y_2, kDestWidth, dst_u_2,
                SUBSAMPLE(kDestWidth, SUBSAMP_X), dst_v_2,
                SUBSAMPLE(kDestWidth, SUBSAMP_X), 0, crop_y, kWidth, kHeight,
                kDestWidth, kDestHeight, libyuv::kRotate0, libyuv::FOURCC_NV12);

  NV12ToI420(src_y + crop_y * kWidth, kWidth,
             src_uv + (crop_y / 2) * kStrideUV * 2, kStrideUV * 2, dst_y,
             kDestWidth, dst_u, SUBSAMPLE(kDestWidth, SUBSAMP_X), dst_v,
             SUBSAMPLE(kDestWidth, SUBSAMP_X), kDestWidth, kDestHeight);

  for (int i = 0; i < kDestHeight; ++i) {
    for (int j = 0; j < kDestWidth; ++j) {
      EXPECT_EQ(dst_y[i * kWidth + j], dst_y_2[i * kWidth + j]);
    }
  }
  for (int i = 0; i < SUBSAMPLE(kDestHeight, SUBSAMP_Y); ++i) {
    for (int j = 0; j < SUBSAMPLE(kDestWidth, SUBSAMP_X); ++j) {
      EXPECT_EQ(dst_u[i * SUBSAMPLE(kDestWidth, SUBSAMP_X) + j],
                dst_u_2[i * SUBSAMPLE(kDestWidth, SUBSAMP_X) + j]);
    }
  }
  for (int i = 0; i < SUBSAMPLE(kDestHeight, SUBSAMP_Y); ++i) {
    for (int j = 0; j < SUBSAMPLE(kDestWidth, SUBSAMP_X); ++j) {
      EXPECT_EQ(dst_v[i * SUBSAMPLE(kDestWidth, SUBSAMP_X) + j],
                dst_v_2[i * SUBSAMPLE(kDestWidth, SUBSAMP_X) + j]);
    }
  }
  free_aligned_buffer_page_end(dst_y);
  free_aligned_buffer_page_end(dst_u);
  free_aligned_buffer_page_end(dst_v);
  free_aligned_buffer_page_end(dst_y_2);
  free_aligned_buffer_page_end(dst_u_2);
  free_aligned_buffer_page_end(dst_v_2);
  free_aligned_buffer_page_end(src_y);
}

TEST_F(LibYUVConvertTest, I420CropOddY) {
  const int SUBSAMP_X = 2;
  const int SUBSAMP_Y = 2;
  const int kWidth = benchmark_width_;
  const int kHeight = benchmark_height_;
  const int crop_y = benchmark_height_ > 1 ? 1 : 0;
  const int kDestWidth = benchmark_width_;
  const int kDestHeight = benchmark_height_ - crop_y * 2;
  const int kStrideU = SUBSAMPLE(kWidth, SUBSAMP_X);
  const int kStrideV = SUBSAMPLE(kWidth, SUBSAMP_X);
  const int sample_size = kWidth * kHeight +
                          kStrideU * SUBSAMPLE(kHeight, SUBSAMP_Y) +
                          kStrideV * SUBSAMPLE(kHeight, SUBSAMP_Y);
  align_buffer_page_end(src_y, sample_size);
  uint8_t* src_u = src_y + kWidth * kHeight;
  uint8_t* src_v = src_u + kStrideU * SUBSAMPLE(kHeight, SUBSAMP_Y);

  align_buffer_page_end(dst_y, kDestWidth * kDestHeight);
  align_buffer_page_end(dst_u, SUBSAMPLE(kDestWidth, SUBSAMP_X) *
                                   SUBSAMPLE(kDestHeight, SUBSAMP_Y));
  align_buffer_page_end(dst_v, SUBSAMPLE(kDestWidth, SUBSAMP_X) *
                                   SUBSAMPLE(kDestHeight, SUBSAMP_Y));

  for (int i = 0; i < kHeight * kWidth; ++i) {
    src_y[i] = (fastrand() & 0xff);
  }
  for (int i = 0; i < SUBSAMPLE(kHeight, SUBSAMP_Y) * kStrideU; ++i) {
    src_u[i] = (fastrand() & 0xff);
  }
  for (int i = 0; i < SUBSAMPLE(kHeight, SUBSAMP_Y) * kStrideV; ++i) {
    src_v[i] = (fastrand() & 0xff);
  }
  memset(dst_y, 1, kDestWidth * kDestHeight);
  memset(dst_u, 2,
         SUBSAMPLE(kDestWidth, SUBSAMP_X) * SUBSAMPLE(kDestHeight, SUBSAMP_Y));
  memset(dst_v, 3,
         SUBSAMPLE(kDestWidth, SUBSAMP_X) * SUBSAMPLE(kDestHeight, SUBSAMP_Y));

  MaskCpuFlags(benchmark_cpu_info_);
  for (int i = 0; i < benchmark_iterations_; ++i) {
    ConvertToI420(src_y, sample_size, dst_y, kDestWidth, dst_u,
                  SUBSAMPLE(kDestWidth, SUBSAMP_X), dst_v,
                  SUBSAMPLE(kDestWidth, SUBSAMP_X), 0, crop_y, kWidth, kHeight,
                  kDestWidth, kDestHeight, libyuv::kRotate0,
                  libyuv::FOURCC_I420);
  }

  for (int i = 0; i < kDestHeight; ++i) {
    for (int j = 0; j < kDestWidth; ++j) {
      EXPECT_EQ(src_y[crop_y * kWidth + i * kWidth + j],
                dst_y[i * kDestWidth + j]);
    }
  }
  for (int i = 0; i < SUBSAMPLE(kDestHeight, SUBSAMP_Y); ++i) {
    for (int j = 0; j < SUBSAMPLE(kDestWidth, SUBSAMP_X); ++j) {
      EXPECT_EQ(src_u[(crop_y / 2 + i) * kStrideU + j],
                dst_u[i * SUBSAMPLE(kDestWidth, SUBSAMP_X) + j]);
    }
  }
  for (int i = 0; i < SUBSAMPLE(kDestHeight, SUBSAMP_Y); ++i) {
    for (int j = 0; j < SUBSAMPLE(kDestWidth, SUBSAMP_X); ++j) {
      EXPECT_EQ(src_v[(crop_y / 2 + i) * kStrideV + j],
                dst_v[i * SUBSAMPLE(kDestWidth, SUBSAMP_X) + j]);
    }
  }

  free_aligned_buffer_page_end(dst_y);
  free_aligned_buffer_page_end(dst_u);
  free_aligned_buffer_page_end(dst_v);
  free_aligned_buffer_page_end(src_y);
}

#define TESTPTOB(NAME, UYVYTOI420, UYVYTONV12)                                \
  TEST_F(LibYUVConvertTest, NAME) {                                           \
    const int kWidth = benchmark_width_;                                      \
    const int kHeight = benchmark_height_;                                    \
                                                                              \
    align_buffer_page_end(orig_uyvy, 4 * SUBSAMPLE(kWidth, 2) * kHeight);     \
    align_buffer_page_end(orig_y, kWidth* kHeight);                           \
    align_buffer_page_end(orig_u,                                             \
                          SUBSAMPLE(kWidth, 2) * SUBSAMPLE(kHeight, 2));      \
    align_buffer_page_end(orig_v,                                             \
                          SUBSAMPLE(kWidth, 2) * SUBSAMPLE(kHeight, 2));      \
                                                                              \
    align_buffer_page_end(dst_y_orig, kWidth* kHeight);                       \
    align_buffer_page_end(dst_uv_orig,                                        \
                          2 * SUBSAMPLE(kWidth, 2) * SUBSAMPLE(kHeight, 2));  \
                                                                              \
    align_buffer_page_end(dst_y, kWidth* kHeight);                            \
    align_buffer_page_end(dst_uv,                                             \
                          2 * SUBSAMPLE(kWidth, 2) * SUBSAMPLE(kHeight, 2));  \
                                                                              \
    MemRandomize(orig_uyvy, 4 * SUBSAMPLE(kWidth, 2) * kHeight);              \
                                                                              \
    /* Convert UYVY to NV12 in 2 steps for reference */                       \
    libyuv::UYVYTOI420(orig_uyvy, 4 * SUBSAMPLE(kWidth, 2), orig_y, kWidth,   \
                       orig_u, SUBSAMPLE(kWidth, 2), orig_v,                  \
                       SUBSAMPLE(kWidth, 2), kWidth, kHeight);                \
    libyuv::I420ToNV12(orig_y, kWidth, orig_u, SUBSAMPLE(kWidth, 2), orig_v,  \
                       SUBSAMPLE(kWidth, 2), dst_y_orig, kWidth, dst_uv_orig, \
                       2 * SUBSAMPLE(kWidth, 2), kWidth, kHeight);            \
                                                                              \
    /* Convert to NV12 */                                                     \
    for (int i = 0; i < benchmark_iterations_; ++i) {                         \
      libyuv::UYVYTONV12(orig_uyvy, 4 * SUBSAMPLE(kWidth, 2), dst_y, kWidth,  \
                         dst_uv, 2 * SUBSAMPLE(kWidth, 2), kWidth, kHeight);  \
    }                                                                         \
                                                                              \
    for (int i = 0; i < kWidth * kHeight; ++i) {                              \
      EXPECT_EQ(orig_y[i], dst_y[i]);                                         \
    }                                                                         \
    for (int i = 0; i < kWidth * kHeight; ++i) {                              \
      EXPECT_EQ(dst_y_orig[i], dst_y[i]);                                     \
    }                                                                         \
    for (int i = 0; i < 2 * SUBSAMPLE(kWidth, 2) * SUBSAMPLE(kHeight, 2);     \
         ++i) {                                                               \
      EXPECT_EQ(dst_uv_orig[i], dst_uv[i]);                                   \
    }                                                                         \
                                                                              \
    free_aligned_buffer_page_end(orig_uyvy);                                  \
    free_aligned_buffer_page_end(orig_y);                                     \
    free_aligned_buffer_page_end(orig_u);                                     \
    free_aligned_buffer_page_end(orig_v);                                     \
    free_aligned_buffer_page_end(dst_y_orig);                                 \
    free_aligned_buffer_page_end(dst_uv_orig);                                \
    free_aligned_buffer_page_end(dst_y);                                      \
    free_aligned_buffer_page_end(dst_uv);                                     \
  }

TESTPTOB(TestYUY2ToNV12, YUY2ToI420, YUY2ToNV12)
TESTPTOB(TestUYVYToNV12, UYVYToI420, UYVYToNV12)

TEST_F(LibYUVConvertTest, MM21ToYUY2) {
  const int kWidth = (benchmark_width_ + 15) & (~15);
  const int kHeight = (benchmark_height_ + 31) & (~31);

  align_buffer_page_end(orig_y, kWidth * kHeight);
  align_buffer_page_end(orig_uv,
                        2 * SUBSAMPLE(kWidth, 2) * SUBSAMPLE(kHeight, 2));

  align_buffer_page_end(tmp_y, kWidth * kHeight);
  align_buffer_page_end(tmp_u, SUBSAMPLE(kWidth, 2) * SUBSAMPLE(kHeight, 2));
  align_buffer_page_end(tmp_v, SUBSAMPLE(kWidth, 2) * SUBSAMPLE(kHeight, 2));

  align_buffer_page_end(dst_yuyv, 4 * SUBSAMPLE(kWidth, 2) * kHeight);
  align_buffer_page_end(golden_yuyv, 4 * SUBSAMPLE(kWidth, 2) * kHeight);

  MemRandomize(orig_y, kWidth * kHeight);
  MemRandomize(orig_uv, 2 * SUBSAMPLE(kWidth, 2) * SUBSAMPLE(kHeight, 2));

  /* Convert MM21 to YUY2 in 2 steps for reference */
  libyuv::MM21ToI420(orig_y, kWidth, orig_uv, 2 * SUBSAMPLE(kWidth, 2), tmp_y,
                     kWidth, tmp_u, SUBSAMPLE(kWidth, 2), tmp_v,
                     SUBSAMPLE(kWidth, 2), kWidth, kHeight);
  libyuv::I420ToYUY2(tmp_y, kWidth, tmp_u, SUBSAMPLE(kWidth, 2), tmp_v,
                     SUBSAMPLE(kWidth, 2), golden_yuyv,
                     4 * SUBSAMPLE(kWidth, 2), kWidth, kHeight);

  /* Convert to NV12 */
  for (int i = 0; i < benchmark_iterations_; ++i) {
    libyuv::MM21ToYUY2(orig_y, kWidth, orig_uv, 2 * SUBSAMPLE(kWidth, 2),
                       dst_yuyv, 4 * SUBSAMPLE(kWidth, 2), kWidth, kHeight);
  }

  for (int i = 0; i < 4 * SUBSAMPLE(kWidth, 2) * kHeight; ++i) {
    EXPECT_EQ(dst_yuyv[i], golden_yuyv[i]);
  }

  free_aligned_buffer_page_end(orig_y);
  free_aligned_buffer_page_end(orig_uv);
  free_aligned_buffer_page_end(tmp_y);
  free_aligned_buffer_page_end(tmp_u);
  free_aligned_buffer_page_end(tmp_v);
  free_aligned_buffer_page_end(dst_yuyv);
  free_aligned_buffer_page_end(golden_yuyv);
}

// Test RGB24 to J420 is exact
#if defined(LIBYUV_BIT_EXACT)
TEST_F(LibYUVConvertTest, TestRGB24ToJ420) {
  const int kSize = 256;
  align_buffer_page_end(orig_rgb24, kSize * 3 * 2);  // 2 rows of RGB24
  align_buffer_page_end(dest_j420, kSize * 3 / 2 * 2);
  int iterations256 = (benchmark_width_ * benchmark_height_ + (kSize * 2 - 1)) /
                      (kSize * 2) * benchmark_iterations_;

  for (int i = 0; i < kSize * 3 * 2; ++i) {
    orig_rgb24[i] = i;
  }

  for (int i = 0; i < iterations256; ++i) {
    RGB24ToJ420(orig_rgb24, kSize * 3, dest_j420, kSize,  // Y plane
                dest_j420 + kSize * 2, kSize / 2,         // U plane
                dest_j420 + kSize * 5 / 2, kSize / 2,     // V plane
                kSize, 2);
  }

  uint32_t checksum = HashDjb2(dest_j420, kSize * 3 / 2 * 2, 5381);
  EXPECT_EQ(2755440272u, checksum);

  free_aligned_buffer_page_end(orig_rgb24);
  free_aligned_buffer_page_end(dest_j420);
}
#endif

// Test RGB24 to I420 is exact
#if defined(LIBYUV_BIT_EXACT)
TEST_F(LibYUVConvertTest, TestRGB24ToI420) {
  const int kSize = 256;
  align_buffer_page_end(orig_rgb24, kSize * 3 * 2);  // 2 rows of RGB24
  align_buffer_page_end(dest_i420, kSize * 3 / 2 * 2);
  int iterations256 = (benchmark_width_ * benchmark_height_ + (kSize * 2 - 1)) /
                      (kSize * 2) * benchmark_iterations_;

  for (int i = 0; i < kSize * 3 * 2; ++i) {
    orig_rgb24[i] = i;
  }

  for (int i = 0; i < iterations256; ++i) {
    RGB24ToI420(orig_rgb24, kSize * 3, dest_i420, kSize,  // Y plane
                dest_i420 + kSize * 2, kSize / 2,         // U plane
                dest_i420 + kSize * 5 / 2, kSize / 2,     // V plane
                kSize, 2);
  }

  uint32_t checksum = HashDjb2(dest_i420, kSize * 3 / 2 * 2, 5381);
  EXPECT_EQ(1526656597u, checksum);

  free_aligned_buffer_page_end(orig_rgb24);
  free_aligned_buffer_page_end(dest_i420);
}
#endif

#endif  // !defined(LEAN_TESTS)

}  // namespace libyuv
