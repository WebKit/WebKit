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

#include "config/aom_config.h"
#include "config/av1_rtcd.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/temporal_filter.h"
#include "aom_dsp/mathutils.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/sum_neon.h"

static inline void get_squared_error(
    const uint16_t *frame1, const uint32_t stride1, const uint16_t *frame2,
    const uint32_t stride2, const uint32_t block_width,
    const uint32_t block_height, uint32_t *frame_sse,
    const unsigned int dst_stride) {
  uint32_t *dst = frame_sse;

  uint32_t i = 0;
  do {
    uint32_t j = 0;
    do {
      uint16x8_t s = vld1q_u16(frame1 + i * stride1 + j);
      uint16x8_t r = vld1q_u16(frame2 + i * stride2 + j);

      uint16x8_t abs_diff = vabdq_u16(s, r);
      uint32x4_t sse_lo =
          vmull_u16(vget_low_u16(abs_diff), vget_low_u16(abs_diff));
      uint32x4_t sse_hi =
          vmull_u16(vget_high_u16(abs_diff), vget_high_u16(abs_diff));

      vst1q_u32(dst + j, sse_lo);
      vst1q_u32(dst + j + 4, sse_hi);

      j += 8;
    } while (j < block_width);

    dst += dst_stride;
    i++;
  } while (i < block_height);
}

static uint32_t sum_kernel5x5_mask_single(const uint32x4_t vsrc[5][2],
                                          const uint32x4_t mask_single) {
  uint32x4_t vsums = vmulq_u32(vsrc[0][0], mask_single);
  vsums = vmlaq_u32(vsums, vsrc[1][0], mask_single);
  vsums = vmlaq_u32(vsums, vsrc[2][0], mask_single);
  vsums = vmlaq_u32(vsums, vsrc[3][0], mask_single);
  vsums = vmlaq_u32(vsums, vsrc[4][0], mask_single);
  return horizontal_add_u32x4(vsums);
}

static uint32x4_t sum_kernel5x5_mask_double(const uint32x4_t vsrc[5][2],
                                            const uint32x4_t mask1,
                                            const uint32x4_t mask2) {
  uint32x4_t vsums = vmulq_u32(vsrc[0][0], mask1);
  vsums = vmlaq_u32(vsums, vsrc[1][0], mask1);
  vsums = vmlaq_u32(vsums, vsrc[2][0], mask1);
  vsums = vmlaq_u32(vsums, vsrc[3][0], mask1);
  vsums = vmlaq_u32(vsums, vsrc[4][0], mask1);
  vsums = vmlaq_u32(vsums, vsrc[0][1], mask2);
  vsums = vmlaq_u32(vsums, vsrc[1][1], mask2);
  vsums = vmlaq_u32(vsums, vsrc[2][1], mask2);
  vsums = vmlaq_u32(vsums, vsrc[3][1], mask2);
  vsums = vmlaq_u32(vsums, vsrc[4][1], mask2);
  return vsums;
}

