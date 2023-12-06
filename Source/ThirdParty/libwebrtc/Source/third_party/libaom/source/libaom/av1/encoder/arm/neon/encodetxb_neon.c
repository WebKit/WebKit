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

#include <arm_neon.h>
#include <assert.h>
#include <math.h>

#include "config/aom_config.h"

#include "aom_dsp/arm/mem_neon.h"
#include "av1/common/txb_common.h"
#include "av1/encoder/encodetxb.h"

void av1_txb_init_levels_neon(const tran_low_t *const coeff, const int width,
                              const int height, uint8_t *const levels) {
  const int stride = height + TX_PAD_HOR;
  memset(levels - TX_PAD_TOP * stride, 0,
         sizeof(*levels) * TX_PAD_TOP * stride);
  memset(levels + stride * width, 0,
         sizeof(*levels) * (TX_PAD_BOTTOM * stride + TX_PAD_END));

  const int32x4_t zeros = vdupq_n_s32(0);
  int i = 0;
  uint8_t *ls = levels;
  const tran_low_t *cf = coeff;
  if (height == 4) {
    do {
      const int32x4_t coeffA = vld1q_s32(cf);
      const int32x4_t coeffB = vld1q_s32(cf + height);
      const int16x8_t coeffAB =
          vcombine_s16(vqmovn_s32(coeffA), vqmovn_s32(coeffB));
      const int16x8_t absAB = vqabsq_s16(coeffAB);
      const int8x8_t absABs = vqmovn_s16(absAB);
#if AOM_ARCH_AARCH64
      const int8x16_t absAB8 =
          vcombine_s8(absABs, vreinterpret_s8_s32(vget_low_s32(zeros)));
      const uint8x16_t lsAB =
          vreinterpretq_u8_s32(vzip1q_s32(vreinterpretq_s32_s8(absAB8), zeros));
#else
      const int32x2x2_t absAB8 =
          vzip_s32(vreinterpret_s32_s8(absABs), vget_low_s32(zeros));
      const uint8x16_t lsAB =
          vreinterpretq_u8_s32(vcombine_s32(absAB8.val[0], absAB8.val[1]));
#endif
      vst1q_u8(ls, lsAB);
      ls += (stride << 1);
      cf += (height << 1);
      i += 2;
    } while (i < width);
  } else if (height == 8) {
    do {
      const int16x8_t coeffAB = load_tran_low_to_s16q(cf);
      const int16x8_t absAB = vqabsq_s16(coeffAB);
      const uint8x16_t absAB8 = vreinterpretq_u8_s8(vcombine_s8(
          vqmovn_s16(absAB), vreinterpret_s8_s32(vget_low_s32(zeros))));
      vst1q_u8(ls, absAB8);
      ls += stride;
      cf += height;
      i += 1;
    } while (i < width);
  } else {
    do {
      int j = 0;
      do {
        const int16x8_t coeffAB = load_tran_low_to_s16q(cf);
        const int16x8_t coeffCD = load_tran_low_to_s16q(cf + 8);
        const int16x8_t absAB = vqabsq_s16(coeffAB);
        const int16x8_t absCD = vqabsq_s16(coeffCD);
        const uint8x16_t absABCD = vreinterpretq_u8_s8(
            vcombine_s8(vqmovn_s16(absAB), vqmovn_s16(absCD)));
        vst1q_u8((ls + j), absABCD);
        j += 16;
        cf += 16;
      } while (j < height);
      *(int32_t *)(ls + height) = 0;
      ls += stride;
      i += 1;
    } while (i < width);
  }
}

// get_4_nz_map_contexts_2d coefficients:
static const DECLARE_ALIGNED(16, uint8_t, c_4_po_2d[2][16]) = {
  { 0, 1, 6, 6, 1, 6, 6, 21, 6, 6, 21, 21, 6, 21, 21, 21 },
  { 0, 16, 16, 16, 16, 16, 16, 16, 6, 6, 21, 21, 6, 21, 21, 21 }
};

// get_4_nz_map_contexts_hor coefficients:
/* clang-format off */
#define SIG_COEF_CONTEXTS_2D_X4_051010                        \
  (SIG_COEF_CONTEXTS_2D + ((SIG_COEF_CONTEXTS_2D + 5) << 8) + \
  ((SIG_COEF_CONTEXTS_2D + 10) << 16) + ((SIG_COEF_CONTEXTS_2D + 10) << 24))
/* clang-format on */

