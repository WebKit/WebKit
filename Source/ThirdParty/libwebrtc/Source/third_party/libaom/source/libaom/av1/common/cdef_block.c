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

#include <math.h>
#include <stdlib.h>

#include "config/aom_dsp_rtcd.h"
#include "config/av1_rtcd.h"

#include "av1/common/cdef.h"
/*
This is Cdef_Directions (section 7.15.3) with 2 padding entries at the
beginning and end of the table. The cdef direction range is [0, 7] and the
first index is offset +/-2. This removes the need to constrain the first
index to the same range using e.g., & 7.
*/
DECLARE_ALIGNED(16, const int, cdef_directions_padded[12][2]) = {
  /* Padding: cdef_directions[6] */
  { 1 * CDEF_BSTRIDE + 0, 2 * CDEF_BSTRIDE + 0 },
  /* Padding: cdef_directions[7] */
  { 1 * CDEF_BSTRIDE + 0, 2 * CDEF_BSTRIDE - 1 },

  /* Begin cdef_directions */
  { -1 * CDEF_BSTRIDE + 1, -2 * CDEF_BSTRIDE + 2 },
  { 0 * CDEF_BSTRIDE + 1, -1 * CDEF_BSTRIDE + 2 },
  { 0 * CDEF_BSTRIDE + 1, 0 * CDEF_BSTRIDE + 2 },
  { 0 * CDEF_BSTRIDE + 1, 1 * CDEF_BSTRIDE + 2 },
  { 1 * CDEF_BSTRIDE + 1, 2 * CDEF_BSTRIDE + 2 },
  { 1 * CDEF_BSTRIDE + 0, 2 * CDEF_BSTRIDE + 1 },
  { 1 * CDEF_BSTRIDE + 0, 2 * CDEF_BSTRIDE + 0 },
  { 1 * CDEF_BSTRIDE + 0, 2 * CDEF_BSTRIDE - 1 },
  /* End cdef_directions */

  /* Padding: cdef_directions[0] */
  { -1 * CDEF_BSTRIDE + 1, -2 * CDEF_BSTRIDE + 2 },
  /* Padding: cdef_directions[1] */
  { 0 * CDEF_BSTRIDE + 1, -1 * CDEF_BSTRIDE + 2 },
};

const int (*const cdef_directions)[2] = cdef_directions_padded + 2;

/* Detect direction. 0 means 45-degree up-right, 2 is horizontal, and so on.
   The search minimizes the weighted variance along all the lines in a
   particular direction, i.e. the squared error between the input and a
   "predicted" block where each pixel is replaced by the average along a line
   in a particular direction. Since each direction have the same sum(x^2) term,
   that term is never computed. See Section 2, step 2, of:
   http://jmvalin.ca/notes/intra_paint.pdf */
