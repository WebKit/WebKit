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

#include <math.h>

#include "test/av1_txfm_test.h"
#include "test/util.h"
#include "av1/common/av1_inv_txfm1d.h"
#include "av1/encoder/av1_fwd_txfm1d.h"

typedef TX_SIZE TxSize;

using libaom_test::ACMRandom;
using libaom_test::input_base;

namespace {
const int txfm_type_num = 2;
const int txfm_size_ls[] = { 4, 8, 16, 32, 64 };

const TxfmFunc fwd_txfm_func_ls[][txfm_type_num] = {
  { av1_fdct4, av1_fadst4 },   { av1_fdct8, av1_fadst8 },
  { av1_fdct16, av1_fadst16 }, { av1_fdct32, nullptr },
  { av1_fdct64, nullptr },
};

const TxfmFunc inv_txfm_func_ls[][txfm_type_num] = {
  { av1_idct4, av1_iadst4 },   { av1_idct8, av1_iadst8 },
  { av1_idct16, av1_iadst16 }, { av1_idct32, nullptr },
  { av1_idct64, nullptr },
};

// the maximum stage number of fwd/inv 1d dct/adst txfm is 12
const int8_t cos_bit = 13;
const int8_t range_bit[12] = { 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20 };

void reference_idct_1d_int(const int32_t *in, int32_t *out, int size) {
  double input[64];
  for (int i = 0; i < size; ++i) input[i] = in[i];

  double output[64];
  libaom_test::reference_idct_1d(input, output, size);

  for (int i = 0; i < size; ++i) {
    ASSERT_GE(output[i], INT32_MIN);
    ASSERT_LE(output[i], INT32_MAX);
    out[i] = static_cast<int32_t>(round(output[i]));
  }
}

void random_matrix(int32_t *dst, int len, ACMRandom *rnd) {
  const int bits = 16;
  const int maxVal = (1 << (bits - 1)) - 1;
  const int minVal = -(1 << (bits - 1));
  for (int i = 0; i < len; ++i) {
    if (rnd->Rand8() % 10)
      dst[i] = minVal + rnd->Rand16() % (1 << bits);
    else
      dst[i] = rnd->Rand8() % 2 ? minVal : maxVal;
  }
}

TEST(av1_inv_txfm1d, InvAccuracyCheck) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  const int count_test_block = 20000;
  const int max_error[] = { 6, 10, 19, 31, 40 };
  ASSERT_EQ(NELEMENTS(max_error), TX_SIZES);
  ASSERT_EQ(NELEMENTS(inv_txfm_func_ls), TX_SIZES);
  for (int i = 0; i < count_test_block; ++i) {
    // choose a random transform to test
    const TxSize tx_size = static_cast<TxSize>(rnd.Rand8() % TX_SIZES);
    const int txfm_size = txfm_size_ls[tx_size];
    const TxfmFunc inv_txfm_func = inv_txfm_func_ls[tx_size][0];

    int32_t input[64];
    random_matrix(input, txfm_size, &rnd);

    // 64x64 transform assumes last 32 values are zero.
    memset(input + 32, 0, 32 * sizeof(input[0]));

    int32_t ref_output[64];
    memset(ref_output, 0, sizeof(ref_output));
    reference_idct_1d_int(input, ref_output, txfm_size);

    int32_t output[64];
    memset(output, 0, sizeof(output));
    inv_txfm_func(input, output, cos_bit, range_bit);

    for (int ni = 0; ni < txfm_size; ++ni) {
      EXPECT_LE(abs(output[ni] - ref_output[ni]), max_error[tx_size])
          << "tx_size = " << tx_size << ", ni = " << ni
          << ", output[ni] = " << output[ni]
          << ", ref_output[ni] = " << ref_output[ni];
    }
  }
}

static INLINE int get_max_bit(int x) {
  int max_bit = -1;
  while (x) {
    x = x >> 1;
    max_bit++;
  }
  return max_bit;
}

TEST(av1_inv_txfm1d, get_max_bit) {
  int max_bit = get_max_bit(8);
  EXPECT_EQ(max_bit, 3);
}

TEST(av1_inv_txfm1d, round_trip) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  for (int si = 0; si < NELEMENTS(fwd_txfm_func_ls); ++si) {
    int txfm_size = txfm_size_ls[si];

    for (int ti = 0; ti < txfm_type_num; ++ti) {
      TxfmFunc fwd_txfm_func = fwd_txfm_func_ls[si][ti];
      TxfmFunc inv_txfm_func = inv_txfm_func_ls[si][ti];
      int max_error = 2;

      if (!fwd_txfm_func) continue;

      const int count_test_block = 5000;
      for (int i = 0; i < count_test_block; ++i) {
        int32_t input[64];
        int32_t output[64];
        int32_t round_trip_output[64];

        ASSERT_LE(txfm_size, NELEMENTS(input));

        for (int ni = 0; ni < txfm_size; ++ni) {
          input[ni] = rnd.Rand16() % input_base - rnd.Rand16() % input_base;
        }

        fwd_txfm_func(input, output, cos_bit, range_bit);
        inv_txfm_func(output, round_trip_output, cos_bit, range_bit);

        for (int ni = 0; ni < txfm_size; ++ni) {
          int node_err =
              abs(input[ni] - round_shift(round_trip_output[ni],
                                          get_max_bit(txfm_size) - 1));
          EXPECT_LE(node_err, max_error);
        }
      }
    }
  }
}

}  // namespace
