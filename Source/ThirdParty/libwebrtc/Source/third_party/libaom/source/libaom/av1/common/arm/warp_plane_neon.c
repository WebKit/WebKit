/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>
#include <arm_neon.h>
#include <memory.h>
#include <math.h>

#include "aom_dsp/aom_dsp_common.h"
#include "aom_ports/mem.h"
#include "config/av1_rtcd.h"
#include "av1/common/warped_motion.h"
#include "av1/common/scale.h"

/* This is a modified version of 'av1_warped_filter' from warped_motion.c:
   * Each coefficient is stored in 8 bits instead of 16 bits
   * The coefficients are rearranged in the column order 0, 2, 4, 6, 1, 3, 5, 7

     This is done in order to avoid overflow: Since the tap with the largest
     coefficient could be any of taps 2, 3, 4 or 5, we can't use the summation
     order ((0 + 1) + (4 + 5)) + ((2 + 3) + (6 + 7)) used in the regular
     convolve functions.

     Instead, we use the summation order
     ((0 + 2) + (4 + 6)) + ((1 + 3) + (5 + 7)).
     The rearrangement of coefficients in this table is so that we can get the
     coefficients into the correct order more quickly.
*/
/* clang-format off */
DECLARE_ALIGNED(8, static const int8_t,
                filter_8bit_neon[WARPEDPIXEL_PREC_SHIFTS * 3 + 1][8]) = {
#if WARPEDPIXEL_PREC_BITS == 6
  // [-1, 0)
  { 0, 127,   0, 0,   0,   1, 0, 0}, { 0, 127,   0, 0,  -1,   2, 0, 0},
  { 1, 127,  -1, 0,  -3,   4, 0, 0}, { 1, 126,  -2, 0,  -4,   6, 1, 0},
  { 1, 126,  -3, 0,  -5,   8, 1, 0}, { 1, 125,  -4, 0,  -6,  11, 1, 0},
  { 1, 124,  -4, 0,  -7,  13, 1, 0}, { 2, 123,  -5, 0,  -8,  15, 1, 0},
  { 2, 122,  -6, 0,  -9,  18, 1, 0}, { 2, 121,  -6, 0, -10,  20, 1, 0},
  { 2, 120,  -7, 0, -11,  22, 2, 0}, { 2, 119,  -8, 0, -12,  25, 2, 0},
  { 3, 117,  -8, 0, -13,  27, 2, 0}, { 3, 116,  -9, 0, -13,  29, 2, 0},
  { 3, 114, -10, 0, -14,  32, 3, 0}, { 3, 113, -10, 0, -15,  35, 2, 0},
  { 3, 111, -11, 0, -15,  37, 3, 0}, { 3, 109, -11, 0, -16,  40, 3, 0},
  { 3, 108, -12, 0, -16,  42, 3, 0}, { 4, 106, -13, 0, -17,  45, 3, 0},
  { 4, 104, -13, 0, -17,  47, 3, 0}, { 4, 102, -14, 0, -17,  50, 3, 0},
  { 4, 100, -14, 0, -17,  52, 3, 0}, { 4,  98, -15, 0, -18,  55, 4, 0},
  { 4,  96, -15, 0, -18,  58, 3, 0}, { 4,  94, -16, 0, -18,  60, 4, 0},
  { 4,  91, -16, 0, -18,  63, 4, 0}, { 4,  89, -16, 0, -18,  65, 4, 0},
  { 4,  87, -17, 0, -18,  68, 4, 0}, { 4,  85, -17, 0, -18,  70, 4, 0},
  { 4,  82, -17, 0, -18,  73, 4, 0}, { 4,  80, -17, 0, -18,  75, 4, 0},
  { 4,  78, -18, 0, -18,  78, 4, 0}, { 4,  75, -18, 0, -17,  80, 4, 0},
  { 4,  73, -18, 0, -17,  82, 4, 0}, { 4,  70, -18, 0, -17,  85, 4, 0},
  { 4,  68, -18, 0, -17,  87, 4, 0}, { 4,  65, -18, 0, -16,  89, 4, 0},
  { 4,  63, -18, 0, -16,  91, 4, 0}, { 4,  60, -18, 0, -16,  94, 4, 0},
  { 3,  58, -18, 0, -15,  96, 4, 0}, { 4,  55, -18, 0, -15,  98, 4, 0},
  { 3,  52, -17, 0, -14, 100, 4, 0}, { 3,  50, -17, 0, -14, 102, 4, 0},
  { 3,  47, -17, 0, -13, 104, 4, 0}, { 3,  45, -17, 0, -13, 106, 4, 0},
  { 3,  42, -16, 0, -12, 108, 3, 0}, { 3,  40, -16, 0, -11, 109, 3, 0},
  { 3,  37, -15, 0, -11, 111, 3, 0}, { 2,  35, -15, 0, -10, 113, 3, 0},
  { 3,  32, -14, 0, -10, 114, 3, 0}, { 2,  29, -13, 0,  -9, 116, 3, 0},
  { 2,  27, -13, 0,  -8, 117, 3, 0}, { 2,  25, -12, 0,  -8, 119, 2, 0},
  { 2,  22, -11, 0,  -7, 120, 2, 0}, { 1,  20, -10, 0,  -6, 121, 2, 0},
  { 1,  18,  -9, 0,  -6, 122, 2, 0}, { 1,  15,  -8, 0,  -5, 123, 2, 0},
  { 1,  13,  -7, 0,  -4, 124, 1, 0}, { 1,  11,  -6, 0,  -4, 125, 1, 0},
  { 1,   8,  -5, 0,  -3, 126, 1, 0}, { 1,   6,  -4, 0,  -2, 126, 1, 0},
  { 0,   4,  -3, 0,  -1, 127, 1, 0}, { 0,   2,  -1, 0,   0, 127, 0, 0},
  // [0, 1)
  { 0,   0,   1, 0, 0, 127,   0,  0}, { 0,  -1,   2, 0, 0, 127,   0,  0},
  { 0,  -3,   4, 1, 1, 127,  -2,  0}, { 0,  -5,   6, 1, 1, 127,  -2,  0},
  { 0,  -6,   8, 1, 2, 126,  -3,  0}, {-1,  -7,  11, 2, 2, 126,  -4, -1},
  {-1,  -8,  13, 2, 3, 125,  -5, -1}, {-1, -10,  16, 3, 3, 124,  -6, -1},
  {-1, -11,  18, 3, 4, 123,  -7, -1}, {-1, -12,  20, 3, 4, 122,  -7, -1},
  {-1, -13,  23, 3, 4, 121,  -8, -1}, {-2, -14,  25, 4, 5, 120,  -9, -1},
  {-1, -15,  27, 4, 5, 119, -10, -1}, {-1, -16,  30, 4, 5, 118, -11, -1},
  {-2, -17,  33, 5, 6, 116, -12, -1}, {-2, -17,  35, 5, 6, 114, -12, -1},
  {-2, -18,  38, 5, 6, 113, -13, -1}, {-2, -19,  41, 6, 7, 111, -14, -2},
  {-2, -19,  43, 6, 7, 110, -15, -2}, {-2, -20,  46, 6, 7, 108, -15, -2},
  {-2, -20,  49, 6, 7, 106, -16, -2}, {-2, -21,  51, 7, 7, 104, -16, -2},
  {-2, -21,  54, 7, 7, 102, -17, -2}, {-2, -21,  56, 7, 8, 100, -18, -2},
  {-2, -22,  59, 7, 8,  98, -18, -2}, {-2, -22,  62, 7, 8,  96, -19, -2},
  {-2, -22,  64, 7, 8,  94, -19, -2}, {-2, -22,  67, 8, 8,  91, -20, -2},
  {-2, -22,  69, 8, 8,  89, -20, -2}, {-2, -22,  72, 8, 8,  87, -21, -2},
  {-2, -21,  74, 8, 8,  84, -21, -2}, {-2, -22,  77, 8, 8,  82, -21, -2},
  {-2, -21,  79, 8, 8,  79, -21, -2}, {-2, -21,  82, 8, 8,  77, -22, -2},
  {-2, -21,  84, 8, 8,  74, -21, -2}, {-2, -21,  87, 8, 8,  72, -22, -2},
  {-2, -20,  89, 8, 8,  69, -22, -2}, {-2, -20,  91, 8, 8,  67, -22, -2},
  {-2, -19,  94, 8, 7,  64, -22, -2}, {-2, -19,  96, 8, 7,  62, -22, -2},
  {-2, -18,  98, 8, 7,  59, -22, -2}, {-2, -18, 100, 8, 7,  56, -21, -2},
  {-2, -17, 102, 7, 7,  54, -21, -2}, {-2, -16, 104, 7, 7,  51, -21, -2},
  {-2, -16, 106, 7, 6,  49, -20, -2}, {-2, -15, 108, 7, 6,  46, -20, -2},
  {-2, -15, 110, 7, 6,  43, -19, -2}, {-2, -14, 111, 7, 6,  41, -19, -2},
  {-1, -13, 113, 6, 5,  38, -18, -2}, {-1, -12, 114, 6, 5,  35, -17, -2},
  {-1, -12, 116, 6, 5,  33, -17, -2}, {-1, -11, 118, 5, 4,  30, -16, -1},
  {-1, -10, 119, 5, 4,  27, -15, -1}, {-1,  -9, 120, 5, 4,  25, -14, -2},
  {-1,  -8, 121, 4, 3,  23, -13, -1}, {-1,  -7, 122, 4, 3,  20, -12, -1},
  {-1,  -7, 123, 4, 3,  18, -11, -1}, {-1,  -6, 124, 3, 3,  16, -10, -1},
  {-1,  -5, 125, 3, 2,  13,  -8, -1}, {-1,  -4, 126, 2, 2,  11,  -7, -1},
  { 0,  -3, 126, 2, 1,   8,  -6,  0}, { 0,  -2, 127, 1, 1,   6,  -5,  0},
  { 0,  -2, 127, 1, 1,   4,  -3,  0}, { 0,   0, 127, 0, 0,   2,  -1,  0},
  // [1, 2)
  { 0, 0, 127,   0, 0,   1,   0, 0}, { 0, 0, 127,   0, 0,  -1,   2, 0},
  { 0, 1, 127,  -1, 0,  -3,   4, 0}, { 0, 1, 126,  -2, 0,  -4,   6, 1},
  { 0, 1, 126,  -3, 0,  -5,   8, 1}, { 0, 1, 125,  -4, 0,  -6,  11, 1},
  { 0, 1, 124,  -4, 0,  -7,  13, 1}, { 0, 2, 123,  -5, 0,  -8,  15, 1},
  { 0, 2, 122,  -6, 0,  -9,  18, 1}, { 0, 2, 121,  -6, 0, -10,  20, 1},
  { 0, 2, 120,  -7, 0, -11,  22, 2}, { 0, 2, 119,  -8, 0, -12,  25, 2},
  { 0, 3, 117,  -8, 0, -13,  27, 2}, { 0, 3, 116,  -9, 0, -13,  29, 2},
  { 0, 3, 114, -10, 0, -14,  32, 3}, { 0, 3, 113, -10, 0, -15,  35, 2},
  { 0, 3, 111, -11, 0, -15,  37, 3}, { 0, 3, 109, -11, 0, -16,  40, 3},
  { 0, 3, 108, -12, 0, -16,  42, 3}, { 0, 4, 106, -13, 0, -17,  45, 3},
  { 0, 4, 104, -13, 0, -17,  47, 3}, { 0, 4, 102, -14, 0, -17,  50, 3},
  { 0, 4, 100, -14, 0, -17,  52, 3}, { 0, 4,  98, -15, 0, -18,  55, 4},
  { 0, 4,  96, -15, 0, -18,  58, 3}, { 0, 4,  94, -16, 0, -18,  60, 4},
  { 0, 4,  91, -16, 0, -18,  63, 4}, { 0, 4,  89, -16, 0, -18,  65, 4},
  { 0, 4,  87, -17, 0, -18,  68, 4}, { 0, 4,  85, -17, 0, -18,  70, 4},
  { 0, 4,  82, -17, 0, -18,  73, 4}, { 0, 4,  80, -17, 0, -18,  75, 4},
  { 0, 4,  78, -18, 0, -18,  78, 4}, { 0, 4,  75, -18, 0, -17,  80, 4},
  { 0, 4,  73, -18, 0, -17,  82, 4}, { 0, 4,  70, -18, 0, -17,  85, 4},
  { 0, 4,  68, -18, 0, -17,  87, 4}, { 0, 4,  65, -18, 0, -16,  89, 4},
  { 0, 4,  63, -18, 0, -16,  91, 4}, { 0, 4,  60, -18, 0, -16,  94, 4},
  { 0, 3,  58, -18, 0, -15,  96, 4}, { 0, 4,  55, -18, 0, -15,  98, 4},
  { 0, 3,  52, -17, 0, -14, 100, 4}, { 0, 3,  50, -17, 0, -14, 102, 4},
  { 0, 3,  47, -17, 0, -13, 104, 4}, { 0, 3,  45, -17, 0, -13, 106, 4},
  { 0, 3,  42, -16, 0, -12, 108, 3}, { 0, 3,  40, -16, 0, -11, 109, 3},
  { 0, 3,  37, -15, 0, -11, 111, 3}, { 0, 2,  35, -15, 0, -10, 113, 3},
  { 0, 3,  32, -14, 0, -10, 114, 3}, { 0, 2,  29, -13, 0,  -9, 116, 3},
  { 0, 2,  27, -13, 0,  -8, 117, 3}, { 0, 2,  25, -12, 0,  -8, 119, 2},
  { 0, 2,  22, -11, 0,  -7, 120, 2}, { 0, 1,  20, -10, 0,  -6, 121, 2},
  { 0, 1,  18,  -9, 0,  -6, 122, 2}, { 0, 1,  15,  -8, 0,  -5, 123, 2},
  { 0, 1,  13,  -7, 0,  -4, 124, 1}, { 0, 1,  11,  -6, 0,  -4, 125, 1},
  { 0, 1,   8,  -5, 0,  -3, 126, 1}, { 0, 1,   6,  -4, 0,  -2, 126, 1},
  { 0, 0,   4,  -3, 0,  -1, 127, 1}, { 0, 0,   2,  -1, 0,   0, 127, 0},
  // dummy (replicate row index 191)
  { 0, 0,   2,  -1, 0,   0, 127, 0},

#else
  // [-1, 0)
  { 0, 127,   0, 0,   0,   1, 0, 0}, { 1, 127,  -1, 0,  -3,   4, 0, 0},
  { 1, 126,  -3, 0,  -5,   8, 1, 0}, { 1, 124,  -4, 0,  -7,  13, 1, 0},
  { 2, 122,  -6, 0,  -9,  18, 1, 0}, { 2, 120,  -7, 0, -11,  22, 2, 0},
  { 3, 117,  -8, 0, -13,  27, 2, 0}, { 3, 114, -10, 0, -14,  32, 3, 0},
  { 3, 111, -11, 0, -15,  37, 3, 0}, { 3, 108, -12, 0, -16,  42, 3, 0},
  { 4, 104, -13, 0, -17,  47, 3, 0}, { 4, 100, -14, 0, -17,  52, 3, 0},
  { 4,  96, -15, 0, -18,  58, 3, 0}, { 4,  91, -16, 0, -18,  63, 4, 0},
  { 4,  87, -17, 0, -18,  68, 4, 0}, { 4,  82, -17, 0, -18,  73, 4, 0},
  { 4,  78, -18, 0, -18,  78, 4, 0}, { 4,  73, -18, 0, -17,  82, 4, 0},
  { 4,  68, -18, 0, -17,  87, 4, 0}, { 4,  63, -18, 0, -16,  91, 4, 0},
  { 3,  58, -18, 0, -15,  96, 4, 0}, { 3,  52, -17, 0, -14, 100, 4, 0},
  { 3,  47, -17, 0, -13, 104, 4, 0}, { 3,  42, -16, 0, -12, 108, 3, 0},
  { 3,  37, -15, 0, -11, 111, 3, 0}, { 3,  32, -14, 0, -10, 114, 3, 0},
  { 2,  27, -13, 0,  -8, 117, 3, 0}, { 2,  22, -11, 0,  -7, 120, 2, 0},
  { 1,  18,  -9, 0,  -6, 122, 2, 0}, { 1,  13,  -7, 0,  -4, 124, 1, 0},
  { 1,   8,  -5, 0,  -3, 126, 1, 0}, { 0,   4,  -3, 0,  -1, 127, 1, 0},
  // [0, 1)
  { 0,   0,   1, 0, 0, 127,   0,  0}, { 0,  -3,   4, 1, 1, 127,  -2,  0},
  { 0,  -6,   8, 1, 2, 126,  -3,  0}, {-1,  -8,  13, 2, 3, 125,  -5, -1},
  {-1, -11,  18, 3, 4, 123,  -7, -1}, {-1, -13,  23, 3, 4, 121,  -8, -1},
  {-1, -15,  27, 4, 5, 119, -10, -1}, {-2, -17,  33, 5, 6, 116, -12, -1},
  {-2, -18,  38, 5, 6, 113, -13, -1}, {-2, -19,  43, 6, 7, 110, -15, -2},
  {-2, -20,  49, 6, 7, 106, -16, -2}, {-2, -21,  54, 7, 7, 102, -17, -2},
  {-2, -22,  59, 7, 8,  98, -18, -2}, {-2, -22,  64, 7, 8,  94, -19, -2},
  {-2, -22,  69, 8, 8,  89, -20, -2}, {-2, -21,  74, 8, 8,  84, -21, -2},
  {-2, -21,  79, 8, 8,  79, -21, -2}, {-2, -21,  84, 8, 8,  74, -21, -2},
  {-2, -20,  89, 8, 8,  69, -22, -2}, {-2, -19,  94, 8, 7,  64, -22, -2},
  {-2, -18,  98, 8, 7,  59, -22, -2}, {-2, -17, 102, 7, 7,  54, -21, -2},
  {-2, -16, 106, 7, 6,  49, -20, -2}, {-2, -15, 110, 7, 6,  43, -19, -2},
  {-1, -13, 113, 6, 5,  38, -18, -2}, {-1, -12, 116, 6, 5,  33, -17, -2},
  {-1, -10, 119, 5, 4,  27, -15, -1}, {-1,  -8, 121, 4, 3,  23, -13, -1},
  {-1,  -7, 123, 4, 3,  18, -11, -1}, {-1,  -5, 125, 3, 2,  13,  -8, -1},
  { 0,  -3, 126, 2, 1,   8,  -6,  0}, { 0,  -2, 127, 1, 1,   4,  -3,  0},
  // [1, 2)
  { 0,  0, 127,   0, 0,   1,   0, 0}, { 0, 1, 127,  -1, 0,  -3,   4, 0},
  { 0,  1, 126,  -3, 0,  -5,   8, 1}, { 0, 1, 124,  -4, 0,  -7,  13, 1},
  { 0,  2, 122,  -6, 0,  -9,  18, 1}, { 0, 2, 120,  -7, 0, -11,  22, 2},
  { 0,  3, 117,  -8, 0, -13,  27, 2}, { 0, 3, 114, -10, 0, -14,  32, 3},
  { 0,  3, 111, -11, 0, -15,  37, 3}, { 0, 3, 108, -12, 0, -16,  42, 3},
  { 0,  4, 104, -13, 0, -17,  47, 3}, { 0, 4, 100, -14, 0, -17,  52, 3},
  { 0,  4,  96, -15, 0, -18,  58, 3}, { 0, 4,  91, -16, 0, -18,  63, 4},
  { 0,  4,  87, -17, 0, -18,  68, 4}, { 0, 4,  82, -17, 0, -18,  73, 4},
  { 0,  4,  78, -18, 0, -18,  78, 4}, { 0, 4,  73, -18, 0, -17,  82, 4},
  { 0,  4,  68, -18, 0, -17,  87, 4}, { 0, 4,  63, -18, 0, -16,  91, 4},
  { 0,  3,  58, -18, 0, -15,  96, 4}, { 0, 3,  52, -17, 0, -14, 100, 4},
  { 0,  3,  47, -17, 0, -13, 104, 4}, { 0, 3,  42, -16, 0, -12, 108, 3},
  { 0,  3,  37, -15, 0, -11, 111, 3}, { 0, 3,  32, -14, 0, -10, 114, 3},
  { 0,  2,  27, -13, 0,  -8, 117, 3}, { 0, 2,  22, -11, 0,  -7, 120, 2},
  { 0,  1,  18,  -9, 0,  -6, 122, 2}, { 0, 1,  13,  -7, 0,  -4, 124, 1},
  { 0,  1,   8,  -5, 0,  -3, 126, 1}, { 0, 0,   4,  -3, 0,  -1, 127, 1},
  // dummy (replicate row index 95)
  { 0, 0,   4,  -3, 0,  -1, 127, 1},
#endif  // WARPEDPIXEL_PREC_BITS == 6
};
/* clang-format on */

