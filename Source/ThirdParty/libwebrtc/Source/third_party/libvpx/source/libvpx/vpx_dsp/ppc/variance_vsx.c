/*
 *  Copyright (c) 2017 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>

#include "./vpx_dsp_rtcd.h"
#include "vpx_dsp/ppc/types_vsx.h"

static inline uint8x16_t read4x2(const uint8_t *a, int stride) {
  const uint32x4_t a0 = (uint32x4_t)vec_vsx_ld(0, a);
  const uint32x4_t a1 = (uint32x4_t)vec_vsx_ld(0, a + stride);

  return (uint8x16_t)vec_mergeh(a0, a1);
}

uint32_t vpx_get4x4sse_cs_vsx(const uint8_t *a, int a_stride, const uint8_t *b,
                              int b_stride) {
  int distortion;

  const int16x8_t a0 = unpack_to_s16_h(read4x2(a, a_stride));
  const int16x8_t a1 = unpack_to_s16_h(read4x2(a + a_stride * 2, a_stride));
  const int16x8_t b0 = unpack_to_s16_h(read4x2(b, b_stride));
  const int16x8_t b1 = unpack_to_s16_h(read4x2(b + b_stride * 2, b_stride));
  const int16x8_t d0 = vec_sub(a0, b0);
  const int16x8_t d1 = vec_sub(a1, b1);
  const int32x4_t ds = vec_msum(d1, d1, vec_msum(d0, d0, vec_splat_s32(0)));
  const int32x4_t d = vec_splat(vec_sums(ds, vec_splat_s32(0)), 3);

  vec_ste(d, 0, &distortion);

  return distortion;
}

// TODO(lu_zero): Unroll
uint32_t vpx_get_mb_ss_vsx(const int16_t *a) {
  unsigned int i, sum = 0;
  int32x4_t s = vec_splat_s32(0);

  for (i = 0; i < 256; i += 8) {
    const int16x8_t v = vec_vsx_ld(0, a + i);
    s = vec_msum(v, v, s);
  }

  s = vec_splat(vec_sums(s, vec_splat_s32(0)), 3);

  vec_ste((uint32x4_t)s, 0, &sum);

  return sum;
}

void vpx_comp_avg_pred_vsx(uint8_t *comp_pred, const uint8_t *pred, int width,
                           int height, const uint8_t *ref, int ref_stride) {
  int i, j;
  /* comp_pred and pred must be 16 byte aligned. */
  assert(((intptr_t)comp_pred & 0xf) == 0);
  assert(((intptr_t)pred & 0xf) == 0);
  if (width >= 16) {
    for (i = 0; i < height; ++i) {
      for (j = 0; j < width; j += 16) {
        const uint8x16_t v = vec_avg(vec_vsx_ld(j, pred), vec_vsx_ld(j, ref));
        vec_vsx_st(v, j, comp_pred);
      }
      comp_pred += width;
      pred += width;
      ref += ref_stride;
    }
  } else if (width == 8) {
    // Process 2 lines at time
    for (i = 0; i < height / 2; ++i) {
      const uint8x16_t r0 = vec_vsx_ld(0, ref);
      const uint8x16_t r1 = vec_vsx_ld(0, ref + ref_stride);
      const uint8x16_t r = xxpermdi(r0, r1, 0);
      const uint8x16_t v = vec_avg(vec_vsx_ld(0, pred), r);
      vec_vsx_st(v, 0, comp_pred);
      comp_pred += 16;  // width * 2;
      pred += 16;       // width * 2;
      ref += ref_stride * 2;
    }
  } else {
    assert(width == 4);
    // process 4 lines at time
    for (i = 0; i < height / 4; ++i) {
      const uint32x4_t r0 = (uint32x4_t)vec_vsx_ld(0, ref);
      const uint32x4_t r1 = (uint32x4_t)vec_vsx_ld(0, ref + ref_stride);
      const uint32x4_t r2 = (uint32x4_t)vec_vsx_ld(0, ref + ref_stride * 2);
      const uint32x4_t r3 = (uint32x4_t)vec_vsx_ld(0, ref + ref_stride * 3);
      const uint8x16_t r =
          (uint8x16_t)xxpermdi(vec_mergeh(r0, r1), vec_mergeh(r2, r3), 0);
      const uint8x16_t v = vec_avg(vec_vsx_ld(0, pred), r);
      vec_vsx_st(v, 0, comp_pred);
      comp_pred += 16;  // width * 4;
      pred += 16;       // width * 4;
      ref += ref_stride * 4;
    }
  }
}
