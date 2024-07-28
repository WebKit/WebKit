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

#include <stdlib.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_ports/mem.h"

static INLINE int8_t signed_char_clamp(int t) {
  return (int8_t)clamp(t, -128, 127);
}

#if CONFIG_AV1_HIGHBITDEPTH
static INLINE int16_t signed_char_clamp_high(int t, int bd) {
  switch (bd) {
    case 10: return (int16_t)clamp(t, -128 * 4, 128 * 4 - 1);
    case 12: return (int16_t)clamp(t, -128 * 16, 128 * 16 - 1);
    case 8:
    default: return (int16_t)clamp(t, -128, 128 - 1);
  }
}
#endif

// should we apply any filter at all: 11111111 yes, 00000000 no
static INLINE int8_t filter_mask2(uint8_t limit, uint8_t blimit, uint8_t p1,
                                  uint8_t p0, uint8_t q0, uint8_t q1) {
  int8_t mask = 0;
  mask |= (abs(p1 - p0) > limit) * -1;
  mask |= (abs(q1 - q0) > limit) * -1;
  mask |= (abs(p0 - q0) * 2 + abs(p1 - q1) / 2 > blimit) * -1;
  return ~mask;
}

static INLINE int8_t filter_mask(uint8_t limit, uint8_t blimit, uint8_t p3,
                                 uint8_t p2, uint8_t p1, uint8_t p0, uint8_t q0,
                                 uint8_t q1, uint8_t q2, uint8_t q3) {
  int8_t mask = 0;
  mask |= (abs(p3 - p2) > limit) * -1;
  mask |= (abs(p2 - p1) > limit) * -1;
  mask |= (abs(p1 - p0) > limit) * -1;
  mask |= (abs(q1 - q0) > limit) * -1;
  mask |= (abs(q2 - q1) > limit) * -1;
  mask |= (abs(q3 - q2) > limit) * -1;
  mask |= (abs(p0 - q0) * 2 + abs(p1 - q1) / 2 > blimit) * -1;
  return ~mask;
}

static INLINE int8_t filter_mask3_chroma(uint8_t limit, uint8_t blimit,
                                         uint8_t p2, uint8_t p1, uint8_t p0,
                                         uint8_t q0, uint8_t q1, uint8_t q2) {
  int8_t mask = 0;
  mask |= (abs(p2 - p1) > limit) * -1;
  mask |= (abs(p1 - p0) > limit) * -1;
  mask |= (abs(q1 - q0) > limit) * -1;
  mask |= (abs(q2 - q1) > limit) * -1;
  mask |= (abs(p0 - q0) * 2 + abs(p1 - q1) / 2 > blimit) * -1;
  return ~mask;
}

static INLINE int8_t flat_mask3_chroma(uint8_t thresh, uint8_t p2, uint8_t p1,
                                       uint8_t p0, uint8_t q0, uint8_t q1,
                                       uint8_t q2) {
  int8_t mask = 0;
  mask |= (abs(p1 - p0) > thresh) * -1;
  mask |= (abs(q1 - q0) > thresh) * -1;
  mask |= (abs(p2 - p0) > thresh) * -1;
  mask |= (abs(q2 - q0) > thresh) * -1;
  return ~mask;
}

static INLINE int8_t flat_mask4(uint8_t thresh, uint8_t p3, uint8_t p2,
                                uint8_t p1, uint8_t p0, uint8_t q0, uint8_t q1,
                                uint8_t q2, uint8_t q3) {
  int8_t mask = 0;
  mask |= (abs(p1 - p0) > thresh) * -1;
  mask |= (abs(q1 - q0) > thresh) * -1;
  mask |= (abs(p2 - p0) > thresh) * -1;
  mask |= (abs(q2 - q0) > thresh) * -1;
  mask |= (abs(p3 - p0) > thresh) * -1;
  mask |= (abs(q3 - q0) > thresh) * -1;
  return ~mask;
}

// is there high edge variance internal edge: 11111111 yes, 00000000 no
static INLINE int8_t hev_mask(uint8_t thresh, uint8_t p1, uint8_t p0,
                              uint8_t q0, uint8_t q1) {
  int8_t hev = 0;
  hev |= (abs(p1 - p0) > thresh) * -1;
  hev |= (abs(q1 - q0) > thresh) * -1;
  return hev;
}

static INLINE void filter4(int8_t mask, uint8_t thresh, uint8_t *op1,
                           uint8_t *op0, uint8_t *oq0, uint8_t *oq1) {
  int8_t filter1, filter2;

  const int8_t ps1 = (int8_t)(*op1 ^ 0x80);
  const int8_t ps0 = (int8_t)(*op0 ^ 0x80);
  const int8_t qs0 = (int8_t)(*oq0 ^ 0x80);
  const int8_t qs1 = (int8_t)(*oq1 ^ 0x80);
  const int8_t hev = hev_mask(thresh, *op1, *op0, *oq0, *oq1);

  // add outer taps if we have high edge variance
  int8_t filter = signed_char_clamp(ps1 - qs1) & hev;

  // inner taps
  filter = signed_char_clamp(filter + 3 * (qs0 - ps0)) & mask;

  // save bottom 3 bits so that we round one side +4 and the other +3
  // if it equals 4 we'll set to adjust by -1 to account for the fact
  // we'd round 3 the other way
  filter1 = signed_char_clamp(filter + 4) >> 3;
  filter2 = signed_char_clamp(filter + 3) >> 3;

  *oq0 = (uint8_t)(signed_char_clamp(qs0 - filter1) ^ 0x80);
  *op0 = (uint8_t)(signed_char_clamp(ps0 + filter2) ^ 0x80);

  // outer tap adjustments
  filter = ROUND_POWER_OF_TWO(filter1, 1) & ~hev;

  *oq1 = (uint8_t)(signed_char_clamp(qs1 - filter) ^ 0x80);
  *op1 = (uint8_t)(signed_char_clamp(ps1 + filter) ^ 0x80);
}

void aom_lpf_horizontal_4_c(uint8_t *s, int p /* pitch */,
                            const uint8_t *blimit, const uint8_t *limit,
                            const uint8_t *thresh) {
  int i;
  int count = 4;

  // loop filter designed to work using chars so that we can make maximum use
  // of 8 bit simd instructions.
  for (i = 0; i < count; ++i) {
    const uint8_t p1 = s[-2 * p], p0 = s[-p];
    const uint8_t q0 = s[0 * p], q1 = s[1 * p];
    const int8_t mask = filter_mask2(*limit, *blimit, p1, p0, q0, q1);
    filter4(mask, *thresh, s - 2 * p, s - 1 * p, s, s + 1 * p);
    ++s;
  }
}

void aom_lpf_horizontal_4_dual_c(uint8_t *s, int p, const uint8_t *blimit0,
                                 const uint8_t *limit0, const uint8_t *thresh0,
                                 const uint8_t *blimit1, const uint8_t *limit1,
                                 const uint8_t *thresh1) {
  aom_lpf_horizontal_4_c(s, p, blimit0, limit0, thresh0);
  aom_lpf_horizontal_4_c(s + 4, p, blimit1, limit1, thresh1);
}

