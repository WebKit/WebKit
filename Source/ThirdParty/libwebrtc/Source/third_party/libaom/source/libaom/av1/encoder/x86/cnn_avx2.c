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

#include <assert.h>
#include <immintrin.h>
#include <math.h>

#include "aom_dsp/aom_dsp_common.h"
#include "av1/common/av1_common_int.h"
#include "av1/encoder/cnn.h"

// This mask rearranges source pixels in the order shown below.
// shuffle_src_layer0[0][8]: applied on source pixels 0 to 7.
// shuffle_src_layer0[1][8]: applied on source pixels 7 to 14.
// This shuffling is needed to process 3 5x5 blocks which need
// source pixels in the following order.
// 1st 5x5 block: source pixels needed are 0 to 4,
// 2nd 5x5 block: source pixels needed are 4 to 8,
// 3rd 5x5 block: source pixels needed are 8 to 12.
// Source pixels are loaded like mentioned below.
// load_src0 : 0, 1, 2, 3, 4, 5, 6, 7
// load_src1 : 7, 8, 9, 10, 11, 12, 13, 14
// After applying masks, source bytes will be in the order:
// load_src0 : 0, 1, 2, 3, 4, 4, 5, 6
//             consists 5 pixels needed for 1st 5x5 block and
//             first 3 pixels needed for 2nd 5x5 block.
// load_src1 : 7, 8, 8, 9, 10, 11, 12, x
//             consists last 2 pixels needed for 2nd 5x5 block and
//             5 pixels needed for 3rd 5x5 block.
DECLARE_ALIGNED(32, static const uint32_t,
                shuffle_src_layer0[2][8]) = { { 0, 1, 2, 3, 4, 4, 5, 6 },
                                              { 0, 1, 1, 2, 3, 4, 5, 0 } };

// This mask rearrange the weights to match shuffled source pixels order.
DECLARE_ALIGNED(32, static const uint32_t,
                shuffle_weight_layer0[2][8]) = { { 0, 1, 2, 3, 4, 0, 1, 2 },
                                                 { 3, 4, 0, 1, 2, 3, 4, 0 } };

// Shuffle mask used to rearrange weights corresponding to layer 1 and layer 2.
// For layer 1 and layer 2, convolution happens at 2x2 as filter_width and
// filter_height are equal to 2. So rearranging the weights in the
// order shown below to match source pixels. Basically this mask replicates
// the weights across the width of 2.
DECLARE_ALIGNED(32, static const uint32_t,
                shuffle_weight_layer_1_and_2[2][8]) = {
  { 0, 1, 0, 1, 0, 1, 0, 1 }, { 2, 3, 2, 3, 2, 3, 2, 3 }
};

// After the stages of multiplication and accumulation, the output values
// in the register will be jumbled. In order to store register into
// output buffer in a proper way, the following mask is applied on output
// register.
DECLARE_ALIGNED(32, static const uint32_t,
                shuffle_output_layer_1_and_2[8]) = { 0, 1, 4, 5, 2, 3, 6, 7 };

