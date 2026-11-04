/*
 *  Copyright 2026 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/rotate_row.h"
#include "libyuv/row.h"

#if !defined(LIBYUV_DISABLE_WASM) && defined(__wasm_simd128__)
#include <wasm_simd128.h>

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

// Interleave the low or high halves of 2 vectors as 8, 16, 32 or 64 bit
// elements. These map to punpck on x86 and zip on Arm.
#define ZIPLO8(a, b)                                                           \
  wasm_i8x16_shuffle(a, b, 0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, \
                     23)
#define ZIPHI8(a, b)                                                         \
  wasm_i8x16_shuffle(a, b, 8, 24, 9, 25, 10, 26, 11, 27, 12, 28, 13, 29, 14, \
                     30, 15, 31)
#define ZIPLO16(a, b) wasm_i16x8_shuffle(a, b, 0, 8, 1, 9, 2, 10, 3, 11)
#define ZIPHI16(a, b) wasm_i16x8_shuffle(a, b, 4, 12, 5, 13, 6, 14, 7, 15)
#define ZIPLO32(a, b) wasm_i32x4_shuffle(a, b, 0, 4, 1, 5)
#define ZIPHI32(a, b) wasm_i32x4_shuffle(a, b, 2, 6, 3, 7)
#define ZIPLO64(a, b) wasm_i64x2_shuffle(a, b, 0, 2)
#define ZIPHI64(a, b) wasm_i64x2_shuffle(a, b, 1, 3)

// Transpose 8 rows x 16 bytes. c[m] holds columns 2m and 2m + 1 of the 8 rows
// in its low and high 8 bytes.
static inline void Transpose8x16_WASMSIMD(const uint8_t* src,
                                          ptrdiff_t s,
                                          v128_t* c) {
  v128_t r0 = wasm_v128_load(src);
  v128_t r1 = wasm_v128_load(src + s);
  v128_t r2 = wasm_v128_load(src + s * 2);
  v128_t r3 = wasm_v128_load(src + s * 3);
  v128_t r4 = wasm_v128_load(src + s * 4);
  v128_t r5 = wasm_v128_load(src + s * 5);
  v128_t r6 = wasm_v128_load(src + s * 6);
  v128_t r7 = wasm_v128_load(src + s * 7);

  // Bytes of row pairs. a0 is columns 0..7 of rows 0, 1.
  v128_t a0 = ZIPLO8(r0, r1);
  v128_t a1 = ZIPHI8(r0, r1);
  v128_t a2 = ZIPLO8(r2, r3);
  v128_t a3 = ZIPHI8(r2, r3);
  v128_t a4 = ZIPLO8(r4, r5);
  v128_t a5 = ZIPHI8(r4, r5);
  v128_t a6 = ZIPLO8(r6, r7);
  v128_t a7 = ZIPHI8(r6, r7);

  // Words. b0 is columns 0..3 of rows 0..3.
  v128_t b0 = ZIPLO16(a0, a2);
  v128_t b1 = ZIPHI16(a0, a2);
  v128_t b2 = ZIPLO16(a1, a3);
  v128_t b3 = ZIPHI16(a1, a3);
  v128_t b4 = ZIPLO16(a4, a6);
  v128_t b5 = ZIPHI16(a4, a6);
  v128_t b6 = ZIPLO16(a5, a7);
  v128_t b7 = ZIPHI16(a5, a7);

  // Dwords. c0 is columns 0, 1 of rows 0..7.
  c[0] = ZIPLO32(b0, b4);
  c[1] = ZIPHI32(b0, b4);
  c[2] = ZIPLO32(b1, b5);
  c[3] = ZIPHI32(b1, b5);
  c[4] = ZIPLO32(b2, b6);
  c[5] = ZIPHI32(b2, b6);
  c[6] = ZIPLO32(b3, b7);
  c[7] = ZIPHI32(b3, b7);
}

#if defined(HAS_TRANSPOSEWX8_WASMSIMD) || defined(HAS_TRANSPOSEUVWX8_WASMSIMD)
// Transpose 8 rows x 16 bytes. Even columns go to dst_a and odd columns go to
// dst_b. Width is a multiple of 16 bytes.
static void TransposeWx8_Byte_WASMSIMD(const uint8_t* src,
                                       int src_stride,
                                       uint8_t* dst_a,
                                       int dst_stride_a,
                                       uint8_t* dst_b,
                                       int dst_stride_b,
                                       int byte_width) {
  const ptrdiff_t sa = dst_stride_a;
  const ptrdiff_t sb = dst_stride_b;
  while (byte_width > 0) {
    v128_t c[8];
    int m;
    Transpose8x16_WASMSIMD(src, src_stride, c);
    for (m = 0; m < 8; ++m) {
      wasm_v128_store64_lane(dst_a + sa * m, c[m], 0);
      wasm_v128_store64_lane(dst_b + sb * m, c[m], 1);
    }
    src += 16;
    dst_a += sa * 8;
    dst_b += sb * 8;
    byte_width -= 16;
  }
}
#endif  // HAS_TRANSPOSEWX8_WASMSIMD || HAS_TRANSPOSEUVWX8_WASMSIMD

#if defined(HAS_TRANSPOSEWX8_WASMSIMD)
// Transpose 16x8. Width is a multiple of 16.
void TransposeWx8_WASMSIMD(const uint8_t* src,
                           int src_stride,
                           uint8_t* dst,
                           int dst_stride,
                           int width) {
  TransposeWx8_Byte_WASMSIMD(src, src_stride, dst, dst_stride * 2,
                             dst + dst_stride, dst_stride * 2, width);
}
#endif  // HAS_TRANSPOSEWX8_WASMSIMD

#if defined(HAS_TRANSPOSEUVWX8_WASMSIMD)
// Transpose UV 8x8. Width is a multiple of 8.
void TransposeUVWx8_WASMSIMD(const uint8_t* src,
                             int src_stride,
                             uint8_t* dst_a,
                             int dst_stride_a,
                             uint8_t* dst_b,
                             int dst_stride_b,
                             int width) {
  TransposeWx8_Byte_WASMSIMD(src, src_stride, dst_a, dst_stride_a, dst_b,
                             dst_stride_b, width * 2);
}
#endif  // HAS_TRANSPOSEUVWX8_WASMSIMD

#if defined(HAS_TRANSPOSEWX16_WASMSIMD) || defined(HAS_TRANSPOSEUVWX16_WASMSIMD)
// Transpose 16 rows x 16 bytes as 2 halves of 8 rows, which are joined with
// 64 bit interleaves to store full 16 byte rows. Even columns go to dst_a and
// odd columns go to dst_b. Width is a multiple of 16 bytes.
static void TransposeWx16_Byte_WASMSIMD(const uint8_t* src,
                                        int src_stride,
                                        uint8_t* dst_a,
                                        int dst_stride_a,
                                        uint8_t* dst_b,
                                        int dst_stride_b,
                                        int byte_width) {
  const ptrdiff_t s = src_stride;
  const ptrdiff_t sa = dst_stride_a;
  const ptrdiff_t sb = dst_stride_b;
  while (byte_width > 0) {
    v128_t lo[8];
    v128_t hi[8];
    int m;
    Transpose8x16_WASMSIMD(src, s, lo);
    Transpose8x16_WASMSIMD(src + s * 8, s, hi);
    for (m = 0; m < 8; ++m) {
      wasm_v128_store(dst_a + sa * m, ZIPLO64(lo[m], hi[m]));
      wasm_v128_store(dst_b + sb * m, ZIPHI64(lo[m], hi[m]));
    }
    src += 16;
    dst_a += sa * 8;
    dst_b += sb * 8;
    byte_width -= 16;
  }
}
#endif  // HAS_TRANSPOSEWX16_WASMSIMD || HAS_TRANSPOSEUVWX16_WASMSIMD

#if defined(HAS_TRANSPOSEWX16_WASMSIMD)
// Transpose 16x16. Width is a multiple of 16.
void TransposeWx16_WASMSIMD(const uint8_t* src,
                            int src_stride,
                            uint8_t* dst,
                            int dst_stride,
                            int width) {
  TransposeWx16_Byte_WASMSIMD(src, src_stride, dst, dst_stride * 2,
                              dst + dst_stride, dst_stride * 2, width);
}
#endif  // HAS_TRANSPOSEWX16_WASMSIMD

#if defined(HAS_TRANSPOSEUVWX16_WASMSIMD)
// Transpose UV 8x16. Width is a multiple of 8.
void TransposeUVWx16_WASMSIMD(const uint8_t* src,
                              int src_stride,
                              uint8_t* dst_a,
                              int dst_stride_a,
                              uint8_t* dst_b,
                              int dst_stride_b,
                              int width) {
  TransposeWx16_Byte_WASMSIMD(src, src_stride, dst_a, dst_stride_a, dst_b,
                              dst_stride_b, width * 2);
}
#endif  // HAS_TRANSPOSEUVWX16_WASMSIMD

#if defined(HAS_TRANSPOSEWX4_32_WASMSIMD)
// Transpose 32 bit values (ARGB) in Wx4 tiles, moving down 4 rows of source
// and across 4 columns of destination. Width is a multiple of 4.
void TransposeWx4_32_WASMSIMD(const uint8_t* src,
                              int src_stride,
                              uint8_t* dst,
                              int dst_stride,
                              int width) {
  const ptrdiff_t s = src_stride;
  const ptrdiff_t d = dst_stride;
  uint8_t* dst1 = dst + d;
  uint8_t* dst2 = dst1 + d;
  uint8_t* dst3 = dst2 + d;
  while (width >= 8) {
    v128_t r0 = wasm_v128_load(src);
    v128_t r1 = wasm_v128_load(src + s);
    v128_t r2 = wasm_v128_load(src + s * 2);
    v128_t r3 = wasm_v128_load(src + s * 3);
    src += s * 4;
    v128_t r4 = wasm_v128_load(src);
    v128_t r5 = wasm_v128_load(src + s);
    v128_t r6 = wasm_v128_load(src + s * 2);
    v128_t r7 = wasm_v128_load(src + s * 3);
    src += s * 4;
    v128_t a0 = ZIPLO32(r0, r1);
    v128_t a1 = ZIPHI32(r0, r1);
    v128_t a2 = ZIPLO32(r2, r3);
    v128_t a3 = ZIPHI32(r2, r3);
    v128_t b0 = ZIPLO32(r4, r5);
    v128_t b1 = ZIPHI32(r4, r5);
    v128_t b2 = ZIPLO32(r6, r7);
    v128_t b3 = ZIPHI32(r6, r7);
    wasm_v128_store(dst, ZIPLO64(a0, a2));
    wasm_v128_store(dst1, ZIPHI64(a0, a2));
    wasm_v128_store(dst2, ZIPLO64(a1, a3));
    wasm_v128_store(dst3, ZIPHI64(a1, a3));
    wasm_v128_store(dst + 16, ZIPLO64(b0, b2));
    wasm_v128_store(dst1 + 16, ZIPHI64(b0, b2));
    wasm_v128_store(dst2 + 16, ZIPLO64(b1, b3));
    wasm_v128_store(dst3 + 16, ZIPHI64(b1, b3));
    dst += 32;
    dst1 += 32;
    dst2 += 32;
    dst3 += 32;
    width -= 8;
  }
  while (width > 0) {
    v128_t r0 = wasm_v128_load(src);
    v128_t r1 = wasm_v128_load(src + s);
    v128_t r2 = wasm_v128_load(src + s * 2);
    v128_t r3 = wasm_v128_load(src + s * 3);
    v128_t a0 = ZIPLO32(r0, r1);
    v128_t a1 = ZIPHI32(r0, r1);
    v128_t a2 = ZIPLO32(r2, r3);
    v128_t a3 = ZIPHI32(r2, r3);
    wasm_v128_store(dst, ZIPLO64(a0, a2));
    wasm_v128_store(dst1, ZIPHI64(a0, a2));
    wasm_v128_store(dst2, ZIPLO64(a1, a3));
    wasm_v128_store(dst3, ZIPHI64(a1, a3));
    src += s * 4;
    dst += 16;
    dst1 += 16;
    dst2 += 16;
    dst3 += 16;
    width -= 4;
  }
}
#endif  // HAS_TRANSPOSEWX4_32_WASMSIMD

#if defined(HAS_TRANSPOSEWX8_16_WASMSIMD)
void TransposeWx8_16_WASMSIMD(const uint16_t* src,
                              int src_stride,
                              uint16_t* dst,
                              int dst_stride,
                              int width) {
  const ptrdiff_t s = src_stride;
  const ptrdiff_t d = dst_stride;
  while (width > 0) {
    v128_t r0 = wasm_v128_load(src);
    v128_t r1 = wasm_v128_load(src + s);
    v128_t r2 = wasm_v128_load(src + s * 2);
    v128_t r3 = wasm_v128_load(src + s * 3);
    v128_t r4 = wasm_v128_load(src + s * 4);
    v128_t r5 = wasm_v128_load(src + s * 5);
    v128_t r6 = wasm_v128_load(src + s * 6);
    v128_t r7 = wasm_v128_load(src + s * 7);

    v128_t t0 = ZIPLO16(r0, r1);
    v128_t t1 = ZIPHI16(r0, r1);
    v128_t t2 = ZIPLO16(r2, r3);
    v128_t t3 = ZIPHI16(r2, r3);
    v128_t t4 = ZIPLO16(r4, r5);
    v128_t t5 = ZIPHI16(r4, r5);
    v128_t t6 = ZIPLO16(r6, r7);
    v128_t t7 = ZIPHI16(r6, r7);

    v128_t u0 = ZIPLO32(t0, t2);
    v128_t u1 = ZIPHI32(t0, t2);
    v128_t u2 = ZIPLO32(t1, t3);
    v128_t u3 = ZIPHI32(t1, t3);
    v128_t u4 = ZIPLO32(t4, t6);
    v128_t u5 = ZIPHI32(t4, t6);
    v128_t u6 = ZIPLO32(t5, t7);
    v128_t u7 = ZIPHI32(t5, t7);

    wasm_v128_store(dst, ZIPLO64(u0, u4));
    wasm_v128_store(dst + d, ZIPHI64(u0, u4));
    wasm_v128_store(dst + d * 2, ZIPLO64(u1, u5));
    wasm_v128_store(dst + d * 3, ZIPHI64(u1, u5));
    wasm_v128_store(dst + d * 4, ZIPLO64(u2, u6));
    wasm_v128_store(dst + d * 5, ZIPHI64(u2, u6));
    wasm_v128_store(dst + d * 6, ZIPLO64(u3, u7));
    wasm_v128_store(dst + d * 7, ZIPHI64(u3, u7));
    src += 8;
    dst += d * 8;
    width -= 8;
  }
}
#endif  // HAS_TRANSPOSEWX8_16_WASMSIMD

#undef ZIPLO8
#undef ZIPHI8
#undef ZIPLO16
#undef ZIPHI16
#undef ZIPLO32
#undef ZIPHI32
#undef ZIPLO64
#undef ZIPHI64

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif

#endif  // !defined(LIBYUV_DISABLE_WASM) && defined(__wasm_simd128__)
