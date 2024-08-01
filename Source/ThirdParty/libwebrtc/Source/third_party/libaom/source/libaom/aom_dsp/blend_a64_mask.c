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

#include "aom/aom_integer.h"
#include "aom_ports/mem.h"
#include "aom_dsp/blend.h"
#include "aom_dsp/aom_dsp_common.h"

#include "config/aom_dsp_rtcd.h"

// Blending with alpha mask. Mask values come from the range [0, 64],
// as described for AOM_BLEND_A64 in aom_dsp/blend.h. src0 or src1 can
// be the same as dst, or dst can be different from both sources.

// NOTE(rachelbarker): The input and output of aom_blend_a64_d16_mask_c() are
// in a higher intermediate precision, and will later be rounded down to pixel
// precision.
// Thus, in order to avoid double-rounding, we want to use normal right shifts
// within this function, not ROUND_POWER_OF_TWO.
// This works because of the identity:
// ROUND_POWER_OF_TWO(x >> y, z) == ROUND_POWER_OF_TWO(x, y+z)
//
// In contrast, the output of the non-d16 functions will not be further rounded,
// so we *should* use ROUND_POWER_OF_TWO there.

void aom_lowbd_blend_a64_d16_mask_c(
    uint8_t *dst, uint32_t dst_stride, const CONV_BUF_TYPE *src0,
    uint32_t src0_stride, const CONV_BUF_TYPE *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h, int subw, int subh,
    ConvolveParams *conv_params) {
  int i, j;
  const int bd = 8;
  const int offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  const int round_offset = (1 << (offset_bits - conv_params->round_1)) +
                           (1 << (offset_bits - conv_params->round_1 - 1));
  const int round_bits =
      2 * FILTER_BITS - conv_params->round_0 - conv_params->round_1;

  assert(IMPLIES((void *)src0 == dst, src0_stride == dst_stride));
  assert(IMPLIES((void *)src1 == dst, src1_stride == dst_stride));

  assert(h >= 4);
  assert(w >= 4);
  assert(IS_POWER_OF_TWO(h));
  assert(IS_POWER_OF_TWO(w));

  if (subw == 0 && subh == 0) {
    for (i = 0; i < h; ++i) {
      for (j = 0; j < w; ++j) {
        int32_t res;
        const int m = mask[i * mask_stride + j];
        res = ((m * (int32_t)src0[i * src0_stride + j] +
                (AOM_BLEND_A64_MAX_ALPHA - m) *
                    (int32_t)src1[i * src1_stride + j]) >>
               AOM_BLEND_A64_ROUND_BITS);
        res -= round_offset;
        dst[i * dst_stride + j] =
            clip_pixel(ROUND_POWER_OF_TWO(res, round_bits));
      }
    }
  } else if (subw == 1 && subh == 1) {
    for (i = 0; i < h; ++i) {
      for (j = 0; j < w; ++j) {
        int32_t res;
        const int m = ROUND_POWER_OF_TWO(
            mask[(2 * i) * mask_stride + (2 * j)] +
                mask[(2 * i + 1) * mask_stride + (2 * j)] +
                mask[(2 * i) * mask_stride + (2 * j + 1)] +
                mask[(2 * i + 1) * mask_stride + (2 * j + 1)],
            2);
        res = ((m * (int32_t)src0[i * src0_stride + j] +
                (AOM_BLEND_A64_MAX_ALPHA - m) *
                    (int32_t)src1[i * src1_stride + j]) >>
               AOM_BLEND_A64_ROUND_BITS);
        res -= round_offset;
        dst[i * dst_stride + j] =
            clip_pixel(ROUND_POWER_OF_TWO(res, round_bits));
      }
    }
  } else if (subw == 1 && subh == 0) {
    for (i = 0; i < h; ++i) {
      for (j = 0; j < w; ++j) {
        int32_t res;
        const int m = AOM_BLEND_AVG(mask[i * mask_stride + (2 * j)],
                                    mask[i * mask_stride + (2 * j + 1)]);
        res = ((m * (int32_t)src0[i * src0_stride + j] +
                (AOM_BLEND_A64_MAX_ALPHA - m) *
                    (int32_t)src1[i * src1_stride + j]) >>
               AOM_BLEND_A64_ROUND_BITS);
        res -= round_offset;
        dst[i * dst_stride + j] =
            clip_pixel(ROUND_POWER_OF_TWO(res, round_bits));
      }
    }
  } else {
    for (i = 0; i < h; ++i) {
      for (j = 0; j < w; ++j) {
        int32_t res;
        const int m = AOM_BLEND_AVG(mask[(2 * i) * mask_stride + j],
                                    mask[(2 * i + 1) * mask_stride + j]);
        res = ((int32_t)(m * (int32_t)src0[i * src0_stride + j] +
                         (AOM_BLEND_A64_MAX_ALPHA - m) *
                             (int32_t)src1[i * src1_stride + j]) >>
               AOM_BLEND_A64_ROUND_BITS);
        res -= round_offset;
        dst[i * dst_stride + j] =
            clip_pixel(ROUND_POWER_OF_TWO(res, round_bits));
      }
    }
  }
}

