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

#ifndef AOM_AOM_DSP_SIMD_V64_INTRINSICS_ARM_H_
#define AOM_AOM_DSP_SIMD_V64_INTRINSICS_ARM_H_

#include <arm_neon.h>
#include <string.h>

#include "config/aom_config.h"

#include "aom_dsp/simd/v64_intrinsics_arm.h"
#include "aom_ports/arm.h"

#ifdef AOM_INCOMPATIBLE_GCC
#error Incompatible gcc
#endif

typedef int64x1_t v64;

SIMD_INLINE uint32_t v64_low_u32(v64 a) {
  return vget_lane_u32(vreinterpret_u32_s64(a), 0);
}

SIMD_INLINE uint32_t v64_high_u32(v64 a) {
  return vget_lane_u32(vreinterpret_u32_s64(a), 1);
}

SIMD_INLINE int32_t v64_low_s32(v64 a) {
  return vget_lane_s32(vreinterpret_s32_s64(a), 0);
}

SIMD_INLINE int32_t v64_high_s32(v64 a) {
  return vget_lane_s32(vreinterpret_s32_s64(a), 1);
}

SIMD_INLINE v64 v64_from_16(uint16_t a, uint16_t b, uint16_t c, uint16_t d) {
  return vcreate_s64((uint64_t)a << 48 | (uint64_t)b << 32 | (uint64_t)c << 16 |
                     d);
}

SIMD_INLINE v64 v64_from_32(uint32_t x, uint32_t y) {
  return vcreate_s64((uint64_t)x << 32 | y);
}

SIMD_INLINE v64 v64_from_64(uint64_t x) { return vcreate_s64(x); }

SIMD_INLINE uint64_t v64_u64(v64 x) { return (uint64_t)vget_lane_s64(x, 0); }

SIMD_INLINE uint32_t u32_load_aligned(const void *p) {
  return *((uint32_t *)p);
}

SIMD_INLINE uint32_t u32_load_unaligned(const void *p) {
  return vget_lane_u32(vreinterpret_u32_u8(vld1_u8((const uint8_t *)p)), 0);
}

SIMD_INLINE void u32_store_aligned(void *p, uint32_t a) {
  *((uint32_t *)p) = a;
}

SIMD_INLINE void u32_store_unaligned(void *p, uint32_t a) { memcpy(p, &a, 4); }

SIMD_INLINE v64 v64_load_aligned(const void *p) {
  return vreinterpret_s64_u8(vld1_u8((const uint8_t *)p));
}

SIMD_INLINE v64 v64_load_unaligned(const void *p) {
  return v64_load_aligned(p);
}

SIMD_INLINE void v64_store_aligned(void *p, v64 r) {
  vst1_u8((uint8_t *)p, vreinterpret_u8_s64(r));
}

SIMD_INLINE void v64_store_unaligned(void *p, v64 r) {
  vst1_u8((uint8_t *)p, vreinterpret_u8_s64(r));
}

// The following function requires an immediate.
// Some compilers will check this if it's optimising, others wont.
SIMD_INLINE v64 v64_align(v64 a, v64 b, unsigned int c) {
#if defined(__OPTIMIZE__) && __OPTIMIZE__ && !defined(__clang__)
  return c ? vreinterpret_s64_s8(
                 vext_s8(vreinterpret_s8_s64(b), vreinterpret_s8_s64(a), c))
           : b;
#else
  return c ? v64_from_64(((uint64_t)vget_lane_s64(b, 0) >> c * 8) |
                         ((uint64_t)vget_lane_s64(a, 0) << (8 - c) * 8))
           : b;
#endif
}

SIMD_INLINE v64 v64_zero(void) { return vreinterpret_s64_u8(vdup_n_u8(0)); }

SIMD_INLINE v64 v64_dup_8(uint8_t x) {
  return vreinterpret_s64_u8(vdup_n_u8(x));
}

