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

#include "aom_dsp/aom_simd.h"
#define SIMD_FUNC(name) name##_ssse3
#include "av1/common/cdef_block_simd.h"

void cdef_find_dir_dual_ssse3(const uint16_t *img1, const uint16_t *img2,
                              int stride, int32_t *var_out_1st,
                              int32_t *var_out_2nd, int coeff_shift,
                              int *out_dir_1st_8x8, int *out_dir_2nd_8x8) {
  // Process first 8x8.
  *out_dir_1st_8x8 = cdef_find_dir(img1, stride, var_out_1st, coeff_shift);

  // Process second 8x8.
  *out_dir_2nd_8x8 = cdef_find_dir(img2, stride, var_out_2nd, coeff_shift);
}

void cdef_copy_rect8_8bit_to_16bit_ssse3(uint16_t *dst, int dstride,
                                         const uint8_t *src, int sstride,
                                         int width, int height) {
  int j;
  for (int i = 0; i < height; i++) {
    for (j = 0; j < (width & ~0x7); j += 8) {
      v64 row = v64_load_unaligned(&src[i * sstride + j]);
      v128_store_unaligned(&dst[i * dstride + j], v128_unpack_u8_s16(row));
    }
    for (; j < width; j++) {
      dst[i * dstride + j] = src[i * sstride + j];
    }
  }
}
