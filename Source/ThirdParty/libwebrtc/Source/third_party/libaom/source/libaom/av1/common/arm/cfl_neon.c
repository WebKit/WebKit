/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved.
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
#include "config/av1_rtcd.h"

#include "av1/common/cfl.h"

static inline void vldsubstq_s16(int16_t *dst, const uint16_t *src, int offset,
                                 int16x8_t sub) {
  vst1q_s16(dst + offset,
            vsubq_s16(vreinterpretq_s16_u16(vld1q_u16(src + offset)), sub));
}

static inline uint16x8_t vldaddq_u16(const uint16_t *buf, size_t offset) {
  return vaddq_u16(vld1q_u16(buf), vld1q_u16(buf + offset));
}

// Load half of a vector and duplicated in other half
static inline uint8x8_t vldh_dup_u8(const uint8_t *ptr) {
  return vreinterpret_u8_u32(vld1_dup_u32((const uint32_t *)ptr));
}

// Store half of a vector.
static inline void vsth_u16(uint16_t *ptr, uint16x4_t val) {
  vst1_lane_u32((uint32_t *)ptr, vreinterpret_u32_u16(val), 0);
}

// Store half of a vector.
static inline void vsth_u8(uint8_t *ptr, uint8x8_t val) {
  vst1_lane_u32((uint32_t *)ptr, vreinterpret_u32_u8(val), 0);
}

static void cfl_luma_subsampling_420_lbd_neon(const uint8_t *input,
                                              int input_stride,
                                              uint16_t *pred_buf_q3, int width,
                                              int height) {
  const uint16_t *end = pred_buf_q3 + (height >> 1) * CFL_BUF_LINE;
  const int luma_stride = input_stride << 1;
  do {
    if (width == 4) {
      const uint16x4_t top = vpaddl_u8(vldh_dup_u8(input));
      const uint16x4_t sum = vpadal_u8(top, vldh_dup_u8(input + input_stride));
      vsth_u16(pred_buf_q3, vshl_n_u16(sum, 1));
    } else if (width == 8) {
      const uint16x4_t top = vpaddl_u8(vld1_u8(input));
      const uint16x4_t sum = vpadal_u8(top, vld1_u8(input + input_stride));
      vst1_u16(pred_buf_q3, vshl_n_u16(sum, 1));
    } else if (width == 16) {
      const uint16x8_t top = vpaddlq_u8(vld1q_u8(input));
      const uint16x8_t sum = vpadalq_u8(top, vld1q_u8(input + input_stride));
      vst1q_u16(pred_buf_q3, vshlq_n_u16(sum, 1));
    } else {
      const uint8x8x4_t top = vld4_u8(input);
      const uint8x8x4_t bot = vld4_u8(input + input_stride);
      // equivalent to a vpaddlq_u8 (because vld4q interleaves)
      const uint16x8_t top_0 = vaddl_u8(top.val[0], top.val[1]);
      // equivalent to a vpaddlq_u8 (because vld4q interleaves)
      const uint16x8_t bot_0 = vaddl_u8(bot.val[0], bot.val[1]);
      // equivalent to a vpaddlq_u8 (because vld4q interleaves)
      const uint16x8_t top_1 = vaddl_u8(top.val[2], top.val[3]);
      // equivalent to a vpaddlq_u8 (because vld4q interleaves)
      const uint16x8_t bot_1 = vaddl_u8(bot.val[2], bot.val[3]);
      uint16x8x2_t sum;
      sum.val[0] = vshlq_n_u16(vaddq_u16(top_0, bot_0), 1);
      sum.val[1] = vshlq_n_u16(vaddq_u16(top_1, bot_1), 1);
      vst2q_u16(pred_buf_q3, sum);
    }
    input += luma_stride;
  } while ((pred_buf_q3 += CFL_BUF_LINE) < end);
}

