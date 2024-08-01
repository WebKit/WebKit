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

#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/sum_neon.h"

static INLINE void get_blk_sse_sum_4xh_neon(const int16_t *data, int stride,
                                            int bh, int *x_sum,
                                            int64_t *x2_sum) {
  int i = bh;
  int32x4_t sum = vdupq_n_s32(0);
  int32x4_t sse = vdupq_n_s32(0);

  do {
    int16x8_t d = vcombine_s16(vld1_s16(data), vld1_s16(data + stride));

    sum = vpadalq_s16(sum, d);

    sse = vmlal_s16(sse, vget_low_s16(d), vget_low_s16(d));
    sse = vmlal_s16(sse, vget_high_s16(d), vget_high_s16(d));

    data += 2 * stride;
    i -= 2;
  } while (i != 0);

  *x_sum = horizontal_add_s32x4(sum);
  *x2_sum = horizontal_long_add_s32x4(sse);
}

static INLINE void get_blk_sse_sum_8xh_neon(const int16_t *data, int stride,
                                            int bh, int *x_sum,
                                            int64_t *x2_sum) {
  int i = bh;
  int32x4_t sum = vdupq_n_s32(0);
  int32x4_t sse = vdupq_n_s32(0);

  // Input is 12-bit wide, so we can add up to 127 squared elements in a signed
  // 32-bits element. Since we're accumulating into an int32x4_t and the maximum
  // value for bh is 32, we don't have to worry about sse overflowing.

  do {
    int16x8_t d = vld1q_s16(data);

    sum = vpadalq_s16(sum, d);

    sse = vmlal_s16(sse, vget_low_s16(d), vget_low_s16(d));
    sse = vmlal_s16(sse, vget_high_s16(d), vget_high_s16(d));

    data += stride;
  } while (--i != 0);

  *x_sum = horizontal_add_s32x4(sum);
  *x2_sum = horizontal_long_add_s32x4(sse);
}

static INLINE void get_blk_sse_sum_large_neon(const int16_t *data, int stride,
                                              int bw, int bh, int *x_sum,
                                              int64_t *x2_sum) {
  int32x4_t sum = vdupq_n_s32(0);
  int64x2_t sse = vdupq_n_s64(0);

  // Input is 12-bit wide, so we can add up to 127 squared elements in a signed
  // 32-bits element. Since we're accumulating into an int32x4_t vector that
  // means we can process up to (127*4)/bw rows before we need to widen to
  // 64 bits.

  int i_limit = (127 * 4) / bw;
  int i_tmp = bh > i_limit ? i_limit : bh;

  int i = 0;
  do {
    int32x4_t sse_s32 = vdupq_n_s32(0);
    do {
      int j = bw;
      const int16_t *data_ptr = data;
      do {
        int16x8_t d = vld1q_s16(data_ptr);

        sum = vpadalq_s16(sum, d);

        sse_s32 = vmlal_s16(sse_s32, vget_low_s16(d), vget_low_s16(d));
        sse_s32 = vmlal_s16(sse_s32, vget_high_s16(d), vget_high_s16(d));

        data_ptr += 8;
        j -= 8;
      } while (j != 0);

      data += stride;
      i++;
    } while (i < i_tmp && i < bh);

    sse = vpadalq_s32(sse, sse_s32);
    i_tmp += i_limit;
  } while (i < bh);

  *x_sum = horizontal_add_s32x4(sum);
  *x2_sum = horizontal_add_s64x2(sse);
}

void aom_get_blk_sse_sum_neon(const int16_t *data, int stride, int bw, int bh,
                              int *x_sum, int64_t *x2_sum) {
  if (bw == 4) {
    get_blk_sse_sum_4xh_neon(data, stride, bh, x_sum, x2_sum);
  } else if (bw == 8) {
    get_blk_sse_sum_8xh_neon(data, stride, bh, x_sum, x2_sum);
  } else {
    assert(bw % 8 == 0);
    get_blk_sse_sum_large_neon(data, stride, bw, bh, x_sum, x2_sum);
  }
}
