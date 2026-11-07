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

#if !defined(LIBYUV_DISABLE_WASM) && defined(__wasm_simd128__)
#include <wasm_simd128.h>

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

#ifdef __wasm_relaxed_simd__
#define LIBYUV_WASM_SWIZZLE wasm_i8x16_relaxed_swizzle
#else
#define LIBYUV_WASM_SWIZZLE wasm_i8x16_swizzle
#endif

// V8 materializes v128.const values and arbitrary i8x16.shuffle masks with
// several instructions at every use, including inside loops. These helpers
// make constants opaque to clang so they are built once, before the loop,
// and kept in registers. Use swizzle with a LoadConst() table instead of
// i8x16.shuffle unless the pattern maps to a native instruction (unpack,
// 32-bit lane shuffle, concat/palignr, even/odd byte unzip).
// Loops that access ptr + 16 etc. use asm("" : "+r"(ptr)) so LLVM keeps the
// constant offsets in the load/store instructions instead of creating extra
// pointer induction variables.
static inline v128_t LoadConst(const void* p) {
  asm("" : "+r"(p));
  return wasm_v128_load(p);
}
static inline v128_t SplatConst8(int v) {
  asm("" : "+r"(v));
  return wasm_i8x16_splat(v);
}
static inline v128_t SplatConst16(int v) {
  asm("" : "+r"(v));
  return wasm_i16x8_splat(v);
}
static inline v128_t SplatConst32(int v) {
  asm("" : "+r"(v));
  return wasm_i32x4_splat(v);
}

#ifdef HAS_RAWTORGB24ROW_WASMSIMD
static const uint8_t kRawToRGB24SwizzleTable[96] = {
    2,   1,   0,   5,   4,   3,   8,   7,   6,   11,  10,  9,   14,  13,  12,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 1,   0,   128, 4,   3,   2,   7,   6,   5,   10,  9,   8,   13,  12,
    11,  128, 15,  128, 1,   128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 2,   128, 128, 3,   2,   1,   6,   5,   4,   9,   8,   7,   12,
    11,  10,  15,  14,  13,  14,  128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128};

void RAWToRGB24Row_WASMSIMD(const uint8_t* src_raw,
                            uint8_t* dst_rgb24,
                            int width) {
  const uint8_t* shuf = kRawToRGB24SwizzleTable;
  // Load shuffle masks into Wasm locals outside the loop so Binaryen's
  // precompute-propagate pass does not sink v128.const into the loop.
  asm("" : "+r"(shuf));
  v128_t kShuf0_0 = wasm_v128_load(shuf);
  v128_t kShuf0_1 = wasm_v128_load(shuf + 16);
  v128_t kShuf1_1 = wasm_v128_load(shuf + 32);
  v128_t kShuf1_02 = wasm_v128_load(shuf + 48);
  v128_t kShuf2_2 = wasm_v128_load(shuf + 64);
  v128_t kShuf2_1 = wasm_v128_load(shuf + 80);

  while (width >= 32) {
    // Prevent LLVM LSR from splitting constant load/store offsets into separate
    // pointer induction variables.
    asm("" : "+r"(src_raw), "+r"(dst_rgb24));
    v128_t s0 = wasm_v128_load(src_raw);
    v128_t s1 = wasm_v128_load(src_raw + 16);
    v128_t s2 = wasm_v128_load(src_raw + 32);
    v128_t s3 = wasm_v128_load(src_raw + 48);
    v128_t s4 = wasm_v128_load(src_raw + 64);
    v128_t s5 = wasm_v128_load(src_raw + 80);

    v128_t s02 = wasm_i8x16_shuffle(s0, s2, 14, 15, 16, 17, 18, 19, 20, 21, 22,
                                    23, 24, 25, 26, 27, 28, 29);
    v128_t s35 = wasm_i8x16_shuffle(s3, s5, 14, 15, 16, 17, 18, 19, 20, 21, 22,
                                    23, 24, 25, 26, 27, 28, 29);

    v128_t d0 = wasm_v128_or(LIBYUV_WASM_SWIZZLE(s0, kShuf0_0),
                             LIBYUV_WASM_SWIZZLE(s1, kShuf0_1));
    v128_t d1 = wasm_v128_or(LIBYUV_WASM_SWIZZLE(s1, kShuf1_1),
                             LIBYUV_WASM_SWIZZLE(s02, kShuf1_02));
    v128_t d2 = wasm_v128_or(LIBYUV_WASM_SWIZZLE(s2, kShuf2_2),
                             LIBYUV_WASM_SWIZZLE(s1, kShuf2_1));
    v128_t d3 = wasm_v128_or(LIBYUV_WASM_SWIZZLE(s3, kShuf0_0),
                             LIBYUV_WASM_SWIZZLE(s4, kShuf0_1));
    v128_t d4 = wasm_v128_or(LIBYUV_WASM_SWIZZLE(s4, kShuf1_1),
                             LIBYUV_WASM_SWIZZLE(s35, kShuf1_02));
    v128_t d5 = wasm_v128_or(LIBYUV_WASM_SWIZZLE(s5, kShuf2_2),
                             LIBYUV_WASM_SWIZZLE(s4, kShuf2_1));

    wasm_v128_store(dst_rgb24, d0);
    wasm_v128_store(dst_rgb24 + 16, d1);
    wasm_v128_store(dst_rgb24 + 32, d2);
    wasm_v128_store(dst_rgb24 + 48, d3);
    wasm_v128_store(dst_rgb24 + 64, d4);
    wasm_v128_store(dst_rgb24 + 80, d5);

    src_raw += 96;
    dst_rgb24 += 96;
    width -= 32;
  }
  if (width > 0) {
    asm("" : "+r"(src_raw), "+r"(dst_rgb24));
    v128_t s0 = wasm_v128_load(src_raw);
    v128_t s1 = wasm_v128_load(src_raw + 16);
    v128_t s2 = wasm_v128_load(src_raw + 32);

    v128_t s02 = wasm_i8x16_shuffle(s0, s2, 14, 15, 16, 17, 18, 19, 20, 21, 22,
                                    23, 24, 25, 26, 27, 28, 29);

    v128_t d0 = wasm_v128_or(LIBYUV_WASM_SWIZZLE(s0, kShuf0_0),
                             LIBYUV_WASM_SWIZZLE(s1, kShuf0_1));
    v128_t d1 = wasm_v128_or(LIBYUV_WASM_SWIZZLE(s1, kShuf1_1),
                             LIBYUV_WASM_SWIZZLE(s02, kShuf1_02));
    v128_t d2 = wasm_v128_or(LIBYUV_WASM_SWIZZLE(s2, kShuf2_2),
                             LIBYUV_WASM_SWIZZLE(s1, kShuf2_1));

    wasm_v128_store(dst_rgb24, d0);
    wasm_v128_store(dst_rgb24 + 16, d1);
    wasm_v128_store(dst_rgb24 + 32, d2);
  }
}
#endif  // HAS_RAWTORGB24ROW_WASMSIMD

#if defined(HAS_RAWTOARGBROW_WASMSIMD) || defined(HAS_RGB24TOARGBROW_WASMSIMD)
// 16 pixels of 3 bytes to 4 bytes. shuf holds 2 masks: one for loads at
// offsets 0, 12, 24 and one for the load at offset 32 (avoids over-read).
static void RGB3ToARGBRow_WASMSIMD(const uint8_t* src,
                                   uint8_t* dst_argb,
                                   int width,
                                   const uint8_t* shuf) {
  v128_t m0 = LoadConst(shuf);
  v128_t m1 = LoadConst(shuf + 16);
  v128_t alpha = SplatConst32((int)0xff000000);
  do {
    asm("" : "+r"(src), "+r"(dst_argb));
    v128_t d0 = LIBYUV_WASM_SWIZZLE(wasm_v128_load(src), m0);
    v128_t d1 = LIBYUV_WASM_SWIZZLE(wasm_v128_load(src + 12), m0);
    v128_t d2 = LIBYUV_WASM_SWIZZLE(wasm_v128_load(src + 24), m0);
    v128_t d3 = LIBYUV_WASM_SWIZZLE(wasm_v128_load(src + 32), m1);
    wasm_v128_store(dst_argb, wasm_v128_or(d0, alpha));
    wasm_v128_store(dst_argb + 16, wasm_v128_or(d1, alpha));
    wasm_v128_store(dst_argb + 32, wasm_v128_or(d2, alpha));
    wasm_v128_store(dst_argb + 48, wasm_v128_or(d3, alpha));
    src += 48;
    dst_argb += 64;
    width -= 16;
  } while (width > 0);
}
#endif

#ifdef HAS_RAWTOARGBROW_WASMSIMD
static const uint8_t kShuffleRAWToARGB[32] = {
    2, 1, 0, 128, 5, 4, 3, 128, 8,  7,  6,  128, 11, 10, 9,  128,
    6, 5, 4, 128, 9, 8, 7, 128, 12, 11, 10, 128, 15, 14, 13, 128};
void RAWToARGBRow_WASMSIMD(const uint8_t* src_raw,
                           uint8_t* dst_argb,
                           int width) {
  RGB3ToARGBRow_WASMSIMD(src_raw, dst_argb, width, kShuffleRAWToARGB);
}
#endif  // HAS_RAWTOARGBROW_WASMSIMD

#ifdef HAS_RGB24TOARGBROW_WASMSIMD
static const uint8_t kShuffleRGB24ToARGB[32] = {
    0, 1, 2, 128, 3, 4, 5, 128, 6,  7,  8,  128, 9,  10, 11, 128,
    4, 5, 6, 128, 7, 8, 9, 128, 10, 11, 12, 128, 13, 14, 15, 128};
void RGB24ToARGBRow_WASMSIMD(const uint8_t* src_rgb24,
                             uint8_t* dst_argb,
                             int width) {
  RGB3ToARGBRow_WASMSIMD(src_rgb24, dst_argb, width, kShuffleRGB24ToARGB);
}
#endif  // HAS_RGB24TOARGBROW_WASMSIMD

#if defined(HAS_ARGBTOYMATRIXROW_WASMSIMD) ||      \
    defined(HAS_ARGBTOUV444MATRIXROW_WASMSIMD) ||  \
    defined(HAS_ARGBTOUVMATRIXROW_WASMSIMD) ||     \
    defined(HAS_RGBTOYMATRIXROW_WASMSIMD) ||       \
    defined(HAS_RGBTOUV444MATRIXROW_WASMSIMD) ||   \
    defined(HAS_RGBTOUVMATRIXROW_WASMSIMD) ||      \
    defined(HAS_RGB565TOYMATRIXROW_WASMSIMD) ||    \
    defined(HAS_ARGB1555TOYMATRIXROW_WASMSIMD) ||  \
    defined(HAS_ARGB4444TOYMATRIXROW_WASMSIMD) ||  \
    defined(HAS_RGB565TOUVMATRIXROW_WASMSIMD) ||   \
    defined(HAS_ARGB1555TOUVMATRIXROW_WASMSIMD) || \
    defined(HAS_ARGB4444TOUVMATRIXROW_WASMSIMD)
// RGB to YUV. Math is in 16 bit lanes, which wrap, but sum + bias fits in
// 16 bits so the results match the C code. i32x4.dot_i16x8_s would be best
// on x86 but is 3 instructions on Arm, so i16x8.mul (mul + mla on Arm) and
// horizontal adds that are cheap on both are used instead.

// lo * k0 + hi * k1.
static inline v128_t MulAdd16(v128_t lo, v128_t hi, v128_t k0, v128_t k1) {
  return wasm_i16x8_add(wasm_i16x8_mul(lo, k0), wasm_i16x8_mul(hi, k1));
}

// Sums adjacent 16 bit lanes of t0 and t1.
static inline v128_t HAddPairs16(v128_t t0, v128_t t1) {
  return wasm_i16x8_add(wasm_i16x8_shuffle(t0, t1, 0, 2, 4, 6, 8, 10, 12, 14),
                        wasm_i16x8_shuffle(t0, t1, 1, 3, 5, 7, 9, 11, 13, 15));
}

// Splats 16 bit {lo, hi} into each 32 bit lane.
static inline v128_t SplatPair16(int lo, int hi) {
  return SplatConst32((lo & 0xffff) | (hi << 16));
}

// (bias - sum) >> 8 for U and V.
static inline v128_t SubShr8(v128_t bias, v128_t sum) {
  return wasm_u16x8_shr(wasm_i16x8_sub(bias, sum), 8);
}
#endif

#if defined(HAS_ARGBTOUV444MATRIXROW_WASMSIMD) || \
    defined(HAS_RGBTOYMATRIXROW_WASMSIMD) ||      \
    defined(HAS_RGBTOUV444MATRIXROW_WASMSIMD)
// Swizzles 4 pixels to 16 bit {c0 x4, c2 x4} and {c1 x4, c3 x4}, so the 2
// halves of the products are summed with a 64 bit interleave.
static const uint8_t kShuffleARGBToPlanar[32] = {
    0, 128, 4, 128, 8, 128, 12, 128, 2, 128, 6, 128, 10, 128, 14, 128,
    1, 128, 5, 128, 9, 128, 13, 128, 3, 128, 7, 128, 11, 128, 15, 128};
// RGB24 to {b x4, r x4} and {g x4, 0 x4}. The second pair of masks is for
// the load at offset 32 (avoids over-read).
static const uint8_t kShuffleRGBToPlanar[64] = {
    0, 128, 3, 128, 6,  128, 9,  128, 2,   128, 5,   128, 8,   128, 11,  128,
    1, 128, 4, 128, 7,  128, 10, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    4, 128, 7, 128, 10, 128, 13, 128, 6,   128, 9,   128, 12,  128, 15,  128,
    5, 128, 8, 128, 11, 128, 14, 128, 128, 128, 128, 128, 128, 128, 128, 128};

