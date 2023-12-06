/*
 *  Copyright (c) 2018, Alliance for Open Media. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AOM_AOM_DSP_ARM_MEM_NEON_H_
#define AOM_AOM_DSP_ARM_MEM_NEON_H_

#include <arm_neon.h>
#include <string.h>
#include "aom_dsp/aom_dsp_common.h"

// Support for xN Neon intrinsics is lacking in some compilers.
#if defined(__arm__) || defined(_M_ARM)
#define ARM_32_BIT
#endif

// DEFICIENT_CLANG_32_BIT includes clang-cl.
#if defined(__clang__) && defined(ARM_32_BIT) && \
    (__clang_major__ <= 6 || (defined(__ANDROID__) && __clang_major__ <= 7))
#define DEFICIENT_CLANG_32_BIT  // This includes clang-cl.
#endif

#if defined(__GNUC__) && !defined(__clang__) && defined(ARM_32_BIT)
#define GCC_32_BIT
#endif

#if defined(DEFICIENT_CLANG_32_BIT) || defined(GCC_32_BIT)

static INLINE uint8x16x3_t vld1q_u8_x3(const uint8_t *ptr) {
  uint8x16x3_t res = { { vld1q_u8(ptr + 0 * 16), vld1q_u8(ptr + 1 * 16),
                         vld1q_u8(ptr + 2 * 16) } };
  return res;
}

static INLINE uint8x16x2_t vld1q_u8_x2(const uint8_t *ptr) {
  uint8x16x2_t res = { { vld1q_u8(ptr + 0 * 16), vld1q_u8(ptr + 1 * 16) } };
  return res;
}

static INLINE uint16x8x2_t vld1q_u16_x2(const uint16_t *ptr) {
  uint16x8x2_t res = { { vld1q_u16(ptr + 0), vld1q_u16(ptr + 8) } };
  return res;
}

static INLINE uint16x8x4_t vld1q_u16_x4(const uint16_t *ptr) {
  uint16x8x4_t res = { { vld1q_u16(ptr + 0 * 8), vld1q_u16(ptr + 1 * 8),
                         vld1q_u16(ptr + 2 * 8), vld1q_u16(ptr + 3 * 8) } };
  return res;
}

#elif defined(__GNUC__) && !defined(__clang__)  // GCC 64-bit.
#if __GNUC__ < 8

static INLINE uint8x16x2_t vld1q_u8_x2(const uint8_t *ptr) {
  uint8x16x2_t res = { { vld1q_u8(ptr + 0 * 16), vld1q_u8(ptr + 1 * 16) } };
  return res;
}

static INLINE uint16x8x4_t vld1q_u16_x4(const uint16_t *ptr) {
  uint16x8x4_t res = { { vld1q_u16(ptr + 0 * 8), vld1q_u16(ptr + 1 * 8),
                         vld1q_u16(ptr + 2 * 8), vld1q_u16(ptr + 3 * 8) } };
  return res;
}
#endif  // __GNUC__ < 8

#if __GNUC__ < 9
static INLINE uint8x16x3_t vld1q_u8_x3(const uint8_t *ptr) {
  uint8x16x3_t res = { { vld1q_u8(ptr + 0 * 16), vld1q_u8(ptr + 1 * 16),
                         vld1q_u8(ptr + 2 * 16) } };
  return res;
}
#endif  // __GNUC__ < 9
#endif  // defined(__GNUC__) && !defined(__clang__)

static INLINE void store_u8_8x2(uint8_t *s, ptrdiff_t p, const uint8x8_t s0,
                                const uint8x8_t s1) {
  vst1_u8(s, s0);
  s += p;
  vst1_u8(s, s1);
  s += p;
}

static INLINE uint8x16_t load_u8_8x2(const uint8_t *s, ptrdiff_t p) {
  return vcombine_u8(vld1_u8(s), vld1_u8(s + p));
}

/* These intrinsics require immediate values, so we must use #defines
   to enforce that. */
#define load_u8_4x1(s, s0, lane)                                           \
  do {                                                                     \
    *(s0) = vreinterpret_u8_u32(                                           \
        vld1_lane_u32((uint32_t *)(s), vreinterpret_u32_u8(*(s0)), lane)); \
  } while (0)
#define load_u16_2x1(s, s0, lane)                                           \
  do {                                                                      \
    *(s0) = vreinterpret_u16_u32(                                           \
        vld1_lane_u32((uint32_t *)(s), vreinterpret_u32_u16(*(s0)), lane)); \
  } while (0)

// Load four bytes into the low half of a uint8x8_t, zero the upper half.
static INLINE uint8x8_t load_u8_4x1_lane0(const uint8_t *p) {
  uint8x8_t ret = vdup_n_u8(0);
  load_u8_4x1(p, &ret, 0);
  return ret;
}

static INLINE void load_u8_8x8(const uint8_t *s, ptrdiff_t p,
                               uint8x8_t *const s0, uint8x8_t *const s1,
                               uint8x8_t *const s2, uint8x8_t *const s3,
                               uint8x8_t *const s4, uint8x8_t *const s5,
                               uint8x8_t *const s6, uint8x8_t *const s7) {
  *s0 = vld1_u8(s);
  s += p;
  *s1 = vld1_u8(s);
  s += p;
  *s2 = vld1_u8(s);
  s += p;
  *s3 = vld1_u8(s);
  s += p;
  *s4 = vld1_u8(s);
  s += p;
  *s5 = vld1_u8(s);
  s += p;
  *s6 = vld1_u8(s);
  s += p;
  *s7 = vld1_u8(s);
}

