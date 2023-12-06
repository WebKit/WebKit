/*
 *  Copyright 2023 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * Copyright (c) 2023 SiFive, Inc. All rights reserved.
 *
 * Contributed by Darren Hsieh <darren.hsieh@sifive.com>
 * Contributed by Bruce Lai <bruce.lai@sifive.com>
 */

#include "libyuv/row.h"
#include "libyuv/scale_row.h"

// This module is for gcc/clang rvv.
#if !defined(LIBYUV_DISABLE_RVV) && defined(__riscv_vector)
#include <riscv_vector.h>

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

void ScaleAddRow_RVV(const uint8_t* src_ptr, uint16_t* dst_ptr, int src_width) {
  size_t w = (size_t)src_width;
  do {
    size_t vl = __riscv_vsetvl_e8m4(w);
    vuint8m4_t v_src = __riscv_vle8_v_u8m4(src_ptr, vl);
    vuint16m8_t v_dst = __riscv_vle16_v_u16m8(dst_ptr, vl);
    // Use widening multiply-add instead of widening + add
    v_dst = __riscv_vwmaccu_vx_u16m8(v_dst, 1, v_src, vl);
    __riscv_vse16_v_u16m8(dst_ptr, v_dst, vl);
    w -= vl;
    src_ptr += vl;
    dst_ptr += vl;
  } while (w > 0);
}

void ScaleARGBRowDown2_RVV(const uint8_t* src_argb,
                           ptrdiff_t src_stride,
                           uint8_t* dst_argb,
                           int dst_width) {
  (void)src_stride;
  size_t w = (size_t)dst_width;
  const uint64_t* src = (const uint64_t*)(src_argb);
  uint32_t* dst = (uint32_t*)(dst_argb);
  do {
    vuint64m8_t v_data;
    vuint32m4_t v_dst;
    size_t vl = __riscv_vsetvl_e64m8(w);
    v_data = __riscv_vle64_v_u64m8(src, vl);
    v_dst = __riscv_vnsrl_wx_u32m4(v_data, 32, vl);
    __riscv_vse32_v_u32m4(dst, v_dst, vl);
    w -= vl;
    src += vl;
    dst += vl;
  } while (w > 0);
}

void ScaleARGBRowDown2Linear_RVV(const uint8_t* src_argb,
                                 ptrdiff_t src_stride,
                                 uint8_t* dst_argb,
                                 int dst_width) {
  (void)src_stride;
  size_t w = (size_t)dst_width;
  const uint32_t* src = (const uint32_t*)(src_argb);
  // NOTE: To match behavior on other platforms, vxrm (fixed-point rounding mode
  // register) is set to round-to-nearest-up mode(0).
  asm volatile("csrwi vxrm, 0");
  do {
    vuint8m4_t v_odd, v_even, v_dst;
    vuint16m8_t v_sum;
    vuint32m4_t v_odd_32, v_even_32;
    size_t vl = __riscv_vsetvl_e32m4(w);
    __riscv_vlseg2e32_v_u32m4(&v_even_32, &v_odd_32, src, vl);
    v_even = __riscv_vreinterpret_v_u32m4_u8m4(v_even_32);
    v_odd = __riscv_vreinterpret_v_u32m4_u8m4(v_odd_32);
    // Use round-to-nearest-up mode for averaging add
    v_dst = __riscv_vaaddu_vv_u8m4(v_even, v_odd, vl * 4);
    __riscv_vse8_v_u8m4(dst_argb, v_dst, vl * 4);
    w -= vl;
    src += vl * 2;
    dst_argb += vl * 4;
  } while (w > 0);
}

