/*
 *  Copyright (c) 2019, Alliance for Open Media. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "config/aom_dsp_rtcd.h"
#include "config/aom_config.h"

#include "aom/aom_integer.h"
#include "aom_ports/mem.h"

static INLINE int horizontal_add_s16x8(const int16x8_t a) {
#if defined(__aarch64__)
  return vaddlvq_s16(a);
#else
  const int32x4_t b = vpaddlq_s16(a);
  const int64x2_t c = vpaddlq_s32(b);
  const int32x2_t d = vadd_s32(vreinterpret_s32_s64(vget_low_s64(c)),
                               vreinterpret_s32_s64(vget_high_s64(c)));
  return vget_lane_s32(d, 0);
#endif
}

static INLINE int horizontal_add_s32x4(const int32x4_t a) {
#if defined(__aarch64__)
  return vaddvq_s32(a);
#else
  const int64x2_t b = vpaddlq_s32(a);
  const int32x2_t c = vadd_s32(vreinterpret_s32_s64(vget_low_s64(b)),
                               vreinterpret_s32_s64(vget_high_s64(b)));
  return vget_lane_s32(c, 0);
#endif
}

static INLINE uint32_t horizontal_long_add_u16x8(const uint16x8_t vec_lo,
                                                 const uint16x8_t vec_hi) {
#if defined(__aarch64__)
  return vaddlvq_u16(vec_lo) + vaddlvq_u16(vec_hi);
#else
  const uint32x4_t vec_l_lo =
      vaddl_u16(vget_low_u16(vec_lo), vget_high_u16(vec_lo));
  const uint32x4_t vec_l_hi =
      vaddl_u16(vget_low_u16(vec_hi), vget_high_u16(vec_hi));
  const uint32x4_t a = vaddq_u32(vec_l_lo, vec_l_hi);
  const uint64x2_t b = vpaddlq_u32(a);
  const uint32x2_t c = vadd_u32(vreinterpret_u32_u64(vget_low_u64(b)),
                                vreinterpret_u32_u64(vget_high_u64(b)));
  return vget_lane_u32(c, 0);
#endif
}

static INLINE uint32_t horizontal_add_u16x8(const uint16x8_t a) {
#if defined(__aarch64__)
  return vaddlvq_u16(a);
#else
  const uint32x4_t b = vpaddlq_u16(a);
  const uint64x2_t c = vpaddlq_u32(b);
  const uint32x2_t d = vadd_u32(vreinterpret_u32_u64(vget_low_u64(c)),
                                vreinterpret_u32_u64(vget_high_u64(c)));
  return vget_lane_u32(d, 0);
#endif
}

static INLINE uint32_t horizontal_add_u16x4(const uint16x4_t a) {
#if defined(__aarch64__)
  return vaddlv_u16(a);
#else
  const uint32x2_t b = vpaddl_u16(a);
  const uint64x1_t c = vpaddl_u32(b);
  return vget_lane_u32(vreinterpret_u32_u64(c), 0);
#endif
}