// {a x4, b x4} as 16 bit lanes.
static inline v128_t Splat4x2(int a, int b) {
  asm("" : "+r"(a), "+r"(b));
  return wasm_i16x8_make(a, a, a, a, b, b, b, b);
}

// Sums the 64 bit halves of t0 and t1.
static inline v128_t HAddHalves64(v128_t t0, v128_t t1) {
  return wasm_i16x8_add(wasm_i64x2_shuffle(t0, t1, 0, 2),
                        wasm_i64x2_shuffle(t0, t1, 1, 3));
}

// 16 U and V from 16 pixels swizzled by m (m[0], m[1] for the first 3
// vectors, m[2], m[3] for the last).
static inline void PlanarToUV16(const v128_t* s,
                                const v128_t* m,
                                v128_t ku02,
                                v128_t ku13,
                                v128_t kv02,
                                v128_t kv13,
                                v128_t bias_u,
                                v128_t bias_v,
                                uint8_t* dst_u,
                                uint8_t* dst_v) {
  v128_t u[4], v[4];
  for (int i = 0; i < 4; ++i) {
    v128_t lo = LIBYUV_WASM_SWIZZLE(s[i], m[i == 3 ? 2 : 0]);
    v128_t hi = LIBYUV_WASM_SWIZZLE(s[i], m[i == 3 ? 3 : 1]);
    u[i] = MulAdd16(lo, hi, ku02, ku13);
    v[i] = MulAdd16(lo, hi, kv02, kv13);
  }
  wasm_v128_store(dst_u, wasm_u8x16_narrow_i16x8(
                             SubShr8(bias_u, HAddHalves64(u[0], u[1])),
                             SubShr8(bias_u, HAddHalves64(u[2], u[3]))));
  wasm_v128_store(dst_v, wasm_u8x16_narrow_i16x8(
                             SubShr8(bias_v, HAddHalves64(v[0], v[1])),
                             SubShr8(bias_v, HAddHalves64(v[2], v[3]))));
}
#endif

#if defined(HAS_ARGBTOUVMATRIXROW_WASMSIMD) || \
    defined(HAS_RGBTOUVMATRIXROW_WASMSIMD)
// Rounded 2x2 average of 8 pixels held as 16 bit {c0, c2} or {c1, c3}
// pairs. a = row 0, b = row 1, 4 pixels per vector. Returns 4 pixels.
static inline v128_t Avg2x2Pairs(v128_t a0,
                                 v128_t a1,
                                 v128_t b0,
                                 v128_t b1,
                                 v128_t two) {
  v128_t s0 = wasm_i16x8_add(a0, b0);
  v128_t s1 = wasm_i16x8_add(a1, b1);
  v128_t s = wasm_i16x8_add(wasm_i32x4_shuffle(s0, s1, 0, 2, 4, 6),
                            wasm_i32x4_shuffle(s0, s1, 1, 3, 5, 7));
  return wasm_u16x8_shr(wasm_i16x8_add(s, two), 2);
}

// Stores 8 U and 8 V from 2 vectors each of {c0, c2} and {c1, c3} pairs.
static inline void PairsToUV8(const v128_t* lo,
                              const v128_t* hi,
                              v128_t ku02,
                              v128_t ku13,
                              v128_t kv02,
                              v128_t kv13,
                              v128_t bias_u,
                              v128_t bias_v,
                              uint8_t* dst_u,
                              uint8_t* dst_v) {
  v128_t u = HAddPairs16(MulAdd16(lo[0], hi[0], ku02, ku13),
                         MulAdd16(lo[1], hi[1], ku02, ku13));
  v128_t v = HAddPairs16(MulAdd16(lo[0], hi[0], kv02, kv13),
                         MulAdd16(lo[1], hi[1], kv02, kv13));
  v128_t uv = wasm_u8x16_narrow_i16x8(SubShr8(bias_u, u), SubShr8(bias_v, v));
  wasm_v128_store64_lane(dst_u, uv, 0);
  wasm_v128_store64_lane(dst_v, uv, 1);
}
#endif

#ifdef HAS_ARGBTOYMATRIXROW_WASMSIMD
void ARGBToYMatrixRow_WASMSIMD(const uint8_t* src_argb,
                               uint8_t* dst_y,
                               int width,
                               const struct ArgbConstants* c) {
  v128_t mask = SplatConst16(0xff);
  v128_t k02 = SplatPair16(c->kRGBToY[0], c->kRGBToY[2]);
  v128_t k13 = SplatPair16(c->kRGBToY[1], c->kRGBToY[3]);
  v128_t bias = SplatConst16(c->kAddY[0]);
  do {
    v128_t t[4];
    for (int i = 0; i < 4; ++i) {
      v128_t s = wasm_v128_load(src_argb + i * 16);
      t[i] = MulAdd16(wasm_v128_and(s, mask), wasm_u16x8_shr(s, 8), k02, k13);
    }
    v128_t y0 = wasm_i16x8_add(HAddPairs16(t[0], t[1]), bias);
    v128_t y1 = wasm_i16x8_add(HAddPairs16(t[2], t[3]), bias);
    wasm_v128_store(dst_y, wasm_u8x16_narrow_i16x8(wasm_u16x8_shr(y0, 8),
                                                   wasm_u16x8_shr(y1, 8)));
    src_argb += 64;
    dst_y += 16;
    width -= 16;
  } while (width > 0);
}
#endif  // HAS_ARGBTOYMATRIXROW_WASMSIMD

#ifdef HAS_RGBTOYMATRIXROW_WASMSIMD
// RGB24 is alpha 255 in the C version, so 255 * kRGBToY[3] is in the bias.
void RGBToYMatrixRow_WASMSIMD(const uint8_t* src_rgb,
                              uint8_t* dst_y,
                              int width,
                              const struct ArgbConstants* c) {
  v128_t m[4];
  for (int i = 0; i < 4; ++i) {
    m[i] = LoadConst(kShuffleRGBToPlanar + i * 16);
  }
  v128_t k02 = Splat4x2(c->kRGBToY[0], c->kRGBToY[2]);
  v128_t k13 = Splat4x2(c->kRGBToY[1], 0);
  v128_t bias = SplatConst16(c->kAddY[0] + 255 * c->kRGBToY[3]);
  do {
    v128_t t[4];
    for (int i = 0; i < 4; ++i) {
      v128_t s = wasm_v128_load(src_rgb + (i == 3 ? 32 : i * 12));
      t[i] = MulAdd16(LIBYUV_WASM_SWIZZLE(s, m[i == 3 ? 2 : 0]),
                      LIBYUV_WASM_SWIZZLE(s, m[i == 3 ? 3 : 1]), k02, k13);
    }
    v128_t y0 = wasm_i16x8_add(HAddHalves64(t[0], t[1]), bias);
    v128_t y1 = wasm_i16x8_add(HAddHalves64(t[2], t[3]), bias);
    wasm_v128_store(dst_y, wasm_u8x16_narrow_i16x8(wasm_u16x8_shr(y0, 8),
                                                   wasm_u16x8_shr(y1, 8)));
    src_rgb += 48;
    dst_y += 16;
    width -= 16;
  } while (width > 0);
}
#endif  // HAS_RGBTOYMATRIXROW_WASMSIMD

#ifdef HAS_ARGBTOUV444MATRIXROW_WASMSIMD
void ARGBToUV444MatrixRow_WASMSIMD(const uint8_t* src_argb,
                                   uint8_t* dst_u,
                                   uint8_t* dst_v,
                                   int width,
                                   const struct ArgbConstants* c) {
  v128_t m[4];
  m[0] = m[2] = LoadConst(kShuffleARGBToPlanar);
  m[1] = m[3] = LoadConst(kShuffleARGBToPlanar + 16);
  v128_t ku02 = Splat4x2(c->kRGBToU[0], c->kRGBToU[2]);
  v128_t ku13 = Splat4x2(c->kRGBToU[1], c->kRGBToU[3]);
  v128_t kv02 = Splat4x2(c->kRGBToV[0], c->kRGBToV[2]);
  v128_t kv13 = Splat4x2(c->kRGBToV[1], c->kRGBToV[3]);
  v128_t bias = SplatConst16(c->kAddUV[0]);
  do {
    v128_t s[4];
    for (int i = 0; i < 4; ++i) {
      s[i] = wasm_v128_load(src_argb + i * 16);
    }
    PlanarToUV16(s, m, ku02, ku13, kv02, kv13, bias, bias, dst_u, dst_v);
    src_argb += 64;
    dst_u += 16;
    dst_v += 16;
    width -= 16;
  } while (width > 0);
}
#endif  // HAS_ARGBTOUV444MATRIXROW_WASMSIMD

#ifdef HAS_RGBTOUV444MATRIXROW_WASMSIMD
void RGBToUV444MatrixRow_WASMSIMD(const uint8_t* src_rgb,
                                  uint8_t* dst_u,
                                  uint8_t* dst_v,
                                  int width,
                                  const struct ArgbConstants* c) {
  v128_t m[4];
  for (int i = 0; i < 4; ++i) {
    m[i] = LoadConst(kShuffleRGBToPlanar + i * 16);
  }
  v128_t ku02 = Splat4x2(c->kRGBToU[0], c->kRGBToU[2]);
  v128_t ku13 = Splat4x2(c->kRGBToU[1], 0);
  v128_t kv02 = Splat4x2(c->kRGBToV[0], c->kRGBToV[2]);
  v128_t kv13 = Splat4x2(c->kRGBToV[1], 0);
  v128_t bias_u = SplatConst16(c->kAddUV[0] - 255 * c->kRGBToU[3]);
  v128_t bias_v = SplatConst16(c->kAddUV[0] - 255 * c->kRGBToV[3]);
  do {
    v128_t s[4];
    for (int i = 0; i < 4; ++i) {
      s[i] = wasm_v128_load(src_rgb + (i == 3 ? 32 : i * 12));
    }
    PlanarToUV16(s, m, ku02, ku13, kv02, kv13, bias_u, bias_v, dst_u, dst_v);
    src_rgb += 48;
    dst_u += 16;
    dst_v += 16;
    width -= 16;
  } while (width > 0);
}
#endif  // HAS_RGBTOUV444MATRIXROW_WASMSIMD

#ifdef HAS_ARGBTOUVMATRIXROW_WASMSIMD
void ARGBToUVMatrixRow_WASMSIMD(const uint8_t* src_argb,
                                int src_stride_argb,
                                uint8_t* dst_u,
                                uint8_t* dst_v,
                                int width,
                                const struct ArgbConstants* c) {
  const uint8_t* src_argb1 = src_argb + src_stride_argb;
  v128_t mask = SplatConst16(0xff);
  v128_t two = SplatConst16(2);
  v128_t ku02 = SplatPair16(c->kRGBToU[0], c->kRGBToU[2]);
  v128_t ku13 = SplatPair16(c->kRGBToU[1], c->kRGBToU[3]);
  v128_t kv02 = SplatPair16(c->kRGBToV[0], c->kRGBToV[2]);
  v128_t kv13 = SplatPair16(c->kRGBToV[1], c->kRGBToV[3]);
  v128_t bias = SplatConst16(c->kAddUV[0]);
  do {
    v128_t lo[2], hi[2];
    for (int i = 0; i < 2; ++i) {
      v128_t a0 = wasm_v128_load(src_argb + i * 32);
      v128_t a1 = wasm_v128_load(src_argb + i * 32 + 16);
      v128_t b0 = wasm_v128_load(src_argb1 + i * 32);
      v128_t b1 = wasm_v128_load(src_argb1 + i * 32 + 16);
      lo[i] =
          Avg2x2Pairs(wasm_v128_and(a0, mask), wasm_v128_and(a1, mask),
                      wasm_v128_and(b0, mask), wasm_v128_and(b1, mask), two);
      hi[i] = Avg2x2Pairs(wasm_u16x8_shr(a0, 8), wasm_u16x8_shr(a1, 8),
                          wasm_u16x8_shr(b0, 8), wasm_u16x8_shr(b1, 8), two);
    }
    PairsToUV8(lo, hi, ku02, ku13, kv02, kv13, bias, bias, dst_u, dst_v);
    src_argb += 64;
    src_argb1 += 64;
    dst_u += 8;
    dst_v += 8;
    width -= 16;
  } while (width > 0);
}
#endif  // HAS_ARGBTOUVMATRIXROW_WASMSIMD

#ifdef HAS_RGBTOUVMATRIXROW_WASMSIMD
// Swizzles 4 RGB24 pixels to 16 bit {b, r} and {g, 0} pairs. The second
// pair of masks is for the load at offset 32 (avoids over-read).
static const uint8_t kShuffleRGBToPairs[64] = {
    0, 128, 2,   128, 3, 128, 5,   128, 6,  128, 8,   128, 9,  128, 11,  128,
    1, 128, 128, 128, 4, 128, 128, 128, 7,  128, 128, 128, 10, 128, 128, 128,
    4, 128, 6,   128, 7, 128, 9,   128, 10, 128, 12,  128, 13, 128, 15,  128,
    5, 128, 128, 128, 8, 128, 128, 128, 11, 128, 128, 128, 14, 128, 128, 128};