void ScaleARGBRowDown2Box_RVV(const uint8_t* src_argb,
                              ptrdiff_t src_stride,
                              uint8_t* dst_argb,
                              int dst_width) {
  size_t w = (size_t)dst_width;
  const uint32_t* src0 = (const uint32_t*)(src_argb);
  const uint32_t* src1 = (const uint32_t*)(src_argb + src_stride);
  // NOTE: To match behavior on other platforms, vxrm (fixed-point rounding mode
  // register) is set to round-to-nearest-up mode(0).
  asm volatile("csrwi vxrm, 0");
  do {
    vuint8m4_t v_row0_odd, v_row0_even, v_row1_odd, v_row1_even, v_dst;
    vuint16m8_t v_row0_sum, v_row1_sum, v_dst_16;
    vuint32m4_t v_row0_odd_32, v_row0_even_32, v_row1_odd_32, v_row1_even_32;
    size_t vl = __riscv_vsetvl_e32m4(w);
    __riscv_vlseg2e32_v_u32m4(&v_row0_even_32, &v_row0_odd_32, src0, vl);
    __riscv_vlseg2e32_v_u32m4(&v_row1_even_32, &v_row1_odd_32, src1, vl);
    v_row0_even = __riscv_vreinterpret_v_u32m4_u8m4(v_row0_even_32);
    v_row0_odd = __riscv_vreinterpret_v_u32m4_u8m4(v_row0_odd_32);
    v_row1_even = __riscv_vreinterpret_v_u32m4_u8m4(v_row1_even_32);
    v_row1_odd = __riscv_vreinterpret_v_u32m4_u8m4(v_row1_odd_32);
    v_row0_sum = __riscv_vwaddu_vv_u16m8(v_row0_even, v_row0_odd, vl * 4);
    v_row1_sum = __riscv_vwaddu_vv_u16m8(v_row1_even, v_row1_odd, vl * 4);
    v_dst_16 = __riscv_vadd_vv_u16m8(v_row0_sum, v_row1_sum, vl * 4);
    // Use round-to-nearest-up mode for vnclip
    v_dst = __riscv_vnclipu_wx_u8m4(v_dst_16, 2, vl * 4);
    __riscv_vse8_v_u8m4(dst_argb, v_dst, vl * 4);
    w -= vl;
    src0 += vl * 2;
    src1 += vl * 2;
    dst_argb += vl * 4;
  } while (w > 0);
}

void ScaleARGBRowDownEven_RVV(const uint8_t* src_argb,
                              ptrdiff_t src_stride,
                              int src_stepx,
                              uint8_t* dst_argb,
                              int dst_width) {
  size_t w = (size_t)dst_width;
  const uint32_t* src = (const uint32_t*)(src_argb);
  uint32_t* dst = (uint32_t*)(dst_argb);
  const int stride_byte = src_stepx * 4;
  do {
    vuint32m8_t v_row;
    size_t vl = __riscv_vsetvl_e32m8(w);
    v_row = __riscv_vlse32_v_u32m8(src, stride_byte, vl);
    __riscv_vse32_v_u32m8(dst, v_row, vl);
    w -= vl;
    src += vl * src_stepx;
    dst += vl;
  } while (w > 0);
}

void ScaleARGBRowDownEvenBox_RVV(const uint8_t* src_argb,
                                 ptrdiff_t src_stride,
                                 int src_stepx,
                                 uint8_t* dst_argb,
                                 int dst_width) {
  size_t w = (size_t)dst_width;
  const uint32_t* src0 = (const uint32_t*)(src_argb);
  const uint32_t* src1 = (const uint32_t*)(src_argb + src_stride);
  const int stride_byte = src_stepx * 4;
  // NOTE: To match behavior on other platforms, vxrm (fixed-point rounding mode
  // register) is set to round-to-nearest-up mode(0).
  asm volatile("csrwi vxrm, 0");
  do {
    vuint8m4_t v_row0_low, v_row0_high, v_row1_low, v_row1_high, v_dst;
    vuint16m8_t v_row0_sum, v_row1_sum, v_sum;
    vuint32m4_t v_row0_low_32, v_row0_high_32, v_row1_low_32, v_row1_high_32;
    size_t vl = __riscv_vsetvl_e32m4(w);
    __riscv_vlsseg2e32_v_u32m4(&v_row0_low_32, &v_row0_high_32, src0,
                               stride_byte, vl);
    __riscv_vlsseg2e32_v_u32m4(&v_row1_low_32, &v_row1_high_32, src1,
                               stride_byte, vl);
    v_row0_low = __riscv_vreinterpret_v_u32m4_u8m4(v_row0_low_32);
    v_row0_high = __riscv_vreinterpret_v_u32m4_u8m4(v_row0_high_32);
    v_row1_low = __riscv_vreinterpret_v_u32m4_u8m4(v_row1_low_32);
    v_row1_high = __riscv_vreinterpret_v_u32m4_u8m4(v_row1_high_32);
    v_row0_sum = __riscv_vwaddu_vv_u16m8(v_row0_low, v_row0_high, vl * 4);
    v_row1_sum = __riscv_vwaddu_vv_u16m8(v_row1_low, v_row1_high, vl * 4);
    v_sum = __riscv_vadd_vv_u16m8(v_row0_sum, v_row1_sum, vl * 4);
    // Use round-to-nearest-up mode for vnclip
    v_dst = __riscv_vnclipu_wx_u8m4(v_sum, 2, vl * 4);
    __riscv_vse8_v_u8m4(dst_argb, v_dst, vl * 4);
    w -= vl;
    src0 += vl * src_stepx;
    src1 += vl * src_stepx;
    dst_argb += vl * 4;
  } while (w > 0);
}