static INLINE void convolve(int32x2x2_t x0, int32x2x2_t x1, uint8x8_t src_0,
                            uint8x8_t src_1, int16x4_t *res) {
  int16x8_t coeff_0, coeff_1;
  int16x8_t pix_0, pix_1;

  coeff_0 = vcombine_s16(vreinterpret_s16_s32(x0.val[0]),
                         vreinterpret_s16_s32(x1.val[0]));
  coeff_1 = vcombine_s16(vreinterpret_s16_s32(x0.val[1]),
                         vreinterpret_s16_s32(x1.val[1]));

  pix_0 = vreinterpretq_s16_u16(vmovl_u8(src_0));
  pix_0 = vmulq_s16(coeff_0, pix_0);

  pix_1 = vreinterpretq_s16_u16(vmovl_u8(src_1));
  pix_0 = vmlaq_s16(pix_0, coeff_1, pix_1);

  *res = vpadd_s16(vget_low_s16(pix_0), vget_high_s16(pix_0));
}

static INLINE void horizontal_filter_neon(uint8x16_t src_1, uint8x16_t src_2,
                                          uint8x16_t src_3, uint8x16_t src_4,
                                          int16x8_t *tmp_dst, int sx, int alpha,
                                          int k, const int offset_bits_horiz,
                                          const int reduce_bits_horiz) {
  const uint8x16_t mask = vreinterpretq_u8_u16(vdupq_n_u16(0x00ff));
  const int32x4_t add_const = vdupq_n_s32((int32_t)(1 << offset_bits_horiz));
  const int16x8_t shift = vdupq_n_s16(-(int16_t)reduce_bits_horiz);

  int16x8_t f0, f1, f2, f3, f4, f5, f6, f7;
  int32x2x2_t b0, b1;
  uint8x8_t src_1_low, src_2_low, src_3_low, src_4_low, src_5_low, src_6_low;
  int32x4_t tmp_res_low, tmp_res_high;
  uint16x8_t res;
  int16x4_t res_0246_even, res_0246_odd, res_1357_even, res_1357_odd;

  uint8x16_t tmp_0 = vandq_u8(src_1, mask);
  uint8x16_t tmp_1 = vandq_u8(src_2, mask);
  uint8x16_t tmp_2 = vandq_u8(src_3, mask);
  uint8x16_t tmp_3 = vandq_u8(src_4, mask);

  tmp_2 = vextq_u8(tmp_0, tmp_0, 1);
  tmp_3 = vextq_u8(tmp_1, tmp_1, 1);

  src_1 = vaddq_u8(tmp_0, tmp_2);
  src_2 = vaddq_u8(tmp_1, tmp_3);

  src_1_low = vget_low_u8(src_1);
  src_2_low = vget_low_u8(src_2);
  src_3_low = vget_low_u8(vextq_u8(src_1, src_1, 4));
  src_4_low = vget_low_u8(vextq_u8(src_2, src_2, 4));
  src_5_low = vget_low_u8(vextq_u8(src_1, src_1, 2));
  src_6_low = vget_low_u8(vextq_u8(src_1, src_1, 6));

  // Loading the 8 filter taps
  f0 = vmovl_s8(
      vld1_s8(filter_8bit_neon[(sx + 0 * alpha) >> WARPEDDIFF_PREC_BITS]));
  f1 = vmovl_s8(
      vld1_s8(filter_8bit_neon[(sx + 1 * alpha) >> WARPEDDIFF_PREC_BITS]));
  f2 = vmovl_s8(
      vld1_s8(filter_8bit_neon[(sx + 2 * alpha) >> WARPEDDIFF_PREC_BITS]));
  f3 = vmovl_s8(
      vld1_s8(filter_8bit_neon[(sx + 3 * alpha) >> WARPEDDIFF_PREC_BITS]));
  f4 = vmovl_s8(
      vld1_s8(filter_8bit_neon[(sx + 4 * alpha) >> WARPEDDIFF_PREC_BITS]));
  f5 = vmovl_s8(
      vld1_s8(filter_8bit_neon[(sx + 5 * alpha) >> WARPEDDIFF_PREC_BITS]));
  f6 = vmovl_s8(
      vld1_s8(filter_8bit_neon[(sx + 6 * alpha) >> WARPEDDIFF_PREC_BITS]));
  f7 = vmovl_s8(
      vld1_s8(filter_8bit_neon[(sx + 7 * alpha) >> WARPEDDIFF_PREC_BITS]));

  b0 = vtrn_s32(vreinterpret_s32_s16(vget_low_s16(f0)),
                vreinterpret_s32_s16(vget_low_s16(f2)));
  b1 = vtrn_s32(vreinterpret_s32_s16(vget_low_s16(f4)),
                vreinterpret_s32_s16(vget_low_s16(f6)));
  convolve(b0, b1, src_1_low, src_3_low, &res_0246_even);

  b0 = vtrn_s32(vreinterpret_s32_s16(vget_low_s16(f1)),
                vreinterpret_s32_s16(vget_low_s16(f3)));
  b1 = vtrn_s32(vreinterpret_s32_s16(vget_low_s16(f5)),
                vreinterpret_s32_s16(vget_low_s16(f7)));
  convolve(b0, b1, src_2_low, src_4_low, &res_0246_odd);

  b0 = vtrn_s32(vreinterpret_s32_s16(vget_high_s16(f0)),
                vreinterpret_s32_s16(vget_high_s16(f2)));
  b1 = vtrn_s32(vreinterpret_s32_s16(vget_high_s16(f4)),
                vreinterpret_s32_s16(vget_high_s16(f6)));
  convolve(b0, b1, src_2_low, src_4_low, &res_1357_even);

  b0 = vtrn_s32(vreinterpret_s32_s16(vget_high_s16(f1)),
                vreinterpret_s32_s16(vget_high_s16(f3)));
  b1 = vtrn_s32(vreinterpret_s32_s16(vget_high_s16(f5)),
                vreinterpret_s32_s16(vget_high_s16(f7)));
  convolve(b0, b1, src_5_low, src_6_low, &res_1357_odd);

  tmp_res_low = vaddl_s16(res_0246_even, res_1357_even);
  tmp_res_high = vaddl_s16(res_0246_odd, res_1357_odd);

  tmp_res_low = vaddq_s32(tmp_res_low, add_const);
  tmp_res_high = vaddq_s32(tmp_res_high, add_const);

  res = vcombine_u16(vqmovun_s32(tmp_res_low), vqmovun_s32(tmp_res_high));
  res = vqrshlq_u16(res, shift);

  tmp_dst[k + 7] = vreinterpretq_s16_u16(res);
}

