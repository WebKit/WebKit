/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_AV1_COMMON_CONVOLVE_2D_SR_HWY_H_
#define AOM_AV1_COMMON_CONVOLVE_2D_SR_HWY_H_

#include "av1/common/convolve.h"
#include "av1/common/filter.h"
#include "config/aom_config.h"
#include "config/av1_rtcd.h"
#include "third_party/highway/hwy/highway.h"

HWY_BEFORE_NAMESPACE();

namespace {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

constexpr hn::ScalableTag<uint8_t> uint8xN_tag;
constexpr hn::ScalableTag<int8_t> int8xN_tag;
constexpr hn::ScalableTag<int16_t> int16xN_tag;
constexpr hn::ScalableTag<uint32_t> uint32xN_tag;
constexpr hn::ScalableTag<int32_t> int32xN_tag;

constexpr hn::CappedTag<uint8_t, 16> uint8x16_capped_tag;
constexpr hn::CappedTag<uint8_t, 8> uint8x8_capped_tag;

using UVec8 = hn::Vec<decltype(uint8xN_tag)>;
using IVec8 = hn::Vec<decltype(int8xN_tag)>;
using IVec16 = hn::Vec<decltype(int16xN_tag)>;
using UVec32 = hn::Vec<decltype(uint32xN_tag)>;
using IVec32 = hn::Vec<decltype(int32xN_tag)>;

HWY_ALIGN constexpr uint8_t kFilt1[16] = { 0, 1, 1, 2, 2, 3, 3, 4,
                                           4, 5, 5, 6, 6, 7, 7, 8 };
HWY_ALIGN constexpr uint8_t kFilt2[16] = { 2, 3, 3, 4, 4, 5, 5, 6,
                                           6, 7, 7, 8, 8, 9, 9, 10 };
HWY_ALIGN constexpr uint8_t kFilt3[16] = { 4, 5, 5, 6,  6,  7,  7,  8,
                                           8, 9, 9, 10, 10, 11, 11, 12 };
HWY_ALIGN constexpr uint8_t kFilt4[16] = { 6,  7,  7,  8,  8,  9,  9,  10,
                                           10, 11, 11, 12, 12, 13, 13, 14 };

// Horizontal convolve helpers
template <int taps, class D16, class V8, class VI8>
HWY_ATTR HWY_INLINE hn::VFromD<D16> ConvolveLowbdX(D16 d16, V8 data,
                                                   const V8 *masks,
                                                   const VI8 *coeffs) {
  constexpr int num_coeffs = taps / 2;
  hn::VFromD<D16> prods[4];
  for (int k = 0; k < num_coeffs; ++k) {
    auto s = hn::TableLookupBytes(data, masks[k]);
    prods[k] = hn::SatWidenMulPairwiseAdd(d16, s, coeffs[k]);
  }

  HWY_IF_CONSTEXPR(num_coeffs == 1) { return prods[0]; }
  HWY_IF_CONSTEXPR(num_coeffs == 2) { return hn::Add(prods[0], prods[1]); }
  HWY_IF_CONSTEXPR(num_coeffs == 3) {
    return hn::Add(hn::Add(prods[0], prods[1]), prods[2]);
  }
  HWY_IF_CONSTEXPR(num_coeffs == 4) {
    return hn::Add(hn::Add(prods[0], prods[1]), hn::Add(prods[2], prods[3]));
  }
}

template <int taps, class D16, class V8, class VI8>
HWY_ATTR HWY_INLINE hn::VFromD<D16> ConvolveLowbdXShuffleFree(
    D16 d16, V8 data, const VI8 *coeffs) {
  // We only support AVX2 and AVX512, which have > 8 lanes of int16_t.
  static_assert(hn::MaxLanes(D16()) > 8, "Only AVX2 and AVX512 are supported");
  constexpr int num_coeffs = taps / 2;

  using D8_twice = hn::DFromV<V8>;
  const D8_twice d8_twice;

  hn::VFromD<D16> prods[4];
  auto mask1 = hn::LoadDup128(d8_twice, kFilt1);
  prods[0] = hn::SatWidenMulPairwiseAdd(d16, hn::TableLookupBytes(data, mask1),
                                        coeffs[0]);

  HWY_IF_CONSTEXPR(num_coeffs >= 2) {
    auto mask2 = hn::LoadDup128(d8_twice, kFilt2);
    prods[1] = hn::SatWidenMulPairwiseAdd(
        d16, hn::TableLookupBytes(data, mask2), coeffs[1]);
  }
  HWY_IF_CONSTEXPR(num_coeffs >= 3) {
    auto mask3 = hn::LoadDup128(d8_twice, kFilt3);
    prods[2] = hn::SatWidenMulPairwiseAdd(
        d16, hn::TableLookupBytes(data, mask3), coeffs[2]);
  }
  HWY_IF_CONSTEXPR(num_coeffs >= 4) {
    auto mask4 = hn::LoadDup128(d8_twice, kFilt4);
    prods[3] = hn::SatWidenMulPairwiseAdd(
        d16, hn::TableLookupBytes(data, mask4), coeffs[3]);
  }

  HWY_IF_CONSTEXPR(num_coeffs == 1) { return prods[0]; }
  HWY_IF_CONSTEXPR(num_coeffs == 2) { return hn::Add(prods[0], prods[1]); }
  HWY_IF_CONSTEXPR(num_coeffs == 3) {
    return hn::Add(hn::Add(prods[0], prods[1]), prods[2]);
  }
  HWY_IF_CONSTEXPR(num_coeffs == 4) {
    return hn::Add(hn::Add(prods[0], prods[1]), hn::Add(prods[2], prods[3]));
  }
}

template <class D16, class V8, class V16_coeff>
HWY_ATTR HWY_INLINE hn::VFromD<D16> ConvolveLowbdX12TapFromS(
    D16 d16, V8 s0, V8 s1, V8 s2, V8 s3, V8 s4, V8 s5, V16_coeff coeff0,
    V16_coeff coeff1, V16_coeff coeff2, V16_coeff coeff3, V16_coeff coeff4,
    V16_coeff coeff5) {
  using D16_half = hn::Half<D16>;
  using D32 = hn::Rebind<int32_t, D16_half>;
  const D32 d32;
  const D16_half d16_half;

  auto s0_low_16 = hn::PromoteLowerTo(d16, s0);
  auto s0_high_16 = hn::PromoteUpperTo(d16, s0);
  auto s1_low_16 = hn::PromoteLowerTo(d16, s1);
  auto s1_high_16 = hn::PromoteUpperTo(d16, s1);
  auto s2_low_16 = hn::PromoteLowerTo(d16, s2);
  auto s2_high_16 = hn::PromoteUpperTo(d16, s2);
  auto s3_low_16 = hn::PromoteLowerTo(d16, s3);
  auto s3_high_16 = hn::PromoteUpperTo(d16, s3);
  auto s4_low_16 = hn::PromoteLowerTo(d16, s4);
  auto s4_high_16 = hn::PromoteUpperTo(d16, s4);
  auto s5_low_16 = hn::PromoteLowerTo(d16, s5);
  auto s5_high_16 = hn::PromoteUpperTo(d16, s5);

  auto sum_low = hn::WidenMulPairwiseAdd(d32, s0_low_16, coeff0);
  sum_low = hn::SatWidenMulPairwiseAccumulate(d32, s1_low_16, coeff1, sum_low);
  sum_low = hn::SatWidenMulPairwiseAccumulate(d32, s2_low_16, coeff2, sum_low);
  sum_low = hn::SatWidenMulPairwiseAccumulate(d32, s3_low_16, coeff3, sum_low);
  sum_low = hn::SatWidenMulPairwiseAccumulate(d32, s4_low_16, coeff4, sum_low);
  sum_low = hn::SatWidenMulPairwiseAccumulate(d32, s5_low_16, coeff5, sum_low);

  auto sum_high = hn::WidenMulPairwiseAdd(d32, s0_high_16, coeff0);
  sum_high =
      hn::SatWidenMulPairwiseAccumulate(d32, s1_high_16, coeff1, sum_high);
  sum_high =
      hn::SatWidenMulPairwiseAccumulate(d32, s2_high_16, coeff2, sum_high);
  sum_high =
      hn::SatWidenMulPairwiseAccumulate(d32, s3_high_16, coeff3, sum_high);
  sum_high =
      hn::SatWidenMulPairwiseAccumulate(d32, s4_high_16, coeff4, sum_high);
  sum_high =
      hn::SatWidenMulPairwiseAccumulate(d32, s5_high_16, coeff5, sum_high);

  auto shifted_low = hn::ShiftRight<1>(sum_low);
  auto shifted_high = hn::ShiftRight<1>(sum_high);
  auto res_low_16 = hn::DemoteTo(d16_half, shifted_low);
  auto res_high_16 = hn::DemoteTo(d16_half, shifted_high);

  return hn::Combine(d16, res_high_16, res_low_16);
}

template <class D16, class V8, class V8_mask, class V16_coeff>
HWY_ATTR HWY_INLINE hn::VFromD<D16> ConvolveLowbdX12Tap(
    D16 d16, V8 data1, V8 data2, V8_mask mask1, V8_mask mask2, V8_mask mask3,
    V16_coeff coeff0, V16_coeff coeff1, V16_coeff coeff2, V16_coeff coeff3,
    V16_coeff coeff4, V16_coeff coeff5) {
  auto s0 = hn::TableLookupBytes(data1, mask1);
  auto s1 = hn::TableLookupBytes(data1, mask2);
  auto s2 = hn::TableLookupBytes(data1, mask3);
  auto s3 = hn::TableLookupBytes(data2, mask1);
  auto s4 = hn::TableLookupBytes(data2, mask2);
  auto s5 = hn::TableLookupBytes(data2, mask3);
  return ConvolveLowbdX12TapFromS(d16, s0, s1, s2, s3, s4, s5, coeff0, coeff1,
                                  coeff2, coeff3, coeff4, coeff5);
}

template <class D16, class V8, class V16_coeff>
HWY_ATTR HWY_INLINE hn::VFromD<D16> ConvolveLowbdX12TapShuffleFree(
    D16 d16, V8 data1, V8 data2, V16_coeff coeff0, V16_coeff coeff1,
    V16_coeff coeff2, V16_coeff coeff3, V16_coeff coeff4, V16_coeff coeff5) {
  // We only support AVX2 and AVX512, which have > 8 lanes of int16_t.
  static_assert(hn::MaxLanes(D16()) > 8, "Only AVX2 and AVX512 are supported");

  using D8_twice = hn::DFromV<V8>;
  const D8_twice d8_twice;

  auto mask1 = hn::LoadDup128(d8_twice, kFilt1);
  auto mask2 = hn::LoadDup128(d8_twice, kFilt2);
  auto mask3 = hn::LoadDup128(d8_twice, kFilt3);
  return ConvolveLowbdX12Tap(d16, data1, data2, mask1, mask2, mask3, coeff0,
                             coeff1, coeff2, coeff3, coeff4, coeff5);
}

// Vertical convolve helper
template <int taps_y, class D32, class V16, class V16_coeff>
HWY_ATTR HWY_INLINE hn::VFromD<D32> ConvolveVertical(
    D32 d32, const V16 *z_arr, const V16_coeff *coeffs_v,
    hn::VFromD<D32> round_const_y) {
  constexpr int num_coeffs = taps_y / 2;
  auto sum = hn::SatWidenMulPairwiseAccumulate(d32, z_arr[0], coeffs_v[0],
                                               round_const_y);
  for (int k = 1; k < num_coeffs; ++k) {
    sum = hn::SatWidenMulPairwiseAccumulate(d32, z_arr[k], coeffs_v[k], sum);
  }
  return sum;
}

template <class DI8>
HWY_ATTR HWY_INLINE void PrepareCoeffsH(DI8 di8, const int16_t *filter,
                                        int taps, hn::VFromD<DI8> *coeffs) {
  int start_idx = (taps == 12) ? 0 : (4 - taps / 2);
  int num_coeffs = taps / 2;
  for (int k = 0; k < num_coeffs; ++k) {
    auto c0 = static_cast<int8_t>(filter[start_idx + k * 2] >> 1);
    auto c1 = static_cast<int8_t>(filter[start_idx + k * 2 + 1] >> 1);
    HWY_ALIGN int8_t coeff_arr[16] = { c0, c1, c0, c1, c0, c1, c0, c1,
                                       c0, c1, c0, c1, c0, c1, c0, c1 };
    coeffs[k] = hn::LoadDup128(di8, coeff_arr);
  }
}

template <class D16>
HWY_ATTR HWY_INLINE void PrepareCoeffsV(D16 d16, const int16_t *filter,
                                        int taps, hn::VFromD<D16> *coeffs) {
  int start_idx = (taps == 12) ? 0 : (4 - taps / 2);
  int num_coeffs = taps / 2;
  for (int k = 0; k < num_coeffs; ++k) {
    int16_t c0 = filter[start_idx + k * 2];
    int16_t c1 = filter[start_idx + k * 2 + 1];
    HWY_ALIGN int16_t coeff_arr[8] = { c0, c1, c0, c1, c0, c1, c0, c1 };
    coeffs[k] = hn::LoadDup128(d16, coeff_arr);
  }
}

template <class D16>
HWY_ATTR HWY_INLINE void PrepareCoeffs12(D16 d16, const int16_t *filter,
                                         hn::VFromD<D16> *coeffs) {
  for (int k = 0; k < 6; ++k) {
    int16_t c0 = filter[k * 2];
    int16_t c1 = filter[k * 2 + 1];
    HWY_ALIGN int16_t coeff_arr[8] = { c0, c1, c0, c1, c0, c1, c0, c1 };
    coeffs[k] = hn::LoadDup128(d16, coeff_arr);
  }
}

template <class D, class V128>
HWY_ATTR HWY_INLINE hn::VFromD<D> Combine4(D d, V128 r0, V128 r1, V128 r2,
                                           V128 r3) {
  using DHalf = hn::Half<D>;
  const DHalf d_half;
  using DQuarter = hn::Half<DHalf>;

  auto r01 = hn::Combine(d_half, hn::BitCast(DQuarter(), r1),
                         hn::BitCast(DQuarter(), r0));
  auto r23 = hn::Combine(d_half, hn::BitCast(DQuarter(), r3),
                         hn::BitCast(DQuarter(), r2));
  return hn::Combine(d, r23, r01);
}

template <class D, class V128>
HWY_ATTR HWY_INLINE hn::VFromD<D> Combine2(D d, V128 r0, V128 r1) {
  using DHalf = hn::Half<D>;
  return hn::Combine(d, hn::BitCast(DHalf(), r1), hn::BitCast(DHalf(), r0));
}

template <int num_rows, class D, class D128>
HWY_ATTR HWY_INLINE hn::VFromD<D> LoadAndCombine(D d, D128 d128,
                                                 const uint8_t *src,
                                                 int src_stride) {
  HWY_IF_CONSTEXPR(num_rows == 4) {
    auto r0 = hn::LoadU(d128, src + 0 * src_stride);
    auto r1 = hn::LoadU(d128, src + 1 * src_stride);
    auto r2 = hn::LoadU(d128, src + 2 * src_stride);
    auto r3 = hn::LoadU(d128, src + 3 * src_stride);
    return Combine4(d, r0, r1, r2, r3);
  }
  HWY_IF_CONSTEXPR(num_rows == 2) {
    auto r0 = hn::LoadU(d128, src + 0 * src_stride);
    auto r1 = hn::LoadU(d128, src + 1 * src_stride);
    return Combine2(d, r0, r1);
  }
  return hn::ResizeBitCast(d, hn::LoadU(d128, src));
}

template <int num_rows, class D, class D128>
HWY_ATTR HWY_INLINE hn::VFromD<D> LoadAndCombineClamped(D d, D128 d128,
                                                        const uint8_t *src,
                                                        int src_stride,
                                                        int row_idx,
                                                        int max_rows) {
  HWY_IF_CONSTEXPR(num_rows == 4) {
    int r0 = row_idx;
    int r1 = std::min(row_idx + 1, max_rows - 1);
    int r2 = std::min(row_idx + 2, max_rows - 1);
    int r3 = std::min(row_idx + 3, max_rows - 1);

    auto v0 = hn::LoadU(d128, src + r0 * src_stride);
    auto v1 = hn::LoadU(d128, src + r1 * src_stride);
    auto v2 = hn::LoadU(d128, src + r2 * src_stride);
    auto v3 = hn::LoadU(d128, src + r3 * src_stride);
    return Combine4(d, v0, v1, v2, v3);
  }
  HWY_IF_CONSTEXPR(num_rows == 2) {
    int r0 = row_idx;
    int r1 = std::min(row_idx + 1, max_rows - 1);

    auto v0 = hn::LoadU(d128, src + r0 * src_stride);
    auto v1 = hn::LoadU(d128, src + r1 * src_stride);
    return Combine2(d, v0, v1);
  }
  return hn::ResizeBitCast(d, hn::LoadU(d128, src + row_idx * src_stride));
}

/// Helper function for the vertical convolve pass.
// Template parameters:
// - `taps_y_val`: Number of vertical filter taps (2, 4, 6, 8, or 12).
// - `h_ge_8`: Compile-time flag indicating if block height is >= 8. Allows the
//   compiler to fully unroll the output loop (limit is 4 if true, 2 if false).
// - `skip_strip_im`: Compile-time flag indicating if we should skip loading
//   from `strip_im_buf` and instead compute horizontal convolve on the fly.
// - `round_0_const`, `round_1_const`: Compile-time rounding offsets, enabling
//   optimized shift calculations when non-zero.
// - `IdxTbl`: Target-specific Highway type for table lookup indices.
// - `ComputeHRowBlock`: Callable type for computing horizontal convolve on the
//   fly (used when `skip_strip_im` is true).
template <int taps_y_val, bool h_ge_8, bool skip_strip_im, int round_0_const,
          int round_1_const, class IdxTbl,
          class ComputeHRowBlock = IVec16 (*)(int)>
HWY_ATTR HWY_INLINE void ConvolveVerticalPass(
    const int16_t *strip_im_buf, uint8_t *dst, int dst_stride, int w, int h,
    const IVec16 *coeffs_v, IVec32 round_const_y, int round_1, int bits,
    IdxTbl idx_tbl, ComputeHRowBlock compute_h_row_block = nullptr) {
  constexpr int num_coeffs = taps_y_val / 2;
  constexpr int num_z = (h_ge_8 ? 8 : 4) / 2 + num_coeffs - 1;
  constexpr int num_h = num_z / 2 + 1;

  hn::Half<decltype(int16xN_tag)> d16_16;
  const auto zero_32 = hn::Zero(int32xN_tag);

  auto round_and_store = [&](IVec32 sum, int y) {
    auto res_32_A = hn::ShiftRightSame(sum, round_1);
    HWY_IF_CONSTEXPR(round_0_const > 0 && round_1_const > 0) {
      constexpr int bits_const =
          FILTER_BITS * 2 - round_0_const - round_1_const;
      HWY_IF_CONSTEXPR(bits_const > 0) {
        res_32_A = hn::ShiftRightSame(res_32_A, bits_const);
      }
    }
    else {
      if (bits > 0) {
        res_32_A = hn::ShiftRightSame(res_32_A, bits);
      }
    }

    auto max_32 = hn::Max(res_32_A, zero_32);
    auto res_8_A =
        hn::DemoteTo(uint8x16_capped_tag, hn::BitCast(uint32xN_tag, max_32));

    if (w == 4) {
      uint8_t tmpA[16];
      hn::StoreU(hn::LowerHalf(uint8x8_capped_tag, res_8_A), uint8x8_capped_tag,
                 tmpA);
      hn::StoreU(hn::UpperHalf(uint8x8_capped_tag, res_8_A), uint8x8_capped_tag,
                 tmpA + 8);
      __builtin_memcpy(dst + (y + 0) * dst_stride, tmpA, 4);
      __builtin_memcpy(dst + (y + 1) * dst_stride, tmpA + 8, 4);
    } else {
      hn::StoreU(hn::LowerHalf(uint8x8_capped_tag, res_8_A), uint8x8_capped_tag,
                 dst + (y + 0) * dst_stride);
      hn::StoreU(hn::UpperHalf(uint8x8_capped_tag, res_8_A), uint8x8_capped_tag,
                 dst + (y + 1) * dst_stride);
    }
  };

  for (int y = 0; y < h; y += 8) {
    IVec16 H[num_h];
    for (int k = 0; k < num_h; ++k) {
      HWY_IF_CONSTEXPR(skip_strip_im) { H[k] = compute_h_row_block(y + k * 4); }
      else {
        H[k] = hn::LoadU(int16xN_tag, strip_im_buf + (y + k * 4) * 8);
      }
    }

    IVec16 Z[num_z];
    for (int z_idx = 0; z_idx < num_z; ++z_idx) {
      IVec16 L;
      if (z_idx % 2 == 0) {
        L = H[z_idx / 2];
      } else {
        L = hn::Combine(int16xN_tag, hn::LowerHalf(d16_16, H[z_idx / 2 + 1]),
                        hn::UpperHalf(d16_16, H[z_idx / 2]));
      }
      Z[z_idx] = hn::TableLookupLanes(L, idx_tbl);
    }

    constexpr int limit = h_ge_8 ? 4 : 2;
    for (int i = 0; i < limit; ++i) {
      auto sum = ConvolveVertical<taps_y_val>(int32xN_tag, &Z[i], coeffs_v,
                                              round_const_y);
      round_and_store(sum, y + 2 * i);
    }
  }
}

template <int taps_x_const = 0, int taps_y_const = 0, int round_0_const = 0,
          int round_1_const = 0>
HWY_ATTR HWY_INLINE void Convolve2DSRHwyImpl(
    const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w,
    int h, const InterpFilterParams *filter_params_x,
    const InterpFilterParams *filter_params_y, const int subpel_x_qn,
    const int subpel_y_qn, ConvolveParams *conv_params) {
  const int taps_x = taps_x_const > 0 ? taps_x_const : filter_params_x->taps;
  const int taps_y = taps_y_const > 0 ? taps_y_const : filter_params_y->taps;
  const int round_0 = round_0_const > 0 ? round_0_const : conv_params->round_0;
  const int round_1 = round_1_const > 0 ? round_1_const : conv_params->round_1;

  const bool is_taps_x_12 =
      (taps_x_const == 12) || (taps_x_const == 0 && taps_x == 12);
  const bool is_taps_y_12 =
      (taps_y_const == 12) || (taps_y_const == 0 && taps_y == 12);

  const int fo_vert = taps_y / 2 - 1;
  const int fo_horiz = taps_x / 2 - 1;

  constexpr int kNumRows = 4;
  HWY_ALIGN_MAX int16_t strip_im_buf[(MAX_SB_SIZE + MAX_FILTER_TAP + 8) * 8];

  const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
      filter_params_x, subpel_x_qn & SUBPEL_MASK);
  const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(
      filter_params_y, subpel_y_qn & SUBPEL_MASK);