void ScaleRowDown2_RVV(const uint8_t* src_ptr,
                       ptrdiff_t src_stride,
                       uint8_t* dst,
                       int dst_width) {
  size_t w = (size_t)dst_width;
  const uint16_t* src = (const uint16_t*)src_ptr;
  (void)src_stride;
  do {
    size_t vl = __riscv_vsetvl_e16m8(w);
    vuint16m8_t v_src = __riscv_vle16_v_u16m8(src, vl);
    vuint8m4_t v_dst = __riscv_vnsrl_wx_u8m4(v_src, 8, vl);
    __riscv_vse8_v_u8m4(dst, v_dst, vl);
    w -= vl;
    src += vl;
    dst += vl;
  } while (w > 0);
}

void ScaleRowDown2Linear_RVV(const uint8_t* src_ptr,
                             ptrdiff_t src_stride,
                             uint8_t* dst,
                             int dst_width) {
  size_t w = (size_t)dst_width;
  (void)src_stride;
  // NOTE: To match behavior on other platforms, vxrm (fixed-point rounding mode
  // register) is set to round-to-nearest-up mode(0).
  asm volatile("csrwi vxrm, 0");
  do {
    vuint8m4_t v_s0, v_s1, v_dst;
    size_t vl = __riscv_vsetvl_e8m4(w);
    __riscv_vlseg2e8_v_u8m4(&v_s0, &v_s1, src_ptr, vl);
    // Use round-to-nearest-up mode for averaging add
    v_dst = __riscv_vaaddu_vv_u8m4(v_s0, v_s1, vl);
    __riscv_vse8_v_u8m4(dst, v_dst, vl);
    w -= vl;
    src_ptr += 2 * vl;
    dst += vl;
  } while (w > 0);
}

void ScaleRowDown2Box_RVV(const uint8_t* src_ptr,
                          ptrdiff_t src_stride,
                          uint8_t* dst,
                          int dst_width) {
  const uint8_t* s = src_ptr;
  const uint8_t* t = src_ptr + src_stride;
  size_t w = (size_t)dst_width;
  // NOTE: To match behavior on other platforms, vxrm (fixed-point rounding mode
  // register) is set to round-to-nearest-up mode(0).
  asm volatile("csrwi vxrm, 0");
  do {
    size_t vl = __riscv_vsetvl_e8m4(w);
    vuint8m4_t v_s0, v_s1, v_t0, v_t1;
    vuint16m8_t v_s01, v_t01, v_st01;
    vuint8m4_t v_dst;
    __riscv_vlseg2e8_v_u8m4(&v_s0, &v_s1, s, vl);
    __riscv_vlseg2e8_v_u8m4(&v_t0, &v_t1, t, vl);
    v_s01 = __riscv_vwaddu_vv_u16m8(v_s0, v_s1, vl);
    v_t01 = __riscv_vwaddu_vv_u16m8(v_t0, v_t1, vl);
    v_st01 = __riscv_vadd_vv_u16m8(v_s01, v_t01, vl);
    // Use round-to-nearest-up mode for vnclip
    v_dst = __riscv_vnclipu_wx_u8m4(v_st01, 2, vl);
    __riscv_vse8_v_u8m4(dst, v_dst, vl);
    w -= vl;
    s += 2 * vl;
    t += 2 * vl;
    dst += vl;
  } while (w > 0);
}

