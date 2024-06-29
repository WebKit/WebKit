/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <arm_neon.h>
#include <arm_sve.h>
#include <string.h>

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "aom_dsp/arm/aom_neon_sve_bridge.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/sum_neon.h"
#include "aom_dsp/arm/transpose_neon.h"
#include "av1/common/restoration.h"
#include "av1/encoder/pickrst.h"
#include "av1/encoder/arm/pickrst_sve.h"

static INLINE uint8_t find_average_sve(const uint8_t *src, int src_stride,
                                       int width, int height) {
  uint32x4_t avg_u32 = vdupq_n_u32(0);
  uint8x16_t ones = vdupq_n_u8(1);

  // Use a predicate to compute the last columns.
  svbool_t pattern = svwhilelt_b8_u32(0, width % 16);

  int h = height;
  do {
    int j = width;
    const uint8_t *src_ptr = src;
    while (j >= 16) {
      uint8x16_t s = vld1q_u8(src_ptr);
      avg_u32 = vdotq_u32(avg_u32, s, ones);

      j -= 16;
      src_ptr += 16;
    }
    uint8x16_t s_end = svget_neonq_u8(svld1_u8(pattern, src_ptr));
    avg_u32 = vdotq_u32(avg_u32, s_end, ones);

    src += src_stride;
  } while (--h != 0);
  return (uint8_t)(vaddlvq_u32(avg_u32) / (width * height));
}

static INLINE void compute_sub_avg(const uint8_t *buf, int buf_stride, int avg,
                                   int16_t *buf_avg, int buf_avg_stride,
                                   int width, int height,
                                   int downsample_factor) {
  uint8x8_t avg_u8 = vdup_n_u8(avg);

  // Use a predicate to compute the last columns.
  svbool_t pattern = svwhilelt_b8_u32(0, width % 8);

  uint8x8_t avg_end = vget_low_u8(svget_neonq_u8(svdup_n_u8_z(pattern, avg)));

  do {
    int j = width;
    const uint8_t *buf_ptr = buf;
    int16_t *buf_avg_ptr = buf_avg;
    while (j >= 8) {
      uint8x8_t d = vld1_u8(buf_ptr);
      vst1q_s16(buf_avg_ptr, vreinterpretq_s16_u16(vsubl_u8(d, avg_u8)));

      j -= 8;
      buf_ptr += 8;
      buf_avg_ptr += 8;
    }
    uint8x8_t d_end = vget_low_u8(svget_neonq_u8(svld1_u8(pattern, buf_ptr)));
    vst1q_s16(buf_avg_ptr, vreinterpretq_s16_u16(vsubl_u8(d_end, avg_end)));

    buf += buf_stride;
    buf_avg += buf_avg_stride;
    height -= downsample_factor;
  } while (height > 0);
}

static INLINE void copy_upper_triangle(int64_t *H, int64_t *H_tmp,
                                       const int wiener_win2, const int scale) {
  for (int i = 0; i < wiener_win2 - 2; i = i + 2) {
    // Transpose the first 2x2 square. It needs a special case as the element
    // of the bottom left is on the diagonal.
    int64x2_t row0 = vld1q_s64(H_tmp + i * wiener_win2 + i + 1);
    int64x2_t row1 = vld1q_s64(H_tmp + (i + 1) * wiener_win2 + i + 1);

    int64x2_t tr_row = aom_vtrn2q_s64(row0, row1);

    vst1_s64(H_tmp + (i + 1) * wiener_win2 + i, vget_low_s64(row0));
    vst1q_s64(H_tmp + (i + 2) * wiener_win2 + i, tr_row);

    // Transpose and store all the remaining 2x2 squares of the line.
    for (int j = i + 3; j < wiener_win2; j = j + 2) {
      row0 = vld1q_s64(H_tmp + i * wiener_win2 + j);
      row1 = vld1q_s64(H_tmp + (i + 1) * wiener_win2 + j);

      int64x2_t tr_row0 = aom_vtrn1q_s64(row0, row1);
      int64x2_t tr_row1 = aom_vtrn2q_s64(row0, row1);

      vst1q_s64(H_tmp + j * wiener_win2 + i, tr_row0);
      vst1q_s64(H_tmp + (j + 1) * wiener_win2 + i, tr_row1);
    }
  }
  for (int i = 0; i < wiener_win2 * wiener_win2; i++) {
    H[i] += H_tmp[i] * scale;
  }
}

