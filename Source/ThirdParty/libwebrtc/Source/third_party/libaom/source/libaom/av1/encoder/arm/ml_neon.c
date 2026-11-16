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

#include <stdbool.h>
#include <assert.h>
#include <arm_neon.h>

#include "config/aom_config.h"
#include "config/av1_rtcd.h"
#include "av1/encoder/ml.h"

static void nn_activate8(float32x4_t *out_h, float32x4_t *out_l,
                         const float32x4_t *zero) {
  *out_h = vmaxq_f32(*out_h, *zero);
  *out_l = vmaxq_f32(*out_l, *zero);
}

static void nn_activate4(float32x4_t *x, const float32x4_t *zero) {
  *x = vmaxq_f32(*x, *zero);
}

#define CLAMP_0(x) (x = x > 0 ? x : 0)

static void nn_propagate_8to1(int num_inputs, const float *const inputs,
                              const float *const weights,
                              const float *layer_bias,
                              float *const output_nodes, bool output_layer) {
  const float32x4_t zero = vdupq_n_f32(0);
  float32x4_t vadd = zero;
  float total = *layer_bias;

  for (int in = 0; in < num_inputs; in += 8) {
    const float32x4_t inputs_h = vld1q_f32(&inputs[in + 4]);
    const float32x4_t inputs_l = vld1q_f32(&inputs[in]);

    const float32x4_t weights_h = vld1q_f32(&weights[in + 4]);
    const float32x4_t weights_l = vld1q_f32(&weights[in]);

    vadd = vmlaq_f32(vadd, inputs_h, weights_h);
    vadd = vmlaq_f32(vadd, inputs_l, weights_l);
  }
#if AOM_ARCH_AARCH64
  total += vaddvq_f32(vadd);
#else
  float32x2_t vadd_lo = vadd_f32(vget_low_f32(vadd), vget_high_f32(vadd));
  vadd_lo = vpadd_f32(vadd_lo, vadd_lo);
  total += vget_lane_f32(vadd_lo, 0);
#endif

  if (!output_layer) CLAMP_0(total);
  *output_nodes = total;
}

static void nn_propagate_xto1(int num_inputs, const float *const inputs,
                              const float *const weights,
                              const float *layer_bias,
                              float *const output_nodes) {
  float32x4_t vadd = vdupq_n_f32(0);

  float total = *layer_bias;
  int j = num_inputs;
  int in = 0;
  while (j > 7) {
    const float32x4_t inputs_h = vld1q_f32(&inputs[in + 4]);
    const float32x4_t inputs_l = vld1q_f32(&inputs[in]);

    const float32x4_t weights_h = vld1q_f32(&weights[in + 4]);
    const float32x4_t weights_l = vld1q_f32(&weights[in]);

    vadd = vmlaq_f32(vadd, inputs_h, weights_h);
    vadd = vmlaq_f32(vadd, inputs_l, weights_l);
    in += 8;
    j -= 8;
  }

#if AOM_ARCH_AARCH64
  total += vaddvq_f32(vadd);

#else
  float32x2_t vadd_lo = vadd_f32(vget_low_f32(vadd), vget_high_f32(vadd));
  vadd_lo = vpadd_f32(vadd_lo, vadd_lo);
  total += vget_lane_f32(vadd_lo, 0);
#endif
  for (; in < num_inputs; in++) total += weights[in] * inputs[in];

  *output_nodes = CLAMP_0(total);
}

static void nn_propagate_xsto1(int num_inputs, const float *const inputs,
                               const float *const weights,
                               const float *layer_bias,
                               float *const output_nodes) {
  float total = *layer_bias;
#if AOM_ARCH_AARCH64
  const float32x4_t v_inputs = vld1q_f32(inputs);
  const float32x4_t v_weights = vld1q_f32(weights);
  const float32x4_t vadd = vmulq_f32(v_inputs, v_weights);
  total += vaddvq_f32(vadd);
  int in = 4;
#else
  int in = 0;
#endif
  for (; in < num_inputs; in++) total += weights[in] * inputs[in];

  *output_nodes = CLAMP_0(total);
}

static void nn_propagate_4to1(int num_inputs, const float *const inputs,
                              const float *const weights,
                              const float *layer_bias,
                              float *const output_nodes, bool output_layer) {
  const float32x4_t zero = vdupq_n_f32(0);
  float32x4_t vadd = zero;
  float total = *layer_bias;

  for (int in = 0; in < num_inputs; in += 4) {
    const float32x4_t v_inputs = vld1q_f32(&inputs[in]);
    const float32x4_t v_weights = vld1q_f32(&weights[in]);
    vadd = vmlaq_f32(vadd, v_inputs, v_weights);
  }

#if AOM_ARCH_AARCH64
  total += vaddvq_f32(vadd);
#else
  float32x2_t vadd_lo = vadd_f32(vget_low_f32(vadd), vget_high_f32(vadd));
  vadd_lo = vpadd_f32(vadd_lo, vadd_lo);
  total += vget_lane_f32(vadd_lo, 0);
#endif

  if (!output_layer) CLAMP_0(total);
  *output_nodes = total;
}