// get_4_nz_map_contexts_ver coefficients:
static const DECLARE_ALIGNED(16, uint8_t, c_4_po_hor[16]) = {
  SIG_COEF_CONTEXTS_2D + 0,  SIG_COEF_CONTEXTS_2D + 0,
  SIG_COEF_CONTEXTS_2D + 0,  SIG_COEF_CONTEXTS_2D + 0,
  SIG_COEF_CONTEXTS_2D + 5,  SIG_COEF_CONTEXTS_2D + 5,
  SIG_COEF_CONTEXTS_2D + 5,  SIG_COEF_CONTEXTS_2D + 5,
  SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
  SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
  SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
  SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10
};

// get_8_coeff_contexts_2d coefficients:
// if (width == 8)
static const DECLARE_ALIGNED(16, uint8_t, c_8_po_2d_8[2][16]) = {
  { 0, 1, 6, 6, 21, 21, 21, 21, 1, 6, 6, 21, 21, 21, 21, 21 },
  { 6, 6, 21, 21, 21, 21, 21, 21, 6, 21, 21, 21, 21, 21, 21, 21 }
};
// if (width < 8)
static const DECLARE_ALIGNED(16, uint8_t, c_8_po_2d_l[2][16]) = {
  { 0, 11, 6, 6, 21, 21, 21, 21, 11, 11, 6, 21, 21, 21, 21, 21 },
  { 11, 11, 21, 21, 21, 21, 21, 21, 11, 11, 21, 21, 21, 21, 21, 21 }
};

// if (width > 8)
static const DECLARE_ALIGNED(16, uint8_t, c_8_po_2d_g[2][16]) = {
  { 0, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16 },
  { 6, 6, 21, 21, 21, 21, 21, 21, 6, 21, 21, 21, 21, 21, 21, 21 }
};

// get_4_nz_map_contexts_ver coefficients:
static const DECLARE_ALIGNED(16, uint8_t, c_8_po_ver[16]) = {
  SIG_COEF_CONTEXTS_2D + 0,  SIG_COEF_CONTEXTS_2D + 5,
  SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
  SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
  SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
  SIG_COEF_CONTEXTS_2D + 0,  SIG_COEF_CONTEXTS_2D + 5,
  SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
  SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
  SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10
};

// get_16n_coeff_contexts_2d coefficients:
// real_width == real_height
static const DECLARE_ALIGNED(16, uint8_t, c_16_po_2d_e[4][16]) = {
  { 0, 1, 6, 6, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21 },
  { 1, 6, 6, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21 },
  { 6, 6, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21 },
  { 6, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21 }
};

// real_width < real_height
static const DECLARE_ALIGNED(16, uint8_t, c_16_po_2d_g[3][16]) = {
  { 0, 11, 6, 6, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21 },
  { 11, 11, 6, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21 },
  { 11, 11, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21 }
};

// real_width > real_height
static const DECLARE_ALIGNED(16, uint8_t, c_16_po_2d_l[3][16]) = {
  { 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16 },
  { 6, 6, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21 },
  { 6, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21 }
};

// get_16n_coeff_contexts_hor coefficients:
static const DECLARE_ALIGNED(16, uint8_t, c_16_po_ver[16]) = {
  SIG_COEF_CONTEXTS_2D + 0,  SIG_COEF_CONTEXTS_2D + 5,
  SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
  SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
  SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
  SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
  SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
  SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
  SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10
};

// end of coefficients declaration area

static INLINE uint8x16_t load_8bit_4x4_to_1_reg(const uint8_t *const src,
                                                const int byte_stride) {
#if AOM_ARCH_AARCH64
  uint32x4_t v_data = vld1q_u32((uint32_t *)src);
  v_data = vld1q_lane_u32((uint32_t *)(src + 1 * byte_stride), v_data, 1);
  v_data = vld1q_lane_u32((uint32_t *)(src + 2 * byte_stride), v_data, 2);
  v_data = vld1q_lane_u32((uint32_t *)(src + 3 * byte_stride), v_data, 3);

  return vreinterpretq_u8_u32(v_data);
#else
  return load_unaligned_u8q(src, byte_stride);
#endif
}