  IVec8 coeffs_h[6] = { hn::Zero(int8xN_tag) };
  IVec16 coeffs_h_12[6];
  if (is_taps_x_12) {
    PrepareCoeffsV(int16xN_tag, x_filter, taps_x, coeffs_h_12);
  } else {
    PrepareCoeffsH(int8xN_tag, x_filter, taps_x, coeffs_h);
  }

  UVec8 mask1, mask2, mask3, mask4;
  HWY_IF_CONSTEXPR(taps_x_const != 8 && taps_x_const != 12) {
    mask1 = hn::LoadDup128(uint8xN_tag, kFilt1);
    mask2 = hn::LoadDup128(uint8xN_tag, kFilt2);
    mask3 = hn::LoadDup128(uint8xN_tag, kFilt3);
    mask4 = hn::LoadDup128(uint8xN_tag, kFilt4);
  }

  const auto round_const_h = hn::Set(int16xN_tag, 1 << (round_0 - 2));

  const int im_h = h + taps_y - 1;
  const uint8_t *const src_ptr = src - fo_vert * src_stride - fo_horiz;

  auto convolve_horizontal = [&](auto data_vec) {
    IVec16 res;
    HWY_IF_CONSTEXPR(taps_x_const == 2 || taps_x_const == 4 ||
                     taps_x_const == 6 || taps_x_const == 8) {
      res = ConvolveLowbdXShuffleFree<taps_x_const>(int16xN_tag, data_vec,
                                                    coeffs_h);
    }
    else HWY_IF_CONSTEXPR(taps_x_const == 0) {
      if (taps_x == 2) {
        res = ConvolveLowbdX<2>(int16xN_tag, data_vec, &mask1, coeffs_h);
      } else if (taps_x == 4) {
        const UVec8 masks[] = { mask1, mask2 };
        res = ConvolveLowbdX<4>(int16xN_tag, data_vec, masks, coeffs_h);
      } else if (taps_x == 6) {
        const UVec8 masks[] = { mask1, mask2, mask3 };
        res = ConvolveLowbdX<6>(int16xN_tag, data_vec, masks, coeffs_h);
      } else {
        const UVec8 masks[] = { mask1, mask2, mask3, mask4 };
        res = ConvolveLowbdX<8>(int16xN_tag, data_vec, masks, coeffs_h);
      }
    }
    return res;
  };

