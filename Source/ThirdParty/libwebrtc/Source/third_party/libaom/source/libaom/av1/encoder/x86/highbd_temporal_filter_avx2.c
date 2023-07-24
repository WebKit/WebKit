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

#include <assert.h>
#include <immintrin.h>

#include "config/av1_rtcd.h"
#include "aom_dsp/mathutils.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/temporal_filter.h"

#define SSE_STRIDE (BW + 4)

DECLARE_ALIGNED(32, static const uint32_t, sse_bytemask[4][8]) = {
  { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0, 0, 0 },
  { 0, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0, 0 },
  { 0, 0, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0 },
  { 0, 0, 0, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF }
};

static AOM_FORCE_INLINE void get_squared_error_16x16_avx2(
    const uint16_t *frame1, const unsigned int stride, const uint16_t *frame2,
    const unsigned int stride2, const int block_width, const int block_height,
    uint32_t *frame_sse, const unsigned int sse_stride) {
  (void)block_width;
  const uint16_t *src1 = frame1;
  const uint16_t *src2 = frame2;
  uint32_t *dst = frame_sse + 2;
  for (int i = 0; i < block_height; i++) {
    __m256i v_src1 = _mm256_loadu_si256((__m256i *)src1);
    __m256i v_src2 = _mm256_loadu_si256((__m256i *)src2);
    __m256i v_diff = _mm256_sub_epi16(v_src1, v_src2);
    __m256i v_mullo = _mm256_mullo_epi16(v_diff, v_diff);
    __m256i v_mulhi = _mm256_mulhi_epi16(v_diff, v_diff);

    __m256i v_lo = _mm256_unpacklo_epi16(v_mullo, v_mulhi);
    __m256i v_hi = _mm256_unpackhi_epi16(v_mullo, v_mulhi);
    __m256i diff_lo =
        _mm256_inserti128_si256(v_lo, _mm256_extracti128_si256(v_hi, 0), 1);
    __m256i diff_hi =
        _mm256_inserti128_si256(v_hi, _mm256_extracti128_si256(v_lo, 1), 0);

    _mm256_storeu_si256((__m256i *)dst, diff_lo);
    dst += 8;
    _mm256_storeu_si256((__m256i *)dst, diff_hi);

    src1 += stride, src2 += stride2;
    dst += sse_stride - 8;
  }
}

static AOM_FORCE_INLINE void get_squared_error_32x32_avx2(
    const uint16_t *frame1, const unsigned int stride, const uint16_t *frame2,
    const unsigned int stride2, const int block_width, const int block_height,
    uint32_t *frame_sse, const unsigned int sse_stride) {
  (void)block_width;
  const uint16_t *src1 = frame1;
  const uint16_t *src2 = frame2;
  uint32_t *dst = frame_sse + 2;
  for (int i = 0; i < block_height; i++) {
    __m256i v_src1 = _mm256_loadu_si256((__m256i *)src1);
    __m256i v_src2 = _mm256_loadu_si256((__m256i *)src2);
    __m256i v_diff = _mm256_sub_epi16(v_src1, v_src2);
    __m256i v_mullo = _mm256_mullo_epi16(v_diff, v_diff);
    __m256i v_mulhi = _mm256_mulhi_epi16(v_diff, v_diff);

    __m256i v_lo = _mm256_unpacklo_epi16(v_mullo, v_mulhi);
    __m256i v_hi = _mm256_unpackhi_epi16(v_mullo, v_mulhi);
    __m256i diff_lo =
        _mm256_inserti128_si256(v_lo, _mm256_extracti128_si256(v_hi, 0), 1);
    __m256i diff_hi =
        _mm256_inserti128_si256(v_hi, _mm256_extracti128_si256(v_lo, 1), 0);

    _mm256_storeu_si256((__m256i *)dst, diff_lo);
    _mm256_storeu_si256((__m256i *)(dst + 8), diff_hi);

    v_src1 = _mm256_loadu_si256((__m256i *)(src1 + 16));
    v_src2 = _mm256_loadu_si256((__m256i *)(src2 + 16));
    v_diff = _mm256_sub_epi16(v_src1, v_src2);
    v_mullo = _mm256_mullo_epi16(v_diff, v_diff);
    v_mulhi = _mm256_mulhi_epi16(v_diff, v_diff);

    v_lo = _mm256_unpacklo_epi16(v_mullo, v_mulhi);
    v_hi = _mm256_unpackhi_epi16(v_mullo, v_mulhi);
    diff_lo =
        _mm256_inserti128_si256(v_lo, _mm256_extracti128_si256(v_hi, 0), 1);
    diff_hi =
        _mm256_inserti128_si256(v_hi, _mm256_extracti128_si256(v_lo, 1), 0);

    _mm256_storeu_si256((__m256i *)(dst + 16), diff_lo);
    _mm256_storeu_si256((__m256i *)(dst + 24), diff_hi);

    src1 += stride;
    src2 += stride2;
    dst += sse_stride;
  }
}

