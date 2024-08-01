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

#include <stdbool.h>
#include <assert.h>
#include <immintrin.h>

#include "config/av1_rtcd.h"
#include "av1/encoder/ml.h"
#include "av1/encoder/x86/ml_sse3.h"

#define CALC_OUTPUT_FOR_2ROWS                                               \
  const int index = weight_idx + (2 * i * tot_num_inputs);                  \
  const __m256 weight0 = _mm256_loadu_ps(&weights[index]);                  \
  const __m256 weight1 = _mm256_loadu_ps(&weights[index + tot_num_inputs]); \
  const __m256 mul0 = _mm256_mul_ps(inputs256, weight0);                    \
  const __m256 mul1 = _mm256_mul_ps(inputs256, weight1);                    \
  hadd[i] = _mm256_hadd_ps(mul0, mul1);

static INLINE void nn_propagate_8to1(
    const float *const inputs, const float *const weights,
    const float *const bias, int num_inputs_to_process, int tot_num_inputs,
    int num_outputs, float *const output_nodes, int is_clip_required) {
  // Process one output row at a time.
  for (int out = 0; out < num_outputs; out++) {
    __m256 in_result = _mm256_setzero_ps();
    float bias_val = bias[out];
    for (int in = 0; in < num_inputs_to_process; in += 8) {
      const __m256 inputs256 = _mm256_loadu_ps(&inputs[in]);
      const int weight_idx = in + (out * tot_num_inputs);
      const __m256 weight0 = _mm256_loadu_ps(&weights[weight_idx]);
      const __m256 mul0 = _mm256_mul_ps(inputs256, weight0);
      in_result = _mm256_add_ps(in_result, mul0);
    }
    const __m128 low_128 = _mm256_castps256_ps128(in_result);
    const __m128 high_128 = _mm256_extractf128_ps(in_result, 1);
    const __m128 sum_par_0 = _mm_add_ps(low_128, high_128);
    const __m128 sum_par_1 = _mm_hadd_ps(sum_par_0, sum_par_0);
    const __m128 sum_tot =
        _mm_add_ps(_mm_shuffle_ps(sum_par_1, sum_par_1, 0x99), sum_par_1);

    bias_val += _mm_cvtss_f32(sum_tot);
    if (is_clip_required) bias_val = AOMMAX(bias_val, 0);
    output_nodes[out] = bias_val;
  }
}

static INLINE void nn_propagate_8to4(
    const float *const inputs, const float *const weights,
    const float *const bias, int num_inputs_to_process, int tot_num_inputs,
    int num_outputs, float *const output_nodes, int is_clip_required) {
  __m256 hadd[2];
  for (int out = 0; out < num_outputs; out += 4) {
    __m128 bias_reg = _mm_loadu_ps(&bias[out]);
    __m128 in_result = _mm_setzero_ps();
    for (int in = 0; in < num_inputs_to_process; in += 8) {
      const __m256 inputs256 = _mm256_loadu_ps(&inputs[in]);
      const int weight_idx = in + (out * tot_num_inputs);
      // Process two output row at a time.
      for (int i = 0; i < 2; i++) {
        CALC_OUTPUT_FOR_2ROWS
      }

      const __m256 sum_par = _mm256_hadd_ps(hadd[0], hadd[1]);
      const __m128 low_128 = _mm256_castps256_ps128(sum_par);
      const __m128 high_128 = _mm256_extractf128_ps(sum_par, 1);
      const __m128 result = _mm_add_ps(low_128, high_128);

      in_result = _mm_add_ps(in_result, result);
    }

    in_result = _mm_add_ps(in_result, bias_reg);
    if (is_clip_required) in_result = _mm_max_ps(in_result, _mm_setzero_ps());
    _mm_storeu_ps(&output_nodes[out], in_result);
  }
}