void RGBToUVMatrixRow_WASMSIMD(const uint8_t* src_rgb,
                               int src_stride_rgb,
                               uint8_t* dst_u,
                               uint8_t* dst_v,
                               int width,
                               const struct ArgbConstants* c) {
  const uint8_t* src_rgb1 = src_rgb + src_stride_rgb;
  v128_t m[4];
  for (int i = 0; i < 4; ++i) {
    m[i] = LoadConst(kShuffleRGBToPairs + i * 16);
  }
  v128_t two = SplatConst16(2);
  v128_t ku02 = SplatPair16(c->kRGBToU[0], c->kRGBToU[2]);
  v128_t ku13 = SplatPair16(c->kRGBToU[1], 0);
  v128_t kv02 = SplatPair16(c->kRGBToV[0], c->kRGBToV[2]);
  v128_t kv13 = SplatPair16(c->kRGBToV[1], 0);
  v128_t bias_u = SplatConst16(c->kAddUV[0] - 255 * c->kRGBToU[3]);
  v128_t bias_v = SplatConst16(c->kAddUV[0] - 255 * c->kRGBToV[3]);
  do {
    v128_t lo[2], hi[2];
    for (int i = 0; i < 2; ++i) {
      // Pixels 8 * i to 8 * i + 3 at offset 24 * i, the rest at 12 or 32.
      int off = i ? 32 : 12;
      v128_t a0 = wasm_v128_load(src_rgb + i * 24);
      v128_t a1 = wasm_v128_load(src_rgb + off);
      v128_t b0 = wasm_v128_load(src_rgb1 + i * 24);
      v128_t b1 = wasm_v128_load(src_rgb1 + off);
      v128_t ml = m[i * 2];
      v128_t mh = m[i * 2 + 1];
      lo[i] = Avg2x2Pairs(
          LIBYUV_WASM_SWIZZLE(a0, m[0]), LIBYUV_WASM_SWIZZLE(a1, ml),
          LIBYUV_WASM_SWIZZLE(b0, m[0]), LIBYUV_WASM_SWIZZLE(b1, ml), two);
      hi[i] = Avg2x2Pairs(
          LIBYUV_WASM_SWIZZLE(a0, m[1]), LIBYUV_WASM_SWIZZLE(a1, mh),
          LIBYUV_WASM_SWIZZLE(b0, m[1]), LIBYUV_WASM_SWIZZLE(b1, mh), two);
    }
    PairsToUV8(lo, hi, ku02, ku13, kv02, kv13, bias_u, bias_v, dst_u, dst_v);
    src_rgb += 48;
    src_rgb1 += 48;
    dst_u += 8;
    dst_v += 8;
    width -= 16;
  } while (width > 0);
}
#endif  // HAS_RGBTOUVMATRIXROW_WASMSIMD

// 16 bit pixel formats keep each channel in its own 16 bit lane.
// 5 and 6 bit channels expand as (v * 33) >> 2 and (v * 65) >> 4, and 4 bit
// channels as v * 17, which match the C bit replication.

#if defined(HAS_RGB565TOUVMATRIXROW_WASMSIMD) ||   \
    defined(HAS_ARGB1555TOUVMATRIXROW_WASMSIMD) || \
    defined(HAS_ARGB4444TOUVMATRIXROW_WASMSIMD)
// Rounded 2x2 average of 16 pixels of 1 channel. a = row 0, b = row 1.
static inline v128_t Avg2x2Lanes(v128_t a0,
                                 v128_t a1,
                                 v128_t b0,
                                 v128_t b1,
                                 v128_t two) {
  v128_t s = HAddPairs16(wasm_i16x8_add(a0, b0), wasm_i16x8_add(a1, b1));
  return wasm_u16x8_shr(wasm_i16x8_add(s, two), 2);
}

// Stores 8 U and 8 V from 8 pixels of 16 bit channels. k holds U
// coefficients for b, g, r, a, then V coefficients, then bias U and V.
static inline void ChannelsToUV8(v128_t b,
                                 v128_t g,
                                 v128_t r,
                                 v128_t a,
                                 const v128_t* k,
                                 uint8_t* dst_u,
                                 uint8_t* dst_v) {
  v128_t u =
      wasm_i16x8_add(MulAdd16(b, g, k[0], k[1]), MulAdd16(r, a, k[2], k[3]));
  v128_t v =
      wasm_i16x8_add(MulAdd16(b, g, k[4], k[5]), MulAdd16(r, a, k[6], k[7]));
  v128_t uv = wasm_u8x16_narrow_i16x8(SubShr8(k[8], u), SubShr8(k[9], v));
  wasm_v128_store64_lane(dst_u, uv, 0);
  wasm_v128_store64_lane(dst_v, uv, 1);
}

// U and V coefficients for ChannelsToUV8. The alpha term is folded into the
// bias if alpha is always 255 (opaque).
static inline void LoadUVChannelConstants(const struct ArgbConstants* c,
                                          int opaque,
                                          v128_t* k) {
  for (int i = 0; i < 4; ++i) {
    k[i] = SplatConst16(c->kRGBToU[i]);
    k[i + 4] = SplatConst16(c->kRGBToV[i]);
  }
  k[8] = SplatConst16(c->kAddUV[0] - opaque * 255 * c->kRGBToU[3]);
  k[9] = SplatConst16(c->kAddUV[0] - opaque * 255 * c->kRGBToV[3]);
}
#endif
#ifdef HAS_RGB565TOYMATRIXROW_WASMSIMD
void RGB565ToYMatrixRow_WASMSIMD(const uint8_t* src_rgb565,
                                 uint8_t* dst_y,
                                 int width,
                                 const struct ArgbConstants* c) {
  v128_t m5 = SplatConst16(0x1f);
  v128_t m6 = SplatConst16(0x3f);
  v128_t k33 = SplatConst16(33);
  v128_t k65 = SplatConst16(65);
  v128_t kb = SplatConst16(c->kRGBToY[0]);
  v128_t kg = SplatConst16(c->kRGBToY[1]);
  v128_t kr = SplatConst16(c->kRGBToY[2]);
  v128_t bias = SplatConst16(c->kAddY[0] + 255 * c->kRGBToY[3]);
  do {
    v128_t y[2];
    for (int i = 0; i < 2; ++i) {
      v128_t x = wasm_v128_load(src_rgb565 + i * 16);
      v128_t b = wasm_u16x8_shr(wasm_i16x8_mul(wasm_v128_and(x, m5), k33), 2);
      v128_t g = wasm_u16x8_shr(
          wasm_i16x8_mul(wasm_v128_and(wasm_u16x8_shr(x, 5), m6), k65), 4);
      v128_t r = wasm_u16x8_shr(wasm_i16x8_mul(wasm_u16x8_shr(x, 11), k33), 2);
      v128_t s = wasm_i16x8_add(wasm_i16x8_mul(b, kb), bias);
      y[i] = wasm_u16x8_shr(wasm_i16x8_add(s, MulAdd16(g, r, kg, kr)), 8);
    }
    wasm_v128_store(dst_y, wasm_u8x16_narrow_i16x8(y[0], y[1]));
    src_rgb565 += 32;
    dst_y += 16;
    width -= 16;
  } while (width > 0);
}
#endif  // HAS_RGB565TOYMATRIXROW_WASMSIMD

#ifdef HAS_ARGB1555TOYMATRIXROW_WASMSIMD
void ARGB1555ToYMatrixRow_WASMSIMD(const uint8_t* src_argb1555,
                                   uint8_t* dst_y,
                                   int width,
                                   const struct ArgbConstants* c) {
  v128_t m5 = SplatConst16(0x1f);
  v128_t k33 = SplatConst16(33);
  v128_t kb = SplatConst16(c->kRGBToY[0]);
  v128_t kg = SplatConst16(c->kRGBToY[1]);
  v128_t kr = SplatConst16(c->kRGBToY[2]);
  v128_t ka = SplatConst16(255 * c->kRGBToY[3]);
  v128_t bias = SplatConst16(c->kAddY[0]);
  do {
    v128_t y[2];
    for (int i = 0; i < 2; ++i) {
      v128_t x = wasm_v128_load(src_argb1555 + i * 16);
      v128_t b = wasm_u16x8_shr(wasm_i16x8_mul(wasm_v128_and(x, m5), k33), 2);
      v128_t g = wasm_u16x8_shr(
          wasm_i16x8_mul(wasm_v128_and(wasm_u16x8_shr(x, 5), m5), k33), 2);
      v128_t r = wasm_u16x8_shr(
          wasm_i16x8_mul(wasm_v128_and(wasm_u16x8_shr(x, 10), m5), k33), 2);
      // Alpha is 0 or 255.
      v128_t s = wasm_i16x8_add(wasm_v128_and(wasm_i16x8_shr(x, 15), ka), bias);
      s = wasm_i16x8_add(s, wasm_i16x8_mul(b, kb));
      y[i] = wasm_u16x8_shr(wasm_i16x8_add(s, MulAdd16(g, r, kg, kr)), 8);
    }
    wasm_v128_store(dst_y, wasm_u8x16_narrow_i16x8(y[0], y[1]));
    src_argb1555 += 32;
    dst_y += 16;
    width -= 16;
  } while (width > 0);
}
#endif  // HAS_ARGB1555TOYMATRIXROW_WASMSIMD

#ifdef HAS_ARGB4444TOYMATRIXROW_WASMSIMD
// 4 bit channels expand as v * 17, so the coefficients are scaled by 17.
void ARGB4444ToYMatrixRow_WASMSIMD(const uint8_t* src_argb4444,
                                   uint8_t* dst_y,
                                   int width,
                                   const struct ArgbConstants* c) {
  v128_t m4 = SplatConst16(0x0f);
  v128_t kb = SplatConst16(c->kRGBToY[0] * 17);
  v128_t kg = SplatConst16(c->kRGBToY[1] * 17);
  v128_t kr = SplatConst16(c->kRGBToY[2] * 17);
  v128_t ka = SplatConst16(c->kRGBToY[3] * 17);
  v128_t bias = SplatConst16(c->kAddY[0]);
  do {
    v128_t y[2];
    for (int i = 0; i < 2; ++i) {
      v128_t x = wasm_v128_load(src_argb4444 + i * 16);
      v128_t s = wasm_i16x8_add(
          MulAdd16(wasm_v128_and(x, m4), wasm_u16x8_shr(x, 12), kb, ka), bias);
      s = wasm_i16x8_add(
          s, MulAdd16(wasm_v128_and(wasm_u16x8_shr(x, 4), m4),
                      wasm_v128_and(wasm_u16x8_shr(x, 8), m4), kg, kr));
      y[i] = wasm_u16x8_shr(s, 8);
    }
    wasm_v128_store(dst_y, wasm_u8x16_narrow_i16x8(y[0], y[1]));
    src_argb4444 += 32;
    dst_y += 16;
    width -= 16;
  } while (width > 0);
}
#endif  // HAS_ARGB4444TOYMATRIXROW_WASMSIMD

#ifdef HAS_RGB565TOUVMATRIXROW_WASMSIMD
void RGB565ToUVMatrixRow_WASMSIMD(const uint8_t* src_rgb565,
                                  int src_stride_rgb565,
                                  uint8_t* dst_u,
                                  uint8_t* dst_v,
                                  int width,
                                  const struct ArgbConstants* c) {
  const uint8_t* src_rgb565_1 = src_rgb565 + src_stride_rgb565;
  v128_t m5 = SplatConst16(0x1f);
  v128_t m6 = SplatConst16(0x3f);
  v128_t k33 = SplatConst16(33);
  v128_t k65 = SplatConst16(65);
  v128_t two = SplatConst16(2);
  v128_t k[10];
  LoadUVChannelConstants(c, 1, k);
  do {
    v128_t b[4], g[4], r[4];
    for (int i = 0; i < 4; ++i) {
      v128_t x =
          wasm_v128_load((i & 2 ? src_rgb565_1 : src_rgb565) + (i & 1) * 16);
      b[i] = wasm_u16x8_shr(wasm_i16x8_mul(wasm_v128_and(x, m5), k33), 2);
      g[i] = wasm_u16x8_shr(
          wasm_i16x8_mul(wasm_v128_and(wasm_u16x8_shr(x, 5), m6), k65), 4);
      r[i] = wasm_u16x8_shr(wasm_i16x8_mul(wasm_u16x8_shr(x, 11), k33), 2);
    }
    ChannelsToUV8(Avg2x2Lanes(b[0], b[1], b[2], b[3], two),
                  Avg2x2Lanes(g[0], g[1], g[2], g[3], two),
                  Avg2x2Lanes(r[0], r[1], r[2], r[3], two),
                  wasm_i16x8_const_splat(0), k, dst_u, dst_v);
    src_rgb565 += 32;
    src_rgb565_1 += 32;
    dst_u += 8;
    dst_v += 8;
    width -= 16;
  } while (width > 0);
}
#endif  // HAS_RGB565TOUVMATRIXROW_WASMSIMD

