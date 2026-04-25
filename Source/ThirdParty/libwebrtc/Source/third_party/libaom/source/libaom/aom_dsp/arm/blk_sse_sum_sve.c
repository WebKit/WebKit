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

#include "config/aom_dsp_rtcd.h"
#include "config/aom_config.h"

#include "aom_dsp/arm/aom_neon_sve_bridge.h"
#include "aom_dsp/arm/mem_neon.h"

static inline void get_blk_sse_sum_4xh_sve(const int16_t *data, int stride,
                                           int bh, int *x_sum,
                                           int64_t *x2_sum) {
  int32x4_t sum = vdupq_n_s32(0);
  int64x2_t sse = vdupq_n_s64(0);

  do {
    int16x8_t d = vcombine_s16(vld1_s16(data), vld1_s16(data + stride));

    sum = vpadalq_s16(sum, d);

    sse = aom_sdotq_s16(sse, d, d);

    data += 2 * stride;
    bh -= 2;
  } while (bh != 0);

  *x_sum = vaddvq_s32(sum);
  *x2_sum = vaddvq_s64(sse);
}

static inline void get_blk_sse_sum_8xh_sve(const int16_t *data, int stride,
                                           int bh, int *x_sum,
                                           int64_t *x2_sum) {
  int32x4_t sum[2] = { vdupq_n_s32(0), vdupq_n_s32(0) };
  int64x2_t sse[2] = { vdupq_n_s64(0), vdupq_n_s64(0) };

  do {
    int16x8_t d0 = vld1q_s16(data);
    int16x8_t d1 = vld1q_s16(data + stride);

    sum[0] = vpadalq_s16(sum[0], d0);
    sum[1] = vpadalq_s16(sum[1], d1);

    sse[0] = aom_sdotq_s16(sse[0], d0, d0);
    sse[1] = aom_sdotq_s16(sse[1], d1, d1);

    data += 2 * stride;
    bh -= 2;
  } while (bh != 0);

  *x_sum = vaddvq_s32(vaddq_s32(sum[0], sum[1]));
  *x2_sum = vaddvq_s64(vaddq_s64(sse[0], sse[1]));
}

static inline void get_blk_sse_sum_large_sve(const int16_t *data, int stride,
                                             int bw, int bh, int *x_sum,
                                             int64_t *x2_sum) {
  int32x4_t sum[2] = { vdupq_n_s32(0), vdupq_n_s32(0) };
  int64x2_t sse[2] = { vdupq_n_s64(0), vdupq_n_s64(0) };

  do {
    int j = bw;
    const int16_t *data_ptr = data;
    do {
      int16x8_t d0 = vld1q_s16(data_ptr);
      int16x8_t d1 = vld1q_s16(data_ptr + 8);

      sum[0] = vpadalq_s16(sum[0], d0);
      sum[1] = vpadalq_s16(sum[1], d1);

      sse[0] = aom_sdotq_s16(sse[0], d0, d0);
      sse[1] = aom_sdotq_s16(sse[1], d1, d1);

      data_ptr += 16;
      j -= 16;
    } while (j != 0);

    data += stride;
  } while (--bh != 0);

  *x_sum = vaddvq_s32(vaddq_s32(sum[0], sum[1]));
  *x2_sum = vaddvq_s64(vaddq_s64(sse[0], sse[1]));
}

void aom_get_blk_sse_sum_sve(const int16_t *data, int stride, int bw, int bh,
                             int *x_sum, int64_t *x2_sum) {
  if (bw == 4) {
    get_blk_sse_sum_4xh_sve(data, stride, bh, x_sum, x2_sum);
  } else if (bw == 8) {
    get_blk_sse_sum_8xh_sve(data, stride, bh, x_sum, x2_sum);
  } else {
    assert(bw % 16 == 0);
    get_blk_sse_sum_large_sve(data, stride, bw, bh, x_sum, x2_sum);
  }
}