static INLINE void nn_propagate_8to8(
    const float *const inputs, const float *const weights,
    const float *const bias, int num_inputs_to_process, int tot_num_inputs,
    int num_outputs, float *const output_nodes, int is_clip_required) {
  __m256 hadd[4];
  for (int out = 0; out < num_outputs; out += 8) {
    __m256 bias_reg = _mm256_loadu_ps(&bias[out]);
    __m256 in_result = _mm256_setzero_ps();
    for (int in = 0; in < num_inputs_to_process; in += 8) {
      const __m256 inputs256 = _mm256_loadu_ps(&inputs[in]);
      const int weight_idx = in + (out * tot_num_inputs);
      // Process two output rows at a time.
      for (int i = 0; i < 4; i++) {
        CALC_OUTPUT_FOR_2ROWS
      }
      const __m256 hh0 = _mm256_hadd_ps(hadd[0], hadd[1]);
      const __m256 hh1 = _mm256_hadd_ps(hadd[2], hadd[3]);

      __m256 ht_0 = _mm256_permute2f128_ps(hh0, hh1, 0x20);
      __m256 ht_1 = _mm256_permute2f128_ps(hh0, hh1, 0x31);

      __m256 result = _mm256_add_ps(ht_0, ht_1);
      in_result = _mm256_add_ps(in_result, result);
    }
    in_result = _mm256_add_ps(in_result, bias_reg);
    if (is_clip_required)
      in_result = _mm256_max_ps(in_result, _mm256_setzero_ps());
    _mm256_storeu_ps(&output_nodes[out], in_result);
  }
}

static INLINE void nn_propagate_input_multiple_of_8(
    const float *const inputs, const float *const weights,
    const float *const bias, int num_inputs_to_process, int tot_num_inputs,
    bool is_output_layer, int num_outputs, float *const output_nodes) {
  // The saturation of output is considered for hidden layer which is not equal
  // to final hidden layer.
  const int is_clip_required =
      !is_output_layer && num_inputs_to_process == tot_num_inputs;
  if (num_outputs % 8 == 0) {
    nn_propagate_8to8(inputs, weights, bias, num_inputs_to_process,
                      tot_num_inputs, num_outputs, output_nodes,
                      is_clip_required);
  } else if (num_outputs % 4 == 0) {
    nn_propagate_8to4(inputs, weights, bias, num_inputs_to_process,
                      tot_num_inputs, num_outputs, output_nodes,
                      is_clip_required);
  } else {
    nn_propagate_8to1(inputs, weights, bias, num_inputs_to_process,
                      tot_num_inputs, num_outputs, output_nodes,
                      is_clip_required);
  }
}