static void nn_propagate_4to4(int num_inputs, const float *const inputs,
                              const float *const weights,
                              const float *layer_bias,
                              float *const output_nodes, bool output_layer) {
  float32x4_t outputs = vld1q_f32(layer_bias);
  const float32x4_t zero = vdupq_n_f32(0);

  float32x4_t mul0[2] = { zero, zero };
  float32x4_t mul1[2] = { zero, zero };
  for (int in = 0; in < num_inputs; in += 4) {
    const float32x4_t v_input = vld1q_f32(&inputs[in]);

    for (int i = 0; i < 2; i++) {
      const float32x4_t weight0 = vld1q_f32(&weights[in + 2 * i * num_inputs]);
      mul0[i] = vmlaq_f32(mul0[i], weight0, v_input);
      const float32x4_t weight1 =
          vld1q_f32(&weights[in + (2 * i + 1) * num_inputs]);
      mul1[i] = vmlaq_f32(mul1[i], weight1, v_input);
    }
  }
  for (int i = 0; i < 2; i++)
#if AOM_ARCH_AARCH64
    mul0[i] = vpaddq_f32(mul0[i], mul1[i]);
  const float32x4_t hh = vpaddq_f32(mul0[0], mul0[1]);
#else
    mul0[i] =
        vcombine_f32(vpadd_f32(vget_low_f32(mul0[i]), vget_high_f32(mul0[i])),
                     vpadd_f32(vget_low_f32(mul1[i]), vget_high_f32(mul1[i])));
  const float32x4_t hh =
      vcombine_f32(vpadd_f32(vget_low_f32(mul0[0]), vget_high_f32(mul0[0])),
                   vpadd_f32(vget_low_f32(mul0[1]), vget_high_f32(mul0[1])));
#endif

  outputs = vaddq_f32(outputs, hh);
  if (!output_layer) nn_activate4(&outputs, &zero);
  vst1q_f32(output_nodes, outputs);
}

static void nn_propagate_4to8(const int num_inputs, const float *const inputs,
                              const float *const weights,
                              const float *layer_bias,
                              float *const output_nodes, bool output_layer) {
  float32x4_t out_h = vld1q_f32(&layer_bias[4]);
  float32x4_t out_l = vld1q_f32(layer_bias);
  const float32x4_t zero = vdupq_n_f32(0);
  float32x4_t mul0[4] = { zero, zero, zero, zero };
  float32x4_t mul1[4] = { zero, zero, zero, zero };

  for (int in = 0; in < num_inputs; in += 4) {
    const float32x4_t v_input = vld1q_f32(&inputs[in]);
    for (int i = 0; i < 4; i++) {
      const float32x4_t weight0 = vld1q_f32(&weights[in + 2 * i * num_inputs]);
      const float32x4_t weight1 =
          vld1q_f32(&weights[in + (2 * i + 1) * num_inputs]);
      mul0[i] = vmlaq_f32(mul0[i], v_input, weight0);
      mul1[i] = vmlaq_f32(mul1[i], v_input, weight1);
    }
  }
  for (int i = 0; i < 4; i++)
#if AOM_ARCH_AARCH64
    mul0[i] = vpaddq_f32(mul0[i], mul1[i]);
  const float32x4_t hh0 = vpaddq_f32(mul0[0], mul0[1]);
  const float32x4_t hh1 = vpaddq_f32(mul0[2], mul0[3]);
#else
    mul0[i] =
        vcombine_f32(vpadd_f32(vget_low_f32(mul0[i]), vget_high_f32(mul0[i])),
                     vpadd_f32(vget_low_f32(mul1[i]), vget_high_f32(mul1[i])));
  const float32x4_t hh0 =
      vcombine_f32(vpadd_f32(vget_low_f32(mul0[0]), vget_high_f32(mul0[0])),
                   vpadd_f32(vget_low_f32(mul0[1]), vget_high_f32(mul0[1])));
  const float32x4_t hh1 =
      vcombine_f32(vpadd_f32(vget_low_f32(mul0[2]), vget_high_f32(mul0[2])),
                   vpadd_f32(vget_low_f32(mul0[3]), vget_high_f32(mul0[3])));
#endif

  out_h = vaddq_f32(out_h, hh1);
  out_l = vaddq_f32(out_l, hh0);

  if (!output_layer) nn_activate8(&out_h, &out_l, &zero);
  vst1q_f32(&output_nodes[4], out_h);
  vst1q_f32(output_nodes, out_l);
}

