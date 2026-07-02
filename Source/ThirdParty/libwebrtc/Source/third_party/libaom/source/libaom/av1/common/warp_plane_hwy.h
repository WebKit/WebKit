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

#ifndef AOM_AV1_COMMON_WARP_PLANE_HWY_H_
#define AOM_AV1_COMMON_WARP_PLANE_HWY_H_

#include "av1/common/warped_motion.h"
#include "config/av1_rtcd.h"
#include "third_party/highway/hwy/highway.h"

HWY_BEFORE_NAMESPACE();

namespace {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

constexpr hn::ScalableTag<uint8_t> uint8xN_tag;
constexpr hn::ScalableTag<uint16_t> uint16xN_tag;

constexpr hn::ScalableTag<int16_t> int16xN_tag;
constexpr hn::ScalableTag<int32_t> int32xN_tag;
constexpr hn::ScalableTag<int64_t> int64xN_tag;

constexpr hn::CappedTag<uint8_t, 32> uint8x32_tag;
constexpr hn::CappedTag<int16_t, 16> int16x16_tag;

constexpr hn::FixedTag<uint8_t, 4> uint8x4_tag;
constexpr hn::FixedTag<uint8_t, 8> uint8x8_tag;
constexpr hn::FixedTag<uint8_t, 16> uint8x16_tag;
constexpr hn::FixedTag<uint16_t, 4> uint16x4_tag;
constexpr hn::FixedTag<uint16_t, 8> uint16x8_tag;

constexpr hn::FixedTag<int8_t, 8> int8x8_tag;
constexpr hn::FixedTag<int8_t, 16> int8x16_tag;
constexpr hn::FixedTag<int16_t, 8> int16x8_tag;
constexpr hn::FixedTag<int32_t, 4> int32x4_tag;
constexpr hn::FixedTag<int64_t, 2> int64x2_tag;

constexpr hn::ScalableTag<int8_t> coeff_tag;

using IVec16 = hn::Vec<decltype(int16xN_tag)>;
using IVec32 = hn::Vec<decltype(int32xN_tag)>;
using IVec8x16 = hn::Vec<decltype(int8x16_tag)>;

template <typename D>
HWY_ATTR inline void FilterPixelsHorizontal(D tag, const hn::VFromD<D> src,
                                            int16_t *HWY_RESTRICT horz_out,
                                            int8_t *HWY_RESTRICT coeff,
                                            const IVec16 round_const,
                                            const int shift, int row) {
  constexpr hn::Repartition<int8_t, D> int8_tag;
  constexpr hn::Repartition<int16_t, D> result_tag;
  constexpr hn::Repartition<uint16_t, D> unsigned_result_tag;
  // N.B. coeffs are stored to support the maximum vector width, which may not
  // be the vector width being filtered on now.
  const auto coeff0 = hn::Load(int8_tag, coeff + hn::MaxLanes(coeff_tag) * 0);
  const auto coeff1 = hn::Load(int8_tag, coeff + hn::MaxLanes(coeff_tag) * 1);
  const auto coeff2 = hn::Load(int8_tag, coeff + hn::MaxLanes(coeff_tag) * 2);
  const auto coeff3 = hn::Load(int8_tag, coeff + hn::MaxLanes(coeff_tag) * 3);

  const auto shuffle0 = hn::Dup128VecFromValues(
      uint8xN_tag, 0, 2, 2, 4, 4, 6, 6, 8, 1, 3, 3, 5, 5, 7, 7, 9  //
  );
  const auto shuffle1 = hn::Dup128VecFromValues(
      uint8xN_tag, 4, 6, 6, 8, 8, 10, 10, 12, 5, 7, 7, 9, 9, 11, 11, 13  //
  );
  const auto shuffle2 = hn::Dup128VecFromValues(
      uint8xN_tag, 1, 3, 3, 5, 5, 7, 7, 9, 2, 4, 4, 6, 6, 8, 8, 10  //
  );
  const auto shuffle3 = hn::Dup128VecFromValues(
      uint8xN_tag, 5, 7, 7, 9, 9, 11, 11, 13, 6, 8, 8, 10, 10, 12, 12, 14  //
  );

  const auto src_0 =
      hn::TableLookupBytes(src, hn::ResizeBitCast(tag, shuffle0));
  const auto src_1 =
      hn::TableLookupBytes(src, hn::ResizeBitCast(tag, shuffle1));
  const auto src_2 =
      hn::TableLookupBytes(src, hn::ResizeBitCast(tag, shuffle2));
  const auto src_3 =
      hn::TableLookupBytes(src, hn::ResizeBitCast(tag, shuffle3));

  const auto res_02 = hn::SatWidenMulPairwiseAdd(result_tag, src_0, coeff0);
  const auto res_46 = hn::SatWidenMulPairwiseAdd(result_tag, src_1, coeff1);
  const auto res_13 = hn::SatWidenMulPairwiseAdd(result_tag, src_2, coeff2);
  const auto res_57 = hn::SatWidenMulPairwiseAdd(result_tag, src_3, coeff3);

  const auto res_even = hn::Add(res_02, res_46);
  const auto res_odd = hn::Add(res_13, res_57);

  const auto res = hn::Add(hn::Add(res_even, res_odd),
                           hn::ResizeBitCast(result_tag, round_const));

  hn::Store(hn::BitCast(result_tag,
                        hn::ShiftRightSame(
                            hn::BitCast(unsigned_result_tag, res), shift)),
            result_tag, horz_out + row * hn::MaxLanes(int16x8_tag));
}

HWY_ATTR HWY_INLINE IVec8x16 LoadAV1Filter8Bit(unsigned int offset) {
  return hn::LoadN(int8x16_tag, av1_filter_8bit[offset >> WARPEDDIFF_PREC_BITS],
                   8);
}

template <typename D>
HWY_ATTR HWY_INLINE hn::VFromD<D> LoadAV1Filter8BitLower(D int8_tag,
                                                         unsigned int offset) {
  return hn::LoadN(int8_tag, av1_filter_8bit[offset >> WARPEDDIFF_PREC_BITS],
                   8);
}

template <int Block, typename D>
HWY_ATTR HWY_INLINE hn::VFromD<D> LoadAV1Filter8BitUpper(D int8_tag,
                                                         unsigned int offset,
                                                         hn::VFromD<D> src) {
  (void)int8_tag;
  return hn::InsertBlock<Block>(
      src, hn::LoadN(int8x16_tag,
                     av1_filter_8bit[offset >> WARPEDDIFF_PREC_BITS], 8));
}

template <typename D>
HWY_ATTR inline void PrepareHorizontalFilterCoefficients(
    D int16_tag, int alpha, int beta, int sx, int8_t *HWY_RESTRICT coeff) {
  constexpr auto int8_tag = hn::Repartition<int8_t, D>();
  constexpr auto int32_tag = hn::Repartition<int32_t, D>();
  constexpr auto int64_tag = hn::Repartition<int64_t, D>();

  auto tmp_0 = LoadAV1Filter8BitLower(int8_tag, sx + 0 * alpha);
  auto tmp_1 = LoadAV1Filter8BitLower(int8_tag, sx + 1 * alpha);
  auto tmp_2 = LoadAV1Filter8BitLower(int8_tag, sx + 2 * alpha);
  auto tmp_3 = LoadAV1Filter8BitLower(int8_tag, sx + 3 * alpha);
  auto tmp_4 = LoadAV1Filter8BitLower(int8_tag, sx + 4 * alpha);
  auto tmp_5 = LoadAV1Filter8BitLower(int8_tag, sx + 5 * alpha);
  auto tmp_6 = LoadAV1Filter8BitLower(int8_tag, sx + 6 * alpha);
  auto tmp_7 = LoadAV1Filter8BitLower(int8_tag, sx + 7 * alpha);

  if constexpr (int16_tag.MaxBlocks() >= 2) {
    tmp_0 = LoadAV1Filter8BitUpper<1>(int8_tag, sx + beta + 0 * alpha, tmp_0);
    tmp_1 = LoadAV1Filter8BitUpper<1>(int8_tag, sx + beta + 1 * alpha, tmp_1);
    tmp_2 = LoadAV1Filter8BitUpper<1>(int8_tag, sx + beta + 2 * alpha, tmp_2);
    tmp_3 = LoadAV1Filter8BitUpper<1>(int8_tag, sx + beta + 3 * alpha, tmp_3);
    tmp_4 = LoadAV1Filter8BitUpper<1>(int8_tag, sx + beta + 4 * alpha, tmp_4);
    tmp_5 = LoadAV1Filter8BitUpper<1>(int8_tag, sx + beta + 5 * alpha, tmp_5);
    tmp_6 = LoadAV1Filter8BitUpper<1>(int8_tag, sx + beta + 6 * alpha, tmp_6);
    tmp_7 = LoadAV1Filter8BitUpper<1>(int8_tag, sx + beta + 7 * alpha, tmp_7);
  }

  if constexpr (int16_tag.MaxBlocks() >= 3) {
    tmp_0 =
        LoadAV1Filter8BitUpper<2>(int8_tag, sx + beta * 2 + 0 * alpha, tmp_0);
    tmp_1 =
        LoadAV1Filter8BitUpper<2>(int8_tag, sx + beta * 2 + 1 * alpha, tmp_1);
    tmp_2 =
        LoadAV1Filter8BitUpper<2>(int8_tag, sx + beta * 2 + 2 * alpha, tmp_2);
    tmp_3 =
        LoadAV1Filter8BitUpper<2>(int8_tag, sx + beta * 2 + 3 * alpha, tmp_3);
    tmp_4 =
        LoadAV1Filter8BitUpper<2>(int8_tag, sx + beta * 2 + 4 * alpha, tmp_4);
    tmp_5 =
        LoadAV1Filter8BitUpper<2>(int8_tag, sx + beta * 2 + 5 * alpha, tmp_5);
    tmp_6 =
        LoadAV1Filter8BitUpper<2>(int8_tag, sx + beta * 2 + 6 * alpha, tmp_6);
    tmp_7 =
        LoadAV1Filter8BitUpper<2>(int8_tag, sx + beta * 2 + 7 * alpha, tmp_7);

    tmp_0 =
        LoadAV1Filter8BitUpper<3>(int8_tag, sx + beta * 3 + 0 * alpha, tmp_0);
    tmp_1 =
        LoadAV1Filter8BitUpper<3>(int8_tag, sx + beta * 3 + 1 * alpha, tmp_1);
    tmp_2 =
        LoadAV1Filter8BitUpper<3>(int8_tag, sx + beta * 3 + 2 * alpha, tmp_2);
    tmp_3 =
        LoadAV1Filter8BitUpper<3>(int8_tag, sx + beta * 3 + 3 * alpha, tmp_3);
    tmp_4 =
        LoadAV1Filter8BitUpper<3>(int8_tag, sx + beta * 3 + 4 * alpha, tmp_4);
    tmp_5 =
        LoadAV1Filter8BitUpper<3>(int8_tag, sx + beta * 3 + 5 * alpha, tmp_5);
    tmp_6 =
        LoadAV1Filter8BitUpper<3>(int8_tag, sx + beta * 3 + 6 * alpha, tmp_6);
    tmp_7 =
        LoadAV1Filter8BitUpper<3>(int8_tag, sx + beta * 3 + 7 * alpha, tmp_7);
  }

  const auto tmp_0_16 = hn::BitCast(int16_tag, tmp_0);
  const auto tmp_1_16 = hn::BitCast(int16_tag, tmp_1);
  const auto tmp_2_16 = hn::BitCast(int16_tag, tmp_2);
  const auto tmp_3_16 = hn::BitCast(int16_tag, tmp_3);
  const auto tmp_4_16 = hn::BitCast(int16_tag, tmp_4);
  const auto tmp_5_16 = hn::BitCast(int16_tag, tmp_5);
  const auto tmp_6_16 = hn::BitCast(int16_tag, tmp_6);
  const auto tmp_7_16 = hn::BitCast(int16_tag, tmp_7);

  const auto tmp_12 = hn::ZipLower(int32_tag, tmp_0_16, tmp_2_16);
  const auto tmp_13 = hn::ZipLower(int32_tag, tmp_1_16, tmp_3_16);
  const auto tmp_14 = hn::ZipLower(int32_tag, tmp_4_16, tmp_6_16);
  const auto tmp_15 = hn::ZipLower(int32_tag, tmp_5_16, tmp_7_16);

  const auto res_0 = hn::ZipLower(int64_tag, tmp_12, tmp_14);
  const auto res_1 = hn::ZipUpper(int64_tag, tmp_12, tmp_14);
  const auto res_2 = hn::ZipLower(int64_tag, tmp_13, tmp_15);
  const auto res_3 = hn::ZipUpper(int64_tag, tmp_13, tmp_15);

  hn::Store(hn::BitCast(int8_tag, hn::InterleaveLower(int64_tag, res_0, res_2)),
            int8_tag, coeff + hn::MaxLanes(coeff_tag) * 0);
  hn::Store(hn::BitCast(int8_tag, hn::InterleaveUpper(int64_tag, res_0, res_2)),
            int8_tag, coeff + hn::MaxLanes(coeff_tag) * 1);
  hn::Store(hn::BitCast(int8_tag, hn::InterleaveLower(int64_tag, res_1, res_3)),
            int8_tag, coeff + hn::MaxLanes(coeff_tag) * 2);
  hn::Store(hn::BitCast(int8_tag, hn::InterleaveUpper(int64_tag, res_1, res_3)),
            int8_tag, coeff + hn::MaxLanes(coeff_tag) * 3);
}

template <typename D>
HWY_ATTR inline void PrepareHorizontalFilterCoefficientsBeta0(
    D int16_tag, int alpha, int beta, int sx, int8_t *HWY_RESTRICT coeff) {
  (void)int16_tag;
  (void)beta;
  const auto tmp_0 =
      hn::BitCast(int16x8_tag, LoadAV1Filter8Bit(sx + 0 * alpha));
  const auto tmp_1 =
      hn::BitCast(int16x8_tag, LoadAV1Filter8Bit(sx + 1 * alpha));
  const auto tmp_2 =
      hn::BitCast(int16x8_tag, LoadAV1Filter8Bit(sx + 2 * alpha));
  const auto tmp_3 =
      hn::BitCast(int16x8_tag, LoadAV1Filter8Bit(sx + 3 * alpha));
  const auto tmp_4 =
      hn::BitCast(int16x8_tag, LoadAV1Filter8Bit(sx + 4 * alpha));
  const auto tmp_5 =
      hn::BitCast(int16x8_tag, LoadAV1Filter8Bit(sx + 5 * alpha));
  const auto tmp_6 =
      hn::BitCast(int16x8_tag, LoadAV1Filter8Bit(sx + 6 * alpha));
  const auto tmp_7 =
      hn::BitCast(int16x8_tag, LoadAV1Filter8Bit(sx + 7 * alpha));

  const auto tmp_02 = hn::ZipLower(int32x4_tag, tmp_0, tmp_2);
  const auto tmp_13 = hn::ZipLower(int32x4_tag, tmp_1, tmp_3);
  const auto tmp_46 = hn::ZipLower(int32x4_tag, tmp_4, tmp_6);
  const auto tmp_57 = hn::ZipLower(int32x4_tag, tmp_5, tmp_7);

  constexpr auto int8_tag = hn::Repartition<int8_t, D>();
  constexpr auto int32_tag = hn::Repartition<int32_t, D>();
  constexpr auto int64_tag = hn::Repartition<int64_t, D>();

  const auto broadcast_12 =
      hn::BroadcastBlock<0>(hn::ResizeBitCast(int32_tag, tmp_02));
  const auto broadcast_13 =
      hn::BroadcastBlock<0>(hn::ResizeBitCast(int32_tag, tmp_13));
  const auto broadcast_14 =
      hn::BroadcastBlock<0>(hn::ResizeBitCast(int32_tag, tmp_46));
  const auto broadcast_15 =
      hn::BroadcastBlock<0>(hn::ResizeBitCast(int32_tag, tmp_57));

  const auto res_0 = hn::ZipLower(int64_tag, broadcast_12, broadcast_14);
  const auto res_1 = hn::ZipUpper(int64_tag, broadcast_12, broadcast_14);
  const auto res_2 = hn::ZipLower(int64_tag, broadcast_13, broadcast_15);
  const auto res_3 = hn::ZipUpper(int64_tag, broadcast_13, broadcast_15);

  hn::Store(hn::BitCast(int8_tag, hn::InterleaveLower(int64_tag, res_0, res_2)),
            int8_tag, coeff + hn::MaxLanes(coeff_tag) * 0);
  hn::Store(hn::BitCast(int8_tag, hn::InterleaveUpper(int64_tag, res_0, res_2)),
            int8_tag, coeff + hn::MaxLanes(coeff_tag) * 1);
  hn::Store(hn::BitCast(int8_tag, hn::InterleaveLower(int64_tag, res_1, res_3)),
            int8_tag, coeff + hn::MaxLanes(coeff_tag) * 2);
  hn::Store(hn::BitCast(int8_tag, hn::InterleaveUpper(int64_tag, res_1, res_3)),
            int8_tag, coeff + hn::MaxLanes(coeff_tag) * 3);
}

template <typename D>
HWY_ATTR inline void PrepareHorizontalFilterCoefficientsAlpha0(
    D int16_tag, int alpha, int beta, int sx, int8_t *HWY_RESTRICT coeff) {
  (void)alpha;
  constexpr auto int8_tag = hn::Repartition<int8_t, D>();
  auto tmp_0 = LoadAV1Filter8BitLower(int8_tag, sx);
  if constexpr (int16_tag.MaxBlocks() >= 2) {
    tmp_0 = LoadAV1Filter8BitUpper<1>(int8_tag, sx + beta, tmp_0);
  }
  if constexpr (int16_tag.MaxBlocks() >= 3) {
    tmp_0 = LoadAV1Filter8BitUpper<2>(int8_tag, sx + beta * 2, tmp_0);
    tmp_0 = LoadAV1Filter8BitUpper<3>(int8_tag, sx + beta * 3, tmp_0);
  }
  const auto res_0 = hn::BitCast(int16_tag, tmp_0);

  hn::Store(hn::BitCast(int8_tag, hn::Broadcast<0>(res_0)), int8_tag,
            coeff + hn::MaxLanes(coeff_tag) * 0);
  hn::Store(hn::BitCast(int8_tag, hn::Broadcast<1>(res_0)), int8_tag,
            coeff + hn::MaxLanes(coeff_tag) * 1);
  hn::Store(hn::BitCast(int8_tag, hn::Broadcast<2>(res_0)), int8_tag,
            coeff + hn::MaxLanes(coeff_tag) * 2);
  hn::Store(hn::BitCast(int8_tag, hn::Broadcast<3>(res_0)), int8_tag,
            coeff + hn::MaxLanes(coeff_tag) * 3);
}

template <typename D>
HWY_ATTR inline void HorizontalFilter(D tag, const hn::VFromD<D> src,
                                      int16_t *HWY_RESTRICT horz_out, int sx,
                                      int alpha, int beta, int row,
                                      const IVec16 round_const,
                                      const int reduce_bits_horiz) {
  HWY_ALIGN int8_t coeff[4 * hn::MaxLanes(coeff_tag)];
  PrepareHorizontalFilterCoefficients(hn::Repartition<int16_t, D>(), alpha,
                                      beta, sx, coeff);
  FilterPixelsHorizontal(tag, src, horz_out, coeff, round_const,
                         reduce_bits_horiz, row);
}

template <typename D>
HWY_ATTR inline void PrepareLastHorizontalFilterCoefficients(
    D int16_tag, int alpha, int beta, int sx, int8_t *HWY_RESTRICT coeff) {
  (void)int16_tag;
  (void)beta;
  const auto tmp_0 =
      hn::BitCast(int16x8_tag, LoadAV1Filter8Bit(sx + 0 * alpha));
  const auto tmp_1 =
      hn::BitCast(int16x8_tag, LoadAV1Filter8Bit(sx + 1 * alpha));
  const auto tmp_2 =
      hn::BitCast(int16x8_tag, LoadAV1Filter8Bit(sx + 2 * alpha));
  const auto tmp_3 =
      hn::BitCast(int16x8_tag, LoadAV1Filter8Bit(sx + 3 * alpha));
  const auto tmp_4 =
      hn::BitCast(int16x8_tag, LoadAV1Filter8Bit(sx + 4 * alpha));
  const auto tmp_5 =
      hn::BitCast(int16x8_tag, LoadAV1Filter8Bit(sx + 5 * alpha));
  const auto tmp_6 =
      hn::BitCast(int16x8_tag, LoadAV1Filter8Bit(sx + 6 * alpha));
  const auto tmp_7 =
      hn::BitCast(int16x8_tag, LoadAV1Filter8Bit(sx + 7 * alpha));

  const auto tmp_8 = hn::ZipLower(int32x4_tag, tmp_0, tmp_2);
  const auto tmp_9 = hn::ZipLower(int32x4_tag, tmp_1, tmp_3);
  const auto tmp_10 = hn::ZipLower(int32x4_tag, tmp_4, tmp_6);
  const auto tmp_11 = hn::ZipLower(int32x4_tag, tmp_5, tmp_7);

  const auto tmp_12 = hn::ZipLower(int64x2_tag, tmp_8, tmp_10);
  const auto tmp_13 = hn::ZipUpper(int64x2_tag, tmp_8, tmp_10);
  const auto tmp_14 = hn::ZipLower(int64x2_tag, tmp_9, tmp_11);
  const auto tmp_15 = hn::ZipUpper(int64x2_tag, tmp_9, tmp_11);

  const auto tmp_16 = hn::InterleaveLower(int64x2_tag, tmp_12, tmp_14);
  const auto tmp_17 = hn::InterleaveUpper(int64x2_tag, tmp_12, tmp_14);
  const auto tmp_18 = hn::InterleaveLower(int64x2_tag, tmp_13, tmp_15);
  const auto tmp_19 = hn::InterleaveUpper(int64x2_tag, tmp_13, tmp_15);

  constexpr auto int8_tag = hn::Repartition<int8_t, D>();

  const auto tmp_20 = hn::ResizeBitCast(int8_tag, tmp_16);
  const auto tmp_21 = hn::ResizeBitCast(int8_tag, tmp_17);
  const auto tmp_22 = hn::ResizeBitCast(int8_tag, tmp_18);
  const auto tmp_23 = hn::ResizeBitCast(int8_tag, tmp_19);

  hn::Store(hn::BroadcastBlock<0>(tmp_20), int8_tag,
            coeff + hn::MaxLanes(coeff_tag) * 0);
  hn::Store(hn::BroadcastBlock<0>(tmp_21), int8_tag,
            coeff + hn::MaxLanes(coeff_tag) * 1);
  hn::Store(hn::BroadcastBlock<0>(tmp_22), int8_tag,
            coeff + hn::MaxLanes(coeff_tag) * 2);
  hn::Store(hn::BroadcastBlock<0>(tmp_23), int8_tag,
            coeff + hn::MaxLanes(coeff_tag) * 3);
}

template <typename D>
HWY_ATTR HWY_INLINE hn::VFromD<D> LoadRowsClamped(
    D tag, const uint8_t *HWY_RESTRICT ref, const int stride, const int iy,
    const int height) {
  constexpr hn::BlockDFromD<D> block_tag;
  const int iy0 = clamp(iy + 0, 0, height - 1);
  auto src = hn::ResizeBitCast(tag, hn::LoadU(block_tag, ref + iy0 * stride));
  if constexpr (tag.MaxBlocks() >= 2) {
    const int iy1 = clamp(iy + 1, 0, height - 1);
    const auto src_1 = hn::LoadU(block_tag, ref + iy1 * stride);
    src = hn::InsertBlock<1>(src, src_1);
  }
  if constexpr (tag.MaxBlocks() >= 3) {
    const int iy2 = clamp(iy + 2, 0, height - 1);
    const auto src_2 = hn::LoadU(block_tag, ref + iy2 * stride);
    const int iy3 = clamp(iy + 3, 0, height - 1);
    const auto src_3 = hn::LoadU(block_tag, ref + iy3 * stride);
    src = hn::InsertBlock<2>(src, src_2);
    src = hn::InsertBlock<3>(src, src_3);
  }
  return src;
}

template <void (*PrepareCoeffs)(int alpha, int beta, int sx,
                                int8_t *HWY_RESTRICT coeffs),
          typename D>
HWY_ATTR int WarpHorizontalFilterLoop(
    D tag, const uint8_t *HWY_RESTRICT ref, int16_t *HWY_RESTRICT horz_out,
    int stride, int32_t ix4, int32_t iy4, int32_t sx4, int alpha, int beta,
    int p_height, int height, int i, const IVec16 round_const,
    const int reduce_bits_horiz, int k, int8_t *HWY_RESTRICT coeff) {
  constexpr int kNumRows = tag.MaxBlocks();
  for (; k < AOMMIN(8, p_height - i) - kNumRows; k += kNumRows) {
    const auto src =
        LoadRowsClamped(tag, ref + ix4 - 7, stride, iy4 + k, height);
    if constexpr (PrepareCoeffs != nullptr) {
      int sx = sx4 + beta * (k + 4);
      PrepareCoeffs(alpha, beta, sx, coeff);
    }
    FilterPixelsHorizontal(tag, src, horz_out, coeff, round_const,
                           reduce_bits_horiz, k + 7);
  }
  return k;
}

enum class HorizontalFilterCoeffs {
  kAlpha0,
  kBeta0,
  kDefault,
};

template <bool IsLast, HorizontalFilterCoeffs Filter, typename D>
HWY_ATTR void WarpHorizontalPrepareCoeffs(int alpha, int beta, int sx,
                                          int8_t *HWY_RESTRICT coeffs) {
  D int16_tag;
  switch (Filter) {
    case HorizontalFilterCoeffs::kAlpha0:
      PrepareHorizontalFilterCoefficientsAlpha0(int16_tag, alpha, beta, sx,
                                                coeffs);
      return;
    case HorizontalFilterCoeffs::kBeta0:
      PrepareHorizontalFilterCoefficientsBeta0(int16_tag, alpha, beta, sx,
                                               coeffs);
      return;
    case HorizontalFilterCoeffs::kDefault:
    default:
      if (IsLast) {
        PrepareLastHorizontalFilterCoefficients(int16_tag, alpha, beta, sx,
                                                coeffs);
      } else {
        PrepareHorizontalFilterCoefficients(int16_tag, alpha, beta, sx, coeffs);
      }
      return;
  }
}

template <bool InnerCoeffUpdate, HorizontalFilterCoeffs Filter>
HWY_ATTR inline void WarpHorizontalFilterTemplate(
    const uint8_t *HWY_RESTRICT ref, int16_t *HWY_RESTRICT horz_out, int stride,
    int32_t ix4, int32_t iy4, int32_t sx4, int alpha, int beta, int p_height,
    int height, int i, const IVec16 round_const, const int reduce_bits_horiz) {
  int k = -7, iy;
  HWY_ALIGN int8_t coeff[4 * hn::MaxLanes(coeff_tag)];
  if constexpr (!InnerCoeffUpdate) {
    WarpHorizontalPrepareCoeffs<false, Filter, decltype(int16xN_tag)>(
        alpha, beta, sx4, coeff);
  }
  if constexpr (uint8xN_tag.MaxBlocks() >= 3) {
    k = WarpHorizontalFilterLoop<(
        InnerCoeffUpdate
            ? WarpHorizontalPrepareCoeffs<false, Filter, decltype(int16xN_tag)>
            : nullptr)>(uint8xN_tag, ref, horz_out, stride, ix4, iy4, sx4,
                        alpha, beta, p_height, height, i, round_const,
                        reduce_bits_horiz, k, coeff);
  }
  if constexpr (uint8xN_tag.MaxBlocks() >= 2) {
    k = WarpHorizontalFilterLoop<(
        InnerCoeffUpdate
            ? WarpHorizontalPrepareCoeffs<false, Filter, decltype(int16x16_tag)>
            : nullptr)>(uint8x32_tag, ref, horz_out, stride, ix4, iy4, sx4,
                        alpha, beta, p_height, height, i, round_const,
                        reduce_bits_horiz, k, coeff);
  }
  if constexpr (uint8xN_tag.MaxBlocks() == 1) {
    k = WarpHorizontalFilterLoop<(
        InnerCoeffUpdate
            ? WarpHorizontalPrepareCoeffs<true, Filter, decltype(int16x8_tag)>
            : nullptr)>(uint8x16_tag, ref, horz_out, stride, ix4, iy4, sx4,
                        alpha, beta, p_height, height, i, round_const,
                        reduce_bits_horiz, k, coeff);
  }
  iy = iy4 + k;
  iy = clamp(iy, 0, height - 1);
  const auto src = hn::LoadU(uint8x16_tag, ref + iy * stride + ix4 - 7);
  if constexpr (InnerCoeffUpdate) {
    int sx = sx4 + beta * (k + 4);
    WarpHorizontalPrepareCoeffs<true, Filter, decltype(int16x8_tag)>(
        alpha, beta, sx, coeff);
  }
  FilterPixelsHorizontal(uint8x16_tag, src, horz_out, coeff, round_const,
                         reduce_bits_horiz, k + 7);
}

HWY_ATTR inline void UnpackWeightsAndSetRoundConst(
    ConvolveParams *HWY_RESTRICT conv_params, const int round_bits,
    const int offset_bits, IVec16 &HWY_RESTRICT res_sub_const,
    IVec16 &HWY_RESTRICT round_bits_const, IVec16 &HWY_RESTRICT wt) {
  res_sub_const =
      hn::Set(int16xN_tag, -(1 << (offset_bits - conv_params->round_1)) -
                               (1 << (offset_bits - conv_params->round_1 - 1)));
  round_bits_const = hn::Set(int16xN_tag, ((1 << round_bits) >> 1));

  const auto w0 = static_cast<int16_t>(conv_params->fwd_offset);
  const auto w1 = static_cast<int16_t>(conv_params->bck_offset);
  const auto wt0 = hn::Set(int16xN_tag, w0);
  const auto wt1 = hn::Set(int16xN_tag, w1);
  wt = hn::InterleaveLower(wt0, wt1);
}

HWY_ATTR HWY_INLINE IVec16 LoadAV1WarpedFilter(size_t offset) {
  return hn::LoadN(int16xN_tag,
                   av1_warped_filter[offset >> WARPEDDIFF_PREC_BITS], 8);
}

HWY_ATTR HWY_INLINE IVec16 LoadAV1WarpedFilterLower(size_t offset) {
  return hn::ResizeBitCast(
      int16xN_tag,
      hn::Load(int16x8_tag, av1_warped_filter[offset >> WARPEDDIFF_PREC_BITS]));
}

template <int Block>
HWY_ATTR HWY_INLINE IVec16 LoadAV1WarpedFilterUpper(size_t offset, IVec16 src) {
  return hn::InsertBlock<Block>(
      src,
      hn::Load(int16x8_tag, av1_warped_filter[offset >> WARPEDDIFF_PREC_BITS]));
}

HWY_ATTR inline void PrepareVerticalFilterCoeffs(int gamma, int delta, int sy,
                                                 int16_t *HWY_RESTRICT coeffs) {
  auto filt_00 = LoadAV1WarpedFilterLower(sy + 0 * gamma);
  auto filt_01 = LoadAV1WarpedFilterLower(sy + 2 * gamma);
  auto filt_02 = LoadAV1WarpedFilterLower(sy + 4 * gamma);
  auto filt_03 = LoadAV1WarpedFilterLower(sy + 6 * gamma);

  if constexpr (int16xN_tag.MaxBlocks() >= 2) {
    filt_00 = LoadAV1WarpedFilterUpper<1>(sy + delta + 0 * gamma, filt_00);
    filt_01 = LoadAV1WarpedFilterUpper<1>(sy + delta + 2 * gamma, filt_01);
    filt_02 = LoadAV1WarpedFilterUpper<1>(sy + delta + 4 * gamma, filt_02);
    filt_03 = LoadAV1WarpedFilterUpper<1>(sy + delta + 6 * gamma, filt_03);
  }

  if constexpr (int16xN_tag.MaxBlocks() >= 3) {
    filt_00 = LoadAV1WarpedFilterUpper<2>(sy + 2 * delta + 0 * gamma, filt_00);
    filt_01 = LoadAV1WarpedFilterUpper<2>(sy + 2 * delta + 2 * gamma, filt_01);
    filt_02 = LoadAV1WarpedFilterUpper<2>(sy + 2 * delta + 4 * gamma, filt_02);
    filt_03 = LoadAV1WarpedFilterUpper<2>(sy + 2 * delta + 6 * gamma, filt_03);

    filt_00 = LoadAV1WarpedFilterUpper<3>(sy + 3 * delta + 0 * gamma, filt_00);
    filt_01 = LoadAV1WarpedFilterUpper<3>(sy + 3 * delta + 2 * gamma, filt_01);
    filt_02 = LoadAV1WarpedFilterUpper<3>(sy + 3 * delta + 4 * gamma, filt_02);
    filt_03 = LoadAV1WarpedFilterUpper<3>(sy + 3 * delta + 6 * gamma, filt_03);
  }

  auto filt_0 = hn::BitCast(int32xN_tag, filt_00);
  auto filt_1 = hn::BitCast(int32xN_tag, filt_01);
  auto filt_2 = hn::BitCast(int32xN_tag, filt_02);
  auto filt_3 = hn::BitCast(int32xN_tag, filt_03);

  auto res_0 = hn::ZipLower(int64xN_tag, filt_0, filt_1);
  auto res_1 = hn::ZipLower(int64xN_tag, filt_2, filt_3);
  auto res_2 = hn::ZipUpper(int64xN_tag, filt_0, filt_1);
  auto res_3 = hn::ZipUpper(int64xN_tag, filt_2, filt_3);

  hn::Store(
      hn::BitCast(int16xN_tag, hn::InterleaveLower(int64xN_tag, res_0, res_1)),
      int16xN_tag, coeffs + 0 * hn::MaxLanes(int16xN_tag));
  hn::Store(
      hn::BitCast(int16xN_tag, hn::InterleaveUpper(int64xN_tag, res_0, res_1)),
      int16xN_tag, coeffs + 1 * hn::MaxLanes(int16xN_tag));
  hn::Store(
      hn::BitCast(int16xN_tag, hn::InterleaveLower(int64xN_tag, res_2, res_3)),
      int16xN_tag, coeffs + 2 * hn::MaxLanes(int16xN_tag));
  hn::Store(
      hn::BitCast(int16xN_tag, hn::InterleaveUpper(int64xN_tag, res_2, res_3)),
      int16xN_tag, coeffs + 3 * hn::MaxLanes(int16xN_tag));

  filt_00 = LoadAV1WarpedFilterLower(sy + 1 * gamma);
  filt_01 = LoadAV1WarpedFilterLower(sy + 3 * gamma);
  filt_02 = LoadAV1WarpedFilterLower(sy + 5 * gamma);
  filt_03 = LoadAV1WarpedFilterLower(sy + 7 * gamma);

  if constexpr (int16xN_tag.MaxBlocks() >= 2) {
    filt_00 = LoadAV1WarpedFilterUpper<1>(sy + delta + 1 * gamma, filt_00);
    filt_01 = LoadAV1WarpedFilterUpper<1>(sy + delta + 3 * gamma, filt_01);
    filt_02 = LoadAV1WarpedFilterUpper<1>(sy + delta + 5 * gamma, filt_02);
    filt_03 = LoadAV1WarpedFilterUpper<1>(sy + delta + 7 * gamma, filt_03);
  }

  if constexpr (int16xN_tag.MaxBlocks() >= 3) {
    filt_00 = LoadAV1WarpedFilterUpper<2>(sy + 2 * delta + 1 * gamma, filt_00);
    filt_01 = LoadAV1WarpedFilterUpper<2>(sy + 2 * delta + 3 * gamma, filt_01);
    filt_02 = LoadAV1WarpedFilterUpper<2>(sy + 2 * delta + 5 * gamma, filt_02);
    filt_03 = LoadAV1WarpedFilterUpper<2>(sy + 2 * delta + 7 * gamma, filt_03);

    filt_00 = LoadAV1WarpedFilterUpper<3>(sy + 3 * delta + 1 * gamma, filt_00);
    filt_01 = LoadAV1WarpedFilterUpper<3>(sy + 3 * delta + 3 * gamma, filt_01);
    filt_02 = LoadAV1WarpedFilterUpper<3>(sy + 3 * delta + 5 * gamma, filt_02);
    filt_03 = LoadAV1WarpedFilterUpper<3>(sy + 3 * delta + 7 * gamma, filt_03);
  }

  filt_0 = hn::BitCast(int32xN_tag, filt_00);
  filt_1 = hn::BitCast(int32xN_tag, filt_01);
  filt_2 = hn::BitCast(int32xN_tag, filt_02);
  filt_3 = hn::BitCast(int32xN_tag, filt_03);

  res_0 = hn::ZipLower(int64xN_tag, filt_0, filt_1);
  res_1 = hn::ZipLower(int64xN_tag, filt_2, filt_3);
  res_2 = hn::ZipUpper(int64xN_tag, filt_0, filt_1);
  res_3 = hn::ZipUpper(int64xN_tag, filt_2, filt_3);

  hn::Store(
      hn::BitCast(int16xN_tag, hn::InterleaveLower(int64xN_tag, res_0, res_1)),
      int16xN_tag, coeffs + 4 * hn::MaxLanes(int16xN_tag));
  hn::Store(
      hn::BitCast(int16xN_tag, hn::InterleaveUpper(int64xN_tag, res_0, res_1)),
      int16xN_tag, coeffs + 5 * hn::MaxLanes(int16xN_tag));
  hn::Store(
      hn::BitCast(int16xN_tag, hn::InterleaveLower(int64xN_tag, res_2, res_3)),
      int16xN_tag, coeffs + 6 * hn::MaxLanes(int16xN_tag));
  hn::Store(
      hn::BitCast(int16xN_tag, hn::InterleaveUpper(int64xN_tag, res_2, res_3)),
      int16xN_tag, coeffs + 7 * hn::MaxLanes(int16xN_tag));
}

HWY_ATTR inline void PrepareVerticalFilterCoeffsDelta0(
    int gamma, int delta, int sy, int16_t *HWY_RESTRICT coeffs) {
  (void)delta;
  auto filt_00 = LoadAV1WarpedFilter(sy + 0 * gamma);
  auto filt_01 = LoadAV1WarpedFilter(sy + 2 * gamma);
  auto filt_02 = LoadAV1WarpedFilter(sy + 4 * gamma);
  auto filt_03 = LoadAV1WarpedFilter(sy + 6 * gamma);

  auto filt_10 = hn::BitCast(int32xN_tag, hn::BroadcastBlock<0>(filt_00));
  auto filt_11 = hn::BitCast(int32xN_tag, hn::BroadcastBlock<0>(filt_01));
  auto filt_12 = hn::BitCast(int32xN_tag, hn::BroadcastBlock<0>(filt_02));
  auto filt_13 = hn::BitCast(int32xN_tag, hn::BroadcastBlock<0>(filt_03));

  auto res_0 = hn::ZipLower(int64xN_tag, filt_10, filt_11);
  auto res_1 = hn::ZipLower(int64xN_tag, filt_12, filt_13);
  auto res_2 = hn::ZipUpper(int64xN_tag, filt_10, filt_11);
  auto res_3 = hn::ZipUpper(int64xN_tag, filt_12, filt_13);

  hn::Store(
      hn::BitCast(int16xN_tag, hn::InterleaveLower(int64xN_tag, res_0, res_1)),
      int16xN_tag, coeffs + 0 * hn::MaxLanes(int16xN_tag));
  hn::Store(
      hn::BitCast(int16xN_tag, hn::InterleaveUpper(int64xN_tag, res_0, res_1)),
      int16xN_tag, coeffs + 1 * hn::MaxLanes(int16xN_tag));
  hn::Store(
      hn::BitCast(int16xN_tag, hn::InterleaveLower(int64xN_tag, res_2, res_3)),
      int16xN_tag, coeffs + 2 * hn::MaxLanes(int16xN_tag));
  hn::Store(
      hn::BitCast(int16xN_tag, hn::InterleaveUpper(int64xN_tag, res_2, res_3)),
      int16xN_tag, coeffs + 3 * hn::MaxLanes(int16xN_tag));

  filt_00 = LoadAV1WarpedFilter(sy + 1 * gamma);
  filt_01 = LoadAV1WarpedFilter(sy + 3 * gamma);
  filt_02 = LoadAV1WarpedFilter(sy + 5 * gamma);
  filt_03 = LoadAV1WarpedFilter(sy + 7 * gamma);

  filt_10 = hn::BitCast(int32xN_tag, hn::BroadcastBlock<0>(filt_00));
  filt_11 = hn::BitCast(int32xN_tag, hn::BroadcastBlock<0>(filt_01));
  filt_12 = hn::BitCast(int32xN_tag, hn::BroadcastBlock<0>(filt_02));
  filt_13 = hn::BitCast(int32xN_tag, hn::BroadcastBlock<0>(filt_03));

  res_0 = hn::ZipLower(int64xN_tag, filt_10, filt_11);
  res_1 = hn::ZipLower(int64xN_tag, filt_12, filt_13);
  res_2 = hn::ZipUpper(int64xN_tag, filt_10, filt_11);
  res_3 = hn::ZipUpper(int64xN_tag, filt_12, filt_13);

  hn::Store(
      hn::BitCast(int16xN_tag, hn::InterleaveLower(int64xN_tag, res_0, res_1)),
      int16xN_tag, coeffs + 4 * hn::MaxLanes(int16xN_tag));
  hn::Store(
      hn::BitCast(int16xN_tag, hn::InterleaveUpper(int64xN_tag, res_0, res_1)),
      int16xN_tag, coeffs + 5 * hn::MaxLanes(int16xN_tag));
  hn::Store(
      hn::BitCast(int16xN_tag, hn::InterleaveLower(int64xN_tag, res_2, res_3)),
      int16xN_tag, coeffs + 6 * hn::MaxLanes(int16xN_tag));
  hn::Store(
      hn::BitCast(int16xN_tag, hn::InterleaveUpper(int64xN_tag, res_2, res_3)),
      int16xN_tag, coeffs + 7 * hn::MaxLanes(int16xN_tag));
}

HWY_ATTR inline void PrepareVerticalFilterCoeffsGamma0(
    int gamma, int delta, int sy, int16_t *HWY_RESTRICT coeffs) {
  (void)gamma;
  auto filt_0 = LoadAV1WarpedFilterLower(sy);
  if constexpr (int16xN_tag.MaxBlocks() >= 2) {
    filt_0 = LoadAV1WarpedFilterUpper<1>(sy + delta, filt_0);
  }
  if constexpr (int16xN_tag.MaxBlocks() >= 3) {
    filt_0 = LoadAV1WarpedFilterUpper<2>(sy + 2 * delta, filt_0);
    filt_0 = LoadAV1WarpedFilterUpper<3>(sy + 3 * delta, filt_0);
  }
  auto res_0 = hn::BitCast(int32xN_tag, filt_0);

  auto broadcast_0 = hn::BitCast(int16xN_tag, hn::Broadcast<0>(res_0));
  auto broadcast_1 = hn::BitCast(int16xN_tag, hn::Broadcast<1>(res_0));
  auto broadcast_2 = hn::BitCast(int16xN_tag, hn::Broadcast<2>(res_0));
  auto broadcast_3 = hn::BitCast(int16xN_tag, hn::Broadcast<3>(res_0));

  hn::Store(broadcast_0, int16xN_tag, coeffs + 0 * hn::MaxLanes(int16xN_tag));
  hn::Store(broadcast_1, int16xN_tag, coeffs + 1 * hn::MaxLanes(int16xN_tag));
  hn::Store(broadcast_2, int16xN_tag, coeffs + 2 * hn::MaxLanes(int16xN_tag));
  hn::Store(broadcast_3, int16xN_tag, coeffs + 3 * hn::MaxLanes(int16xN_tag));
  hn::Store(broadcast_0, int16xN_tag, coeffs + 4 * hn::MaxLanes(int16xN_tag));
  hn::Store(broadcast_1, int16xN_tag, coeffs + 5 * hn::MaxLanes(int16xN_tag));
  hn::Store(broadcast_2, int16xN_tag, coeffs + 6 * hn::MaxLanes(int16xN_tag));
  hn::Store(broadcast_3, int16xN_tag, coeffs + 7 * hn::MaxLanes(int16xN_tag));
}

HWY_ATTR inline void FilterPixelsVertical(
    int16_t *HWY_RESTRICT horz_out, int16_t *HWY_RESTRICT src_lo,
    int16_t *HWY_RESTRICT src_hi, int16_t *HWY_RESTRICT coeffs,
    IVec32 &HWY_RESTRICT res_lo, IVec32 &HWY_RESTRICT res_hi, int row) {
  if constexpr (int16xN_tag.MaxBlocks() >= 3) {
    const auto horz_out_4 =
        hn::Load(int16xN_tag, horz_out + (row + 4) * hn::MaxLanes(int16x8_tag));
    const auto horz_out_5 = hn::LoadU(
        int16xN_tag, horz_out + (row + 5) * hn::MaxLanes(int16x8_tag));
    const auto horz_out_6 = hn::LoadU(
        int16xN_tag, horz_out + (row + 6) * hn::MaxLanes(int16x8_tag));
    const auto horz_out_7 = hn::LoadU(
        int16xN_tag, horz_out + (row + 7) * hn::MaxLanes(int16x8_tag));
    const auto src_lo_2 =
        hn::InterleaveLower(int16xN_tag, horz_out_4, horz_out_5);
    const auto src_hi_2 =
        hn::InterleaveUpper(int16xN_tag, horz_out_4, horz_out_5);
    const auto src_lo_3 =
        hn::InterleaveLower(int16xN_tag, horz_out_6, horz_out_7);
    const auto src_hi_3 =
        hn::InterleaveUpper(int16xN_tag, horz_out_6, horz_out_7);
    hn::Store(src_lo_2, int16xN_tag, src_lo + 2 * hn::MaxLanes(int16xN_tag));
    hn::Store(src_hi_2, int16xN_tag, src_hi + 2 * hn::MaxLanes(int16xN_tag));
    hn::Store(src_lo_3, int16xN_tag, src_lo + 3 * hn::MaxLanes(int16xN_tag));
    hn::Store(src_hi_3, int16xN_tag, src_hi + 3 * hn::MaxLanes(int16xN_tag));
  } else if constexpr (int16xN_tag.MaxBlocks() == 2) {
    const auto horz_out_6 =
        hn::Load(int16xN_tag, horz_out + (row + 6) * hn::MaxLanes(int16x8_tag));
    const auto horz_out_8 =
        hn::Load(int16xN_tag, horz_out + (row + 8) * hn::MaxLanes(int16x8_tag));
    const auto horz_out_7 =
        hn::ConcatLowerUpper(int16xN_tag, horz_out_8, horz_out_6);
    const auto src_lo_3 =
        hn::InterleaveLower(int16xN_tag, horz_out_6, horz_out_7);
    const auto src_hi_3 =
        hn::InterleaveUpper(int16xN_tag, horz_out_6, horz_out_7);
    hn::Store(src_lo_3, int16xN_tag, src_lo + 3 * hn::MaxLanes(int16xN_tag));
    hn::Store(src_hi_3, int16xN_tag, src_hi + 3 * hn::MaxLanes(int16xN_tag));
  } else if constexpr (int16xN_tag.MaxBlocks() == 1) {
    const auto horz_out_6 =
        hn::Load(int16x8_tag, horz_out + (row + 6) * hn::MaxLanes(int16x8_tag));
    const auto horz_out_7 =
        hn::Load(int16x8_tag, horz_out + (row + 7) * hn::MaxLanes(int16x8_tag));
    const auto src_lo_3 =
        hn::InterleaveLower(int16x8_tag, horz_out_6, horz_out_7);
    const auto src_hi_3 =
        hn::InterleaveUpper(int16x8_tag, horz_out_6, horz_out_7);
    hn::Store(src_lo_3, int16x8_tag, src_lo + 3 * hn::MaxLanes(int16x8_tag));
    hn::Store(src_hi_3, int16x8_tag, src_hi + 3 * hn::MaxLanes(int16x8_tag));
  }

  const auto coeff_0 =
      hn::Load(int16xN_tag, coeffs + 0 * hn::MaxLanes(int16xN_tag));
  const auto coeff_1 =
      hn::Load(int16xN_tag, coeffs + 1 * hn::MaxLanes(int16xN_tag));
  const auto coeff_2 =
      hn::Load(int16xN_tag, coeffs + 2 * hn::MaxLanes(int16xN_tag));
  const auto coeff_3 =
      hn::Load(int16xN_tag, coeffs + 3 * hn::MaxLanes(int16xN_tag));
  const auto coeff_4 =
      hn::Load(int16xN_tag, coeffs + 4 * hn::MaxLanes(int16xN_tag));
  const auto coeff_5 =
      hn::Load(int16xN_tag, coeffs + 5 * hn::MaxLanes(int16xN_tag));
  const auto coeff_6 =
      hn::Load(int16xN_tag, coeffs + 6 * hn::MaxLanes(int16xN_tag));
  const auto coeff_7 =
      hn::Load(int16xN_tag, coeffs + 7 * hn::MaxLanes(int16xN_tag));

  const auto src_lo_0 =
      hn::Load(int16xN_tag, src_lo + 0 * hn::MaxLanes(int16xN_tag));
  const auto src_lo_1 =
      hn::Load(int16xN_tag, src_lo + 1 * hn::MaxLanes(int16xN_tag));
  const auto src_lo_2 =
      hn::Load(int16xN_tag, src_lo + 2 * hn::MaxLanes(int16xN_tag));
  const auto src_lo_3 =
      hn::Load(int16xN_tag, src_lo + 3 * hn::MaxLanes(int16xN_tag));
  const auto src_hi_0 =
      hn::Load(int16xN_tag, src_hi + 0 * hn::MaxLanes(int16xN_tag));
  const auto src_hi_1 =
      hn::Load(int16xN_tag, src_hi + 1 * hn::MaxLanes(int16xN_tag));
  const auto src_hi_2 =
      hn::Load(int16xN_tag, src_hi + 2 * hn::MaxLanes(int16xN_tag));
  const auto src_hi_3 =
      hn::Load(int16xN_tag, src_hi + 3 * hn::MaxLanes(int16xN_tag));

  auto even_sum0 = hn::Zero(int32xN_tag);
  auto even_sum1 = hn::Zero(int32xN_tag);
  even_sum0 = hn::ReorderWidenMulAccumulate(int32xN_tag, src_lo_0, coeff_0,
                                            even_sum0, even_sum1);
  even_sum0 = hn::ReorderWidenMulAccumulate(int32xN_tag, src_lo_1, coeff_1,
                                            even_sum0, even_sum1);
  even_sum0 = hn::ReorderWidenMulAccumulate(int32xN_tag, src_lo_2, coeff_2,
                                            even_sum0, even_sum1);
  even_sum0 = hn::ReorderWidenMulAccumulate(int32xN_tag, src_lo_3, coeff_3,
                                            even_sum0, even_sum1);
  auto res_even = hn::RearrangeToOddPlusEven(even_sum0, even_sum1);

  auto odd_sum0 = hn::Zero(int32xN_tag);
  auto odd_sum1 = hn::Zero(int32xN_tag);
  odd_sum0 = hn::ReorderWidenMulAccumulate(int32xN_tag, src_hi_0, coeff_4,
                                           odd_sum0, odd_sum1);
  odd_sum0 = hn::ReorderWidenMulAccumulate(int32xN_tag, src_hi_1, coeff_5,
                                           odd_sum0, odd_sum1);
  odd_sum0 = hn::ReorderWidenMulAccumulate(int32xN_tag, src_hi_2, coeff_6,
                                           odd_sum0, odd_sum1);
  odd_sum0 = hn::ReorderWidenMulAccumulate(int32xN_tag, src_hi_3, coeff_7,
                                           odd_sum0, odd_sum1);
  auto res_odd = hn::RearrangeToOddPlusEven(odd_sum0, odd_sum1);

  // Rearrange pixels back into the order 0 ... 7
  res_lo = hn::InterleaveLower(int32xN_tag, res_even, res_odd);
  res_hi = hn::InterleaveUpper(int32xN_tag, res_even, res_odd);
}

template <typename DS, typename DR, typename A, typename B, typename C>
HWY_ATTR HWY_INLINE void StoreRows(DS store_tag, DR row_tag, hn::VFromD<DR> vec,
                                   A stride, B y, C x,
                                   hn::TFromD<DS> *HWY_RESTRICT out) {
  hn::TFromD<DS> *HWY_RESTRICT pointers[row_tag.MaxBlocks()];
  for (int i = 0; i < static_cast<int>(row_tag.MaxBlocks()); ++i) {
    pointers[i] = &out[(y + i) * stride + x];
  }
  hn::Store(hn::ResizeBitCast(store_tag, hn::ExtractBlock<0>(vec)), store_tag,
            pointers[0]);
  if constexpr (row_tag.MaxBlocks() >= 2) {
    hn::Store(hn::ResizeBitCast(store_tag, hn::ExtractBlock<1>(vec)), store_tag,
              pointers[1]);
  }
  if constexpr (row_tag.MaxBlocks() >= 3) {
    hn::Store(hn::ResizeBitCast(store_tag, hn::ExtractBlock<2>(vec)), store_tag,
              pointers[2]);
    hn::Store(hn::ResizeBitCast(store_tag, hn::ExtractBlock<3>(vec)), store_tag,
              pointers[3]);
  }
}

HWY_ATTR HWY_INLINE void StoreVerticalFilterOutput(
    IVec32 res_lo, IVec32 res_hi, const IVec32 res_add_const, const IVec16 wt,
    const IVec16 res_sub_const, const IVec16 round_bits_const,
    uint8_t *HWY_RESTRICT pred, ConvolveParams *HWY_RESTRICT conv_params, int i,
    int j, int k, const int reduce_bits_vert, int p_stride, int p_width,
    const int round_bits) {
  constexpr int kNumRows = uint16xN_tag.MaxBlocks();
  if (conv_params->is_compound) {
    uint16_t *HWY_RESTRICT pointers[kNumRows];
    for (int row = 0; row < kNumRows; ++row) {
      pointers[row] =
          &conv_params->dst[(i + k + row) * conv_params->dst_stride + j];
    }

    res_lo =
        hn::ShiftRightSame(hn::Add(res_lo, res_add_const), reduce_bits_vert);

    const auto temp_lo_16 = hn::ReorderDemote2To(uint16xN_tag, res_lo, res_lo);
    if (conv_params->do_average) {
      auto p_16 =
          hn::ResizeBitCast(uint16xN_tag, hn::Load(uint16x4_tag, pointers[0]));
      if constexpr (kNumRows >= 2) {
        p_16 = hn::InsertBlock<1>(
            p_16, hn::ResizeBitCast(uint16x8_tag,
                                    hn::Load(uint16x4_tag, pointers[1])));
      }
      if constexpr (kNumRows >= 3) {
        p_16 = hn::InsertBlock<2>(
            p_16, hn::ResizeBitCast(uint16x8_tag,
                                    hn::Load(uint16x4_tag, pointers[2])));
        p_16 = hn::InsertBlock<3>(
            p_16, hn::ResizeBitCast(uint16x8_tag,
                                    hn::Load(uint16x4_tag, pointers[3])));
      }
      auto res_lo_16 = hn::Undefined(int16xN_tag);
      if (conv_params->use_dist_wtd_comp_avg) {
        const auto p_16_lo =
            hn::BitCast(int16xN_tag, hn::InterleaveLower(p_16, temp_lo_16));
        const auto wt_res_lo =
            hn::WidenMulPairwiseAdd(int32xN_tag, p_16_lo, wt);
        const auto shifted_32 = hn::ShiftRight<DIST_PRECISION_BITS>(wt_res_lo);
        res_lo_16 = hn::BitCast(
            int16xN_tag,
            hn::ReorderDemote2To(uint16xN_tag, shifted_32, shifted_32));
      } else {
        res_lo_16 = hn::ShiftRight<1>(
            hn::BitCast(int16xN_tag, hn::Add(p_16, temp_lo_16)));
      }
      res_lo_16 = hn::Add(res_lo_16, res_sub_const);
      res_lo_16 =
          hn::ShiftRightSame(hn::Add(res_lo_16, round_bits_const), round_bits);
      const auto res_8_lo =
          hn::ReorderDemote2To(uint8xN_tag, res_lo_16, res_lo_16);
      StoreRows(uint8x4_tag, uint8xN_tag, res_8_lo, p_stride, i + k, j, pred);
    } else {
      hn::Store(
          hn::ResizeBitCast(uint16x4_tag, hn::ExtractBlock<0>(temp_lo_16)),
          uint16x4_tag, pointers[0]);
      if constexpr (kNumRows >= 2) {
        hn::Store(
            hn::ResizeBitCast(uint16x4_tag, hn::ExtractBlock<1>(temp_lo_16)),
            uint16x4_tag, pointers[1]);
      }
      if constexpr (kNumRows >= 3) {
        hn::Store(
            hn::ResizeBitCast(uint16x4_tag, hn::ExtractBlock<2>(temp_lo_16)),
            uint16x4_tag, pointers[2]);
        hn::Store(
            hn::ResizeBitCast(uint16x4_tag, hn::ExtractBlock<3>(temp_lo_16)),
            uint16x4_tag, pointers[3]);
      }
    }
    if (p_width > 4) {
      uint16_t *HWY_RESTRICT pointers4[kNumRows];
      for (int row = 0; row < kNumRows; ++row) {
        pointers4[row] =
            &conv_params->dst[(i + k + row) * conv_params->dst_stride + j + 4];
      }
      res_hi =
          hn::ShiftRightSame(hn::Add(res_hi, res_add_const), reduce_bits_vert);
      const auto temp_hi_16 =
          hn::ReorderDemote2To(uint16xN_tag, res_hi, res_hi);
      if (conv_params->do_average) {
        auto p4_16 = hn::ResizeBitCast(uint16xN_tag,
                                       hn::Load(uint16x4_tag, pointers4[0]));
        if constexpr (kNumRows >= 2) {
          p4_16 = hn::InsertBlock<1>(
              p4_16, hn::ResizeBitCast(uint16x8_tag,
                                       hn::Load(uint16x4_tag, pointers4[1])));
        }
        if constexpr (kNumRows >= 3) {
          p4_16 = hn::InsertBlock<2>(
              p4_16, hn::ResizeBitCast(uint16x8_tag,
                                       hn::Load(uint16x4_tag, pointers4[2])));
          p4_16 = hn::InsertBlock<3>(
              p4_16, hn::ResizeBitCast(uint16x8_tag,
                                       hn::Load(uint16x4_tag, pointers4[3])));
        }

        auto res_hi_16 = hn::Undefined(int16xN_tag);
        if (conv_params->use_dist_wtd_comp_avg) {
          const auto p_16_hi =
              hn::BitCast(int16xN_tag, hn::InterleaveLower(p4_16, temp_hi_16));
          const auto wt_res_hi =
              hn::WidenMulPairwiseAdd(int32xN_tag, p_16_hi, wt);
          const auto shifted_32 =
              hn::ShiftRight<DIST_PRECISION_BITS>(wt_res_hi);
          res_hi_16 = hn::BitCast(
              int16xN_tag,
              hn::ReorderDemote2To(uint16xN_tag, shifted_32, shifted_32));
        } else {
          res_hi_16 = hn::ShiftRight<1>(
              hn::BitCast(int16xN_tag, hn::Add(p4_16, temp_hi_16)));
        }
        res_hi_16 = hn::Add(res_hi_16, res_sub_const);
        res_hi_16 = hn::ShiftRightSame(hn::Add(res_hi_16, round_bits_const),
                                       round_bits);
        const auto res_8_hi =
            hn::ReorderDemote2To(uint8xN_tag, res_hi_16, res_hi_16);
        StoreRows(uint8x4_tag, uint8xN_tag, res_8_hi, p_stride, i + k, j + 4,
                  pred);
      } else {
        hn::Store(hn::ResizeBitCast(uint16x4_tag, temp_hi_16), uint16x4_tag,
                  pointers4[0]);
        if constexpr (kNumRows >= 2) {
          hn::Store(
              hn::ResizeBitCast(uint16x4_tag, hn::ExtractBlock<1>(temp_hi_16)),
              uint16x4_tag, pointers4[1]);
        }
        if constexpr (kNumRows >= 3) {
          hn::Store(
              hn::ResizeBitCast(uint16x4_tag, hn::ExtractBlock<2>(temp_hi_16)),
              uint16x4_tag, pointers4[2]);
          hn::Store(
              hn::ResizeBitCast(uint16x4_tag, hn::ExtractBlock<3>(temp_hi_16)),
              uint16x4_tag, pointers4[3]);
        }
      }
    }
  } else {
    const auto res_lo_round =
        hn::ShiftRightSame(hn::Add(res_lo, res_add_const), reduce_bits_vert);
    const auto res_hi_round =
        hn::ShiftRightSame(hn::Add(res_hi, res_add_const), reduce_bits_vert);

    const auto res_16bit =
        hn::ReorderDemote2To(int16xN_tag, res_lo_round, res_hi_round);
    const auto res_8bit =
        hn::ReorderDemote2To(uint8xN_tag, res_16bit, res_16bit);
    // Store, blending with 'pred' if needed
    if (p_width == 4) {
      StoreRows(uint8x4_tag, uint8xN_tag, res_8bit, p_stride, i + k, j, pred);
    } else {
      StoreRows(uint8x8_tag, uint8xN_tag, res_8bit, p_stride, i + k, j, pred);
    }
  }
}

template <bool InnerCoeffUpdate,
          void (*PrepareCoeffs)(int gamma, int delta, int sy,
                                int16_t *HWY_RESTRICT coeffs)>
HWY_ATTR inline void WarpVerticalFilterTemplate(
    uint8_t *HWY_RESTRICT pred, int16_t *HWY_RESTRICT horz_out,
    ConvolveParams *HWY_RESTRICT conv_params, int16_t gamma, int16_t delta,
    int p_height, int p_stride, int p_width, int i, int j, int sy4,
    const int reduce_bits_vert, const IVec32 res_add_const,
    const int round_bits, const IVec16 res_sub_const,
    const IVec16 round_bits_const, const IVec16 wt) {
  HWY_ALIGN int16_t src_lo[4 * hn::MaxLanes(int16xN_tag)];
  HWY_ALIGN int16_t src_hi[4 * hn::MaxLanes(int16xN_tag)];
  if constexpr (int16xN_tag.MaxBlocks() >= 3) {
    const auto horz_out_0 =
        hn::Load(int16xN_tag, horz_out + 0 * hn::MaxLanes(int16x8_tag));
    const auto horz_out_1 =
        hn::LoadU(int16xN_tag, horz_out + 1 * hn::MaxLanes(int16x8_tag));
    const auto horz_out_2 =
        hn::LoadU(int16xN_tag, horz_out + 2 * hn::MaxLanes(int16x8_tag));
    const auto horz_out_3 =
        hn::LoadU(int16xN_tag, horz_out + 3 * hn::MaxLanes(int16x8_tag));
    hn::Store(hn::InterleaveLower(int16xN_tag, horz_out_0, horz_out_1),
              int16xN_tag, src_lo + 0 * hn::MaxLanes(int16xN_tag));
    hn::Store(hn::InterleaveUpper(int16xN_tag, horz_out_0, horz_out_1),
              int16xN_tag, src_hi + 0 * hn::MaxLanes(int16xN_tag));
    hn::Store(hn::InterleaveLower(int16xN_tag, horz_out_2, horz_out_3),
              int16xN_tag, src_lo + 1 * hn::MaxLanes(int16xN_tag));
    hn::Store(hn::InterleaveUpper(int16xN_tag, horz_out_2, horz_out_3),
              int16xN_tag, src_hi + 1 * hn::MaxLanes(int16xN_tag));
  } else if constexpr (int16xN_tag.MaxBlocks() == 2) {
    const auto horz_out_0 =
        hn::Load(int16xN_tag, horz_out + 0 * hn::MaxLanes(int16xN_tag));
    const auto horz_out_2 =
        hn::Load(int16xN_tag, horz_out + 1 * hn::MaxLanes(int16xN_tag));
    const auto horz_out_4 =
        hn::Load(int16xN_tag, horz_out + 2 * hn::MaxLanes(int16xN_tag));
    const auto horz_out_6 =
        hn::Load(int16xN_tag, horz_out + 3 * hn::MaxLanes(int16xN_tag));
    const auto horz_out_1 =
        hn::ConcatLowerUpper(int16xN_tag, horz_out_2, horz_out_0);
    const auto horz_out_3 =
        hn::ConcatLowerUpper(int16xN_tag, horz_out_4, horz_out_2);
    const auto horz_out_5 =
        hn::ConcatLowerUpper(int16xN_tag, horz_out_6, horz_out_4);
    hn::Store(hn::InterleaveLower(int16xN_tag, horz_out_0, horz_out_1),
              int16xN_tag, src_lo + 0 * hn::MaxLanes(int16xN_tag));
    hn::Store(hn::InterleaveUpper(int16xN_tag, horz_out_0, horz_out_1),
              int16xN_tag, src_hi + 0 * hn::MaxLanes(int16xN_tag));
    hn::Store(hn::InterleaveLower(int16xN_tag, horz_out_2, horz_out_3),
              int16xN_tag, src_lo + 1 * hn::MaxLanes(int16xN_tag));
    hn::Store(hn::InterleaveUpper(int16xN_tag, horz_out_2, horz_out_3),
              int16xN_tag, src_hi + 1 * hn::MaxLanes(int16xN_tag));
    hn::Store(hn::InterleaveLower(int16xN_tag, horz_out_4, horz_out_5),
              int16xN_tag, src_lo + 2 * hn::MaxLanes(int16xN_tag));
    hn::Store(hn::InterleaveUpper(int16xN_tag, horz_out_4, horz_out_5),
              int16xN_tag, src_hi + 2 * hn::MaxLanes(int16xN_tag));
  } else {
    const auto horz_out_0 =
        hn::Load(int16xN_tag, horz_out + 0 * hn::MaxLanes(int16xN_tag));
    const auto horz_out_1 =
        hn::Load(int16xN_tag, horz_out + 1 * hn::MaxLanes(int16xN_tag));
    const auto horz_out_2 =
        hn::Load(int16xN_tag, horz_out + 2 * hn::MaxLanes(int16xN_tag));
    const auto horz_out_3 =
        hn::Load(int16xN_tag, horz_out + 3 * hn::MaxLanes(int16xN_tag));
    const auto horz_out_4 =
        hn::Load(int16xN_tag, horz_out + 4 * hn::MaxLanes(int16xN_tag));
    const auto horz_out_5 =
        hn::Load(int16xN_tag, horz_out + 5 * hn::MaxLanes(int16xN_tag));
    hn::Store(hn::InterleaveLower(int16xN_tag, horz_out_0, horz_out_1),
              int16xN_tag, src_lo + 0 * hn::MaxLanes(int16xN_tag));
    hn::Store(hn::InterleaveUpper(int16xN_tag, horz_out_0, horz_out_1),
              int16xN_tag, src_hi + 0 * hn::MaxLanes(int16xN_tag));
    hn::Store(hn::InterleaveLower(int16xN_tag, horz_out_2, horz_out_3),
              int16xN_tag, src_lo + 1 * hn::MaxLanes(int16xN_tag));
    hn::Store(hn::InterleaveUpper(int16xN_tag, horz_out_2, horz_out_3),
              int16xN_tag, src_hi + 1 * hn::MaxLanes(int16xN_tag));
    hn::Store(hn::InterleaveLower(int16xN_tag, horz_out_4, horz_out_5),
              int16xN_tag, src_lo + 2 * hn::MaxLanes(int16xN_tag));
    hn::Store(hn::InterleaveUpper(int16xN_tag, horz_out_4, horz_out_5),
              int16xN_tag, src_hi + 2 * hn::MaxLanes(int16xN_tag));
  }

  HWY_ALIGN int16_t coeffs[8 * hn::MaxLanes(int16xN_tag)];
  if constexpr (!InnerCoeffUpdate) {
    PrepareCoeffs(gamma, delta, sy4, coeffs);
  }

  for (int k = -4; k < AOMMIN(4, p_height - i - 4);
       k += static_cast<int>(int16xN_tag.MaxBlocks())) {
    if constexpr (InnerCoeffUpdate) {
      int sy = sy4 + delta * (k + 4);
      PrepareCoeffs(gamma, delta, sy, coeffs);
    }

    IVec32 res_lo, res_hi;
    FilterPixelsVertical(horz_out, src_lo, src_hi, coeffs, res_lo, res_hi,
                         k + 4);
    StoreVerticalFilterOutput(res_lo, res_hi, res_add_const, wt, res_sub_const,
                              round_bits_const, pred, conv_params, i, j, k + 4,
                              reduce_bits_vert, p_stride, p_width, round_bits);

    if constexpr (int16xN_tag.MaxBlocks() >= 3) {
      hn::Store(hn::Load(int16xN_tag, src_lo + 2 * hn::MaxLanes(int16xN_tag)),
                int16xN_tag, src_lo + 0 * hn::MaxLanes(int16xN_tag));
      hn::Store(hn::Load(int16xN_tag, src_lo + 3 * hn::MaxLanes(int16xN_tag)),
                int16xN_tag, src_lo + 1 * hn::MaxLanes(int16xN_tag));
      hn::Store(hn::Load(int16xN_tag, src_hi + 2 * hn::MaxLanes(int16xN_tag)),
                int16xN_tag, src_hi + 0 * hn::MaxLanes(int16xN_tag));
      hn::Store(hn::Load(int16xN_tag, src_hi + 3 * hn::MaxLanes(int16xN_tag)),
                int16xN_tag, src_hi + 1 * hn::MaxLanes(int16xN_tag));
    } else if constexpr (int16xN_tag.MaxBlocks() == 2) {
      hn::Store(hn::Load(int16xN_tag, src_lo + 1 * hn::MaxLanes(int16xN_tag)),
                int16xN_tag, src_lo + 0 * hn::MaxLanes(int16xN_tag));
      hn::Store(hn::Load(int16xN_tag, src_lo + 2 * hn::MaxLanes(int16xN_tag)),
                int16xN_tag, src_lo + 1 * hn::MaxLanes(int16xN_tag));
      hn::Store(hn::Load(int16xN_tag, src_lo + 3 * hn::MaxLanes(int16xN_tag)),
                int16xN_tag, src_lo + 2 * hn::MaxLanes(int16xN_tag));
      hn::Store(hn::Load(int16xN_tag, src_hi + 1 * hn::MaxLanes(int16xN_tag)),
                int16xN_tag, src_hi + 0 * hn::MaxLanes(int16xN_tag));
      hn::Store(hn::Load(int16xN_tag, src_hi + 2 * hn::MaxLanes(int16xN_tag)),
                int16xN_tag, src_hi + 1 * hn::MaxLanes(int16xN_tag));
      hn::Store(hn::Load(int16xN_tag, src_hi + 3 * hn::MaxLanes(int16xN_tag)),
                int16xN_tag, src_hi + 2 * hn::MaxLanes(int16xN_tag));
    } else if constexpr (int16xN_tag.MaxBlocks() == 1) {
      const auto src_lo_0 =
          hn::Load(int16xN_tag, src_lo + 0 * hn::MaxLanes(int16xN_tag));
      const auto src_lo_1 =
          hn::Load(int16xN_tag, src_lo + 1 * hn::MaxLanes(int16xN_tag));
      const auto src_lo_2 =
          hn::Load(int16xN_tag, src_lo + 2 * hn::MaxLanes(int16xN_tag));
      const auto src_lo_3 =
          hn::Load(int16xN_tag, src_lo + 3 * hn::MaxLanes(int16xN_tag));
      const auto src_lo_0_new = hn::InterleaveEven(
          hn::ShiftRightLanes<1>(int16xN_tag, src_lo_0), src_lo_1);
      const auto src_lo_1_new = hn::InterleaveEven(
          hn::ShiftRightLanes<1>(int16xN_tag, src_lo_1), src_lo_2);
      const auto src_lo_2_new = hn::InterleaveEven(
          hn::ShiftRightLanes<1>(int16xN_tag, src_lo_2), src_lo_3);
      hn::Store(src_lo_0_new, int16xN_tag,
                src_lo + 0 * hn::MaxLanes(int16xN_tag));
      hn::Store(src_lo_1_new, int16xN_tag,
                src_lo + 1 * hn::MaxLanes(int16xN_tag));
      hn::Store(src_lo_2_new, int16xN_tag,
                src_lo + 2 * hn::MaxLanes(int16xN_tag));
      const auto src_hi_0 =
          hn::Load(int16xN_tag, src_hi + 0 * hn::MaxLanes(int16xN_tag));
      const auto src_hi_1 =
          hn::Load(int16xN_tag, src_hi + 1 * hn::MaxLanes(int16xN_tag));
      const auto src_hi_2 =
          hn::Load(int16xN_tag, src_hi + 2 * hn::MaxLanes(int16xN_tag));
      const auto src_hi_3 =
          hn::Load(int16xN_tag, src_hi + 3 * hn::MaxLanes(int16xN_tag));
      const auto src_hi_0_new = hn::InterleaveEven(
          hn::ShiftRightLanes<1>(int16xN_tag, src_hi_0), src_hi_1);
      const auto src_hi_1_new = hn::InterleaveEven(
          hn::ShiftRightLanes<1>(int16xN_tag, src_hi_1), src_hi_2);
      const auto src_hi_2_new = hn::InterleaveEven(
          hn::ShiftRightLanes<1>(int16xN_tag, src_hi_2), src_hi_3);
      hn::Store(src_hi_0_new, int16xN_tag,
                src_hi + 0 * hn::MaxLanes(int16xN_tag));
      hn::Store(src_hi_1_new, int16xN_tag,
                src_hi + 1 * hn::MaxLanes(int16xN_tag));
      hn::Store(src_hi_2_new, int16xN_tag,
                src_hi + 2 * hn::MaxLanes(int16xN_tag));
    }
  }
}

HWY_ATTR inline void PrepareWarpVerticalFilter(
    uint8_t *HWY_RESTRICT pred, int16_t *HWY_RESTRICT horz_out,
    ConvolveParams *HWY_RESTRICT conv_params, int16_t gamma, int16_t delta,
    int p_height, int p_stride, int p_width, int i, int j, int sy4,
    const int reduce_bits_vert, const IVec32 res_add_const,
    const int round_bits, const IVec16 res_sub_const,
    const IVec16 round_bits_const, const IVec16 wt) {
  if (gamma == 0 && delta == 0)
    WarpVerticalFilterTemplate<false, PrepareVerticalFilterCoeffsGamma0>(
        pred, horz_out, conv_params, gamma, delta, p_height, p_stride, p_width,
        i, j, sy4, reduce_bits_vert, res_add_const, round_bits, res_sub_const,
        round_bits_const, wt);
  else if (gamma == 0 && delta != 0)
    WarpVerticalFilterTemplate<true, PrepareVerticalFilterCoeffsGamma0>(
        pred, horz_out, conv_params, gamma, delta, p_height, p_stride, p_width,
        i, j, sy4, reduce_bits_vert, res_add_const, round_bits, res_sub_const,
        round_bits_const, wt);
  else if (gamma != 0 && delta == 0)
    WarpVerticalFilterTemplate<false, PrepareVerticalFilterCoeffsDelta0>(
        pred, horz_out, conv_params, gamma, delta, p_height, p_stride, p_width,
        i, j, sy4, reduce_bits_vert, res_add_const, round_bits, res_sub_const,
        round_bits_const, wt);
  else
    WarpVerticalFilterTemplate<true, PrepareVerticalFilterCoeffs>(
        pred, horz_out, conv_params, gamma, delta, p_height, p_stride, p_width,
        i, j, sy4, reduce_bits_vert, res_add_const, round_bits, res_sub_const,
        round_bits_const, wt);
}

HWY_ATTR inline void PrepareWarpHorizontalFilter(
    const uint8_t *HWY_RESTRICT ref, int16_t *HWY_RESTRICT horz_out, int stride,
    int32_t ix4, int32_t iy4, int32_t sx4, int alpha, int beta, int p_height,
    int height, int i, const IVec16 round_const, const int reduce_bits_horiz) {
  if (alpha == 0 && beta == 0)
    WarpHorizontalFilterTemplate<false, HorizontalFilterCoeffs::kAlpha0>(
        ref, horz_out, stride, ix4, iy4, sx4, alpha, beta, p_height, height, i,
        round_const, reduce_bits_horiz);
  else if (alpha == 0 && beta != 0)
    WarpHorizontalFilterTemplate<true, HorizontalFilterCoeffs::kAlpha0>(
        ref, horz_out, stride, ix4, iy4, sx4, alpha, beta, p_height, height, i,
        round_const, reduce_bits_horiz);
  else if (alpha != 0 && beta == 0)
    WarpHorizontalFilterTemplate<false, HorizontalFilterCoeffs::kBeta0>(
        ref, horz_out, stride, ix4, iy4, sx4, alpha, beta, p_height, height, i,
        round_const, reduce_bits_horiz);
  else
    WarpHorizontalFilterTemplate<true, HorizontalFilterCoeffs::kDefault>(
        ref, horz_out, stride, ix4, iy4, sx4, alpha, beta, p_height, height, i,
        round_const, reduce_bits_horiz);
}

template <typename D>
HWY_ATTR HWY_INLINE int WarpHorizontalFilterOutOfBoundsSetLoop(
    D tag, const uint8_t *HWY_RESTRICT ref, int height, int stride,
    int p_height, int i, int iy4, int16_t const4, int16_t const5, int offset,
    int k, int16_t *HWY_RESTRICT horz_out) {
  constexpr int kNumRows = tag.MaxBlocks();
  for (; k < AOMMIN(8, p_height - i) - kNumRows; k += kNumRows) {
    int iy = clamp(iy4 + k + 0, 0, height - 1);
    auto src = hn::ResizeBitCast(
        tag, hn::Set(int16x8_tag, const4 + ref[iy * stride + offset] * const5));
    if constexpr (kNumRows >= 2) {
      iy = clamp(iy4 + k + 1, 0, height - 1);
      src = hn::InsertBlock<1>(
          src,
          hn::Set(int16x8_tag, const4 + ref[iy * stride + offset] * const5));
    }
    if constexpr (kNumRows >= 3) {
      iy = clamp(iy4 + k + 2, 0, height - 1);
      src = hn::InsertBlock<2>(
          src,
          hn::Set(int16x8_tag, const4 + ref[iy * stride + offset] * const5));
      iy = clamp(iy4 + k + 3, 0, height - 1);
      src = hn::InsertBlock<3>(
          src,
          hn::Set(int16x8_tag, const4 + ref[iy * stride + offset] * const5));
    }
    hn::Store(src, tag, horz_out + (k + 7) * hn::MaxLanes(int16x8_tag));
  }
  return k;
}

HWY_ATTR void WarpHorizontalFilterOutOfBoundsSet(
    const uint8_t *HWY_RESTRICT ref, int height, int stride, int p_height,
    int i, int iy4, int16_t const4, int16_t const5, int offset,
    int16_t *HWY_RESTRICT horz_out) {
  int k = -7, iy;
  if constexpr (int16xN_tag.MaxBlocks() >= 3) {
    k = WarpHorizontalFilterOutOfBoundsSetLoop(int16xN_tag, ref, height, stride,
                                               p_height, i, iy4, const4, const5,
                                               offset, k, horz_out);
  }
  if constexpr (int16xN_tag.MaxBlocks() >= 2) {
    k = WarpHorizontalFilterOutOfBoundsSetLoop(int16x16_tag, ref, height,
                                               stride, p_height, i, iy4, const4,
                                               const5, offset, k, horz_out);
  }
  if constexpr (int16xN_tag.MaxBlocks() == 1) {
    k = WarpHorizontalFilterOutOfBoundsSetLoop(int16x8_tag, ref, height, stride,
                                               p_height, i, iy4, const4, const5,
                                               offset, k, horz_out);
  }
  iy = iy4 + k;
  iy = clamp(iy4 + k, 0, height - 1);
  hn::Store(hn::Set(int16x8_tag, const4 + ref[iy * stride + offset] * const5),
            int16x8_tag, horz_out + (k + 7) * hn::MaxLanes(int16x8_tag));
}

template <typename D>
HWY_ATTR int WarpHorizontalFilterOutOfBoundsPadLoop(
    D tag, const uint8_t *HWY_RESTRICT ref, int stride, int32_t ix4,
    int32_t iy4, int32_t sx4, int alpha, int beta, int p_height, int height,
    int i, const IVec16 round_const, const int reduce_bits_horiz,
    int out_of_boundary_left, int out_of_boundary_right, int k,
    int16_t *HWY_RESTRICT horz_out) {
  constexpr int kNumRows = tag.MaxBlocks();
  for (; k < (AOMMIN(8, p_height - i) - kNumRows); k += kNumRows) {
    auto src = LoadRowsClamped(tag, ref + ix4 - 7, stride, iy4 + k, height);
    if (out_of_boundary_left >= 0) {
      const auto shuffle_reg_left =
          hn::LoadDup128(tag, warp_pad_left[out_of_boundary_left]);
      src = hn::TableLookupBytes(src, shuffle_reg_left);
    }
    if (out_of_boundary_right >= 0) {
      const auto shuffle_reg_right =
          hn::LoadDup128(tag, warp_pad_right[out_of_boundary_right]);
      src = hn::TableLookupBytes(src, shuffle_reg_right);
    }
    int sx = sx4 + beta * (k + 4);
    HorizontalFilter(tag, src, horz_out, sx, alpha, beta, k + 7, round_const,
                     reduce_bits_horiz);
  }
  return k;
}

HWY_ATTR void WarpHorizontalFilterOutOfBoundsPad(
    const uint8_t *HWY_RESTRICT ref, int stride, int32_t ix4, int32_t iy4,
    int32_t sx4, int alpha, int beta, int p_height, int width, int height,
    int i, const IVec16 round_const, const int reduce_bits_horiz,
    int16_t *HWY_RESTRICT horz_out) {
  const int out_of_boundary_left = -(ix4 - 6);
  const int out_of_boundary_right = (ix4 + 8) - width;
  int k = -7, iy, sx;
  if constexpr (uint8xN_tag.MaxBlocks() >= 3) {
    k = WarpHorizontalFilterOutOfBoundsPadLoop(
        uint8xN_tag, ref, stride, ix4, iy4, sx4, alpha, beta, p_height, height,
        i, round_const, reduce_bits_horiz, out_of_boundary_left,
        out_of_boundary_right, k, horz_out);
  }
  if constexpr (uint8xN_tag.MaxBlocks() >= 2) {
    k = WarpHorizontalFilterOutOfBoundsPadLoop(
        uint8x32_tag, ref, stride, ix4, iy4, sx4, alpha, beta, p_height, height,
        i, round_const, reduce_bits_horiz, out_of_boundary_left,
        out_of_boundary_right, k, horz_out);
  }
  if constexpr (uint8xN_tag.MaxBlocks() == 1) {
    k = WarpHorizontalFilterOutOfBoundsPadLoop(
        uint8xN_tag, ref, stride, ix4, iy4, sx4, alpha, beta, p_height, height,
        i, round_const, reduce_bits_horiz, out_of_boundary_left,
        out_of_boundary_right, k, horz_out);
  }
  iy = iy4 + k;
  iy = clamp(iy, 0, height - 1);
  auto src = hn::LoadU(uint8x16_tag, ref + iy * stride + ix4 - 7);
  if (out_of_boundary_left >= 0) {
    const auto shuffle_reg_left =
        hn::LoadU(uint8x16_tag, warp_pad_left[out_of_boundary_left]);
    src = hn::TableLookupBytes(src, shuffle_reg_left);
  }
  if (out_of_boundary_right >= 0) {
    const auto shuffle_reg_right =
        hn::LoadU(uint8x16_tag, warp_pad_right[out_of_boundary_right]);
    src = hn::TableLookupBytes(src, shuffle_reg_right);
  }
  sx = sx4 + beta * (k + 4);
  HWY_ALIGN int8_t coeff[4 * hn::MaxLanes(coeff_tag)];
  PrepareLastHorizontalFilterCoefficients(int16xN_tag, alpha, beta, sx, coeff);
  FilterPixelsHorizontal(uint8x16_tag, src, horz_out, coeff, round_const,
                         reduce_bits_horiz, k + 7);
}

HWY_ATTR void WarpAffine(const int32_t *HWY_RESTRICT mat,
                         const uint8_t *HWY_RESTRICT ref, int width, int height,
                         int stride, uint8_t *HWY_RESTRICT pred, int p_col,
                         int p_row, int p_width, int p_height, int p_stride,
                         int subsampling_x, int subsampling_y,
                         ConvolveParams *HWY_RESTRICT conv_params,
                         int16_t alpha, int16_t beta, int16_t gamma,
                         int16_t delta) {
  int i, j;
  const int bd = 8;
  const int reduce_bits_horiz = conv_params->round_0;
  const int reduce_bits_vert = conv_params->is_compound
                                   ? conv_params->round_1
                                   : 2 * FILTER_BITS - reduce_bits_horiz;
  const int offset_bits_horiz = bd + FILTER_BITS - 1;
  assert(IMPLIES(conv_params->is_compound, conv_params->dst != NULL));

  const int offset_bits_vert = bd + 2 * FILTER_BITS - reduce_bits_horiz;
  const auto reduce_bits_vert_const =
      hn::Set(int32xN_tag, ((1 << reduce_bits_vert) >> 1));
  const auto res_add_const = hn::Set(int32xN_tag, 1 << offset_bits_vert);
  const int round_bits =
      2 * FILTER_BITS - conv_params->round_0 - conv_params->round_1;
  const int offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  assert(IMPLIES(conv_params->do_average, conv_params->is_compound));

  const auto round_const = hn::Set(
      int16xN_tag, (1 << offset_bits_horiz) + ((1 << reduce_bits_horiz) >> 1));

  IVec16 res_sub_const, round_bits_const, wt;
  UnpackWeightsAndSetRoundConst(conv_params, round_bits, offset_bits,
                                res_sub_const, round_bits_const, wt);

  IVec32 res_add_const_1;
  if (conv_params->is_compound == 1) {
    res_add_const_1 = hn::Add(reduce_bits_vert_const, res_add_const);
  } else {
    res_add_const_1 = hn::Set(int32xN_tag, -(1 << (bd + reduce_bits_vert - 1)) +
                                               ((1 << reduce_bits_vert) >> 1));
  }
  const int32_t const1 = alpha * (-4) + beta * (-4) +
                         (1 << (WARPEDDIFF_PREC_BITS - 1)) +
                         (WARPEDPIXEL_PREC_SHIFTS << WARPEDDIFF_PREC_BITS);
  const int32_t const2 = gamma * (-4) + delta * (-4) +
                         (1 << (WARPEDDIFF_PREC_BITS - 1)) +
                         (WARPEDPIXEL_PREC_SHIFTS << WARPEDDIFF_PREC_BITS);
  const int32_t const3 = ((1 << WARP_PARAM_REDUCE_BITS) - 1);
  const int16_t const4 = (1 << (bd + FILTER_BITS - reduce_bits_horiz - 1));
  const int16_t const5 = (1 << (FILTER_BITS - reduce_bits_horiz));

  for (i = 0; i < p_height; i += 8) {
    for (j = 0; j < p_width; j += 8) {
      HWY_ALIGN int16_t horz_out[8 * 16 + hn::MaxLanes(int16xN_tag)];
      const int32_t src_x = (p_col + j + 4) << subsampling_x;
      const int32_t src_y = (p_row + i + 4) << subsampling_y;
      const int64_t dst_x =
          (int64_t)mat[2] * src_x + (int64_t)mat[3] * src_y + (int64_t)mat[0];
      const int64_t dst_y =
          (int64_t)mat[4] * src_x + (int64_t)mat[5] * src_y + (int64_t)mat[1];
      const int64_t x4 = dst_x >> subsampling_x;
      const int64_t y4 = dst_y >> subsampling_y;

      int32_t ix4 = (int32_t)(x4 >> WARPEDMODEL_PREC_BITS);
      int32_t sx4 = x4 & ((1 << WARPEDMODEL_PREC_BITS) - 1);
      int32_t iy4 = (int32_t)(y4 >> WARPEDMODEL_PREC_BITS);
      int32_t sy4 = y4 & ((1 << WARPEDMODEL_PREC_BITS) - 1);

      // Add in all the constant terms, including rounding and offset
      sx4 += const1;
      sy4 += const2;

      sx4 &= ~const3;
      sy4 &= ~const3;

      // Horizontal filter
      // If the block is aligned such that, after clamping, every sample
      // would be taken from the leftmost/rightmost column, then we can
      // skip the expensive horizontal filter.

      if (ix4 <= -7) {
        WarpHorizontalFilterOutOfBoundsSet(ref, height, stride, p_height, i,
                                           iy4, const4, const5, 0, horz_out);
      } else if (ix4 >= width + 6) {
        WarpHorizontalFilterOutOfBoundsSet(ref, height, stride, p_height, i,
                                           iy4, const4, const5, width - 1,
                                           horz_out);
      } else if (((ix4 - 7) < 0) || ((ix4 + 9) > width)) {
        WarpHorizontalFilterOutOfBoundsPad(
            ref, stride, ix4, iy4, sx4, alpha, beta, p_height, width, height, i,
            round_const, reduce_bits_horiz, horz_out);
      } else {
        PrepareWarpHorizontalFilter(ref, horz_out, stride, ix4, iy4, sx4, alpha,
                                    beta, p_height, height, i, round_const,
                                    reduce_bits_horiz);
      }

      // Vertical filter
      PrepareWarpVerticalFilter(pred, horz_out, conv_params, gamma, delta,
                                p_height, p_stride, p_width, i, j, sy4,
                                reduce_bits_vert, res_add_const_1, round_bits,
                                res_sub_const, round_bits_const, wt);
    }
  }
}

}  // namespace HWY_NAMESPACE
}  // namespace

