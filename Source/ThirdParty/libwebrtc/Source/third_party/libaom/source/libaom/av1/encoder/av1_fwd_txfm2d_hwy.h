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

#ifndef AOM_AV1_ENCODER_AV1_FWD_TXFM2D_HWY_H_
#define AOM_AV1_ENCODER_AV1_FWD_TXFM2D_HWY_H_

#include <stdint.h>

#include "config/aom_config.h"
#include "config/av1_rtcd.h"
#include "third_party/highway/hwy/highway.h"
#include "aom_dsp/txfm_common.h"
#include "av1/common/av1_txfm.h"
#include "av1/common/enums.h"
#include "av1/encoder/av1_fwd_txfm1d.h"
#include "av1/encoder/av1_fwd_txfm1d_cfg.h"

#define FOR_EACH_TXFM2D(X, suffix) \
  X(4, 4, suffix)                  \
  X(8, 8, suffix)                  \
  X(16, 16, suffix)                \
  X(32, 32, suffix)                \
  X(64, 64, suffix)                \
  X(4, 8, suffix)                  \
  X(8, 4, suffix)                  \
  X(8, 16, suffix)                 \
  X(16, 8, suffix)                 \
  X(16, 32, suffix)                \
  X(32, 16, suffix)                \
  X(32, 64, suffix)                \
  X(64, 32, suffix)                \
  X(4, 16, suffix)                 \
  X(16, 4, suffix)                 \
  X(8, 32, suffix)                 \
  X(32, 8, suffix)                 \
  X(16, 64, suffix)                \
  X(64, 16, suffix)

#if HWY_CXX_LANG >= 201703L
#define CONSTEXPR_IF constexpr
#else
#define CONSTEXPR_IF
#endif

HWY_BEFORE_NAMESPACE();

namespace {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

constexpr int8_t kForwardTransformShift[TX_SIZES_ALL][3] = {
  { 2, 0, 0 },    //
  { 2, -1, 0 },   //
  { 2, -2, 0 },   //
  { 2, -4, 0 },   //
  { 0, -2, -2 },  //
  { 2, -1, 0 },   //
  { 2, -1, 0 },   //
  { 2, -2, 0 },   //
  { 2, -2, 0 },   //
  { 2, -4, 0 },   //
  { 2, -4, 0 },   //
  { 0, -2, -2 },  //
  { 2, -4, -2 },  //
  { 2, -1, 0 },   //
  { 2, -1, 0 },   //
  { 2, -2, 0 },   //
  { 2, -2, 0 },   //
  { 0, -2, 0 },   //
  { 2, -4, 0 },   //
};

constexpr int kTxSizeWideLog2[TX_SIZES_ALL] = {
  2, 3, 4, 5, 6, 2, 3, 3, 4, 4, 5, 5, 6, 2, 4, 3, 5, 4, 6,
};

// Transform block height in log2
constexpr int kTxSizeHighLog2[TX_SIZES_ALL] = {
  2, 3, 4, 5, 6, 3, 2, 4, 3, 5, 4, 6, 5, 4, 2, 5, 3, 6, 4,
};

constexpr bool kApplyRectScaleList[TX_SIZES_ALL] = {
  false, false, false, false, false, true,  true,  true,  true,  true,
  true,  true,  true,  false, false, false, false, false, false,
};

constexpr int8_t kForwardCosBitCol[MAX_TXWH_IDX /*txw_idx*/]
                                  [MAX_TXWH_IDX /*txh_idx*/] = {
                                    { 13, 13, 13, 0, 0 },
                                    { 13, 13, 13, 12, 0 },
                                    { 13, 13, 13, 12, 13 },
                                    { 0, 13, 13, 12, 13 },
                                    { 0, 0, 13, 12, 13 }
                                  };

constexpr int8_t kForwardCosBitRow[MAX_TXWH_IDX /*txw_idx*/]
                                  [MAX_TXWH_IDX /*txh_idx*/] = {
                                    { 13, 13, 12, 0, 0 },
                                    { 13, 13, 13, 12, 0 },
                                    { 13, 13, 12, 13, 12 },
                                    { 0, 12, 13, 12, 11 },
                                    { 0, 0, 12, 11, 10 }
                                  };

// Transform block width in pixels
constexpr int8_t kTxSizeWide[TX_SIZES_ALL] = {
  4, 8, 16, 32, 64, 4, 8, 8, 16, 16, 32, 32, 64, 4, 16, 8, 32, 16, 64,
};

// Transform block height in pixels
constexpr int8_t kTxSizeHigh[TX_SIZES_ALL] = {
  4, 8, 16, 32, 64, 8, 4, 16, 8, 32, 16, 64, 32, 16, 4, 32, 8, 64, 16,
};

constexpr int GetTxwIndex(TX_SIZE tx_size) {
  return kTxSizeWideLog2[tx_size] - kTxSizeWideLog2[0];
}

constexpr int GetTxhIndex(TX_SIZE tx_size) {
  return kTxSizeHighLog2[tx_size] - kTxSizeHighLog2[0];
}

template <typename D>
HWY_ATTR HWY_INLINE hn::VFromD<D> SetPair(D int_tag, int a, int b) {
  return hn::BitCast(
      int_tag,
      hn::Set(hn::RepartitionToWide<D>(),
              static_cast<int32_t>(
                  static_cast<uint16_t>(a) |
                  (static_cast<uint32_t>(static_cast<uint16_t>(b)) << 16))));
}

template <size_t LaneSize>
struct ButterflyTraits {};

template <>
struct ButterflyTraits<2> {
  template <typename D>
  HWY_ATTR HWY_INLINE static void Whole(
      D int_tag, int w0, int w1, const hn::TFromD<D> *HWY_RESTRICT in0,
      const hn::TFromD<D> *HWY_RESTRICT in1, hn::TFromD<D> *HWY_RESTRICT out0,
      hn::TFromD<D> *HWY_RESTRICT out1, int bit,
      hn::VFromD<hn::Repartition<int32_t, D>> round) {
    constexpr hn::RepartitionToWide<D> int32_tag;
    const auto ww0 = SetPair(int_tag, w0, w1);
    const auto ww1 = SetPair(int_tag, w1, -w0);
    const auto i0 = hn::Load(int_tag, in0);
    const auto i1 = hn::Load(int_tag, in1);
    const auto t0 = hn::InterleaveLower(int_tag, i0, i1);
    const auto t1 = hn::InterleaveUpper(int_tag, i0, i1);
    const auto u0 = hn::WidenMulPairwiseAdd(int32_tag, t0, ww0);
    const auto u1 = hn::WidenMulPairwiseAdd(int32_tag, t1, ww0);
    const auto v0 = hn::WidenMulPairwiseAdd(int32_tag, t0, ww1);
    const auto v1 = hn::WidenMulPairwiseAdd(int32_tag, t1, ww1);
    const auto c0 = hn::ShiftRightSame(hn::Add(u0, round), bit);
    const auto c1 = hn::ShiftRightSame(hn::Add(u1, round), bit);
    const auto d0 = hn::ShiftRightSame(hn::Add(v0, round), bit);
    const auto d1 = hn::ShiftRightSame(hn::Add(v1, round), bit);
    hn::Store(hn::ReorderDemote2To(int_tag, c0, c1), int_tag, out0);
    hn::Store(hn::ReorderDemote2To(int_tag, d0, d1), int_tag, out1);
  }

  template <typename D>
  HWY_ATTR HWY_INLINE static void Half(
      D int_tag, int w0, int w1, const hn::TFromD<D> *HWY_RESTRICT in0,
      const hn::TFromD<D> *HWY_RESTRICT in1, hn::TFromD<D> *HWY_RESTRICT out,
      int bit, hn::VFromD<hn::Repartition<int32_t, D>> round) {
    constexpr hn::RepartitionToWide<D> int32_tag;
    const auto i0 = hn::Load(int_tag, in0);
    const auto i1 = hn::Load(int_tag, in1);
    const auto t0 = hn::InterleaveLower(int_tag, i0, i1);
    const auto t1 = hn::InterleaveUpper(int_tag, i0, i1);
    const auto ww0 = SetPair(int_tag, w0, w1);
    const auto u0 = hn::WidenMulPairwiseAdd(int32_tag, t0, ww0);
    const auto u1 = hn::WidenMulPairwiseAdd(int32_tag, t1, ww0);
    const auto c0 = hn::ShiftRightSame(hn::Add(u0, round), bit);
    const auto c1 = hn::ShiftRightSame(hn::Add(u1, round), bit);
    hn::Store(hn::ReorderDemote2To(int_tag, c0, c1), int_tag, out);
  }
};

template <>
struct ButterflyTraits<4> {
  template <typename D>
  HWY_ATTR HWY_INLINE static void Whole(
      D int_tag, int w0, int w1, const hn::TFromD<D> *HWY_RESTRICT in0,
      const hn::TFromD<D> *HWY_RESTRICT in1, hn::TFromD<D> *HWY_RESTRICT out0,
      hn::TFromD<D> *HWY_RESTRICT out1, int bit,
      hn::VFromD<hn::Repartition<int32_t, D>> round) {
    const auto i0 = hn::Load(int_tag, in0);
    const auto i1 = hn::Load(int_tag, in1);
    const auto ww0 = hn::Set(int_tag, w0);
    const auto ww1 = hn::Set(int_tag, w1);
    const auto in1_w1 = hn::Mul(i1, ww1);
    const auto o0 = hn::MulAdd(i0, ww0, in1_w1);
    hn::Store(hn::ShiftRightSame(hn::Add(o0, round), bit), int_tag, out0);
    const auto in1_w0 = hn::Mul(i1, ww0);
    const auto o1 = hn::MulSub(i0, ww1, in1_w0);
    hn::Store(hn::ShiftRightSame(hn::Add(o1, round), bit), int_tag, out1);
  }