void ScaleRowDown4_RVV(const uint8_t* src_ptr,
                       ptrdiff_t src_stride,
                       uint8_t* dst_ptr,
                       int dst_width) {
  size_t w = (size_t)dst_width;
  (void)src_stride;
  do {
    size_t vl = __riscv_vsetvl_e8m2(w);
    vuint8m2_t v_s0, v_s1, v_s2, v_s3;
    __riscv_vlseg4e8_v_u8m2(&v_s0, &v_s1, &v_s2, &v_s3, src_ptr, vl);
    __riscv_vse8_v_u8m2(dst_ptr, v_s2, vl);
    w -= vl;
    src_ptr += (4 * vl);
    dst_ptr += vl;
  } while (w > 0);
}

void ScaleRowDown4Box_RVV(const uint8_t* src_ptr,
                          ptrdiff_t src_stride,
                          uint8_t* dst_ptr,
                          int dst_width) {
  const uint8_t* src_ptr1 = src_ptr + src_stride;
  const uint8_t* src_ptr2 = src_ptr + src_stride * 2;
  const uint8_t* src_ptr3 = src_ptr + src_stride * 3;
  size_t w = (size_t)dst_width;
  // NOTE: To match behavior on other platforms, vxrm (fixed-point rounding mode
  // register) is set to round-to-nearest-up mode(0).
  asm volatile("csrwi vxrm, 0");
  do {
    vuint8m2_t v_s0, v_s1, v_s2, v_s3;
    vuint8m2_t v_t0, v_t1, v_t2, v_t3;
    vuint8m2_t v_u0, v_u1, v_u2, v_u3;
    vuint8m2_t v_v0, v_v1, v_v2, v_v3;
    vuint16m4_t v_s01, v_s23, v_t01, v_t23;
    vuint16m4_t v_u01, v_u23, v_v01, v_v23;
    vuint16m4_t v_st01, v_st23, v_uv01, v_uv23;
    vuint16m4_t v_st0123, v_uv0123, v_stuv0123;
    vuint8m2_t v_dst;
    size_t vl = __riscv_vsetvl_e8m2(w);

    __riscv_vlseg4e8_v_u8m2(&v_s0, &v_s1, &v_s2, &v_s3, src_ptr, vl);
    v_s01 = __riscv_vwaddu_vv_u16m4(v_s0, v_s1, vl);

    __riscv_vlseg4e8_v_u8m2(&v_t0, &v_t1, &v_t2, &v_t3, src_ptr1, vl);
    v_t01 = __riscv_vwaddu_vv_u16m4(v_t0, v_t1, vl);

    __riscv_vlseg4e8_v_u8m2(&v_u0, &v_u1, &v_u2, &v_u3, src_ptr2, vl);
    v_u01 = __riscv_vwaddu_vv_u16m4(v_u0, v_u1, vl);
    v_u23 = __riscv_vwaddu_vv_u16m4(v_u2, v_u3, vl);

    v_s23 = __riscv_vwaddu_vv_u16m4(v_s2, v_s3, vl);
    v_t23 = __riscv_vwaddu_vv_u16m4(v_t2, v_t3, vl);
    v_st01 = __riscv_vadd_vv_u16m4(v_s01, v_t01, vl);
    v_st23 = __riscv_vadd_vv_u16m4(v_s23, v_t23, vl);

    __riscv_vlseg4e8_v_u8m2(&v_v0, &v_v1, &v_v2, &v_v3, src_ptr3, vl);

    v_v01 = __riscv_vwaddu_vv_u16m4(v_v0, v_v1, vl);
    v_v23 = __riscv_vwaddu_vv_u16m4(v_v2, v_v3, vl);

    v_uv01 = __riscv_vadd_vv_u16m4(v_u01, v_v01, vl);
    v_uv23 = __riscv_vadd_vv_u16m4(v_u23, v_v23, vl);

    v_st0123 = __riscv_vadd_vv_u16m4(v_st01, v_st23, vl);
    v_uv0123 = __riscv_vadd_vv_u16m4(v_uv01, v_uv23, vl);
    v_stuv0123 = __riscv_vadd_vv_u16m4(v_st0123, v_uv0123, vl);
    // Use round-to-nearest-up mode for vnclip
    v_dst = __riscv_vnclipu_wx_u8m2(v_stuv0123, 4, vl);
    __riscv_vse8_v_u8m2(dst_ptr, v_dst, vl);
    w -= vl;
    src_ptr += 4 * vl;
    src_ptr1 += 4 * vl;
    src_ptr2 += 4 * vl;
    src_ptr3 += 4 * vl;
    dst_ptr += vl;
  } while (w > 0);
}