// Transpose the matrix that has just been computed and accumulate it in M.
static INLINE void acc_transpose_M(int64_t *M, const int64_t *M_trn,
                                   const int wiener_win, int scale) {
  for (int i = 0; i < wiener_win; ++i) {
    for (int j = 0; j < wiener_win; ++j) {
      int tr_idx = j * wiener_win + i;
      *M++ += (int64_t)(M_trn[tr_idx] * scale);
    }
  }
}

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
static INLINE void compute_stats_win7_sve(int16_t *dgd_avg, int dgd_avg_stride,
                                          int16_t *src_avg, int src_avg_stride,
                                          int width, int height, int64_t *M,
                                          int64_t *H, int downsample_factor) {
  const int wiener_win = 7;
  const int wiener_win2 = wiener_win * wiener_win;

  // Use a predicate to compute the last columns of the block for H.
  svbool_t pattern = svwhilelt_b16_u32(0, width % 8);

  // Use intermediate matrices for H and M to perform the computation, they
  // will be accumulated into the original H and M at the end.
  int64_t M_trn[49];
  memset(M_trn, 0, sizeof(M_trn));

  int64_t H_tmp[49 * 49];
  memset(H_tmp, 0, sizeof(H_tmp));

  do {
    // Cross-correlation (M).
    for (int row = 0; row < wiener_win; row++) {
      int j = 0;
      while (j < width) {
        int16x8_t dgd[7];
        load_s16_8x7(dgd_avg + row * dgd_avg_stride + j, 1, &dgd[0], &dgd[1],
                     &dgd[2], &dgd[3], &dgd[4], &dgd[5], &dgd[6]);
        int16x8_t s = vld1q_s16(src_avg + j);

        // Compute all the elements of one row of M.
        compute_M_one_row_win7(s, dgd, M_trn, row);

        j += 8;
      }
    }

    // Auto-covariance (H).
    int j = 0;
    while (j <= width - 8) {
      for (int col0 = 0; col0 < wiener_win; col0++) {
        int16x8_t dgd0[7];
        load_s16_8x7(dgd_avg + j + col0, dgd_avg_stride, &dgd0[0], &dgd0[1],
                     &dgd0[2], &dgd0[3], &dgd0[4], &dgd0[5], &dgd0[6]);

        // Perform computation of the first column with itself (28 elements).
        // For the first column this will fill the upper triangle of the 7x7
        // matrix at the top left of the H matrix. For the next columns this
        // will fill the upper triangle of the other 7x7 matrices around H's
        // diagonal.
        compute_H_one_col(dgd0, col0, H_tmp, wiener_win, wiener_win2);

        // All computation next to the matrix diagonal has already been done.
        for (int col1 = col0 + 1; col1 < wiener_win; col1++) {
          // Load second column and scale based on downsampling factor.
          int16x8_t dgd1[7];
          load_s16_8x7(dgd_avg + j + col1, dgd_avg_stride, &dgd1[0], &dgd1[1],
                       &dgd1[2], &dgd1[3], &dgd1[4], &dgd1[5], &dgd1[6]);

          // Compute all elements from the combination of both columns (49
          // elements).
          compute_H_two_rows_win7(dgd0, dgd1, col0, col1, H_tmp);
        }
      }
      j += 8;
    }

    if (j < width) {
      // Process remaining columns using a predicate to discard excess elements.
      for (int col0 = 0; col0 < wiener_win; col0++) {
        // Load first column.
        int16x8_t dgd0[7];
        dgd0[0] = svget_neonq_s16(
            svld1_s16(pattern, dgd_avg + 0 * dgd_avg_stride + j + col0));
        dgd0[1] = svget_neonq_s16(
            svld1_s16(pattern, dgd_avg + 1 * dgd_avg_stride + j + col0));
        dgd0[2] = svget_neonq_s16(
            svld1_s16(pattern, dgd_avg + 2 * dgd_avg_stride + j + col0));
        dgd0[3] = svget_neonq_s16(
            svld1_s16(pattern, dgd_avg + 3 * dgd_avg_stride + j + col0));
        dgd0[4] = svget_neonq_s16(
            svld1_s16(pattern, dgd_avg + 4 * dgd_avg_stride + j + col0));
        dgd0[5] = svget_neonq_s16(
            svld1_s16(pattern, dgd_avg + 5 * dgd_avg_stride + j + col0));
        dgd0[6] = svget_neonq_s16(
            svld1_s16(pattern, dgd_avg + 6 * dgd_avg_stride + j + col0));

        // Perform computation of the first column with itself (28 elements).
        // For the first column this will fill the upper triangle of the 7x7
        // matrix at the top left of the H matrix. For the next columns this
        // will fill the upper triangle of the other 7x7 matrices around H's
        // diagonal.
        compute_H_one_col(dgd0, col0, H_tmp, wiener_win, wiener_win2);

        // All computation next to the matrix diagonal has already been done.
        for (int col1 = col0 + 1; col1 < wiener_win; col1++) {
          // Load second column and scale based on downsampling factor.
          int16x8_t dgd1[7];
          load_s16_8x7(dgd_avg + j + col1, dgd_avg_stride, &dgd1[0], &dgd1[1],
                       &dgd1[2], &dgd1[3], &dgd1[4], &dgd1[5], &dgd1[6]);

          // Compute all elements from the combination of both columns (49
          // elements).
          compute_H_two_rows_win7(dgd0, dgd1, col0, col1, H_tmp);
        }
      }
    }
    dgd_avg += downsample_factor * dgd_avg_stride;
    src_avg += src_avg_stride;
  } while (--height != 0);

  // Transpose M_trn.
  acc_transpose_M(M, M_trn, 7, downsample_factor);

  // Copy upper triangle of H in the lower one.
  copy_upper_triangle(H, H_tmp, wiener_win2, downsample_factor);
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
static INLINE void compute_stats_win5_sve(int16_t *dgd_avg, int dgd_avg_stride,
                                          int16_t *src_avg, int src_avg_stride,
                                          int width, int height, int64_t *M,
                                          int64_t *H, int downsample_factor) {
  const int wiener_win = 5;
  const int wiener_win2 = wiener_win * wiener_win;

  // Use a predicate to compute the last columns of the block for H.
  svbool_t pattern = svwhilelt_b16_u32(0, width % 8);

  // Use intermediate matrices for H and M to perform the computation, they
  // will be accumulated into the original H and M at the end.
  int64_t M_trn[25];
  memset(M_trn, 0, sizeof(M_trn));

  int64_t H_tmp[25 * 25];
  memset(H_tmp, 0, sizeof(H_tmp));

  do {
    // Cross-correlation (M).
    for (int row = 0; row < wiener_win; row++) {
      int j = 0;
      while (j < width) {
        int16x8_t dgd[5];
        load_s16_8x5(dgd_avg + row * dgd_avg_stride + j, 1, &dgd[0], &dgd[1],
                     &dgd[2], &dgd[3], &dgd[4]);
        int16x8_t s = vld1q_s16(src_avg + j);

        // Compute all the elements of one row of M.
        compute_M_one_row_win5(s, dgd, M_trn, row);

        j += 8;
      }
    }

    // Auto-covariance (H).
    int j = 0;
    while (j <= width - 8) {
      for (int col0 = 0; col0 < wiener_win; col0++) {
        // Load first column.
        int16x8_t dgd0[5];
        load_s16_8x5(dgd_avg + j + col0, dgd_avg_stride, &dgd0[0], &dgd0[1],
                     &dgd0[2], &dgd0[3], &dgd0[4]);

        // Perform computation of the first column with itself (15 elements).
        // For the first column this will fill the upper triangle of the 5x5
        // matrix at the top left of the H matrix. For the next columns this
        // will fill the upper triangle of the other 5x5 matrices around H's
        // diagonal.
        compute_H_one_col(dgd0, col0, H_tmp, wiener_win, wiener_win2);

        // All computation next to the matrix diagonal has already been done.
        for (int col1 = col0 + 1; col1 < wiener_win; col1++) {
          // Load second column and scale based on downsampling factor.
          int16x8_t dgd1[5];
          load_s16_8x5(dgd_avg + j + col1, dgd_avg_stride, &dgd1[0], &dgd1[1],
                       &dgd1[2], &dgd1[3], &dgd1[4]);

          // Compute all elements from the combination of both columns (25
          // elements).
          compute_H_two_rows_win5(dgd0, dgd1, col0, col1, H_tmp);
        }
      }
      j += 8;
    }

    // Process remaining columns using a predicate to discard excess elements.
    if (j < width) {
      for (int col0 = 0; col0 < wiener_win; col0++) {
        int16x8_t dgd0[5];
        dgd0[0] = svget_neonq_s16(
            svld1_s16(pattern, dgd_avg + 0 * dgd_avg_stride + j + col0));
        dgd0[1] = svget_neonq_s16(
            svld1_s16(pattern, dgd_avg + 1 * dgd_avg_stride + j + col0));
        dgd0[2] = svget_neonq_s16(
            svld1_s16(pattern, dgd_avg + 2 * dgd_avg_stride + j + col0));
        dgd0[3] = svget_neonq_s16(
            svld1_s16(pattern, dgd_avg + 3 * dgd_avg_stride + j + col0));
        dgd0[4] = svget_neonq_s16(
            svld1_s16(pattern, dgd_avg + 4 * dgd_avg_stride + j + col0));

        // Perform computation of the first column with itself (15 elements).
        // For the first column this will fill the upper triangle of the 5x5
        // matrix at the top left of the H matrix. For the next columns this
        // will fill the upper triangle of the other 5x5 matrices around H's
        // diagonal.
        compute_H_one_col(dgd0, col0, H_tmp, wiener_win, wiener_win2);

        // All computation next to the matrix diagonal has already been done.
        for (int col1 = col0 + 1; col1 < wiener_win; col1++) {
          // Load second column and scale based on downsampling factor.
          int16x8_t dgd1[5];
          load_s16_8x5(dgd_avg + j + col1, dgd_avg_stride, &dgd1[0], &dgd1[1],
                       &dgd1[2], &dgd1[3], &dgd1[4]);

          // Compute all elements from the combination of both columns (25
          // elements).
          compute_H_two_rows_win5(dgd0, dgd1, col0, col1, H_tmp);
        }
      }
    }
    dgd_avg += downsample_factor * dgd_avg_stride;
    src_avg += src_avg_stride;
  } while (--height != 0);

  // Transpose M_trn.
  acc_transpose_M(M, M_trn, 5, downsample_factor);

  // Copy upper triangle of H in the lower one.
  copy_upper_triangle(H, H_tmp, wiener_win2, downsample_factor);
}

void av1_compute_stats_sve(int wiener_win, const uint8_t *dgd,
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
  memset(M, 0, sizeof(*M) * wiener_win * wiener_win);

  const uint8_t avg = find_average_sve(dgd_start, dgd_stride, width, height);
  const int downsample_factor =
      use_downsampled_wiener_stats ? WIENER_STATS_DOWNSAMPLE_FACTOR : 1;

  // dgd_avg and src_avg have been memset to zero before calling this
  // function, so round up the stride to the next multiple of 8 so that we
  // don't have to worry about a tail loop when computing M.
  const int dgd_avg_stride = ((width + 2 * wiener_halfwin) & ~7) + 8;
  const int src_avg_stride = (width & ~7) + 8;

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

  const int downsample_height = height / downsample_factor;

  // Since the height is not necessarily a multiple of the downsample factor,
  // the last line of src will be scaled according to how many rows remain.
  const int downsample_remainder = height % downsample_factor;

  if (wiener_win == WIENER_WIN) {
    compute_stats_win7_sve(dgd_avg, dgd_avg_stride, src_avg, src_avg_stride,
                           width, downsample_height, M, H, downsample_factor);
  } else {
    compute_stats_win5_sve(dgd_avg, dgd_avg_stride, src_avg, src_avg_stride,
                           width, downsample_height, M, H, downsample_factor);
  }

  if (downsample_remainder > 0) {
    const int remainder_offset = height - downsample_remainder;
    if (wiener_win == WIENER_WIN) {
      compute_stats_win7_sve(
          dgd_avg + remainder_offset * dgd_avg_stride, dgd_avg_stride,
          src_avg + downsample_height * src_avg_stride, src_avg_stride, width,
          1, M, H, downsample_remainder);
    } else {
      compute_stats_win5_sve(
          dgd_avg + remainder_offset * dgd_avg_stride, dgd_avg_stride,
          src_avg + downsample_height * src_avg_stride, src_avg_stride, width,
          1, M, H, downsample_remainder);
    }
  }
}