static void highbd_apply_temporal_filter(
    const uint16_t *frame, const unsigned int stride,
    const uint32_t block_width, const uint32_t block_height,
    const int *subblock_mses, unsigned int *accumulator, uint16_t *count,
    const uint32_t *frame_sse, const uint32_t frame_sse_stride,
    const uint32_t *luma_sse_sum, const double inv_num_ref_pixels,
    const double decay_factor, const double inv_factor,
    const double weight_factor, const double *d_factor, int tf_wgt_calc_lvl,
    int bd) {
  assert(((block_width == 16) || (block_width == 32)) &&
         ((block_height == 16) || (block_height == 32)));

  uint32_t acc_5x5_neon[BH][BW] = { 0 };
  const int half_window = TF_WINDOW_LENGTH >> 1;

  uint32x4_t vsrc[5][2] = { 0 };
  const uint32x4_t k0000 = vdupq_n_u32(0);
  const uint32x4_t k1111 = vdupq_n_u32(1);
  const uint32_t k3110_u32[4] = { 0, 1, 1, 3 };
  const uint32_t k2111_u32[4] = { 1, 1, 1, 2 };
  const uint32_t k1112_u32[4] = { 2, 1, 1, 1 };
  const uint32_t k0113_u32[4] = { 3, 1, 1, 0 };
  const uint32x4_t k3110 = vld1q_u32(k3110_u32);
  const uint32x4_t k2111 = vld1q_u32(k2111_u32);
  const uint32x4_t k1112 = vld1q_u32(k1112_u32);
  const uint32x4_t k0113 = vld1q_u32(k0113_u32);

  uint32x4_t vmask1[4], vmask2[4];
  vmask1[0] = k1111;
  vmask2[0] = vextq_u32(k1111, k0000, 3);
  vmask1[1] = vextq_u32(k0000, k1111, 3);
  vmask2[1] = vextq_u32(k1111, k0000, 2);
  vmask1[2] = vextq_u32(k0000, k1111, 2);
  vmask2[2] = vextq_u32(k1111, k0000, 1);
  vmask1[3] = vextq_u32(k0000, k1111, 1);
  vmask2[3] = k1111;

  uint32_t row = 0;
  do {
    uint32_t col = 0;
    const uint32_t *src = frame_sse + row * frame_sse_stride;
    if (row == 0) {
      vsrc[2][0] = vld1q_u32(src);
      vsrc[3][0] = vld1q_u32(src + frame_sse_stride);
      vsrc[4][0] = vld1q_u32(src + 2 * frame_sse_stride);

      // First 2 rows of the 5x5 matrix are padded from the 1st.
      vsrc[0][0] = vsrc[2][0];
      vsrc[1][0] = vsrc[2][0];
    } else if (row == 1) {
      vsrc[1][0] = vld1q_u32(src - frame_sse_stride);
      vsrc[2][0] = vld1q_u32(src);
      vsrc[3][0] = vld1q_u32(src + frame_sse_stride);
      vsrc[4][0] = vld1q_u32(src + 2 * frame_sse_stride);

      // First row of the 5x5 matrix are padded from the 1st.
      vsrc[0][0] = vsrc[1][0];
    } else if (row == block_height - 2) {
      vsrc[0][0] = vld1q_u32(src - 2 * frame_sse_stride);
      vsrc[1][0] = vld1q_u32(src - frame_sse_stride);
      vsrc[2][0] = vld1q_u32(src);
      vsrc[3][0] = vld1q_u32(src + frame_sse_stride);

      // Last row of the 5x5 matrix are padded from the one before.
      vsrc[4][0] = vsrc[3][0];
    } else if (row == block_height - 1) {
      vsrc[0][0] = vld1q_u32(src - 2 * frame_sse_stride);
      vsrc[1][0] = vld1q_u32(src - frame_sse_stride);
      vsrc[2][0] = vld1q_u32(src);

      // Last 2 rows of the 5x5 matrix are padded from the 3rd.
      vsrc[3][0] = vsrc[2][0];
      vsrc[4][0] = vsrc[2][0];
    } else {
      vsrc[0][0] = vld1q_u32(src - 2 * frame_sse_stride);
      vsrc[1][0] = vld1q_u32(src - frame_sse_stride);
      vsrc[2][0] = vld1q_u32(src);
      vsrc[3][0] = vld1q_u32(src + frame_sse_stride);
      vsrc[4][0] = vld1q_u32(src + 2 * frame_sse_stride);
    }

    acc_5x5_neon[row][0] = sum_kernel5x5_mask_single(vsrc, k0113);
    acc_5x5_neon[row][1] = sum_kernel5x5_mask_single(vsrc, k1112);

    col += 4;
    src += 4;
    // Traverse 4 columns at a time
    do {
      if (row == 0) {
        vsrc[2][1] = vld1q_u32(src);
        vsrc[3][1] = vld1q_u32(src + frame_sse_stride);
        vsrc[4][1] = vld1q_u32(src + 2 * frame_sse_stride);

        // First 2 rows of the 5x5 matrix are padded from the 1st.
        vsrc[0][1] = vsrc[2][1];
        vsrc[1][1] = vsrc[2][1];
      } else if (row == 1) {
        vsrc[1][1] = vld1q_u32(src - frame_sse_stride);
        vsrc[2][1] = vld1q_u32(src);
        vsrc[3][1] = vld1q_u32(src + frame_sse_stride);
        vsrc[4][1] = vld1q_u32(src + 2 * frame_sse_stride);

        // First row of the 5x5 matrix are padded from the 1st.
        vsrc[0][1] = vsrc[1][1];
      } else if (row == block_height - 2) {
        vsrc[0][1] = vld1q_u32(src - 2 * frame_sse_stride);
        vsrc[1][1] = vld1q_u32(src - frame_sse_stride);
        vsrc[2][1] = vld1q_u32(src);
        vsrc[3][1] = vld1q_u32(src + frame_sse_stride);

        // Last row of the 5x5 matrix are padded from the one before.
        vsrc[4][1] = vsrc[3][1];
      } else if (row == block_height - 1) {
        vsrc[0][1] = vld1q_u32(src - 2 * frame_sse_stride);
        vsrc[1][1] = vld1q_u32(src - frame_sse_stride);
        vsrc[2][1] = vld1q_u32(src);

        // Last 2 rows of the 5x5 matrix are padded from the 3rd.
        vsrc[3][1] = vsrc[2][1];
        vsrc[4][1] = vsrc[2][1];
      } else {
        vsrc[0][1] = vld1q_u32(src - 2 * frame_sse_stride);
        vsrc[1][1] = vld1q_u32(src - frame_sse_stride);
        vsrc[2][1] = vld1q_u32(src);
        vsrc[3][1] = vld1q_u32(src + frame_sse_stride);
        vsrc[4][1] = vld1q_u32(src + 2 * frame_sse_stride);
      }

      uint32x4_t sums[4];
      sums[0] = sum_kernel5x5_mask_double(vsrc, vmask1[0], vmask2[0]);
      sums[1] = sum_kernel5x5_mask_double(vsrc, vmask1[1], vmask2[1]);
      sums[2] = sum_kernel5x5_mask_double(vsrc, vmask1[2], vmask2[2]);
      sums[3] = sum_kernel5x5_mask_double(vsrc, vmask1[3], vmask2[3]);
      vst1q_u32(&acc_5x5_neon[row][col - half_window],
                horizontal_add_4d_u32x4(sums));

      vsrc[0][0] = vsrc[0][1];
      vsrc[1][0] = vsrc[1][1];
      vsrc[2][0] = vsrc[2][1];
      vsrc[3][0] = vsrc[3][1];
      vsrc[4][0] = vsrc[4][1];

      src += 4;
      col += 4;
    } while (col <= block_width - 4);

    acc_5x5_neon[row][col - half_window] =
        sum_kernel5x5_mask_single(vsrc, k2111);
    acc_5x5_neon[row][col - half_window + 1] =
        sum_kernel5x5_mask_single(vsrc, k3110);

    row++;
  } while (row < block_height);

  // Perform filtering.
  if (tf_wgt_calc_lvl == 0) {
    for (unsigned int i = 0, k = 0; i < block_height; i++) {
      for (unsigned int j = 0; j < block_width; j++, k++) {
        const int pixel_value = frame[i * stride + j];
        // Scale down the difference for high bit depth input.
        const uint32_t diff_sse =
            (acc_5x5_neon[i][j] + luma_sse_sum[i * BW + j]) >> ((bd - 8) * 2);

        const double window_error = diff_sse * inv_num_ref_pixels;
        const int subblock_idx =
            (i >= block_height / 2) * 2 + (j >= block_width / 2);
        const double block_error = (double)subblock_mses[subblock_idx];
        const double combined_error =
            weight_factor * window_error + block_error * inv_factor;
        // Compute filter weight.
        double scaled_error =
            combined_error * d_factor[subblock_idx] * decay_factor;
        scaled_error = AOMMIN(scaled_error, 7);
        const int weight = (int)(exp(-scaled_error) * TF_WEIGHT_SCALE);
        accumulator[k] += weight * pixel_value;
        count[k] += weight;
      }
    }
  } else {
    for (unsigned int i = 0, k = 0; i < block_height; i++) {
      for (unsigned int j = 0; j < block_width; j++, k++) {
        const int pixel_value = frame[i * stride + j];
        // Scale down the difference for high bit depth input.
        const uint32_t diff_sse =
            (acc_5x5_neon[i][j] + luma_sse_sum[i * BW + j]) >> ((bd - 8) * 2);

        const double window_error = diff_sse * inv_num_ref_pixels;
        const int subblock_idx =
            (i >= block_height / 2) * 2 + (j >= block_width / 2);
        const double block_error = (double)subblock_mses[subblock_idx];
        const double combined_error =
            weight_factor * window_error + block_error * inv_factor;
        // Compute filter weight.
        double scaled_error =
            combined_error * d_factor[subblock_idx] * decay_factor;
        scaled_error = AOMMIN(scaled_error, 7);
        const float fweight =
            approx_exp((float)-scaled_error) * TF_WEIGHT_SCALE;
        const int weight = iroundpf(fweight);
        accumulator[k] += weight * pixel_value;
        count[k] += weight;
      }
    }
  }
}