void aom_lpf_horizontal_4_quad_c(uint8_t *s, int p, const uint8_t *blimit0,
                                 const uint8_t *limit0,
                                 const uint8_t *thresh0) {
  aom_lpf_horizontal_4_c(s, p, blimit0, limit0, thresh0);
  aom_lpf_horizontal_4_c(s + 4, p, blimit0, limit0, thresh0);
  aom_lpf_horizontal_4_c(s + 8, p, blimit0, limit0, thresh0);
  aom_lpf_horizontal_4_c(s + 12, p, blimit0, limit0, thresh0);
}

void aom_lpf_vertical_4_c(uint8_t *s, int pitch, const uint8_t *blimit,
                          const uint8_t *limit, const uint8_t *thresh) {
  int i;
  int count = 4;

  // loop filter designed to work using chars so that we can make maximum use
  // of 8 bit simd instructions.
  for (i = 0; i < count; ++i) {
    const uint8_t p1 = s[-2], p0 = s[-1];
    const uint8_t q0 = s[0], q1 = s[1];
    const int8_t mask = filter_mask2(*limit, *blimit, p1, p0, q0, q1);
    filter4(mask, *thresh, s - 2, s - 1, s, s + 1);
    s += pitch;
  }
}

void aom_lpf_vertical_4_dual_c(uint8_t *s, int pitch, const uint8_t *blimit0,
                               const uint8_t *limit0, const uint8_t *thresh0,
                               const uint8_t *blimit1, const uint8_t *limit1,
                               const uint8_t *thresh1) {
  aom_lpf_vertical_4_c(s, pitch, blimit0, limit0, thresh0);
  aom_lpf_vertical_4_c(s + 4 * pitch, pitch, blimit1, limit1, thresh1);
}

void aom_lpf_vertical_4_quad_c(uint8_t *s, int pitch, const uint8_t *blimit0,
                               const uint8_t *limit0, const uint8_t *thresh0) {
  aom_lpf_vertical_4_c(s, pitch, blimit0, limit0, thresh0);
  aom_lpf_vertical_4_c(s + 4 * pitch, pitch, blimit0, limit0, thresh0);
  aom_lpf_vertical_4_c(s + 8 * pitch, pitch, blimit0, limit0, thresh0);
  aom_lpf_vertical_4_c(s + 12 * pitch, pitch, blimit0, limit0, thresh0);
}

static INLINE void filter6(int8_t mask, uint8_t thresh, int8_t flat,
                           uint8_t *op2, uint8_t *op1, uint8_t *op0,
                           uint8_t *oq0, uint8_t *oq1, uint8_t *oq2) {
  if (flat && mask) {
    const uint8_t p2 = *op2, p1 = *op1, p0 = *op0;
    const uint8_t q0 = *oq0, q1 = *oq1, q2 = *oq2;

    // 5-tap filter [1, 2, 2, 2, 1]
    *op1 = ROUND_POWER_OF_TWO(p2 * 3 + p1 * 2 + p0 * 2 + q0, 3);
    *op0 = ROUND_POWER_OF_TWO(p2 + p1 * 2 + p0 * 2 + q0 * 2 + q1, 3);
    *oq0 = ROUND_POWER_OF_TWO(p1 + p0 * 2 + q0 * 2 + q1 * 2 + q2, 3);
    *oq1 = ROUND_POWER_OF_TWO(p0 + q0 * 2 + q1 * 2 + q2 * 3, 3);
  } else {
    filter4(mask, thresh, op1, op0, oq0, oq1);
  }
}

static INLINE void filter8(int8_t mask, uint8_t thresh, int8_t flat,
                           uint8_t *op3, uint8_t *op2, uint8_t *op1,
                           uint8_t *op0, uint8_t *oq0, uint8_t *oq1,
                           uint8_t *oq2, uint8_t *oq3) {
  if (flat && mask) {
    const uint8_t p3 = *op3, p2 = *op2, p1 = *op1, p0 = *op0;
    const uint8_t q0 = *oq0, q1 = *oq1, q2 = *oq2, q3 = *oq3;

    // 7-tap filter [1, 1, 1, 2, 1, 1, 1]
    *op2 = ROUND_POWER_OF_TWO(p3 + p3 + p3 + 2 * p2 + p1 + p0 + q0, 3);
    *op1 = ROUND_POWER_OF_TWO(p3 + p3 + p2 + 2 * p1 + p0 + q0 + q1, 3);
    *op0 = ROUND_POWER_OF_TWO(p3 + p2 + p1 + 2 * p0 + q0 + q1 + q2, 3);
    *oq0 = ROUND_POWER_OF_TWO(p2 + p1 + p0 + 2 * q0 + q1 + q2 + q3, 3);
    *oq1 = ROUND_POWER_OF_TWO(p1 + p0 + q0 + 2 * q1 + q2 + q3 + q3, 3);
    *oq2 = ROUND_POWER_OF_TWO(p0 + q0 + q1 + 2 * q2 + q3 + q3 + q3, 3);
  } else {
    filter4(mask, thresh, op1, op0, oq0, oq1);
  }
}

void aom_lpf_horizontal_6_c(uint8_t *s, int p, const uint8_t *blimit,
                            const uint8_t *limit, const uint8_t *thresh) {
  int i;
  int count = 4;

  // loop filter designed to work using chars so that we can make maximum use
  // of 8 bit simd instructions.
  for (i = 0; i < count; ++i) {
    const uint8_t p2 = s[-3 * p], p1 = s[-2 * p], p0 = s[-p];
    const uint8_t q0 = s[0 * p], q1 = s[1 * p], q2 = s[2 * p];

    const int8_t mask =
        filter_mask3_chroma(*limit, *blimit, p2, p1, p0, q0, q1, q2);
    const int8_t flat = flat_mask3_chroma(1, p2, p1, p0, q0, q1, q2);
    filter6(mask, *thresh, flat, s - 3 * p, s - 2 * p, s - 1 * p, s, s + 1 * p,
            s + 2 * p);
    ++s;
  }
}

void aom_lpf_horizontal_6_dual_c(uint8_t *s, int p, const uint8_t *blimit0,
                                 const uint8_t *limit0, const uint8_t *thresh0,
                                 const uint8_t *blimit1, const uint8_t *limit1,
                                 const uint8_t *thresh1) {
  aom_lpf_horizontal_6_c(s, p, blimit0, limit0, thresh0);
  aom_lpf_horizontal_6_c(s + 4, p, blimit1, limit1, thresh1);
}

