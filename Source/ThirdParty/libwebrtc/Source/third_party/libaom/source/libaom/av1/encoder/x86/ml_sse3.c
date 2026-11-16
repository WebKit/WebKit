/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved.
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

#include "config/av1_rtcd.h"
#include "av1/encoder/ml.h"
#include "av1/encoder/x86/ml_sse3.h"

// In order to avoid the high-latency of swapping between FPU and SIMD
// operations, we keep the result in a 128-bit register even though we only
// care about a single value.
static void nn_propagate_8to1(const float *const inputs,
                              const float *const weights,
                              __m128 *const output) {
  const __m128 inputs_h = _mm_loadu_ps(&inputs[4]);
  const __m128 inputs_l = _mm_loadu_ps(inputs);

  const __m128 weights_h = _mm_loadu_ps(&weights[4]);
  const __m128 weights_l = _mm_loadu_ps(weights);

  const __m128 mul_h = _mm_mul_ps(inputs_h, weights_h);
  const __m128 mul_l = _mm_mul_ps(inputs_l, weights_l);
  // [7 6 5 4] [3 2 1 0] (weight and input indices)

  const __m128 vadd = _mm_add_ps(mul_l, mul_h);
  // [7+3 6+2 5+1 4+0]
  const __m128 hadd1 = _mm_hadd_ps(vadd, vadd);
  // [7+6+3+2 5+4+1+0 7+6+3+2 5+4+1+0]
  const __m128 hadd2 = _mm_hadd_ps(hadd1, hadd1);
  // [7+6+5+4+3+2+1+0 7+6+5+4+3+2+1+0 7+6+5+4+3+2+1+0 7+6+5+4+3+2+1+0]
  *output = _mm_add_ps(*output, hadd2);
}

void av1_nn_propagate_4to1_sse3(const float *const inputs,
                                const float *const weights,
                                __m128 *const output) {
  const __m128 inputs128 = _mm_loadu_ps(inputs);

  const __m128 weights128 = _mm_loadu_ps(weights);

  const __m128 mul = _mm_mul_ps(inputs128, weights128);
  // [3 2 1 0] (weight and input indices)

  const __m128 hadd1 = _mm_hadd_ps(mul, mul);
  // [3+2 1+0 3+2 1+0]
  const __m128 hadd2 = _mm_hadd_ps(hadd1, hadd1);
  // [3+2+1+0 3+2+1+0 3+2+1+0 3+2+1+0]
  *output = _mm_add_ps(*output, hadd2);
}

void av1_nn_propagate_4to4_sse3(const float *const inputs,
                                const float *const weights,
                                __m128 *const outputs, const int num_inputs) {
  const __m128 inputs128 = _mm_loadu_ps(inputs);

  __m128 hadd[2];
  for (int i = 0; i < 2; i++) {  // For each pair of outputs
    const __m128 weight0 = _mm_loadu_ps(&weights[2 * i * num_inputs]);
    const __m128 mul0 = _mm_mul_ps(weight0, inputs128);
    const __m128 weight1 = _mm_loadu_ps(&weights[(2 * i + 1) * num_inputs]);
    const __m128 mul1 = _mm_mul_ps(weight1, inputs128);
    hadd[i] = _mm_hadd_ps(mul0, mul1);
  }
  // hadd[0] = [7+6 5+4 3+2 1+0] (weight indices)
  // hadd[1] = [15+14 13+12 11+10 9+8]

  const __m128 hh = _mm_hadd_ps(hadd[0], hadd[1]);
  // [15+14+13+12 11+10+9+8 7+6+5+4 3+2+1+0]

  *outputs = _mm_add_ps(*outputs, hh);
}

