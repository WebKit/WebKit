/*
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved
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

#include "aom_dsp/arm/transpose_neon.h"
#include "aom_dsp/arm/sum_neon.h"
#include "av1/common/restoration.h"
#include "av1/encoder/pickrst.h"

int64_t av1_lowbd_pixel_proj_error_neon(
    const uint8_t *src8, int width, int height, int src_stride,
    const uint8_t *dat8, int dat_stride, int32_t *flt0, int flt0_stride,
    int32_t *flt1, int flt1_stride, int xq[2], const sgr_params_type *params) {
  int i, j, k;
  const int32_t shift = SGRPROJ_RST_BITS + SGRPROJ_PRJ_BITS;
  const int32x4_t zero = vdupq_n_s32(0);
  uint64x2_t sum64 = vreinterpretq_u64_s32(zero);
  const uint8_t *src = src8;
  const uint8_t *dat = dat8;

  int64_t err = 0;
  if (params->r[0] > 0 && params->r[1] > 0) {
    for (i = 0; i < height; ++i) {
      int32x4_t err0 = zero;
      for (j = 0; j <= width - 8; j += 8) {
        const uint8x8_t d0 = vld1_u8(&dat[j]);
        const uint8x8_t s0 = vld1_u8(&src[j]);
        const int16x8_t flt0_16b =
            vcombine_s16(vqmovn_s32(vld1q_s32(&flt0[j])),
                         vqmovn_s32(vld1q_s32(&flt0[j + 4])));
        const int16x8_t flt1_16b =
            vcombine_s16(vqmovn_s32(vld1q_s32(&flt1[j])),
                         vqmovn_s32(vld1q_s32(&flt1[j + 4])));
        const int16x8_t u0 =
            vreinterpretq_s16_u16(vshll_n_u8(d0, SGRPROJ_RST_BITS));
        const int16x8_t flt0_0_sub_u = vsubq_s16(flt0_16b, u0);
        const int16x8_t flt1_0_sub_u = vsubq_s16(flt1_16b, u0);
        const int16x4_t flt0_16b_sub_u_lo = vget_low_s16(flt0_0_sub_u);
        const int16x4_t flt0_16b_sub_u_hi = vget_high_s16(flt0_0_sub_u);
        const int16x4_t flt1_16b_sub_u_lo = vget_low_s16(flt1_0_sub_u);
        const int16x4_t flt1_16b_sub_u_hi = vget_high_s16(flt1_0_sub_u);

        int32x4_t v0 = vmull_n_s16(flt0_16b_sub_u_lo, (int16_t)xq[0]);
        v0 = vmlal_n_s16(v0, flt1_16b_sub_u_lo, (int16_t)xq[1]);
        int32x4_t v1 = vmull_n_s16(flt0_16b_sub_u_hi, (int16_t)xq[0]);
        v1 = vmlal_n_s16(v1, flt1_16b_sub_u_hi, (int16_t)xq[1]);
        const int16x4_t vr0 = vqrshrn_n_s32(v0, 11);
        const int16x4_t vr1 = vqrshrn_n_s32(v1, 11);
        const int16x8_t e0 = vaddq_s16(vcombine_s16(vr0, vr1),
                                       vreinterpretq_s16_u16(vsubl_u8(d0, s0)));
        const int16x4_t e0_lo = vget_low_s16(e0);
        const int16x4_t e0_hi = vget_high_s16(e0);
        err0 = vmlal_s16(err0, e0_lo, e0_lo);
        err0 = vmlal_s16(err0, e0_hi, e0_hi);
      }
      for (k = j; k < width; ++k) {
        const int32_t u = dat[k] << SGRPROJ_RST_BITS;
        int32_t v = xq[0] * (flt0[k] - u) + xq[1] * (flt1[k] - u);
        const int32_t e = ROUND_POWER_OF_TWO(v, 11) + dat[k] - src[k];
        err += e * e;
      }
      dat += dat_stride;
      src += src_stride;
      flt0 += flt0_stride;
      flt1 += flt1_stride;
      sum64 = vpadalq_u32(sum64, vreinterpretq_u32_s32(err0));
    }

  } else if (params->r[0] > 0 || params->r[1] > 0) {
    const int xq_active = (params->r[0] > 0) ? xq[0] : xq[1];
    const int32_t *flt = (params->r[0] > 0) ? flt0 : flt1;
    const int flt_stride = (params->r[0] > 0) ? flt0_stride : flt1_stride;
    for (i = 0; i < height; ++i) {
      int32x4_t err0 = zero;
      for (j = 0; j <= width - 8; j += 8) {
        const uint8x8_t d0 = vld1_u8(&dat[j]);
        const uint8x8_t s0 = vld1_u8(&src[j]);
        const uint16x8_t d0s0 = vsubl_u8(d0, s0);
        const uint16x8x2_t d0w =
            vzipq_u16(vmovl_u8(d0), vreinterpretq_u16_s32(zero));

        const int32x4_t flt_16b_lo = vld1q_s32(&flt[j]);
        const int32x4_t flt_16b_hi = vld1q_s32(&flt[j + 4]);

        int32x4_t v0 = vmulq_n_s32(flt_16b_lo, xq_active);
        v0 = vmlsq_n_s32(v0, vreinterpretq_s32_u16(d0w.val[0]),
                         xq_active * (1 << SGRPROJ_RST_BITS));
        int32x4_t v1 = vmulq_n_s32(flt_16b_hi, xq_active);
        v1 = vmlsq_n_s32(v1, vreinterpretq_s32_u16(d0w.val[1]),
                         xq_active * (1 << SGRPROJ_RST_BITS));
        const int16x4_t vr0 = vqrshrn_n_s32(v0, 11);
        const int16x4_t vr1 = vqrshrn_n_s32(v1, 11);
        const int16x8_t e0 =
            vaddq_s16(vcombine_s16(vr0, vr1), vreinterpretq_s16_u16(d0s0));
        const int16x4_t e0_lo = vget_low_s16(e0);
        const int16x4_t e0_hi = vget_high_s16(e0);
        err0 = vmlal_s16(err0, e0_lo, e0_lo);
        err0 = vmlal_s16(err0, e0_hi, e0_hi);
      }
      for (k = j; k < width; ++k) {
        const int32_t u = dat[k] << SGRPROJ_RST_BITS;
        int32_t v = xq_active * (flt[k] - u);
        const int32_t e = ROUND_POWER_OF_TWO(v, shift) + dat[k] - src[k];
        err += e * e;
      }
      dat += dat_stride;
      src += src_stride;
      flt += flt_stride;
      sum64 = vpadalq_u32(sum64, vreinterpretq_u32_s32(err0));
    }
  } else {
    uint32x4_t err0 = vreinterpretq_u32_s32(zero);
    for (i = 0; i < height; ++i) {
      for (j = 0; j <= width - 16; j += 16) {
        const uint8x16_t d = vld1q_u8(&dat[j]);
        const uint8x16_t s = vld1q_u8(&src[j]);
        const uint8x16_t diff = vabdq_u8(d, s);
        const uint8x8_t diff0 = vget_low_u8(diff);
        const uint8x8_t diff1 = vget_high_u8(diff);
        err0 = vpadalq_u16(err0, vmull_u8(diff0, diff0));
        err0 = vpadalq_u16(err0, vmull_u8(diff1, diff1));
      }
      for (k = j; k < width; ++k) {
        const int32_t e = dat[k] - src[k];
        err += e * e;
      }
      dat += dat_stride;
      src += src_stride;
    }
    sum64 = vpaddlq_u32(err0);
  }
#if AOM_ARCH_AARCH64
  err += vaddvq_u64(sum64);
#else
  err += vget_lane_u64(vadd_u64(vget_low_u64(sum64), vget_high_u64(sum64)), 0);
#endif  // AOM_ARCH_AARCH64
  return err;
}

static INLINE uint8_t find_average_neon(const uint8_t *src, int src_stride,
                                        int width, int height) {
  uint64_t sum = 0;

  if (width >= 16) {
    int h = 0;
    // We can accumulate up to 257 8-bit values in a 16-bit value, given
    // that each 16-bit vector has 8 elements, that means we can process up to
    // int(257*8/width) rows before we need to widen to 32-bit vector
    // elements.
    int h_overflow = 257 * 8 / width;
    int h_limit = height > h_overflow ? h_overflow : height;
    uint32x4_t avg_u32 = vdupq_n_u32(0);
    do {
      uint16x8_t avg_u16 = vdupq_n_u16(0);
      do {
        int j = width;
        const uint8_t *src_ptr = src;
        do {
          uint8x16_t s = vld1q_u8(src_ptr);
          avg_u16 = vpadalq_u8(avg_u16, s);
          j -= 16;
          src_ptr += 16;
        } while (j >= 16);
        if (j >= 8) {
          uint8x8_t s = vld1_u8(src_ptr);
          avg_u16 = vaddw_u8(avg_u16, s);
          j -= 8;
          src_ptr += 8;
        }
        // Scalar tail case.
        while (j > 0) {
          sum += src[width - j];
          j--;
        }
        src += src_stride;
      } while (++h < h_limit);
      avg_u32 = vpadalq_u16(avg_u32, avg_u16);

      h_limit += h_overflow;
      h_limit = height > h_overflow ? h_overflow : height;
    } while (h < height);
    return (uint8_t)((horizontal_long_add_u32x4(avg_u32) + sum) /
                     (width * height));
  }
  if (width >= 8) {
    int h = 0;
    // We can accumulate up to 257 8-bit values in a 16-bit value, given
    // that each 16-bit vector has 4 elements, that means we can process up to
    // int(257*4/width) rows before we need to widen to 32-bit vector
    // elements.
    int h_overflow = 257 * 4 / width;
    int h_limit = height > h_overflow ? h_overflow : height;
    uint32x2_t avg_u32 = vdup_n_u32(0);
    do {
      uint16x4_t avg_u16 = vdup_n_u16(0);
      do {
        int j = width;
        const uint8_t *src_ptr = src;
        uint8x8_t s = vld1_u8(src_ptr);
        avg_u16 = vpadal_u8(avg_u16, s);
        j -= 8;
        src_ptr += 8;
        // Scalar tail case.
        while (j > 0) {
          sum += src[width - j];
          j--;
        }
        src += src_stride;
      } while (++h < h_limit);
      avg_u32 = vpadal_u16(avg_u32, avg_u16);

      h_limit += h_overflow;
      h_limit = height > h_overflow ? h_overflow : height;
    } while (h < height);
    return (uint8_t)((horizontal_long_add_u32x2(avg_u32) + sum) /
                     (width * height));
  }
  int i = height;
  do {
    int j = 0;
    do {
      sum += src[j];
    } while (++j < width);
    src += src_stride;
  } while (--i != 0);
  return (uint8_t)(sum / (width * height));
}

static INLINE void compute_sub_avg(const uint8_t *buf, int buf_stride, int avg,
                                   int16_t *buf_avg, int buf_avg_stride,
                                   int width, int height,
                                   int downsample_factor) {
  uint8x8_t avg_u8 = vdup_n_u8(avg);

  if (width > 8) {
    int i = 0;
    do {
      int j = width;
      const uint8_t *buf_ptr = buf;
      int16_t *buf_avg_ptr = buf_avg;
      do {
        uint8x8_t d = vld1_u8(buf_ptr);
        vst1q_s16(buf_avg_ptr, vreinterpretq_s16_u16(vsubl_u8(d, avg_u8)));

        j -= 8;
        buf_ptr += 8;
        buf_avg_ptr += 8;
      } while (j >= 8);
      while (j > 0) {
        *buf_avg_ptr = (int16_t)buf[width - j] - (int16_t)avg;
        buf_avg_ptr++;
        j--;
      }
      buf += buf_stride;
      buf_avg += buf_avg_stride;
      i += downsample_factor;
    } while (i < height);
  } else {
    // For width < 8, don't use Neon.
    for (int i = 0; i < height; i = i + downsample_factor) {
      for (int j = 0; j < width; j++) {
        buf_avg[j] = (int16_t)buf[j] - (int16_t)avg;
      }
      buf += buf_stride;
      buf_avg += buf_avg_stride;
    }
  }
}

static INLINE void copy_upper_triangle(int64_t *H, const int wiener_win2) {
  for (int i = 0; i < wiener_win2 - 2; i = i + 2) {
    // Transpose the first 2x2 square. It needs a special case as the element
    // of the bottom left is on the diagonal.
    int64x2_t row0 = vld1q_s64(H + i * wiener_win2 + i + 1);
    int64x2_t row1 = vld1q_s64(H + (i + 1) * wiener_win2 + i + 1);

    int64x2_t tr_row = aom_vtrn2q_s64(row0, row1);

    vst1_s64(H + (i + 1) * wiener_win2 + i, vget_low_s64(row0));
    vst1q_s64(H + (i + 2) * wiener_win2 + i, tr_row);

    // Transpose and store all the remaining 2x2 squares of the line.
    for (int j = i + 3; j < wiener_win2; j = j + 2) {
      row0 = vld1q_s64(H + i * wiener_win2 + j);
      row1 = vld1q_s64(H + (i + 1) * wiener_win2 + j);

      int64x2_t tr_row0 = aom_vtrn1q_s64(row0, row1);
      int64x2_t tr_row1 = aom_vtrn2q_s64(row0, row1);

      vst1q_s64(H + j * wiener_win2 + i, tr_row0);
      vst1q_s64(H + (j + 1) * wiener_win2 + i, tr_row1);
    }
  }
}

static INLINE void transpose_M_win7(int64_t *M, int64_t *M_trn,
                                    const int wiener_win) {
  // 1st and 2nd rows.
  int64x2_t row00 = vld1q_s64(M_trn);
  int64x2_t row10 = vld1q_s64(M_trn + wiener_win);
  vst1q_s64(M, aom_vtrn1q_s64(row00, row10));
  vst1q_s64(M + wiener_win, aom_vtrn2q_s64(row00, row10));

  int64x2_t row02 = vld1q_s64(M_trn + 2);
  int64x2_t row12 = vld1q_s64(M_trn + wiener_win + 2);
  vst1q_s64(M + 2 * wiener_win, aom_vtrn1q_s64(row02, row12));
  vst1q_s64(M + 3 * wiener_win, aom_vtrn2q_s64(row02, row12));

  int64x2_t row04 = vld1q_s64(M_trn + 4);
  int64x2_t row14 = vld1q_s64(M_trn + wiener_win + 4);
  vst1q_s64(M + 4 * wiener_win, aom_vtrn1q_s64(row04, row14));
  vst1q_s64(M + 5 * wiener_win, aom_vtrn2q_s64(row04, row14));

  // Last column only needs trn2.
  int64x2_t row05 = vld1q_s64(M_trn + 5);
  int64x2_t row15 = vld1q_s64(M_trn + wiener_win + 5);
  vst1q_s64(M + 6 * wiener_win, aom_vtrn2q_s64(row05, row15));

  // 3rd and 4th rows.
  int64x2_t row20 = vld1q_s64(M_trn + 2 * wiener_win);
  int64x2_t row30 = vld1q_s64(M_trn + 3 * wiener_win);
  vst1q_s64(M + 2, aom_vtrn1q_s64(row20, row30));
  vst1q_s64(M + wiener_win + 2, aom_vtrn2q_s64(row20, row30));

  int64x2_t row22 = vld1q_s64(M_trn + 2 * wiener_win + 2);
  int64x2_t row32 = vld1q_s64(M_trn + 3 * wiener_win + 2);
  vst1q_s64(M + 2 * wiener_win + 2, aom_vtrn1q_s64(row22, row32));
  vst1q_s64(M + 3 * wiener_win + 2, aom_vtrn2q_s64(row22, row32));

  int64x2_t row24 = vld1q_s64(M_trn + 2 * wiener_win + 4);
  int64x2_t row34 = vld1q_s64(M_trn + 3 * wiener_win + 4);
  vst1q_s64(M + 4 * wiener_win + 2, aom_vtrn1q_s64(row24, row34));
  vst1q_s64(M + 5 * wiener_win + 2, aom_vtrn2q_s64(row24, row34));

  // Last column only needs trn2.
  int64x2_t row25 = vld1q_s64(M_trn + 2 * wiener_win + 5);
  int64x2_t row35 = vld1q_s64(M_trn + 3 * wiener_win + 5);
  vst1q_s64(M + 6 * wiener_win + 2, aom_vtrn2q_s64(row25, row35));

  // 5th and 6th rows.
  int64x2_t row40 = vld1q_s64(M_trn + 4 * wiener_win);
  int64x2_t row50 = vld1q_s64(M_trn + 5 * wiener_win);
  vst1q_s64(M + 4, aom_vtrn1q_s64(row40, row50));
  vst1q_s64(M + wiener_win + 4, aom_vtrn2q_s64(row40, row50));

  int64x2_t row42 = vld1q_s64(M_trn + 4 * wiener_win + 2);
  int64x2_t row52 = vld1q_s64(M_trn + 5 * wiener_win + 2);
  vst1q_s64(M + 2 * wiener_win + 4, aom_vtrn1q_s64(row42, row52));
  vst1q_s64(M + 3 * wiener_win + 4, aom_vtrn2q_s64(row42, row52));

  int64x2_t row44 = vld1q_s64(M_trn + 4 * wiener_win + 4);
  int64x2_t row54 = vld1q_s64(M_trn + 5 * wiener_win + 4);
  vst1q_s64(M + 4 * wiener_win + 4, aom_vtrn1q_s64(row44, row54));
  vst1q_s64(M + 5 * wiener_win + 4, aom_vtrn2q_s64(row44, row54));

  // Last column only needs trn2.
  int64x2_t row45 = vld1q_s64(M_trn + 4 * wiener_win + 5);
  int64x2_t row55 = vld1q_s64(M_trn + 5 * wiener_win + 5);
  vst1q_s64(M + 6 * wiener_win + 4, aom_vtrn2q_s64(row45, row55));

  // Last row.
  int64x2_t row60 = vld1q_s64(M_trn + 6 * wiener_win);
  vst1_s64(M + 6, vget_low_s64(row60));
  vst1_s64(M + 1 * wiener_win + 6, vget_high_s64(row60));

  int64x2_t row62 = vld1q_s64(M_trn + 6 * wiener_win + 2);
  vst1_s64(M + 2 * wiener_win + 6, vget_low_s64(row62));
  vst1_s64(M + 3 * wiener_win + 6, vget_high_s64(row62));

  int64x2_t row64 = vld1q_s64(M_trn + 6 * wiener_win + 4);
  vst1_s64(M + 4 * wiener_win + 6, vget_low_s64(row64));
  vst1_s64(M + 5 * wiener_win + 6, vget_high_s64(row64));

  // Element on the bottom right of M_trn is copied as is.
  vst1_s64(M + 6 * wiener_win + 6, vld1_s64(M_trn + 6 * wiener_win + 6));
}

static INLINE void compute_M_one_row_win7(int16x8_t src, int16x8_t dgd0,
                                          int16x8_t dgd1, int64_t *M,
                                          const int wiener_win, int row) {
  int64x2_t m_01 = vld1q_s64(M + row * wiener_win + 0);
  int64x2_t m_23 = vld1q_s64(M + row * wiener_win + 2);
  int64x2_t m_45 = vld1q_s64(M + row * wiener_win + 4);

  int32x4_t m0 = vmull_s16(vget_low_s16(src), vget_low_s16(dgd0));
  m0 = vmlal_s16(m0, vget_high_s16(src), vget_high_s16(dgd0));

  int16x8_t dgd01 = vextq_s16(dgd0, dgd1, 1);
  int32x4_t m1 = vmull_s16(vget_low_s16(src), vget_low_s16(dgd01));
  m1 = vmlal_s16(m1, vget_high_s16(src), vget_high_s16(dgd01));

  m0 = horizontal_add_2d_s32(m0, m1);
  m_01 = vpadalq_s32(m_01, m0);
  vst1q_s64(M + row * wiener_win + 0, m_01);

  int16x8_t dgd02 = vextq_s16(dgd0, dgd1, 2);
  int32x4_t m2 = vmull_s16(vget_low_s16(src), vget_low_s16(dgd02));
  m2 = vmlal_s16(m2, vget_high_s16(src), vget_high_s16(dgd02));

  int16x8_t dgd03 = vextq_s16(dgd0, dgd1, 3);
  int32x4_t m3 = vmull_s16(vget_low_s16(src), vget_low_s16(dgd03));
  m3 = vmlal_s16(m3, vget_high_s16(src), vget_high_s16(dgd03));

  m2 = horizontal_add_2d_s32(m2, m3);
  m_23 = vpadalq_s32(m_23, m2);
  vst1q_s64(M + row * wiener_win + 2, m_23);

  int16x8_t dgd04 = vextq_s16(dgd0, dgd1, 4);
  int32x4_t m4 = vmull_s16(vget_low_s16(src), vget_low_s16(dgd04));
  m4 = vmlal_s16(m4, vget_high_s16(src), vget_high_s16(dgd04));

  int16x8_t dgd05 = vextq_s16(dgd0, dgd1, 5);
  int32x4_t m5 = vmull_s16(vget_low_s16(src), vget_low_s16(dgd05));
  m5 = vmlal_s16(m5, vget_high_s16(src), vget_high_s16(dgd05));

  m4 = horizontal_add_2d_s32(m4, m5);
  m_45 = vpadalq_s32(m_45, m4);
  vst1q_s64(M + row * wiener_win + 4, m_45);

  int16x8_t dgd06 = vextq_s16(dgd0, dgd1, 6);
  int32x4_t m6 = vmull_s16(vget_low_s16(src), vget_low_s16(dgd06));
  m6 = vmlal_s16(m6, vget_high_s16(src), vget_high_s16(dgd06));
  M[row * wiener_win + 6] += horizontal_long_add_s32x4(m6);
}

static INLINE void compute_H_one_col(int16x8_t *dgd, int col, int64_t *H,
                                     const int wiener_win,
                                     const int wiener_win2, int32x4_t df_s32) {
  for (int row0 = 0; row0 < wiener_win; row0++) {
    for (int row1 = row0; row1 < wiener_win; row1++) {
      int auto_cov_idx =
          (col * wiener_win + row0) * wiener_win2 + (col * wiener_win) + row1;

      int32x4_t auto_cov =
          vmull_s16(vget_low_s16(dgd[row0]), vget_low_s16(dgd[row1]));
      auto_cov = vmlal_s16(auto_cov, vget_high_s16(dgd[row0]),
                           vget_high_s16(dgd[row1]));
      auto_cov = vshlq_s32(auto_cov, df_s32);

      H[auto_cov_idx] += horizontal_long_add_s32x4(auto_cov);
    }
  }
}

static INLINE void compute_H_two_cols(int16x8_t *dgd0, int16x8_t *dgd1,
                                      int col0, int col1, int64_t *H,
                                      const int wiener_win,
                                      const int wiener_win2) {
  for (int row0 = 0; row0 < wiener_win; row0++) {
    for (int row1 = 0; row1 < wiener_win; row1++) {
      int auto_cov_idx =
          (col0 * wiener_win + row0) * wiener_win2 + (col1 * wiener_win) + row1;

      int32x4_t auto_cov =
          vmull_s16(vget_low_s16(dgd0[row0]), vget_low_s16(dgd1[row1]));
      auto_cov = vmlal_s16(auto_cov, vget_high_s16(dgd0[row0]),
                           vget_high_s16(dgd1[row1]));

      H[auto_cov_idx] += horizontal_long_add_s32x4(auto_cov);
    }
  }
}

static INLINE void compute_H_one_col_last_row(int16x8_t *dgd, int col,
                                              int64_t *H, const int wiener_win,
                                              const int wiener_win2,
                                              int last_row_df) {
  for (int row0 = 0; row0 < wiener_win; row0++) {
    for (int row1 = row0; row1 < wiener_win; row1++) {
      int auto_cov_idx =
          (col * wiener_win + row0) * wiener_win2 + (col * wiener_win) + row1;

      int32x4_t auto_cov =
          vmull_s16(vget_low_s16(dgd[row0]), vget_low_s16(dgd[row1]));
      auto_cov = vmlal_s16(auto_cov, vget_high_s16(dgd[row0]),
                           vget_high_s16(dgd[row1]));
      auto_cov = vmulq_n_s32(auto_cov, last_row_df);

      H[auto_cov_idx] += horizontal_long_add_s32x4(auto_cov);
    }
  }
}

// When we load 8 values of int16_t type and need less than 8 values for
// processing, the below mask is used to make the extra values zero.
static const int16_t mask_16bit[16] = {
  -1, -1, -1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0,
};

// This function computes two matrices: the cross-correlation between the src
// buffer and dgd buffer (M), and the auto-covariance of the dgd buffer (H).
//
// M is of size 7 * 7. It needs to be filled such that multiplying one element
// from src with each element of a row of the wiener window will fill one
// column of M. However this is not very convenient in terms of memory
// accesses, as it means we do contiguous loads of dgd but strided stores to M.
// As a result, we use an intermediate matrix M_trn which is instead filled
// such that one row of the wiener window gives one row of M_trn. Once fully
// computed, M_trn is then transposed to return M.
//
// H is of size 49 * 49. It is filled by multiplying every pair of elements of
// the wiener window together. Since it is a symmetric matrix, we only compute
// the upper triangle, and then copy it down to the lower one. Here we fill it
// by taking each different pair of columns, and multiplying all the elements of
// the first one with all the elements of the second one, with a special case
// when multiplying a column by itself.
static INLINE void compute_stats_win7_neon(int16_t *dgd_avg, int dgd_avg_stride,
                                           int16_t *src_avg, int src_avg_stride,
                                           int width, int v_start, int v_end,
                                           int64_t *M, int64_t *H,
                                           int downsample_factor,
                                           int last_row_downsample_factor) {
  const int wiener_win = 7;
  const int wiener_win2 = wiener_win * wiener_win;
  // The downsample factor can be either 1 or 4, so instead of multiplying the
  // values by 1 or 4, we can left shift by 0 or 2 respectively, which is
  // faster. (This doesn't apply to the last row where we can scale the values
  // by 1, 2 or 3, so we keep the multiplication).
  const int downsample_shift = downsample_factor >> 1;
  const int16x8_t df_s16 = vdupq_n_s16(downsample_shift);
  const int32x4_t df_s32 = vdupq_n_s32(downsample_shift);
  const int16x8_t mask = vld1q_s16(&mask_16bit[8] - (width % 8));

  // We use an intermediate matrix that will be transposed to get M.
  int64_t M_trn[49];
  memset(M_trn, 0, sizeof(M_trn));

  int h = v_start;
  do {
    // Cross-correlation (M).
    for (int row = 0; row < wiener_win; row++) {
      int16x8_t dgd0 = vld1q_s16(dgd_avg + row * dgd_avg_stride);
      int j = 0;
      while (j <= width - 8) {
        int16x8_t dgd1 = vld1q_s16(dgd_avg + row * dgd_avg_stride + j + 8);
        // Load src and scale based on downsampling factor.
        int16x8_t s = vshlq_s16(vld1q_s16(src_avg + j), df_s16);

        // Compute all the elements of one row of M.
        compute_M_one_row_win7(s, dgd0, dgd1, M_trn, wiener_win, row);

        dgd0 = dgd1;
        j += 8;
      }
      // Process remaining elements without Neon.
      while (j < width) {
        int16_t s = src_avg[j] * downsample_factor;
        int16_t d0 = dgd_avg[row * dgd_avg_stride + 0 + j];
        int16_t d1 = dgd_avg[row * dgd_avg_stride + 1 + j];
        int16_t d2 = dgd_avg[row * dgd_avg_stride + 2 + j];
        int16_t d3 = dgd_avg[row * dgd_avg_stride + 3 + j];
        int16_t d4 = dgd_avg[row * dgd_avg_stride + 4 + j];
        int16_t d5 = dgd_avg[row * dgd_avg_stride + 5 + j];
        int16_t d6 = dgd_avg[row * dgd_avg_stride + 6 + j];

        M_trn[row * wiener_win + 0] += d0 * s;
        M_trn[row * wiener_win + 1] += d1 * s;
        M_trn[row * wiener_win + 2] += d2 * s;
        M_trn[row * wiener_win + 3] += d3 * s;
        M_trn[row * wiener_win + 4] += d4 * s;
        M_trn[row * wiener_win + 5] += d5 * s;
        M_trn[row * wiener_win + 6] += d6 * s;

        j++;
      }
    }

    // Auto-covariance (H).
    int j = 0;
    while (j <= width - 8) {
      for (int col0 = 0; col0 < wiener_win; col0++) {
        int16x8_t dgd0[7];
        dgd0[0] = vld1q_s16(dgd_avg + 0 * dgd_avg_stride + j + col0);
        dgd0[1] = vld1q_s16(dgd_avg + 1 * dgd_avg_stride + j + col0);
        dgd0[2] = vld1q_s16(dgd_avg + 2 * dgd_avg_stride + j + col0);
        dgd0[3] = vld1q_s16(dgd_avg + 3 * dgd_avg_stride + j + col0);
        dgd0[4] = vld1q_s16(dgd_avg + 4 * dgd_avg_stride + j + col0);
        dgd0[5] = vld1q_s16(dgd_avg + 5 * dgd_avg_stride + j + col0);
        dgd0[6] = vld1q_s16(dgd_avg + 6 * dgd_avg_stride + j + col0);

        // Perform computation of the first column with itself (28 elements).
        // For the first column this will fill the upper triangle of the 7x7
        // matrix at the top left of the H matrix. For the next columns this
        // will fill the upper triangle of the other 7x7 matrices around H's
        // diagonal.
        compute_H_one_col(dgd0, col0, H, wiener_win, wiener_win2, df_s32);

        // All computation next to the matrix diagonal has already been done.
        for (int col1 = col0 + 1; col1 < wiener_win; col1++) {
          // Load second column and scale based on downsampling factor.
          int16x8_t dgd1[7];
          dgd1[0] = vld1q_s16(dgd_avg + 0 * dgd_avg_stride + j + col1);
          dgd1[0] = vshlq_s16(dgd1[0], df_s16);
          dgd1[1] = vld1q_s16(dgd_avg + 1 * dgd_avg_stride + j + col1);
          dgd1[1] = vshlq_s16(dgd1[1], df_s16);
          dgd1[2] = vld1q_s16(dgd_avg + 2 * dgd_avg_stride + j + col1);
          dgd1[2] = vshlq_s16(dgd1[2], df_s16);
          dgd1[3] = vld1q_s16(dgd_avg + 3 * dgd_avg_stride + j + col1);
          dgd1[3] = vshlq_s16(dgd1[3], df_s16);
          dgd1[4] = vld1q_s16(dgd_avg + 4 * dgd_avg_stride + j + col1);
          dgd1[4] = vshlq_s16(dgd1[4], df_s16);
          dgd1[5] = vld1q_s16(dgd_avg + 5 * dgd_avg_stride + j + col1);
          dgd1[5] = vshlq_s16(dgd1[5], df_s16);
          dgd1[6] = vld1q_s16(dgd_avg + 6 * dgd_avg_stride + j + col1);
          dgd1[6] = vshlq_s16(dgd1[6], df_s16);

          // Compute all elements from the combination of both columns (49
          // elements).
          compute_H_two_cols(dgd0, dgd1, col0, col1, H, wiener_win,
                             wiener_win2);
        }
      }
      j += 8;
    }

    if (j < width) {
      // Process remaining columns using a mask to discard excess elements.
      for (int col0 = 0; col0 < wiener_win; col0++) {
        // Load first column.
        int16x8_t dgd0[7];
        dgd0[0] = vld1q_s16(dgd_avg + 0 * dgd_avg_stride + j + col0);
        dgd0[0] = vandq_s16(dgd0[0], mask);
        dgd0[1] = vld1q_s16(dgd_avg + 1 * dgd_avg_stride + j + col0);
        dgd0[1] = vandq_s16(dgd0[1], mask);
        dgd0[2] = vld1q_s16(dgd_avg + 2 * dgd_avg_stride + j + col0);
        dgd0[2] = vandq_s16(dgd0[2], mask);
        dgd0[3] = vld1q_s16(dgd_avg + 3 * dgd_avg_stride + j + col0);
        dgd0[3] = vandq_s16(dgd0[3], mask);
        dgd0[4] = vld1q_s16(dgd_avg + 4 * dgd_avg_stride + j + col0);
        dgd0[4] = vandq_s16(dgd0[4], mask);
        dgd0[5] = vld1q_s16(dgd_avg + 5 * dgd_avg_stride + j + col0);
        dgd0[5] = vandq_s16(dgd0[5], mask);
        dgd0[6] = vld1q_s16(dgd_avg + 6 * dgd_avg_stride + j + col0);
        dgd0[6] = vandq_s16(dgd0[6], mask);

        // Perform computation of the first column with itself (28 elements).
        // For the first column this will fill the upper triangle of the 7x7
        // matrix at the top left of the H matrix. For the next columns this
        // will fill the upper triangle of the other 7x7 matrices around H's
        // diagonal.
        compute_H_one_col(dgd0, col0, H, wiener_win, wiener_win2, df_s32);

        // All computation next to the matrix diagonal has already been done.
        for (int col1 = col0 + 1; col1 < wiener_win; col1++) {
          // Load second column and scale based on downsampling factor.
          int16x8_t dgd1[7];
          dgd1[0] = vld1q_s16(dgd_avg + 0 * dgd_avg_stride + j + col1);
          dgd1[0] = vshlq_s16(dgd1[0], df_s16);
          dgd1[1] = vld1q_s16(dgd_avg + 1 * dgd_avg_stride + j + col1);
          dgd1[1] = vshlq_s16(dgd1[1], df_s16);
          dgd1[2] = vld1q_s16(dgd_avg + 2 * dgd_avg_stride + j + col1);
          dgd1[2] = vshlq_s16(dgd1[2], df_s16);
          dgd1[3] = vld1q_s16(dgd_avg + 3 * dgd_avg_stride + j + col1);
          dgd1[3] = vshlq_s16(dgd1[3], df_s16);
          dgd1[4] = vld1q_s16(dgd_avg + 4 * dgd_avg_stride + j + col1);
          dgd1[4] = vshlq_s16(dgd1[4], df_s16);
          dgd1[5] = vld1q_s16(dgd_avg + 5 * dgd_avg_stride + j + col1);
          dgd1[5] = vshlq_s16(dgd1[5], df_s16);
          dgd1[6] = vld1q_s16(dgd_avg + 6 * dgd_avg_stride + j + col1);
          dgd1[6] = vshlq_s16(dgd1[6], df_s16);

          // Compute all elements from the combination of both columns (49
          // elements).
          compute_H_two_cols(dgd0, dgd1, col0, col1, H, wiener_win,
                             wiener_win2);
        }
      }
    }
    dgd_avg += downsample_factor * dgd_avg_stride;
    src_avg += src_avg_stride;
    h += downsample_factor;
  } while (h <= v_end - downsample_factor);

  if (h < v_end) {
    // The last row is scaled by a different downsample factor, so process
    // separately.

    // Cross-correlation (M).
    for (int row = 0; row < 7; row++) {
      int16x8_t dgd0 = vld1q_s16(dgd_avg + row * dgd_avg_stride);
      int j = 0;
      while (j <= width - 8) {
        int16x8_t dgd1 = vld1q_s16(dgd_avg + row * dgd_avg_stride + j + 8);
        // Load src vector and scale based on downsampling factor.
        int16x8_t s =
            vmulq_n_s16(vld1q_s16(src_avg + j), last_row_downsample_factor);

        // Compute all the elements of one row of M.
        compute_M_one_row_win7(s, dgd0, dgd1, M_trn, wiener_win, row);

        dgd0 = dgd1;
        j += 8;
      }
      // Process remaining elements without Neon.
      while (j < width) {
        int16_t s = src_avg[j];
        int16_t d0 = dgd_avg[row * dgd_avg_stride + 0 + j];
        int16_t d1 = dgd_avg[row * dgd_avg_stride + 1 + j];
        int16_t d2 = dgd_avg[row * dgd_avg_stride + 2 + j];
        int16_t d3 = dgd_avg[row * dgd_avg_stride + 3 + j];
        int16_t d4 = dgd_avg[row * dgd_avg_stride + 4 + j];
        int16_t d5 = dgd_avg[row * dgd_avg_stride + 5 + j];
        int16_t d6 = dgd_avg[row * dgd_avg_stride + 6 + j];

        M_trn[row * wiener_win + 0] += d0 * s * last_row_downsample_factor;
        M_trn[row * wiener_win + 1] += d1 * s * last_row_downsample_factor;
        M_trn[row * wiener_win + 2] += d2 * s * last_row_downsample_factor;
        M_trn[row * wiener_win + 3] += d3 * s * last_row_downsample_factor;
        M_trn[row * wiener_win + 4] += d4 * s * last_row_downsample_factor;
        M_trn[row * wiener_win + 5] += d5 * s * last_row_downsample_factor;
        M_trn[row * wiener_win + 6] += d6 * s * last_row_downsample_factor;

        j++;
      }
    }

    // Auto-covariance (H).
    int j = 0;
    while (j <= width - 8) {
      int col0 = 0;
      do {
        // Load first column.
        int16x8_t dgd0[7];
        dgd0[0] = vld1q_s16(dgd_avg + 0 * dgd_avg_stride + j + col0);
        dgd0[1] = vld1q_s16(dgd_avg + 1 * dgd_avg_stride + j + col0);
        dgd0[2] = vld1q_s16(dgd_avg + 2 * dgd_avg_stride + j + col0);
        dgd0[3] = vld1q_s16(dgd_avg + 3 * dgd_avg_stride + j + col0);
        dgd0[4] = vld1q_s16(dgd_avg + 4 * dgd_avg_stride + j + col0);
        dgd0[5] = vld1q_s16(dgd_avg + 5 * dgd_avg_stride + j + col0);
        dgd0[6] = vld1q_s16(dgd_avg + 6 * dgd_avg_stride + j + col0);

        // Perform computation of the first column with itself (28 elements).
        // For the first column this will fill the upper triangle of the 7x7
        // matrix at the top left of the H matrix. For the next columns this
        // will fill the upper triangle of the other 7x7 matrices around H's
        // diagonal.
        compute_H_one_col_last_row(dgd0, col0, H, wiener_win, wiener_win2,
                                   last_row_downsample_factor);

        // All computation next to the matrix diagonal has already been done.
        for (int col1 = col0 + 1; col1 < wiener_win; col1++) {
          // Load second column and scale based on downsampling factor.
          int16x8_t dgd1[7];
          dgd1[0] = vld1q_s16(dgd_avg + 0 * dgd_avg_stride + j + col1);
          dgd1[0] = vmulq_n_s16(dgd1[0], last_row_downsample_factor);
          dgd1[1] = vld1q_s16(dgd_avg + 1 * dgd_avg_stride + j + col1);
          dgd1[1] = vmulq_n_s16(dgd1[1], last_row_downsample_factor);
          dgd1[2] = vld1q_s16(dgd_avg + 2 * dgd_avg_stride + j + col1);
          dgd1[2] = vmulq_n_s16(dgd1[2], last_row_downsample_factor);
          dgd1[3] = vld1q_s16(dgd_avg + 3 * dgd_avg_stride + j + col1);
          dgd1[3] = vmulq_n_s16(dgd1[3], last_row_downsample_factor);
          dgd1[4] = vld1q_s16(dgd_avg + 4 * dgd_avg_stride + j + col1);
          dgd1[4] = vmulq_n_s16(dgd1[4], last_row_downsample_factor);
          dgd1[5] = vld1q_s16(dgd_avg + 5 * dgd_avg_stride + j + col1);
          dgd1[5] = vmulq_n_s16(dgd1[5], last_row_downsample_factor);
          dgd1[6] = vld1q_s16(dgd_avg + 6 * dgd_avg_stride + j + col1);
          dgd1[6] = vmulq_n_s16(dgd1[6], last_row_downsample_factor);

          // Compute all elements from the combination of both columns (49
          // elements).
          compute_H_two_cols(dgd0, dgd1, col0, col1, H, wiener_win,
                             wiener_win2);
        }
      } while (++col0 < wiener_win);
      j += 8;
    }

    // Process remaining columns using a mask to discard excess elements.
    if (j < width) {
      int col0 = 0;
      do {
        // Load first column.
        int16x8_t dgd0[7];
        dgd0[0] = vld1q_s16(dgd_avg + 0 * dgd_avg_stride + j + col0);
        dgd0[0] = vandq_s16(dgd0[0], mask);
        dgd0[1] = vld1q_s16(dgd_avg + 1 * dgd_avg_stride + j + col0);
        dgd0[1] = vandq_s16(dgd0[1], mask);
        dgd0[2] = vld1q_s16(dgd_avg + 2 * dgd_avg_stride + j + col0);
        dgd0[2] = vandq_s16(dgd0[2], mask);
        dgd0[3] = vld1q_s16(dgd_avg + 3 * dgd_avg_stride + j + col0);
        dgd0[3] = vandq_s16(dgd0[3], mask);
        dgd0[4] = vld1q_s16(dgd_avg + 4 * dgd_avg_stride + j + col0);
        dgd0[4] = vandq_s16(dgd0[4], mask);
        dgd0[5] = vld1q_s16(dgd_avg + 5 * dgd_avg_stride + j + col0);
        dgd0[5] = vandq_s16(dgd0[5], mask);
        dgd0[6] = vld1q_s16(dgd_avg + 6 * dgd_avg_stride + j + col0);
        dgd0[6] = vandq_s16(dgd0[6], mask);

        // Perform computation of the first column with itself (15 elements).
        // For the first column this will fill the upper triangle of the 7x7
        // matrix at the top left of the H matrix. For the next columns this
        // will fill the upper triangle of the other 7x7 matrices around H's
        // diagonal.
        compute_H_one_col_last_row(dgd0, col0, H, wiener_win, wiener_win2,
                                   last_row_downsample_factor);

        // All computation next to the matrix diagonal has already been done.
        for (int col1 = col0 + 1; col1 < wiener_win; col1++) {
          // Load second column and scale based on downsampling factor.
          int16x8_t dgd1[7];
          dgd1[0] = vld1q_s16(dgd_avg + 0 * dgd_avg_stride + j + col1);
          dgd1[0] = vmulq_n_s16(dgd1[0], last_row_downsample_factor);
          dgd1[1] = vld1q_s16(dgd_avg + 1 * dgd_avg_stride + j + col1);
          dgd1[1] = vmulq_n_s16(dgd1[1], last_row_downsample_factor);
          dgd1[2] = vld1q_s16(dgd_avg + 2 * dgd_avg_stride + j + col1);
          dgd1[2] = vmulq_n_s16(dgd1[2], last_row_downsample_factor);
          dgd1[3] = vld1q_s16(dgd_avg + 3 * dgd_avg_stride + j + col1);
          dgd1[3] = vmulq_n_s16(dgd1[3], last_row_downsample_factor);
          dgd1[4] = vld1q_s16(dgd_avg + 4 * dgd_avg_stride + j + col1);
          dgd1[4] = vmulq_n_s16(dgd1[4], last_row_downsample_factor);
          dgd1[5] = vld1q_s16(dgd_avg + 5 * dgd_avg_stride + j + col1);
          dgd1[5] = vmulq_n_s16(dgd1[5], last_row_downsample_factor);
          dgd1[6] = vld1q_s16(dgd_avg + 6 * dgd_avg_stride + j + col1);
          dgd1[6] = vmulq_n_s16(dgd1[6], last_row_downsample_factor);

          // Compute all elements from the combination of both columns (49
          // elements).
          compute_H_two_cols(dgd0, dgd1, col0, col1, H, wiener_win,
                             wiener_win2);
        }
      } while (++col0 < wiener_win);
    }
  }

  // Transpose M_trn.
  transpose_M_win7(M, M_trn, 7);

  // Copy upper triangle of H in the lower one.
  copy_upper_triangle(H, wiener_win2);
}

static INLINE void transpose_M_win5(int64_t *M, int64_t *M_trn,
                                    const int wiener_win) {
  // 1st and 2nd rows.
  int64x2_t row00 = vld1q_s64(M_trn);
  int64x2_t row10 = vld1q_s64(M_trn + wiener_win);
  vst1q_s64(M, aom_vtrn1q_s64(row00, row10));
  vst1q_s64(M + wiener_win, aom_vtrn2q_s64(row00, row10));

  int64x2_t row02 = vld1q_s64(M_trn + 2);
  int64x2_t row12 = vld1q_s64(M_trn + wiener_win + 2);
  vst1q_s64(M + 2 * wiener_win, aom_vtrn1q_s64(row02, row12));
  vst1q_s64(M + 3 * wiener_win, aom_vtrn2q_s64(row02, row12));

  // Last column only needs trn2.
  int64x2_t row03 = vld1q_s64(M_trn + 3);
  int64x2_t row13 = vld1q_s64(M_trn + wiener_win + 3);
  vst1q_s64(M + 4 * wiener_win, aom_vtrn2q_s64(row03, row13));

  // 3rd and 4th rows.
  int64x2_t row20 = vld1q_s64(M_trn + 2 * wiener_win);
  int64x2_t row30 = vld1q_s64(M_trn + 3 * wiener_win);
  vst1q_s64(M + 2, aom_vtrn1q_s64(row20, row30));
  vst1q_s64(M + wiener_win + 2, aom_vtrn2q_s64(row20, row30));

  int64x2_t row22 = vld1q_s64(M_trn + 2 * wiener_win + 2);
  int64x2_t row32 = vld1q_s64(M_trn + 3 * wiener_win + 2);
  vst1q_s64(M + 2 * wiener_win + 2, aom_vtrn1q_s64(row22, row32));
  vst1q_s64(M + 3 * wiener_win + 2, aom_vtrn2q_s64(row22, row32));

  // Last column only needs trn2.
  int64x2_t row23 = vld1q_s64(M_trn + 2 * wiener_win + 3);
  int64x2_t row33 = vld1q_s64(M_trn + 3 * wiener_win + 3);
  vst1q_s64(M + 4 * wiener_win + 2, aom_vtrn2q_s64(row23, row33));

  // Last row.
  int64x2_t row40 = vld1q_s64(M_trn + 4 * wiener_win);
  vst1_s64(M + 4, vget_low_s64(row40));
  vst1_s64(M + 1 * wiener_win + 4, vget_high_s64(row40));

  int64x2_t row42 = vld1q_s64(M_trn + 4 * wiener_win + 2);
  vst1_s64(M + 2 * wiener_win + 4, vget_low_s64(row42));
  vst1_s64(M + 3 * wiener_win + 4, vget_high_s64(row42));

  // Element on the bottom right of M_trn is copied as is.
  vst1_s64(M + 4 * wiener_win + 4, vld1_s64(M_trn + 4 * wiener_win + 4));
}

static INLINE void compute_M_one_row_win5(int16x8_t src, int16x8_t dgd0,
                                          int16x8_t dgd1, int64_t *M,
                                          const int wiener_win, int row) {
  int64x2_t m_01 = vld1q_s64(M + row * wiener_win + 0);
  int64x2_t m_23 = vld1q_s64(M + row * wiener_win + 2);

  int32x4_t m0 = vmull_s16(vget_low_s16(src), vget_low_s16(dgd0));
  m0 = vmlal_s16(m0, vget_high_s16(src), vget_high_s16(dgd0));

  int16x8_t dgd01 = vextq_s16(dgd0, dgd1, 1);
  int32x4_t m1 = vmull_s16(vget_low_s16(src), vget_low_s16(dgd01));
  m1 = vmlal_s16(m1, vget_high_s16(src), vget_high_s16(dgd01));

  m0 = horizontal_add_2d_s32(m0, m1);
  m_01 = vpadalq_s32(m_01, m0);
  vst1q_s64(M + row * wiener_win + 0, m_01);

  int16x8_t dgd02 = vextq_s16(dgd0, dgd1, 2);
  int32x4_t m2 = vmull_s16(vget_low_s16(src), vget_low_s16(dgd02));
  m2 = vmlal_s16(m2, vget_high_s16(src), vget_high_s16(dgd02));

  int16x8_t dgd03 = vextq_s16(dgd0, dgd1, 3);
  int32x4_t m3 = vmull_s16(vget_low_s16(src), vget_low_s16(dgd03));
  m3 = vmlal_s16(m3, vget_high_s16(src), vget_high_s16(dgd03));

  m2 = horizontal_add_2d_s32(m2, m3);
  m_23 = vpadalq_s32(m_23, m2);
  vst1q_s64(M + row * wiener_win + 2, m_23);

  int16x8_t dgd04 = vextq_s16(dgd0, dgd1, 4);
  int32x4_t m4 = vmull_s16(vget_low_s16(src), vget_low_s16(dgd04));
  m4 = vmlal_s16(m4, vget_high_s16(src), vget_high_s16(dgd04));
  M[row * wiener_win + 4] += horizontal_long_add_s32x4(m4);
}

// This function computes two matrices: the cross-correlation between the src
// buffer and dgd buffer (M), and the auto-covariance of the dgd buffer (H).
//
// M is of size 5 * 5. It needs to be filled such that multiplying one element
// from src with each element of a row of the wiener window will fill one
// column of M. However this is not very convenient in terms of memory
// accesses, as it means we do contiguous loads of dgd but strided stores to M.
// As a result, we use an intermediate matrix M_trn which is instead filled
// such that one row of the wiener window gives one row of M_trn. Once fully
// computed, M_trn is then transposed to return M.
//
// H is of size 25 * 25. It is filled by multiplying every pair of elements of
// the wiener window together. Since it is a symmetric matrix, we only compute
// the upper triangle, and then copy it down to the lower one. Here we fill it
// by taking each different pair of columns, and multiplying all the elements of
// the first one with all the elements of the second one, with a special case
// when multiplying a column by itself.
static INLINE void compute_stats_win5_neon(int16_t *dgd_avg, int dgd_avg_stride,
                                           int16_t *src_avg, int src_avg_stride,
                                           int width, int v_start, int v_end,
                                           int64_t *M, int64_t *H,
                                           int downsample_factor,
                                           int last_row_downsample_factor) {
  const int wiener_win = 5;
  const int wiener_win2 = wiener_win * wiener_win;
  // The downsample factor can be either 1 or 4, so instead of multiplying the
  // values by 1 or 4, we can left shift by 0 or 2 respectively, which is
  // faster. (This doesn't apply to the last row where we can scale the values
  // by 1, 2 or 3, so we keep the multiplication).
  const int downsample_shift = downsample_factor >> 1;
  const int16x8_t df_s16 = vdupq_n_s16(downsample_shift);
  const int32x4_t df_s32 = vdupq_n_s32(downsample_shift);
  const int16x8_t mask = vld1q_s16(&mask_16bit[8] - (width % 8));

  // We use an intermediate matrix that will be transposed to get M.
  int64_t M_trn[25];
  memset(M_trn, 0, sizeof(M_trn));

  int h = v_start;
  do {
    // Cross-correlation (M).
    for (int row = 0; row < wiener_win; row++) {
      int16x8_t dgd0 = vld1q_s16(dgd_avg + row * dgd_avg_stride);
      int j = 0;
      while (j <= width - 8) {
        int16x8_t dgd1 = vld1q_s16(dgd_avg + row * dgd_avg_stride + j + 8);
        // Load src vector and scale based on downsampling factor.
        int16x8_t s = vshlq_s16(vld1q_s16(src_avg + j), df_s16);

        // Compute all the elements of one row of M.
        compute_M_one_row_win5(s, dgd0, dgd1, M_trn, wiener_win, row);

        dgd0 = dgd1;
        j += 8;
      }

      // Process remaining elements without Neon.
      while (j < width) {
        int16_t s = src_avg[j];
        int16_t d0 = dgd_avg[row * dgd_avg_stride + 0 + j];
        int16_t d1 = dgd_avg[row * dgd_avg_stride + 1 + j];
        int16_t d2 = dgd_avg[row * dgd_avg_stride + 2 + j];
        int16_t d3 = dgd_avg[row * dgd_avg_stride + 3 + j];
        int16_t d4 = dgd_avg[row * dgd_avg_stride + 4 + j];

        M_trn[row * wiener_win + 0] += d0 * s * downsample_factor;
        M_trn[row * wiener_win + 1] += d1 * s * downsample_factor;
        M_trn[row * wiener_win + 2] += d2 * s * downsample_factor;
        M_trn[row * wiener_win + 3] += d3 * s * downsample_factor;
        M_trn[row * wiener_win + 4] += d4 * s * downsample_factor;

        j++;
      }
    }

    // Auto-covariance (H).
    int j = 0;
    while (j <= width - 8) {
      for (int col0 = 0; col0 < wiener_win; col0++) {
        // Load first column.
        int16x8_t dgd0[5];
        dgd0[0] = vld1q_s16(dgd_avg + 0 * dgd_avg_stride + j + col0);
        dgd0[1] = vld1q_s16(dgd_avg + 1 * dgd_avg_stride + j + col0);
        dgd0[2] = vld1q_s16(dgd_avg + 2 * dgd_avg_stride + j + col0);
        dgd0[3] = vld1q_s16(dgd_avg + 3 * dgd_avg_stride + j + col0);
        dgd0[4] = vld1q_s16(dgd_avg + 4 * dgd_avg_stride + j + col0);

        // Perform computation of the first column with itself (15 elements).
        // For the first column this will fill the upper triangle of the 5x5
        // matrix at the top left of the H matrix. For the next columns this
        // will fill the upper triangle of the other 5x5 matrices around H's
        // diagonal.
        compute_H_one_col(dgd0, col0, H, wiener_win, wiener_win2, df_s32);

        // All computation next to the matrix diagonal has already been done.
        for (int col1 = col0 + 1; col1 < wiener_win; col1++) {
          // Load second column and scale based on downsampling factor.
          int16x8_t dgd1[5];
          dgd1[0] = vld1q_s16(dgd_avg + 0 * dgd_avg_stride + j + col1);
          dgd1[0] = vshlq_s16(dgd1[0], df_s16);
          dgd1[1] = vld1q_s16(dgd_avg + 1 * dgd_avg_stride + j + col1);
          dgd1[1] = vshlq_s16(dgd1[1], df_s16);
          dgd1[2] = vld1q_s16(dgd_avg + 2 * dgd_avg_stride + j + col1);
          dgd1[2] = vshlq_s16(dgd1[2], df_s16);
          dgd1[3] = vld1q_s16(dgd_avg + 3 * dgd_avg_stride + j + col1);
          dgd1[3] = vshlq_s16(dgd1[3], df_s16);
          dgd1[4] = vld1q_s16(dgd_avg + 4 * dgd_avg_stride + j + col1);
          dgd1[4] = vshlq_s16(dgd1[4], df_s16);

          // Compute all elements from the combination of both columns (25
          // elements).
          compute_H_two_cols(dgd0, dgd1, col0, col1, H, wiener_win,
                             wiener_win2);
        }
      }
      j += 8;
    }

    // Process remaining columns using a mask to discard excess elements.
    if (j < width) {
      for (int col0 = 0; col0 < wiener_win; col0++) {
        // Load first column.
        int16x8_t dgd0[5];
        dgd0[0] = vld1q_s16(dgd_avg + 0 * dgd_avg_stride + j + col0);
        dgd0[0] = vandq_s16(dgd0[0], mask);
        dgd0[1] = vld1q_s16(dgd_avg + 1 * dgd_avg_stride + j + col0);
        dgd0[1] = vandq_s16(dgd0[1], mask);
        dgd0[2] = vld1q_s16(dgd_avg + 2 * dgd_avg_stride + j + col0);
        dgd0[2] = vandq_s16(dgd0[2], mask);
        dgd0[3] = vld1q_s16(dgd_avg + 3 * dgd_avg_stride + j + col0);
        dgd0[3] = vandq_s16(dgd0[3], mask);
        dgd0[4] = vld1q_s16(dgd_avg + 4 * dgd_avg_stride + j + col0);
        dgd0[4] = vandq_s16(dgd0[4], mask);

        // Perform computation of the first column with itself (15 elements).
        // For the first column this will fill the upper triangle of the 5x5
        // matrix at the top left of the H matrix. For the next columns this
        // will fill the upper triangle of the other 5x5 matrices around H's
        // diagonal.
        compute_H_one_col(dgd0, col0, H, wiener_win, wiener_win2, df_s32);

        // All computation next to the matrix diagonal has already been done.
        for (int col1 = col0 + 1; col1 < wiener_win; col1++) {
          // Load second column and scale based on downsampling factor.
          int16x8_t dgd1[5];
          dgd1[0] = vld1q_s16(dgd_avg + 0 * dgd_avg_stride + j + col1);
          dgd1[0] = vshlq_s16(dgd1[0], df_s16);
          dgd1[1] = vld1q_s16(dgd_avg + 1 * dgd_avg_stride + j + col1);
          dgd1[1] = vshlq_s16(dgd1[1], df_s16);
          dgd1[2] = vld1q_s16(dgd_avg + 2 * dgd_avg_stride + j + col1);
          dgd1[2] = vshlq_s16(dgd1[2], df_s16);
          dgd1[3] = vld1q_s16(dgd_avg + 3 * dgd_avg_stride + j + col1);
          dgd1[3] = vshlq_s16(dgd1[3], df_s16);
          dgd1[4] = vld1q_s16(dgd_avg + 4 * dgd_avg_stride + j + col1);
          dgd1[4] = vshlq_s16(dgd1[4], df_s16);

          // Compute all elements from the combination of both columns (25
          // elements).
          compute_H_two_cols(dgd0, dgd1, col0, col1, H, wiener_win,
                             wiener_win2);
        }
      }
    }
    dgd_avg += downsample_factor * dgd_avg_stride;
    src_avg += src_avg_stride;
    h += downsample_factor;
  } while (h <= v_end - downsample_factor);

  if (h < v_end) {
    // The last row is scaled by a different downsample factor, so process
    // separately.

    // Cross-correlation (M).
    for (int row = 0; row < wiener_win; row++) {
      int16x8_t dgd0 = vld1q_s16(dgd_avg + row * dgd_avg_stride);
      int j = 0;
      while (j <= width - 8) {
        int16x8_t dgd1 = vld1q_s16(dgd_avg + row * dgd_avg_stride + j + 8);
        // Load src vector and scale based on downsampling factor.
        int16x8_t s =
            vmulq_n_s16(vld1q_s16(src_avg + j), last_row_downsample_factor);

        // Compute all the elements of one row of M.
        compute_M_one_row_win5(s, dgd0, dgd1, M_trn, wiener_win, row);

        dgd0 = dgd1;
        j += 8;
      }

      // Process remaining elements without Neon.
      while (j < width) {
        int16_t s = src_avg[j];
        int16_t d0 = dgd_avg[row * dgd_avg_stride + 0 + j];
        int16_t d1 = dgd_avg[row * dgd_avg_stride + 1 + j];
        int16_t d2 = dgd_avg[row * dgd_avg_stride + 2 + j];
        int16_t d3 = dgd_avg[row * dgd_avg_stride + 3 + j];
        int16_t d4 = dgd_avg[row * dgd_avg_stride + 4 + j];

        M_trn[row * wiener_win + 0] += d0 * s * last_row_downsample_factor;
        M_trn[row * wiener_win + 1] += d1 * s * last_row_downsample_factor;
        M_trn[row * wiener_win + 2] += d2 * s * last_row_downsample_factor;
        M_trn[row * wiener_win + 3] += d3 * s * last_row_downsample_factor;
        M_trn[row * wiener_win + 4] += d4 * s * last_row_downsample_factor;

        j++;
      }
    }

    // Auto-covariance (H).
    int j = 0;
    while (j <= width - 8) {
      for (int col0 = 0; col0 < wiener_win; col0++) {
        // Load first column.
        int16x8_t dgd0[5];
        dgd0[0] = vld1q_s16(dgd_avg + 0 * dgd_avg_stride + j + col0);
        dgd0[1] = vld1q_s16(dgd_avg + 1 * dgd_avg_stride + j + col0);
        dgd0[2] = vld1q_s16(dgd_avg + 2 * dgd_avg_stride + j + col0);
        dgd0[3] = vld1q_s16(dgd_avg + 3 * dgd_avg_stride + j + col0);
        dgd0[4] = vld1q_s16(dgd_avg + 4 * dgd_avg_stride + j + col0);

        // Perform computation of the first column with itself (15 elements).
        // For the first column this will fill the upper triangle of the 5x5
        // matrix at the top left of the H matrix. For the next columns this
        // will fill the upper triangle of the other 5x5 matrices around H's
        // diagonal.
        compute_H_one_col_last_row(dgd0, col0, H, wiener_win, wiener_win2,
                                   last_row_downsample_factor);

        // All computation next to the matrix diagonal has already been done.
        for (int col1 = col0 + 1; col1 < wiener_win; col1++) {
          // Load second column and scale based on downsampling factor.
          int16x8_t dgd1[5];
          dgd1[0] = vld1q_s16(dgd_avg + 0 * dgd_avg_stride + j + col1);
          dgd1[0] = vmulq_n_s16(dgd1[0], last_row_downsample_factor);
          dgd1[1] = vld1q_s16(dgd_avg + 1 * dgd_avg_stride + j + col1);
          dgd1[1] = vmulq_n_s16(dgd1[1], last_row_downsample_factor);
          dgd1[2] = vld1q_s16(dgd_avg + 2 * dgd_avg_stride + j + col1);
          dgd1[2] = vmulq_n_s16(dgd1[2], last_row_downsample_factor);
          dgd1[3] = vld1q_s16(dgd_avg + 3 * dgd_avg_stride + j + col1);
          dgd1[3] = vmulq_n_s16(dgd1[3], last_row_downsample_factor);
          dgd1[4] = vld1q_s16(dgd_avg + 4 * dgd_avg_stride + j + col1);
          dgd1[4] = vmulq_n_s16(dgd1[4], last_row_downsample_factor);

          // Compute all elements from the combination of both columns (25
          // elements).
          compute_H_two_cols(dgd0, dgd1, col0, col1, H, wiener_win,
                             wiener_win2);
        }
      }
      j += 8;
    }

    // Process remaining columns using a mask to discard excess elements.
    if (j < width) {
      for (int col0 = 0; col0 < wiener_win; col0++) {
        // Load first column.
        int16x8_t dgd0[5];
        dgd0[0] = vld1q_s16(dgd_avg + 0 * dgd_avg_stride + j + col0);
        dgd0[0] = vandq_s16(dgd0[0], mask);
        dgd0[1] = vld1q_s16(dgd_avg + 1 * dgd_avg_stride + j + col0);
        dgd0[1] = vandq_s16(dgd0[1], mask);
        dgd0[2] = vld1q_s16(dgd_avg + 2 * dgd_avg_stride + j + col0);
        dgd0[2] = vandq_s16(dgd0[2], mask);
        dgd0[3] = vld1q_s16(dgd_avg + 3 * dgd_avg_stride + j + col0);
        dgd0[3] = vandq_s16(dgd0[3], mask);
        dgd0[4] = vld1q_s16(dgd_avg + 4 * dgd_avg_stride + j + col0);
        dgd0[4] = vandq_s16(dgd0[4], mask);

        // Perform computation of the first column with itself (15 elements).
        // For the first column this will fill the upper triangle of the 5x5
        // matrix at the top left of the H matrix. For the next columns this
        // will fill the upper triangle of the other 5x5 matrices around H's
        // diagonal.
        compute_H_one_col_last_row(dgd0, col0, H, wiener_win, wiener_win2,
                                   last_row_downsample_factor);

        // All computation next to the matrix diagonal has already been done.
        for (int col1 = col0 + 1; col1 < wiener_win; col1++) {
          // Load second column and scale based on downsampling factor.
          int16x8_t dgd1[5];
          dgd1[0] = vld1q_s16(dgd_avg + 0 * dgd_avg_stride + j + col1);
          dgd1[0] = vmulq_n_s16(dgd1[0], last_row_downsample_factor);
          dgd1[1] = vld1q_s16(dgd_avg + 1 * dgd_avg_stride + j + col1);
          dgd1[1] = vmulq_n_s16(dgd1[1], last_row_downsample_factor);
          dgd1[2] = vld1q_s16(dgd_avg + 2 * dgd_avg_stride + j + col1);
          dgd1[2] = vmulq_n_s16(dgd1[2], last_row_downsample_factor);
          dgd1[3] = vld1q_s16(dgd_avg + 3 * dgd_avg_stride + j + col1);
          dgd1[3] = vmulq_n_s16(dgd1[3], last_row_downsample_factor);
          dgd1[4] = vld1q_s16(dgd_avg + 4 * dgd_avg_stride + j + col1);
          dgd1[4] = vmulq_n_s16(dgd1[4], last_row_downsample_factor);

          // Compute all elements from the combination of both columns (25
          // elements).
          compute_H_two_cols(dgd0, dgd1, col0, col1, H, wiener_win,
                             wiener_win2);
        }
      }
    }
  }

  // Transpose M_trn.
  transpose_M_win5(M, M_trn, 5);

  // Copy upper triangle of H in the lower one.
  copy_upper_triangle(H, wiener_win2);
}

void av1_compute_stats_neon(int wiener_win, const uint8_t *dgd,
                            const uint8_t *src, int16_t *dgd_avg,
                            int16_t *src_avg, int h_start, int h_end,
                            int v_start, int v_end, int dgd_stride,
                            int src_stride, int64_t *M, int64_t *H,
                            int use_downsampled_wiener_stats) {
  assert(wiener_win == WIENER_WIN || wiener_win == WIENER_WIN_CHROMA);

  const int wiener_win2 = wiener_win * wiener_win;
  const int wiener_halfwin = wiener_win >> 1;
  const int32_t width = h_end - h_start;
  const int32_t height = v_end - v_start;
  const uint8_t *dgd_start = &dgd[v_start * dgd_stride + h_start];
  memset(H, 0, sizeof(*H) * wiener_win2 * wiener_win2);

  uint8_t avg = find_average_neon(dgd_start, dgd_stride, width, height);
  assert(WIENER_STATS_DOWNSAMPLE_FACTOR == 4);
  int downsample_factor =
      use_downsampled_wiener_stats ? WIENER_STATS_DOWNSAMPLE_FACTOR : 1;

  int dgd_avg_stride = width + 2 * wiener_halfwin;
  int src_avg_stride = width;

  // Compute (dgd - avg) and store it in dgd_avg.
  // The wiener window will slide along the dgd frame, centered on each pixel.
  // For the top left pixel and all the pixels on the side of the frame this
  // means half of the window will be outside of the frame. As such the actual
  // buffer that we need to subtract the avg from will be 2 * wiener_halfwin
  // wider and 2 * wiener_halfwin higher than the original dgd buffer.
  const int vert_offset = v_start - wiener_halfwin;
  const int horiz_offset = h_start - wiener_halfwin;
  const uint8_t *dgd_win = dgd + horiz_offset + vert_offset * dgd_stride;
  compute_sub_avg(dgd_win, dgd_stride, avg, dgd_avg, dgd_avg_stride,
                  width + 2 * wiener_halfwin, height + 2 * wiener_halfwin, 1);

  // Compute (src - avg), downsample if necessary and store in src-avg.
  const uint8_t *src_start = src + h_start + v_start * src_stride;
  compute_sub_avg(src_start, src_stride * downsample_factor, avg, src_avg,
                  src_avg_stride, width, height, downsample_factor);

  // Since the height is not necessarily a multiple of the downsample factor,
  // the last line of src will be scaled according to how many rows remain.
  int last_row_downsample_factor =
      use_downsampled_wiener_stats ? height % downsample_factor : 1;

  if (wiener_win == WIENER_WIN) {
    compute_stats_win7_neon(dgd_avg, dgd_avg_stride, src_avg, src_avg_stride,
                            width, v_start, v_end, M, H, downsample_factor,
                            last_row_downsample_factor);
  } else {
    compute_stats_win5_neon(dgd_avg, dgd_avg_stride, src_avg, src_avg_stride,
                            width, v_start, v_end, M, H, downsample_factor,
                            last_row_downsample_factor);
  }
}

static INLINE void calc_proj_params_r0_r1_neon(
    const uint8_t *src8, int width, int height, int src_stride,
    const uint8_t *dat8, int dat_stride, int32_t *flt0, int flt0_stride,
    int32_t *flt1, int flt1_stride, int64_t H[2][2], int64_t C[2]) {
  assert(width % 8 == 0);
  const int size = width * height;

  int64x2_t h00_lo = vdupq_n_s64(0);
  int64x2_t h00_hi = vdupq_n_s64(0);
  int64x2_t h11_lo = vdupq_n_s64(0);
  int64x2_t h11_hi = vdupq_n_s64(0);
  int64x2_t h01_lo = vdupq_n_s64(0);
  int64x2_t h01_hi = vdupq_n_s64(0);
  int64x2_t c0_lo = vdupq_n_s64(0);
  int64x2_t c0_hi = vdupq_n_s64(0);
  int64x2_t c1_lo = vdupq_n_s64(0);
  int64x2_t c1_hi = vdupq_n_s64(0);

  do {
    const uint8_t *src_ptr = src8;
    const uint8_t *dat_ptr = dat8;
    int32_t *flt0_ptr = flt0;
    int32_t *flt1_ptr = flt1;
    int w = width;

    do {
      uint8x8_t s = vld1_u8(src_ptr);
      uint8x8_t d = vld1_u8(dat_ptr);
      int32x4_t f0_lo = vld1q_s32(flt0_ptr);
      int32x4_t f0_hi = vld1q_s32(flt0_ptr + 4);
      int32x4_t f1_lo = vld1q_s32(flt1_ptr);
      int32x4_t f1_hi = vld1q_s32(flt1_ptr + 4);

      int16x8_t u = vreinterpretq_s16_u16(vshll_n_u8(d, SGRPROJ_RST_BITS));
      int16x8_t s_s16 = vreinterpretq_s16_u16(vshll_n_u8(s, SGRPROJ_RST_BITS));

      int32x4_t s_lo = vsubl_s16(vget_low_s16(s_s16), vget_low_s16(u));
      int32x4_t s_hi = vsubl_s16(vget_high_s16(s_s16), vget_high_s16(u));
      f0_lo = vsubw_s16(f0_lo, vget_low_s16(u));
      f0_hi = vsubw_s16(f0_hi, vget_high_s16(u));
      f1_lo = vsubw_s16(f1_lo, vget_low_s16(u));
      f1_hi = vsubw_s16(f1_hi, vget_high_s16(u));

      h00_lo = vmlal_s32(h00_lo, vget_low_s32(f0_lo), vget_low_s32(f0_lo));
      h00_lo = vmlal_s32(h00_lo, vget_high_s32(f0_lo), vget_high_s32(f0_lo));
      h00_hi = vmlal_s32(h00_hi, vget_low_s32(f0_hi), vget_low_s32(f0_hi));
      h00_hi = vmlal_s32(h00_hi, vget_high_s32(f0_hi), vget_high_s32(f0_hi));

      h11_lo = vmlal_s32(h11_lo, vget_low_s32(f1_lo), vget_low_s32(f1_lo));
      h11_lo = vmlal_s32(h11_lo, vget_high_s32(f1_lo), vget_high_s32(f1_lo));
      h11_hi = vmlal_s32(h11_hi, vget_low_s32(f1_hi), vget_low_s32(f1_hi));
      h11_hi = vmlal_s32(h11_hi, vget_high_s32(f1_hi), vget_high_s32(f1_hi));

      h01_lo = vmlal_s32(h01_lo, vget_low_s32(f0_lo), vget_low_s32(f1_lo));
      h01_lo = vmlal_s32(h01_lo, vget_high_s32(f0_lo), vget_high_s32(f1_lo));
      h01_hi = vmlal_s32(h01_hi, vget_low_s32(f0_hi), vget_low_s32(f1_hi));
      h01_hi = vmlal_s32(h01_hi, vget_high_s32(f0_hi), vget_high_s32(f1_hi));

      c0_lo = vmlal_s32(c0_lo, vget_low_s32(f0_lo), vget_low_s32(s_lo));
      c0_lo = vmlal_s32(c0_lo, vget_high_s32(f0_lo), vget_high_s32(s_lo));
      c0_hi = vmlal_s32(c0_hi, vget_low_s32(f0_hi), vget_low_s32(s_hi));
      c0_hi = vmlal_s32(c0_hi, vget_high_s32(f0_hi), vget_high_s32(s_hi));

      c1_lo = vmlal_s32(c1_lo, vget_low_s32(f1_lo), vget_low_s32(s_lo));
      c1_lo = vmlal_s32(c1_lo, vget_high_s32(f1_lo), vget_high_s32(s_lo));
      c1_hi = vmlal_s32(c1_hi, vget_low_s32(f1_hi), vget_low_s32(s_hi));
      c1_hi = vmlal_s32(c1_hi, vget_high_s32(f1_hi), vget_high_s32(s_hi));

      src_ptr += 8;
      dat_ptr += 8;
      flt0_ptr += 8;
      flt1_ptr += 8;
      w -= 8;
    } while (w != 0);

    src8 += src_stride;
    dat8 += dat_stride;
    flt0 += flt0_stride;
    flt1 += flt1_stride;
  } while (--height != 0);

  H[0][0] = horizontal_add_s64x2(vaddq_s64(h00_lo, h00_hi)) / size;
  H[0][1] = horizontal_add_s64x2(vaddq_s64(h01_lo, h01_hi)) / size;
  H[1][1] = horizontal_add_s64x2(vaddq_s64(h11_lo, h11_hi)) / size;
  H[1][0] = H[0][1];
  C[0] = horizontal_add_s64x2(vaddq_s64(c0_lo, c0_hi)) / size;
  C[1] = horizontal_add_s64x2(vaddq_s64(c1_lo, c1_hi)) / size;
}

static INLINE void calc_proj_params_r0_neon(const uint8_t *src8, int width,
                                            int height, int src_stride,
                                            const uint8_t *dat8, int dat_stride,
                                            int32_t *flt0, int flt0_stride,
                                            int64_t H[2][2], int64_t C[2]) {
  assert(width % 8 == 0);
  const int size = width * height;

  int64x2_t h00_lo = vdupq_n_s64(0);
  int64x2_t h00_hi = vdupq_n_s64(0);
  int64x2_t c0_lo = vdupq_n_s64(0);
  int64x2_t c0_hi = vdupq_n_s64(0);

  do {
    const uint8_t *src_ptr = src8;
    const uint8_t *dat_ptr = dat8;
    int32_t *flt0_ptr = flt0;
    int w = width;

    do {
      uint8x8_t s = vld1_u8(src_ptr);
      uint8x8_t d = vld1_u8(dat_ptr);
      int32x4_t f0_lo = vld1q_s32(flt0_ptr);
      int32x4_t f0_hi = vld1q_s32(flt0_ptr + 4);

      int16x8_t u = vreinterpretq_s16_u16(vshll_n_u8(d, SGRPROJ_RST_BITS));
      int16x8_t s_s16 = vreinterpretq_s16_u16(vshll_n_u8(s, SGRPROJ_RST_BITS));

      int32x4_t s_lo = vsubl_s16(vget_low_s16(s_s16), vget_low_s16(u));
      int32x4_t s_hi = vsubl_s16(vget_high_s16(s_s16), vget_high_s16(u));
      f0_lo = vsubw_s16(f0_lo, vget_low_s16(u));
      f0_hi = vsubw_s16(f0_hi, vget_high_s16(u));

      h00_lo = vmlal_s32(h00_lo, vget_low_s32(f0_lo), vget_low_s32(f0_lo));
      h00_lo = vmlal_s32(h00_lo, vget_high_s32(f0_lo), vget_high_s32(f0_lo));
      h00_hi = vmlal_s32(h00_hi, vget_low_s32(f0_hi), vget_low_s32(f0_hi));
      h00_hi = vmlal_s32(h00_hi, vget_high_s32(f0_hi), vget_high_s32(f0_hi));

      c0_lo = vmlal_s32(c0_lo, vget_low_s32(f0_lo), vget_low_s32(s_lo));
      c0_lo = vmlal_s32(c0_lo, vget_high_s32(f0_lo), vget_high_s32(s_lo));
      c0_hi = vmlal_s32(c0_hi, vget_low_s32(f0_hi), vget_low_s32(s_hi));
      c0_hi = vmlal_s32(c0_hi, vget_high_s32(f0_hi), vget_high_s32(s_hi));

      src_ptr += 8;
      dat_ptr += 8;
      flt0_ptr += 8;
      w -= 8;
    } while (w != 0);

    src8 += src_stride;
    dat8 += dat_stride;
    flt0 += flt0_stride;
  } while (--height != 0);

  H[0][0] = horizontal_add_s64x2(vaddq_s64(h00_lo, h00_hi)) / size;
  C[0] = horizontal_add_s64x2(vaddq_s64(c0_lo, c0_hi)) / size;
}

static INLINE void calc_proj_params_r1_neon(const uint8_t *src8, int width,
                                            int height, int src_stride,
                                            const uint8_t *dat8, int dat_stride,
                                            int32_t *flt1, int flt1_stride,
                                            int64_t H[2][2], int64_t C[2]) {
  assert(width % 8 == 0);
  const int size = width * height;

  int64x2_t h11_lo = vdupq_n_s64(0);
  int64x2_t h11_hi = vdupq_n_s64(0);
  int64x2_t c1_lo = vdupq_n_s64(0);
  int64x2_t c1_hi = vdupq_n_s64(0);

  do {
    const uint8_t *src_ptr = src8;
    const uint8_t *dat_ptr = dat8;
    int32_t *flt1_ptr = flt1;
    int w = width;

    do {
      uint8x8_t s = vld1_u8(src_ptr);
      uint8x8_t d = vld1_u8(dat_ptr);
      int32x4_t f1_lo = vld1q_s32(flt1_ptr);
      int32x4_t f1_hi = vld1q_s32(flt1_ptr + 4);

      int16x8_t u = vreinterpretq_s16_u16(vshll_n_u8(d, SGRPROJ_RST_BITS));
      int16x8_t s_s16 = vreinterpretq_s16_u16(vshll_n_u8(s, SGRPROJ_RST_BITS));

      int32x4_t s_lo = vsubl_s16(vget_low_s16(s_s16), vget_low_s16(u));
      int32x4_t s_hi = vsubl_s16(vget_high_s16(s_s16), vget_high_s16(u));
      f1_lo = vsubw_s16(f1_lo, vget_low_s16(u));
      f1_hi = vsubw_s16(f1_hi, vget_high_s16(u));

      h11_lo = vmlal_s32(h11_lo, vget_low_s32(f1_lo), vget_low_s32(f1_lo));
      h11_lo = vmlal_s32(h11_lo, vget_high_s32(f1_lo), vget_high_s32(f1_lo));
      h11_hi = vmlal_s32(h11_hi, vget_low_s32(f1_hi), vget_low_s32(f1_hi));
      h11_hi = vmlal_s32(h11_hi, vget_high_s32(f1_hi), vget_high_s32(f1_hi));

      c1_lo = vmlal_s32(c1_lo, vget_low_s32(f1_lo), vget_low_s32(s_lo));
      c1_lo = vmlal_s32(c1_lo, vget_high_s32(f1_lo), vget_high_s32(s_lo));
      c1_hi = vmlal_s32(c1_hi, vget_low_s32(f1_hi), vget_low_s32(s_hi));
      c1_hi = vmlal_s32(c1_hi, vget_high_s32(f1_hi), vget_high_s32(s_hi));

      src_ptr += 8;
      dat_ptr += 8;
      flt1_ptr += 8;
      w -= 8;
    } while (w != 0);

    src8 += src_stride;
    dat8 += dat_stride;
    flt1 += flt1_stride;
  } while (--height != 0);

  H[1][1] = horizontal_add_s64x2(vaddq_s64(h11_lo, h11_hi)) / size;
  C[1] = horizontal_add_s64x2(vaddq_s64(c1_lo, c1_hi)) / size;
}

// The function calls 3 subfunctions for the following cases :
// 1) When params->r[0] > 0 and params->r[1] > 0. In this case all elements
//    of C and H need to be computed.
// 2) When only params->r[0] > 0. In this case only H[0][0] and C[0] are
//    non-zero and need to be computed.
// 3) When only params->r[1] > 0. In this case only H[1][1] and C[1] are
//    non-zero and need to be computed.
void av1_calc_proj_params_neon(const uint8_t *src8, int width, int height,
                               int src_stride, const uint8_t *dat8,
                               int dat_stride, int32_t *flt0, int flt0_stride,
                               int32_t *flt1, int flt1_stride, int64_t H[2][2],
                               int64_t C[2], const sgr_params_type *params) {
  if ((params->r[0] > 0) && (params->r[1] > 0)) {
    calc_proj_params_r0_r1_neon(src8, width, height, src_stride, dat8,
                                dat_stride, flt0, flt0_stride, flt1,
                                flt1_stride, H, C);
  } else if (params->r[0] > 0) {
    calc_proj_params_r0_neon(src8, width, height, src_stride, dat8, dat_stride,
                             flt0, flt0_stride, H, C);
  } else if (params->r[1] > 0) {
    calc_proj_params_r1_neon(src8, width, height, src_stride, dat8, dat_stride,
                             flt1, flt1_stride, H, C);
  }
}