static INLINE void load_u8_8x7(const uint8_t *s, ptrdiff_t p,
                               uint8x8_t *const s0, uint8x8_t *const s1,
                               uint8x8_t *const s2, uint8x8_t *const s3,
                               uint8x8_t *const s4, uint8x8_t *const s5,
                               uint8x8_t *const s6) {
  *s0 = vld1_u8(s);
  s += p;
  *s1 = vld1_u8(s);
  s += p;
  *s2 = vld1_u8(s);
  s += p;
  *s3 = vld1_u8(s);
  s += p;
  *s4 = vld1_u8(s);
  s += p;
  *s5 = vld1_u8(s);
  s += p;
  *s6 = vld1_u8(s);
}

static INLINE void load_u8_8x4(const uint8_t *s, const ptrdiff_t p,
                               uint8x8_t *const s0, uint8x8_t *const s1,
                               uint8x8_t *const s2, uint8x8_t *const s3) {
  *s0 = vld1_u8(s);
  s += p;
  *s1 = vld1_u8(s);
  s += p;
  *s2 = vld1_u8(s);
  s += p;
  *s3 = vld1_u8(s);
}

static INLINE void load_u16_4x4(const uint16_t *s, const ptrdiff_t p,
                                uint16x4_t *const s0, uint16x4_t *const s1,
                                uint16x4_t *const s2, uint16x4_t *const s3) {
  *s0 = vld1_u16(s);
  s += p;
  *s1 = vld1_u16(s);
  s += p;
  *s2 = vld1_u16(s);
  s += p;
  *s3 = vld1_u16(s);
  s += p;
}

static INLINE void load_u16_4x7(const uint16_t *s, ptrdiff_t p,
                                uint16x4_t *const s0, uint16x4_t *const s1,
                                uint16x4_t *const s2, uint16x4_t *const s3,
                                uint16x4_t *const s4, uint16x4_t *const s5,
                                uint16x4_t *const s6) {
  *s0 = vld1_u16(s);
  s += p;
  *s1 = vld1_u16(s);
  s += p;
  *s2 = vld1_u16(s);
  s += p;
  *s3 = vld1_u16(s);
  s += p;
  *s4 = vld1_u16(s);
  s += p;
  *s5 = vld1_u16(s);
  s += p;
  *s6 = vld1_u16(s);
}

static INLINE void load_s16_8x2(const int16_t *s, const ptrdiff_t p,
                                int16x8_t *const s0, int16x8_t *const s1) {
  *s0 = vld1q_s16(s);
  s += p;
  *s1 = vld1q_s16(s);
}

static INLINE void load_u16_8x2(const uint16_t *s, const ptrdiff_t p,
                                uint16x8_t *const s0, uint16x8_t *const s1) {
  *s0 = vld1q_u16(s);
  s += p;
  *s1 = vld1q_u16(s);
}

static INLINE void load_u16_8x4(const uint16_t *s, const ptrdiff_t p,
                                uint16x8_t *const s0, uint16x8_t *const s1,
                                uint16x8_t *const s2, uint16x8_t *const s3) {
  *s0 = vld1q_u16(s);
  s += p;
  *s1 = vld1q_u16(s);
  s += p;
  *s2 = vld1q_u16(s);
  s += p;
  *s3 = vld1q_u16(s);
  s += p;
}

static INLINE void load_s16_4x12(const int16_t *s, ptrdiff_t p,
                                 int16x4_t *const s0, int16x4_t *const s1,
                                 int16x4_t *const s2, int16x4_t *const s3,
                                 int16x4_t *const s4, int16x4_t *const s5,
                                 int16x4_t *const s6, int16x4_t *const s7,
                                 int16x4_t *const s8, int16x4_t *const s9,
                                 int16x4_t *const s10, int16x4_t *const s11) {
  *s0 = vld1_s16(s);
  s += p;
  *s1 = vld1_s16(s);
  s += p;
  *s2 = vld1_s16(s);
  s += p;
  *s3 = vld1_s16(s);
  s += p;
  *s4 = vld1_s16(s);
  s += p;
  *s5 = vld1_s16(s);
  s += p;
  *s6 = vld1_s16(s);
  s += p;
  *s7 = vld1_s16(s);
  s += p;
  *s8 = vld1_s16(s);
  s += p;
  *s9 = vld1_s16(s);
  s += p;
  *s10 = vld1_s16(s);
  s += p;
  *s11 = vld1_s16(s);
}

static INLINE void load_s16_4x11(const int16_t *s, ptrdiff_t p,
                                 int16x4_t *const s0, int16x4_t *const s1,
                                 int16x4_t *const s2, int16x4_t *const s3,
                                 int16x4_t *const s4, int16x4_t *const s5,
                                 int16x4_t *const s6, int16x4_t *const s7,
                                 int16x4_t *const s8, int16x4_t *const s9,
                                 int16x4_t *const s10) {
  *s0 = vld1_s16(s);
  s += p;
  *s1 = vld1_s16(s);
  s += p;
  *s2 = vld1_s16(s);
  s += p;
  *s3 = vld1_s16(s);
  s += p;
  *s4 = vld1_s16(s);
  s += p;
  *s5 = vld1_s16(s);
  s += p;
  *s6 = vld1_s16(s);
  s += p;
  *s7 = vld1_s16(s);
  s += p;
  *s8 = vld1_s16(s);
  s += p;
  *s9 = vld1_s16(s);
  s += p;
  *s10 = vld1_s16(s);
}

