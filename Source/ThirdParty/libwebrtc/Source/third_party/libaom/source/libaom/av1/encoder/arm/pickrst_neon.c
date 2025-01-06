/*
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved.
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

#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/sum_neon.h"
#include "aom_dsp/arm/transpose_neon.h"
#include "av1/common/restoration.h"
#include "av1/encoder/arm/pickrst_neon.h"
#include "av1/encoder/pickrst.h"

int64_t av1_lowbd_pixel_proj_error_neon(
    const uint8_t *src, int width, int height, int src_stride,
    const uint8_t *dat, int dat_stride, int32_t *flt0, int flt0_stride,
    int32_t *flt1, int flt1_stride, int xq[2], const sgr_params_type *params) {
  int64_t sse = 0;
  int64x2_t sse_s64 = vdupq_n_s64(0);

  if (params->r[0] > 0 && params->r[1] > 0) {
    int32x2_t xq_v = vld1_s32(xq);
    int32x2_t xq_sum_v = vshl_n_s32(vpadd_s32(xq_v, xq_v), SGRPROJ_RST_BITS);

    do {
      int j = 0;
      int32x4_t sse_s32 = vdupq_n_s32(0);

      do {
        const uint8x8_t d = vld1_u8(&dat[j]);
        const uint8x8_t s = vld1_u8(&src[j]);
        int32x4_t flt0_0 = vld1q_s32(&flt0[j]);
        int32x4_t flt0_1 = vld1q_s32(&flt0[j + 4]);
        int32x4_t flt1_0 = vld1q_s32(&flt1[j]);
        int32x4_t flt1_1 = vld1q_s32(&flt1[j + 4]);

        int32x4_t offset =
            vdupq_n_s32(1 << (SGRPROJ_RST_BITS + SGRPROJ_PRJ_BITS - 1));
        int32x4_t v0 = vmlaq_lane_s32(offset, flt0_0, xq_v, 0);
        int32x4_t v1 = vmlaq_lane_s32(offset, flt0_1, xq_v, 0);

        v0 = vmlaq_lane_s32(v0, flt1_0, xq_v, 1);
        v1 = vmlaq_lane_s32(v1, flt1_1, xq_v, 1);

        int16x8_t d_s16 = vreinterpretq_s16_u16(vmovl_u8(d));
        v0 = vmlsl_lane_s16(v0, vget_low_s16(d_s16),
                            vreinterpret_s16_s32(xq_sum_v), 0);
        v1 = vmlsl_lane_s16(v1, vget_high_s16(d_s16),
                            vreinterpret_s16_s32(xq_sum_v), 0);

        int16x4_t vr0 = vshrn_n_s32(v0, SGRPROJ_RST_BITS + SGRPROJ_PRJ_BITS);
        int16x4_t vr1 = vshrn_n_s32(v1, SGRPROJ_RST_BITS + SGRPROJ_PRJ_BITS);

        int16x8_t diff = vreinterpretq_s16_u16(vsubl_u8(d, s));
        int16x8_t e = vaddq_s16(vcombine_s16(vr0, vr1), diff);
        int16x4_t e_lo = vget_low_s16(e);
        int16x4_t e_hi = vget_high_s16(e);

        sse_s32 = vmlal_s16(sse_s32, e_lo, e_lo);
        sse_s32 = vmlal_s16(sse_s32, e_hi, e_hi);

        j += 8;
      } while (j <= width - 8);

      for (int k = j; k < width; ++k) {
        int32_t u = (dat[k] << SGRPROJ_RST_BITS);
        int32_t v = (1 << (SGRPROJ_RST_BITS + SGRPROJ_PRJ_BITS - 1)) +
                    xq[0] * flt0[k] + xq[1] * flt1[k] - u * (xq[0] + xq[1]);
        int32_t e =
            (v >> (SGRPROJ_RST_BITS + SGRPROJ_PRJ_BITS)) + dat[k] - src[k];
        sse += e * e;
      }

      sse_s64 = vpadalq_s32(sse_s64, sse_s32);

      dat += dat_stride;
      src += src_stride;
      flt0 += flt0_stride;
      flt1 += flt1_stride;
    } while (--height != 0);
  } else if (params->r[0] > 0 || params->r[1] > 0) {
    int xq_active = (params->r[0] > 0) ? xq[0] : xq[1];
    int32_t *flt = (params->r[0] > 0) ? flt0 : flt1;
    int flt_stride = (params->r[0] > 0) ? flt0_stride : flt1_stride;
    int32x2_t xq_v = vdup_n_s32(xq_active);

    do {
      int32x4_t sse_s32 = vdupq_n_s32(0);
      int j = 0;

      do {
        const uint8x8_t d = vld1_u8(&dat[j]);
        const uint8x8_t s = vld1_u8(&src[j]);
        int32x4_t flt_0 = vld1q_s32(&flt[j]);
        int32x4_t flt_1 = vld1q_s32(&flt[j + 4]);
        int16x8_t d_s16 =
            vreinterpretq_s16_u16(vshll_n_u8(d, SGRPROJ_RST_BITS));

        int32x4_t sub_0 = vsubw_s16(flt_0, vget_low_s16(d_s16));
        int32x4_t sub_1 = vsubw_s16(flt_1, vget_high_s16(d_s16));

        int32x4_t offset =
            vdupq_n_s32(1 << (SGRPROJ_RST_BITS + SGRPROJ_PRJ_BITS - 1));
        int32x4_t v0 = vmlaq_lane_s32(offset, sub_0, xq_v, 0);
        int32x4_t v1 = vmlaq_lane_s32(offset, sub_1, xq_v, 0);

        int16x4_t vr0 = vshrn_n_s32(v0, SGRPROJ_RST_BITS + SGRPROJ_PRJ_BITS);
        int16x4_t vr1 = vshrn_n_s32(v1, SGRPROJ_RST_BITS + SGRPROJ_PRJ_BITS);

        int16x8_t diff = vreinterpretq_s16_u16(vsubl_u8(d, s));
        int16x8_t e = vaddq_s16(vcombine_s16(vr0, vr1), diff);
        int16x4_t e_lo = vget_low_s16(e);
        int16x4_t e_hi = vget_high_s16(e);

        sse_s32 = vmlal_s16(sse_s32, e_lo, e_lo);
        sse_s32 = vmlal_s16(sse_s32, e_hi, e_hi);

        j += 8;
      } while (j <= width - 8);

      for (int k = j; k < width; ++k) {
        int32_t u = dat[k] << SGRPROJ_RST_BITS;
        int32_t v = xq_active * (flt[k] - u);
        int32_t e = ROUND_POWER_OF_TWO(v, SGRPROJ_RST_BITS + SGRPROJ_PRJ_BITS) +
                    dat[k] - src[k];
        sse += e * e;
      }

      sse_s64 = vpadalq_s32(sse_s64, sse_s32);

      dat += dat_stride;
      src += src_stride;
      flt += flt_stride;
    } while (--height != 0);
  } else {
    uint32x4_t sse_s32 = vdupq_n_u32(0);

    do {
      int j = 0;

      do {
        const uint8x16_t d = vld1q_u8(&dat[j]);
        const uint8x16_t s = vld1q_u8(&src[j]);

        uint8x16_t diff = vabdq_u8(d, s);
        uint8x8_t diff_lo = vget_low_u8(diff);
        uint8x8_t diff_hi = vget_high_u8(diff);

        sse_s32 = vpadalq_u16(sse_s32, vmull_u8(diff_lo, diff_lo));
        sse_s32 = vpadalq_u16(sse_s32, vmull_u8(diff_hi, diff_hi));

        j += 16;
      } while (j <= width - 16);

      for (int k = j; k < width; ++k) {
        int32_t e = dat[k] - src[k];
        sse += e * e;
      }

      dat += dat_stride;
      src += src_stride;
    } while (--height != 0);

    sse_s64 = vreinterpretq_s64_u64(vpaddlq_u32(sse_s32));
  }

  sse += horizontal_add_s64x2(sse_s64);
  return sse;
}

// We can accumulate up to 32768 8-bit multiplication results in a signed
// 32-bit integer. We are processing 2 pixels at a time, so the accumulator max
// can be as high as 16384 for the compute stats.
#define STAT_ACCUMULATOR_MAX 16384

static inline uint8x8_t tbl2(uint8x16_t a, uint8x16_t b, uint8x8_t idx) {
#if AOM_ARCH_AARCH64
  uint8x16x2_t table = { { a, b } };
  return vqtbl2_u8(table, idx);
#else
  uint8x8x4_t table = { { vget_low_u8(a), vget_high_u8(a), vget_low_u8(b),
                          vget_high_u8(b) } };
  return vtbl4_u8(table, idx);
#endif
}

static inline uint8x16_t tbl2q(uint8x16_t a, uint8x16_t b, uint8x16_t idx) {
#if AOM_ARCH_AARCH64
  uint8x16x2_t table = { { a, b } };
  return vqtbl2q_u8(table, idx);
#else
  uint8x8x4_t table = { { vget_low_u8(a), vget_high_u8(a), vget_low_u8(b),
                          vget_high_u8(b) } };
  return vcombine_u8(vtbl4_u8(table, vget_low_u8(idx)),
                     vtbl4_u8(table, vget_high_u8(idx)));
#endif
}

// The M matrix is accumulated in STAT_ACCUMULATOR_MAX steps to speed-up the
// computation. This function computes the final M from the accumulated
// (src_s64) and the residual parts (src_s32). It also transposes the result as
// the output needs to be column-major.
static inline void acc_transpose_M(int64_t *dst, const int64_t *src_s64,
                                   const int32_t *src_s32, const int wiener_win,
                                   int scale) {
  for (int i = 0; i < wiener_win; ++i) {
    for (int j = 0; j < wiener_win; ++j) {
      int tr_idx = j * wiener_win + i;
      *dst++ += (int64_t)(src_s64[tr_idx] + src_s32[tr_idx]) * scale;
    }
  }
}

// The resulting H is a column-major matrix accumulated from the transposed
// (column-major) samples of the filter kernel (5x5 or 7x7) viewed as a single
// vector. For the 7x7 filter case: H(49x49) = [49 x 1] x [1 x 49]. This
// function transforms back to the originally expected format (double
// transpose). The H matrix is accumulated in STAT_ACCUMULATOR_MAX steps to
// speed-up the computation. This function computes the final H from the
// accumulated (src_s64) and the residual parts (src_s32). The computed H is
// only an upper triangle matrix, this function also fills the lower triangle of
// the resulting matrix.
static void update_H(int64_t *dst, const int64_t *src_s64,
                     const int32_t *src_s32, const int wiener_win, int stride,
                     int scale) {
  // For a simplified theoretical 3x3 case where `wiener_win` is 3 and
  // `wiener_win2` is 9, the M matrix is 3x3:
  // 0, 3, 6
  // 1, 4, 7
  // 2, 5, 8
  //
  // This is viewed as a vector to compute H (9x9) by vector outer product:
  // 0, 3, 6, 1, 4, 7, 2, 5, 8
  //
  // Double transpose and upper triangle remapping for 3x3 -> 9x9 case:
  // 0,    3,    6,    1,    4,    7,    2,    5,    8,
  // 3,   30,   33,   12,   31,   34,   21,   32,   35,
  // 6,   33,   60,   15,   42,   61,   24,   51,   62,
  // 1,   12,   15,   10,   13,   16,   11,   14,   17,
  // 4,   31,   42,   13,   40,   43,   22,   41,   44,
  // 7,   34,   61,   16,   43,   70,   25,   52,   71,
  // 2,   21,   24,   11,   22,   25,   20,   23,   26,
  // 5,   32,   51,   14,   41,   52,   23,   50,   53,
  // 8,   35,   62,   17,   44,   71,   26,   53,   80,
  const int wiener_win2 = wiener_win * wiener_win;

  // Loop through the indices according to the remapping above, along the
  // columns:
  // 0, wiener_win, 2 * wiener_win, ..., 1, 1 + 2 * wiener_win, ...,
  // wiener_win - 1, wiener_win - 1 + wiener_win, ...
  // For the 3x3 case `j` will be: 0, 3, 6, 1, 4, 7, 2, 5, 8.
  for (int i = 0; i < wiener_win; ++i) {
    for (int j = i; j < wiener_win2; j += wiener_win) {
      // These two inner loops are the same as the two outer loops, but running
      // along rows instead of columns. For the 3x3 case `l` will be:
      // 0, 3, 6, 1, 4, 7, 2, 5, 8.
      for (int k = 0; k < wiener_win; ++k) {
        for (int l = k; l < wiener_win2; l += wiener_win) {
          // The nominal double transpose indexing would be:
          // int idx = stride * j + l;
          // However we need the upper-triangle indices, it is easy with some
          // min/max operations.
          int tr_idx = stride * AOMMIN(j, l) + AOMMAX(j, l);

          // Resulting matrix is filled by combining the 64-bit and the residual
          // 32-bit matrices together with scaling.
          *dst++ += (int64_t)(src_s64[tr_idx] + src_s32[tr_idx]) * scale;
        }
      }
    }
  }
}

// Load 7x7 matrix into 3 and a half 128-bit vectors from consecutive rows, the
// last load address is offset to prevent out-of-bounds access.
static inline void load_and_pack_u8_8x7(uint8x16_t dst[4], const uint8_t *src,
                                        ptrdiff_t stride) {
  dst[0] = vcombine_u8(vld1_u8(src), vld1_u8(src + stride));
  src += 2 * stride;
  dst[1] = vcombine_u8(vld1_u8(src), vld1_u8(src + stride));
  src += 2 * stride;
  dst[2] = vcombine_u8(vld1_u8(src), vld1_u8(src + stride));
  src += 2 * stride;
  dst[3] = vcombine_u8(vld1_u8(src - 1), vdup_n_u8(0));
}

static inline void compute_stats_win7_downsampled_neon(
    const uint8_t *dgd, const uint8_t *src, int width, int height,
    int dgd_stride, int src_stride, int avg, int64_t *M, int64_t *H,
    int downsample_factor) {
  // Matrix names are capitalized to help readability.
  DECLARE_ALIGNED(64, int16_t, DGD_AVG0[WIENER_WIN2_ALIGN3]);
  DECLARE_ALIGNED(64, int16_t, DGD_AVG1[WIENER_WIN2_ALIGN3]);
  DECLARE_ALIGNED(64, int32_t, M_s32[WIENER_WIN2_ALIGN3]);
  DECLARE_ALIGNED(64, int64_t, M_s64[WIENER_WIN2_ALIGN3]);
  DECLARE_ALIGNED(64, int32_t, H_s32[WIENER_WIN2 * WIENER_WIN2_ALIGN2]);
  DECLARE_ALIGNED(64, int64_t, H_s64[WIENER_WIN2 * WIENER_WIN2_ALIGN2]);

  memset(M_s32, 0, sizeof(M_s32));
  memset(M_s64, 0, sizeof(M_s64));
  memset(H_s32, 0, sizeof(H_s32));
  memset(H_s64, 0, sizeof(H_s64));

  // Look-up tables to create 8x6 matrix with consecutive elements from two 7x7
  // matrices.
  // clang-format off
  DECLARE_ALIGNED(16, static const uint8_t, shuffle_stats7[96]) = {
    0,  1,  2,  3,  4,  5,  6,  8,  9, 10, 11, 12, 13, 14, 16, 17,
    2,  3,  4,  5,  6,  8,  9, 10, 11, 12, 13, 14, 16, 17, 18, 19,
    4,  5,  6,  8,  9, 10, 11, 12, 13, 14, 17, 18, 19, 20, 21, 22,
    1,  2,  3,  4,  5,  6,  7,  9, 10, 11, 12, 13, 14, 15, 17, 18,
    3,  4,  5,  6,  7,  9, 10, 11, 12, 13, 14, 15, 17, 18, 19, 20,
    5,  6,  7,  9, 10, 11, 12, 13, 14, 15, 18, 19, 20, 21, 22, 23,
  };
  // clang-format on

  const uint8x16_t lut0 = vld1q_u8(shuffle_stats7 + 0);
  const uint8x16_t lut1 = vld1q_u8(shuffle_stats7 + 16);
  const uint8x16_t lut2 = vld1q_u8(shuffle_stats7 + 32);
  const uint8x16_t lut3 = vld1q_u8(shuffle_stats7 + 48);
  const uint8x16_t lut4 = vld1q_u8(shuffle_stats7 + 64);
  const uint8x16_t lut5 = vld1q_u8(shuffle_stats7 + 80);

  int acc_cnt = STAT_ACCUMULATOR_MAX;
  const int src_next = downsample_factor * src_stride - width;
  const int dgd_next = downsample_factor * dgd_stride - width;
  const uint8x8_t avg_u8 = vdup_n_u8(avg);

  do {
    int j = width;
    while (j >= 2) {
      // Load two adjacent, overlapping 7x7 matrices: a 8x7 matrix with the
      // middle 6x7 elements being shared.
      uint8x16_t dgd_rows[4];
      load_and_pack_u8_8x7(dgd_rows, dgd, dgd_stride);

      const uint8_t *dgd_ptr = dgd + dgd_stride * 6;
      dgd += 2;

      // Re-arrange (and widen) the combined 8x7 matrix to have the 2 whole 7x7
      // matrices (1 for each of the 2 pixels) separated into distinct
      // int16x8_t[6] arrays. These arrays contain 48 elements of the 49 (7x7).
      // Compute `dgd - avg` for both buffers. Each DGD_AVG buffer contains 49
      // consecutive elements.
      int16x8_t dgd_avg0[6];
      int16x8_t dgd_avg1[6];
      uint8x16_t dgd_shuf0 = tbl2q(dgd_rows[0], dgd_rows[1], lut0);
      uint8x16_t dgd_shuf3 = tbl2q(dgd_rows[0], dgd_rows[1], lut3);

      dgd_avg0[0] =
          vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(dgd_shuf0), avg_u8));
      dgd_avg0[1] =
          vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(dgd_shuf0), avg_u8));
      dgd_avg1[0] =
          vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(dgd_shuf3), avg_u8));
      dgd_avg1[1] =
          vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(dgd_shuf3), avg_u8));

      vst1q_s16(DGD_AVG0, dgd_avg0[0]);
      vst1q_s16(DGD_AVG0 + 8, dgd_avg0[1]);
      vst1q_s16(DGD_AVG1, dgd_avg1[0]);
      vst1q_s16(DGD_AVG1 + 8, dgd_avg1[1]);

      uint8x16_t dgd_shuf1 = tbl2q(dgd_rows[1], dgd_rows[2], lut1);
      uint8x16_t dgd_shuf4 = tbl2q(dgd_rows[1], dgd_rows[2], lut4);

      dgd_avg0[2] =
          vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(dgd_shuf1), avg_u8));
      dgd_avg0[3] =
          vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(dgd_shuf1), avg_u8));
      dgd_avg1[2] =
          vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(dgd_shuf4), avg_u8));
      dgd_avg1[3] =
          vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(dgd_shuf4), avg_u8));

      vst1q_s16(DGD_AVG0 + 16, dgd_avg0[2]);
      vst1q_s16(DGD_AVG0 + 24, dgd_avg0[3]);
      vst1q_s16(DGD_AVG1 + 16, dgd_avg1[2]);
      vst1q_s16(DGD_AVG1 + 24, dgd_avg1[3]);

      uint8x16_t dgd_shuf2 = tbl2q(dgd_rows[2], dgd_rows[3], lut2);
      uint8x16_t dgd_shuf5 = tbl2q(dgd_rows[2], dgd_rows[3], lut5);

      dgd_avg0[4] =
          vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(dgd_shuf2), avg_u8));
      dgd_avg0[5] =
          vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(dgd_shuf2), avg_u8));
      dgd_avg1[4] =
          vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(dgd_shuf5), avg_u8));
      dgd_avg1[5] =
          vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(dgd_shuf5), avg_u8));

      vst1q_s16(DGD_AVG0 + 32, dgd_avg0[4]);
      vst1q_s16(DGD_AVG0 + 40, dgd_avg0[5]);
      vst1q_s16(DGD_AVG1 + 32, dgd_avg1[4]);
      vst1q_s16(DGD_AVG1 + 40, dgd_avg1[5]);

      // The remaining last (49th) elements of `dgd - avg`.
      DGD_AVG0[48] = dgd_ptr[6] - avg;
      DGD_AVG1[48] = dgd_ptr[7] - avg;

      // Accumulate into row-major variant of matrix M (cross-correlation) for 2
      // output pixels at a time. M is of size 7 * 7. It needs to be filled such
      // that multiplying one element from src with each element of a row of the
      // wiener window will fill one column of M. However this is not very
      // convenient in terms of memory access, as it means we do contiguous
      // loads of dgd but strided stores to M. As a result, we use an
      // intermediate matrix M_s32 which is instead filled such that one row of
      // the wiener window gives one row of M_s32. Once fully computed, M_s32 is
      // then transposed to return M.
      int src_avg0 = *src++ - avg;
      int src_avg1 = *src++ - avg;
      int16x4_t src_avg0_s16 = vdup_n_s16(src_avg0);
      int16x4_t src_avg1_s16 = vdup_n_s16(src_avg1);
      update_M_2pixels(M_s32 + 0, src_avg0_s16, src_avg1_s16, dgd_avg0[0],
                       dgd_avg1[0]);
      update_M_2pixels(M_s32 + 8, src_avg0_s16, src_avg1_s16, dgd_avg0[1],
                       dgd_avg1[1]);
      update_M_2pixels(M_s32 + 16, src_avg0_s16, src_avg1_s16, dgd_avg0[2],
                       dgd_avg1[2]);
      update_M_2pixels(M_s32 + 24, src_avg0_s16, src_avg1_s16, dgd_avg0[3],
                       dgd_avg1[3]);
      update_M_2pixels(M_s32 + 32, src_avg0_s16, src_avg1_s16, dgd_avg0[4],
                       dgd_avg1[4]);
      update_M_2pixels(M_s32 + 40, src_avg0_s16, src_avg1_s16, dgd_avg0[5],
                       dgd_avg1[5]);

      // Last (49th) element of M_s32 can be computed as scalar more efficiently
      // for 2 output pixels.
      M_s32[48] += DGD_AVG0[48] * src_avg0 + DGD_AVG1[48] * src_avg1;

      // Start accumulating into row-major version of matrix H
      // (auto-covariance), it expects the DGD_AVG[01] matrices to also be
      // row-major. H is of size 49 * 49. It is filled by multiplying every pair
      // of elements of the wiener window together (vector outer product). Since
      // it is a symmetric matrix, we only compute the upper-right triangle, and
      // then copy it down to the lower-left later. The upper triangle is
      // covered by 4x4 tiles. The original algorithm assumes the M matrix is
      // column-major and the resulting H matrix is also expected to be
      // column-major. It is not efficient to work with column-major matrices,
      // so we accumulate into a row-major matrix H_s32. At the end of the
      // algorithm a double transpose transformation will convert H_s32 back to
      // the expected output layout.
      update_H_7x7_2pixels(H_s32, DGD_AVG0, DGD_AVG1);

      // The last element of the triangle of H_s32 matrix can be computed as a
      // scalar more efficiently.
      H_s32[48 * WIENER_WIN2_ALIGN2 + 48] +=
          DGD_AVG0[48] * DGD_AVG0[48] + DGD_AVG1[48] * DGD_AVG1[48];

      // Accumulate into 64-bit after STAT_ACCUMULATOR_MAX iterations to prevent
      // overflow.
      if (--acc_cnt == 0) {
        acc_cnt = STAT_ACCUMULATOR_MAX;

        accumulate_and_clear(M_s64, M_s32, WIENER_WIN2_ALIGN2);

        // The widening accumulation is only needed for the upper triangle part
        // of the matrix.
        int64_t *lh = H_s64;
        int32_t *lh32 = H_s32;
        for (int k = 0; k < WIENER_WIN2; ++k) {
          // The widening accumulation is only run for the relevant parts
          // (upper-right triangle) in a row 4-element aligned.
          int k4 = k / 4 * 4;
          accumulate_and_clear(lh + k4, lh32 + k4, 48 - k4);

          // Last element of the row is computed separately.
          lh[48] += lh32[48];
          lh32[48] = 0;

          lh += WIENER_WIN2_ALIGN2;
          lh32 += WIENER_WIN2_ALIGN2;
        }
      }

      j -= 2;
    }

    // Computations for odd pixel in the row.
    if (width & 1) {
      // Load two adjacent, overlapping 7x7 matrices: a 8x7 matrix with the
      // middle 6x7 elements being shared.
      uint8x16_t dgd_rows[4];
      load_and_pack_u8_8x7(dgd_rows, dgd, dgd_stride);

      const uint8_t *dgd_ptr = dgd + dgd_stride * 6;
      ++dgd;

      // Re-arrange (and widen) the combined 8x7 matrix to have a whole 7x7
      // matrix tightly packed into a int16x8_t[6] array. This array contains
      // 48 elements of the 49 (7x7). Compute `dgd - avg` for the whole buffer.
      // The DGD_AVG buffer contains 49 consecutive elements.
      int16x8_t dgd_avg0[6];
      uint8x16_t dgd_shuf0 = tbl2q(dgd_rows[0], dgd_rows[1], lut0);
      dgd_avg0[0] =
          vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(dgd_shuf0), avg_u8));
      dgd_avg0[1] =
          vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(dgd_shuf0), avg_u8));
      vst1q_s16(DGD_AVG0, dgd_avg0[0]);
      vst1q_s16(DGD_AVG0 + 8, dgd_avg0[1]);

      uint8x16_t dgd_shuf1 = tbl2q(dgd_rows[1], dgd_rows[2], lut1);
      dgd_avg0[2] =
          vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(dgd_shuf1), avg_u8));
      dgd_avg0[3] =
          vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(dgd_shuf1), avg_u8));
      vst1q_s16(DGD_AVG0 + 16, dgd_avg0[2]);
      vst1q_s16(DGD_AVG0 + 24, dgd_avg0[3]);

      uint8x16_t dgd_shuf2 = tbl2q(dgd_rows[2], dgd_rows[3], lut2);
      dgd_avg0[4] =
          vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(dgd_shuf2), avg_u8));
      dgd_avg0[5] =
          vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(dgd_shuf2), avg_u8));
      vst1q_s16(DGD_AVG0 + 32, dgd_avg0[4]);
      vst1q_s16(DGD_AVG0 + 40, dgd_avg0[5]);

      // The remaining last (49th) element of `dgd - avg`.
      DGD_AVG0[48] = dgd_ptr[6] - avg;

      // Accumulate into row-major order variant of matrix M (cross-correlation)
      // for 1 output pixel at a time. M is of size 7 * 7. It needs to be filled
      // such that multiplying one element from src with each element of a row
      // of the wiener window will fill one column of M. However this is not
      // very convenient in terms of memory access, as it means we do
      // contiguous loads of dgd but strided stores to M. As a result, we use an
      // intermediate matrix M_s32 which is instead filled such that one row of
      // the wiener window gives one row of M_s32. Once fully computed, M_s32 is
      // then transposed to return M.
      int src_avg0 = *src++ - avg;
      int16x4_t src_avg0_s16 = vdup_n_s16(src_avg0);
      update_M_1pixel(M_s32 + 0, src_avg0_s16, dgd_avg0[0]);
      update_M_1pixel(M_s32 + 8, src_avg0_s16, dgd_avg0[1]);
      update_M_1pixel(M_s32 + 16, src_avg0_s16, dgd_avg0[2]);
      update_M_1pixel(M_s32 + 24, src_avg0_s16, dgd_avg0[3]);
      update_M_1pixel(M_s32 + 32, src_avg0_s16, dgd_avg0[4]);
      update_M_1pixel(M_s32 + 40, src_avg0_s16, dgd_avg0[5]);

      // Last (49th) element of M_s32 can be computed as scalar more efficiently
      // for 1 output pixel.
      M_s32[48] += DGD_AVG0[48] * src_avg0;

      // Start accumulating into row-major order version of matrix H
      // (auto-covariance), it expects the DGD_AVG0 matrix to also be row-major.
      // H is of size 49 * 49. It is filled by multiplying every pair of
      // elements of the wiener window together (vector outer product). Since it
      // is a symmetric matrix, we only compute the upper-right triangle, and
      // then copy it down to the lower-left later. The upper triangle is
      // covered by 4x4 tiles. The original algorithm assumes the M matrix is
      // column-major and the resulting H matrix is also expected to be
      // column-major. It is not efficient to work column-major matrices, so we
      // accumulate into a row-major matrix H_s32. At the end of the algorithm a
      // double transpose transformation will convert H_s32 back to the expected
      // output layout.
      update_H_1pixel(H_s32, DGD_AVG0, WIENER_WIN2_ALIGN2, 48);

      // The last element of the triangle of H_s32 matrix can be computed as
      // scalar more efficiently.
      H_s32[48 * WIENER_WIN2_ALIGN2 + 48] += DGD_AVG0[48] * DGD_AVG0[48];
    }

    src += src_next;
    dgd += dgd_next;
  } while (--height != 0);

  acc_transpose_M(M, M_s64, M_s32, WIENER_WIN, downsample_factor);

  update_H(H, H_s64, H_s32, WIENER_WIN, WIENER_WIN2_ALIGN2, downsample_factor);
}

// Load 5x5 matrix into 2 and a half 128-bit vectors from consecutive rows, the
// last load address is offset to prevent out-of-bounds access.
static inline void load_and_pack_u8_6x5(uint8x16_t dst[3], const uint8_t *src,
                                        ptrdiff_t stride) {
  dst[0] = vcombine_u8(vld1_u8(src), vld1_u8(src + stride));
  src += 2 * stride;
  dst[1] = vcombine_u8(vld1_u8(src), vld1_u8(src + stride));
  src += 2 * stride;
  dst[2] = vcombine_u8(vld1_u8(src - 3), vdup_n_u8(0));
}

static inline void compute_stats_win5_downsampled_neon(
    const uint8_t *dgd, const uint8_t *src, int width, int height,
    int dgd_stride, int src_stride, int avg, int64_t *M, int64_t *H,
    int downsample_factor) {
  // Matrix names are capitalized to help readability.
  DECLARE_ALIGNED(64, int16_t, DGD_AVG0[WIENER_WIN2_REDUCED_ALIGN3]);
  DECLARE_ALIGNED(64, int16_t, DGD_AVG1[WIENER_WIN2_REDUCED_ALIGN3]);
  DECLARE_ALIGNED(64, int32_t, M_s32[WIENER_WIN2_REDUCED_ALIGN3]);
  DECLARE_ALIGNED(64, int64_t, M_s64[WIENER_WIN2_REDUCED_ALIGN3]);
  DECLARE_ALIGNED(64, int32_t,
                  H_s32[WIENER_WIN2_REDUCED * WIENER_WIN2_REDUCED_ALIGN2]);
  DECLARE_ALIGNED(64, int64_t,
                  H_s64[WIENER_WIN2_REDUCED * WIENER_WIN2_REDUCED_ALIGN2]);

  memset(M_s32, 0, sizeof(M_s32));
  memset(M_s64, 0, sizeof(M_s64));
  memset(H_s32, 0, sizeof(H_s32));
  memset(H_s64, 0, sizeof(H_s64));

  // Look-up tables to create 8x3 matrix with consecutive elements from two 5x5
  // matrices.
  // clang-format off
  DECLARE_ALIGNED(16, static const uint8_t, shuffle_stats5[48]) = {
    0,  1,  2,  3,  4,  8,  9, 10, 11, 12, 16, 17, 18, 19, 20, 24,
    1,  2,  3,  4,  5,  9, 10, 11, 12, 13, 17, 18, 19, 20, 21, 25,
    9, 10, 11, 12, 19, 20, 21, 22, 10, 11, 12, 13, 20, 21, 22, 23,
  };
  // clang-format on

  const uint8x16_t lut0 = vld1q_u8(shuffle_stats5 + 0);
  const uint8x16_t lut1 = vld1q_u8(shuffle_stats5 + 16);
  const uint8x16_t lut2 = vld1q_u8(shuffle_stats5 + 32);

  int acc_cnt = STAT_ACCUMULATOR_MAX;
  const int src_next = downsample_factor * src_stride - width;
  const int dgd_next = downsample_factor * dgd_stride - width;
  const uint8x8_t avg_u8 = vdup_n_u8(avg);

  do {
    int j = width;
    while (j >= 2) {
      // Load two adjacent, overlapping 5x5 matrices: a 6x5 matrix with the
      // middle 4x5 elements being shared.
      uint8x16_t dgd_rows[3];
      load_and_pack_u8_6x5(dgd_rows, dgd, dgd_stride);

      const uint8_t *dgd_ptr = dgd + dgd_stride * 4;
      dgd += 2;

      // Re-arrange (and widen) the combined 6x5 matrix to have the 2 whole 5x5
      // matrices (1 for each of the 2 pixels) separated into distinct
      // int16x8_t[3] arrays. These arrays contain 24 elements of the 25 (5x5).
      // Compute `dgd - avg` for both buffers. Each DGD_AVG buffer contains 25
      // consecutive elements.
      int16x8_t dgd_avg0[3];
      int16x8_t dgd_avg1[3];
      uint8x16_t dgd_shuf0 = tbl2q(dgd_rows[0], dgd_rows[1], lut0);
      uint8x16_t dgd_shuf1 = tbl2q(dgd_rows[0], dgd_rows[1], lut1);
      uint8x16_t dgd_shuf2 = tbl2q(dgd_rows[1], dgd_rows[2], lut2);

      dgd_avg0[0] =
          vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(dgd_shuf0), avg_u8));
      dgd_avg0[1] =
          vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(dgd_shuf0), avg_u8));
      dgd_avg0[2] =
          vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(dgd_shuf2), avg_u8));
      dgd_avg1[0] =
          vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(dgd_shuf1), avg_u8));
      dgd_avg1[1] =
          vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(dgd_shuf1), avg_u8));
      dgd_avg1[2] =
          vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(dgd_shuf2), avg_u8));

      vst1q_s16(DGD_AVG0 + 0, dgd_avg0[0]);
      vst1q_s16(DGD_AVG0 + 8, dgd_avg0[1]);
      vst1q_s16(DGD_AVG0 + 16, dgd_avg0[2]);
      vst1q_s16(DGD_AVG1 + 0, dgd_avg1[0]);
      vst1q_s16(DGD_AVG1 + 8, dgd_avg1[1]);
      vst1q_s16(DGD_AVG1 + 16, dgd_avg1[2]);

      // The remaining last (25th) elements of `dgd - avg`.
      DGD_AVG0[24] = dgd_ptr[4] - avg;
      DGD_AVG1[24] = dgd_ptr[5] - avg;

      // Accumulate into row-major variant of matrix M (cross-correlation) for 2
      // output pixels at a time. M is of size 5 * 5. It needs to be filled such
      // that multiplying one element from src with each element of a row of the
      // wiener window will fill one column of M. However this is not very
      // convenient in terms of memory access, as it means we do contiguous
      // loads of dgd but strided stores to M. As a result, we use an
      // intermediate matrix M_s32 which is instead filled such that one row of
      // the wiener window gives one row of M_s32. Once fully computed, M_s32 is
      // then transposed to return M.
      int src_avg0 = *src++ - avg;
      int src_avg1 = *src++ - avg;
      int16x4_t src_avg0_s16 = vdup_n_s16(src_avg0);
      int16x4_t src_avg1_s16 = vdup_n_s16(src_avg1);
      update_M_2pixels(M_s32 + 0, src_avg0_s16, src_avg1_s16, dgd_avg0[0],
                       dgd_avg1[0]);
      update_M_2pixels(M_s32 + 8, src_avg0_s16, src_avg1_s16, dgd_avg0[1],
                       dgd_avg1[1]);
      update_M_2pixels(M_s32 + 16, src_avg0_s16, src_avg1_s16, dgd_avg0[2],
                       dgd_avg1[2]);

      // Last (25th) element of M_s32 can be computed as scalar more efficiently
      // for 2 output pixels.
      M_s32[24] += DGD_AVG0[24] * src_avg0 + DGD_AVG1[24] * src_avg1;

      // Start accumulating into row-major version of matrix H
      // (auto-covariance), it expects the DGD_AVG[01] matrices to also be
      // row-major. H is of size 25 * 25. It is filled by multiplying every pair
      // of elements of the wiener window together (vector outer product). Since
      // it is a symmetric matrix, we only compute the upper-right triangle, and
      // then copy it down to the lower-left later. The upper triangle is
      // covered by 4x4 tiles. The original algorithm assumes the M matrix is
      // column-major and the resulting H matrix is also expected to be
      // column-major. It is not efficient to work with column-major matrices,
      // so we accumulate into a row-major matrix H_s32. At the end of the
      // algorithm a double transpose transformation will convert H_s32 back to
      // the expected output layout.
      update_H_5x5_2pixels(H_s32, DGD_AVG0, DGD_AVG1);

      // The last element of the triangle of H_s32 matrix can be computed as a
      // scalar more efficiently.
      H_s32[24 * WIENER_WIN2_REDUCED_ALIGN2 + 24] +=
          DGD_AVG0[24] * DGD_AVG0[24] + DGD_AVG1[24] * DGD_AVG1[24];

      // Accumulate into 64-bit after STAT_ACCUMULATOR_MAX iterations to prevent
      // overflow.
      if (--acc_cnt == 0) {
        acc_cnt = STAT_ACCUMULATOR_MAX;

        accumulate_and_clear(M_s64, M_s32, WIENER_WIN2_REDUCED_ALIGN2);

        // The widening accumulation is only needed for the upper triangle part
        // of the matrix.
        int64_t *lh = H_s64;
        int32_t *lh32 = H_s32;
        for (int k = 0; k < WIENER_WIN2_REDUCED; ++k) {
          // The widening accumulation is only run for the relevant parts
          // (upper-right triangle) in a row 4-element aligned.
          int k4 = k / 4 * 4;
          accumulate_and_clear(lh + k4, lh32 + k4, 24 - k4);

          // Last element of the row is computed separately.
          lh[24] += lh32[24];
          lh32[24] = 0;

          lh += WIENER_WIN2_REDUCED_ALIGN2;
          lh32 += WIENER_WIN2_REDUCED_ALIGN2;
        }
      }

      j -= 2;
    }

    // Computations for odd pixel in the row.
    if (width & 1) {
      // Load two adjacent, overlapping 5x5 matrices: a 6x5 matrix with the
      // middle 4x5 elements being shared.
      uint8x16_t dgd_rows[3];
      load_and_pack_u8_6x5(dgd_rows, dgd, dgd_stride);

      const uint8_t *dgd_ptr = dgd + dgd_stride * 4;
      ++dgd;

      // Re-arrange (and widen) the combined 6x5 matrix to have a whole 5x5
      // matrix tightly packed into a int16x8_t[3] array. This array contains
      // 24 elements of the 25 (5x5). Compute `dgd - avg` for the whole buffer.
      // The DGD_AVG buffer contains 25 consecutive elements.
      int16x8_t dgd_avg0[3];
      uint8x16_t dgd_shuf0 = tbl2q(dgd_rows[0], dgd_rows[1], lut0);
      uint8x8_t dgd_shuf1 = tbl2(dgd_rows[1], dgd_rows[2], vget_low_u8(lut2));

      dgd_avg0[0] =
          vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(dgd_shuf0), avg_u8));
      dgd_avg0[1] =
          vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(dgd_shuf0), avg_u8));
      dgd_avg0[2] = vreinterpretq_s16_u16(vsubl_u8(dgd_shuf1, avg_u8));

      vst1q_s16(DGD_AVG0 + 0, dgd_avg0[0]);
      vst1q_s16(DGD_AVG0 + 8, dgd_avg0[1]);
      vst1q_s16(DGD_AVG0 + 16, dgd_avg0[2]);

      // The remaining last (25th) element of `dgd - avg`.
      DGD_AVG0[24] = dgd_ptr[4] - avg;

      // Accumulate into row-major order variant of matrix M (cross-correlation)
      // for 1 output pixel at a time. M is of size 5 * 5. It needs to be filled
      // such that multiplying one element from src with each element of a row
      // of the wiener window will fill one column of M. However this is not
      // very convenient in terms of memory access, as it means we do
      // contiguous loads of dgd but strided stores to M. As a result, we use an
      // intermediate matrix M_s32 which is instead filled such that one row of
      // the wiener window gives one row of M_s32. Once fully computed, M_s32 is
      // then transposed to return M.
      int src_avg0 = *src++ - avg;
      int16x4_t src_avg0_s16 = vdup_n_s16(src_avg0);
      update_M_1pixel(M_s32 + 0, src_avg0_s16, dgd_avg0[0]);
      update_M_1pixel(M_s32 + 8, src_avg0_s16, dgd_avg0[1]);
      update_M_1pixel(M_s32 + 16, src_avg0_s16, dgd_avg0[2]);

      // Last (25th) element of M_s32 can be computed as scalar more efficiently
      // for 1 output pixel.
      M_s32[24] += DGD_AVG0[24] * src_avg0;

      // Start accumulating into row-major order version of matrix H
      // (auto-covariance), it expects the DGD_AVG0 matrix to also be row-major.
      // H is of size 25 * 25. It is filled by multiplying every pair of
      // elements of the wiener window together (vector outer product). Since it
      // is a symmetric matrix, we only compute the upper-right triangle, and
      // then copy it down to the lower-left later. The upper triangle is
      // covered by 4x4 tiles. The original algorithm assumes the M matrix is
      // column-major and the resulting H matrix is also expected to be
      // column-major. It is not efficient to work column-major matrices, so we
      // accumulate into a row-major matrix H_s32. At the end of the algorithm a
      // double transpose transformation will convert H_s32 back to the expected
      // output layout.
      update_H_1pixel(H_s32, DGD_AVG0, WIENER_WIN2_REDUCED_ALIGN2, 24);

      // The last element of the triangle of H_s32 matrix can be computed as a
      // scalar more efficiently.
      H_s32[24 * WIENER_WIN2_REDUCED_ALIGN2 + 24] +=
          DGD_AVG0[24] * DGD_AVG0[24];
    }

    src += src_next;
    dgd += dgd_next;
  } while (--height != 0);

  acc_transpose_M(M, M_s64, M_s32, WIENER_WIN_REDUCED, downsample_factor);

  update_H(H, H_s64, H_s32, WIENER_WIN_REDUCED, WIENER_WIN2_REDUCED_ALIGN2,
           downsample_factor);
}

static inline void hadd_update_6_stats_neon(const int64_t *const src,
                                            const int32x4_t *deltas,
                                            int64_t *const dst) {
  int32x4_t delta01 = horizontal_add_2d_s32(deltas[0], deltas[1]);
  int32x4_t delta23 = horizontal_add_2d_s32(deltas[2], deltas[3]);
  int32x4_t delta45 = horizontal_add_2d_s32(deltas[4], deltas[5]);

  int64x2_t delta01_s64 = vpaddlq_s32(delta01);
  int64x2_t delta23_s64 = vpaddlq_s32(delta23);
  int64x2_t delta45_s64 = vpaddlq_s32(delta45);

  int64x2_t src0 = vld1q_s64(src);
  int64x2_t src1 = vld1q_s64(src + 2);
  int64x2_t src2 = vld1q_s64(src + 4);

  vst1q_s64(dst, vaddq_s64(src0, delta01_s64));
  vst1q_s64(dst + 2, vaddq_s64(src1, delta23_s64));
  vst1q_s64(dst + 4, vaddq_s64(src2, delta45_s64));
}

static inline void hadd_update_4_stats_neon(const int64_t *const src,
                                            const int32x4_t *deltas,
                                            int64_t *const dst) {
  int32x4_t delta01 = horizontal_add_2d_s32(deltas[0], deltas[1]);
  int32x4_t delta23 = horizontal_add_2d_s32(deltas[2], deltas[3]);
  int64x2_t delta01_s64 = vpaddlq_s32(delta01);
  int64x2_t delta23_s64 = vpaddlq_s32(delta23);

  int64x2_t src0 = vld1q_s64(src);
  int64x2_t src1 = vld1q_s64(src + 2);
  vst1q_s64(dst, vaddq_s64(src0, delta01_s64));
  vst1q_s64(dst + 2, vaddq_s64(src1, delta23_s64));
}

static inline void compute_stats_win5_neon(
    const int16_t *const d, const int32_t d_stride, const int16_t *const s,
    const int32_t s_stride, const int32_t width, const int32_t height,
    int64_t *const M, int64_t *const H) {
  const int32_t wiener_win = WIENER_WIN_CHROMA;
  const int32_t wiener_win2 = wiener_win * wiener_win;
  const int32_t w16 = width & ~15;
  const int32_t h8 = height & ~7;
  int16x8_t mask[2];
  mask[0] = vld1q_s16(&(mask_16bit[16]) - width % 16);
  mask[1] = vld1q_s16(&(mask_16bit[16]) - width % 16 + 8);
  const int bit_depth = 8;
  int32_t i, j, x, y;

  const int32_t num_bit_left =
      32 - 1 /* sign */ - 2 * bit_depth /* energy */ + 2 /* SIMD */;
  const int32_t h_allowed =
      (1 << num_bit_left) / (w16 + ((w16 != width) ? 16 : 0));

  // Step 1: Calculate the top edge of the whole matrix, i.e., the top
  // edge of each triangle and square on the top row.
  j = 0;
  do {
    const int16_t *s_t = s;
    const int16_t *d_t = d;
    int32_t height_t = 0;
    int64x2_t sum_m[WIENER_WIN_CHROMA] = { vdupq_n_s64(0) };
    int64x2_t sum_h[WIENER_WIN_CHROMA] = { vdupq_n_s64(0) };
    int16x8_t src[2], dgd[2];

    do {
      const int32_t h_t =
          ((height - height_t) < h_allowed) ? (height - height_t) : h_allowed;
      int32x4_t row_m[WIENER_WIN_CHROMA] = { vdupq_n_s32(0) };
      int32x4_t row_h[WIENER_WIN_CHROMA] = { vdupq_n_s32(0) };

      y = h_t;
      do {
        x = 0;
        while (x < w16) {
          src[0] = vld1q_s16(s_t + x + 0);
          src[1] = vld1q_s16(s_t + x + 8);
          dgd[0] = vld1q_s16(d_t + x + 0);
          dgd[1] = vld1q_s16(d_t + x + 8);
          stats_top_win5_neon(src, dgd, d_t + j + x, d_stride, row_m, row_h);
          x += 16;
        }

        if (w16 != width) {
          src[0] = vld1q_s16(s_t + w16 + 0);
          src[1] = vld1q_s16(s_t + w16 + 8);
          dgd[0] = vld1q_s16(d_t + w16 + 0);
          dgd[1] = vld1q_s16(d_t + w16 + 8);
          src[0] = vandq_s16(src[0], mask[0]);
          src[1] = vandq_s16(src[1], mask[1]);
          dgd[0] = vandq_s16(dgd[0], mask[0]);
          dgd[1] = vandq_s16(dgd[1], mask[1]);
          stats_top_win5_neon(src, dgd, d_t + j + w16, d_stride, row_m, row_h);
        }

        s_t += s_stride;
        d_t += d_stride;
      } while (--y);

      sum_m[0] = vpadalq_s32(sum_m[0], row_m[0]);
      sum_m[1] = vpadalq_s32(sum_m[1], row_m[1]);
      sum_m[2] = vpadalq_s32(sum_m[2], row_m[2]);
      sum_m[3] = vpadalq_s32(sum_m[3], row_m[3]);
      sum_m[4] = vpadalq_s32(sum_m[4], row_m[4]);
      sum_h[0] = vpadalq_s32(sum_h[0], row_h[0]);
      sum_h[1] = vpadalq_s32(sum_h[1], row_h[1]);
      sum_h[2] = vpadalq_s32(sum_h[2], row_h[2]);
      sum_h[3] = vpadalq_s32(sum_h[3], row_h[3]);
      sum_h[4] = vpadalq_s32(sum_h[4], row_h[4]);

      height_t += h_t;
    } while (height_t < height);

#if AOM_ARCH_AARCH64
    int64x2_t sum_m0 = vpaddq_s64(sum_m[0], sum_m[1]);
    int64x2_t sum_m2 = vpaddq_s64(sum_m[2], sum_m[3]);
    vst1q_s64(&M[wiener_win * j + 0], sum_m0);
    vst1q_s64(&M[wiener_win * j + 2], sum_m2);
    M[wiener_win * j + 4] = vaddvq_s64(sum_m[4]);

    int64x2_t sum_h0 = vpaddq_s64(sum_h[0], sum_h[1]);
    int64x2_t sum_h2 = vpaddq_s64(sum_h[2], sum_h[3]);
    vst1q_s64(&H[wiener_win * j + 0], sum_h0);
    vst1q_s64(&H[wiener_win * j + 2], sum_h2);
    H[wiener_win * j + 4] = vaddvq_s64(sum_h[4]);
#else
    M[wiener_win * j + 0] = horizontal_add_s64x2(sum_m[0]);
    M[wiener_win * j + 1] = horizontal_add_s64x2(sum_m[1]);
    M[wiener_win * j + 2] = horizontal_add_s64x2(sum_m[2]);
    M[wiener_win * j + 3] = horizontal_add_s64x2(sum_m[3]);
    M[wiener_win * j + 4] = horizontal_add_s64x2(sum_m[4]);

    H[wiener_win * j + 0] = horizontal_add_s64x2(sum_h[0]);
    H[wiener_win * j + 1] = horizontal_add_s64x2(sum_h[1]);
    H[wiener_win * j + 2] = horizontal_add_s64x2(sum_h[2]);
    H[wiener_win * j + 3] = horizontal_add_s64x2(sum_h[3]);
    H[wiener_win * j + 4] = horizontal_add_s64x2(sum_h[4]);
#endif  // AOM_ARCH_AARCH64
  } while (++j < wiener_win);

  // Step 2: Calculate the left edge of each square on the top row.
  j = 1;
  do {
    const int16_t *d_t = d;
    int32_t height_t = 0;
    int64x2_t sum_h[WIENER_WIN_CHROMA - 1] = { vdupq_n_s64(0) };
    int16x8_t dgd[2];

    do {
      const int32_t h_t =
          ((height - height_t) < h_allowed) ? (height - height_t) : h_allowed;
      int32x4_t row_h[WIENER_WIN_CHROMA - 1] = { vdupq_n_s32(0) };

      y = h_t;
      do {
        x = 0;
        while (x < w16) {
          dgd[0] = vld1q_s16(d_t + j + x + 0);
          dgd[1] = vld1q_s16(d_t + j + x + 8);
          stats_left_win5_neon(dgd, d_t + x, d_stride, row_h);
          x += 16;
        }

        if (w16 != width) {
          dgd[0] = vld1q_s16(d_t + j + x + 0);
          dgd[1] = vld1q_s16(d_t + j + x + 8);
          dgd[0] = vandq_s16(dgd[0], mask[0]);
          dgd[1] = vandq_s16(dgd[1], mask[1]);
          stats_left_win5_neon(dgd, d_t + x, d_stride, row_h);
        }

        d_t += d_stride;
      } while (--y);

      sum_h[0] = vpadalq_s32(sum_h[0], row_h[0]);
      sum_h[1] = vpadalq_s32(sum_h[1], row_h[1]);
      sum_h[2] = vpadalq_s32(sum_h[2], row_h[2]);
      sum_h[3] = vpadalq_s32(sum_h[3], row_h[3]);

      height_t += h_t;
    } while (height_t < height);

#if AOM_ARCH_AARCH64
    int64x2_t sum_h0 = vpaddq_s64(sum_h[0], sum_h[1]);
    int64x2_t sum_h1 = vpaddq_s64(sum_h[2], sum_h[3]);
    vst1_s64(&H[1 * wiener_win2 + j * wiener_win], vget_low_s64(sum_h0));
    vst1_s64(&H[2 * wiener_win2 + j * wiener_win], vget_high_s64(sum_h0));
    vst1_s64(&H[3 * wiener_win2 + j * wiener_win], vget_low_s64(sum_h1));
    vst1_s64(&H[4 * wiener_win2 + j * wiener_win], vget_high_s64(sum_h1));
#else
    H[1 * wiener_win2 + j * wiener_win] = horizontal_add_s64x2(sum_h[0]);
    H[2 * wiener_win2 + j * wiener_win] = horizontal_add_s64x2(sum_h[1]);
    H[3 * wiener_win2 + j * wiener_win] = horizontal_add_s64x2(sum_h[2]);
    H[4 * wiener_win2 + j * wiener_win] = horizontal_add_s64x2(sum_h[3]);
#endif  // AOM_ARCH_AARCH64
  } while (++j < wiener_win);

  // Step 3: Derive the top edge of each triangle along the diagonal. No
  // triangle in top row.
  {
    const int16_t *d_t = d;

    if (height % 2) {
      int32x4_t deltas[(WIENER_WIN + 1) * 2] = { vdupq_n_s32(0) };
      int32x4_t deltas_tr[(WIENER_WIN + 1) * 2] = { vdupq_n_s32(0) };
      int16x8_t ds[WIENER_WIN * 2];

      load_s16_8x4(d_t, d_stride, &ds[0], &ds[2], &ds[4], &ds[6]);
      load_s16_8x4(d_t + width, d_stride, &ds[1], &ds[3], &ds[5], &ds[7]);
      d_t += 4 * d_stride;

      step3_win5_oneline_neon(&d_t, d_stride, width, height, ds, deltas);
      transpose_arrays_s32_8x8(deltas, deltas_tr);

      update_5_stats_neon(H + 0 * wiener_win * wiener_win2 + 0 * wiener_win,
                          deltas_tr[0], vgetq_lane_s32(deltas_tr[4], 0),
                          H + 1 * wiener_win * wiener_win2 + 1 * wiener_win);

      update_5_stats_neon(H + 1 * wiener_win * wiener_win2 + 1 * wiener_win,
                          deltas_tr[1], vgetq_lane_s32(deltas_tr[5], 0),
                          H + 2 * wiener_win * wiener_win2 + 2 * wiener_win);

      update_5_stats_neon(H + 2 * wiener_win * wiener_win2 + 2 * wiener_win,
                          deltas_tr[2], vgetq_lane_s32(deltas_tr[6], 0),
                          H + 3 * wiener_win * wiener_win2 + 3 * wiener_win);

      update_5_stats_neon(H + 3 * wiener_win * wiener_win2 + 3 * wiener_win,
                          deltas_tr[3], vgetq_lane_s32(deltas_tr[7], 0),
                          H + 4 * wiener_win * wiener_win2 + 4 * wiener_win);

    } else {
      int32x4_t deltas[WIENER_WIN_CHROMA * 2] = { vdupq_n_s32(0) };
      int16x8_t ds[WIENER_WIN_CHROMA * 2];

      ds[0] = load_unaligned_s16_4x2(d_t + 0 * d_stride, width);
      ds[1] = load_unaligned_s16_4x2(d_t + 1 * d_stride, width);
      ds[2] = load_unaligned_s16_4x2(d_t + 2 * d_stride, width);
      ds[3] = load_unaligned_s16_4x2(d_t + 3 * d_stride, width);

      step3_win5_neon(d_t + 4 * d_stride, d_stride, width, height, ds, deltas);

      transpose_elems_inplace_s32_4x4(&deltas[0], &deltas[1], &deltas[2],
                                      &deltas[3]);

      update_5_stats_neon(H + 0 * wiener_win * wiener_win2 + 0 * wiener_win,
                          deltas[0], vgetq_lane_s32(deltas[4], 0),
                          H + 1 * wiener_win * wiener_win2 + 1 * wiener_win);

      update_5_stats_neon(H + 1 * wiener_win * wiener_win2 + 1 * wiener_win,
                          deltas[1], vgetq_lane_s32(deltas[4], 1),
                          H + 2 * wiener_win * wiener_win2 + 2 * wiener_win);

      update_5_stats_neon(H + 2 * wiener_win * wiener_win2 + 2 * wiener_win,
                          deltas[2], vgetq_lane_s32(deltas[4], 2),
                          H + 3 * wiener_win * wiener_win2 + 3 * wiener_win);

      update_5_stats_neon(H + 3 * wiener_win * wiener_win2 + 3 * wiener_win,
                          deltas[3], vgetq_lane_s32(deltas[4], 3),
                          H + 4 * wiener_win * wiener_win2 + 4 * wiener_win);
    }
  }

  // Step 4: Derive the top and left edge of each square. No square in top and
  // bottom row.

  {
    y = h8;

    int16x4_t d_s[12];
    int16x4_t d_e[12];
    const int16_t *d_t = d;
    int16x4_t zeros = vdup_n_s16(0);
    load_s16_4x4(d_t, d_stride, &d_s[0], &d_s[1], &d_s[2], &d_s[3]);
    load_s16_4x4(d_t + width, d_stride, &d_e[0], &d_e[1], &d_e[2], &d_e[3]);
    int32x4_t deltas[6][18] = { { vdupq_n_s32(0) }, { vdupq_n_s32(0) } };

    while (y >= 8) {
      load_s16_4x8(d_t + 4 * d_stride, d_stride, &d_s[4], &d_s[5], &d_s[6],
                   &d_s[7], &d_s[8], &d_s[9], &d_s[10], &d_s[11]);
      load_s16_4x8(d_t + width + 4 * d_stride, d_stride, &d_e[4], &d_e[5],
                   &d_e[6], &d_e[7], &d_e[8], &d_e[9], &d_e[10], &d_e[11]);

      int16x8_t s_tr[8], e_tr[8];
      transpose_elems_s16_4x8(d_s[0], d_s[1], d_s[2], d_s[3], d_s[4], d_s[5],
                              d_s[6], d_s[7], &s_tr[0], &s_tr[1], &s_tr[2],
                              &s_tr[3]);
      transpose_elems_s16_4x8(d_s[8], d_s[9], d_s[10], d_s[11], zeros, zeros,
                              zeros, zeros, &s_tr[4], &s_tr[5], &s_tr[6],
                              &s_tr[7]);

      transpose_elems_s16_4x8(d_e[0], d_e[1], d_e[2], d_e[3], d_e[4], d_e[5],
                              d_e[6], d_e[7], &e_tr[0], &e_tr[1], &e_tr[2],
                              &e_tr[3]);
      transpose_elems_s16_4x8(d_e[8], d_e[9], d_e[10], d_e[11], zeros, zeros,
                              zeros, zeros, &e_tr[4], &e_tr[5], &e_tr[6],
                              &e_tr[7]);

      int16x8_t start_col0[5], start_col1[5], start_col2[5], start_col3[5];
      start_col0[0] = s_tr[0];
      start_col0[1] = vextq_s16(s_tr[0], s_tr[4], 1);
      start_col0[2] = vextq_s16(s_tr[0], s_tr[4], 2);
      start_col0[3] = vextq_s16(s_tr[0], s_tr[4], 3);
      start_col0[4] = vextq_s16(s_tr[0], s_tr[4], 4);

      start_col1[0] = s_tr[1];
      start_col1[1] = vextq_s16(s_tr[1], s_tr[5], 1);
      start_col1[2] = vextq_s16(s_tr[1], s_tr[5], 2);
      start_col1[3] = vextq_s16(s_tr[1], s_tr[5], 3);
      start_col1[4] = vextq_s16(s_tr[1], s_tr[5], 4);

      start_col2[0] = s_tr[2];
      start_col2[1] = vextq_s16(s_tr[2], s_tr[6], 1);
      start_col2[2] = vextq_s16(s_tr[2], s_tr[6], 2);
      start_col2[3] = vextq_s16(s_tr[2], s_tr[6], 3);
      start_col2[4] = vextq_s16(s_tr[2], s_tr[6], 4);

      start_col3[0] = s_tr[3];
      start_col3[1] = vextq_s16(s_tr[3], s_tr[7], 1);
      start_col3[2] = vextq_s16(s_tr[3], s_tr[7], 2);
      start_col3[3] = vextq_s16(s_tr[3], s_tr[7], 3);
      start_col3[4] = vextq_s16(s_tr[3], s_tr[7], 4);

      // i = 1, j = 2;
      sub_deltas_step4(start_col0, start_col1, deltas[0]);

      // i = 1, j = 3;
      sub_deltas_step4(start_col0, start_col2, deltas[1]);

      // i = 1, j = 4
      sub_deltas_step4(start_col0, start_col3, deltas[2]);

      // i = 2, j =3
      sub_deltas_step4(start_col1, start_col2, deltas[3]);

      // i = 2, j = 4
      sub_deltas_step4(start_col1, start_col3, deltas[4]);

      // i = 3, j = 4
      sub_deltas_step4(start_col2, start_col3, deltas[5]);

      int16x8_t end_col0[5], end_col1[5], end_col2[5], end_col3[5];
      end_col0[0] = e_tr[0];
      end_col0[1] = vextq_s16(e_tr[0], e_tr[4], 1);
      end_col0[2] = vextq_s16(e_tr[0], e_tr[4], 2);
      end_col0[3] = vextq_s16(e_tr[0], e_tr[4], 3);
      end_col0[4] = vextq_s16(e_tr[0], e_tr[4], 4);

      end_col1[0] = e_tr[1];
      end_col1[1] = vextq_s16(e_tr[1], e_tr[5], 1);
      end_col1[2] = vextq_s16(e_tr[1], e_tr[5], 2);
      end_col1[3] = vextq_s16(e_tr[1], e_tr[5], 3);
      end_col1[4] = vextq_s16(e_tr[1], e_tr[5], 4);

      end_col2[0] = e_tr[2];
      end_col2[1] = vextq_s16(e_tr[2], e_tr[6], 1);
      end_col2[2] = vextq_s16(e_tr[2], e_tr[6], 2);
      end_col2[3] = vextq_s16(e_tr[2], e_tr[6], 3);
      end_col2[4] = vextq_s16(e_tr[2], e_tr[6], 4);

      end_col3[0] = e_tr[3];
      end_col3[1] = vextq_s16(e_tr[3], e_tr[7], 1);
      end_col3[2] = vextq_s16(e_tr[3], e_tr[7], 2);
      end_col3[3] = vextq_s16(e_tr[3], e_tr[7], 3);
      end_col3[4] = vextq_s16(e_tr[3], e_tr[7], 4);

      // i = 1, j = 2;
      add_deltas_step4(end_col0, end_col1, deltas[0]);

      // i = 1, j = 3;
      add_deltas_step4(end_col0, end_col2, deltas[1]);

      // i = 1, j = 4
      add_deltas_step4(end_col0, end_col3, deltas[2]);

      // i = 2, j =3
      add_deltas_step4(end_col1, end_col2, deltas[3]);

      // i = 2, j = 4
      add_deltas_step4(end_col1, end_col3, deltas[4]);

      // i = 3, j = 4
      add_deltas_step4(end_col2, end_col3, deltas[5]);

      d_s[0] = d_s[8];
      d_s[1] = d_s[9];
      d_s[2] = d_s[10];
      d_s[3] = d_s[11];
      d_e[0] = d_e[8];
      d_e[1] = d_e[9];
      d_e[2] = d_e[10];
      d_e[3] = d_e[11];

      d_t += 8 * d_stride;
      y -= 8;
    }

    if (h8 != height) {
      const int16x8_t mask_h = vld1q_s16(&mask_16bit[16] - (height % 8));

      load_s16_4x8(d_t + 4 * d_stride, d_stride, &d_s[4], &d_s[5], &d_s[6],
                   &d_s[7], &d_s[8], &d_s[9], &d_s[10], &d_s[11]);
      load_s16_4x8(d_t + width + 4 * d_stride, d_stride, &d_e[4], &d_e[5],
                   &d_e[6], &d_e[7], &d_e[8], &d_e[9], &d_e[10], &d_e[11]);
      int16x8_t s_tr[8], e_tr[8];
      transpose_elems_s16_4x8(d_s[0], d_s[1], d_s[2], d_s[3], d_s[4], d_s[5],
                              d_s[6], d_s[7], &s_tr[0], &s_tr[1], &s_tr[2],
                              &s_tr[3]);
      transpose_elems_s16_4x8(d_s[8], d_s[9], d_s[10], d_s[11], zeros, zeros,
                              zeros, zeros, &s_tr[4], &s_tr[5], &s_tr[6],
                              &s_tr[7]);
      transpose_elems_s16_4x8(d_e[0], d_e[1], d_e[2], d_e[3], d_e[4], d_e[5],
                              d_e[6], d_e[7], &e_tr[0], &e_tr[1], &e_tr[2],
                              &e_tr[3]);
      transpose_elems_s16_4x8(d_e[8], d_e[9], d_e[10], d_e[11], zeros, zeros,
                              zeros, zeros, &e_tr[4], &e_tr[5], &e_tr[6],
                              &e_tr[7]);

      int16x8_t start_col0[5], start_col1[5], start_col2[5], start_col3[5];
      start_col0[0] = vandq_s16(s_tr[0], mask_h);
      start_col0[1] = vandq_s16(vextq_s16(s_tr[0], s_tr[4], 1), mask_h);
      start_col0[2] = vandq_s16(vextq_s16(s_tr[0], s_tr[4], 2), mask_h);
      start_col0[3] = vandq_s16(vextq_s16(s_tr[0], s_tr[4], 3), mask_h);
      start_col0[4] = vandq_s16(vextq_s16(s_tr[0], s_tr[4], 4), mask_h);

      start_col1[0] = vandq_s16(s_tr[1], mask_h);
      start_col1[1] = vandq_s16(vextq_s16(s_tr[1], s_tr[5], 1), mask_h);
      start_col1[2] = vandq_s16(vextq_s16(s_tr[1], s_tr[5], 2), mask_h);
      start_col1[3] = vandq_s16(vextq_s16(s_tr[1], s_tr[5], 3), mask_h);
      start_col1[4] = vandq_s16(vextq_s16(s_tr[1], s_tr[5], 4), mask_h);

      start_col2[0] = vandq_s16(s_tr[2], mask_h);
      start_col2[1] = vandq_s16(vextq_s16(s_tr[2], s_tr[6], 1), mask_h);
      start_col2[2] = vandq_s16(vextq_s16(s_tr[2], s_tr[6], 2), mask_h);
      start_col2[3] = vandq_s16(vextq_s16(s_tr[2], s_tr[6], 3), mask_h);
      start_col2[4] = vandq_s16(vextq_s16(s_tr[2], s_tr[6], 4), mask_h);

      start_col3[0] = vandq_s16(s_tr[3], mask_h);
      start_col3[1] = vandq_s16(vextq_s16(s_tr[3], s_tr[7], 1), mask_h);
      start_col3[2] = vandq_s16(vextq_s16(s_tr[3], s_tr[7], 2), mask_h);
      start_col3[3] = vandq_s16(vextq_s16(s_tr[3], s_tr[7], 3), mask_h);
      start_col3[4] = vandq_s16(vextq_s16(s_tr[3], s_tr[7], 4), mask_h);

      // i = 1, j = 2;
      sub_deltas_step4(start_col0, start_col1, deltas[0]);

      // i = 1, j = 3;
      sub_deltas_step4(start_col0, start_col2, deltas[1]);

      // i = 1, j = 4
      sub_deltas_step4(start_col0, start_col3, deltas[2]);

      // i = 2, j = 3
      sub_deltas_step4(start_col1, start_col2, deltas[3]);

      // i = 2, j = 4
      sub_deltas_step4(start_col1, start_col3, deltas[4]);

      // i = 3, j = 4
      sub_deltas_step4(start_col2, start_col3, deltas[5]);

      int16x8_t end_col0[5], end_col1[5], end_col2[5], end_col3[5];
      end_col0[0] = vandq_s16(e_tr[0], mask_h);
      end_col0[1] = vandq_s16(vextq_s16(e_tr[0], e_tr[4], 1), mask_h);
      end_col0[2] = vandq_s16(vextq_s16(e_tr[0], e_tr[4], 2), mask_h);
      end_col0[3] = vandq_s16(vextq_s16(e_tr[0], e_tr[4], 3), mask_h);
      end_col0[4] = vandq_s16(vextq_s16(e_tr[0], e_tr[4], 4), mask_h);

      end_col1[0] = vandq_s16(e_tr[1], mask_h);
      end_col1[1] = vandq_s16(vextq_s16(e_tr[1], e_tr[5], 1), mask_h);
      end_col1[2] = vandq_s16(vextq_s16(e_tr[1], e_tr[5], 2), mask_h);
      end_col1[3] = vandq_s16(vextq_s16(e_tr[1], e_tr[5], 3), mask_h);
      end_col1[4] = vandq_s16(vextq_s16(e_tr[1], e_tr[5], 4), mask_h);

      end_col2[0] = vandq_s16(e_tr[2], mask_h);
      end_col2[1] = vandq_s16(vextq_s16(e_tr[2], e_tr[6], 1), mask_h);
      end_col2[2] = vandq_s16(vextq_s16(e_tr[2], e_tr[6], 2), mask_h);
      end_col2[3] = vandq_s16(vextq_s16(e_tr[2], e_tr[6], 3), mask_h);
      end_col2[4] = vandq_s16(vextq_s16(e_tr[2], e_tr[6], 4), mask_h);

      end_col3[0] = vandq_s16(e_tr[3], mask_h);
      end_col3[1] = vandq_s16(vextq_s16(e_tr[3], e_tr[7], 1), mask_h);
      end_col3[2] = vandq_s16(vextq_s16(e_tr[3], e_tr[7], 2), mask_h);
      end_col3[3] = vandq_s16(vextq_s16(e_tr[3], e_tr[7], 3), mask_h);
      end_col3[4] = vandq_s16(vextq_s16(e_tr[3], e_tr[7], 4), mask_h);

      // i = 1, j = 2;
      add_deltas_step4(end_col0, end_col1, deltas[0]);

      // i = 1, j = 3;
      add_deltas_step4(end_col0, end_col2, deltas[1]);

      // i = 1, j = 4
      add_deltas_step4(end_col0, end_col3, deltas[2]);

      // i = 2, j =3
      add_deltas_step4(end_col1, end_col2, deltas[3]);

      // i = 2, j = 4
      add_deltas_step4(end_col1, end_col3, deltas[4]);

      // i = 3, j = 4
      add_deltas_step4(end_col2, end_col3, deltas[5]);
    }

    int32x4_t delta[6][2];
    int32_t single_delta[6];

    delta[0][0] = horizontal_add_4d_s32x4(&deltas[0][0]);
    delta[1][0] = horizontal_add_4d_s32x4(&deltas[1][0]);
    delta[2][0] = horizontal_add_4d_s32x4(&deltas[2][0]);
    delta[3][0] = horizontal_add_4d_s32x4(&deltas[3][0]);
    delta[4][0] = horizontal_add_4d_s32x4(&deltas[4][0]);
    delta[5][0] = horizontal_add_4d_s32x4(&deltas[5][0]);

    delta[0][1] = horizontal_add_4d_s32x4(&deltas[0][5]);
    delta[1][1] = horizontal_add_4d_s32x4(&deltas[1][5]);
    delta[2][1] = horizontal_add_4d_s32x4(&deltas[2][5]);
    delta[3][1] = horizontal_add_4d_s32x4(&deltas[3][5]);
    delta[4][1] = horizontal_add_4d_s32x4(&deltas[4][5]);
    delta[5][1] = horizontal_add_4d_s32x4(&deltas[5][5]);

    single_delta[0] = horizontal_add_s32x4(deltas[0][4]);
    single_delta[1] = horizontal_add_s32x4(deltas[1][4]);
    single_delta[2] = horizontal_add_s32x4(deltas[2][4]);
    single_delta[3] = horizontal_add_s32x4(deltas[3][4]);
    single_delta[4] = horizontal_add_s32x4(deltas[4][4]);
    single_delta[5] = horizontal_add_s32x4(deltas[5][4]);

    int idx = 0;
    for (i = 1; i < wiener_win - 1; i++) {
      for (j = i + 1; j < wiener_win; j++) {
        update_4_stats_neon(
            H + (i - 1) * wiener_win * wiener_win2 + (j - 1) * wiener_win,
            delta[idx][0], H + i * wiener_win * wiener_win2 + j * wiener_win);
        H[i * wiener_win * wiener_win2 + j * wiener_win + 4] =
            H[(i - 1) * wiener_win * wiener_win2 + (j - 1) * wiener_win + 4] +
            single_delta[idx];

        H[(i * wiener_win + 1) * wiener_win2 + j * wiener_win] =
            H[((i - 1) * wiener_win + 1) * wiener_win2 + (j - 1) * wiener_win] +
            vgetq_lane_s32(delta[idx][1], 0);
        H[(i * wiener_win + 2) * wiener_win2 + j * wiener_win] =
            H[((i - 1) * wiener_win + 2) * wiener_win2 + (j - 1) * wiener_win] +
            vgetq_lane_s32(delta[idx][1], 1);
        H[(i * wiener_win + 3) * wiener_win2 + j * wiener_win] =
            H[((i - 1) * wiener_win + 3) * wiener_win2 + (j - 1) * wiener_win] +
            vgetq_lane_s32(delta[idx][1], 2);
        H[(i * wiener_win + 4) * wiener_win2 + j * wiener_win] =
            H[((i - 1) * wiener_win + 4) * wiener_win2 + (j - 1) * wiener_win] +
            vgetq_lane_s32(delta[idx][1], 3);

        idx++;
      }
    }
  }

  // Step 5: Derive other points of each square. No square in bottom row.
  i = 0;
  do {
    const int16_t *const di = d + i;

    j = i + 1;
    do {
      const int16_t *const dj = d + j;
      int32x4_t deltas[WIENER_WIN_CHROMA - 1][WIENER_WIN_CHROMA - 1] = {
        { vdupq_n_s32(0) }, { vdupq_n_s32(0) }
      };
      int16x8_t d_is[WIN_CHROMA], d_ie[WIN_CHROMA];
      int16x8_t d_js[WIN_CHROMA], d_je[WIN_CHROMA];

      x = 0;
      while (x < w16) {
        load_square_win5_neon(di + x, dj + x, d_stride, height, d_is, d_ie,
                              d_js, d_je);
        derive_square_win5_neon(d_is, d_ie, d_js, d_je, deltas);
        x += 16;
      }

      if (w16 != width) {
        load_square_win5_neon(di + x, dj + x, d_stride, height, d_is, d_ie,
                              d_js, d_je);
        d_is[0] = vandq_s16(d_is[0], mask[0]);
        d_is[1] = vandq_s16(d_is[1], mask[1]);
        d_is[2] = vandq_s16(d_is[2], mask[0]);
        d_is[3] = vandq_s16(d_is[3], mask[1]);
        d_is[4] = vandq_s16(d_is[4], mask[0]);
        d_is[5] = vandq_s16(d_is[5], mask[1]);
        d_is[6] = vandq_s16(d_is[6], mask[0]);
        d_is[7] = vandq_s16(d_is[7], mask[1]);
        d_ie[0] = vandq_s16(d_ie[0], mask[0]);
        d_ie[1] = vandq_s16(d_ie[1], mask[1]);
        d_ie[2] = vandq_s16(d_ie[2], mask[0]);
        d_ie[3] = vandq_s16(d_ie[3], mask[1]);
        d_ie[4] = vandq_s16(d_ie[4], mask[0]);
        d_ie[5] = vandq_s16(d_ie[5], mask[1]);
        d_ie[6] = vandq_s16(d_ie[6], mask[0]);
        d_ie[7] = vandq_s16(d_ie[7], mask[1]);
        derive_square_win5_neon(d_is, d_ie, d_js, d_je, deltas);
      }

      hadd_update_4_stats_neon(
          H + (i * wiener_win + 0) * wiener_win2 + j * wiener_win, deltas[0],
          H + (i * wiener_win + 1) * wiener_win2 + j * wiener_win + 1);
      hadd_update_4_stats_neon(
          H + (i * wiener_win + 1) * wiener_win2 + j * wiener_win, deltas[1],
          H + (i * wiener_win + 2) * wiener_win2 + j * wiener_win + 1);
      hadd_update_4_stats_neon(
          H + (i * wiener_win + 2) * wiener_win2 + j * wiener_win, deltas[2],
          H + (i * wiener_win + 3) * wiener_win2 + j * wiener_win + 1);
      hadd_update_4_stats_neon(
          H + (i * wiener_win + 3) * wiener_win2 + j * wiener_win, deltas[3],
          H + (i * wiener_win + 4) * wiener_win2 + j * wiener_win + 1);
    } while (++j < wiener_win);
  } while (++i < wiener_win - 1);

  // Step 6: Derive other points of each upper triangle along the diagonal.
  i = 0;
  do {
    const int16_t *const di = d + i;
    int32x4_t deltas[WIENER_WIN_CHROMA * 2 + 1] = { vdupq_n_s32(0) };
    int16x8_t d_is[WIN_CHROMA], d_ie[WIN_CHROMA];

    x = 0;
    while (x < w16) {
      load_triangle_win5_neon(di + x, d_stride, height, d_is, d_ie);
      derive_triangle_win5_neon(d_is, d_ie, deltas);
      x += 16;
    }

    if (w16 != width) {
      load_triangle_win5_neon(di + x, d_stride, height, d_is, d_ie);
      d_is[0] = vandq_s16(d_is[0], mask[0]);
      d_is[1] = vandq_s16(d_is[1], mask[1]);
      d_is[2] = vandq_s16(d_is[2], mask[0]);
      d_is[3] = vandq_s16(d_is[3], mask[1]);
      d_is[4] = vandq_s16(d_is[4], mask[0]);
      d_is[5] = vandq_s16(d_is[5], mask[1]);
      d_is[6] = vandq_s16(d_is[6], mask[0]);
      d_is[7] = vandq_s16(d_is[7], mask[1]);
      d_ie[0] = vandq_s16(d_ie[0], mask[0]);
      d_ie[1] = vandq_s16(d_ie[1], mask[1]);
      d_ie[2] = vandq_s16(d_ie[2], mask[0]);
      d_ie[3] = vandq_s16(d_ie[3], mask[1]);
      d_ie[4] = vandq_s16(d_ie[4], mask[0]);
      d_ie[5] = vandq_s16(d_ie[5], mask[1]);
      d_ie[6] = vandq_s16(d_ie[6], mask[0]);
      d_ie[7] = vandq_s16(d_ie[7], mask[1]);
      derive_triangle_win5_neon(d_is, d_ie, deltas);
    }

    // Row 1: 4 points
    hadd_update_4_stats_neon(
        H + (i * wiener_win + 0) * wiener_win2 + i * wiener_win, deltas,
        H + (i * wiener_win + 1) * wiener_win2 + i * wiener_win + 1);

    // Row 2: 3 points
    int32x4_t deltas45 = horizontal_add_2d_s32(deltas[4], deltas[5]);
    int32x4_t deltas78 = horizontal_add_2d_s32(deltas[7], deltas[8]);

    int64x2_t deltas45_s64 = vpaddlq_s32(deltas45);
    int64x2_t deltas78_s64 = vpaddlq_s32(deltas78);

    int64x2_t src =
        vld1q_s64(H + (i * wiener_win + 1) * wiener_win2 + i * wiener_win + 1);
    int64x2_t dst = vaddq_s64(src, deltas45_s64);
    vst1q_s64(H + (i * wiener_win + 2) * wiener_win2 + i * wiener_win + 2, dst);

    int32x4_t delta69 = horizontal_add_2d_s32(deltas[6], deltas[9]);
    int64x2_t delta69_s64 = vpaddlq_s32(delta69);
    H[(i * wiener_win + 2) * wiener_win2 + i * wiener_win + 4] =
        H[(i * wiener_win + 1) * wiener_win2 + i * wiener_win + 3] +
        vgetq_lane_s64(delta69_s64, 0);

    // Row 3: 2 points
    vst1q_s64(H + (i * wiener_win + 3) * wiener_win2 + i * wiener_win + 3,
              vaddq_s64(dst, deltas78_s64));

    // Row 4: 1 point
    H[(i * wiener_win + 4) * wiener_win2 + i * wiener_win + 4] =
        H[(i * wiener_win + 3) * wiener_win2 + i * wiener_win + 3] +
        vgetq_lane_s64(delta69_s64, 1);
  } while (++i < wiener_win);
}