static void nn_propagate_8to4(const int num_inputs, const float *const inputs,
                              const float *const weights,
                              const float *layer_bias,
                              float *const output_nodes, bool output_layer) {
  float32x4_t outputs = vld1q_f32(layer_bias);
  const float32x4_t zero = vdupq_n_f32(0);
  float32x4_t add[4] = { zero, zero, zero, zero };
  for (int in = 0; in < num_inputs; in += 8) {
    const float32x4_t inputs_l = vld1q_f32(&inputs[in]);
    const float32x4_t inputs_h = vld1q_f32(&inputs[in + 4]);

    for (int i = 0; i < 4; i++) {
      const float32x4_t weight_l = vld1q_f32(&weights[in + i * num_inputs]);
      const float32x4_t weight_h = vld1q_f32(&weights[in + i * num_inputs + 4]);
      add[i] = vmlaq_f32(add[i], inputs_l, weight_l);
      add[i] = vmlaq_f32(add[i], inputs_h, weight_h);
    }
  }
#if AOM_ARCH_AARCH64
  const float32x4_t hadd_h = vpaddq_f32(add[2], add[3]);
  const float32x4_t hadd_l = vpaddq_f32(add[0], add[1]);
  const float32x4_t haddhadd = vpaddq_f32(hadd_l, hadd_h);
#else
  const float32x4_t hadd_h =
      vcombine_f32(vpadd_f32(vget_low_f32(add[2]), vget_high_f32(add[2])),
                   vpadd_f32(vget_low_f32(add[3]), vget_high_f32(add[3])));
  const float32x4_t hadd_l =
      vcombine_f32(vpadd_f32(vget_low_f32(add[0]), vget_high_f32(add[0])),
                   vpadd_f32(vget_low_f32(add[1]), vget_high_f32(add[1])));
  const float32x4_t haddhadd =
      vcombine_f32(vpadd_f32(vget_low_f32(hadd_l), vget_high_f32(hadd_l)),
                   vpadd_f32(vget_low_f32(hadd_h), vget_high_f32(hadd_h)));
#endif

  outputs = vaddq_f32(outputs, haddhadd);
  if (!output_layer) nn_activate4(&outputs, &zero);
  vst1q_f32(output_nodes, outputs);
}

// Calculate prediction based on the given input features and neural net config.
// Assume there are no more than NN_MAX_NODES_PER_LAYER nodes in each hidden
// layer.
void av1_nn_predict_neon(const float *input_nodes,
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
    float *const output_nodes = output_layer ? output : buf[buf_index];
    const int num_outputs = output_layer ? nn_config->num_outputs
                                         : nn_config->num_hidden_nodes[layer];

    if (num_inputs % 4 == 0 && num_outputs % 8 == 0) {
      for (int out = 0; out < num_outputs; out += 8) {
        nn_propagate_4to8(num_inputs, input_nodes,
                          &layer_weights[out * num_inputs], &layer_bias[out],
                          &output_nodes[out], output_layer);
      }
    } else if (num_inputs % 8 == 0 && num_outputs % 4 == 0) {
      for (int out = 0; out < num_outputs; out += 4) {
        nn_propagate_8to4(num_inputs, input_nodes,
                          &layer_weights[out * num_inputs], &layer_bias[out],
                          &output_nodes[out], output_layer);
      }
    } else if (num_inputs % 4 == 0 && num_outputs % 4 == 0) {
      for (int out = 0; out < num_outputs; out += 4) {
        nn_propagate_4to4(num_inputs, input_nodes,
                          &layer_weights[out * num_inputs], &layer_bias[out],
                          &output_nodes[out], output_layer);
      }
    } else if (num_inputs % 8 == 0) {
      for (int out = 0; out < num_outputs; out++) {
        nn_propagate_8to1(num_inputs, input_nodes,
                          &layer_weights[out * num_inputs], &layer_bias[out],
                          &output_nodes[out], output_layer);
      }
    } else if (num_inputs % 4 == 0) {
      for (int out = 0; out < num_outputs; out++) {
        nn_propagate_4to1(num_inputs, input_nodes,
                          &layer_weights[out * num_inputs], &layer_bias[out],
                          &output_nodes[out], output_layer);
      }
    } else if (num_inputs > 8) {
      for (int out = 0; out < num_outputs; out++) {
        nn_propagate_xto1(num_inputs, input_nodes,
                          &layer_weights[out * num_inputs], &layer_bias[out],
                          &output_nodes[out]);
      }
    } else if (num_inputs >= 4) {
      for (int out = 0; out < num_outputs; out++) {
        nn_propagate_xsto1(num_inputs, input_nodes,
                           &layer_weights[out * num_inputs], &layer_bias[out],
                           &output_nodes[out]);
      }
    } else {
      for (int node = 0; node < num_outputs; ++node) {
        float val = layer_bias[node];
        for (int i = 0; i < num_inputs; ++i)
          val += layer_weights[node * num_inputs + i] * input_nodes[i];
        // ReLU as activation function.
        val = val > 0.0f ? val : 0.0f;  // Could use AOMMAX().
        output_nodes[node] = val;
      }
    }
    input_nodes = output_nodes;
    num_inputs = num_outputs;
    buf_index = 1 - buf_index;
  }
  if (reduce_prec) av1_nn_output_prec_reduce(output, nn_config->num_outputs);
}