#ifdef HAS_ARGB1555TOUVMATRIXROW_WASMSIMD
void ARGB1555ToUVMatrixRow_WASMSIMD(const uint8_t* src_argb1555,
                                    int src_stride_argb1555,
                                    uint8_t* dst_u,
                                    uint8_t* dst_v,
                                    int width,
                                    const struct ArgbConstants* c) {
  const uint8_t* src_argb1555_1 = src_argb1555 + src_stride_argb1555;
  v128_t m5 = SplatConst16(0x1f);
  v128_t k33 = SplatConst16(33);
  v128_t two = SplatConst16(2);
  v128_t k[10];
  LoadUVChannelConstants(c, 0, k);
  do {
    v128_t b[4], g[4], r[4], a[4];
    for (int i = 0; i < 4; ++i) {
      v128_t x = wasm_v128_load((i & 2 ? src_argb1555_1 : src_argb1555) +
                                (i & 1) * 16);
      b[i] = wasm_u16x8_shr(wasm_i16x8_mul(wasm_v128_and(x, m5), k33), 2);
      g[i] = wasm_u16x8_shr(
          wasm_i16x8_mul(wasm_v128_and(wasm_u16x8_shr(x, 5), m5), k33), 2);
      r[i] = wasm_u16x8_shr(
          wasm_i16x8_mul(wasm_v128_and(wasm_u16x8_shr(x, 10), m5), k33), 2);
      a[i] = wasm_u16x8_shr(wasm_i16x8_shr(x, 15), 8);
    }
    ChannelsToUV8(Avg2x2Lanes(b[0], b[1], b[2], b[3], two),
                  Avg2x2Lanes(g[0], g[1], g[2], g[3], two),
                  Avg2x2Lanes(r[0], r[1], r[2], r[3], two),
                  Avg2x2Lanes(a[0], a[1], a[2], a[3], two), k, dst_u, dst_v);
    src_argb1555 += 32;
    src_argb1555_1 += 32;
    dst_u += 8;
    dst_v += 8;
    width -= 16;
  } while (width > 0);
}
#endif  // HAS_ARGB1555TOUVMATRIXROW_WASMSIMD

#ifdef HAS_ARGB4444TOUVMATRIXROW_WASMSIMD
// Sums the nibble pairs {b, r} and {g, a} of 2x2 pixels in bytes, then
// expands the sums by 17 before rounding.
void ARGB4444ToUVMatrixRow_WASMSIMD(const uint8_t* src_argb4444,
                                    int src_stride_argb4444,
                                    uint8_t* dst_u,
                                    uint8_t* dst_v,
                                    int width,
                                    const struct ArgbConstants* c) {
  const uint8_t* src_argb4444_1 = src_argb4444 + src_stride_argb4444;
  v128_t m4 = SplatConst16(0x0f0f);
  v128_t m8 = SplatConst16(0xff);
  v128_t k17 = SplatConst16(17);
  v128_t two = SplatConst16(2);
  v128_t k[10];
  LoadUVChannelConstants(c, 0, k);
  do {
    v128_t s[2], t[2];
    for (int i = 0; i < 2; ++i) {
      v128_t a = wasm_v128_load(src_argb4444 + i * 16);
      v128_t b = wasm_v128_load(src_argb4444_1 + i * 16);
      s[i] = wasm_i16x8_add(wasm_v128_and(a, m4), wasm_v128_and(b, m4));
      t[i] = wasm_i16x8_add(wasm_v128_and(wasm_u16x8_shr(a, 4), m4),
                            wasm_v128_and(wasm_u16x8_shr(b, 4), m4));
    }
    v128_t br = HAddPairs16(s[0], s[1]);
    v128_t ga = HAddPairs16(t[0], t[1]);
    v128_t ch[4] = {wasm_v128_and(br, m8), wasm_v128_and(ga, m8),
                    wasm_u16x8_shr(br, 8), wasm_u16x8_shr(ga, 8)};
    for (int i = 0; i < 4; ++i) {
      ch[i] =
          wasm_u16x8_shr(wasm_i16x8_add(wasm_i16x8_mul(ch[i], k17), two), 2);
    }
    ChannelsToUV8(ch[0], ch[1], ch[2], ch[3], k, dst_u, dst_v);
    src_argb4444 += 32;
    src_argb4444_1 += 32;
    dst_u += 8;
    dst_v += 8;
    width -= 16;
  } while (width > 0);
}
#endif  // HAS_ARGB4444TOUVMATRIXROW_WASMSIMD

#ifdef HAS_CONVERT8TO16ROW_WASMSIMD
// For bits 8 to 16, (y * 0x0101) >> (16 - bits) == p + (p >> 8) where
// p = y << (bits - 8). This avoids a variable shift count.
void Convert8To16Row_WASMSIMD(const uint8_t* src_y,
                              uint16_t* dst_y,
                              int bits,
                              int width) {
  if (bits < 8) {
    Convert8To16Row_C(src_y, dst_y, bits, width);
    return;
  }
  v128_t scale = SplatConst16(1 << (bits - 8));
  do {
    asm("" : "+r"(dst_y));
    v128_t y = wasm_v128_load(src_y);
    v128_t y0 = wasm_i16x8_mul(wasm_u16x8_extend_low_u8x16(y), scale);
    v128_t y1 = wasm_i16x8_mul(wasm_u16x8_extend_high_u8x16(y), scale);
    wasm_v128_store(dst_y, wasm_i16x8_add(y0, wasm_u16x8_shr(y0, 8)));
    wasm_v128_store(dst_y + 8, wasm_i16x8_add(y1, wasm_u16x8_shr(y1, 8)));
    src_y += 16;
    dst_y += 16;
    width -= 16;
  } while (width > 0);
}
#endif  // HAS_CONVERT8TO16ROW_WASMSIMD

#ifdef HAS_MULTIPLYROW_16_WASMSIMD
void MultiplyRow_16_WASMSIMD(const uint16_t* src_y,
                             uint16_t* dst_y,
                             int scale,
                             int width) {
  v128_t vscale = wasm_i16x8_splat(scale);
  do {
    asm("" : "+r"(src_y), "+r"(dst_y));
    v128_t y0 = wasm_v128_load(src_y);
    v128_t y1 = wasm_v128_load(src_y + 8);
    y0 = wasm_i16x8_mul(y0, vscale);
    y1 = wasm_i16x8_mul(y1, vscale);
    wasm_v128_store(dst_y, y0);
    wasm_v128_store(dst_y + 8, y1);
    src_y += 16;
    dst_y += 16;
    width -= 16;
  } while (width > 0);
}
#endif  // HAS_MULTIPLYROW_16_WASMSIMD

#ifdef HAS_MERGEUVROW_WASMSIMD
void MergeUVRow_WASMSIMD(const uint8_t* src_u,
                         const uint8_t* src_v,
                         uint8_t* dst_uv,
                         int width) {
  do {
    asm("" : "+r"(dst_uv));
    v128_t u = wasm_v128_load(src_u);
    v128_t v = wasm_v128_load(src_v);
    v128_t uv0 = wasm_i8x16_shuffle(u, v, 0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5,
                                    21, 6, 22, 7, 23);
    v128_t uv1 = wasm_i8x16_shuffle(u, v, 8, 24, 9, 25, 10, 26, 11, 27, 12, 28,
                                    13, 29, 14, 30, 15, 31);
    wasm_v128_store(dst_uv, uv0);
    wasm_v128_store(dst_uv + 16, uv1);
    src_u += 16;
    src_v += 16;
    dst_uv += 32;
    width -= 16;
  } while (width > 0);
}
#endif  // HAS_MERGEUVROW_WASMSIMD

#ifdef HAS_MIRRORROW_WASMSIMD
static const uint8_t kShuffleMirror[16] = {15, 14, 13, 12, 11, 10, 9, 8,
                                           7,  6,  5,  4,  3,  2,  1, 0};
void MirrorRow_WASMSIMD(const uint8_t* src, uint8_t* dst, int width) {
  v128_t shuf = LoadConst(kShuffleMirror);
  src += width - 16;
  do {
    wasm_v128_store(dst, LIBYUV_WASM_SWIZZLE(wasm_v128_load(src), shuf));
    src -= 16;
    dst += 16;
    width -= 16;
  } while (width > 0);
}
#endif  // HAS_MIRRORROW_WASMSIMD

#ifdef HAS_MIRRORUVROW_WASMSIMD
static const uint8_t kShuffleMirrorUV[16] = {14, 15, 12, 13, 10, 11, 8, 9,
                                             6,  7,  4,  5,  2,  3,  0, 1};
void MirrorUVRow_WASMSIMD(const uint8_t* src_uv, uint8_t* dst_uv, int width) {
  v128_t shuf = LoadConst(kShuffleMirrorUV);
  src_uv += (width - 8) * 2;
  do {
    wasm_v128_store(dst_uv, LIBYUV_WASM_SWIZZLE(wasm_v128_load(src_uv), shuf));
    src_uv -= 16;
    dst_uv += 16;
    width -= 8;
  } while (width > 0);
}
#endif  // HAS_MIRRORUVROW_WASMSIMD

#ifdef HAS_SWAPUVROW_WASMSIMD
void SwapUVRow_WASMSIMD(const uint8_t* src_uv, uint8_t* dst_vu, int width) {
  do {
    // Keep constant offsets in the load/store instructions.
    asm("" : "+r"(src_uv), "+r"(dst_vu));
    v128_t uv0 = wasm_v128_load(src_uv);
    v128_t uv1 = wasm_v128_load(src_uv + 16);
    // 16 bit byte swap: rev16 on Arm, shifts on x86.
    wasm_v128_store(
        dst_vu, wasm_v128_or(wasm_i16x8_shl(uv0, 8), wasm_u16x8_shr(uv0, 8)));
    wasm_v128_store(dst_vu + 16, wasm_v128_or(wasm_i16x8_shl(uv1, 8),
                                              wasm_u16x8_shr(uv1, 8)));
    src_uv += 32;
    dst_vu += 32;
    width -= 16;
  } while (width > 0);
}
#endif  // HAS_SWAPUVROW_WASMSIMD

#ifdef HAS_MIRRORSPLITUVROW_WASMSIMD
// Reverses 8 UV pairs into 8 U then 8 V.
static const uint8_t kShuffleMirrorSplitUV[16] = {14, 12, 10, 8, 6, 4, 2, 0,
                                                  15, 13, 11, 9, 7, 5, 3, 1};
void MirrorSplitUVRow_WASMSIMD(const uint8_t* src_uv,
                               uint8_t* dst_u,
                               uint8_t* dst_v,
                               int width) {
  v128_t shuf = LoadConst(kShuffleMirrorSplitUV);
  src_uv += (width - 16) * 2;
  do {
    asm("" : "+r"(src_uv));
    v128_t a = LIBYUV_WASM_SWIZZLE(wasm_v128_load(src_uv), shuf);
    v128_t b = LIBYUV_WASM_SWIZZLE(wasm_v128_load(src_uv + 16), shuf);
    wasm_v128_store(dst_u, wasm_i64x2_shuffle(b, a, 0, 2));
    wasm_v128_store(dst_v, wasm_i64x2_shuffle(b, a, 1, 3));
    src_uv -= 32;
    dst_u += 16;
    dst_v += 16;
    width -= 16;
  } while (width > 0);
}
#endif  // HAS_MIRRORSPLITUVROW_WASMSIMD

#ifdef HAS_RGB24MIRRORROW_WASMSIMD
// Each 16 byte output needs 18 source bytes, so it is the OR of 2 swizzles.
static const uint8_t kShuffleRGB24Mirror[96] = {
    13,  14,  15,  10,  11,  12,  7,   8,   9,   4,   5,   6,   1,   2,
    3,   128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 14,  15,  128, 11,  12,  13,  8,   9,   10,  5,   6,
    7,   2,   3,   4,   128, 0,   128, 1,   128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 0,   128, 128, 12,  13,  14,  9,   10,
    11,  6,   7,   8,   3,   4,   5,   0,   1,   2,   1,   128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128};
void RGB24MirrorRow_WASMSIMD(const uint8_t* src_rgb24,
                             uint8_t* dst_rgb24,
                             int width) {
  const uint8_t* shuf = kShuffleRGB24Mirror;
  v128_t m0a = LoadConst(shuf);
  v128_t m0b = LoadConst(shuf + 16);
  v128_t m1a = LoadConst(shuf + 32);
  v128_t m1b = LoadConst(shuf + 48);
  v128_t m2a = LoadConst(shuf + 64);
  v128_t m2b = LoadConst(shuf + 80);
  src_rgb24 += (width - 16) * 3;
  do {
    asm("" : "+r"(src_rgb24), "+r"(dst_rgb24));
    v128_t s0 = wasm_v128_load(src_rgb24);
    v128_t s1 = wasm_v128_load(src_rgb24 + 16);
    v128_t s2 = wasm_v128_load(src_rgb24 + 32);
    // Bytes 15 and 32 of the 48 source bytes.
    v128_t s02 = wasm_i8x16_shuffle(s0, s2, 15, 16, 17, 18, 19, 20, 21, 22, 23,
                                    24, 25, 26, 27, 28, 29, 30);
    wasm_v128_store(dst_rgb24, wasm_v128_or(LIBYUV_WASM_SWIZZLE(s2, m0a),
                                            LIBYUV_WASM_SWIZZLE(s1, m0b)));
    wasm_v128_store(dst_rgb24 + 16,
                    wasm_v128_or(LIBYUV_WASM_SWIZZLE(s1, m1a),
                                 LIBYUV_WASM_SWIZZLE(s02, m1b)));
    wasm_v128_store(dst_rgb24 + 32, wasm_v128_or(LIBYUV_WASM_SWIZZLE(s0, m2a),
                                                 LIBYUV_WASM_SWIZZLE(s1, m2b)));
    src_rgb24 -= 48;
    dst_rgb24 += 48;
    width -= 16;
  } while (width > 0);
}
#endif  // HAS_RGB24MIRRORROW_WASMSIMD

#ifdef HAS_ARGBMIRRORROW_WASMSIMD
void ARGBMirrorRow_WASMSIMD(const uint8_t* src, uint8_t* dst, int width) {
  src += (width - 4) * 4;
  do {
    v128_t v = wasm_v128_load(src);
    v = wasm_i32x4_shuffle(v, v, 3, 2, 1, 0);
    wasm_v128_store(dst, v);
    src -= 16;
    dst += 16;
    width -= 4;
  } while (width > 0);
}
#endif  // HAS_ARGBMIRRORROW_WASMSIMD