void av1_nn_propagate_4to8_sse3(const float *const inputs,
                                const float *const weights, __m128 *const out_h,
                                __m128 *const out_l, const int num_inputs) {
  const __m128 inputs128 = _mm_loadu_ps(inputs);

  __m128 hadd[4];
  for (int i = 0; i < 4; i++) {  // For each pair of outputs
    const __m128 weight0 = _mm_loadu_ps(&weights[2 * i * num_inputs]);
    const __m128 weight1 = _mm_loadu_ps(&weights[(2 * i + 1) * num_inputs]);
    const __m128 mul0 = _mm_mul_ps(inputs128, weight0);
    const __m128 mul1 = _mm_mul_ps(inputs128, weight1);
    hadd[i] = _mm_hadd_ps(mul0, mul1);
  }
  // hadd[0] = [7+6 5+4 3+2 1+0] (weight indices)
  // hadd[1] = [15+14 13+12 11+10 9+8]
  // hadd[2] = [23+22 21+20 19+18 17+16]
  // hadd[3] = [31+30 29+28 27+26 25+24]

  const __m128 hh0 = _mm_hadd_ps(hadd[0], hadd[1]);
  // [15+14+13+12 11+10+9+8 7+6+5+4 3+2+1+0]
  const __m128 hh1 = _mm_hadd_ps(hadd[2], hadd[3]);
  // [31+30+29+28 27+26+25+24 23+22+21+20 19+18+17+16]

  *out_h = _mm_add_ps(*out_h, hh1);
  *out_l = _mm_add_ps(*out_l, hh0);
}

static void nn_propagate_8to4(const float *const inputs,
                              const float *const weights, __m128 *const outputs,
                              const int num_inputs) {
  const __m128 inputs_h = _mm_loadu_ps(inputs + 4);
  const __m128 inputs_l = _mm_loadu_ps(inputs);
  // [7 6 5 4] [3 2 1 0] (input indices)

  __m128 add[4];
  for (int i = 0; i < 4; i++) {  // For each output:
    const __m128 weight_h = _mm_loadu_ps(&weights[i * num_inputs + 4]);
    const __m128 weight_l = _mm_loadu_ps(&weights[i * num_inputs]);
    const __m128 mul_h = _mm_mul_ps(inputs_h, weight_h);
    const __m128 mul_l = _mm_mul_ps(inputs_l, weight_l);
    add[i] = _mm_add_ps(mul_l, mul_h);
  }
  // add[0] = [7+3 6+2 5+1 4+0]
  // add[1] = [15+11 14+10 13+9 12+8]
  // add[2] = [23+19 22+18 21+17 20+16]
  // add[3] = [31+27 30+26 29+25 28+24]

  const __m128 hadd_h = _mm_hadd_ps(add[2], add[3]);
  // [31+30+27+26 29+28+25+24 23+22+19+18 21+20+17+16]
  const __m128 hadd_l = _mm_hadd_ps(add[0], add[1]);
  // [15+14+11+10 13+12+9+8 7+6+3+2 5+4+1+0]

  const __m128 haddhadd = _mm_hadd_ps(hadd_l, hadd_h);
  // [31+30+29+28+27+26+25+24 23+22+21+20+19+18+17+16
  //  15+14+13+12+11+10+9+8 7+6+5+4+3+2+1+0]

  *outputs = _mm_add_ps(*outputs, haddhadd);
}

static void nn_activate8(__m128 *out_h, __m128 *out_l) {
  const __m128 zero = _mm_setzero_ps();
  *out_h = _mm_max_ps(*out_h, zero);
  *out_l = _mm_max_ps(*out_l, zero);
}

static void nn_activate4(__m128 *x) { *x = _mm_max_ps(*x, _mm_setzero_ps()); }

