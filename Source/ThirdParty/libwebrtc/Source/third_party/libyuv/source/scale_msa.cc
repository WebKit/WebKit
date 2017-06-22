/*
 *  Copyright 2016 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>

#include "libyuv/scale_row.h"

// This module is for GCC MSA
#if !defined(LIBYUV_DISABLE_MSA) && defined(__mips_msa)
#include "libyuv/macros_msa.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

void ScaleARGBRowDown2_MSA(const uint8_t* src_argb,
                           ptrdiff_t src_stride,
                           uint8_t* dst_argb,
                           int dst_width) {
  int x;
  v16u8 src0, src1, dst0;
  (void)src_stride;

  for (x = 0; x < dst_width; x += 4) {
    src0 = (v16u8)__msa_ld_b((v16i8*)src_argb, 0);
    src1 = (v16u8)__msa_ld_b((v16i8*)src_argb, 16);
    dst0 = (v16u8)__msa_pckod_w((v4i32)src1, (v4i32)src0);
    ST_UB(dst0, dst_argb);
    src_argb += 32;
    dst_argb += 16;
  }
}

void ScaleARGBRowDown2Linear_MSA(const uint8_t* src_argb,
                                 ptrdiff_t src_stride,
                                 uint8_t* dst_argb,
                                 int dst_width) {
  int x;
  v16u8 src0, src1, vec0, vec1, dst0;
  (void)src_stride;

  for (x = 0; x < dst_width; x += 4) {
    src0 = (v16u8)__msa_ld_b((v16i8*)src_argb, 0);
    src1 = (v16u8)__msa_ld_b((v16i8*)src_argb, 16);
    vec0 = (v16u8)__msa_pckev_w((v4i32)src1, (v4i32)src0);
    vec1 = (v16u8)__msa_pckod_w((v4i32)src1, (v4i32)src0);
    dst0 = (v16u8)__msa_aver_u_b((v16u8)vec0, (v16u8)vec1);
    ST_UB(dst0, dst_argb);
    src_argb += 32;
    dst_argb += 16;
  }
}

void ScaleARGBRowDown2Box_MSA(const uint8_t* src_argb,
                              ptrdiff_t src_stride,
                              uint8_t* dst_argb,
                              int dst_width) {
  int x;
  const uint8_t* s = src_argb;
  const uint8_t* t = src_argb + src_stride;
  v16u8 src0, src1, src2, src3, vec0, vec1, vec2, vec3, dst0;
  v8u16 reg0, reg1, reg2, reg3;
  v16i8 shuffler = {0, 4, 1, 5, 2, 6, 3, 7, 8, 12, 9, 13, 10, 14, 11, 15};

  for (x = 0; x < dst_width; x += 4) {
    src0 = (v16u8)__msa_ld_b((v16i8*)s, 0);
    src1 = (v16u8)__msa_ld_b((v16i8*)s, 16);
    src2 = (v16u8)__msa_ld_b((v16i8*)t, 0);
    src3 = (v16u8)__msa_ld_b((v16i8*)t, 16);
    vec0 = (v16u8)__msa_vshf_b(shuffler, (v16i8)src0, (v16i8)src0);
    vec1 = (v16u8)__msa_vshf_b(shuffler, (v16i8)src1, (v16i8)src1);
    vec2 = (v16u8)__msa_vshf_b(shuffler, (v16i8)src2, (v16i8)src2);
    vec3 = (v16u8)__msa_vshf_b(shuffler, (v16i8)src3, (v16i8)src3);
    reg0 = __msa_hadd_u_h(vec0, vec0);
    reg1 = __msa_hadd_u_h(vec1, vec1);
    reg2 = __msa_hadd_u_h(vec2, vec2);
    reg3 = __msa_hadd_u_h(vec3, vec3);
    reg0 += reg2;
    reg1 += reg3;
    reg0 = (v8u16)__msa_srari_h((v8i16)reg0, 2);
    reg1 = (v8u16)__msa_srari_h((v8i16)reg1, 2);
    dst0 = (v16u8)__msa_pckev_b((v16i8)reg1, (v16i8)reg0);
    ST_UB(dst0, dst_argb);
    s += 32;
    t += 32;
    dst_argb += 16;
  }
}

void ScaleARGBRowDownEven_MSA(const uint8_t* src_argb,
                              ptrdiff_t src_stride,
                              int32_t src_stepx,
                              uint8_t* dst_argb,
                              int dst_width) {
  int x;
  int32_t stepx = src_stepx * 4;
  int32_t data0, data1, data2, data3;
  (void)src_stride;

  for (x = 0; x < dst_width; x += 4) {
    data0 = LW(src_argb);
    data1 = LW(src_argb + stepx);
    data2 = LW(src_argb + stepx * 2);
    data3 = LW(src_argb + stepx * 3);
    SW(data0, dst_argb);
    SW(data1, dst_argb + 4);
    SW(data2, dst_argb + 8);
    SW(data3, dst_argb + 12);
    src_argb += stepx * 4;
    dst_argb += 16;
  }
}

void ScaleARGBRowDownEvenBox_MSA(const uint8* src_argb,
                                 ptrdiff_t src_stride,
                                 int src_stepx,
                                 uint8* dst_argb,
                                 int dst_width) {
  int x;
  const uint8* nxt_argb = src_argb + src_stride;
  int32_t stepx = src_stepx * 4;
  int64_t data0, data1, data2, data3;
  v16u8 src0 = {0}, src1 = {0}, src2 = {0}, src3 = {0};
  v16u8 vec0, vec1, vec2, vec3;
  v8u16 reg0, reg1, reg2, reg3, reg4, reg5, reg6, reg7;
  v16u8 dst0;

  for (x = 0; x < dst_width; x += 4) {
    data0 = LD(src_argb);
    data1 = LD(src_argb + stepx);
    data2 = LD(src_argb + stepx * 2);
    data3 = LD(src_argb + stepx * 3);
    src0 = (v16u8)__msa_insert_d((v2i64)src0, 0, data0);
    src0 = (v16u8)__msa_insert_d((v2i64)src0, 1, data1);
    src1 = (v16u8)__msa_insert_d((v2i64)src1, 0, data2);
    src1 = (v16u8)__msa_insert_d((v2i64)src1, 1, data3);
    data0 = LD(nxt_argb);
    data1 = LD(nxt_argb + stepx);
    data2 = LD(nxt_argb + stepx * 2);
    data3 = LD(nxt_argb + stepx * 3);
    src2 = (v16u8)__msa_insert_d((v2i64)src2, 0, data0);
    src2 = (v16u8)__msa_insert_d((v2i64)src2, 1, data1);
    src3 = (v16u8)__msa_insert_d((v2i64)src3, 0, data2);
    src3 = (v16u8)__msa_insert_d((v2i64)src3, 1, data3);
    vec0 = (v16u8)__msa_ilvr_b((v16i8)src2, (v16i8)src0);
    vec1 = (v16u8)__msa_ilvr_b((v16i8)src3, (v16i8)src1);
    vec2 = (v16u8)__msa_ilvl_b((v16i8)src2, (v16i8)src0);
    vec3 = (v16u8)__msa_ilvl_b((v16i8)src3, (v16i8)src1);
    reg0 = __msa_hadd_u_h(vec0, vec0);
    reg1 = __msa_hadd_u_h(vec1, vec1);
    reg2 = __msa_hadd_u_h(vec2, vec2);
    reg3 = __msa_hadd_u_h(vec3, vec3);
    reg4 = (v8u16)__msa_pckev_d((v2i64)reg2, (v2i64)reg0);
    reg5 = (v8u16)__msa_pckev_d((v2i64)reg3, (v2i64)reg1);
    reg6 = (v8u16)__msa_pckod_d((v2i64)reg2, (v2i64)reg0);
    reg7 = (v8u16)__msa_pckod_d((v2i64)reg3, (v2i64)reg1);
    reg4 += reg6;
    reg5 += reg7;
    reg4 = (v8u16)__msa_srari_h((v8i16)reg4, 2);
    reg5 = (v8u16)__msa_srari_h((v8i16)reg5, 2);
    dst0 = (v16u8)__msa_pckev_b((v16i8)reg5, (v16i8)reg4);
    ST_UB(dst0, dst_argb);
    src_argb += stepx * 4;
    nxt_argb += stepx * 4;
    dst_argb += 16;
  }
}

void ScaleRowDown2_MSA(const uint8_t* src_ptr,
                       ptrdiff_t src_stride,
                       uint8_t* dst,
                       int dst_width) {
  int x;
  v16u8 src0, src1, src2, src3, dst0, dst1;
  (void)src_stride;

  for (x = 0; x < dst_width; x += 32) {
    src0 = (v16u8)__msa_ld_b((v16i8*)src_ptr, 0);
    src1 = (v16u8)__msa_ld_b((v16i8*)src_ptr, 16);
    src2 = (v16u8)__msa_ld_b((v16i8*)src_ptr, 32);
    src3 = (v16u8)__msa_ld_b((v16i8*)src_ptr, 48);
    dst0 = (v16u8)__msa_pckod_b((v16i8)src1, (v16i8)src0);
    dst1 = (v16u8)__msa_pckod_b((v16i8)src3, (v16i8)src2);
    ST_UB2(dst0, dst1, dst, 16);
    src_ptr += 64;
    dst += 32;
  }
}

void ScaleRowDown2Linear_MSA(const uint8_t* src_ptr,
                             ptrdiff_t src_stride,
                             uint8_t* dst,
                             int dst_width) {
  int x;
  v16u8 src0, src1, src2, src3, vec0, vec1, vec2, vec3, dst0, dst1;
  (void)src_stride;

  for (x = 0; x < dst_width; x += 32) {
    src0 = (v16u8)__msa_ld_b((v16i8*)src_ptr, 0);
    src1 = (v16u8)__msa_ld_b((v16i8*)src_ptr, 16);
    src2 = (v16u8)__msa_ld_b((v16i8*)src_ptr, 32);
    src3 = (v16u8)__msa_ld_b((v16i8*)src_ptr, 48);
    vec0 = (v16u8)__msa_pckev_b((v16i8)src1, (v16i8)src0);
    vec2 = (v16u8)__msa_pckev_b((v16i8)src3, (v16i8)src2);
    vec1 = (v16u8)__msa_pckod_b((v16i8)src1, (v16i8)src0);
    vec3 = (v16u8)__msa_pckod_b((v16i8)src3, (v16i8)src2);
    dst0 = __msa_aver_u_b(vec1, vec0);
    dst1 = __msa_aver_u_b(vec3, vec2);
    ST_UB2(dst0, dst1, dst, 16);
    src_ptr += 64;
    dst += 32;
  }
}

void ScaleRowDown2Box_MSA(const uint8_t* src_ptr,
                          ptrdiff_t src_stride,
                          uint8_t* dst,
                          int dst_width) {
  int x;
  const uint8_t* s = src_ptr;
  const uint8_t* t = src_ptr + src_stride;
  v16u8 src0, src1, src2, src3, src4, src5, src6, src7, dst0, dst1;
  v8u16 vec0, vec1, vec2, vec3;

  for (x = 0; x < dst_width; x += 32) {
    src0 = (v16u8)__msa_ld_b((v16i8*)s, 0);
    src1 = (v16u8)__msa_ld_b((v16i8*)s, 16);
    src2 = (v16u8)__msa_ld_b((v16i8*)s, 32);
    src3 = (v16u8)__msa_ld_b((v16i8*)s, 48);
    src4 = (v16u8)__msa_ld_b((v16i8*)t, 0);
    src5 = (v16u8)__msa_ld_b((v16i8*)t, 16);
    src6 = (v16u8)__msa_ld_b((v16i8*)t, 32);
    src7 = (v16u8)__msa_ld_b((v16i8*)t, 48);
    vec0 = __msa_hadd_u_h(src0, src0);
    vec1 = __msa_hadd_u_h(src1, src1);
    vec2 = __msa_hadd_u_h(src2, src2);
    vec3 = __msa_hadd_u_h(src3, src3);
    vec0 += __msa_hadd_u_h(src4, src4);
    vec1 += __msa_hadd_u_h(src5, src5);
    vec2 += __msa_hadd_u_h(src6, src6);
    vec3 += __msa_hadd_u_h(src7, src7);
    vec0 = (v8u16)__msa_srari_h((v8i16)vec0, 2);
    vec1 = (v8u16)__msa_srari_h((v8i16)vec1, 2);
    vec2 = (v8u16)__msa_srari_h((v8i16)vec2, 2);
    vec3 = (v8u16)__msa_srari_h((v8i16)vec3, 2);
    dst0 = (v16u8)__msa_pckev_b((v16i8)vec1, (v16i8)vec0);
    dst1 = (v16u8)__msa_pckev_b((v16i8)vec3, (v16i8)vec2);
    ST_UB2(dst0, dst1, dst, 16);
    s += 64;
    t += 64;
    dst += 32;
  }
}

void ScaleRowDown4_MSA(const uint8_t* src_ptr,
                       ptrdiff_t src_stride,
                       uint8_t* dst,
                       int dst_width) {
  int x;
  v16u8 src0, src1, src2, src3, vec0, vec1, dst0;
  (void)src_stride;

  for (x = 0; x < dst_width; x += 16) {
    src0 = (v16u8)__msa_ld_b((v16i8*)src_ptr, 0);
    src1 = (v16u8)__msa_ld_b((v16i8*)src_ptr, 16);
    src2 = (v16u8)__msa_ld_b((v16i8*)src_ptr, 32);
    src3 = (v16u8)__msa_ld_b((v16i8*)src_ptr, 48);
    vec0 = (v16u8)__msa_pckev_b((v16i8)src1, (v16i8)src0);
    vec1 = (v16u8)__msa_pckev_b((v16i8)src3, (v16i8)src2);
    dst0 = (v16u8)__msa_pckod_b((v16i8)vec1, (v16i8)vec0);
    ST_UB(dst0, dst);
    src_ptr += 64;
    dst += 16;
  }
}

void ScaleRowDown4Box_MSA(const uint8_t* src_ptr,
                          ptrdiff_t src_stride,
                          uint8_t* dst,
                          int dst_width) {
  int x;
  const uint8_t* s = src_ptr;
  const uint8_t* t0 = s + src_stride;
  const uint8_t* t1 = s + src_stride * 2;
  const uint8_t* t2 = s + src_stride * 3;
  v16u8 src0, src1, src2, src3, src4, src5, src6, src7, dst0;
  v8u16 vec0, vec1, vec2, vec3;
  v4u32 reg0, reg1, reg2, reg3;

  for (x = 0; x < dst_width; x += 16) {
    src0 = (v16u8)__msa_ld_b((v16i8*)s, 0);
    src1 = (v16u8)__msa_ld_b((v16i8*)s, 16);
    src2 = (v16u8)__msa_ld_b((v16i8*)s, 32);
    src3 = (v16u8)__msa_ld_b((v16i8*)s, 48);
    src4 = (v16u8)__msa_ld_b((v16i8*)t0, 0);
    src5 = (v16u8)__msa_ld_b((v16i8*)t0, 16);
    src6 = (v16u8)__msa_ld_b((v16i8*)t0, 32);
    src7 = (v16u8)__msa_ld_b((v16i8*)t0, 48);
    vec0 = __msa_hadd_u_h(src0, src0);
    vec1 = __msa_hadd_u_h(src1, src1);
    vec2 = __msa_hadd_u_h(src2, src2);
    vec3 = __msa_hadd_u_h(src3, src3);
    vec0 += __msa_hadd_u_h(src4, src4);
    vec1 += __msa_hadd_u_h(src5, src5);
    vec2 += __msa_hadd_u_h(src6, src6);
    vec3 += __msa_hadd_u_h(src7, src7);
    src0 = (v16u8)__msa_ld_b((v16i8*)t1, 0);
    src1 = (v16u8)__msa_ld_b((v16i8*)t1, 16);
    src2 = (v16u8)__msa_ld_b((v16i8*)t1, 32);
    src3 = (v16u8)__msa_ld_b((v16i8*)t1, 48);
    src4 = (v16u8)__msa_ld_b((v16i8*)t2, 0);
    src5 = (v16u8)__msa_ld_b((v16i8*)t2, 16);
    src6 = (v16u8)__msa_ld_b((v16i8*)t2, 32);
    src7 = (v16u8)__msa_ld_b((v16i8*)t2, 48);
    vec0 += __msa_hadd_u_h(src0, src0);
    vec1 += __msa_hadd_u_h(src1, src1);
    vec2 += __msa_hadd_u_h(src2, src2);
    vec3 += __msa_hadd_u_h(src3, src3);
    vec0 += __msa_hadd_u_h(src4, src4);
    vec1 += __msa_hadd_u_h(src5, src5);
    vec2 += __msa_hadd_u_h(src6, src6);
    vec3 += __msa_hadd_u_h(src7, src7);
    reg0 = __msa_hadd_u_w(vec0, vec0);
    reg1 = __msa_hadd_u_w(vec1, vec1);
    reg2 = __msa_hadd_u_w(vec2, vec2);
    reg3 = __msa_hadd_u_w(vec3, vec3);
    reg0 = (v4u32)__msa_srari_w((v4i32)reg0, 4);
    reg1 = (v4u32)__msa_srari_w((v4i32)reg1, 4);
    reg2 = (v4u32)__msa_srari_w((v4i32)reg2, 4);
    reg3 = (v4u32)__msa_srari_w((v4i32)reg3, 4);
    vec0 = (v8u16)__msa_pckev_h((v8i16)reg1, (v8i16)reg0);
    vec1 = (v8u16)__msa_pckev_h((v8i16)reg3, (v8i16)reg2);
    dst0 = (v16u8)__msa_pckev_b((v16i8)vec1, (v16i8)vec0);
    ST_UB(dst0, dst);
    s += 64;
    t0 += 64;
    t1 += 64;
    t2 += 64;
    dst += 16;
  }
}

void ScaleRowDown38_MSA(const uint8_t* src_ptr,
                        ptrdiff_t src_stride,
                        uint8_t* dst,
                        int dst_width) {
  int x, width;
  uint64_t dst0;
  uint32_t dst1;
  v16u8 src0, src1, vec0;
  v16i8 mask = {0, 3, 6, 8, 11, 14, 16, 19, 22, 24, 27, 30, 0, 0, 0, 0};
  (void)src_stride;

  assert(dst_width % 3 == 0);
  width = dst_width / 3;

  for (x = 0; x < width; x += 4) {
    src0 = (v16u8)__msa_ld_b((v16i8*)src_ptr, 0);
    src1 = (v16u8)__msa_ld_b((v16i8*)src_ptr, 16);
    vec0 = (v16u8)__msa_vshf_b(mask, (v16i8)src1, (v16i8)src0);
    dst0 = __msa_copy_u_d((v2i64)vec0, 0);
    dst1 = __msa_copy_u_w((v4i32)vec0, 2);
    SD(dst0, dst);
    SW(dst1, dst + 8);
    src_ptr += 32;
    dst += 12;
  }
}

void ScaleRowDown38_2_Box_MSA(const uint8_t* src_ptr,
                              ptrdiff_t src_stride,
                              uint8_t* dst_ptr,
                              int dst_width) {
  int x, width;
  const uint8_t* s = src_ptr;
  const uint8_t* t = src_ptr + src_stride;
  uint64_t dst0;
  uint32_t dst1;
  v16u8 src0, src1, src2, src3, out;
  v8u16 vec0, vec1, vec2, vec3, vec4, vec5, vec6, vec7;
  v4u32 tmp0, tmp1, tmp2, tmp3, tmp4;
  v8i16 zero = {0};
  v8i16 mask = {0, 1, 2, 8, 3, 4, 5, 9};
  v16i8 dst_mask = {0, 2, 16, 4, 6, 18, 8, 10, 20, 12, 14, 22, 0, 0, 0, 0};
  v4u32 const_0x2AAA = (v4u32)__msa_fill_w(0x2AAA);
  v4u32 const_0x4000 = (v4u32)__msa_fill_w(0x4000);

  assert((dst_width % 3 == 0) && (dst_width > 0));
  width = dst_width / 3;

  for (x = 0; x < width; x += 4) {
    src0 = (v16u8)__msa_ld_b((v16i8*)s, 0);
    src1 = (v16u8)__msa_ld_b((v16i8*)s, 16);
    src2 = (v16u8)__msa_ld_b((v16i8*)t, 0);
    src3 = (v16u8)__msa_ld_b((v16i8*)t, 16);
    vec0 = (v8u16)__msa_ilvr_b((v16i8)src2, (v16i8)src0);
    vec1 = (v8u16)__msa_ilvl_b((v16i8)src2, (v16i8)src0);
    vec2 = (v8u16)__msa_ilvr_b((v16i8)src3, (v16i8)src1);
    vec3 = (v8u16)__msa_ilvl_b((v16i8)src3, (v16i8)src1);
    vec0 = __msa_hadd_u_h((v16u8)vec0, (v16u8)vec0);
    vec1 = __msa_hadd_u_h((v16u8)vec1, (v16u8)vec1);
    vec2 = __msa_hadd_u_h((v16u8)vec2, (v16u8)vec2);
    vec3 = __msa_hadd_u_h((v16u8)vec3, (v16u8)vec3);
    vec4 = (v8u16)__msa_vshf_h(mask, zero, (v8i16)vec0);
    vec5 = (v8u16)__msa_vshf_h(mask, zero, (v8i16)vec1);
    vec6 = (v8u16)__msa_vshf_h(mask, zero, (v8i16)vec2);
    vec7 = (v8u16)__msa_vshf_h(mask, zero, (v8i16)vec3);
    vec0 = (v8u16)__msa_pckod_w((v4i32)vec1, (v4i32)vec0);
    vec1 = (v8u16)__msa_pckod_w((v4i32)vec3, (v4i32)vec2);
    vec0 = (v8u16)__msa_pckod_w((v4i32)vec1, (v4i32)vec0);
    tmp0 = __msa_hadd_u_w(vec4, vec4);
    tmp1 = __msa_hadd_u_w(vec5, vec5);
    tmp2 = __msa_hadd_u_w(vec6, vec6);
    tmp3 = __msa_hadd_u_w(vec7, vec7);
    tmp4 = __msa_hadd_u_w(vec0, vec0);
    vec0 = (v8u16)__msa_pckev_h((v8i16)tmp1, (v8i16)tmp0);
    vec1 = (v8u16)__msa_pckev_h((v8i16)tmp3, (v8i16)tmp2);
    tmp0 = __msa_hadd_u_w(vec0, vec0);
    tmp1 = __msa_hadd_u_w(vec1, vec1);
    tmp0 *= const_0x2AAA;
    tmp1 *= const_0x2AAA;
    tmp4 *= const_0x4000;
    tmp0 = (v4u32)__msa_srai_w((v4i32)tmp0, 16);
    tmp1 = (v4u32)__msa_srai_w((v4i32)tmp1, 16);
    tmp4 = (v4u32)__msa_srai_w((v4i32)tmp4, 16);
    vec0 = (v8u16)__msa_pckev_h((v8i16)tmp1, (v8i16)tmp0);
    vec1 = (v8u16)__msa_pckev_h((v8i16)tmp4, (v8i16)tmp4);
    out = (v16u8)__msa_vshf_b(dst_mask, (v16i8)vec1, (v16i8)vec0);
    dst0 = __msa_copy_u_d((v2i64)out, 0);
    dst1 = __msa_copy_u_w((v4i32)out, 2);
    SD(dst0, dst_ptr);
    SW(dst1, dst_ptr + 8);
    s += 32;
    t += 32;
    dst_ptr += 12;
  }
}

void ScaleRowDown38_3_Box_MSA(const uint8_t* src_ptr,
                              ptrdiff_t src_stride,
                              uint8_t* dst_ptr,
                              int dst_width) {
  int x, width;
  const uint8_t* s = src_ptr;
  const uint8_t* t0 = s + src_stride;
  const uint8_t* t1 = s + src_stride * 2;
  uint64_t dst0;
  uint32_t dst1;
  v16u8 src0, src1, src2, src3, src4, src5, out;
  v8u16 vec0, vec1, vec2, vec3, vec4, vec5, vec6, vec7;
  v4u32 tmp0, tmp1, tmp2, tmp3, tmp4;
  v8u16 zero = {0};
  v8i16 mask = {0, 1, 2, 8, 3, 4, 5, 9};
  v16i8 dst_mask = {0, 2, 16, 4, 6, 18, 8, 10, 20, 12, 14, 22, 0, 0, 0, 0};
  v4u32 const_0x1C71 = (v4u32)__msa_fill_w(0x1C71);
  v4u32 const_0x2AAA = (v4u32)__msa_fill_w(0x2AAA);

  assert((dst_width % 3 == 0) && (dst_width > 0));
  width = dst_width / 3;

  for (x = 0; x < width; x += 4) {
    src0 = (v16u8)__msa_ld_b((v16i8*)s, 0);
    src1 = (v16u8)__msa_ld_b((v16i8*)s, 16);
    src2 = (v16u8)__msa_ld_b((v16i8*)t0, 0);
    src3 = (v16u8)__msa_ld_b((v16i8*)t0, 16);
    src4 = (v16u8)__msa_ld_b((v16i8*)t1, 0);
    src5 = (v16u8)__msa_ld_b((v16i8*)t1, 16);
    vec0 = (v8u16)__msa_ilvr_b((v16i8)src2, (v16i8)src0);
    vec1 = (v8u16)__msa_ilvl_b((v16i8)src2, (v16i8)src0);
    vec2 = (v8u16)__msa_ilvr_b((v16i8)src3, (v16i8)src1);
    vec3 = (v8u16)__msa_ilvl_b((v16i8)src3, (v16i8)src1);
    vec4 = (v8u16)__msa_ilvr_b((v16i8)zero, (v16i8)src4);
    vec5 = (v8u16)__msa_ilvl_b((v16i8)zero, (v16i8)src4);
    vec6 = (v8u16)__msa_ilvr_b((v16i8)zero, (v16i8)src5);
    vec7 = (v8u16)__msa_ilvl_b((v16i8)zero, (v16i8)src5);
    vec0 = __msa_hadd_u_h((v16u8)vec0, (v16u8)vec0);
    vec1 = __msa_hadd_u_h((v16u8)vec1, (v16u8)vec1);
    vec2 = __msa_hadd_u_h((v16u8)vec2, (v16u8)vec2);
    vec3 = __msa_hadd_u_h((v16u8)vec3, (v16u8)vec3);
    vec0 += __msa_hadd_u_h((v16u8)vec4, (v16u8)vec4);
    vec1 += __msa_hadd_u_h((v16u8)vec5, (v16u8)vec5);
    vec2 += __msa_hadd_u_h((v16u8)vec6, (v16u8)vec6);
    vec3 += __msa_hadd_u_h((v16u8)vec7, (v16u8)vec7);
    vec4 = (v8u16)__msa_vshf_h(mask, (v8i16)zero, (v8i16)vec0);
    vec5 = (v8u16)__msa_vshf_h(mask, (v8i16)zero, (v8i16)vec1);
    vec6 = (v8u16)__msa_vshf_h(mask, (v8i16)zero, (v8i16)vec2);
    vec7 = (v8u16)__msa_vshf_h(mask, (v8i16)zero, (v8i16)vec3);
    vec0 = (v8u16)__msa_pckod_w((v4i32)vec1, (v4i32)vec0);
    vec1 = (v8u16)__msa_pckod_w((v4i32)vec3, (v4i32)vec2);
    vec0 = (v8u16)__msa_pckod_w((v4i32)vec1, (v4i32)vec0);
    tmp0 = __msa_hadd_u_w(vec4, vec4);
    tmp1 = __msa_hadd_u_w(vec5, vec5);
    tmp2 = __msa_hadd_u_w(vec6, vec6);
    tmp3 = __msa_hadd_u_w(vec7, vec7);
    tmp4 = __msa_hadd_u_w(vec0, vec0);
    vec0 = (v8u16)__msa_pckev_h((v8i16)tmp1, (v8i16)tmp0);
    vec1 = (v8u16)__msa_pckev_h((v8i16)tmp3, (v8i16)tmp2);
    tmp0 = __msa_hadd_u_w(vec0, vec0);
    tmp1 = __msa_hadd_u_w(vec1, vec1);
    tmp0 *= const_0x1C71;
    tmp1 *= const_0x1C71;
    tmp4 *= const_0x2AAA;
    tmp0 = (v4u32)__msa_srai_w((v4i32)tmp0, 16);
    tmp1 = (v4u32)__msa_srai_w((v4i32)tmp1, 16);
    tmp4 = (v4u32)__msa_srai_w((v4i32)tmp4, 16);
    vec0 = (v8u16)__msa_pckev_h((v8i16)tmp1, (v8i16)tmp0);
    vec1 = (v8u16)__msa_pckev_h((v8i16)tmp4, (v8i16)tmp4);
    out = (v16u8)__msa_vshf_b(dst_mask, (v16i8)vec1, (v16i8)vec0);
    dst0 = __msa_copy_u_d((v2i64)out, 0);
    dst1 = __msa_copy_u_w((v4i32)out, 2);
    SD(dst0, dst_ptr);
    SW(dst1, dst_ptr + 8);
    s += 32;
    t0 += 32;
    t1 += 32;
    dst_ptr += 12;
  }
}

void ScaleAddRow_MSA(const uint8_t* src_ptr, uint16_t* dst_ptr, int src_width) {
  int x;
  v16u8 src0;
  v8u16 dst0, dst1;
  v16i8 zero = {0};

  assert(src_width > 0);

  for (x = 0; x < src_width; x += 16) {
    src0 = LD_UB(src_ptr);
    dst0 = (v8u16)__msa_ld_h((v8i16*)dst_ptr, 0);
    dst1 = (v8u16)__msa_ld_h((v8i16*)dst_ptr, 16);
    dst0 += (v8u16)__msa_ilvr_b(zero, (v16i8)src0);
    dst1 += (v8u16)__msa_ilvl_b(zero, (v16i8)src0);
    ST_UH2(dst0, dst1, dst_ptr, 8);
    src_ptr += 16;
    dst_ptr += 16;
  }
}

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif

#endif  // !defined(LIBYUV_DISABLE_MSA) && defined(__mips_msa)