int cdef_find_dir_c(const uint16_t *img, int stride, int32_t *var,
                    int coeff_shift) {
  int i;
  int32_t cost[8] = { 0 };
  int partial[8][15] = { { 0 } };
  int32_t best_cost = 0;
  int best_dir = 0;
  /* Instead of dividing by n between 2 and 8, we multiply by 3*5*7*8/n.
     The output is then 840 times larger, but we don't care for finding
     the max. */
  static const int div_table[] = { 0, 840, 420, 280, 210, 168, 140, 120, 105 };
  for (i = 0; i < 8; i++) {
    int j;
    for (j = 0; j < 8; j++) {
      int x;
      /* We subtract 128 here to reduce the maximum range of the squared
         partial sums. */
      x = (img[i * stride + j] >> coeff_shift) - 128;
      partial[0][i + j] += x;
      partial[1][i + j / 2] += x;
      partial[2][i] += x;
      partial[3][3 + i - j / 2] += x;
      partial[4][7 + i - j] += x;
      partial[5][3 - i / 2 + j] += x;
      partial[6][j] += x;
      partial[7][i / 2 + j] += x;
    }
  }
  for (i = 0; i < 8; i++) {
    cost[2] += partial[2][i] * partial[2][i];
    cost[6] += partial[6][i] * partial[6][i];
  }
  cost[2] *= div_table[8];
  cost[6] *= div_table[8];
  for (i = 0; i < 7; i++) {
    cost[0] += (partial[0][i] * partial[0][i] +
                partial[0][14 - i] * partial[0][14 - i]) *
               div_table[i + 1];
    cost[4] += (partial[4][i] * partial[4][i] +
                partial[4][14 - i] * partial[4][14 - i]) *
               div_table[i + 1];
  }
  cost[0] += partial[0][7] * partial[0][7] * div_table[8];
  cost[4] += partial[4][7] * partial[4][7] * div_table[8];
  for (i = 1; i < 8; i += 2) {
    int j;
    for (j = 0; j < 4 + 1; j++) {
      cost[i] += partial[i][3 + j] * partial[i][3 + j];
    }
    cost[i] *= div_table[8];
    for (j = 0; j < 4 - 1; j++) {
      cost[i] += (partial[i][j] * partial[i][j] +
                  partial[i][10 - j] * partial[i][10 - j]) *
                 div_table[2 * j + 2];
    }
  }
  for (i = 0; i < 8; i++) {
    if (cost[i] > best_cost) {
      best_cost = cost[i];
      best_dir = i;
    }
  }
  /* Difference between the optimal variance and the variance along the
     orthogonal direction. Again, the sum(x^2) terms cancel out. */
  *var = best_cost - cost[(best_dir + 4) & 7];
  /* We'd normally divide by 840, but dividing by 1024 is close enough
     for what we're going to do with this. */
  *var >>= 10;
  return best_dir;
}

void cdef_find_dir_dual_c(const uint16_t *img1, const uint16_t *img2,
                          int stride, int32_t *var1, int32_t *var2,
                          int coeff_shift, int *out1, int *out2) {
  *out1 = cdef_find_dir_c(img1, stride, var1, coeff_shift);
  *out2 = cdef_find_dir_c(img2, stride, var2, coeff_shift);
}

const int cdef_pri_taps[2][2] = { { 4, 2 }, { 3, 3 } };
const int cdef_sec_taps[2] = { 2, 1 };

/* Smooth in the direction detected. */
static void cdef_filter_block_internal(
    uint8_t *dst8, uint16_t *dst16, int dstride, const uint16_t *in,
    int pri_strength, int sec_strength, int dir, int pri_damping,
    int sec_damping, int coeff_shift, int block_width, int block_height,
    int enable_primary, int enable_secondary) {
  const int clipping_required = (enable_primary && enable_secondary);
  int i, j, k;
  const int s = CDEF_BSTRIDE;
  const int *pri_taps = cdef_pri_taps[(pri_strength >> coeff_shift) & 1];
  const int *sec_taps = cdef_sec_taps;
  for (i = 0; i < block_height; i++) {
    for (j = 0; j < block_width; j++) {
      int16_t sum = 0;
      int16_t y;
      int16_t x = in[i * s + j];
      int max = x;
      int min = x;
      for (k = 0; k < 2; k++) {
        if (enable_primary) {
          int16_t p0 = in[i * s + j + cdef_directions[dir][k]];
          int16_t p1 = in[i * s + j - cdef_directions[dir][k]];
          sum += pri_taps[k] * constrain(p0 - x, pri_strength, pri_damping);
          sum += pri_taps[k] * constrain(p1 - x, pri_strength, pri_damping);
          if (clipping_required) {
            if (p0 != CDEF_VERY_LARGE) max = AOMMAX(p0, max);
            if (p1 != CDEF_VERY_LARGE) max = AOMMAX(p1, max);
            min = AOMMIN(p0, min);
            min = AOMMIN(p1, min);
          }
        }
        if (enable_secondary) {
          int16_t s0 = in[i * s + j + cdef_directions[dir + 2][k]];
          int16_t s1 = in[i * s + j - cdef_directions[dir + 2][k]];
          int16_t s2 = in[i * s + j + cdef_directions[dir - 2][k]];
          int16_t s3 = in[i * s + j - cdef_directions[dir - 2][k]];
          if (clipping_required) {
            if (s0 != CDEF_VERY_LARGE) max = AOMMAX(s0, max);
            if (s1 != CDEF_VERY_LARGE) max = AOMMAX(s1, max);
            if (s2 != CDEF_VERY_LARGE) max = AOMMAX(s2, max);
            if (s3 != CDEF_VERY_LARGE) max = AOMMAX(s3, max);
            min = AOMMIN(s0, min);
            min = AOMMIN(s1, min);
            min = AOMMIN(s2, min);
            min = AOMMIN(s3, min);
          }
          sum += sec_taps[k] * constrain(s0 - x, sec_strength, sec_damping);
          sum += sec_taps[k] * constrain(s1 - x, sec_strength, sec_damping);
          sum += sec_taps[k] * constrain(s2 - x, sec_strength, sec_damping);
          sum += sec_taps[k] * constrain(s3 - x, sec_strength, sec_damping);
        }
      }
      y = ((int16_t)x + ((8 + sum - (sum < 0)) >> 4));
      if (clipping_required) {
        y = clamp(y, min, max);
      }

      if (dst8)
        dst8[i * dstride + j] = (uint8_t)y;
      else
        dst16[i * dstride + j] = (uint16_t)y;
    }
  }
}

