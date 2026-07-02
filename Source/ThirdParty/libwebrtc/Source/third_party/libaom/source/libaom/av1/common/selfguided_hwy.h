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

#ifndef AV1_COMMON_SELFGUIDED_HWY_H_
#define AV1_COMMON_SELFGUIDED_HWY_H_

#include "av1/common/restoration.h"
#include "config/aom_config.h"
#include "config/av1_rtcd.h"
#include "third_party/highway/hwy/highway.h"

HWY_BEFORE_NAMESPACE();

namespace {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

template <int NumBlocks>
struct ScanTraits {};

template <>
struct ScanTraits<1> {
  template <typename D>
  HWY_ATTR HWY_INLINE static hn::VFromD<D> AddBlocks(D int32_tag,
                                                     hn::VFromD<D> v) {
    (void)int32_tag;
    return v;
  }
};

template <>
struct ScanTraits<2> {
  template <typename D>
  HWY_ATTR HWY_INLINE static hn::VFromD<D> AddBlocks(D int32_tag,
                                                     hn::VFromD<D> v) {
    constexpr hn::Half<D> half_tag;
    const int32_t s = hn::ExtractLane(v, 3);
    const auto s01 = hn::Set(half_tag, s);
    const auto s02 = hn::InsertBlock<1>(hn::Zero(int32_tag), s01);
    return hn::Add(v, s02);
  }
};

template <>
struct ScanTraits<4> {
  template <typename D>
  HWY_ATTR HWY_INLINE static hn::VFromD<D> AddBlocks(D int32_tag,
                                                     hn::VFromD<D> v) {
    HWY_ALIGN static const int32_t kA[] = {
      0, 0, 0, 0, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
    };
    HWY_ALIGN static const int32_t kB[] = {
      0, 0, 0, 0, 0, 0, 0, 0, 23, 23, 23, 23, 23, 23, 23, 23,
    };
    HWY_ALIGN static const int32_t kC[] = {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 27, 27, 27, 27,
    };
    const auto a = hn::SetTableIndices(int32_tag, kA);
    const auto b = hn::SetTableIndices(int32_tag, kB);
    const auto c = hn::SetTableIndices(int32_tag, kC);
    const auto s01 =
        hn::TwoTablesLookupLanes(int32_tag, hn::Zero(int32_tag), v, a);
    const auto s02 =
        hn::TwoTablesLookupLanes(int32_tag, hn::Zero(int32_tag), v, b);
    const auto s03 =
        hn::TwoTablesLookupLanes(int32_tag, hn::Zero(int32_tag), v, c);
    v = hn::Add(v, s01);
    v = hn::Add(v, s02);
    v = hn::Add(v, s03);
    return v;
  }
};

// Compute the scan of a register holding 32-bit integers. If the register holds
// x0..x7 then the scan will hold x0, x0+x1, x0+x1+x2, ..., x0+x1+...+x7
//
// For the AVX2 example below, let [...] represent a 128-bit block, and let a,
// ..., h be 32-bit integers (assumed small enough to be able to add them
// without overflow).
//
// Use -> as shorthand for summing, i.e. h->a = h + g + f + e + d + c + b + a.
//
// x   = [h g f e][d c b a]
// x01 = [g f e 0][c b a 0]
// x02 = [g+h f+g e+f e][c+d b+c a+b a]
// x03 = [e+f e 0 0][a+b a 0 0]
// x04 = [e->h e->g e->f e][a->d a->c a->b a]
// s   = a->d
// s01 = [a->d a->d a->d a->d]
// s02 = [a->d a->d a->d a->d][0 0 0 0]
// ret = [a->h a->g a->f a->e][a->d a->c a->b a]
template <typename D>
HWY_ATTR HWY_INLINE hn::VFromD<D> Scan32(D int32_tag, hn::VFromD<D> x) {
  const auto x01 = hn::ShiftLeftBytes<4>(x);
  const auto x02 = hn::Add(x, x01);
  const auto x03 = hn::ShiftLeftBytes<8>(x02);
  const auto x04 = hn::Add(x02, x03);
  return ScanTraits<int32_tag.MaxBlocks()>::AddBlocks(int32_tag, x04);
}

// Compute two integral images from src. B sums elements; A sums their
// squares. The images are offset by one pixel, so will have width and height
// equal to width + 1, height + 1 and the first row and column will be zero.
//
// A+1 and B+1 should be aligned to 32 bytes. buf_stride should be a multiple
// of 8.
template <typename T, typename D>
HWY_ATTR HWY_INLINE void IntegralImages(D int32_tag, const T *HWY_RESTRICT src,
                                        int src_stride, int width, int height,
                                        int32_t *HWY_RESTRICT A,
                                        int32_t *HWY_RESTRICT B,
                                        int buf_stride) {
  constexpr hn::Rebind<T, D> uint_tag;
  constexpr hn::Repartition<int16_t, D> int16_tag;
  // Write out the zero top row
  hwy::ZeroBytes(A, 4 * (width + 8));
  hwy::ZeroBytes(B, 4 * (width + 8));

  for (int i = 0; i < height; ++i) {
    // Zero the left column.
    A[(i + 1) * buf_stride] = B[(i + 1) * buf_stride] = 0;

    // ldiff is the difference H - D where H is the output sample immediately
    // to the left and D is the output sample above it. These are scalars,
    // replicated across the eight lanes.
    auto ldiff1 = hn::Zero(int32_tag);
    auto ldiff2 = hn::Zero(int32_tag);
    for (int j = 0; j < width; j += hn::MaxLanes(int32_tag)) {
      const int ABj = 1 + j;

      const auto above1 = hn::Load(int32_tag, B + ABj + i * buf_stride);
      const auto above2 = hn::Load(int32_tag, A + ABj + i * buf_stride);

      const auto x1 = hn::PromoteTo(
          int32_tag, hn::LoadU(uint_tag, src + j + i * src_stride));
      const auto x2 = hn::WidenMulPairwiseAdd(
          int32_tag, hn::BitCast(int16_tag, x1), hn::BitCast(int16_tag, x1));

      const auto sc1 = Scan32(int32_tag, x1);
      const auto sc2 = Scan32(int32_tag, x2);

      const auto row1 = hn::Add(hn::Add(sc1, above1), ldiff1);
      const auto row2 = hn::Add(hn::Add(sc2, above2), ldiff2);

      hn::Store(row1, int32_tag, B + ABj + (i + 1) * buf_stride);
      hn::Store(row2, int32_tag, A + ABj + (i + 1) * buf_stride);

      // Calculate the new H - D.
      ldiff1 = hn::Set(int32_tag, hn::ExtractLane(hn::Sub(row1, above1),
                                                  hn::MaxLanes(int32_tag) - 1));
      ldiff2 = hn::Set(int32_tag, hn::ExtractLane(hn::Sub(row2, above2),
                                                  hn::MaxLanes(int32_tag) - 1));
    }
  }
}

template <typename D>
HWY_ATTR HWY_INLINE hn::VFromD<D> BoxSumFromII(D int32_tag,
                                               const int32_t *HWY_RESTRICT ii,
                                               int stride, int r) {
  const auto tl = hn::LoadU(int32_tag, ii - (r + 1) - (r + 1) * stride);
  const auto tr = hn::LoadU(int32_tag, ii + (r + 0) - (r + 1) * stride);
  const auto bl = hn::LoadU(int32_tag, ii - (r + 1) + r * stride);
  const auto br = hn::LoadU(int32_tag, ii + (r + 0) + r * stride);
  const auto u = hn::Sub(tr, tl);
  const auto v = hn::Sub(br, bl);
  return hn::Sub(v, u);
}

template <typename D>
HWY_ATTR HWY_INLINE hn::VFromD<D> RoundForShift(D int32_tag,
                                                unsigned int shift) {
  return hn::Set(int32_tag, (1 << shift) >> 1);
}

template <typename D>
HWY_ATTR HWY_INLINE hn::VFromD<D> ComputeP(D int32_tag, hn::VFromD<D> sum1,
                                           hn::VFromD<D> sum2, int bit_depth,
                                           int n) {
  constexpr hn::Repartition<int16_t, D> int16_tag;
  if (bit_depth > 8) {
    const auto rounding_a = RoundForShift(int32_tag, 2 * (bit_depth - 8));
    const auto rounding_b = RoundForShift(int32_tag, bit_depth - 8);
    const auto a =
        hn::ShiftRightSame(hn::Add(sum2, rounding_a), 2 * (bit_depth - 8));
    const auto b = hn::ShiftRightSame(hn::Add(sum1, rounding_b), bit_depth - 8);
    // b < 2^14, so we can use a 16-bit madd rather than a 32-bit
    // mullo to square it
    const auto b_16 = hn::BitCast(int16_tag, b);
    const auto bb = hn::WidenMulPairwiseAdd(int32_tag, b_16, b_16);
    const auto an = hn::Max(hn::Mul(a, hn::Set(int32_tag, n)), bb);
    return hn::Sub(an, bb);
  }
  const auto sum1_16 = hn::BitCast(int16_tag, sum1);
  const auto bb = hn::WidenMulPairwiseAdd(int32_tag, sum1_16, sum1_16);
  const auto an = hn::Mul(sum2, hn::Set(int32_tag, n));
  return hn::Sub(an, bb);
}

// Calculate 8 values of the "cross sum" starting at buf. This is a 3x3 filter
// where the outer four corners have weight 3 and all other pixels have weight
// 4.
//
// Pixels are indexed as follows:
// xtl  xt   xtr
// xl    x   xr
// xbl  xb   xbr
//
// buf points to x
//
// fours = xl + xt + xr + xb + x
// threes = xtl + xtr + xbr + xbl
// cross_sum = 4 * fours + 3 * threes
//           = 4 * (fours + threes) - threes
//           = (fours + threes) << 2 - threes
template <typename D>
HWY_ATTR HWY_INLINE hn::VFromD<D> CrossSum(D int32_tag,
                                           const int32_t *HWY_RESTRICT buf,
                                           int stride) {
  const auto xtl = hn::LoadU(int32_tag, buf - 1 - stride);
  const auto xt = hn::LoadU(int32_tag, buf - stride);
  const auto xtr = hn::LoadU(int32_tag, buf + 1 - stride);
  const auto xl = hn::LoadU(int32_tag, buf - 1);
  const auto x = hn::LoadU(int32_tag, buf);
  const auto xr = hn::LoadU(int32_tag, buf + 1);
  const auto xbl = hn::LoadU(int32_tag, buf - 1 + stride);
  const auto xb = hn::LoadU(int32_tag, buf + stride);
  const auto xbr = hn::LoadU(int32_tag, buf + 1 + stride);

  const auto fours = hn::Add(xl, hn::Add(xt, hn::Add(xr, hn::Add(xb, x))));
  const auto threes = hn::Add(xtl, hn::Add(xtr, hn::Add(xbr, xbl)));

  return hn::Sub(hn::ShiftLeft<2>(hn::Add(fours, threes)), threes);
}

// The final filter for self-guided restoration. Computes a weighted average
// across A, B with "cross sums" (see CrossSum implementation above).
template <typename DL>
HWY_ATTR HWY_INLINE void FinalFilter(
    DL int32_tag, int32_t *HWY_RESTRICT dst, int dst_stride,
    const int32_t *HWY_RESTRICT A, const int32_t *HWY_RESTRICT B,
    int buf_stride, const void *HWY_RESTRICT dgd8, int dgd_stride, int width,
    int height, int highbd) {
  constexpr hn::Repartition<uint8_t, hn::Half<DL>> uint8_half_tag;
  constexpr hn::Repartition<int16_t, DL> int16_tag;
  constexpr int nb = 5;
  constexpr int kShift = SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS;
  const auto rounding = RoundForShift(int32_tag, kShift);
  const uint8_t *HWY_RESTRICT dgd_real =
      highbd ? reinterpret_cast<const uint8_t *>(CONVERT_TO_SHORTPTR(dgd8))
             : reinterpret_cast<const uint8_t *>(dgd8);

  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < width; j += hn::MaxLanes(int32_tag)) {
      const auto a = CrossSum(int32_tag, A + i * buf_stride + j, buf_stride);
      const auto b = CrossSum(int32_tag, B + i * buf_stride + j, buf_stride);

      const auto raw = hn::LoadU(uint8_half_tag,
                                 dgd_real + ((i * dgd_stride + j) << highbd));
      const auto src =
          highbd ? hn::PromoteTo(
                       int32_tag,
                       hn::BitCast(
                           hn::Repartition<int16_t, decltype(uint8_half_tag)>(),
                           raw))
                 : hn::PromoteTo(int32_tag, hn::LowerHalf(raw));

      auto v =
          hn::Add(hn::WidenMulPairwiseAdd(int32_tag, hn::BitCast(int16_tag, a),
                                          hn::BitCast(int16_tag, src)),
                  b);
      auto w = hn::ShiftRight<kShift>(hn::Add(v, rounding));

      hn::StoreU(w, int32_tag, dst + i * dst_stride + j);
    }
  }
}

