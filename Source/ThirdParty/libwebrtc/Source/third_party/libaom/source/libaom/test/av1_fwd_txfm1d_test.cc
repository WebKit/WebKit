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

#include <memory>
#include <new>

#include "av1/encoder/av1_fwd_txfm1d.h"
#include "test/av1_txfm_test.h"

using libaom_test::ACMRandom;
using libaom_test::input_base;
using libaom_test::reference_hybrid_1d;
using libaom_test::TYPE_ADST;
using libaom_test::TYPE_DCT;
using libaom_test::TYPE_IDTX;
using libaom_test::TYPE_TXFM;

namespace {
const int txfm_type_num = 3;
const TYPE_TXFM txfm_type_ls[txfm_type_num] = { TYPE_DCT, TYPE_ADST,
                                                TYPE_IDTX };

const int txfm_size_num = 5;

const int txfm_size_ls[] = { 4, 8, 16, 32, 64 };

const TxfmFunc fwd_txfm_func_ls[][txfm_type_num] = {
  { av1_fdct4, av1_fadst4, av1_fidentity4_c },
  { av1_fdct8, av1_fadst8, av1_fidentity8_c },
  { av1_fdct16, av1_fadst16, av1_fidentity16_c },
  { av1_fdct32, nullptr, av1_fidentity32_c },
  { av1_fdct64, nullptr, nullptr },
};

// the maximum stage number of fwd/inv 1d dct/adst txfm is 12
const int8_t cos_bit = 14;
const int8_t range_bit[12] = { 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20 };

TEST(av1_fwd_txfm1d, round_shift) {
  EXPECT_EQ(round_shift(7, 1), 4);
  EXPECT_EQ(round_shift(-7, 1), -3);

  EXPECT_EQ(round_shift(7, 2), 2);
  EXPECT_EQ(round_shift(-7, 2), -2);

  EXPECT_EQ(round_shift(8, 2), 2);
  EXPECT_EQ(round_shift(-8, 2), -2);
}

TEST(av1_fwd_txfm1d, av1_cospi_arr_data) {
  for (int i = 0; i < 7; i++) {
    for (int j = 0; j < 64; j++) {
      EXPECT_EQ(av1_cospi_arr_data[i][j],
                (int32_t)round(cos(PI * j / 128) * (1 << (cos_bit_min + i))));
    }
  }
}

TEST(av1_fwd_txfm1d, accuracy) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  for (int si = 0; si < txfm_size_num; ++si) {
    int txfm_size = txfm_size_ls[si];
    std::unique_ptr<int32_t[]> input(new (std::nothrow) int32_t[txfm_size]);
    std::unique_ptr<int32_t[]> output(new (std::nothrow) int32_t[txfm_size]);
    std::unique_ptr<double[]> ref_input(new (std::nothrow) double[txfm_size]);
    std::unique_ptr<double[]> ref_output(new (std::nothrow) double[txfm_size]);
    ASSERT_NE(input, nullptr);
    ASSERT_NE(output, nullptr);
    ASSERT_NE(ref_input, nullptr);
    ASSERT_NE(ref_output, nullptr);

    for (int ti = 0; ti < txfm_type_num; ++ti) {
      TYPE_TXFM txfm_type = txfm_type_ls[ti];
      TxfmFunc fwd_txfm_func = fwd_txfm_func_ls[si][ti];
      int max_error = 7;

      const int count_test_block = 5000;
      if (fwd_txfm_func != nullptr) {
        for (int ti = 0; ti < count_test_block; ++ti) {
          for (int ni = 0; ni < txfm_size; ++ni) {
            input[ni] = rnd.Rand16() % input_base - rnd.Rand16() % input_base;
            ref_input[ni] = static_cast<double>(input[ni]);
          }

          fwd_txfm_func(input.get(), output.get(), cos_bit, range_bit);
          reference_hybrid_1d(ref_input.get(), ref_output.get(), txfm_size,
                              txfm_type);

          for (int ni = 0; ni < txfm_size; ++ni) {
            ASSERT_LE(
                abs(output[ni] - static_cast<int32_t>(round(ref_output[ni]))),
                max_error)
                << "tx size = " << txfm_size << ", tx type = " << txfm_type;
          }
        }
      }
    }
  }
}
}  // namespace