SIMD_INLINE v64 v64_dup_16(uint16_t x) {
  return vreinterpret_s64_u16(vdup_n_u16(x));
}

SIMD_INLINE v64 v64_dup_32(uint32_t x) {
  return vreinterpret_s64_u32(vdup_n_u32(x));
}

SIMD_INLINE int64_t v64_dotp_su8(v64 x, v64 y) {
  int16x8_t t =
      vmulq_s16(vmovl_s8(vreinterpret_s8_s64(x)),
                vreinterpretq_s16_u16(vmovl_u8(vreinterpret_u8_s64(y))));
#if AOM_ARCH_AARCH64
  return vaddlvq_s16(t);
#else
  int64x2_t r = vpaddlq_s32(vpaddlq_s16(t));
  return vget_lane_s64(vadd_s64(vget_high_s64(r), vget_low_s64(r)), 0);
#endif
}

SIMD_INLINE int64_t v64_dotp_s16(v64 x, v64 y) {
#if AOM_ARCH_AARCH64
  return vaddlvq_s32(
      vmull_s16(vreinterpret_s16_s64(x), vreinterpret_s16_s64(y)));
#else
  int64x2_t r =
      vpaddlq_s32(vmull_s16(vreinterpret_s16_s64(x), vreinterpret_s16_s64(y)));
  return vget_lane_s64(vadd_s64(vget_high_s64(r), vget_low_s64(r)), 0);
#endif
}

SIMD_INLINE uint64_t v64_hadd_u8(v64 x) {
#if AOM_ARCH_AARCH64
  return vaddlv_u8(vreinterpret_u8_s64(x));
#else
  return vget_lane_u64(
      vpaddl_u32(vpaddl_u16(vpaddl_u8(vreinterpret_u8_s64(x)))), 0);
#endif
}

SIMD_INLINE int64_t v64_hadd_s16(v64 a) {
  return vget_lane_s64(vpaddl_s32(vpaddl_s16(vreinterpret_s16_s64(a))), 0);
}

typedef uint16x8_t sad64_internal;

SIMD_INLINE sad64_internal v64_sad_u8_init(void) { return vdupq_n_u16(0); }

// Implementation dependent return value. Result must be finalised with
// v64_sad_u8_sum().
SIMD_INLINE sad64_internal v64_sad_u8(sad64_internal s, v64 a, v64 b) {
  return vabal_u8(s, vreinterpret_u8_s64(a), vreinterpret_u8_s64(b));
}

SIMD_INLINE uint32_t v64_sad_u8_sum(sad64_internal s) {
#if AOM_ARCH_AARCH64
  return vaddlvq_u16(s);
#else
  uint64x2_t r = vpaddlq_u32(vpaddlq_u16(s));
  return (uint32_t)vget_lane_u64(vadd_u64(vget_high_u64(r), vget_low_u64(r)),
                                 0);
#endif
}

typedef uint32x4_t ssd64_internal;

SIMD_INLINE ssd64_internal v64_ssd_u8_init(void) { return vdupq_n_u32(0); }

// Implementation dependent return value. Result must be finalised with
// v64_ssd_u8_sum().
SIMD_INLINE ssd64_internal v64_ssd_u8(ssd64_internal s, v64 a, v64 b) {
  uint8x8_t t = vabd_u8(vreinterpret_u8_s64(a), vreinterpret_u8_s64(b));
  return vaddq_u32(s, vpaddlq_u16(vmull_u8(t, t)));
}

SIMD_INLINE uint32_t v64_ssd_u8_sum(ssd64_internal s) {
#if AOM_ARCH_AARCH64
  return vaddvq_u32(s);
#else
  uint64x2_t t = vpaddlq_u32(s);
  return vget_lane_u32(
      vreinterpret_u32_u64(vadd_u64(vget_high_u64(t), vget_low_u64(t))), 0);
#endif
}

SIMD_INLINE v64 v64_or(v64 x, v64 y) { return vorr_s64(x, y); }