void aom_lpf_horizontal_6_quad_c(uint8_t *s, int p, const uint8_t *blimit0,
                                 const uint8_t *limit0,
                                 const uint8_t *thresh0) {
  aom_lpf_horizontal_6_c(s, p, blimit0, limit0, thresh0);
  aom_lpf_horizontal_6_c(s + 4, p, blimit0, limit0, thresh0);
  aom_lpf_horizontal_6_c(s + 8, p, blimit0, limit0, thresh0);
  aom_lpf_horizontal_6_c(s + 12, p, blimit0, limit0, thresh0);
}

void aom_lpf_horizontal_8_c(uint8_t *s, int p, const uint8_t *blimit,
                            const uint8_t *limit, const uint8_t *thresh) {
  int i;
  int count = 4;

  // loop filter designed to work using chars so that we can make maximum use
  // of 8 bit simd instructions.
  for (i = 0; i < count; ++i) {
    const uint8_t p3 = s[-4 * p], p2 = s[-3 * p], p1 = s[-2 * p], p0 = s[-p];
    const uint8_t q0 = s[0 * p], q1 = s[1 * p], q2 = s[2 * p], q3 = s[3 * p];

    const int8_t mask =
        filter_mask(*limit, *blimit, p3, p2, p1, p0, q0, q1, q2, q3);
    const int8_t flat = flat_mask4(1, p3, p2, p1, p0, q0, q1, q2, q3);
    filter8(mask, *thresh, flat, s - 4 * p, s - 3 * p, s - 2 * p, s - 1 * p, s,
            s + 1 * p, s + 2 * p, s + 3 * p);
    ++s;
  }
}

void aom_lpf_horizontal_8_dual_c(uint8_t *s, int p, const uint8_t *blimit0,
                                 const uint8_t *limit0, const uint8_t *thresh0,
                                 const uint8_t *blimit1, const uint8_t *limit1,
                                 const uint8_t *thresh1) {
  aom_lpf_horizontal_8_c(s, p, blimit0, limit0, thresh0);
  aom_lpf_horizontal_8_c(s + 4, p, blimit1, limit1, thresh1);
}

void aom_lpf_horizontal_8_quad_c(uint8_t *s, int p, const uint8_t *blimit0,
                                 const uint8_t *limit0,
                                 const uint8_t *thresh0) {
  aom_lpf_horizontal_8_c(s, p, blimit0, limit0, thresh0);
  aom_lpf_horizontal_8_c(s + 4, p, blimit0, limit0, thresh0);
  aom_lpf_horizontal_8_c(s + 8, p, blimit0, limit0, thresh0);
  aom_lpf_horizontal_8_c(s + 12, p, blimit0, limit0, thresh0);
}

void aom_lpf_vertical_6_c(uint8_t *s, int pitch, const uint8_t *blimit,
                          const uint8_t *limit, const uint8_t *thresh) {
  int i;
  int count = 4;

  for (i = 0; i < count; ++i) {
    const uint8_t p2 = s[-3], p1 = s[-2], p0 = s[-1];
    const uint8_t q0 = s[0], q1 = s[1], q2 = s[2];
    const int8_t mask =
        filter_mask3_chroma(*limit, *blimit, p2, p1, p0, q0, q1, q2);
    const int8_t flat = flat_mask3_chroma(1, p2, p1, p0, q0, q1, q2);
    filter6(mask, *thresh, flat, s - 3, s - 2, s - 1, s, s + 1, s + 2);
    s += pitch;
  }
}

void aom_lpf_vertical_6_dual_c(uint8_t *s, int pitch, const uint8_t *blimit0,
                               const uint8_t *limit0, const uint8_t *thresh0,
                               const uint8_t *blimit1, const uint8_t *limit1,
                               const uint8_t *thresh1) {
  aom_lpf_vertical_6_c(s, pitch, blimit0, limit0, thresh0);
  aom_lpf_vertical_6_c(s + 4 * pitch, pitch, blimit1, limit1, thresh1);
}

void aom_lpf_vertical_6_quad_c(uint8_t *s, int pitch, const uint8_t *blimit0,
                               const uint8_t *limit0, const uint8_t *thresh0) {
  aom_lpf_vertical_6_c(s, pitch, blimit0, limit0, thresh0);
  aom_lpf_vertical_6_c(s + 4 * pitch, pitch, blimit0, limit0, thresh0);
  aom_lpf_vertical_6_c(s + 8 * pitch, pitch, blimit0, limit0, thresh0);
  aom_lpf_vertical_6_c(s + 12 * pitch, pitch, blimit0, limit0, thresh0);
}

void aom_lpf_vertical_8_c(uint8_t *s, int pitch, const uint8_t *blimit,
                          const uint8_t *limit, const uint8_t *thresh) {
  int i;
  int count = 4;

  for (i = 0; i < count; ++i) {
    const uint8_t p3 = s[-4], p2 = s[-3], p1 = s[-2], p0 = s[-1];
    const uint8_t q0 = s[0], q1 = s[1], q2 = s[2], q3 = s[3];
    const int8_t mask =
        filter_mask(*limit, *blimit, p3, p2, p1, p0, q0, q1, q2, q3);
    const int8_t flat = flat_mask4(1, p3, p2, p1, p0, q0, q1, q2, q3);
    filter8(mask, *thresh, flat, s - 4, s - 3, s - 2, s - 1, s, s + 1, s + 2,
            s + 3);
    s += pitch;
  }
}

void aom_lpf_vertical_8_dual_c(uint8_t *s, int pitch, const uint8_t *blimit0,
                               const uint8_t *limit0, const uint8_t *thresh0,
                               const uint8_t *blimit1, const uint8_t *limit1,
                               const uint8_t *thresh1) {
  aom_lpf_vertical_8_c(s, pitch, blimit0, limit0, thresh0);
  aom_lpf_vertical_8_c(s + 4 * pitch, pitch, blimit1, limit1, thresh1);
}

void aom_lpf_vertical_8_quad_c(uint8_t *s, int pitch, const uint8_t *blimit0,
                               const uint8_t *limit0, const uint8_t *thresh0) {
  aom_lpf_vertical_8_c(s, pitch, blimit0, limit0, thresh0);
  aom_lpf_vertical_8_c(s + 4 * pitch, pitch, blimit0, limit0, thresh0);
  aom_lpf_vertical_8_c(s + 8 * pitch, pitch, blimit0, limit0, thresh0);
  aom_lpf_vertical_8_c(s + 12 * pitch, pitch, blimit0, limit0, thresh0);
}