// Load weights needed for layer 0 (for 5x5 block processing),
// and fill the registers appropriately to match source pixel mapping.
static INLINE void prepare_weights_for_5x5_convolve(
    const float *layer_config_weights, int off, float weight[5][8],
    const int cstep, __m256 *shuffle_weight, const __m256i weight_mask_0,
    const __m256i weight_mask_1) {
  for (int row = 0; row < 5; ++row) {
    for (int col = 0; col < 5; ++col) {
      weight[row][col] = layer_config_weights[off];
      off += cstep;
    }
  }
  shuffle_weight[0] = _mm256_loadu_ps(weight[0]);
  shuffle_weight[1] = _mm256_loadu_ps(weight[1]);
  shuffle_weight[2] = _mm256_loadu_ps(weight[2]);
  shuffle_weight[3] = _mm256_loadu_ps(weight[3]);
  shuffle_weight[4] = _mm256_loadu_ps(weight[4]);

  shuffle_weight[0] =
      _mm256_permutevar8x32_ps(shuffle_weight[0], weight_mask_0);
  shuffle_weight[1] =
      _mm256_permutevar8x32_ps(shuffle_weight[1], weight_mask_0);
  shuffle_weight[2] =
      _mm256_permutevar8x32_ps(shuffle_weight[2], weight_mask_0);
  shuffle_weight[3] =
      _mm256_permutevar8x32_ps(shuffle_weight[3], weight_mask_0);
  shuffle_weight[4] =
      _mm256_permutevar8x32_ps(shuffle_weight[4], weight_mask_0);
  shuffle_weight[5] =
      _mm256_permutevar8x32_ps(shuffle_weight[0], weight_mask_1);
  shuffle_weight[6] =
      _mm256_permutevar8x32_ps(shuffle_weight[1], weight_mask_1);
  shuffle_weight[7] =
      _mm256_permutevar8x32_ps(shuffle_weight[2], weight_mask_1);
  shuffle_weight[8] =
      _mm256_permutevar8x32_ps(shuffle_weight[3], weight_mask_1);
  shuffle_weight[9] =
      _mm256_permutevar8x32_ps(shuffle_weight[4], weight_mask_1);
}

// For each row, loads source pixels 0 to 7(load_src_0), 7 to 14(load_src_1) and
// arranges them appropriately to process 3 blocks.
#define PERFORM_CONVOLVE_FOR_3_5X5_BLOCKS()                            \
  do {                                                                 \
    for (int row = 0; row < 5; row++) {                                \
      load_src_0 = _mm256_loadu_ps(input_ptr);                         \
      load_src_1 = _mm256_loadu_ps(input_ptr + 7);                     \
      load_src_0 = _mm256_permutevar8x32_ps(load_src_0, block0_1);     \
      load_src_1 = _mm256_permutevar8x32_ps(load_src_1, block1_2);     \
      load_src_0 = _mm256_mul_ps(load_src_0, shuffle_weight[0 + row]); \
      load_src_1 = _mm256_mul_ps(load_src_1, shuffle_weight[5 + row]); \
      accum_src_0 = _mm256_add_ps(load_src_0, accum_src_0);            \
      accum_src_1 = _mm256_add_ps(load_src_1, accum_src_1);            \
      input_ptr += in_stride;                                          \
    }                                                                  \
  } while (0)

// Load masks needed for shuffling of output and weights.
static INLINE void load_shuffle_masks_for_2x2_convolve(__m256i *output_mask,
                                                       __m256i *weight_mask) {
  // Load shuffle buffer needed to sort the output.
  *output_mask =
      _mm256_load_si256((const __m256i *)shuffle_output_layer_1_and_2);

  // Load shuffle buffers needed for weight.
  weight_mask[0] =
      _mm256_load_si256((const __m256i *)shuffle_weight_layer_1_and_2[0]);
  weight_mask[1] =
      _mm256_load_si256((const __m256i *)shuffle_weight_layer_1_and_2[1]);
}

// Load weights needed for layer 1 and 2 (for 2x2 block processing),
// and fill the registers appropriately to match source pixel mapping.
static INLINE void prepare_weights_for_2x2_convolve(
    const float *layer_config_weights, int off, const int cstep,
    __m256 *shuffle_weight, __m256i *weight_mask) {
  // Weights needed for 2x2 block.
  float weight[4] = { 0 };
  for (int i = 0; i < 4; ++i) {
    weight[i] = layer_config_weights[off];
    off += cstep;
  }

  const __m256 weight_vec = _mm256_castps128_ps256(_mm_loadu_ps(weight));
  shuffle_weight[0] = _mm256_permutevar8x32_ps(weight_vec, weight_mask[0]);
  shuffle_weight[1] = _mm256_permutevar8x32_ps(weight_vec, weight_mask[1]);
}

