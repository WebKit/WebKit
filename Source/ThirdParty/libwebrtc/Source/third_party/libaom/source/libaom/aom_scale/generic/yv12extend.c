/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>

#include "config/aom_config.h"
#include "config/aom_scale_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/mem.h"
#include "aom_scale/yv12config.h"

static void extend_plane(uint8_t *const src, int src_stride, int width,
                         int height, int extend_top, int extend_left,
                         int extend_bottom, int extend_right, int v_start,
                         int v_end) {
  assert(src != NULL);
  int i;
  const int linesize = extend_left + extend_right + width;
  assert(linesize <= src_stride);

  /* copy the left and right most columns out */
  uint8_t *src_ptr1 = src + v_start * src_stride;
  uint8_t *src_ptr2 = src + v_start * src_stride + width - 1;
  uint8_t *dst_ptr1 = src + v_start * src_stride - extend_left;
  uint8_t *dst_ptr2 = src_ptr2 + 1;

  for (i = v_start; i < v_end; ++i) {
    memset(dst_ptr1, src_ptr1[0], extend_left);
    memset(dst_ptr2, src_ptr2[0], extend_right);
    src_ptr1 += src_stride;
    src_ptr2 += src_stride;
    dst_ptr1 += src_stride;
    dst_ptr2 += src_stride;
  }

  /* Now copy the top and bottom lines into each line of the respective
   * borders
   */
  src_ptr1 = src - extend_left;
  dst_ptr1 = src_ptr1 + src_stride * -extend_top;

  for (i = 0; i < extend_top; ++i) {
    memcpy(dst_ptr1, src_ptr1, linesize);
    dst_ptr1 += src_stride;
  }

  src_ptr2 = src_ptr1 + src_stride * (height - 1);
  dst_ptr2 = src_ptr2;

  for (i = 0; i < extend_bottom; ++i) {
    dst_ptr2 += src_stride;
    memcpy(dst_ptr2, src_ptr2, linesize);
  }
}

#if CONFIG_AV1_HIGHBITDEPTH
static void extend_plane_high(uint8_t *const src8, int src_stride, int width,
                              int height, int extend_top, int extend_left,
                              int extend_bottom, int extend_right, int v_start,
                              int v_end) {
  int i;
  const int linesize = extend_left + extend_right + width;
  assert(linesize <= src_stride);
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);

  /* copy the left and right most columns out */
  uint16_t *src_ptr1 = src + v_start * src_stride;
  uint16_t *src_ptr2 = src + v_start * src_stride + width - 1;
  uint16_t *dst_ptr1 = src + v_start * src_stride - extend_left;
  uint16_t *dst_ptr2 = src_ptr2 + 1;

  for (i = v_start; i < v_end; ++i) {
    aom_memset16(dst_ptr1, src_ptr1[0], extend_left);
    aom_memset16(dst_ptr2, src_ptr2[0], extend_right);
    src_ptr1 += src_stride;
    src_ptr2 += src_stride;
    dst_ptr1 += src_stride;
    dst_ptr2 += src_stride;
  }

  /* Now copy the top and bottom lines into each line of the respective
   * borders
   */
  src_ptr1 = src - extend_left;
  dst_ptr1 = src_ptr1 + src_stride * -extend_top;

  for (i = 0; i < extend_top; ++i) {
    memcpy(dst_ptr1, src_ptr1, linesize * sizeof(uint16_t));
    dst_ptr1 += src_stride;
  }

  src_ptr2 = src_ptr1 + src_stride * (height - 1);
  dst_ptr2 = src_ptr2;

  for (i = 0; i < extend_bottom; ++i) {
    dst_ptr2 += src_stride;
    memcpy(dst_ptr2, src_ptr2, linesize * sizeof(uint16_t));
  }
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

