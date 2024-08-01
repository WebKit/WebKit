/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <arm_neon.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_dsp/arm/blend_neon.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/sum_neon.h"
#include "aom_dsp/blend.h"

static INLINE uint16x8_t masked_sad_8x1_neon(uint16x8_t sad,
                                             const uint16_t *src,
                                             const uint16_t *a,
                                             const uint16_t *b,
                                             const uint8_t *m) {
  const uint16x8_t s0 = vld1q_u16(src);
  const uint16x8_t a0 = vld1q_u16(a);
  const uint16x8_t b0 = vld1q_u16(b);
  const uint16x8_t m0 = vmovl_u8(vld1_u8(m));

  uint16x8_t blend_u16 = alpha_blend_a64_u16x8(m0, a0, b0);

  return vaddq_u16(sad, vabdq_u16(blend_u16, s0));
}

static INLINE uint16x8_t masked_sad_16x1_neon(uint16x8_t sad,
                                              const uint16_t *src,
                                              const uint16_t *a,
                                              const uint16_t *b,
                                              const uint8_t *m) {
  sad = masked_sad_8x1_neon(sad, src, a, b, m);
  return masked_sad_8x1_neon(sad, &src[8], &a[8], &b[8], &m[8]);
}

static INLINE uint16x8_t masked_sad_32x1_neon(uint16x8_t sad,
                                              const uint16_t *src,
                                              const uint16_t *a,
                                              const uint16_t *b,
                                              const uint8_t *m) {
  sad = masked_sad_16x1_neon(sad, src, a, b, m);
  return masked_sad_16x1_neon(sad, &src[16], &a[16], &b[16], &m[16]);
}

static INLINE unsigned int masked_sad_128xh_large_neon(
    const uint8_t *src8, int src_stride, const uint8_t *a8, int a_stride,
    const uint8_t *b8, int b_stride, const uint8_t *m, int m_stride,
    int height) {
  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  const uint16_t *a = CONVERT_TO_SHORTPTR(a8);
  const uint16_t *b = CONVERT_TO_SHORTPTR(b8);
  uint32x4_t sad_u32[] = { vdupq_n_u32(0), vdupq_n_u32(0), vdupq_n_u32(0),
                           vdupq_n_u32(0) };

  do {
    uint16x8_t sad[] = { vdupq_n_u16(0), vdupq_n_u16(0), vdupq_n_u16(0),
                         vdupq_n_u16(0) };
    for (int h = 0; h < 4; ++h) {
      sad[0] = masked_sad_32x1_neon(sad[0], src, a, b, m);
      sad[1] = masked_sad_32x1_neon(sad[1], &src[32], &a[32], &b[32], &m[32]);
      sad[2] = masked_sad_32x1_neon(sad[2], &src[64], &a[64], &b[64], &m[64]);
      sad[3] = masked_sad_32x1_neon(sad[3], &src[96], &a[96], &b[96], &m[96]);

      src += src_stride;
      a += a_stride;
      b += b_stride;
      m += m_stride;
    }

    sad_u32[0] = vpadalq_u16(sad_u32[0], sad[0]);
    sad_u32[1] = vpadalq_u16(sad_u32[1], sad[1]);
    sad_u32[2] = vpadalq_u16(sad_u32[2], sad[2]);
    sad_u32[3] = vpadalq_u16(sad_u32[3], sad[3]);
    height -= 4;
  } while (height != 0);

  sad_u32[0] = vaddq_u32(sad_u32[0], sad_u32[1]);
  sad_u32[2] = vaddq_u32(sad_u32[2], sad_u32[3]);
  sad_u32[0] = vaddq_u32(sad_u32[0], sad_u32[2]);

  return horizontal_add_u32x4(sad_u32[0]);
}

static INLINE unsigned int masked_sad_64xh_large_neon(
    const uint8_t *src8, int src_stride, const uint8_t *a8, int a_stride,
    const uint8_t *b8, int b_stride, const uint8_t *m, int m_stride,
    int height) {
  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  const uint16_t *a = CONVERT_TO_SHORTPTR(a8);
  const uint16_t *b = CONVERT_TO_SHORTPTR(b8);
  uint32x4_t sad_u32[] = { vdupq_n_u32(0), vdupq_n_u32(0) };

  do {
    uint16x8_t sad[] = { vdupq_n_u16(0), vdupq_n_u16(0) };
    for (int h = 0; h < 4; ++h) {
      sad[0] = masked_sad_32x1_neon(sad[0], src, a, b, m);
      sad[1] = masked_sad_32x1_neon(sad[1], &src[32], &a[32], &b[32], &m[32]);

      src += src_stride;
      a += a_stride;
      b += b_stride;
      m += m_stride;
    }

    sad_u32[0] = vpadalq_u16(sad_u32[0], sad[0]);
    sad_u32[1] = vpadalq_u16(sad_u32[1], sad[1]);
    height -= 4;
  } while (height != 0);

  return horizontal_add_u32x4(vaddq_u32(sad_u32[0], sad_u32[1]));
}