static INLINE void filter14(int8_t mask, uint8_t thresh, int8_t flat,
                            int8_t flat2, uint8_t *op6, uint8_t *op5,
                            uint8_t *op4, uint8_t *op3, uint8_t *op2,
                            uint8_t *op1, uint8_t *op0, uint8_t *oq0,
                            uint8_t *oq1, uint8_t *oq2, uint8_t *oq3,
                            uint8_t *oq4, uint8_t *oq5, uint8_t *oq6) {
  if (flat2 && flat && mask) {
    const uint8_t p6 = *op6, p5 = *op5, p4 = *op4, p3 = *op3, p2 = *op2,
                  p1 = *op1, p0 = *op0;
    const uint8_t q0 = *oq0, q1 = *oq1, q2 = *oq2, q3 = *oq3, q4 = *oq4,
                  q5 = *oq5, q6 = *oq6;

    // 13-tap filter [1, 1, 1, 1, 1, 2, 2, 2, 1, 1, 1, 1, 1]
    *op5 = ROUND_POWER_OF_TWO(p6 * 7 + p5 * 2 + p4 * 2 + p3 + p2 + p1 + p0 + q0,
                              4);
    *op4 = ROUND_POWER_OF_TWO(
        p6 * 5 + p5 * 2 + p4 * 2 + p3 * 2 + p2 + p1 + p0 + q0 + q1, 4);
    *op3 = ROUND_POWER_OF_TWO(
        p6 * 4 + p5 + p4 * 2 + p3 * 2 + p2 * 2 + p1 + p0 + q0 + q1 + q2, 4);
    *op2 = ROUND_POWER_OF_TWO(
        p6 * 3 + p5 + p4 + p3 * 2 + p2 * 2 + p1 * 2 + p0 + q0 + q1 + q2 + q3,
        4);
    *op1 = ROUND_POWER_OF_TWO(p6 * 2 + p5 + p4 + p3 + p2 * 2 + p1 * 2 + p0 * 2 +
                                  q0 + q1 + q2 + q3 + q4,
                              4);
    *op0 = ROUND_POWER_OF_TWO(p6 + p5 + p4 + p3 + p2 + p1 * 2 + p0 * 2 +
                                  q0 * 2 + q1 + q2 + q3 + q4 + q5,
                              4);
    *oq0 = ROUND_POWER_OF_TWO(p5 + p4 + p3 + p2 + p1 + p0 * 2 + q0 * 2 +
                                  q1 * 2 + q2 + q3 + q4 + q5 + q6,
                              4);
    *oq1 = ROUND_POWER_OF_TWO(p4 + p3 + p2 + p1 + p0 + q0 * 2 + q1 * 2 +
                                  q2 * 2 + q3 + q4 + q5 + q6 * 2,
                              4);
    *oq2 = ROUND_POWER_OF_TWO(
        p3 + p2 + p1 + p0 + q0 + q1 * 2 + q2 * 2 + q3 * 2 + q4 + q5 + q6 * 3,
        4);
    *oq3 = ROUND_POWER_OF_TWO(
        p2 + p1 + p0 + q0 + q1 + q2 * 2 + q3 * 2 + q4 * 2 + q5 + q6 * 4, 4);
    *oq4 = ROUND_POWER_OF_TWO(
        p1 + p0 + q0 + q1 + q2 + q3 * 2 + q4 * 2 + q5 * 2 + q6 * 5, 4);
    *oq5 = ROUND_POWER_OF_TWO(p0 + q0 + q1 + q2 + q3 + q4 * 2 + q5 * 2 + q6 * 7,
                              4);
  } else {
    filter8(mask, thresh, flat, op3, op2, op1, op0, oq0, oq1, oq2, oq3);
  }
}

static void mb_lpf_horizontal_edge_w(uint8_t *s, int p, const uint8_t *blimit,
                                     const uint8_t *limit,
                                     const uint8_t *thresh, int count) {
  int i;
  int step = 4;

  // loop filter designed to work using chars so that we can make maximum use
  // of 8 bit simd instructions.
  for (i = 0; i < step * count; ++i) {
    const uint8_t p6 = s[-7 * p], p5 = s[-6 * p], p4 = s[-5 * p],
                  p3 = s[-4 * p], p2 = s[-3 * p], p1 = s[-2 * p], p0 = s[-p];
    const uint8_t q0 = s[0 * p], q1 = s[1 * p], q2 = s[2 * p], q3 = s[3 * p],
                  q4 = s[4 * p], q5 = s[5 * p], q6 = s[6 * p];
    const int8_t mask =
        filter_mask(*limit, *blimit, p3, p2, p1, p0, q0, q1, q2, q3);
    const int8_t flat = flat_mask4(1, p3, p2, p1, p0, q0, q1, q2, q3);
    const int8_t flat2 = flat_mask4(1, p6, p5, p4, p0, q0, q4, q5, q6);

    filter14(mask, *thresh, flat, flat2, s - 7 * p, s - 6 * p, s - 5 * p,
             s - 4 * p, s - 3 * p, s - 2 * p, s - 1 * p, s, s + 1 * p,
             s + 2 * p, s + 3 * p, s + 4 * p, s + 5 * p, s + 6 * p);
    ++s;
  }
}

void aom_lpf_horizontal_14_c(uint8_t *s, int p, const uint8_t *blimit,
                             const uint8_t *limit, const uint8_t *thresh) {
  mb_lpf_horizontal_edge_w(s, p, blimit, limit, thresh, 1);
}

void aom_lpf_horizontal_14_dual_c(uint8_t *s, int p, const uint8_t *blimit0,
                                  const uint8_t *limit0, const uint8_t *thresh0,
                                  const uint8_t *blimit1, const uint8_t *limit1,
                                  const uint8_t *thresh1) {
  mb_lpf_horizontal_edge_w(s, p, blimit0, limit0, thresh0, 1);
  mb_lpf_horizontal_edge_w(s + 4, p, blimit1, limit1, thresh1, 1);
}

void aom_lpf_horizontal_14_quad_c(uint8_t *s, int p, const uint8_t *blimit0,
                                  const uint8_t *limit0,
                                  const uint8_t *thresh0) {
  mb_lpf_horizontal_edge_w(s, p, blimit0, limit0, thresh0, 1);
  mb_lpf_horizontal_edge_w(s + 4, p, blimit0, limit0, thresh0, 1);
  mb_lpf_horizontal_edge_w(s + 8, p, blimit0, limit0, thresh0, 1);
  mb_lpf_horizontal_edge_w(s + 12, p, blimit0, limit0, thresh0, 1);
}

static void mb_lpf_vertical_edge_w(uint8_t *s, int p, const uint8_t *blimit,
                                   const uint8_t *limit, const uint8_t *thresh,
                                   int count) {
  int i;

  for (i = 0; i < count; ++i) {
    const uint8_t p6 = s[-7], p5 = s[-6], p4 = s[-5], p3 = s[-4], p2 = s[-3],
                  p1 = s[-2], p0 = s[-1];
    const uint8_t q0 = s[0], q1 = s[1], q2 = s[2], q3 = s[3], q4 = s[4],
                  q5 = s[5], q6 = s[6];
    const int8_t mask =
        filter_mask(*limit, *blimit, p3, p2, p1, p0, q0, q1, q2, q3);
    const int8_t flat = flat_mask4(1, p3, p2, p1, p0, q0, q1, q2, q3);
    const int8_t flat2 = flat_mask4(1, p6, p5, p4, p0, q0, q4, q5, q6);

    filter14(mask, *thresh, flat, flat2, s - 7, s - 6, s - 5, s - 4, s - 3,
             s - 2, s - 1, s, s + 1, s + 2, s + 3, s + 4, s + 5, s + 6);
    s += p;
  }
}