void aom_extend_frame_borders_plane_row_c(const YV12_BUFFER_CONFIG *ybf,
                                          int plane, int v_start, int v_end) {
  const int ext_size = ybf->border;
  const int ss_x = ybf->subsampling_x;
  const int ss_y = ybf->subsampling_y;

  assert(ybf->y_height - ybf->y_crop_height < 16);
  assert(ybf->y_width - ybf->y_crop_width < 16);
  assert(ybf->y_height - ybf->y_crop_height >= 0);
  assert(ybf->y_width - ybf->y_crop_width >= 0);

  const int is_uv = plane > 0;
  const int top = ext_size >> (is_uv ? ss_y : 0);
  const int left = ext_size >> (is_uv ? ss_x : 0);
  const int bottom = top + ybf->heights[is_uv] - ybf->crop_heights[is_uv];
  const int right = left + ybf->widths[is_uv] - ybf->crop_widths[is_uv];
  const int extend_top_border = (v_start == 0);
  const int extend_bottom_border = (v_end == ybf->crop_heights[is_uv]);

#if CONFIG_AV1_HIGHBITDEPTH
  if (ybf->flags & YV12_FLAG_HIGHBITDEPTH) {
    extend_plane_high(ybf->buffers[plane], ybf->strides[is_uv],
                      ybf->crop_widths[is_uv], ybf->crop_heights[is_uv],
                      extend_top_border ? top : 0, left,
                      extend_bottom_border ? bottom : 0, right, v_start, v_end);
    return;
  }
#endif

  extend_plane(ybf->buffers[plane], ybf->strides[is_uv],
               ybf->crop_widths[is_uv], ybf->crop_heights[is_uv],
               extend_top_border ? top : 0, left,
               extend_bottom_border ? bottom : 0, right, v_start, v_end);
}

void aom_yv12_extend_frame_borders_c(YV12_BUFFER_CONFIG *ybf,
                                     const int num_planes) {
  assert(ybf->border % 2 == 0);
  assert(ybf->y_height - ybf->y_crop_height < 16);
  assert(ybf->y_width - ybf->y_crop_width < 16);
  assert(ybf->y_height - ybf->y_crop_height >= 0);
  assert(ybf->y_width - ybf->y_crop_width >= 0);

#if CONFIG_AV1_HIGHBITDEPTH
  if (ybf->flags & YV12_FLAG_HIGHBITDEPTH) {
    for (int plane = 0; plane < num_planes; ++plane) {
      const int is_uv = plane > 0;
      const int plane_border = ybf->border >> is_uv;
      extend_plane_high(
          ybf->buffers[plane], ybf->strides[is_uv], ybf->crop_widths[is_uv],
          ybf->crop_heights[is_uv], plane_border, plane_border,
          plane_border + ybf->heights[is_uv] - ybf->crop_heights[is_uv],
          plane_border + ybf->widths[is_uv] - ybf->crop_widths[is_uv], 0,
          ybf->crop_heights[is_uv]);
    }
    return;
  }
#endif

  for (int plane = 0; plane < num_planes; ++plane) {
    const int is_uv = plane > 0;
    const int plane_border = ybf->border >> is_uv;
    extend_plane(ybf->buffers[plane], ybf->strides[is_uv],
                 ybf->crop_widths[is_uv], ybf->crop_heights[is_uv],
                 plane_border, plane_border,
                 plane_border + ybf->heights[is_uv] - ybf->crop_heights[is_uv],
                 plane_border + ybf->widths[is_uv] - ybf->crop_widths[is_uv], 0,
                 ybf->crop_heights[is_uv]);
  }
}

