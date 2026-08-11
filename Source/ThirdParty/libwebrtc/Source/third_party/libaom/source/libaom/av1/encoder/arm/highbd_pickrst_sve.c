/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved.
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

#include <assert.h>
#include <stdint.h>

#include "aom_dsp/arm/aom_neon_sve_bridge.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/sum_neon.h"
#include "aom_dsp/arm/transpose_neon.h"
#include "av1/encoder/arm/pickrst_neon.h"
#include "av1/encoder/arm/pickrst_sve.h"
#include "av1/encoder/pickrst.h"

static inline uint16_t highbd_find_average_sve(const uint16_t *src,
                                               int src_stride, int width,
                                               int height) {
  uint64x2_t avg_u64 = vdupq_n_u64(0);
  uint16x8_t ones = vdupq_n_u16(1);

  // Use a predicate to compute the last columns.
  svbool_t pattern = svwhilelt_b16_u32(0, width % 8 == 0 ? 8 : width % 8);

  int h = height;
  do {
    int j = width;
    const uint16_t *src_ptr = src;
    while (j > 8) {
      uint16x8_t s = vld1q_u16(src_ptr);
      avg_u64 = aom_udotq_u16(avg_u64, s, ones);

      j -= 8;
      src_ptr += 8;
    }
    uint16x8_t s_end = svget_neonq_u16(svld1_u16(pattern, src_ptr));
    avg_u64 = aom_udotq_u16(avg_u64, s_end, ones);

    src += src_stride;
  } while (--h != 0);
  return (uint16_t)(vaddvq_u64(avg_u64) / (width * height));
}

static inline void sub_avg_block_highbd_sve(const uint16_t *buf, int buf_stride,
                                            int16_t avg, int width, int height,
                                            int16_t *buf_avg,
                                            int buf_avg_stride) {
  uint16x8_t avg_u16 = vdupq_n_u16(avg);

  // Use a predicate to compute the last columns.
  svbool_t pattern = svwhilelt_b16_u32(0, width % 8 == 0 ? 8 : width % 8);

  uint16x8_t avg_end = svget_neonq_u16(svdup_n_u16_z(pattern, avg));

  do {
    int j = width;
    const uint16_t *buf_ptr = buf;
    int16_t *buf_avg_ptr = buf_avg;
    while (j > 8) {
      uint16x8_t d = vld1q_u16(buf_ptr);
      vst1q_s16(buf_avg_ptr, vreinterpretq_s16_u16(vsubq_u16(d, avg_u16)));

      j -= 8;
      buf_ptr += 8;
      buf_avg_ptr += 8;
    }
    uint16x8_t d_end = svget_neonq_u16(svld1_u16(pattern, buf_ptr));
    vst1q_s16(buf_avg_ptr, vreinterpretq_s16_u16(vsubq_u16(d_end, avg_end)));

    buf += buf_stride;
    buf_avg += buf_avg_stride;
  } while (--height > 0);
}

void av1_compute_stats_highbd_sve(int32_t wiener_win, const uint8_t *dgd8,
                                  const uint8_t *src8, int16_t *dgd_avg,
                                  int16_t *src_avg, int32_t h_start,
                                  int32_t h_end, int32_t v_start, int32_t v_end,
                                  int32_t dgd_stride, int32_t src_stride,
                                  int64_t *M, int64_t *H,
                                  aom_bit_depth_t bit_depth) {
  const int32_t wiener_win2 = wiener_win * wiener_win;
  const int32_t wiener_halfwin = (wiener_win >> 1);
  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  const uint16_t *dgd = CONVERT_TO_SHORTPTR(dgd8);
  const int32_t width = h_end - h_start;
  const int32_t height = v_end - v_start;
  const int32_t d_stride = (width + 2 * wiener_halfwin + 15) & ~15;
  const int32_t s_stride = (width + 15) & ~15;

  const uint16_t *dgd_start = dgd + h_start + v_start * dgd_stride;
  const uint16_t *src_start = src + h_start + v_start * src_stride;
  const uint16_t avg =
      highbd_find_average_sve(dgd_start, dgd_stride, width, height);

  sub_avg_block_highbd_sve(src_start, src_stride, avg, width, height, src_avg,
                           s_stride);
  sub_avg_block_highbd_sve(
      dgd + (v_start - wiener_halfwin) * dgd_stride + h_start - wiener_halfwin,
      dgd_stride, avg, width + 2 * wiener_halfwin, height + 2 * wiener_halfwin,
      dgd_avg, d_stride);

  if (wiener_win == WIENER_WIN) {
    compute_stats_win7_sve(dgd_avg, d_stride, src_avg, s_stride, width, height,
                           M, H);
  } else {
    assert(wiener_win == WIENER_WIN_CHROMA);
    compute_stats_win5_sve(dgd_avg, d_stride, src_avg, s_stride, width, height,
                           M, H);
  }

  // H is a symmetric matrix, so we only need to fill out the upper triangle.
  // We can copy it down to the lower triangle outside the (i, j) loops.
  if (bit_depth == AOM_BITS_8) {
    diagonal_copy_stats_neon(wiener_win2, H);
  } else if (bit_depth == AOM_BITS_10) {  // bit_depth == EB_TEN_BIT
    const int32_t k4 = wiener_win2 & ~3;

    int32_t k = 0;
    do {
      int64x2_t dst = div4_neon(vld1q_s64(M + k));
      vst1q_s64(M + k, dst);
      dst = div4_neon(vld1q_s64(M + k + 2));
      vst1q_s64(M + k + 2, dst);
      H[k * wiener_win2 + k] /= 4;
      k += 4;
    } while (k < k4);

    H[k * wiener_win2 + k] /= 4;

    for (; k < wiener_win2; ++k) {
      M[k] /= 4;
    }

    div4_diagonal_copy_stats_neon(wiener_win2, H);
  } else {  // bit_depth == AOM_BITS_12
    const int32_t k4 = wiener_win2 & ~3;

    int32_t k = 0;
    do {
      int64x2_t dst = div16_neon(vld1q_s64(M + k));
      vst1q_s64(M + k, dst);
      dst = div16_neon(vld1q_s64(M + k + 2));
      vst1q_s64(M + k + 2, dst);
      H[k * wiener_win2 + k] /= 16;
      k += 4;
    } while (k < k4);

    H[k * wiener_win2 + k] /= 16;

    for (; k < wiener_win2; ++k) {
      M[k] /= 16;
    }

    div16_diagonal_copy_stats_neon(wiener_win2, H);
  }
}