// Assumes that C, D are integral images for the original buffer which has been
// extended to have a padding of SGRPROJ_BORDER_VERT/SGRPROJ_BORDER_HORZ pixels
// on the sides. A, B, C, D point at logical position (0, 0).
template <int Step, typename DL>
HWY_ATTR HWY_INLINE void CalcAB(DL int32_tag, int32_t *HWY_RESTRICT A,
                                int32_t *HWY_RESTRICT B,
                                const int32_t *HWY_RESTRICT C,
                                const int32_t *HWY_RESTRICT D, int width,
                                int height, int buf_stride, int bit_depth,
                                int sgr_params_idx, int radius_idx) {
  constexpr hn::Repartition<int16_t, DL> int16_tag;
  constexpr hn::Repartition<uint32_t, DL> uint32_tag;
  const sgr_params_type *HWY_RESTRICT const params =
      &av1_sgr_params[sgr_params_idx];
  const int r = params->r[radius_idx];
  const int n = (2 * r + 1) * (2 * r + 1);
  const auto s = hn::Set(int32_tag, params->s[radius_idx]);
  // one_over_n[n-1] is 2^12/n, so easily fits in an int16
  const auto one_over_n =
      hn::BitCast(int16_tag, hn::Set(int32_tag, av1_one_by_x[n - 1]));

  const auto rnd_z = RoundForShift(int32_tag, SGRPROJ_MTABLE_BITS);
  const auto rnd_res = RoundForShift(int32_tag, SGRPROJ_RECIP_BITS);

  // Set up masks
  const int max_lanes = static_cast<int>(hn::MaxLanes(int32_tag));
  HWY_ALIGN hn::Mask<decltype(int32_tag)> mask[max_lanes];
  for (int idx = 0; idx < max_lanes; idx++) {
    mask[idx] = hn::FirstN(int32_tag, idx);
  }

  for (int i = -1; i < height + 1; i += Step) {
    for (int j = -1; j < width + 1; j += max_lanes) {
      const int32_t *HWY_RESTRICT Cij = C + i * buf_stride + j;
      const int32_t *HWY_RESTRICT Dij = D + i * buf_stride + j;

      auto sum1 = BoxSumFromII(int32_tag, Dij, buf_stride, r);
      auto sum2 = BoxSumFromII(int32_tag, Cij, buf_stride, r);

      // When width + 2 isn't a multiple of 8, sum1 and sum2 will contain
      // some uninitialised data in their upper words. We use a mask to
      // ensure that these bits are set to 0.
      int idx = AOMMIN(max_lanes, width + 1 - j);
      assert(idx >= 1);

      if (idx < max_lanes) {
        sum1 = hn::IfThenElseZero(mask[idx], sum1);
        sum2 = hn::IfThenElseZero(mask[idx], sum2);
      }

      const auto p = ComputeP(int32_tag, sum1, sum2, bit_depth, n);

      const auto z = hn::BitCast(
          int32_tag, hn::Min(hn::ShiftRight<SGRPROJ_MTABLE_BITS>(hn::BitCast(
                                 uint32_tag, hn::MulAdd(p, s, rnd_z))),
                             hn::Set(uint32_tag, 255)));

      const auto a_res = hn::GatherIndex(int32_tag, av1_x_by_xplus1, z);

      hn::StoreU(a_res, int32_tag, A + i * buf_stride + j);

      const auto a_complement = hn::Sub(hn::Set(int32_tag, SGRPROJ_SGR), a_res);

      // sum1 might have lanes greater than 2^15, so we can't use madd to do
      // multiplication involving sum1. However, a_complement and one_over_n
      // are both less than 256, so we can multiply them first.
      const auto a_comp_over_n = hn::WidenMulPairwiseAdd(
          int32_tag, hn::BitCast(int16_tag, a_complement), one_over_n);
      const auto b_int = hn::Mul(a_comp_over_n, sum1);
      const auto b_res =
          hn::ShiftRight<SGRPROJ_RECIP_BITS>(hn::Add(b_int, rnd_res));

      hn::StoreU(b_res, int32_tag, B + i * buf_stride + j);
    }
  }
}

