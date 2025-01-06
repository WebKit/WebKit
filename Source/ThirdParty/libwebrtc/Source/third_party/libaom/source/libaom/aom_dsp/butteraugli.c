/*
 * Copyright (c) 2021, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>
#include <jxl/butteraugli.h>

#include "aom_dsp/butteraugli.h"
#include "aom_mem/aom_mem.h"
#include "third_party/libyuv/include/libyuv/convert_argb.h"

int aom_calc_butteraugli(const YV12_BUFFER_CONFIG *source,
                         const YV12_BUFFER_CONFIG *distorted, int bit_depth,
                         aom_matrix_coefficients_t matrix_coefficients,
                         aom_color_range_t color_range, float *dist_map) {
  (void)bit_depth;
  assert(bit_depth == 8);
  const int width = source->y_crop_width;
  const int height = source->y_crop_height;
  const int ss_x = source->subsampling_x;
  const int ss_y = source->subsampling_y;

  const struct YuvConstants *yuv_constants;
  if (matrix_coefficients == AOM_CICP_MC_BT_709) {
    if (color_range == AOM_CR_FULL_RANGE) return 0;
    yuv_constants = &kYuvH709Constants;
  } else {
    yuv_constants = color_range == AOM_CR_FULL_RANGE ? &kYuvJPEGConstants
                                                     : &kYuvI601Constants;
  }

  const int stride_argb = width * 4;
  const size_t buffer_size = (size_t)height * stride_argb;
  uint8_t *src_argb = (uint8_t *)aom_malloc(buffer_size);
  uint8_t *distorted_argb = (uint8_t *)aom_malloc(buffer_size);
  if (!src_argb || !distorted_argb) {
    aom_free(src_argb);
    aom_free(distorted_argb);
    return 0;
  }

  if (ss_x == 1 && ss_y == 1) {
    I420ToARGBMatrix(source->y_buffer, source->y_stride, source->u_buffer,
                     source->uv_stride, source->v_buffer, source->uv_stride,
                     src_argb, stride_argb, yuv_constants, width, height);
    I420ToARGBMatrix(distorted->y_buffer, distorted->y_stride,
                     distorted->u_buffer, distorted->uv_stride,
                     distorted->v_buffer, distorted->uv_stride, distorted_argb,
                     stride_argb, yuv_constants, width, height);
  } else if (ss_x == 1 && ss_y == 0) {
    I422ToARGBMatrix(source->y_buffer, source->y_stride, source->u_buffer,
                     source->uv_stride, source->v_buffer, source->uv_stride,
                     src_argb, stride_argb, yuv_constants, width, height);
    I422ToARGBMatrix(distorted->y_buffer, distorted->y_stride,
                     distorted->u_buffer, distorted->uv_stride,
                     distorted->v_buffer, distorted->uv_stride, distorted_argb,
                     stride_argb, yuv_constants, width, height);
  } else if (ss_x == 0 && ss_y == 0) {
    I444ToARGBMatrix(source->y_buffer, source->y_stride, source->u_buffer,
                     source->uv_stride, source->v_buffer, source->uv_stride,
                     src_argb, stride_argb, yuv_constants, width, height);
    I444ToARGBMatrix(distorted->y_buffer, distorted->y_stride,
                     distorted->u_buffer, distorted->uv_stride,
                     distorted->v_buffer, distorted->uv_stride, distorted_argb,
                     stride_argb, yuv_constants, width, height);
  } else {
    aom_free(src_argb);
    aom_free(distorted_argb);
    return 0;
  }

  JxlPixelFormat pixel_format = { 4, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0 };
  JxlButteraugliApi *api = JxlButteraugliApiCreate(NULL);
  JxlButteraugliApiSetHFAsymmetry(api, 0.8f);

  JxlButteraugliResult *result = JxlButteraugliCompute(
      api, width, height, &pixel_format, src_argb, buffer_size, &pixel_format,
      distorted_argb, buffer_size);

  const float *distmap = NULL;
  uint32_t row_stride;
  JxlButteraugliResultGetDistmap(result, &distmap, &row_stride);
  if (distmap == NULL) {
    JxlButteraugliApiDestroy(api);
    JxlButteraugliResultDestroy(result);
    aom_free(src_argb);
    aom_free(distorted_argb);
    return 0;
  }

  for (int j = 0; j < height; ++j) {
    for (int i = 0; i < width; ++i) {
      dist_map[j * width + i] = distmap[j * row_stride + i];
    }
  }

  JxlButteraugliApiDestroy(api);
  JxlButteraugliResultDestroy(result);
  aom_free(src_argb);
  aom_free(distorted_argb);
  return 1;
}