static INLINE unsigned int masked_sad_32xh_large_neon(
    const uint8_t *src8, int src_stride, const uint8_t *a8, int a_stride,
    const uint8_t *b8, int b_stride, const uint8_t *m, int m_stride,
    int height) {
  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  const uint16_t *a = CONVERT_TO_SHORTPTR(a8);
  const uint16_t *b = CONVERT_TO_SHORTPTR(b8);
  uint32x4_t sad_u32 = vdupq_n_u32(0);

  do {
    uint16x8_t sad = vdupq_n_u16(0);
    for (int h = 0; h < 4; ++h) {
      sad = masked_sad_32x1_neon(sad, src, a, b, m);

      src += src_stride;
      a += a_stride;
      b += b_stride;
      m += m_stride;
    }

    sad_u32 = vpadalq_u16(sad_u32, sad);
    height -= 4;
  } while (height != 0);

  return horizontal_add_u32x4(sad_u32);
}

static INLINE unsigned int masked_sad_16xh_large_neon(
    const uint8_t *src8, int src_stride, const uint8_t *a8, int a_stride,
    const uint8_t *b8, int b_stride, const uint8_t *m, int m_stride,
    int height) {
  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  const uint16_t *a = CONVERT_TO_SHORTPTR(a8);
  const uint16_t *b = CONVERT_TO_SHORTPTR(b8);
  uint32x4_t sad_u32 = vdupq_n_u32(0);

  do {
    uint16x8_t sad_u16 = vdupq_n_u16(0);

    for (int h = 0; h < 8; ++h) {
      sad_u16 = masked_sad_16x1_neon(sad_u16, src, a, b, m);

      src += src_stride;
      a += a_stride;
      b += b_stride;
      m += m_stride;
    }

    sad_u32 = vpadalq_u16(sad_u32, sad_u16);
    height -= 8;
  } while (height != 0);

  return horizontal_add_u32x4(sad_u32);
}

#if !CONFIG_REALTIME_ONLY
static INLINE unsigned int masked_sad_8xh_large_neon(
    const uint8_t *src8, int src_stride, const uint8_t *a8, int a_stride,
    const uint8_t *b8, int b_stride, const uint8_t *m, int m_stride,
    int height) {
  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  const uint16_t *a = CONVERT_TO_SHORTPTR(a8);
  const uint16_t *b = CONVERT_TO_SHORTPTR(b8);
  uint32x4_t sad_u32 = vdupq_n_u32(0);

  do {
    uint16x8_t sad_u16 = vdupq_n_u16(0);

    for (int h = 0; h < 16; ++h) {
      sad_u16 = masked_sad_8x1_neon(sad_u16, src, a, b, m);

      src += src_stride;
      a += a_stride;
      b += b_stride;
      m += m_stride;
    }

    sad_u32 = vpadalq_u16(sad_u32, sad_u16);
    height -= 16;
  } while (height != 0);

  return horizontal_add_u32x4(sad_u32);
}
#endif  // !CONFIG_REALTIME_ONLY

static INLINE unsigned int masked_sad_16xh_small_neon(
    const uint8_t *src8, int src_stride, const uint8_t *a8, int a_stride,
    const uint8_t *b8, int b_stride, const uint8_t *m, int m_stride,
    int height) {
  // For 12-bit data, we can only accumulate up to 128 elements in the
  // uint16x8_t type sad accumulator, so we can only process up to 8 rows
  // before we have to accumulate into 32-bit elements.
  assert(height <= 8);
  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  const uint16_t *a = CONVERT_TO_SHORTPTR(a8);
  const uint16_t *b = CONVERT_TO_SHORTPTR(b8);
  uint16x8_t sad = vdupq_n_u16(0);

  do {
    sad = masked_sad_16x1_neon(sad, src, a, b, m);

    src += src_stride;
    a += a_stride;
    b += b_stride;
    m += m_stride;
  } while (--height != 0);

  return horizontal_add_u16x8(sad);
}

static INLINE unsigned int masked_sad_8xh_small_neon(
    const uint8_t *src8, int src_stride, const uint8_t *a8, int a_stride,
    const uint8_t *b8, int b_stride, const uint8_t *m, int m_stride,
    int height) {
  // For 12-bit data, we can only accumulate up to 128 elements in the
  // uint16x8_t type sad accumulator, so we can only process up to 16 rows
  // before we have to accumulate into 32-bit elements.
  assert(height <= 16);
  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  const uint16_t *a = CONVERT_TO_SHORTPTR(a8);
  const uint16_t *b = CONVERT_TO_SHORTPTR(b8);
  uint16x8_t sad = vdupq_n_u16(0);

  do {
    sad = masked_sad_8x1_neon(sad, src, a, b, m);

    src += src_stride;
    a += a_stride;
    b += b_stride;
    m += m_stride;
  } while (--height != 0);

  return horizontal_add_u16x8(sad);
}