#ifdef HAS_INTERPOLATEROW_WASMSIMD
void InterpolateRow_WASMSIMD(uint8_t* dst_ptr,
                             const uint8_t* src_ptr,
                             ptrdiff_t src_stride,
                             int width,
                             int source_y_fraction) {
  const uint8_t* src_ptr1 = src_ptr + src_stride;
  if (source_y_fraction == 0) {
    do {
      wasm_v128_store(dst_ptr, wasm_v128_load(src_ptr));
      src_ptr += 16;
      dst_ptr += 16;
      width -= 16;
    } while (width > 0);
    return;
  }
  if (source_y_fraction == 128) {
    do {
      v128_t s0 = wasm_v128_load(src_ptr);
      v128_t s1 = wasm_v128_load(src_ptr1);
      wasm_v128_store(dst_ptr, wasm_u8x16_avgr(s0, s1));
      src_ptr += 16;
      src_ptr1 += 16;
      dst_ptr += 16;
      width -= 16;
    } while (width > 0);
    return;
  }
  // s0 * (256 - f) + s1 * f + 128 fits in 16 bits. Both weights fit in 8
  // bits since f is 1 to 255. The high bytes are the result (uzp2 on Arm).
  v128_t w0 = SplatConst8(256 - source_y_fraction);
  v128_t w1 = SplatConst8(source_y_fraction);
  v128_t round = SplatConst16(128);
  do {
    v128_t s0 = wasm_v128_load(src_ptr);
    v128_t s1 = wasm_v128_load(src_ptr1);
    v128_t lo = wasm_i16x8_add(wasm_u16x8_extmul_low_u8x16(s0, w0),
                               wasm_u16x8_extmul_low_u8x16(s1, w1));
    v128_t hi = wasm_i16x8_add(wasm_u16x8_extmul_high_u8x16(s0, w0),
                               wasm_u16x8_extmul_high_u8x16(s1, w1));
    lo = wasm_i16x8_add(lo, round);
    hi = wasm_i16x8_add(hi, round);
    wasm_v128_store(
        dst_ptr, wasm_i8x16_shuffle(lo, hi, 1, 3, 5, 7, 9, 11, 13, 15, 17, 19,
                                    21, 23, 25, 27, 29, 31));
    src_ptr += 16;
    src_ptr1 += 16;
    dst_ptr += 16;
    width -= 16;
  } while (width > 0);
}
#endif  // HAS_INTERPOLATEROW_WASMSIMD

#ifdef HAS_INTERPOLATEROW_16_WASMSIMD
void InterpolateRow_16_WASMSIMD(uint16_t* dst_ptr,
                                const uint16_t* src_ptr,
                                ptrdiff_t src_stride,
                                int width,
                                int source_y_fraction) {
  const uint16_t* src_ptr1 = src_ptr + src_stride;
  if (source_y_fraction == 0) {
    do {
      wasm_v128_store(dst_ptr, wasm_v128_load(src_ptr));
      src_ptr += 8;
      dst_ptr += 8;
      width -= 8;
    } while (width > 0);
    return;
  }
  if (source_y_fraction == 128) {
    do {
      v128_t s0 = wasm_v128_load(src_ptr);
      v128_t s1 = wasm_v128_load(src_ptr1);
      wasm_v128_store(dst_ptr, wasm_u16x8_avgr(s0, s1));
      src_ptr += 8;
      src_ptr1 += 8;
      dst_ptr += 8;
      width -= 8;
    } while (width > 0);
    return;
  }
  // s0 * (256 - f) + s1 * f + 128 fits in 24 bits. Shift the result into
  // the high 16 bits of each lane, then take the odd 16 bit lanes.
  v128_t w0 = SplatConst16(256 - source_y_fraction);
  v128_t w1 = SplatConst16(source_y_fraction);
  v128_t round = SplatConst32(128);
  do {
    v128_t s0 = wasm_v128_load(src_ptr);
    v128_t s1 = wasm_v128_load(src_ptr1);
    v128_t lo = wasm_i32x4_add(wasm_u32x4_extmul_low_u16x8(s0, w0),
                               wasm_u32x4_extmul_low_u16x8(s1, w1));
    v128_t hi = wasm_i32x4_add(wasm_u32x4_extmul_high_u16x8(s0, w0),
                               wasm_u32x4_extmul_high_u16x8(s1, w1));
    lo = wasm_i32x4_shl(wasm_i32x4_add(lo, round), 8);
    hi = wasm_i32x4_shl(wasm_i32x4_add(hi, round), 8);
    wasm_v128_store(dst_ptr,
                    wasm_i16x8_shuffle(lo, hi, 1, 3, 5, 7, 9, 11, 13, 15));
    src_ptr += 8;
    src_ptr1 += 8;
    dst_ptr += 8;
    width -= 8;
  } while (width > 0);
}
#endif  // HAS_INTERPOLATEROW_16_WASMSIMD

#ifdef HAS_J400TOARGBROW_WASMSIMD
static const uint8_t kShuffleJ400ToARGB[64] = {
    0,  0,  0,  128, 1,  1,  1,  128, 2,  2,  2,  128, 3,  3,  3,  128,
    4,  4,  4,  128, 5,  5,  5,  128, 6,  6,  6,  128, 7,  7,  7,  128,
    8,  8,  8,  128, 9,  9,  9,  128, 10, 10, 10, 128, 11, 11, 11, 128,
    12, 12, 12, 128, 13, 13, 13, 128, 14, 14, 14, 128, 15, 15, 15, 128};
void J400ToARGBRow_WASMSIMD(const uint8_t* src_y,
                            uint8_t* dst_argb,
                            int width) {
  v128_t shuf0 = LoadConst(kShuffleJ400ToARGB);
  v128_t shuf1 = LoadConst(kShuffleJ400ToARGB + 16);
  v128_t shuf2 = LoadConst(kShuffleJ400ToARGB + 32);
  v128_t shuf3 = LoadConst(kShuffleJ400ToARGB + 48);
  v128_t alpha = SplatConst32((int)0xff000000);
  do {
    asm("" : "+r"(dst_argb));
    v128_t y = wasm_v128_load(src_y);
    wasm_v128_store(dst_argb,
                    wasm_v128_or(LIBYUV_WASM_SWIZZLE(y, shuf0), alpha));
    wasm_v128_store(dst_argb + 16,
                    wasm_v128_or(LIBYUV_WASM_SWIZZLE(y, shuf1), alpha));
    wasm_v128_store(dst_argb + 32,
                    wasm_v128_or(LIBYUV_WASM_SWIZZLE(y, shuf2), alpha));
    wasm_v128_store(dst_argb + 48,
                    wasm_v128_or(LIBYUV_WASM_SWIZZLE(y, shuf3), alpha));
    src_y += 16;
    dst_argb += 64;
    width -= 16;
  } while (width > 0);
}
#endif  // HAS_J400TOARGBROW_WASMSIMD

#ifdef HAS_RGB565TOARGBROW_WASMSIMD
void RGB565ToARGBRow_WASMSIMD(const uint8_t* src_rgb565,
                              uint8_t* dst_argb,
                              int width) {
  v128_t mask_b5 = SplatConst16(0x001f);
  v128_t mask_g6 = SplatConst16(0x07e0);
  v128_t mask_r5 = SplatConst16(0xf800);
  v128_t mask_hi = SplatConst16(0xff00);
  do {
    asm("" : "+r"(dst_argb));
    v128_t p = wasm_v128_load(src_rgb565);
    v128_t b5 = wasm_v128_and(p, mask_b5);
    v128_t b8 = wasm_v128_or(wasm_i16x8_shl(b5, 3), wasm_u16x8_shr(b5, 2));
    v128_t g6 = wasm_v128_and(p, mask_g6);
    v128_t g8_hi = wasm_v128_and(
        wasm_v128_or(wasm_i16x8_shl(g6, 5), wasm_u16x8_shr(g6, 1)), mask_hi);
    v128_t bg = wasm_v128_or(b8, g8_hi);

    v128_t r8 = wasm_v128_or(wasm_u16x8_shr(wasm_v128_and(p, mask_r5), 8),
                             wasm_u16x8_shr(p, 13));
    v128_t ra = wasm_v128_or(r8, mask_hi);

    v128_t d0 = wasm_i16x8_shuffle(bg, ra, 0, 8, 1, 9, 2, 10, 3, 11);
    v128_t d1 = wasm_i16x8_shuffle(bg, ra, 4, 12, 5, 13, 6, 14, 7, 15);
    wasm_v128_store(dst_argb, d0);
    wasm_v128_store(dst_argb + 16, d1);
    src_rgb565 += 16;
    dst_argb += 32;
    width -= 8;
  } while (width > 0);
}
#endif  // HAS_RGB565TOARGBROW_WASMSIMD

#ifdef HAS_ARGB1555TOARGBROW_WASMSIMD
// 5 to 8 bit replication with multiplies: (v * 0x108) >> 5 == v << 3 | v >> 2.
void ARGB1555ToARGBRow_WASMSIMD(const uint8_t* src_argb1555,
                                uint8_t* dst_argb,
                                int width) {
  v128_t mask_b5 = SplatConst16(0x001f);
  v128_t mask_g5 = SplatConst16(0x03e0);
  v128_t mask_hi = SplatConst16(0xff00);
  v128_t mul_br = SplatConst16(0x0108);
  v128_t mul_g = SplatConst16(0x0042);
  do {
    asm("" : "+r"(dst_argb));
    v128_t p = wasm_v128_load(src_argb1555);
    v128_t b8 =
        wasm_u16x8_shr(wasm_i16x8_mul(wasm_v128_and(p, mask_b5), mul_br), 5);
    // g5 << 5 times 0x42 is g5 << 11 | g5 << 6; the high byte is g8.
    v128_t g8_hi = wasm_v128_and(
        wasm_i16x8_mul(wasm_v128_and(p, mask_g5), mul_g), mask_hi);
    v128_t bg = wasm_v128_or(b8, g8_hi);
    v128_t r5 = wasm_u16x8_shr(wasm_i16x8_shl(p, 1), 11);
    v128_t r8 = wasm_u16x8_shr(wasm_i16x8_mul(r5, mul_br), 5);
    v128_t a8_hi = wasm_v128_and(wasm_i16x8_shr(p, 15), mask_hi);
    v128_t ra = wasm_v128_or(r8, a8_hi);

    v128_t d0 = wasm_i16x8_shuffle(bg, ra, 0, 8, 1, 9, 2, 10, 3, 11);
    v128_t d1 = wasm_i16x8_shuffle(bg, ra, 4, 12, 5, 13, 6, 14, 7, 15);
    wasm_v128_store(dst_argb, d0);
    wasm_v128_store(dst_argb + 16, d1);
    src_argb1555 += 16;
    dst_argb += 32;
    width -= 8;
  } while (width > 0);
}
#endif  // HAS_ARGB1555TOARGBROW_WASMSIMD

#ifdef HAS_ARGB4444TOARGBROW_WASMSIMD
void ARGB4444ToARGBRow_WASMSIMD(const uint8_t* src_argb4444,
                                uint8_t* dst_argb,
                                int width) {
  v128_t mask_lo = SplatConst8(0x0f);
  v128_t mask_hi = SplatConst8(0xf0);
  do {
    asm("" : "+r"(dst_argb));
    v128_t v = wasm_v128_load(src_argb4444);
    v128_t v_lo = wasm_v128_and(v, mask_lo);
    v_lo = wasm_v128_or(v_lo, wasm_i16x8_shl(v_lo, 4));
    v128_t v_hi = wasm_v128_and(v, mask_hi);
    v_hi = wasm_v128_or(v_hi, wasm_u16x8_shr(v_hi, 4));

    v128_t d0 = wasm_i8x16_shuffle(v_lo, v_hi, 0, 16, 1, 17, 2, 18, 3, 19, 4,
                                   20, 5, 21, 6, 22, 7, 23);
    v128_t d1 = wasm_i8x16_shuffle(v_lo, v_hi, 8, 24, 9, 25, 10, 26, 11, 27, 12,
                                   28, 13, 29, 14, 30, 15, 31);
    wasm_v128_store(dst_argb, d0);
    wasm_v128_store(dst_argb + 16, d1);
    src_argb4444 += 16;
    dst_argb += 32;
    width -= 8;
  } while (width > 0);
}
#endif  // HAS_ARGB4444TOARGBROW_WASMSIMD

#ifdef HAS_ARGBTORGB565DITHERROW_WASMSIMD
void ARGBToRGB565DitherRow_WASMSIMD(const uint8_t* src_argb,
                                    uint8_t* dst_rgb,
                                    uint32_t dither4,
                                    int width) {
  v128_t dither = wasm_i32x4_splat((int32_t)dither4);
  dither = wasm_i8x16_shuffle(dither, dither, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                              2, 3, 3, 3, 3);
  v128_t mask_b = SplatConst32(0x0000001f);
  v128_t mask_g = SplatConst32(0x000007e0);
  v128_t mask_r = SplatConst32(0x0000f800);
  do {
    asm("" : "+r"(src_argb));
    v128_t s0 = wasm_u8x16_add_sat(wasm_v128_load(src_argb), dither);
    v128_t s1 = wasm_u8x16_add_sat(wasm_v128_load(src_argb + 16), dither);
    v128_t b0 = wasm_v128_and(wasm_u32x4_shr(s0, 3), mask_b);
    v128_t g0 = wasm_v128_and(wasm_u32x4_shr(s0, 5), mask_g);
    v128_t r0 = wasm_v128_and(wasm_u32x4_shr(s0, 8), mask_r);
    v128_t b1 = wasm_v128_and(wasm_u32x4_shr(s1, 3), mask_b);
    v128_t g1 = wasm_v128_and(wasm_u32x4_shr(s1, 5), mask_g);
    v128_t r1 = wasm_v128_and(wasm_u32x4_shr(s1, 8), mask_r);
    v128_t p0 = wasm_v128_or(wasm_v128_or(b0, g0), r0);
    v128_t p1 = wasm_v128_or(wasm_v128_or(b1, g1), r1);
    wasm_v128_store(dst_rgb, wasm_u16x8_narrow_i32x4(p0, p1));
    src_argb += 32;
    dst_rgb += 16;
    width -= 8;
  } while (width > 0);
}
#endif  // HAS_ARGBTORGB565DITHERROW_WASMSIMD