static AOM_FORCE_INLINE void xx_load_and_pad_left(uint32_t *src,
                                                  __m256i *v256tmp) {
  *v256tmp = _mm256_loadu_si256((__m256i *)src);
  // For the first column, replicate the first element twice to the left
  __m256i v256tmp1 = _mm256_shuffle_epi32(*v256tmp, 0xEA);
  *v256tmp = _mm256_inserti128_si256(*v256tmp,
                                     _mm256_extracti128_si256(v256tmp1, 0), 0);
}

static AOM_FORCE_INLINE void xx_load_and_pad_right(uint32_t *src,
                                                   __m256i *v256tmp) {
  *v256tmp = _mm256_loadu_si256((__m256i *)src);
  // For the last column, replicate the last element twice to the right
  __m256i v256tmp1 = _mm256_shuffle_epi32(*v256tmp, 0x54);
  *v256tmp = _mm256_inserti128_si256(*v256tmp,
                                     _mm256_extracti128_si256(v256tmp1, 1), 1);
}

static AOM_FORCE_INLINE int32_t xx_mask_and_hadd(__m256i vsum, int i) {
  // Mask the required 5 values inside the vector
  __m256i vtmp = _mm256_and_si256(vsum, *(__m256i *)sse_bytemask[i]);
  __m128i v128a, v128b;
  // Extract 256b as two 128b registers A and B
  v128a = _mm256_castsi256_si128(vtmp);
  v128b = _mm256_extracti128_si256(vtmp, 1);
  // A = [A0+B0, A1+B1, A2+B2, A3+B3]
  v128a = _mm_add_epi32(v128a, v128b);
  // B = [A2+B2, A3+B3, 0, 0]
  v128b = _mm_srli_si128(v128a, 8);
  // A = [A0+B0+A2+B2, A1+B1+A3+B3, X, X]
  v128a = _mm_add_epi32(v128a, v128b);
  // B = [A1+B1+A3+B3, 0, 0, 0]
  v128b = _mm_srli_si128(v128a, 4);
  // A = [A0+B0+A2+B2+A1+B1+A3+B3, X, X, X]
  v128a = _mm_add_epi32(v128a, v128b);
  return _mm_extract_epi32(v128a, 0);
}