static INLINE void load_u16_4x11(const uint16_t *s, ptrdiff_t p,
                                 uint16x4_t *const s0, uint16x4_t *const s1,
                                 uint16x4_t *const s2, uint16x4_t *const s3,
                                 uint16x4_t *const s4, uint16x4_t *const s5,
                                 uint16x4_t *const s6, uint16x4_t *const s7,
                                 uint16x4_t *const s8, uint16x4_t *const s9,
                                 uint16x4_t *const s10) {
  *s0 = vld1_u16(s);
  s += p;
  *s1 = vld1_u16(s);
  s += p;
  *s2 = vld1_u16(s);
  s += p;
  *s3 = vld1_u16(s);
  s += p;
  *s4 = vld1_u16(s);
  s += p;
  *s5 = vld1_u16(s);
  s += p;
  *s6 = vld1_u16(s);
  s += p;
  *s7 = vld1_u16(s);
  s += p;
  *s8 = vld1_u16(s);
  s += p;
  *s9 = vld1_u16(s);
  s += p;
  *s10 = vld1_u16(s);
}

static INLINE void load_s16_4x8(const int16_t *s, ptrdiff_t p,
                                int16x4_t *const s0, int16x4_t *const s1,
                                int16x4_t *const s2, int16x4_t *const s3,
                                int16x4_t *const s4, int16x4_t *const s5,
                                int16x4_t *const s6, int16x4_t *const s7) {
  *s0 = vld1_s16(s);
  s += p;
  *s1 = vld1_s16(s);
  s += p;
  *s2 = vld1_s16(s);
  s += p;
  *s3 = vld1_s16(s);
  s += p;
  *s4 = vld1_s16(s);
  s += p;
  *s5 = vld1_s16(s);
  s += p;
  *s6 = vld1_s16(s);
  s += p;
  *s7 = vld1_s16(s);
}

static INLINE void load_s16_4x7(const int16_t *s, ptrdiff_t p,
                                int16x4_t *const s0, int16x4_t *const s1,
                                int16x4_t *const s2, int16x4_t *const s3,
                                int16x4_t *const s4, int16x4_t *const s5,
                                int16x4_t *const s6) {
  *s0 = vld1_s16(s);
  s += p;
  *s1 = vld1_s16(s);
  s += p;
  *s2 = vld1_s16(s);
  s += p;
  *s3 = vld1_s16(s);
  s += p;
  *s4 = vld1_s16(s);
  s += p;
  *s5 = vld1_s16(s);
  s += p;
  *s6 = vld1_s16(s);
}

static INLINE void load_s16_4x6(const int16_t *s, ptrdiff_t p,
                                int16x4_t *const s0, int16x4_t *const s1,
                                int16x4_t *const s2, int16x4_t *const s3,
                                int16x4_t *const s4, int16x4_t *const s5) {
  *s0 = vld1_s16(s);
  s += p;
  *s1 = vld1_s16(s);
  s += p;
  *s2 = vld1_s16(s);
  s += p;
  *s3 = vld1_s16(s);
  s += p;
  *s4 = vld1_s16(s);
  s += p;
  *s5 = vld1_s16(s);
}

static INLINE void load_s16_4x5(const int16_t *s, ptrdiff_t p,
                                int16x4_t *const s0, int16x4_t *const s1,
                                int16x4_t *const s2, int16x4_t *const s3,
                                int16x4_t *const s4) {
  *s0 = vld1_s16(s);
  s += p;
  *s1 = vld1_s16(s);
  s += p;
  *s2 = vld1_s16(s);
  s += p;
  *s3 = vld1_s16(s);
  s += p;
  *s4 = vld1_s16(s);
}

static INLINE void load_u16_4x5(const uint16_t *s, const ptrdiff_t p,
                                uint16x4_t *const s0, uint16x4_t *const s1,
                                uint16x4_t *const s2, uint16x4_t *const s3,
                                uint16x4_t *const s4) {
  *s0 = vld1_u16(s);
  s += p;
  *s1 = vld1_u16(s);
  s += p;
  *s2 = vld1_u16(s);
  s += p;
  *s3 = vld1_u16(s);
  s += p;
  *s4 = vld1_u16(s);
  s += p;
}

static INLINE void load_u8_8x5(const uint8_t *s, ptrdiff_t p,
                               uint8x8_t *const s0, uint8x8_t *const s1,
                               uint8x8_t *const s2, uint8x8_t *const s3,
                               uint8x8_t *const s4) {
  *s0 = vld1_u8(s);
  s += p;
  *s1 = vld1_u8(s);
  s += p;
  *s2 = vld1_u8(s);
  s += p;
  *s3 = vld1_u8(s);
  s += p;
  *s4 = vld1_u8(s);
}

static INLINE void load_u16_8x5(const uint16_t *s, const ptrdiff_t p,
                                uint16x8_t *const s0, uint16x8_t *const s1,
                                uint16x8_t *const s2, uint16x8_t *const s3,
                                uint16x8_t *const s4) {
  *s0 = vld1q_u16(s);
  s += p;
  *s1 = vld1q_u16(s);
  s += p;
  *s2 = vld1q_u16(s);
  s += p;
  *s3 = vld1q_u16(s);
  s += p;
  *s4 = vld1q_u16(s);
  s += p;
}

static INLINE void load_s16_4x4(const int16_t *s, ptrdiff_t p,
                                int16x4_t *const s0, int16x4_t *const s1,
                                int16x4_t *const s2, int16x4_t *const s3) {
  *s0 = vld1_s16(s);
  s += p;
  *s1 = vld1_s16(s);
  s += p;
  *s2 = vld1_s16(s);
  s += p;
  *s3 = vld1_s16(s);
}

/* These intrinsics require immediate values, so we must use #defines
   to enforce that. */
#define store_u8_2x1(s, s0, lane)                                  \
  do {                                                             \
    vst1_lane_u16((uint16_t *)(s), vreinterpret_u16_u8(s0), lane); \
  } while (0)

#define store_u8_4x1(s, s0, lane)                                  \
  do {                                                             \
    vst1_lane_u32((uint32_t *)(s), vreinterpret_u32_u8(s0), lane); \
  } while (0)

