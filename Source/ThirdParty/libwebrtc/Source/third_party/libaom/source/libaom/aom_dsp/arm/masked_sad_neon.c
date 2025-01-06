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

static inline uint16x8_t masked_sad_16x1_neon(uint16x8_t sad,
                                              const uint8_t *src,
                                              const uint8_t *a,
                                              const uint8_t *b,
                                              const uint8_t *m) {
  uint8x16_t m0 = vld1q_u8(m);
  uint8x16_t a0 = vld1q_u8(a);
  uint8x16_t b0 = vld1q_u8(b);
  uint8x16_t s0 = vld1q_u8(src);

  uint8x16_t blend_u8 = alpha_blend_a64_u8x16(m0, a0, b0);

  return vpadalq_u8(sad, vabdq_u8(blend_u8, s0));
}

static inline unsigned masked_sad_128xh_neon(const uint8_t *src, int src_stride,
                                             const uint8_t *a, int a_stride,
                                             const uint8_t *b, int b_stride,
                                             const uint8_t *m, int m_stride,
                                             int height) {
  // Eight accumulator vectors are required to avoid overflow in the 128x128
  // case.
  assert(height <= 128);
  uint16x8_t sad[] = { vdupq_n_u16(0), vdupq_n_u16(0), vdupq_n_u16(0),
                       vdupq_n_u16(0), vdupq_n_u16(0), vdupq_n_u16(0),
                       vdupq_n_u16(0), vdupq_n_u16(0) };

  do {
    sad[0] = masked_sad_16x1_neon(sad[0], &src[0], &a[0], &b[0], &m[0]);
    sad[1] = masked_sad_16x1_neon(sad[1], &src[16], &a[16], &b[16], &m[16]);
    sad[2] = masked_sad_16x1_neon(sad[2], &src[32], &a[32], &b[32], &m[32]);
    sad[3] = masked_sad_16x1_neon(sad[3], &src[48], &a[48], &b[48], &m[48]);
    sad[4] = masked_sad_16x1_neon(sad[4], &src[64], &a[64], &b[64], &m[64]);
    sad[5] = masked_sad_16x1_neon(sad[5], &src[80], &a[80], &b[80], &m[80]);
    sad[6] = masked_sad_16x1_neon(sad[6], &src[96], &a[96], &b[96], &m[96]);
    sad[7] = masked_sad_16x1_neon(sad[7], &src[112], &a[112], &b[112], &m[112]);

    src += src_stride;
    a += a_stride;
    b += b_stride;
    m += m_stride;
    height--;
  } while (height != 0);

  return horizontal_long_add_u16x8(sad[0], sad[1]) +
         horizontal_long_add_u16x8(sad[2], sad[3]) +
         horizontal_long_add_u16x8(sad[4], sad[5]) +
         horizontal_long_add_u16x8(sad[6], sad[7]);
}

static inline unsigned masked_sad_64xh_neon(const uint8_t *src, int src_stride,
                                            const uint8_t *a, int a_stride,
                                            const uint8_t *b, int b_stride,
                                            const uint8_t *m, int m_stride,
                                            int height) {
  // Four accumulator vectors are required to avoid overflow in the 64x128 case.
  assert(height <= 128);
  uint16x8_t sad[] = { vdupq_n_u16(0), vdupq_n_u16(0), vdupq_n_u16(0),
                       vdupq_n_u16(0) };

  do {
    sad[0] = masked_sad_16x1_neon(sad[0], &src[0], &a[0], &b[0], &m[0]);
    sad[1] = masked_sad_16x1_neon(sad[1], &src[16], &a[16], &b[16], &m[16]);
    sad[2] = masked_sad_16x1_neon(sad[2], &src[32], &a[32], &b[32], &m[32]);
    sad[3] = masked_sad_16x1_neon(sad[3], &src[48], &a[48], &b[48], &m[48]);

    src += src_stride;
    a += a_stride;
    b += b_stride;
    m += m_stride;
    height--;
  } while (height != 0);

  return horizontal_long_add_u16x8(sad[0], sad[1]) +
         horizontal_long_add_u16x8(sad[2], sad[3]);
}

static inline unsigned masked_sad_32xh_neon(const uint8_t *src, int src_stride,
                                            const uint8_t *a, int a_stride,
                                            const uint8_t *b, int b_stride,
                                            const uint8_t *m, int m_stride,
                                            int height) {
  // We could use a single accumulator up to height=64 without overflow.
  assert(height <= 64);
  uint16x8_t sad = vdupq_n_u16(0);

  do {
    sad = masked_sad_16x1_neon(sad, &src[0], &a[0], &b[0], &m[0]);
    sad = masked_sad_16x1_neon(sad, &src[16], &a[16], &b[16], &m[16]);

    src += src_stride;
    a += a_stride;
    b += b_stride;
    m += m_stride;
    height--;
  } while (height != 0);

  return horizontal_add_u16x8(sad);
}