void ScaleRowDown34_RVV(const uint8_t* src_ptr,
                        ptrdiff_t src_stride,
                        uint8_t* dst_ptr,
                        int dst_width) {
  size_t w = (size_t)dst_width / 3u;
  do {
    size_t vl = __riscv_vsetvl_e8m2(w);
    vuint8m2_t v_s0, v_s1, v_s2, v_s3;
    __riscv_vlseg4e8_v_u8m2(&v_s0, &v_s1, &v_s2, &v_s3, src_ptr, vl);
    __riscv_vsseg3e8_v_u8m2(dst_ptr, v_s0, v_s1, v_s3, vl);
    w -= vl;
    src_ptr += 4 * vl;
    dst_ptr += 3 * vl;
  } while (w > 0);
}

void ScaleRowDown34_0_Box_RVV(const uint8_t* src_ptr,
                              ptrdiff_t src_stride,
                              uint8_t* dst_ptr,
                              int dst_width) {
  size_t w = (size_t)dst_width / 3u;
  const uint8_t* s = src_ptr;
  const uint8_t* t = src_ptr + src_stride;
  // NOTE: To match behavior on other platforms, vxrm (fixed-point rounding mode
  // register) is set to round-to-nearest-up mode(0).
  asm volatile("csrwi vxrm, 0");
  do {
    vuint8m2_t v_s0, v_s1, v_s2, v_s3;
    vuint16m4_t v_t0_u16, v_t1_u16, v_t2_u16, v_t3_u16;
    vuint8m2_t v_u0, v_u1, v_u2, v_u3;
    vuint16m4_t v_u1_u16;
    vuint8m2_t v_a0, v_a1, v_a2;
    size_t vl = __riscv_vsetvl_e8m2(w);
    __riscv_vlseg4e8_v_u8m2(&v_s0, &v_s1, &v_s2, &v_s3, s, vl);

    if (src_stride == 0) {
      v_t0_u16 = __riscv_vwaddu_vx_u16m4(v_s0, 2, vl);
      v_t1_u16 = __riscv_vwaddu_vx_u16m4(v_s1, 2, vl);
      v_t2_u16 = __riscv_vwaddu_vx_u16m4(v_s2, 2, vl);
      v_t3_u16 = __riscv_vwaddu_vx_u16m4(v_s3, 2, vl);
    } else {
      vuint8m2_t v_t0, v_t1, v_t2, v_t3;
      __riscv_vlseg4e8_v_u8m2(&v_t0, &v_t1, &v_t2, &v_t3, t, vl);
      v_t0_u16 = __riscv_vwaddu_vx_u16m4(v_t0, 0, vl);
      v_t1_u16 = __riscv_vwaddu_vx_u16m4(v_t1, 0, vl);
      v_t2_u16 = __riscv_vwaddu_vx_u16m4(v_t2, 0, vl);
      v_t3_u16 = __riscv_vwaddu_vx_u16m4(v_t3, 0, vl);
      t += 4 * vl;
    }

    v_t0_u16 = __riscv_vwmaccu_vx_u16m4(v_t0_u16, 3, v_s0, vl);
    v_t1_u16 = __riscv_vwmaccu_vx_u16m4(v_t1_u16, 3, v_s1, vl);
    v_t2_u16 = __riscv_vwmaccu_vx_u16m4(v_t2_u16, 3, v_s2, vl);
    v_t3_u16 = __riscv_vwmaccu_vx_u16m4(v_t3_u16, 3, v_s3, vl);

    // Use round-to-nearest-up mode for vnclip & averaging add
    v_u0 = __riscv_vnclipu_wx_u8m2(v_t0_u16, 2, vl);
    v_u1 = __riscv_vnclipu_wx_u8m2(v_t1_u16, 2, vl);
    v_u2 = __riscv_vnclipu_wx_u8m2(v_t2_u16, 2, vl);
    v_u3 = __riscv_vnclipu_wx_u8m2(v_t3_u16, 2, vl);

    // a0 = (src[0] * 3 + s[1] * 1 + 2) >> 2
    v_u1_u16 = __riscv_vwaddu_vx_u16m4(v_u1, 0, vl);
    v_u1_u16 = __riscv_vwmaccu_vx_u16m4(v_u1_u16, 3, v_u0, vl);
    v_a0 = __riscv_vnclipu_wx_u8m2(v_u1_u16, 2, vl);

    // a1 = (src[1] * 1 + s[2] * 1 + 1) >> 1
    v_a1 = __riscv_vaaddu_vv_u8m2(v_u1, v_u2, vl);

    // a2 = (src[2] * 1 + s[3] * 3 + 2) >> 2
    v_u1_u16 = __riscv_vwaddu_vx_u16m4(v_u2, 0, vl);
    v_u1_u16 = __riscv_vwmaccu_vx_u16m4(v_u1_u16, 3, v_u3, vl);
    v_a2 = __riscv_vnclipu_wx_u8m2(v_u1_u16, 2, vl);

    __riscv_vsseg3e8_v_u8m2(dst_ptr, v_a0, v_a1, v_a2, vl);

    w -= vl;
    s += 4 * vl;
    dst_ptr += 3 * vl;
  } while (w > 0);
}