void av1_nn_predict_avx2(const float *input_nodes,
                         const NN_CONFIG *const nn_config, int reduce_prec,
                         float *const output) {
  float buf[2][NN_MAX_NODES_PER_LAYER];
  int buf_index = 0;
  int num_inputs = nn_config->num_inputs;
  assert(num_inputs > 0 && num_inputs <= NN_MAX_NODES_PER_LAYER);

  for (int layer = 0; layer <= nn_config->num_hidden_layers; layer++) {
    const float *layer_weights = nn_config->weights[layer];
    const float *layer_bias = nn_config->bias[layer];
    bool is_output_layer = layer == nn_config->num_hidden_layers;
    float *const output_nodes = is_output_layer ? output : &buf[buf_index][0];
    const int num_outputs = is_output_layer
                                ? nn_config->num_outputs
                                : nn_config->num_hidden_nodes[layer];
    assert(num_outputs > 0 && num_outputs <= NN_MAX_NODES_PER_LAYER);

    // Process input multiple of 8 using AVX2 intrinsic.
    if (num_inputs % 8 == 0) {
      nn_propagate_input_multiple_of_8(input_nodes, layer_weights, layer_bias,
                                       num_inputs, num_inputs, is_output_layer,
                                       num_outputs, output_nodes);
    } else {
      // When number of inputs is not multiple of 8, use hybrid approach of AVX2
      // and SSE3 based on the need.
      const int in_mul_8 = num_inputs / 8;
      const int num_inputs_to_process = in_mul_8 * 8;
      int bias_is_considered = 0;
      if (in_mul_8) {
        nn_propagate_input_multiple_of_8(
            input_nodes, layer_weights, layer_bias, num_inputs_to_process,
            num_inputs, is_output_layer, num_outputs, output_nodes);
        bias_is_considered = 1;
      }

      const float *out_temp = bias_is_considered ? output_nodes : layer_bias;
      const int input_remaining = num_inputs % 8;
      if (input_remaining % 4 == 0 && num_outputs % 8 == 0) {
        for (int out = 0; out < num_outputs; out += 8) {
          __m128 out_h = _mm_loadu_ps(&out_temp[out + 4]);
          __m128 out_l = _mm_loadu_ps(&out_temp[out]);
          for (int in = in_mul_8 * 8; in < num_inputs; in += 4) {
            av1_nn_propagate_4to8_sse3(&input_nodes[in],
                                       &layer_weights[out * num_inputs + in],
                                       &out_h, &out_l, num_inputs);
          }
          if (!is_output_layer) {
            const __m128 zero = _mm_setzero_ps();
            out_h = _mm_max_ps(out_h, zero);
            out_l = _mm_max_ps(out_l, zero);
          }
          _mm_storeu_ps(&output_nodes[out + 4], out_h);
          _mm_storeu_ps(&output_nodes[out], out_l);
        }
      } else if (input_remaining % 4 == 0 && num_outputs % 4 == 0) {
        for (int out = 0; out < num_outputs; out += 4) {
          __m128 outputs = _mm_loadu_ps(&out_temp[out]);
          for (int in = in_mul_8 * 8; in < num_inputs; in += 4) {
            av1_nn_propagate_4to4_sse3(&input_nodes[in],
                                       &layer_weights[out * num_inputs + in],
                                       &outputs, num_inputs);
          }
          if (!is_output_layer) outputs = _mm_max_ps(outputs, _mm_setzero_ps());
          _mm_storeu_ps(&output_nodes[out], outputs);
        }
      } else if (input_remaining % 4 == 0) {
        for (int out = 0; out < num_outputs; out++) {
          __m128 outputs = _mm_load1_ps(&out_temp[out]);
          for (int in = in_mul_8 * 8; in < num_inputs; in += 4) {
            av1_nn_propagate_4to1_sse3(&input_nodes[in],
                                       &layer_weights[out * num_inputs + in],
                                       &outputs);
          }
          if (!is_output_layer) outputs = _mm_max_ps(outputs, _mm_setzero_ps());
          output_nodes[out] = _mm_cvtss_f32(outputs);
        }
      } else {
        // Use SSE instructions for scalar operations to avoid the latency
        // of swapping between SIMD and FPU modes.
        for (int out = 0; out < num_outputs; out++) {
          __m128 outputs = _mm_load1_ps(&out_temp[out]);
          for (int in_node = in_mul_8 * 8; in_node < num_inputs; in_node++) {
            __m128 input = _mm_load1_ps(&input_nodes[in_node]);
            __m128 weight =
                _mm_load1_ps(&layer_weights[num_inputs * out + in_node]);
            outputs = _mm_add_ps(outputs, _mm_mul_ps(input, weight));
          }
          if (!is_output_layer) outputs = _mm_max_ps(outputs, _mm_setzero_ps());
          output_nodes[out] = _mm_cvtss_f32(outputs);
        }
      }
    }
    // Before processing the next layer, treat the output of current layer as
    // input to next layer.
    input_nodes = output_nodes;
    num_inputs = num_outputs;
    buf_index = 1 - buf_index;
  }
  if (reduce_prec) av1_nn_output_prec_reduce(output, nn_config->num_outputs);
}