SIMD_INLINE v64 v64_xor(v64 x, v64 y) { return veor_s64(x, y); }

SIMD_INLINE v64 v64_and(v64 x, v64 y) { return vand_s64(x, y); }

SIMD_INLINE v64 v64_andn(v64 x, v64 y) { return vbic_s64(x, y); }

SIMD_INLINE v64 v64_add_8(v64 x, v64 y) {
  return vreinterpret_s64_u8(
      vadd_u8(vreinterpret_u8_s64(x), vreinterpret_u8_s64(y)));
}

SIMD_INLINE v64 v64_sadd_u8(v64 x, v64 y) {
  return vreinterpret_s64_u8(
      vqadd_u8(vreinterpret_u8_s64(x), vreinterpret_u8_s64(y)));
}

SIMD_INLINE v64 v64_sadd_s8(v64 x, v64 y) {
  return vreinterpret_s64_s8(
      vqadd_s8(vreinterpret_s8_s64(x), vreinterpret_s8_s64(y)));
}

SIMD_INLINE v64 v64_add_16(v64 x, v64 y) {
  return vreinterpret_s64_s16(
      vadd_s16(vreinterpret_s16_s64(x), vreinterpret_s16_s64(y)));
}

SIMD_INLINE v64 v64_sadd_s16(v64 x, v64 y) {
  return vreinterpret_s64_s16(
      vqadd_s16(vreinterpret_s16_s64(x), vreinterpret_s16_s64(y)));
}

SIMD_INLINE v64 v64_add_32(v64 x, v64 y) {
  return vreinterpret_s64_u32(
      vadd_u32(vreinterpret_u32_s64(x), vreinterpret_u32_s64(y)));
}

SIMD_INLINE v64 v64_sub_8(v64 x, v64 y) {
  return vreinterpret_s64_u8(
      vsub_u8(vreinterpret_u8_s64(x), vreinterpret_u8_s64(y)));
}

SIMD_INLINE v64 v64_sub_16(v64 x, v64 y) {
  return vreinterpret_s64_s16(
      vsub_s16(vreinterpret_s16_s64(x), vreinterpret_s16_s64(y)));
}

SIMD_INLINE v64 v64_ssub_s16(v64 x, v64 y) {
  return vreinterpret_s64_s16(
      vqsub_s16(vreinterpret_s16_s64(x), vreinterpret_s16_s64(y)));
}

SIMD_INLINE v64 v64_ssub_u16(v64 x, v64 y) {
  return vreinterpret_s64_u16(
      vqsub_u16(vreinterpret_u16_s64(x), vreinterpret_u16_s64(y)));
}

SIMD_INLINE v64 v64_ssub_u8(v64 x, v64 y) {
  return vreinterpret_s64_u8(
      vqsub_u8(vreinterpret_u8_s64(x), vreinterpret_u8_s64(y)));
}

SIMD_INLINE v64 v64_ssub_s8(v64 x, v64 y) {
  return vreinterpret_s64_s8(
      vqsub_s8(vreinterpret_s8_s64(x), vreinterpret_s8_s64(y)));
}

SIMD_INLINE v64 v64_sub_32(v64 x, v64 y) {
  return vreinterpret_s64_s32(
      vsub_s32(vreinterpret_s32_s64(x), vreinterpret_s32_s64(y)));
}

SIMD_INLINE v64 v64_abs_s16(v64 x) {
  return vreinterpret_s64_s16(vabs_s16(vreinterpret_s16_s64(x)));
}

SIMD_INLINE v64 v64_abs_s8(v64 x) {
  return vreinterpret_s64_s8(vabs_s8(vreinterpret_s8_s64(x)));
}

SIMD_INLINE v64 v64_mullo_s16(v64 x, v64 y) {
  return vreinterpret_s64_s16(
      vmul_s16(vreinterpret_s16_s64(x), vreinterpret_s16_s64(y)));
}