void ScaleRowDown34_1_Box_RVV(const uint8_t* src_ptr,
                              ptrdiff_t src_stride,
                              uint8_t* dst_ptr,
                              int dst_width) {
  size_t w = (size_t)dst_width / 3u;
  const uint8_t* s = src_ptr;
  const uint8_t* t = src_ptr + src_stride;
  // NOTE: To match behavior on other platforms, vxrm (fixed-point rounding mode
  // register) is set to round-to-nearest-up mode(0).
  asm volatile("csrwi vxrm, 0");
  do {
    vuint8m2_t v_s0, v_s1, v_s2, v_s3;
    vuint8m2_t v_ave0, v_ave1, v_ave2, v_ave3;
    vuint16m4_t v_u1_u16;
    vuint8m2_t v_a0, v_a1, v_a2;
    size_t vl = __riscv_vsetvl_e8m2(w);
    __riscv_vlseg4e8_v_u8m2(&v_s0, &v_s1, &v_s2, &v_s3, s, vl);

    // Use round-to-nearest-up mode for vnclip & averaging add
    if (src_stride == 0) {
      v_ave0 = __riscv_vaaddu_vv_u8m2(v_s0, v_s0, vl);
      v_ave1 = __riscv_vaaddu_vv_u8m2(v_s1, v_s1, vl);
      v_ave2 = __riscv_vaaddu_vv_u8m2(v_s2, v_s2, vl);
      v_ave3 = __riscv_vaaddu_vv_u8m2(v_s3, v_s3, vl);
    } else {
      vuint8m2_t v_t0, v_t1, v_t2, v_t3;
      __riscv_vlseg4e8_v_u8m2(&v_t0, &v_t1, &v_t2, &v_t3, t, vl);
      v_ave0 = __riscv_vaaddu_vv_u8m2(v_s0, v_t0, vl);
      v_ave1 = __riscv_vaaddu_vv_u8m2(v_s1, v_t1, vl);
      v_ave2 = __riscv_vaaddu_vv_u8m2(v_s2, v_t2, vl);
      v_ave3 = __riscv_vaaddu_vv_u8m2(v_s3, v_t3, vl);
      t += 4 * vl;
    }
    // a0 = (src[0] * 3 + s[1] * 1 + 2) >> 2
    v_u1_u16 = __riscv_vwaddu_vx_u16m4(v_ave1, 0, vl);
    v_u1_u16 = __riscv_vwmaccu_vx_u16m4(v_u1_u16, 3, v_ave0, vl);
    v_a0 = __riscv_vnclipu_wx_u8m2(v_u1_u16, 2, vl);

    // a1 = (src[1] * 1 + s[2] * 1 + 1) >> 1
    v_a1 = __riscv_vaaddu_vv_u8m2(v_ave1, v_ave2, vl);

    // a2 = (src[2] * 1 + s[3] * 3 + 2) >> 2
    v_u1_u16 = __riscv_vwaddu_vx_u16m4(v_ave2, 0, vl);
    v_u1_u16 = __riscv_vwmaccu_vx_u16m4(v_u1_u16, 3, v_ave3, vl);
    v_a2 = __riscv_vnclipu_wx_u8m2(v_u1_u16, 2, vl);

    __riscv_vsseg3e8_v_u8m2(dst_ptr, v_a0, v_a1, v_a2, vl);

    w -= vl;
    s += 4 * vl;
    dst_ptr += 3 * vl;
  } while (w > 0);
}

