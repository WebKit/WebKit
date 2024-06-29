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

#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/psnr.h"
#include "aom_scale/yv12config.h"

double aom_sse_to_psnr(double samples, double peak, double sse) {
  if (sse > 0.0) {
    const double psnr = 10.0 * log10(samples * peak * peak / sse);
    return psnr > MAX_PSNR ? MAX_PSNR : psnr;
  } else {
    return MAX_PSNR;
  }
}

static int64_t encoder_sse(const uint8_t *a, int a_stride, const uint8_t *b,
                           int b_stride, int w, int h) {
  int i, j;
  int64_t sse = 0;

  for (i = 0; i < h; i++) {
    for (j = 0; j < w; j++) {
      const int diff = a[j] - b[j];
      sse += diff * diff;
    }

    a += a_stride;
    b += b_stride;
  }
  return sse;
}

#if CONFIG_AV1_HIGHBITDEPTH
static int64_t encoder_highbd_sse(const uint8_t *a8, int a_stride,
                                  const uint8_t *b8, int b_stride, int w,
                                  int h) {
  const uint16_t *a = CONVERT_TO_SHORTPTR(a8);
  const uint16_t *b = CONVERT_TO_SHORTPTR(b8);
  int64_t sse = 0;
  for (int i = 0; i < h; ++i) {
    for (int j = 0; j < w; ++j) {
      const int diff = a[j] - b[j];
      sse += diff * diff;
    }
    a += a_stride;
    b += b_stride;
  }
  return sse;
}

#endif  // CONFIG_AV1_HIGHBITDEPTH

static int64_t get_sse(const uint8_t *a, int a_stride, const uint8_t *b,
                       int b_stride, int width, int height) {
  const int dw = width % 16;
  const int dh = height % 16;
  int64_t total_sse = 0;
  int x, y;

  if (dw > 0) {
    total_sse += encoder_sse(&a[width - dw], a_stride, &b[width - dw], b_stride,
                             dw, height);
  }

  if (dh > 0) {
    total_sse +=
        encoder_sse(&a[(height - dh) * a_stride], a_stride,
                    &b[(height - dh) * b_stride], b_stride, width - dw, dh);
  }

  for (y = 0; y < height / 16; ++y) {
    const uint8_t *pa = a;
    const uint8_t *pb = b;
    for (x = 0; x < width / 16; ++x) {
      total_sse += aom_sse(pa, a_stride, pb, b_stride, 16, 16);

      pa += 16;
      pb += 16;
    }

    a += 16 * a_stride;
    b += 16 * b_stride;
  }

  return total_sse;
}

#if CONFIG_AV1_HIGHBITDEPTH
static int64_t highbd_get_sse_shift(const uint8_t *a8, int a_stride,
                                    const uint8_t *b8, int b_stride, int width,
                                    int height, unsigned int input_shift) {
  const uint16_t *a = CONVERT_TO_SHORTPTR(a8);
  const uint16_t *b = CONVERT_TO_SHORTPTR(b8);
  int64_t total_sse = 0;
  int x, y;
  for (y = 0; y < height; ++y) {
    for (x = 0; x < width; ++x) {
      int64_t diff;
      diff = (a[x] >> input_shift) - (b[x] >> input_shift);
      total_sse += diff * diff;
    }
    a += a_stride;
    b += b_stride;
  }
  return total_sse;
}