  for (int j = 0; j < w; j += 8) {
    const bool skip_strip_im_buf =
        (is_taps_y_12 || is_taps_x_12) ? false : (h == 4 || h == 8);

    if (!skip_strip_im_buf) {
      int i = 0;
      for (; i < (im_h & ~(kNumRows - 1)); i += kNumRows) {
        IVec16 res;
        if (is_taps_x_12) {
          auto data1 = LoadAndCombine<kNumRows>(
              uint8xN_tag, uint8x16_capped_tag,
              src_ptr + i * src_stride + j + 0, src_stride);
          auto data2 = LoadAndCombine<kNumRows>(
              uint8xN_tag, uint8x16_capped_tag,
              src_ptr + i * src_stride + j + 6, src_stride);
          HWY_IF_CONSTEXPR(taps_x_const == 12) {
            res = ConvolveLowbdX12TapShuffleFree(
                int16xN_tag, data1, data2, coeffs_h_12[0], coeffs_h_12[1],
                coeffs_h_12[2], coeffs_h_12[3], coeffs_h_12[4], coeffs_h_12[5]);
          }
          else {
            res = ConvolveLowbdX12Tap(int16xN_tag, data1, data2, mask1, mask2,
                                      mask3, coeffs_h_12[0], coeffs_h_12[1],
                                      coeffs_h_12[2], coeffs_h_12[3],
                                      coeffs_h_12[4], coeffs_h_12[5]);
          }
        } else {
          auto data = LoadAndCombine<kNumRows>(uint8xN_tag, uint8x16_capped_tag,
                                               src_ptr + i * src_stride + j,
                                               src_stride);
          res = convolve_horizontal(data);
        }

        auto shifted_res =
            hn::ShiftRightSame(hn::Add(res, round_const_h), round_0 - 1);
        hn::StoreU(shifted_res, int16xN_tag, strip_im_buf + i * 8);
      }
      for (; i < im_h; i += kNumRows) {
        IVec16 res;
        if (is_taps_x_12) {
          auto data1 = LoadAndCombineClamped<kNumRows>(
              uint8xN_tag, uint8x16_capped_tag, src_ptr + j + 0, src_stride, i,
              im_h);
          auto data2 = LoadAndCombineClamped<kNumRows>(
              uint8xN_tag, uint8x16_capped_tag, src_ptr + j + 6, src_stride, i,
              im_h);
          HWY_IF_CONSTEXPR(taps_x_const == 12) {
            res = ConvolveLowbdX12TapShuffleFree(
                int16xN_tag, data1, data2, coeffs_h_12[0], coeffs_h_12[1],
                coeffs_h_12[2], coeffs_h_12[3], coeffs_h_12[4], coeffs_h_12[5]);
          }
          else {
            res = ConvolveLowbdX12Tap(int16xN_tag, data1, data2, mask1, mask2,
                                      mask3, coeffs_h_12[0], coeffs_h_12[1],
                                      coeffs_h_12[2], coeffs_h_12[3],
                                      coeffs_h_12[4], coeffs_h_12[5]);
          }
        } else {
          auto data =
              LoadAndCombineClamped<kNumRows>(uint8xN_tag, uint8x16_capped_tag,
                                              src_ptr + j, src_stride, i, im_h);
          res = convolve_horizontal(data);
        }

        auto shifted_res =
            hn::ShiftRightSame(hn::Add(res, round_const_h), round_0 - 1);
        hn::StoreU(shifted_res, int16xN_tag, strip_im_buf + i * 8);
      }
    }

    const int bits = FILTER_BITS * 2 - round_0 - round_1;

    IVec16 coeffs_v[6];
    HWY_IF_CONSTEXPR(taps_y_const > 0) {
      PrepareCoeffsV(int16xN_tag, y_filter, taps_y_const, coeffs_v);
    }
    else {
      PrepareCoeffsV(int16xN_tag, y_filter, taps_y, coeffs_v);
    }

    const int round_const_y_val =
        ((1 << round_1) >> 1) + ((bits > 0) ? (1 << (bits - 1 + round_1)) : 0);
    const auto round_const_y = hn::Set(int32xN_tag, round_const_y_val);

    HWY_ALIGN_MAX static constexpr int16_t idx_arr[32] = {
      0, 8,  1, 9,  2,  10, 3,  11, 4,  12, 5,  13, 6,  14, 7,  15,
      8, 16, 9, 17, 10, 18, 11, 19, 12, 20, 13, 21, 14, 22, 15, 23
    };
    auto idx_tbl = hn::SetTableIndices(int16xN_tag, idx_arr);

    auto compute_h_row_block = [&](int row_idx) {
      auto data =
          LoadAndCombineClamped<4>(uint8xN_tag, uint8x16_capped_tag,
                                   src_ptr + j, src_stride, row_idx, im_h);
      auto res = convolve_horizontal(data);
      return hn::ShiftRightSame(hn::Add(res, round_const_h), round_0 - 1);
    };

    if (h == 4 && !is_taps_x_12) {
      HWY_IF_CONSTEXPR(taps_y_const > 0) {
        constexpr bool skip_strip_im = taps_y_const == 12 ? false : true;
        ConvolveVerticalPass<taps_y_const, false, skip_strip_im, round_0_const,
                             round_1_const>(
            strip_im_buf, dst + j, dst_stride, w, h, coeffs_v, round_const_y,
            round_1, bits, idx_tbl, compute_h_row_block);
      }
      else {
        if (taps_y == 2) {
          ConvolveVerticalPass<2, false, true, round_0_const, round_1_const>(
              strip_im_buf, dst + j, dst_stride, w, h, coeffs_v, round_const_y,
              round_1, bits, idx_tbl, compute_h_row_block);
        } else if (taps_y == 4) {
          ConvolveVerticalPass<4, false, true, round_0_const, round_1_const>(
              strip_im_buf, dst + j, dst_stride, w, h, coeffs_v, round_const_y,
              round_1, bits, idx_tbl, compute_h_row_block);
        } else if (taps_y == 6) {
          ConvolveVerticalPass<6, false, true, round_0_const, round_1_const>(
              strip_im_buf, dst + j, dst_stride, w, h, coeffs_v, round_const_y,
              round_1, bits, idx_tbl, compute_h_row_block);
        } else if (taps_y == 8) {
          ConvolveVerticalPass<8, false, true, round_0_const, round_1_const>(
              strip_im_buf, dst + j, dst_stride, w, h, coeffs_v, round_const_y,
              round_1, bits, idx_tbl, compute_h_row_block);
        }
      }
    } else if (h == 8 && !is_taps_x_12) {
      HWY_IF_CONSTEXPR(taps_y_const > 0) {
        constexpr bool skip_strip_im = taps_y_const == 12 ? false : true;
        ConvolveVerticalPass<taps_y_const, true, skip_strip_im, round_0_const,
                             round_1_const>(
            strip_im_buf, dst + j, dst_stride, w, h, coeffs_v, round_const_y,
            round_1, bits, idx_tbl, compute_h_row_block);
      }
      else {
        if (taps_y == 2) {
          ConvolveVerticalPass<2, true, true, round_0_const, round_1_const>(
              strip_im_buf, dst + j, dst_stride, w, h, coeffs_v, round_const_y,
              round_1, bits, idx_tbl, compute_h_row_block);
        } else if (taps_y == 4) {
          ConvolveVerticalPass<4, true, true, round_0_const, round_1_const>(
              strip_im_buf, dst + j, dst_stride, w, h, coeffs_v, round_const_y,
              round_1, bits, idx_tbl, compute_h_row_block);
        } else if (taps_y == 6) {
          ConvolveVerticalPass<6, true, true, round_0_const, round_1_const>(
              strip_im_buf, dst + j, dst_stride, w, h, coeffs_v, round_const_y,
              round_1, bits, idx_tbl, compute_h_row_block);
        } else if (taps_y == 8) {
          ConvolveVerticalPass<8, true, true, round_0_const, round_1_const>(
              strip_im_buf, dst + j, dst_stride, w, h, coeffs_v, round_const_y,
              round_1, bits, idx_tbl, compute_h_row_block);
        }
      }
    } else {
      if (h >= 8) {
        if (is_taps_y_12) {
          ConvolveVerticalPass<12, true, false, round_0_const, round_1_const>(
              strip_im_buf, dst + j, dst_stride, w, h, coeffs_v, round_const_y,
              round_1, bits, idx_tbl);
        } else {
          HWY_IF_CONSTEXPR(taps_y_const > 0) {
            ConvolveVerticalPass<taps_y_const, true, false, round_0_const,
                                 round_1_const>(
                strip_im_buf, dst + j, dst_stride, w, h, coeffs_v,
                round_const_y, round_1, bits, idx_tbl);
          }
          else {
            if (taps_y == 2) {
              ConvolveVerticalPass<2, true, false, round_0_const,
                                   round_1_const>(
                  strip_im_buf, dst + j, dst_stride, w, h, coeffs_v,
                  round_const_y, round_1, bits, idx_tbl);
            } else if (taps_y == 4) {
              ConvolveVerticalPass<4, true, false, round_0_const,
                                   round_1_const>(
                  strip_im_buf, dst + j, dst_stride, w, h, coeffs_v,
                  round_const_y, round_1, bits, idx_tbl);
            } else if (taps_y == 6) {
              ConvolveVerticalPass<6, true, false, round_0_const,
                                   round_1_const>(
                  strip_im_buf, dst + j, dst_stride, w, h, coeffs_v,
                  round_const_y, round_1, bits, idx_tbl);
            } else if (taps_y == 8) {
              ConvolveVerticalPass<8, true, false, round_0_const,
                                   round_1_const>(
                  strip_im_buf, dst + j, dst_stride, w, h, coeffs_v,
                  round_const_y, round_1, bits, idx_tbl);
            }
          }
        }
      } else {
        if (is_taps_y_12) {
          ConvolveVerticalPass<12, false, false, round_0_const, round_1_const>(
              strip_im_buf, dst + j, dst_stride, w, h, coeffs_v, round_const_y,
              round_1, bits, idx_tbl);
        } else {
          HWY_IF_CONSTEXPR(taps_y_const > 0) {
            ConvolveVerticalPass<taps_y_const, false, false, round_0_const,
                                 round_1_const>(
                strip_im_buf, dst + j, dst_stride, w, h, coeffs_v,
                round_const_y, round_1, bits, idx_tbl);
          }
          else {
            if (taps_y == 2) {
              ConvolveVerticalPass<2, false, false, round_0_const,
                                   round_1_const>(
                  strip_im_buf, dst + j, dst_stride, w, h, coeffs_v,
                  round_const_y, round_1, bits, idx_tbl);
            } else if (taps_y == 4) {
              ConvolveVerticalPass<4, false, false, round_0_const,
                                   round_1_const>(
                  strip_im_buf, dst + j, dst_stride, w, h, coeffs_v,
                  round_const_y, round_1, bits, idx_tbl);
            } else if (taps_y == 6) {
              ConvolveVerticalPass<6, false, false, round_0_const,
                                   round_1_const>(
                  strip_im_buf, dst + j, dst_stride, w, h, coeffs_v,
                  round_const_y, round_1, bits, idx_tbl);
            } else if (taps_y == 8) {
              ConvolveVerticalPass<8, false, false, round_0_const,
                                   round_1_const>(
                  strip_im_buf, dst + j, dst_stride, w, h, coeffs_v,
                  round_const_y, round_1, bits, idx_tbl);
            }
          }
        }
      }
    }
  }
}