SIMD_INLINE v64 v64_mulhi_s16(v64 x, v64 y) {
#if AOM_ARCH_AARCH64
  int16x8_t t = vreinterpretq_s16_s32(
      vmull_s16(vreinterpret_s16_s64(x), vreinterpret_s16_s64(y)));
  return vget_low_s64(vreinterpretq_s64_s16(vuzp2q_s16(t, t)));
#else
  return vreinterpret_s64_s16(vmovn_s32(vshrq_n_s32(
      vmull_s16(vreinterpret_s16_s64(x), vreinterpret_s16_s64(y)), 16)));
#endif
}

SIMD_INLINE v64 v64_mullo_s32(v64 x, v64 y) {
  return vreinterpret_s64_s32(
      vmul_s32(vreinterpret_s32_s64(x), vreinterpret_s32_s64(y)));
}

SIMD_INLINE v64 v64_madd_s16(v64 x, v64 y) {
  int32x4_t t = vmull_s16(vreinterpret_s16_s64(x), vreinterpret_s16_s64(y));
  return vreinterpret_s64_s32(
      vpadd_s32(vreinterpret_s32_s64(vget_low_s64(vreinterpretq_s64_s32(t))),
                vreinterpret_s32_s64(vget_high_s64(vreinterpretq_s64_s32(t)))));
}

SIMD_INLINE v64 v64_madd_us8(v64 x, v64 y) {
  int16x8_t t =
      vmulq_s16(vreinterpretq_s16_u16(vmovl_u8(vreinterpret_u8_s64(x))),
                vmovl_s8(vreinterpret_s8_s64(y)));
  return vreinterpret_s64_s16(vqmovn_s32(vpaddlq_s16(t)));
}

SIMD_INLINE v64 v64_avg_u8(v64 x, v64 y) {
  return vreinterpret_s64_u8(
      vrhadd_u8(vreinterpret_u8_s64(x), vreinterpret_u8_s64(y)));
}

SIMD_INLINE v64 v64_rdavg_u8(v64 x, v64 y) {
  return vreinterpret_s64_u8(
      vhadd_u8(vreinterpret_u8_s64(x), vreinterpret_u8_s64(y)));
}

SIMD_INLINE v64 v64_rdavg_u16(v64 x, v64 y) {
  return vreinterpret_s64_u16(
      vhadd_u16(vreinterpret_u16_s64(x), vreinterpret_u16_s64(y)));
}

SIMD_INLINE v64 v64_avg_u16(v64 x, v64 y) {
  return vreinterpret_s64_u16(
      vrhadd_u16(vreinterpret_u16_s64(x), vreinterpret_u16_s64(y)));
}

SIMD_INLINE v64 v64_max_u8(v64 x, v64 y) {
  return vreinterpret_s64_u8(
      vmax_u8(vreinterpret_u8_s64(x), vreinterpret_u8_s64(y)));
}

SIMD_INLINE v64 v64_min_u8(v64 x, v64 y) {
  return vreinterpret_s64_u8(
      vmin_u8(vreinterpret_u8_s64(x), vreinterpret_u8_s64(y)));
}

SIMD_INLINE v64 v64_max_s8(v64 x, v64 y) {
  return vreinterpret_s64_s8(
      vmax_s8(vreinterpret_s8_s64(x), vreinterpret_s8_s64(y)));
}

SIMD_INLINE v64 v64_min_s8(v64 x, v64 y) {
  return vreinterpret_s64_s8(
      vmin_s8(vreinterpret_s8_s64(x), vreinterpret_s8_s64(y)));
}

SIMD_INLINE v64 v64_max_s16(v64 x, v64 y) {
  return vreinterpret_s64_s16(
      vmax_s16(vreinterpret_s16_s64(x), vreinterpret_s16_s64(y)));
}

SIMD_INLINE v64 v64_min_s16(v64 x, v64 y) {
  return vreinterpret_s64_s16(
      vmin_s16(vreinterpret_s16_s64(x), vreinterpret_s16_s64(y)));
}