void aom_lpf_vertical_14_c(uint8_t *s, int p, const uint8_t *blimit,
                           const uint8_t *limit, const uint8_t *thresh) {
  mb_lpf_vertical_edge_w(s, p, blimit, limit, thresh, 4);
}

void aom_lpf_vertical_14_dual_c(uint8_t *s, int pitch, const uint8_t *blimit0,
                                const uint8_t *limit0, const uint8_t *thresh0,
                                const uint8_t *blimit1, const uint8_t *limit1,
                                const uint8_t *thresh1) {
  mb_lpf_vertical_edge_w(s, pitch, blimit0, limit0, thresh0, 4);
  mb_lpf_vertical_edge_w(s + 4 * pitch, pitch, blimit1, limit1, thresh1, 4);
}

void aom_lpf_vertical_14_quad_c(uint8_t *s, int pitch, const uint8_t *blimit0,
                                const uint8_t *limit0, const uint8_t *thresh0) {
  mb_lpf_vertical_edge_w(s, pitch, blimit0, limit0, thresh0, 4);
  mb_lpf_vertical_edge_w(s + 4 * pitch, pitch, blimit0, limit0, thresh0, 4);
  mb_lpf_vertical_edge_w(s + 8 * pitch, pitch, blimit0, limit0, thresh0, 4);
  mb_lpf_vertical_edge_w(s + 12 * pitch, pitch, blimit0, limit0, thresh0, 4);
}

#if CONFIG_AV1_HIGHBITDEPTH
// Should we apply any filter at all: 11111111 yes, 00000000 no ?
static INLINE int8_t highbd_filter_mask2(uint8_t limit, uint8_t blimit,
                                         uint16_t p1, uint16_t p0, uint16_t q0,
                                         uint16_t q1, int bd) {
  int8_t mask = 0;
  int16_t limit16 = (uint16_t)limit << (bd - 8);
  int16_t blimit16 = (uint16_t)blimit << (bd - 8);
  mask |= (abs(p1 - p0) > limit16) * -1;
  mask |= (abs(q1 - q0) > limit16) * -1;
  mask |= (abs(p0 - q0) * 2 + abs(p1 - q1) / 2 > blimit16) * -1;
  return ~mask;
}

// Should we apply any filter at all: 11111111 yes, 00000000 no ?
static INLINE int8_t highbd_filter_mask(uint8_t limit, uint8_t blimit,
                                        uint16_t p3, uint16_t p2, uint16_t p1,
                                        uint16_t p0, uint16_t q0, uint16_t q1,
                                        uint16_t q2, uint16_t q3, int bd) {
  int8_t mask = 0;
  int16_t limit16 = (uint16_t)limit << (bd - 8);
  int16_t blimit16 = (uint16_t)blimit << (bd - 8);
  mask |= (abs(p3 - p2) > limit16) * -1;
  mask |= (abs(p2 - p1) > limit16) * -1;
  mask |= (abs(p1 - p0) > limit16) * -1;
  mask |= (abs(q1 - q0) > limit16) * -1;
  mask |= (abs(q2 - q1) > limit16) * -1;
  mask |= (abs(q3 - q2) > limit16) * -1;
  mask |= (abs(p0 - q0) * 2 + abs(p1 - q1) / 2 > blimit16) * -1;
  return ~mask;
}

static INLINE int8_t highbd_filter_mask3_chroma(uint8_t limit, uint8_t blimit,
                                                uint16_t p2, uint16_t p1,
                                                uint16_t p0, uint16_t q0,
                                                uint16_t q1, uint16_t q2,
                                                int bd) {
  int8_t mask = 0;
  int16_t limit16 = (uint16_t)limit << (bd - 8);
  int16_t blimit16 = (uint16_t)blimit << (bd - 8);
  mask |= (abs(p2 - p1) > limit16) * -1;
  mask |= (abs(p1 - p0) > limit16) * -1;
  mask |= (abs(q1 - q0) > limit16) * -1;
  mask |= (abs(q2 - q1) > limit16) * -1;
  mask |= (abs(p0 - q0) * 2 + abs(p1 - q1) / 2 > blimit16) * -1;
  return ~mask;
}

static INLINE int8_t highbd_flat_mask3_chroma(uint8_t thresh, uint16_t p2,
                                              uint16_t p1, uint16_t p0,
                                              uint16_t q0, uint16_t q1,
                                              uint16_t q2, int bd) {
  int8_t mask = 0;
  int16_t thresh16 = (uint16_t)thresh << (bd - 8);
  mask |= (abs(p1 - p0) > thresh16) * -1;
  mask |= (abs(q1 - q0) > thresh16) * -1;
  mask |= (abs(p2 - p0) > thresh16) * -1;
  mask |= (abs(q2 - q0) > thresh16) * -1;
  return ~mask;
}

static INLINE int8_t highbd_flat_mask4(uint8_t thresh, uint16_t p3, uint16_t p2,
                                       uint16_t p1, uint16_t p0, uint16_t q0,
                                       uint16_t q1, uint16_t q2, uint16_t q3,
                                       int bd) {
  int8_t mask = 0;
  int16_t thresh16 = (uint16_t)thresh << (bd - 8);
  mask |= (abs(p1 - p0) > thresh16) * -1;
  mask |= (abs(q1 - q0) > thresh16) * -1;
  mask |= (abs(p2 - p0) > thresh16) * -1;
  mask |= (abs(q2 - q0) > thresh16) * -1;
  mask |= (abs(p3 - p0) > thresh16) * -1;
  mask |= (abs(q3 - q0) > thresh16) * -1;
  return ~mask;
}

// Is there high edge variance internal edge:
// 11111111_11111111 yes, 00000000_00000000 no ?
static INLINE int16_t highbd_hev_mask(uint8_t thresh, uint16_t p1, uint16_t p0,
                                      uint16_t q0, uint16_t q1, int bd) {
  int16_t hev = 0;
  int16_t thresh16 = (uint16_t)thresh << (bd - 8);
  hev |= (abs(p1 - p0) > thresh16) * -1;
  hev |= (abs(q1 - q0) > thresh16) * -1;
  return hev;
}