static INLINE uint8x16_t load_8bit_8x2_to_1_reg(const uint8_t *const src,
                                                const int byte_stride) {
#if AOM_ARCH_AARCH64
  uint64x2_t v_data = vld1q_u64((uint64_t *)src);
  v_data = vld1q_lane_u64((uint64_t *)(src + 1 * byte_stride), v_data, 1);

  return vreinterpretq_u8_u64(v_data);
#else
  uint8x8_t v_data_low = vld1_u8(src);
  uint8x8_t v_data_high = vld1_u8(src + byte_stride);

  return vcombine_u8(v_data_low, v_data_high);
#endif
}

static INLINE uint8x16_t load_8bit_16x1_to_1_reg(const uint8_t *const src,
                                                 const int byte_stride) {
  (void)byte_stride;
  return vld1q_u8(src);
}

static INLINE void load_levels_4x4x5(const uint8_t *const src, const int stride,
                                     const ptrdiff_t *const offsets,
                                     uint8x16_t *const level) {
  level[0] = load_8bit_4x4_to_1_reg(&src[1], stride);
  level[1] = load_8bit_4x4_to_1_reg(&src[stride], stride);
  level[2] = load_8bit_4x4_to_1_reg(&src[offsets[0]], stride);
  level[3] = load_8bit_4x4_to_1_reg(&src[offsets[1]], stride);
  level[4] = load_8bit_4x4_to_1_reg(&src[offsets[2]], stride);
}

static INLINE void load_levels_8x2x5(const uint8_t *const src, const int stride,
                                     const ptrdiff_t *const offsets,
                                     uint8x16_t *const level) {
  level[0] = load_8bit_8x2_to_1_reg(&src[1], stride);
  level[1] = load_8bit_8x2_to_1_reg(&src[stride], stride);
  level[2] = load_8bit_8x2_to_1_reg(&src[offsets[0]], stride);
  level[3] = load_8bit_8x2_to_1_reg(&src[offsets[1]], stride);
  level[4] = load_8bit_8x2_to_1_reg(&src[offsets[2]], stride);
}

static INLINE void load_levels_16x1x5(const uint8_t *const src,
                                      const int stride,
                                      const ptrdiff_t *const offsets,
                                      uint8x16_t *const level) {
  level[0] = load_8bit_16x1_to_1_reg(&src[1], stride);
  level[1] = load_8bit_16x1_to_1_reg(&src[stride], stride);
  level[2] = load_8bit_16x1_to_1_reg(&src[offsets[0]], stride);
  level[3] = load_8bit_16x1_to_1_reg(&src[offsets[1]], stride);
  level[4] = load_8bit_16x1_to_1_reg(&src[offsets[2]], stride);
}

static INLINE uint8x16_t get_coeff_contexts_kernel(uint8x16_t *const level) {
  const uint8x16_t const_3 = vdupq_n_u8(3);
  const uint8x16_t const_4 = vdupq_n_u8(4);
  uint8x16_t count;

  count = vminq_u8(level[0], const_3);
  level[1] = vminq_u8(level[1], const_3);
  level[2] = vminq_u8(level[2], const_3);
  level[3] = vminq_u8(level[3], const_3);
  level[4] = vminq_u8(level[4], const_3);
  count = vaddq_u8(count, level[1]);
  count = vaddq_u8(count, level[2]);
  count = vaddq_u8(count, level[3]);
  count = vaddq_u8(count, level[4]);

  count = vrshrq_n_u8(count, 1);
  count = vminq_u8(count, const_4);
  return count;
}

static INLINE void get_4_nz_map_contexts_2d(const uint8_t *levels,
                                            const int width,
                                            const ptrdiff_t *const offsets,
                                            uint8_t *const coeff_contexts) {
  const int stride = 4 + TX_PAD_HOR;
  const uint8x16_t pos_to_offset_large = vdupq_n_u8(21);

  uint8x16_t pos_to_offset =
      (width == 4) ? vld1q_u8(c_4_po_2d[0]) : vld1q_u8(c_4_po_2d[1]);

  uint8x16_t count;
  uint8x16_t level[5];
  uint8_t *cc = coeff_contexts;

  assert(!(width % 4));

  int col = width;
  do {
    load_levels_4x4x5(levels, stride, offsets, level);
    count = get_coeff_contexts_kernel(level);
    count = vaddq_u8(count, pos_to_offset);
    vst1q_u8(cc, count);
    pos_to_offset = pos_to_offset_large;
    levels += 4 * stride;
    cc += 16;
    col -= 4;
  } while (col);

  coeff_contexts[0] = 0;
}

