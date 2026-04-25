/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_AOM_DSP_RISCV_MEM_RVV_H_
#define AOM_AOM_DSP_RISCV_MEM_RVV_H_

#include <riscv_vector.h>

static inline void load_s16_4x5(const int16_t *s, int p, vint16mf2_t *const s0,
                                vint16mf2_t *const s1, vint16mf2_t *const s2,
                                vint16mf2_t *const s3, vint16mf2_t *const s4,
                                size_t vl) {
  *s0 = __riscv_vle16_v_i16mf2(s, vl);
  s += p;
  *s1 = __riscv_vle16_v_i16mf2(s, vl);
  s += p;
  *s2 = __riscv_vle16_v_i16mf2(s, vl);
  s += p;
  *s3 = __riscv_vle16_v_i16mf2(s, vl);
  s += p;
  *s4 = __riscv_vle16_v_i16mf2(s, vl);
}

static inline void load_s16_4x4(const int16_t *s, int p, vint16mf2_t *const s0,
                                vint16mf2_t *const s1, vint16mf2_t *const s2,
                                vint16mf2_t *const s3, size_t vl) {
  *s0 = __riscv_vle16_v_i16mf2(s, vl);
  s += p;
  *s1 = __riscv_vle16_v_i16mf2(s, vl);
  s += p;
  *s2 = __riscv_vle16_v_i16mf2(s, vl);
  s += p;
  *s3 = __riscv_vle16_v_i16mf2(s, vl);
}

static inline void load_s16_8x5(const int16_t *s, int p, vint16m1_t *const s0,
                                vint16m1_t *const s1, vint16m1_t *const s2,
                                vint16m1_t *const s3, vint16m1_t *const s4,
                                size_t vl) {
  *s0 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s1 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s2 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s3 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s4 = __riscv_vle16_v_i16m1(s, vl);
}

static inline void load_s16_8x4(const int16_t *s, int p, vint16m1_t *const s0,
                                vint16m1_t *const s1, vint16m1_t *const s2,
                                vint16m1_t *const s3, size_t vl) {
  *s0 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s1 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s2 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s3 = __riscv_vle16_v_i16m1(s, vl);
}

static inline void store_u16_8x4(uint16_t *s, int p, const vuint16m1_t s0,
                                 const vuint16m1_t s1, const vuint16m1_t s2,
                                 const vuint16m1_t s3, size_t vl) {
  __riscv_vse16_v_u16m1(s, s0, vl);
  s += p;
  __riscv_vse16_v_u16m1(s, s1, vl);
  s += p;
  __riscv_vse16_v_u16m1(s, s2, vl);
  s += p;
  __riscv_vse16_v_u16m1(s, s3, vl);
}

static inline void load_s16_4x7(const int16_t *s, int p, vint16mf2_t *const s0,
                                vint16mf2_t *const s1, vint16mf2_t *const s2,
                                vint16mf2_t *const s3, vint16mf2_t *const s4,
                                vint16mf2_t *const s5, vint16mf2_t *const s6,
                                size_t vl) {
  *s0 = __riscv_vle16_v_i16mf2(s, vl);
  s += p;
  *s1 = __riscv_vle16_v_i16mf2(s, vl);
  s += p;
  *s2 = __riscv_vle16_v_i16mf2(s, vl);
  s += p;
  *s3 = __riscv_vle16_v_i16mf2(s, vl);
  s += p;
  *s4 = __riscv_vle16_v_i16mf2(s, vl);
  s += p;
  *s5 = __riscv_vle16_v_i16mf2(s, vl);
  s += p;
  *s6 = __riscv_vle16_v_i16mf2(s, vl);
}

static inline void load_s16_8x7(const int16_t *s, int p, vint16m1_t *const s0,
                                vint16m1_t *const s1, vint16m1_t *const s2,
                                vint16m1_t *const s3, vint16m1_t *const s4,
                                vint16m1_t *const s5, vint16m1_t *const s6,
                                size_t vl) {
  *s0 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s1 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s2 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s3 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s4 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s5 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s6 = __riscv_vle16_v_i16m1(s, vl);
}

static inline void load_s16_4x11(const int16_t *s, int p, vint16mf2_t *const s0,
                                 vint16mf2_t *const s1, vint16mf2_t *const s2,
                                 vint16mf2_t *const s3, vint16mf2_t *const s4,
                                 vint16mf2_t *const s5, vint16mf2_t *const s6,
                                 vint16mf2_t *const s7, vint16mf2_t *const s8,
                                 vint16mf2_t *const s9, vint16mf2_t *const s10,
                                 size_t vl) {
  *s0 = __riscv_vle16_v_i16mf2(s, vl);
  s += p;
  *s1 = __riscv_vle16_v_i16mf2(s, vl);
  s += p;
  *s2 = __riscv_vle16_v_i16mf2(s, vl);
  s += p;
  *s3 = __riscv_vle16_v_i16mf2(s, vl);
  s += p;
  *s4 = __riscv_vle16_v_i16mf2(s, vl);
  s += p;
  *s5 = __riscv_vle16_v_i16mf2(s, vl);
  s += p;
  *s6 = __riscv_vle16_v_i16mf2(s, vl);
  s += p;
  *s7 = __riscv_vle16_v_i16mf2(s, vl);
  s += p;
  *s8 = __riscv_vle16_v_i16mf2(s, vl);
  s += p;
  *s9 = __riscv_vle16_v_i16mf2(s, vl);
  s += p;
  *s10 = __riscv_vle16_v_i16mf2(s, vl);
}