static INLINE void highbd_filter4(int8_t mask, uint8_t thresh, uint16_t *op1,
                                  uint16_t *op0, uint16_t *oq0, uint16_t *oq1,
                                  int bd) {
  int16_t filter1, filter2;
  // ^0x80 equivalent to subtracting 0x80 from the values to turn them
  // into -128 to +127 instead of 0 to 255.
  int shift = bd - 8;
  const int16_t ps1 = (int16_t)*op1 - (0x80 << shift);
  const int16_t ps0 = (int16_t)*op0 - (0x80 << shift);
  const int16_t qs0 = (int16_t)*oq0 - (0x80 << shift);
  const int16_t qs1 = (int16_t)*oq1 - (0x80 << shift);
  const int16_t hev = highbd_hev_mask(thresh, *op1, *op0, *oq0, *oq1, bd);

  // Add outer taps if we have high edge variance.
  int16_t filter = signed_char_clamp_high(ps1 - qs1, bd) & hev;

  // Inner taps.
  filter = signed_char_clamp_high(filter + 3 * (qs0 - ps0), bd) & mask;

  // Save bottom 3 bits so that we round one side +4 and the other +3
  // if it equals 4 we'll set to adjust by -1 to account for the fact
  // we'd round 3 the other way.
  filter1 = signed_char_clamp_high(filter + 4, bd) >> 3;
  filter2 = signed_char_clamp_high(filter + 3, bd) >> 3;

  *oq0 = signed_char_clamp_high(qs0 - filter1, bd) + (0x80 << shift);
  *op0 = signed_char_clamp_high(ps0 + filter2, bd) + (0x80 << shift);

  // Outer tap adjustments.
  filter = ROUND_POWER_OF_TWO(filter1, 1) & ~hev;

  *oq1 = signed_char_clamp_high(qs1 - filter, bd) + (0x80 << shift);
  *op1 = signed_char_clamp_high(ps1 + filter, bd) + (0x80 << shift);
}

void aom_highbd_lpf_horizontal_4_c(uint16_t *s, int p /* pitch */,
                                   const uint8_t *blimit, const uint8_t *limit,
                                   const uint8_t *thresh, int bd) {
  int i;
  int count = 4;

  // loop filter designed to work using chars so that we can make maximum use
  // of 8 bit simd instructions.
  for (i = 0; i < count; ++i) {
    const uint16_t p1 = s[-2 * p];
    const uint16_t p0 = s[-p];
    const uint16_t q0 = s[0 * p];
    const uint16_t q1 = s[1 * p];
    const int8_t mask =
        highbd_filter_mask2(*limit, *blimit, p1, p0, q0, q1, bd);
    highbd_filter4(mask, *thresh, s - 2 * p, s - 1 * p, s, s + 1 * p, bd);
    ++s;
  }
}

void aom_highbd_lpf_horizontal_4_dual_c(
    uint16_t *s, int p, const uint8_t *blimit0, const uint8_t *limit0,
    const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1,
    const uint8_t *thresh1, int bd) {
  aom_highbd_lpf_horizontal_4_c(s, p, blimit0, limit0, thresh0, bd);
  aom_highbd_lpf_horizontal_4_c(s + 4, p, blimit1, limit1, thresh1, bd);
}

void aom_highbd_lpf_vertical_4_c(uint16_t *s, int pitch, const uint8_t *blimit,
                                 const uint8_t *limit, const uint8_t *thresh,
                                 int bd) {
  int i;
  int count = 4;

  // loop filter designed to work using chars so that we can make maximum use
  // of 8 bit simd instructions.
  for (i = 0; i < count; ++i) {
    const uint16_t p1 = s[-2], p0 = s[-1];
    const uint16_t q0 = s[0], q1 = s[1];
    const int8_t mask =
        highbd_filter_mask2(*limit, *blimit, p1, p0, q0, q1, bd);
    highbd_filter4(mask, *thresh, s - 2, s - 1, s, s + 1, bd);
    s += pitch;
  }
}

void aom_highbd_lpf_vertical_4_dual_c(
    uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0,
    const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1,
    const uint8_t *thresh1, int bd) {
  aom_highbd_lpf_vertical_4_c(s, pitch, blimit0, limit0, thresh0, bd);
  aom_highbd_lpf_vertical_4_c(s + 4 * pitch, pitch, blimit1, limit1, thresh1,
                              bd);
}

static INLINE void highbd_filter6(int8_t mask, uint8_t thresh, int8_t flat,
                                  uint16_t *op2, uint16_t *op1, uint16_t *op0,
                                  uint16_t *oq0, uint16_t *oq1, uint16_t *oq2,
                                  int bd) {
  if (flat && mask) {
    const uint16_t p2 = *op2, p1 = *op1, p0 = *op0;
    const uint16_t q0 = *oq0, q1 = *oq1, q2 = *oq2;

    // 5-tap filter [1, 2, 2, 2, 1]
    *op1 = ROUND_POWER_OF_TWO(p2 * 3 + p1 * 2 + p0 * 2 + q0, 3);
    *op0 = ROUND_POWER_OF_TWO(p2 + p1 * 2 + p0 * 2 + q0 * 2 + q1, 3);
    *oq0 = ROUND_POWER_OF_TWO(p1 + p0 * 2 + q0 * 2 + q1 * 2 + q2, 3);
    *oq1 = ROUND_POWER_OF_TWO(p0 + q0 * 2 + q1 * 2 + q2 * 3, 3);
  } else {
    highbd_filter4(mask, thresh, op1, op0, oq0, oq1, bd);
  }
}

static INLINE void highbd_filter8(int8_t mask, uint8_t thresh, int8_t flat,
                                  uint16_t *op3, uint16_t *op2, uint16_t *op1,
                                  uint16_t *op0, uint16_t *oq0, uint16_t *oq1,
                                  uint16_t *oq2, uint16_t *oq3, int bd) {
  if (flat && mask) {
    const uint16_t p3 = *op3, p2 = *op2, p1 = *op1, p0 = *op0;
    const uint16_t q0 = *oq0, q1 = *oq1, q2 = *oq2, q3 = *oq3;

    // 7-tap filter [1, 1, 1, 2, 1, 1, 1]
    *op2 = ROUND_POWER_OF_TWO(p3 + p3 + p3 + 2 * p2 + p1 + p0 + q0, 3);
    *op1 = ROUND_POWER_OF_TWO(p3 + p3 + p2 + 2 * p1 + p0 + q0 + q1, 3);
    *op0 = ROUND_POWER_OF_TWO(p3 + p2 + p1 + 2 * p0 + q0 + q1 + q2, 3);
    *oq0 = ROUND_POWER_OF_TWO(p2 + p1 + p0 + 2 * q0 + q1 + q2 + q3, 3);
    *oq1 = ROUND_POWER_OF_TWO(p1 + p0 + q0 + 2 * q1 + q2 + q3 + q3, 3);
    *oq2 = ROUND_POWER_OF_TWO(p0 + q0 + q1 + 2 * q2 + q3 + q3 + q3, 3);
  } else {
    highbd_filter4(mask, thresh, op1, op0, oq0, oq1, bd);
  }
}