// Calculate prediction based on the given input features and neural net config.
// Assume there are no more than NN_MAX_NODES_PER_LAYER nodes in each hidden
// layer.
void av1_nn_predict_sse3(const float *input_nodes,
                         const NN_CONFIG *const nn_config, int reduce_prec,
                         float *const output) {
  float buf[2][NN_MAX_NODES_PER_LAYER];
  int buf_index = 0;
  int num_inputs = nn_config->num_inputs;

  // Hidden layers, except the final iteration is the output layer.
  for (int layer = 0; layer <= nn_config->num_hidden_layers; layer++) {
    const float *layer_weights = nn_config->weights[layer];
    const float *layer_bias = nn_config->bias[layer];
    bool output_layer = (layer == nn_config->num_hidden_layers);
    float *const output_nodes = output_layer ? output : &buf[buf_index][0];
    const int num_outputs = output_layer ? nn_config->num_outputs
                                         : nn_config->num_hidden_nodes[layer];

    if (num_inputs % 4 == 0 && num_outputs % 8 == 0) {
      for (int out = 0; out < num_outputs; out += 8) {
        __m128 out_h = _mm_loadu_ps(&layer_bias[out + 4]);
        __m128 out_l = _mm_loadu_ps(&layer_bias[out]);
        for (int in = 0; in < num_inputs; in += 4) {
          av1_nn_propagate_4to8_sse3(&input_nodes[in],
                                     &layer_weights[out * num_inputs + in],
                                     &out_h, &out_l, num_inputs);
        }
        if (!output_layer) nn_activate8(&out_h, &out_l);
        _mm_storeu_ps(&output_nodes[out + 4], out_h);
        _mm_storeu_ps(&output_nodes[out], out_l);
      }
    } else if (num_inputs % 8 == 0 && num_outputs % 4 == 0) {
      for (int out = 0; out < num_outputs; out += 4) {
        __m128 outputs = _mm_loadu_ps(&layer_bias[out]);
        for (int in = 0; in < num_inputs; in += 8) {
          nn_propagate_8to4(&input_nodes[in],
                            &layer_weights[out * num_inputs + in], &outputs,
                            num_inputs);
        }
        if (!output_layer) nn_activate4(&outputs);
        _mm_storeu_ps(&output_nodes[out], outputs);
      }
    } else if (num_inputs % 4 == 0 && num_outputs % 4 == 0) {
      for (int out = 0; out < num_outputs; out += 4) {
        __m128 outputs = _mm_loadu_ps(&layer_bias[out]);
        for (int in = 0; in < num_inputs; in += 4) {
          av1_nn_propagate_4to4_sse3(&input_nodes[in],
                                     &layer_weights[out * num_inputs + in],
                                     &outputs, num_inputs);
        }
        if (!output_layer) nn_activate4(&outputs);
        _mm_storeu_ps(&output_nodes[out], outputs);
      }
    } else if (num_inputs % 8 == 0) {
      for (int out = 0; out < num_outputs; out++) {
        __m128 total = _mm_load1_ps(&layer_bias[out]);
        for (int in = 0; in < num_inputs; in += 8) {
          nn_propagate_8to1(&input_nodes[in],
                            &layer_weights[out * num_inputs + in], &total);
        }
        if (!output_layer) nn_activate4(&total);
        output_nodes[out] = _mm_cvtss_f32(total);
      }
    } else if (num_inputs % 4 == 0) {
      for (int out = 0; out < num_outputs; out++) {
        __m128 total = _mm_load1_ps(&layer_bias[out]);
        for (int in = 0; in < num_inputs; in += 4) {
          av1_nn_propagate_4to1_sse3(
              &input_nodes[in], &layer_weights[out * num_inputs + in], &total);
        }
        if (!output_layer) nn_activate4(&total);
        output_nodes[out] = _mm_cvtss_f32(total);
      }
    } else {
      // Use SSE instructions for scalar operations to avoid the latency of
      // swapping between SIMD and FPU modes.
      for (int out = 0; out < num_outputs; out++) {
        __m128 total = _mm_load1_ps(&layer_bias[out]);
        for (int in_node = 0; in_node < num_inputs; in_node++) {
          __m128 input = _mm_load1_ps(&input_nodes[in_node]);
          __m128 weight =
              _mm_load1_ps(&layer_weights[num_inputs * out + in_node]);
          total = _mm_add_ps(total, _mm_mul_ps(input, weight));
        }
        if (!output_layer) nn_activate4(&total);
        output_nodes[out] = _mm_cvtss_f32(total);
      }
    }
    input_nodes = output_nodes;
    num_inputs = num_outputs;
    buf_index = 1 - buf_index;
  }
  if (reduce_prec) av1_nn_output_prec_reduce(output, nn_config->num_outputs);
}