SIMD_INLINE v64 v64_ziplo_8(v64 x, v64 y) {
#if AOM_ARCH_AARCH64
  return vreinterpret_s64_u8(
      vzip1_u8(vreinterpret_u8_s64(y), vreinterpret_u8_s64(x)));
#else
  uint8x8x2_t r = vzip_u8(vreinterpret_u8_s64(y), vreinterpret_u8_s64(x));
  return vreinterpret_s64_u8(r.val[0]);
#endif
}

SIMD_INLINE v64 v64_ziphi_8(v64 x, v64 y) {
#if AOM_ARCH_AARCH64
  return vreinterpret_s64_u8(
      vzip2_u8(vreinterpret_u8_s64(y), vreinterpret_u8_s64(x)));
#else
  uint8x8x2_t r = vzip_u8(vreinterpret_u8_s64(y), vreinterpret_u8_s64(x));
  return vreinterpret_s64_u8(r.val[1]);
#endif
}

SIMD_INLINE v64 v64_ziplo_16(v64 x, v64 y) {
#if AOM_ARCH_AARCH64
  return vreinterpret_s64_u16(
      vzip1_u16(vreinterpret_u16_s64(y), vreinterpret_u16_s64(x)));
#else
  int16x4x2_t r = vzip_s16(vreinterpret_s16_s64(y), vreinterpret_s16_s64(x));
  return vreinterpret_s64_s16(r.val[0]);
#endif
}

SIMD_INLINE v64 v64_ziphi_16(v64 x, v64 y) {
#if AOM_ARCH_AARCH64
  return vreinterpret_s64_u16(
      vzip2_u16(vreinterpret_u16_s64(y), vreinterpret_u16_s64(x)));
#else
  int16x4x2_t r = vzip_s16(vreinterpret_s16_s64(y), vreinterpret_s16_s64(x));
  return vreinterpret_s64_s16(r.val[1]);
#endif
}

SIMD_INLINE v64 v64_ziplo_32(v64 x, v64 y) {
#if AOM_ARCH_AARCH64
  return vreinterpret_s64_u32(
      vzip1_u32(vreinterpret_u32_s64(y), vreinterpret_u32_s64(x)));
#else
  int32x2x2_t r = vzip_s32(vreinterpret_s32_s64(y), vreinterpret_s32_s64(x));
  return vreinterpret_s64_s32(r.val[0]);
#endif
}

SIMD_INLINE v64 v64_ziphi_32(v64 x, v64 y) {
#if AOM_ARCH_AARCH64
  return vreinterpret_s64_u32(
      vzip2_u32(vreinterpret_u32_s64(y), vreinterpret_u32_s64(x)));
#else
  int32x2x2_t r = vzip_s32(vreinterpret_s32_s64(y), vreinterpret_s32_s64(x));
  return vreinterpret_s64_s32(r.val[1]);
#endif
}

SIMD_INLINE v64 v64_unpacklo_u8_s16(v64 a) {
  return vreinterpret_s64_u16(vget_low_u16(vmovl_u8(vreinterpret_u8_s64(a))));
}

SIMD_INLINE v64 v64_unpackhi_u8_s16(v64 a) {
  return vreinterpret_s64_u16(vget_high_u16(vmovl_u8(vreinterpret_u8_s64(a))));
}

SIMD_INLINE v64 v64_unpacklo_s8_s16(v64 a) {
  return vreinterpret_s64_s16(vget_low_s16(vmovl_s8(vreinterpret_s8_s64(a))));
}

SIMD_INLINE v64 v64_unpackhi_s8_s16(v64 a) {
  return vreinterpret_s64_s16(vget_high_s16(vmovl_s8(vreinterpret_s8_s64(a))));
}