void cdef_filter_8_0_c(void *dst8, int dstride, const uint16_t *in,
                       int pri_strength, int sec_strength, int dir,
                       int pri_damping, int sec_damping, int coeff_shift,
                       int block_width, int block_height) {
  cdef_filter_block_internal((uint8_t *)dst8, NULL, dstride, in, pri_strength,
                             sec_strength, dir, pri_damping, sec_damping,
                             coeff_shift, block_width, block_height,
                             /*enable_primary=*/1, /*enable_secondary=*/1);
}

void cdef_filter_8_1_c(void *dst8, int dstride, const uint16_t *in,
                       int pri_strength, int sec_strength, int dir,
                       int pri_damping, int sec_damping, int coeff_shift,
                       int block_width, int block_height) {
  cdef_filter_block_internal((uint8_t *)dst8, NULL, dstride, in, pri_strength,
                             sec_strength, dir, pri_damping, sec_damping,
                             coeff_shift, block_width, block_height,
                             /*enable_primary=*/1, /*enable_secondary=*/0);
}

void cdef_filter_8_2_c(void *dst8, int dstride, const uint16_t *in,
                       int pri_strength, int sec_strength, int dir,
                       int pri_damping, int sec_damping, int coeff_shift,
                       int block_width, int block_height) {
  cdef_filter_block_internal((uint8_t *)dst8, NULL, dstride, in, pri_strength,
                             sec_strength, dir, pri_damping, sec_damping,
                             coeff_shift, block_width, block_height,
                             /*enable_primary=*/0, /*enable_secondary=*/1);
}

void cdef_filter_8_3_c(void *dst8, int dstride, const uint16_t *in,
                       int pri_strength, int sec_strength, int dir,
                       int pri_damping, int sec_damping, int coeff_shift,
                       int block_width, int block_height) {
  cdef_filter_block_internal((uint8_t *)dst8, NULL, dstride, in, pri_strength,
                             sec_strength, dir, pri_damping, sec_damping,
                             coeff_shift, block_width, block_height,
                             /*enable_primary=*/0, /*enable_secondary=*/0);
}

void cdef_filter_16_0_c(void *dst16, int dstride, const uint16_t *in,
                        int pri_strength, int sec_strength, int dir,
                        int pri_damping, int sec_damping, int coeff_shift,
                        int block_width, int block_height) {
  cdef_filter_block_internal(NULL, (uint16_t *)dst16, dstride, in, pri_strength,
                             sec_strength, dir, pri_damping, sec_damping,
                             coeff_shift, block_width, block_height,
                             /*enable_primary=*/1, /*enable_secondary=*/1);
}

void cdef_filter_16_1_c(void *dst16, int dstride, const uint16_t *in,
                        int pri_strength, int sec_strength, int dir,
                        int pri_damping, int sec_damping, int coeff_shift,
                        int block_width, int block_height) {
  cdef_filter_block_internal(NULL, (uint16_t *)dst16, dstride, in, pri_strength,
                             sec_strength, dir, pri_damping, sec_damping,
                             coeff_shift, block_width, block_height,
                             /*enable_primary=*/1, /*enable_secondary=*/0);
}