// Calculate 8 values of the "cross sum" starting at buf.
//
// Pixels are indexed like this:
// xtl  xt   xtr
//  -   buf   -
// xbl  xb   xbr
//
// Pixels are weighted like this:
//  5    6    5
//  0    0    0
//  5    6    5
//
// fives = xtl + xtr + xbl + xbr
// sixes = xt + xb
// cross_sum = 6 * sixes + 5 * fives
//           = 5 * (fives + sixes) - sixes
//           = (fives + sixes) << 2 + (fives + sixes) + sixes
template <typename D>
HWY_ATTR HWY_INLINE hn::VFromD<D> CrossSumFastEvenRow(
    D int32_tag, const int32_t *HWY_RESTRICT buf, int stride) {
  const auto xtl = hn::LoadU(int32_tag, buf - 1 - stride);
  const auto xt = hn::LoadU(int32_tag, buf - stride);
  const auto xtr = hn::LoadU(int32_tag, buf + 1 - stride);
  const auto xbl = hn::LoadU(int32_tag, buf - 1 + stride);
  const auto xb = hn::LoadU(int32_tag, buf + stride);
  const auto xbr = hn::LoadU(int32_tag, buf + 1 + stride);

  const auto fives = hn::Add(xtl, hn::Add(xtr, hn::Add(xbr, xbl)));
  const auto sixes = hn::Add(xt, xb);
  const auto fives_plus_sixes = hn::Add(fives, sixes);

  return hn::Add(hn::Add(hn::ShiftLeft<2>(fives_plus_sixes), fives_plus_sixes),
                 sixes);
}

