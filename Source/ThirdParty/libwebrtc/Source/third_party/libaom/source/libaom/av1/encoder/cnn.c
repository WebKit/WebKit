/*
 * Copyright (c) 2019, Alliance for Open Media. All rights reserved.
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
#include <stdbool.h>

#include "aom_dsp/aom_dsp_common.h"
#include "av1/common/av1_common_int.h"
#include "av1/encoder/cnn.h"

#define CLAMPINDEX(a, hi) ((a) < 0 ? 0 : ((a) >= (hi) ? ((hi)-1) : (a)))

typedef struct {
  const float **input;
  int in_width;
  int in_height;
  int in_stride;
  const CNN_LAYER_CONFIG *layer_config;
  float **output;
  int out_stride;
  int start_idx;
  int th_step;
} CONVOLVE_OPS;

static inline float softsign(float x) { return x / (fabsf(x) + 1.0f); }

static inline float relu(float x) { return (x < 0) ? 0 : x; }

typedef struct {
  int allocsize;
  int channels;
  int width, height, stride;
  float *buf[CNN_MAX_CHANNELS];
} TENSOR;

static void init_tensor(TENSOR *tensor) { memset(tensor, 0, sizeof(*tensor)); }

static void free_tensor(TENSOR *tensor) {
  if (tensor->allocsize) {
    aom_free(tensor->buf[0]);
    tensor->buf[0] = NULL;
    tensor->allocsize = 0;
  }
}

static bool realloc_tensor(TENSOR *tensor, int channels, int width,
                           int height) {
  const int newallocsize = channels * width * height;
  if (tensor->allocsize < newallocsize) {
    free_tensor(tensor);
    tensor->buf[0] =
        (float *)aom_malloc(sizeof(*tensor->buf[0]) * newallocsize);
    if (!tensor->buf[0]) return false;
    tensor->allocsize = newallocsize;
  }
  tensor->width = width;
  tensor->height = height;
  tensor->stride = width;
  tensor->channels = channels;
  for (int c = 1; c < channels; ++c)
    tensor->buf[c] = &tensor->buf[0][c * width * height];
  return true;
}

static void copy_tensor(const TENSOR *src, int copy_channels, int dst_offset,
                        TENSOR *dst) {
  assert(src->width == dst->width);
  assert(src->height == dst->height);
  assert(copy_channels <= src->channels);
  if (src->stride == dst->width && dst->stride == dst->width) {
    for (int c = 0; c < copy_channels; ++c) {
      memcpy(dst->buf[dst_offset + c], src->buf[c],
             sizeof(*dst->buf[0]) * src->width * src->height);
    }
  } else {
    for (int c = 0; c < copy_channels; ++c) {
      for (int r = 0; r < dst->height; ++r) {
        memcpy(&dst->buf[dst_offset + c][r * dst->stride],
               &src->buf[c][r * src->stride],
               dst->width * sizeof(*dst->buf[c]));
      }
    }
  }
}

static void assign_tensor(TENSOR *tensor, float *buf[CNN_MAX_CHANNELS],
                          int channels, int width, int height, int stride) {
  tensor->allocsize = 0;
  tensor->channels = channels;
  tensor->width = width;
  tensor->height = height;
  tensor->stride = stride;
  if (buf) {
    for (int c = 0; c < channels; ++c) tensor->buf[c] = buf[c];
  } else {
    for (int c = 0; c < channels; ++c) tensor->buf[c] = NULL;
  }
}

static void swap_tensor(TENSOR *t1, TENSOR *t2) {
  TENSOR t = *t1;
  *t1 = *t2;
  *t2 = t;
}

// The concatenated tensor goes into dst with first the channels in
// original dst followed by the channels in the src
static bool concat_tensor(const TENSOR *src, TENSOR *dst) {
  assert(src->width == dst->width);
  assert(src->height == dst->height);

  const int dst_channels = dst->channels;
  const int channels = dst->channels + src->channels;
  const int newallocsize = channels * dst->width * dst->height;
  if (dst->allocsize < newallocsize) {
    TENSOR t;
    init_tensor(&t);
    // allocate new buffers and copy first the dst channels
    if (!realloc_tensor(&t, channels, dst->width, dst->height)) return false;
    copy_tensor(dst, dst->channels, 0, &t);
    // Swap the tensors and free the old buffers
    swap_tensor(dst, &t);
    free_tensor(&t);
  }
  for (int c = 1; c < channels; ++c)
    dst->buf[c] = &dst->buf[0][c * dst->width * dst->height];
  // Copy the channels in src after the first dst_channels channels.
  copy_tensor(src, src->channels, dst_channels, dst);
  return true;
}

#ifndef NDEBUG
static int check_tensor_equal_dims(TENSOR *t1, TENSOR *t2) {
  return (t1->width == t2->width && t1->height == t2->height);
}

static int check_tensor_equal_size(TENSOR *t1, TENSOR *t2) {
  return (t1->channels == t2->channels && t1->width == t2->width &&
          t1->height == t2->height);
}
#endif  // NDEBUG

void av1_find_cnn_layer_output_size(int in_width, int in_height,
                                    const CNN_LAYER_CONFIG *layer_config,
                                    int *out_width, int *out_height) {
  assert(layer_config->skip_width > 0);
  assert(layer_config->skip_height > 0);
  if (!layer_config->deconvolve) {
    switch (layer_config->pad) {
      case PADDING_SAME_ZERO:
      case PADDING_SAME_REPLICATE:
        *out_width = (in_width + layer_config->skip_width - 1) /
                     layer_config->skip_width;
        *out_height = (in_height + layer_config->skip_height - 1) /
                      layer_config->skip_height;
        break;
      case PADDING_VALID:
        *out_width =
            (in_width - layer_config->filter_width + layer_config->skip_width) /
            layer_config->skip_width;
        *out_height = (in_height - layer_config->filter_height +
                       layer_config->skip_height) /
                      layer_config->skip_height;
        break;
      default: assert(0 && "Unknown padding type");
    }
  } else {
    switch (layer_config->pad) {
      case PADDING_SAME_ZERO:
      case PADDING_SAME_REPLICATE:
        *out_width = in_width * layer_config->skip_width;
        *out_height = in_height * layer_config->skip_height;
        break;
      case PADDING_VALID:
        *out_width = (in_width - 1) * layer_config->skip_width +
                     layer_config->filter_width;
        *out_height = (in_height - 1) * layer_config->skip_height +
                      layer_config->filter_height;
        break;
      default: assert(0 && "Unknown padding type");
    }
  }
}

static void find_cnn_out_channels(const CNN_LAYER_CONFIG *layer_config,
                                  int channels_per_branch[]) {
  int branch = layer_config->branch;
  const CNN_BRANCH_CONFIG *branch_config = &layer_config->branch_config;
  for (int b = 0; b < CNN_MAX_BRANCHES; ++b) {
    if ((branch_config->input_to_branches & (1 << b)) && b != branch) {
      if (layer_config->branch_copy_type == BRANCH_INPUT) {
        channels_per_branch[b] = layer_config->in_channels;
      } else if (layer_config->branch_copy_type == BRANCH_OUTPUT) {
        channels_per_branch[b] = layer_config->out_channels;
      } else if (layer_config->branch_copy_type == BRANCH_COMBINED) {
        channels_per_branch[b] = layer_config->out_channels;
        for (int c = 0; c < CNN_MAX_BRANCHES; ++c) {
          if ((branch_config->branches_to_combine & (1 << c)) && c != branch) {
            assert(channels_per_branch[c] > 0);
            channels_per_branch[b] += channels_per_branch[c];
          }
        }
      }
    }
  }
  channels_per_branch[branch] = layer_config->out_channels;
  for (int c = 0; c < CNN_MAX_BRANCHES; ++c) {
    if ((branch_config->branches_to_combine & (1 << c)) && c != branch) {
      assert(channels_per_branch[c] > 0);
      channels_per_branch[branch] += channels_per_branch[c];
    }
  }
}

#if CONFIG_DEBUG
static inline int cnn_has_at_least_one_output(const CNN_CONFIG *cnn_config) {
  const int num_layers = cnn_config->num_layers;
  const CNN_LAYER_CONFIG *layer_configs = cnn_config->layer_config;

  for (int idx = 0; idx < num_layers; idx++) {
    if (layer_configs[idx].output_num != -1) {
      return 1;
    }
  }
  return 0;
}
#endif

void av1_find_cnn_output_size(int in_width, int in_height,
                              const CNN_CONFIG *cnn_config, int *out_width,
                              int *out_height, int *out_channels) {
  int channels_per_branch[CNN_MAX_BRANCHES] = { 0 };
  int i_width[CNN_MAX_BRANCHES] = { 0 };
  int i_height[CNN_MAX_BRANCHES] = { 0 };
  i_width[0] = in_width + cnn_config->ext_width * 2;
  i_height[0] = in_height + cnn_config->ext_height * 2;

#if CONFIG_DEBUG
  assert(cnn_has_at_least_one_output(cnn_config));
#endif

  for (int i = 0; i < cnn_config->num_layers; ++i) {
    const CNN_LAYER_CONFIG *layer_config = &cnn_config->layer_config[i];
    const CNN_BRANCH_CONFIG *branch_config = &layer_config->branch_config;
    const int branch = layer_config->branch;
    int o_width = 0, o_height = 0;

    if (layer_config->branch_copy_type == BRANCH_INPUT) {
      for (int b = 0; b < CNN_MAX_BRANCHES; ++b) {
        if ((branch_config->input_to_branches & (1 << b)) && b != branch) {
          assert(i_width[branch] > 0 && i_height[branch] > 0);
          i_width[b] = i_width[branch];
          i_height[b] = i_height[branch];
        }
      }
    }

    av1_find_cnn_layer_output_size(i_width[branch], i_height[branch],
                                   layer_config, &o_width, &o_height);
    i_width[branch] = o_width;
    i_height[branch] = o_height;

    if (layer_config->branch_copy_type == BRANCH_OUTPUT) {
      for (int b = 0; b < CNN_MAX_BRANCHES; ++b) {
        if ((branch_config->input_to_branches & (1 << b)) && b != branch) {
          i_width[b] = o_width;
          i_height[b] = o_height;
        }
      }
    }

    find_cnn_out_channels(layer_config, channels_per_branch);

    const int output_num = layer_config->output_num;
    if (output_num != -1) {  // Current layer is an output layer
      out_width[output_num] = o_width;
      out_height[output_num] = o_height;
      out_channels[output_num] = channels_per_branch[layer_config->branch];
    }
  }
}

static inline int get_start_shift_convolve(int width, int filt_width,
                                           int stride) {
  const int mod = (width % stride);
  const int filt_off = (filt_width - 1) / 2;
  const int dif = (mod ? mod - 1 : stride - 1);
  return AOMMIN((dif + (filt_width % 2)) / 2, filt_off);
}

void av1_cnn_add_c(float **output, int channels, int width, int height,
                   int stride, const float **add) {
  for (int c = 0; c < channels; ++c) {
    for (int i = 0; i < height; ++i)
      for (int j = 0; j < width; ++j)
        output[c][i * stride + j] += add[c][i * stride + j];
  }
}

void av1_cnn_activate_c(float **output, int channels, int width, int height,
                        int stride, ACTIVATION layer_activation) {
  if (layer_activation == RELU) {
    for (int c = 0; c < channels; ++c) {
      for (int i = 0; i < height; ++i)
        for (int j = 0; j < width; ++j)
          output[c][i * stride + j] = relu(output[c][i * stride + j]);
    }
  } else if (layer_activation == SOFTSIGN) {
    for (int c = 0; c < channels; ++c) {
      for (int i = 0; i < height; ++i)
        for (int j = 0; j < width; ++j)
          output[c][i * stride + j] = softsign(output[c][i * stride + j]);
    }
  } else if (layer_activation == SIGMOID) {
    assert(0 && "Sigmoid has not been supported in CNN.");  // TO DO
  } else if (layer_activation != NONE) {
    assert(0 && "Unknown activation type");
  }
}

static bool copy_active_tensor_to_branches(const TENSOR *layer_active_tensor,
                                           const CNN_LAYER_CONFIG *layer_config,
                                           int branch, TENSOR branch_output[]) {
  const CNN_BRANCH_CONFIG *branch_config = &layer_config->branch_config;
  for (int b = 0; b < CNN_MAX_BRANCHES; ++b) {
    if ((branch_config->input_to_branches & (1 << b)) && b != branch) {
      // Copy layer's active tensor to output tensor of branch b if set in
      // mask. The output becomes the input of the first layer of the branch
      // because the layer of the branch is not the first layer.
      int copy_channels = branch_config->channels_to_copy > 0
                              ? branch_config->channels_to_copy
                              : layer_active_tensor->channels;
      if (!realloc_tensor(&branch_output[b], copy_channels,
                          layer_active_tensor->width,
                          layer_active_tensor->height)) {
        return false;
      }
      copy_tensor(layer_active_tensor, copy_channels, 0, &branch_output[b]);
    }
  }
  return true;
}

// CNNConvolve specific to maxpool set as 1, either skip_width or skip_height
// greater than 1 and padding equal to PADDING_SAME_ZERO.
static void convolve_maxpool_padding_zero(
    const float **input, int in_width, int in_height, int in_stride,
    const CNN_LAYER_CONFIG *const layer_config, float **output, int out_stride,
    const int cstep, const int filter_width_half,
    const int filter_height_half) {
  for (int i = 0; i < layer_config->out_channels; ++i) {
    for (int h = 0, u = 0; h < in_height; h += layer_config->skip_height, ++u) {
      for (int w = 0, v = 0; w < in_width; w += layer_config->skip_width, ++v) {
        for (int hh = h; hh < AOMMIN(in_height, h + layer_config->skip_height);
             ++hh) {
          for (int ww = w; ww < AOMMIN(in_width, w + layer_config->skip_width);
               ++ww) {
            float sum = layer_config->bias[i];
            for (int k = 0; k < layer_config->in_channels; ++k) {
              int off = k * layer_config->out_channels + i;
              for (int l = 0; l < layer_config->filter_height; ++l) {
                const int ii = hh + l - filter_height_half;
                for (int m = 0; m < layer_config->filter_width;
                     ++m, off += cstep) {
                  const int jj = ww + m - filter_width_half;
                  if (ii < 0 || ii >= in_height || jj < 0 || jj >= in_width)
                    continue;
                  sum += layer_config->weights[off] *
                         input[k][ii * in_stride + jj];
                }
              }
            }
            const float a = sum;
            if (h == hh && w == ww)
              output[i][u * out_stride + v] = a;
            else
              output[i][u * out_stride + v] =
                  AOMMAX(output[i][u * out_stride + v], a);
          }
        }
      }
    }
  }
}

// CNNConvolve specific to maxpool set as 1, either skip_width or skip_height
// greater than 1 and padding equal to PADDING_SAME_REPLICATE.
static void convolve_maxpool_padding_replicate(
    const float **input, int in_width, int in_height, int in_stride,
    const CNN_LAYER_CONFIG *const layer_config, float **output, int out_stride,
    const int cstep, const int filter_width_half,
    const int filter_height_half) {
  for (int i = 0; i < layer_config->out_channels; ++i) {
    for (int h = 0, u = 0; h < in_height; h += layer_config->skip_height, ++u) {
      for (int w = 0, v = 0; w < in_width; w += layer_config->skip_width, ++v) {
        for (int hh = h; hh < AOMMIN(in_height, h + layer_config->skip_height);
             ++hh) {
          for (int ww = w; ww < AOMMIN(in_width, w + layer_config->skip_width);
               ++ww) {
            float sum = layer_config->bias[i];
            for (int k = 0; k < layer_config->in_channels; ++k) {
              int off = k * layer_config->out_channels + i;
              for (int l = 0; l < layer_config->filter_height; ++l) {
                const int ii =
                    CLAMPINDEX(hh + l - filter_height_half, in_height);
                for (int m = 0; m < layer_config->filter_width;
                     ++m, off += cstep) {
                  const int jj =
                      CLAMPINDEX(ww + m - filter_width_half, in_width);
                  assert(ii >= 0 && ii < in_height && jj >= 0 && jj < in_width);
                  sum += layer_config->weights[off] *
                         input[k][ii * in_stride + jj];
                }
              }
            }
            const float a = sum;
            if (h == hh && w == ww)
              output[i][u * out_stride + v] = a;
            else
              output[i][u * out_stride + v] =
                  AOMMAX(output[i][u * out_stride + v], a);
          }
        }
      }
    }
  }
}

// CNNConvolve specific to maxpool set as 1, either skip_width or skip_height
// greater than 1 and padding equal to PADDING_VALID.
static void convolve_maxpool_padding_valid(
    const float **input, int in_width, int in_height, int in_stride,
    const CNN_LAYER_CONFIG *const layer_config, float **output, int out_stride,
    const int cstep) {
  for (int i = 0; i < layer_config->out_channels; ++i) {
    for (int h = 0, u = 0; h < in_height - layer_config->filter_height + 1;
         h += layer_config->skip_height, ++u) {
      for (int w = 0, v = 0; w < in_width - layer_config->filter_width + 1;
           w += layer_config->skip_width, ++v) {
        for (int hh = h; hh < AOMMIN(in_height, h + layer_config->skip_height);
             ++hh) {
          for (int ww = w; ww < AOMMIN(in_width, w + layer_config->skip_width);
               ++ww) {
            float sum = layer_config->bias[i];
            for (int k = 0; k < layer_config->in_channels; ++k) {
              int off = k * layer_config->out_channels + i;
              for (int l = 0; l < layer_config->filter_height; ++l) {
                const int ii = hh + l;
                for (int m = 0; m < layer_config->filter_width;
                     ++m, off += cstep) {
                  const int jj = ww + m;
                  assert(ii >= 0 && ii < in_height && jj >= 0 && jj < in_width);
                  sum += layer_config->weights[off] *
                         input[k][ii * in_stride + jj];
                }
              }
            }
            const float a = sum;
            if (h == hh && w == ww)
              output[i][u * out_stride + v] = a;
            else
              output[i][u * out_stride + v] =
                  AOMMAX(output[i][u * out_stride + v], a);
          }
        }
      }
    }
  }
}

// CNNConvolve specific to maxpool set as 0 with filter_height and filter_width
// equal to 1.
static void convolve_element_wise(const float **input, int in_width,
                                  int in_height, int in_stride,
                                  const CNN_LAYER_CONFIG *const layer_config,
                                  float **output, int out_stride, int start_idx,
                                  int step) {
  const int start_h = get_start_shift_convolve(
      in_height, layer_config->filter_height, layer_config->skip_height);
  const int start_w =
      get_start_shift_convolve(in_width, layer_config->filter_width,
                               layer_config->skip_width) +
      start_idx * layer_config->skip_width;
  const int out_w_step = AOMMAX(step, 1);
  const int in_w_step = layer_config->skip_width * out_w_step;
  for (int i = 0; i < layer_config->out_channels; ++i) {
    for (int h = start_h, u = 0; h < in_height;
         h += layer_config->skip_height, ++u) {
      const int in_h = h * in_stride;
      const int out_h = u * out_stride + start_idx;
      for (int w = start_w, out_index = out_h; w < in_width;
           w += in_w_step, out_index += out_w_step) {
        float sum = layer_config->bias[i];
        for (int k = 0; k < layer_config->in_channels; ++k) {
          sum += layer_config->weights[k * layer_config->out_channels + i] *
                 input[k][in_h + w];
        }
        output[i][out_index] = sum;
      }
    }
  }
}

// CNNConvolve specific to maxpool set as 0 and padding equal to
// PADDING_SAME_ZERO.
static void convolve_no_maxpool_padding_zero(
    const float **input, int in_width, int in_height, int in_stride,
    const CNN_LAYER_CONFIG *const layer_config, float **output, int out_stride,
    int start_idx, const int cstep, const int filter_width_half,
    const int filter_height_half, const int ii_shift, const int jj_shift,
    const int channel_step) {
  const int start_h = get_start_shift_convolve(
      in_height, layer_config->filter_height, layer_config->skip_height);
  const int start_w = get_start_shift_convolve(
      in_width, layer_config->filter_width, layer_config->skip_width);
  const int end_ii_shift = filter_height_half + 1;
  const int end_jj_shift = filter_width_half + 1;
  // *_filter_margin stores the number of pixels along a dimension in the
  // intersection of the complement of the image in the extended image
  // and the filter.
  const int top_filter_margin = layer_config->filter_width * ii_shift;
  const int right_filter_margin = end_jj_shift - in_width;
  for (int i = start_idx; i < layer_config->out_channels; i += channel_step) {
    for (int h = start_h, u = 0; h < in_height;
         h += layer_config->skip_height, ++u) {
      const int out_h = u * out_stride;
      const int top_cstep =
          AOMMAX(0, top_filter_margin - h * layer_config->filter_width) *
              cstep +
          i;
      const int start_ii = AOMMAX(0, h - ii_shift);
      const int end_ii = AOMMIN(in_height, h + end_ii_shift);
      for (int w = start_w, out_index = out_h; w < in_width;
           w += layer_config->skip_width, ++out_index) {
        const int left_cstep = AOMMAX(0, jj_shift - w) * cstep;
        const int right_cstep = AOMMAX(0, right_filter_margin + w) * cstep;
        const int start_jj = AOMMAX(0, w - jj_shift);
        const int end_jj = AOMMIN(in_width, w + end_jj_shift);
        float sum = layer_config->bias[i];
        for (int k = 0; k < layer_config->in_channels; ++k) {
          int off = k * layer_config->out_channels + top_cstep;
          for (int ii = start_ii; ii < end_ii; ++ii) {
            off += left_cstep;
            for (int jj = start_jj; jj < end_jj; ++jj, off += cstep) {
              sum += layer_config->weights[off] * input[k][ii * in_stride + jj];
            }
            off += right_cstep;
          }
        }
        output[i][out_index] = sum;
      }
    }
  }
}

// CNNConvolve specific to maxpool set as 0 and padding equal to
// PADDING_SAME_REPLICATE.
static void convolve_no_maxpool_padding_replicate(
    const float **input, int in_width, int in_height, int in_stride,
    const CNN_LAYER_CONFIG *const layer_config, float **output, int out_stride,
    int start_idx, const int cstep, const int ii_shift, const int jj_shift,
    const int channel_step) {
  // h and w are shifted to an offset coordinate system to reduce in-loop
  // computation.
  const int start_h =
      get_start_shift_convolve(in_height, layer_config->filter_height,
                               layer_config->skip_height) -
      ii_shift;
  const int start_w =
      get_start_shift_convolve(in_width, layer_config->filter_width,
                               layer_config->skip_width) -
      jj_shift;
  const int end_h = in_height - ii_shift;
  const int end_w = in_width - jj_shift;
  for (int i = start_idx; i < layer_config->out_channels; i += channel_step) {
    for (int h = start_h, u = 0; h < end_h;
         h += layer_config->skip_height, ++u) {
      const int out_h = u * out_stride;
      const int upper_ii_index = layer_config->filter_height + h;
      for (int w = start_w, out_index = out_h; w < end_w;
           w += layer_config->skip_width, ++out_index) {
        const int upper_jj_index = layer_config->filter_width + w;
        float sum = layer_config->bias[i];
        for (int k = 0; k < layer_config->in_channels; ++k) {
          int off = k * layer_config->out_channels + i;
          for (int ii = h; ii < upper_ii_index; ++ii) {
            const int clamped_ii = CLAMPINDEX(ii, in_height);
            for (int jj = w; jj < upper_jj_index; ++jj) {
              const int clamped_jj = CLAMPINDEX(jj, in_width);
              assert(clamped_ii >= 0 && clamped_ii < in_height &&
                     clamped_jj >= 0 && clamped_jj < in_width);
              sum += layer_config->weights[off] *
                     input[k][clamped_ii * in_stride + clamped_jj];
              off += cstep;
            }
          }
        }
        output[i][out_index] = sum;
      }
    }
  }
}

// CNNConvolve specific to maxpool set as 0 and padding equal to
// PADDING_VALID.
void av1_cnn_convolve_no_maxpool_padding_valid_c(
    const float **input, int in_width, int in_height, int in_stride,
    const CNN_LAYER_CONFIG *layer_config, float **output, int out_stride,
    int start_idx, int cstep, int channel_step) {
  assert((layer_config->skip_height == 1 && layer_config->skip_width == 1) ||
         !layer_config->maxpool);
  assert(layer_config->filter_height > 1 || layer_config->filter_width > 1);
  assert(layer_config->pad == PADDING_VALID);
  for (int i = start_idx; i < layer_config->out_channels; i += channel_step) {
    for (int h = 0, u = 0; h < in_height - layer_config->filter_height + 1;
         h += layer_config->skip_height, ++u) {
      const int out_h = u * out_stride;
      const int upper_ii_index = layer_config->filter_height + h;
      for (int w = 0, out_index = out_h;
           w < in_width - layer_config->filter_width + 1;
           w += layer_config->skip_width, ++out_index) {
        const int upper_jj_index = layer_config->filter_width + w;
        float sum = layer_config->bias[i];
        for (int k = 0; k < layer_config->in_channels; ++k) {
          int off = k * layer_config->out_channels + i;
          for (int ii = h; ii < upper_ii_index; ++ii) {
            for (int jj = w; jj < upper_jj_index; ++jj) {
              assert(ii >= 0 && ii < in_height && jj >= 0 && jj < in_width);
              sum += layer_config->weights[off] * input[k][ii * in_stride + jj];
              off += cstep;
            }
          }
        }
        output[i][out_index] = sum;
      }
    }
  }
}

static void av1_cnn_convolve(const float **input, int in_width, int in_height,
                             int in_stride,
                             const CNN_LAYER_CONFIG *layer_config,
                             float **output, int out_stride, int start_idx,
                             int step) {
  assert(!layer_config->deconvolve);
  const int cstep = layer_config->in_channels * layer_config->out_channels;
  const int filter_height_half = layer_config->filter_height >> 1;
  const int filter_width_half = layer_config->filter_width >> 1;
  const int channel_step = AOMMAX(step, 1);

  if (layer_config->maxpool &&
      (layer_config->skip_height > 1 || layer_config->skip_width > 1)) {
    switch (layer_config->pad) {
      case PADDING_SAME_ZERO:
        convolve_maxpool_padding_zero(input, in_width, in_height, in_stride,
                                      layer_config, output, out_stride, cstep,
                                      filter_width_half, filter_height_half);
        break;
      case PADDING_SAME_REPLICATE:
        convolve_maxpool_padding_replicate(
            input, in_width, in_height, in_stride, layer_config, output,
            out_stride, cstep, filter_width_half, filter_height_half);
        break;
      case PADDING_VALID:
        convolve_maxpool_padding_valid(input, in_width, in_height, in_stride,
                                       layer_config, output, out_stride, cstep);
        break;
      default: assert(0 && "Unknown padding type");
    }
  } else {
    // Results in element-wise matrix multiplication.
    if (layer_config->filter_height == 1 && layer_config->filter_width == 1) {
      convolve_element_wise(input, in_width, in_height, in_stride, layer_config,
                            output, out_stride, start_idx, step);
      return;
    }
    const int ii_shift =
        filter_height_half - (layer_config->filter_height - 1) % 2;
    const int jj_shift =
        filter_width_half - (layer_config->filter_width - 1) % 2;
    switch (layer_config->pad) {
      case PADDING_SAME_ZERO:
        convolve_no_maxpool_padding_zero(
            input, in_width, in_height, in_stride, layer_config, output,
            out_stride, start_idx, cstep, filter_width_half, filter_height_half,
            ii_shift, jj_shift, channel_step);
        break;
      case PADDING_SAME_REPLICATE:
        convolve_no_maxpool_padding_replicate(
            input, in_width, in_height, in_stride, layer_config, output,
            out_stride, start_idx, cstep, ii_shift, jj_shift, channel_step);
        break;
      case PADDING_VALID:
        av1_cnn_convolve_no_maxpool_padding_valid(
            input, in_width, in_height, in_stride, layer_config, output,
            out_stride, start_idx, cstep, channel_step);
        break;
      default: assert(0 && "Unknown padding type");
    }
  }
}

static int convolve_layer(void *arg1, void *arg2) {
  const CONVOLVE_OPS *convolve_ops = arg1;
  (void)arg2;
  av1_cnn_convolve(
      convolve_ops->input, convolve_ops->in_width, convolve_ops->in_height,
      convolve_ops->in_stride, convolve_ops->layer_config, convolve_ops->output,
      convolve_ops->out_stride, convolve_ops->start_idx, convolve_ops->th_step);
  return 1;
}

static void convolve_layer_mt(const float **input, int in_width, int in_height,
                              int in_stride,
                              const CNN_LAYER_CONFIG *layer_config,
                              const CNN_THREAD_DATA *thread_data,
                              float **output, int out_stride) {
  const AVxWorkerInterface *const winterface = aom_get_worker_interface();
  const int num_workers = thread_data->num_workers;
  assert(thread_data->workers);

  CONVOLVE_OPS convolve_ops[CNN_MAX_THREADS];
  for (int th = 0; th < AOMMIN(num_workers, CNN_MAX_THREADS); ++th) {
    AVxWorker *const worker = &thread_data->workers[th];
    winterface->reset(worker);

    CONVOLVE_OPS convolve_op = { input,      in_width,     in_height,
                                 in_stride,  layer_config, output,
                                 out_stride, th,           num_workers };
    convolve_ops[th] = convolve_op;
    worker->hook = convolve_layer;
    worker->data1 = &(convolve_ops[th]);
    worker->data2 = NULL;

    // Start convolving.
    if (th == num_workers - 1) {
      winterface->execute(worker);
    } else {
      winterface->launch(worker);
    }
  }

  // Wait until all workers have finished.
  for (int th = 0; th < AOMMIN(num_workers, CNN_MAX_THREADS); ++th) {
    winterface->sync(&thread_data->workers[th]);
  }
}

static inline int get_start_shift_deconvolve(int filt_width, int stride) {
  const int dif = AOMMAX(filt_width - stride, 0);
  return dif / 2;
}

void av1_cnn_batchnorm_c(float **image, int channels, int width, int height,
                         int stride, const float *gamma, const float *beta,
                         const float *mean, const float *std) {
  assert(gamma && beta && beta && std && "batchnorm has null parameter!");
  for (int ch = 0; ch < channels; ch++) {
    const float ch_gamma = gamma[ch];
    const float ch_beta = beta[ch];
    const float ch_mean = mean[ch];
    const float ch_std = std[ch];
    float *image_row = image[ch];

    for (int row = 0; row < height; row++) {
      for (int col = 0; col < width; col++) {
        image_row[col] =
            ch_gamma * (image_row[col] - ch_mean) / ch_std + ch_beta;
      }
      image_row += stride;
    }
  }
}

void av1_cnn_deconvolve_c(const float **input, int in_width, int in_height,
                          int in_stride, const CNN_LAYER_CONFIG *layer_config,
                          float **output, int out_stride) {
  assert(layer_config->deconvolve);

  const int cstep = layer_config->in_channels * layer_config->out_channels;

  int out_width = 0;
  int out_height = 0;
  av1_find_cnn_layer_output_size(in_width, in_height, layer_config, &out_width,
                                 &out_height);
  switch (layer_config->pad) {
    case PADDING_SAME_ZERO:
      for (int i = 0; i < layer_config->out_channels; ++i) {
        for (int u = 0; u < out_height; ++u) {
          for (int v = 0; v < out_width; ++v) {
            float sum = layer_config->bias[i];
            for (int k = 0; k < layer_config->in_channels; ++k) {
              int off = k * layer_config->out_channels + i;
              for (int l = 0; l < layer_config->filter_height; ++l) {
                const int h =
                    u - l +
                    get_start_shift_deconvolve(layer_config->filter_height,
                                               layer_config->skip_height);
                for (int m = 0; m < layer_config->filter_width;
                     ++m, off += cstep) {
                  const int w =
                      v - m +
                      get_start_shift_deconvolve(layer_config->filter_width,
                                                 layer_config->skip_width);
                  if ((h % layer_config->skip_height) != 0 ||
                      (w % layer_config->skip_width) != 0)
                    continue;
                  const int ii = h / layer_config->skip_height;
                  const int jj = w / layer_config->skip_width;
                  if (ii < 0 || ii >= in_height || jj < 0 || jj >= in_width)
                    continue;
                  sum += layer_config->weights[off] *
                         input[k][ii * in_stride + jj];
                }
              }
            }
            output[i][u * out_stride + v] = sum;
          }
        }
      }
      break;
    case PADDING_SAME_REPLICATE:
      for (int i = 0; i < layer_config->out_channels; ++i) {
        for (int u = 0; u < out_height; ++u) {
          for (int v = 0; v < out_width; ++v) {
            float sum = layer_config->bias[i];
            for (int k = 0; k < layer_config->in_channels; ++k) {
              int off = k * layer_config->out_channels + i;
              for (int l = 0; l < layer_config->filter_height; ++l) {
                const int h =
                    u - l +
                    get_start_shift_deconvolve(layer_config->filter_height,
                                               layer_config->skip_height);
                for (int m = 0; m < layer_config->filter_width;
                     ++m, off += cstep) {
                  const int w =
                      v - m +
                      get_start_shift_deconvolve(layer_config->filter_width,
                                                 layer_config->skip_width);
                  if ((h % layer_config->skip_height) != 0 ||
                      (w % layer_config->skip_width) != 0)
                    continue;
                  const int ii =
                      CLAMPINDEX(h / layer_config->skip_height, in_height);
                  const int jj =
                      CLAMPINDEX(w / layer_config->skip_width, in_width);
                  assert(ii >= 0 && ii < in_height && jj >= 0 && jj < in_width);
                  sum += layer_config->weights[off] *
                         input[k][ii * in_stride + jj];
                }
              }
            }
            output[i][u * out_stride + v] = sum;
          }
        }
      }
      break;
    case PADDING_VALID:
      for (int i = 0; i < layer_config->out_channels; ++i) {
        for (int u = 0; u < out_height; ++u) {
          for (int v = 0; v < out_width; ++v) {
            float sum = layer_config->bias[i];
            for (int k = 0; k < layer_config->in_channels; ++k) {
              int off = k * layer_config->out_channels + i;
              for (int l = 0; l < layer_config->filter_height; ++l) {
                const int h = u - l;
                for (int m = 0; m < layer_config->filter_width;
                     ++m, off += cstep) {
                  const int w = v - m;
                  if ((h % layer_config->skip_height) != 0 ||
                      (w % layer_config->skip_width) != 0)
                    continue;
                  const int ii = h / layer_config->skip_height;
                  const int jj = w / layer_config->skip_width;
                  if (ii < 0 || ii >= in_height || jj < 0 || jj >= in_width)
                    continue;
                  sum += layer_config->weights[off] *
                         input[k][ii * in_stride + jj];
                }
              }
            }
            output[i][u * out_stride + v] = sum;
          }
        }
      }
      break;
    default: assert(0 && "Unknown padding type");
  }
}

bool av1_cnn_predict_c(const float **input, int in_width, int in_height,
                       int in_stride, const CNN_CONFIG *cnn_config,
                       const CNN_THREAD_DATA *thread_data,
                       CNN_MULTI_OUT *output_struct) {
  bool success = false;
  TENSOR tensor1[CNN_MAX_BRANCHES] = { { 0 } };
  TENSOR tensor2[CNN_MAX_BRANCHES] = { { 0 } };

  float **output[CNN_MAX_BRANCHES];
  const int *out_chs = output_struct->output_channels;
  output[0] = output_struct->output_buffer;
  for (int out_idx = 1; out_idx < output_struct->num_outputs; out_idx++) {
    output[out_idx] = output[out_idx - 1] + out_chs[out_idx - 1];
  }

  int i_width = in_width;
  int i_height = in_height;
  int o_width = 0, o_height = 0;
  for (int b = 0; b < CNN_MAX_BRANCHES; ++b) {
    init_tensor(&tensor1[b]);
    init_tensor(&tensor2[b]);
  }

  const int *out_stride = output_struct->output_strides;
  for (int layer = 0; layer < cnn_config->num_layers; ++layer) {
    const CNN_LAYER_CONFIG *layer_config = &cnn_config->layer_config[layer];
    const int branch = layer_config->branch;
    const CNN_BRANCH_CONFIG *branch_config = &layer_config->branch_config;

    // Allocate input tensor
    if (layer == 0) {       // First layer
      assert(branch == 0);  // First layer must be primary branch
      assign_tensor(&tensor1[branch], (float **)input,
                    layer_config->in_channels, in_width, in_height, in_stride);
    } else {  // Non-first layer
      // Swap tensor1 and tensor2
      swap_tensor(&tensor1[branch], &tensor2[branch]);

      i_width = tensor1[branch].width;
      i_height = tensor1[branch].height;
    }

    // Allocate output tensor
    av1_find_cnn_layer_output_size(i_width, i_height, layer_config, &o_width,
                                   &o_height);
    const int output_num = layer_config->output_num;
    if (output_num == -1) {  // Non-output layer
      if (!realloc_tensor(&tensor2[branch], layer_config->out_channels, o_width,
                          o_height)) {
        goto Error;
      }
    } else {  // Output layer
      free_tensor(&tensor2[branch]);
      assign_tensor(&tensor2[branch], output[output_num],
                    layer_config->out_channels, o_width, o_height,
                    out_stride[output_num]);
    }

    // If we are combining branches make sure that the branch to combine
    // is different from the current branch.
    assert(IMPLIES(layer_config->branch_combine_type != BRANCH_NOC,
                   !(branch_config->branches_to_combine & (1 << branch))));

    if (layer_config->branch_copy_type == BRANCH_INPUT) {
      if (!copy_active_tensor_to_branches(&tensor1[branch], layer_config,
                                          branch, tensor2)) {
        goto Error;
      }
    }
    // Check consistency of input and output channels
    assert(tensor1[branch].channels == layer_config->in_channels);
    assert(tensor2[branch].channels == layer_config->out_channels);

    // Convolve/Deconvolve
    if (!cnn_config->layer_config[layer].deconvolve) {
      if (thread_data->num_workers > 1) {
        convolve_layer_mt((const float **)tensor1[branch].buf,
                          tensor1[branch].width, tensor1[branch].height,
                          tensor1[branch].stride, layer_config, thread_data,
                          tensor2[branch].buf, tensor2[branch].stride);
      } else {
        av1_cnn_convolve((const float **)tensor1[branch].buf,
                         tensor1[branch].width, tensor1[branch].height,
                         tensor1[branch].stride, layer_config,
                         tensor2[branch].buf, tensor2[branch].stride, 0, 1);
      }
    } else {
      av1_cnn_deconvolve((const float **)tensor1[branch].buf,
                         tensor1[branch].width, tensor1[branch].height,
                         tensor1[branch].stride, layer_config,
                         tensor2[branch].buf, tensor2[branch].stride);
    }

    if (layer_config->branch_copy_type == BRANCH_OUTPUT) {
      if (!copy_active_tensor_to_branches(&tensor2[branch], layer_config,
                                          branch, tensor2)) {
        goto Error;
      }
    }

    // Add tensors from other branches if needed
    if (layer_config->branch_combine_type == BRANCH_ADD) {
      for (int b = 0; b < CNN_MAX_BRANCHES; ++b) {
        if ((branch_config->branches_to_combine & (1 << b)) && b != branch) {
          assert(check_tensor_equal_size(&tensor2[b], &tensor2[branch]));
          av1_cnn_add(tensor2[branch].buf, tensor2[branch].channels,
                      tensor2[branch].width, tensor2[branch].height,
                      tensor2[branch].stride, (const float **)tensor2[b].buf);
        }
      }
    }

    // Non-linearity
    av1_cnn_activate(tensor2[branch].buf, tensor2[branch].channels,
                     tensor2[branch].width, tensor2[branch].height,
                     tensor2[branch].stride, layer_config->activation);

    if (layer_config->bn_params.bn_gamma) {
      av1_cnn_batchnorm(
          tensor2[branch].buf, tensor2[branch].channels, tensor2[branch].width,
          tensor2[branch].height, tensor2[branch].stride,
          layer_config->bn_params.bn_gamma, layer_config->bn_params.bn_beta,
          layer_config->bn_params.bn_mean, layer_config->bn_params.bn_std);
    }

    // Concatenate tensors
    if (layer_config->branch_combine_type == BRANCH_CAT) {
      if (output_num == -1) {  // Non-output layer
        for (int b = 0; b < CNN_MAX_BRANCHES; ++b) {
          if ((branch_config->branches_to_combine & (1 << b)) && b != branch) {
            assert(check_tensor_equal_dims(&tensor2[b], &tensor2[branch]));
            assert(tensor2[b].channels > 0);
            if (!concat_tensor(&tensor2[b], &tensor2[branch])) goto Error;
          }
        }
      } else {  // Output layer
        const int existing_channels = tensor2[branch].channels;
        int num_chs = existing_channels;
        for (int b = 0; b < CNN_MAX_BRANCHES; ++b) {
          if ((branch_config->branches_to_combine & (1 << b)) && b != branch) {
            assert(check_tensor_equal_dims(&tensor2[b], &tensor2[branch]));
            // Needed only to assign the new channel buffers
            num_chs += tensor2[b].channels;
          }
        }
        assign_tensor(&tensor2[branch], output[output_num], num_chs, o_width,
                      o_height, out_stride[output_num]);

        num_chs = existing_channels;
        for (int b = 0; b < CNN_MAX_BRANCHES; ++b) {
          if ((branch_config->branches_to_combine & (1 << b)) && b != branch) {
            assert(check_tensor_equal_dims(&tensor2[b], &tensor2[branch]));
            // Needed only to assign the new channel buffers
            copy_tensor(&tensor2[b], tensor2[b].channels, num_chs,
                        &tensor2[branch]);
            num_chs += tensor2[b].channels;
          }
        }
      }
    }

    if (layer_config->branch_copy_type == BRANCH_COMBINED) {
      if (!copy_active_tensor_to_branches(&tensor2[branch], layer_config,
                                          branch, tensor2)) {
        goto Error;
      }
    }
  }

  success = true;
Error:
  for (int b = 0; b < CNN_MAX_BRANCHES; ++b) {
    free_tensor(&tensor1[b]);
    free_tensor(&tensor2[b]);
  }
  return success;
}

// Assume output already has proper allocation
// Assume input image buffers all have same resolution and strides
bool av1_cnn_predict_img_multi_out(uint8_t **dgd, int width, int height,
                                   int stride, const CNN_CONFIG *cnn_config,
                                   const CNN_THREAD_DATA *thread_data,
                                   CNN_MULTI_OUT *output) {
  const float max_val = 255.0;

  const int in_width = width + 2 * cnn_config->ext_width;
  const int in_height = height + 2 * cnn_config->ext_height;
  const int in_channels = cnn_config->layer_config[0].in_channels;
  float *inputs[CNN_MAX_CHANNELS];
  float *input_ =
      (float *)aom_malloc(in_width * in_height * in_channels * sizeof(*input_));
  if (!input_) return false;
  const int in_stride = in_width;

  for (int c = 0; c < in_channels; ++c) {
    inputs[c] = input_ + c * in_stride * in_height;
    float *input =
        inputs[c] + cnn_config->ext_height * in_stride + cnn_config->ext_width;

    if (cnn_config->strict_bounds) {
      for (int i = 0; i < height; ++i)
        for (int j = 0; j < width; ++j)
          input[i * in_stride + j] = (float)dgd[c][i * stride + j] / max_val;
      // extend left and right
      for (int i = 0; i < height; ++i) {
        for (int j = -cnn_config->ext_width; j < 0; ++j)
          input[i * in_stride + j] = input[i * in_stride];
        for (int j = width; j < width + cnn_config->ext_width; ++j)
          input[i * in_stride + j] = input[i * in_stride + width - 1];
      }
      // extend top and bottom
      for (int i = -cnn_config->ext_height; i < 0; ++i)
        memcpy(&input[i * in_stride - cnn_config->ext_width],
               &input[-cnn_config->ext_width], in_width * sizeof(*input));
      for (int i = height; i < height + cnn_config->ext_height; ++i)
        memcpy(&input[i * in_stride - cnn_config->ext_width],
               &input[(height - 1) * in_stride - cnn_config->ext_width],
               in_width * sizeof(*input));
    } else {
      for (int i = -cnn_config->ext_height; i < height + cnn_config->ext_height;
           ++i)
        for (int j = -cnn_config->ext_width; j < width + cnn_config->ext_width;
             ++j)
          input[i * in_stride + j] = (float)dgd[c][i * stride + j] / max_val;
    }
  }
  bool success = av1_cnn_predict((const float **)inputs, in_width, in_height,
                                 in_stride, cnn_config, thread_data, output);

  aom_free(input_);
  return success;
}

// Assume output already has proper allocation
// Assume input image buffers all have same resolution and strides
bool av1_cnn_predict_img_multi_out_highbd(uint16_t **dgd, int width, int height,
                                          int stride,
                                          const CNN_CONFIG *cnn_config,
                                          const CNN_THREAD_DATA *thread_data,
                                          int bit_depth,
                                          CNN_MULTI_OUT *output) {
  const float max_val = (float)((1 << bit_depth) - 1);

  const int in_width = width + 2 * cnn_config->ext_width;
  const int in_height = height + 2 * cnn_config->ext_height;
  const int in_channels = cnn_config->layer_config[0].in_channels;
  float *inputs[CNN_MAX_CHANNELS];
  float *input_ =
      (float *)aom_malloc(in_width * in_height * in_channels * sizeof(*input_));
  if (!input_) return false;
  const int in_stride = in_width;

  for (int c = 0; c < in_channels; ++c) {
    inputs[c] = input_ + c * in_stride * in_height;
    float *input =
        inputs[c] + cnn_config->ext_height * in_stride + cnn_config->ext_width;

    if (cnn_config->strict_bounds) {
      for (int i = 0; i < height; ++i)
        for (int j = 0; j < width; ++j)
          input[i * in_stride + j] = (float)dgd[c][i * stride + j] / max_val;
      // extend left and right
      for (int i = 0; i < height; ++i) {
        for (int j = -cnn_config->ext_width; j < 0; ++j)
          input[i * in_stride + j] = input[i * in_stride];
        for (int j = width; j < width + cnn_config->ext_width; ++j)
          input[i * in_stride + j] = input[i * in_stride + width - 1];
      }
      // extend top and bottom
      for (int i = -cnn_config->ext_height; i < 0; ++i)
        memcpy(&input[i * in_stride - cnn_config->ext_width],
               &input[-cnn_config->ext_width], in_width * sizeof(*input));
      for (int i = height; i < height + cnn_config->ext_height; ++i)
        memcpy(&input[i * in_stride - cnn_config->ext_width],
               &input[(height - 1) * in_stride - cnn_config->ext_width],
               in_width * sizeof(*input));
    } else {
      for (int i = -cnn_config->ext_height; i < height + cnn_config->ext_height;
           ++i)
        for (int j = -cnn_config->ext_width; j < width + cnn_config->ext_width;
             ++j)
          input[i * in_stride + j] = (float)dgd[c][i * stride + j] / max_val;
    }
  }

  bool success = av1_cnn_predict((const float **)inputs, in_width, in_height,
                                 in_stride, cnn_config, thread_data, output);

  aom_free(input_);
  return success;
}