void cdef_filter_16_2_c(void *dst16, int dstride, const uint16_t *in,
                        int pri_strength, int sec_strength, int dir,
                        int pri_damping, int sec_damping, int coeff_shift,
                        int block_width, int block_height) {
  cdef_filter_block_internal(NULL, (uint16_t *)dst16, dstride, in, pri_strength,
                             sec_strength, dir, pri_damping, sec_damping,
                             coeff_shift, block_width, block_height,
                             /*enable_primary=*/0, /*enable_secondary=*/1);
}

void cdef_filter_16_3_c(void *dst16, int dstride, const uint16_t *in,
                        int pri_strength, int sec_strength, int dir,
                        int pri_damping, int sec_damping, int coeff_shift,
                        int block_width, int block_height) {
  cdef_filter_block_internal(NULL, (uint16_t *)dst16, dstride, in, pri_strength,
                             sec_strength, dir, pri_damping, sec_damping,
                             coeff_shift, block_width, block_height,
                             /*enable_primary=*/0, /*enable_secondary=*/0);
}

/* Compute the primary filter strength for an 8x8 block based on the
   directional variance difference. A high variance difference means
   that we have a highly directional pattern (e.g. a high contrast
   edge), so we can apply more deringing. A low variance means that we
   either have a low contrast edge, or a non-directional texture, so
   we want to be careful not to blur. */
static INLINE int adjust_strength(int strength, int32_t var) {
  const int i = var >> 6 ? AOMMIN(get_msb(var >> 6), 12) : 0;
  /* We use the variance of 8x8 blocks to adjust the strength. */
  return var ? (strength * (4 + i) + 8) >> 4 : 0;
}

static AOM_INLINE void aom_cdef_find_dir(uint16_t *in, cdef_list *dlist,
                                         int var[CDEF_NBLOCKS][CDEF_NBLOCKS],
                                         int cdef_count, int coeff_shift,
                                         int dir[CDEF_NBLOCKS][CDEF_NBLOCKS]) {
  int bi;

  // Find direction of two 8x8 blocks together.
  for (bi = 0; bi < cdef_count - 1; bi += 2) {
    const int by = dlist[bi].by;
    const int bx = dlist[bi].bx;
    const int by2 = dlist[bi + 1].by;
    const int bx2 = dlist[bi + 1].bx;
    const int pos1 = 8 * by * CDEF_BSTRIDE + 8 * bx;
    const int pos2 = 8 * by2 * CDEF_BSTRIDE + 8 * bx2;
    cdef_find_dir_dual(&in[pos1], &in[pos2], CDEF_BSTRIDE, &var[by][bx],
                       &var[by2][bx2], coeff_shift, &dir[by][bx],
                       &dir[by2][bx2]);
  }

  // Process remaining 8x8 blocks here. One 8x8 at a time.
  if (cdef_count % 2) {
    const int by = dlist[bi].by;
    const int bx = dlist[bi].bx;
    dir[by][bx] = cdef_find_dir(&in[8 * by * CDEF_BSTRIDE + 8 * bx],
                                CDEF_BSTRIDE, &var[by][bx], coeff_shift);
  }
}