// Calculate 8 values of the "cross sum" starting at buf.
//
// Pixels are indexed like this:
// xl    x   xr
//
// Pixels are weighted like this:
//  5    6    5
//
// buf points to x
//
// fives = xl + xr
// sixes = x
// cross_sum = 5 * fives + 6 * sixes
//           = 4 * (fives + sixes) + (fives + sixes) + sixes
//           = (fives + sixes) << 2 + (fives + sixes) + sixes
template <typename D>
HWY_ATTR HWY_INLINE hn::VFromD<D> CrossSumFastOddRow(
    D int32_tag, const int32_t *HWY_RESTRICT buf) {
  const auto xl = hn::LoadU(int32_tag, buf - 1);
  const auto x = hn::LoadU(int32_tag, buf);
  const auto xr = hn::LoadU(int32_tag, buf + 1);

  const auto fives = hn::Add(xl, xr);
  const auto sixes = x;

  const auto fives_plus_sixes = hn::Add(fives, sixes);

  return hn::Add(hn::Add(hn::ShiftLeft<2>(fives_plus_sixes), fives_plus_sixes),
                 sixes);
}

// The final filter for the self-guided restoration. Computes a
// weighted average across A, B with "cross sums" (see cross_sum_...
// implementations above).
template <typename DL>
HWY_ATTR HWY_INLINE void FinalFilterFast(
    DL int32_tag, int32_t *HWY_RESTRICT dst, int dst_stride,
    const int32_t *HWY_RESTRICT A, const int32_t *HWY_RESTRICT B,
    int buf_stride, const void *HWY_RESTRICT dgd8, int dgd_stride, int width,
    int height, int highbd) {
  constexpr hn::Repartition<uint8_t, hn::Half<DL>> uint8_half_tag;
  constexpr hn::Repartition<int16_t, DL> int16_tag;
  constexpr int nb0 = 5;
  constexpr int nb1 = 4;
  constexpr int kShift0 = SGRPROJ_SGR_BITS + nb0 - SGRPROJ_RST_BITS;
  constexpr int kShift1 = SGRPROJ_SGR_BITS + nb1 - SGRPROJ_RST_BITS;

  const auto rounding0 = RoundForShift(int32_tag, kShift0);
  const auto rounding1 = RoundForShift(int32_tag, kShift1);

  const uint8_t *HWY_RESTRICT dgd_real =
      highbd ? reinterpret_cast<const uint8_t *>(CONVERT_TO_SHORTPTR(dgd8))
             : reinterpret_cast<const uint8_t *>(dgd8);

  for (int i = 0; i < height; ++i) {
    if (!(i & 1)) {  // even row
      for (int j = 0; j < width; j += hn::MaxLanes(int32_tag)) {
        const auto a =
            CrossSumFastEvenRow(int32_tag, A + i * buf_stride + j, buf_stride);
        const auto b =
            CrossSumFastEvenRow(int32_tag, B + i * buf_stride + j, buf_stride);

        const auto raw = hn::LoadU(uint8_half_tag,
                                   dgd_real + ((i * dgd_stride + j) << highbd));
        const auto src =
            highbd
                ? hn::PromoteTo(
                      int32_tag,
                      hn::BitCast(
                          hn::Repartition<int16_t, decltype(uint8_half_tag)>(),
                          raw))
                : hn::PromoteTo(int32_tag, hn::LowerHalf(raw));

        auto v = hn::Add(
            hn::WidenMulPairwiseAdd(int32_tag, hn::BitCast(int16_tag, a),
                                    hn::BitCast(int16_tag, src)),
            b);
        auto w = hn::ShiftRight<kShift0>(hn::Add(v, rounding0));

        hn::StoreU(w, int32_tag, dst + i * dst_stride + j);
      }
    } else {  // odd row
      for (int j = 0; j < width; j += hn::MaxLanes(int32_tag)) {
        const auto a = CrossSumFastOddRow(int32_tag, A + i * buf_stride + j);
        const auto b = CrossSumFastOddRow(int32_tag, B + i * buf_stride + j);

        const auto raw = hn::LoadU(uint8_half_tag,
                                   dgd_real + ((i * dgd_stride + j) << highbd));
        const auto src =
            highbd
                ? hn::PromoteTo(
                      int32_tag,
                      hn::BitCast(
                          hn::Repartition<int16_t, decltype(uint8_half_tag)>(),
                          raw))
                : hn::PromoteTo(int32_tag, hn::LowerHalf(raw));

        auto v = hn::Add(
            hn::WidenMulPairwiseAdd(int32_tag, hn::BitCast(int16_tag, a),
                                    hn::BitCast(int16_tag, src)),
            b);
        auto w = hn::ShiftRight<kShift1>(hn::Add(v, rounding1));

        hn::StoreU(w, int32_tag, dst + i * dst_stride + j);
      }
    }
  }
}