static INLINE void store_u8_8x8(uint8_t *s, ptrdiff_t p, const uint8x8_t s0,
                                const uint8x8_t s1, const uint8x8_t s2,
                                const uint8x8_t s3, const uint8x8_t s4,
                                const uint8x8_t s5, const uint8x8_t s6,
                                const uint8x8_t s7) {
  vst1_u8(s, s0);
  s += p;
  vst1_u8(s, s1);
  s += p;
  vst1_u8(s, s2);
  s += p;
  vst1_u8(s, s3);
  s += p;
  vst1_u8(s, s4);
  s += p;
  vst1_u8(s, s5);
  s += p;
  vst1_u8(s, s6);
  s += p;
  vst1_u8(s, s7);
}

static INLINE void store_u8_8x4(uint8_t *s, ptrdiff_t p, const uint8x8_t s0,
                                const uint8x8_t s1, const uint8x8_t s2,
                                const uint8x8_t s3) {
  vst1_u8(s, s0);
  s += p;
  vst1_u8(s, s1);
  s += p;
  vst1_u8(s, s2);
  s += p;
  vst1_u8(s, s3);
}

static INLINE void store_u8_8x16(uint8_t *s, ptrdiff_t p, const uint8x16_t s0,
                                 const uint8x16_t s1, const uint8x16_t s2,
                                 const uint8x16_t s3) {
  vst1q_u8(s, s0);
  s += p;
  vst1q_u8(s, s1);
  s += p;
  vst1q_u8(s, s2);
  s += p;
  vst1q_u8(s, s3);
}

static INLINE void store_u16_8x8(uint16_t *s, ptrdiff_t dst_stride,
                                 const uint16x8_t s0, const uint16x8_t s1,
                                 const uint16x8_t s2, const uint16x8_t s3,
                                 const uint16x8_t s4, const uint16x8_t s5,
                                 const uint16x8_t s6, const uint16x8_t s7) {
  vst1q_u16(s, s0);
  s += dst_stride;
  vst1q_u16(s, s1);
  s += dst_stride;
  vst1q_u16(s, s2);
  s += dst_stride;
  vst1q_u16(s, s3);
  s += dst_stride;
  vst1q_u16(s, s4);
  s += dst_stride;
  vst1q_u16(s, s5);
  s += dst_stride;
  vst1q_u16(s, s6);
  s += dst_stride;
  vst1q_u16(s, s7);
}

static INLINE void store_u16_4x4(uint16_t *s, ptrdiff_t dst_stride,
                                 const uint16x4_t s0, const uint16x4_t s1,
                                 const uint16x4_t s2, const uint16x4_t s3) {
  vst1_u16(s, s0);
  s += dst_stride;
  vst1_u16(s, s1);
  s += dst_stride;
  vst1_u16(s, s2);
  s += dst_stride;
  vst1_u16(s, s3);
}

static INLINE void store_u16_8x2(uint16_t *s, ptrdiff_t dst_stride,
                                 const uint16x8_t s0, const uint16x8_t s1) {
  vst1q_u16(s, s0);
  s += dst_stride;
  vst1q_u16(s, s1);
}

static INLINE void store_u16_8x4(uint16_t *s, ptrdiff_t dst_stride,
                                 const uint16x8_t s0, const uint16x8_t s1,
                                 const uint16x8_t s2, const uint16x8_t s3) {
  vst1q_u16(s, s0);
  s += dst_stride;
  vst1q_u16(s, s1);
  s += dst_stride;
  vst1q_u16(s, s2);
  s += dst_stride;
  vst1q_u16(s, s3);
}

static INLINE void store_s16_8x8(int16_t *s, ptrdiff_t dst_stride,
                                 const int16x8_t s0, const int16x8_t s1,
                                 const int16x8_t s2, const int16x8_t s3,
                                 const int16x8_t s4, const int16x8_t s5,
                                 const int16x8_t s6, const int16x8_t s7) {
  vst1q_s16(s, s0);
  s += dst_stride;
  vst1q_s16(s, s1);
  s += dst_stride;
  vst1q_s16(s, s2);
  s += dst_stride;
  vst1q_s16(s, s3);
  s += dst_stride;
  vst1q_s16(s, s4);
  s += dst_stride;
  vst1q_s16(s, s5);
  s += dst_stride;
  vst1q_s16(s, s6);
  s += dst_stride;
  vst1q_s16(s, s7);
}

static INLINE void store_s16_4x4(int16_t *s, ptrdiff_t dst_stride,
                                 const int16x4_t s0, const int16x4_t s1,
                                 const int16x4_t s2, const int16x4_t s3) {
  vst1_s16(s, s0);
  s += dst_stride;
  vst1_s16(s, s1);
  s += dst_stride;
  vst1_s16(s, s2);
  s += dst_stride;
  vst1_s16(s, s3);
}

/* These intrinsics require immediate values, so we must use #defines
   to enforce that. */
#define store_s16_2x1(s, s0, lane)                                 \
  do {                                                             \
    vst1_lane_s32((int32_t *)(s), vreinterpret_s32_s16(s0), lane); \
  } while (0)
#define store_u16_2x1(s, s0, lane)                                  \
  do {                                                              \
    vst1_lane_u32((uint32_t *)(s), vreinterpret_u32_u16(s0), lane); \
  } while (0)
#define store_u16q_2x1(s, s0, lane)                                   \
  do {                                                                \
    vst1q_lane_u32((uint32_t *)(s), vreinterpretq_u32_u16(s0), lane); \
  } while (0)

static INLINE void store_s16_8x4(int16_t *s, ptrdiff_t dst_stride,
                                 const int16x8_t s0, const int16x8_t s1,
                                 const int16x8_t s2, const int16x8_t s3) {
  vst1q_s16(s, s0);
  s += dst_stride;
  vst1q_s16(s, s1);
  s += dst_stride;
  vst1q_s16(s, s2);
  s += dst_stride;
  vst1q_s16(s, s3);
}