static void extend_frame(YV12_BUFFER_CONFIG *const ybf, int ext_size,
                         const int num_planes) {
  const int ss_x = ybf->subsampling_x;
  const int ss_y = ybf->subsampling_y;

  assert(ybf->y_height - ybf->y_crop_height < 16);
  assert(ybf->y_width - ybf->y_crop_width < 16);
  assert(ybf->y_height - ybf->y_crop_height >= 0);
  assert(ybf->y_width - ybf->y_crop_width >= 0);

#if CONFIG_AV1_HIGHBITDEPTH
  if (ybf->flags & YV12_FLAG_HIGHBITDEPTH) {
    for (int plane = 0; plane < num_planes; ++plane) {
      const int is_uv = plane > 0;
      const int top = ext_size >> (is_uv ? ss_y : 0);
      const int left = ext_size >> (is_uv ? ss_x : 0);
      const int bottom = top + ybf->heights[is_uv] - ybf->crop_heights[is_uv];
      const int right = left + ybf->widths[is_uv] - ybf->crop_widths[is_uv];
      extend_plane_high(ybf->buffers[plane], ybf->strides[is_uv],
                        ybf->crop_widths[is_uv], ybf->crop_heights[is_uv], top,
                        left, bottom, right, 0, ybf->crop_heights[is_uv]);
    }
    return;
  }
#endif

  for (int plane = 0; plane < num_planes; ++plane) {
    const int is_uv = plane > 0;
    const int top = ext_size >> (is_uv ? ss_y : 0);
    const int left = ext_size >> (is_uv ? ss_x : 0);
    const int bottom = top + ybf->heights[is_uv] - ybf->crop_heights[is_uv];
    const int right = left + ybf->widths[is_uv] - ybf->crop_widths[is_uv];
    extend_plane(ybf->buffers[plane], ybf->strides[is_uv],
                 ybf->crop_widths[is_uv], ybf->crop_heights[is_uv], top, left,
                 bottom, right, 0, ybf->crop_heights[is_uv]);
  }
}

void aom_extend_frame_borders_c(YV12_BUFFER_CONFIG *ybf, const int num_planes) {
  extend_frame(ybf, ybf->border, num_planes);
}

void aom_extend_frame_inner_borders_c(YV12_BUFFER_CONFIG *ybf,
                                      const int num_planes) {
  const int inner_bw = (ybf->border > AOMINNERBORDERINPIXELS)
                           ? AOMINNERBORDERINPIXELS
                           : ybf->border;
  extend_frame(ybf, inner_bw, num_planes);
}

void aom_extend_frame_borders_y_c(YV12_BUFFER_CONFIG *ybf) {
  int ext_size = ybf->border;
  assert(ybf->y_height - ybf->y_crop_height < 16);
  assert(ybf->y_width - ybf->y_crop_width < 16);
  assert(ybf->y_height - ybf->y_crop_height >= 0);
  assert(ybf->y_width - ybf->y_crop_width >= 0);
#if CONFIG_AV1_HIGHBITDEPTH
  if (ybf->flags & YV12_FLAG_HIGHBITDEPTH) {
    extend_plane_high(
        ybf->y_buffer, ybf->y_stride, ybf->y_crop_width, ybf->y_crop_height,
        ext_size, ext_size, ext_size + ybf->y_height - ybf->y_crop_height,
        ext_size + ybf->y_width - ybf->y_crop_width, 0, ybf->y_crop_height);
    return;
  }
#endif
  extend_plane(
      ybf->y_buffer, ybf->y_stride, ybf->y_crop_width, ybf->y_crop_height,
      ext_size, ext_size, ext_size + ybf->y_height - ybf->y_crop_height,
      ext_size + ybf->y_width - ybf->y_crop_width, 0, ybf->y_crop_height);
}

#if CONFIG_AV1_HIGHBITDEPTH
static void memcpy_short_addr(uint8_t *dst8, const uint8_t *src8, int num) {
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  memcpy(dst, src, num * sizeof(uint16_t));
}
#endif