static INLINE void vertical_filter_neon(const int16x8_t *src,
                                        int32x4_t *res_low, int32x4_t *res_high,
                                        int sy, int gamma) {
  int16x4_t src_0, src_1, fltr_0, fltr_1;
  int32x4_t res_0, res_1;
  int32x2_t res_0_im, res_1_im;
  int32x4_t res_even, res_odd, im_res_0, im_res_1;

  int16x8_t f0, f1, f2, f3, f4, f5, f6, f7;
  int16x8x2_t b0, b1, b2, b3;
  int32x4x2_t c0, c1, c2, c3;
  int32x4x2_t d0, d1, d2, d3;

  b0 = vtrnq_s16(src[0], src[1]);
  b1 = vtrnq_s16(src[2], src[3]);
  b2 = vtrnq_s16(src[4], src[5]);
  b3 = vtrnq_s16(src[6], src[7]);

  c0 = vtrnq_s32(vreinterpretq_s32_s16(b0.val[0]),
                 vreinterpretq_s32_s16(b0.val[1]));
  c1 = vtrnq_s32(vreinterpretq_s32_s16(b1.val[0]),
                 vreinterpretq_s32_s16(b1.val[1]));
  c2 = vtrnq_s32(vreinterpretq_s32_s16(b2.val[0]),
                 vreinterpretq_s32_s16(b2.val[1]));
  c3 = vtrnq_s32(vreinterpretq_s32_s16(b3.val[0]),
                 vreinterpretq_s32_s16(b3.val[1]));

  f0 = vld1q_s16((int16_t *)(av1_warped_filter +
                             ((sy + 0 * gamma) >> WARPEDDIFF_PREC_BITS)));
  f1 = vld1q_s16((int16_t *)(av1_warped_filter +
                             ((sy + 1 * gamma) >> WARPEDDIFF_PREC_BITS)));
  f2 = vld1q_s16((int16_t *)(av1_warped_filter +
                             ((sy + 2 * gamma) >> WARPEDDIFF_PREC_BITS)));
  f3 = vld1q_s16((int16_t *)(av1_warped_filter +
                             ((sy + 3 * gamma) >> WARPEDDIFF_PREC_BITS)));
  f4 = vld1q_s16((int16_t *)(av1_warped_filter +
                             ((sy + 4 * gamma) >> WARPEDDIFF_PREC_BITS)));
  f5 = vld1q_s16((int16_t *)(av1_warped_filter +
                             ((sy + 5 * gamma) >> WARPEDDIFF_PREC_BITS)));
  f6 = vld1q_s16((int16_t *)(av1_warped_filter +
                             ((sy + 6 * gamma) >> WARPEDDIFF_PREC_BITS)));
  f7 = vld1q_s16((int16_t *)(av1_warped_filter +
                             ((sy + 7 * gamma) >> WARPEDDIFF_PREC_BITS)));

  d0 = vtrnq_s32(vreinterpretq_s32_s16(f0), vreinterpretq_s32_s16(f2));
  d1 = vtrnq_s32(vreinterpretq_s32_s16(f4), vreinterpretq_s32_s16(f6));
  d2 = vtrnq_s32(vreinterpretq_s32_s16(f1), vreinterpretq_s32_s16(f3));
  d3 = vtrnq_s32(vreinterpretq_s32_s16(f5), vreinterpretq_s32_s16(f7));

  // row:0,1 even_col:0,2
  src_0 = vget_low_s16(vreinterpretq_s16_s32(c0.val[0]));
  fltr_0 = vget_low_s16(vreinterpretq_s16_s32(d0.val[0]));
  res_0 = vmull_s16(src_0, fltr_0);

  // row:0,1,2,3 even_col:0,2
  src_0 = vget_low_s16(vreinterpretq_s16_s32(c1.val[0]));
  fltr_0 = vget_low_s16(vreinterpretq_s16_s32(d0.val[1]));
  res_0 = vmlal_s16(res_0, src_0, fltr_0);
  res_0_im = vpadd_s32(vget_low_s32(res_0), vget_high_s32(res_0));

  // row:0,1 even_col:4,6
  src_1 = vget_low_s16(vreinterpretq_s16_s32(c0.val[1]));
  fltr_1 = vget_low_s16(vreinterpretq_s16_s32(d1.val[0]));
  res_1 = vmull_s16(src_1, fltr_1);

  // row:0,1,2,3 even_col:4,6
  src_1 = vget_low_s16(vreinterpretq_s16_s32(c1.val[1]));
  fltr_1 = vget_low_s16(vreinterpretq_s16_s32(d1.val[1]));
  res_1 = vmlal_s16(res_1, src_1, fltr_1);
  res_1_im = vpadd_s32(vget_low_s32(res_1), vget_high_s32(res_1));

  // row:0,1,2,3 even_col:0,2,4,6
  im_res_0 = vcombine_s32(res_0_im, res_1_im);

  // row:4,5 even_col:0,2
  src_0 = vget_low_s16(vreinterpretq_s16_s32(c2.val[0]));
  fltr_0 = vget_high_s16(vreinterpretq_s16_s32(d0.val[0]));
  res_0 = vmull_s16(src_0, fltr_0);

  // row:4,5,6,7 even_col:0,2
  src_0 = vget_low_s16(vreinterpretq_s16_s32(c3.val[0]));
  fltr_0 = vget_high_s16(vreinterpretq_s16_s32(d0.val[1]));
  res_0 = vmlal_s16(res_0, src_0, fltr_0);
  res_0_im = vpadd_s32(vget_low_s32(res_0), vget_high_s32(res_0));

  // row:4,5 even_col:4,6
  src_1 = vget_low_s16(vreinterpretq_s16_s32(c2.val[1]));
  fltr_1 = vget_high_s16(vreinterpretq_s16_s32(d1.val[0]));
  res_1 = vmull_s16(src_1, fltr_1);

  // row:4,5,6,7 even_col:4,6
  src_1 = vget_low_s16(vreinterpretq_s16_s32(c3.val[1]));
  fltr_1 = vget_high_s16(vreinterpretq_s16_s32(d1.val[1]));
  res_1 = vmlal_s16(res_1, src_1, fltr_1);
  res_1_im = vpadd_s32(vget_low_s32(res_1), vget_high_s32(res_1));

  // row:4,5,6,7 even_col:0,2,4,6
  im_res_1 = vcombine_s32(res_0_im, res_1_im);

  // row:0-7 even_col:0,2,4,6
  res_even = vaddq_s32(im_res_0, im_res_1);

  // row:0,1 odd_col:1,3
  src_0 = vget_high_s16(vreinterpretq_s16_s32(c0.val[0]));
  fltr_0 = vget_low_s16(vreinterpretq_s16_s32(d2.val[0]));
  res_0 = vmull_s16(src_0, fltr_0);

  // row:0,1,2,3 odd_col:1,3
  src_0 = vget_high_s16(vreinterpretq_s16_s32(c1.val[0]));
  fltr_0 = vget_low_s16(vreinterpretq_s16_s32(d2.val[1]));
  res_0 = vmlal_s16(res_0, src_0, fltr_0);
  res_0_im = vpadd_s32(vget_low_s32(res_0), vget_high_s32(res_0));

  // row:0,1 odd_col:5,7
  src_1 = vget_high_s16(vreinterpretq_s16_s32(c0.val[1]));
  fltr_1 = vget_low_s16(vreinterpretq_s16_s32(d3.val[0]));
  res_1 = vmull_s16(src_1, fltr_1);

  // row:0,1,2,3 odd_col:5,7
  src_1 = vget_high_s16(vreinterpretq_s16_s32(c1.val[1]));
  fltr_1 = vget_low_s16(vreinterpretq_s16_s32(d3.val[1]));
  res_1 = vmlal_s16(res_1, src_1, fltr_1);
  res_1_im = vpadd_s32(vget_low_s32(res_1), vget_high_s32(res_1));

  // row:0,1,2,3 odd_col:1,3,5,7
  im_res_0 = vcombine_s32(res_0_im, res_1_im);

  // row:4,5 odd_col:1,3
  src_0 = vget_high_s16(vreinterpretq_s16_s32(c2.val[0]));
  fltr_0 = vget_high_s16(vreinterpretq_s16_s32(d2.val[0]));
  res_0 = vmull_s16(src_0, fltr_0);

  // row:4,5,6,7 odd_col:1,3
  src_0 = vget_high_s16(vreinterpretq_s16_s32(c3.val[0]));
  fltr_0 = vget_high_s16(vreinterpretq_s16_s32(d2.val[1]));
  res_0 = vmlal_s16(res_0, src_0, fltr_0);
  res_0_im = vpadd_s32(vget_low_s32(res_0), vget_high_s32(res_0));

  // row:4,5 odd_col:5,7
  src_1 = vget_high_s16(vreinterpretq_s16_s32(c2.val[1]));
  fltr_1 = vget_high_s16(vreinterpretq_s16_s32(d3.val[0]));
  res_1 = vmull_s16(src_1, fltr_1);

  // row:4,5,6,7 odd_col:5,7
  src_1 = vget_high_s16(vreinterpretq_s16_s32(c3.val[1]));
  fltr_1 = vget_high_s16(vreinterpretq_s16_s32(d3.val[1]));
  res_1 = vmlal_s16(res_1, src_1, fltr_1);
  res_1_im = vpadd_s32(vget_low_s32(res_1), vget_high_s32(res_1));

  // row:4,5,6,7 odd_col:1,3,5,7
  im_res_1 = vcombine_s32(res_0_im, res_1_im);

  // row:0-7 odd_col:1,3,5,7
  res_odd = vaddq_s32(im_res_0, im_res_1);

  // reordering as 0 1 2 3 | 4 5 6 7
  c0 = vtrnq_s32(res_even, res_odd);

  // Final store
  *res_low = vcombine_s32(vget_low_s32(c0.val[0]), vget_low_s32(c0.val[1]));
  *res_high = vcombine_s32(vget_high_s32(c0.val[0]), vget_high_s32(c0.val[1]));
}