static void cfl_luma_subsampling_422_lbd_neon(const uint8_t *input,
                                              int input_stride,
                                              uint16_t *pred_buf_q3, int width,
                                              int height) {
  const uint16_t *end = pred_buf_q3 + height * CFL_BUF_LINE;
  do {
    if (width == 4) {
      const uint16x4_t top = vpaddl_u8(vldh_dup_u8(input));
      vsth_u16(pred_buf_q3, vshl_n_u16(top, 2));
    } else if (width == 8) {
      const uint16x4_t top = vpaddl_u8(vld1_u8(input));
      vst1_u16(pred_buf_q3, vshl_n_u16(top, 2));
    } else if (width == 16) {
      const uint16x8_t top = vpaddlq_u8(vld1q_u8(input));
      vst1q_u16(pred_buf_q3, vshlq_n_u16(top, 2));
    } else {
      const uint8x8x4_t top = vld4_u8(input);
      uint16x8x2_t sum;
      // vaddl_u8 is equivalent to a vpaddlq_u8 (because vld4q interleaves)
      sum.val[0] = vshlq_n_u16(vaddl_u8(top.val[0], top.val[1]), 2);
      sum.val[1] = vshlq_n_u16(vaddl_u8(top.val[2], top.val[3]), 2);
      vst2q_u16(pred_buf_q3, sum);
    }
    input += input_stride;
  } while ((pred_buf_q3 += CFL_BUF_LINE) < end);
}

static void cfl_luma_subsampling_444_lbd_neon(const uint8_t *input,
                                              int input_stride,
                                              uint16_t *pred_buf_q3, int width,
                                              int height) {
  const uint16_t *end = pred_buf_q3 + height * CFL_BUF_LINE;
  do {
    if (width == 4) {
      const uint16x8_t top = vshll_n_u8(vldh_dup_u8(input), 3);
      vst1_u16(pred_buf_q3, vget_low_u16(top));
    } else if (width == 8) {
      const uint16x8_t top = vshll_n_u8(vld1_u8(input), 3);
      vst1q_u16(pred_buf_q3, top);
    } else {
      const uint8x16_t top = vld1q_u8(input);
      vst1q_u16(pred_buf_q3, vshll_n_u8(vget_low_u8(top), 3));
      vst1q_u16(pred_buf_q3 + 8, vshll_n_u8(vget_high_u8(top), 3));
      if (width == 32) {
        const uint8x16_t next_top = vld1q_u8(input + 16);
        vst1q_u16(pred_buf_q3 + 16, vshll_n_u8(vget_low_u8(next_top), 3));
        vst1q_u16(pred_buf_q3 + 24, vshll_n_u8(vget_high_u8(next_top), 3));
      }
    }
    input += input_stride;
  } while ((pred_buf_q3 += CFL_BUF_LINE) < end);
}

#if CONFIG_AV1_HIGHBITDEPTH
#if !AOM_ARCH_AARCH64
static uint16x8_t vpaddq_u16(uint16x8_t a, uint16x8_t b) {
  return vcombine_u16(vpadd_u16(vget_low_u16(a), vget_high_u16(a)),
                      vpadd_u16(vget_low_u16(b), vget_high_u16(b)));
}
#endif