#ifdef HAS_ARGBTORGB565ROW_WASMSIMD
void ARGBToRGB565Row_WASMSIMD(const uint8_t* src_argb,
                              uint8_t* dst_rgb,
                              int width) {
  v128_t mask_b = SplatConst32(0x0000001f);
  v128_t mask_g = SplatConst32(0x000007e0);
  v128_t mask_r = SplatConst32(0x0000f800);
  do {
    asm("" : "+r"(src_argb));
    v128_t s0 = wasm_v128_load(src_argb);
    v128_t s1 = wasm_v128_load(src_argb + 16);
    v128_t b0 = wasm_v128_and(wasm_u32x4_shr(s0, 3), mask_b);
    v128_t g0 = wasm_v128_and(wasm_u32x4_shr(s0, 5), mask_g);
    v128_t r0 = wasm_v128_and(wasm_u32x4_shr(s0, 8), mask_r);
    v128_t b1 = wasm_v128_and(wasm_u32x4_shr(s1, 3), mask_b);
    v128_t g1 = wasm_v128_and(wasm_u32x4_shr(s1, 5), mask_g);
    v128_t r1 = wasm_v128_and(wasm_u32x4_shr(s1, 8), mask_r);
    v128_t p0 = wasm_v128_or(wasm_v128_or(b0, g0), r0);
    v128_t p1 = wasm_v128_or(wasm_v128_or(b1, g1), r1);
    wasm_v128_store(dst_rgb, wasm_u16x8_narrow_i32x4(p0, p1));
    src_argb += 32;
    dst_rgb += 16;
    width -= 8;
  } while (width > 0);
}
#endif  // HAS_ARGBTORGB565ROW_WASMSIMD

#ifdef HAS_ARGBTOARGB1555ROW_WASMSIMD
void ARGBToARGB1555Row_WASMSIMD(const uint8_t* src_argb,
                                uint8_t* dst_argb1555,
                                int width) {
  v128_t mask_b = SplatConst32(0x0000001f);
  v128_t mask_g = SplatConst32(0x000003e0);
  v128_t mask_r = SplatConst32(0x00007c00);
  v128_t mask_a = SplatConst32(0x00008000);
  do {
    asm("" : "+r"(src_argb));
    v128_t s0 = wasm_v128_load(src_argb);
    v128_t s1 = wasm_v128_load(src_argb + 16);
    v128_t b0 = wasm_v128_and(wasm_u32x4_shr(s0, 3), mask_b);
    v128_t g0 = wasm_v128_and(wasm_u32x4_shr(s0, 6), mask_g);
    v128_t r0 = wasm_v128_and(wasm_u32x4_shr(s0, 9), mask_r);
    v128_t a0 = wasm_v128_and(wasm_u32x4_shr(s0, 16), mask_a);
    v128_t b1 = wasm_v128_and(wasm_u32x4_shr(s1, 3), mask_b);
    v128_t g1 = wasm_v128_and(wasm_u32x4_shr(s1, 6), mask_g);
    v128_t r1 = wasm_v128_and(wasm_u32x4_shr(s1, 9), mask_r);
    v128_t a1 = wasm_v128_and(wasm_u32x4_shr(s1, 16), mask_a);
    v128_t p0 = wasm_v128_or(wasm_v128_or(b0, g0), wasm_v128_or(r0, a0));
    v128_t p1 = wasm_v128_or(wasm_v128_or(b1, g1), wasm_v128_or(r1, a1));
    wasm_v128_store(dst_argb1555, wasm_u16x8_narrow_i32x4(p0, p1));
    src_argb += 32;
    dst_argb1555 += 16;
    width -= 8;
  } while (width > 0);
}
#endif  // HAS_ARGBTOARGB1555ROW_WASMSIMD

#ifdef HAS_ARGBTOARGB4444ROW_WASMSIMD
void ARGBToARGB4444Row_WASMSIMD(const uint8_t* src_argb,
                                uint8_t* dst_argb4444,
                                int width) {
  v128_t mask_lo = SplatConst16(0x000f);
  v128_t mask_hi = SplatConst16(0x00f0);
  do {
    asm("" : "+r"(src_argb));
    v128_t s0 = wasm_v128_load(src_argb);
    v128_t s1 = wasm_v128_load(src_argb + 16);
    v128_t n0 = wasm_v128_or(wasm_v128_and(wasm_u16x8_shr(s0, 4), mask_lo),
                             wasm_v128_and(wasm_u16x8_shr(s0, 8), mask_hi));
    v128_t n1 = wasm_v128_or(wasm_v128_and(wasm_u16x8_shr(s1, 4), mask_lo),
                             wasm_v128_and(wasm_u16x8_shr(s1, 8), mask_hi));
    wasm_v128_store(dst_argb4444, wasm_u8x16_narrow_i16x8(n0, n1));
    src_argb += 32;
    dst_argb4444 += 16;
    width -= 8;
  } while (width > 0);
}
#endif  // HAS_ARGBTOARGB4444ROW_WASMSIMD

#ifdef HAS_ARGBSHUFFLEROW_WASMSIMD
void ARGBShuffleRow_WASMSIMD(const uint8_t* src_argb,
                             uint8_t* dst_argb,
                             const uint8_t* shuffler,
                             int width) {
  v128_t shuf = wasm_v128_load(shuffler);
  while (width >= 8) {
    asm("" : "+r"(src_argb), "+r"(dst_argb));
    v128_t s0 = wasm_v128_load(src_argb);
    v128_t s1 = wasm_v128_load(src_argb + 16);
    wasm_v128_store(dst_argb, LIBYUV_WASM_SWIZZLE(s0, shuf));
    wasm_v128_store(dst_argb + 16, LIBYUV_WASM_SWIZZLE(s1, shuf));
    src_argb += 32;
    dst_argb += 32;
    width -= 8;
  }
  if (width > 0) {
    v128_t s0 = wasm_v128_load(src_argb);
    wasm_v128_store(dst_argb, LIBYUV_WASM_SWIZZLE(s0, shuf));
  }
}
#endif  // HAS_ARGBSHUFFLEROW_WASMSIMD

#if defined(HAS_I422TOARGBROW_WASMSIMD) ||     \
    defined(HAS_I422TOAR30ROW_WASMSIMD) ||     \
    defined(HAS_NV12TOARGBROW_WASMSIMD) ||     \
    defined(HAS_NV21TOARGBROW_WASMSIMD) ||     \
    defined(HAS_I422TORGB24ROW_WASMSIMD) ||    \
    defined(HAS_I422TORGB565ROW_WASMSIMD) ||   \
    defined(HAS_I422TOARGB1555ROW_WASMSIMD) || \
    defined(HAS_I422TOARGB4444ROW_WASMSIMD) || \
    defined(HAS_NV12TORGB565ROW_WASMSIMD)
// Interleaves map to punpck on x64 and zip on arm64 when the two inputs
// differ. V8 arm64 lowers shuffle(x, x) to tbl with a rebuilt mask.
static __inline v128_t ZipLo8_WASMSIMD(v128_t a, v128_t b) {
  return wasm_i8x16_shuffle(a, b, 0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21, 6,
                            22, 7, 23);
}
static __inline v128_t ZipHi8_WASMSIMD(v128_t a, v128_t b) {
  return wasm_i8x16_shuffle(a, b, 8, 24, 9, 25, 10, 26, 11, 27, 12, 28, 13, 29,
                            14, 30, 15, 31);
}

// YuvConstants splatted to 16-bit lanes, built once per row.
struct YuvCoefs_WASMSIMD {
  v128_t ub, ug, vg, vr, yg, yb, x80, k00ff, k0101;
};

static __inline void LoadYuvCoefs_WASMSIMD(const struct YuvConstants* yc,
                                           int yb_adjust,
                                           struct YuvCoefs_WASMSIMD* c) {
  c->ub = wasm_i16x8_splat(yc->kUVToB[0]);
  c->ug = wasm_i16x8_splat(yc->kUVToG[0]);
  c->vg = wasm_i16x8_splat(yc->kUVToG[1]);
  c->vr = wasm_i16x8_splat(yc->kUVToR[1]);
  c->yg = wasm_i16x8_splat(yc->kYToRgb[0]);
  c->yb = wasm_i16x8_splat(yc->kYBiasToRgb[0] - yb_adjust);
  c->x80 = SplatConst8(0x80);
  c->k00ff = SplatConst16(0x00ff);
  c->k0101 = SplatConst16(0x0101);
}

// Returns ((y * 0x0101 * yg) >> 16) + yb for 8 Y in 16-bit lanes.
static __inline v128_t YToRgb_WASMSIMD(v128_t y,
                                       const struct YuvCoefs_WASMSIMD* c) {
  // The barrier keeps clang from widening yg outside the loop, which would
  // turn extmul (umull on arm64) into extend + i32x4.mul.
  v128_t yg = c->yg;
  asm volatile("" : "+r"(yg));
  v128_t y16 = wasm_i16x8_mul(y, c->k0101);
  v128_t lo = wasm_u32x4_extmul_low_u16x8(y16, yg);
  v128_t hi = wasm_u32x4_extmul_high_u16x8(y16, yg);
  return wasm_i16x8_add(wasm_i16x8_shuffle(lo, hi, 1, 3, 5, 7, 9, 11, 13, 15),
                        c->yb);
}

// Converts 16 pixels to B, G, R with 6 fractional bits (unclamped int16).
// y = 16 Y. uv = 8 U/V pairs, U in the low byte of each 16-bit lane (V if vu).
// Even and odd pixels are kept apart so each pair shares its chroma terms:
// rgb[0], rgb[2], rgb[4] = even B, G, R and rgb[1], rgb[3], rgb[5] = odd.
static __inline void YuvToRgb16_WASMSIMD(v128_t y,
                                         v128_t uv,
                                         int vu,
                                         const struct YuvCoefs_WASMSIMD* c,
                                         v128_t* rgb) {
  v128_t ye = YToRgb_WASMSIMD(wasm_v128_and(y, c->k00ff), c);
  v128_t yo = YToRgb_WASMSIMD(wasm_u16x8_shr(y, 8), c);
  // u - 128 and v - 128 as int8, sign extended to int16.
  v128_t uvs = wasm_v128_xor(uv, c->x80);
  v128_t lo = wasm_i16x8_shr(wasm_i16x8_shl(uvs, 8), 8);
  v128_t hi = wasm_i16x8_shr(uvs, 8);
  v128_t ui = vu ? hi : lo;
  v128_t vi = vu ? lo : hi;
  v128_t b = wasm_i16x8_mul(ui, c->ub);
  v128_t g =
      wasm_i16x8_add(wasm_i16x8_mul(ui, c->ug), wasm_i16x8_mul(vi, c->vg));
  v128_t r = wasm_i16x8_mul(vi, c->vr);
  rgb[0] = wasm_i16x8_add_sat(ye, b);
  rgb[1] = wasm_i16x8_add_sat(yo, b);
  rgb[2] = wasm_i16x8_sub_sat(ye, g);
  rgb[3] = wasm_i16x8_sub_sat(yo, g);
  rgb[4] = wasm_i16x8_add_sat(ye, r);
  rgb[5] = wasm_i16x8_add_sat(yo, r);
}

// Loads 16 Y and 8 U/V, or 8 Y and 4 U/V when width < 16.
static __inline void ReadI422_WASMSIMD(const uint8_t* src_y,
                                       const uint8_t* src_u,
                                       const uint8_t* src_v,
                                       int width,
                                       v128_t* y,
                                       v128_t* uv) {
  if (width >= 16) {
    *y = wasm_v128_load(src_y);
    *uv = ZipLo8_WASMSIMD(wasm_v128_load64_zero(src_u),
                          wasm_v128_load64_zero(src_v));
  } else {
    *y = wasm_v128_load64_zero(src_y);
    *uv = ZipLo8_WASMSIMD(wasm_v128_load32_zero(src_u),
                          wasm_v128_load32_zero(src_v));
  }
}

// Loads 16 Y and 8 UV pairs, or 8 Y and 4 UV pairs when width < 16.
static __inline void ReadNV12_WASMSIMD(const uint8_t* src_y,
                                       const uint8_t* src_uv,
                                       int width,
                                       v128_t* y,
                                       v128_t* uv) {
  if (width >= 16) {
    *y = wasm_v128_load(src_y);
    *uv = wasm_v128_load(src_uv);
  } else {
    *y = wasm_v128_load64_zero(src_y);
    *uv = wasm_v128_load64_zero(src_uv);
  }
}
#endif

#if defined(HAS_I422TOARGBROW_WASMSIMD) || \
    defined(HAS_NV12TOARGBROW_WASMSIMD) || defined(HAS_NV21TOARGBROW_WASMSIMD)
