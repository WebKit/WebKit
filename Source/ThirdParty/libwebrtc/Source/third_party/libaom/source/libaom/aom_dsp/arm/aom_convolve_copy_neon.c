/*
 *  Copyright (c) 2020, Alliance for Open Media. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <arm_neon.h>

#include "config/aom_dsp_rtcd.h"

void aom_convolve_copy_neon(const uint8_t *src, ptrdiff_t src_stride,
                            uint8_t *dst, ptrdiff_t dst_stride, int w, int h) {
  const uint8_t *src1;
  uint8_t *dst1;
  int y;

  if (!(w & 0x0F)) {
    for (y = 0; y < h; ++y) {
      src1 = src;
      dst1 = dst;
      for (int x = 0; x < (w >> 4); ++x) {
        vst1q_u8(dst1, vld1q_u8(src1));
        src1 += 16;
        dst1 += 16;
      }
      src += src_stride;
      dst += dst_stride;
    }
  } else if (!(w & 0x07)) {
    for (y = 0; y < h; ++y) {
      vst1_u8(dst, vld1_u8(src));
      src += src_stride;
      dst += dst_stride;
    }
  } else if (!(w & 0x03)) {
    for (y = 0; y < h; ++y) {
      vst1_lane_u32((uint32_t *)(dst), vreinterpret_u32_u8(vld1_u8(src)), 0);
      src += src_stride;
      dst += dst_stride;
    }
  } else if (!(w & 0x01)) {
    for (y = 0; y < h; ++y) {
      vst1_lane_u16((uint16_t *)(dst), vreinterpret_u16_u8(vld1_u8(src)), 0);
      src += src_stride;
      dst += dst_stride;
    }
  }
}