void ScaleUVRowDown2_RVV(const uint8_t* src_uv,
                         ptrdiff_t src_stride,
                         uint8_t* dst_uv,
                         int dst_width) {
  size_t w = (size_t)dst_width;
  const uint32_t* src = (const uint32_t*)src_uv;
  uint16_t* dst = (uint16_t*)dst_uv;
  (void)src_stride;
  do {
    size_t vl = __riscv_vsetvl_e32m8(w);
    vuint32m8_t v_data = __riscv_vle32_v_u32m8(src, vl);
    vuint16m4_t v_u1v1 = __riscv_vnsrl_wx_u16m4(v_data, 16, vl);
    __riscv_vse16_v_u16m4(dst, v_u1v1, vl);
    w -= vl;
    src += vl;
    dst += vl;
  } while (w > 0);
}

void ScaleUVRowDown2Linear_RVV(const uint8_t* src_uv,
                               ptrdiff_t src_stride,
                               uint8_t* dst_uv,
                               int dst_width) {
  size_t w = (size_t)dst_width;
  const uint16_t* src = (const uint16_t*)src_uv;
  (void)src_stride;
  // NOTE: To match behavior on other platforms, vxrm (fixed-point rounding mode
  // register) is set to round-to-nearest-up mode(0).
  asm volatile("csrwi vxrm, 0");
  do {
    vuint8m4_t v_u0v0, v_u1v1, v_avg;
    vuint16m4_t v_u0v0_16, v_u1v1_16;
    size_t vl = __riscv_vsetvl_e16m4(w);
    vlseg2e16_v_u16m4(&v_u0v0_16, &v_u1v1_16, src, vl);
    v_u0v0 = __riscv_vreinterpret_v_u16m4_u8m4(v_u0v0_16);
    v_u1v1 = __riscv_vreinterpret_v_u16m4_u8m4(v_u1v1_16);
    // Use round-to-nearest-up mode for averaging add
    v_avg = __riscv_vaaddu_vv_u8m4(v_u0v0, v_u1v1, vl * 2);
    __riscv_vse8_v_u8m4(dst_uv, v_avg, vl * 2);
    w -= vl;
    src += vl * 2;
    dst_uv += vl * 2;
  } while (w > 0);
}