void av1_warp_affine_neon(const int32_t *mat, const uint8_t *ref, int width,
                          int height, int stride, uint8_t *pred, int p_col,
                          int p_row, int p_width, int p_height, int p_stride,
                          int subsampling_x, int subsampling_y,
                          ConvolveParams *conv_params, int16_t alpha,
                          int16_t beta, int16_t gamma, int16_t delta) {
  int16x8_t tmp[15];
  const int bd = 8;
  const int w0 = conv_params->fwd_offset;
  const int w1 = conv_params->bck_offset;
  const int32x4_t fwd = vdupq_n_s32((int32_t)w0);
  const int32x4_t bwd = vdupq_n_s32((int32_t)w1);
  const int16x8_t sub_constant = vdupq_n_s16((1 << (bd - 1)) + (1 << bd));

  int limit = 0;
  uint8x16_t vec_dup, mask_val;
  int32x4_t res_lo, res_hi;
  int16x8_t result_final;
  uint8x16_t src_1, src_2, src_3, src_4;
  static const uint8_t k0To15[16] = { 0, 1, 2,  3,  4,  5,  6,  7,
                                      8, 9, 10, 11, 12, 13, 14, 15 };
  uint8x16_t indx_vec = vld1q_u8(k0To15);
  uint8x16_t cmp_vec;

  const int reduce_bits_horiz = conv_params->round_0;
  const int reduce_bits_vert = conv_params->is_compound
                                   ? conv_params->round_1
                                   : 2 * FILTER_BITS - reduce_bits_horiz;
  const int32x4_t shift_vert = vdupq_n_s32(-(int32_t)reduce_bits_vert);
  const int offset_bits_horiz = bd + FILTER_BITS - 1;

  assert(IMPLIES(conv_params->is_compound, conv_params->dst != NULL));

  const int offset_bits_vert = bd + 2 * FILTER_BITS - reduce_bits_horiz;
  int32x4_t add_const_vert = vdupq_n_s32((int32_t)(1 << offset_bits_vert));
  const int round_bits =
      2 * FILTER_BITS - conv_params->round_0 - conv_params->round_1;
  const int16x4_t round_bits_vec = vdup_n_s16(-(int16_t)round_bits);
  const int offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  const int16x4_t res_sub_const =
      vdup_n_s16(-((1 << (offset_bits - conv_params->round_1)) +
                   (1 << (offset_bits - conv_params->round_1 - 1))));
  int k;

  assert(IMPLIES(conv_params->do_average, conv_params->is_compound));

  for (int i = 0; i < p_height; i += 8) {
    for (int j = 0; j < p_width; j += 8) {
      const int32_t src_x = (p_col + j + 4) << subsampling_x;
      const int32_t src_y = (p_row + i + 4) << subsampling_y;
      const int64_t dst_x =
          (int64_t)mat[2] * src_x + (int64_t)mat[3] * src_y + (int64_t)mat[0];
      const int64_t dst_y =
          (int64_t)mat[4] * src_x + (int64_t)mat[5] * src_y + (int64_t)mat[1];
      const int64_t x4 = dst_x >> subsampling_x;
      const int64_t y4 = dst_y >> subsampling_y;

      int32_t ix4 = (int32_t)(x4 >> WARPEDMODEL_PREC_BITS);
      int32_t sx4 = x4 & ((1 << WARPEDMODEL_PREC_BITS) - 1);
      int32_t iy4 = (int32_t)(y4 >> WARPEDMODEL_PREC_BITS);
      int32_t sy4 = y4 & ((1 << WARPEDMODEL_PREC_BITS) - 1);

      sx4 += alpha * (-4) + beta * (-4) + (1 << (WARPEDDIFF_PREC_BITS - 1)) +
             (WARPEDPIXEL_PREC_SHIFTS << WARPEDDIFF_PREC_BITS);
      sy4 += gamma * (-4) + delta * (-4) + (1 << (WARPEDDIFF_PREC_BITS - 1)) +
             (WARPEDPIXEL_PREC_SHIFTS << WARPEDDIFF_PREC_BITS);

      sx4 &= ~((1 << WARP_PARAM_REDUCE_BITS) - 1);
      sy4 &= ~((1 << WARP_PARAM_REDUCE_BITS) - 1);
      // horizontal
      if (ix4 <= -7) {
        for (k = -7; k < AOMMIN(8, p_height - i); ++k) {
          int iy = iy4 + k;
          if (iy < 0)
            iy = 0;
          else if (iy > height - 1)
            iy = height - 1;
          int16_t dup_val =
              (1 << (bd + FILTER_BITS - reduce_bits_horiz - 1)) +
              ref[iy * stride] * (1 << (FILTER_BITS - reduce_bits_horiz));

          tmp[k + 7] = vdupq_n_s16(dup_val);
        }
      } else if (ix4 >= width + 6) {
        for (k = -7; k < AOMMIN(8, p_height - i); ++k) {
          int iy = iy4 + k;
          if (iy < 0)
            iy = 0;
          else if (iy > height - 1)
            iy = height - 1;
          int16_t dup_val = (1 << (bd + FILTER_BITS - reduce_bits_horiz - 1)) +
                            ref[iy * stride + (width - 1)] *
                                (1 << (FILTER_BITS - reduce_bits_horiz));
          tmp[k + 7] = vdupq_n_s16(dup_val);
        }
      } else if (((ix4 - 7) < 0) || ((ix4 + 9) > width)) {
        const int out_of_boundary_left = -(ix4 - 6);
        const int out_of_boundary_right = (ix4 + 8) - width;

        for (k = -7; k < AOMMIN(8, p_height - i); ++k) {
          int iy = iy4 + k;
          if (iy < 0)
            iy = 0;
          else if (iy > height - 1)
            iy = height - 1;
          int sx = sx4 + beta * (k + 4);

          const uint8_t *src = ref + iy * stride + ix4 - 7;
          src_1 = vld1q_u8(src);

          if (out_of_boundary_left >= 0) {
            limit = out_of_boundary_left + 1;
            cmp_vec = vdupq_n_u8(out_of_boundary_left);
            vec_dup = vdupq_n_u8(*(src + limit));
            mask_val = vcleq_u8(indx_vec, cmp_vec);
            src_1 = vbslq_u8(mask_val, vec_dup, src_1);
          }
          if (out_of_boundary_right >= 0) {
            limit = 15 - (out_of_boundary_right + 1);
            cmp_vec = vdupq_n_u8(15 - out_of_boundary_right);
            vec_dup = vdupq_n_u8(*(src + limit));
            mask_val = vcgeq_u8(indx_vec, cmp_vec);
            src_1 = vbslq_u8(mask_val, vec_dup, src_1);
          }
          src_2 = vextq_u8(src_1, src_1, 1);
          src_3 = vextq_u8(src_2, src_2, 1);
          src_4 = vextq_u8(src_3, src_3, 1);

          horizontal_filter_neon(src_1, src_2, src_3, src_4, tmp, sx, alpha, k,
                                 offset_bits_horiz, reduce_bits_horiz);
        }
      } else {
        for (k = -7; k < AOMMIN(8, p_height - i); ++k) {
          int iy = iy4 + k;
          if (iy < 0)
            iy = 0;
          else if (iy > height - 1)
            iy = height - 1;
          int sx = sx4 + beta * (k + 4);

          const uint8_t *src = ref + iy * stride + ix4 - 7;
          src_1 = vld1q_u8(src);
          src_2 = vextq_u8(src_1, src_1, 1);
          src_3 = vextq_u8(src_2, src_2, 1);
          src_4 = vextq_u8(src_3, src_3, 1);

          horizontal_filter_neon(src_1, src_2, src_3, src_4, tmp, sx, alpha, k,
                                 offset_bits_horiz, reduce_bits_horiz);
        }
      }

      // vertical
      for (k = -4; k < AOMMIN(4, p_height - i - 4); ++k) {
        int sy = sy4 + delta * (k + 4);

        const int16x8_t *v_src = tmp + (k + 4);

        vertical_filter_neon(v_src, &res_lo, &res_hi, sy, gamma);

        res_lo = vaddq_s32(res_lo, add_const_vert);
        res_hi = vaddq_s32(res_hi, add_const_vert);

        if (conv_params->is_compound) {
          uint16_t *const p =
              (uint16_t *)&conv_params
                  ->dst[(i + k + 4) * conv_params->dst_stride + j];

          res_lo = vrshlq_s32(res_lo, shift_vert);
          if (conv_params->do_average) {
            uint8_t *const dst8 = &pred[(i + k + 4) * p_stride + j];
            uint16x4_t tmp16_lo = vld1_u16(p);
            int32x4_t tmp32_lo = vreinterpretq_s32_u32(vmovl_u16(tmp16_lo));
            int16x4_t tmp16_low;
            if (conv_params->use_dist_wtd_comp_avg) {
              res_lo = vmulq_s32(res_lo, bwd);
              tmp32_lo = vmulq_s32(tmp32_lo, fwd);
              tmp32_lo = vaddq_s32(tmp32_lo, res_lo);
              tmp16_low = vshrn_n_s32(tmp32_lo, DIST_PRECISION_BITS);
            } else {
              tmp32_lo = vaddq_s32(tmp32_lo, res_lo);
              tmp16_low = vshrn_n_s32(tmp32_lo, 1);
            }
            int16x4_t res_low = vadd_s16(tmp16_low, res_sub_const);
            res_low = vqrshl_s16(res_low, round_bits_vec);
            int16x8_t final_res_low = vcombine_s16(res_low, res_low);
            uint8x8_t res_8_low = vqmovun_s16(final_res_low);

            vst1_lane_u32((uint32_t *)dst8, vreinterpret_u32_u8(res_8_low), 0);
          } else {
            uint16x4_t res_u16_low = vqmovun_s32(res_lo);
            vst1_u16(p, res_u16_low);
          }
          if (p_width > 4) {
            uint16_t *const p4 =
                (uint16_t *)&conv_params
                    ->dst[(i + k + 4) * conv_params->dst_stride + j + 4];

            res_hi = vrshlq_s32(res_hi, shift_vert);
            if (conv_params->do_average) {
              uint8_t *const dst8_4 = &pred[(i + k + 4) * p_stride + j + 4];

              uint16x4_t tmp16_hi = vld1_u16(p4);
              int32x4_t tmp32_hi = vreinterpretq_s32_u32(vmovl_u16(tmp16_hi));
              int16x4_t tmp16_high;
              if (conv_params->use_dist_wtd_comp_avg) {
                res_hi = vmulq_s32(res_hi, bwd);
                tmp32_hi = vmulq_s32(tmp32_hi, fwd);
                tmp32_hi = vaddq_s32(tmp32_hi, res_hi);
                tmp16_high = vshrn_n_s32(tmp32_hi, DIST_PRECISION_BITS);
              } else {
                tmp32_hi = vaddq_s32(tmp32_hi, res_hi);
                tmp16_high = vshrn_n_s32(tmp32_hi, 1);
              }
              int16x4_t res_high = vadd_s16(tmp16_high, res_sub_const);
              res_high = vqrshl_s16(res_high, round_bits_vec);
              int16x8_t final_res_high = vcombine_s16(res_high, res_high);
              uint8x8_t res_8_high = vqmovun_s16(final_res_high);

              vst1_lane_u32((uint32_t *)dst8_4, vreinterpret_u32_u8(res_8_high),
                            0);
            } else {
              uint16x4_t res_u16_high = vqmovun_s32(res_hi);
              vst1_u16(p4, res_u16_high);
            }
          }
        } else {
          res_lo = vrshlq_s32(res_lo, shift_vert);
          res_hi = vrshlq_s32(res_hi, shift_vert);

          result_final = vcombine_s16(vmovn_s32(res_lo), vmovn_s32(res_hi));
          result_final = vsubq_s16(result_final, sub_constant);

          uint8_t *const p = (uint8_t *)&pred[(i + k + 4) * p_stride + j];
          uint8x8_t val = vqmovun_s16(result_final);

          if (p_width == 4) {
            vst1_lane_u32((uint32_t *)p, vreinterpret_u32_u8(val), 0);
          } else {
            vst1_u8(p, val);
          }
        }
      }
    }
  }
}