static INLINE void load_u8_8x11(const uint8_t *s, ptrdiff_t p,
                                uint8x8_t *const s0, uint8x8_t *const s1,
                                uint8x8_t *const s2, uint8x8_t *const s3,
                                uint8x8_t *const s4, uint8x8_t *const s5,
                                uint8x8_t *const s6, uint8x8_t *const s7,
                                uint8x8_t *const s8, uint8x8_t *const s9,
                                uint8x8_t *const s10) {
  *s0 = vld1_u8(s);
  s += p;
  *s1 = vld1_u8(s);
  s += p;
  *s2 = vld1_u8(s);
  s += p;
  *s3 = vld1_u8(s);
  s += p;
  *s4 = vld1_u8(s);
  s += p;
  *s5 = vld1_u8(s);
  s += p;
  *s6 = vld1_u8(s);
  s += p;
  *s7 = vld1_u8(s);
  s += p;
  *s8 = vld1_u8(s);
  s += p;
  *s9 = vld1_u8(s);
  s += p;
  *s10 = vld1_u8(s);
}

static INLINE void load_s16_8x10(const int16_t *s, ptrdiff_t p,
                                 int16x8_t *const s0, int16x8_t *const s1,
                                 int16x8_t *const s2, int16x8_t *const s3,
                                 int16x8_t *const s4, int16x8_t *const s5,
                                 int16x8_t *const s6, int16x8_t *const s7,
                                 int16x8_t *const s8, int16x8_t *const s9) {
  *s0 = vld1q_s16(s);
  s += p;
  *s1 = vld1q_s16(s);
  s += p;
  *s2 = vld1q_s16(s);
  s += p;
  *s3 = vld1q_s16(s);
  s += p;
  *s4 = vld1q_s16(s);
  s += p;
  *s5 = vld1q_s16(s);
  s += p;
  *s6 = vld1q_s16(s);
  s += p;
  *s7 = vld1q_s16(s);
  s += p;
  *s8 = vld1q_s16(s);
  s += p;
  *s9 = vld1q_s16(s);
}

static INLINE void load_s16_8x11(const int16_t *s, ptrdiff_t p,
                                 int16x8_t *const s0, int16x8_t *const s1,
                                 int16x8_t *const s2, int16x8_t *const s3,
                                 int16x8_t *const s4, int16x8_t *const s5,
                                 int16x8_t *const s6, int16x8_t *const s7,
                                 int16x8_t *const s8, int16x8_t *const s9,
                                 int16x8_t *const s10) {
  *s0 = vld1q_s16(s);
  s += p;
  *s1 = vld1q_s16(s);
  s += p;
  *s2 = vld1q_s16(s);
  s += p;
  *s3 = vld1q_s16(s);
  s += p;
  *s4 = vld1q_s16(s);
  s += p;
  *s5 = vld1q_s16(s);
  s += p;
  *s6 = vld1q_s16(s);
  s += p;
  *s7 = vld1q_s16(s);
  s += p;
  *s8 = vld1q_s16(s);
  s += p;
  *s9 = vld1q_s16(s);
  s += p;
  *s10 = vld1q_s16(s);
}

static INLINE void load_s16_8x12(const int16_t *s, ptrdiff_t p,
                                 int16x8_t *const s0, int16x8_t *const s1,
                                 int16x8_t *const s2, int16x8_t *const s3,
                                 int16x8_t *const s4, int16x8_t *const s5,
                                 int16x8_t *const s6, int16x8_t *const s7,
                                 int16x8_t *const s8, int16x8_t *const s9,
                                 int16x8_t *const s10, int16x8_t *const s11) {
  *s0 = vld1q_s16(s);
  s += p;
  *s1 = vld1q_s16(s);
  s += p;
  *s2 = vld1q_s16(s);
  s += p;
  *s3 = vld1q_s16(s);
  s += p;
  *s4 = vld1q_s16(s);
  s += p;
  *s5 = vld1q_s16(s);
  s += p;
  *s6 = vld1q_s16(s);
  s += p;
  *s7 = vld1q_s16(s);
  s += p;
  *s8 = vld1q_s16(s);
  s += p;
  *s9 = vld1q_s16(s);
  s += p;
  *s10 = vld1q_s16(s);
  s += p;
  *s11 = vld1q_s16(s);
}

static INLINE void load_u16_8x11(const uint16_t *s, ptrdiff_t p,
                                 uint16x8_t *const s0, uint16x8_t *const s1,
                                 uint16x8_t *const s2, uint16x8_t *const s3,
                                 uint16x8_t *const s4, uint16x8_t *const s5,
                                 uint16x8_t *const s6, uint16x8_t *const s7,
                                 uint16x8_t *const s8, uint16x8_t *const s9,
                                 uint16x8_t *const s10) {
  *s0 = vld1q_u16(s);
  s += p;
  *s1 = vld1q_u16(s);
  s += p;
  *s2 = vld1q_u16(s);
  s += p;
  *s3 = vld1q_u16(s);
  s += p;
  *s4 = vld1q_u16(s);
  s += p;
  *s5 = vld1q_u16(s);
  s += p;
  *s6 = vld1q_u16(s);
  s += p;
  *s7 = vld1q_u16(s);
  s += p;
  *s8 = vld1q_u16(s);
  s += p;
  *s9 = vld1q_u16(s);
  s += p;
  *s10 = vld1q_u16(s);
}