HWY_ATTR HWY_INLINE int SelfGuidedRestoration(
    const uint8_t *dgd8, int width, int height, int dgd_stride,
    int32_t *HWY_RESTRICT flt0, int32_t *HWY_RESTRICT flt1, int flt_stride,
    int sgr_params_idx, int bit_depth, int highbd) {
  constexpr hn::ScalableTag<int32_t> int32_tag;
  constexpr int kAlignment32Log2 = hwy::CeilLog2(hn::MaxLanes(int32_tag));
  // The ALIGN_POWER_OF_TWO macro here ensures that column 1 of Atl, Btl, Ctl
  // and Dtl is vector aligned.
  const int buf_elts =
      ALIGN_POWER_OF_TWO(RESTORATION_PROC_UNIT_PELS, kAlignment32Log2);

  int32_t *buf = reinterpret_cast<int32_t *>(
      aom_memalign(4 << kAlignment32Log2, 4 * sizeof(*buf) * buf_elts));
  if (!buf) {
    return -1;
  }

  const int width_ext = width + 2 * SGRPROJ_BORDER_HORZ;
  const int height_ext = height + 2 * SGRPROJ_BORDER_VERT;

  // Adjusting the stride of A and B here appears to avoid bad cache effects,
  // leading to a significant speed improvement.
  // We also align the stride to a multiple of the vector size for efficiency.
  int buf_stride =
      ALIGN_POWER_OF_TWO(width_ext + (2 << kAlignment32Log2), kAlignment32Log2);

  // The "tl" pointers point at the top-left of the initialised data for the
  // array.
  int32_t *Atl = buf + 0 * buf_elts + (1 << kAlignment32Log2) - 1;
  int32_t *Btl = buf + 1 * buf_elts + (1 << kAlignment32Log2) - 1;
  int32_t *Ctl = buf + 2 * buf_elts + (1 << kAlignment32Log2) - 1;
  int32_t *Dtl = buf + 3 * buf_elts + (1 << kAlignment32Log2) - 1;

  // The "0" pointers are (- SGRPROJ_BORDER_VERT, -SGRPROJ_BORDER_HORZ). Note
  // there's a zero row and column in A, B (integral images), so we move down
  // and right one for them.
  const int buf_diag_border =
      SGRPROJ_BORDER_HORZ + buf_stride * SGRPROJ_BORDER_VERT;

  int32_t *A0 = Atl + 1 + buf_stride;
  int32_t *B0 = Btl + 1 + buf_stride;
  int32_t *C0 = Ctl + 1 + buf_stride;
  int32_t *D0 = Dtl + 1 + buf_stride;

  // Finally, A, B, C, D point at position (0, 0).
  int32_t *A = A0 + buf_diag_border;
  int32_t *B = B0 + buf_diag_border;
  int32_t *C = C0 + buf_diag_border;
  int32_t *D = D0 + buf_diag_border;

  const int dgd_diag_border =
      SGRPROJ_BORDER_HORZ + dgd_stride * SGRPROJ_BORDER_VERT;
  const uint8_t *dgd0 = dgd8 - dgd_diag_border;

  // Generate integral images from the input. C will contain sums of squares; D
  // will contain just sums
  if (highbd) {
    IntegralImages(int32_tag, CONVERT_TO_SHORTPTR(dgd0), dgd_stride, width_ext,
                   height_ext, Ctl, Dtl, buf_stride);
  } else {
    IntegralImages(int32_tag, dgd0, dgd_stride, width_ext, height_ext, Ctl, Dtl,
                   buf_stride);
  }

  const sgr_params_type *const params = &av1_sgr_params[sgr_params_idx];
  // Write to flt0 and flt1
  // If params->r == 0 we skip the corresponding filter. We only allow one of
  // the radii to be 0, as having both equal to 0 would be equivalent to
  // skipping SGR entirely.
  assert(!(params->r[0] == 0 && params->r[1] == 0));
  assert(params->r[0] < AOMMIN(SGRPROJ_BORDER_VERT, SGRPROJ_BORDER_HORZ));
  assert(params->r[1] < AOMMIN(SGRPROJ_BORDER_VERT, SGRPROJ_BORDER_HORZ));

  if (params->r[0] > 0) {
    CalcAB<2>(int32_tag, A, B, C, D, width, height, buf_stride, bit_depth,
              sgr_params_idx, 0);
    FinalFilterFast(int32_tag, flt0, flt_stride, A, B, buf_stride, dgd8,
                    dgd_stride, width, height, highbd);
  }

  if (params->r[1] > 0) {
    CalcAB<1>(int32_tag, A, B, C, D, width, height, buf_stride, bit_depth,
              sgr_params_idx, 1);
    FinalFilter(int32_tag, flt1, flt_stride, A, B, buf_stride, dgd8, dgd_stride,
                width, height, highbd);
  }
  aom_free(buf);
  return 0;
}