void av1_highbd_apply_temporal_filter_neon(
    const YV12_BUFFER_CONFIG *frame_to_filter, const MACROBLOCKD *mbd,
    const BLOCK_SIZE block_size, const int mb_row, const int mb_col,
    const int num_planes, const double *noise_levels, const MV *subblock_mvs,
    const int *subblock_mses, const int q_factor, const int filter_strength,
    int tf_wgt_calc_lvl, const uint8_t *pred8, uint32_t *accum,
    uint16_t *count) {
  const int is_high_bitdepth = frame_to_filter->flags & YV12_FLAG_HIGHBITDEPTH;
  assert(TF_WINDOW_LENGTH == 5 && "Only support window length 5 with Neon!");
  assert(num_planes >= 1 && num_planes <= MAX_MB_PLANE);
  (void)is_high_bitdepth;
  assert(is_high_bitdepth);

  // Block information.
  const int mb_height = block_size_high[block_size];
  const int mb_width = block_size_wide[block_size];
  // Frame information.
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
  uint32_t frame_sse[BW * BH] = { 0 };
  uint32_t luma_sse_sum[BW * BH] = { 0 };
  uint16_t *pred = CONVERT_TO_SHORTPTR(pred8);

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
    const uint32_t frame_stride =
        frame_to_filter->strides[plane == AOM_PLANE_Y ? 0 : 1];
    const uint32_t frame_sse_stride = plane_w;
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
      for (unsigned int i = 0; i < plane_h; i++) {
        for (unsigned int j = 0; j < plane_w; j++) {
          for (int ii = 0; ii < (1 << ss_y_shift); ++ii) {
            for (int jj = 0; jj < (1 << ss_x_shift); ++jj) {
              const int yy = (i << ss_y_shift) + ii;  // Y-coord on Y-plane.
              const int xx = (j << ss_x_shift) + jj;  // X-coord on Y-plane.
              const int ww = frame_sse_stride
                             << ss_x_shift;  // Width of Y-plane.
              luma_sse_sum[i * BW + j] += frame_sse[yy * ww + xx];
            }
          }
        }
      }
    }
    get_squared_error(ref, frame_stride, pred + plane_offset, plane_w, plane_w,
                      plane_h, frame_sse, frame_sse_stride);

    highbd_apply_temporal_filter(
        pred + plane_offset, plane_w, plane_w, plane_h, subblock_mses,
        accum + plane_offset, count + plane_offset, frame_sse, frame_sse_stride,
        luma_sse_sum, inv_num_ref_pixels, decay_factor, inv_factor,
        weight_factor, d_factor, tf_wgt_calc_lvl, mbd->bd);

    plane_offset += plane_h * plane_w;
  }
}