// Do convolution of one 5x5 block.
#define PERFORM_CONVOLVE_FOR_1_5X5_BLOCK(w, accum0, in_stride)           \
  do {                                                                   \
    __m128 load_src[5];                                                  \
    load_src[0] = _mm_loadu_ps(input_ptr);                               \
    last_column_sum += input_ptr[4] * weight[0][4];                      \
    input_ptr += in_stride;                                              \
    load_src[1] = _mm_loadu_ps(input_ptr);                               \
    last_column_sum += input_ptr[4] * weight[1][4];                      \
    input_ptr += in_stride;                                              \
    load_src[2] = _mm_loadu_ps(input_ptr);                               \
    last_column_sum += input_ptr[4] * weight[2][4];                      \
    input_ptr += in_stride;                                              \
    load_src[3] = _mm_loadu_ps(input_ptr);                               \
    last_column_sum += input_ptr[4] * weight[3][4];                      \
    input_ptr += in_stride;                                              \
    load_src[4] = _mm_loadu_ps(input_ptr);                               \
    last_column_sum += input_ptr[4] * weight[4][4];                      \
                                                                         \
    load_src[0] = _mm_mul_ps(load_src[0], _mm256_castps256_ps128(w[0])); \
    load_src[1] = _mm_mul_ps(load_src[1], _mm256_castps256_ps128(w[1])); \
    load_src[2] = _mm_mul_ps(load_src[2], _mm256_castps256_ps128(w[2])); \
    load_src[3] = _mm_mul_ps(load_src[3], _mm256_castps256_ps128(w[3])); \
    load_src[4] = _mm_mul_ps(load_src[4], _mm256_castps256_ps128(w[4])); \
                                                                         \
    accum0 = _mm_add_ps(load_src[0], accum0);                            \
    load_src[1] = _mm_add_ps(load_src[1], load_src[2]);                  \
    load_src[3] = _mm_add_ps(load_src[3], load_src[4]);                  \
    load_src[1] = _mm_add_ps(load_src[1], load_src[3]);                  \
    accum0 = _mm_add_ps(accum0, load_src[1]);                            \
  } while (0)

// Do convolution on 8 horizontal 2x2 blocks.
static INLINE void perform_convolve_for_8h_2x2_blocks(
    const float *input_ptr, int in_stride, __m256 *weight, __m256 *out_accum,
    __m256i shuffle_output_mask) {
  __m256 load_src[4];
  // Load input into source registers.
  load_src[0] = _mm256_loadu_ps(input_ptr);
  load_src[1] = _mm256_loadu_ps(input_ptr + 8);
  load_src[2] = _mm256_loadu_ps(input_ptr + in_stride);
  load_src[3] = _mm256_loadu_ps(input_ptr + in_stride + 8);

  // Multiply the loaded input with corresponding weights.
  load_src[0] = _mm256_mul_ps(load_src[0], weight[0]);
  load_src[1] = _mm256_mul_ps(load_src[1], weight[0]);
  load_src[2] = _mm256_mul_ps(load_src[2], weight[1]);
  load_src[3] = _mm256_mul_ps(load_src[3], weight[1]);

  // Accumulate across 2x2 blocks.
  load_src[0] = _mm256_add_ps(load_src[0], load_src[2]);
  load_src[1] = _mm256_add_ps(load_src[1], load_src[3]);
  load_src[0] = _mm256_hadd_ps(load_src[0], load_src[1]);

  // Sort the output in order to store into output buffer.
  load_src[0] = _mm256_permutevar8x32_ps(load_src[0], shuffle_output_mask);
  *out_accum = _mm256_add_ps(*out_accum, load_src[0]);
}