SIMD_INLINE v64 v64_pack_s32_s16(v64 x, v64 y) {
  return vreinterpret_s64_s16(vqmovn_s32(
      vcombine_s32(vreinterpret_s32_s64(y), vreinterpret_s32_s64(x))));
}

SIMD_INLINE v64 v64_pack_s32_u16(v64 x, v64 y) {
  return vreinterpret_s64_u16(vqmovun_s32(
      vcombine_s32(vreinterpret_s32_s64(y), vreinterpret_s32_s64(x))));
}

SIMD_INLINE v64 v64_pack_s16_u8(v64 x, v64 y) {
  return vreinterpret_s64_u8(vqmovun_s16(vreinterpretq_s16_s32(
      vcombine_s32(vreinterpret_s32_s64(y), vreinterpret_s32_s64(x)))));
}

SIMD_INLINE v64 v64_pack_s16_s8(v64 x, v64 y) {
  return vreinterpret_s64_s8(vqmovn_s16(vreinterpretq_s16_s32(
      vcombine_s32(vreinterpret_s32_s64(y), vreinterpret_s32_s64(x)))));
}

SIMD_INLINE v64 v64_unziplo_8(v64 x, v64 y) {
#if AOM_ARCH_AARCH64
  return vreinterpret_s64_u8(
      vuzp1_u8(vreinterpret_u8_s64(y), vreinterpret_u8_s64(x)));
#else
  uint8x8x2_t r = vuzp_u8(vreinterpret_u8_s64(y), vreinterpret_u8_s64(x));
  return vreinterpret_s64_u8(r.val[0]);
#endif
}

SIMD_INLINE v64 v64_unziphi_8(v64 x, v64 y) {
#if AOM_ARCH_AARCH64
  return vreinterpret_s64_u8(
      vuzp2_u8(vreinterpret_u8_s64(y), vreinterpret_u8_s64(x)));
#else
  uint8x8x2_t r = vuzp_u8(vreinterpret_u8_s64(y), vreinterpret_u8_s64(x));
  return vreinterpret_s64_u8(r.val[1]);
#endif
}

SIMD_INLINE v64 v64_unziplo_16(v64 x, v64 y) {
#if AOM_ARCH_AARCH64
  return vreinterpret_s64_u16(
      vuzp1_u16(vreinterpret_u16_s64(y), vreinterpret_u16_s64(x)));
#else
  uint16x4x2_t r = vuzp_u16(vreinterpret_u16_s64(y), vreinterpret_u16_s64(x));
  return vreinterpret_s64_u16(r.val[0]);
#endif
}

SIMD_INLINE v64 v64_unziphi_16(v64 x, v64 y) {
#if AOM_ARCH_AARCH64
  return vreinterpret_s64_u16(
      vuzp2_u16(vreinterpret_u16_s64(y), vreinterpret_u16_s64(x)));
#else
  uint16x4x2_t r = vuzp_u16(vreinterpret_u16_s64(y), vreinterpret_u16_s64(x));
  return vreinterpret_s64_u16(r.val[1]);
#endif
}

SIMD_INLINE v64 v64_unpacklo_s16_s32(v64 x) {
  return vreinterpret_s64_s32(vget_low_s32(vmovl_s16(vreinterpret_s16_s64(x))));
}

SIMD_INLINE v64 v64_unpacklo_u16_s32(v64 x) {
  return vreinterpret_s64_u32(vget_low_u32(vmovl_u16(vreinterpret_u16_s64(x))));
}

SIMD_INLINE v64 v64_unpackhi_s16_s32(v64 x) {
  return vreinterpret_s64_s32(
      vget_high_s32(vmovl_s16(vreinterpret_s16_s64(x))));
}

SIMD_INLINE v64 v64_unpackhi_u16_s32(v64 x) {
  return vreinterpret_s64_u32(
      vget_high_u32(vmovl_u16(vreinterpret_u16_s64(x))));
}

SIMD_INLINE v64 v64_shuffle_8(v64 x, v64 pattern) {
  return vreinterpret_s64_u8(
      vtbl1_u8(vreinterpret_u8_s64(x), vreinterpret_u8_s64(pattern)));
}