static INLINE void load_s16_8x8(const int16_t *s, ptrdiff_t p,
                                int16x8_t *const s0, int16x8_t *const s1,
                                int16x8_t *const s2, int16x8_t *const s3,
                                int16x8_t *const s4, int16x8_t *const s5,
                                int16x8_t *const s6, int16x8_t *const s7) {
  *s0 = vld1q_s16(s);
  s += p;
  *s1 = vld1q_s16(s);
  s += p;
  *s2 = vld1q_s16(s);
  s += p;
  *s3 = vld1q_s16(s);
  s += p;
  *s4 = vld1q_s16(s);
  s += p;
  *s5 = vld1q_s16(s);
  s += p;
  *s6 = vld1q_s16(s);
  s += p;
  *s7 = vld1q_s16(s);
}

static INLINE void load_u16_8x7(const uint16_t *s, ptrdiff_t p,
                                uint16x8_t *const s0, uint16x8_t *const s1,
                                uint16x8_t *const s2, uint16x8_t *const s3,
                                uint16x8_t *const s4, uint16x8_t *const s5,
                                uint16x8_t *const s6) {
  *s0 = vld1q_u16(s);
  s += p;
  *s1 = vld1q_u16(s);
  s += p;
  *s2 = vld1q_u16(s);
  s += p;
  *s3 = vld1q_u16(s);
  s += p;
  *s4 = vld1q_u16(s);
  s += p;
  *s5 = vld1q_u16(s);
  s += p;
  *s6 = vld1q_u16(s);
}

static INLINE void load_s16_8x7(const int16_t *s, ptrdiff_t p,
                                int16x8_t *const s0, int16x8_t *const s1,
                                int16x8_t *const s2, int16x8_t *const s3,
                                int16x8_t *const s4, int16x8_t *const s5,
                                int16x8_t *const s6) {
  *s0 = vld1q_s16(s);
  s += p;
  *s1 = vld1q_s16(s);
  s += p;
  *s2 = vld1q_s16(s);
  s += p;
  *s3 = vld1q_s16(s);
  s += p;
  *s4 = vld1q_s16(s);
  s += p;
  *s5 = vld1q_s16(s);
  s += p;
  *s6 = vld1q_s16(s);
}

static INLINE void load_s16_8x6(const int16_t *s, ptrdiff_t p,
                                int16x8_t *const s0, int16x8_t *const s1,
                                int16x8_t *const s2, int16x8_t *const s3,
                                int16x8_t *const s4, int16x8_t *const s5) {
  *s0 = vld1q_s16(s);
  s += p;
  *s1 = vld1q_s16(s);
  s += p;
  *s2 = vld1q_s16(s);
  s += p;
  *s3 = vld1q_s16(s);
  s += p;
  *s4 = vld1q_s16(s);
  s += p;
  *s5 = vld1q_s16(s);
}

static INLINE void load_s16_8x5(const int16_t *s, ptrdiff_t p,
                                int16x8_t *const s0, int16x8_t *const s1,
                                int16x8_t *const s2, int16x8_t *const s3,
                                int16x8_t *const s4) {
  *s0 = vld1q_s16(s);
  s += p;
  *s1 = vld1q_s16(s);
  s += p;
  *s2 = vld1q_s16(s);
  s += p;
  *s3 = vld1q_s16(s);
  s += p;
  *s4 = vld1q_s16(s);
}

static INLINE void load_s16_8x4(const int16_t *s, ptrdiff_t p,
                                int16x8_t *const s0, int16x8_t *const s1,
                                int16x8_t *const s2, int16x8_t *const s3) {
  *s0 = vld1q_s16(s);
  s += p;
  *s1 = vld1q_s16(s);
  s += p;
  *s2 = vld1q_s16(s);
  s += p;
  *s3 = vld1q_s16(s);
}

// Load 2 sets of 4 bytes when alignment is not guaranteed.
static INLINE uint8x8_t load_unaligned_u8(const uint8_t *buf, int stride) {
  uint32_t a;
  memcpy(&a, buf, 4);
  buf += stride;
  uint32x2_t a_u32 = vdup_n_u32(a);
  memcpy(&a, buf, 4);
  a_u32 = vset_lane_u32(a, a_u32, 1);
  return vreinterpret_u8_u32(a_u32);
}

// Load 4 sets of 4 bytes when alignment is not guaranteed.
static INLINE uint8x16_t load_unaligned_u8q(const uint8_t *buf, int stride) {
  uint32_t a;
  uint32x4_t a_u32;
  if (stride == 4) return vld1q_u8(buf);
  memcpy(&a, buf, 4);
  buf += stride;
  a_u32 = vdupq_n_u32(a);
  memcpy(&a, buf, 4);
  buf += stride;
  a_u32 = vsetq_lane_u32(a, a_u32, 1);
  memcpy(&a, buf, 4);
  buf += stride;
  a_u32 = vsetq_lane_u32(a, a_u32, 2);
  memcpy(&a, buf, 4);
  a_u32 = vsetq_lane_u32(a, a_u32, 3);
  return vreinterpretq_u8_u32(a_u32);
}

static INLINE uint8x8_t load_unaligned_u8_2x2(const uint8_t *buf, int stride) {
  uint16_t a;
  uint16x4_t a_u16;

  memcpy(&a, buf, 2);
  buf += stride;
  a_u16 = vdup_n_u16(a);
  memcpy(&a, buf, 2);
  a_u16 = vset_lane_u16(a, a_u16, 1);
  return vreinterpret_u8_u16(a_u16);
}

static INLINE uint8x8_t load_unaligned_u8_4x1(const uint8_t *buf) {
  uint32_t a;
  uint32x2_t a_u32;

  memcpy(&a, buf, 4);
  a_u32 = vdup_n_u32(0);
  a_u32 = vset_lane_u32(a, a_u32, 0);
  return vreinterpret_u8_u32(a_u32);
}