static inline void compute_stats_win7_neon(
    const int16_t *const d, const int32_t d_stride, const int16_t *const s,
    const int32_t s_stride, const int32_t width, const int32_t height,
    int64_t *const M, int64_t *const H) {
  const int32_t wiener_win = WIENER_WIN;
  const int32_t wiener_win2 = wiener_win * wiener_win;
  const int32_t w16 = width & ~15;
  const int32_t h8 = height & ~7;
  int16x8_t mask[2];
  mask[0] = vld1q_s16(&(mask_16bit[16]) - width % 16);
  mask[1] = vld1q_s16(&(mask_16bit[16]) - width % 16 + 8);
  const int bit_depth = 8;
  int32_t i, j, x, y;

  const int32_t num_bit_left =
      32 - 1 /* sign */ - 2 * bit_depth /* energy */ + 2 /* SIMD */;
  const int32_t h_allowed =
      (1 << num_bit_left) / (w16 + ((w16 != width) ? 16 : 0));

  // Step 1: Calculate the top edge of the whole matrix, i.e., the top
  // edge of each triangle and square on the top row.
  j = 0;
  do {
    const int16_t *s_t = s;
    const int16_t *d_t = d;
    int32_t height_t = 0;
    int64x2_t sum_m[WIENER_WIN] = { vdupq_n_s64(0) };
    int64x2_t sum_h[WIENER_WIN] = { vdupq_n_s64(0) };
    int16x8_t src[2], dgd[2];

    do {
      const int32_t h_t =
          ((height - height_t) < h_allowed) ? (height - height_t) : h_allowed;
      int32x4_t row_m[WIENER_WIN * 2] = { vdupq_n_s32(0) };
      int32x4_t row_h[WIENER_WIN * 2] = { vdupq_n_s32(0) };

      y = h_t;
      do {
        x = 0;
        while (x < w16) {
          src[0] = vld1q_s16(s_t + x);
          src[1] = vld1q_s16(s_t + x + 8);
          dgd[0] = vld1q_s16(d_t + x);
          dgd[1] = vld1q_s16(d_t + x + 8);
          stats_top_win7_neon(src, dgd, d_t + j + x, d_stride, row_m, row_h);
          x += 16;
        }

        if (w16 != width) {
          src[0] = vld1q_s16(s_t + w16);
          src[1] = vld1q_s16(s_t + w16 + 8);
          dgd[0] = vld1q_s16(d_t + w16);
          dgd[1] = vld1q_s16(d_t + w16 + 8);
          src[0] = vandq_s16(src[0], mask[0]);
          src[1] = vandq_s16(src[1], mask[1]);
          dgd[0] = vandq_s16(dgd[0], mask[0]);
          dgd[1] = vandq_s16(dgd[1], mask[1]);
          stats_top_win7_neon(src, dgd, d_t + j + w16, d_stride, row_m, row_h);
        }

        s_t += s_stride;
        d_t += d_stride;
      } while (--y);

      sum_m[0] = vpadalq_s32(sum_m[0], row_m[0]);
      sum_m[1] = vpadalq_s32(sum_m[1], row_m[1]);
      sum_m[2] = vpadalq_s32(sum_m[2], row_m[2]);
      sum_m[3] = vpadalq_s32(sum_m[3], row_m[3]);
      sum_m[4] = vpadalq_s32(sum_m[4], row_m[4]);
      sum_m[5] = vpadalq_s32(sum_m[5], row_m[5]);
      sum_m[6] = vpadalq_s32(sum_m[6], row_m[6]);

      sum_h[0] = vpadalq_s32(sum_h[0], row_h[0]);
      sum_h[1] = vpadalq_s32(sum_h[1], row_h[1]);
      sum_h[2] = vpadalq_s32(sum_h[2], row_h[2]);
      sum_h[3] = vpadalq_s32(sum_h[3], row_h[3]);
      sum_h[4] = vpadalq_s32(sum_h[4], row_h[4]);
      sum_h[5] = vpadalq_s32(sum_h[5], row_h[5]);
      sum_h[6] = vpadalq_s32(sum_h[6], row_h[6]);

      height_t += h_t;
    } while (height_t < height);

#if AOM_ARCH_AARCH64
    vst1q_s64(M + wiener_win * j + 0, vpaddq_s64(sum_m[0], sum_m[1]));
    vst1q_s64(M + wiener_win * j + 2, vpaddq_s64(sum_m[2], sum_m[3]));
    vst1q_s64(M + wiener_win * j + 4, vpaddq_s64(sum_m[4], sum_m[5]));
    M[wiener_win * j + 6] = vaddvq_s64(sum_m[6]);

    vst1q_s64(H + wiener_win * j + 0, vpaddq_s64(sum_h[0], sum_h[1]));
    vst1q_s64(H + wiener_win * j + 2, vpaddq_s64(sum_h[2], sum_h[3]));
    vst1q_s64(H + wiener_win * j + 4, vpaddq_s64(sum_h[4], sum_h[5]));
    H[wiener_win * j + 6] = vaddvq_s64(sum_h[6]);
#else
    M[wiener_win * j + 0] = horizontal_add_s64x2(sum_m[0]);
    M[wiener_win * j + 1] = horizontal_add_s64x2(sum_m[1]);
    M[wiener_win * j + 2] = horizontal_add_s64x2(sum_m[2]);
    M[wiener_win * j + 3] = horizontal_add_s64x2(sum_m[3]);
    M[wiener_win * j + 4] = horizontal_add_s64x2(sum_m[4]);
    M[wiener_win * j + 5] = horizontal_add_s64x2(sum_m[5]);
    M[wiener_win * j + 6] = horizontal_add_s64x2(sum_m[6]);

    H[wiener_win * j + 0] = horizontal_add_s64x2(sum_h[0]);
    H[wiener_win * j + 1] = horizontal_add_s64x2(sum_h[1]);
    H[wiener_win * j + 2] = horizontal_add_s64x2(sum_h[2]);
    H[wiener_win * j + 3] = horizontal_add_s64x2(sum_h[3]);
    H[wiener_win * j + 4] = horizontal_add_s64x2(sum_h[4]);
    H[wiener_win * j + 5] = horizontal_add_s64x2(sum_h[5]);
    H[wiener_win * j + 6] = horizontal_add_s64x2(sum_h[6]);
#endif  // AOM_ARCH_AARCH64
  } while (++j < wiener_win);

  // Step 2: Calculate the left edge of each square on the top row.
  j = 1;
  do {
    const int16_t *d_t = d;
    int32_t height_t = 0;
    int64x2_t sum_h[WIENER_WIN - 1] = { vdupq_n_s64(0) };
    int16x8_t dgd[2];

    do {
      const int32_t h_t =
          ((height - height_t) < h_allowed) ? (height - height_t) : h_allowed;
      int32x4_t row_h[WIENER_WIN - 1] = { vdupq_n_s32(0) };

      y = h_t;
      do {
        x = 0;
        while (x < w16) {
          dgd[0] = vld1q_s16(d_t + j + x + 0);
          dgd[1] = vld1q_s16(d_t + j + x + 8);
          stats_left_win7_neon(dgd, d_t + x, d_stride, row_h);
          x += 16;
        }

        if (w16 != width) {
          dgd[0] = vld1q_s16(d_t + j + x + 0);
          dgd[1] = vld1q_s16(d_t + j + x + 8);
          dgd[0] = vandq_s16(dgd[0], mask[0]);
          dgd[1] = vandq_s16(dgd[1], mask[1]);
          stats_left_win7_neon(dgd, d_t + x, d_stride, row_h);
        }

        d_t += d_stride;
      } while (--y);

      sum_h[0] = vpadalq_s32(sum_h[0], row_h[0]);
      sum_h[1] = vpadalq_s32(sum_h[1], row_h[1]);
      sum_h[2] = vpadalq_s32(sum_h[2], row_h[2]);
      sum_h[3] = vpadalq_s32(sum_h[3], row_h[3]);
      sum_h[4] = vpadalq_s32(sum_h[4], row_h[4]);
      sum_h[5] = vpadalq_s32(sum_h[5], row_h[5]);

      height_t += h_t;
    } while (height_t < height);

#if AOM_ARCH_AARCH64
    int64x2_t sum_h0 = vpaddq_s64(sum_h[0], sum_h[1]);
    int64x2_t sum_h2 = vpaddq_s64(sum_h[2], sum_h[3]);
    int64x2_t sum_h4 = vpaddq_s64(sum_h[4], sum_h[5]);
    vst1_s64(&H[1 * wiener_win2 + j * wiener_win], vget_low_s64(sum_h0));
    vst1_s64(&H[2 * wiener_win2 + j * wiener_win], vget_high_s64(sum_h0));
    vst1_s64(&H[3 * wiener_win2 + j * wiener_win], vget_low_s64(sum_h2));
    vst1_s64(&H[4 * wiener_win2 + j * wiener_win], vget_high_s64(sum_h2));
    vst1_s64(&H[5 * wiener_win2 + j * wiener_win], vget_low_s64(sum_h4));
    vst1_s64(&H[6 * wiener_win2 + j * wiener_win], vget_high_s64(sum_h4));
#else
    H[1 * wiener_win2 + j * wiener_win] = horizontal_add_s64x2(sum_h[0]);
    H[2 * wiener_win2 + j * wiener_win] = horizontal_add_s64x2(sum_h[1]);
    H[3 * wiener_win2 + j * wiener_win] = horizontal_add_s64x2(sum_h[2]);
    H[4 * wiener_win2 + j * wiener_win] = horizontal_add_s64x2(sum_h[3]);
    H[5 * wiener_win2 + j * wiener_win] = horizontal_add_s64x2(sum_h[4]);
    H[6 * wiener_win2 + j * wiener_win] = horizontal_add_s64x2(sum_h[5]);
#endif  // AOM_ARCH_AARCH64
  } while (++j < wiener_win);

  // Step 3: Derive the top edge of each triangle along the diagonal. No
  // triangle in top row.
  {
    const int16_t *d_t = d;
    // Pad to call transpose function.
    int32x4_t deltas[(WIENER_WIN + 1) * 2] = { vdupq_n_s32(0) };
    int32x4_t deltas_tr[(WIENER_WIN + 1) * 2] = { vdupq_n_s32(0) };
    int16x8_t ds[WIENER_WIN * 2];

    load_s16_8x6(d_t, d_stride, &ds[0], &ds[2], &ds[4], &ds[6], &ds[8],
                 &ds[10]);
    load_s16_8x6(d_t + width, d_stride, &ds[1], &ds[3], &ds[5], &ds[7], &ds[9],
                 &ds[11]);

    d_t += 6 * d_stride;

    step3_win7_neon(d_t, d_stride, width, height, ds, deltas);
    transpose_arrays_s32_8x8(deltas, deltas_tr);

    update_8_stats_neon(H + 0 * wiener_win * wiener_win2 + 0 * wiener_win,
                        deltas_tr[0], deltas_tr[4],
                        H + 1 * wiener_win * wiener_win2 + 1 * wiener_win);
    update_8_stats_neon(H + 1 * wiener_win * wiener_win2 + 1 * wiener_win,
                        deltas_tr[1], deltas_tr[5],
                        H + 2 * wiener_win * wiener_win2 + 2 * wiener_win);
    update_8_stats_neon(H + 2 * wiener_win * wiener_win2 + 2 * wiener_win,
                        deltas_tr[2], deltas_tr[6],
                        H + 3 * wiener_win * wiener_win2 + 3 * wiener_win);
    update_8_stats_neon(H + 3 * wiener_win * wiener_win2 + 3 * wiener_win,
                        deltas_tr[3], deltas_tr[7],
                        H + 4 * wiener_win * wiener_win2 + 4 * wiener_win);
    update_8_stats_neon(H + 4 * wiener_win * wiener_win2 + 4 * wiener_win,
                        deltas_tr[8], deltas_tr[12],
                        H + 5 * wiener_win * wiener_win2 + 5 * wiener_win);
    update_8_stats_neon(H + 5 * wiener_win * wiener_win2 + 5 * wiener_win,
                        deltas_tr[9], deltas_tr[13],
                        H + 6 * wiener_win * wiener_win2 + 6 * wiener_win);
  }

  // Step 4: Derive the top and left edge of each square. No square in top and
  // bottom row.

  i = 1;
  do {
    j = i + 1;
    do {
      const int16_t *di = d + i - 1;
      const int16_t *dj = d + j - 1;
      int32x4_t deltas[(2 * WIENER_WIN - 1) * 2] = { vdupq_n_s32(0) };
      int16x8_t dd[WIENER_WIN * 2], ds[WIENER_WIN * 2];

      dd[5] = vdupq_n_s16(0);  // Initialize to avoid warning.
      const int16_t dd0_values[] = { di[0 * d_stride],
                                     di[1 * d_stride],
                                     di[2 * d_stride],
                                     di[3 * d_stride],
                                     di[4 * d_stride],
                                     di[5 * d_stride],
                                     0,
                                     0 };
      dd[0] = vld1q_s16(dd0_values);
      const int16_t dd1_values[] = { di[0 * d_stride + width],
                                     di[1 * d_stride + width],
                                     di[2 * d_stride + width],
                                     di[3 * d_stride + width],
                                     di[4 * d_stride + width],
                                     di[5 * d_stride + width],
                                     0,
                                     0 };
      dd[1] = vld1q_s16(dd1_values);
      const int16_t ds0_values[] = { dj[0 * d_stride],
                                     dj[1 * d_stride],
                                     dj[2 * d_stride],
                                     dj[3 * d_stride],
                                     dj[4 * d_stride],
                                     dj[5 * d_stride],
                                     0,
                                     0 };
      ds[0] = vld1q_s16(ds0_values);
      int16_t ds1_values[] = { dj[0 * d_stride + width],
                               dj[1 * d_stride + width],
                               dj[2 * d_stride + width],
                               dj[3 * d_stride + width],
                               dj[4 * d_stride + width],
                               dj[5 * d_stride + width],
                               0,
                               0 };
      ds[1] = vld1q_s16(ds1_values);

      y = 0;
      while (y < h8) {
        // 00s 10s 20s 30s 40s 50s 60s 70s  00e 10e 20e 30e 40e 50e 60e 70e
        dd[0] = vsetq_lane_s16(di[6 * d_stride], dd[0], 6);
        dd[0] = vsetq_lane_s16(di[7 * d_stride], dd[0], 7);
        dd[1] = vsetq_lane_s16(di[6 * d_stride + width], dd[1], 6);
        dd[1] = vsetq_lane_s16(di[7 * d_stride + width], dd[1], 7);

        // 00s 10s 20s 30s 40s 50s 60s 70s  00e 10e 20e 30e 40e 50e 60e 70e
        // 01s 11s 21s 31s 41s 51s 61s 71s  01e 11e 21e 31e 41e 51e 61e 71e
        ds[0] = vsetq_lane_s16(dj[6 * d_stride], ds[0], 6);
        ds[0] = vsetq_lane_s16(dj[7 * d_stride], ds[0], 7);
        ds[1] = vsetq_lane_s16(dj[6 * d_stride + width], ds[1], 6);
        ds[1] = vsetq_lane_s16(dj[7 * d_stride + width], ds[1], 7);

        load_more_16_neon(di + 8 * d_stride, width, &dd[0], &dd[2]);
        load_more_16_neon(dj + 8 * d_stride, width, &ds[0], &ds[2]);
        load_more_16_neon(di + 9 * d_stride, width, &dd[2], &dd[4]);
        load_more_16_neon(dj + 9 * d_stride, width, &ds[2], &ds[4]);
        load_more_16_neon(di + 10 * d_stride, width, &dd[4], &dd[6]);
        load_more_16_neon(dj + 10 * d_stride, width, &ds[4], &ds[6]);
        load_more_16_neon(di + 11 * d_stride, width, &dd[6], &dd[8]);
        load_more_16_neon(dj + 11 * d_stride, width, &ds[6], &ds[8]);
        load_more_16_neon(di + 12 * d_stride, width, &dd[8], &dd[10]);
        load_more_16_neon(dj + 12 * d_stride, width, &ds[8], &ds[10]);
        load_more_16_neon(di + 13 * d_stride, width, &dd[10], &dd[12]);
        load_more_16_neon(dj + 13 * d_stride, width, &ds[10], &ds[12]);

        madd_neon(&deltas[0], dd[0], ds[0]);
        madd_neon(&deltas[1], dd[1], ds[1]);
        madd_neon(&deltas[2], dd[0], ds[2]);
        madd_neon(&deltas[3], dd[1], ds[3]);
        madd_neon(&deltas[4], dd[0], ds[4]);
        madd_neon(&deltas[5], dd[1], ds[5]);
        madd_neon(&deltas[6], dd[0], ds[6]);
        madd_neon(&deltas[7], dd[1], ds[7]);
        madd_neon(&deltas[8], dd[0], ds[8]);
        madd_neon(&deltas[9], dd[1], ds[9]);
        madd_neon(&deltas[10], dd[0], ds[10]);
        madd_neon(&deltas[11], dd[1], ds[11]);
        madd_neon(&deltas[12], dd[0], ds[12]);
        madd_neon(&deltas[13], dd[1], ds[13]);
        madd_neon(&deltas[14], dd[2], ds[0]);
        madd_neon(&deltas[15], dd[3], ds[1]);
        madd_neon(&deltas[16], dd[4], ds[0]);
        madd_neon(&deltas[17], dd[5], ds[1]);
        madd_neon(&deltas[18], dd[6], ds[0]);
        madd_neon(&deltas[19], dd[7], ds[1]);
        madd_neon(&deltas[20], dd[8], ds[0]);
        madd_neon(&deltas[21], dd[9], ds[1]);
        madd_neon(&deltas[22], dd[10], ds[0]);
        madd_neon(&deltas[23], dd[11], ds[1]);
        madd_neon(&deltas[24], dd[12], ds[0]);
        madd_neon(&deltas[25], dd[13], ds[1]);

        dd[0] = vextq_s16(dd[12], vdupq_n_s16(0), 2);
        dd[1] = vextq_s16(dd[13], vdupq_n_s16(0), 2);
        ds[0] = vextq_s16(ds[12], vdupq_n_s16(0), 2);
        ds[1] = vextq_s16(ds[13], vdupq_n_s16(0), 2);

        di += 8 * d_stride;
        dj += 8 * d_stride;
        y += 8;
      }

      deltas[0] = hadd_four_32_neon(deltas[0], deltas[2], deltas[4], deltas[6]);
      deltas[1] = hadd_four_32_neon(deltas[1], deltas[3], deltas[5], deltas[7]);
      deltas[2] =
          hadd_four_32_neon(deltas[8], deltas[10], deltas[12], deltas[12]);
      deltas[3] =
          hadd_four_32_neon(deltas[9], deltas[11], deltas[13], deltas[13]);
      deltas[4] =
          hadd_four_32_neon(deltas[14], deltas[16], deltas[18], deltas[20]);
      deltas[5] =
          hadd_four_32_neon(deltas[15], deltas[17], deltas[19], deltas[21]);
      deltas[6] =
          hadd_four_32_neon(deltas[22], deltas[24], deltas[22], deltas[24]);
      deltas[7] =
          hadd_four_32_neon(deltas[23], deltas[25], deltas[23], deltas[25]);
      deltas[0] = vsubq_s32(deltas[1], deltas[0]);
      deltas[1] = vsubq_s32(deltas[3], deltas[2]);
      deltas[2] = vsubq_s32(deltas[5], deltas[4]);
      deltas[3] = vsubq_s32(deltas[7], deltas[6]);

      if (h8 != height) {
        const int16_t ds0_vals[] = {
          dj[0 * d_stride], dj[0 * d_stride + width],
          dj[1 * d_stride], dj[1 * d_stride + width],
          dj[2 * d_stride], dj[2 * d_stride + width],
          dj[3 * d_stride], dj[3 * d_stride + width]
        };
        ds[0] = vld1q_s16(ds0_vals);

        ds[1] = vsetq_lane_s16(dj[4 * d_stride], ds[1], 0);
        ds[1] = vsetq_lane_s16(dj[4 * d_stride + width], ds[1], 1);
        ds[1] = vsetq_lane_s16(dj[5 * d_stride], ds[1], 2);
        ds[1] = vsetq_lane_s16(dj[5 * d_stride + width], ds[1], 3);
        const int16_t dd4_vals[] = {
          -di[1 * d_stride], di[1 * d_stride + width],
          -di[2 * d_stride], di[2 * d_stride + width],
          -di[3 * d_stride], di[3 * d_stride + width],
          -di[4 * d_stride], di[4 * d_stride + width]
        };
        dd[4] = vld1q_s16(dd4_vals);

        dd[5] = vsetq_lane_s16(-di[5 * d_stride], dd[5], 0);
        dd[5] = vsetq_lane_s16(di[5 * d_stride + width], dd[5], 1);
        do {
          dd[0] = vdupq_n_s16(-di[0 * d_stride]);
          dd[2] = dd[3] = vdupq_n_s16(di[0 * d_stride + width]);
          dd[0] = dd[1] = vzipq_s16(dd[0], dd[2]).val[0];

          ds[4] = vdupq_n_s16(dj[0 * d_stride]);
          ds[6] = ds[7] = vdupq_n_s16(dj[0 * d_stride + width]);
          ds[4] = ds[5] = vzipq_s16(ds[4], ds[6]).val[0];

          dd[5] = vsetq_lane_s16(-di[6 * d_stride], dd[5], 2);
          dd[5] = vsetq_lane_s16(di[6 * d_stride + width], dd[5], 3);
          ds[1] = vsetq_lane_s16(dj[6 * d_stride], ds[1], 4);
          ds[1] = vsetq_lane_s16(dj[6 * d_stride + width], ds[1], 5);

          madd_neon_pairwise(&deltas[0], dd[0], ds[0]);
          madd_neon_pairwise(&deltas[1], dd[1], ds[1]);
          madd_neon_pairwise(&deltas[2], dd[4], ds[4]);
          madd_neon_pairwise(&deltas[3], dd[5], ds[5]);

          int32_t tmp0 = vgetq_lane_s32(vreinterpretq_s32_s16(ds[0]), 0);
          ds[0] = vextq_s16(ds[0], ds[1], 2);
          ds[1] = vextq_s16(ds[1], ds[0], 2);
          ds[1] = vreinterpretq_s16_s32(
              vsetq_lane_s32(tmp0, vreinterpretq_s32_s16(ds[1]), 3));
          int32_t tmp1 = vgetq_lane_s32(vreinterpretq_s32_s16(dd[4]), 0);
          dd[4] = vextq_s16(dd[4], dd[5], 2);
          dd[5] = vextq_s16(dd[5], dd[4], 2);
          dd[5] = vreinterpretq_s16_s32(
              vsetq_lane_s32(tmp1, vreinterpretq_s32_s16(dd[5]), 3));
          di += d_stride;
          dj += d_stride;
        } while (++y < height);
      }

      // Writing one more element on the top edge of a square falls to
      // the next square in the same row or the first element in the next
      // row, which will just be overwritten later.
      update_8_stats_neon(
          H + (i - 1) * wiener_win * wiener_win2 + (j - 1) * wiener_win,
          deltas[0], deltas[1],
          H + i * wiener_win * wiener_win2 + j * wiener_win);

      H[(i * wiener_win + 1) * wiener_win2 + j * wiener_win] =
          H[((i - 1) * wiener_win + 1) * wiener_win2 + (j - 1) * wiener_win] +
          vgetq_lane_s32(deltas[2], 0);
      H[(i * wiener_win + 2) * wiener_win2 + j * wiener_win] =
          H[((i - 1) * wiener_win + 2) * wiener_win2 + (j - 1) * wiener_win] +
          vgetq_lane_s32(deltas[2], 1);
      H[(i * wiener_win + 3) * wiener_win2 + j * wiener_win] =
          H[((i - 1) * wiener_win + 3) * wiener_win2 + (j - 1) * wiener_win] +
          vgetq_lane_s32(deltas[2], 2);
      H[(i * wiener_win + 4) * wiener_win2 + j * wiener_win] =
          H[((i - 1) * wiener_win + 4) * wiener_win2 + (j - 1) * wiener_win] +
          vgetq_lane_s32(deltas[2], 3);
      H[(i * wiener_win + 5) * wiener_win2 + j * wiener_win] =
          H[((i - 1) * wiener_win + 5) * wiener_win2 + (j - 1) * wiener_win] +
          vgetq_lane_s32(deltas[3], 0);
      H[(i * wiener_win + 6) * wiener_win2 + j * wiener_win] =
          H[((i - 1) * wiener_win + 6) * wiener_win2 + (j - 1) * wiener_win] +
          vgetq_lane_s32(deltas[3], 1);
    } while (++j < wiener_win);
  } while (++i < wiener_win - 1);

  // Step 5: Derive other points of each square. No square in bottom row.
  i = 0;
  do {
    const int16_t *const di = d + i;

    j = i + 1;
    do {
      const int16_t *const dj = d + j;
      int32x4_t deltas[WIENER_WIN - 1][WIN_7] = { { vdupq_n_s32(0) },
                                                  { vdupq_n_s32(0) } };
      int16x8_t d_is[WIN_7];
      int16x8_t d_ie[WIN_7];
      int16x8_t d_js[WIN_7];
      int16x8_t d_je[WIN_7];

      x = 0;
      while (x < w16) {
        load_square_win7_neon(di + x, dj + x, d_stride, height, d_is, d_ie,
                              d_js, d_je);
        derive_square_win7_neon(d_is, d_ie, d_js, d_je, deltas);
        x += 16;
      }

      if (w16 != width) {
        load_square_win7_neon(di + x, dj + x, d_stride, height, d_is, d_ie,
                              d_js, d_je);
        d_is[0] = vandq_s16(d_is[0], mask[0]);
        d_is[1] = vandq_s16(d_is[1], mask[1]);
        d_is[2] = vandq_s16(d_is[2], mask[0]);
        d_is[3] = vandq_s16(d_is[3], mask[1]);
        d_is[4] = vandq_s16(d_is[4], mask[0]);
        d_is[5] = vandq_s16(d_is[5], mask[1]);
        d_is[6] = vandq_s16(d_is[6], mask[0]);
        d_is[7] = vandq_s16(d_is[7], mask[1]);
        d_is[8] = vandq_s16(d_is[8], mask[0]);
        d_is[9] = vandq_s16(d_is[9], mask[1]);
        d_is[10] = vandq_s16(d_is[10], mask[0]);
        d_is[11] = vandq_s16(d_is[11], mask[1]);
        d_ie[0] = vandq_s16(d_ie[0], mask[0]);
        d_ie[1] = vandq_s16(d_ie[1], mask[1]);
        d_ie[2] = vandq_s16(d_ie[2], mask[0]);
        d_ie[3] = vandq_s16(d_ie[3], mask[1]);
        d_ie[4] = vandq_s16(d_ie[4], mask[0]);
        d_ie[5] = vandq_s16(d_ie[5], mask[1]);
        d_ie[6] = vandq_s16(d_ie[6], mask[0]);
        d_ie[7] = vandq_s16(d_ie[7], mask[1]);
        d_ie[8] = vandq_s16(d_ie[8], mask[0]);
        d_ie[9] = vandq_s16(d_ie[9], mask[1]);
        d_ie[10] = vandq_s16(d_ie[10], mask[0]);
        d_ie[11] = vandq_s16(d_ie[11], mask[1]);
        derive_square_win7_neon(d_is, d_ie, d_js, d_je, deltas);
      }

      hadd_update_6_stats_neon(
          H + (i * wiener_win + 0) * wiener_win2 + j * wiener_win, deltas[0],
          H + (i * wiener_win + 1) * wiener_win2 + j * wiener_win + 1);
      hadd_update_6_stats_neon(
          H + (i * wiener_win + 1) * wiener_win2 + j * wiener_win, deltas[1],
          H + (i * wiener_win + 2) * wiener_win2 + j * wiener_win + 1);
      hadd_update_6_stats_neon(
          H + (i * wiener_win + 2) * wiener_win2 + j * wiener_win, deltas[2],
          H + (i * wiener_win + 3) * wiener_win2 + j * wiener_win + 1);
      hadd_update_6_stats_neon(
          H + (i * wiener_win + 3) * wiener_win2 + j * wiener_win, deltas[3],
          H + (i * wiener_win + 4) * wiener_win2 + j * wiener_win + 1);
      hadd_update_6_stats_neon(
          H + (i * wiener_win + 4) * wiener_win2 + j * wiener_win, deltas[4],
          H + (i * wiener_win + 5) * wiener_win2 + j * wiener_win + 1);
      hadd_update_6_stats_neon(
          H + (i * wiener_win + 5) * wiener_win2 + j * wiener_win, deltas[5],
          H + (i * wiener_win + 6) * wiener_win2 + j * wiener_win + 1);
    } while (++j < wiener_win);
  } while (++i < wiener_win - 1);

  // Step 6: Derive other points of each upper triangle along the diagonal.
  i = 0;
  do {
    const int16_t *const di = d + i;
    int32x4_t deltas[WIENER_WIN * (WIENER_WIN - 1)] = { vdupq_n_s32(0) };
    int16x8_t d_is[WIN_7], d_ie[WIN_7];

    x = 0;
    while (x < w16) {
      load_triangle_win7_neon(di + x, d_stride, height, d_is, d_ie);
      derive_triangle_win7_neon(d_is, d_ie, deltas);
      x += 16;
    }

    if (w16 != width) {
      load_triangle_win7_neon(di + x, d_stride, height, d_is, d_ie);
      d_is[0] = vandq_s16(d_is[0], mask[0]);
      d_is[1] = vandq_s16(d_is[1], mask[1]);
      d_is[2] = vandq_s16(d_is[2], mask[0]);
      d_is[3] = vandq_s16(d_is[3], mask[1]);
      d_is[4] = vandq_s16(d_is[4], mask[0]);
      d_is[5] = vandq_s16(d_is[5], mask[1]);
      d_is[6] = vandq_s16(d_is[6], mask[0]);
      d_is[7] = vandq_s16(d_is[7], mask[1]);
      d_is[8] = vandq_s16(d_is[8], mask[0]);
      d_is[9] = vandq_s16(d_is[9], mask[1]);
      d_is[10] = vandq_s16(d_is[10], mask[0]);
      d_is[11] = vandq_s16(d_is[11], mask[1]);
      d_ie[0] = vandq_s16(d_ie[0], mask[0]);
      d_ie[1] = vandq_s16(d_ie[1], mask[1]);
      d_ie[2] = vandq_s16(d_ie[2], mask[0]);
      d_ie[3] = vandq_s16(d_ie[3], mask[1]);
      d_ie[4] = vandq_s16(d_ie[4], mask[0]);
      d_ie[5] = vandq_s16(d_ie[5], mask[1]);
      d_ie[6] = vandq_s16(d_ie[6], mask[0]);
      d_ie[7] = vandq_s16(d_ie[7], mask[1]);
      d_ie[8] = vandq_s16(d_ie[8], mask[0]);
      d_ie[9] = vandq_s16(d_ie[9], mask[1]);
      d_ie[10] = vandq_s16(d_ie[10], mask[0]);
      d_ie[11] = vandq_s16(d_ie[11], mask[1]);
      derive_triangle_win7_neon(d_is, d_ie, deltas);
    }

    // Row 1: 6 points
    hadd_update_6_stats_neon(
        H + (i * wiener_win + 0) * wiener_win2 + i * wiener_win, deltas,
        H + (i * wiener_win + 1) * wiener_win2 + i * wiener_win + 1);

    int32x4_t delta1710 = horizontal_add_2d_s32(deltas[17], deltas[10]);
    int32x4_t delta1516 = horizontal_add_2d_s32(deltas[15], deltas[16]);

    int64x2_t delta1710_s64 = vpaddlq_s32(delta1710);
    int64x2_t delta1516_s64 = vpaddlq_s32(delta1516);

    // Row 2: 5 points
    hadd_update_4_stats_neon(
        H + (i * wiener_win + 1) * wiener_win2 + i * wiener_win + 1, deltas + 6,
        H + (i * wiener_win + 2) * wiener_win2 + i * wiener_win + 2);
    H[(i * wiener_win + 2) * wiener_win2 + i * wiener_win + 6] =
        H[(i * wiener_win + 1) * wiener_win2 + i * wiener_win + 5] +
        vgetq_lane_s64(delta1710_s64, 1);

    // Row 3: 4 points
    hadd_update_4_stats_neon(
        H + (i * wiener_win + 2) * wiener_win2 + i * wiener_win + 2,
        deltas + 11,
        H + (i * wiener_win + 3) * wiener_win2 + i * wiener_win + 3);

    // Row 4: 3 points
    int64x2_t h0 =
        vld1q_s64(H + (i * wiener_win + 3) * wiener_win2 + i * wiener_win + 3);
    vst1q_s64(H + (i * wiener_win + 4) * wiener_win2 + i * wiener_win + 4,
              vaddq_s64(h0, delta1516_s64));
    H[(i * wiener_win + 4) * wiener_win2 + i * wiener_win + 6] =
        H[(i * wiener_win + 3) * wiener_win2 + i * wiener_win + 5] +
        vgetq_lane_s64(delta1710_s64, 0);

    int32x4_t delta1819 = horizontal_add_2d_s32(deltas[18], deltas[19]);
    int64x2_t delta1819_s64 = vpaddlq_s32(delta1819);

    // Row 5: 2 points
    int64x2_t h1 =
        vld1q_s64(H + (i * wiener_win + 4) * wiener_win2 + i * wiener_win + 4);
    vst1q_s64(H + (i * wiener_win + 5) * wiener_win2 + i * wiener_win + 5,
              vaddq_s64(h1, delta1819_s64));

    // Row 6: 1 points
    H[(i * wiener_win + 6) * wiener_win2 + i * wiener_win + 6] =
        H[(i * wiener_win + 5) * wiener_win2 + i * wiener_win + 5] +
        horizontal_long_add_s32x4(deltas[20]);
  } while (++i < wiener_win);
}