template <int tx, int ty, int r0, int r1>
HWY_ATTR void Convolve2DSRRun(const uint8_t *src, int src_stride, uint8_t *dst,
                              int dst_stride, int w, int h,
                              const InterpFilterParams *filter_params_x,
                              const InterpFilterParams *filter_params_y,
                              const int subpel_x_qn, const int subpel_y_qn,
                              ConvolveParams *conv_params) {
  Convolve2DSRHwyImpl<tx, ty, r0, r1>(src, src_stride, dst, dst_stride, w, h,
                                      filter_params_x, filter_params_y,
                                      subpel_x_qn, subpel_y_qn, conv_params);
}

HWY_ATTR inline void Convolve2DSR(const uint8_t *src, int src_stride,
                                  uint8_t *dst, int dst_stride, int w, int h,
                                  const InterpFilterParams *filter_params_x,
                                  const InterpFilterParams *filter_params_y,
                                  const int subpel_x_qn, const int subpel_y_qn,
                                  ConvolveParams *conv_params) {
  const int round_0 = conv_params->round_0;
  const int round_1 = conv_params->round_1;

  if (round_0 == 3 && round_1 == 11) {
    const int tap_x = get_filter_tap(filter_params_x, subpel_x_qn);
    const int tap_y = get_filter_tap(filter_params_y, subpel_y_qn);
    switch (tap_x) {
      case 2:
        switch (tap_y) {
          case 2:
            Convolve2DSRRun<2, 2, 3, 11>(src, src_stride, dst, dst_stride, w, h,
                                         filter_params_x, filter_params_y,
                                         subpel_x_qn, subpel_y_qn, conv_params);
            return;
          case 4:
            Convolve2DSRRun<2, 4, 3, 11>(src, src_stride, dst, dst_stride, w, h,
                                         filter_params_x, filter_params_y,
                                         subpel_x_qn, subpel_y_qn, conv_params);
            return;
          case 6:
            Convolve2DSRRun<2, 6, 3, 11>(src, src_stride, dst, dst_stride, w, h,
                                         filter_params_x, filter_params_y,
                                         subpel_x_qn, subpel_y_qn, conv_params);
            return;
          case 8:
            Convolve2DSRRun<2, 8, 3, 11>(src, src_stride, dst, dst_stride, w, h,
                                         filter_params_x, filter_params_y,
                                         subpel_x_qn, subpel_y_qn, conv_params);
            return;
          case 12:
            Convolve2DSRRun<2, 12, 3, 11>(
                src, src_stride, dst, dst_stride, w, h, filter_params_x,
                filter_params_y, subpel_x_qn, subpel_y_qn, conv_params);
            return;
        }
        break;
      case 4:
        switch (tap_y) {
          case 2:
            Convolve2DSRRun<4, 2, 3, 11>(src, src_stride, dst, dst_stride, w, h,
                                         filter_params_x, filter_params_y,
                                         subpel_x_qn, subpel_y_qn, conv_params);
            return;
          case 4:
            Convolve2DSRRun<4, 4, 3, 11>(src, src_stride, dst, dst_stride, w, h,
                                         filter_params_x, filter_params_y,
                                         subpel_x_qn, subpel_y_qn, conv_params);
            return;
          case 6:
            Convolve2DSRRun<4, 6, 3, 11>(src, src_stride, dst, dst_stride, w, h,
                                         filter_params_x, filter_params_y,
                                         subpel_x_qn, subpel_y_qn, conv_params);
            return;
          case 8:
            Convolve2DSRRun<4, 8, 3, 11>(src, src_stride, dst, dst_stride, w, h,
                                         filter_params_x, filter_params_y,
                                         subpel_x_qn, subpel_y_qn, conv_params);
            return;
          case 12:
            Convolve2DSRRun<4, 12, 3, 11>(
                src, src_stride, dst, dst_stride, w, h, filter_params_x,
                filter_params_y, subpel_x_qn, subpel_y_qn, conv_params);
            return;
        }
        break;
      case 6:
        switch (tap_y) {
          case 2:
            Convolve2DSRRun<6, 2, 3, 11>(src, src_stride, dst, dst_stride, w, h,
                                         filter_params_x, filter_params_y,
                                         subpel_x_qn, subpel_y_qn, conv_params);
            return;
          case 4:
            Convolve2DSRRun<6, 4, 3, 11>(src, src_stride, dst, dst_stride, w, h,
                                         filter_params_x, filter_params_y,
                                         subpel_x_qn, subpel_y_qn, conv_params);
            return;
          case 6:
            Convolve2DSRRun<6, 6, 3, 11>(src, src_stride, dst, dst_stride, w, h,
                                         filter_params_x, filter_params_y,
                                         subpel_x_qn, subpel_y_qn, conv_params);
            return;
          case 8:
            Convolve2DSRRun<6, 8, 3, 11>(src, src_stride, dst, dst_stride, w, h,
                                         filter_params_x, filter_params_y,
                                         subpel_x_qn, subpel_y_qn, conv_params);
            return;
          case 12:
            Convolve2DSRRun<6, 12, 3, 11>(
                src, src_stride, dst, dst_stride, w, h, filter_params_x,
                filter_params_y, subpel_x_qn, subpel_y_qn, conv_params);
            return;
        }
        break;
      case 8:
        switch (tap_y) {
          case 2:
            Convolve2DSRRun<8, 2, 3, 11>(src, src_stride, dst, dst_stride, w, h,
                                         filter_params_x, filter_params_y,
                                         subpel_x_qn, subpel_y_qn, conv_params);
            return;
          case 4:
            Convolve2DSRRun<8, 4, 3, 11>(src, src_stride, dst, dst_stride, w, h,
                                         filter_params_x, filter_params_y,
                                         subpel_x_qn, subpel_y_qn, conv_params);
            return;
          case 6:
            Convolve2DSRRun<8, 6, 3, 11>(src, src_stride, dst, dst_stride, w, h,
                                         filter_params_x, filter_params_y,
                                         subpel_x_qn, subpel_y_qn, conv_params);
            return;
          case 8:
            Convolve2DSRRun<8, 8, 3, 11>(src, src_stride, dst, dst_stride, w, h,
                                         filter_params_x, filter_params_y,
                                         subpel_x_qn, subpel_y_qn, conv_params);
            return;
          case 12:
            Convolve2DSRRun<8, 12, 3, 11>(
                src, src_stride, dst, dst_stride, w, h, filter_params_x,
                filter_params_y, subpel_x_qn, subpel_y_qn, conv_params);
            return;
        }
        break;
      case 12:
        switch (tap_y) {
          case 2:
            Convolve2DSRRun<12, 2, 3, 11>(
                src, src_stride, dst, dst_stride, w, h, filter_params_x,
                filter_params_y, subpel_x_qn, subpel_y_qn, conv_params);
            return;
          case 4:
            Convolve2DSRRun<12, 4, 3, 11>(
                src, src_stride, dst, dst_stride, w, h, filter_params_x,
                filter_params_y, subpel_x_qn, subpel_y_qn, conv_params);
            return;
          case 6:
            Convolve2DSRRun<12, 6, 3, 11>(
                src, src_stride, dst, dst_stride, w, h, filter_params_x,
                filter_params_y, subpel_x_qn, subpel_y_qn, conv_params);
            return;
          case 8:
            Convolve2DSRRun<12, 8, 3, 11>(
                src, src_stride, dst, dst_stride, w, h, filter_params_x,
                filter_params_y, subpel_x_qn, subpel_y_qn, conv_params);
            return;
          case 12:
            Convolve2DSRRun<12, 12, 3, 11>(
                src, src_stride, dst, dst_stride, w, h, filter_params_x,
                filter_params_y, subpel_x_qn, subpel_y_qn, conv_params);
            return;
        }
        break;
    }
  }

  Convolve2DSRRun<0, 0, 0, 0>(src, src_stride, dst, dst_stride, w, h,
                              filter_params_x, filter_params_y, subpel_x_qn,
                              subpel_y_qn, conv_params);
}

}  // namespace HWY_NAMESPACE
}  // namespace