void aom_highbd_lpf_horizontal_8_c(uint16_t *s, int p, const uint8_t *blimit,
                                   const uint8_t *limit, const uint8_t *thresh,
                                   int bd) {
  int i;
  int count = 4;

  // loop filter designed to work using chars so that we can make maximum use
  // of 8 bit simd instructions.
  for (i = 0; i < count; ++i) {
    const uint16_t p3 = s[-4 * p], p2 = s[-3 * p], p1 = s[-2 * p], p0 = s[-p];
    const uint16_t q0 = s[0 * p], q1 = s[1 * p], q2 = s[2 * p], q3 = s[3 * p];

    const int8_t mask =
        highbd_filter_mask(*limit, *blimit, p3, p2, p1, p0, q0, q1, q2, q3, bd);
    const int8_t flat =
        highbd_flat_mask4(1, p3, p2, p1, p0, q0, q1, q2, q3, bd);
    highbd_filter8(mask, *thresh, flat, s - 4 * p, s - 3 * p, s - 2 * p,
                   s - 1 * p, s, s + 1 * p, s + 2 * p, s + 3 * p, bd);
    ++s;
  }
}

void aom_highbd_lpf_horizontal_6_c(uint16_t *s, int p, const uint8_t *blimit,
                                   const uint8_t *limit, const uint8_t *thresh,
                                   int bd) {
  int i;
  int count = 4;

  // loop filter designed to work using chars so that we can make maximum use
  // of 8 bit simd instructions.
  for (i = 0; i < count; ++i) {
    const uint16_t p2 = s[-3 * p], p1 = s[-2 * p], p0 = s[-p];
    const uint16_t q0 = s[0 * p], q1 = s[1 * p], q2 = s[2 * p];

    const int8_t mask =
        highbd_filter_mask3_chroma(*limit, *blimit, p2, p1, p0, q0, q1, q2, bd);
    const int8_t flat = highbd_flat_mask3_chroma(1, p2, p1, p0, q0, q1, q2, bd);
    highbd_filter6(mask, *thresh, flat, s - 3 * p, s - 2 * p, s - 1 * p, s,
                   s + 1 * p, s + 2 * p, bd);
    ++s;
  }
}

void aom_highbd_lpf_horizontal_6_dual_c(
    uint16_t *s, int p, const uint8_t *blimit0, const uint8_t *limit0,
    const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1,
    const uint8_t *thresh1, int bd) {
  aom_highbd_lpf_horizontal_6_c(s, p, blimit0, limit0, thresh0, bd);
  aom_highbd_lpf_horizontal_6_c(s + 4, p, blimit1, limit1, thresh1, bd);
}

void aom_highbd_lpf_horizontal_8_dual_c(
    uint16_t *s, int p, const uint8_t *blimit0, const uint8_t *limit0,
    const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1,
    const uint8_t *thresh1, int bd) {
  aom_highbd_lpf_horizontal_8_c(s, p, blimit0, limit0, thresh0, bd);
  aom_highbd_lpf_horizontal_8_c(s + 4, p, blimit1, limit1, thresh1, bd);
}

void aom_highbd_lpf_vertical_6_c(uint16_t *s, int pitch, const uint8_t *blimit,
                                 const uint8_t *limit, const uint8_t *thresh,
                                 int bd) {
  int i;
  int count = 4;

  for (i = 0; i < count; ++i) {
    const uint16_t p2 = s[-3], p1 = s[-2], p0 = s[-1];
    const uint16_t q0 = s[0], q1 = s[1], q2 = s[2];
    const int8_t mask =
        highbd_filter_mask3_chroma(*limit, *blimit, p2, p1, p0, q0, q1, q2, bd);
    const int8_t flat = highbd_flat_mask3_chroma(1, p2, p1, p0, q0, q1, q2, bd);
    highbd_filter6(mask, *thresh, flat, s - 3, s - 2, s - 1, s, s + 1, s + 2,
                   bd);
    s += pitch;
  }
}

void aom_highbd_lpf_vertical_6_dual_c(
    uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0,
    const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1,
    const uint8_t *thresh1, int bd) {
  aom_highbd_lpf_vertical_6_c(s, pitch, blimit0, limit0, thresh0, bd);
  aom_highbd_lpf_vertical_6_c(s + 4 * pitch, pitch, blimit1, limit1, thresh1,
                              bd);
}

void aom_highbd_lpf_vertical_8_c(uint16_t *s, int pitch, const uint8_t *blimit,
                                 const uint8_t *limit, const uint8_t *thresh,
                                 int bd) {
  int i;
  int count = 4;

  for (i = 0; i < count; ++i) {
    const uint16_t p3 = s[-4], p2 = s[-3], p1 = s[-2], p0 = s[-1];
    const uint16_t q0 = s[0], q1 = s[1], q2 = s[2], q3 = s[3];
    const int8_t mask =
        highbd_filter_mask(*limit, *blimit, p3, p2, p1, p0, q0, q1, q2, q3, bd);
    const int8_t flat =
        highbd_flat_mask4(1, p3, p2, p1, p0, q0, q1, q2, q3, bd);
    highbd_filter8(mask, *thresh, flat, s - 4, s - 3, s - 2, s - 1, s, s + 1,
                   s + 2, s + 3, bd);
    s += pitch;
  }
}

void aom_highbd_lpf_vertical_8_dual_c(
    uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0,
    const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1,
    const uint8_t *thresh1, int bd) {
  aom_highbd_lpf_vertical_8_c(s, pitch, blimit0, limit0, thresh0, bd);
  aom_highbd_lpf_vertical_8_c(s + 4 * pitch, pitch, blimit1, limit1, thresh1,
                              bd);
}

