/*
 *  Copyright 2015 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/rotate.h"
#include "libyuv/rotate_row.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

#define TANY(NAMEANY, TPOS_SIMD, TPOS_C, MASK)                          \
  void NAMEANY(const uint8_t* src, int src_stride, uint8_t* dst,        \
               int dst_stride, int width) {                             \
    int r = width & MASK;                                               \
    int n = width - r;                                                  \
    if (n > 0) {                                                        \
      TPOS_SIMD(src, src_stride, dst, dst_stride, n);                   \
    }                                                                   \
    ptrdiff_t np = n;                                                   \
    TPOS_C(src + np, src_stride, dst + np * dst_stride, dst_stride, r); \
  }

#ifdef HAS_TRANSPOSEWX8_NEON
TANY(TransposeWx8_Any_NEON, TransposeWx8_NEON, TransposeWx8_C, 7)
#endif
#ifdef HAS_TRANSPOSEWX16_NEON
TANY(TransposeWx16_Any_NEON, TransposeWx16_NEON, TransposeWx16_C, 15)
#endif
#ifdef HAS_TRANSPOSEWX8_SSSE3
TANY(TransposeWx8_Any_SSSE3, TransposeWx8_SSSE3, TransposeWx8_C, 15)
#endif
#ifdef HAS_TRANSPOSEWX16_AVX2
TANY(TransposeWx16_Any_AVX2, TransposeWx16_AVX2, TransposeWx16_C, 15)
#endif
#ifdef HAS_TRANSPOSEWX16_LSX
TANY(TransposeWx16_Any_LSX, TransposeWx16_LSX, TransposeWx16_C, 15)
#endif
#ifdef HAS_TRANSPOSEWX8_WASMSIMD
TANY(TransposeWx8_Any_WASMSIMD, TransposeWx8_WASMSIMD, TransposeWx8_C, 15)
#endif
#ifdef HAS_TRANSPOSEWX16_WASMSIMD
TANY(TransposeWx16_Any_WASMSIMD, TransposeWx16_WASMSIMD, TransposeWx16_C, 15)
#endif
#undef TANY

#define TUVANY(NAMEANY, TPOS_SIMD, TPOS_C, MASK)                               \
  void NAMEANY(const uint8_t* src, int src_stride, uint8_t* dst_a,             \
               int dst_stride_a, uint8_t* dst_b, int dst_stride_b,             \
               int width) {                                                    \
    int r = width & MASK;                                                      \
    int n = width - r;                                                         \
    if (n > 0) {                                                               \
      TPOS_SIMD(src, src_stride, dst_a, dst_stride_a, dst_b, dst_stride_b, n); \
    }                                                                          \
    ptrdiff_t np = n;                                                          \
    TPOS_C(src + np * 2, src_stride, dst_a + np * dst_stride_a, dst_stride_a,  \
           dst_b + np * dst_stride_b, dst_stride_b, r);                        \
  }

#ifdef HAS_TRANSPOSEUVWX8_NEON
TUVANY(TransposeUVWx8_Any_NEON, TransposeUVWx8_NEON, TransposeUVWx8_C, 7)
#endif
#ifdef HAS_TRANSPOSEUVWX8_SSE2
TUVANY(TransposeUVWx8_Any_SSE2, TransposeUVWx8_SSE2, TransposeUVWx8_C, 7)
#endif
#ifdef HAS_TRANSPOSEUVWX16_AVX2
TUVANY(TransposeUVWx16_Any_AVX2, TransposeUVWx16_AVX2, TransposeUVWx16_C, 7)
#endif
#ifdef HAS_TRANSPOSEUVWX16_LSX
TUVANY(TransposeUVWx16_Any_LSX, TransposeUVWx16_LSX, TransposeUVWx16_C, 7)
#endif
#ifdef HAS_TRANSPOSEUVWX8_WASMSIMD
TUVANY(TransposeUVWx8_Any_WASMSIMD,
       TransposeUVWx8_WASMSIMD,
       TransposeUVWx8_C,
       7)
#endif
#ifdef HAS_TRANSPOSEUVWX16_WASMSIMD
TUVANY(TransposeUVWx16_Any_WASMSIMD,
       TransposeUVWx16_WASMSIMD,
       TransposeUVWx16_C,
       7)
#endif
#undef TUVANY

#define TANY32(NAMEANY, TPOS_SIMD, TPOS_C, MASK)                 \
  void NAMEANY(const uint8_t* src, int src_stride, uint8_t* dst, \
               int dst_stride, int width) {                      \
    int r = width & MASK;                                        \
    int n = width - r;                                           \
    if (n > 0) {                                                 \
      TPOS_SIMD(src, src_stride, dst, dst_stride, n);            \
    }                                                            \
    if (r > 0) {                                                 \
      TPOS_C(src + (ptrdiff_t)n * src_stride, src_stride,        \
             dst + (ptrdiff_t)n * 4, dst_stride, 4, r);          \
    }                                                            \
  }

#ifdef HAS_TRANSPOSEWX4_32_SSE2
TANY32(TransposeWx4_32_Any_SSE2, TransposeWx4_32_SSE2, TransposeWxH_32_C, 3)
#endif
#ifdef HAS_TRANSPOSEWX4_32_LSX
TANY32(TransposeWx4_32_Any_LSX, TransposeWx4_32_LSX, TransposeWxH_32_C, 3)
#endif
#ifdef HAS_TRANSPOSEWX4_32_LASX
TANY32(TransposeWx4_32_Any_LASX, TransposeWx4_32_LASX, TransposeWxH_32_C, 7)
#endif
#ifdef HAS_TRANSPOSEWX4_32_WASMSIMD
TANY32(TransposeWx4_32_Any_WASMSIMD,
       TransposeWx4_32_WASMSIMD,
       TransposeWxH_32_C,
       3)
#endif
#undef TANY32

#define TANY32_SRC(NAMEANY, TPOS_SIMD, MASK, HEIGHT)                   \
  void NAMEANY(const uint8_t* src, int src_stride, uint8_t* dst,       \
               int dst_stride, int width) {                            \
    int r = width & MASK;                                              \
    int n = width - r;                                                 \
    if (n > 0) {                                                       \
      TPOS_SIMD(src, src_stride, dst, dst_stride, n);                  \
    }                                                                  \
    ptrdiff_t np = n;                                                  \
    TransposeWxH_32_C(src + np * 4, src_stride, dst + np * dst_stride, \
                      dst_stride, r, HEIGHT);                          \
  }

#ifdef HAS_TRANSPOSEWX8_32_AVX2
TANY32_SRC(TransposeWx8_32_Any_AVX2, TransposeWx8_32_AVX2, 3, 8)
#endif
#undef TANY32_SRC

#define TANY16(NAMEANY, TPOS_SIMD, MASK, HEIGHT)                               \
  void NAMEANY(const uint16_t* src, int src_stride, uint16_t* dst,             \
               int dst_stride, int width) {                                    \
    int r = width & MASK;                                                      \
    int n = width - r;                                                         \
    if (n > 0) {                                                               \
      TPOS_SIMD(src, src_stride, dst, dst_stride, n);                          \
    }                                                                          \
    ptrdiff_t np = n;                                                          \
    TransposeWxH_16_C(src + np, src_stride, dst + np * dst_stride, dst_stride, \
                      r, HEIGHT);                                              \
  }

#ifdef HAS_TRANSPOSEWX8_16_NEON
TANY16(TransposeWx8_16_Any_NEON, TransposeWx8_16_NEON, 7, 8)
#endif
#ifdef HAS_TRANSPOSEWX16_16_AVX2
TANY16(TransposeWx16_16_Any_AVX2, TransposeWx16_16_AVX2, 7, 16)
#endif
#ifdef HAS_TRANSPOSEWX8_16_LSX
TANY16(TransposeWx8_16_Any_LSX, TransposeWx8_16_LSX, 7, 8)
#endif
#ifdef HAS_TRANSPOSEWX16_16_LASX
TANY16(TransposeWx16_16_Any_LASX, TransposeWx16_16_LASX, 7, 16)
#endif
#ifdef HAS_TRANSPOSEWX8_16_WASMSIMD
TANY16(TransposeWx8_16_Any_WASMSIMD, TransposeWx8_16_WASMSIMD, 7, 8)
#endif
#undef TANY16

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