static INLINE void get_4_nz_map_contexts_ver(const uint8_t *levels,
                                             const int width,
                                             const ptrdiff_t *const offsets,
                                             uint8_t *coeff_contexts) {
  const int stride = 4 + TX_PAD_HOR;

  const uint8x16_t pos_to_offset =
      vreinterpretq_u8_u32(vdupq_n_u32(SIG_COEF_CONTEXTS_2D_X4_051010));

  uint8x16_t count;
  uint8x16_t level[5];

  assert(!(width % 4));

  int col = width;
  do {
    load_levels_4x4x5(levels, stride, offsets, level);
    count = get_coeff_contexts_kernel(level);
    count = vaddq_u8(count, pos_to_offset);
    vst1q_u8(coeff_contexts, count);
    levels += 4 * stride;
    coeff_contexts += 16;
    col -= 4;
  } while (col);
}

static INLINE void get_4_nz_map_contexts_hor(const uint8_t *levels,
                                             const int width,
                                             const ptrdiff_t *const offsets,
                                             uint8_t *coeff_contexts) {
  const int stride = 4 + TX_PAD_HOR;
  const uint8x16_t pos_to_offset_large = vdupq_n_u8(SIG_COEF_CONTEXTS_2D + 10);

  uint8x16_t pos_to_offset = vld1q_u8(c_4_po_hor);

  uint8x16_t count;
  uint8x16_t level[5];

  assert(!(width % 4));

  int col = width;
  do {
    load_levels_4x4x5(levels, stride, offsets, level);
    count = get_coeff_contexts_kernel(level);
    count = vaddq_u8(count, pos_to_offset);
    vst1q_u8(coeff_contexts, count);
    pos_to_offset = pos_to_offset_large;
    levels += 4 * stride;
    coeff_contexts += 16;
    col -= 4;
  } while (col);
}

static INLINE void get_8_coeff_contexts_2d(const uint8_t *levels,
                                           const int width,
                                           const ptrdiff_t *const offsets,
                                           uint8_t *coeff_contexts) {
  const int stride = 8 + TX_PAD_HOR;
  uint8_t *cc = coeff_contexts;
  uint8x16_t count;
  uint8x16_t level[5];
  uint8x16_t pos_to_offset[3];

  assert(!(width % 2));

  if (width == 8) {
    pos_to_offset[0] = vld1q_u8(c_8_po_2d_8[0]);
    pos_to_offset[1] = vld1q_u8(c_8_po_2d_8[1]);
  } else if (width < 8) {
    pos_to_offset[0] = vld1q_u8(c_8_po_2d_l[0]);
    pos_to_offset[1] = vld1q_u8(c_8_po_2d_l[1]);
  } else {
    pos_to_offset[0] = vld1q_u8(c_8_po_2d_g[0]);
    pos_to_offset[1] = vld1q_u8(c_8_po_2d_g[1]);
  }
  pos_to_offset[2] = vdupq_n_u8(21);

  int col = width;
  do {
    load_levels_8x2x5(levels, stride, offsets, level);
    count = get_coeff_contexts_kernel(level);
    count = vaddq_u8(count, pos_to_offset[0]);
    vst1q_u8(cc, count);
    pos_to_offset[0] = pos_to_offset[1];
    pos_to_offset[1] = pos_to_offset[2];
    levels += 2 * stride;
    cc += 16;
    col -= 2;
  } while (col);

  coeff_contexts[0] = 0;
}

static INLINE void get_8_coeff_contexts_ver(const uint8_t *levels,
                                            const int width,
                                            const ptrdiff_t *const offsets,
                                            uint8_t *coeff_contexts) {
  const int stride = 8 + TX_PAD_HOR;

  const uint8x16_t pos_to_offset = vld1q_u8(c_8_po_ver);

  uint8x16_t count;
  uint8x16_t level[5];

  assert(!(width % 2));

  int col = width;
  do {
    load_levels_8x2x5(levels, stride, offsets, level);
    count = get_coeff_contexts_kernel(level);
    count = vaddq_u8(count, pos_to_offset);
    vst1q_u8(coeff_contexts, count);
    levels += 2 * stride;
    coeff_contexts += 16;
    col -= 2;
  } while (col);
}