// Do convolution on 8 (4 horizontal x 2 vertical) 2x2 blocks.
static INLINE void perform_convolve_for_4hx2v_2x2_blocks(
    const float *input_ptr, int in_stride, __m256 *weight, __m256 *out_accum,
    __m256i shuffle_output_mask) {
  __m256 load_src[4];
  // Load input into source registers.
  load_src[0] = _mm256_loadu_ps(input_ptr);
  load_src[1] = _mm256_loadu_ps(input_ptr + in_stride);
  load_src[2] = _mm256_loadu_ps(input_ptr + (in_stride * 2));
  load_src[3] = _mm256_loadu_ps(input_ptr + (in_stride * 3));

  // Multiply the loaded input with corresponding weights.
  load_src[0] = _mm256_mul_ps(load_src[0], weight[0]);
  load_src[1] = _mm256_mul_ps(load_src[1], weight[1]);
  load_src[2] = _mm256_mul_ps(load_src[2], weight[0]);
  load_src[3] = _mm256_mul_ps(load_src[3], weight[1]);

  // Accumulate across 2x2 blocks.
  load_src[0] = _mm256_add_ps(load_src[0], load_src[1]);
  load_src[2] = _mm256_add_ps(load_src[2], load_src[3]);
  load_src[0] = _mm256_hadd_ps(load_src[0], load_src[2]);

  // Sort the output in order to store into output buffer.
  load_src[0] = _mm256_permutevar8x32_ps(load_src[0], shuffle_output_mask);
  *out_accum = _mm256_add_ps(*out_accum, load_src[0]);
}

