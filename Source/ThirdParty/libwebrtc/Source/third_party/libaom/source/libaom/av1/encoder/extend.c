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

#include "aom_dsp/aom_dsp_common.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/mem.h"

#include "av1/common/common.h"
#include "av1/encoder/extend.h"

static void copy_and_extend_plane(const uint8_t *src, int src_pitch,
                                  uint8_t *dst, int dst_pitch, int w, int h,
                                  int extend_top, int extend_left,
                                  int extend_bottom, int extend_right,
                                  int chroma_step) {
  int i, linesize;
  // copy the left and right most columns out
  const uint8_t *src_ptr1 = src;
  const uint8_t *src_ptr2 = src + (w - 1) * chroma_step;
  uint8_t *dst_ptr1 = dst - extend_left;
  uint8_t *dst_ptr2 = dst + w;

  for (i = 0; i < h; i++) {
    memset(dst_ptr1, src_ptr1[0], extend_left);
    if (chroma_step == 1) {
      memcpy(dst_ptr1 + extend_left, src_ptr1, w);
    } else {
      for (int j = 0; j < w; j++) {
        dst_ptr1[extend_left + j] = src_ptr1[chroma_step * j];
      }
    }
    memset(dst_ptr2, src_ptr2[0], extend_right);
    src_ptr1 += src_pitch;
    src_ptr2 += src_pitch;
    dst_ptr1 += dst_pitch;
    dst_ptr2 += dst_pitch;
  }

  // Now copy the top and bottom lines into each line of the respective
  // borders
  src_ptr1 = dst - extend_left;
  src_ptr2 = dst + dst_pitch * (h - 1) - extend_left;
  dst_ptr1 = dst + dst_pitch * (-extend_top) - extend_left;
  dst_ptr2 = dst + dst_pitch * (h)-extend_left;
  linesize = extend_left + extend_right + w;
  assert(linesize <= dst_pitch);

  for (i = 0; i < extend_top; i++) {
    memcpy(dst_ptr1, src_ptr1, linesize);
    dst_ptr1 += dst_pitch;
  }

  for (i = 0; i < extend_bottom; i++) {
    memcpy(dst_ptr2, src_ptr2, linesize);
    dst_ptr2 += dst_pitch;
  }
}

static void highbd_copy_and_extend_plane(const uint8_t *src8, int src_pitch,
                                         uint8_t *dst8, int dst_pitch, int w,
                                         int h, int extend_top, int extend_left,
                                         int extend_bottom, int extend_right) {
  int i, linesize;
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);

  // copy the left and right most columns out
  const uint16_t *src_ptr1 = src;
  const uint16_t *src_ptr2 = src + w - 1;
  uint16_t *dst_ptr1 = dst - extend_left;
  uint16_t *dst_ptr2 = dst + w;

  for (i = 0; i < h; i++) {
    aom_memset16(dst_ptr1, src_ptr1[0], extend_left);
    memcpy(dst_ptr1 + extend_left, src_ptr1, w * sizeof(src_ptr1[0]));
    aom_memset16(dst_ptr2, src_ptr2[0], extend_right);
    src_ptr1 += src_pitch;
    src_ptr2 += src_pitch;
    dst_ptr1 += dst_pitch;
    dst_ptr2 += dst_pitch;
  }

  // Now copy the top and bottom lines into each line of the respective
  // borders
  src_ptr1 = dst - extend_left;
  src_ptr2 = dst + dst_pitch * (h - 1) - extend_left;
  dst_ptr1 = dst + dst_pitch * (-extend_top) - extend_left;
  dst_ptr2 = dst + dst_pitch * (h)-extend_left;
  linesize = extend_left + extend_right + w;
  assert(linesize <= dst_pitch);

  for (i = 0; i < extend_top; i++) {
    memcpy(dst_ptr1, src_ptr1, linesize * sizeof(src_ptr1[0]));
    dst_ptr1 += dst_pitch;
  }

  for (i = 0; i < extend_bottom; i++) {
    memcpy(dst_ptr2, src_ptr2, linesize * sizeof(src_ptr2[0]));
    dst_ptr2 += dst_pitch;
  }
}

void av1_copy_and_extend_frame(const YV12_BUFFER_CONFIG *src,
                               YV12_BUFFER_CONFIG *dst) {
  // Extend src frame in buffer
  const int et_y = dst->border;
  const int el_y = dst->border;
  const int er_y =
      AOMMAX(src->y_width + dst->border, ALIGN_POWER_OF_TWO(src->y_width, 6)) -
      src->y_crop_width;
  const int eb_y = AOMMAX(src->y_height + dst->border,
                          ALIGN_POWER_OF_TWO(src->y_height, 6)) -
                   src->y_crop_height;
  const int uv_width_subsampling = src->subsampling_x;
  const int uv_height_subsampling = src->subsampling_y;
  const int et_uv = et_y >> uv_height_subsampling;
  const int el_uv = el_y >> uv_width_subsampling;
  const int eb_uv = eb_y >> uv_height_subsampling;
  const int er_uv = er_y >> uv_width_subsampling;

  if (src->flags & YV12_FLAG_HIGHBITDEPTH) {
    highbd_copy_and_extend_plane(src->y_buffer, src->y_stride, dst->y_buffer,
                                 dst->y_stride, src->y_crop_width,
                                 src->y_crop_height, et_y, el_y, eb_y, er_y);
    if (!src->monochrome) {
      highbd_copy_and_extend_plane(
          src->u_buffer, src->uv_stride, dst->u_buffer, dst->uv_stride,
          src->uv_crop_width, src->uv_crop_height, et_uv, el_uv, eb_uv, er_uv);
      highbd_copy_and_extend_plane(
          src->v_buffer, src->uv_stride, dst->v_buffer, dst->uv_stride,
          src->uv_crop_width, src->uv_crop_height, et_uv, el_uv, eb_uv, er_uv);
    }
    return;
  }

  copy_and_extend_plane(src->y_buffer, src->y_stride, dst->y_buffer,
                        dst->y_stride, src->y_crop_width, src->y_crop_height,
                        et_y, el_y, eb_y, er_y, 1);
  if (!src->monochrome) {
    // detect nv12 format
    const int chroma_step = src->v_buffer ? 1 : 2;
    const uint8_t *src_v_buffer =
        src->v_buffer ? src->v_buffer : src->u_buffer + 1;
    copy_and_extend_plane(src->u_buffer, src->uv_stride, dst->u_buffer,
                          dst->uv_stride, src->uv_crop_width,
                          src->uv_crop_height, et_uv, el_uv, eb_uv, er_uv,
                          chroma_step);
    copy_and_extend_plane(src_v_buffer, src->uv_stride, dst->v_buffer,
                          dst->uv_stride, src->uv_crop_width,
                          src->uv_crop_height, et_uv, el_uv, eb_uv, er_uv,
                          chroma_step);
  }
}