static INLINE uint8x8_t load_unaligned_dup_u8_4x2(const uint8_t *buf) {
  uint32_t a;
  uint32x2_t a_u32;

  memcpy(&a, buf, 4);
  a_u32 = vdup_n_u32(a);
  return vreinterpret_u8_u32(a_u32);
}

static INLINE uint8x8_t load_unaligned_dup_u8_2x4(const uint8_t *buf) {
  uint16_t a;
  uint16x4_t a_u32;

  memcpy(&a, buf, 2);
  a_u32 = vdup_n_u16(a);
  return vreinterpret_u8_u16(a_u32);
}

static INLINE uint8x8_t load_unaligned_u8_4x2(const uint8_t *buf, int stride) {
  uint32_t a;
  uint32x2_t a_u32;

  memcpy(&a, buf, 4);
  buf += stride;
  a_u32 = vdup_n_u32(a);
  memcpy(&a, buf, 4);
  a_u32 = vset_lane_u32(a, a_u32, 1);
  return vreinterpret_u8_u32(a_u32);
}

static INLINE void load_unaligned_u8_4x4(const uint8_t *buf, int stride,
                                         uint8x8_t *tu0, uint8x8_t *tu1) {
  *tu0 = load_unaligned_u8_4x2(buf, stride);
  buf += 2 * stride;
  *tu1 = load_unaligned_u8_4x2(buf, stride);
}

static INLINE void load_unaligned_u8_3x8(const uint8_t *buf, int stride,
                                         uint8x8_t *tu0, uint8x8_t *tu1,
                                         uint8x8_t *tu2) {
  load_unaligned_u8_4x4(buf, stride, tu0, tu1);
  buf += 4 * stride;
  *tu2 = load_unaligned_u8_4x2(buf, stride);
}

static INLINE void load_unaligned_u8_4x8(const uint8_t *buf, int stride,
                                         uint8x8_t *tu0, uint8x8_t *tu1,
                                         uint8x8_t *tu2, uint8x8_t *tu3) {
  load_unaligned_u8_4x4(buf, stride, tu0, tu1);
  buf += 4 * stride;
  load_unaligned_u8_4x4(buf, stride, tu2, tu3);
}

/* These intrinsics require immediate values, so we must use #defines
   to enforce that. */
#define store_unaligned_u8_4x1(dst, src, lane)         \
  do {                                                 \
    uint32_t a;                                        \
    a = vget_lane_u32(vreinterpret_u32_u8(src), lane); \
    memcpy(dst, &a, 4);                                \
  } while (0)

#define store_unaligned_u8_2x1(dst, src, lane)         \
  do {                                                 \
    uint16_t a;                                        \
    a = vget_lane_u16(vreinterpret_u16_u8(src), lane); \
    memcpy(dst, &a, 2);                                \
  } while (0)

#define store_unaligned_u16_2x1(dst, src, lane)         \
  do {                                                  \
    uint32_t a;                                         \
    a = vget_lane_u32(vreinterpret_u32_u16(src), lane); \
    memcpy(dst, &a, 4);                                 \
  } while (0)

#define store_unaligned_u16_4x1(dst, src, lane)           \
  do {                                                    \
    uint64_t a;                                           \
    a = vgetq_lane_u64(vreinterpretq_u64_u16(src), lane); \
    memcpy(dst, &a, 8);                                   \
  } while (0)

static INLINE void load_u8_16x8(const uint8_t *s, ptrdiff_t p,
                                uint8x16_t *const s0, uint8x16_t *const s1,
                                uint8x16_t *const s2, uint8x16_t *const s3,
                                uint8x16_t *const s4, uint8x16_t *const s5,
                                uint8x16_t *const s6, uint8x16_t *const s7) {
  *s0 = vld1q_u8(s);
  s += p;
  *s1 = vld1q_u8(s);
  s += p;
  *s2 = vld1q_u8(s);
  s += p;
  *s3 = vld1q_u8(s);
  s += p;
  *s4 = vld1q_u8(s);
  s += p;
  *s5 = vld1q_u8(s);
  s += p;
  *s6 = vld1q_u8(s);
  s += p;
  *s7 = vld1q_u8(s);
}

static INLINE void load_u8_16x4(const uint8_t *s, ptrdiff_t p,
                                uint8x16_t *const s0, uint8x16_t *const s1,
                                uint8x16_t *const s2, uint8x16_t *const s3) {
  *s0 = vld1q_u8(s);
  s += p;
  *s1 = vld1q_u8(s);
  s += p;
  *s2 = vld1q_u8(s);
  s += p;
  *s3 = vld1q_u8(s);
}

static INLINE void load_u16_8x8(const uint16_t *s, const ptrdiff_t p,
                                uint16x8_t *s0, uint16x8_t *s1, uint16x8_t *s2,
                                uint16x8_t *s3, uint16x8_t *s4, uint16x8_t *s5,
                                uint16x8_t *s6, uint16x8_t *s7) {
  *s0 = vld1q_u16(s);
  s += p;
  *s1 = vld1q_u16(s);
  s += p;
  *s2 = vld1q_u16(s);
  s += p;
  *s3 = vld1q_u16(s);
  s += p;
  *s4 = vld1q_u16(s);
  s += p;
  *s5 = vld1q_u16(s);
  s += p;
  *s6 = vld1q_u16(s);
  s += p;
  *s7 = vld1q_u16(s);
}

static INLINE void load_u16_16x4(const uint16_t *s, ptrdiff_t p,
                                 uint16x8_t *const s0, uint16x8_t *const s1,
                                 uint16x8_t *const s2, uint16x8_t *const s3,
                                 uint16x8_t *const s4, uint16x8_t *const s5,
                                 uint16x8_t *const s6, uint16x8_t *const s7) {
  *s0 = vld1q_u16(s);
  *s1 = vld1q_u16(s + 8);
  s += p;
  *s2 = vld1q_u16(s);
  *s3 = vld1q_u16(s + 8);
  s += p;
  *s4 = vld1q_u16(s);
  *s5 = vld1q_u16(s + 8);
  s += p;
  *s6 = vld1q_u16(s);
  *s7 = vld1q_u16(s + 8);
}