static void highbd_apply_temporal_filter(
    const uint16_t *frame1, const unsigned int stride, const uint16_t *frame2,
    const unsigned int stride2, const int block_width, const int block_height,
    const int *subblock_mses, unsigned int *accumulator, uint16_t *count,
    uint32_t *frame_sse, uint32_t *luma_sse_sum, int bd,
    const double inv_num_ref_pixels, const double decay_factor,
    const double inv_factor, const double weight_factor, double *d_factor,
    int tf_wgt_calc_lvl) {
  assert(((block_width == 16) || (block_width == 32)) &&
         ((block_height == 16) || (block_height == 32)));

  uint32_t acc_5x5_sse[BH][BW];

  if (block_width == 32) {
    get_squared_error_32x32_avx2(frame1, stride, frame2, stride2, block_width,
                                 block_height, frame_sse, SSE_STRIDE);
  } else {
    get_squared_error_16x16_avx2(frame1, stride, frame2, stride2, block_width,
                                 block_height, frame_sse, SSE_STRIDE);
  }

  __m256i vsrc[5];

  // Traverse 4 columns at a time
  // First and last columns will require padding
  int col;
  uint32_t *src = frame_sse;
  for (int i = 2; i < 5; i++) {
    xx_load_and_pad_left(src, &vsrc[i]);
    src += SSE_STRIDE;
  }

  // Copy first row to first 2 vectors
  vsrc[0] = vsrc[2];
  vsrc[1] = vsrc[2];

  for (int row = 0; row < block_height - 3; row++) {
    __m256i vsum1 = _mm256_add_epi32(vsrc[0], vsrc[1]);
    __m256i vsum2 = _mm256_add_epi32(vsrc[2], vsrc[3]);
    __m256i vsum3 = _mm256_add_epi32(vsum1, vsum2);
    __m256i vsum = _mm256_add_epi32(vsum3, vsrc[4]);

    for (int i = 0; i < 4; i++) {
      vsrc[i] = vsrc[i + 1];
    }

    xx_load_and_pad_left(src, &vsrc[4]);
    src += SSE_STRIDE;

    acc_5x5_sse[row][0] = xx_mask_and_hadd(vsum, 0);
    acc_5x5_sse[row][1] = xx_mask_and_hadd(vsum, 1);
    acc_5x5_sse[row][2] = xx_mask_and_hadd(vsum, 2);
    acc_5x5_sse[row][3] = xx_mask_and_hadd(vsum, 3);
  }
  for (int row = block_height - 3; row < block_height; row++) {
    __m256i vsum1 = _mm256_add_epi32(vsrc[0], vsrc[1]);
    __m256i vsum2 = _mm256_add_epi32(vsrc[2], vsrc[3]);
    __m256i vsum3 = _mm256_add_epi32(vsum1, vsum2);
    __m256i vsum = _mm256_add_epi32(vsum3, vsrc[4]);

    for (int i = 0; i < 4; i++) {
      vsrc[i] = vsrc[i + 1];
    }

    acc_5x5_sse[row][0] = xx_mask_and_hadd(vsum, 0);
    acc_5x5_sse[row][1] = xx_mask_and_hadd(vsum, 1);
    acc_5x5_sse[row][2] = xx_mask_and_hadd(vsum, 2);
    acc_5x5_sse[row][3] = xx_mask_and_hadd(vsum, 3);
  }
  for (col = 4; col < block_width - 4; col += 4) {
    src = frame_sse + col;

    // Load and pad(for first and last col) 3 rows from the top
    for (int i = 2; i < 5; i++) {
      vsrc[i] = _mm256_loadu_si256((__m256i *)src);
      src += SSE_STRIDE;
    }

    // Copy first row to first 2 vectors
    vsrc[0] = vsrc[2];
    vsrc[1] = vsrc[2];

    for (int row = 0; row < block_height - 3; row++) {
      __m256i vsum1 = _mm256_add_epi32(vsrc[0], vsrc[1]);
      __m256i vsum2 = _mm256_add_epi32(vsrc[2], vsrc[3]);
      __m256i vsum3 = _mm256_add_epi32(vsum1, vsum2);
      __m256i vsum = _mm256_add_epi32(vsum3, vsrc[4]);

      for (int i = 0; i < 4; i++) {
        vsrc[i] = vsrc[i + 1];
      }

      vsrc[4] = _mm256_loadu_si256((__m256i *)src);

      src += SSE_STRIDE;

      acc_5x5_sse[row][col] = xx_mask_and_hadd(vsum, 0);
      acc_5x5_sse[row][col + 1] = xx_mask_and_hadd(vsum, 1);
      acc_5x5_sse[row][col + 2] = xx_mask_and_hadd(vsum, 2);
      acc_5x5_sse[row][col + 3] = xx_mask_and_hadd(vsum, 3);
    }
    for (int row = block_height - 3; row < block_height; row++) {
      __m256i vsum1 = _mm256_add_epi32(vsrc[0], vsrc[1]);
      __m256i vsum2 = _mm256_add_epi32(vsrc[2], vsrc[3]);
      __m256i vsum3 = _mm256_add_epi32(vsum1, vsum2);
      __m256i vsum = _mm256_add_epi32(vsum3, vsrc[4]);

      for (int i = 0; i < 4; i++) {
        vsrc[i] = vsrc[i + 1];
      }

      acc_5x5_sse[row][col] = xx_mask_and_hadd(vsum, 0);
      acc_5x5_sse[row][col + 1] = xx_mask_and_hadd(vsum, 1);
      acc_5x5_sse[row][col + 2] = xx_mask_and_hadd(vsum, 2);
      acc_5x5_sse[row][col + 3] = xx_mask_and_hadd(vsum, 3);
    }
  }

  src = frame_sse + col;

  // Load and pad(for first and last col) 3 rows from the top
  for (int i = 2; i < 5; i++) {
    xx_load_and_pad_right(src, &vsrc[i]);
    src += SSE_STRIDE;
  }

  // Copy first row to first 2 vectors
  vsrc[0] = vsrc[2];
  vsrc[1] = vsrc[2];

  for (int row = 0; row < block_height - 3; row++) {
    __m256i vsum1 = _mm256_add_epi32(vsrc[0], vsrc[1]);
    __m256i vsum2 = _mm256_add_epi32(vsrc[2], vsrc[3]);
    __m256i vsum3 = _mm256_add_epi32(vsum1, vsum2);
    __m256i vsum = _mm256_add_epi32(vsum3, vsrc[4]);

    for (int i = 0; i < 4; i++) {
      vsrc[i] = vsrc[i + 1];
    }

    xx_load_and_pad_right(src, &vsrc[4]);
    src += SSE_STRIDE;

    acc_5x5_sse[row][col] = xx_mask_and_hadd(vsum, 0);
    acc_5x5_sse[row][col + 1] = xx_mask_and_hadd(vsum, 1);
    acc_5x5_sse[row][col + 2] = xx_mask_and_hadd(vsum, 2);
    acc_5x5_sse[row][col + 3] = xx_mask_and_hadd(vsum, 3);
  }
  for (int row = block_height - 3; row < block_height; row++) {
    __m256i vsum1 = _mm256_add_epi32(vsrc[0], vsrc[1]);
    __m256i vsum2 = _mm256_add_epi32(vsrc[2], vsrc[3]);
    __m256i vsum3 = _mm256_add_epi32(vsum1, vsum2);
    __m256i vsum = _mm256_add_epi32(vsum3, vsrc[4]);

    for (int i = 0; i < 4; i++) {
      vsrc[i] = vsrc[i + 1];
    }

    acc_5x5_sse[row][col] = xx_mask_and_hadd(vsum, 0);
    acc_5x5_sse[row][col + 1] = xx_mask_and_hadd(vsum, 1);
    acc_5x5_sse[row][col + 2] = xx_mask_and_hadd(vsum, 2);
    acc_5x5_sse[row][col + 3] = xx_mask_and_hadd(vsum, 3);
  }

  double subblock_mses_scaled[4];
  double d_factor_decayed[4];
  for (int idx = 0; idx < 4; idx++) {
    subblock_mses_scaled[idx] = subblock_mses[idx] * inv_factor;
    d_factor_decayed[idx] = d_factor[idx] * decay_factor;
  }
  if (tf_wgt_calc_lvl == 0) {
    for (int i = 0, k = 0; i < block_height; i++) {
      const int y_blk_raster_offset = (i >= block_height / 2) * 2;
      for (int j = 0; j < block_width; j++, k++) {
        const int pixel_value = frame2[i * stride2 + j];
        uint32_t diff_sse = acc_5x5_sse[i][j] + luma_sse_sum[i * BW + j];

        // Scale down the difference for high bit depth input.
        diff_sse >>= ((bd - 8) * 2);

        const double window_error = diff_sse * inv_num_ref_pixels;
        const int subblock_idx = y_blk_raster_offset + (j >= block_width / 2);

        const double combined_error =
            weight_factor * window_error + subblock_mses_scaled[subblock_idx];

        double scaled_error = combined_error * d_factor_decayed[subblock_idx];
        scaled_error = AOMMIN(scaled_error, 7);
        const int weight = (int)(exp(-scaled_error) * TF_WEIGHT_SCALE);

        count[k] += weight;
        accumulator[k] += weight * pixel_value;
      }
    }
  } else {
    for (int i = 0, k = 0; i < block_height; i++) {
      const int y_blk_raster_offset = (i >= block_height / 2) * 2;
      for (int j = 0; j < block_width; j++, k++) {
        const int pixel_value = frame2[i * stride2 + j];
        uint32_t diff_sse = acc_5x5_sse[i][j] + luma_sse_sum[i * BW + j];

        // Scale down the difference for high bit depth input.
        diff_sse >>= ((bd - 8) * 2);

        const double window_error = diff_sse * inv_num_ref_pixels;
        const int subblock_idx = y_blk_raster_offset + (j >= block_width / 2);

        const double combined_error =
            weight_factor * window_error + subblock_mses_scaled[subblock_idx];

        double scaled_error = combined_error * d_factor_decayed[subblock_idx];
        scaled_error = AOMMIN(scaled_error, 7);
        const float fweight =
            approx_exp((float)-scaled_error) * TF_WEIGHT_SCALE;
        const int weight = iroundpf(fweight);

        count[k] += weight;
        accumulator[k] += weight * pixel_value;
      }
    }
  }
}

