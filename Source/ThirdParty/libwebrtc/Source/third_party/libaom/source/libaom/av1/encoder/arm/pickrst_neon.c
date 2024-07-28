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

#include "aom_dsp/arm/sum_neon.h"
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

// We can accumulate up to 65536 8-bit multiplication results in 32-bit. We are
// processing 2 pixels at a time, so the accumulator max can be as high as 32768
// for the compute stats.
#define STAT_ACCUMULATOR_MAX 32768

static INLINE uint8x8_t tbl2(uint8x16_t a, uint8x16_t b, uint8x8_t idx) {
#if AOM_ARCH_AARCH64
  uint8x16x2_t table = { { a, b } };
  return vqtbl2_u8(table, idx);
#else
  uint8x8x4_t table = { { vget_low_u8(a), vget_high_u8(a), vget_low_u8(b),
                          vget_high_u8(b) } };
  return vtbl4_u8(table, idx);
#endif
}

static INLINE uint8x16_t tbl2q(uint8x16_t a, uint8x16_t b, uint8x16_t idx) {
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
static INLINE void acc_transpose_M(int64_t *dst, const int64_t *src_s64,
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
static INLINE void load_and_pack_u8_8x7(uint8x16_t dst[4], const uint8_t *src,
                                        ptrdiff_t stride) {
  dst[0] = vcombine_u8(vld1_u8(src), vld1_u8(src + stride));
  src += 2 * stride;
  dst[1] = vcombine_u8(vld1_u8(src), vld1_u8(src + stride));
  src += 2 * stride;
  dst[2] = vcombine_u8(vld1_u8(src), vld1_u8(src + stride));
  src += 2 * stride;
  dst[3] = vcombine_u8(vld1_u8(src - 1), vdup_n_u8(0));
}

static INLINE void compute_stats_win7_neon(const uint8_t *dgd,
                                           const uint8_t *src, int width,
                                           int height, int dgd_stride,
                                           int src_stride, int avg, int64_t *M,
                                           int64_t *H, int downsample_factor) {
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
static INLINE void load_and_pack_u8_6x5(uint8x16_t dst[3], const uint8_t *src,
                                        ptrdiff_t stride) {
  dst[0] = vcombine_u8(vld1_u8(src), vld1_u8(src + stride));
  src += 2 * stride;
  dst[1] = vcombine_u8(vld1_u8(src), vld1_u8(src + stride));
  src += 2 * stride;
  dst[2] = vcombine_u8(vld1_u8(src - 3), vdup_n_u8(0));
}

static INLINE void compute_stats_win5_neon(const uint8_t *dgd,
                                           const uint8_t *src, int width,
                                           int height, int dgd_stride,
                                           int src_stride, int avg, int64_t *M,
                                           int64_t *H, int downsample_factor) {
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

void av1_compute_stats_neon(int wiener_win, const uint8_t *dgd,
                            const uint8_t *src, int16_t *dgd_avg,
                            int16_t *src_avg, int h_start, int h_end,
                            int v_start, int v_end, int dgd_stride,
                            int src_stride, int64_t *M, int64_t *H,
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
      compute_stats_win7_neon(dgd_win, src_start, width, downsampled_height,
                              dgd_stride, src_stride, avg, M, H,
                              downsample_factor);
    } else {
      compute_stats_win5_neon(dgd_win, src_start, width, downsampled_height,
                              dgd_stride, src_stride, avg, M, H,
                              downsample_factor);
    }
  }

  // Accumulate the remaining last rows in the downsampled case.
  if (downsample_remainder > 0) {
    int remainder_offset = height - downsample_remainder;
    if (wiener_win == WIENER_WIN) {
      compute_stats_win7_neon(dgd_win + remainder_offset * dgd_stride,
                              src_start + remainder_offset * src_stride, width,
                              1, dgd_stride, src_stride, avg, M, H,
                              downsample_remainder);
    } else {
      compute_stats_win5_neon(dgd_win + remainder_offset * dgd_stride,
                              src_start + remainder_offset * src_stride, width,
                              1, dgd_stride, src_stride, avg, M, H,
                              downsample_remainder);
    }
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