// Copies the source image into the destination image and updates the
// destination's UMV borders.
// Note: The frames are assumed to be identical in size.
void aom_yv12_copy_frame_c(const YV12_BUFFER_CONFIG *src_bc,
                           YV12_BUFFER_CONFIG *dst_bc, const int num_planes) {
  assert(src_bc->y_width == dst_bc->y_width);
  assert(src_bc->y_height == dst_bc->y_height);

#if CONFIG_AV1_HIGHBITDEPTH
  assert((src_bc->flags & YV12_FLAG_HIGHBITDEPTH) ==
         (dst_bc->flags & YV12_FLAG_HIGHBITDEPTH));

  if (src_bc->flags & YV12_FLAG_HIGHBITDEPTH) {
    for (int plane = 0; plane < num_planes; ++plane) {
      const uint8_t *plane_src = src_bc->buffers[plane];
      uint8_t *plane_dst = dst_bc->buffers[plane];
      const int is_uv = plane > 0;

      for (int row = 0; row < src_bc->heights[is_uv]; ++row) {
        memcpy_short_addr(plane_dst, plane_src, src_bc->widths[is_uv]);
        plane_src += src_bc->strides[is_uv];
        plane_dst += dst_bc->strides[is_uv];
      }
    }
    aom_yv12_extend_frame_borders_c(dst_bc, num_planes);
    return;
  }
#endif
  for (int plane = 0; plane < num_planes; ++plane) {
    const uint8_t *plane_src = src_bc->buffers[plane];
    uint8_t *plane_dst = dst_bc->buffers[plane];
    const int is_uv = plane > 0;

    for (int row = 0; row < src_bc->heights[is_uv]; ++row) {
      memcpy(plane_dst, plane_src, src_bc->widths[is_uv]);
      plane_src += src_bc->strides[is_uv];
      plane_dst += dst_bc->strides[is_uv];
    }
  }
  aom_yv12_extend_frame_borders_c(dst_bc, num_planes);
}

void aom_yv12_copy_y_c(const YV12_BUFFER_CONFIG *src_ybc,
                       YV12_BUFFER_CONFIG *dst_ybc, int use_crop) {
  int row;
  int width = use_crop ? src_ybc->y_crop_width : src_ybc->y_width;
  int height = use_crop ? src_ybc->y_crop_height : src_ybc->y_height;
  const uint8_t *src = src_ybc->y_buffer;
  uint8_t *dst = dst_ybc->y_buffer;

#if CONFIG_AV1_HIGHBITDEPTH
  if (src_ybc->flags & YV12_FLAG_HIGHBITDEPTH) {
    const uint16_t *src16 = CONVERT_TO_SHORTPTR(src);
    uint16_t *dst16 = CONVERT_TO_SHORTPTR(dst);
    for (row = 0; row < height; ++row) {
      memcpy(dst16, src16, width * sizeof(uint16_t));
      src16 += src_ybc->y_stride;
      dst16 += dst_ybc->y_stride;
    }
    return;
  }
#endif

  for (row = 0; row < height; ++row) {
    memcpy(dst, src, width);
    src += src_ybc->y_stride;
    dst += dst_ybc->y_stride;
  }
}

void aom_yv12_copy_u_c(const YV12_BUFFER_CONFIG *src_bc,
                       YV12_BUFFER_CONFIG *dst_bc, int use_crop) {
  int row;
  int width = use_crop ? src_bc->uv_crop_width : src_bc->uv_width;
  int height = use_crop ? src_bc->uv_crop_height : src_bc->uv_height;
  const uint8_t *src = src_bc->u_buffer;
  uint8_t *dst = dst_bc->u_buffer;
#if CONFIG_AV1_HIGHBITDEPTH
  if (src_bc->flags & YV12_FLAG_HIGHBITDEPTH) {
    const uint16_t *src16 = CONVERT_TO_SHORTPTR(src);
    uint16_t *dst16 = CONVERT_TO_SHORTPTR(dst);
    for (row = 0; row < height; ++row) {
      memcpy(dst16, src16, width * sizeof(uint16_t));
      src16 += src_bc->uv_stride;
      dst16 += dst_bc->uv_stride;
    }
    return;
  }
#endif
  for (row = 0; row < height; ++row) {
    memcpy(dst, src, width);
    src += src_bc->uv_stride;
    dst += dst_bc->uv_stride;
  }
}