HWY_ATTR HWY_INLINE int ApplySelfGuidedRestoration(
    const uint8_t *HWY_RESTRICT dat8, int width, int height, int stride,
    int eps, const int *HWY_RESTRICT xqd, uint8_t *HWY_RESTRICT dst8,
    int dst_stride, int32_t *HWY_RESTRICT tmpbuf, int bit_depth, int highbd) {
  constexpr hn::CappedTag<int32_t, 16> int32_tag;
  constexpr size_t kBatchSize = hn::MaxLanes(int32_tag) * 2;
  int32_t *flt0 = tmpbuf;
  int32_t *flt1 = flt0 + RESTORATION_UNITPELS_MAX;
  assert(width * height <= RESTORATION_UNITPELS_MAX);
#if HWY_TARGET == HWY_SSE4
  const int ret = av1_selfguided_restoration_sse4_1(
      dat8, width, height, stride, flt0, flt1, width, eps, bit_depth, highbd);
#elif HWY_TARGET == HWY_AVX2
  const int ret = av1_selfguided_restoration_avx2(
      dat8, width, height, stride, flt0, flt1, width, eps, bit_depth, highbd);
#elif HWY_TARGET <= HWY_AVX3
  const int ret = av1_selfguided_restoration_avx512(
      dat8, width, height, stride, flt0, flt1, width, eps, bit_depth, highbd);
#else
#error "HWY_TARGET is not supported."
  const int ret = -1;
#endif
  if (ret != 0) {
    return ret;
  }
  const sgr_params_type *const params = &av1_sgr_params[eps];
  int xq[2];
  av1_decode_xq(xqd, xq, params);

  auto xq0 = hn::Set(int32_tag, xq[0]);
  auto xq1 = hn::Set(int32_tag, xq[1]);

  for (int i = 0; i < height; ++i) {
    // Calculate output in batches of pixels
    for (int j = 0; j < width; j += kBatchSize) {
      const int k = i * width + j;
      const int m = i * dst_stride + j;

      const uint8_t *dat8ij = dat8 + i * stride + j;
      auto ep_0 = hn::Undefined(int32_tag);
      auto ep_1 = hn::Undefined(int32_tag);
      if (highbd) {
        constexpr hn::Repartition<uint16_t, hn::Half<decltype(int32_tag)>>
            uint16_tag;
        const auto src_0 = hn::LoadU(uint16_tag, CONVERT_TO_SHORTPTR(dat8ij));
        const auto src_1 = hn::LoadU(
            uint16_tag, CONVERT_TO_SHORTPTR(dat8ij) + hn::MaxLanes(int32_tag));
        ep_0 = hn::PromoteTo(int32_tag, src_0);
        ep_1 = hn::PromoteTo(int32_tag, src_1);
      } else {
        constexpr hn::Repartition<uint8_t, hn::Half<decltype(int32_tag)>>
            uint8_tag;
        const auto src_0 = hn::LoadU(uint8_tag, dat8ij);
        ep_0 = hn::PromoteLowerTo(int32_tag, src_0);
        ep_1 = hn::PromoteUpperTo(int32_tag, src_0);
      }

      const auto u_0 = hn::ShiftLeft<SGRPROJ_RST_BITS>(ep_0);
      const auto u_1 = hn::ShiftLeft<SGRPROJ_RST_BITS>(ep_1);

      auto v_0 = hn::ShiftLeft<SGRPROJ_PRJ_BITS>(u_0);
      auto v_1 = hn::ShiftLeft<SGRPROJ_PRJ_BITS>(u_1);

      if (params->r[0] > 0) {
        const auto f1_0 = hn::Sub(hn::LoadU(int32_tag, &flt0[k]), u_0);
        v_0 = hn::Add(v_0, hn::Mul(xq0, f1_0));

        const auto f1_1 = hn::Sub(
            hn::LoadU(int32_tag, &flt0[k + hn::MaxLanes(int32_tag)]), u_1);
        v_1 = hn::Add(v_1, hn::Mul(xq0, f1_1));
      }

      if (params->r[1] > 0) {
        const auto f2_0 = hn::Sub(hn::LoadU(int32_tag, &flt1[k]), u_0);
        v_0 = hn::Add(v_0, hn::Mul(xq1, f2_0));

        const auto f2_1 = hn::Sub(
            hn::LoadU(int32_tag, &flt1[k + hn::MaxLanes(int32_tag)]), u_1);
        v_1 = hn::Add(v_1, hn::Mul(xq1, f2_1));
      }

      const auto rounding =
          RoundForShift(int32_tag, SGRPROJ_PRJ_BITS + SGRPROJ_RST_BITS);
      const auto w_0 = hn::ShiftRight<SGRPROJ_PRJ_BITS + SGRPROJ_RST_BITS>(
          hn::Add(v_0, rounding));
      const auto w_1 = hn::ShiftRight<SGRPROJ_PRJ_BITS + SGRPROJ_RST_BITS>(
          hn::Add(v_1, rounding));

      if (highbd) {
        // Pack into 16 bits and clamp to [0, 2^bit_depth)
        // Note that packing into 16 bits messes up the order of the bits,
        // so we use a permute function to correct this
        constexpr hn::Repartition<uint16_t, decltype(int32_tag)> uint16_tag;
        const auto tmp = hn::OrderedDemote2To(uint16_tag, w_0, w_1);
        const auto max = hn::Set(uint16_tag, (1 << bit_depth) - 1);
        const auto res = hn::Min(tmp, max);
        hn::StoreU(res, uint16_tag, CONVERT_TO_SHORTPTR(dst8 + m));
      } else {
        // Pack into 8 bits and clamp to [0, 256)
        // Note that each pack messes up the order of the bits,
        // so we use a permute function to correct this
        constexpr hn::Repartition<int16_t, decltype(int32_tag)> int16_tag;
        constexpr hn::Repartition<uint8_t, hn::Half<decltype(int32_tag)>>
            uint8_tag;
        const auto tmp = hn::OrderedDemote2To(int16_tag, w_0, w_1);
        const auto res = hn::DemoteTo(uint8_tag, tmp);
        hn::StoreU(res, uint8_tag, dst8 + m);
      }
    }
  }
  return 0;
}

}  // namespace HWY_NAMESPACE
}  // namespace