// Clamps even/odd B, G, R to 8 bits and stores 16 ARGB pixels, or 8 when
// width < 16.
static __inline void StoreARGB_WASMSIMD(const v128_t* rgb,
                                        v128_t ff,
                                        uint8_t* dst_argb,
                                        int width) {
  v128_t bg_e = wasm_u8x16_narrow_i16x8(wasm_i16x8_shr(rgb[0], 6),
                                        wasm_i16x8_shr(rgb[2], 6));
  v128_t gb_o = wasm_u8x16_narrow_i16x8(wasm_i16x8_shr(rgb[3], 6),
                                        wasm_i16x8_shr(rgb[1], 6));
  v128_t rr = wasm_u8x16_narrow_i16x8(wasm_i16x8_shr(rgb[4], 6),
                                      wasm_i16x8_shr(rgb[5], 6));
  v128_t br_e = ZipLo8_WASMSIMD(bg_e, rr);
  v128_t ga_e = ZipHi8_WASMSIMD(bg_e, ff);
  v128_t br_o = ZipHi8_WASMSIMD(gb_o, rr);
  v128_t ga_o = ZipLo8_WASMSIMD(gb_o, ff);
  v128_t e = ZipLo8_WASMSIMD(br_e, ga_e);  // Pixels 0, 2, 4, 6.
  v128_t o = ZipLo8_WASMSIMD(br_o, ga_o);  // Pixels 1, 3, 5, 7.
  wasm_v128_store(dst_argb, wasm_i32x4_shuffle(e, o, 0, 4, 1, 5));
  wasm_v128_store(dst_argb + 16, wasm_i32x4_shuffle(e, o, 2, 6, 3, 7));
  if (width >= 16) {
    e = ZipHi8_WASMSIMD(br_e, ga_e);
    o = ZipHi8_WASMSIMD(br_o, ga_o);
    wasm_v128_store(dst_argb + 32, wasm_i32x4_shuffle(e, o, 0, 4, 1, 5));
    wasm_v128_store(dst_argb + 48, wasm_i32x4_shuffle(e, o, 2, 6, 3, 7));
  }
}
#endif

#ifdef HAS_I422TOARGBROW_WASMSIMD
void I422ToARGBRow_WASMSIMD(const uint8_t* src_y,
                            const uint8_t* src_u,
                            const uint8_t* src_v,
                            uint8_t* dst_argb,
                            const struct YuvConstants* yuvconstants,
                            int width) {
  struct YuvCoefs_WASMSIMD c;
  v128_t ff = SplatConst8(0xff);
  LoadYuvCoefs_WASMSIMD(yuvconstants, 0, &c);
  do {
    v128_t y, uv, rgb[6];
    ReadI422_WASMSIMD(src_y, src_u, src_v, width, &y, &uv);
    YuvToRgb16_WASMSIMD(y, uv, 0, &c, rgb);
    StoreARGB_WASMSIMD(rgb, ff, dst_argb, width);
    src_y += 16;
    src_u += 8;
    src_v += 8;
    dst_argb += 64;
    width -= 16;
  } while (width > 0);
}
#endif  // HAS_I422TOARGBROW_WASMSIMD

#ifdef HAS_I422TOAR30ROW_WASMSIMD
// Clamps B, G, R with 6 fractional bits to 10 bits and packs them into the
// low and high 16 bits of AR30, without the alpha bits.
static __inline void RgbToAR30_WASMSIMD(v128_t b,
                                        v128_t g,
                                        v128_t r,
                                        v128_t zero,
                                        v128_t k3ff0,
                                        v128_t* lo,
                                        v128_t* hi) {
  b = wasm_i16x8_max(wasm_i16x8_min(b, k3ff0), zero);
  g = wasm_i16x8_max(wasm_i16x8_min(g, k3ff0), zero);
  r = wasm_i16x8_max(wasm_i16x8_min(r, k3ff0), zero);
  *lo = wasm_v128_or(wasm_u16x8_shr(b, 4),
                     wasm_i16x8_shl(wasm_v128_and(g, k3ff0), 6));
  *hi = wasm_v128_or(wasm_v128_and(r, k3ff0), wasm_u16x8_shr(g, 10));
}

void I422ToAR30Row_WASMSIMD(const uint8_t* src_y,
                            const uint8_t* src_u,
                            const uint8_t* src_v,
                            uint8_t* dst_ar30,
                            const struct YuvConstants* yuvconstants,
                            int width) {
  struct YuvCoefs_WASMSIMD c;
  v128_t zero = SplatConst16(0);
  v128_t k3ff0 = SplatConst16(0x3ff0);
  v128_t kc000 = SplatConst16(0xc000);
  LoadYuvCoefs_WASMSIMD(yuvconstants, 24, &c);  // AR30 bias.
  do {
    v128_t y, uv, rgb[6], lo_e, hi_e, lo_o, hi_o;
    ReadI422_WASMSIMD(src_y, src_u, src_v, width, &y, &uv);
    YuvToRgb16_WASMSIMD(y, uv, 0, &c, rgb);
    RgbToAR30_WASMSIMD(rgb[0], rgb[2], rgb[4], zero, k3ff0, &lo_e, &hi_e);
    RgbToAR30_WASMSIMD(rgb[1], rgb[3], rgb[5], zero, k3ff0, &lo_o, &hi_o);
    hi_e = wasm_v128_or(hi_e, kc000);
    hi_o = wasm_v128_or(hi_o, kc000);
    // Even pixels 0, 2, 4, 6 and odd pixels 1, 3, 5, 7 as 32 bits.
    v128_t e = wasm_i16x8_shuffle(lo_e, hi_e, 0, 8, 1, 9, 2, 10, 3, 11);
    v128_t o = wasm_i16x8_shuffle(lo_o, hi_o, 0, 8, 1, 9, 2, 10, 3, 11);
    wasm_v128_store(dst_ar30, wasm_i32x4_shuffle(e, o, 0, 4, 1, 5));
    wasm_v128_store(dst_ar30 + 16, wasm_i32x4_shuffle(e, o, 2, 6, 3, 7));
    if (width >= 16) {
      e = wasm_i16x8_shuffle(lo_e, hi_e, 4, 12, 5, 13, 6, 14, 7, 15);
      o = wasm_i16x8_shuffle(lo_o, hi_o, 4, 12, 5, 13, 6, 14, 7, 15);
      wasm_v128_store(dst_ar30 + 32, wasm_i32x4_shuffle(e, o, 0, 4, 1, 5));
      wasm_v128_store(dst_ar30 + 48, wasm_i32x4_shuffle(e, o, 2, 6, 3, 7));
    }
    src_y += 16;
    src_u += 8;
    src_v += 8;
    dst_ar30 += 64;
    width -= 16;
  } while (width > 0);
}
#endif  // HAS_I422TOAR30ROW_WASMSIMD

#ifdef HAS_NV12TOARGBROW_WASMSIMD
void NV12ToARGBRow_WASMSIMD(const uint8_t* src_y,
                            const uint8_t* src_uv,
                            uint8_t* dst_argb,
                            const struct YuvConstants* yuvconstants,
                            int width) {
  struct YuvCoefs_WASMSIMD c;
  v128_t ff = SplatConst8(0xff);
  LoadYuvCoefs_WASMSIMD(yuvconstants, 0, &c);
  do {
    v128_t y, uv, rgb[6];
    ReadNV12_WASMSIMD(src_y, src_uv, width, &y, &uv);
    YuvToRgb16_WASMSIMD(y, uv, 0, &c, rgb);
    StoreARGB_WASMSIMD(rgb, ff, dst_argb, width);
    src_y += 16;
    src_uv += 16;
    dst_argb += 64;
    width -= 16;
  } while (width > 0);
}
#endif  // HAS_NV12TOARGBROW_WASMSIMD

#ifdef HAS_NV21TOARGBROW_WASMSIMD
void NV21ToARGBRow_WASMSIMD(const uint8_t* src_y,
                            const uint8_t* src_vu,
                            uint8_t* dst_argb,
                            const struct YuvConstants* yuvconstants,
                            int width) {
  struct YuvCoefs_WASMSIMD c;
  v128_t ff = SplatConst8(0xff);
  LoadYuvCoefs_WASMSIMD(yuvconstants, 0, &c);
  do {
    v128_t y, vu, rgb[6];
    ReadNV12_WASMSIMD(src_y, src_vu, width, &y, &vu);
    YuvToRgb16_WASMSIMD(y, vu, 1, &c, rgb);
    StoreARGB_WASMSIMD(rgb, ff, dst_argb, width);
    src_y += 16;
    src_vu += 16;
    dst_argb += 64;
    width -= 16;
  } while (width > 0);
}
#endif  // HAS_NV21TOARGBROW_WASMSIMD

#ifdef HAS_I422TORGB24ROW_WASMSIMD
// Swizzles that interleave B/G pairs and R into 3 vectors of RGB24. R is
// 8 even pixels followed by 8 odd pixels.
static const uint8_t kI422ToRGB24Swizzle[96] = {
    0,   1,   128, 2,   3,   128, 4,   5,   128, 6,   7,   128, 8,   9,
    128, 10,  128, 128, 0,   128, 128, 8,   128, 128, 1,   128, 128, 9,
    128, 128, 2,   128, 3,   128, 4,   5,   128, 6,   7,   128, 8,   9,
    128, 10,  11,  128, 12,  13,  128, 10,  128, 128, 3,   128, 128, 11,
    128, 128, 4,   128, 128, 12,  128, 128, 128, 6,   7,   128, 8,   9,
    128, 10,  11,  128, 12,  13,  128, 14,  15,  128, 5,   128, 128, 13,
    128, 128, 6,   128, 128, 14,  128, 128, 7,   128, 128, 15};

void I422ToRGB24Row_WASMSIMD(const uint8_t* src_y,
                             const uint8_t* src_u,
                             const uint8_t* src_v,
                             uint8_t* dst_rgb24,
                             const struct YuvConstants* yuvconstants,
                             int width) {
  struct YuvCoefs_WASMSIMD c;
  v128_t m0 = LoadConst(kI422ToRGB24Swizzle);
  v128_t m1 = LoadConst(kI422ToRGB24Swizzle + 16);
  v128_t m2 = LoadConst(kI422ToRGB24Swizzle + 32);
  v128_t m3 = LoadConst(kI422ToRGB24Swizzle + 48);
  v128_t m4 = LoadConst(kI422ToRGB24Swizzle + 64);
  v128_t m5 = LoadConst(kI422ToRGB24Swizzle + 80);
  LoadYuvCoefs_WASMSIMD(yuvconstants, 0, &c);
  do {
    v128_t y, uv, rgb[6];
    ReadI422_WASMSIMD(src_y, src_u, src_v, 16, &y, &uv);
    YuvToRgb16_WASMSIMD(y, uv, 0, &c, rgb);
    v128_t bg_e = wasm_u8x16_narrow_i16x8(wasm_i16x8_shr(rgb[0], 6),
                                          wasm_i16x8_shr(rgb[2], 6));
    v128_t bg_o = wasm_u8x16_narrow_i16x8(wasm_i16x8_shr(rgb[1], 6),
                                          wasm_i16x8_shr(rgb[3], 6));
    v128_t r = wasm_u8x16_narrow_i16x8(wasm_i16x8_shr(rgb[4], 6),
                                       wasm_i16x8_shr(rgb[5], 6));
    v128_t b = ZipLo8_WASMSIMD(bg_e, bg_o);
    v128_t g = ZipHi8_WASMSIMD(bg_e, bg_o);
    v128_t bg0 = ZipLo8_WASMSIMD(b, g);
    v128_t bg2 = ZipHi8_WASMSIMD(b, g);
    v128_t bg1 = wasm_i8x16_shuffle(bg0, bg2, 8, 9, 10, 11, 12, 13, 14, 15, 16,
                                    17, 18, 19, 20, 21, 22, 23);
    wasm_v128_store(dst_rgb24, wasm_v128_or(LIBYUV_WASM_SWIZZLE(bg0, m0),
                                            LIBYUV_WASM_SWIZZLE(r, m1)));
    wasm_v128_store(dst_rgb24 + 16, wasm_v128_or(LIBYUV_WASM_SWIZZLE(bg1, m2),
                                                 LIBYUV_WASM_SWIZZLE(r, m3)));
    wasm_v128_store(dst_rgb24 + 32, wasm_v128_or(LIBYUV_WASM_SWIZZLE(bg2, m4),
                                                 LIBYUV_WASM_SWIZZLE(r, m5)));
    src_y += 16;
    src_u += 8;
    src_v += 8;
    dst_rgb24 += 48;
    width -= 16;
  } while (width > 0);
}
#endif  // HAS_I422TORGB24ROW_WASMSIMD

#if defined(HAS_I422TORGB565ROW_WASMSIMD) ||   \
    defined(HAS_I422TOARGB1555ROW_WASMSIMD) || \
    defined(HAS_I422TOARGB4444ROW_WASMSIMD) || \
    defined(HAS_NV12TORGB565ROW_WASMSIMD)
// Clamps x with 6 fractional bits to 8 bits, keeps the top 'bits' and moves
// them to bit 'pos'.
static __inline v128_t PackChannel_WASMSIMD(v128_t x,
                                            v128_t zero,
                                            v128_t k3fff,
                                            int bits,
                                            int pos) {
  x = wasm_i16x8_max(wasm_i16x8_min(x, k3fff), zero);
  return wasm_i16x8_shl(wasm_u16x8_shr(x, 14 - bits), pos);
}