static inline uint8_t find_average_neon(const uint8_t *src, int src_stride,
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

static inline void compute_sub_avg(const uint8_t *buf, int buf_stride, int avg,
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

static inline void av1_compute_stats_downsampled_neon(
    int wiener_win, const uint8_t *dgd, const uint8_t *src, int16_t *dgd_avg,
    int16_t *src_avg, int h_start, int h_end, int v_start, int v_end,
    int dgd_stride, int src_stride, int64_t *M, int64_t *H,
    int use_downsampled_wiener_stats) {
  assert(wiener_win == WIENER_WIN || wiener_win == WIENER_WIN_CHROMA);
  assert(WIENER_STATS_DOWNSAMPLE_FACTOR == 4);
  (void)dgd_avg;
  (void)src_avg;

  const int wiener_win2 = wiener_win * wiener_win;
  const int wiener_halfwin = wiener_win >> 1;
  const int width = h_end - h_start;
  const int height = v_end - v_start;

  const uint8_t *dgd_start = dgd + h_start + v_start * dgd_stride;
  const uint8_t *src_start = src + h_start + v_start * src_stride;

  // The wiener window will slide along the dgd frame, centered on each pixel.
  // For the top left pixel and all the pixels on the side of the frame this
  // means half of the window will be outside of the frame. As such the actual
  // buffer that we need to subtract the avg from will be 2 * wiener_halfwin
  // wider and 2 * wiener_halfwin higher than the original dgd buffer.
  const int vert_offset = v_start - wiener_halfwin;
  const int horiz_offset = h_start - wiener_halfwin;
  const uint8_t *dgd_win = dgd + horiz_offset + vert_offset * dgd_stride;

  uint8_t avg = find_average_neon(dgd_start, dgd_stride, width, height);

  // Since the height is not necessarily a multiple of the downsample factor,
  // the last line of src will be scaled according to how many rows remain.
  int downsample_factor =
      use_downsampled_wiener_stats ? WIENER_STATS_DOWNSAMPLE_FACTOR : 1;

  int downsampled_height = height / downsample_factor;
  int downsample_remainder = height % downsample_factor;

  memset(M, 0, wiener_win2 * sizeof(*M));
  memset(H, 0, wiener_win2 * wiener_win2 * sizeof(*H));

  // Calculate the M and H matrices for the normal and downsampled cases.
  if (downsampled_height > 0) {
    if (wiener_win == WIENER_WIN) {
      compute_stats_win7_downsampled_neon(
          dgd_win, src_start, width, downsampled_height, dgd_stride, src_stride,
          avg, M, H, downsample_factor);
    } else {
      compute_stats_win5_downsampled_neon(
          dgd_win, src_start, width, downsampled_height, dgd_stride, src_stride,
          avg, M, H, downsample_factor);
    }
  }

  // Accumulate the remaining last rows in the downsampled case.
  if (downsample_remainder > 0) {
    int remainder_offset = height - downsample_remainder;
    if (wiener_win == WIENER_WIN) {
      compute_stats_win7_downsampled_neon(
          dgd_win + remainder_offset * dgd_stride,
          src_start + remainder_offset * src_stride, width, 1, dgd_stride,
          src_stride, avg, M, H, downsample_remainder);
    } else {
      compute_stats_win5_downsampled_neon(
          dgd_win + remainder_offset * dgd_stride,
          src_start + remainder_offset * src_stride, width, 1, dgd_stride,
          src_stride, avg, M, H, downsample_remainder);
    }
  }
}

void av1_compute_stats_neon(int32_t wiener_win, const uint8_t *dgd,
                            const uint8_t *src, int16_t *dgd_avg,
                            int16_t *src_avg, int32_t h_start, int32_t h_end,
                            int32_t v_start, int32_t v_end, int32_t dgd_stride,
                            int32_t src_stride, int64_t *M, int64_t *H,
                            int use_downsampled_wiener_stats) {
  assert(WIENER_STATS_DOWNSAMPLE_FACTOR == 4);
  if (use_downsampled_wiener_stats) {
    av1_compute_stats_downsampled_neon(
        wiener_win, dgd, src, dgd_avg, src_avg, h_start, h_end, v_start, v_end,
        dgd_stride, src_stride, M, H, use_downsampled_wiener_stats);
    return;
  }

  const int32_t wiener_win2 = wiener_win * wiener_win;
  const int32_t wiener_halfwin = (wiener_win >> 1);
  const int32_t width = h_end - h_start;
  const int32_t height = v_end - v_start;
  const uint8_t *dgd_start = dgd + h_start + v_start * dgd_stride;
  const uint8_t avg = find_average_neon(dgd_start, dgd_stride, width, height);
  const int32_t d_stride = (width + 2 * wiener_halfwin + 15) & ~15;
  const int32_t s_stride = (width + 15) & ~15;

  compute_sub_avg(src + v_start * src_stride + h_start, src_stride, avg,
                  src_avg, s_stride, width, height, 1);
  compute_sub_avg(
      dgd + (v_start - wiener_halfwin) * dgd_stride + h_start - wiener_halfwin,
      dgd_stride, avg, dgd_avg, d_stride, width + 2 * wiener_halfwin,
      height + 2 * wiener_halfwin, 1);

  if (wiener_win == WIENER_WIN) {
    compute_stats_win7_neon(dgd_avg, d_stride, src_avg, s_stride, width, height,
                            M, H);
  } else if (wiener_win == WIENER_WIN_CHROMA) {
    compute_stats_win5_neon(dgd_avg, d_stride, src_avg, s_stride, width, height,
                            M, H);
  }

  // H is a symmetric matrix, so we only need to fill out the upper triangle.
  // We can copy it down to the lower triangle outside the (i, j) loops.
  diagonal_copy_stats_neon(wiener_win2, H);
}

static inline void calc_proj_params_r0_r1_neon(
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

static inline void calc_proj_params_r0_neon(const uint8_t *src8, int width,
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

static inline void calc_proj_params_r1_neon(const uint8_t *src8, int width,
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