// AVX2 variant of av1_cnn_convolve_no_maxpool_padding_valid_c(), when
// filter_width and filter_height are equal to 5.
// CNN convolve parsing is based on av1_intra_mode_cnn_partition_cnn_config.
// Based on the configuration set for each layer, the current encoder
// always chooses the case of no_maxpool_padding_valid.
// And also for layer 0 convolution happens at 5x5 level as the
// filter_width and filter_height are set as 5.
static void cnn_convolve_no_maxpool_padding_valid_5x5_avx2(
    const float **input, int in_width, int in_height, int in_stride,
    const CNN_LAYER_CONFIG *const layer_config, float **output, int out_stride,
    int start_idx, const int cstep, const int channel_step) {
  const int kFilterWidth = 5;
  const int kFilterHeight = 5;
  const int kSkipWidth = 4;
  const int kSkipHeight = 4;
  assert(layer_config->filter_width == kFilterWidth &&
         layer_config->filter_height == kFilterHeight);
  assert(layer_config->skip_width == kSkipWidth &&
         layer_config->skip_height == kSkipHeight);

  // Load shuffle buffers needed for source.
  const __m256i block0_1 =
      _mm256_load_si256((const __m256i *)shuffle_src_layer0[0]);
  const __m256i block1_2 =
      _mm256_load_si256((const __m256i *)shuffle_src_layer0[1]);

  // Load shuffle buffers needed for weight.
  const __m256i weight_mask_0 =
      _mm256_load_si256((const __m256i *)shuffle_weight_layer0[0]);
  const __m256i weight_mask_1 =
      _mm256_load_si256((const __m256i *)shuffle_weight_layer0[1]);

  // Width needs to be moved to go to next iteration of processing 3 5x5 blocks.
  const int kSkipWidthForNextIter = kSkipWidth * 3;

  // Minimum width required to process 3 5x5 blocks at a time.
  // min width (for processing 3 5x5 block) = 2*skip_width + filter_width
  // Here, skip_width specifies how much width we should move while processing
  // next block convolution and filter_width specifies for how many pixels
  // filter needs to be applied.
  const int kMinWidthFor3_5x5Blocks = (kSkipWidth * 2) + kFilterWidth;
  for (int i = start_idx; i < layer_config->out_channels; i += channel_step) {
    const float out_ch_bias = layer_config->bias[i];
    for (int k = 0; k < layer_config->in_channels; ++k) {
      __m256 shuffle_weight[10];

      // Weights needed are 5x5, for SIMD purpose made this array as 5x8.
      float weight[5][8] = { { 0 } };
      int off = k * layer_config->out_channels + i;

      // In layer 0, the convolution process happens at 5x5.
      // The weights needed for 5x5 block are same across the in-channels,
      // which is why the load of weights happens once for each in-channel.
      prepare_weights_for_5x5_convolve(layer_config->weights, off, weight,
                                       cstep, shuffle_weight, weight_mask_0,
                                       weight_mask_1);

      for (int h = 0, u = 0; h < in_height - kFilterHeight + 1;
           h += kSkipHeight, ++u) {
        const int out_h = u * out_stride;
        int v = 0;
        int w = 0;
        int rem_width = in_width;
        // Processing 3 5x5 blocks at a time, if sufficient width is present.
        while (rem_width >= kMinWidthFor3_5x5Blocks) {
          __m256 load_src_0, load_src_1;
          __m256 accum_src_0 = _mm256_setzero_ps();
          __m256 accum_src_1 = _mm256_setzero_ps();
          const float *input_ptr = &input[k][h * in_stride + w];
          PERFORM_CONVOLVE_FOR_3_5X5_BLOCKS();

          // Accumulate across column.
          __m256 accum = _mm256_hadd_ps(accum_src_0, accum_src_1);
          __m128 tmp_reg_0 = _mm256_extractf128_ps(accum_src_0, 1);
          __m128 tmp_reg_1 = _mm256_extractf128_ps(accum_src_1, 1);

          __m128 accum_l = _mm256_castps256_ps128(accum);
          __m128 accum_h = _mm256_extractf128_ps(accum, 1);

          __m128 tmp_reg_2 = _mm_add_ps(accum_l, tmp_reg_0);
          __m128 tmp_reg_3 = _mm_add_ps(tmp_reg_0, accum_h);
          __m128 tmp_reg_4 = _mm_add_ps(tmp_reg_1, accum_h);

          // 1st 5x5 block output.
          output[i][out_h + v] =
              out_ch_bias + _mm_cvtss_f32(tmp_reg_2) +
              _mm_cvtss_f32(_mm_shuffle_ps(accum_l, accum_l, 1));

          // 2nd 5x5 block output.
          output[i][out_h + v + 1] =
              out_ch_bias +
              _mm_cvtss_f32(_mm_shuffle_ps(tmp_reg_3, tmp_reg_3, 1)) +
              _mm_cvtss_f32(_mm_shuffle_ps(accum_l, accum_l, 2));

          // 3rd 5x5 block output.
          output[i][out_h + v + 2] =
              out_ch_bias +
              _mm_cvtss_f32(_mm_shuffle_ps(tmp_reg_4, tmp_reg_4, 2)) +
              _mm_cvtss_f32(_mm_shuffle_ps(accum_l, accum_l, 3));

          v += 3;
          w += kSkipWidthForNextIter;
          rem_width -= kSkipWidthForNextIter;
        }

        // Process remaining blocks as single 5x5 block at a time.
        while (rem_width >= kFilterWidth) {
          float last_column_sum = 0;
          __m128 accum = _mm_setzero_ps();
          const float *input_ptr = &input[k][h * in_stride + w];
          PERFORM_CONVOLVE_FOR_1_5X5_BLOCK(shuffle_weight, accum, in_stride);

          // Accumulate across column.
          accum = _mm_hadd_ps(accum, accum);
          output[i][out_h + v] = out_ch_bias + last_column_sum +
                                 _mm_cvtss_f32(accum) +
                                 _mm_cvtss_f32(_mm_shuffle_ps(accum, accum, 1));

          v += 1;
          w += kSkipWidth;
          rem_width -= kSkipWidth;
        }
      }
    }
  }
}