  template <typename D>
  HWY_ATTR HWY_INLINE static void Half(
      D int_tag, int w0, int w1, const hn::TFromD<D> *HWY_RESTRICT in0,
      const hn::TFromD<D> *HWY_RESTRICT in1, hn::TFromD<D> *HWY_RESTRICT out,
      int bit, hn::VFromD<hn::Repartition<int32_t, D>> round) {
    const auto i0 = hn::Load(int_tag, in0);
    const auto i1 = hn::Load(int_tag, in1);
    const auto ww0 = hn::Set(int_tag, w0);
    const auto ww1 = hn::Set(int_tag, w1);
    const auto in1_w1 = hn::Mul(i1, ww1);
    const auto o0 = hn::MulAdd(i0, ww0, in1_w1);
    hn::Store(hn::ShiftRightSame(hn::Add(o0, round), bit), int_tag, out);
  }
};

template <typename D>
HWY_ATTR HWY_INLINE void Butterfly(
    D int_tag, int w0, int w1, const hn::TFromD<D> *HWY_RESTRICT in0,
    const hn::TFromD<D> *HWY_RESTRICT in1, hn::TFromD<D> *HWY_RESTRICT out0,
    hn::TFromD<D> *HWY_RESTRICT out1, int bit,
    hn::VFromD<hn::Repartition<int32_t, D>> round) {
  ButterflyTraits<sizeof(hn::TFromD<D>)>::Whole(int_tag, w0, w1, in0, in1, out0,
                                                out1, bit, round);
}

template <typename D>
HWY_ATTR HWY_INLINE void HalfButterfly(
    D int_tag, int w0, int w1, const hn::TFromD<D> *HWY_RESTRICT in0,
    const hn::TFromD<D> *HWY_RESTRICT in1, hn::TFromD<D> *HWY_RESTRICT out,
    int bit, hn::VFromD<hn::Repartition<int32_t, D>> round) {
  ButterflyTraits<sizeof(hn::TFromD<D>)>::Half(int_tag, w0, w1, in0, in1, out,
                                               bit, round);
}

template <typename D>
HWY_ATTR HWY_INLINE void AddSub(D int_tag, const hn::TFromD<D> *in0,
                                const hn::TFromD<D> *in1,
                                hn::TFromD<D> *out_add,
                                hn::TFromD<D> *out_sub) {
  const auto i0 = hn::Load(int_tag, in0);
  const auto i1 = hn::Load(int_tag, in1);
  if CONSTEXPR_IF (sizeof(hn::TFromD<D>) == 2) {
    hn::Store(hn::SaturatedAdd(i0, i1), int_tag, out_add);
    hn::Store(hn::SaturatedSub(i0, i1), int_tag, out_sub);
  } else {
    hn::Store(hn::Add(i0, i1), int_tag, out_add);
    hn::Store(hn::Sub(i0, i1), int_tag, out_sub);
  }
}

template <size_t LaneSize, size_t NumLanes>
struct Fdct4Traits {
  template <typename D>
  HWY_ATTR HWY_INLINE static void Fdct4(D int_tag,
                                        hn::TFromD<D> *HWY_RESTRICT in,
                                        const int8_t cos_bit, size_t instride) {
    using T = hn::TFromD<D>;
    constexpr size_t kNumLanes = hn::MaxLanes(int_tag);
    HWY_ALIGN_MAX T buf0[4 * kNumLanes];
    const int32_t *HWY_RESTRICT const cospi = cospi_arr(cos_bit);
    constexpr hn::Repartition<int32_t, D> int32_tag;
    const auto round = hn::Set(int32_tag, 1 << (cos_bit - 1));
    AddSub(int_tag, &in[0 * instride], &in[3 * instride], &buf0[0 * kNumLanes],
           &buf0[3 * kNumLanes]);
    AddSub(int_tag, &in[1 * instride], &in[2 * instride], &buf0[1 * kNumLanes],
           &buf0[2 * kNumLanes]);
    Butterfly(int_tag, cospi[32], cospi[32], &buf0[0 * kNumLanes],
              &buf0[1 * kNumLanes], &in[0 * instride], &in[2 * instride],
              cos_bit, round);
    Butterfly(int_tag, cospi[16], cospi[48], &buf0[3 * kNumLanes],
              &buf0[2 * kNumLanes], &in[1 * instride], &in[3 * instride],
              cos_bit, round);
  }
};

template <>
struct Fdct4Traits<2, 4> {
  template <typename D>
  HWY_ATTR HWY_INLINE static void Fdct4(D int_tag,
                                        hn::TFromD<D> *HWY_RESTRICT in,
                                        const int8_t cos_bit, size_t instride) {
    const int32_t *HWY_RESTRICT const cospi = cospi_arr(cos_bit);
    constexpr hn::FixedTag<hn::TFromD<D>, 8> demote_tag;
    constexpr hn::Repartition<int32_t, decltype(demote_tag)> int32_tag;
    const auto round = hn::Set(int32_tag, 1 << (cos_bit - 1));
    const auto cospi_p32_p32 = SetPair(int_tag, cospi[32], cospi[32]);
    const auto cospi_p32_m32 = SetPair(int_tag, cospi[32], -cospi[32]);
    const auto cospi_p16_p48 = SetPair(int_tag, cospi[16], cospi[48]);
    const auto cospi_p48_m16 = SetPair(int_tag, cospi[48], -cospi[16]);
    const auto i0 = hn::Load(int_tag, &in[0 * instride]);
    const auto i1 = hn::Load(int_tag, &in[1 * instride]);
    const auto i2 = hn::Load(int_tag, &in[2 * instride]);
    const auto i3 = hn::Load(int_tag, &in[3 * instride]);
    const auto u0 = hn::InterleaveLower(int_tag, i0, i1);
    const auto u1 = hn::InterleaveLower(int_tag, i3, i2);
    const auto v0 = hn::Add(u0, u1);
    const auto v1 = hn::Sub(u0, u1);
    const auto x0 = hn::WidenMulPairwiseAdd(int32_tag, v0, cospi_p32_p32);
    const auto x1 = hn::WidenMulPairwiseAdd(int32_tag, v0, cospi_p32_m32);
    const auto x2 = hn::WidenMulPairwiseAdd(int32_tag, v1, cospi_p16_p48);
    const auto x3 = hn::WidenMulPairwiseAdd(int32_tag, v1, cospi_p48_m16);
    const auto v0w0 = hn::ShiftRightSame(hn::Add(x0, round), cos_bit);
    const auto v0w1 = hn::ShiftRightSame(hn::Add(x1, round), cos_bit);
    const auto v1w0 = hn::ShiftRightSame(hn::Add(x2, round), cos_bit);
    const auto v1w1 = hn::ShiftRightSame(hn::Add(x3, round), cos_bit);
    const auto o0 = hn::ReorderDemote2To(demote_tag, v0w0, v0w1);
    const auto o1 = hn::ReorderDemote2To(demote_tag, v1w0, v1w1);
    hn::Store(o0, demote_tag, &in[0 * instride]);
    hn::Store(o1, demote_tag, &in[1 * instride]);
    hn::Store(hn::ShiftRightLanes<4>(demote_tag, o0), demote_tag,
              &in[2 * instride]);
    hn::Store(hn::ShiftRightLanes<4>(demote_tag, o1), demote_tag,
              &in[3 * instride]);
  }
};

template <typename D>
HWY_ATTR HWY_INLINE void Fdct4(D int_tag, hn::TFromD<D> *HWY_RESTRICT in,
                               const int8_t cos_bit, size_t instride) {
  Fdct4Traits<sizeof(hn::TFromD<D>), hn::MaxLanes(int_tag)>::Fdct4(
      int_tag, in, cos_bit, instride);
}

template <typename D>
HWY_ATTR HWY_INLINE void Fdct8(D int_tag, hn::TFromD<D> *HWY_RESTRICT in,
                               const int8_t cos_bit, size_t instride) {
  constexpr size_t kNumLanes = hn::MaxLanes(int_tag);
  HWY_ALIGN_MAX hn::TFromD<D> buf0[8 * kNumLanes];
  HWY_ALIGN_MAX hn::TFromD<D> buf1[8 * kNumLanes];
  const int32_t *HWY_RESTRICT const cospi = cospi_arr(cos_bit);
  const auto round = hn::Set(hn::Repartition<int32_t, D>(), 1 << (cos_bit - 1));

  // Even 8 points 0, 2, ..., 14
  // stage 0
  // stage 1
  // buf0/buf1
  AddSub(int_tag, &in[0 * instride], &in[7 * instride], &buf0[0 * kNumLanes],
         &buf1[7 * kNumLanes]);
  // buf0/buf0
  AddSub(int_tag, &in[1 * instride], &in[6 * instride], &buf0[1 * kNumLanes],
         &buf0[6 * kNumLanes]);
  // buf0/buf0
  AddSub(int_tag, &in[2 * instride], &in[5 * instride], &buf0[2 * kNumLanes],
         &buf0[5 * kNumLanes]);
  // buf0/buf1
  AddSub(int_tag, &in[3 * instride], &in[4 * instride], &buf0[3 * kNumLanes],
         &buf1[4 * kNumLanes]);

  // stage 2
  for (size_t i = 0; i < 2; ++i) {
    AddSub(int_tag, &buf0[i * kNumLanes], &buf0[(3 - i) * kNumLanes],
           &buf1[i * kNumLanes], &buf1[(3 - i) * kNumLanes]);
  }

  Butterfly(int_tag, -cospi[32], cospi[32], &buf0[5 * kNumLanes],
            &buf0[6 * kNumLanes], &buf1[5 * kNumLanes], &buf1[6 * kNumLanes],
            cos_bit, round);

  // stage 3
  // type 0
  Butterfly(int_tag, cospi[32], cospi[32], &buf1[0 * kNumLanes],
            &buf1[1 * kNumLanes], &in[0 * instride], &in[4 * instride], cos_bit,
            round);

  // type 1
  Butterfly(int_tag, cospi[16], cospi[48], &buf1[3 * kNumLanes],
            &buf1[2 * kNumLanes], &in[2 * instride], &in[6 * instride], cos_bit,
            round);

  AddSub(int_tag, &buf1[4 * kNumLanes], &buf1[5 * kNumLanes],
         &buf0[4 * kNumLanes], &buf0[5 * kNumLanes]);
  AddSub(int_tag, &buf1[7 * kNumLanes], &buf1[6 * kNumLanes],
         &buf0[7 * kNumLanes], &buf0[6 * kNumLanes]);

  // stage 4
  // stage 5
  Butterfly(int_tag, cospi[8], cospi[56], &buf0[7 * kNumLanes],
            &buf0[4 * kNumLanes], &in[1 * instride], &in[7 * instride], cos_bit,
            round);
  Butterfly(int_tag, cospi[40], cospi[24], &buf0[6 * kNumLanes],
            &buf0[5 * kNumLanes], &in[5 * instride], &in[3 * instride], cos_bit,
            round);
}

template <typename D>
HWY_ATTR HWY_INLINE void Fdct16(D int_tag, hn::TFromD<D> *HWY_RESTRICT in,
                                const int8_t cos_bit, size_t instride) {
  constexpr size_t kNumLanes = hn::MaxLanes(int_tag);
  HWY_ALIGN_MAX hn::TFromD<D> buf0[16 * kNumLanes];
  HWY_ALIGN_MAX hn::TFromD<D> buf1[16 * kNumLanes];
  const int32_t *HWY_RESTRICT const cospi = cospi_arr(cos_bit);
  const auto round = hn::Set(hn::Repartition<int32_t, D>(), 1 << (cos_bit - 1));

  // Calculate the column 0, 1, 2, 3
  // stage 0
  // stage 1
  for (size_t i = 0; i < 8; ++i) {
    AddSub(int_tag, &in[i * instride], &in[(15 - i) * instride],
           &buf0[i * kNumLanes], &buf0[(15 - i) * kNumLanes]);
  }

  // stage 2
  for (size_t i = 0; i < 4; ++i) {
    AddSub(int_tag, &buf0[i * kNumLanes], &buf0[(7 - i) * kNumLanes],
           &buf1[i * kNumLanes], &buf1[(7 - i) * kNumLanes]);
  }

  Butterfly(int_tag, -cospi[32], cospi[32], &buf0[10 * kNumLanes],
            &buf0[13 * kNumLanes], &buf1[10 * kNumLanes], &buf1[13 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, -cospi[32], cospi[32], &buf0[11 * kNumLanes],
            &buf0[12 * kNumLanes], &buf1[11 * kNumLanes], &buf1[12 * kNumLanes],
            cos_bit, round);

  // stage 3
  for (size_t i = 0; i < 2; ++i) {
    AddSub(int_tag, &buf1[i * kNumLanes], &buf1[(3 - i) * kNumLanes],
           &buf0[i * kNumLanes], &buf0[(3 - i) * kNumLanes]);
  }

  Butterfly(int_tag, -cospi[32], cospi[32], &buf1[5 * kNumLanes],
            &buf1[6 * kNumLanes], &buf0[5 * kNumLanes], &buf0[6 * kNumLanes],
            cos_bit, round);

  for (size_t i = 0; i < 2; ++i) {
    AddSub(int_tag, &buf0[(8 + i) * kNumLanes], &buf1[(11 - i) * kNumLanes],
           &buf0[(8 + i) * kNumLanes], &buf0[(11 - i) * kNumLanes]);
  }
  for (size_t i = 0; i < 2; ++i) {
    AddSub(int_tag, &buf0[(15 - i) * kNumLanes], &buf1[(12 + i) * kNumLanes],
           &buf0[(15 - i) * kNumLanes], &buf0[(12 + i) * kNumLanes]);
  }

  // stage 4
  Butterfly(int_tag, cospi[32], cospi[32], &buf0[0 * kNumLanes],
            &buf0[1 * kNumLanes], &in[0 * instride], &in[8 * instride], cos_bit,
            round);

  Butterfly(int_tag, cospi[16], cospi[48], &buf0[3 * kNumLanes],
            &buf0[2 * kNumLanes], &in[4 * instride], &in[12 * instride],
            cos_bit, round);

  AddSub(int_tag, &buf1[4 * kNumLanes], &buf0[5 * kNumLanes],
         &buf1[4 * kNumLanes], &buf1[5 * kNumLanes]);
  AddSub(int_tag, &buf1[7 * kNumLanes], &buf0[6 * kNumLanes],
         &buf1[7 * kNumLanes], &buf1[6 * kNumLanes]);

  Butterfly(int_tag, -cospi[16], cospi[48], &buf0[9 * kNumLanes],
            &buf0[14 * kNumLanes], &buf1[9 * kNumLanes], &buf1[14 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, -cospi[48], -cospi[16], &buf0[10 * kNumLanes],
            &buf0[13 * kNumLanes], &buf1[10 * kNumLanes], &buf1[13 * kNumLanes],
            cos_bit, round);

  // stage 5
  Butterfly(int_tag, cospi[8], cospi[56], &buf1[7 * kNumLanes],
            &buf1[4 * kNumLanes], &in[2 * instride], &in[14 * instride],
            cos_bit, round);
  Butterfly(int_tag, cospi[40], cospi[24], &buf1[6 * kNumLanes],
            &buf1[5 * kNumLanes], &in[10 * instride], &in[6 * instride],
            cos_bit, round);

  AddSub(int_tag, &buf0[8 * kNumLanes], &buf1[9 * kNumLanes],
         &buf0[8 * kNumLanes], &buf0[9 * kNumLanes]);
  AddSub(int_tag, &buf0[11 * kNumLanes], &buf1[10 * kNumLanes],
         &buf0[11 * kNumLanes], &buf0[10 * kNumLanes]);
  AddSub(int_tag, &buf0[12 * kNumLanes], &buf1[13 * kNumLanes],
         &buf0[12 * kNumLanes], &buf0[13 * kNumLanes]);
  AddSub(int_tag, &buf0[15 * kNumLanes], &buf1[14 * kNumLanes],
         &buf0[15 * kNumLanes], &buf0[14 * kNumLanes]);

  // stage 6
  Butterfly(int_tag, cospi[4], cospi[60], &buf0[15 * kNumLanes],
            &buf0[8 * kNumLanes], &in[1 * instride], &in[15 * instride],
            cos_bit, round);
  Butterfly(int_tag, cospi[36], cospi[28], &buf0[14 * kNumLanes],
            &buf0[9 * kNumLanes], &in[9 * instride], &in[7 * instride], cos_bit,
            round);
  Butterfly(int_tag, cospi[20], cospi[44], &buf0[13 * kNumLanes],
            &buf0[10 * kNumLanes], &in[5 * instride], &in[11 * instride],
            cos_bit, round);
  Butterfly(int_tag, cospi[52], cospi[12], &buf0[12 * kNumLanes],
            &buf0[11 * kNumLanes], &in[13 * instride], &in[3 * instride],
            cos_bit, round);
}

template <typename D>
HWY_ATTR HWY_INLINE void Fdct32(D int_tag, hn::TFromD<D> *HWY_RESTRICT in,
                                const int8_t cos_bit, size_t instride) {
  constexpr size_t kNumLanes = hn::MaxLanes(int_tag);
  HWY_ALIGN_MAX hn::TFromD<D> buf0[32 * kNumLanes];
  HWY_ALIGN_MAX hn::TFromD<D> buf1[32 * kNumLanes];
  const int32_t *HWY_RESTRICT const cospi = cospi_arr(cos_bit);
  const auto round = hn::Set(hn::Repartition<int32_t, D>(), 1 << (cos_bit - 1));
  // stage 0
  // stage 1
  for (size_t i = 0; i < 16; ++i) {
    AddSub(int_tag, &in[i * instride], &in[(31 - i) * instride],
           &buf1[i * kNumLanes], &buf1[(31 - i) * kNumLanes]);
  }

  // stage 2
  for (size_t i = 0; i < 8; ++i) {
    AddSub(int_tag, &buf1[i * kNumLanes], &buf1[(15 - i) * kNumLanes],
           &buf0[i * kNumLanes], &buf0[(15 - i) * kNumLanes]);
  }

  Butterfly(int_tag, -cospi[32], cospi[32], &buf1[20 * kNumLanes],
            &buf1[27 * kNumLanes], &buf0[20 * kNumLanes], &buf0[27 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, -cospi[32], cospi[32], &buf1[21 * kNumLanes],
            &buf1[26 * kNumLanes], &buf0[21 * kNumLanes], &buf0[26 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, -cospi[32], cospi[32], &buf1[22 * kNumLanes],
            &buf1[25 * kNumLanes], &buf0[22 * kNumLanes], &buf0[25 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, -cospi[32], cospi[32], &buf1[23 * kNumLanes],
            &buf1[24 * kNumLanes], &buf0[23 * kNumLanes], &buf0[24 * kNumLanes],
            cos_bit, round);

  // stage 3
  for (size_t i = 0; i < 4; ++i) {
    AddSub(int_tag, &buf0[i * kNumLanes], &buf0[(7 - i) * kNumLanes],
           &buf1[i * kNumLanes], &buf1[(7 - i) * kNumLanes]);
  }

  Butterfly(int_tag, -cospi[32], cospi[32], &buf0[10 * kNumLanes],
            &buf0[13 * kNumLanes], &buf1[10 * kNumLanes], &buf1[13 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, -cospi[32], cospi[32], &buf0[11 * kNumLanes],
            &buf0[12 * kNumLanes], &buf1[11 * kNumLanes], &buf1[12 * kNumLanes],
            cos_bit, round);

  for (size_t i = 0; i < 4; ++i) {
    AddSub(int_tag, &buf1[(16 + i) * kNumLanes], &buf0[(23 - i) * kNumLanes],
           &buf1[(16 + i) * kNumLanes], &buf1[(23 - i) * kNumLanes]);
  }
  for (size_t i = 0; i < 4; ++i) {
    AddSub(int_tag, &buf1[(31 - i) * kNumLanes], &buf0[(24 + i) * kNumLanes],
           &buf1[(31 - i) * kNumLanes], &buf1[(24 + i) * kNumLanes]);
  }

  // stage 4
  for (size_t i = 0; i < 2; ++i) {
    AddSub(int_tag, &buf1[i * kNumLanes], &buf1[(3 - i) * kNumLanes],
           &buf0[i * kNumLanes], &buf0[(3 - i) * kNumLanes]);
  }

  Butterfly(int_tag, -cospi[32], cospi[32], &buf1[5 * kNumLanes],
            &buf1[6 * kNumLanes], &buf0[5 * kNumLanes], &buf0[6 * kNumLanes],
            cos_bit, round);

  for (size_t i = 0; i < 2; ++i) {
    AddSub(int_tag, &buf0[(8 + i) * kNumLanes], &buf1[(11 - i) * kNumLanes],
           &buf0[(8 + i) * kNumLanes], &buf0[(11 - i) * kNumLanes]);
  }
  for (size_t i = 0; i < 2; ++i) {
    AddSub(int_tag, &buf0[(15 - i) * kNumLanes], &buf1[(12 + i) * kNumLanes],
           &buf0[(15 - i) * kNumLanes], &buf0[(12 + i) * kNumLanes]);
  }

  Butterfly(int_tag, -cospi[16], cospi[48], &buf1[18 * kNumLanes],
            &buf1[29 * kNumLanes], &buf0[18 * kNumLanes], &buf0[29 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, -cospi[16], cospi[48], &buf1[19 * kNumLanes],
            &buf1[28 * kNumLanes], &buf0[19 * kNumLanes], &buf0[28 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, -cospi[48], -cospi[16], &buf1[20 * kNumLanes],
            &buf1[27 * kNumLanes], &buf0[20 * kNumLanes], &buf0[27 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, -cospi[48], -cospi[16], &buf1[21 * kNumLanes],
            &buf1[26 * kNumLanes], &buf0[21 * kNumLanes], &buf0[26 * kNumLanes],
            cos_bit, round);

  // stage 5
  Butterfly(int_tag, cospi[32], cospi[32], &buf0[0 * kNumLanes],
            &buf0[1 * kNumLanes], &in[0 * instride], &in[16 * instride],
            cos_bit, round);
  Butterfly(int_tag, cospi[16], cospi[48], &buf0[3 * kNumLanes],
            &buf0[2 * kNumLanes], &in[8 * instride], &in[24 * instride],
            cos_bit, round);
  AddSub(int_tag, &buf1[4 * kNumLanes], &buf0[5 * kNumLanes],
         &buf1[4 * kNumLanes], &buf1[5 * kNumLanes]);
  AddSub(int_tag, &buf1[7 * kNumLanes], &buf0[6 * kNumLanes],
         &buf1[7 * kNumLanes], &buf1[6 * kNumLanes]);
  Butterfly(int_tag, -cospi[16], cospi[48], &buf0[9 * kNumLanes],
            &buf0[14 * kNumLanes], &buf1[9 * kNumLanes], &buf1[14 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, -cospi[48], -cospi[16], &buf0[10 * kNumLanes],
            &buf0[13 * kNumLanes], &buf1[10 * kNumLanes], &buf1[13 * kNumLanes],
            cos_bit, round);

  AddSub(int_tag, &buf1[16 * kNumLanes], &buf0[19 * kNumLanes],
         &buf1[16 * kNumLanes], &buf1[19 * kNumLanes]);
  AddSub(int_tag, &buf1[17 * kNumLanes], &buf0[18 * kNumLanes],
         &buf1[17 * kNumLanes], &buf1[18 * kNumLanes]);
  AddSub(int_tag, &buf1[23 * kNumLanes], &buf0[20 * kNumLanes],
         &buf1[23 * kNumLanes], &buf1[20 * kNumLanes]);
  AddSub(int_tag, &buf1[22 * kNumLanes], &buf0[21 * kNumLanes],
         &buf1[22 * kNumLanes], &buf1[21 * kNumLanes]);
  AddSub(int_tag, &buf1[24 * kNumLanes], &buf0[27 * kNumLanes],
         &buf1[24 * kNumLanes], &buf1[27 * kNumLanes]);
  AddSub(int_tag, &buf1[25 * kNumLanes], &buf0[26 * kNumLanes],
         &buf1[25 * kNumLanes], &buf1[26 * kNumLanes]);
  AddSub(int_tag, &buf1[31 * kNumLanes], &buf0[28 * kNumLanes],
         &buf1[31 * kNumLanes], &buf1[28 * kNumLanes]);
  AddSub(int_tag, &buf1[30 * kNumLanes], &buf0[29 * kNumLanes],
         &buf1[30 * kNumLanes], &buf1[29 * kNumLanes]);

  // stage 6
  Butterfly(int_tag, cospi[8], cospi[56], &buf1[7 * kNumLanes],
            &buf1[4 * kNumLanes], &in[4 * instride], &in[28 * instride],
            cos_bit, round);
  Butterfly(int_tag, cospi[40], cospi[24], &buf1[6 * kNumLanes],
            &buf1[5 * kNumLanes], &in[20 * instride], &in[12 * instride],
            cos_bit, round);
  AddSub(int_tag, &buf0[8 * kNumLanes], &buf1[9 * kNumLanes],
         &buf0[8 * kNumLanes], &buf0[9 * kNumLanes]);
  AddSub(int_tag, &buf0[11 * kNumLanes], &buf1[10 * kNumLanes],
         &buf0[11 * kNumLanes], &buf0[10 * kNumLanes]);
  AddSub(int_tag, &buf0[12 * kNumLanes], &buf1[13 * kNumLanes],
         &buf0[12 * kNumLanes], &buf0[13 * kNumLanes]);
  AddSub(int_tag, &buf0[15 * kNumLanes], &buf1[14 * kNumLanes],
         &buf0[15 * kNumLanes], &buf0[14 * kNumLanes]);
  Butterfly(int_tag, -cospi[8], cospi[56], &buf1[17 * kNumLanes],
            &buf1[30 * kNumLanes], &buf0[17 * kNumLanes], &buf0[30 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, -cospi[56], -cospi[8], &buf1[18 * kNumLanes],
            &buf1[29 * kNumLanes], &buf0[18 * kNumLanes], &buf0[29 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, -cospi[40], cospi[24], &buf1[21 * kNumLanes],
            &buf1[26 * kNumLanes], &buf0[21 * kNumLanes], &buf0[26 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, -cospi[24], -cospi[40], &buf1[22 * kNumLanes],
            &buf1[25 * kNumLanes], &buf0[22 * kNumLanes], &buf0[25 * kNumLanes],
            cos_bit, round);

  // stage 7
  Butterfly(int_tag, cospi[4], cospi[60], &buf0[15 * kNumLanes],
            &buf0[8 * kNumLanes], &in[2 * instride], &in[30 * instride],
            cos_bit, round);
  Butterfly(int_tag, cospi[36], cospi[28], &buf0[14 * kNumLanes],
            &buf0[9 * kNumLanes], &in[18 * instride], &in[14 * instride],
            cos_bit, round);
  Butterfly(int_tag, cospi[20], cospi[44], &buf0[13 * kNumLanes],
            &buf0[10 * kNumLanes], &in[10 * instride], &in[22 * instride],
            cos_bit, round);
  Butterfly(int_tag, cospi[52], cospi[12], &buf0[12 * kNumLanes],
            &buf0[11 * kNumLanes], &in[26 * instride], &in[6 * instride],
            cos_bit, round);
  AddSub(int_tag, &buf1[16 * kNumLanes], &buf0[17 * kNumLanes],
         &buf1[16 * kNumLanes], &buf1[17 * kNumLanes]);
  AddSub(int_tag, &buf1[19 * kNumLanes], &buf0[18 * kNumLanes],
         &buf1[19 * kNumLanes], &buf1[18 * kNumLanes]);
  AddSub(int_tag, &buf1[20 * kNumLanes], &buf0[21 * kNumLanes],
         &buf1[20 * kNumLanes], &buf1[21 * kNumLanes]);
  AddSub(int_tag, &buf1[23 * kNumLanes], &buf0[22 * kNumLanes],
         &buf1[23 * kNumLanes], &buf1[22 * kNumLanes]);
  AddSub(int_tag, &buf1[24 * kNumLanes], &buf0[25 * kNumLanes],
         &buf1[24 * kNumLanes], &buf1[25 * kNumLanes]);
  AddSub(int_tag, &buf1[27 * kNumLanes], &buf0[26 * kNumLanes],
         &buf1[27 * kNumLanes], &buf1[26 * kNumLanes]);
  AddSub(int_tag, &buf1[28 * kNumLanes], &buf0[29 * kNumLanes],
         &buf1[28 * kNumLanes], &buf1[29 * kNumLanes]);
  AddSub(int_tag, &buf1[31 * kNumLanes], &buf0[30 * kNumLanes],
         &buf1[31 * kNumLanes], &buf1[30 * kNumLanes]);

  // stage 8 & 9
  Butterfly(int_tag, cospi[2], cospi[62], &buf1[31 * kNumLanes],
            &buf1[16 * kNumLanes], &in[1 * instride], &in[31 * instride],
            cos_bit, round);
  Butterfly(int_tag, cospi[34], cospi[30], &buf1[30 * kNumLanes],
            &buf1[17 * kNumLanes], &in[17 * instride], &in[15 * instride],
            cos_bit, round);
  Butterfly(int_tag, cospi[18], cospi[46], &buf1[29 * kNumLanes],
            &buf1[18 * kNumLanes], &in[9 * instride], &in[23 * instride],
            cos_bit, round);
  Butterfly(int_tag, cospi[50], cospi[14], &buf1[28 * kNumLanes],
            &buf1[19 * kNumLanes], &in[25 * instride], &in[7 * instride],
            cos_bit, round);
  Butterfly(int_tag, cospi[10], cospi[54], &buf1[27 * kNumLanes],
            &buf1[20 * kNumLanes], &in[5 * instride], &in[27 * instride],
            cos_bit, round);
  Butterfly(int_tag, cospi[42], cospi[22], &buf1[26 * kNumLanes],
            &buf1[21 * kNumLanes], &in[21 * instride], &in[11 * instride],
            cos_bit, round);
  Butterfly(int_tag, cospi[26], cospi[38], &buf1[25 * kNumLanes],
            &buf1[22 * kNumLanes], &in[13 * instride], &in[19 * instride],
            cos_bit, round);
  Butterfly(int_tag, cospi[58], cospi[6], &buf1[24 * kNumLanes],
            &buf1[23 * kNumLanes], &in[29 * instride], &in[3 * instride],
            cos_bit, round);

  // stage 9 was fused with prior stages.
}

template <size_t InStride, size_t OutStride, typename D>
HWY_ATTR HWY_NOINLINE void Fdct64(D int_tag, hn::TFromD<D> *HWY_RESTRICT in,
                                  const int8_t cos_bit) {
  constexpr size_t kNumLanes = hn::MaxLanes(int_tag);
  constexpr size_t kNumBytes = kNumLanes * sizeof(hn::TFromD<D>);
  HWY_ALIGN_MAX hn::TFromD<D> buf0[64 * kNumLanes];
  HWY_ALIGN_MAX hn::TFromD<D> buf1[64 * kNumLanes];
  const int32_t *HWY_RESTRICT const cospi = cospi_arr(cos_bit);
  const auto round = hn::Set(hn::Repartition<int32_t, D>(), 1 << (cos_bit - 1));

  // stage 1
#if HWY_TARGET == HWY_SSE4
  // For whatever reason, some compilers don't unroll this when building for
  // SSE4; help them along.
  HWY_UNROLL(32)
#endif
  for (size_t i = 0; i < 32; ++i) {
    AddSub(int_tag, &in[i * InStride], &in[(63 - i) * InStride],
           &buf0[i * kNumLanes], &buf0[(63 - i) * kNumLanes]);
  }

  // stage 2
  for (size_t i = 0; i < 16; ++i) {
    AddSub(int_tag, &buf0[i * kNumLanes], &buf0[(31 - i) * kNumLanes],
           &buf1[i * kNumLanes], &buf1[(31 - i) * kNumLanes]);
  }
  for (size_t i = 0; i < 8; ++i) {
    Butterfly(int_tag, -cospi[32], cospi[32], &buf0[(40 + i) * kNumLanes],
              &buf0[(55 - i) * kNumLanes], &buf1[(40 + i) * kNumLanes],
              &buf1[(55 - i) * kNumLanes], cos_bit, round);
  }

  // stage 3
  for (size_t i = 0; i < 8; ++i) {
    AddSub(int_tag, &buf1[i * kNumLanes], &buf1[(15 - i) * kNumLanes],
           &buf0[i * kNumLanes], &buf0[(15 - i) * kNumLanes]);
  }
  for (size_t i = 0; i < 4; ++i) {
    Butterfly(int_tag, -cospi[32], cospi[32], &buf1[(20 + i) * kNumLanes],
              &buf1[(27 - i) * kNumLanes], &buf0[(20 + i) * kNumLanes],
              &buf0[(27 - i) * kNumLanes], cos_bit, round);
  }
  for (size_t i = 0; i < 8; ++i) {
    AddSub(int_tag, &buf0[(32 + i) * kNumLanes], &buf1[(47 - i) * kNumLanes],
           &buf0[(32 + i) * kNumLanes], &buf0[(47 - i) * kNumLanes]);
  }
  for (size_t i = 0; i < 8; ++i) {
    AddSub(int_tag, &buf0[(63 - i) * kNumLanes], &buf1[(48 + i) * kNumLanes],
           &buf0[(63 - i) * kNumLanes], &buf0[(48 + i) * kNumLanes]);
  }
  // stage 4
  for (size_t i = 0; i < 4; ++i) {
    AddSub(int_tag, &buf0[(0 + i) * kNumLanes], &buf0[(7 - i) * kNumLanes],
           &buf1[(0 + i) * kNumLanes], &buf1[(7 - i) * kNumLanes]);
  }
  for (size_t i = 0; i < 2; ++i) {
    Butterfly(int_tag, -cospi[32], cospi[32], &buf0[(10 + i) * kNumLanes],
              &buf0[(13 - i) * kNumLanes], &buf1[(10 + i) * kNumLanes],
              &buf1[(13 - i) * kNumLanes], cos_bit, round);
  }
  for (size_t i = 0; i < 4; ++i) {
    AddSub(int_tag, &buf1[(16 + i) * kNumLanes], &buf0[(23 - i) * kNumLanes],
           &buf1[(16 + i) * kNumLanes], &buf1[(23 - i) * kNumLanes]);
  }
  for (size_t i = 0; i < 4; ++i) {
    AddSub(int_tag, &buf1[(31 - i) * kNumLanes], &buf0[(24 + i) * kNumLanes],
           &buf1[(31 - i) * kNumLanes], &buf1[(24 + i) * kNumLanes]);
  }
  for (size_t i = 0; i < 4; ++i) {
    Butterfly(int_tag, -cospi[16], cospi[48], &buf0[(36 + i) * kNumLanes],
              &buf0[(59 - i) * kNumLanes], &buf1[(36 + i) * kNumLanes],
              &buf1[(59 - i) * kNumLanes], cos_bit, round);
  }
  for (size_t i = 4; i < 8; ++i) {
    Butterfly(int_tag, -cospi[48], -cospi[16], &buf0[(36 + i) * kNumLanes],
              &buf0[(59 - i) * kNumLanes], &buf1[(36 + i) * kNumLanes],
              &buf1[(59 - i) * kNumLanes], cos_bit, round);
  }
  // stage 5
  for (size_t i = 0; i < 2; ++i) {
    AddSub(int_tag, &buf1[(0 + i) * kNumLanes], &buf1[(3 - i) * kNumLanes],
           &buf0[(0 + i) * kNumLanes], &buf0[(3 - i) * kNumLanes]);
  }
  Butterfly(int_tag, -cospi[32], cospi[32], &buf1[5 * kNumLanes],
            &buf1[6 * kNumLanes], &buf0[5 * kNumLanes], &buf0[6 * kNumLanes],
            cos_bit, round);
  for (size_t i = 0; i < 2; ++i) {
    AddSub(int_tag, &buf0[(8 + i) * kNumLanes], &buf1[(11 - i) * kNumLanes],
           &buf0[(8 + i) * kNumLanes], &buf0[(11 - i) * kNumLanes]);
  }
  for (size_t i = 0; i < 2; ++i) {
    AddSub(int_tag, &buf0[(15 - i) * kNumLanes], &buf1[(12 + i) * kNumLanes],
           &buf0[(15 - i) * kNumLanes], &buf0[(12 + i) * kNumLanes]);
  }
  for (size_t i = 0; i < 2; ++i) {
    Butterfly(int_tag, -cospi[16], cospi[48], &buf1[(18 + i) * kNumLanes],
              &buf1[(29 - i) * kNumLanes], &buf0[(18 + i) * kNumLanes],
              &buf0[(29 - i) * kNumLanes], cos_bit, round);
  }
  for (size_t i = 2; i < 4; ++i) {
    Butterfly(int_tag, -cospi[48], -cospi[16], &buf1[(18 + i) * kNumLanes],
              &buf1[(29 - i) * kNumLanes], &buf0[(18 + i) * kNumLanes],
              &buf0[(29 - i) * kNumLanes], cos_bit, round);
  }
  for (size_t i = 0; i < 4; ++i) {
    AddSub(int_tag, &buf0[(32 + i) * kNumLanes], &buf1[(39 - i) * kNumLanes],
           &buf0[(32 + i) * kNumLanes], &buf0[(39 - i) * kNumLanes]);
  }
  for (size_t i = 0; i < 4; ++i) {
    AddSub(int_tag, &buf0[(47 - i) * kNumLanes], &buf1[(40 + i) * kNumLanes],
           &buf0[(47 - i) * kNumLanes], &buf0[(40 + i) * kNumLanes]);
  }
  for (size_t i = 0; i < 4; ++i) {
    AddSub(int_tag, &buf0[(48 + i) * kNumLanes], &buf1[(55 - i) * kNumLanes],
           &buf0[(48 + i) * kNumLanes], &buf0[(55 - i) * kNumLanes]);
  }
  for (size_t i = 0; i < 4; ++i) {
    AddSub(int_tag, &buf0[(63 - i) * kNumLanes], &buf1[(56 + i) * kNumLanes],
           &buf0[(63 - i) * kNumLanes], &buf0[(56 + i) * kNumLanes]);
  }
  // stage 6
  Butterfly(int_tag, cospi[32], cospi[32], &buf0[0 * kNumLanes],
            &buf0[1 * kNumLanes], &buf1[0 * kNumLanes], &buf1[1 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, cospi[16], cospi[48], &buf0[3 * kNumLanes],
            &buf0[2 * kNumLanes], &buf1[2 * kNumLanes], &buf1[3 * kNumLanes],
            cos_bit, round);
  AddSub(int_tag, &buf1[4 * kNumLanes], &buf0[5 * kNumLanes],
         &buf1[4 * kNumLanes], &buf1[5 * kNumLanes]);
  AddSub(int_tag, &buf1[7 * kNumLanes], &buf0[6 * kNumLanes],
         &buf1[7 * kNumLanes], &buf1[6 * kNumLanes]);
  Butterfly(int_tag, -cospi[16], cospi[48], &buf0[9 * kNumLanes],
            &buf0[14 * kNumLanes], &buf1[9 * kNumLanes], &buf1[14 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, -cospi[48], -cospi[16], &buf0[10 * kNumLanes],
            &buf0[13 * kNumLanes], &buf1[10 * kNumLanes], &buf1[13 * kNumLanes],
            cos_bit, round);
  for (size_t i = 0; i < 2; ++i) {
    AddSub(int_tag, &buf1[(16 + i) * kNumLanes], &buf0[(19 - i) * kNumLanes],
           &buf1[(16 + i) * kNumLanes], &buf1[(19 - i) * kNumLanes]);
  }
  for (size_t i = 0; i < 2; ++i) {
    AddSub(int_tag, &buf1[(23 - i) * kNumLanes], &buf0[(20 + i) * kNumLanes],
           &buf1[(23 - i) * kNumLanes], &buf1[(20 + i) * kNumLanes]);
  }
  for (size_t i = 0; i < 2; ++i) {
    AddSub(int_tag, &buf1[(24 + i) * kNumLanes], &buf0[(27 - i) * kNumLanes],
           &buf1[(24 + i) * kNumLanes], &buf1[(27 - i) * kNumLanes]);
  }
  for (size_t i = 0; i < 2; ++i) {
    AddSub(int_tag, &buf1[(31 - i) * kNumLanes], &buf0[(28 + i) * kNumLanes],
           &buf1[(31 - i) * kNumLanes], &buf1[(28 + i) * kNumLanes]);
  }
  for (size_t i = 0; i < 2; ++i) {
    Butterfly(int_tag, -cospi[8], cospi[56], &buf0[(34 + i) * kNumLanes],
              &buf0[(61 - i) * kNumLanes], &buf1[(34 + i) * kNumLanes],
              &buf1[(61 - i) * kNumLanes], cos_bit, round);
  }
  for (size_t i = 2; i < 4; ++i) {
    Butterfly(int_tag, -cospi[56], -cospi[8], &buf0[(34 + i) * kNumLanes],
              &buf0[(61 - i) * kNumLanes], &buf1[(34 + i) * kNumLanes],
              &buf1[(61 - i) * kNumLanes], cos_bit, round);
  }
  for (size_t i = 0; i < 2; ++i) {
    Butterfly(int_tag, -cospi[40], cospi[24], &buf0[(42 + i) * kNumLanes],
              &buf0[(53 - i) * kNumLanes], &buf1[(42 + i) * kNumLanes],
              &buf1[(53 - i) * kNumLanes], cos_bit, round);
  }
  for (size_t i = 2; i < 4; ++i) {
    Butterfly(int_tag, -cospi[24], -cospi[40], &buf0[(42 + i) * kNumLanes],
              &buf0[(53 - i) * kNumLanes], &buf1[(42 + i) * kNumLanes],
              &buf1[(53 - i) * kNumLanes], cos_bit, round);
  }
  // stage 7
  Butterfly(int_tag, cospi[8], cospi[56], &buf1[7 * kNumLanes],
            &buf1[4 * kNumLanes], &buf0[4 * kNumLanes], &buf0[7 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, cospi[40], cospi[24], &buf1[6 * kNumLanes],
            &buf1[5 * kNumLanes], &buf0[5 * kNumLanes], &buf0[6 * kNumLanes],
            cos_bit, round);
  AddSub(int_tag, &buf0[8 * kNumLanes], &buf1[9 * kNumLanes],
         &buf0[8 * kNumLanes], &buf0[9 * kNumLanes]);
  AddSub(int_tag, &buf0[11 * kNumLanes], &buf1[10 * kNumLanes],
         &buf0[11 * kNumLanes], &buf0[10 * kNumLanes]);
  AddSub(int_tag, &buf0[12 * kNumLanes], &buf1[13 * kNumLanes],
         &buf0[12 * kNumLanes], &buf0[13 * kNumLanes]);
  AddSub(int_tag, &buf0[15 * kNumLanes], &buf1[14 * kNumLanes],
         &buf0[15 * kNumLanes], &buf0[14 * kNumLanes]);
  Butterfly(int_tag, -cospi[8], cospi[56], &buf1[17 * kNumLanes],
            &buf1[30 * kNumLanes], &buf0[17 * kNumLanes], &buf0[30 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, -cospi[56], -cospi[8], &buf1[18 * kNumLanes],
            &buf1[29 * kNumLanes], &buf0[18 * kNumLanes], &buf0[29 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, -cospi[40], cospi[24], &buf1[21 * kNumLanes],
            &buf1[26 * kNumLanes], &buf0[21 * kNumLanes], &buf0[26 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, -cospi[24], -cospi[40], &buf1[22 * kNumLanes],
            &buf1[25 * kNumLanes], &buf0[22 * kNumLanes], &buf0[25 * kNumLanes],
            cos_bit, round);
  for (size_t i = 0; i < 2; ++i) {
    AddSub(int_tag, &buf0[(32 + i) * kNumLanes], &buf1[(35 - i) * kNumLanes],
           &buf0[(32 + i) * kNumLanes], &buf0[(35 - i) * kNumLanes]);
  }
  for (size_t i = 0; i < 2; ++i) {
    AddSub(int_tag, &buf0[(39 - i) * kNumLanes], &buf1[(36 + i) * kNumLanes],
           &buf0[(39 - i) * kNumLanes], &buf0[(36 + i) * kNumLanes]);
  }
  for (size_t i = 0; i < 2; ++i) {
    AddSub(int_tag, &buf0[(40 + i) * kNumLanes], &buf1[(43 - i) * kNumLanes],
           &buf0[(40 + i) * kNumLanes], &buf0[(43 - i) * kNumLanes]);
  }
  for (size_t i = 0; i < 2; ++i) {
    AddSub(int_tag, &buf0[(47 - i) * kNumLanes], &buf1[(44 + i) * kNumLanes],
           &buf0[(47 - i) * kNumLanes], &buf0[(44 + i) * kNumLanes]);
  }
  for (size_t i = 0; i < 2; ++i) {
    AddSub(int_tag, &buf0[(48 + i) * kNumLanes], &buf1[(51 - i) * kNumLanes],
           &buf0[(48 + i) * kNumLanes], &buf0[(51 - i) * kNumLanes]);
  }
  for (size_t i = 0; i < 2; ++i) {
    AddSub(int_tag, &buf0[(55 - i) * kNumLanes], &buf1[(52 + i) * kNumLanes],
           &buf0[(55 - i) * kNumLanes], &buf0[(52 + i) * kNumLanes]);
  }
  for (size_t i = 0; i < 2; ++i) {
    AddSub(int_tag, &buf0[(56 + i) * kNumLanes], &buf1[(59 - i) * kNumLanes],
           &buf0[(56 + i) * kNumLanes], &buf0[(59 - i) * kNumLanes]);
  }
  for (size_t i = 0; i < 2; ++i) {
    AddSub(int_tag, &buf0[(63 - i) * kNumLanes], &buf1[(60 + i) * kNumLanes],
           &buf0[(63 - i) * kNumLanes], &buf0[(60 + i) * kNumLanes]);
  }
  // stage 8
  Butterfly(int_tag, cospi[4], cospi[60], &buf0[15 * kNumLanes],
            &buf0[8 * kNumLanes], &buf1[8 * kNumLanes], &buf1[15 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, cospi[36], cospi[28], &buf0[14 * kNumLanes],
            &buf0[9 * kNumLanes], &buf1[9 * kNumLanes], &buf1[14 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, cospi[20], cospi[44], &buf0[13 * kNumLanes],
            &buf0[10 * kNumLanes], &buf1[10 * kNumLanes], &buf1[13 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, cospi[52], cospi[12], &buf0[12 * kNumLanes],
            &buf0[11 * kNumLanes], &buf1[11 * kNumLanes], &buf1[12 * kNumLanes],
            cos_bit, round);
  AddSub(int_tag, &buf1[16 * kNumLanes], &buf0[17 * kNumLanes],
         &buf1[16 * kNumLanes], &buf1[17 * kNumLanes]);
  AddSub(int_tag, &buf1[19 * kNumLanes], &buf0[18 * kNumLanes],
         &buf1[19 * kNumLanes], &buf1[18 * kNumLanes]);
  AddSub(int_tag, &buf1[20 * kNumLanes], &buf0[21 * kNumLanes],
         &buf1[20 * kNumLanes], &buf1[21 * kNumLanes]);
  AddSub(int_tag, &buf1[23 * kNumLanes], &buf0[22 * kNumLanes],
         &buf1[23 * kNumLanes], &buf1[22 * kNumLanes]);
  AddSub(int_tag, &buf1[24 * kNumLanes], &buf0[25 * kNumLanes],
         &buf1[24 * kNumLanes], &buf1[25 * kNumLanes]);
  AddSub(int_tag, &buf1[27 * kNumLanes], &buf0[26 * kNumLanes],
         &buf1[27 * kNumLanes], &buf1[26 * kNumLanes]);
  AddSub(int_tag, &buf1[28 * kNumLanes], &buf0[29 * kNumLanes],
         &buf1[28 * kNumLanes], &buf1[29 * kNumLanes]);
  AddSub(int_tag, &buf1[31 * kNumLanes], &buf0[30 * kNumLanes],
         &buf1[31 * kNumLanes], &buf1[30 * kNumLanes]);
  Butterfly(int_tag, -cospi[4], cospi[60], &buf0[33 * kNumLanes],
            &buf0[62 * kNumLanes], &buf1[33 * kNumLanes], &buf1[62 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, -cospi[60], -cospi[4], &buf0[34 * kNumLanes],
            &buf0[61 * kNumLanes], &buf1[34 * kNumLanes], &buf1[61 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, -cospi[36], cospi[28], &buf0[37 * kNumLanes],
            &buf0[58 * kNumLanes], &buf1[37 * kNumLanes], &buf1[58 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, -cospi[28], -cospi[36], &buf0[38 * kNumLanes],
            &buf0[57 * kNumLanes], &buf1[38 * kNumLanes], &buf1[57 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, -cospi[20], cospi[44], &buf0[41 * kNumLanes],
            &buf0[54 * kNumLanes], &buf1[41 * kNumLanes], &buf1[54 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, -cospi[44], -cospi[20], &buf0[42 * kNumLanes],
            &buf0[53 * kNumLanes], &buf1[42 * kNumLanes], &buf1[53 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, -cospi[52], cospi[12], &buf0[45 * kNumLanes],
            &buf0[50 * kNumLanes], &buf1[45 * kNumLanes], &buf1[50 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, -cospi[12], -cospi[52], &buf0[46 * kNumLanes],
            &buf0[49 * kNumLanes], &buf1[46 * kNumLanes], &buf1[49 * kNumLanes],
            cos_bit, round);
  // stage 9
  Butterfly(int_tag, cospi[2], cospi[62], &buf1[31 * kNumLanes],
            &buf1[16 * kNumLanes], &buf0[16 * kNumLanes], &buf0[31 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, cospi[34], cospi[30], &buf1[30 * kNumLanes],
            &buf1[17 * kNumLanes], &buf0[17 * kNumLanes], &buf0[30 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, cospi[18], cospi[46], &buf1[29 * kNumLanes],
            &buf1[18 * kNumLanes], &buf0[18 * kNumLanes], &buf0[29 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, cospi[50], cospi[14], &buf1[28 * kNumLanes],
            &buf1[19 * kNumLanes], &buf0[19 * kNumLanes], &buf0[28 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, cospi[10], cospi[54], &buf1[27 * kNumLanes],
            &buf1[20 * kNumLanes], &buf0[20 * kNumLanes], &buf0[27 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, cospi[42], cospi[22], &buf1[26 * kNumLanes],
            &buf1[21 * kNumLanes], &buf0[21 * kNumLanes], &buf0[26 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, cospi[26], cospi[38], &buf1[25 * kNumLanes],
            &buf1[22 * kNumLanes], &buf0[22 * kNumLanes], &buf0[25 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, cospi[58], cospi[6], &buf1[24 * kNumLanes],
            &buf1[23 * kNumLanes], &buf0[23 * kNumLanes], &buf0[24 * kNumLanes],
            cos_bit, round);
  AddSub(int_tag, &buf0[32 * kNumLanes], &buf1[33 * kNumLanes],
         &buf0[32 * kNumLanes], &buf0[33 * kNumLanes]);
  AddSub(int_tag, &buf0[35 * kNumLanes], &buf1[34 * kNumLanes],
         &buf0[35 * kNumLanes], &buf0[34 * kNumLanes]);
  AddSub(int_tag, &buf0[36 * kNumLanes], &buf1[37 * kNumLanes],
         &buf0[36 * kNumLanes], &buf0[37 * kNumLanes]);
  AddSub(int_tag, &buf0[39 * kNumLanes], &buf1[38 * kNumLanes],
         &buf0[39 * kNumLanes], &buf0[38 * kNumLanes]);
  AddSub(int_tag, &buf0[40 * kNumLanes], &buf1[41 * kNumLanes],
         &buf0[40 * kNumLanes], &buf0[41 * kNumLanes]);
  AddSub(int_tag, &buf0[43 * kNumLanes], &buf1[42 * kNumLanes],
         &buf0[43 * kNumLanes], &buf0[42 * kNumLanes]);
  AddSub(int_tag, &buf0[44 * kNumLanes], &buf1[45 * kNumLanes],
         &buf0[44 * kNumLanes], &buf0[45 * kNumLanes]);
  AddSub(int_tag, &buf0[47 * kNumLanes], &buf1[46 * kNumLanes],
         &buf0[47 * kNumLanes], &buf0[46 * kNumLanes]);
  AddSub(int_tag, &buf0[48 * kNumLanes], &buf1[49 * kNumLanes],
         &buf0[48 * kNumLanes], &buf0[49 * kNumLanes]);
  AddSub(int_tag, &buf0[51 * kNumLanes], &buf1[50 * kNumLanes],
         &buf0[51 * kNumLanes], &buf0[50 * kNumLanes]);
  AddSub(int_tag, &buf0[52 * kNumLanes], &buf1[53 * kNumLanes],
         &buf0[52 * kNumLanes], &buf0[53 * kNumLanes]);
  AddSub(int_tag, &buf0[55 * kNumLanes], &buf1[54 * kNumLanes],
         &buf0[55 * kNumLanes], &buf0[54 * kNumLanes]);
  AddSub(int_tag, &buf0[56 * kNumLanes], &buf1[57 * kNumLanes],
         &buf0[56 * kNumLanes], &buf0[57 * kNumLanes]);
  AddSub(int_tag, &buf0[59 * kNumLanes], &buf1[58 * kNumLanes],
         &buf0[59 * kNumLanes], &buf0[58 * kNumLanes]);
  AddSub(int_tag, &buf0[60 * kNumLanes], &buf1[61 * kNumLanes],
         &buf0[60 * kNumLanes], &buf0[61 * kNumLanes]);
  AddSub(int_tag, &buf0[63 * kNumLanes], &buf1[62 * kNumLanes],
         &buf0[63 * kNumLanes], &buf0[62 * kNumLanes]);
  // stage 10
  Butterfly(int_tag, cospi[1], cospi[63], &buf0[63 * kNumLanes],
            &buf0[32 * kNumLanes], &buf1[32 * kNumLanes], &buf1[63 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, cospi[33], cospi[31], &buf0[62 * kNumLanes],
            &buf0[33 * kNumLanes], &buf1[33 * kNumLanes], &buf1[62 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, cospi[17], cospi[47], &buf0[61 * kNumLanes],
            &buf0[34 * kNumLanes], &buf1[34 * kNumLanes], &buf1[61 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, cospi[49], cospi[15], &buf0[60 * kNumLanes],
            &buf0[35 * kNumLanes], &buf1[35 * kNumLanes], &buf1[60 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, cospi[9], cospi[55], &buf0[59 * kNumLanes],
            &buf0[36 * kNumLanes], &buf1[36 * kNumLanes], &buf1[59 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, cospi[41], cospi[23], &buf0[58 * kNumLanes],
            &buf0[37 * kNumLanes], &buf1[37 * kNumLanes], &buf1[58 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, cospi[25], cospi[39], &buf0[57 * kNumLanes],
            &buf0[38 * kNumLanes], &buf1[38 * kNumLanes], &buf1[57 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, cospi[57], cospi[7], &buf0[56 * kNumLanes],
            &buf0[39 * kNumLanes], &buf1[39 * kNumLanes], &buf1[56 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, cospi[05], cospi[59], &buf0[55 * kNumLanes],
            &buf0[40 * kNumLanes], &buf1[40 * kNumLanes], &buf1[55 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, cospi[37], cospi[27], &buf0[54 * kNumLanes],
            &buf0[41 * kNumLanes], &buf1[41 * kNumLanes], &buf1[54 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, cospi[21], cospi[43], &buf0[53 * kNumLanes],
            &buf0[42 * kNumLanes], &buf1[42 * kNumLanes], &buf1[53 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, cospi[53], cospi[11], &buf0[52 * kNumLanes],
            &buf0[43 * kNumLanes], &buf1[43 * kNumLanes], &buf1[52 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, cospi[13], cospi[51], &buf0[51 * kNumLanes],
            &buf0[44 * kNumLanes], &buf1[44 * kNumLanes], &buf1[51 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, cospi[45], cospi[19], &buf0[50 * kNumLanes],
            &buf0[45 * kNumLanes], &buf1[45 * kNumLanes], &buf1[50 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, cospi[29], cospi[35], &buf0[49 * kNumLanes],
            &buf0[46 * kNumLanes], &buf1[46 * kNumLanes], &buf1[49 * kNumLanes],
            cos_bit, round);
  Butterfly(int_tag, cospi[61], cospi[3], &buf0[48 * kNumLanes],
            &buf0[47 * kNumLanes], &buf1[47 * kNumLanes], &buf1[48 * kNumLanes],
            cos_bit, round);

  // stage 11
  hwy::CopyBytes<kNumBytes>(&buf1[0 * kNumLanes], &in[0 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[63 * kNumLanes], &in[63 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[32 * kNumLanes], &in[1 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[31 * kNumLanes], &in[62 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf0[16 * kNumLanes], &in[2 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[47 * kNumLanes], &in[61 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[48 * kNumLanes], &in[3 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[15 * kNumLanes], &in[60 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[8 * kNumLanes], &in[4 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[55 * kNumLanes], &in[59 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[40 * kNumLanes], &in[5 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[23 * kNumLanes], &in[58 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf0[24 * kNumLanes], &in[6 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[39 * kNumLanes], &in[57 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[56 * kNumLanes], &in[7 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[7 * kNumLanes], &in[56 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf0[4 * kNumLanes], &in[8 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[59 * kNumLanes], &in[55 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[36 * kNumLanes], &in[9 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[27 * kNumLanes], &in[54 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf0[20 * kNumLanes], &in[10 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[43 * kNumLanes], &in[53 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[52 * kNumLanes], &in[11 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[11 * kNumLanes], &in[52 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[12 * kNumLanes], &in[12 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[51 * kNumLanes], &in[51 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[44 * kNumLanes], &in[13 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[19 * kNumLanes], &in[50 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf0[28 * kNumLanes], &in[14 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[35 * kNumLanes], &in[49 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[60 * kNumLanes], &in[15 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[3 * kNumLanes], &in[48 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[2 * kNumLanes], &in[16 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[61 * kNumLanes], &in[47 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[34 * kNumLanes], &in[17 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[29 * kNumLanes], &in[46 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf0[18 * kNumLanes], &in[18 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[45 * kNumLanes], &in[45 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[50 * kNumLanes], &in[19 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[13 * kNumLanes], &in[44 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[10 * kNumLanes], &in[20 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[53 * kNumLanes], &in[43 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[42 * kNumLanes], &in[21 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[21 * kNumLanes], &in[42 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf0[26 * kNumLanes], &in[22 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[37 * kNumLanes], &in[41 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[58 * kNumLanes], &in[23 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[5 * kNumLanes], &in[40 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf0[6 * kNumLanes], &in[24 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[57 * kNumLanes], &in[39 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[38 * kNumLanes], &in[25 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[25 * kNumLanes], &in[38 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf0[22 * kNumLanes], &in[26 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[41 * kNumLanes], &in[37 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[54 * kNumLanes], &in[27 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[9 * kNumLanes], &in[36 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[14 * kNumLanes], &in[28 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[49 * kNumLanes], &in[35 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[46 * kNumLanes], &in[29 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[17 * kNumLanes], &in[34 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf0[30 * kNumLanes], &in[30 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[33 * kNumLanes], &in[33 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[62 * kNumLanes], &in[31 * OutStride]);
  hwy::CopyBytes<kNumBytes>(&buf1[1 * kNumLanes], &in[32 * OutStride]);
}

template <size_t LaneSize, size_t NumLanes>
struct Fadst4Traits {
  template <size_t Width, typename D>
  HWY_ATTR HWY_INLINE static void Fadst4(D int_tag,
                                         hn::TFromD<D> *HWY_RESTRICT in,
                                         const int8_t cos_bit,
                                         const size_t instride) {
    const int32_t *HWY_RESTRICT const sinpi = sinpi_arr(cos_bit);
    const auto round = hn::Set(int_tag, 1 << (cos_bit - 1));
    const auto sinpi1 = hn::Set(int_tag, sinpi[1]);
    const auto sinpi2 = hn::Set(int_tag, sinpi[2]);
    const auto sinpi3 = hn::Set(int_tag, sinpi[3]);
    const auto sinpi4 = hn::Set(int_tag, sinpi[4]);
    const auto in0 = hn::Load(int_tag, &in[0 * instride]);
    const auto in1 = hn::Load(int_tag, &in[1 * instride]);
    const auto in2 = hn::Load(int_tag, &in[2 * instride]);
    const auto in3 = hn::Load(int_tag, &in[3 * instride]);
    auto s0 = hn::Mul(in0, sinpi1);
    auto s1 = hn::Mul(in0, sinpi4);
    auto s2 = hn::Mul(in1, sinpi2);
    auto s3 = hn::Mul(in1, sinpi1);
    auto s4 = hn::Mul(in2, sinpi3);
    auto s5 = hn::Mul(in3, sinpi4);
    auto s6 = hn::Mul(in3, sinpi2);
    auto s7 = hn::Sub(hn::Add(in0, in1), in3);
    auto x0 = hn::Add(hn::Add(s0, s2), s5);
    auto x1 = hn::Mul(s7, sinpi3);
    auto x2 = hn::Add(hn::Sub(s1, s3), s6);
    auto x3 = s4;
    s0 = hn::Add(x0, x3);
    s1 = x1;
    s2 = hn::Sub(x2, x3);
    s3 = hn::Add(hn::Sub(x2, x0), x3);
    auto u0 = hn::Add(s0, round);
    u0 = hn::ShiftRightSame(u0, cos_bit);
    auto u1 = hn::Add(s1, round);
    u1 = hn::ShiftRightSame(u1, cos_bit);
    auto u2 = hn::Add(s2, round);
    u2 = hn::ShiftRightSame(u2, cos_bit);
    auto u3 = hn::Add(s3, round);
    u3 = hn::ShiftRightSame(u3, cos_bit);
    hn::Store(u0, int_tag, &in[0 * instride]);
    hn::Store(u1, int_tag, &in[1 * instride]);
    hn::Store(u2, int_tag, &in[2 * instride]);
    hn::Store(u3, int_tag, &in[3 * instride]);
  }
};

template <>
struct Fadst4Traits<2, 4> {
  template <size_t Width, typename D>
  HWY_ATTR HWY_INLINE static void Fadst4(D int_tag,
                                         hn::TFromD<D> *HWY_RESTRICT in,
                                         const int8_t cos_bit,
                                         const size_t instride) {
    (void)int_tag;
    const int32_t *HWY_RESTRICT const sinpi = sinpi_arr(cos_bit);
    constexpr hn::FixedTag<hn::TFromD<D>, 8> demote_tag;
    constexpr hn::RepartitionToWide<decltype(demote_tag)> int32_tag;
    const auto round = hn::Set(int32_tag, 1 << (cos_bit - 1));
    const auto sinpi_p01_p02 = SetPair(demote_tag, sinpi[1], sinpi[2]);
    const auto sinpi_p04_m01 = SetPair(demote_tag, sinpi[4], -sinpi[1]);
    const auto sinpi_p03_p04 = SetPair(demote_tag, sinpi[3], sinpi[4]);
    const auto sinpi_m03_p02 = SetPair(demote_tag, -sinpi[3], sinpi[2]);
    const auto sinpi_p03_p03 = hn::Set(demote_tag, sinpi[3]);
    const auto in0 = hn::Load(demote_tag, &in[0 * instride]);
    const auto in1 = hn::Load(demote_tag, &in[1 * instride]);
    const auto in2 = hn::Load(demote_tag, &in[2 * instride]);
    const auto in3 = hn::Load(demote_tag, &in[3 * instride]);
    const auto in7 = hn::Add(in0, in1);
    auto u0 = hn::InterleaveLower(in0, in1);
    auto u1 = hn::InterleaveLower(in2, in3);
    auto u2 = hn::InterleaveLower(in7, hn::Zero(demote_tag));
    auto u3 = hn::InterleaveLower(in2, hn::Zero(demote_tag));
    auto u4 = hn::InterleaveLower(in3, hn::Zero(demote_tag));
    auto v0 = hn::WidenMulPairwiseAdd(int32_tag, u0, sinpi_p01_p02);  // s0 + s2
    auto v1 = hn::WidenMulPairwiseAdd(int32_tag, u1, sinpi_p03_p04);  // s4 + s5
    auto v2 = hn::WidenMulPairwiseAdd(int32_tag, u2, sinpi_p03_p03);  // x1
    auto v3 = hn::WidenMulPairwiseAdd(int32_tag, u0, sinpi_p04_m01);  // s1 - s3
    auto v4 =
        hn::WidenMulPairwiseAdd(int32_tag, u1, sinpi_m03_p02);  // -s4 + s6
    auto v5 = hn::WidenMulPairwiseAdd(int32_tag, u3, sinpi_p03_p03);  // s4
    auto v6 = hn::WidenMulPairwiseAdd(int32_tag, u4, sinpi_p03_p03);
    auto w0 = hn::Add(v0, v1);
    auto w1 = hn::Sub(v2, v6);
    auto w2 = hn::Add(v3, v4);
    auto w3 = hn::Sub(w2, w0);
    auto w4 = hn::ShiftLeft<2>(v5);
    auto w5 = hn::Sub(w4, v5);
    auto w6 = hn::Add(w3, w5);
    v0 = hn::Add(w0, round);
    v1 = hn::Add(w1, round);
    v2 = hn::Add(w2, round);
    v3 = hn::Add(w6, round);
    w0 = hn::ShiftRightSame(v0, cos_bit);
    w1 = hn::ShiftRightSame(v1, cos_bit);
    w2 = hn::ShiftRightSame(v2, cos_bit);
    w3 = hn::ShiftRightSame(v3, cos_bit);
    auto o0 = hn::ReorderDemote2To(demote_tag, w0, w2);
    auto o1 = hn::ReorderDemote2To(demote_tag, w1, w3);
    hn::Store(o0, demote_tag, &in[0 * instride]);
    hn::Store(o1, demote_tag, &in[1 * instride]);
    hn::Store(hn::ShiftRightLanes<4>(demote_tag, o0), demote_tag,
              &in[2 * instride]);
    hn::Store(hn::ShiftRightLanes<4>(demote_tag, o1), demote_tag,
              &in[3 * instride]);
  }
};

template <size_t NumLanes>
struct Fadst4Traits<2, NumLanes> {
  template <size_t Width, typename D>
  HWY_ATTR HWY_INLINE static void Fadst4(D int_tag,
                                         hn::TFromD<D> *HWY_RESTRICT in,
                                         const int8_t cos_bit,
                                         const size_t instride) {
    const int32_t *HWY_RESTRICT const sinpi = sinpi_arr(cos_bit);
    constexpr hn::RepartitionToWide<D> int32_tag;
    const auto round = hn::Set(int32_tag, 1 << (cos_bit - 1));
    const auto sinpi_p01_p02 = SetPair(int_tag, sinpi[1], sinpi[2]);
    const auto sinpi_p04_m01 = SetPair(int_tag, sinpi[4], -sinpi[1]);
    const auto sinpi_p03_p04 = SetPair(int_tag, sinpi[3], sinpi[4]);
    const auto sinpi_m03_p02 = SetPair(int_tag, -sinpi[3], sinpi[2]);
    const auto sinpi_p03_p03 = hn::Set(int_tag, sinpi[3]);
    const auto in0 = hn::Load(int_tag, &in[0 * instride]);
    const auto in1 = hn::Load(int_tag, &in[1 * instride]);
    const auto in2 = hn::Load(int_tag, &in[2 * instride]);
    const auto in3 = hn::Load(int_tag, &in[3 * instride]);
    const auto in7 = hn::Add(in0, in1);
    auto ul0 = hn::InterleaveLower(int_tag, in0, in1);
    auto uh0 = hn::InterleaveUpper(int_tag, in0, in1);
    auto ul1 = hn::InterleaveLower(int_tag, in2, in3);
    auto uh1 = hn::InterleaveUpper(int_tag, in2, in3);
    auto ul2 = hn::InterleaveLower(int_tag, in7, hn::Zero(int_tag));
    auto uh2 = hn::InterleaveUpper(int_tag, in7, hn::Zero(int_tag));
    auto ul3 = hn::InterleaveLower(int_tag, in2, hn::Zero(int_tag));
    auto uh3 = hn::InterleaveUpper(int_tag, in2, hn::Zero(int_tag));
    auto ul4 = hn::InterleaveLower(int_tag, in3, hn::Zero(int_tag));
    auto uh4 = hn::InterleaveUpper(int_tag, in3, hn::Zero(int_tag));
    auto vl0 =
        hn::WidenMulPairwiseAdd(int32_tag, ul0, sinpi_p01_p02);  // s0 + s2
    auto vh0 =
        hn::WidenMulPairwiseAdd(int32_tag, uh0, sinpi_p01_p02);  // s0 + s2
    auto vl1 =
        hn::WidenMulPairwiseAdd(int32_tag, ul1, sinpi_p03_p04);  // s4 + s5
    auto vh1 =
        hn::WidenMulPairwiseAdd(int32_tag, uh1, sinpi_p03_p04);  // s4 + s5
    auto vl2 = hn::WidenMulPairwiseAdd(int32_tag, ul2, sinpi_p03_p03);  // x1
    auto vh2 = hn::WidenMulPairwiseAdd(int32_tag, uh2, sinpi_p03_p03);  // x1
    auto vl3 =
        hn::WidenMulPairwiseAdd(int32_tag, ul0, sinpi_p04_m01);  // s1 - s3
    auto vh3 =
        hn::WidenMulPairwiseAdd(int32_tag, uh0, sinpi_p04_m01);  // s1 - s3
    auto vl4 =
        hn::WidenMulPairwiseAdd(int32_tag, ul1, sinpi_m03_p02);  // -s4 + s6
    auto vh4 =
        hn::WidenMulPairwiseAdd(int32_tag, uh1, sinpi_m03_p02);  // -s4 + s6
    auto vl5 = hn::WidenMulPairwiseAdd(int32_tag, ul3, sinpi_p03_p03);  // s4
    auto vh5 = hn::WidenMulPairwiseAdd(int32_tag, uh3, sinpi_p03_p03);  // s4
    auto vl6 = hn::WidenMulPairwiseAdd(int32_tag, ul4, sinpi_p03_p03);
    auto vh6 = hn::WidenMulPairwiseAdd(int32_tag, uh4, sinpi_p03_p03);
    auto wl0 = hn::Add(vl0, vl1);
    auto wh0 = hn::Add(vh0, vh1);
    auto wl1 = hn::Sub(vl2, vl6);
    auto wh1 = hn::Sub(vh2, vh6);
    auto wl2 = hn::Add(vl3, vl4);
    auto wh2 = hn::Add(vh3, vh4);
    auto wl3 = hn::Sub(wl2, wl0);
    auto wh3 = hn::Sub(wh2, wh0);
    auto wl4 = hn::ShiftLeft<2>(vl5);
    auto wh4 = hn::ShiftLeft<2>(vh5);
    auto wl5 = hn::Sub(wl4, vl5);
    auto wh5 = hn::Sub(wh4, vh5);
    auto wl6 = hn::Add(wl3, wl5);
    auto wh6 = hn::Add(wh3, wh5);
    vl0 = hn::Add(wl0, round);
    vh0 = hn::Add(wh0, round);
    vl1 = hn::Add(wl1, round);
    vh1 = hn::Add(wh1, round);
    vl2 = hn::Add(wl2, round);
    vh2 = hn::Add(wh2, round);
    vl3 = hn::Add(wl6, round);
    vh3 = hn::Add(wh6, round);
    wl0 = hn::ShiftRightSame(vl0, cos_bit);
    wh0 = hn::ShiftRightSame(vh0, cos_bit);
    wl1 = hn::ShiftRightSame(vl1, cos_bit);
    wh1 = hn::ShiftRightSame(vh1, cos_bit);
    wl2 = hn::ShiftRightSame(vl2, cos_bit);
    wh2 = hn::ShiftRightSame(vh2, cos_bit);
    wl3 = hn::ShiftRightSame(vl3, cos_bit);
    wh3 = hn::ShiftRightSame(vh3, cos_bit);
    auto o0 = hn::ReorderDemote2To(int_tag, wl0, wh0);
    auto o1 = hn::ReorderDemote2To(int_tag, wl1, wh1);
    auto o2 = hn::ReorderDemote2To(int_tag, wl2, wh2);
    auto o3 = hn::ReorderDemote2To(int_tag, wl3, wh3);
    hn::Store(o0, int_tag, &in[0 * instride]);
    hn::Store(o1, int_tag, &in[1 * instride]);
    hn::Store(o2, int_tag, &in[2 * instride]);
    hn::Store(o3, int_tag, &in[3 * instride]);
  }
};

template <size_t Width, typename D>
HWY_ATTR HWY_INLINE void Fadst4(D int_tag, hn::TFromD<D> *HWY_RESTRICT in,
                                const int8_t cos_bit, const size_t instride) {
  Fadst4Traits<sizeof(hn::TFromD<D>),
               hn::MaxLanes(int_tag)>::template Fadst4<Width>(int_tag, in,
                                                              cos_bit,
                                                              instride);
}

template <size_t Width, typename D>
HWY_ATTR HWY_INLINE void Fadst8(D int_tag, hn::TFromD<D> *HWY_RESTRICT in,
                                const int8_t cos_bit, const size_t instride) {
  constexpr size_t kNumLanes = hn::MaxLanes(int_tag);
  constexpr size_t kNumBytes = kNumLanes * sizeof(hn::TFromD<D>);
  HWY_ALIGN_MAX hn::TFromD<D> buf0[8 * kNumLanes];
  HWY_ALIGN_MAX hn::TFromD<D> buf1[8 * kNumLanes];
  const int32_t *HWY_RESTRICT cospi = cospi_arr(cos_bit);
  const auto round = hn::Set(hn::Repartition<int32_t, D>(), 1 << (cos_bit - 1));

  // stage 0
  // stage 1
  hn::Store(hn::Load(int_tag, &in[0 * instride]), int_tag,
            &buf0[0 * kNumLanes]);
  hn::Store(hn::Neg(hn::Load(int_tag, &in[7 * instride])), int_tag,
            &buf0[1 * kNumLanes]);
  hn::Store(hn::Neg(hn::Load(int_tag, &in[3 * instride])), int_tag,
            &buf0[2 * kNumLanes]);
  hn::Store(hn::Load(int_tag, &in[4 * instride]), int_tag,
            &buf0[3 * kNumLanes]);
  hn::Store(hn::Neg(hn::Load(int_tag, &in[1 * instride])), int_tag,
            &buf0[4 * kNumLanes]);
  hn::Store(hn::Load(int_tag, &in[6 * instride]), int_tag,
            &buf0[5 * kNumLanes]);
  hn::Store(hn::Load(int_tag, &in[2 * instride]), int_tag,
            &buf0[6 * kNumLanes]);
  hn::Store(hn::Neg(hn::Load(int_tag, &in[5 * instride])), int_tag,
            &buf0[7 * kNumLanes]);

  // stage 2
  hwy::CopyBytes<2 * kNumBytes>(&buf0[0 * kNumLanes], &buf1[0 * kNumLanes]);
  Butterfly(int_tag, cospi[32], cospi[32], &buf0[2 * kNumLanes],
            &buf0[3 * kNumLanes], &buf1[2 * kNumLanes], &buf1[3 * kNumLanes],
            cos_bit, round);
  hwy::CopyBytes<2 * kNumBytes>(&buf0[4 * kNumLanes], &buf1[4 * kNumLanes]);
  Butterfly(int_tag, cospi[32], cospi[32], &buf0[6 * kNumLanes],
            &buf0[7 * kNumLanes], &buf1[6 * kNumLanes], &buf1[7 * kNumLanes],
            cos_bit, round);

  // stage 3
  for (size_t j = 0; j < 8; j += 4) {
    for (size_t i = 0; i < 2; ++i) {
      AddSub(int_tag, &buf1[(0 + i + j) * kNumLanes],
             &buf1[(2 + i + j) * kNumLanes], &buf0[(0 + i + j) * kNumLanes],
             &buf0[(2 + i + j) * kNumLanes]);
    }
  }

  // stage 4
  hwy::CopyBytes<4 * kNumBytes>(&buf0[0 * kNumLanes], &buf1[0 * kNumLanes]);
  HalfButterfly(int_tag, cospi[16], cospi[48], &buf0[4 * kNumLanes],
                &buf0[5 * kNumLanes], &buf1[4 * kNumLanes], cos_bit, round);
  HalfButterfly(int_tag, cospi[48], -cospi[16], &buf0[4 * kNumLanes],
                &buf0[5 * kNumLanes], &buf1[5 * kNumLanes], cos_bit, round);
  HalfButterfly(int_tag, -cospi[48], cospi[16], &buf0[6 * kNumLanes],
                &buf0[7 * kNumLanes], &buf1[6 * kNumLanes], cos_bit, round);
  HalfButterfly(int_tag, cospi[16], cospi[48], &buf0[6 * kNumLanes],
                &buf0[7 * kNumLanes], &buf1[7 * kNumLanes], cos_bit, round);

  // stage 5
  for (size_t i = 0; i < 4; ++i) {
    AddSub(int_tag, &buf1[(0 + i) * kNumLanes], &buf1[(4 + i) * kNumLanes],
           &buf0[(0 + i) * kNumLanes], &buf0[(4 + i) * kNumLanes]);
  }

  // stage 6
  HalfButterfly(int_tag, cospi[4], cospi[60], &buf0[0 * kNumLanes],
                &buf0[1 * kNumLanes], &buf1[0 * kNumLanes], cos_bit, round);
  HalfButterfly(int_tag, cospi[60], -cospi[4], &buf0[0 * kNumLanes],
                &buf0[1 * kNumLanes], &buf1[1 * kNumLanes], cos_bit, round);
  HalfButterfly(int_tag, cospi[20], cospi[44], &buf0[2 * kNumLanes],
                &buf0[3 * kNumLanes], &buf1[2 * kNumLanes], cos_bit, round);
  HalfButterfly(int_tag, cospi[44], -cospi[20], &buf0[2 * kNumLanes],
                &buf0[3 * kNumLanes], &buf1[3 * kNumLanes], cos_bit, round);
  HalfButterfly(int_tag, cospi[36], cospi[28], &buf0[4 * kNumLanes],
                &buf0[5 * kNumLanes], &buf1[4 * kNumLanes], cos_bit, round);
  HalfButterfly(int_tag, cospi[28], -cospi[36], &buf0[4 * kNumLanes],
                &buf0[5 * kNumLanes], &buf1[5 * kNumLanes], cos_bit, round);
  HalfButterfly(int_tag, cospi[52], cospi[12], &buf0[6 * kNumLanes],
                &buf0[7 * kNumLanes], &buf1[6 * kNumLanes], cos_bit, round);
  HalfButterfly(int_tag, cospi[12], -cospi[52], &buf0[6 * kNumLanes],
                &buf0[7 * kNumLanes], &buf1[7 * kNumLanes], cos_bit, round);

  // stage 7
  hwy::CopyBytes<kNumBytes>(&buf1[1 * kNumLanes], &in[0 * instride]);
  hwy::CopyBytes<kNumBytes>(&buf1[6 * kNumLanes], &in[1 * instride]);
  hwy::CopyBytes<kNumBytes>(&buf1[3 * kNumLanes], &in[2 * instride]);
  hwy::CopyBytes<kNumBytes>(&buf1[4 * kNumLanes], &in[3 * instride]);
  hwy::CopyBytes<kNumBytes>(&buf1[5 * kNumLanes], &in[4 * instride]);
  hwy::CopyBytes<kNumBytes>(&buf1[2 * kNumLanes], &in[5 * instride]);
  hwy::CopyBytes<kNumBytes>(&buf1[7 * kNumLanes], &in[6 * instride]);
  hwy::CopyBytes<kNumBytes>(&buf1[0 * kNumLanes], &in[7 * instride]);
}

template <size_t Width, typename D>
HWY_ATTR HWY_INLINE void Fadst16(D int_tag, hn::TFromD<D> *HWY_RESTRICT in,
                                 const int8_t cos_bit, const size_t instride) {
  constexpr size_t kNumLanes = hn::MaxLanes(int_tag);
  constexpr size_t kNumBytes = kNumLanes * sizeof(hn::TFromD<D>);
  HWY_ALIGN_MAX hn::TFromD<D> buf0[16 * kNumLanes];
  HWY_ALIGN_MAX hn::TFromD<D> buf1[16 * kNumLanes];
  const int32_t *HWY_RESTRICT const cospi = cospi_arr(cos_bit);
  const auto round = hn::Set(hn::Repartition<int32_t, D>(), 1 << (cos_bit - 1));

  // stage 0
  // stage 1
  hn::Store(hn::Load(int_tag, &in[0 * instride]), int_tag,
            &buf0[0 * kNumLanes]);
  hn::Store(hn::Neg(hn::Load(int_tag, &in[15 * instride])), int_tag,
            &buf0[1 * kNumLanes]);
  hn::Store(hn::Neg(hn::Load(int_tag, &in[7 * instride])), int_tag,
            &buf0[2 * kNumLanes]);
  hn::Store(hn::Load(int_tag, &in[8 * instride]), int_tag,
            &buf0[3 * kNumLanes]);
  hn::Store(hn::Neg(hn::Load(int_tag, &in[3 * instride])), int_tag,
            &buf0[4 * kNumLanes]);
  hn::Store(hn::Load(int_tag, &in[12 * instride]), int_tag,
            &buf0[5 * kNumLanes]);
  hn::Store(hn::Load(int_tag, &in[4 * instride]), int_tag,
            &buf0[6 * kNumLanes]);
  hn::Store(hn::Neg(hn::Load(int_tag, &in[11 * instride])), int_tag,
            &buf0[7 * kNumLanes]);
  hn::Store(hn::Neg(hn::Load(int_tag, &in[1 * instride])), int_tag,
            &buf0[8 * kNumLanes]);
  hn::Store(hn::Load(int_tag, &in[14 * instride]), int_tag,
            &buf0[9 * kNumLanes]);
  hn::Store(hn::Load(int_tag, &in[6 * instride]), int_tag,
            &buf0[10 * kNumLanes]);
  hn::Store(hn::Neg(hn::Load(int_tag, &in[9 * instride])), int_tag,
            &buf0[11 * kNumLanes]);
  hn::Store(hn::Load(int_tag, &in[2 * instride]), int_tag,
            &buf0[12 * kNumLanes]);
  hn::Store(hn::Neg(hn::Load(int_tag, &in[13 * instride])), int_tag,
            &buf0[13 * kNumLanes]);
  hn::Store(hn::Neg(hn::Load(int_tag, &in[5 * instride])), int_tag,
            &buf0[14 * kNumLanes]);
  hn::Store(hn::Load(int_tag, &in[10 * instride]), int_tag,
            &buf0[15 * kNumLanes]);

  // stage 2
  hwy::CopyBytes<kNumBytes * 2>(&buf0[0 * kNumLanes], &buf1[0 * kNumLanes]);
  Butterfly(int_tag, cospi[32], cospi[32], &buf0[2 * kNumLanes],
            &buf0[3 * kNumLanes], &buf1[2 * kNumLanes], &buf1[3 * kNumLanes],
            cos_bit, round);
  hwy::CopyBytes<kNumBytes * 2>(&buf0[4 * kNumLanes], &buf1[4 * kNumLanes]);
  Butterfly(int_tag, cospi[32], cospi[32], &buf0[6 * kNumLanes],
            &buf0[7 * kNumLanes], &buf1[6 * kNumLanes], &buf1[7 * kNumLanes],
            cos_bit, round);
  hwy::CopyBytes<kNumBytes * 2>(&buf0[8 * kNumLanes], &buf1[8 * kNumLanes]);
  Butterfly(int_tag, cospi[32], cospi[32], &buf0[10 * kNumLanes],
            &buf0[11 * kNumLanes], &buf1[10 * kNumLanes], &buf1[11 * kNumLanes],
            cos_bit, round);
  hwy::CopyBytes<kNumBytes * 2>(&buf0[12 * kNumLanes], &buf1[12 * kNumLanes]);
  Butterfly(int_tag, cospi[32], cospi[32], &buf0[14 * kNumLanes],
            &buf0[15 * kNumLanes], &buf1[14 * kNumLanes], &buf1[15 * kNumLanes],
            cos_bit, round);

  // stage 3
  for (size_t j = 0; j < 16; j += 4) {
    for (size_t i = 0; i < 2; ++i) {
      AddSub(int_tag, &buf1[(0 + i + j) * kNumLanes],
             &buf1[(2 + i + j) * kNumLanes], &buf0[(0 + i + j) * kNumLanes],
             &buf0[(2 + i + j) * kNumLanes]);
    }
  }

  // stage 4
  hwy::CopyBytes<kNumBytes * 4>(&buf0[0 * kNumLanes], &buf1[0 * kNumLanes]);
  HalfButterfly(int_tag, cospi[16], cospi[48], &buf0[4 * kNumLanes],
                &buf0[5 * kNumLanes], &buf1[4 * kNumLanes], cos_bit, round);
  HalfButterfly(int_tag, cospi[48], -cospi[16], &buf0[4 * kNumLanes],
                &buf0[5 * kNumLanes], &buf1[5 * kNumLanes], cos_bit, round);
  HalfButterfly(int_tag, -cospi[48], cospi[16], &buf0[6 * kNumLanes],
                &buf0[7 * kNumLanes], &buf1[6 * kNumLanes], cos_bit, round);
  HalfButterfly(int_tag, cospi[16], cospi[48], &buf0[6 * kNumLanes],
                &buf0[7 * kNumLanes], &buf1[7 * kNumLanes], cos_bit, round);
  hwy::CopyBytes<kNumBytes * 4>(&buf0[8 * kNumLanes], &buf1[8 * kNumLanes]);
  HalfButterfly(int_tag, cospi[16], cospi[48], &buf0[12 * kNumLanes],
                &buf0[13 * kNumLanes], &buf1[12 * kNumLanes], cos_bit, round);
  HalfButterfly(int_tag, cospi[48], -cospi[16], &buf0[12 * kNumLanes],
                &buf0[13 * kNumLanes], &buf1[13 * kNumLanes], cos_bit, round);
  HalfButterfly(int_tag, -cospi[48], cospi[16], &buf0[14 * kNumLanes],
                &buf0[15 * kNumLanes], &buf1[14 * kNumLanes], cos_bit, round);
  HalfButterfly(int_tag, cospi[16], cospi[48], &buf0[14 * kNumLanes],
                &buf0[15 * kNumLanes], &buf1[15 * kNumLanes], cos_bit, round);

  // stage 5
  for (size_t j = 0; j < 16; j += 8) {
    for (size_t i = 0; i < 4; ++i) {
      AddSub(int_tag, &buf1[(0 + i + j) * kNumLanes],
             &buf1[(4 + i + j) * kNumLanes], &buf0[(0 + i + j) * kNumLanes],
             &buf0[(4 + i + j) * kNumLanes]);
    }
  }

  // stage 6
  hwy::CopyBytes<kNumBytes * 8>(&buf0[0 * kNumLanes], &buf1[0 * kNumLanes]);
  HalfButterfly(int_tag, cospi[8], cospi[56], &buf0[8 * kNumLanes],
                &buf0[9 * kNumLanes], &buf1[8 * kNumLanes], cos_bit, round);
  HalfButterfly(int_tag, cospi[56], -cospi[8], &buf0[8 * kNumLanes],
                &buf0[9 * kNumLanes], &buf1[9 * kNumLanes], cos_bit, round);
  HalfButterfly(int_tag, cospi[40], cospi[24], &buf0[10 * kNumLanes],
                &buf0[11 * kNumLanes], &buf1[10 * kNumLanes], cos_bit, round);
  HalfButterfly(int_tag, cospi[24], -cospi[40], &buf0[10 * kNumLanes],
                &buf0[11 * kNumLanes], &buf1[11 * kNumLanes], cos_bit, round);
  HalfButterfly(int_tag, -cospi[56], cospi[8], &buf0[12 * kNumLanes],
                &buf0[13 * kNumLanes], &buf1[12 * kNumLanes], cos_bit, round);
  HalfButterfly(int_tag, cospi[8], cospi[56], &buf0[12 * kNumLanes],
                &buf0[13 * kNumLanes], &buf1[13 * kNumLanes], cos_bit, round);
  HalfButterfly(int_tag, -cospi[24], cospi[40], &buf0[14 * kNumLanes],
                &buf0[15 * kNumLanes], &buf1[14 * kNumLanes], cos_bit, round);
  HalfButterfly(int_tag, cospi[40], cospi[24], &buf0[14 * kNumLanes],
                &buf0[15 * kNumLanes], &buf1[15 * kNumLanes], cos_bit, round);

  // stage 7
  for (size_t i = 0; i < 8; ++i) {
    AddSub(int_tag, &buf1[(0 + i) * kNumLanes], &buf1[(8 + i) * kNumLanes],
           &buf0[(0 + i) * kNumLanes], &buf0[(8 + i) * kNumLanes]);
  }

  // stage 8
  HalfButterfly(int_tag, cospi[2], cospi[62], &buf0[0 * kNumLanes],
                &buf0[1 * kNumLanes], &buf1[0 * kNumLanes], cos_bit, round);
  HalfButterfly(int_tag, cospi[62], -cospi[2], &buf0[0 * kNumLanes],
                &buf0[1 * kNumLanes], &buf1[1 * kNumLanes], cos_bit, round);
  HalfButterfly(int_tag, cospi[10], cospi[54], &buf0[2 * kNumLanes],
                &buf0[3 * kNumLanes], &buf1[2 * kNumLanes], cos_bit, round);
  HalfButterfly(int_tag, cospi[54], -cospi[10], &buf0[2 * kNumLanes],
                &buf0[3 * kNumLanes], &buf1[3 * kNumLanes], cos_bit, round);
  HalfButterfly(int_tag, cospi[18], cospi[46], &buf0[4 * kNumLanes],
                &buf0[5 * kNumLanes], &buf1[4 * kNumLanes], cos_bit, round);
  HalfButterfly(int_tag, cospi[46], -cospi[18], &buf0[4 * kNumLanes],
                &buf0[5 * kNumLanes], &buf1[5 * kNumLanes], cos_bit, round);
  HalfButterfly(int_tag, cospi[26], cospi[38], &buf0[6 * kNumLanes],
                &buf0[7 * kNumLanes], &buf1[6 * kNumLanes], cos_bit, round);
  HalfButterfly(int_tag, cospi[38], -cospi[26], &buf0[6 * kNumLanes],
                &buf0[7 * kNumLanes], &buf1[7 * kNumLanes], cos_bit, round);
  HalfButterfly(int_tag, cospi[34], cospi[30], &buf0[8 * kNumLanes],
                &buf0[9 * kNumLanes], &buf1[8 * kNumLanes], cos_bit, round);
  HalfButterfly(int_tag, cospi[30], -cospi[34], &buf0[8 * kNumLanes],
                &buf0[9 * kNumLanes], &buf1[9 * kNumLanes], cos_bit, round);
  HalfButterfly(int_tag, cospi[42], cospi[22], &buf0[10 * kNumLanes],
                &buf0[11 * kNumLanes], &buf1[10 * kNumLanes], cos_bit, round);
  HalfButterfly(int_tag, cospi[22], -cospi[42], &buf0[10 * kNumLanes],
                &buf0[11 * kNumLanes], &buf1[11 * kNumLanes], cos_bit, round);
  HalfButterfly(int_tag, cospi[50], cospi[14], &buf0[12 * kNumLanes],
                &buf0[13 * kNumLanes], &buf1[12 * kNumLanes], cos_bit, round);
  HalfButterfly(int_tag, cospi[14], -cospi[50], &buf0[12 * kNumLanes],
                &buf0[13 * kNumLanes], &buf1[13 * kNumLanes], cos_bit, round);
  HalfButterfly(int_tag, cospi[58], cospi[6], &buf0[14 * kNumLanes],
                &buf0[15 * kNumLanes], &buf1[14 * kNumLanes], cos_bit, round);
  HalfButterfly(int_tag, cospi[6], -cospi[58], &buf0[14 * kNumLanes],
                &buf0[15 * kNumLanes], &buf1[15 * kNumLanes], cos_bit, round);

  // stage 9
  hwy::CopyBytes<kNumBytes>(&buf1[1 * kNumLanes], &in[0 * instride]);
  hwy::CopyBytes<kNumBytes>(&buf1[14 * kNumLanes], &in[1 * instride]);
  hwy::CopyBytes<kNumBytes>(&buf1[3 * kNumLanes], &in[2 * instride]);
  hwy::CopyBytes<kNumBytes>(&buf1[12 * kNumLanes], &in[3 * instride]);
  hwy::CopyBytes<kNumBytes>(&buf1[5 * kNumLanes], &in[4 * instride]);
  hwy::CopyBytes<kNumBytes>(&buf1[10 * kNumLanes], &in[5 * instride]);
  hwy::CopyBytes<kNumBytes>(&buf1[7 * kNumLanes], &in[6 * instride]);
  hwy::CopyBytes<kNumBytes>(&buf1[8 * kNumLanes], &in[7 * instride]);
  hwy::CopyBytes<kNumBytes>(&buf1[9 * kNumLanes], &in[8 * instride]);
  hwy::CopyBytes<kNumBytes>(&buf1[6 * kNumLanes], &in[9 * instride]);
  hwy::CopyBytes<kNumBytes>(&buf1[11 * kNumLanes], &in[10 * instride]);
  hwy::CopyBytes<kNumBytes>(&buf1[4 * kNumLanes], &in[11 * instride]);
  hwy::CopyBytes<kNumBytes>(&buf1[13 * kNumLanes], &in[12 * instride]);
  hwy::CopyBytes<kNumBytes>(&buf1[2 * kNumLanes], &in[13 * instride]);
  hwy::CopyBytes<kNumBytes>(&buf1[15 * kNumLanes], &in[14 * instride]);
  hwy::CopyBytes<kNumBytes>(&buf1[0 * kNumLanes], &in[15 * instride]);
}

template <size_t Width, typename D>
HWY_ATTR HWY_INLINE void IdtxAdd2(D tag, hn::TFromD<D> *HWY_RESTRICT in) {
  for (size_t x = 0; x < Width; x += hn::MaxLanes(tag)) {
    auto v = hn::Load(tag, &in[x]);
    hn::Store(hn::Add(v, v), tag, &in[x]);
  }
}

template <size_t Width, int Shift, typename D>
HWY_ATTR HWY_INLINE void IdtxShift(D tag, hn::TFromD<D> *HWY_RESTRICT in) {
  for (size_t x = 0; x < Width; x += hn::MaxLanes(tag)) {
    hn::Store(hn::ShiftLeft<Shift>(hn::Load(tag, &in[x])), tag, &in[x]);
  }
}

template <int Scale, typename D>
HWY_ATTR HWY_INLINE void PromoteScale2x16ByNewSqrt2(
    D tag, hn::VFromD<D> v, hn::VFromD<hn::RepartitionToWide<D>> &out0,
    hn::VFromD<hn::RepartitionToWide<D>> &out1) {
  constexpr hn::RepartitionToWide<D> int32_tag;
  auto one = hn::Set(tag, 1);
  auto scale_rounding = SetPair(tag, Scale * NewSqrt2, 1 << (NewSqrt2Bits - 1));
  auto a0 = hn::InterleaveLower(tag, v, one);
  auto a1 = hn::InterleaveUpper(tag, v, one);
  out0 = hn::ShiftRight<NewSqrt2Bits>(
      hn::WidenMulPairwiseAdd(int32_tag, a0, scale_rounding));
  out1 = hn::ShiftRight<NewSqrt2Bits>(
      hn::WidenMulPairwiseAdd(int32_tag, a1, scale_rounding));
}

template <size_t LaneSize, size_t NumLanes>
struct ScaleByNewSqrt2Traits {
  template <int Scale, typename D>
  HWY_ATTR HWY_INLINE static hn::VFromD<D> ScaleByNewSqrt2(D tag,
                                                           hn::VFromD<D> v) {
    auto fact = hn::Set(tag, Scale * NewSqrt2);
    auto offset = hn::Set(tag, 1 << (NewSqrt2Bits - 1));
    return hn::ShiftRight<NewSqrt2Bits>(hn::MulAdd(v, fact, offset));
  }
};

template <>
struct ScaleByNewSqrt2Traits<2, 4> {
  template <int Scale, typename D>
  HWY_ATTR HWY_INLINE static hn::VFromD<D> ScaleByNewSqrt2(D tag,
                                                           hn::VFromD<D> v) {
    auto one = hn::Set(tag, 1);
    auto scale_rounding =
        SetPair(tag, Scale * NewSqrt2, 1 << (NewSqrt2Bits - 1));
    constexpr hn::Rebind<int32_t, D> int32_tag;
    auto a = hn::InterleaveLower(tag, v, one);
    auto b = hn::ShiftRight<NewSqrt2Bits>(
        hn::WidenMulPairwiseAdd(int32_tag, a, scale_rounding));
    return hn::DemoteTo(tag, b);
  }
};

template <size_t NumLanes>
struct ScaleByNewSqrt2Traits<2, NumLanes> {
  template <int Scale, typename D>
  HWY_ATTR HWY_INLINE static hn::VFromD<D> ScaleByNewSqrt2(D tag,
                                                           hn::VFromD<D> v) {
    hn::VFromD<hn::RepartitionToWide<D>> b0, b1;
    PromoteScale2x16ByNewSqrt2<Scale>(tag, v, b0, b1);
    return hn::ReorderDemote2To(tag, b0, b1);
  }
};

template <int Scale, typename D>
HWY_ATTR HWY_INLINE hn::VFromD<D> ScaleByNewSqrt2(D tag, hn::VFromD<D> v) {
  return ScaleByNewSqrt2Traits<sizeof(hn::TFromD<D>), hn::MaxLanes(tag)>::
      template ScaleByNewSqrt2<Scale>(tag, v);
}

template <size_t Width, int Scale, typename D>
HWY_ATTR HWY_INLINE void IdtxSqrt2(D tag, hn::TFromD<D> *HWY_RESTRICT in) {
  for (size_t x = 0; x < Width; x += hn::MaxLanes(tag)) {
    hn::Store(ScaleByNewSqrt2<Scale>(tag, hn::Load(tag, &in[x])), tag, &in[x]);
  }
}

template <size_t Width, size_t Stride, typename T>
HWY_ATTR void FdctNx4Block(T *HWY_RESTRICT in, int8_t cos_bit) {
  constexpr auto int_tag = hn::CappedTag<T, Width>();
  for (size_t i = 0; i < Width; i += hn::MaxLanes(int_tag)) {
    Fdct4(int_tag, &in[i], cos_bit, Stride);
  }
}

template <size_t Width, size_t Stride, typename T>
HWY_ATTR void FdctNx8Block(T *HWY_RESTRICT in, int8_t cos_bit) {
  constexpr auto int_tag = hn::CappedTag<T, Stride>();
  for (size_t i = 0; i < Width; i += hn::MaxLanes(int_tag)) {
    Fdct8(int_tag, &in[i], cos_bit, Stride);
  }
}

template <size_t Width, size_t Stride, typename T>
HWY_ATTR void FdctNx16Block(T *HWY_RESTRICT in, int8_t cos_bit) {
  constexpr auto int_tag = hn::CappedTag<T, Stride>();
  for (size_t i = 0; i < Width; i += hn::MaxLanes(int_tag)) {
    Fdct16(int_tag, &in[i], cos_bit, Stride);
  }
}

template <size_t Width, size_t Stride, typename T>
HWY_ATTR void FdctNx32Block(T *HWY_RESTRICT in, int8_t cos_bit) {
  constexpr auto int_tag = hn::CappedTag<T, Stride>();
  for (size_t i = 0; i < Width; i += hn::MaxLanes(int_tag)) {
    Fdct32(int_tag, &in[i], cos_bit, Stride);
  }
}

template <size_t InWidth, size_t InStride, size_t OutWidth, size_t OutStride,
          typename T>
HWY_ATTR void FdctNx64Block(T *HWY_RESTRICT in, int8_t cos_bit) {
  constexpr auto int_tag = hn::CappedTag<T, InWidth>();
  for (size_t i = 0; i < OutWidth; i += hn::MaxLanes(int_tag)) {
    Fdct64<InStride, OutStride>(int_tag, &in[i], cos_bit);
  }
}

template <size_t Width, size_t Stride, typename T>
HWY_ATTR HWY_INLINE void FadstNx4Block(T *HWY_RESTRICT in, int8_t cos_bit) {
  constexpr auto int_tag = hn::CappedTag<T, Width>();
  for (size_t i = 0; i < Width; i += hn::MaxLanes(int_tag)) {
    Fadst4<Width>(int_tag, &in[i], cos_bit, Stride);
  }
}

template <size_t Width, size_t Stride, typename T>
HWY_ATTR void FadstNx8Block(T *HWY_RESTRICT in, int8_t cos_bit) {
  constexpr auto int_tag = hn::CappedTag<T, Stride>();
  for (size_t i = 0; i < Width; i += hn::MaxLanes(int_tag)) {
    Fadst8<Width>(int_tag, &in[i], cos_bit, Stride);
  }
}

template <size_t Width, size_t Stride, typename T>
HWY_ATTR void FadstNx16Block(T *HWY_RESTRICT in, int8_t cos_bit) {
  constexpr auto int_tag = hn::CappedTag<T, Stride>();
  for (size_t i = 0; i < Width; i += hn::MaxLanes(int_tag)) {
    Fadst16<Width>(int_tag, &in[i], cos_bit, Stride);
  }
}

template <size_t Width, size_t Stride, size_t BlockHeight, typename T>
HWY_ATTR void IdtxAdd2Block(T *HWY_RESTRICT in, int8_t cos_bit) {
  (void)cos_bit;
  constexpr auto int_tag = hn::CappedTag<T, Width>();
  for (size_t y = 0; y < BlockHeight; ++y) {
    IdtxAdd2<Width>(int_tag, &in[y * Stride]);
  }
}

template <size_t Width, size_t Stride, size_t BlockHeight, int Scale,
          typename T>
HWY_ATTR void IdtxSqrt2Block(T *HWY_RESTRICT in, int8_t cos_bit) {
  (void)cos_bit;
  constexpr auto int_tag = hn::CappedTag<T, Width>();
  for (size_t y = 0; y < BlockHeight; ++y) {
    IdtxSqrt2<Width, Scale>(int_tag, &in[y * Stride]);
  }
}

template <size_t Width, size_t Stride, size_t BlockHeight, int Shift,
          typename T>
HWY_ATTR void IdtxShiftBlock(T *HWY_RESTRICT in, int8_t cos_bit) {
  (void)cos_bit;
  constexpr auto int_tag = hn::CappedTag<T, Width>();
  for (size_t y = 0; y < BlockHeight; ++y) {
    IdtxShift<Width, Shift>(int_tag, &in[y * Stride]);
  }
}

template <typename T>
void TransformFail(T *in, int8_t cos_bit) {
  (void)in;
  (void)cos_bit;
  assert(false && "Incorrect transform requested.");
}

template <typename T>
using Transform1D = void (*)(T *in, int8_t cos_bit);

template <bool PositiveOrZero>
struct RoundShiftTraits {};

template <>
struct RoundShiftTraits<true> {
  template <int Bit, typename D>
  HWY_ATTR HWY_INLINE static hn::VFromD<D> Shift(D int_tag,
                                                 hn::VFromD<D> value) {
    (void)int_tag;
    if CONSTEXPR_IF (Bit == 0) {
      return value;
    } else {
      return hn::ShiftLeft<Bit>(value);
    }
  }
};

template <>
struct RoundShiftTraits<false> {
  template <int Bit, typename D>
  HWY_ATTR HWY_INLINE static hn::VFromD<D> Shift(D int_tag,
                                                 hn::VFromD<D> value) {
    const auto round = hn::Set(int_tag, 1 << (-Bit - 1));
    return hn::ShiftRight<-Bit>(hn::Add(value, round));
  }
};

template <int Bit, typename D>
HWY_ATTR HWY_INLINE hn::VFromD<D> RoundShift(D int_tag, hn::VFromD<D> value) {
  return RoundShiftTraits<(Bit >= 0)>::template Shift<Bit>(int_tag, value);
}

template <bool ApplyRectScale, typename D>
HWY_ATTR HWY_INLINE hn::VFromD<D> RectScale(D int_tag, hn::VFromD<D> v) {
  if CONSTEXPR_IF (ApplyRectScale) {
    return ScaleByNewSqrt2<1>(int_tag, v);
  }
  return v;
}

template <bool IsSame>
struct MaybePromoteTraits {};

template <>
struct MaybePromoteTraits<true> {
  template <typename VIn, typename D>
  HWY_ATTR HWY_INLINE static hn::VFromD<D> PromoteTo(D out_tag, VIn in) {
    (void)out_tag;
    return in;
  }

  template <typename VIn, typename D>
  HWY_ATTR HWY_INLINE static void PromoteStore2(D int_tag, VIn v,
                                                hn::TFromD<D> *out) {
    hn::StoreU(v, int_tag, out);
  }
};

template <>
struct MaybePromoteTraits<false> {
  template <typename VIn, typename D>
  HWY_ATTR HWY_INLINE static hn::VFromD<D> PromoteTo(D out_tag, VIn in) {
    return hn::PromoteTo(out_tag, in);
  }

  template <typename VIn, typename TOut, typename D>
  HWY_ATTR HWY_INLINE static void PromoteStore2(D int_tag, VIn v, TOut *out) {
    (void)int_tag;
    constexpr hn::Repartition<TOut, D> store_tag;
    hn::StoreU(hn::PromoteLowerTo(store_tag, v), store_tag, out);
    hn::StoreU(hn::PromoteUpperTo(store_tag, v), store_tag,
               out + hn::MaxLanes(store_tag));
  }
};

template <typename VIn, typename D>
HWY_ATTR HWY_INLINE hn::VFromD<D> MaybePromoteTo(D out_tag, VIn in) {
  return MaybePromoteTraits<
      std::is_same<hn::TFromD<D>, hn::TFromV<VIn>>::value>::PromoteTo(out_tag,
                                                                      in);
}

template <int8_t Bit, bool ApplyRectScale, typename TIn, typename TOut>
HWY_ATTR HWY_INLINE void Transpose4(const TIn *HWY_RESTRICT in,
                                    TOut *HWY_RESTRICT out, size_t instride,
                                    size_t outstride) {
  constexpr hn::FixedTag<TIn, 4> int_tag;
  auto i0 = RectScale<ApplyRectScale>(
      int_tag, RoundShift<Bit>(int_tag, hn::Load(int_tag, &in[0 * instride])));
  auto i1 = RectScale<ApplyRectScale>(
      int_tag, RoundShift<Bit>(int_tag, hn::Load(int_tag, &in[1 * instride])));
  auto i2 = RectScale<ApplyRectScale>(
      int_tag, RoundShift<Bit>(int_tag, hn::Load(int_tag, &in[2 * instride])));
  auto i3 = RectScale<ApplyRectScale>(
      int_tag, RoundShift<Bit>(int_tag, hn::Load(int_tag, &in[3 * instride])));
  HWY_ALIGN_MAX TOut interleaved[16];
  constexpr hn::FixedTag<TOut, 4> out_tag;
  hn::StoreInterleaved4(MaybePromoteTo(out_tag, i0),
                        MaybePromoteTo(out_tag, i1),
                        MaybePromoteTo(out_tag, i2),
                        MaybePromoteTo(out_tag, i3), out_tag, interleaved);
  for (size_t i = 0; i < 4; ++i) {
    hwy::CopyBytes<hn::MaxLanes(int_tag) * sizeof(*out)>(&interleaved[i * 4],
                                                         &out[i * outstride]);
  }
}

template <int8_t Bit, bool ApplyRectScale, typename TIn, typename TOut>
HWY_ATTR HWY_INLINE void Transpose8(const TIn *HWY_RESTRICT in,
                                    TOut *HWY_RESTRICT out, size_t instride,
                                    size_t outstride) {
  constexpr hn::FixedTag<TIn, 8> int_tag;
  constexpr hn::Rebind<TOut, decltype(int_tag)> out_tag;
  // N.B. there isn't a StoreInterleaved8, so hand-code Transpose8.
  constexpr hn::RepartitionToWide<decltype(out_tag)> wide_int_tag;
  HWY_ALIGN_MAX hn::TFromD<decltype(wide_int_tag)> interleaved0[16];
  HWY_ALIGN_MAX hn::TFromD<decltype(wide_int_tag)> interleaved1[16];
  auto i0 = hn::Load(int_tag, &in[0 * instride]);
  auto i1 = hn::Load(int_tag, &in[1 * instride]);
  auto i2 = hn::Load(int_tag, &in[2 * instride]);
  auto i3 = hn::Load(int_tag, &in[3 * instride]);
  auto i4 = hn::Load(int_tag, &in[4 * instride]);
  auto i5 = hn::Load(int_tag, &in[5 * instride]);
  auto i6 = hn::Load(int_tag, &in[6 * instride]);
  auto i7 = hn::Load(int_tag, &in[7 * instride]);
  auto s0 = hn::Undefined(out_tag);
  auto s1 = hn::Undefined(out_tag);
  auto s2 = hn::Undefined(out_tag);
  auto s3 = hn::Undefined(out_tag);
  auto s4 = hn::Undefined(out_tag);
  auto s5 = hn::Undefined(out_tag);
  auto s6 = hn::Undefined(out_tag);
  auto s7 = hn::Undefined(out_tag);
  auto ip0 = MaybePromoteTo(out_tag, i0);
  auto ip1 = MaybePromoteTo(out_tag, i1);
  auto ip2 = MaybePromoteTo(out_tag, i2);
  auto ip3 = MaybePromoteTo(out_tag, i3);
  auto ip4 = MaybePromoteTo(out_tag, i4);
  auto ip5 = MaybePromoteTo(out_tag, i5);
  auto ip6 = MaybePromoteTo(out_tag, i6);
  auto ip7 = MaybePromoteTo(out_tag, i7);
  s0 = RectScale<ApplyRectScale>(out_tag, RoundShift<Bit>(out_tag, ip0));
  s1 = RectScale<ApplyRectScale>(out_tag, RoundShift<Bit>(out_tag, ip1));
  s2 = RectScale<ApplyRectScale>(out_tag, RoundShift<Bit>(out_tag, ip2));
  s3 = RectScale<ApplyRectScale>(out_tag, RoundShift<Bit>(out_tag, ip3));
  s4 = RectScale<ApplyRectScale>(out_tag, RoundShift<Bit>(out_tag, ip4));
  s5 = RectScale<ApplyRectScale>(out_tag, RoundShift<Bit>(out_tag, ip5));
  s6 = RectScale<ApplyRectScale>(out_tag, RoundShift<Bit>(out_tag, ip6));
  s7 = RectScale<ApplyRectScale>(out_tag, RoundShift<Bit>(out_tag, ip7));
  auto u0 = hn::ZipLower(wide_int_tag, s0, s1);
  auto u1 = hn::ZipUpper(wide_int_tag, s0, s1);
  auto u2 = hn::ZipLower(wide_int_tag, s2, s3);
  auto u3 = hn::ZipUpper(wide_int_tag, s2, s3);
  auto u4 = hn::ZipLower(wide_int_tag, s4, s5);
  auto u5 = hn::ZipUpper(wide_int_tag, s4, s5);
  auto u6 = hn::ZipLower(wide_int_tag, s6, s7);
  auto u7 = hn::ZipUpper(wide_int_tag, s6, s7);
  hn::StoreInterleaved4(u0, u2, u4, u6, wide_int_tag, interleaved0);
  hn::StoreInterleaved4(u1, u3, u5, u7, wide_int_tag, interleaved1);
  constexpr size_t kNumBytes = hn::MaxLanes(int_tag) * sizeof(*out);
  if CONSTEXPR_IF (sizeof(TOut) == 2) {
    hwy::CopyBytes<kNumBytes>(&interleaved0[0], &out[0 * outstride]);
    hwy::CopyBytes<kNumBytes>(&interleaved0[4], &out[1 * outstride]);
    hwy::CopyBytes<kNumBytes>(&interleaved0[8], &out[2 * outstride]);
    hwy::CopyBytes<kNumBytes>(&interleaved0[12], &out[3 * outstride]);
    hwy::CopyBytes<kNumBytes>(&interleaved1[0], &out[4 * outstride]);
    hwy::CopyBytes<kNumBytes>(&interleaved1[4], &out[5 * outstride]);
    hwy::CopyBytes<kNumBytes>(&interleaved1[8], &out[6 * outstride]);
    hwy::CopyBytes<kNumBytes>(&interleaved1[12], &out[7 * outstride]);
  } else {
    hwy::CopyBytes<kNumBytes>(&interleaved0[0], &out[0 * outstride]);
    hwy::CopyBytes<kNumBytes>(&interleaved0[4], &out[1 * outstride]);
    hwy::CopyBytes<kNumBytes>(&interleaved1[0], &out[2 * outstride]);
    hwy::CopyBytes<kNumBytes>(&interleaved1[4], &out[3 * outstride]);
    hwy::CopyBytes<kNumBytes>(&interleaved0[8], &out[4 * outstride]);
    hwy::CopyBytes<kNumBytes>(&interleaved0[12], &out[5 * outstride]);
    hwy::CopyBytes<kNumBytes>(&interleaved1[8], &out[6 * outstride]);
    hwy::CopyBytes<kNumBytes>(&interleaved1[12], &out[7 * outstride]);
  }
}

template <typename D>
HWY_ATTR HWY_INLINE hn::VFromD<D> LocalInterleaveEvenBlocks(D tag,
                                                            hn::VFromD<D> a,
                                                            hn::VFromD<D> b) {
  static_assert(sizeof(hn::TFromD<D>) == 8,
                "LocalInterleaveEvenBlocks requires 64-bit lanes.");
  HWY_ALIGN static constexpr int64_t kIndices[] = { 0, 1, 8 + 0, 8 + 1,
                                                    4, 5, 8 + 4, 8 + 5 };
  auto indices = hn::SetTableIndices(tag, kIndices);
  return hn::TwoTablesLookupLanes(tag, a, b, indices);
}

template <typename D>
HWY_ATTR HWY_INLINE hn::VFromD<D> LocalInterleaveOddBlocks(D tag,
                                                           hn::VFromD<D> a,
                                                           hn::VFromD<D> b) {
  static_assert(sizeof(hn::TFromD<D>) == 8,
                "LocalInterleaveOddBlocks requires 64-bit lanes.");
  HWY_ALIGN static constexpr int64_t kIndices[] = { 2, 3, 8 + 2, 8 + 3,
                                                    6, 7, 8 + 6, 8 + 7 };
  auto indices = hn::SetTableIndices(tag, kIndices);
  return hn::TwoTablesLookupLanes(tag, a, b, indices);
}

template <size_t LaneSize>
struct Transpose16Traits {};

template <>
struct Transpose16Traits<2> {
  template <int8_t Bit, bool ApplyRectScale, typename TIn, typename TOut>
  HWY_ATTR HWY_INLINE static void Transpose16(const TIn *HWY_RESTRICT in,
                                              TOut *HWY_RESTRICT out,
                                              size_t instride,
                                              size_t outstride) {
    constexpr hn::FixedTag<TIn, 16> int_tag;
    static_assert(hn::MaxLanes(int_tag) == 16,
                  "16-bit Transpose16 requires an 16-lane int_tag");
    constexpr hn::RepartitionToWide<decltype(int_tag)> wide_int_tag;
    constexpr hn::RepartitionToWide<decltype(wide_int_tag)> widex2_int_tag;
    HWY_ALIGN_MAX hn::TFromD<decltype(wide_int_tag)>
        y[16 * hn::MaxLanes(wide_int_tag)];
    HWY_ALIGN_MAX hn::TFromD<decltype(widex2_int_tag)>
        z[16 * hn::MaxLanes(widex2_int_tag)];
    for (size_t i = 0; i < 16; i += 2) {
      auto i0 = RectScale<ApplyRectScale>(
          int_tag,
          RoundShift<Bit>(int_tag, hn::Load(int_tag, &in[(i + 0) * instride])));
      auto i1 = RectScale<ApplyRectScale>(
          int_tag,
          RoundShift<Bit>(int_tag, hn::Load(int_tag, &in[(i + 1) * instride])));
      hn::Store(hn::ZipLower(wide_int_tag, i0, i1), wide_int_tag,
                &y[(i + 0) * hn::MaxLanes(wide_int_tag)]);
      hn::Store(hn::ZipUpper(wide_int_tag, i0, i1), wide_int_tag,
                &y[(i + 1) * hn::MaxLanes(wide_int_tag)]);
    }
    for (size_t i = 0; i < 16; i += 4) {
      for (size_t j = 0; j < 2; ++j) {
        auto i0 = hn::Load(wide_int_tag,
                           &y[(i + j + 0) * hn::MaxLanes(wide_int_tag)]);
        auto i2 = hn::Load(wide_int_tag,
                           &y[(i + j + 2) * hn::MaxLanes(wide_int_tag)]);
        hn::Store(hn::ZipLower(widex2_int_tag, i0, i2), widex2_int_tag,
                  &z[(i + j + 0) * hn::MaxLanes(widex2_int_tag)]);
        hn::Store(hn::ZipUpper(widex2_int_tag, i0, i2), widex2_int_tag,
                  &z[(i + j + 2) * hn::MaxLanes(widex2_int_tag)]);
      }
    }
    for (size_t i = 0; i < 16; i += 8) {
      for (size_t j = 0; j < 4; ++j) {
        auto i0 = hn::Load(widex2_int_tag,
                           &z[(i + j + 0) * hn::MaxLanes(widex2_int_tag)]);
        auto i4 = hn::Load(widex2_int_tag,
                           &z[(i + j + 4) * hn::MaxLanes(widex2_int_tag)]);
        hn::Store(hn::InterleaveLower(widex2_int_tag, i0, i4), widex2_int_tag,
                  &z[(i + j + 0) * hn::MaxLanes(widex2_int_tag)]);
        hn::Store(hn::InterleaveUpper(widex2_int_tag, i0, i4), widex2_int_tag,
                  &z[(i + j + 4) * hn::MaxLanes(widex2_int_tag)]);
      }
    }
    static constexpr size_t kStoreIndex[] = { 0, 4,  2,  6,  1, 5,  3,  7,
                                              8, 12, 10, 14, 9, 13, 11, 15 };
    for (size_t j = 0; j < 8; ++j) {
      auto i0 =
          hn::Load(widex2_int_tag, &z[(j + 0) * hn::MaxLanes(widex2_int_tag)]);
      auto i8 =
          hn::Load(widex2_int_tag, &z[(j + 8) * hn::MaxLanes(widex2_int_tag)]);
      hn::StoreU(
          hn::BitCast(int_tag, hn::ConcatLowerLower(widex2_int_tag, i8, i0)),
          int_tag, &out[kStoreIndex[j + 0] * outstride]);
      hn::StoreU(
          hn::BitCast(int_tag, hn::ConcatUpperUpper(widex2_int_tag, i8, i0)),
          int_tag, &out[kStoreIndex[j + 8] * outstride]);
    }
  }
};

template <>
struct Transpose16Traits<4> {
  template <int8_t Bit, bool ApplyRectScale, typename TIn, typename TOut>
  HWY_ATTR HWY_INLINE static void Transpose16(const TIn *HWY_RESTRICT in,
                                              TOut *HWY_RESTRICT out,
                                              size_t instride,
                                              size_t outstride) {
    constexpr hn::FixedTag<TIn, 16> int_tag;
    static_assert(hn::MaxLanes(int_tag) == 16,
                  "32-bit Transpose16 requires an 16-lane int_tag");
    constexpr hn::RepartitionToWide<decltype(int_tag)> wide_int_tag;
    HWY_ALIGN_MAX hn::TFromD<decltype(wide_int_tag)>
        z[16 * hn::MaxLanes(wide_int_tag)];
    for (size_t i = 0; i < 16; i += 2) {
      auto i0 = RectScale<ApplyRectScale>(
          int_tag,
          RoundShift<Bit>(int_tag, hn::Load(int_tag, &in[(i + 0) * instride])));
      auto i1 = RectScale<ApplyRectScale>(
          int_tag,
          RoundShift<Bit>(int_tag, hn::Load(int_tag, &in[(i + 1) * instride])));
      hn::Store(hn::ZipLower(wide_int_tag, i0, i1), wide_int_tag,
                &z[(i + 0) * hn::MaxLanes(wide_int_tag)]);
      hn::Store(hn::ZipUpper(wide_int_tag, i0, i1), wide_int_tag,
                &z[(i + 1) * hn::MaxLanes(wide_int_tag)]);
    }
    for (size_t i = 0; i < 16; i += 4) {
      for (size_t j = 0; j < 2; ++j) {
        auto i0 = hn::Load(wide_int_tag,
                           &z[(i + j + 0) * hn::MaxLanes(wide_int_tag)]);
        auto i2 = hn::Load(wide_int_tag,
                           &z[(i + j + 2) * hn::MaxLanes(wide_int_tag)]);
        hn::Store(hn::InterleaveLower(wide_int_tag, i0, i2), wide_int_tag,
                  &z[(i + j + 0) * hn::MaxLanes(wide_int_tag)]);
        hn::Store(hn::InterleaveUpper(wide_int_tag, i0, i2), wide_int_tag,
                  &z[(i + j + 2) * hn::MaxLanes(wide_int_tag)]);
      }
    }
    for (size_t i = 0; i < 16; i += 8) {
      for (size_t j = 0; j < 4; ++j) {
        auto i0 = hn::Load(wide_int_tag,
                           &z[(i + j + 0) * hn::MaxLanes(wide_int_tag)]);
        auto i4 = hn::Load(wide_int_tag,
                           &z[(i + j + 4) * hn::MaxLanes(wide_int_tag)]);
        hn::Store(LocalInterleaveEvenBlocks(wide_int_tag, i0, i4), wide_int_tag,
                  &z[(i + j + 0) * hn::MaxLanes(wide_int_tag)]);
        hn::Store(LocalInterleaveOddBlocks(wide_int_tag, i0, i4), wide_int_tag,
                  &z[(i + j + 4) * hn::MaxLanes(wide_int_tag)]);
      }
    }
    static constexpr size_t kStoreIndex[] = { 0, 2,  1, 3,  4,  6,  5,  7,
                                              8, 10, 9, 11, 12, 14, 13, 15 };
    for (size_t j = 0; j < 8; ++j) {
      auto i0 =
          hn::Load(wide_int_tag, &z[(j + 0) * hn::MaxLanes(wide_int_tag)]);
      auto i8 =
          hn::Load(wide_int_tag, &z[(j + 8) * hn::MaxLanes(wide_int_tag)]);
      hn::StoreU(
          hn::BitCast(int_tag, hn::ConcatLowerLower(wide_int_tag, i8, i0)),
          int_tag, &out[kStoreIndex[j + 0] * outstride]);
      hn::StoreU(
          hn::BitCast(int_tag, hn::ConcatUpperUpper(wide_int_tag, i8, i0)),
          int_tag, &out[kStoreIndex[j + 8] * outstride]);
    }
  }
};

template <int8_t Bit, bool ApplyRectScale, typename TIn, typename TOut>
HWY_ATTR HWY_INLINE void Transpose16(const TIn *HWY_RESTRICT in,
                                     TOut *HWY_RESTRICT out, size_t instride,
                                     size_t outstride) {
  static_assert(sizeof(TOut) == sizeof(TIn),
                "Transpose16 does not directly support integer promotion.");
  Transpose16Traits<sizeof(TIn)>::template Transpose16<Bit, ApplyRectScale>(
      in, out, instride, outstride);
}

template <size_t NumLanes, bool RequiresPromotion>
struct TransposeTraits {};

template <>
struct TransposeTraits<16, true> {
  template <size_t Width, size_t Height, int8_t Bit, bool ApplyRectScale,
            typename TIn, typename TOut>
  HWY_ATTR HWY_INLINE static void Transpose(const TIn *HWY_RESTRICT in,
                                            TOut *HWY_RESTRICT out,
                                            size_t instride, size_t outstride) {
    constexpr auto int_tag =
        hn::CappedTag<TOut, AOMMIN(16, AOMMIN(Width, Height))>();
    constexpr hn::Rebind<TIn, decltype(int_tag)> input_tag;
    HWY_ALIGN_MAX hn::TFromD<decltype(int_tag)> p[16 * hn::MaxLanes(int_tag)];
    for (size_t r = 0; r < Height; r += 16) {
      for (size_t c = 0; c < Width; c += 16) {
        for (size_t i = 0; i < 16; ++i) {
          hn::Store(
              hn::PromoteTo(int_tag,
                            hn::Load(input_tag, &in[(r + i) * instride + c])),
              int_tag, &p[i * hn::MaxLanes(int_tag)]);
        }
        Transpose16<Bit, ApplyRectScale>(p, &out[c * outstride + r],
                                         hn::MaxLanes(int_tag), outstride);
      }
    }
  }
};

template <>
struct TransposeTraits<16, false> {
  template <size_t Width, size_t Height, int8_t Bit, bool ApplyRectScale,
            typename TIn, typename TOut>
  HWY_ATTR HWY_INLINE static void Transpose(const TIn *HWY_RESTRICT in,
                                            TOut *HWY_RESTRICT out,
                                            size_t instride, size_t outstride) {
    for (size_t r = 0; r < Height; r += 16) {
      for (size_t c = 0; c < Width; c += 16) {
        Transpose16<Bit, ApplyRectScale>(&in[r * instride + c],
                                         &out[c * outstride + r], instride,
                                         outstride);
      }
    }
  }
};

template <bool RequiresPromotion>
struct TransposeTraits<8, RequiresPromotion> {
  template <size_t Width, size_t Height, int8_t Bit, bool ApplyRectScale,
            typename TIn, typename TOut>
  HWY_ATTR HWY_INLINE static void Transpose(const TIn *HWY_RESTRICT in,
                                            TOut *HWY_RESTRICT out,
                                            size_t instride, size_t outstride) {
    for (size_t r = 0; r < Height; r += 8) {
      for (size_t c = 0; c < Width; c += 8) {
        Transpose8<Bit, ApplyRectScale>(&in[r * instride + c],
                                        &out[c * outstride + r], instride,
                                        outstride);
      }
    }
  }
};

template <bool RequiresPromotion>
struct TransposeTraits<4, RequiresPromotion> {
  template <size_t Width, size_t Height, int8_t Bit, bool ApplyRectScale,
            typename TIn, typename TOut>
  HWY_ATTR HWY_INLINE static void Transpose(const TIn *HWY_RESTRICT in,
                                            TOut *HWY_RESTRICT out,
                                            size_t instride, size_t outstride) {
    for (size_t r = 0; r < Height; r += 4) {
      for (size_t c = 0; c < Width; c += 4) {
        Transpose4<Bit, ApplyRectScale>(&in[r * instride + c],
                                        &out[c * outstride + r], instride,
                                        outstride);
      }
    }
  }
};

template <size_t Width, size_t Height, int8_t Bit, bool ApplyRectScale,
          typename TIn, typename TOut>
HWY_ATTR HWY_INLINE void Transpose(const TIn *HWY_RESTRICT in,
                                   TOut *HWY_RESTRICT out, size_t instride,
                                   size_t outstride) {
  constexpr auto int_tag =
      hn::CappedTag<TOut, AOMMIN(16, AOMMIN(Width, Height))>();
  TransposeTraits<hn::MaxLanes(int_tag), !std::is_same<TIn, TOut>::value>::
      template Transpose<Width, Height, Bit, ApplyRectScale>(in, out, instride,
                                                             outstride);
}

template <size_t Width, size_t Height, int Shift, bool ApplyRectScale,
          typename TIn, typename TOut>
HWY_ATTR HWY_INLINE void StoreBlock(const TIn *HWY_RESTRICT in, size_t instride,
                                    TOut *HWY_RESTRICT out, size_t outstride) {
  constexpr hn::CappedTag<TIn, Width> load_tag;
  for (size_t r = 0; r < Height; ++r) {
    for (size_t c = 0; c < Width; c += hn::MaxLanes(load_tag)) {
      auto v = RectScale<ApplyRectScale>(
          load_tag, RoundShift<Shift>(
                        load_tag, hn::Load(load_tag, &in[r * instride + c])));
      MaybePromoteTraits<std::is_same<TIn, TOut>::value>::PromoteStore2(
          load_tag, v, &out[r * outstride + c]);
    }
  }
}

template <int8_t Shift, size_t Width, bool FlipLeftRight, typename TInput,
          typename TIn>
HWY_ATTR HWY_INLINE void LoadLine(const TInput *HWY_RESTRICT input,
                                  TIn *HWY_RESTRICT in) {
  constexpr hn::CappedTag<TIn, Width> store_tag;
  constexpr hn::Rebind<TInput, decltype(store_tag)> load_tag;
  for (size_t x = 0; x < Width / hn::MaxLanes(load_tag); ++x) {
    auto v = hn::LoadU(load_tag, &input[x * hn::MaxLanes(load_tag)]);
    if CONSTEXPR_IF (FlipLeftRight) {
      v = hn::Reverse(load_tag, v);
    }
    auto vp = MaybePromoteTo(store_tag, v);
    hn::Store(
        hn::ShiftLeft<Shift>(vp), store_tag,
        &in[(FlipLeftRight ? (Width / hn::MaxLanes(store_tag)) - x - 1 : x) *
            hn::MaxLanes(store_tag)]);
  }
}

template <int8_t Shift, size_t Width, size_t OutStride, size_t Height,
          bool FlipUpDown, bool FlipLeftRight, typename TInput, typename TIn>
HWY_ATTR HWY_INLINE void LoadBuffer(const TInput *HWY_RESTRICT input,
                                    TIn *HWY_RESTRICT in, size_t stride) {
  for (size_t y = 0; y < Height; ++y) {
    LoadLine<Shift, Width, FlipLeftRight>(
        input + y * stride, &in[(FlipUpDown ? Height - y - 1 : y) * OutStride]);
  }
}

template <size_t TransformWidth, size_t BlockWidth, size_t BlockHeight,
          typename T>
HWY_ATTR HWY_FLATTEN HWY_INLINE void Transform4(TX_TYPE_1D tx_type, T *in,
                                                int8_t cos_bit) {
  switch (tx_type) {
    case DCT_1D: FdctNx4Block<TransformWidth, BlockWidth>(in, cos_bit); break;
    case IDTX_1D:
      IdtxSqrt2Block<TransformWidth, BlockWidth, BlockHeight, 1>(in, cos_bit);
      break;
    default: FadstNx4Block<TransformWidth, BlockWidth>(in, cos_bit); break;
  }
}

template <size_t TransformWidth, size_t BlockWidth, size_t BlockHeight,
          typename T>
HWY_ATTR HWY_FLATTEN HWY_INLINE void Transform8(TX_TYPE_1D tx_type, T *in,
                                                int8_t cos_bit) {
  switch (tx_type) {
    case DCT_1D: FdctNx8Block<TransformWidth, BlockWidth>(in, cos_bit); break;
    case IDTX_1D:
      IdtxAdd2Block<TransformWidth, BlockWidth, BlockHeight>(in, cos_bit);
      break;
    default: FadstNx8Block<TransformWidth, BlockWidth>(in, cos_bit); break;
  }
}

template <size_t TransformWidth, size_t BlockWidth, size_t BlockHeight,
          typename T>
HWY_ATTR HWY_INLINE void Transform16(TX_TYPE_1D tx_type, T *in,
                                     int8_t cos_bit) {
  static const Transform1D<T> kTransform[] = {
    FdctNx16Block<TransformWidth, BlockWidth, T>,   // DCT_1D
    FadstNx16Block<TransformWidth, BlockWidth, T>,  // ADST_1D
    FadstNx16Block<TransformWidth, BlockWidth, T>,  // FLIPADST_1D
    IdtxSqrt2Block<TransformWidth, BlockWidth, BlockHeight, 2, T>,  // IDTX_1D
  };
  kTransform[tx_type](in, cos_bit);
}

template <size_t TransformWidth, size_t BlockWidth, size_t BlockHeight,
          typename T>
HWY_ATTR HWY_INLINE void Transform32(TX_TYPE_1D tx_type, T *in,
                                     int8_t cos_bit) {
  static const Transform1D<T> kTransform[] = {
    FdctNx32Block<TransformWidth, BlockWidth, T>,  // DCT_1D
    TransformFail<T>,                              // ADST_1D
    TransformFail<T>,                              // FLIPADST_1D
    IdtxShiftBlock<TransformWidth, BlockWidth, BlockHeight, 2, T>,  // IDTX_1D
  };
  kTransform[tx_type](in, cos_bit);
}

template <size_t TransformWidth, size_t BlockWidth, typename T>
HWY_ATTR HWY_INLINE void TransformFull64(TX_TYPE_1D tx_type, T *in,
                                         int8_t cos_bit) {
  (void)tx_type;
  assert(tx_type == DCT_1D);
  FdctNx64Block<TransformWidth, BlockWidth, TransformWidth, BlockWidth>(
      in, cos_bit);
}

template <size_t TransformWidth, size_t BlockWidth, size_t TransformHeight,
          size_t BlockHeight, typename T>
HWY_ATTR HWY_INLINE void TransformBelow32(TX_TYPE_1D tx_type, T *in,
                                          int8_t cos_bit) {
  if CONSTEXPR_IF (TransformHeight == 4) {
    Transform4<TransformWidth, BlockWidth, BlockHeight>(tx_type, in, cos_bit);
  } else if CONSTEXPR_IF (TransformHeight == 8) {
    Transform8<TransformWidth, BlockWidth, BlockHeight>(tx_type, in, cos_bit);
  } else if CONSTEXPR_IF (TransformHeight == 16) {
    Transform16<TransformWidth, BlockWidth, BlockHeight>(tx_type, in, cos_bit);
  } else if CONSTEXPR_IF (TransformHeight == 32) {
    Transform32<TransformWidth, BlockWidth, BlockHeight>(tx_type, in, cos_bit);
  } else {
    assert(false && "Unsupported transform size.");
  }
}

template <size_t TransformWidth, size_t BlockWidth, size_t TransformHeight,
          size_t BlockHeight, typename T>
HWY_ATTR HWY_INLINE void RowTransform(TX_TYPE_1D tx_type, T *in,
                                      int8_t cos_bit) {
  if CONSTEXPR_IF (TransformWidth == 64 && TransformHeight == 64) {
    assert(tx_type == DCT_1D);
    // 64x64 only writes 32x32 of coefficients.
    FdctNx64Block<TransformWidth, BlockWidth, 32, 32>(in, cos_bit);
  } else if CONSTEXPR_IF (TransformHeight == 64) {
    TransformFull64<TransformWidth, BlockWidth>(tx_type, in, cos_bit);
  } else {
    TransformBelow32<TransformWidth, BlockWidth, TransformHeight, BlockHeight>(
        tx_type, in, cos_bit);
  }
}

template <TX_SIZE TxSize, typename T>
HWY_ATTR HWY_MAYBE_UNUSED void ForwardTransform2D(const int16_t *input,
                                                  int32_t *output,
                                                  size_t stride,
                                                  TX_TYPE tx_type) {
  constexpr size_t kWidth = kTxSizeWide[TxSize];
  constexpr size_t kHeight = kTxSizeHigh[TxSize];
  // Ensure the storage is aligned to the architecture's block width.
  constexpr size_t kMinVectorSize =
      hn::BlockDFromD<hn::ScalableTag<T>>().MaxBytes() / sizeof(uint8_t);
  constexpr size_t kBlockWidth = AOMMAX(kMinVectorSize / sizeof(T), kWidth);
  constexpr size_t kBlockHeight = AOMMAX(kMinVectorSize / sizeof(T), kHeight);
  HWY_ALIGN_MAX T buf0[kBlockWidth * kBlockHeight];
  constexpr bool kBigRectangle = (kBlockWidth == 64 && kBlockHeight >= 32) ||
                                 (kBlockWidth >= 32 && kBlockHeight == 64);
  using T2 = typename std::conditional<kBigRectangle, int32_t, T>::type;
  HWY_ALIGN_MAX T2 buf1[kBlockWidth * kBlockHeight];
  constexpr int8_t kShift[3] = { kForwardTransformShift[TxSize][0],
                                 kForwardTransformShift[TxSize][1],
                                 kForwardTransformShift[TxSize][2] };
  constexpr int kTransformWidthIndex = GetTxwIndex(TxSize);
  constexpr int kTransformHeightIndex = GetTxhIndex(TxSize);
  constexpr int8_t cos_bit_col =
      kForwardCosBitCol[kTransformWidthIndex][kTransformHeightIndex];
  constexpr int8_t cos_bit_row =
      kForwardCosBitRow[kTransformWidthIndex][kTransformHeightIndex];
  const TX_TYPE_1D vertical_transform = vtx_tab[tx_type];
  const TX_TYPE_1D horizontal_transform = htx_tab[tx_type];
  constexpr bool kApplyRectScale = kApplyRectScaleList[TxSize];
  switch ((vertical_transform == FLIPADST_1D ? 1 : 0) |
          (horizontal_transform == FLIPADST_1D ? 2 : 0)) {
    case 0:
      LoadBuffer<kShift[0], kWidth, kBlockWidth, kHeight, false, false>(
          input, buf0, stride);
      break;
    case 1:
      LoadBuffer<kShift[0], kWidth, kBlockWidth, kHeight, true, false>(
          input, buf0, stride);
      break;
    case 2:
      LoadBuffer<kShift[0], kWidth, kBlockWidth, kHeight, false, true>(
          input, buf0, stride);
      break;
    case 3:
      LoadBuffer<kShift[0], kWidth, kBlockWidth, kHeight, true, true>(
          input, buf0, stride);
      break;
  }
  if CONSTEXPR_IF (kHeight == 64) {
    TransformFull64<kWidth, kBlockWidth>(vertical_transform, buf0, cos_bit_col);
  } else {
    TransformBelow32<kWidth, kBlockWidth, kHeight, kBlockHeight>(
        vertical_transform, buf0, cos_bit_col);
  }
  Transpose<kWidth, kHeight, kShift[1], false>(buf0, buf1, kBlockWidth,
                                               kBlockHeight);
  if CONSTEXPR_IF (kWidth == 64 && kHeight == 64) {
    // 64x64 only writes 32x32 of coefficients.
    assert(tx_type == DCT_1D);
    FdctNx64Block<kHeight, kBlockHeight, 32, 32>(buf1, cos_bit_row);
    StoreBlock<32, 32, kShift[2], kApplyRectScale>(buf1, 32, output, 32);
  } else if CONSTEXPR_IF (kHeight == 64 && (kWidth == 16 || kWidth == 32)) {
    // 32x64 and 16x64 coefficients are packed into Wx32, discarding the
    // right-most results.
    RowTransform<32, kBlockHeight, kWidth, kBlockWidth>(horizontal_transform,
                                                        buf1, cos_bit_row);
    StoreBlock<kHeight, kWidth, kShift[2], kApplyRectScale>(buf1, kBlockHeight,
                                                            output, 32);
  } else {
    RowTransform<kHeight, kBlockHeight, kWidth, kBlockWidth>(
        horizontal_transform, buf1, cos_bit_row);
    StoreBlock<kHeight, kWidth, kShift[2], kApplyRectScale>(buf1, kBlockHeight,
                                                            output, kHeight);
  }
  if CONSTEXPR_IF (kHeight <= 16 && kWidth == 64) {
    hwy::ZeroBytes<kHeight * 32 * sizeof(*output)>(output + kHeight * 32);
  }
}

HWY_MAYBE_UNUSED void LowBitdepthForwardTransform2D(const int16_t *src_diff,
                                                    tran_low_t *coeff,
                                                    int diff_stride,
                                                    TxfmParam *txfm_param) {
  if (txfm_param->lossless && txfm_param->tx_size == TX_4X4) {
    assert(txfm_param->tx_type == DCT_DCT);
    av1_fwht4x4(src_diff, coeff, diff_stride);
    return;
  }
  using TransformFunction = decltype(&ForwardTransform2D<TX_4X4, int16_t>);
  constexpr TransformFunction kTable[] = {
#define POINTER(w, h, _) &ForwardTransform2D<TX_##w##X##h, int16_t>,
    FOR_EACH_TXFM2D(POINTER, _)
#undef POINTER
  };
  kTable[txfm_param->tx_size](src_diff, coeff, diff_stride,
                              txfm_param->tx_type);
}

}  // namespace HWY_NAMESPACE
}  // namespace

HWY_AFTER_NAMESPACE();

#define MAKE_HIGHBD_TXFM2D(w, h, suffix)                                       \
  extern "C" void av1_fwd_txfm2d_##w##x##h##_##suffix(                         \
      const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type,      \
      int bd);                                                                 \
  HWY_ATTR void av1_fwd_txfm2d_##w##x##h##_##suffix(                           \
      const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type,      \
      int bd) {                                                                \
    (void)bd;                                                                  \
    HWY_NAMESPACE::ForwardTransform2D<TX_##w##X##h, int32_t>(input, output,    \
                                                             stride, tx_type); \
  }

#define MAKE_LOWBD_TXFM2D(w, h, suffix)                                        \
  extern "C" void av1_lowbd_fwd_txfm2d_##w##x##h##_##suffix(                   \
      const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type,      \
      int bd);                                                                 \
  HWY_ATTR void av1_lowbd_fwd_txfm2d_##w##x##h##_##suffix(                     \
      const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type,      \
      int bd) {                                                                \
    (void)bd;                                                                  \
    HWY_NAMESPACE::ForwardTransform2D<TX_##w##X##h, int16_t>(input, output,    \
                                                             stride, tx_type); \
  }

#define MAKE_LOWBD_TXFM2D_DISPATCH(suffix)                                     \
  extern "C" void av1_lowbd_fwd_txfm_##suffix(                                 \
      const int16_t *src_diff, tran_low_t *coeff, int diff_stride,             \
      TxfmParam *txfm_param);                                                  \
  HWY_ATTR void av1_lowbd_fwd_txfm_##suffix(                                   \
      const int16_t *src_diff, tran_low_t *coeff, int diff_stride,             \
      TxfmParam *txfm_param) {                                                 \
    HWY_NAMESPACE::LowBitdepthForwardTransform2D(src_diff, coeff, diff_stride, \
                                                 txfm_param);                  \
  }

#endif  // AOM_AV1_ENCODER_AV1_FWD_TXFM2D_HWY_H_