void aom_yv12_copy_v_c(const YV12_BUFFER_CONFIG *src_bc,
                       YV12_BUFFER_CONFIG *dst_bc, int use_crop) {
  int row;
  int width = use_crop ? src_bc->uv_crop_width : src_bc->uv_width;
  int height = use_crop ? src_bc->uv_crop_height : src_bc->uv_height;
  const uint8_t *src = src_bc->v_buffer;
  uint8_t *dst = dst_bc->v_buffer;
#if CONFIG_AV1_HIGHBITDEPTH
  if (src_bc->flags & YV12_FLAG_HIGHBITDEPTH) {
    const uint16_t *src16 = CONVERT_TO_SHORTPTR(src);
    uint16_t *dst16 = CONVERT_TO_SHORTPTR(dst);
    for (row = 0; row < height; ++row) {
      memcpy(dst16, src16, width * sizeof(uint16_t));
      src16 += src_bc->uv_stride;
      dst16 += dst_bc->uv_stride;
    }
    return;
  }
#endif
  for (row = 0; row < height; ++row) {
    memcpy(dst, src, width);
    src += src_bc->uv_stride;
    dst += dst_bc->uv_stride;
  }
}

void aom_yv12_partial_copy_y_c(const YV12_BUFFER_CONFIG *src_ybc, int hstart1,
                               int hend1, int vstart1, int vend1,
                               YV12_BUFFER_CONFIG *dst_ybc, int hstart2,
                               int vstart2) {
  int row;
  const uint8_t *src = src_ybc->y_buffer;
  uint8_t *dst = dst_ybc->y_buffer;
#if CONFIG_AV1_HIGHBITDEPTH
  if (src_ybc->flags & YV12_FLAG_HIGHBITDEPTH) {
    const uint16_t *src16 =
        CONVERT_TO_SHORTPTR(src + vstart1 * src_ybc->y_stride + hstart1);
    uint16_t *dst16 =
        CONVERT_TO_SHORTPTR(dst + vstart2 * dst_ybc->y_stride + hstart2);

    for (row = vstart1; row < vend1; ++row) {
      memcpy(dst16, src16, (hend1 - hstart1) * sizeof(uint16_t));
      src16 += src_ybc->y_stride;
      dst16 += dst_ybc->y_stride;
    }
    return;
  }
#endif
  src = (src + vstart1 * src_ybc->y_stride + hstart1);
  dst = (dst + vstart2 * dst_ybc->y_stride + hstart2);

  for (row = vstart1; row < vend1; ++row) {
    memcpy(dst, src, (hend1 - hstart1));
    src += src_ybc->y_stride;
    dst += dst_ybc->y_stride;
  }
}

void aom_yv12_partial_coloc_copy_y_c(const YV12_BUFFER_CONFIG *src_ybc,
                                     YV12_BUFFER_CONFIG *dst_ybc, int hstart,
                                     int hend, int vstart, int vend) {
  aom_yv12_partial_copy_y_c(src_ybc, hstart, hend, vstart, vend, dst_ybc,
                            hstart, vstart);
}

void aom_yv12_partial_copy_u_c(const YV12_BUFFER_CONFIG *src_bc, int hstart1,
                               int hend1, int vstart1, int vend1,
                               YV12_BUFFER_CONFIG *dst_bc, int hstart2,
                               int vstart2) {
  int row;
  const uint8_t *src = src_bc->u_buffer;
  uint8_t *dst = dst_bc->u_buffer;
#if CONFIG_AV1_HIGHBITDEPTH
  if (src_bc->flags & YV12_FLAG_HIGHBITDEPTH) {
    const uint16_t *src16 =
        CONVERT_TO_SHORTPTR(src + vstart1 * src_bc->uv_stride + hstart1);
    uint16_t *dst16 =
        CONVERT_TO_SHORTPTR(dst + vstart2 * dst_bc->uv_stride + hstart2);
    for (row = vstart1; row < vend1; ++row) {
      memcpy(dst16, src16, (hend1 - hstart1) * sizeof(uint16_t));
      src16 += src_bc->uv_stride;
      dst16 += dst_bc->uv_stride;
    }
    return;
  }
#endif
  src = (src + vstart1 * src_bc->uv_stride + hstart1);
  dst = (dst + vstart2 * dst_bc->uv_stride + hstart2);

  for (row = vstart1; row < vend1; ++row) {
    memcpy(dst, src, (hend1 - hstart1));
    src += src_bc->uv_stride;
    dst += dst_bc->uv_stride;
  }
}