HWY_AFTER_NAMESPACE();

#define MAKE_SELFGUIDED_RESTORATION(suffix)                              \
  extern "C" int av1_selfguided_restoration_##suffix(                    \
      const uint8_t *dgd8, int width, int height, int dgd_stride,        \
      int32_t *flt0, int32_t *flt1, int flt_stride, int sgr_params_idx,  \
      int bit_depth, int highbd);                                        \
  HWY_ATTR HWY_NOINLINE int av1_selfguided_restoration_##suffix(         \
      const uint8_t *dgd8, int width, int height, int dgd_stride,        \
      int32_t *flt0, int32_t *flt1, int flt_stride, int sgr_params_idx,  \
      int bit_depth, int highbd) {                                       \
    return HWY_NAMESPACE::SelfGuidedRestoration(                         \
        dgd8, width, height, dgd_stride, flt0, flt1, flt_stride,         \
        sgr_params_idx, bit_depth, highbd);                              \
  }                                                                      \
  extern "C" int av1_apply_selfguided_restoration_##suffix(              \
      const uint8_t *dat8, int width, int height, int stride, int eps,   \
      const int *xqd, uint8_t *dst8, int dst_stride, int32_t *tmpbuf,    \
      int bit_depth, int highbd);                                        \
  HWY_ATTR int av1_apply_selfguided_restoration_##suffix(                \
      const uint8_t *dat8, int width, int height, int stride, int eps,   \
      const int *xqd, uint8_t *dst8, int dst_stride, int32_t *tmpbuf,    \
      int bit_depth, int highbd) {                                       \
    return HWY_NAMESPACE::ApplySelfGuidedRestoration(                    \
        dat8, width, height, stride, eps, xqd, dst8, dst_stride, tmpbuf, \
        bit_depth, highbd);                                              \
  }

#endif  // AV1_COMMON_SELFGUIDED_HWY_H_
