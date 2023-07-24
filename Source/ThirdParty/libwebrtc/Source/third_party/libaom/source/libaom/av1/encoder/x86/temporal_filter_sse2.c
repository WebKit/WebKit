/*
 * Copyright (c) 2019, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>
#include <emmintrin.h>

#include "config/av1_rtcd.h"
#include "aom_dsp/mathutils.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/temporal_filter.h"

// For the squared error buffer, keep a padding for 4 samples
#define SSE_STRIDE (BW + 4)

DECLARE_ALIGNED(32, static const uint32_t, sse_bytemask_2x4[4][2][4]) = {
  { { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF },
    { 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 } },
  { { 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF },
    { 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x00000000 } },
  { { 0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF },
    { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000 } },
  { { 0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF },
    { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF } }
};

static void get_squared_error(const uint8_t *frame1, const unsigned int stride,
                              const uint8_t *frame2, const unsigned int stride2,
                              const int block_width, const int block_height,
                              uint16_t *frame_sse,
                              const unsigned int dst_stride) {
  const uint8_t *src1 = frame1;
  const uint8_t *src2 = frame2;
  uint16_t *dst = frame_sse;

  for (int i = 0; i < block_height; i++) {
    for (int j = 0; j < block_width; j += 16) {
      // Set zero to uninitialized memory to avoid uninitialized loads later
      *(int *)(dst) = _mm_cvtsi128_si32(_mm_setzero_si128());

      __m128i vsrc1 = _mm_loadu_si128((__m128i *)(src1 + j));
      __m128i vsrc2 = _mm_loadu_si128((__m128i *)(src2 + j));

      __m128i vmax = _mm_max_epu8(vsrc1, vsrc2);
      __m128i vmin = _mm_min_epu8(vsrc1, vsrc2);
      __m128i vdiff = _mm_subs_epu8(vmax, vmin);

      __m128i vzero = _mm_setzero_si128();
      __m128i vdiff1 = _mm_unpacklo_epi8(vdiff, vzero);
      __m128i vdiff2 = _mm_unpackhi_epi8(vdiff, vzero);

      __m128i vres1 = _mm_mullo_epi16(vdiff1, vdiff1);
      __m128i vres2 = _mm_mullo_epi16(vdiff2, vdiff2);

      _mm_storeu_si128((__m128i *)(dst + j + 2), vres1);
      _mm_storeu_si128((__m128i *)(dst + j + 10), vres2);
    }

    // Set zero to uninitialized memory to avoid uninitialized loads later
    *(int *)(dst + block_width + 2) = _mm_cvtsi128_si32(_mm_setzero_si128());

    src1 += stride;
    src2 += stride2;
    dst += dst_stride;
  }
}

static void xx_load_and_pad(uint16_t *src, __m128i *dstvec, int col,
                            int block_width) {
  __m128i vtmp = _mm_loadu_si128((__m128i *)src);
  __m128i vzero = _mm_setzero_si128();
  __m128i vtmp1 = _mm_unpacklo_epi16(vtmp, vzero);
  __m128i vtmp2 = _mm_unpackhi_epi16(vtmp, vzero);
  // For the first column, replicate the first element twice to the left
  dstvec[0] = (col) ? vtmp1 : _mm_shuffle_epi32(vtmp1, 0xEA);
  // For the last column, replicate the last element twice to the right
  dstvec[1] = (col < block_width - 4) ? vtmp2 : _mm_shuffle_epi32(vtmp2, 0x54);
}

static int32_t xx_mask_and_hadd(__m128i vsum1, __m128i vsum2, int i) {
  __m128i veca, vecb;
  // Mask and obtain the required 5 values inside the vector
  veca = _mm_and_si128(vsum1, *(__m128i *)sse_bytemask_2x4[i][0]);
  vecb = _mm_and_si128(vsum2, *(__m128i *)sse_bytemask_2x4[i][1]);
  // A = [A0+B0, A1+B1, A2+B2, A3+B3]
  veca = _mm_add_epi32(veca, vecb);
  // B = [A2+B2, A3+B3, 0, 0]
  vecb = _mm_srli_si128(veca, 8);
  // A = [A0+B0+A2+B2, A1+B1+A3+B3, X, X]
  veca = _mm_add_epi32(veca, vecb);
  // B = [A1+B1+A3+B3, 0, 0, 0]
  vecb = _mm_srli_si128(veca, 4);
  // A = [A0+B0+A2+B2+A1+B1+A3+B3, X, X, X]
  veca = _mm_add_epi32(veca, vecb);
  return _mm_cvtsi128_si32(veca);
}

static void apply_temporal_filter(
    const uint8_t *frame1, const unsigned int stride, const uint8_t *frame2,
    const unsigned int stride2, const int block_width, const int block_height,
    const int *subblock_mses, unsigned int *accumulator, uint16_t *count,
    uint16_t *frame_sse, uint32_t *luma_sse_sum,
    const double inv_num_ref_pixels, const double decay_factor,
    const double inv_factor, const double weight_factor, double *d_factor,
    int tf_wgt_calc_lvl) {
  assert(((block_width == 16) || (block_width == 32)) &&
         ((block_height == 16) || (block_height == 32)));

  uint32_t acc_5x5_sse[BH][BW];

  get_squared_error(frame1, stride, frame2, stride2, block_width, block_height,
                    frame_sse, SSE_STRIDE);

  __m128i vsrc[5][2];

  // Traverse 4 columns at a time
  // First and last columns will require padding
  for (int col = 0; col < block_width; col += 4) {
    uint16_t *src = frame_sse + col;

    // Load and pad(for first and last col) 3 rows from the top
    for (int i = 2; i < 5; i++) {
      xx_load_and_pad(src, vsrc[i], col, block_width);
      src += SSE_STRIDE;
    }

    // Padding for top 2 rows
    vsrc[0][0] = vsrc[2][0];
    vsrc[0][1] = vsrc[2][1];
    vsrc[1][0] = vsrc[2][0];
    vsrc[1][1] = vsrc[2][1];

    for (int row = 0; row < block_height; row++) {
      __m128i vsum1 = _mm_setzero_si128();
      __m128i vsum2 = _mm_setzero_si128();

      // Add 5 consecutive rows
      for (int i = 0; i < 5; i++) {
        vsum1 = _mm_add_epi32(vsrc[i][0], vsum1);
        vsum2 = _mm_add_epi32(vsrc[i][1], vsum2);
      }

      // Push all elements by one element to the top
      for (int i = 0; i < 4; i++) {
        vsrc[i][0] = vsrc[i + 1][0];
        vsrc[i][1] = vsrc[i + 1][1];
      }

      if (row <= block_height - 4) {
        // Load next row
        xx_load_and_pad(src, vsrc[4], col, block_width);
        src += SSE_STRIDE;
      } else {
        // Padding for bottom 2 rows
        vsrc[4][0] = vsrc[3][0];
        vsrc[4][1] = vsrc[3][1];
      }

      // Accumulate the sum horizontally
      for (int i = 0; i < 4; i++) {
        acc_5x5_sse[row][col + i] = xx_mask_and_hadd(vsum1, vsum2, i);
      }
    }
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

void av1_apply_temporal_filter_sse2(
    const YV12_BUFFER_CONFIG *frame_to_filter, const MACROBLOCKD *mbd,
    const BLOCK_SIZE block_size, const int mb_row, const int mb_col,
    const int num_planes, const double *noise_levels, const MV *subblock_mvs,
    const int *subblock_mses, const int q_factor, const int filter_strength,
    int tf_wgt_calc_lvl, const uint8_t *pred, uint32_t *accum,
    uint16_t *count) {
  const int is_high_bitdepth = frame_to_filter->flags & YV12_FLAG_HIGHBITDEPTH;
  assert(block_size == BLOCK_32X32 && "Only support 32x32 block with sse2!");
  assert(TF_WINDOW_LENGTH == 5 && "Only support window length 5 with sse2!");
  assert(!is_high_bitdepth && "Only support low bit-depth with sse2!");
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
  uint16_t frame_sse[SSE_STRIDE * BH] = { 0 };
  uint32_t luma_sse_sum[BW * BH] = { 0 };

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

    const uint8_t *ref = frame_to_filter->buffers[plane] + frame_offset;
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

    apply_temporal_filter(ref, frame_stride, pred + plane_offset, plane_w,
                          plane_w, plane_h, subblock_mses, accum + plane_offset,
                          count + plane_offset, frame_sse, luma_sse_sum,
                          inv_num_ref_pixels, decay_factor, inv_factor,
                          weight_factor, d_factor, tf_wgt_calc_lvl);
    plane_offset += plane_h * plane_w;
  }
}