void ScaleUVRowDown2Box_RVV(const uint8_t* src_uv,
                            ptrdiff_t src_stride,
                            uint8_t* dst_uv,
                            int dst_width) {
  const uint8_t* src_uv_row1 = src_uv + src_stride;
  size_t w = (size_t)dst_width;
  // NOTE: To match behavior on other platforms, vxrm (fixed-point rounding mode
  // register) is set to round-to-nearest-up mode(0).
  asm volatile("csrwi vxrm, 0");
  do {
    vuint8m2_t v_u0_row0, v_v0_row0, v_u1_row0, v_v1_row0;
    vuint8m2_t v_u0_row1, v_v0_row1, v_u1_row1, v_v1_row1;
    vuint16m4_t v_u0u1_row0, v_u0u1_row1, v_v0v1_row0, v_v0v1_row1;
    vuint16m4_t v_sum0, v_sum1;
    vuint8m2_t v_dst_u, v_dst_v;
    size_t vl = __riscv_vsetvl_e8m2(w);

    __riscv_vlseg4e8_v_u8m2(&v_u0_row0, &v_v0_row0, &v_u1_row0, &v_v1_row0,
                            src_uv, vl);
    __riscv_vlseg4e8_v_u8m2(&v_u0_row1, &v_v0_row1, &v_u1_row1, &v_v1_row1,
                            src_uv_row1, vl);

    v_u0u1_row0 = __riscv_vwaddu_vv_u16m4(v_u0_row0, v_u1_row0, vl);
    v_u0u1_row1 = __riscv_vwaddu_vv_u16m4(v_u0_row1, v_u1_row1, vl);
    v_v0v1_row0 = __riscv_vwaddu_vv_u16m4(v_v0_row0, v_v1_row0, vl);
    v_v0v1_row1 = __riscv_vwaddu_vv_u16m4(v_v0_row1, v_v1_row1, vl);

    v_sum0 = __riscv_vadd_vv_u16m4(v_u0u1_row0, v_u0u1_row1, vl);
    v_sum1 = __riscv_vadd_vv_u16m4(v_v0v1_row0, v_v0v1_row1, vl);
    // Use round-to-nearest-up mode for vnclip
    v_dst_u = __riscv_vnclipu_wx_u8m2(v_sum0, 2, vl);
    v_dst_v = __riscv_vnclipu_wx_u8m2(v_sum1, 2, vl);

    __riscv_vsseg2e8_v_u8m2(dst_uv, v_dst_u, v_dst_v, vl);

    dst_uv += 2 * vl;
    src_uv += 4 * vl;
    w -= vl;
    src_uv_row1 += 4 * vl;
  } while (w > 0);
}

void ScaleUVRowDown4_RVV(const uint8_t* src_uv,
                         ptrdiff_t src_stride,
                         int src_stepx,
                         uint8_t* dst_uv,
                         int dst_width) {
  // Overflow will never happen here, since sizeof(size_t)/sizeof(int)=2.
  // dst_width = src_width / 4 and src_width is also int.
  size_t w = (size_t)dst_width * 8;
  (void)src_stride;
  (void)src_stepx;
  do {
    size_t vl = __riscv_vsetvl_e8m8(w);
    vuint8m8_t v_row = __riscv_vle8_v_u8m8(src_uv, vl);
    vuint64m8_t v_row_64 = __riscv_vreinterpret_v_u8m8_u64m8(v_row);
    // Narrowing without clipping
    vuint32m4_t v_tmp = __riscv_vncvt_x_x_w_u32m4(v_row_64, vl / 8);
    vuint16m2_t v_dst_16 = __riscv_vncvt_x_x_w_u16m2(v_tmp, vl / 8);
    vuint8m2_t v_dst = __riscv_vreinterpret_v_u16m2_u8m2(v_dst_16);
    __riscv_vse8_v_u8m2(dst_uv, v_dst, vl / 4);
    w -= vl;
    src_uv += vl;
    dst_uv += vl / 4;
  } while (w > 0);
}

void ScaleUVRowDownEven_RVV(const uint8_t* src_uv,
                            ptrdiff_t src_stride,
                            int src_stepx,
                            uint8_t* dst_uv,
                            int dst_width) {
  size_t w = (size_t)dst_width;
  const ptrdiff_t stride_byte = (ptrdiff_t)src_stepx * 2;
  const uint16_t* src = (const uint16_t*)(src_uv);
  uint16_t* dst = (uint16_t*)(dst_uv);
  (void)src_stride;
  do {
    size_t vl = __riscv_vsetvl_e16m8(w);
    vuint16m8_t v_row = __riscv_vlse16_v_u16m8(src, stride_byte, vl);
    __riscv_vse16_v_u16m8(dst, v_row, vl);
    w -= vl;
    src += vl * src_stepx;
    dst += vl;
  } while (w > 0);
}

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif

#endif  // !defined(LIBYUV_DISABLE_RVV) && defined(__riscv_vector)