// AVX2 implementation for layer 1.
static INLINE void cnn_convolve_no_maxpool_padding_valid_layer1_avx2(
    const float **input, int in_stride,
    const CNN_LAYER_CONFIG *const layer_config, float **output, int out_stride,
    int start_idx, const int cstep, const int channel_step) {
  __m256i weight_mask[2];
  __m256i shuffle_output_mask;
  load_shuffle_masks_for_2x2_convolve(&shuffle_output_mask, weight_mask);

  const int kInHeight = 16;
  const int kFilterHeight = 2;
  const int kSkipHeight = 2;
  for (int i = start_idx; i < layer_config->out_channels; i += channel_step) {
    __m256 bias_reg = _mm256_set1_ps(layer_config->bias[i]);
    // out_accum registers are used to store the 2x2 convolve outputs
    // (calculated over input block size), which are accumulated across the
    // in_channels. As per the design, each iteration of for loop processes 8
    // (horizontal) 2x2 blocks and stores in corresponding out_accum register
    // (as input size is 16x16, a total of 64 2x2 blocks are present and 8
    // out_accum registers are enough to store the outputs).
    // Hence for loops corresponding to 'j' and 'h', below, run over the number
    // of out_accum registers.
    __m256 out_accum[8];
    for (int j = 0; j < 8; ++j) out_accum[j] = bias_reg;
    for (int k = 0; k < layer_config->in_channels; ++k) {
      __m256 shuffle_weight[2];
      int off = k * layer_config->out_channels + i;
      // In layer 1, the convolution process happens at 2x2.
      // The weights needed for 2x2 block are same across the in-channels,
      // which is why the load of weights happens once for each in-channel.
      prepare_weights_for_2x2_convolve(layer_config->weights, off, cstep,
                                       shuffle_weight, weight_mask);

      for (int h = 0, u = 0; h < kInHeight - kFilterHeight + 1;
           h += kSkipHeight, ++u) {
        const float *input_ptr = &input[k][h * in_stride];
        perform_convolve_for_8h_2x2_blocks(input_ptr, in_stride, shuffle_weight,
                                           &out_accum[u], shuffle_output_mask);
      }
    }
    // Store output of layer 1.
    for (int j = 0; j < 8; ++j) {
      _mm256_storeu_ps(&output[i][j * out_stride], out_accum[j]);
    }
  }
}

// AVX2 implementation for layer 2.
static INLINE void cnn_convolve_no_maxpool_padding_valid_layer2_avx2(
    const float **input, int in_stride,
    const CNN_LAYER_CONFIG *const layer_config, float **output, int out_stride,
    int start_idx, const int cstep, const int channel_step) {
  __m256i weight_mask[2];
  __m256i shuffle_output_mask;
  load_shuffle_masks_for_2x2_convolve(&shuffle_output_mask, weight_mask);

  const int kInHeight = 8;
  const int kFilterHeight = 2;
  const int kSkipHeight = 2;
  for (int i = start_idx; i < layer_config->out_channels; i += channel_step) {
    __m256 bias_reg = _mm256_set1_ps(layer_config->bias[i]);
    // out_accum registers are used to store the 2x2 convolve outputs
    // (calculated over input block size), which are accumulated across the
    // in_channels. As per the design, each iteration of for loop processes 8
    // (4 horizontal x 2 vertical) 2x2 blocks and stores in corresponding
    // out_accum register (as input size is 8x8, a total of 16 2x2 blocks are
    // present and 2 out_accum registers are enough to store the outputs).
    // Hence for loops corresponding to 'j' and 'h', below, run over the number
    // of out_accum registers.
    __m256 out_accum[2];

    // Height needs to be moved to go to next iteration of processing
    // while processing 2 2x2 blocks vertically.
    const int kSkipHeightForNextIter = kSkipHeight * 2;
    for (int j = 0; j < 2; ++j) out_accum[j] = bias_reg;
    for (int k = 0; k < layer_config->in_channels; ++k) {
      __m256 shuffle_weight[2];
      int off = k * layer_config->out_channels + i;
      // In layer 2, the convolution process happens at 2x2.
      // The weights needed for 2x2 block are same across the in-channels,
      // which is why the load of weights happens once for each in-channel.
      prepare_weights_for_2x2_convolve(layer_config->weights, off, cstep,
                                       shuffle_weight, weight_mask);

      for (int h = 0, u = 0; h < kInHeight - kFilterHeight + 1;
           h += kSkipHeightForNextIter, ++u) {
        const float *input_ptr = &input[k][h * in_stride];
        perform_convolve_for_4hx2v_2x2_blocks(input_ptr, in_stride,
                                              shuffle_weight, &out_accum[u],
                                              shuffle_output_mask);
      }
    }
    // Store output of layer 2.
    for (int j = 0; j < 2; ++j) {
      _mm256_storeu_ps(&output[i][j * out_stride * 2], out_accum[j]);
    }
  }
}