void av1_highbd_apply_temporal_filter_avx2(
    const YV12_BUFFER_CONFIG *frame_to_filter, const MACROBLOCKD *mbd,
    const BLOCK_SIZE block_size, const int mb_row, const int mb_col,
    const int num_planes, const double *noise_levels, const MV *subblock_mvs,
    const int *subblock_mses, const int q_factor, const int filter_strength,
    int tf_wgt_calc_lvl, const uint8_t *pred, uint32_t *accum,
    uint16_t *count) {
  const int is_high_bitdepth = frame_to_filter->flags & YV12_FLAG_HIGHBITDEPTH;
  assert(block_size == BLOCK_32X32 && "Only support 32x32 block with sse2!");
  assert(TF_WINDOW_LENGTH == 5 && "Only support window length 5 with sse2!");
  assert(num_planes >= 1 && num_planes <= MAX_MB_PLANE);
  (void)is_high_bitdepth;

  const int mb_height = block_size_high[block_size];
  const int mb_width = block_size_wide[block_size];
  const int frame_height = frame_to_filter->y_crop_height;
  const int frame_width = frame_to_filter->y_crop_width;
  const int min_frame_size = AOMMIN(frame_height, frame_width);
  // Variables to simplify combined error calculation.
  const double inv_factor = 1.0 / ((TF_WINDOW_BLOCK_BALANCE_WEIGHT + 1) *
                                   TF_SEARCH_ERROR_NORM_WEIGHT);
  const double weight_factor =
      (double)TF_WINDOW_BLOCK_BALANCE_WEIGHT * inv_factor;
  // Adjust filtering based on q.
  // Larger q -> stronger filtering -> larger weight.
  // Smaller q -> weaker filtering -> smaller weight.
  double q_decay = pow((double)q_factor / TF_Q_DECAY_THRESHOLD, 2);
  q_decay = CLIP(q_decay, 1e-5, 1);
  if (q_factor >= TF_QINDEX_CUTOFF) {
    // Max q_factor is 255, therefore the upper bound of q_decay is 8.
    // We do not need a clip here.
    q_decay = 0.5 * pow((double)q_factor / 64, 2);
  }
  // Smaller strength -> smaller filtering weight.
  double s_decay = pow((double)filter_strength / TF_STRENGTH_THRESHOLD, 2);
  s_decay = CLIP(s_decay, 1e-5, 1);
  double d_factor[4] = { 0 };
  uint32_t frame_sse[SSE_STRIDE * BH] = { 0 };
  uint32_t luma_sse_sum[BW * BH] = { 0 };
  uint16_t *pred1 = CONVERT_TO_SHORTPTR(pred);

  for (int subblock_idx = 0; subblock_idx < 4; subblock_idx++) {
    // Larger motion vector -> smaller filtering weight.
    const MV mv = subblock_mvs[subblock_idx];
    const double distance = sqrt(pow(mv.row, 2) + pow(mv.col, 2));
    double distance_threshold = min_frame_size * TF_SEARCH_DISTANCE_THRESHOLD;
    distance_threshold = AOMMAX(distance_threshold, 1);
    d_factor[subblock_idx] = distance / distance_threshold;
    d_factor[subblock_idx] = AOMMAX(d_factor[subblock_idx], 1);
  }

  // Handle planes in sequence.
  int plane_offset = 0;
  for (int plane = 0; plane < num_planes; ++plane) {
    const uint32_t plane_h = mb_height >> mbd->plane[plane].subsampling_y;
    const uint32_t plane_w = mb_width >> mbd->plane[plane].subsampling_x;
    const uint32_t frame_stride = frame_to_filter->strides[plane == 0 ? 0 : 1];
    const int frame_offset = mb_row * plane_h * frame_stride + mb_col * plane_w;

    const uint16_t *ref =
        CONVERT_TO_SHORTPTR(frame_to_filter->buffers[plane]) + frame_offset;
    const int ss_x_shift =
        mbd->plane[plane].subsampling_x - mbd->plane[AOM_PLANE_Y].subsampling_x;
    const int ss_y_shift =
        mbd->plane[plane].subsampling_y - mbd->plane[AOM_PLANE_Y].subsampling_y;
    const int num_ref_pixels = TF_WINDOW_LENGTH * TF_WINDOW_LENGTH +
                               ((plane) ? (1 << (ss_x_shift + ss_y_shift)) : 0);
    const double inv_num_ref_pixels = 1.0 / num_ref_pixels;
    // Larger noise -> larger filtering weight.
    const double n_decay = 0.5 + log(2 * noise_levels[plane] + 5.0);
    // Decay factors for non-local mean approach.
    const double decay_factor = 1 / (n_decay * q_decay * s_decay);

    // Filter U-plane and V-plane using Y-plane. This is because motion
    // search is only done on Y-plane, so the information from Y-plane
    // will be more accurate. The luma sse sum is reused in both chroma
    // planes.
    if (plane == AOM_PLANE_U) {
      for (unsigned int i = 0, k = 0; i < plane_h; i++) {
        for (unsigned int j = 0; j < plane_w; j++, k++) {
          for (int ii = 0; ii < (1 << ss_y_shift); ++ii) {
            for (int jj = 0; jj < (1 << ss_x_shift); ++jj) {
              const int yy = (i << ss_y_shift) + ii;  // Y-coord on Y-plane.
              const int xx = (j << ss_x_shift) + jj;  // X-coord on Y-plane.
              luma_sse_sum[i * BW + j] += frame_sse[yy * SSE_STRIDE + xx + 2];
            }
          }
        }
      }
    }

    highbd_apply_temporal_filter(
        ref, frame_stride, pred1 + plane_offset, plane_w, plane_w, plane_h,
        subblock_mses, accum + plane_offset, count + plane_offset, frame_sse,
        luma_sse_sum, mbd->bd, inv_num_ref_pixels, decay_factor, inv_factor,
        weight_factor, d_factor, tf_wgt_calc_lvl);
    plane_offset += plane_h * plane_w;
  }
}
