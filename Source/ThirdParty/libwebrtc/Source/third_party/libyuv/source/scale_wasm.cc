/*
 *  Copyright 2026 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/row.h"
#include "libyuv/scale_row.h"

#if !defined(LIBYUV_DISABLE_WASM) && defined(__wasm_simd128__)
#include <wasm_simd128.h>

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

// Arbitrary i8x16.shuffle masks are rematerialized on every use by V8, so
// shuffles that do not map to a single instruction use swizzle with masks
// loaded before the loop.  Mask indices of 128 produce 0.
#ifdef __wasm_relaxed_simd__
#define LIBYUV_WASM_SWIZZLE wasm_i8x16_relaxed_swizzle
#else
#define LIBYUV_WASM_SWIZZLE wasm_i8x16_swizzle
#endif

// Load a constant vector.  The empty asm hides the address so the compiler
// keeps the vector in a local instead of sinking a v128.const into the loop.
static inline v128_t LoadConst(const uint8_t* p) {
  asm("" : "+r"(p));
  return wasm_v128_load(p);
}

#ifdef HAS_SCALEROWDOWN2_WASMSIMD
// Point sample 32 pixels to 16, keeping the odd pixels.
void ScaleRowDown2_WASMSIMD(const uint8_t* src_ptr,
                            ptrdiff_t src_stride,
                            uint8_t* dst_ptr,
                            int dst_width) {
  (void)src_stride;
  while (dst_width > 0) {
    v128_t a = wasm_u16x8_shr(wasm_v128_load(src_ptr), 8);
    v128_t b = wasm_u16x8_shr(wasm_v128_load(src_ptr + 16), 8);
    wasm_v128_store(dst_ptr, wasm_u8x16_narrow_i16x8(a, b));
    src_ptr += 32;
    dst_ptr += 16;
    dst_width -= 16;
  }
}

// Average pairs of pixels: (s0 + s1 + 1) >> 1.
void ScaleRowDown2Linear_WASMSIMD(const uint8_t* src_ptr,
                                  ptrdiff_t src_stride,
                                  uint8_t* dst_ptr,
                                  int dst_width) {
  const v128_t zero = wasm_i16x8_const_splat(0);
  (void)src_stride;
  while (dst_width > 0) {
    v128_t a = wasm_u16x8_extadd_pairwise_u8x16(wasm_v128_load(src_ptr));
    v128_t b = wasm_u16x8_extadd_pairwise_u8x16(wasm_v128_load(src_ptr + 16));
    a = wasm_u16x8_avgr(a, zero);
    b = wasm_u16x8_avgr(b, zero);
    wasm_v128_store(dst_ptr, wasm_u8x16_narrow_i16x8(a, b));
    src_ptr += 32;
    dst_ptr += 16;
    dst_width -= 16;
  }
}

// Average 2x2 boxes: (s0 + s1 + t0 + t1 + 2) >> 2.
void ScaleRowDown2Box_WASMSIMD(const uint8_t* src_ptr,
                               ptrdiff_t src_stride,
                               uint8_t* dst_ptr,
                               int dst_width) {
  const uint8_t* t = src_ptr + src_stride;
  const v128_t two = wasm_i16x8_const_splat(2);
  while (dst_width > 0) {
    v128_t a = wasm_i16x8_add(
        wasm_u16x8_extadd_pairwise_u8x16(wasm_v128_load(src_ptr)),
        wasm_u16x8_extadd_pairwise_u8x16(wasm_v128_load(t)));
    v128_t b = wasm_i16x8_add(
        wasm_u16x8_extadd_pairwise_u8x16(wasm_v128_load(src_ptr + 16)),
        wasm_u16x8_extadd_pairwise_u8x16(wasm_v128_load(t + 16)));
    a = wasm_u16x8_shr(wasm_i16x8_add(a, two), 2);
    b = wasm_u16x8_shr(wasm_i16x8_add(b, two), 2);
    wasm_v128_store(dst_ptr, wasm_u8x16_narrow_i16x8(a, b));
    src_ptr += 32;
    t += 32;
    dst_ptr += 16;
    dst_width -= 16;
  }
}
#endif  // HAS_SCALEROWDOWN2_WASMSIMD

#ifdef HAS_SCALEROWDOWN4_WASMSIMD
// Point sample 64 pixels to 16, keeping pixel 2 of every 4.  Odd 16 bit
// lanes then even bytes, which are unzips (uzp on Arm, pack on x86).
void ScaleRowDown4_WASMSIMD(const uint8_t* src_ptr,
                            ptrdiff_t src_stride,
                            uint8_t* dst_ptr,
                            int dst_width) {
  const v128_t mask = wasm_i16x8_const_splat(0xff);
  (void)src_stride;
  while (dst_width > 0) {
    v128_t a = wasm_i16x8_shuffle(wasm_v128_load(src_ptr),
                                  wasm_v128_load(src_ptr + 16), 1, 3, 5, 7, 9,
                                  11, 13, 15);
    v128_t b = wasm_i16x8_shuffle(wasm_v128_load(src_ptr + 32),
                                  wasm_v128_load(src_ptr + 48), 1, 3, 5, 7, 9,
                                  11, 13, 15);
    wasm_v128_store(dst_ptr, wasm_u8x16_narrow_i16x8(wasm_v128_and(a, mask),
                                                     wasm_v128_and(b, mask)));
    src_ptr += 64;
    dst_ptr += 16;
    dst_width -= 16;
  }
}

// Average 4x4 boxes: (sum + 8) >> 4.  8 pixels per loop.
void ScaleRowDown4Box_WASMSIMD(const uint8_t* src_ptr,
                               ptrdiff_t src_stride,
                               uint8_t* dst_ptr,
                               int dst_width) {
  const v128_t eight = wasm_i16x8_const_splat(8);
  while (dst_width > 0) {
    v128_t a = wasm_i16x8_const_splat(0);
    v128_t b = wasm_i16x8_const_splat(0);
    const uint8_t* s = src_ptr;
    for (int i = 0; i < 4; ++i) {
      a = wasm_i16x8_add(a,
                         wasm_u16x8_extadd_pairwise_u8x16(wasm_v128_load(s)));
      b = wasm_i16x8_add(
          b, wasm_u16x8_extadd_pairwise_u8x16(wasm_v128_load(s + 16)));
      s += src_stride;
    }
    a = wasm_u16x8_narrow_i32x4(wasm_u32x4_extadd_pairwise_u16x8(a),
                                wasm_u32x4_extadd_pairwise_u16x8(b));
    a = wasm_u16x8_shr(wasm_i16x8_add(a, eight), 4);
    wasm_v128_store64_lane(dst_ptr, wasm_u8x16_narrow_i16x8(a, a), 0);
    src_ptr += 32;
    dst_ptr += 8;
    dst_width -= 8;
  }
}
#endif  // HAS_SCALEROWDOWN4_WASMSIMD

#ifdef HAS_SCALEROWDOWN34_WASMSIMD
static const uint8_t kShuf34[8][16] = {
    // ScaleRowDown34: 16 bytes from a, then b, then 8 bytes from b.
    {0, 1, 3, 4, 5, 7, 8, 9, 11, 12, 13, 15, 128, 128, 128, 128},
    {128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 0, 1, 3, 4},
    {5, 7, 8, 9, 11, 12, 13, 15, 128, 128, 128, 128, 128, 128, 128, 128},
    // Filter34: s0 s1 s3 and s0 s2 s2 of every 4.
    {0, 1, 3, 3, 4, 5, 7, 7, 8, 9, 11, 11, 12, 13, 15, 15},
    {0, 2, 2, 3, 4, 6, 6, 7, 8, 10, 10, 11, 12, 14, 14, 15},
    // Store34: bytes 0, 1, 2 of every 4 from a, then b, then 8 from b.
    {0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14, 128, 128, 128, 128},
    {128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 0, 1, 2, 4},
    {5, 6, 8, 9, 10, 12, 13, 14, 128, 128, 128, 128, 128, 128, 128, 128},
};

// Point sample 32 pixels to 24, keeping pixels 0, 1 and 3 of every 4.
void ScaleRowDown34_WASMSIMD(const uint8_t* src_ptr,
                             ptrdiff_t src_stride,
                             uint8_t* dst_ptr,
                             int dst_width) {
  const v128_t m0 = LoadConst(kShuf34[0]);
  const v128_t m1 = LoadConst(kShuf34[1]);
  const v128_t m2 = LoadConst(kShuf34[2]);
  (void)src_stride;
  while (dst_width > 0) {
    v128_t a = wasm_v128_load(src_ptr);
    v128_t b = wasm_v128_load(src_ptr + 16);
    wasm_v128_store(dst_ptr, wasm_v128_or(LIBYUV_WASM_SWIZZLE(a, m0),
                                          LIBYUV_WASM_SWIZZLE(b, m1)));
    wasm_v128_store64_lane(dst_ptr + 16, LIBYUV_WASM_SWIZZLE(b, m2), 0);
    src_ptr += 32;
    dst_ptr += 24;
    dst_width -= 24;
  }
}

// Rounding-down average: (a + b) >> 1.
static inline v128_t AvgFloor(v128_t a, v128_t b) {
  return wasm_i8x16_sub(
      wasm_u8x16_avgr(a, b),
      wasm_v128_and(wasm_v128_xor(a, b), wasm_i8x16_const_splat(1)));
}

// Horizontal 4 to 3 filter of 16 pixels.  Result bytes 4n+0..2 are
//   (3 * s0 + s1 + 2) >> 2, (s1 + s2 + 1) >> 1, (s2 + 3 * s3 + 2) >> 2
// using (3 * a + b + 2) >> 2 == avgr(a, (a + b) >> 1).  Byte 4n+3 is unused.
static inline v128_t Filter34(v128_t s, v128_t mp, v128_t ma) {
  v128_t p = LIBYUV_WASM_SWIZZLE(s, mp);  // s0 s1 s3
  v128_t a = LIBYUV_WASM_SWIZZLE(s, ma);  // s0 s2 s2
  v128_t b = wasm_u32x4_shr(s, 8);        // s1 s2 s3
  return wasm_u8x16_avgr(p, AvgFloor(a, b));
}

// Filter rows 1 and 2 together, 1 : 1
void ScaleRowDown34_1_Box_WASMSIMD(const uint8_t* src_ptr,
                                   ptrdiff_t src_stride,
                                   uint8_t* dst_ptr,
                                   int dst_width) {
  const uint8_t* t = src_ptr + src_stride;
  const v128_t mp = LoadConst(kShuf34[3]);
  const v128_t ma = LoadConst(kShuf34[4]);
  const v128_t m0 = LoadConst(kShuf34[5]);
  const v128_t m1 = LoadConst(kShuf34[6]);
  const v128_t m2 = LoadConst(kShuf34[7]);
  while (dst_width > 0) {
    v128_t a = wasm_u8x16_avgr(Filter34(wasm_v128_load(src_ptr), mp, ma),
                               Filter34(wasm_v128_load(t), mp, ma));
    v128_t b = wasm_u8x16_avgr(Filter34(wasm_v128_load(src_ptr + 16), mp, ma),
                               Filter34(wasm_v128_load(t + 16), mp, ma));
    wasm_v128_store(dst_ptr, wasm_v128_or(LIBYUV_WASM_SWIZZLE(a, m0),
                                          LIBYUV_WASM_SWIZZLE(b, m1)));
    wasm_v128_store64_lane(dst_ptr + 16, LIBYUV_WASM_SWIZZLE(b, m2), 0);
    src_ptr += 32;
    t += 32;
    dst_ptr += 24;
    dst_width -= 24;
  }
}

// Filter rows 0 and 1 together, 3 : 1
void ScaleRowDown34_0_Box_WASMSIMD(const uint8_t* src_ptr,
                                   ptrdiff_t src_stride,
                                   uint8_t* dst_ptr,
                                   int dst_width) {
  const uint8_t* t = src_ptr + src_stride;
  const v128_t mp = LoadConst(kShuf34[3]);
  const v128_t ma = LoadConst(kShuf34[4]);
  const v128_t m0 = LoadConst(kShuf34[5]);
  const v128_t m1 = LoadConst(kShuf34[6]);
  const v128_t m2 = LoadConst(kShuf34[7]);
  while (dst_width > 0) {
    v128_t a0 = Filter34(wasm_v128_load(src_ptr), mp, ma);
    v128_t b0 = Filter34(wasm_v128_load(src_ptr + 16), mp, ma);
    v128_t a1 = Filter34(wasm_v128_load(t), mp, ma);
    v128_t b1 = Filter34(wasm_v128_load(t + 16), mp, ma);
    v128_t a = wasm_u8x16_avgr(a0, AvgFloor(a0, a1));
    v128_t b = wasm_u8x16_avgr(b0, AvgFloor(b0, b1));
    wasm_v128_store(dst_ptr, wasm_v128_or(LIBYUV_WASM_SWIZZLE(a, m0),
                                          LIBYUV_WASM_SWIZZLE(b, m1)));
    wasm_v128_store64_lane(dst_ptr + 16, LIBYUV_WASM_SWIZZLE(b, m2), 0);
    src_ptr += 32;
    t += 32;
    dst_ptr += 24;
    dst_width -= 24;
  }
}
#endif  // HAS_SCALEROWDOWN34_WASMSIMD

#ifdef HAS_SCALEROWDOWN38_WASMSIMD
static const uint8_t kShuf38[3][16] = {
    // ScaleRowDown38: 6 bytes from a, then 6 bytes from b.
    {0, 3, 6, 8, 11, 14, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128},
    {128, 128, 128, 128, 128, 128, 0, 3, 6, 8, 11, 14, 128, 128, 128, 128},
    // Store38: bytes 0, 1, 3 of every 4.
    {0, 1, 3, 4, 5, 7, 8, 9, 11, 12, 13, 15, 128, 128, 128, 128},
};

// Point sample 32 pixels to 12, keeping pixels 0, 3 and 6 of every 8.
void ScaleRowDown38_WASMSIMD(const uint8_t* src_ptr,
                             ptrdiff_t src_stride,
                             uint8_t* dst_ptr,
                             int dst_width) {
  const v128_t m0 = LoadConst(kShuf38[0]);
  const v128_t m1 = LoadConst(kShuf38[1]);
  (void)src_stride;
  while (dst_width > 0) {
    v128_t d =
        wasm_v128_or(LIBYUV_WASM_SWIZZLE(wasm_v128_load(src_ptr), m0),
                     LIBYUV_WASM_SWIZZLE(wasm_v128_load(src_ptr + 16), m1));
    wasm_v128_store64_lane(dst_ptr, d, 0);
    wasm_v128_store32_lane(dst_ptr + 8, d, 2);
    src_ptr += 32;
    dst_ptr += 12;
    dst_width -= 12;
  }
}

// Sums of columns 0-2, 3-5 and 6-7 of 8 columns of u16 are scaled by the
// i16 factors in lanes 0, 3 and 6 of k.  Returns the high 16 bits of the
// products in 32 bit lanes 0, 1 and 3.
static inline v128_t Sum38(v128_t s, v128_t k) {
  const v128_t zero = wasm_i16x8_const_splat(0);
  v128_t s1 = wasm_i8x16_shuffle(s, zero, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
                                 13, 14, 15, 16, 17);
  v128_t s2 = wasm_i8x16_shuffle(s, zero, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
                                 15, 16, 17, 18, 19);
  s = wasm_i16x8_add(wasm_i16x8_add(s, s1), s2);
  return wasm_u32x4_shr(wasm_i32x4_dot_i16x8(s, k), 16);
}

// Pack 12 results from Sum38 and store them.
static inline void Store38(uint8_t* dst_ptr,
                           v128_t m,
                           v128_t a,
                           v128_t b,
                           v128_t c,
                           v128_t d) {
  v128_t r = wasm_u8x16_narrow_i16x8(wasm_u16x8_narrow_i32x4(a, b),
                                     wasm_u16x8_narrow_i32x4(c, d));
  r = LIBYUV_WASM_SWIZZLE(r, m);
  wasm_v128_store64_lane(dst_ptr, r, 0);
  wasm_v128_store32_lane(dst_ptr + 8, r, 2);
}

// 8x3 -> 3x1
void ScaleRowDown38_3_Box_WASMSIMD(const uint8_t* src_ptr,
                                   ptrdiff_t src_stride,
                                   uint8_t* dst_ptr,
                                   int dst_width) {
  const v128_t k =
      wasm_i16x8_const(65536 / 9, 0, 0, 65536 / 9, 0, 0, 65536 / 6, 0);
  const v128_t m = LoadConst(kShuf38[2]);
  while (dst_width > 0) {
    v128_t s[4];
    for (int i = 0; i < 4; ++i) {
      const uint8_t* p = src_ptr + i * 8;
      s[i] = wasm_i16x8_add(wasm_i16x8_add(wasm_u16x8_load8x8(p),
                                           wasm_u16x8_load8x8(p + src_stride)),
                            wasm_u16x8_load8x8(p + src_stride * 2));
    }
    Store38(dst_ptr, m, Sum38(s[0], k), Sum38(s[1], k), Sum38(s[2], k),
            Sum38(s[3], k));
    src_ptr += 32;
    dst_ptr += 12;
    dst_width -= 12;
  }
}

// 8x2 -> 3x1
void ScaleRowDown38_2_Box_WASMSIMD(const uint8_t* src_ptr,
                                   ptrdiff_t src_stride,
                                   uint8_t* dst_ptr,
                                   int dst_width) {
  const v128_t k =
      wasm_i16x8_const(65536 / 6, 0, 0, 65536 / 6, 0, 0, 65536 / 4, 0);
  const v128_t m = LoadConst(kShuf38[2]);
  while (dst_width > 0) {
    v128_t s[4];
    for (int i = 0; i < 4; ++i) {
      const uint8_t* p = src_ptr + i * 8;
      s[i] = wasm_i16x8_add(wasm_u16x8_load8x8(p),
                            wasm_u16x8_load8x8(p + src_stride));
    }
    Store38(dst_ptr, m, Sum38(s[0], k), Sum38(s[1], k), Sum38(s[2], k),
            Sum38(s[3], k));
    src_ptr += 32;
    dst_ptr += 12;
    dst_width -= 12;
  }
}
#endif  // HAS_SCALEROWDOWN38_WASMSIMD

#ifdef HAS_SCALEFILTERCOLS_WASMSIMD
// Bilinear column filtering with 7 bit fractions, matching ScaleFilterCols_C:
//   a + ((f * (b - a) + 64) >> 7).  8 pixels per loop.  The fraction
// (x >> 9) & 0x7f only uses the low 16 bits of x, so x is tracked in 16 bit
// lanes.
void ScaleFilterCols_WASMSIMD(uint8_t* dst_ptr,
                              const uint8_t* src_ptr,
                              int dst_width,
                              int x,
                              int dx) {
  uint32_t ux = (uint32_t)x;
  const uint32_t udx = (uint32_t)dx;
  v128_t xv = wasm_i16x8_make((int16_t)ux, (int16_t)(ux + udx),
                              (int16_t)(ux + udx * 2), (int16_t)(ux + udx * 3),
                              (int16_t)(ux + udx * 4), (int16_t)(ux + udx * 5),
                              (int16_t)(ux + udx * 6), (int16_t)(ux + udx * 7));
  const v128_t dx8 = wasm_i16x8_splat((int16_t)(udx * 8));
  const v128_t maskff = wasm_i16x8_const_splat(0xff);
  const v128_t round = wasm_i16x8_const_splat(64);
  while (dst_width > 0) {
    v128_t p = wasm_i16x8_const_splat(0);
    p = wasm_v128_load16_lane(src_ptr + ((int)ux >> 16), p, 0);
    ux += udx;
    p = wasm_v128_load16_lane(src_ptr + ((int)ux >> 16), p, 1);
    ux += udx;
    p = wasm_v128_load16_lane(src_ptr + ((int)ux >> 16), p, 2);
    ux += udx;
    p = wasm_v128_load16_lane(src_ptr + ((int)ux >> 16), p, 3);
    ux += udx;
    p = wasm_v128_load16_lane(src_ptr + ((int)ux >> 16), p, 4);
    ux += udx;
    p = wasm_v128_load16_lane(src_ptr + ((int)ux >> 16), p, 5);
    ux += udx;
    p = wasm_v128_load16_lane(src_ptr + ((int)ux >> 16), p, 6);
    ux += udx;
    p = wasm_v128_load16_lane(src_ptr + ((int)ux >> 16), p, 7);
    ux += udx;
    v128_t f = wasm_u16x8_shr(xv, 9);
    xv = wasm_i16x8_add(xv, dx8);
    v128_t a = wasm_v128_and(p, maskff);
    v128_t d = wasm_i16x8_sub(wasm_u16x8_shr(p, 8), a);
    d = wasm_i16x8_shr(wasm_i16x8_add(wasm_i16x8_mul(d, f), round), 7);
    d = wasm_i16x8_add(d, a);
    wasm_v128_store64_lane(dst_ptr, wasm_u8x16_narrow_i16x8(d, d), 0);
    dst_ptr += 8;
    dst_width -= 8;
  }
}
#endif  // HAS_SCALEFILTERCOLS_WASMSIMD

#if defined(HAS_SCALEARGBROWDOWN2_WASMSIMD) || \
    defined(HAS_SCALEARGBROWDOWNEVEN_WASMSIMD)
// Average 2x2 boxes of ARGB pixels.  s0 s1 hold 4 pairs of pixels from row
// 0 and t0 t1 hold the same pairs from row 1.  Returns 4 pixels.
static inline v128_t BoxARGB(v128_t s0, v128_t s1, v128_t t0, v128_t t1) {
  v128_t se = wasm_i32x4_shuffle(s0, s1, 0, 2, 4, 6);
  v128_t so = wasm_i32x4_shuffle(s0, s1, 1, 3, 5, 7);
  v128_t te = wasm_i32x4_shuffle(t0, t1, 0, 2, 4, 6);
  v128_t to = wasm_i32x4_shuffle(t0, t1, 1, 3, 5, 7);
  // Interleave even and odd pixels so pairwise adds sum the same channel.
  v128_t a = wasm_i16x8_add(
      wasm_u16x8_extadd_pairwise_u8x16(wasm_i8x16_shuffle(
          se, so, 0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, 23)),
      wasm_u16x8_extadd_pairwise_u8x16(wasm_i8x16_shuffle(
          te, to, 0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, 23)));
  v128_t b = wasm_i16x8_add(wasm_u16x8_extadd_pairwise_u8x16(wasm_i8x16_shuffle(
                                se, so, 8, 24, 9, 25, 10, 26, 11, 27, 12, 28,
                                13, 29, 14, 30, 15, 31)),
                            wasm_u16x8_extadd_pairwise_u8x16(wasm_i8x16_shuffle(
                                te, to, 8, 24, 9, 25, 10, 26, 11, 27, 12, 28,
                                13, 29, 14, 30, 15, 31)));
  a = wasm_u16x8_shr(wasm_i16x8_add(a, wasm_i16x8_const_splat(2)), 2);
  b = wasm_u16x8_shr(wasm_i16x8_add(b, wasm_i16x8_const_splat(2)), 2);
  return wasm_u8x16_narrow_i16x8(a, b);
}
#endif

#ifdef HAS_SCALEARGBROWDOWN2_WASMSIMD
// Point sample 8 ARGB pixels to 4, keeping the odd pixels.
void ScaleARGBRowDown2_WASMSIMD(const uint8_t* src_argb,
                                ptrdiff_t src_stride,
                                uint8_t* dst_argb,
                                int dst_width) {
  (void)src_stride;
  while (dst_width > 0) {
    v128_t a = wasm_v128_load(src_argb);
    v128_t b = wasm_v128_load(src_argb + 16);
    wasm_v128_store(dst_argb, wasm_i32x4_shuffle(a, b, 1, 3, 5, 7));
    src_argb += 32;
    dst_argb += 16;
    dst_width -= 4;
  }
}

// Average pairs of ARGB pixels.
void ScaleARGBRowDown2Linear_WASMSIMD(const uint8_t* src_argb,
                                      ptrdiff_t src_stride,
                                      uint8_t* dst_argb,
                                      int dst_width) {
  (void)src_stride;
  while (dst_width > 0) {
    v128_t a = wasm_v128_load(src_argb);
    v128_t b = wasm_v128_load(src_argb + 16);
    wasm_v128_store(dst_argb,
                    wasm_u8x16_avgr(wasm_i32x4_shuffle(a, b, 0, 2, 4, 6),
                                    wasm_i32x4_shuffle(a, b, 1, 3, 5, 7)));
    src_argb += 32;
    dst_argb += 16;
    dst_width -= 4;
  }
}

// Average 2x2 boxes of ARGB pixels.
void ScaleARGBRowDown2Box_WASMSIMD(const uint8_t* src_argb,
                                   ptrdiff_t src_stride,
                                   uint8_t* dst_argb,
                                   int dst_width) {
  const uint8_t* t = src_argb + src_stride;
  while (dst_width > 0) {
    wasm_v128_store(
        dst_argb,
        BoxARGB(wasm_v128_load(src_argb), wasm_v128_load(src_argb + 16),
                wasm_v128_load(t), wasm_v128_load(t + 16)));
    src_argb += 32;
    t += 32;
    dst_argb += 16;
    dst_width -= 4;
  }
}
#endif  // HAS_SCALEARGBROWDOWN2_WASMSIMD

#ifdef HAS_SCALEARGBROWDOWNEVEN_WASMSIMD
// Point sample ARGB pixels with an even step.  4 pixels per loop.
void ScaleARGBRowDownEven_WASMSIMD(const uint8_t* src_argb,
                                   ptrdiff_t src_stride,
                                   int src_stepx,
                                   uint8_t* dst_argb,
                                   int dst_width) {
  const ptrdiff_t step = (ptrdiff_t)src_stepx * 4;
  (void)src_stride;
  while (dst_width > 0) {
    v128_t v = wasm_v128_load32_zero(src_argb);
    v = wasm_v128_load32_lane(src_argb + step, v, 1);
    v = wasm_v128_load32_lane(src_argb + step * 2, v, 2);
    v = wasm_v128_load32_lane(src_argb + step * 3, v, 3);
    wasm_v128_store(dst_argb, v);
    src_argb += step * 4;
    dst_argb += 16;
    dst_width -= 4;
  }
}

// Average 2x2 boxes of ARGB pixels with an even step.  4 pixels per loop.
void ScaleARGBRowDownEvenBox_WASMSIMD(const uint8_t* src_argb,
                                      ptrdiff_t src_stride,
                                      int src_stepx,
                                      uint8_t* dst_argb,
                                      int dst_width) {
  const ptrdiff_t step = (ptrdiff_t)src_stepx * 4;
  while (dst_width > 0) {
    const uint8_t* t = src_argb + src_stride;
    v128_t s0 = wasm_v128_load64_zero(src_argb);
    v128_t t0 = wasm_v128_load64_zero(t);
    s0 = wasm_v128_load64_lane(src_argb + step, s0, 1);
    t0 = wasm_v128_load64_lane(t + step, t0, 1);
    v128_t s1 = wasm_v128_load64_zero(src_argb + step * 2);
    v128_t t1 = wasm_v128_load64_zero(t + step * 2);
    s1 = wasm_v128_load64_lane(src_argb + step * 3, s1, 1);
    t1 = wasm_v128_load64_lane(t + step * 3, t1, 1);
    wasm_v128_store(dst_argb, BoxARGB(s0, s1, t0, t1));
    src_argb += step * 4;
    dst_argb += 16;
    dst_width -= 4;
  }
}
#endif  // HAS_SCALEARGBROWDOWNEVEN_WASMSIMD

#ifdef HAS_SCALEARGBCOLS_WASMSIMD
// Point sample ARGB columns.  4 pixels per loop.
void ScaleARGBCols_WASMSIMD(uint8_t* dst_argb,
                            const uint8_t* src_argb,
                            int dst_width,
                            int x,
                            int dx) {
  uint32_t ux = (uint32_t)x;
  const uint32_t udx = (uint32_t)dx;
  while (dst_width > 0) {
    v128_t v = wasm_v128_load32_zero(src_argb + ((int)ux >> 16) * 4);
    ux += udx;
    v = wasm_v128_load32_lane(src_argb + ((int)ux >> 16) * 4, v, 1);
    ux += udx;
    v = wasm_v128_load32_lane(src_argb + ((int)ux >> 16) * 4, v, 2);
    ux += udx;
    v = wasm_v128_load32_lane(src_argb + ((int)ux >> 16) * 4, v, 3);
    ux += udx;
    wasm_v128_store(dst_argb, v);
    dst_argb += 16;
    dst_width -= 4;
  }
}
#endif  // HAS_SCALEARGBCOLS_WASMSIMD

#ifdef HAS_SCALEARGBFILTERCOLS_WASMSIMD
// Bilinear ARGB column filtering with 7 bit fractions, matching
// ScaleARGBFilterCols_C: (a * (127 - f) + b * f) >> 7.  4 pixels per loop.
// The fraction (x >> 9) & 0x7f only uses the low 16 bits of x, so x is
// tracked in 16 bit lanes, 4 per pixel to match the channels.
void ScaleARGBFilterCols_WASMSIMD(uint8_t* dst_argb,
                                  const uint8_t* src_argb,
                                  int dst_width,
                                  int x,
                                  int dx) {
  uint32_t ux = (uint32_t)x;
  const uint32_t udx = (uint32_t)dx;
  int16_t x0 = (int16_t)ux;
  int16_t x1 = (int16_t)(ux + udx);
  int16_t x2 = (int16_t)(ux + udx * 2);
  int16_t x3 = (int16_t)(ux + udx * 3);
  v128_t x01 = wasm_i16x8_make(x0, x0, x0, x0, x1, x1, x1, x1);
  v128_t x23 = wasm_i16x8_make(x2, x2, x2, x2, x3, x3, x3, x3);
  const v128_t dx4 = wasm_i16x8_splat((int16_t)(udx * 4));
  const v128_t k7f = wasm_i16x8_const_splat(0x7f);
  while (dst_width > 0) {
    // Load pixel pairs (a, b) for 4 pixels.
    v128_t p0 = wasm_v128_load64_zero(src_argb + ((int)ux >> 16) * 4);
    ux += udx;
    p0 = wasm_v128_load64_lane(src_argb + ((int)ux >> 16) * 4, p0, 1);
    ux += udx;
    v128_t p1 = wasm_v128_load64_zero(src_argb + ((int)ux >> 16) * 4);
    ux += udx;
    p1 = wasm_v128_load64_lane(src_argb + ((int)ux >> 16) * 4, p1, 1);
    ux += udx;
    v128_t a = wasm_i32x4_shuffle(p0, p1, 0, 2, 4, 6);  // a0 a1 a2 a3
    v128_t b = wasm_i32x4_shuffle(p0, p1, 1, 3, 5, 7);  // b0 b1 b2 b3
    v128_t f01 = wasm_u16x8_shr(x01, 9);
    v128_t f23 = wasm_u16x8_shr(x23, 9);
    x01 = wasm_i16x8_add(x01, dx4);
    x23 = wasm_i16x8_add(x23, dx4);
    v128_t r0 = wasm_i16x8_add(
        wasm_i16x8_mul(wasm_u16x8_extend_low_u8x16(a), wasm_v128_xor(f01, k7f)),
        wasm_i16x8_mul(wasm_u16x8_extend_low_u8x16(b), f01));
    v128_t r1 =
        wasm_i16x8_add(wasm_i16x8_mul(wasm_u16x8_extend_high_u8x16(a),
                                      wasm_v128_xor(f23, k7f)),
                       wasm_i16x8_mul(wasm_u16x8_extend_high_u8x16(b), f23));
    wasm_v128_store(dst_argb, wasm_u8x16_narrow_i16x8(wasm_u16x8_shr(r0, 7),
                                                      wasm_u16x8_shr(r1, 7)));
    dst_argb += 16;
    dst_width -= 4;
  }
}
#endif  // HAS_SCALEARGBFILTERCOLS_WASMSIMD

#ifdef HAS_SCALEARGBCOLSUP2_WASMSIMD
// Point sample ARGB up by 2x.  8 pixels per loop.
void ScaleARGBColsUp2_WASMSIMD(uint8_t* dst_argb,
                               const uint8_t* src_argb,
                               int dst_width,
                               int x,
                               int dx) {
  (void)x;
  (void)dx;
  while (dst_width > 0) {
    // V8 lowers an Arm zip of a vector with itself to several instructions,
    // so zip with copies of the pixels loaded by load64_splat instead.
    v128_t s = wasm_v128_load(src_argb);
    v128_t t = wasm_v128_load64_splat(src_argb);
    v128_t u = wasm_v128_load64_splat(src_argb + 8);
    wasm_v128_store(dst_argb, wasm_i32x4_shuffle(s, t, 0, 4, 1, 5));
    wasm_v128_store(dst_argb + 16, wasm_i32x4_shuffle(s, u, 2, 6, 3, 7));
    src_argb += 16;
    dst_argb += 32;
    dst_width -= 8;
  }
}
#endif  // HAS_SCALEARGBCOLSUP2_WASMSIMD

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif

#endif  // !defined(LIBYUV_DISABLE_WASM) && defined(__wasm_simd128__)
