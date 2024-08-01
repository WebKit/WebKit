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

#include "aom_dsp/arm/aom_neon_sve_bridge.h"
#include "aom_dsp/arm/mem_neon.h"
#include "config/aom_dsp_rtcd.h"

static INLINE void highbd_sse_8x1_neon(const uint16_t *src, const uint16_t *ref,
                                       uint64x2_t *sse) {
  uint16x8_t s = vld1q_u16(src);
  uint16x8_t r = vld1q_u16(ref);

  uint16x8_t abs_diff = vabdq_u16(s, r);

  *sse = aom_udotq_u16(*sse, abs_diff, abs_diff);
}

static INLINE int64_t highbd_sse_128xh_sve(const uint16_t *src, int src_stride,
                                           const uint16_t *ref, int ref_stride,
                                           int height) {
  uint64x2_t sse[4] = { vdupq_n_u64(0), vdupq_n_u64(0), vdupq_n_u64(0),
                        vdupq_n_u64(0) };

  do {
    highbd_sse_8x1_neon(src + 0 * 8, ref + 0 * 8, &sse[0]);
    highbd_sse_8x1_neon(src + 1 * 8, ref + 1 * 8, &sse[1]);
    highbd_sse_8x1_neon(src + 2 * 8, ref + 2 * 8, &sse[2]);
    highbd_sse_8x1_neon(src + 3 * 8, ref + 3 * 8, &sse[3]);
    highbd_sse_8x1_neon(src + 4 * 8, ref + 4 * 8, &sse[0]);
    highbd_sse_8x1_neon(src + 5 * 8, ref + 5 * 8, &sse[1]);
    highbd_sse_8x1_neon(src + 6 * 8, ref + 6 * 8, &sse[2]);
    highbd_sse_8x1_neon(src + 7 * 8, ref + 7 * 8, &sse[3]);
    highbd_sse_8x1_neon(src + 8 * 8, ref + 8 * 8, &sse[0]);
    highbd_sse_8x1_neon(src + 9 * 8, ref + 9 * 8, &sse[1]);
    highbd_sse_8x1_neon(src + 10 * 8, ref + 10 * 8, &sse[2]);
    highbd_sse_8x1_neon(src + 11 * 8, ref + 11 * 8, &sse[3]);
    highbd_sse_8x1_neon(src + 12 * 8, ref + 12 * 8, &sse[0]);
    highbd_sse_8x1_neon(src + 13 * 8, ref + 13 * 8, &sse[1]);
    highbd_sse_8x1_neon(src + 14 * 8, ref + 14 * 8, &sse[2]);
    highbd_sse_8x1_neon(src + 15 * 8, ref + 15 * 8, &sse[3]);

    src += src_stride;
    ref += ref_stride;
  } while (--height != 0);

  sse[0] = vaddq_u64(sse[0], sse[1]);
  sse[2] = vaddq_u64(sse[2], sse[3]);
  sse[0] = vaddq_u64(sse[0], sse[2]);
  return vaddvq_u64(sse[0]);
}

static INLINE int64_t highbd_sse_64xh_sve(const uint16_t *src, int src_stride,
                                          const uint16_t *ref, int ref_stride,
                                          int height) {
  uint64x2_t sse[4] = { vdupq_n_u64(0), vdupq_n_u64(0), vdupq_n_u64(0),
                        vdupq_n_u64(0) };

  do {
    highbd_sse_8x1_neon(src + 0 * 8, ref + 0 * 8, &sse[0]);
    highbd_sse_8x1_neon(src + 1 * 8, ref + 1 * 8, &sse[1]);
    highbd_sse_8x1_neon(src + 2 * 8, ref + 2 * 8, &sse[2]);
    highbd_sse_8x1_neon(src + 3 * 8, ref + 3 * 8, &sse[3]);
    highbd_sse_8x1_neon(src + 4 * 8, ref + 4 * 8, &sse[0]);
    highbd_sse_8x1_neon(src + 5 * 8, ref + 5 * 8, &sse[1]);
    highbd_sse_8x1_neon(src + 6 * 8, ref + 6 * 8, &sse[2]);
    highbd_sse_8x1_neon(src + 7 * 8, ref + 7 * 8, &sse[3]);

    src += src_stride;
    ref += ref_stride;
  } while (--height != 0);

  sse[0] = vaddq_u64(sse[0], sse[1]);
  sse[2] = vaddq_u64(sse[2], sse[3]);
  sse[0] = vaddq_u64(sse[0], sse[2]);
  return vaddvq_u64(sse[0]);
}

