/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <altivec.h>

#include "config/av1_rtcd.h"

#include "av1/common/cfl.h"

#define OFF_0 0
#define OFF_1 16
#define OFF_2 32
#define OFF_3 48
#define CFL_BUF_LINE_BYTES 64
#define CFL_LINE_1 64
#define CFL_LINE_2 128
#define CFL_LINE_3 192

typedef vector signed char int8x16_t;          // NOLINT(runtime/int)
typedef vector unsigned char uint8x16_t;       // NOLINT(runtime/int)
typedef vector signed short int16x8_t;         // NOLINT(runtime/int)
typedef vector unsigned short uint16x8_t;      // NOLINT(runtime/int)
typedef vector signed int int32x4_t;           // NOLINT(runtime/int)
typedef vector unsigned int uint32x4_t;        // NOLINT(runtime/int)
typedef vector unsigned long long uint64x2_t;  // NOLINT(runtime/int)

static INLINE void subtract_average_vsx(const uint16_t *src_ptr, int16_t *dst,
                                        int width, int height, int round_offset,
                                        int num_pel_log2) {
  //  int16_t *dst = dst_ptr;
  const int16_t *dst_end = dst + height * CFL_BUF_LINE;
  const int16_t *sum_buf = (const int16_t *)src_ptr;
  const int16_t *end = sum_buf + height * CFL_BUF_LINE;
  const uint32x4_t div_shift = vec_splats((uint32_t)num_pel_log2);
  const uint8x16_t mask_64 = { 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
                               0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };
  const uint8x16_t mask_32 = { 0x14, 0x15, 0x16, 0x17, 0x00, 0x01, 0x02, 0x03,
                               0x1C, 0x1D, 0x1E, 0x1F, 0x08, 0x09, 0x0A, 0x0B };

  int32x4_t sum_32x4_0 = { 0, 0, 0, round_offset };
  int32x4_t sum_32x4_1 = { 0, 0, 0, 0 };
  do {
    sum_32x4_0 = vec_sum4s(vec_vsx_ld(OFF_0, sum_buf), sum_32x4_0);
    sum_32x4_1 = vec_sum4s(vec_vsx_ld(OFF_0 + CFL_LINE_1, sum_buf), sum_32x4_1);
    if (width >= 16) {
      sum_32x4_0 = vec_sum4s(vec_vsx_ld(OFF_1, sum_buf), sum_32x4_0);
      sum_32x4_1 =
          vec_sum4s(vec_vsx_ld(OFF_1 + CFL_LINE_1, sum_buf), sum_32x4_1);
    }
    if (width == 32) {
      sum_32x4_0 = vec_sum4s(vec_vsx_ld(OFF_2, sum_buf), sum_32x4_0);
      sum_32x4_1 =
          vec_sum4s(vec_vsx_ld(OFF_2 + CFL_LINE_1, sum_buf), sum_32x4_1);
      sum_32x4_0 = vec_sum4s(vec_vsx_ld(OFF_3, sum_buf), sum_32x4_0);
      sum_32x4_1 =
          vec_sum4s(vec_vsx_ld(OFF_3 + CFL_LINE_1, sum_buf), sum_32x4_1);
    }
  } while ((sum_buf += (CFL_BUF_LINE * 2)) < end);
  int32x4_t sum_32x4 = vec_add(sum_32x4_0, sum_32x4_1);

  const int32x4_t perm_64 = vec_perm(sum_32x4, sum_32x4, mask_64);
  sum_32x4 = vec_add(sum_32x4, perm_64);
  const int32x4_t perm_32 = vec_perm(sum_32x4, sum_32x4, mask_32);
  sum_32x4 = vec_add(sum_32x4, perm_32);
  const int32x4_t avg = vec_sr(sum_32x4, div_shift);
  const int16x8_t vec_avg = vec_pack(avg, avg);
  do {
    vec_vsx_st(vec_sub(vec_vsx_ld(OFF_0, dst), vec_avg), OFF_0, dst);
    vec_vsx_st(vec_sub(vec_vsx_ld(OFF_0 + CFL_LINE_1, dst), vec_avg),
               OFF_0 + CFL_BUF_LINE_BYTES, dst);
    vec_vsx_st(vec_sub(vec_vsx_ld(OFF_0 + CFL_LINE_2, dst), vec_avg),
               OFF_0 + CFL_LINE_2, dst);
    vec_vsx_st(vec_sub(vec_vsx_ld(OFF_0 + CFL_LINE_3, dst), vec_avg),
               OFF_0 + CFL_LINE_3, dst);
    if (width >= 16) {
      vec_vsx_st(vec_sub(vec_vsx_ld(OFF_1, dst), vec_avg), OFF_1, dst);
      vec_vsx_st(vec_sub(vec_vsx_ld(OFF_1 + CFL_LINE_1, dst), vec_avg),
                 OFF_1 + CFL_LINE_1, dst);
      vec_vsx_st(vec_sub(vec_vsx_ld(OFF_1 + CFL_LINE_2, dst), vec_avg),
                 OFF_1 + CFL_LINE_2, dst);
      vec_vsx_st(vec_sub(vec_vsx_ld(OFF_1 + CFL_LINE_3, dst), vec_avg),
                 OFF_1 + CFL_LINE_3, dst);
    }
    if (width == 32) {
      vec_vsx_st(vec_sub(vec_vsx_ld(OFF_2, dst), vec_avg), OFF_2, dst);
      vec_vsx_st(vec_sub(vec_vsx_ld(OFF_2 + CFL_LINE_1, dst), vec_avg),
                 OFF_2 + CFL_LINE_1, dst);
      vec_vsx_st(vec_sub(vec_vsx_ld(OFF_2 + CFL_LINE_2, dst), vec_avg),
                 OFF_2 + CFL_LINE_2, dst);
      vec_vsx_st(vec_sub(vec_vsx_ld(OFF_2 + CFL_LINE_3, dst), vec_avg),
                 OFF_2 + CFL_LINE_3, dst);

      vec_vsx_st(vec_sub(vec_vsx_ld(OFF_3, dst), vec_avg), OFF_3, dst);
      vec_vsx_st(vec_sub(vec_vsx_ld(OFF_3 + CFL_LINE_1, dst), vec_avg),
                 OFF_3 + CFL_LINE_1, dst);
      vec_vsx_st(vec_sub(vec_vsx_ld(OFF_3 + CFL_LINE_2, dst), vec_avg),
                 OFF_3 + CFL_LINE_2, dst);
      vec_vsx_st(vec_sub(vec_vsx_ld(OFF_3 + CFL_LINE_3, dst), vec_avg),
                 OFF_3 + CFL_LINE_3, dst);
    }
  } while ((dst += CFL_BUF_LINE * 4) < dst_end);
}