static void cfl_luma_subsampling_420_hbd_neon(const uint16_t *input,
                                              int input_stride,
                                              uint16_t *pred_buf_q3, int width,
                                              int height) {
  const uint16_t *end = pred_buf_q3 + (height >> 1) * CFL_BUF_LINE;
  const int luma_stride = input_stride << 1;
  do {
    if (width == 4) {
      const uint16x4_t top = vld1_u16(input);
      const uint16x4_t bot = vld1_u16(input + input_stride);
      const uint16x4_t sum = vadd_u16(top, bot);
      const uint16x4_t hsum = vpadd_u16(sum, sum);
      vsth_u16(pred_buf_q3, vshl_n_u16(hsum, 1));
    } else if (width < 32) {
      const uint16x8_t top = vld1q_u16(input);
      const uint16x8_t bot = vld1q_u16(input + input_stride);
      const uint16x8_t sum = vaddq_u16(top, bot);
      if (width == 8) {
        const uint16x4_t hsum = vget_low_u16(vpaddq_u16(sum, sum));
        vst1_u16(pred_buf_q3, vshl_n_u16(hsum, 1));
      } else {
        const uint16x8_t top_1 = vld1q_u16(input + 8);
        const uint16x8_t bot_1 = vld1q_u16(input + 8 + input_stride);
        const uint16x8_t sum_1 = vaddq_u16(top_1, bot_1);
        const uint16x8_t hsum = vpaddq_u16(sum, sum_1);
        vst1q_u16(pred_buf_q3, vshlq_n_u16(hsum, 1));
      }
    } else {
      const uint16x8x4_t top = vld4q_u16(input);
      const uint16x8x4_t bot = vld4q_u16(input + input_stride);
      // equivalent to a vpaddq_u16 (because vld4q interleaves)
      const uint16x8_t top_0 = vaddq_u16(top.val[0], top.val[1]);
      // equivalent to a vpaddq_u16 (because vld4q interleaves)
      const uint16x8_t bot_0 = vaddq_u16(bot.val[0], bot.val[1]);
      // equivalent to a vpaddq_u16 (because vld4q interleaves)
      const uint16x8_t top_1 = vaddq_u16(top.val[2], top.val[3]);
      // equivalent to a vpaddq_u16 (because vld4q interleaves)
      const uint16x8_t bot_1 = vaddq_u16(bot.val[2], bot.val[3]);
      uint16x8x2_t sum;
      sum.val[0] = vshlq_n_u16(vaddq_u16(top_0, bot_0), 1);
      sum.val[1] = vshlq_n_u16(vaddq_u16(top_1, bot_1), 1);
      vst2q_u16(pred_buf_q3, sum);
    }
    input += luma_stride;
  } while ((pred_buf_q3 += CFL_BUF_LINE) < end);
}

static void cfl_luma_subsampling_422_hbd_neon(const uint16_t *input,
                                              int input_stride,
                                              uint16_t *pred_buf_q3, int width,
                                              int height) {
  const uint16_t *end = pred_buf_q3 + height * CFL_BUF_LINE;
  do {
    if (width == 4) {
      const uint16x4_t top = vld1_u16(input);
      const uint16x4_t hsum = vpadd_u16(top, top);
      vsth_u16(pred_buf_q3, vshl_n_u16(hsum, 2));
    } else if (width == 8) {
      const uint16x4x2_t top = vld2_u16(input);
      // equivalent to a vpadd_u16 (because vld2 interleaves)
      const uint16x4_t hsum = vadd_u16(top.val[0], top.val[1]);
      vst1_u16(pred_buf_q3, vshl_n_u16(hsum, 2));
    } else if (width == 16) {
      const uint16x8x2_t top = vld2q_u16(input);
      // equivalent to a vpaddq_u16 (because vld2q interleaves)
      const uint16x8_t hsum = vaddq_u16(top.val[0], top.val[1]);
      vst1q_u16(pred_buf_q3, vshlq_n_u16(hsum, 2));
    } else {
      const uint16x8x4_t top = vld4q_u16(input);
      // equivalent to a vpaddq_u16 (because vld4q interleaves)
      const uint16x8_t hsum_0 = vaddq_u16(top.val[0], top.val[1]);
      // equivalent to a vpaddq_u16 (because vld4q interleaves)
      const uint16x8_t hsum_1 = vaddq_u16(top.val[2], top.val[3]);
      uint16x8x2_t result = { { vshlq_n_u16(hsum_0, 2),
                                vshlq_n_u16(hsum_1, 2) } };
      vst2q_u16(pred_buf_q3, result);
    }
    input += input_stride;
  } while ((pred_buf_q3 += CFL_BUF_LINE) < end);
}