// Based on N. N. Schraudolph. A Fast, Compact Approximation of the Exponential
// Function. Neural Computation, 11(4):853–862, 1999.
static inline __m128 approx_exp(__m128 y) {
#define A ((1 << 23) / 0.69314718056f)  // (1 << 23) / ln(2)
#define B \
  127  // Offset for the exponent according to IEEE floating point standard.
#define C 60801  // Magic number controls the accuracy of approximation
  const __m128 multiplier = _mm_set1_ps(A);
  const __m128i offset = _mm_set1_epi32(B * (1 << 23) - C);

  y = _mm_mul_ps(y, multiplier);
  y = _mm_castsi128_ps(_mm_add_epi32(_mm_cvtps_epi32(y), offset));
  return y;
#undef A
#undef B
#undef C
}

static inline __m128 reduce_max(__m128 reg) {
  __m128 tmp_reg;

  tmp_reg = _mm_shuffle_ps(reg, reg, 0x4e);  // 01 00 11 10
  reg = _mm_max_ps(reg, tmp_reg);

  tmp_reg = _mm_shuffle_ps(reg, reg, 0xb1);  // 10 11 00 01
  reg = _mm_max_ps(reg, tmp_reg);

  return reg;
}

static inline __m128 reduce_sum(__m128 reg) {
  __m128 tmp_reg;

  tmp_reg = _mm_shuffle_ps(reg, reg, 0x4e);  // 01 00 11 10
  reg = _mm_add_ps(reg, tmp_reg);

  tmp_reg = _mm_shuffle_ps(reg, reg, 0xb1);  // 10 11 00 01
  reg = _mm_add_ps(reg, tmp_reg);

  return reg;
}

void av1_nn_fast_softmax_16_sse3(const float *input, float *output) {
  // Clips at -10 to avoid underflowing
  const __m128 clipper = _mm_set1_ps(-10.0f);

  // Load in 16 values
  __m128 in_0 = _mm_loadu_ps(&input[0]);
  __m128 in_1 = _mm_loadu_ps(&input[4]);
  __m128 in_2 = _mm_loadu_ps(&input[8]);
  __m128 in_3 = _mm_loadu_ps(&input[12]);

  // Get the max
  __m128 max_0 = _mm_max_ps(in_0, in_1);
  __m128 max_1 = _mm_max_ps(in_2, in_3);

  max_0 = _mm_max_ps(max_0, max_1);
  max_0 = reduce_max(max_0);

  // Subtract the max off and clip
  in_0 = _mm_sub_ps(in_0, max_0);
  in_1 = _mm_sub_ps(in_1, max_0);
  in_2 = _mm_sub_ps(in_2, max_0);
  in_3 = _mm_sub_ps(in_3, max_0);

  in_0 = _mm_max_ps(in_0, clipper);
  in_1 = _mm_max_ps(in_1, clipper);
  in_2 = _mm_max_ps(in_2, clipper);
  in_3 = _mm_max_ps(in_3, clipper);

  // Exponentiate and compute the denominator
  __m128 sum = in_0 = approx_exp(in_0);
  in_1 = approx_exp(in_1);
  sum = _mm_add_ps(sum, in_1);
  in_2 = approx_exp(in_2);
  sum = _mm_add_ps(sum, in_2);
  in_3 = approx_exp(in_3);
  sum = _mm_add_ps(sum, in_3);
  sum = reduce_sum(sum);

  // Divide to get the probability
  in_0 = _mm_div_ps(in_0, sum);
  in_1 = _mm_div_ps(in_1, sum);
  in_2 = _mm_div_ps(in_2, sum);
  in_3 = _mm_div_ps(in_3, sum);

  _mm_storeu_ps(&output[0], in_0);
  _mm_storeu_ps(&output[4], in_1);
  _mm_storeu_ps(&output[8], in_2);
  _mm_storeu_ps(&output[12], in_3);
}