static INLINE void get_8_coeff_contexts_hor(const uint8_t *levels,
                                            const int width,
                                            const ptrdiff_t *const offsets,
                                            uint8_t *coeff_contexts) {
  const int stride = 8 + TX_PAD_HOR;
  const uint8x16_t pos_to_offset_large = vdupq_n_u8(SIG_COEF_CONTEXTS_2D + 10);

  uint8x16_t pos_to_offset = vcombine_u8(vdup_n_u8(SIG_COEF_CONTEXTS_2D + 0),
                                         vdup_n_u8(SIG_COEF_CONTEXTS_2D + 5));

  uint8x16_t count;
  uint8x16_t level[5];

  assert(!(width % 2));

  int col = width;
  do {
    load_levels_8x2x5(levels, stride, offsets, level);
    count = get_coeff_contexts_kernel(level);
    count = vaddq_u8(count, pos_to_offset);
    vst1q_u8(coeff_contexts, count);
    pos_to_offset = pos_to_offset_large;
    levels += 2 * stride;
    coeff_contexts += 16;
    col -= 2;
  } while (col);
}

static INLINE void get_16n_coeff_contexts_2d(const uint8_t *levels,
                                             const int real_width,
                                             const int real_height,
                                             const int width, const int height,
                                             const ptrdiff_t *const offsets,
                                             uint8_t *coeff_contexts) {
  const int stride = height + TX_PAD_HOR;
  uint8_t *cc = coeff_contexts;
  int col = width;
  uint8x16_t pos_to_offset[5];
  uint8x16_t pos_to_offset_large[3];
  uint8x16_t count;
  uint8x16_t level[5];

  assert(!(height % 16));

  pos_to_offset_large[2] = vdupq_n_u8(21);
  if (real_width == real_height) {
    pos_to_offset[0] = vld1q_u8(c_16_po_2d_e[0]);
    pos_to_offset[1] = vld1q_u8(c_16_po_2d_e[1]);
    pos_to_offset[2] = vld1q_u8(c_16_po_2d_e[2]);
    pos_to_offset[3] = vld1q_u8(c_16_po_2d_e[3]);
    pos_to_offset[4] = pos_to_offset_large[0] = pos_to_offset_large[1] =
        pos_to_offset_large[2];
  } else if (real_width < real_height) {
    pos_to_offset[0] = vld1q_u8(c_16_po_2d_g[0]);
    pos_to_offset[1] = vld1q_u8(c_16_po_2d_g[1]);
    pos_to_offset[2] = pos_to_offset[3] = pos_to_offset[4] =
        vld1q_u8(c_16_po_2d_g[2]);
    pos_to_offset_large[0] = pos_to_offset_large[1] = pos_to_offset_large[2];
  } else {  // real_width > real_height
    pos_to_offset[0] = pos_to_offset[1] = vld1q_u8(c_16_po_2d_l[0]);
    pos_to_offset[2] = vld1q_u8(c_16_po_2d_l[1]);
    pos_to_offset[3] = vld1q_u8(c_16_po_2d_l[2]);
    pos_to_offset[4] = pos_to_offset_large[2];
    pos_to_offset_large[0] = pos_to_offset_large[1] = vdupq_n_u8(16);
  }

  do {
    int h = height;

    do {
      load_levels_16x1x5(levels, stride, offsets, level);
      count = get_coeff_contexts_kernel(level);
      count = vaddq_u8(count, pos_to_offset[0]);
      vst1q_u8(cc, count);
      levels += 16;
      cc += 16;
      h -= 16;
      pos_to_offset[0] = pos_to_offset_large[0];
    } while (h);

    pos_to_offset[0] = pos_to_offset[1];
    pos_to_offset[1] = pos_to_offset[2];
    pos_to_offset[2] = pos_to_offset[3];
    pos_to_offset[3] = pos_to_offset[4];
    pos_to_offset_large[0] = pos_to_offset_large[1];
    pos_to_offset_large[1] = pos_to_offset_large[2];
    levels += TX_PAD_HOR;
  } while (--col);

  coeff_contexts[0] = 0;
}

static INLINE void get_16n_coeff_contexts_ver(const uint8_t *levels,
                                              const int width, const int height,
                                              const ptrdiff_t *const offsets,
                                              uint8_t *coeff_contexts) {
  const int stride = height + TX_PAD_HOR;

  const uint8x16_t pos_to_offset_large = vdupq_n_u8(SIG_COEF_CONTEXTS_2D + 10);

  uint8x16_t count;
  uint8x16_t level[5];

  assert(!(height % 16));

  int col = width;
  do {
    uint8x16_t pos_to_offset = vld1q_u8(c_16_po_ver);

    int h = height;
    do {
      load_levels_16x1x5(levels, stride, offsets, level);
      count = get_coeff_contexts_kernel(level);
      count = vaddq_u8(count, pos_to_offset);
      vst1q_u8(coeff_contexts, count);
      pos_to_offset = pos_to_offset_large;
      levels += 16;
      coeff_contexts += 16;
      h -= 16;
    } while (h);

    levels += TX_PAD_HOR;
  } while (--col);
}