// AVX2 variant of av1_cnn_convolve_no_maxpool_padding_valid_c(), when
// filter_width and filter_height are equal to 2.
// As per the layer config set by av1_intra_mode_cnn_partition_cnn_config,
// the filter_width and filter_height are equal to 2 for layer >= 1. So
// convolution happens at 2x2 for layer >= 1.
static void cnn_convolve_no_maxpool_padding_valid_2x2_avx2(
    const float **input, int in_width, int in_height, int in_stride,
    const CNN_LAYER_CONFIG *const layer_config, float **output, int out_stride,
    int start_idx, const int cstep, const int channel_step) {
  assert(layer_config->filter_width == 2 && layer_config->filter_height == 2);
  assert(layer_config->skip_width == 2 && layer_config->skip_height == 2);

  if (in_width == 16 && in_height == 16) {
    // This case of in_width and in_height equal to 16 corresponds to layer 1.
    // The output size of this layer is 8x8.
    cnn_convolve_no_maxpool_padding_valid_layer1_avx2(
        input, in_stride, layer_config, output, out_stride, start_idx, cstep,
        channel_step);
  } else if (in_width == 8 && in_height == 8) {
    // This case of in_width and in_height equal to 8 corresponds to layer 2.
    // The output size of this layer is 4x4.
    cnn_convolve_no_maxpool_padding_valid_layer2_avx2(
        input, in_stride, layer_config, output, out_stride, start_idx, cstep,
        channel_step);
  } else {
    // For layer equal to 3 and 4, the input is of size 4x4 and 2x2
    // respectively. Implementing SIMD for these cases might not be optimal,
    // which is why we call C path for layer >= 3.
    av1_cnn_convolve_no_maxpool_padding_valid_c(
        input, in_width, in_height, in_stride, layer_config, output, out_stride,
        start_idx, cstep, channel_step);
  }
}

// AVX2 variant of av1_cnn_convolve_no_maxpool_padding_valid_c().
// As per the current encoder, av1_cnn_convolve function gets called for
// block size equal to 64x64. av1_cnn_convolve() uses layer config values
// set by av1_intra_mode_cnn_partition_cnn_config. The following are a few
// details related to each layer's config parameters.
// Layer_Number in_size out_size filter_wd filter_ht skip_wd skip_ht
//     0         64x64    16x16      5         5         4       4
//     1         16x16    8x8        2         2         2       2
//     2         8x8      4x4        2         2         2       2
//     3         4x4      2x2        2         2         2       2
//     4         2x2      1x1        2         2         2       2
// Here,
// filter_wd = filter_width and filter_ht = filter_height,
// skip_wd = skip_width and skip_ht = skip_height.
void av1_cnn_convolve_no_maxpool_padding_valid_avx2(
    const float **input, int in_width, int in_height, int in_stride,
    const CNN_LAYER_CONFIG *layer_config, float **output, int out_stride,
    int start_idx, int cstep, int channel_step) {
  if (layer_config->filter_width == 5 && layer_config->filter_height == 5 &&
      layer_config->skip_width == 4 && layer_config->skip_height == 4) {
    cnn_convolve_no_maxpool_padding_valid_5x5_avx2(
        input, in_width, in_height, in_stride, layer_config, output, out_stride,
        start_idx, cstep, channel_step);
  } else if (layer_config->filter_width == 2 &&
             layer_config->filter_height == 2 &&
             layer_config->skip_width == 2 && layer_config->skip_height == 2) {
    cnn_convolve_no_maxpool_padding_valid_2x2_avx2(
        input, in_width, in_height, in_stride, layer_config, output, out_stride,
        start_idx, cstep, channel_step);
  } else {
    av1_cnn_convolve_no_maxpool_padding_valid_c(
        input, in_width, in_height, in_stride, layer_config, output, out_stride,
        start_idx, cstep, channel_step);
  }
}