static inline unsigned masked_sad_16xh_neon(const uint8_t *src, int src_stride,
                                            const uint8_t *a, int a_stride,
                                            const uint8_t *b, int b_stride,
                                            const uint8_t *m, int m_stride,
                                            int height) {
  // We could use a single accumulator up to height=128 without overflow.
  assert(height <= 128);
  uint16x8_t sad = vdupq_n_u16(0);

  do {
    sad = masked_sad_16x1_neon(sad, src, a, b, m);

    src += src_stride;
    a += a_stride;
    b += b_stride;
    m += m_stride;
    height--;
  } while (height != 0);

  return horizontal_add_u16x8(sad);
}

static inline unsigned masked_sad_8xh_neon(const uint8_t *src, int src_stride,
                                           const uint8_t *a, int a_stride,
                                           const uint8_t *b, int b_stride,
                                           const uint8_t *m, int m_stride,
                                           int height) {
  // We could use a single accumulator up to height=128 without overflow.
  assert(height <= 128);
  uint16x4_t sad = vdup_n_u16(0);

  do {
    uint8x8_t m0 = vld1_u8(m);
    uint8x8_t a0 = vld1_u8(a);
    uint8x8_t b0 = vld1_u8(b);
    uint8x8_t s0 = vld1_u8(src);

    uint8x8_t blend_u8 = alpha_blend_a64_u8x8(m0, a0, b0);

    sad = vpadal_u8(sad, vabd_u8(blend_u8, s0));

    src += src_stride;
    a += a_stride;
    b += b_stride;
    m += m_stride;
    height--;
  } while (height != 0);

  return horizontal_add_u16x4(sad);
}

static inline unsigned masked_sad_4xh_neon(const uint8_t *src, int src_stride,
                                           const uint8_t *a, int a_stride,
                                           const uint8_t *b, int b_stride,
                                           const uint8_t *m, int m_stride,
                                           int height) {
  // Process two rows per loop iteration.
  assert(height % 2 == 0);

  // We could use a single accumulator up to height=256 without overflow.
  assert(height <= 256);
  uint16x4_t sad = vdup_n_u16(0);

  do {
    uint8x8_t m0 = load_unaligned_u8(m, m_stride);
    uint8x8_t a0 = load_unaligned_u8(a, a_stride);
    uint8x8_t b0 = load_unaligned_u8(b, b_stride);
    uint8x8_t s0 = load_unaligned_u8(src, src_stride);

    uint8x8_t blend_u8 = alpha_blend_a64_u8x8(m0, a0, b0);

    sad = vpadal_u8(sad, vabd_u8(blend_u8, s0));

    src += 2 * src_stride;
    a += 2 * a_stride;
    b += 2 * b_stride;
    m += 2 * m_stride;
    height -= 2;
  } while (height != 0);

  return horizontal_add_u16x4(sad);
}

#define MASKED_SAD_WXH_NEON(width, height)                                    \
  unsigned aom_masked_sad##width##x##height##_neon(                           \
      const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, \
      const uint8_t *second_pred, const uint8_t *msk, int msk_stride,         \
      int invert_mask) {                                                      \
    if (!invert_mask)                                                         \
      return masked_sad_##width##xh_neon(src, src_stride, ref, ref_stride,    \
                                         second_pred, width, msk, msk_stride, \
                                         height);                             \
    else                                                                      \
      return masked_sad_##width##xh_neon(src, src_stride, second_pred, width, \
                                         ref, ref_stride, msk, msk_stride,    \
                                         height);                             \
  }

MASKED_SAD_WXH_NEON(4, 4)
MASKED_SAD_WXH_NEON(4, 8)
MASKED_SAD_WXH_NEON(8, 4)
MASKED_SAD_WXH_NEON(8, 8)
MASKED_SAD_WXH_NEON(8, 16)
MASKED_SAD_WXH_NEON(16, 8)
MASKED_SAD_WXH_NEON(16, 16)
MASKED_SAD_WXH_NEON(16, 32)
MASKED_SAD_WXH_NEON(32, 16)
MASKED_SAD_WXH_NEON(32, 32)
MASKED_SAD_WXH_NEON(32, 64)
MASKED_SAD_WXH_NEON(64, 32)
MASKED_SAD_WXH_NEON(64, 64)
MASKED_SAD_WXH_NEON(64, 128)
MASKED_SAD_WXH_NEON(128, 64)
MASKED_SAD_WXH_NEON(128, 128)
#if !CONFIG_REALTIME_ONLY
MASKED_SAD_WXH_NEON(4, 16)
MASKED_SAD_WXH_NEON(16, 4)
MASKED_SAD_WXH_NEON(8, 32)
MASKED_SAD_WXH_NEON(32, 8)
MASKED_SAD_WXH_NEON(16, 64)
MASKED_SAD_WXH_NEON(64, 16)
#endif