static INLINE void get_16n_coeff_contexts_hor(const uint8_t *levels,
                                              const int width, const int height,
                                              const ptrdiff_t *const offsets,
                                              uint8_t *coeff_contexts) {
  const int stride = height + TX_PAD_HOR;

  uint8x16_t pos_to_offset[3];
  uint8x16_t count;
  uint8x16_t level[5];

  assert(!(height % 16));

  pos_to_offset[0] = vdupq_n_u8(SIG_COEF_CONTEXTS_2D + 0);
  pos_to_offset[1] = vdupq_n_u8(SIG_COEF_CONTEXTS_2D + 5);
  pos_to_offset[2] = vdupq_n_u8(SIG_COEF_CONTEXTS_2D + 10);

  int col = width;
  do {
    int h = height;
    do {
      load_levels_16x1x5(levels, stride, offsets, level);
      count = get_coeff_contexts_kernel(level);
      count = vaddq_u8(count, pos_to_offset[0]);
      vst1q_u8(coeff_contexts, count);
      levels += 16;
      coeff_contexts += 16;
      h -= 16;
    } while (h);

    pos_to_offset[0] = pos_to_offset[1];
    pos_to_offset[1] = pos_to_offset[2];
    levels += TX_PAD_HOR;
  } while (--col);
}

// Note: levels[] must be in the range [0, 127], inclusive.
void av1_get_nz_map_contexts_neon(const uint8_t *const levels,
                                  const int16_t *const scan, const uint16_t eob,
                                  const TX_SIZE tx_size,
                                  const TX_CLASS tx_class,
                                  int8_t *const coeff_contexts) {
  const int last_idx = eob - 1;
  if (!last_idx) {
    coeff_contexts[0] = 0;
    return;
  }

  uint8_t *const coefficients = (uint8_t *const)coeff_contexts;

  const int real_width = tx_size_wide[tx_size];
  const int real_height = tx_size_high[tx_size];
  const int width = get_txb_wide(tx_size);
  const int height = get_txb_high(tx_size);
  const int stride = height + TX_PAD_HOR;
  ptrdiff_t offsets[3];

  /* coeff_contexts must be 16 byte aligned. */
  assert(!((intptr_t)coeff_contexts & 0xf));

  if (tx_class == TX_CLASS_2D) {
    offsets[0] = 0 * stride + 2;
    offsets[1] = 1 * stride + 1;
    offsets[2] = 2 * stride + 0;

    if (height == 4) {
      get_4_nz_map_contexts_2d(levels, width, offsets, coefficients);
    } else if (height == 8) {
      get_8_coeff_contexts_2d(levels, width, offsets, coefficients);
    } else {
      get_16n_coeff_contexts_2d(levels, real_width, real_height, width, height,
                                offsets, coefficients);
    }
  } else if (tx_class == TX_CLASS_HORIZ) {
    offsets[0] = 2 * stride;
    offsets[1] = 3 * stride;
    offsets[2] = 4 * stride;
    if (height == 4) {
      get_4_nz_map_contexts_hor(levels, width, offsets, coefficients);
    } else if (height == 8) {
      get_8_coeff_contexts_hor(levels, width, offsets, coefficients);
    } else {
      get_16n_coeff_contexts_hor(levels, width, height, offsets, coefficients);
    }
  } else {  // TX_CLASS_VERT
    offsets[0] = 2;
    offsets[1] = 3;
    offsets[2] = 4;
    if (height == 4) {
      get_4_nz_map_contexts_ver(levels, width, offsets, coefficients);
    } else if (height == 8) {
      get_8_coeff_contexts_ver(levels, width, offsets, coefficients);
    } else {
      get_16n_coeff_contexts_ver(levels, width, height, offsets, coefficients);
    }
  }

  const int bhl = get_txb_bhl(tx_size);
  const int pos = scan[last_idx];
  if (last_idx <= (width << bhl) / 8)
    coeff_contexts[pos] = 1;
  else if (last_idx <= (width << bhl) / 4)
    coeff_contexts[pos] = 2;
  else
    coeff_contexts[pos] = 3;
}