static inline void load_s16_8x11(const int16_t *s, int p, vint16m1_t *const s0,
                                 vint16m1_t *const s1, vint16m1_t *const s2,
                                 vint16m1_t *const s3, vint16m1_t *const s4,
                                 vint16m1_t *const s5, vint16m1_t *const s6,
                                 vint16m1_t *const s7, vint16m1_t *const s8,
                                 vint16m1_t *const s9, vint16m1_t *const s10,
                                 size_t vl) {
  *s0 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s1 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s2 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s3 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s4 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s5 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s6 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s7 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s8 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s9 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s10 = __riscv_vle16_v_i16m1(s, vl);
}

static inline void load_s16_8x6(const int16_t *s, int p, vint16m1_t *const s0,
                                vint16m1_t *const s1, vint16m1_t *const s2,
                                vint16m1_t *const s3, vint16m1_t *const s4,
                                vint16m1_t *const s5, size_t vl) {
  *s0 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s1 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s2 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s3 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s4 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s5 = __riscv_vle16_v_i16m1(s, vl);
}

static inline void load_s16_8x8(const int16_t *s, int p, vint16m1_t *const s0,
                                vint16m1_t *const s1, vint16m1_t *const s2,
                                vint16m1_t *const s3, vint16m1_t *const s4,
                                vint16m1_t *const s5, vint16m1_t *const s6,
                                vint16m1_t *const s7, size_t vl) {
  *s0 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s1 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s2 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s3 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s4 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s5 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s6 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s7 = __riscv_vle16_v_i16m1(s, vl);
}

static inline void load_s16_8x12(const int16_t *s, int p, vint16m1_t *const s0,
                                 vint16m1_t *const s1, vint16m1_t *const s2,
                                 vint16m1_t *const s3, vint16m1_t *const s4,
                                 vint16m1_t *const s5, vint16m1_t *const s6,
                                 vint16m1_t *const s7, vint16m1_t *const s8,
                                 vint16m1_t *const s9, vint16m1_t *const s10,
                                 vint16m1_t *const s11, size_t vl) {
  *s0 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s1 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s2 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s3 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s4 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s5 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s6 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s7 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s8 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s9 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s10 = __riscv_vle16_v_i16m1(s, vl);
  s += p;
  *s11 = __riscv_vle16_v_i16m1(s, vl);
}

static inline void load_s16_4x12(const int16_t *s, int p, vint16mf2_t *const s0,
                                 vint16mf2_t *const s1, vint16mf2_t *const s2,
                                 vint16mf2_t *const s3, vint16mf2_t *const s4,
                                 vint16mf2_t *const s5, vint16mf2_t *const s6,
                                 vint16mf2_t *const s7, vint16mf2_t *const s8,
                                 vint16mf2_t *const s9, vint16mf2_t *const s10,
                                 vint16mf2_t *const s11, size_t vl) {
  *s0 = __riscv_vle16_v_i16mf2(s, vl);
  s += p;
  *s1 = __riscv_vle16_v_i16mf2(s, vl);
  s += p;
  *s2 = __riscv_vle16_v_i16mf2(s, vl);
  s += p;
  *s3 = __riscv_vle16_v_i16mf2(s, vl);
  s += p;
  *s4 = __riscv_vle16_v_i16mf2(s, vl);
  s += p;
  *s5 = __riscv_vle16_v_i16mf2(s, vl);
  s += p;
  *s6 = __riscv_vle16_v_i16mf2(s, vl);
  s += p;
  *s7 = __riscv_vle16_v_i16mf2(s, vl);
  s += p;
  *s8 = __riscv_vle16_v_i16mf2(s, vl);
  s += p;
  *s9 = __riscv_vle16_v_i16mf2(s, vl);
  s += p;
  *s10 = __riscv_vle16_v_i16mf2(s, vl);
  s += p;
  *s11 = __riscv_vle16_v_i16mf2(s, vl);
}

static inline void store_u16_4x4(uint16_t *s, int p, const vuint16mf2_t s0,
                                 const vuint16mf2_t s1, const vuint16mf2_t s2,
                                 const vuint16mf2_t s3, size_t vl) {
  __riscv_vse16_v_u16mf2(s, s0, vl);
  s += p;
  __riscv_vse16_v_u16mf2(s, s1, vl);
  s += p;
  __riscv_vse16_v_u16mf2(s, s2, vl);
  s += p;
  __riscv_vse16_v_u16mf2(s, s3, vl);
}

#endif  // AOM_AOM_DSP_RISCV_MEM_RVV_H_