#define MAKE_WARP_AFFINE(suffix)                                             \
  extern "C" void av1_warp_affine_##suffix(                                  \
      const int32_t *HWY_RESTRICT mat, const uint8_t *HWY_RESTRICT ref,      \
      int width, int height, int stride, uint8_t *HWY_RESTRICT pred,         \
      int p_col, int p_row, int p_width, int p_height, int p_stride,         \
      int subsampling_x, int subsampling_y,                                  \
      ConvolveParams *HWY_RESTRICT conv_params, int16_t alpha, int16_t beta, \
      int16_t gamma, int16_t delta);                                         \
  HWY_ATTR void av1_warp_affine_##suffix(                                    \
      const int32_t *HWY_RESTRICT mat, const uint8_t *HWY_RESTRICT ref,      \
      int width, int height, int stride, uint8_t *HWY_RESTRICT pred,         \
      int p_col, int p_row, int p_width, int p_height, int p_stride,         \
      int subsampling_x, int subsampling_y,                                  \
      ConvolveParams *HWY_RESTRICT conv_params, int16_t alpha, int16_t beta, \
      int16_t gamma, int16_t delta) {                                        \
    HWY_NAMESPACE::WarpAffine(mat, ref, width, height, stride, pred, p_col,  \
                              p_row, p_width, p_height, p_stride,            \
                              subsampling_x, subsampling_y, conv_params,     \
                              alpha, beta, gamma, delta);                    \
  }

HWY_AFTER_NAMESPACE();

#endif  // AOM_AV1_COMMON_WARP_PLANE_HWY_H_