void aom_yv12_partial_coloc_copy_u_c(const YV12_BUFFER_CONFIG *src_bc,
                                     YV12_BUFFER_CONFIG *dst_bc, int hstart,
                                     int hend, int vstart, int vend) {
  aom_yv12_partial_copy_u_c(src_bc, hstart, hend, vstart, vend, dst_bc, hstart,
                            vstart);
}

void aom_yv12_partial_copy_v_c(const YV12_BUFFER_CONFIG *src_bc, int hstart1,
                               int hend1, int vstart1, int vend1,
                               YV12_BUFFER_CONFIG *dst_bc, int hstart2,
                               int vstart2) {
  int row;
  const uint8_t *src = src_bc->v_buffer;
  uint8_t *dst = dst_bc->v_buffer;
#if CONFIG_AV1_HIGHBITDEPTH
  if (src_bc->flags & YV12_FLAG_HIGHBITDEPTH) {
    const uint16_t *src16 =
        CONVERT_TO_SHORTPTR(src + vstart1 * src_bc->uv_stride + hstart1);
    uint16_t *dst16 =
        CONVERT_TO_SHORTPTR(dst + vstart2 * dst_bc->uv_stride + hstart2);
    for (row = vstart1; row < vend1; ++row) {
      memcpy(dst16, src16, (hend1 - hstart1) * sizeof(uint16_t));
      src16 += src_bc->uv_stride;
      dst16 += dst_bc->uv_stride;
    }
    return;
  }
#endif
  src = (src + vstart1 * src_bc->uv_stride + hstart1);
  dst = (dst + vstart2 * dst_bc->uv_stride + hstart2);

  for (row = vstart1; row < vend1; ++row) {
    memcpy(dst, src, (hend1 - hstart1));
    src += src_bc->uv_stride;
    dst += dst_bc->uv_stride;
  }
}

void aom_yv12_partial_coloc_copy_v_c(const YV12_BUFFER_CONFIG *src_bc,
                                     YV12_BUFFER_CONFIG *dst_bc, int hstart,
                                     int hend, int vstart, int vend) {
  aom_yv12_partial_copy_v_c(src_bc, hstart, hend, vstart, vend, dst_bc, hstart,
                            vstart);
}

int aom_yv12_realloc_with_new_border_c(YV12_BUFFER_CONFIG *ybf, int new_border,
                                       int byte_alignment, bool alloc_pyramid,
                                       int num_planes) {
  if (ybf) {
    if (new_border == ybf->border) return 0;
    YV12_BUFFER_CONFIG new_buf;
    memset(&new_buf, 0, sizeof(new_buf));
    const int error = aom_alloc_frame_buffer(
        &new_buf, ybf->y_crop_width, ybf->y_crop_height, ybf->subsampling_x,
        ybf->subsampling_y, ybf->flags & YV12_FLAG_HIGHBITDEPTH, new_border,
        byte_alignment, alloc_pyramid, 0);
    if (error) return error;
    // Copy image buffer
    aom_yv12_copy_frame(ybf, &new_buf, num_planes);

    // Extend up to new border
    aom_extend_frame_borders(&new_buf, num_planes);

    // Now free the old buffer and replace with the new
    aom_free_frame_buffer(ybf);
    memcpy(ybf, &new_buf, sizeof(new_buf));
    return 0;
  }
  return -2;
}