static INLINE void highbd_filter14(int8_t mask, uint8_t thresh, int8_t flat,
                                   int8_t flat2, uint16_t *op6, uint16_t *op5,
                                   uint16_t *op4, uint16_t *op3, uint16_t *op2,
                                   uint16_t *op1, uint16_t *op0, uint16_t *oq0,
                                   uint16_t *oq1, uint16_t *oq2, uint16_t *oq3,
                                   uint16_t *oq4, uint16_t *oq5, uint16_t *oq6,
                                   int bd) {
  if (flat2 && flat && mask) {
    const uint16_t p6 = *op6;
    const uint16_t p5 = *op5;
    const uint16_t p4 = *op4;
    const uint16_t p3 = *op3;
    const uint16_t p2 = *op2;
    const uint16_t p1 = *op1;
    const uint16_t p0 = *op0;
    const uint16_t q0 = *oq0;
    const uint16_t q1 = *oq1;
    const uint16_t q2 = *oq2;
    const uint16_t q3 = *oq3;
    const uint16_t q4 = *oq4;
    const uint16_t q5 = *oq5;
    const uint16_t q6 = *oq6;

    // 13-tap filter [1, 1, 1, 1, 1, 2, 2, 2, 1, 1, 1, 1, 1]
    *op5 = ROUND_POWER_OF_TWO(p6 * 7 + p5 * 2 + p4 * 2 + p3 + p2 + p1 + p0 + q0,
                              4);
    *op4 = ROUND_POWER_OF_TWO(
        p6 * 5 + p5 * 2 + p4 * 2 + p3 * 2 + p2 + p1 + p0 + q0 + q1, 4);
    *op3 = ROUND_POWER_OF_TWO(
        p6 * 4 + p5 + p4 * 2 + p3 * 2 + p2 * 2 + p1 + p0 + q0 + q1 + q2, 4);
    *op2 = ROUND_POWER_OF_TWO(
        p6 * 3 + p5 + p4 + p3 * 2 + p2 * 2 + p1 * 2 + p0 + q0 + q1 + q2 + q3,
        4);
    *op1 = ROUND_POWER_OF_TWO(p6 * 2 + p5 + p4 + p3 + p2 * 2 + p1 * 2 + p0 * 2 +
                                  q0 + q1 + q2 + q3 + q4,
                              4);
    *op0 = ROUND_POWER_OF_TWO(p6 + p5 + p4 + p3 + p2 + p1 * 2 + p0 * 2 +
                                  q0 * 2 + q1 + q2 + q3 + q4 + q5,
                              4);
    *oq0 = ROUND_POWER_OF_TWO(p5 + p4 + p3 + p2 + p1 + p0 * 2 + q0 * 2 +
                                  q1 * 2 + q2 + q3 + q4 + q5 + q6,
                              4);
    *oq1 = ROUND_POWER_OF_TWO(p4 + p3 + p2 + p1 + p0 + q0 * 2 + q1 * 2 +
                                  q2 * 2 + q3 + q4 + q5 + q6 * 2,
                              4);
    *oq2 = ROUND_POWER_OF_TWO(
        p3 + p2 + p1 + p0 + q0 + q1 * 2 + q2 * 2 + q3 * 2 + q4 + q5 + q6 * 3,
        4);
    *oq3 = ROUND_POWER_OF_TWO(
        p2 + p1 + p0 + q0 + q1 + q2 * 2 + q3 * 2 + q4 * 2 + q5 + q6 * 4, 4);
    *oq4 = ROUND_POWER_OF_TWO(
        p1 + p0 + q0 + q1 + q2 + q3 * 2 + q4 * 2 + q5 * 2 + q6 * 5, 4);
    *oq5 = ROUND_POWER_OF_TWO(p0 + q0 + q1 + q2 + q3 + q4 * 2 + q5 * 2 + q6 * 7,
                              4);
  } else {
    highbd_filter8(mask, thresh, flat, op3, op2, op1, op0, oq0, oq1, oq2, oq3,
                   bd);
  }
}

static void highbd_mb_lpf_horizontal_edge_w(uint16_t *s, int p,
                                            const uint8_t *blimit,
                                            const uint8_t *limit,
                                            const uint8_t *thresh, int count,
                                            int bd) {
  int i;
  int step = 4;

  // loop filter designed to work using chars so that we can make maximum use
  // of 8 bit simd instructions.
  for (i = 0; i < step * count; ++i) {
    const uint16_t p3 = s[-4 * p];
    const uint16_t p2 = s[-3 * p];
    const uint16_t p1 = s[-2 * p];
    const uint16_t p0 = s[-p];
    const uint16_t q0 = s[0 * p];
    const uint16_t q1 = s[1 * p];
    const uint16_t q2 = s[2 * p];
    const uint16_t q3 = s[3 * p];
    const int8_t mask =
        highbd_filter_mask(*limit, *blimit, p3, p2, p1, p0, q0, q1, q2, q3, bd);
    const int8_t flat =
        highbd_flat_mask4(1, p3, p2, p1, p0, q0, q1, q2, q3, bd);

    const int8_t flat2 =
        highbd_flat_mask4(1, s[-7 * p], s[-6 * p], s[-5 * p], p0, q0, s[4 * p],
                          s[5 * p], s[6 * p], bd);

    highbd_filter14(mask, *thresh, flat, flat2, s - 7 * p, s - 6 * p, s - 5 * p,
                    s - 4 * p, s - 3 * p, s - 2 * p, s - 1 * p, s, s + 1 * p,
                    s + 2 * p, s + 3 * p, s + 4 * p, s + 5 * p, s + 6 * p, bd);
    ++s;
  }
}

void aom_highbd_lpf_horizontal_14_c(uint16_t *s, int pitch,
                                    const uint8_t *blimit, const uint8_t *limit,
                                    const uint8_t *thresh, int bd) {
  highbd_mb_lpf_horizontal_edge_w(s, pitch, blimit, limit, thresh, 1, bd);
}

void aom_highbd_lpf_horizontal_14_dual_c(
    uint16_t *s, int p, const uint8_t *blimit0, const uint8_t *limit0,
    const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1,
    const uint8_t *thresh1, int bd) {
  highbd_mb_lpf_horizontal_edge_w(s, p, blimit0, limit0, thresh0, 1, bd);
  highbd_mb_lpf_horizontal_edge_w(s + 4, p, blimit1, limit1, thresh1, 1, bd);
}

static void highbd_mb_lpf_vertical_edge_w(uint16_t *s, int p,
                                          const uint8_t *blimit,
                                          const uint8_t *limit,
                                          const uint8_t *thresh, int count,
                                          int bd) {
  int i;

  for (i = 0; i < count; ++i) {
    const uint16_t p3 = s[-4];
    const uint16_t p2 = s[-3];
    const uint16_t p1 = s[-2];
    const uint16_t p0 = s[-1];
    const uint16_t q0 = s[0];
    const uint16_t q1 = s[1];
    const uint16_t q2 = s[2];
    const uint16_t q3 = s[3];
    const int8_t mask =
        highbd_filter_mask(*limit, *blimit, p3, p2, p1, p0, q0, q1, q2, q3, bd);
    const int8_t flat =
        highbd_flat_mask4(1, p3, p2, p1, p0, q0, q1, q2, q3, bd);
    const int8_t flat2 =
        highbd_flat_mask4(1, s[-7], s[-6], s[-5], p0, q0, s[4], s[5], s[6], bd);

    highbd_filter14(mask, *thresh, flat, flat2, s - 7, s - 6, s - 5, s - 4,
                    s - 3, s - 2, s - 1, s, s + 1, s + 2, s + 3, s + 4, s + 5,
                    s + 6, bd);
    s += p;
  }
}

void aom_highbd_lpf_vertical_14_c(uint16_t *s, int p, const uint8_t *blimit,
                                  const uint8_t *limit, const uint8_t *thresh,
                                  int bd) {
  highbd_mb_lpf_vertical_edge_w(s, p, blimit, limit, thresh, 4, bd);
}

void aom_highbd_lpf_vertical_14_dual_c(
    uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0,
    const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1,
    const uint8_t *thresh1, int bd) {
  highbd_mb_lpf_vertical_edge_w(s, pitch, blimit0, limit0, thresh0, 4, bd);
  highbd_mb_lpf_vertical_edge_w(s + 4 * pitch, pitch, blimit1, limit1, thresh1,
                                4, bd);
}
#endif  // CONFIG_AV1_HIGHBITDEPTH