// Declare wrappers for VSX sizes
CFL_SUB_AVG_X(vsx, 8, 4, 16, 5)
CFL_SUB_AVG_X(vsx, 8, 8, 32, 6)
CFL_SUB_AVG_X(vsx, 8, 16, 64, 7)
CFL_SUB_AVG_X(vsx, 8, 32, 128, 8)
CFL_SUB_AVG_X(vsx, 16, 4, 32, 6)
CFL_SUB_AVG_X(vsx, 16, 8, 64, 7)
CFL_SUB_AVG_X(vsx, 16, 16, 128, 8)
CFL_SUB_AVG_X(vsx, 16, 32, 256, 9)
CFL_SUB_AVG_X(vsx, 32, 8, 128, 8)
CFL_SUB_AVG_X(vsx, 32, 16, 256, 9)
CFL_SUB_AVG_X(vsx, 32, 32, 512, 10)

// Based on observation, for small blocks VSX does not outperform C (no 64bit
// load and store intrinsics). So we call the C code for block widths 4.
cfl_subtract_average_fn cfl_get_subtract_average_fn_vsx(TX_SIZE tx_size) {
  static const cfl_subtract_average_fn sub_avg[TX_SIZES_ALL] = {
    cfl_subtract_average_4x4_c,     /* 4x4 */
    cfl_subtract_average_8x8_vsx,   /* 8x8 */
    cfl_subtract_average_16x16_vsx, /* 16x16 */
    cfl_subtract_average_32x32_vsx, /* 32x32 */
    NULL,                           /* 64x64 (invalid CFL size) */
    cfl_subtract_average_4x8_c,     /* 4x8 */
    cfl_subtract_average_8x4_vsx,   /* 8x4 */
    cfl_subtract_average_8x16_vsx,  /* 8x16 */
    cfl_subtract_average_16x8_vsx,  /* 16x8 */
    cfl_subtract_average_16x32_vsx, /* 16x32 */
    cfl_subtract_average_32x16_vsx, /* 32x16 */
    NULL,                           /* 32x64 (invalid CFL size) */
    NULL,                           /* 64x32 (invalid CFL size) */
    cfl_subtract_average_4x16_c,    /* 4x16 */
    cfl_subtract_average_16x4_vsx,  /* 16x4 */
    cfl_subtract_average_8x32_vsx,  /* 8x32 */
    cfl_subtract_average_32x8_vsx,  /* 32x8 */
    NULL,                           /* 16x64 (invalid CFL size) */
    NULL,                           /* 64x16 (invalid CFL size) */
  };
  // Modulo TX_SIZES_ALL to ensure that an attacker won't be able to
  // index the function pointer array out of bounds.
  return sub_avg[tx_size % TX_SIZES_ALL];
}
