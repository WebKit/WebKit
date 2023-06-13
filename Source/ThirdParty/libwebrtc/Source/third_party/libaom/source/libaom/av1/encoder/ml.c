/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>
#include <math.h>

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/mathutils.h"
#include "av1/encoder/ml.h"

void av1_nn_output_prec_reduce(float *const output, int num_output) {
  const int prec_bits = 9;
  const int prec = 1 << prec_bits;
  const float inv_prec = (float)(1.0 / prec);
  for (int i = 0; i < num_output; i++) {
    output[i] = ((int)(output[i] * prec + 0.5)) * inv_prec;
  }
}

// Calculate prediction based on the given input features and neural net config.
// Assume there are no more than NN_MAX_NODES_PER_LAYER nodes in each hidden
// layer.
void av1_nn_predict_c(const float *input_nodes,
                      const NN_CONFIG *const nn_config, int reduce_prec,
                      float *const output) {
  int num_input_nodes = nn_config->num_inputs;
  int buf_index = 0;
  float buf[2][NN_MAX_NODES_PER_LAYER];

  // Propagate hidden layers.
  const int num_layers = nn_config->num_hidden_layers;
  assert(num_layers <= NN_MAX_HIDDEN_LAYERS);
  for (int layer = 0; layer < num_layers; ++layer) {
    const float *layer_weights = nn_config->weights[layer];
    const float *layer_bias = nn_config->bias[layer];
    float *output_nodes = buf[buf_index];
    const int num_output_nodes = nn_config->num_hidden_nodes[layer];
    assert(num_output_nodes < NN_MAX_NODES_PER_LAYER);
    for (int node = 0; node < num_output_nodes; ++node) {
      float val = layer_bias[node];
      for (int i = 0; i < num_input_nodes; ++i)
        val += layer_weights[node * num_input_nodes + i] * input_nodes[i];
      // ReLU as activation function.
      val = val > 0.0f ? val : 0.0f;  // Could use AOMMAX().
      output_nodes[node] = val;
    }
    num_input_nodes = num_output_nodes;
    input_nodes = output_nodes;
    buf_index = 1 - buf_index;
  }

  // Final output layer.
  const float *layer_weights = nn_config->weights[num_layers];
  const float *layer_bias = nn_config->bias[num_layers];
  for (int node = 0; node < nn_config->num_outputs; ++node) {
    float val = layer_bias[node];
    for (int i = 0; i < num_input_nodes; ++i)
      val += layer_weights[node * num_input_nodes + i] * input_nodes[i];
    output[node] = val;
  }
  if (reduce_prec) av1_nn_output_prec_reduce(output, nn_config->num_outputs);
}

#if CONFIG_NN_V2
// Applies the ReLu activation to one fc layer
// output[i] = Max(input[i],0.0f)
static float *nn_relu(const float *input, FC_LAYER *layer) {
  for (int i = 0; i < layer->num_outputs; ++i) {
    layer->output[i] = AOMMAX(input[i], 0.0f);
  }

  return layer->output;
}

// Applies the Sigmoid activation to one fc layer
// output[i] = 1/(1+exp(input[i]))
static float *nn_sigmoid(const float *input, FC_LAYER *layer) {
  for (int i = 0; i < layer->num_outputs; ++i) {
    const float tmp = AOMMIN(AOMMAX(input[i], -10.0f), 10.0f);
    layer->output[i] = 1.0f / (1.0f + expf(-tmp));
  }

  return layer->output;
}

// Forward prediction in one fc layer, used in function av1_nn_predict_V2
static float *nn_fc_forward(const float *input, FC_LAYER *layer) {
  const float *weights = layer->weights;
  const float *bias = layer->bias;
  assert(layer->num_outputs < NN_MAX_NODES_PER_LAYER);
  // fc
  for (int node = 0; node < layer->num_outputs; ++node) {
    float val = bias[node];
    for (int i = 0; i < layer->num_inputs; ++i) val += weights[i] * input[i];
    layer->output[node] = val;
    weights += layer->num_inputs;
  }

  // activation
  switch (layer->activation) {
    case NONE: return layer->output;
    case RELU: return nn_relu(layer->output, layer);
    case SIGMOID: return nn_sigmoid(layer->output, layer);
    case SOFTSIGN:
      assert(0 && "Softsign has not been supported in NN.");  // TO DO
      return NULL;
    default:
      assert(0 && "Unknown activation");  // Unknown activation
      return NULL;
  }
}

void av1_nn_predict_v2(const float *feature, NN_CONFIG_V2 *nn_config,
                       int reduce_prec, float *output) {
  const float *input_nodes = feature;

  // Propagate the layers.
  const int num_layers = nn_config->num_hidden_layers;
  assert(num_layers <= NN_MAX_HIDDEN_LAYERS);
  for (int i = 0; i < num_layers; ++i) {
    input_nodes = nn_fc_forward(input_nodes, nn_config->layer + i);
    assert(nn_config->layer[i + 1].num_inputs ==
           nn_config->layer[i].num_outputs);
  }

  // Final layer
  input_nodes = nn_fc_forward(input_nodes, nn_config->layer + num_layers);
  assert(nn_config->layer[num_layers].num_outputs == nn_config->num_logits);
  // Copy the final layer output
  memcpy(output, input_nodes, sizeof(*input_nodes) * nn_config->num_logits);
  if (reduce_prec) av1_nn_output_prec_reduce(output, nn_config->num_logits);
}
#endif  // CONFIG_NN_V2

void av1_nn_softmax(const float *input, float *output, int n) {
  // Softmax function is invariant to adding the same constant
  // to all input values, so we subtract the maximum input to avoid
  // possible overflow.
  float max_input = input[0];
  for (int i = 1; i < n; i++) max_input = AOMMAX(max_input, input[i]);
  float sum_out = 0.0f;
  for (int i = 0; i < n; i++) {
    // Clamp to range [-10.0, 0.0] to prevent FE_UNDERFLOW errors.
    const float normalized_input = AOMMAX(input[i] - max_input, -10.0f);
    output[i] = expf(normalized_input);
    sum_out += output[i];
  }
  for (int i = 0; i < n; i++) output[i] /= sum_out;
}

void av1_nn_fast_softmax_16_c(const float *input, float *output) {
  const int kNumClasses = 16;
  float max_input = input[0];
  for (int i = 1; i < kNumClasses; i++) max_input = AOMMAX(max_input, input[i]);
  float sum_out = 0.0f;
  for (int i = 0; i < kNumClasses; i++) {
    // Clamp to range [-10.0, 0.0] to prevent FE_UNDERFLOW errors.
    const float normalized_input = AOMMAX(input[i] - max_input, -10.0f);
    output[i] = approx_exp(normalized_input);
    sum_out += output[i];
  }
  for (int i = 0; i < kNumClasses; i++) output[i] /= sum_out;
}