// Packs even/odd B, G, R into 16 bit pixels with the given channel sizes and
// stores 16 pixels, or 8 when width < 16. k = {0, 0x3fff, alpha} in 16-bit
// lanes. Alpha is set if the channels use fewer than 16 bits.
static __inline void Store16Bit_WASMSIMD(const v128_t* rgb,
                                         int bbits,
                                         int gbits,
                                         int rbits,
                                         const v128_t* k,
                                         uint8_t* dst,
                                         int width) {
  v128_t p[2];
  int i;
  for (i = 0; i < 2; ++i) {
    p[i] = wasm_v128_or(
        PackChannel_WASMSIMD(rgb[i], k[0], k[1], bbits, 0),
        wasm_v128_or(PackChannel_WASMSIMD(rgb[2 + i], k[0], k[1], gbits, bbits),
                     PackChannel_WASMSIMD(rgb[4 + i], k[0], k[1], rbits,
                                          bbits + gbits)));
    if (bbits + gbits + rbits < 16) {
      p[i] = wasm_v128_or(p[i], k[2]);
    }
  }
  wasm_v128_store(dst,
                  wasm_i16x8_shuffle(p[0], p[1], 0, 8, 1, 9, 2, 10, 3, 11));
  if (width >= 16) {
    wasm_v128_store(dst + 16,
                    wasm_i16x8_shuffle(p[0], p[1], 4, 12, 5, 13, 6, 14, 7, 15));
  }
}
#endif

#ifdef HAS_I422TORGB565ROW_WASMSIMD
void I422ToRGB565Row_WASMSIMD(const uint8_t* src_y,
                              const uint8_t* src_u,
                              const uint8_t* src_v,
                              uint8_t* dst_rgb565,
                              const struct YuvConstants* yuvconstants,
                              int width) {
  struct YuvCoefs_WASMSIMD c;
  v128_t k[2] = {SplatConst16(0), SplatConst16(0x3fff)};
  LoadYuvCoefs_WASMSIMD(yuvconstants, 0, &c);
  do {
    v128_t y, uv, rgb[6];
    ReadI422_WASMSIMD(src_y, src_u, src_v, width, &y, &uv);
    YuvToRgb16_WASMSIMD(y, uv, 0, &c, rgb);
    Store16Bit_WASMSIMD(rgb, 5, 6, 5, k, dst_rgb565, width);
    src_y += 16;
    src_u += 8;
    src_v += 8;
    dst_rgb565 += 32;
    width -= 16;
  } while (width > 0);
}
#endif  // HAS_I422TORGB565ROW_WASMSIMD

#ifdef HAS_I422TOARGB1555ROW_WASMSIMD
void I422ToARGB1555Row_WASMSIMD(const uint8_t* src_y,
                                const uint8_t* src_u,
                                const uint8_t* src_v,
                                uint8_t* dst_argb1555,
                                const struct YuvConstants* yuvconstants,
                                int width) {
  struct YuvCoefs_WASMSIMD c;
  v128_t k[3] = {SplatConst16(0), SplatConst16(0x3fff), SplatConst16(0x8000)};
  LoadYuvCoefs_WASMSIMD(yuvconstants, 0, &c);
  do {
    v128_t y, uv, rgb[6];
    ReadI422_WASMSIMD(src_y, src_u, src_v, width, &y, &uv);
    YuvToRgb16_WASMSIMD(y, uv, 0, &c, rgb);
    Store16Bit_WASMSIMD(rgb, 5, 5, 5, k, dst_argb1555, width);
    src_y += 16;
    src_u += 8;
    src_v += 8;
    dst_argb1555 += 32;
    width -= 16;
  } while (width > 0);
}
#endif  // HAS_I422TOARGB1555ROW_WASMSIMD

#ifdef HAS_I422TOARGB4444ROW_WASMSIMD
void I422ToARGB4444Row_WASMSIMD(const uint8_t* src_y,
                                const uint8_t* src_u,
                                const uint8_t* src_v,
                                uint8_t* dst_argb4444,
                                const struct YuvConstants* yuvconstants,
                                int width) {
  struct YuvCoefs_WASMSIMD c;
  v128_t k[3] = {SplatConst16(0), SplatConst16(0x3fff), SplatConst16(0xf000)};
  LoadYuvCoefs_WASMSIMD(yuvconstants, 0, &c);
  do {
    v128_t y, uv, rgb[6];
    ReadI422_WASMSIMD(src_y, src_u, src_v, width, &y, &uv);
    YuvToRgb16_WASMSIMD(y, uv, 0, &c, rgb);
    Store16Bit_WASMSIMD(rgb, 4, 4, 4, k, dst_argb4444, width);
    src_y += 16;
    src_u += 8;
    src_v += 8;
    dst_argb4444 += 32;
    width -= 16;
  } while (width > 0);
}
#endif  // HAS_I422TOARGB4444ROW_WASMSIMD

#ifdef HAS_NV12TORGB565ROW_WASMSIMD
void NV12ToRGB565Row_WASMSIMD(const uint8_t* src_y,
                              const uint8_t* src_uv,
                              uint8_t* dst_rgb565,
                              const struct YuvConstants* yuvconstants,
                              int width) {
  struct YuvCoefs_WASMSIMD c;
  v128_t k[2] = {SplatConst16(0), SplatConst16(0x3fff)};
  LoadYuvCoefs_WASMSIMD(yuvconstants, 0, &c);
  do {
    v128_t y, uv, rgb[6];
    ReadNV12_WASMSIMD(src_y, src_uv, width, &y, &uv);
    YuvToRgb16_WASMSIMD(y, uv, 0, &c, rgb);
    Store16Bit_WASMSIMD(rgb, 5, 6, 5, k, dst_rgb565, width);
    src_y += 16;
    src_uv += 16;
    dst_rgb565 += 32;
    width -= 16;
  } while (width > 0);
}
#endif  // HAS_NV12TORGB565ROW_WASMSIMD

#ifdef HAS_ARGBBLENDROW_WASMSIMD
// Broadcast 255 - alpha (from byte 3 of each pixel) to 16 bit lanes.
static const uint8_t kShuffleAlpha16[16] = {3,  128, 3,  128, 7,  128, 7,  128,
                                            11, 128, 11, 128, 15, 128, 15, 128};
void ARGBBlendRow_WASMSIMD(const uint8_t* src_argb,
                           const uint8_t* src_argb1,
                           uint8_t* dst_argb,
                           int width) {
  v128_t shuf_a = LoadConst(kShuffleAlpha16);
  v128_t one = SplatConst16(1);
  v128_t mask_lo = SplatConst16(0x00ff);
  v128_t mask_alpha = SplatConst32((int)0xff000000);

  while (width >= 4) {
    v128_t s0 = wasm_v128_load(src_argb);
    v128_t s1 = wasm_v128_load(src_argb1);
    src_argb += 16;
    src_argb1 += 16;

    // a = 256 - alpha.
    v128_t a = wasm_i16x8_add(
        LIBYUV_WASM_SWIZZLE(wasm_v128_xor(s0, mask_alpha), shuf_a), one);
    v128_t s1_lo =
        wasm_u16x8_shr(wasm_i16x8_mul(wasm_v128_and(s1, mask_lo), a), 8);
    v128_t s1_hi =
        wasm_v128_andnot(wasm_i16x8_mul(wasm_u16x8_shr(s1, 8), a), mask_lo);

    v128_t res = wasm_v128_or(s0, mask_alpha);
    res = wasm_u8x16_add_sat(res, s1_lo);
    res = wasm_u8x16_add_sat(res, s1_hi);

    wasm_v128_store(dst_argb, res);
    dst_argb += 16;
    width -= 4;
  }

  while (width > 0) {
    uint32_t s0 = *(const uint32_t*)src_argb;
    uint32_t s1 = *(const uint32_t*)src_argb1;
    src_argb += 4;
    src_argb1 += 4;

    uint32_t a = 256 - (s0 >> 24);
    uint32_t b = (((s1 & 0x000000ff) * a) >> 8) + (s0 & 0x000000ff);
    uint32_t g = (((s1 & 0x0000ff00) * a) >> 8) + (s0 & 0x0000ff00);
    uint32_t r = (((s1 & 0x00ff0000) >> 8) * a) + (s0 & 0x00ff0000);

    b = (b > 255) ? 255 : b;
    g = (g > 0xff00) ? 0xff00 : (g & 0xff00);
    r = (r > 0x00ff0000) ? 0x00ff0000 : (r & 0x00ff0000);

    *(uint32_t*)dst_argb = 0xff000000u | r | g | b;
    dst_argb += 4;
    --width;
  }
}
#endif  // HAS_ARGBBLENDROW_WASMSIMD

#ifdef HAS_BLENDPLANEROW_WASMSIMD
void BlendPlaneRow_WASMSIMD(const uint8_t* src0,
                            const uint8_t* src1,
                            const uint8_t* alpha,
                            uint8_t* dst,
                            int width) {
  // src0 * a + src1 * (255 - a) + 255 fits in 16 bits. 255 - a is a ^ 255.
  // The high bytes are the result (uzp2 on Arm).
  v128_t c255 = SplatConst16(255);
  v128_t ff = SplatConst8(255);
  do {
    v128_t s0 = wasm_v128_load(src0);
    v128_t s1 = wasm_v128_load(src1);
    v128_t a = wasm_v128_load(alpha);
    v128_t ia = wasm_v128_xor(a, ff);
    src0 += 16;
    src1 += 16;
    alpha += 16;
    v128_t lo = wasm_i16x8_add(wasm_u16x8_extmul_low_u8x16(s0, a),
                               wasm_u16x8_extmul_low_u8x16(s1, ia));
    v128_t hi = wasm_i16x8_add(wasm_u16x8_extmul_high_u8x16(s0, a),
                               wasm_u16x8_extmul_high_u8x16(s1, ia));
    lo = wasm_i16x8_add(lo, c255);
    hi = wasm_i16x8_add(hi, c255);
    wasm_v128_store(dst, wasm_i8x16_shuffle(lo, hi, 1, 3, 5, 7, 9, 11, 13, 15,
                                            17, 19, 21, 23, 25, 27, 29, 31));
    dst += 16;
    width -= 16;
  } while (width > 0);
}
#endif  // HAS_BLENDPLANEROW_WASMSIMD

#ifdef HAS_ARGBEXTRACTALPHAROW_WASMSIMD
void ARGBExtractAlphaRow_WASMSIMD(const uint8_t* src_argb,
                                  uint8_t* dst_a,
                                  int width) {
  do {
    asm("" : "+r"(src_argb), "+r"(dst_a));
    v128_t s0 = wasm_v128_load(src_argb);
    v128_t s1 = wasm_v128_load(src_argb + 16);
    v128_t s2 = wasm_v128_load(src_argb + 32);
    v128_t s3 = wasm_v128_load(src_argb + 48);
    s0 = wasm_u32x4_shr(s0, 24);
    s1 = wasm_u32x4_shr(s1, 24);
    s2 = wasm_u32x4_shr(s2, 24);
    s3 = wasm_u32x4_shr(s3, 24);
    v128_t a = wasm_u8x16_narrow_i16x8(wasm_i16x8_narrow_i32x4(s0, s1),
                                       wasm_i16x8_narrow_i32x4(s2, s3));
    wasm_v128_store(dst_a, a);
    src_argb += 64;
    dst_a += 16;
    width -= 16;
  } while (width > 0);
}
#endif  // HAS_ARGBEXTRACTALPHAROW_WASMSIMD

#ifdef HAS_ARGBCOPYYTOALPHAROW_WASMSIMD
// Swizzle Y[i] to the alpha byte of 32 bit lane i; 128 indices give 0.
static const uint8_t kYToAlphaSwizzleTable[64] = {
    128, 128, 128, 0,  128, 128, 128, 1,  128, 128, 128, 2,  128, 128, 128, 3,
    128, 128, 128, 4,  128, 128, 128, 5,  128, 128, 128, 6,  128, 128, 128, 7,
    128, 128, 128, 8,  128, 128, 128, 9,  128, 128, 128, 10, 128, 128, 128, 11,
    128, 128, 128, 12, 128, 128, 128, 13, 128, 128, 128, 14, 128, 128, 128, 15};

void ARGBCopyYToAlphaRow_WASMSIMD(const uint8_t* src, uint8_t* dst, int width) {
  v128_t rgb_mask = SplatConst32(0x00ffffff);
  v128_t idx0 = LoadConst(kYToAlphaSwizzleTable);
  v128_t idx1 = LoadConst(kYToAlphaSwizzleTable + 16);
  v128_t idx2 = LoadConst(kYToAlphaSwizzleTable + 32);
  v128_t idx3 = LoadConst(kYToAlphaSwizzleTable + 48);
  do {
    asm("" : "+r"(src), "+r"(dst));
    v128_t y = wasm_v128_load(src);
    v128_t d0 = wasm_v128_and(wasm_v128_load(dst), rgb_mask);
    v128_t d1 = wasm_v128_and(wasm_v128_load(dst + 16), rgb_mask);
    v128_t d2 = wasm_v128_and(wasm_v128_load(dst + 32), rgb_mask);
    v128_t d3 = wasm_v128_and(wasm_v128_load(dst + 48), rgb_mask);
    wasm_v128_store(dst, wasm_v128_or(d0, LIBYUV_WASM_SWIZZLE(y, idx0)));
    wasm_v128_store(dst + 16, wasm_v128_or(d1, LIBYUV_WASM_SWIZZLE(y, idx1)));
    wasm_v128_store(dst + 32, wasm_v128_or(d2, LIBYUV_WASM_SWIZZLE(y, idx2)));
    wasm_v128_store(dst + 48, wasm_v128_or(d3, LIBYUV_WASM_SWIZZLE(y, idx3)));
    src += 16;
    dst += 64;
    width -= 16;
  } while (width > 0);
}
#endif  // HAS_ARGBCOPYYTOALPHAROW_WASMSIMD

#undef LIBYUV_WASM_SWIZZLE

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif

#endif  // !defined(LIBYUV_DISABLE_WASM) && defined(__wasm_simd128__)