static INLINE int64_t highbd_sse_32xh_sve(const uint16_t *src, int src_stride,
                                          const uint16_t *ref, int ref_stride,
                                          int height) {
  uint64x2_t sse[4] = { vdupq_n_u64(0), vdupq_n_u64(0), vdupq_n_u64(0),
                        vdupq_n_u64(0) };

  do {
    highbd_sse_8x1_neon(src + 0 * 8, ref + 0 * 8, &sse[0]);
    highbd_sse_8x1_neon(src + 1 * 8, ref + 1 * 8, &sse[1]);
    highbd_sse_8x1_neon(src + 2 * 8, ref + 2 * 8, &sse[2]);
    highbd_sse_8x1_neon(src + 3 * 8, ref + 3 * 8, &sse[3]);

    src += src_stride;
    ref += ref_stride;
  } while (--height != 0);

  sse[0] = vaddq_u64(sse[0], sse[1]);
  sse[2] = vaddq_u64(sse[2], sse[3]);
  sse[0] = vaddq_u64(sse[0], sse[2]);
  return vaddvq_u64(sse[0]);
}

static INLINE int64_t highbd_sse_16xh_sve(const uint16_t *src, int src_stride,
                                          const uint16_t *ref, int ref_stride,
                                          int height) {
  uint64x2_t sse[2] = { vdupq_n_u64(0), vdupq_n_u64(0) };

  do {
    highbd_sse_8x1_neon(src + 0 * 8, ref + 0 * 8, &sse[0]);
    highbd_sse_8x1_neon(src + 1 * 8, ref + 1 * 8, &sse[1]);

    src += src_stride;
    ref += ref_stride;
  } while (--height != 0);

  return vaddvq_u64(vaddq_u64(sse[0], sse[1]));
}

static INLINE int64_t highbd_sse_8xh_sve(const uint16_t *src, int src_stride,
                                         const uint16_t *ref, int ref_stride,
                                         int height) {
  uint64x2_t sse[2] = { vdupq_n_u64(0), vdupq_n_u64(0) };

  do {
    highbd_sse_8x1_neon(src + 0 * src_stride, ref + 0 * ref_stride, &sse[0]);
    highbd_sse_8x1_neon(src + 1 * src_stride, ref + 1 * ref_stride, &sse[1]);

    src += 2 * src_stride;
    ref += 2 * ref_stride;
    height -= 2;
  } while (height != 0);

  return vaddvq_u64(vaddq_u64(sse[0], sse[1]));
}

static INLINE int64_t highbd_sse_4xh_sve(const uint16_t *src, int src_stride,
                                         const uint16_t *ref, int ref_stride,
                                         int height) {
  uint64x2_t sse = vdupq_n_u64(0);

  do {
    uint16x8_t s = load_unaligned_u16_4x2(src, src_stride);
    uint16x8_t r = load_unaligned_u16_4x2(ref, ref_stride);

    uint16x8_t abs_diff = vabdq_u16(s, r);
    sse = aom_udotq_u16(sse, abs_diff, abs_diff);

    src += 2 * src_stride;
    ref += 2 * ref_stride;
    height -= 2;
  } while (height != 0);

  return vaddvq_u64(sse);
}

static INLINE int64_t highbd_sse_wxh_sve(const uint16_t *src, int src_stride,
                                         const uint16_t *ref, int ref_stride,
                                         int width, int height) {
  svuint64_t sse = svdup_n_u64(0);
  uint64_t step = svcnth();

  do {
    int w = 0;
    const uint16_t *src_ptr = src;
    const uint16_t *ref_ptr = ref;

    do {
      svbool_t pred = svwhilelt_b16_u32(w, width);
      svuint16_t s = svld1_u16(pred, src_ptr);
      svuint16_t r = svld1_u16(pred, ref_ptr);

      svuint16_t abs_diff = svabd_u16_z(pred, s, r);

      sse = svdot_u64(sse, abs_diff, abs_diff);

      src_ptr += step;
      ref_ptr += step;
      w += step;
    } while (w < width);

    src += src_stride;
    ref += ref_stride;
  } while (--height != 0);

  return svaddv_u64(svptrue_b64(), sse);
}

int64_t aom_highbd_sse_sve(const uint8_t *src8, int src_stride,
                           const uint8_t *ref8, int ref_stride, int width,
                           int height) {
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);

  switch (width) {
    case 4: return highbd_sse_4xh_sve(src, src_stride, ref, ref_stride, height);
    case 8: return highbd_sse_8xh_sve(src, src_stride, ref, ref_stride, height);
    case 16:
      return highbd_sse_16xh_sve(src, src_stride, ref, ref_stride, height);
    case 32:
      return highbd_sse_32xh_sve(src, src_stride, ref, ref_stride, height);
    case 64:
      return highbd_sse_64xh_sve(src, src_stride, ref, ref_stride, height);
    case 128:
      return highbd_sse_128xh_sve(src, src_stride, ref, ref_stride, height);
    default:
      return highbd_sse_wxh_sve(src, src_stride, ref, ref_stride, width,
                                height);
  }
}