static int64_t highbd_get_sse(const uint8_t *a, int a_stride, const uint8_t *b,
                              int b_stride, int width, int height) {
  int64_t total_sse = 0;
  int x, y;
  const int dw = width % 16;
  const int dh = height % 16;

  if (dw > 0) {
    total_sse += encoder_highbd_sse(&a[width - dw], a_stride, &b[width - dw],
                                    b_stride, dw, height);
  }
  if (dh > 0) {
    total_sse += encoder_highbd_sse(&a[(height - dh) * a_stride], a_stride,
                                    &b[(height - dh) * b_stride], b_stride,
                                    width - dw, dh);
  }

  for (y = 0; y < height / 16; ++y) {
    const uint8_t *pa = a;
    const uint8_t *pb = b;
    for (x = 0; x < width / 16; ++x) {
      total_sse += aom_highbd_sse(pa, a_stride, pb, b_stride, 16, 16);
      pa += 16;
      pb += 16;
    }
    a += 16 * a_stride;
    b += 16 * b_stride;
  }
  return total_sse;
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

uint64_t aom_get_y_var(const YV12_BUFFER_CONFIG *a, int hstart, int width,
                       int vstart, int height) {
  return aom_var_2d_u8(a->y_buffer + vstart * a->y_stride + hstart, a->y_stride,
                       width, height) /
         (width * height);
}

uint64_t aom_get_u_var(const YV12_BUFFER_CONFIG *a, int hstart, int width,
                       int vstart, int height) {
  return aom_var_2d_u8(a->u_buffer + vstart * a->uv_stride + hstart,
                       a->uv_stride, width, height) /
         (width * height);
}

uint64_t aom_get_v_var(const YV12_BUFFER_CONFIG *a, int hstart, int width,
                       int vstart, int height) {
  return aom_var_2d_u8(a->v_buffer + vstart * a->uv_stride + hstart,
                       a->uv_stride, width, height) /
         (width * height);
}

int64_t aom_get_y_sse_part(const YV12_BUFFER_CONFIG *a,
                           const YV12_BUFFER_CONFIG *b, int hstart, int width,
                           int vstart, int height) {
  return get_sse(a->y_buffer + vstart * a->y_stride + hstart, a->y_stride,
                 b->y_buffer + vstart * b->y_stride + hstart, b->y_stride,
                 width, height);
}

int64_t aom_get_y_sse(const YV12_BUFFER_CONFIG *a,
                      const YV12_BUFFER_CONFIG *b) {
  assert(a->y_crop_width == b->y_crop_width);
  assert(a->y_crop_height == b->y_crop_height);

  return get_sse(a->y_buffer, a->y_stride, b->y_buffer, b->y_stride,
                 a->y_crop_width, a->y_crop_height);
}

int64_t aom_get_u_sse_part(const YV12_BUFFER_CONFIG *a,
                           const YV12_BUFFER_CONFIG *b, int hstart, int width,
                           int vstart, int height) {
  return get_sse(a->u_buffer + vstart * a->uv_stride + hstart, a->uv_stride,
                 b->u_buffer + vstart * b->uv_stride + hstart, b->uv_stride,
                 width, height);
}

int64_t aom_get_u_sse(const YV12_BUFFER_CONFIG *a,
                      const YV12_BUFFER_CONFIG *b) {
  assert(a->uv_crop_width == b->uv_crop_width);
  assert(a->uv_crop_height == b->uv_crop_height);

  return get_sse(a->u_buffer, a->uv_stride, b->u_buffer, b->uv_stride,
                 a->uv_crop_width, a->uv_crop_height);
}

int64_t aom_get_v_sse_part(const YV12_BUFFER_CONFIG *a,
                           const YV12_BUFFER_CONFIG *b, int hstart, int width,
                           int vstart, int height) {
  return get_sse(a->v_buffer + vstart * a->uv_stride + hstart, a->uv_stride,
                 b->v_buffer + vstart * b->uv_stride + hstart, b->uv_stride,
                 width, height);
}

int64_t aom_get_v_sse(const YV12_BUFFER_CONFIG *a,
                      const YV12_BUFFER_CONFIG *b) {
  assert(a->uv_crop_width == b->uv_crop_width);
  assert(a->uv_crop_height == b->uv_crop_height);

  return get_sse(a->v_buffer, a->uv_stride, b->v_buffer, b->uv_stride,
                 a->uv_crop_width, a->uv_crop_height);
}

#if CONFIG_AV1_HIGHBITDEPTH
uint64_t aom_highbd_get_y_var(const YV12_BUFFER_CONFIG *a, int hstart,
                              int width, int vstart, int height) {
  return aom_var_2d_u16(a->y_buffer + vstart * a->y_stride + hstart,
                        a->y_stride, width, height) /
         (width * height);
}

uint64_t aom_highbd_get_u_var(const YV12_BUFFER_CONFIG *a, int hstart,
                              int width, int vstart, int height) {
  return aom_var_2d_u16(a->u_buffer + vstart * a->uv_stride + hstart,
                        a->uv_stride, width, height) /
         (width * height);
}

uint64_t aom_highbd_get_v_var(const YV12_BUFFER_CONFIG *a, int hstart,
                              int width, int vstart, int height) {
  return aom_var_2d_u16(a->v_buffer + vstart * a->uv_stride + hstart,
                        a->uv_stride, width, height) /
         (width * height);
}

int64_t aom_highbd_get_y_sse_part(const YV12_BUFFER_CONFIG *a,
                                  const YV12_BUFFER_CONFIG *b, int hstart,
                                  int width, int vstart, int height) {
  return highbd_get_sse(
      a->y_buffer + vstart * a->y_stride + hstart, a->y_stride,
      b->y_buffer + vstart * b->y_stride + hstart, b->y_stride, width, height);
}

int64_t aom_highbd_get_y_sse(const YV12_BUFFER_CONFIG *a,
                             const YV12_BUFFER_CONFIG *b) {
  assert(a->y_crop_width == b->y_crop_width);
  assert(a->y_crop_height == b->y_crop_height);
  assert((a->flags & YV12_FLAG_HIGHBITDEPTH) != 0);
  assert((b->flags & YV12_FLAG_HIGHBITDEPTH) != 0);

  return highbd_get_sse(a->y_buffer, a->y_stride, b->y_buffer, b->y_stride,
                        a->y_crop_width, a->y_crop_height);
}

int64_t aom_highbd_get_u_sse_part(const YV12_BUFFER_CONFIG *a,
                                  const YV12_BUFFER_CONFIG *b, int hstart,
                                  int width, int vstart, int height) {
  return highbd_get_sse(a->u_buffer + vstart * a->uv_stride + hstart,
                        a->uv_stride,
                        b->u_buffer + vstart * b->uv_stride + hstart,
                        b->uv_stride, width, height);
}

int64_t aom_highbd_get_u_sse(const YV12_BUFFER_CONFIG *a,
                             const YV12_BUFFER_CONFIG *b) {
  assert(a->uv_crop_width == b->uv_crop_width);
  assert(a->uv_crop_height == b->uv_crop_height);
  assert((a->flags & YV12_FLAG_HIGHBITDEPTH) != 0);
  assert((b->flags & YV12_FLAG_HIGHBITDEPTH) != 0);

  return highbd_get_sse(a->u_buffer, a->uv_stride, b->u_buffer, b->uv_stride,
                        a->uv_crop_width, a->uv_crop_height);
}

int64_t aom_highbd_get_v_sse_part(const YV12_BUFFER_CONFIG *a,
                                  const YV12_BUFFER_CONFIG *b, int hstart,
                                  int width, int vstart, int height) {
  return highbd_get_sse(a->v_buffer + vstart * a->uv_stride + hstart,
                        a->uv_stride,
                        b->v_buffer + vstart * b->uv_stride + hstart,
                        b->uv_stride, width, height);
}

int64_t aom_highbd_get_v_sse(const YV12_BUFFER_CONFIG *a,
                             const YV12_BUFFER_CONFIG *b) {
  assert(a->uv_crop_width == b->uv_crop_width);
  assert(a->uv_crop_height == b->uv_crop_height);
  assert((a->flags & YV12_FLAG_HIGHBITDEPTH) != 0);
  assert((b->flags & YV12_FLAG_HIGHBITDEPTH) != 0);

  return highbd_get_sse(a->v_buffer, a->uv_stride, b->v_buffer, b->uv_stride,
                        a->uv_crop_width, a->uv_crop_height);
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

int64_t aom_get_sse_plane(const YV12_BUFFER_CONFIG *a,
                          const YV12_BUFFER_CONFIG *b, int plane, int highbd) {
#if CONFIG_AV1_HIGHBITDEPTH
  if (highbd) {
    switch (plane) {
      case 0: return aom_highbd_get_y_sse(a, b);
      case 1: return aom_highbd_get_u_sse(a, b);
      case 2: return aom_highbd_get_v_sse(a, b);
      default: assert(plane >= 0 && plane <= 2); return 0;
    }
  } else {
    switch (plane) {
      case 0: return aom_get_y_sse(a, b);
      case 1: return aom_get_u_sse(a, b);
      case 2: return aom_get_v_sse(a, b);
      default: assert(plane >= 0 && plane <= 2); return 0;
    }
  }
#else
  (void)highbd;
  switch (plane) {
    case 0: return aom_get_y_sse(a, b);
    case 1: return aom_get_u_sse(a, b);
    case 2: return aom_get_v_sse(a, b);
    default: assert(plane >= 0 && plane <= 2); return 0;
  }
#endif
}

#if CONFIG_AV1_HIGHBITDEPTH
void aom_calc_highbd_psnr(const YV12_BUFFER_CONFIG *a,
                          const YV12_BUFFER_CONFIG *b, PSNR_STATS *psnr,
                          uint32_t bit_depth, uint32_t in_bit_depth) {
  assert(a->y_crop_width == b->y_crop_width);
  assert(a->y_crop_height == b->y_crop_height);
  assert(a->uv_crop_width == b->uv_crop_width);
  assert(a->uv_crop_height == b->uv_crop_height);
  const int widths[3] = { a->y_crop_width, a->uv_crop_width, a->uv_crop_width };
  const int heights[3] = { a->y_crop_height, a->uv_crop_height,
                           a->uv_crop_height };
  const int a_strides[3] = { a->y_stride, a->uv_stride, a->uv_stride };
  const int b_strides[3] = { b->y_stride, b->uv_stride, b->uv_stride };
  int i;
  uint64_t total_sse = 0;
  uint32_t total_samples = 0;
#if CONFIG_LIBVMAF_PSNR_PEAK
  double peak = (double)(255 << (in_bit_depth - 8));
#else
  double peak = (double)((1 << in_bit_depth) - 1);
#endif  // CONFIG_LIBVMAF_PSNR_PEAK
  const unsigned int input_shift = bit_depth - in_bit_depth;

  for (i = 0; i < 3; ++i) {
    const int w = widths[i];
    const int h = heights[i];
    const uint32_t samples = w * h;
    uint64_t sse;
    if (a->flags & YV12_FLAG_HIGHBITDEPTH) {
      if (input_shift) {
        sse = highbd_get_sse_shift(a->buffers[i], a_strides[i], b->buffers[i],
                                   b_strides[i], w, h, input_shift);
      } else {
        sse = highbd_get_sse(a->buffers[i], a_strides[i], b->buffers[i],
                             b_strides[i], w, h);
      }
    } else {
      sse = get_sse(a->buffers[i], a_strides[i], b->buffers[i], b_strides[i], w,
                    h);
    }
    psnr->sse[1 + i] = sse;
    psnr->samples[1 + i] = samples;
    psnr->psnr[1 + i] = aom_sse_to_psnr(samples, peak, (double)sse);

    total_sse += sse;
    total_samples += samples;
  }

  psnr->sse[0] = total_sse;
  psnr->samples[0] = total_samples;
  psnr->psnr[0] =
      aom_sse_to_psnr((double)total_samples, peak, (double)total_sse);

  // Compute PSNR based on stream bit depth
  if ((a->flags & YV12_FLAG_HIGHBITDEPTH) && (in_bit_depth < bit_depth)) {
#if CONFIG_LIBVMAF_PSNR_PEAK
    peak = (double)(255 << (bit_depth - 8));
#else
    peak = (double)((1 << bit_depth) - 1);
#endif  // CONFIG_LIBVMAF_PSNR_PEAK
    total_sse = 0;
    total_samples = 0;
    for (i = 0; i < 3; ++i) {
      const int w = widths[i];
      const int h = heights[i];
      const uint32_t samples = w * h;
      uint64_t sse;
      sse = highbd_get_sse(a->buffers[i], a_strides[i], b->buffers[i],
                           b_strides[i], w, h);
      psnr->sse_hbd[1 + i] = sse;
      psnr->samples_hbd[1 + i] = samples;
      psnr->psnr_hbd[1 + i] = aom_sse_to_psnr(samples, peak, (double)sse);
      total_sse += sse;
      total_samples += samples;
    }

    psnr->sse_hbd[0] = total_sse;
    psnr->samples_hbd[0] = total_samples;
    psnr->psnr_hbd[0] =
        aom_sse_to_psnr((double)total_samples, peak, (double)total_sse);
  }
}
#endif

void aom_calc_psnr(const YV12_BUFFER_CONFIG *a, const YV12_BUFFER_CONFIG *b,
                   PSNR_STATS *psnr) {
  assert(a->y_crop_width == b->y_crop_width);
  assert(a->y_crop_height == b->y_crop_height);
  assert(a->uv_crop_width == b->uv_crop_width);
  assert(a->uv_crop_height == b->uv_crop_height);
  static const double peak = 255.0;
  const int widths[3] = { a->y_crop_width, a->uv_crop_width, a->uv_crop_width };
  const int heights[3] = { a->y_crop_height, a->uv_crop_height,
                           a->uv_crop_height };
  const int a_strides[3] = { a->y_stride, a->uv_stride, a->uv_stride };
  const int b_strides[3] = { b->y_stride, b->uv_stride, b->uv_stride };
  int i;
  uint64_t total_sse = 0;
  uint32_t total_samples = 0;

  for (i = 0; i < 3; ++i) {
    const int w = widths[i];
    const int h = heights[i];
    const uint32_t samples = w * h;
    const uint64_t sse =
        get_sse(a->buffers[i], a_strides[i], b->buffers[i], b_strides[i], w, h);
    psnr->sse[1 + i] = sse;
    psnr->samples[1 + i] = samples;
    psnr->psnr[1 + i] = aom_sse_to_psnr(samples, peak, (double)sse);

    total_sse += sse;
    total_samples += samples;
  }

  psnr->sse[0] = total_sse;
  psnr->samples[0] = total_samples;
  psnr->psnr[0] =
      aom_sse_to_psnr((double)total_samples, peak, (double)total_sse);
}
