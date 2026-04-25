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
#include <assert.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"
#include "aom/aom_integer.h"
#include "aom_dsp/arm/aom_neon_sve_bridge.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_ports/mem.h"

int aom_vector_var_sve(const int16_t *ref, const int16_t *src, int bwl) {
  assert(bwl >= 2 && bwl <= 5);
  int width = 4 << bwl;

  int64x2_t sse_s64[2] = { vdupq_n_s64(0), vdupq_n_s64(0) };
  int16x8_t v_mean[2] = { vdupq_n_s16(0), vdupq_n_s16(0) };

  do {
    int16x8_t r0 = vld1q_s16(ref);
    int16x8_t s0 = vld1q_s16(src);

    // diff: dynamic range [-510, 510] 10 (signed) bits.
    int16x8_t diff0 = vsubq_s16(r0, s0);
    // v_mean: dynamic range 16 * diff -> [-8160, 8160], 14 (signed) bits.
    v_mean[0] = vaddq_s16(v_mean[0], diff0);

    // v_sse: dynamic range 2 * 16 * diff^2 -> [0, 8,323,200], 24 (signed) bits.
    sse_s64[0] = aom_sdotq_s16(sse_s64[0], diff0, diff0);

    int16x8_t r1 = vld1q_s16(ref + 8);
    int16x8_t s1 = vld1q_s16(src + 8);

    // diff: dynamic range [-510, 510] 10 (signed) bits.
    int16x8_t diff1 = vsubq_s16(r1, s1);
    // v_mean: dynamic range 16 * diff -> [-8160, 8160], 14 (signed) bits.
    v_mean[1] = vaddq_s16(v_mean[1], diff1);

    // v_sse: dynamic range 2 * 16 * diff^2 -> [0, 8,323,200], 24 (signed) bits.
    sse_s64[1] = aom_sdotq_s16(sse_s64[1], diff1, diff1);

    ref += 16;
    src += 16;
    width -= 16;
  } while (width != 0);

  // Dynamic range [0, 65280], 16 (unsigned) bits.
  const uint32_t mean_abs = abs(vaddlvq_s16(vaddq_s16(v_mean[0], v_mean[1])));
  const int64_t sse = vaddvq_s64(vaddq_s64(sse_s64[0], sse_s64[1]));

  // (mean_abs * mean_abs): dynamic range 32 (unsigned) bits.
  return (int)(sse - ((mean_abs * mean_abs) >> (bwl + 2)));
}
