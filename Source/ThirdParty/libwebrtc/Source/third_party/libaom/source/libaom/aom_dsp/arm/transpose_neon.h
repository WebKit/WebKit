/*
 *  Copyright (c) 2018, Alliance for Open Media. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AOM_AOM_DSP_ARM_TRANSPOSE_NEON_H_
#define AOM_AOM_DSP_ARM_TRANSPOSE_NEON_H_

#include <arm_neon.h>

// Swap high and low halves.
static INLINE uint16x8_t transpose64_u16q(const uint16x8_t a) {
  return vextq_u16(a, a, 4);
}

static INLINE void transpose_u8_8x8(uint8x8_t *a0, uint8x8_t *a1, uint8x8_t *a2,
                                    uint8x8_t *a3, uint8x8_t *a4, uint8x8_t *a5,
                                    uint8x8_t *a6, uint8x8_t *a7) {
  // Swap 8 bit elements. Goes from:
  // a0: 00 01 02 03 04 05 06 07
  // a1: 10 11 12 13 14 15 16 17
  // a2: 20 21 22 23 24 25 26 27
  // a3: 30 31 32 33 34 35 36 37
  // a4: 40 41 42 43 44 45 46 47
  // a5: 50 51 52 53 54 55 56 57
  // a6: 60 61 62 63 64 65 66 67
  // a7: 70 71 72 73 74 75 76 77
  // to:
  // b0.val[0]: 00 10 02 12 04 14 06 16  40 50 42 52 44 54 46 56
  // b0.val[1]: 01 11 03 13 05 15 07 17  41 51 43 53 45 55 47 57
  // b1.val[0]: 20 30 22 32 24 34 26 36  60 70 62 72 64 74 66 76
  // b1.val[1]: 21 31 23 33 25 35 27 37  61 71 63 73 65 75 67 77

  const uint8x16x2_t b0 =
      vtrnq_u8(vcombine_u8(*a0, *a4), vcombine_u8(*a1, *a5));
  const uint8x16x2_t b1 =
      vtrnq_u8(vcombine_u8(*a2, *a6), vcombine_u8(*a3, *a7));

  // Swap 16 bit elements resulting in:
  // c0.val[0]: 00 10 20 30 04 14 24 34  40 50 60 70 44 54 64 74
  // c0.val[1]: 02 12 22 32 06 16 26 36  42 52 62 72 46 56 66 76
  // c1.val[0]: 01 11 21 31 05 15 25 35  41 51 61 71 45 55 65 75
  // c1.val[1]: 03 13 23 33 07 17 27 37  43 53 63 73 47 57 67 77

  const uint16x8x2_t c0 = vtrnq_u16(vreinterpretq_u16_u8(b0.val[0]),
                                    vreinterpretq_u16_u8(b1.val[0]));
  const uint16x8x2_t c1 = vtrnq_u16(vreinterpretq_u16_u8(b0.val[1]),
                                    vreinterpretq_u16_u8(b1.val[1]));

  // Unzip 32 bit elements resulting in:
  // d0.val[0]: 00 10 20 30 40 50 60 70  01 11 21 31 41 51 61 71
  // d0.val[1]: 04 14 24 34 44 54 64 74  05 15 25 35 45 55 65 75
  // d1.val[0]: 02 12 22 32 42 52 62 72  03 13 23 33 43 53 63 73
  // d1.val[1]: 06 16 26 36 46 56 66 76  07 17 27 37 47 57 67 77
  const uint32x4x2_t d0 = vuzpq_u32(vreinterpretq_u32_u16(c0.val[0]),
                                    vreinterpretq_u32_u16(c1.val[0]));
  const uint32x4x2_t d1 = vuzpq_u32(vreinterpretq_u32_u16(c0.val[1]),
                                    vreinterpretq_u32_u16(c1.val[1]));

  *a0 = vreinterpret_u8_u32(vget_low_u32(d0.val[0]));
  *a1 = vreinterpret_u8_u32(vget_high_u32(d0.val[0]));
  *a2 = vreinterpret_u8_u32(vget_low_u32(d1.val[0]));
  *a3 = vreinterpret_u8_u32(vget_high_u32(d1.val[0]));
  *a4 = vreinterpret_u8_u32(vget_low_u32(d0.val[1]));
  *a5 = vreinterpret_u8_u32(vget_high_u32(d0.val[1]));
  *a6 = vreinterpret_u8_u32(vget_low_u32(d1.val[1]));
  *a7 = vreinterpret_u8_u32(vget_high_u32(d1.val[1]));
}

static INLINE void transpose_u8_8x4(uint8x8_t *a0, uint8x8_t *a1, uint8x8_t *a2,
                                    uint8x8_t *a3) {
  // Swap 8 bit elements. Goes from:
  // a0: 00 01 02 03 04 05 06 07
  // a1: 10 11 12 13 14 15 16 17
  // a2: 20 21 22 23 24 25 26 27
  // a3: 30 31 32 33 34 35 36 37
  // to:
  // b0.val[0]: 00 10 02 12 04 14 06 16
  // b0.val[1]: 01 11 03 13 05 15 07 17
  // b1.val[0]: 20 30 22 32 24 34 26 36
  // b1.val[1]: 21 31 23 33 25 35 27 37

  const uint8x8x2_t b0 = vtrn_u8(*a0, *a1);
  const uint8x8x2_t b1 = vtrn_u8(*a2, *a3);

  // Swap 16 bit elements resulting in:
  // c0.val[0]: 00 10 20 30 04 14 24 34
  // c0.val[1]: 02 12 22 32 06 16 26 36
  // c1.val[0]: 01 11 21 31 05 15 25 35
  // c1.val[1]: 03 13 23 33 07 17 27 37

  const uint16x4x2_t c0 =
      vtrn_u16(vreinterpret_u16_u8(b0.val[0]), vreinterpret_u16_u8(b1.val[0]));
  const uint16x4x2_t c1 =
      vtrn_u16(vreinterpret_u16_u8(b0.val[1]), vreinterpret_u16_u8(b1.val[1]));

  *a0 = vreinterpret_u8_u16(c0.val[0]);
  *a1 = vreinterpret_u8_u16(c1.val[0]);
  *a2 = vreinterpret_u8_u16(c0.val[1]);
  *a3 = vreinterpret_u8_u16(c1.val[1]);
}

static INLINE void transpose_u8_4x4(uint8x8_t *a0, uint8x8_t *a1) {
  // Swap 16 bit elements. Goes from:
  // a0: 00 01 02 03  10 11 12 13
  // a1: 20 21 22 23  30 31 32 33
  // to:
  // b0.val[0]: 00 01 20 21  10 11 30 31
  // b0.val[1]: 02 03 22 23  12 13 32 33

  const uint16x4x2_t b0 =
      vtrn_u16(vreinterpret_u16_u8(*a0), vreinterpret_u16_u8(*a1));

  // Swap 32 bit elements resulting in:
  // c0.val[0]: 00 01 20 21  02 03 22 23
  // c0.val[1]: 10 11 30 31  12 13 32 33

  const uint32x2x2_t c0 = vtrn_u32(vreinterpret_u32_u16(b0.val[0]),
                                   vreinterpret_u32_u16(b0.val[1]));

  // Swap 8 bit elements resulting in:
  // d0.val[0]: 00 10 20 30  02 12 22 32
  // d0.val[1]: 01 11 21 31  03 13 23 33

  const uint8x8x2_t d0 =
      vtrn_u8(vreinterpret_u8_u32(c0.val[0]), vreinterpret_u8_u32(c0.val[1]));

  *a0 = d0.val[0];
  *a1 = d0.val[1];
}

static INLINE void transpose_u8_4x8(uint8x8_t *a0, uint8x8_t *a1, uint8x8_t *a2,
                                    uint8x8_t *a3, const uint8x8_t a4,
                                    const uint8x8_t a5, const uint8x8_t a6,
                                    const uint8x8_t a7) {
  // Swap 32 bit elements. Goes from:
  // a0: 00 01 02 03 XX XX XX XX
  // a1: 10 11 12 13 XX XX XX XX
  // a2: 20 21 22 23 XX XX XX XX
  // a3; 30 31 32 33 XX XX XX XX
  // a4: 40 41 42 43 XX XX XX XX
  // a5: 50 51 52 53 XX XX XX XX
  // a6: 60 61 62 63 XX XX XX XX
  // a7: 70 71 72 73 XX XX XX XX
  // to:
  // b0.val[0]: 00 01 02 03 40 41 42 43
  // b1.val[0]: 10 11 12 13 50 51 52 53
  // b2.val[0]: 20 21 22 23 60 61 62 63
  // b3.val[0]: 30 31 32 33 70 71 72 73

  const uint32x2x2_t b0 =
      vtrn_u32(vreinterpret_u32_u8(*a0), vreinterpret_u32_u8(a4));
  const uint32x2x2_t b1 =
      vtrn_u32(vreinterpret_u32_u8(*a1), vreinterpret_u32_u8(a5));
  const uint32x2x2_t b2 =
      vtrn_u32(vreinterpret_u32_u8(*a2), vreinterpret_u32_u8(a6));
  const uint32x2x2_t b3 =
      vtrn_u32(vreinterpret_u32_u8(*a3), vreinterpret_u32_u8(a7));

  // Swap 16 bit elements resulting in:
  // c0.val[0]: 00 01 20 21 40 41 60 61
  // c0.val[1]: 02 03 22 23 42 43 62 63
  // c1.val[0]: 10 11 30 31 50 51 70 71
  // c1.val[1]: 12 13 32 33 52 53 72 73

  const uint16x4x2_t c0 = vtrn_u16(vreinterpret_u16_u32(b0.val[0]),
                                   vreinterpret_u16_u32(b2.val[0]));
  const uint16x4x2_t c1 = vtrn_u16(vreinterpret_u16_u32(b1.val[0]),
                                   vreinterpret_u16_u32(b3.val[0]));

  // Swap 8 bit elements resulting in:
  // d0.val[0]: 00 10 20 30 40 50 60 70
  // d0.val[1]: 01 11 21 31 41 51 61 71
  // d1.val[0]: 02 12 22 32 42 52 62 72
  // d1.val[1]: 03 13 23 33 43 53 63 73

  const uint8x8x2_t d0 =
      vtrn_u8(vreinterpret_u8_u16(c0.val[0]), vreinterpret_u8_u16(c1.val[0]));
  const uint8x8x2_t d1 =
      vtrn_u8(vreinterpret_u8_u16(c0.val[1]), vreinterpret_u8_u16(c1.val[1]));

  *a0 = d0.val[0];
  *a1 = d0.val[1];
  *a2 = d1.val[0];
  *a3 = d1.val[1];
}

// Input:
// 00 01 02 03
// 10 11 12 13
// 20 21 22 23
// 30 31 32 33
// Output:
// 00 10 20 30
// 01 11 21 31
// 02 12 22 32
// 03 13 23 33
static INLINE void transpose_u16_4x4(uint16x4_t a[4]) {
  // b:
  // 00 10 02 12
  // 01 11 03 13
  const uint16x4x2_t b = vtrn_u16(a[0], a[1]);
  // c:
  // 20 30 22 32
  // 21 31 23 33
  const uint16x4x2_t c = vtrn_u16(a[2], a[3]);
  // d:
  // 00 10 20 30
  // 02 12 22 32
  const uint32x2x2_t d =
      vtrn_u32(vreinterpret_u32_u16(b.val[0]), vreinterpret_u32_u16(c.val[0]));
  // e:
  // 01 11 21 31
  // 03 13 23 33
  const uint32x2x2_t e =
      vtrn_u32(vreinterpret_u32_u16(b.val[1]), vreinterpret_u32_u16(c.val[1]));
  a[0] = vreinterpret_u16_u32(d.val[0]);
  a[1] = vreinterpret_u16_u32(e.val[0]);
  a[2] = vreinterpret_u16_u32(d.val[1]);
  a[3] = vreinterpret_u16_u32(e.val[1]);
}

// 4x8 Input:
// a[0]: 00 01 02 03 04 05 06 07
// a[1]: 10 11 12 13 14 15 16 17
// a[2]: 20 21 22 23 24 25 26 27
// a[3]: 30 31 32 33 34 35 36 37
// 8x4 Output:
// a[0]: 00 10 20 30 04 14 24 34
// a[1]: 01 11 21 31 05 15 25 35
// a[2]: 02 12 22 32 06 16 26 36
// a[3]: 03 13 23 33 07 17 27 37
static INLINE void transpose_u16_4x8q(uint16x8_t a[4]) {
  // b0.val[0]: 00 10 02 12 04 14 06 16
  // b0.val[1]: 01 11 03 13 05 15 07 17
  // b1.val[0]: 20 30 22 32 24 34 26 36
  // b1.val[1]: 21 31 23 33 25 35 27 37
  const uint16x8x2_t b0 = vtrnq_u16(a[0], a[1]);
  const uint16x8x2_t b1 = vtrnq_u16(a[2], a[3]);

  // c0.val[0]: 00 10 20 30 04 14 24 34
  // c0.val[1]: 02 12 22 32 06 16 26 36
  // c1.val[0]: 01 11 21 31 05 15 25 35
  // c1.val[1]: 03 13 23 33 07 17 27 37
  const uint32x4x2_t c0 = vtrnq_u32(vreinterpretq_u32_u16(b0.val[0]),
                                    vreinterpretq_u32_u16(b1.val[0]));
  const uint32x4x2_t c1 = vtrnq_u32(vreinterpretq_u32_u16(b0.val[1]),
                                    vreinterpretq_u32_u16(b1.val[1]));

  a[0] = vreinterpretq_u16_u32(c0.val[0]);
  a[1] = vreinterpretq_u16_u32(c1.val[0]);
  a[2] = vreinterpretq_u16_u32(c0.val[1]);
  a[3] = vreinterpretq_u16_u32(c1.val[1]);
}

static INLINE uint16x8x2_t aom_vtrnq_u64_to_u16(uint32x4_t a0, uint32x4_t a1) {
  uint16x8x2_t b0;
#if defined(__aarch64__)
  b0.val[0] = vreinterpretq_u16_u64(
      vtrn1q_u64(vreinterpretq_u64_u32(a0), vreinterpretq_u64_u32(a1)));
  b0.val[1] = vreinterpretq_u16_u64(
      vtrn2q_u64(vreinterpretq_u64_u32(a0), vreinterpretq_u64_u32(a1)));
#else
  b0.val[0] = vcombine_u16(vreinterpret_u16_u32(vget_low_u32(a0)),
                           vreinterpret_u16_u32(vget_low_u32(a1)));
  b0.val[1] = vcombine_u16(vreinterpret_u16_u32(vget_high_u32(a0)),
                           vreinterpret_u16_u32(vget_high_u32(a1)));
#endif
  return b0;
}

// Special transpose for loop filter.
// 4x8 Input:
// p_q:  p3 p2 p1 p0 q0 q1 q2 q3
// a[0]: 00 01 02 03 04 05 06 07
// a[1]: 10 11 12 13 14 15 16 17
// a[2]: 20 21 22 23 24 25 26 27
// a[3]: 30 31 32 33 34 35 36 37
// 8x4 Output:
// a[0]: 03 13 23 33 04 14 24 34  p0q0
// a[1]: 02 12 22 32 05 15 25 35  p1q1
// a[2]: 01 11 21 31 06 16 26 36  p2q2
// a[3]: 00 10 20 30 07 17 27 37  p3q3
// Direct reapplication of the function will reset the high halves, but
// reverse the low halves:
// p_q:  p0 p1 p2 p3 q0 q1 q2 q3
// a[0]: 33 32 31 30 04 05 06 07
// a[1]: 23 22 21 20 14 15 16 17
// a[2]: 13 12 11 10 24 25 26 27
// a[3]: 03 02 01 00 34 35 36 37
// Simply reordering the inputs (3, 2, 1, 0) will reset the low halves, but
// reverse the high halves.
// The standard transpose_u16_4x8q will produce the same reversals, but with the
// order of the low halves also restored relative to the high halves. This is
// preferable because it puts all values from the same source row back together,
// but some post-processing is inevitable.
static INLINE void loop_filter_transpose_u16_4x8q(uint16x8_t a[4]) {
  // b0.val[0]: 00 10 02 12 04 14 06 16
  // b0.val[1]: 01 11 03 13 05 15 07 17
  // b1.val[0]: 20 30 22 32 24 34 26 36
  // b1.val[1]: 21 31 23 33 25 35 27 37
  const uint16x8x2_t b0 = vtrnq_u16(a[0], a[1]);
  const uint16x8x2_t b1 = vtrnq_u16(a[2], a[3]);

  // Reverse odd vectors to bring the appropriate items to the front of zips.
  // b0.val[0]: 00 10 02 12 04 14 06 16
  // r0       : 03 13 01 11 07 17 05 15
  // b1.val[0]: 20 30 22 32 24 34 26 36
  // r1       : 23 33 21 31 27 37 25 35
  const uint32x4_t r0 = vrev64q_u32(vreinterpretq_u32_u16(b0.val[1]));
  const uint32x4_t r1 = vrev64q_u32(vreinterpretq_u32_u16(b1.val[1]));

  // Zip to complete the halves.
  // c0.val[0]: 00 10 20 30 02 12 22 32  p3p1
  // c0.val[1]: 04 14 24 34 06 16 26 36  q0q2
  // c1.val[0]: 03 13 23 33 01 11 21 31  p0p2
  // c1.val[1]: 07 17 27 37 05 15 25 35  q3q1
  const uint32x4x2_t c0 = vzipq_u32(vreinterpretq_u32_u16(b0.val[0]),
                                    vreinterpretq_u32_u16(b1.val[0]));
  const uint32x4x2_t c1 = vzipq_u32(r0, r1);

  // d0.val[0]: 00 10 20 30 07 17 27 37  p3q3
  // d0.val[1]: 02 12 22 32 05 15 25 35  p1q1
  // d1.val[0]: 03 13 23 33 04 14 24 34  p0q0
  // d1.val[1]: 01 11 21 31 06 16 26 36  p2q2
  const uint16x8x2_t d0 = aom_vtrnq_u64_to_u16(c0.val[0], c1.val[1]);
  // The third row of c comes first here to swap p2 with q0.
  const uint16x8x2_t d1 = aom_vtrnq_u64_to_u16(c1.val[0], c0.val[1]);

  // 8x4 Output:
  // a[0]: 03 13 23 33 04 14 24 34  p0q0
  // a[1]: 02 12 22 32 05 15 25 35  p1q1
  // a[2]: 01 11 21 31 06 16 26 36  p2q2
  // a[3]: 00 10 20 30 07 17 27 37  p3q3
  a[0] = d1.val[0];  // p0q0
  a[1] = d0.val[1];  // p1q1
  a[2] = d1.val[1];  // p2q2
  a[3] = d0.val[0];  // p3q3
}

static INLINE void transpose_u16_4x8(uint16x4_t *a0, uint16x4_t *a1,
                                     uint16x4_t *a2, uint16x4_t *a3,
                                     uint16x4_t *a4, uint16x4_t *a5,
                                     uint16x4_t *a6, uint16x4_t *a7,
                                     uint16x8_t *o0, uint16x8_t *o1,
                                     uint16x8_t *o2, uint16x8_t *o3) {
  // Combine rows. Goes from:
  // a0: 00 01 02 03
  // a1: 10 11 12 13
  // a2: 20 21 22 23
  // a3: 30 31 32 33
  // a4: 40 41 42 43
  // a5: 50 51 52 53
  // a6: 60 61 62 63
  // a7: 70 71 72 73
  // to:
  // b0: 00 01 02 03 40 41 42 43
  // b1: 10 11 12 13 50 51 52 53
  // b2: 20 21 22 23 60 61 62 63
  // b3: 30 31 32 33 70 71 72 73

  const uint16x8_t b0 = vcombine_u16(*a0, *a4);
  const uint16x8_t b1 = vcombine_u16(*a1, *a5);
  const uint16x8_t b2 = vcombine_u16(*a2, *a6);
  const uint16x8_t b3 = vcombine_u16(*a3, *a7);

  // Swap 16 bit elements resulting in:
  // c0.val[0]: 00 10 02 12 40 50 42 52
  // c0.val[1]: 01 11 03 13 41 51 43 53
  // c1.val[0]: 20 30 22 32 60 70 62 72
  // c1.val[1]: 21 31 23 33 61 71 63 73

  const uint16x8x2_t c0 = vtrnq_u16(b0, b1);
  const uint16x8x2_t c1 = vtrnq_u16(b2, b3);

  // Swap 32 bit elements resulting in:
  // d0.val[0]: 00 10 20 30 40 50 60 70
  // d0.val[1]: 02 12 22 32 42 52 62 72
  // d1.val[0]: 01 11 21 31 41 51 61 71
  // d1.val[1]: 03 13 23 33 43 53 63 73

  const uint32x4x2_t d0 = vtrnq_u32(vreinterpretq_u32_u16(c0.val[0]),
                                    vreinterpretq_u32_u16(c1.val[0]));
  const uint32x4x2_t d1 = vtrnq_u32(vreinterpretq_u32_u16(c0.val[1]),
                                    vreinterpretq_u32_u16(c1.val[1]));

  *o0 = vreinterpretq_u16_u32(d0.val[0]);
  *o1 = vreinterpretq_u16_u32(d1.val[0]);
  *o2 = vreinterpretq_u16_u32(d0.val[1]);
  *o3 = vreinterpretq_u16_u32(d1.val[1]);
}

static INLINE void transpose_s16_4x8(int16x4_t *a0, int16x4_t *a1,
                                     int16x4_t *a2, int16x4_t *a3,
                                     int16x4_t *a4, int16x4_t *a5,
                                     int16x4_t *a6, int16x4_t *a7,
                                     int16x8_t *o0, int16x8_t *o1,
                                     int16x8_t *o2, int16x8_t *o3) {
  // Combine rows. Goes from:
  // a0: 00 01 02 03
  // a1: 10 11 12 13
  // a2: 20 21 22 23
  // a3: 30 31 32 33
  // a4: 40 41 42 43
  // a5: 50 51 52 53
  // a6: 60 61 62 63
  // a7: 70 71 72 73
  // to:
  // b0: 00 01 02 03 40 41 42 43
  // b1: 10 11 12 13 50 51 52 53
  // b2: 20 21 22 23 60 61 62 63
  // b3: 30 31 32 33 70 71 72 73

  const int16x8_t b0 = vcombine_s16(*a0, *a4);
  const int16x8_t b1 = vcombine_s16(*a1, *a5);
  const int16x8_t b2 = vcombine_s16(*a2, *a6);
  const int16x8_t b3 = vcombine_s16(*a3, *a7);

  // Swap 16 bit elements resulting in:
  // c0.val[0]: 00 10 02 12 40 50 42 52
  // c0.val[1]: 01 11 03 13 41 51 43 53
  // c1.val[0]: 20 30 22 32 60 70 62 72
  // c1.val[1]: 21 31 23 33 61 71 63 73

  const int16x8x2_t c0 = vtrnq_s16(b0, b1);
  const int16x8x2_t c1 = vtrnq_s16(b2, b3);

  // Swap 32 bit elements resulting in:
  // d0.val[0]: 00 10 20 30 40 50 60 70
  // d0.val[1]: 02 12 22 32 42 52 62 72
  // d1.val[0]: 01 11 21 31 41 51 61 71
  // d1.val[1]: 03 13 23 33 43 53 63 73

  const int32x4x2_t d0 = vtrnq_s32(vreinterpretq_s32_s16(c0.val[0]),
                                   vreinterpretq_s32_s16(c1.val[0]));
  const int32x4x2_t d1 = vtrnq_s32(vreinterpretq_s32_s16(c0.val[1]),
                                   vreinterpretq_s32_s16(c1.val[1]));

  *o0 = vreinterpretq_s16_s32(d0.val[0]);
  *o1 = vreinterpretq_s16_s32(d1.val[0]);
  *o2 = vreinterpretq_s16_s32(d0.val[1]);
  *o3 = vreinterpretq_s16_s32(d1.val[1]);
}

static INLINE void transpose_u16_8x8(uint16x8_t *a0, uint16x8_t *a1,
                                     uint16x8_t *a2, uint16x8_t *a3,
                                     uint16x8_t *a4, uint16x8_t *a5,
                                     uint16x8_t *a6, uint16x8_t *a7) {
  // Swap 16 bit elements. Goes from:
  // a0: 00 01 02 03 04 05 06 07
  // a1: 10 11 12 13 14 15 16 17
  // a2: 20 21 22 23 24 25 26 27
  // a3: 30 31 32 33 34 35 36 37
  // a4: 40 41 42 43 44 45 46 47
  // a5: 50 51 52 53 54 55 56 57
  // a6: 60 61 62 63 64 65 66 67
  // a7: 70 71 72 73 74 75 76 77
  // to:
  // b0.val[0]: 00 10 02 12 04 14 06 16
  // b0.val[1]: 01 11 03 13 05 15 07 17
  // b1.val[0]: 20 30 22 32 24 34 26 36
  // b1.val[1]: 21 31 23 33 25 35 27 37
  // b2.val[0]: 40 50 42 52 44 54 46 56
  // b2.val[1]: 41 51 43 53 45 55 47 57
  // b3.val[0]: 60 70 62 72 64 74 66 76
  // b3.val[1]: 61 71 63 73 65 75 67 77

  const uint16x8x2_t b0 = vtrnq_u16(*a0, *a1);
  const uint16x8x2_t b1 = vtrnq_u16(*a2, *a3);
  const uint16x8x2_t b2 = vtrnq_u16(*a4, *a5);
  const uint16x8x2_t b3 = vtrnq_u16(*a6, *a7);

  // Swap 32 bit elements resulting in:
  // c0.val[0]: 00 10 20 30 04 14 24 34
  // c0.val[1]: 02 12 22 32 06 16 26 36
  // c1.val[0]: 01 11 21 31 05 15 25 35
  // c1.val[1]: 03 13 23 33 07 17 27 37
  // c2.val[0]: 40 50 60 70 44 54 64 74
  // c2.val[1]: 42 52 62 72 46 56 66 76
  // c3.val[0]: 41 51 61 71 45 55 65 75
  // c3.val[1]: 43 53 63 73 47 57 67 77

  const uint32x4x2_t c0 = vtrnq_u32(vreinterpretq_u32_u16(b0.val[0]),
                                    vreinterpretq_u32_u16(b1.val[0]));
  const uint32x4x2_t c1 = vtrnq_u32(vreinterpretq_u32_u16(b0.val[1]),
                                    vreinterpretq_u32_u16(b1.val[1]));
  const uint32x4x2_t c2 = vtrnq_u32(vreinterpretq_u32_u16(b2.val[0]),
                                    vreinterpretq_u32_u16(b3.val[0]));
  const uint32x4x2_t c3 = vtrnq_u32(vreinterpretq_u32_u16(b2.val[1]),
                                    vreinterpretq_u32_u16(b3.val[1]));

  // Swap 64 bit elements resulting in:
  // d0.val[0]: 00 10 20 30 40 50 60 70
  // d0.val[1]: 04 14 24 34 44 54 64 74
  // d1.val[0]: 01 11 21 31 41 51 61 71
  // d1.val[1]: 05 15 25 35 45 55 65 75
  // d2.val[0]: 02 12 22 32 42 52 62 72
  // d2.val[1]: 06 16 26 36 46 56 66 76
  // d3.val[0]: 03 13 23 33 43 53 63 73
  // d3.val[1]: 07 17 27 37 47 57 67 77

  const uint16x8x2_t d0 = aom_vtrnq_u64_to_u16(c0.val[0], c2.val[0]);
  const uint16x8x2_t d1 = aom_vtrnq_u64_to_u16(c1.val[0], c3.val[0]);
  const uint16x8x2_t d2 = aom_vtrnq_u64_to_u16(c0.val[1], c2.val[1]);
  const uint16x8x2_t d3 = aom_vtrnq_u64_to_u16(c1.val[1], c3.val[1]);

  *a0 = d0.val[0];
  *a1 = d1.val[0];
  *a2 = d2.val[0];
  *a3 = d3.val[0];
  *a4 = d0.val[1];
  *a5 = d1.val[1];
  *a6 = d2.val[1];
  *a7 = d3.val[1];
}

static INLINE int16x8x2_t aom_vtrnq_s64_to_s16(int32x4_t a0, int32x4_t a1) {
  int16x8x2_t b0;
#if defined(__aarch64__)
  b0.val[0] = vreinterpretq_s16_s64(
      vtrn1q_s64(vreinterpretq_s64_s32(a0), vreinterpretq_s64_s32(a1)));
  b0.val[1] = vreinterpretq_s16_s64(
      vtrn2q_s64(vreinterpretq_s64_s32(a0), vreinterpretq_s64_s32(a1)));
#else
  b0.val[0] = vcombine_s16(vreinterpret_s16_s32(vget_low_s32(a0)),
                           vreinterpret_s16_s32(vget_low_s32(a1)));
  b0.val[1] = vcombine_s16(vreinterpret_s16_s32(vget_high_s32(a0)),
                           vreinterpret_s16_s32(vget_high_s32(a1)));
#endif
  return b0;
}

static INLINE void transpose_s16_8x8(int16x8_t *a0, int16x8_t *a1,
                                     int16x8_t *a2, int16x8_t *a3,
                                     int16x8_t *a4, int16x8_t *a5,
                                     int16x8_t *a6, int16x8_t *a7) {
  // Swap 16 bit elements. Goes from:
  // a0: 00 01 02 03 04 05 06 07
  // a1: 10 11 12 13 14 15 16 17
  // a2: 20 21 22 23 24 25 26 27
  // a3: 30 31 32 33 34 35 36 37
  // a4: 40 41 42 43 44 45 46 47
  // a5: 50 51 52 53 54 55 56 57
  // a6: 60 61 62 63 64 65 66 67
  // a7: 70 71 72 73 74 75 76 77
  // to:
  // b0.val[0]: 00 10 02 12 04 14 06 16
  // b0.val[1]: 01 11 03 13 05 15 07 17
  // b1.val[0]: 20 30 22 32 24 34 26 36
  // b1.val[1]: 21 31 23 33 25 35 27 37
  // b2.val[0]: 40 50 42 52 44 54 46 56
  // b2.val[1]: 41 51 43 53 45 55 47 57
  // b3.val[0]: 60 70 62 72 64 74 66 76
  // b3.val[1]: 61 71 63 73 65 75 67 77

  const int16x8x2_t b0 = vtrnq_s16(*a0, *a1);
  const int16x8x2_t b1 = vtrnq_s16(*a2, *a3);
  const int16x8x2_t b2 = vtrnq_s16(*a4, *a5);
  const int16x8x2_t b3 = vtrnq_s16(*a6, *a7);

  // Swap 32 bit elements resulting in:
  // c0.val[0]: 00 10 20 30 04 14 24 34
  // c0.val[1]: 02 12 22 32 06 16 26 36
  // c1.val[0]: 01 11 21 31 05 15 25 35
  // c1.val[1]: 03 13 23 33 07 17 27 37
  // c2.val[0]: 40 50 60 70 44 54 64 74
  // c2.val[1]: 42 52 62 72 46 56 66 76
  // c3.val[0]: 41 51 61 71 45 55 65 75
  // c3.val[1]: 43 53 63 73 47 57 67 77

  const int32x4x2_t c0 = vtrnq_s32(vreinterpretq_s32_s16(b0.val[0]),
                                   vreinterpretq_s32_s16(b1.val[0]));
  const int32x4x2_t c1 = vtrnq_s32(vreinterpretq_s32_s16(b0.val[1]),
                                   vreinterpretq_s32_s16(b1.val[1]));
  const int32x4x2_t c2 = vtrnq_s32(vreinterpretq_s32_s16(b2.val[0]),
                                   vreinterpretq_s32_s16(b3.val[0]));
  const int32x4x2_t c3 = vtrnq_s32(vreinterpretq_s32_s16(b2.val[1]),
                                   vreinterpretq_s32_s16(b3.val[1]));

  // Swap 64 bit elements resulting in:
  // d0.val[0]: 00 10 20 30 40 50 60 70
  // d0.val[1]: 04 14 24 34 44 54 64 74
  // d1.val[0]: 01 11 21 31 41 51 61 71
  // d1.val[1]: 05 15 25 35 45 55 65 75
  // d2.val[0]: 02 12 22 32 42 52 62 72
  // d2.val[1]: 06 16 26 36 46 56 66 76
  // d3.val[0]: 03 13 23 33 43 53 63 73
  // d3.val[1]: 07 17 27 37 47 57 67 77

  const int16x8x2_t d0 = aom_vtrnq_s64_to_s16(c0.val[0], c2.val[0]);
  const int16x8x2_t d1 = aom_vtrnq_s64_to_s16(c1.val[0], c3.val[0]);
  const int16x8x2_t d2 = aom_vtrnq_s64_to_s16(c0.val[1], c2.val[1]);
  const int16x8x2_t d3 = aom_vtrnq_s64_to_s16(c1.val[1], c3.val[1]);

  *a0 = d0.val[0];
  *a1 = d1.val[0];
  *a2 = d2.val[0];
  *a3 = d3.val[0];
  *a4 = d0.val[1];
  *a5 = d1.val[1];
  *a6 = d2.val[1];
  *a7 = d3.val[1];
}

static INLINE void transpose_s16_8x8q(int16x8_t *a, int16x8_t *out) {
  // Swap 16 bit elements. Goes from:
  // a0: 00 01 02 03 04 05 06 07
  // a1: 10 11 12 13 14 15 16 17
  // a2: 20 21 22 23 24 25 26 27
  // a3: 30 31 32 33 34 35 36 37
  // a4: 40 41 42 43 44 45 46 47
  // a5: 50 51 52 53 54 55 56 57
  // a6: 60 61 62 63 64 65 66 67
  // a7: 70 71 72 73 74 75 76 77
  // to:
  // b0.val[0]: 00 10 02 12 04 14 06 16
  // b0.val[1]: 01 11 03 13 05 15 07 17
  // b1.val[0]: 20 30 22 32 24 34 26 36
  // b1.val[1]: 21 31 23 33 25 35 27 37
  // b2.val[0]: 40 50 42 52 44 54 46 56
  // b2.val[1]: 41 51 43 53 45 55 47 57
  // b3.val[0]: 60 70 62 72 64 74 66 76
  // b3.val[1]: 61 71 63 73 65 75 67 77

  const int16x8x2_t b0 = vtrnq_s16(a[0], a[1]);
  const int16x8x2_t b1 = vtrnq_s16(a[2], a[3]);
  const int16x8x2_t b2 = vtrnq_s16(a[4], a[5]);
  const int16x8x2_t b3 = vtrnq_s16(a[6], a[7]);

  // Swap 32 bit elements resulting in:
  // c0.val[0]: 00 10 20 30 04 14 24 34
  // c0.val[1]: 02 12 22 32 06 16 26 36
  // c1.val[0]: 01 11 21 31 05 15 25 35
  // c1.val[1]: 03 13 23 33 07 17 27 37
  // c2.val[0]: 40 50 60 70 44 54 64 74
  // c2.val[1]: 42 52 62 72 46 56 66 76
  // c3.val[0]: 41 51 61 71 45 55 65 75
  // c3.val[1]: 43 53 63 73 47 57 67 77

  const int32x4x2_t c0 = vtrnq_s32(vreinterpretq_s32_s16(b0.val[0]),
                                   vreinterpretq_s32_s16(b1.val[0]));
  const int32x4x2_t c1 = vtrnq_s32(vreinterpretq_s32_s16(b0.val[1]),
                                   vreinterpretq_s32_s16(b1.val[1]));
  const int32x4x2_t c2 = vtrnq_s32(vreinterpretq_s32_s16(b2.val[0]),
                                   vreinterpretq_s32_s16(b3.val[0]));
  const int32x4x2_t c3 = vtrnq_s32(vreinterpretq_s32_s16(b2.val[1]),
                                   vreinterpretq_s32_s16(b3.val[1]));

  // Swap 64 bit elements resulting in:
  // d0.val[0]: 00 10 20 30 40 50 60 70
  // d0.val[1]: 04 14 24 34 44 54 64 74
  // d1.val[0]: 01 11 21 31 41 51 61 71
  // d1.val[1]: 05 15 25 35 45 55 65 75
  // d2.val[0]: 02 12 22 32 42 52 62 72
  // d2.val[1]: 06 16 26 36 46 56 66 76
  // d3.val[0]: 03 13 23 33 43 53 63 73
  // d3.val[1]: 07 17 27 37 47 57 67 77

  const int16x8x2_t d0 = aom_vtrnq_s64_to_s16(c0.val[0], c2.val[0]);
  const int16x8x2_t d1 = aom_vtrnq_s64_to_s16(c1.val[0], c3.val[0]);
  const int16x8x2_t d2 = aom_vtrnq_s64_to_s16(c0.val[1], c2.val[1]);
  const int16x8x2_t d3 = aom_vtrnq_s64_to_s16(c1.val[1], c3.val[1]);

  out[0] = d0.val[0];
  out[1] = d1.val[0];
  out[2] = d2.val[0];
  out[3] = d3.val[0];
  out[4] = d0.val[1];
  out[5] = d1.val[1];
  out[6] = d2.val[1];
  out[7] = d3.val[1];
}

static INLINE void transpose_u16_4x4d(uint16x4_t *a0, uint16x4_t *a1,
                                      uint16x4_t *a2, uint16x4_t *a3) {
  // Swap 16 bit elements. Goes from:
  // a0: 00 01 02 03
  // a1: 10 11 12 13
  // a2: 20 21 22 23
  // a3: 30 31 32 33
  // to:
  // b0.val[0]: 00 10 02 12
  // b0.val[1]: 01 11 03 13
  // b1.val[0]: 20 30 22 32
  // b1.val[1]: 21 31 23 33

  const uint16x4x2_t b0 = vtrn_u16(*a0, *a1);
  const uint16x4x2_t b1 = vtrn_u16(*a2, *a3);

  // Swap 32 bit elements resulting in:
  // c0.val[0]: 00 10 20 30
  // c0.val[1]: 02 12 22 32
  // c1.val[0]: 01 11 21 31
  // c1.val[1]: 03 13 23 33

  const uint32x2x2_t c0 = vtrn_u32(vreinterpret_u32_u16(b0.val[0]),
                                   vreinterpret_u32_u16(b1.val[0]));
  const uint32x2x2_t c1 = vtrn_u32(vreinterpret_u32_u16(b0.val[1]),
                                   vreinterpret_u32_u16(b1.val[1]));

  *a0 = vreinterpret_u16_u32(c0.val[0]);
  *a1 = vreinterpret_u16_u32(c1.val[0]);
  *a2 = vreinterpret_u16_u32(c0.val[1]);
  *a3 = vreinterpret_u16_u32(c1.val[1]);
}

static INLINE void transpose_s16_4x4d(int16x4_t *a0, int16x4_t *a1,
                                      int16x4_t *a2, int16x4_t *a3) {
  // Swap 16 bit elements. Goes from:
  // a0: 00 01 02 03
  // a1: 10 11 12 13
  // a2: 20 21 22 23
  // a3: 30 31 32 33
  // to:
  // b0.val[0]: 00 10 02 12
  // b0.val[1]: 01 11 03 13
  // b1.val[0]: 20 30 22 32
  // b1.val[1]: 21 31 23 33

  const int16x4x2_t b0 = vtrn_s16(*a0, *a1);
  const int16x4x2_t b1 = vtrn_s16(*a2, *a3);

  // Swap 32 bit elements resulting in:
  // c0.val[0]: 00 10 20 30
  // c0.val[1]: 02 12 22 32
  // c1.val[0]: 01 11 21 31
  // c1.val[1]: 03 13 23 33

  const int32x2x2_t c0 = vtrn_s32(vreinterpret_s32_s16(b0.val[0]),
                                  vreinterpret_s32_s16(b1.val[0]));
  const int32x2x2_t c1 = vtrn_s32(vreinterpret_s32_s16(b0.val[1]),
                                  vreinterpret_s32_s16(b1.val[1]));

  *a0 = vreinterpret_s16_s32(c0.val[0]);
  *a1 = vreinterpret_s16_s32(c1.val[0]);
  *a2 = vreinterpret_s16_s32(c0.val[1]);
  *a3 = vreinterpret_s16_s32(c1.val[1]);
}

static INLINE int32x4x2_t aom_vtrnq_s64_to_s32(int32x4_t a0, int32x4_t a1) {
  int32x4x2_t b0;
#if defined(__aarch64__)
  b0.val[0] = vreinterpretq_s32_s64(
      vtrn1q_s64(vreinterpretq_s64_s32(a0), vreinterpretq_s64_s32(a1)));
  b0.val[1] = vreinterpretq_s32_s64(
      vtrn2q_s64(vreinterpretq_s64_s32(a0), vreinterpretq_s64_s32(a1)));
#else
  b0.val[0] = vcombine_s32(vget_low_s32(a0), vget_low_s32(a1));
  b0.val[1] = vcombine_s32(vget_high_s32(a0), vget_high_s32(a1));
#endif
  return b0;
}

static INLINE void transpose_s32_4x4(int32x4_t *a0, int32x4_t *a1,
                                     int32x4_t *a2, int32x4_t *a3) {
  // Swap 32 bit elements. Goes from:
  // a0: 00 01 02 03
  // a1: 10 11 12 13
  // a2: 20 21 22 23
  // a3: 30 31 32 33
  // to:
  // b0.val[0]: 00 10 02 12
  // b0.val[1]: 01 11 03 13
  // b1.val[0]: 20 30 22 32
  // b1.val[1]: 21 31 23 33

  const int32x4x2_t b0 = vtrnq_s32(*a0, *a1);
  const int32x4x2_t b1 = vtrnq_s32(*a2, *a3);

  // Swap 64 bit elements resulting in:
  // c0.val[0]: 00 10 20 30
  // c0.val[1]: 02 12 22 32
  // c1.val[0]: 01 11 21 31
  // c1.val[1]: 03 13 23 33

  const int32x4x2_t c0 = aom_vtrnq_s64_to_s32(b0.val[0], b1.val[0]);
  const int32x4x2_t c1 = aom_vtrnq_s64_to_s32(b0.val[1], b1.val[1]);

  *a0 = c0.val[0];
  *a1 = c1.val[0];
  *a2 = c0.val[1];
  *a3 = c1.val[1];
}

#endif  // AOM_AOM_DSP_ARM_TRANSPOSE_NEON_H_