SIMD_INLINE v64 v64_cmpgt_s8(v64 x, v64 y) {
  return vreinterpret_s64_u8(
      vcgt_s8(vreinterpret_s8_s64(x), vreinterpret_s8_s64(y)));
}

SIMD_INLINE v64 v64_cmplt_s8(v64 x, v64 y) {
  return vreinterpret_s64_u8(
      vclt_s8(vreinterpret_s8_s64(x), vreinterpret_s8_s64(y)));
}

SIMD_INLINE v64 v64_cmpeq_8(v64 x, v64 y) {
  return vreinterpret_s64_u8(
      vceq_u8(vreinterpret_u8_s64(x), vreinterpret_u8_s64(y)));
}

SIMD_INLINE v64 v64_cmpgt_s16(v64 x, v64 y) {
  return vreinterpret_s64_u16(
      vcgt_s16(vreinterpret_s16_s64(x), vreinterpret_s16_s64(y)));
}

SIMD_INLINE v64 v64_cmplt_s16(v64 x, v64 y) {
  return vreinterpret_s64_u16(
      vclt_s16(vreinterpret_s16_s64(x), vreinterpret_s16_s64(y)));
}

SIMD_INLINE v64 v64_cmpeq_16(v64 x, v64 y) {
  return vreinterpret_s64_u16(
      vceq_s16(vreinterpret_s16_s64(x), vreinterpret_s16_s64(y)));
}

SIMD_INLINE v64 v64_shl_8(v64 a, unsigned int c) {
  return vreinterpret_s64_u8(
      vshl_u8(vreinterpret_u8_s64(a), vdup_n_s8((int8_t)c)));
}

SIMD_INLINE v64 v64_shr_u8(v64 a, unsigned int c) {
  return vreinterpret_s64_u8(
      vshl_u8(vreinterpret_u8_s64(a), vdup_n_s8(-(int8_t)c)));
}

SIMD_INLINE v64 v64_shr_s8(v64 a, unsigned int c) {
  return vreinterpret_s64_s8(
      vshl_s8(vreinterpret_s8_s64(a), vdup_n_s8(-(int8_t)c)));
}

SIMD_INLINE v64 v64_shl_16(v64 a, unsigned int c) {
  return vreinterpret_s64_u16(
      vshl_u16(vreinterpret_u16_s64(a), vdup_n_s16((int16_t)c)));
}

SIMD_INLINE v64 v64_shr_u16(v64 a, unsigned int c) {
  return vreinterpret_s64_u16(
      vshl_u16(vreinterpret_u16_s64(a), vdup_n_s16(-(int16_t)c)));
}

SIMD_INLINE v64 v64_shr_s16(v64 a, unsigned int c) {
  return vreinterpret_s64_s16(
      vshl_s16(vreinterpret_s16_s64(a), vdup_n_s16(-(int16_t)c)));
}

SIMD_INLINE v64 v64_shl_32(v64 a, unsigned int c) {
  return vreinterpret_s64_u32(
      vshl_u32(vreinterpret_u32_s64(a), vdup_n_s32((int32_t)c)));
}

SIMD_INLINE v64 v64_shr_u32(v64 a, unsigned int c) {
  return vreinterpret_s64_u32(
      vshl_u32(vreinterpret_u32_s64(a), vdup_n_s32(-(int32_t)c)));
}

SIMD_INLINE v64 v64_shr_s32(v64 a, unsigned int c) {
  return vreinterpret_s64_s32(
      vshl_s32(vreinterpret_s32_s64(a), vdup_n_s32(-(int32_t)c)));
}

// The following functions require an immediate.
// Some compilers will check this during optimisation, others wont.
#if defined(__OPTIMIZE__) && __OPTIMIZE__ && !defined(__clang__)