static INLINE uint16x4_t load_unaligned_u16_2x2(const uint16_t *buf,
                                                int stride) {
  uint32_t a;
  uint32x2_t a_u32;

  memcpy(&a, buf, 4);
  buf += stride;
  a_u32 = vdup_n_u32(a);
  memcpy(&a, buf, 4);
  a_u32 = vset_lane_u32(a, a_u32, 1);
  return vreinterpret_u16_u32(a_u32);
}

static INLINE uint16x4_t load_unaligned_u16_4x1(const uint16_t *buf) {
  uint64_t a;
  uint64x1_t a_u64 = vdup_n_u64(0);
  memcpy(&a, buf, 8);
  a_u64 = vset_lane_u64(a, a_u64, 0);
  return vreinterpret_u16_u64(a_u64);
}

static INLINE uint16x8_t load_unaligned_u16_4x2(const uint16_t *buf,
                                                uint32_t stride) {
  uint64_t a;
  uint64x2_t a_u64;

  memcpy(&a, buf, 8);
  buf += stride;
  a_u64 = vdupq_n_u64(0);
  a_u64 = vsetq_lane_u64(a, a_u64, 0);
  memcpy(&a, buf, 8);
  buf += stride;
  a_u64 = vsetq_lane_u64(a, a_u64, 1);
  return vreinterpretq_u16_u64(a_u64);
}

static INLINE void load_unaligned_u16_4x4(const uint16_t *buf, uint32_t stride,
                                          uint16x8_t *tu0, uint16x8_t *tu1) {
  *tu0 = load_unaligned_u16_4x2(buf, stride);
  buf += 2 * stride;
  *tu1 = load_unaligned_u16_4x2(buf, stride);
}

static INLINE void load_s32_4x4(int32_t *s, int32_t p, int32x4_t *s1,
                                int32x4_t *s2, int32x4_t *s3, int32x4_t *s4) {
  *s1 = vld1q_s32(s);
  s += p;
  *s2 = vld1q_s32(s);
  s += p;
  *s3 = vld1q_s32(s);
  s += p;
  *s4 = vld1q_s32(s);
}

static INLINE void store_s32_4x4(int32_t *s, int32_t p, int32x4_t s1,
                                 int32x4_t s2, int32x4_t s3, int32x4_t s4) {
  vst1q_s32(s, s1);
  s += p;
  vst1q_s32(s, s2);
  s += p;
  vst1q_s32(s, s3);
  s += p;
  vst1q_s32(s, s4);
}

static INLINE void load_u32_4x4(uint32_t *s, int32_t p, uint32x4_t *s1,
                                uint32x4_t *s2, uint32x4_t *s3,
                                uint32x4_t *s4) {
  *s1 = vld1q_u32(s);
  s += p;
  *s2 = vld1q_u32(s);
  s += p;
  *s3 = vld1q_u32(s);
  s += p;
  *s4 = vld1q_u32(s);
}

static INLINE void store_u32_4x4(uint32_t *s, int32_t p, uint32x4_t s1,
                                 uint32x4_t s2, uint32x4_t s3, uint32x4_t s4) {
  vst1q_u32(s, s1);
  s += p;
  vst1q_u32(s, s2);
  s += p;
  vst1q_u32(s, s3);
  s += p;
  vst1q_u32(s, s4);
}

static INLINE int16x8_t load_tran_low_to_s16q(const tran_low_t *buf) {
  const int32x4_t v0 = vld1q_s32(buf);
  const int32x4_t v1 = vld1q_s32(buf + 4);
  const int16x4_t s0 = vmovn_s32(v0);
  const int16x4_t s1 = vmovn_s32(v1);
  return vcombine_s16(s0, s1);
}

static INLINE void store_s16q_to_tran_low(tran_low_t *buf, const int16x8_t a) {
  const int32x4_t v0 = vmovl_s16(vget_low_s16(a));
  const int32x4_t v1 = vmovl_s16(vget_high_s16(a));
  vst1q_s32(buf, v0);
  vst1q_s32(buf + 4, v1);
}

static INLINE void store_s16_to_tran_low(tran_low_t *buf, const int16x4_t a) {
  const int32x4_t v0 = vmovl_s16(a);
  vst1q_s32(buf, v0);
}

static INLINE void store_unaligned_u8_2x2(uint8_t *dst, uint32_t dst_stride,
                                          uint8x8_t src) {
  store_unaligned_u8_2x1(dst, src, 0);
  dst += dst_stride;
  store_unaligned_u8_2x1(dst, src, 1);
}

static INLINE void store_unaligned_u8_4x2(uint8_t *dst, uint32_t dst_stride,
                                          uint8x8_t src) {
  store_unaligned_u8_4x1(dst, src, 0);
  dst += dst_stride;
  store_unaligned_u8_4x1(dst, src, 1);
}

static INLINE void store_unaligned_u16_2x2(uint16_t *dst, uint32_t dst_stride,
                                           uint16x4_t src) {
  store_unaligned_u16_2x1(dst, src, 0);
  dst += dst_stride;
  store_unaligned_u16_2x1(dst, src, 1);
}

static INLINE void store_unaligned_u16_4x2(uint16_t *dst, uint32_t dst_stride,
                                           uint16x8_t src) {
  store_unaligned_u16_4x1(dst, src, 0);
  dst += dst_stride;
  store_unaligned_u16_4x1(dst, src, 1);
}

#endif  // AOM_AOM_DSP_ARM_MEM_NEON_H_