static void cfl_luma_subsampling_444_hbd_neon(const uint16_t *input,
                                              int input_stride,
                                              uint16_t *pred_buf_q3, int width,
                                              int height) {
  const uint16_t *end = pred_buf_q3 + height * CFL_BUF_LINE;
  do {
    if (width == 4) {
      const uint16x4_t top = vld1_u16(input);
      vst1_u16(pred_buf_q3, vshl_n_u16(top, 3));
    } else if (width == 8) {
      const uint16x8_t top = vld1q_u16(input);
      vst1q_u16(pred_buf_q3, vshlq_n_u16(top, 3));
    } else if (width == 16) {
      uint16x8x2_t top = vld2q_u16(input);
      top.val[0] = vshlq_n_u16(top.val[0], 3);
      top.val[1] = vshlq_n_u16(top.val[1], 3);
      vst2q_u16(pred_buf_q3, top);
    } else {
      uint16x8x4_t top = vld4q_u16(input);
      top.val[0] = vshlq_n_u16(top.val[0], 3);
      top.val[1] = vshlq_n_u16(top.val[1], 3);
      top.val[2] = vshlq_n_u16(top.val[2], 3);
      top.val[3] = vshlq_n_u16(top.val[3], 3);
      vst4q_u16(pred_buf_q3, top);
    }
    input += input_stride;
  } while ((pred_buf_q3 += CFL_BUF_LINE) < end);
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

CFL_GET_SUBSAMPLE_FUNCTION(neon)

static inline void subtract_average_neon(const uint16_t *src, int16_t *dst,
                                         int width, int height,
                                         int round_offset,
                                         const int num_pel_log2) {
  const uint16_t *const end = src + height * CFL_BUF_LINE;

  // Round offset is not needed, because NEON will handle the rounding.
  (void)round_offset;

  // To optimize the use of the CPU pipeline, we process 4 rows per iteration
  const int step = 4 * CFL_BUF_LINE;

  // At this stage, the prediction buffer contains scaled reconstructed luma
  // pixels, which are positive integer and only require 15 bits. By using
  // unsigned integer for the sum, we can do one addition operation inside 16
  // bits (8 lanes) before having to convert to 32 bits (4 lanes).
  const uint16_t *sum_buf = src;
  uint32x4_t sum_32x4 = vdupq_n_u32(0);
  do {
    // For all widths, we load, add and combine the data so it fits in 4 lanes.
    if (width == 4) {
      const uint16x4_t a0 =
          vadd_u16(vld1_u16(sum_buf), vld1_u16(sum_buf + CFL_BUF_LINE));
      const uint16x4_t a1 = vadd_u16(vld1_u16(sum_buf + 2 * CFL_BUF_LINE),
                                     vld1_u16(sum_buf + 3 * CFL_BUF_LINE));
      sum_32x4 = vaddq_u32(sum_32x4, vaddl_u16(a0, a1));
    } else if (width == 8) {
      const uint16x8_t a0 = vldaddq_u16(sum_buf, CFL_BUF_LINE);
      const uint16x8_t a1 =
          vldaddq_u16(sum_buf + 2 * CFL_BUF_LINE, CFL_BUF_LINE);
      sum_32x4 = vpadalq_u16(sum_32x4, a0);
      sum_32x4 = vpadalq_u16(sum_32x4, a1);
    } else {
      const uint16x8_t row0 = vldaddq_u16(sum_buf, 8);
      const uint16x8_t row1 = vldaddq_u16(sum_buf + CFL_BUF_LINE, 8);
      const uint16x8_t row2 = vldaddq_u16(sum_buf + 2 * CFL_BUF_LINE, 8);
      const uint16x8_t row3 = vldaddq_u16(sum_buf + 3 * CFL_BUF_LINE, 8);
      sum_32x4 = vpadalq_u16(sum_32x4, row0);
      sum_32x4 = vpadalq_u16(sum_32x4, row1);
      sum_32x4 = vpadalq_u16(sum_32x4, row2);
      sum_32x4 = vpadalq_u16(sum_32x4, row3);

      if (width == 32) {
        const uint16x8_t row0_1 = vldaddq_u16(sum_buf + 16, 8);
        const uint16x8_t row1_1 = vldaddq_u16(sum_buf + CFL_BUF_LINE + 16, 8);
        const uint16x8_t row2_1 =
            vldaddq_u16(sum_buf + 2 * CFL_BUF_LINE + 16, 8);
        const uint16x8_t row3_1 =
            vldaddq_u16(sum_buf + 3 * CFL_BUF_LINE + 16, 8);

        sum_32x4 = vpadalq_u16(sum_32x4, row0_1);
        sum_32x4 = vpadalq_u16(sum_32x4, row1_1);
        sum_32x4 = vpadalq_u16(sum_32x4, row2_1);
        sum_32x4 = vpadalq_u16(sum_32x4, row3_1);
      }
    }
    sum_buf += step;
  } while (sum_buf < end);

  // Permute and add in such a way that each lane contains the block sum.
  // [A+C+B+D, B+D+A+C, C+A+D+B, D+B+C+A]
#if AOM_ARCH_AARCH64
  sum_32x4 = vpaddq_u32(sum_32x4, sum_32x4);
  sum_32x4 = vpaddq_u32(sum_32x4, sum_32x4);
#else
  uint32x4_t flip =
      vcombine_u32(vget_high_u32(sum_32x4), vget_low_u32(sum_32x4));
  sum_32x4 = vaddq_u32(sum_32x4, flip);
  sum_32x4 = vaddq_u32(sum_32x4, vrev64q_u32(sum_32x4));
#endif

  // Computing the average could be done using scalars, but getting off the NEON
  // engine introduces latency, so we use vqrshrn.
  int16x4_t avg_16x4;
  // Constant propagation makes for some ugly code.
  switch (num_pel_log2) {
    case 4: avg_16x4 = vreinterpret_s16_u16(vqrshrn_n_u32(sum_32x4, 4)); break;
    case 5: avg_16x4 = vreinterpret_s16_u16(vqrshrn_n_u32(sum_32x4, 5)); break;
    case 6: avg_16x4 = vreinterpret_s16_u16(vqrshrn_n_u32(sum_32x4, 6)); break;
    case 7: avg_16x4 = vreinterpret_s16_u16(vqrshrn_n_u32(sum_32x4, 7)); break;
    case 8: avg_16x4 = vreinterpret_s16_u16(vqrshrn_n_u32(sum_32x4, 8)); break;
    case 9: avg_16x4 = vreinterpret_s16_u16(vqrshrn_n_u32(sum_32x4, 9)); break;
    case 10:
      avg_16x4 = vreinterpret_s16_u16(vqrshrn_n_u32(sum_32x4, 10));
      break;
    default: assert(0);
  }

  if (width == 4) {
    do {
      vst1_s16(dst, vsub_s16(vreinterpret_s16_u16(vld1_u16(src)), avg_16x4));
      src += CFL_BUF_LINE;
      dst += CFL_BUF_LINE;
    } while (src < end);
  } else {
    const int16x8_t avg_16x8 = vcombine_s16(avg_16x4, avg_16x4);
    do {
      vldsubstq_s16(dst, src, 0, avg_16x8);
      vldsubstq_s16(dst, src, CFL_BUF_LINE, avg_16x8);
      vldsubstq_s16(dst, src, 2 * CFL_BUF_LINE, avg_16x8);
      vldsubstq_s16(dst, src, 3 * CFL_BUF_LINE, avg_16x8);

      if (width > 8) {
        vldsubstq_s16(dst, src, 8, avg_16x8);
        vldsubstq_s16(dst, src, 8 + CFL_BUF_LINE, avg_16x8);
        vldsubstq_s16(dst, src, 8 + 2 * CFL_BUF_LINE, avg_16x8);
        vldsubstq_s16(dst, src, 8 + 3 * CFL_BUF_LINE, avg_16x8);
      }
      if (width == 32) {
        vldsubstq_s16(dst, src, 16, avg_16x8);
        vldsubstq_s16(dst, src, 16 + CFL_BUF_LINE, avg_16x8);
        vldsubstq_s16(dst, src, 16 + 2 * CFL_BUF_LINE, avg_16x8);
        vldsubstq_s16(dst, src, 16 + 3 * CFL_BUF_LINE, avg_16x8);
        vldsubstq_s16(dst, src, 24, avg_16x8);
        vldsubstq_s16(dst, src, 24 + CFL_BUF_LINE, avg_16x8);
        vldsubstq_s16(dst, src, 24 + 2 * CFL_BUF_LINE, avg_16x8);
        vldsubstq_s16(dst, src, 24 + 3 * CFL_BUF_LINE, avg_16x8);
      }
      src += step;
      dst += step;
    } while (src < end);
  }
}

CFL_SUB_AVG_FN(neon)

// Saturating negate 16-bit integers in a when the corresponding signed 16-bit
// integer in b is negative.
// Notes:
//   * Negating INT16_MIN results in INT16_MIN. However, this cannot occur in
//   practice, as scaled_luma is the multiplication of two absolute values.
//   * In the Intel equivalent, elements in a are zeroed out when the
//   corresponding elements in b are zero. Because vsign is used twice in a
//   row, with b in the first call becoming a in the second call, there's no
//   impact from not zeroing out.
static int16x4_t vsign_s16(int16x4_t a, int16x4_t b) {
  const int16x4_t mask = vshr_n_s16(b, 15);
  return veor_s16(vadd_s16(a, mask), mask);
}

// Saturating negate 16-bit integers in a when the corresponding signed 16-bit
// integer in b is negative.
// Notes:
//   * Negating INT16_MIN results in INT16_MIN. However, this cannot occur in
//   practice, as scaled_luma is the multiplication of two absolute values.
//   * In the Intel equivalent, elements in a are zeroed out when the
//   corresponding elements in b are zero. Because vsignq is used twice in a
//   row, with b in the first call becoming a in the second call, there's no
//   impact from not zeroing out.
static int16x8_t vsignq_s16(int16x8_t a, int16x8_t b) {
  const int16x8_t mask = vshrq_n_s16(b, 15);
  return veorq_s16(vaddq_s16(a, mask), mask);
}

static inline int16x4_t predict_w4(const int16_t *pred_buf_q3,
                                   int16x4_t alpha_sign, int abs_alpha_q12,
                                   int16x4_t dc) {
  const int16x4_t ac_q3 = vld1_s16(pred_buf_q3);
  const int16x4_t ac_sign = veor_s16(alpha_sign, ac_q3);
  int16x4_t scaled_luma = vqrdmulh_n_s16(vabs_s16(ac_q3), abs_alpha_q12);
  return vadd_s16(vsign_s16(scaled_luma, ac_sign), dc);
}

static inline int16x8_t predict_w8(const int16_t *pred_buf_q3,
                                   int16x8_t alpha_sign, int abs_alpha_q12,
                                   int16x8_t dc) {
  const int16x8_t ac_q3 = vld1q_s16(pred_buf_q3);
  const int16x8_t ac_sign = veorq_s16(alpha_sign, ac_q3);
  int16x8_t scaled_luma = vqrdmulhq_n_s16(vabsq_s16(ac_q3), abs_alpha_q12);
  return vaddq_s16(vsignq_s16(scaled_luma, ac_sign), dc);
}

static inline int16x8x2_t predict_w16(const int16_t *pred_buf_q3,
                                      int16x8_t alpha_sign, int abs_alpha_q12,
                                      int16x8_t dc) {
  // vld2q_s16 interleaves, which is not useful for prediction. vst1q_s16_x2
  // does not interleave, but is not currently available in the compilier used
  // by the AOM build system.
  const int16x8x2_t ac_q3 = vld2q_s16(pred_buf_q3);
  const int16x8_t ac_sign_0 = veorq_s16(alpha_sign, ac_q3.val[0]);
  const int16x8_t ac_sign_1 = veorq_s16(alpha_sign, ac_q3.val[1]);
  const int16x8_t scaled_luma_0 =
      vqrdmulhq_n_s16(vabsq_s16(ac_q3.val[0]), abs_alpha_q12);
  const int16x8_t scaled_luma_1 =
      vqrdmulhq_n_s16(vabsq_s16(ac_q3.val[1]), abs_alpha_q12);
  int16x8x2_t result;
  result.val[0] = vaddq_s16(vsignq_s16(scaled_luma_0, ac_sign_0), dc);
  result.val[1] = vaddq_s16(vsignq_s16(scaled_luma_1, ac_sign_1), dc);
  return result;
}

static inline int16x8x4_t predict_w32(const int16_t *pred_buf_q3,
                                      int16x8_t alpha_sign, int abs_alpha_q12,
                                      int16x8_t dc) {
  // vld4q_s16 interleaves, which is not useful for prediction. vst1q_s16_x4
  // does not interleave, but is not currently available in the compilier used
  // by the AOM build system.
  const int16x8x4_t ac_q3 = vld4q_s16(pred_buf_q3);
  const int16x8_t ac_sign_0 = veorq_s16(alpha_sign, ac_q3.val[0]);
  const int16x8_t ac_sign_1 = veorq_s16(alpha_sign, ac_q3.val[1]);
  const int16x8_t ac_sign_2 = veorq_s16(alpha_sign, ac_q3.val[2]);
  const int16x8_t ac_sign_3 = veorq_s16(alpha_sign, ac_q3.val[3]);
  const int16x8_t scaled_luma_0 =
      vqrdmulhq_n_s16(vabsq_s16(ac_q3.val[0]), abs_alpha_q12);
  const int16x8_t scaled_luma_1 =
      vqrdmulhq_n_s16(vabsq_s16(ac_q3.val[1]), abs_alpha_q12);
  const int16x8_t scaled_luma_2 =
      vqrdmulhq_n_s16(vabsq_s16(ac_q3.val[2]), abs_alpha_q12);
  const int16x8_t scaled_luma_3 =
      vqrdmulhq_n_s16(vabsq_s16(ac_q3.val[3]), abs_alpha_q12);
  int16x8x4_t result;
  result.val[0] = vaddq_s16(vsignq_s16(scaled_luma_0, ac_sign_0), dc);
  result.val[1] = vaddq_s16(vsignq_s16(scaled_luma_1, ac_sign_1), dc);
  result.val[2] = vaddq_s16(vsignq_s16(scaled_luma_2, ac_sign_2), dc);
  result.val[3] = vaddq_s16(vsignq_s16(scaled_luma_3, ac_sign_3), dc);
  return result;
}

static inline void cfl_predict_lbd_neon(const int16_t *pred_buf_q3,
                                        uint8_t *dst, int dst_stride,
                                        int alpha_q3, int width, int height) {
  const int16_t abs_alpha_q12 = abs(alpha_q3) << 9;
  const int16_t *const end = pred_buf_q3 + height * CFL_BUF_LINE;
  if (width == 4) {
    const int16x4_t alpha_sign = vdup_n_s16(alpha_q3);
    const int16x4_t dc = vdup_n_s16(*dst);
    do {
      const int16x4_t pred =
          predict_w4(pred_buf_q3, alpha_sign, abs_alpha_q12, dc);
      vsth_u8(dst, vqmovun_s16(vcombine_s16(pred, pred)));
      dst += dst_stride;
    } while ((pred_buf_q3 += CFL_BUF_LINE) < end);
  } else {
    const int16x8_t alpha_sign = vdupq_n_s16(alpha_q3);
    const int16x8_t dc = vdupq_n_s16(*dst);
    do {
      if (width == 8) {
        vst1_u8(dst, vqmovun_s16(predict_w8(pred_buf_q3, alpha_sign,
                                            abs_alpha_q12, dc)));
      } else if (width == 16) {
        const int16x8x2_t pred =
            predict_w16(pred_buf_q3, alpha_sign, abs_alpha_q12, dc);
        const uint8x8x2_t predun = { { vqmovun_s16(pred.val[0]),
                                       vqmovun_s16(pred.val[1]) } };
        vst2_u8(dst, predun);
      } else {
        const int16x8x4_t pred =
            predict_w32(pred_buf_q3, alpha_sign, abs_alpha_q12, dc);
        const uint8x8x4_t predun = {
          { vqmovun_s16(pred.val[0]), vqmovun_s16(pred.val[1]),
            vqmovun_s16(pred.val[2]), vqmovun_s16(pred.val[3]) }
        };
        vst4_u8(dst, predun);
      }
      dst += dst_stride;
    } while ((pred_buf_q3 += CFL_BUF_LINE) < end);
  }
}

CFL_PREDICT_FN(neon, lbd)

#if CONFIG_AV1_HIGHBITDEPTH
static inline uint16x4_t clamp_s16(int16x4_t a, int16x4_t max) {
  return vreinterpret_u16_s16(vmax_s16(vmin_s16(a, max), vdup_n_s16(0)));
}

static inline uint16x8_t clampq_s16(int16x8_t a, int16x8_t max) {
  return vreinterpretq_u16_s16(vmaxq_s16(vminq_s16(a, max), vdupq_n_s16(0)));
}

static inline uint16x8x2_t clamp2q_s16(int16x8x2_t a, int16x8_t max) {
  uint16x8x2_t result;
  result.val[0] = vreinterpretq_u16_s16(
      vmaxq_s16(vminq_s16(a.val[0], max), vdupq_n_s16(0)));
  result.val[1] = vreinterpretq_u16_s16(
      vmaxq_s16(vminq_s16(a.val[1], max), vdupq_n_s16(0)));
  return result;
}

static inline uint16x8x4_t clamp4q_s16(int16x8x4_t a, int16x8_t max) {
  uint16x8x4_t result;
  result.val[0] = vreinterpretq_u16_s16(
      vmaxq_s16(vminq_s16(a.val[0], max), vdupq_n_s16(0)));
  result.val[1] = vreinterpretq_u16_s16(
      vmaxq_s16(vminq_s16(a.val[1], max), vdupq_n_s16(0)));
  result.val[2] = vreinterpretq_u16_s16(
      vmaxq_s16(vminq_s16(a.val[2], max), vdupq_n_s16(0)));
  result.val[3] = vreinterpretq_u16_s16(
      vmaxq_s16(vminq_s16(a.val[3], max), vdupq_n_s16(0)));
  return result;
}

static inline void cfl_predict_hbd_neon(const int16_t *pred_buf_q3,
                                        uint16_t *dst, int dst_stride,
                                        int alpha_q3, int bd, int width,
                                        int height) {
  const int max = (1 << bd) - 1;
  const int16_t abs_alpha_q12 = abs(alpha_q3) << 9;
  const int16_t *const end = pred_buf_q3 + height * CFL_BUF_LINE;
  if (width == 4) {
    const int16x4_t alpha_sign = vdup_n_s16(alpha_q3);
    const int16x4_t dc = vdup_n_s16(*dst);
    const int16x4_t max_16x4 = vdup_n_s16(max);
    do {
      const int16x4_t scaled_luma =
          predict_w4(pred_buf_q3, alpha_sign, abs_alpha_q12, dc);
      vst1_u16(dst, clamp_s16(scaled_luma, max_16x4));
      dst += dst_stride;
    } while ((pred_buf_q3 += CFL_BUF_LINE) < end);
  } else {
    const int16x8_t alpha_sign = vdupq_n_s16(alpha_q3);
    const int16x8_t dc = vdupq_n_s16(*dst);
    const int16x8_t max_16x8 = vdupq_n_s16(max);
    do {
      if (width == 8) {
        const int16x8_t pred =
            predict_w8(pred_buf_q3, alpha_sign, abs_alpha_q12, dc);
        vst1q_u16(dst, clampq_s16(pred, max_16x8));
      } else if (width == 16) {
        const int16x8x2_t pred =
            predict_w16(pred_buf_q3, alpha_sign, abs_alpha_q12, dc);
        vst2q_u16(dst, clamp2q_s16(pred, max_16x8));
      } else {
        const int16x8x4_t pred =
            predict_w32(pred_buf_q3, alpha_sign, abs_alpha_q12, dc);
        vst4q_u16(dst, clamp4q_s16(pred, max_16x8));
      }
      dst += dst_stride;
    } while ((pred_buf_q3 += CFL_BUF_LINE) < end);
  }
}

CFL_PREDICT_FN(neon, hbd)
#endif  // CONFIG_AV1_HIGHBITDEPTH