double av1_highbd_estimate_noise_from_single_plane_neon(const uint16_t *src,
                                                        int height, int width,
                                                        int stride,
                                                        int bitdepth,
                                                        int edge_thresh) {
  uint16x8_t thresh = vdupq_n_u16(edge_thresh);
  uint64x2_t acc = vdupq_n_u64(0);
  // Count is in theory positive as it counts the number of times we're under
  // the threshold, but it will be counted negatively in order to make best use
  // of the vclt instruction, which sets every bit of a lane to 1 when the
  // condition is true.
  int32x4_t count = vdupq_n_s32(0);
  int final_count = 0;
  uint64_t final_acc = 0;
  const uint16_t *src_start = src + stride + 1;
  int h = 1;

  do {
    int w = 1;
    const uint16_t *src_ptr = src_start;

    while (w <= (width - 1) - 8) {
      uint16x8_t mat[3][3];
      mat[0][0] = vld1q_u16(src_ptr - stride - 1);
      mat[0][1] = vld1q_u16(src_ptr - stride);
      mat[0][2] = vld1q_u16(src_ptr - stride + 1);
      mat[1][0] = vld1q_u16(src_ptr - 1);
      mat[1][1] = vld1q_u16(src_ptr);
      mat[1][2] = vld1q_u16(src_ptr + 1);
      mat[2][0] = vld1q_u16(src_ptr + stride - 1);
      mat[2][1] = vld1q_u16(src_ptr + stride);
      mat[2][2] = vld1q_u16(src_ptr + stride + 1);

      // Compute Sobel gradients.
      uint16x8_t gxa = vaddq_u16(mat[0][0], mat[2][0]);
      uint16x8_t gxb = vaddq_u16(mat[0][2], mat[2][2]);
      gxa = vaddq_u16(gxa, vaddq_u16(mat[1][0], mat[1][0]));
      gxb = vaddq_u16(gxb, vaddq_u16(mat[1][2], mat[1][2]));

      uint16x8_t gya = vaddq_u16(mat[0][0], mat[0][2]);
      uint16x8_t gyb = vaddq_u16(mat[2][0], mat[2][2]);
      gya = vaddq_u16(gya, vaddq_u16(mat[0][1], mat[0][1]));
      gyb = vaddq_u16(gyb, vaddq_u16(mat[2][1], mat[2][1]));

      uint16x8_t ga = vabaq_u16(vabdq_u16(gxa, gxb), gya, gyb);
      ga = vrshlq_u16(ga, vdupq_n_s16(8 - bitdepth));

      // Check which vector elements are under the threshold. The Laplacian is
      // then unconditionnally computed and we accumulate zeros if we're not
      // under the threshold. This is much faster than using an if statement.
      uint16x8_t thresh_u16 = vcltq_u16(ga, thresh);

      uint16x8_t center = vshlq_n_u16(mat[1][1], 2);

      uint16x8_t adj0 = vaddq_u16(mat[0][1], mat[2][1]);
      uint16x8_t adj1 = vaddq_u16(mat[1][0], mat[1][2]);
      uint16x8_t adj = vaddq_u16(adj0, adj1);
      adj = vaddq_u16(adj, adj);

      uint16x8_t diag0 = vaddq_u16(mat[0][0], mat[0][2]);
      uint16x8_t diag1 = vaddq_u16(mat[2][0], mat[2][2]);
      uint16x8_t diag = vaddq_u16(diag0, diag1);

      uint16x8_t v = vabdq_u16(vaddq_u16(center, diag), adj);
      v = vandq_u16(vrshlq_u16(v, vdupq_n_s16(8 - bitdepth)), thresh_u16);
      uint32x4_t v_u32 = vpaddlq_u16(v);

      acc = vpadalq_u32(acc, v_u32);
      // Add -1 for each lane where the gradient is under the threshold.
      count = vpadalq_s16(count, vreinterpretq_s16_u16(thresh_u16));

      w += 8;
      src_ptr += 8;
    }

    if (w <= (width - 1) - 4) {
      uint16x4_t mat[3][3];
      mat[0][0] = vld1_u16(src_ptr - stride - 1);
      mat[0][1] = vld1_u16(src_ptr - stride);
      mat[0][2] = vld1_u16(src_ptr - stride + 1);
      mat[1][0] = vld1_u16(src_ptr - 1);
      mat[1][1] = vld1_u16(src_ptr);
      mat[1][2] = vld1_u16(src_ptr + 1);
      mat[2][0] = vld1_u16(src_ptr + stride - 1);
      mat[2][1] = vld1_u16(src_ptr + stride);
      mat[2][2] = vld1_u16(src_ptr + stride + 1);

      // Compute Sobel gradients.
      uint16x4_t gxa = vadd_u16(mat[0][0], mat[2][0]);
      uint16x4_t gxb = vadd_u16(mat[0][2], mat[2][2]);
      gxa = vadd_u16(gxa, vadd_u16(mat[1][0], mat[1][0]));
      gxb = vadd_u16(gxb, vadd_u16(mat[1][2], mat[1][2]));

      uint16x4_t gya = vadd_u16(mat[0][0], mat[0][2]);
      uint16x4_t gyb = vadd_u16(mat[2][0], mat[2][2]);
      gya = vadd_u16(gya, vadd_u16(mat[0][1], mat[0][1]));
      gyb = vadd_u16(gyb, vadd_u16(mat[2][1], mat[2][1]));

      uint16x4_t ga = vaba_u16(vabd_u16(gxa, gxb), gya, gyb);
      ga = vrshl_u16(ga, vdup_n_s16(8 - bitdepth));

      // Check which vector elements are under the threshold. The Laplacian is
      // then unconditionnally computed and we accumulate zeros if we're not
      // under the threshold. This is much faster than using an if statement.
      uint16x4_t thresh_u16 = vclt_u16(ga, vget_low_u16(thresh));

      uint16x4_t center = vshl_n_u16(mat[1][1], 2);

      uint16x4_t adj0 = vadd_u16(mat[0][1], mat[2][1]);
      uint16x4_t adj1 = vadd_u16(mat[1][0], mat[1][2]);
      uint16x4_t adj = vadd_u16(adj0, adj1);
      adj = vadd_u16(adj, adj);

      uint16x4_t diag0 = vadd_u16(mat[0][0], mat[0][2]);
      uint16x4_t diag1 = vadd_u16(mat[2][0], mat[2][2]);
      uint16x4_t diag = vadd_u16(diag0, diag1);

      uint16x4_t v = vabd_u16(vadd_u16(center, diag), adj);
      v = vand_u16(v, thresh_u16);
      uint32x4_t v_u32 = vmovl_u16(vrshl_u16(v, vdup_n_s16(8 - bitdepth)));

      acc = vpadalq_u32(acc, v_u32);
      // Add -1 for each lane where the gradient is under the threshold.
      count = vaddw_s16(count, vreinterpret_s16_u16(thresh_u16));

      w += 4;
      src_ptr += 4;
    }

    while (w < width - 1) {
      int mat[3][3];
      mat[0][0] = *(src_ptr - stride - 1);
      mat[0][1] = *(src_ptr - stride);
      mat[0][2] = *(src_ptr - stride + 1);
      mat[1][0] = *(src_ptr - 1);
      mat[1][1] = *(src_ptr);
      mat[1][2] = *(src_ptr + 1);
      mat[2][0] = *(src_ptr + stride - 1);
      mat[2][1] = *(src_ptr + stride);
      mat[2][2] = *(src_ptr + stride + 1);

      // Compute Sobel gradients.
      const int gx = (mat[0][0] - mat[0][2]) + (mat[2][0] - mat[2][2]) +
                     2 * (mat[1][0] - mat[1][2]);
      const int gy = (mat[0][0] - mat[2][0]) + (mat[0][2] - mat[2][2]) +
                     2 * (mat[0][1] - mat[2][1]);
      const int ga = ROUND_POWER_OF_TWO(abs(gx) + abs(gy), bitdepth - 8);

      // Accumulate Laplacian.
      const int is_under = ga < edge_thresh;
      const int v = 4 * mat[1][1] -
                    2 * (mat[0][1] + mat[2][1] + mat[1][0] + mat[1][2]) +
                    (mat[0][0] + mat[0][2] + mat[2][0] + mat[2][2]);
      final_acc += ROUND_POWER_OF_TWO(abs(v), bitdepth - 8) * is_under;
      final_count += is_under;

      src_ptr++;
      w++;
    }
    src_start += stride;
  } while (++h < height - 1);

  // We counted negatively, so subtract to get the final value.
  final_count -= horizontal_add_s32x4(count);
  final_acc += horizontal_add_u64x2(acc);
  return (final_count < 16)
             ? -1.0
             : (double)final_acc / (6 * final_count) * SQRT_PI_BY_2;
}