#if CONFIG_AV1_HIGHBITDEPTH
void aom_highbd_blend_a64_d16_mask_c(
    uint8_t *dst_8, uint32_t dst_stride, const CONV_BUF_TYPE *src0,
    uint32_t src0_stride, const CONV_BUF_TYPE *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h, int subw, int subh,
    ConvolveParams *conv_params, const int bd) {
  const int offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  const int round_offset = (1 << (offset_bits - conv_params->round_1)) +
                           (1 << (offset_bits - conv_params->round_1 - 1));
  const int round_bits =
      2 * FILTER_BITS - conv_params->round_0 - conv_params->round_1;
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst_8);

  assert(IMPLIES(src0 == dst, src0_stride == dst_stride));
  assert(IMPLIES(src1 == dst, src1_stride == dst_stride));

  assert(h >= 1);
  assert(w >= 1);
  assert(IS_POWER_OF_TWO(h));
  assert(IS_POWER_OF_TWO(w));

  // excerpt from clip_pixel_highbd()
  // set saturation_value to (1 << bd) - 1
  unsigned int saturation_value;
  switch (bd) {
    case 8:
    default: saturation_value = 255; break;
    case 10: saturation_value = 1023; break;
    case 12: saturation_value = 4095; break;
  }

  if (subw == 0 && subh == 0) {
    for (int i = 0; i < h; ++i) {
      for (int j = 0; j < w; ++j) {
        int32_t res;
        const int m = mask[j];
        res = ((m * src0[j] + (AOM_BLEND_A64_MAX_ALPHA - m) * src1[j]) >>
               AOM_BLEND_A64_ROUND_BITS);
        res -= round_offset;
        unsigned int v = negative_to_zero(ROUND_POWER_OF_TWO(res, round_bits));
        dst[j] = AOMMIN(v, saturation_value);
      }
      mask += mask_stride;
      src0 += src0_stride;
      src1 += src1_stride;
      dst += dst_stride;
    }
  } else if (subw == 1 && subh == 1) {
    for (int i = 0; i < h; ++i) {
      for (int j = 0; j < w; ++j) {
        int32_t res;
        const int m = ROUND_POWER_OF_TWO(
            mask[2 * j] + mask[mask_stride + 2 * j] + mask[2 * j + 1] +
                mask[mask_stride + 2 * j + 1],
            2);
        res = (m * src0[j] + (AOM_BLEND_A64_MAX_ALPHA - m) * src1[j]) >>
              AOM_BLEND_A64_ROUND_BITS;
        res -= round_offset;
        unsigned int v = negative_to_zero(ROUND_POWER_OF_TWO(res, round_bits));
        dst[j] = AOMMIN(v, saturation_value);
      }
      mask += 2 * mask_stride;
      src0 += src0_stride;
      src1 += src1_stride;
      dst += dst_stride;
    }
  } else if (subw == 1 && subh == 0) {
    for (int i = 0; i < h; ++i) {
      for (int j = 0; j < w; ++j) {
        int32_t res;
        const int m = AOM_BLEND_AVG(mask[2 * j], mask[2 * j + 1]);
        res = (m * src0[j] + (AOM_BLEND_A64_MAX_ALPHA - m) * src1[j]) >>
              AOM_BLEND_A64_ROUND_BITS;
        res -= round_offset;
        unsigned int v = negative_to_zero(ROUND_POWER_OF_TWO(res, round_bits));
        dst[j] = AOMMIN(v, saturation_value);
      }
      mask += mask_stride;
      src0 += src0_stride;
      src1 += src1_stride;
      dst += dst_stride;
    }
  } else {
    for (int i = 0; i < h; ++i) {
      for (int j = 0; j < w; ++j) {
        int32_t res;
        const int m = AOM_BLEND_AVG(mask[j], mask[mask_stride + j]);
        res = (m * src0[j] + (AOM_BLEND_A64_MAX_ALPHA - m) * src1[j]) >>
              AOM_BLEND_A64_ROUND_BITS;
        res -= round_offset;
        unsigned int v = negative_to_zero(ROUND_POWER_OF_TWO(res, round_bits));
        dst[j] = AOMMIN(v, saturation_value);
      }
      mask += 2 * mask_stride;
      src0 += src0_stride;
      src1 += src1_stride;
      dst += dst_stride;
    }
  }
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

// Blending with alpha mask. Mask values come from the range [0, 64],
// as described for AOM_BLEND_A64 in aom_dsp/blend.h. src0 or src1 can
// be the same as dst, or dst can be different from both sources.