static INLINE unsigned int masked_sad_4xh_small_neon(
    const uint8_t *src8, int src_stride, const uint8_t *a8, int a_stride,
    const uint8_t *b8, int b_stride, const uint8_t *m, int m_stride,
    int height) {
  // For 12-bit data, we can only accumulate up to 64 elements in the
  // uint16x4_t type sad accumulator, so we can only process up to 16 rows
  // before we have to accumulate into 32-bit elements.
  assert(height <= 16);
  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  const uint16_t *a = CONVERT_TO_SHORTPTR(a8);
  const uint16_t *b = CONVERT_TO_SHORTPTR(b8);

  uint16x4_t sad = vdup_n_u16(0);
  do {
    uint16x4_t m0 = vget_low_u16(vmovl_u8(load_unaligned_u8_4x1(m)));
    uint16x4_t a0 = load_unaligned_u16_4x1(a);
    uint16x4_t b0 = load_unaligned_u16_4x1(b);
    uint16x4_t s0 = load_unaligned_u16_4x1(src);

    uint16x4_t blend_u16 = alpha_blend_a64_u16x4(m0, a0, b0);

    sad = vadd_u16(sad, vabd_u16(blend_u16, s0));

    src += src_stride;
    a += a_stride;
    b += b_stride;
    m += m_stride;
  } while (--height != 0);

  return horizontal_add_u16x4(sad);
}

#define HIGHBD_MASKED_SAD_WXH_SMALL_NEON(w, h)                                \
  unsigned int aom_highbd_masked_sad##w##x##h##_neon(                         \
      const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, \
      const uint8_t *second_pred, const uint8_t *msk, int msk_stride,         \
      int invert_mask) {                                                      \
    if (!invert_mask)                                                         \
      return masked_sad_##w##xh_small_neon(src, src_stride, ref, ref_stride,  \
                                           second_pred, w, msk, msk_stride,   \
                                           h);                                \
    else                                                                      \
      return masked_sad_##w##xh_small_neon(src, src_stride, second_pred, w,   \
                                           ref, ref_stride, msk, msk_stride,  \
                                           h);                                \
  }

#define HIGHBD_MASKED_SAD_WXH_LARGE_NEON(w, h)                                \
  unsigned int aom_highbd_masked_sad##w##x##h##_neon(                         \
      const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, \
      const uint8_t *second_pred, const uint8_t *msk, int msk_stride,         \
      int invert_mask) {                                                      \
    if (!invert_mask)                                                         \
      return masked_sad_##w##xh_large_neon(src, src_stride, ref, ref_stride,  \
                                           second_pred, w, msk, msk_stride,   \
                                           h);                                \
    else                                                                      \
      return masked_sad_##w##xh_large_neon(src, src_stride, second_pred, w,   \
                                           ref, ref_stride, msk, msk_stride,  \
                                           h);                                \
  }

HIGHBD_MASKED_SAD_WXH_SMALL_NEON(4, 4)
HIGHBD_MASKED_SAD_WXH_SMALL_NEON(4, 8)

HIGHBD_MASKED_SAD_WXH_SMALL_NEON(8, 4)
HIGHBD_MASKED_SAD_WXH_SMALL_NEON(8, 8)
HIGHBD_MASKED_SAD_WXH_SMALL_NEON(8, 16)

HIGHBD_MASKED_SAD_WXH_SMALL_NEON(16, 8)
HIGHBD_MASKED_SAD_WXH_LARGE_NEON(16, 16)
HIGHBD_MASKED_SAD_WXH_LARGE_NEON(16, 32)

HIGHBD_MASKED_SAD_WXH_LARGE_NEON(32, 16)
HIGHBD_MASKED_SAD_WXH_LARGE_NEON(32, 32)
HIGHBD_MASKED_SAD_WXH_LARGE_NEON(32, 64)

HIGHBD_MASKED_SAD_WXH_LARGE_NEON(64, 32)
HIGHBD_MASKED_SAD_WXH_LARGE_NEON(64, 64)
HIGHBD_MASKED_SAD_WXH_LARGE_NEON(64, 128)

HIGHBD_MASKED_SAD_WXH_LARGE_NEON(128, 64)
HIGHBD_MASKED_SAD_WXH_LARGE_NEON(128, 128)

#if !CONFIG_REALTIME_ONLY
HIGHBD_MASKED_SAD_WXH_SMALL_NEON(4, 16)

HIGHBD_MASKED_SAD_WXH_LARGE_NEON(8, 32)

HIGHBD_MASKED_SAD_WXH_SMALL_NEON(16, 4)
HIGHBD_MASKED_SAD_WXH_LARGE_NEON(16, 64)

HIGHBD_MASKED_SAD_WXH_LARGE_NEON(32, 8)

HIGHBD_MASKED_SAD_WXH_LARGE_NEON(64, 16)
#endif  // !CONFIG_REALTIME_ONLY
