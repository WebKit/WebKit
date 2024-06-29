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
#include <string.h>

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
      memcpy(dst, src, sizeof(uint32_t));
      src += src_stride;
      dst += dst_stride;
    }
  } else if (!(w & 0x01)) {
    for (y = 0; y < h; ++y) {
      memcpy(dst, src, sizeof(uint16_t));
      src += src_stride;
      dst += dst_stride;
    }
  }
}

#if CONFIG_AV1_HIGHBITDEPTH
void aom_highbd_convolve_copy_neon(const uint16_t *src, ptrdiff_t src_stride,
                                   uint16_t *dst, ptrdiff_t dst_stride, int w,
                                   int h) {
  if (w < 8) {  // copy4
    uint16x4_t s0, s1;
    do {
      s0 = vld1_u16(src);
      src += src_stride;
      s1 = vld1_u16(src);
      src += src_stride;

      vst1_u16(dst, s0);
      dst += dst_stride;
      vst1_u16(dst, s1);
      dst += dst_stride;
      h -= 2;
    } while (h != 0);
  } else if (w == 8) {  // copy8
    uint16x8_t s0, s1;
    do {
      s0 = vld1q_u16(src);
      src += src_stride;
      s1 = vld1q_u16(src);
      src += src_stride;

      vst1q_u16(dst, s0);
      dst += dst_stride;
      vst1q_u16(dst, s1);
      dst += dst_stride;
      h -= 2;
    } while (h != 0);
  } else if (w < 32) {  // copy16
    uint16x8_t s0, s1, s2, s3;
    do {
      s0 = vld1q_u16(src);
      s1 = vld1q_u16(src + 8);
      src += src_stride;
      s2 = vld1q_u16(src);
      s3 = vld1q_u16(src + 8);
      src += src_stride;

      vst1q_u16(dst, s0);
      vst1q_u16(dst + 8, s1);
      dst += dst_stride;
      vst1q_u16(dst, s2);
      vst1q_u16(dst + 8, s3);
      dst += dst_stride;
      h -= 2;
    } while (h != 0);
  } else if (w == 32) {  // copy32
    uint16x8_t s0, s1, s2, s3;
    do {
      s0 = vld1q_u16(src);
      s1 = vld1q_u16(src + 8);
      s2 = vld1q_u16(src + 16);
      s3 = vld1q_u16(src + 24);
      src += src_stride;

      vst1q_u16(dst, s0);
      vst1q_u16(dst + 8, s1);
      vst1q_u16(dst + 16, s2);
      vst1q_u16(dst + 24, s3);
      dst += dst_stride;
    } while (--h != 0);
  } else {  // copy64
    uint16x8_t s0, s1, s2, s3, s4, s5, s6, s7;
    do {
      const uint16_t *s = src;
      uint16_t *d = dst;
      int width = w;
      do {
        s0 = vld1q_u16(s);
        s1 = vld1q_u16(s + 8);
        s2 = vld1q_u16(s + 16);
        s3 = vld1q_u16(s + 24);
        s4 = vld1q_u16(s + 32);
        s5 = vld1q_u16(s + 40);
        s6 = vld1q_u16(s + 48);
        s7 = vld1q_u16(s + 56);

        vst1q_u16(d, s0);
        vst1q_u16(d + 8, s1);
        vst1q_u16(d + 16, s2);
        vst1q_u16(d + 24, s3);
        vst1q_u16(d + 32, s4);
        vst1q_u16(d + 40, s5);
        vst1q_u16(d + 48, s6);
        vst1q_u16(d + 56, s7);
        s += 64;
        d += 64;
        width -= 64;
      } while (width > 0);
      src += src_stride;
      dst += dst_stride;
    } while (--h != 0);
  }
}

#endif  // CONFIG_AV1_HIGHBITDEPTH