void aom_blend_a64_mask_c(uint8_t *dst, uint32_t dst_stride,
                          const uint8_t *src0, uint32_t src0_stride,
                          const uint8_t *src1, uint32_t src1_stride,
                          const uint8_t *mask, uint32_t mask_stride, int w,
                          int h, int subw, int subh) {
  int i, j;

  assert(IMPLIES(src0 == dst, src0_stride == dst_stride));
  assert(IMPLIES(src1 == dst, src1_stride == dst_stride));

  assert(h >= 1);
  assert(w >= 1);
  assert(IS_POWER_OF_TWO(h));
  assert(IS_POWER_OF_TWO(w));

  if (subw == 0 && subh == 0) {
    for (i = 0; i < h; ++i) {
      for (j = 0; j < w; ++j) {
        const int m = mask[i * mask_stride + j];
        dst[i * dst_stride + j] = AOM_BLEND_A64(m, src0[i * src0_stride + j],
                                                src1[i * src1_stride + j]);
      }
    }
  } else if (subw == 1 && subh == 1) {
    for (i = 0; i < h; ++i) {
      for (j = 0; j < w; ++j) {
        const int m = ROUND_POWER_OF_TWO(
            mask[(2 * i) * mask_stride + (2 * j)] +
                mask[(2 * i + 1) * mask_stride + (2 * j)] +
                mask[(2 * i) * mask_stride + (2 * j + 1)] +
                mask[(2 * i + 1) * mask_stride + (2 * j + 1)],
            2);
        dst[i * dst_stride + j] = AOM_BLEND_A64(m, src0[i * src0_stride + j],
                                                src1[i * src1_stride + j]);
      }
    }
  } else if (subw == 1 && subh == 0) {
    for (i = 0; i < h; ++i) {
      for (j = 0; j < w; ++j) {
        const int m = AOM_BLEND_AVG(mask[i * mask_stride + (2 * j)],
                                    mask[i * mask_stride + (2 * j + 1)]);
        dst[i * dst_stride + j] = AOM_BLEND_A64(m, src0[i * src0_stride + j],
                                                src1[i * src1_stride + j]);
      }
    }
  } else {
    for (i = 0; i < h; ++i) {
      for (j = 0; j < w; ++j) {
        const int m = AOM_BLEND_AVG(mask[(2 * i) * mask_stride + j],
                                    mask[(2 * i + 1) * mask_stride + j]);
        dst[i * dst_stride + j] = AOM_BLEND_A64(m, src0[i * src0_stride + j],
                                                src1[i * src1_stride + j]);
      }
    }
  }
}

#if CONFIG_AV1_HIGHBITDEPTH
void aom_highbd_blend_a64_mask_c(uint8_t *dst_8, uint32_t dst_stride,
                                 const uint8_t *src0_8, uint32_t src0_stride,
                                 const uint8_t *src1_8, uint32_t src1_stride,
                                 const uint8_t *mask, uint32_t mask_stride,
                                 int w, int h, int subw, int subh, int bd) {
  int i, j;
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst_8);
  const uint16_t *src0 = CONVERT_TO_SHORTPTR(src0_8);
  const uint16_t *src1 = CONVERT_TO_SHORTPTR(src1_8);
  (void)bd;

  assert(IMPLIES(src0 == dst, src0_stride == dst_stride));
  assert(IMPLIES(src1 == dst, src1_stride == dst_stride));

  assert(h >= 1);
  assert(w >= 1);
  assert(IS_POWER_OF_TWO(h));
  assert(IS_POWER_OF_TWO(w));

  assert(bd == 8 || bd == 10 || bd == 12);

  if (subw == 0 && subh == 0) {
    for (i = 0; i < h; ++i) {
      for (j = 0; j < w; ++j) {
        const int m = mask[i * mask_stride + j];
        dst[i * dst_stride + j] = AOM_BLEND_A64(m, src0[i * src0_stride + j],
                                                src1[i * src1_stride + j]);
      }
    }
  } else if (subw == 1 && subh == 1) {
    for (i = 0; i < h; ++i) {
      for (j = 0; j < w; ++j) {
        const int m = ROUND_POWER_OF_TWO(
            mask[(2 * i) * mask_stride + (2 * j)] +
                mask[(2 * i + 1) * mask_stride + (2 * j)] +
                mask[(2 * i) * mask_stride + (2 * j + 1)] +
                mask[(2 * i + 1) * mask_stride + (2 * j + 1)],
            2);
        dst[i * dst_stride + j] = AOM_BLEND_A64(m, src0[i * src0_stride + j],
                                                src1[i * src1_stride + j]);
      }
    }
  } else if (subw == 1 && subh == 0) {
    for (i = 0; i < h; ++i) {
      for (j = 0; j < w; ++j) {
        const int m = AOM_BLEND_AVG(mask[i * mask_stride + (2 * j)],
                                    mask[i * mask_stride + (2 * j + 1)]);
        dst[i * dst_stride + j] = AOM_BLEND_A64(m, src0[i * src0_stride + j],
                                                src1[i * src1_stride + j]);
      }
    }
  } else {
    for (i = 0; i < h; ++i) {
      for (j = 0; j < w; ++j) {
        const int m = AOM_BLEND_AVG(mask[(2 * i) * mask_stride + j],
                                    mask[(2 * i + 1) * mask_stride + j]);
        dst[i * dst_stride + j] = AOM_BLEND_A64(m, src0[i * src0_stride + j],
                                                src1[i * src1_stride + j]);
      }
    }
  }
}
#endif  // CONFIG_AV1_HIGHBITDEPTH