void av1_cdef_filter_fb(uint8_t *dst8, uint16_t *dst16, int dstride,
                        uint16_t *in, int xdec, int ydec,
                        int dir[CDEF_NBLOCKS][CDEF_NBLOCKS], int *dirinit,
                        int var[CDEF_NBLOCKS][CDEF_NBLOCKS], int pli,
                        cdef_list *dlist, int cdef_count, int level,
                        int sec_strength, int damping, int coeff_shift) {
  int bi;
  int bx;
  int by;
  const int pri_strength = level << coeff_shift;
  sec_strength <<= coeff_shift;
  damping += coeff_shift - (pli != AOM_PLANE_Y);
  const int bw_log2 = 3 - xdec;
  const int bh_log2 = 3 - ydec;
  if (dirinit && pri_strength == 0 && sec_strength == 0) {
    // If we're here, both primary and secondary strengths are 0, and
    // we still haven't written anything to y[] yet, so we just copy
    // the input to y[]. This is necessary only for av1_cdef_search()
    // and only av1_cdef_search() sets dirinit.
    for (bi = 0; bi < cdef_count; bi++) {
      by = dlist[bi].by;
      bx = dlist[bi].bx;
      // TODO(stemidts/jmvalin): SIMD optimisations
      for (int iy = 0; iy < 1 << bh_log2; iy++) {
        memcpy(&dst16[(bi << (bw_log2 + bh_log2)) + (iy << bw_log2)],
               &in[((by << bh_log2) + iy) * CDEF_BSTRIDE + (bx << bw_log2)],
               ((size_t)1 << bw_log2) * sizeof(*dst16));
      }
    }
    return;
  }

  if (pli == 0) {
    if (!dirinit || !*dirinit) {
      aom_cdef_find_dir(in, dlist, var, cdef_count, coeff_shift, dir);
      if (dirinit) *dirinit = 1;
    }
  }
  if (pli == 1 && xdec != ydec) {
    for (bi = 0; bi < cdef_count; bi++) {
      static const int conv422[8] = { 7, 0, 2, 4, 5, 6, 6, 6 };
      static const int conv440[8] = { 1, 2, 2, 2, 3, 4, 6, 0 };
      by = dlist[bi].by;
      bx = dlist[bi].bx;
      dir[by][bx] = (xdec ? conv422 : conv440)[dir[by][bx]];
    }
  }

  if (dst8) {
    const int block_width = 8 >> xdec;
    const int block_height = 8 >> ydec;
    /*
     * strength_index == 0 : enable_primary = 1, enable_secondary = 1
     * strength_index == 1 : enable_primary = 1, enable_secondary = 0
     * strength_index == 2 : enable_primary = 0, enable_secondary = 1
     * strength_index == 3 : enable_primary = 0, enable_secondary = 0
     */
    const cdef_filter_block_func cdef_filter_fn[4] = {
      cdef_filter_8_0, cdef_filter_8_1, cdef_filter_8_2, cdef_filter_8_3
    };

    for (bi = 0; bi < cdef_count; bi++) {
      by = dlist[bi].by;
      bx = dlist[bi].bx;
      const int t =
          (pli ? pri_strength : adjust_strength(pri_strength, var[by][bx]));
      const int strength_index = (sec_strength == 0) | ((t == 0) << 1);

      cdef_filter_fn[strength_index](
          &dst8[(by << bh_log2) * dstride + (bx << bw_log2)], dstride,
          &in[(by * CDEF_BSTRIDE << bh_log2) + (bx << bw_log2)], t,
          sec_strength, pri_strength ? dir[by][bx] : 0, damping, damping,
          coeff_shift, block_width, block_height);
    }
  } else {
    const int block_width = 8 >> xdec;
    const int block_height = 8 >> ydec;
    /*
     * strength_index == 0 : enable_primary = 1, enable_secondary = 1
     * strength_index == 1 : enable_primary = 1, enable_secondary = 0
     * strength_index == 2 : enable_primary = 0, enable_secondary = 1
     * strength_index == 3 : enable_primary = 0, enable_secondary = 0
     */
    const cdef_filter_block_func cdef_filter_fn[4] = {
      cdef_filter_16_0, cdef_filter_16_1, cdef_filter_16_2, cdef_filter_16_3
    };

    for (bi = 0; bi < cdef_count; bi++) {
      by = dlist[bi].by;
      bx = dlist[bi].bx;
      const int t =
          (pli ? pri_strength : adjust_strength(pri_strength, var[by][bx]));
      const int strength_index = (sec_strength == 0) | ((t == 0) << 1);

      cdef_filter_fn[strength_index](
          &dst16[dirinit ? bi << (bw_log2 + bh_log2)
                         : (by << bh_log2) * dstride + (bx << bw_log2)],
          dirinit ? 1 << bw_log2 : dstride,
          &in[(by * CDEF_BSTRIDE << bh_log2) + (bx << bw_log2)], t,
          sec_strength, pri_strength ? dir[by][bx] : 0, damping, damping,
          coeff_shift, block_width, block_height);
    }
  }
}