SIMD_INLINE v64 v64_shl_n_byte(v64 a, unsigned int c) {
  return vshl_n_s64(a, c * 8);
}

SIMD_INLINE v64 v64_shr_n_byte(v64 a, unsigned int c) {
  return c ? (v64)vshr_n_u64(vreinterpret_u64_s64(a), c * 8) : a;
}

SIMD_INLINE v64 v64_shl_n_8(v64 a, unsigned int c) {
  return c ? vreinterpret_s64_u8(vshl_n_u8(vreinterpret_u8_s64(a), c)) : a;
}

SIMD_INLINE v64 v64_shr_n_u8(v64 a, unsigned int c) {
  return c ? vreinterpret_s64_u8(vshr_n_u8(vreinterpret_u8_s64(a), c)) : a;
}

SIMD_INLINE v64 v64_shr_n_s8(v64 a, unsigned int c) {
  return c ? vreinterpret_s64_s8(vshr_n_s8(vreinterpret_s8_s64(a), c)) : a;
}

SIMD_INLINE v64 v64_shl_n_16(v64 a, unsigned int c) {
  return c ? vreinterpret_s64_u16(vshl_n_u16(vreinterpret_u16_s64(a), c)) : a;
}

SIMD_INLINE v64 v64_shr_n_u16(v64 a, unsigned int c) {
  return c ? vreinterpret_s64_u16(vshr_n_u16(vreinterpret_u16_s64(a), c)) : a;
}

SIMD_INLINE v64 v64_shr_n_s16(v64 a, unsigned int c) {
  return c ? vreinterpret_s64_s16(vshr_n_s16(vreinterpret_s16_s64(a), c)) : a;
}

SIMD_INLINE v64 v64_shl_n_32(v64 a, unsigned int c) {
  return c ? vreinterpret_s64_u32(vshl_n_u32(vreinterpret_u32_s64(a), c)) : a;
}

SIMD_INLINE v64 v64_shr_n_u32(v64 a, unsigned int c) {
  return c ? vreinterpret_s64_u32(vshr_n_u32(vreinterpret_u32_s64(a), c)) : a;
}

SIMD_INLINE v64 v64_shr_n_s32(v64 a, unsigned int c) {
  return c ? vreinterpret_s64_s32(vshr_n_s32(vreinterpret_s32_s64(a), c)) : a;
}

#else

SIMD_INLINE v64 v64_shl_n_byte(v64 a, unsigned int c) {
  return v64_from_64(v64_u64(a) << c * 8);
}

SIMD_INLINE v64 v64_shr_n_byte(v64 a, unsigned int c) {
  return v64_from_64(v64_u64(a) >> c * 8);
}

SIMD_INLINE v64 v64_shl_n_8(v64 a, unsigned int c) { return v64_shl_8(a, c); }

SIMD_INLINE v64 v64_shr_n_u8(v64 a, unsigned int c) { return v64_shr_u8(a, c); }

SIMD_INLINE v64 v64_shr_n_s8(v64 a, unsigned int c) { return v64_shr_s8(a, c); }

SIMD_INLINE v64 v64_shl_n_16(v64 a, unsigned int c) { return v64_shl_16(a, c); }

SIMD_INLINE v64 v64_shr_n_u16(v64 a, unsigned int c) {
  return v64_shr_u16(a, c);
}

SIMD_INLINE v64 v64_shr_n_s16(v64 a, unsigned int c) {
  return v64_shr_s16(a, c);
}

SIMD_INLINE v64 v64_shl_n_32(v64 a, unsigned int c) { return v64_shl_32(a, c); }

SIMD_INLINE v64 v64_shr_n_u32(v64 a, unsigned int c) {
  return v64_shr_u32(a, c);
}

SIMD_INLINE v64 v64_shr_n_s32(v64 a, unsigned int c) {
  return v64_shr_s32(a, c);
}

#endif

#endif  // AOM_AOM_DSP_SIMD_V64_INTRINSICS_ARM_H_