HWY_AFTER_NAMESPACE();

#define MAKE_CONVOLVE_2D_SR(suffix)                                            \
  extern "C" void av1_convolve_2d_sr_##suffix(                                 \
      const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, \
      int h, const InterpFilterParams *filter_params_x,                        \
      const InterpFilterParams *filter_params_y, const int subpel_x_qn,        \
      const int subpel_y_qn, ConvolveParams *conv_params);                     \
  HWY_ATTR void av1_convolve_2d_sr_##suffix(                                   \
      const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, \
      int h, const InterpFilterParams *filter_params_x,                        \
      const InterpFilterParams *filter_params_y, const int subpel_x_qn,        \
      const int subpel_y_qn, ConvolveParams *conv_params) {                    \
    if (w < 4) {                                                               \
      av1_convolve_2d_sr_avx2(src, src_stride, dst, dst_stride, w, h,          \
                              filter_params_x, filter_params_y, subpel_x_qn,   \
                              subpel_y_qn, conv_params);                       \
      return;                                                                  \
    }                                                                          \
    HWY_NAMESPACE::Convolve2DSR(src, src_stride, dst, dst_stride, w, h,        \
                                filter_params_x, filter_params_y, subpel_x_qn, \
                                subpel_y_qn, conv_params);                     \
  }

#endif  // AOM_AV1_COMMON_CONVOLVE_2D_SR_HWY_H_
