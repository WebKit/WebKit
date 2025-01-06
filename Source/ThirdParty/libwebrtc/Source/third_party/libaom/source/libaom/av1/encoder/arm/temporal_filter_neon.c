/*
 * Copyright (c) 2022, Alliance for Open Media. All rights reserved.
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

// For the squared error buffer, add padding for 4 samples.
#define SSE_STRIDE (BW + 4)

// When using vld1q_u16_x4 compilers may insert an alignment hint of 256 bits.
DECLARE_ALIGNED(32, static const uint16_t, kSlidingWindowMask[]) = {
  0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000, 0x0000, 0x0000,
  0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000, 0x0000,
  0x0000, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000,
  0x0000, 0x0000, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF
};

static inline void get_squared_error(
    const uint8_t *frame1, const uint32_t stride1, const uint8_t *frame2,
    const uint32_t stride2, const uint32_t block_width,
    const uint32_t block_height, uint16_t *frame_sse,
    const unsigned int dst_stride) {
  uint16_t *dst = frame_sse;

  uint32_t i = 0;
  do {
    uint32_t j = 0;
    do {
      uint8x16_t s = vld1q_u8(frame1 + i * stride1 + j);
      uint8x16_t r = vld1q_u8(frame2 + i * stride2 + j);

      uint8x16_t abs_diff = vabdq_u8(s, r);
      uint16x8_t sse_lo =
          vmull_u8(vget_low_u8(abs_diff), vget_low_u8(abs_diff));
      uint16x8_t sse_hi =
          vmull_u8(vget_high_u8(abs_diff), vget_high_u8(abs_diff));

      vst1q_u16(dst + j + 2, sse_lo);
      vst1q_u16(dst + j + 10, sse_hi);

      j += 16;
    } while (j < block_width);

    dst += dst_stride;
  } while (++i < block_height);
}

static inline uint16x8_t load_and_pad(const uint16_t *src, const uint32_t col,
                                      const uint32_t block_width) {
  uint16x8_t s = vld1q_u16(src);

  if (col == 0) {
    const uint16_t lane2 = vgetq_lane_u16(s, 2);
    s = vsetq_lane_u16(lane2, s, 0);
    s = vsetq_lane_u16(lane2, s, 1);
  } else if (col >= block_width - 4) {
    const uint16_t lane5 = vgetq_lane_u16(s, 5);
    s = vsetq_lane_u16(lane5, s, 6);
    s = vsetq_lane_u16(lane5, s, 7);
  }
  return s;
}

static void apply_temporal_filter(
    const uint8_t *frame, const unsigned int stride, const uint32_t block_width,
    const uint32_t block_height, const int *subblock_mses,
    unsigned int *accumulator, uint16_t *count, const uint16_t *frame_sse,
    const uint32_t *luma_sse_sum, const double inv_num_ref_pixels,
    const double decay_factor, const double inv_factor,
    const double weight_factor, const double *d_factor, int tf_wgt_calc_lvl) {
  assert(((block_width == 16) || (block_width == 32)) &&
         ((block_height == 16) || (block_height == 32)));

  uint32_t diff_sse[BH][BW];
  const uint16x8x4_t vmask = vld1q_u16_x4(kSlidingWindowMask);

  // Traverse 4 columns at a time - first and last two columns need padding.
  for (uint32_t col = 0; col < block_width; col += 4) {
    uint16x8_t vsrc[5];
    const uint16_t *src = frame_sse + col;

    // Load and pad (for first and last two columns) 3 rows from the top.
    for (int i = 2; i < 5; i++) {
      vsrc[i] = load_and_pad(src, col, block_width);
      src += SSE_STRIDE;
    }

    // Pad the top 2 rows.
    vsrc[0] = vsrc[2];
    vsrc[1] = vsrc[2];

    uint32x4_t vsum[4] = { vdupq_n_u32(0), vdupq_n_u32(0), vdupq_n_u32(0),
                           vdupq_n_u32(0) };

    for (int i = 0; i < 4; i++) {
      vsum[i] = vpadalq_u16(vsum[i], vandq_u16(vsrc[0], vmask.val[i]));
      vsum[i] = vpadalq_u16(vsum[i], vandq_u16(vsrc[1], vmask.val[i]));
      vsum[i] = vpadalq_u16(vsum[i], vandq_u16(vsrc[2], vmask.val[i]));
      vsum[i] = vpadalq_u16(vsum[i], vandq_u16(vsrc[3], vmask.val[i]));
      vsum[i] = vpadalq_u16(vsum[i], vandq_u16(vsrc[4], vmask.val[i]));
    }

    for (unsigned int row = 0; row < block_height; row++) {
      uint32x4_t sum_luma = vld1q_u32(luma_sse_sum + row * BW + col);
      uint32x4_t sum_src = horizontal_add_4d_u32x4(vsum);

      vst1q_u32(&diff_sse[row][col], vaddq_u32(sum_src, sum_luma));

      for (int i = 0; i < 4; i++) {
        uint32x4_t vsum_0 = vpaddlq_u16(vandq_u16(vsrc[0], vmask.val[i]));
        vsum[i] = vsubq_u32(vsum[i], vsum_0);
      }

      // Push all rows in the sliding window up one.
      for (int i = 0; i < 4; i++) {
        vsrc[i] = vsrc[i + 1];
      }

      if (row <= block_height - 4) {
        // Load next row into the bottom of the sliding window.
        vsrc[4] = load_and_pad(src, col, block_width);
        src += SSE_STRIDE;
      } else {
        // Pad the bottom 2 rows.
        vsrc[4] = vsrc[3];
      }

      for (int i = 0; i < 4; i++) {
        vsum[i] = vpadalq_u16(vsum[i], vandq_u16(vsrc[4], vmask.val[i]));
      }
    }
  }

  // Perform filtering.
  if (tf_wgt_calc_lvl == 0) {
    for (unsigned int i = 0, k = 0; i < block_height; i++) {
      for (unsigned int j = 0; j < block_width; j++, k++) {
        const int pixel_value = frame[i * stride + j];

        const double window_error = diff_sse[i][j] * inv_num_ref_pixels;
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

        const double window_error = diff_sse[i][j] * inv_num_ref_pixels;
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

void av1_apply_temporal_filter_neon(
    const YV12_BUFFER_CONFIG *frame_to_filter, const MACROBLOCKD *mbd,
    const BLOCK_SIZE block_size, const int mb_row, const int mb_col,
    const int num_planes, const double *noise_levels, const MV *subblock_mvs,
    const int *subblock_mses, const int q_factor, const int filter_strength,
    int tf_wgt_calc_lvl, const uint8_t *pred, uint32_t *accum,
    uint16_t *count) {
  const int is_high_bitdepth = frame_to_filter->flags & YV12_FLAG_HIGHBITDEPTH;
  assert(block_size == BLOCK_32X32 && "Only support 32x32 block with Neon!");
  assert(TF_WINDOW_LENGTH == 5 && "Only support window length 5 with Neon!");
  assert(!is_high_bitdepth && "Only support low bit-depth with Neon!");
  assert(num_planes >= 1 && num_planes <= MAX_MB_PLANE);
  (void)is_high_bitdepth;

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
    const uint32_t frame_stride =
        frame_to_filter->strides[plane == AOM_PLANE_Y ? 0 : 1];
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
      for (unsigned int i = 0; i < plane_h; i++) {
        for (unsigned int j = 0; j < plane_w; j++) {
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

    get_squared_error(ref, frame_stride, pred + plane_offset, plane_w, plane_w,
                      plane_h, frame_sse, SSE_STRIDE);

    apply_temporal_filter(pred + plane_offset, plane_w, plane_w, plane_h,
                          subblock_mses, accum + plane_offset,
                          count + plane_offset, frame_sse, luma_sse_sum,
                          inv_num_ref_pixels, decay_factor, inv_factor,
                          weight_factor, d_factor, tf_wgt_calc_lvl);

    plane_offset += plane_h * plane_w;
  }
}

double av1_estimate_noise_from_single_plane_neon(const uint8_t *src, int height,
                                                 int width, int stride,
                                                 int edge_thresh) {
  uint16x8_t thresh = vdupq_n_u16(edge_thresh);
  uint32x4_t acc = vdupq_n_u32(0);
  // Count is in theory positive as it counts the number of times we're under
  // the threshold, but it will be counted negatively in order to make best use
  // of the vclt instruction, which sets every bit of a lane to 1 when the
  // condition is true.
  int32x4_t count = vdupq_n_s32(0);
  int final_count = 0;
  int64_t final_acc = 0;
  const uint8_t *src_start = src + stride + 1;
  int h = 1;

  do {
    int w = 1;
    const uint8_t *src_ptr = src_start;

    while (w <= (width - 1) - 16) {
      uint8x16_t mat[3][3];
      mat[0][0] = vld1q_u8(src_ptr - stride - 1);
      mat[0][1] = vld1q_u8(src_ptr - stride);
      mat[0][2] = vld1q_u8(src_ptr - stride + 1);
      mat[1][0] = vld1q_u8(src_ptr - 1);
      mat[1][1] = vld1q_u8(src_ptr);
      mat[1][2] = vld1q_u8(src_ptr + 1);
      mat[2][0] = vld1q_u8(src_ptr + stride - 1);
      mat[2][1] = vld1q_u8(src_ptr + stride);
      mat[2][2] = vld1q_u8(src_ptr + stride + 1);

      // Compute Sobel gradients.
      uint16x8_t gxa_lo =
          vaddl_u8(vget_low_u8(mat[0][0]), vget_low_u8(mat[2][0]));
      uint16x8_t gxa_hi =
          vaddl_u8(vget_high_u8(mat[0][0]), vget_high_u8(mat[2][0]));
      uint16x8_t gxb_lo =
          vaddl_u8(vget_low_u8(mat[0][2]), vget_low_u8(mat[2][2]));
      uint16x8_t gxb_hi =
          vaddl_u8(vget_high_u8(mat[0][2]), vget_high_u8(mat[2][2]));
      gxa_lo = vaddq_u16(
          gxa_lo, vaddl_u8(vget_low_u8(mat[1][0]), vget_low_u8(mat[1][0])));
      gxa_hi = vaddq_u16(
          gxa_hi, vaddl_u8(vget_high_u8(mat[1][0]), vget_high_u8(mat[1][0])));
      gxb_lo = vaddq_u16(
          gxb_lo, vaddl_u8(vget_low_u8(mat[1][2]), vget_low_u8(mat[1][2])));
      gxb_hi = vaddq_u16(
          gxb_hi, vaddl_u8(vget_high_u8(mat[1][2]), vget_high_u8(mat[1][2])));

      uint16x8_t gya_lo =
          vaddl_u8(vget_low_u8(mat[0][0]), vget_low_u8(mat[0][2]));
      uint16x8_t gya_hi =
          vaddl_u8(vget_high_u8(mat[0][0]), vget_high_u8(mat[0][2]));
      uint16x8_t gyb_lo =
          vaddl_u8(vget_low_u8(mat[2][0]), vget_low_u8(mat[2][2]));
      uint16x8_t gyb_hi =
          vaddl_u8(vget_high_u8(mat[2][0]), vget_high_u8(mat[2][2]));
      gya_lo = vaddq_u16(
          gya_lo, vaddl_u8(vget_low_u8(mat[0][1]), vget_low_u8(mat[0][1])));
      gya_hi = vaddq_u16(
          gya_hi, vaddl_u8(vget_high_u8(mat[0][1]), vget_high_u8(mat[0][1])));
      gyb_lo = vaddq_u16(
          gyb_lo, vaddl_u8(vget_low_u8(mat[2][1]), vget_low_u8(mat[2][1])));
      gyb_hi = vaddq_u16(
          gyb_hi, vaddl_u8(vget_high_u8(mat[2][1]), vget_high_u8(mat[2][1])));

      uint16x8_t ga_lo = vabaq_u16(vabdq_u16(gxa_lo, gxb_lo), gya_lo, gyb_lo);
      uint16x8_t ga_hi = vabaq_u16(vabdq_u16(gxa_hi, gxb_hi), gya_hi, gyb_hi);

      // Check which vector elements are under the threshold. The Laplacian is
      // then unconditionally computed and we accumulate zeros if we're not
      // under the threshold. This is much faster than using an if statement.
      uint16x8_t thresh_u16_lo = vcltq_u16(ga_lo, thresh);
      uint16x8_t thresh_u16_hi = vcltq_u16(ga_hi, thresh);

      uint16x8_t center_lo = vshll_n_u8(vget_low_u8(mat[1][1]), 2);
      uint16x8_t center_hi = vshll_n_u8(vget_high_u8(mat[1][1]), 2);

      uint16x8_t adj0_lo =
          vaddl_u8(vget_low_u8(mat[0][1]), vget_low_u8(mat[2][1]));
      uint16x8_t adj0_hi =
          vaddl_u8(vget_high_u8(mat[0][1]), vget_high_u8(mat[2][1]));
      uint16x8_t adj1_lo =
          vaddl_u8(vget_low_u8(mat[1][0]), vget_low_u8(mat[1][2]));
      uint16x8_t adj1_hi =
          vaddl_u8(vget_high_u8(mat[1][0]), vget_high_u8(mat[1][2]));
      uint16x8_t adj_lo = vaddq_u16(adj0_lo, adj1_lo);
      adj_lo = vaddq_u16(adj_lo, adj_lo);
      uint16x8_t adj_hi = vaddq_u16(adj0_hi, adj1_hi);
      adj_hi = vaddq_u16(adj_hi, adj_hi);

      uint16x8_t diag0_lo =
          vaddl_u8(vget_low_u8(mat[0][0]), vget_low_u8(mat[0][2]));
      uint16x8_t diag0_hi =
          vaddl_u8(vget_high_u8(mat[0][0]), vget_high_u8(mat[0][2]));
      uint16x8_t diag1_lo =
          vaddl_u8(vget_low_u8(mat[2][0]), vget_low_u8(mat[2][2]));
      uint16x8_t diag1_hi =
          vaddl_u8(vget_high_u8(mat[2][0]), vget_high_u8(mat[2][2]));
      uint16x8_t diag_lo = vaddq_u16(diag0_lo, diag1_lo);
      uint16x8_t diag_hi = vaddq_u16(diag0_hi, diag1_hi);

      uint16x8_t v_lo = vaddq_u16(center_lo, diag_lo);
      v_lo = vabdq_u16(v_lo, adj_lo);
      uint16x8_t v_hi = vaddq_u16(center_hi, diag_hi);
      v_hi = vabdq_u16(v_hi, adj_hi);

      acc = vpadalq_u16(acc, vandq_u16(v_lo, thresh_u16_lo));
      acc = vpadalq_u16(acc, vandq_u16(v_hi, thresh_u16_hi));

      // Add -1 for each lane where the gradient is under the threshold.
      count = vpadalq_s16(count, vreinterpretq_s16_u16(thresh_u16_lo));
      count = vpadalq_s16(count, vreinterpretq_s16_u16(thresh_u16_hi));

      w += 16;
      src_ptr += 16;
    }

    if (w <= (width - 1) - 8) {
      uint8x8_t mat[3][3];
      mat[0][0] = vld1_u8(src_ptr - stride - 1);
      mat[0][1] = vld1_u8(src_ptr - stride);
      mat[0][2] = vld1_u8(src_ptr - stride + 1);
      mat[1][0] = vld1_u8(src_ptr - 1);
      mat[1][1] = vld1_u8(src_ptr);
      mat[1][2] = vld1_u8(src_ptr + 1);
      mat[2][0] = vld1_u8(src_ptr + stride - 1);
      mat[2][1] = vld1_u8(src_ptr + stride);
      mat[2][2] = vld1_u8(src_ptr + stride + 1);

      // Compute Sobel gradients.
      uint16x8_t gxa = vaddl_u8(mat[0][0], mat[2][0]);
      uint16x8_t gxb = vaddl_u8(mat[0][2], mat[2][2]);
      gxa = vaddq_u16(gxa, vaddl_u8(mat[1][0], mat[1][0]));
      gxb = vaddq_u16(gxb, vaddl_u8(mat[1][2], mat[1][2]));

      uint16x8_t gya = vaddl_u8(mat[0][0], mat[0][2]);
      uint16x8_t gyb = vaddl_u8(mat[2][0], mat[2][2]);
      gya = vaddq_u16(gya, vaddl_u8(mat[0][1], mat[0][1]));
      gyb = vaddq_u16(gyb, vaddl_u8(mat[2][1], mat[2][1]));

      uint16x8_t ga = vabaq_u16(vabdq_u16(gxa, gxb), gya, gyb);

      // Check which vector elements are under the threshold. The Laplacian is
      // then unconditionally computed and we accumulate zeros if we're not
      // under the threshold. This is much faster than using an if statement.
      uint16x8_t thresh_u16 = vcltq_u16(ga, thresh);

      uint16x8_t center = vshll_n_u8(mat[1][1], 2);

      uint16x8_t adj0 = vaddl_u8(mat[0][1], mat[2][1]);
      uint16x8_t adj1 = vaddl_u8(mat[1][0], mat[1][2]);
      uint16x8_t adj = vaddq_u16(adj0, adj1);
      adj = vaddq_u16(adj, adj);

      uint16x8_t diag0 = vaddl_u8(mat[0][0], mat[0][2]);
      uint16x8_t diag1 = vaddl_u8(mat[2][0], mat[2][2]);
      uint16x8_t diag = vaddq_u16(diag0, diag1);

      uint16x8_t v = vaddq_u16(center, diag);
      v = vabdq_u16(v, adj);

      acc = vpadalq_u16(acc, vandq_u16(v, thresh_u16));
      // Add -1 for each lane where the gradient is under the threshold.
      count = vpadalq_s16(count, vreinterpretq_s16_u16(thresh_u16));

      w += 8;
      src_ptr += 8;
    }

    if (w <= (width - 1) - 4) {
      uint16x8_t mask = vcombine_u16(vdup_n_u16(65535), vdup_n_u16(0));
      uint8x8_t mat[3][3];
      mat[0][0] = load_unaligned_u8_4x1(src_ptr - stride - 1);
      mat[0][1] = load_unaligned_u8_4x1(src_ptr - stride);
      mat[0][2] = load_unaligned_u8_4x1(src_ptr - stride + 1);
      mat[1][0] = load_unaligned_u8_4x1(src_ptr - 1);
      mat[1][1] = load_unaligned_u8_4x1(src_ptr);
      mat[1][2] = load_unaligned_u8_4x1(src_ptr + 1);
      mat[2][0] = load_unaligned_u8_4x1(src_ptr + stride - 1);
      mat[2][1] = load_unaligned_u8_4x1(src_ptr + stride);
      mat[2][2] = load_unaligned_u8_4x1(src_ptr + stride + 1);

      // Compute Sobel gradients.
      uint16x8_t gxa = vaddl_u8(mat[0][0], mat[2][0]);
      uint16x8_t gxb = vaddl_u8(mat[0][2], mat[2][2]);
      gxa = vaddq_u16(gxa, vaddl_u8(mat[1][0], mat[1][0]));
      gxb = vaddq_u16(gxb, vaddl_u8(mat[1][2], mat[1][2]));

      uint16x8_t gya = vaddl_u8(mat[0][0], mat[0][2]);
      uint16x8_t gyb = vaddl_u8(mat[2][0], mat[2][2]);
      gya = vaddq_u16(gya, vaddl_u8(mat[0][1], mat[0][1]));
      gyb = vaddq_u16(gyb, vaddl_u8(mat[2][1], mat[2][1]));

      uint16x8_t ga = vabaq_u16(vabdq_u16(gxa, gxb), gya, gyb);

      // Check which vector elements are under the threshold. The Laplacian is
      // then unconditionally computed and we accumulate zeros if we're not
      // under the threshold. This is much faster than using an if statement.
      uint16x8_t thresh_u16 = vandq_u16(vcltq_u16(ga, thresh), mask);

      uint16x8_t center = vshll_n_u8(mat[1][1], 2);

      uint16x8_t adj0 = vaddl_u8(mat[0][1], mat[2][1]);
      uint16x8_t adj1 = vaddl_u8(mat[1][0], mat[1][2]);
      uint16x8_t adj = vaddq_u16(adj0, adj1);
      adj = vaddq_u16(adj, adj);

      uint16x8_t diag0 = vaddl_u8(mat[0][0], mat[0][2]);
      uint16x8_t diag1 = vaddl_u8(mat[2][0], mat[2][2]);
      uint16x8_t diag = vaddq_u16(diag0, diag1);

      uint16x8_t v = vaddq_u16(center, diag);
      v = vabdq_u16(v, adj);

      acc = vpadalq_u16(acc, vandq_u16(v, thresh_u16));
      // Add -1 for each lane where the gradient is under the threshold.
      count = vpadalq_s16(count, vreinterpretq_s16_u16(thresh_u16));

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
      const int ga = abs(gx) + abs(gy);

      // Accumulate Laplacian.
      const int is_under = ga < edge_thresh;
      const int v = 4 * mat[1][1] -
                    2 * (mat[0][1] + mat[2][1] + mat[1][0] + mat[1][2]) +
                    (mat[0][0] + mat[0][2] + mat[2][0] + mat[2][2]);
      final_acc += abs(v) * is_under;
      final_count += is_under;

      src_ptr++;
      w++;
    }
    src_start += stride;
  } while (++h < height - 1);

  // We counted negatively, so subtract to get the final value.
  final_count -= horizontal_add_s32x4(count);
  final_acc += horizontal_long_add_u32x4(acc);
  return (final_count < 16)
             ? -1.0
             : (double)final_acc / (6 * final_count) * SQRT_PI_BY_2;
}
