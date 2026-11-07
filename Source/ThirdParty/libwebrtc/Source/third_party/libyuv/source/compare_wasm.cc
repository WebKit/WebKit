/*
 *  Copyright 2026 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/compare_row.h"
#include "libyuv/row.h"

#if !defined(LIBYUV_DISABLE_WASM) && defined(__wasm_simd128__)
#include <wasm_simd128.h>

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

// Add the 4 32 bit lanes.
static inline uint32_t SumLanes_WASMSIMD(v128_t sum) {
  sum = wasm_i32x4_add(sum, wasm_i64x2_shuffle(sum, sum, 1, 0));
  sum = wasm_i32x4_add(sum, wasm_i32x4_shuffle(sum, sum, 1, 0, 3, 2));
  return wasm_u32x4_extract_lane(sum, 0);
}

#if defined(HAS_HAMMINGDISTANCE_WASMSIMD)
// Bit counts of 64 bytes are added as bytes, then pairs of bytes are added
// to 16 bit sums, which hold up to 64 per 64 bytes, so count can be up to
// 65535. Count is a multiple of 64.
uint32_t HammingDistance_WASMSIMD(const uint8_t* src_a,
                                  const uint8_t* src_b,
                                  int count) {
  v128_t sum = wasm_i16x8_splat(0);
  while (count > 0) {
    v128_t c = wasm_i8x16_splat(0);
    int i;
    for (i = 0; i < 64; i += 16) {
      c = wasm_i8x16_add(
          c, wasm_i8x16_popcnt(wasm_v128_xor(wasm_v128_load(src_a + i),
                                             wasm_v128_load(src_b + i))));
    }
    sum = wasm_i16x8_add(sum, wasm_u16x8_extadd_pairwise_u8x16(c));
    src_a += 64;
    src_b += 64;
    count -= 64;
  }
  return SumLanes_WASMSIMD(wasm_u32x4_extadd_pairwise_u16x8(sum));
}
#endif  // HAS_HAMMINGDISTANCE_WASMSIMD

#if defined(HAS_SUMSQUAREERROR_WASMSIMD)
// Differences are widened to signed 16 bits and squared with i32x4.dot.
// Count is a multiple of 16.
uint32_t SumSquareError_WASMSIMD(const uint8_t* src_a,
                                 const uint8_t* src_b,
                                 int count) {
  v128_t sum = wasm_i32x4_splat(0);
  while (count > 0) {
    v128_t a = wasm_v128_load(src_a);
    v128_t b = wasm_v128_load(src_b);
    v128_t lo = wasm_i16x8_sub(wasm_u16x8_extend_low_u8x16(a),
                               wasm_u16x8_extend_low_u8x16(b));
    v128_t hi = wasm_i16x8_sub(wasm_u16x8_extend_high_u8x16(a),
                               wasm_u16x8_extend_high_u8x16(b));
    sum = wasm_i32x4_add(sum, wasm_i32x4_add(wasm_i32x4_dot_i16x8(lo, lo),
                                             wasm_i32x4_dot_i16x8(hi, hi)));
    src_a += 16;
    src_b += 16;
    count -= 16;
  }
  return SumLanes_WASMSIMD(sum);
}
#endif  // HAS_SUMSQUAREERROR_WASMSIMD

#if defined(HAS_HASHDJB2_WASMSIMD)
static const uint32_t kHashMul_WASMSIMD[24] = {
    // 33 ^ (30 - 2 * i) for byte pair i of 32 bytes.
    0xccc4cfc1, 0xd61beb81, 0x13791741, 0xac185301,  // 33 ^ 30 .. 24
    0xc8359ec1, 0x510cfa81, 0xb0da6641, 0x92d9e201,  // 33 ^ 22 .. 16
    0xa3476dc1, 0x4f5f0981, 0x855cb541, 0x747c7101,  // 33 ^ 14 .. 8
    0x4cfa3cc1, 0x00121881, 0x00000441, 0x00000001,  // 33 ^ 6 .. 0
    0x1137c401, 0x1137c401, 0x1137c401, 0x1137c401,  // 33 ^ 32
    0x00010021, 0x00010021, 0x00010021, 0x00010021,  // 16 bit 33, 1
};

// Sum of 16 bytes times 33 ^ n, as 4 lanes. Byte pairs are combined as
// src[2 * i] * 33 + src[2 * i + 1] with i32x4.dot, then multiplied by
// 33 ^ n for the pair.
static inline v128_t HashMul16_WASMSIMD(const uint8_t* src,
                                        const v128_t* m,
                                        v128_t k33) {
  v128_t s = wasm_v128_load(src);
  v128_t lo = wasm_i32x4_dot_i16x8(wasm_u16x8_extend_low_u8x16(s), k33);
  v128_t hi = wasm_i32x4_dot_i16x8(wasm_u16x8_extend_high_u8x16(s), k33);
  return wasm_i32x4_add(wasm_i32x4_mul(lo, m[0]), wasm_i32x4_mul(hi, m[1]));
}

// HashDjb2 of n bytes is hash * 33 ^ n + sum(src[i] * 33 ^ (n - 1 - i)).
// Products are summed per lane, multiplying the sums by 33 ^ 32 for each 32
// bytes, so one horizontal add is done at the end.
// Count is a multiple of 16.
uint32_t HashDjb2_WASMSIMD(const uint8_t* src, int count, uint32_t seed) {
  const v128_t* m = (const v128_t*)kHashMul_WASMSIMD;
  v128_t sum = wasm_i32x4_make(0, 0, 0, (int32_t)seed);
  // Load constants into Wasm locals outside the loop, instead of v128.const
  // in the loop.
  asm("" : "+r"(m));
  const v128_t k33_32 = m[4];
  const v128_t k33 = m[5];
  if (count & 16) {
    // Lane 3 of m[1] is 33 ^ 16, and lanes 0..2 of sum are 0.
    sum = wasm_i32x4_add(wasm_i32x4_mul(sum, m[1]),
                         HashMul16_WASMSIMD(src, m + 2, k33));
    src += 16;
    count -= 16;
  }
  while (count > 0) {
    sum = wasm_i32x4_add(
        wasm_i32x4_mul(sum, k33_32),
        wasm_i32x4_add(HashMul16_WASMSIMD(src, m, k33),
                       HashMul16_WASMSIMD(src + 16, m + 2, k33)));
    src += 32;
    count -= 32;
  }
  return SumLanes_WASMSIMD(sum);
}
#endif  // HAS_HASHDJB2_WASMSIMD

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif

#endif  // !defined(LIBYUV_DISABLE_WASM) && defined(__wasm_simd128__)
