/*
 * Copyright (c) 2021, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <stdint.h>
#include <vector>

#include "gtest/gtest.h"

#include "av1/encoder/block.h"
#include "av1/encoder/encodemb.h"
#include "av1/common/scan.h"

namespace {

// Reorders 'qcoeff_lexico', which is in lexicographic order (row by row), into
// scan order (zigzag) in 'qcoeff_scan'.
void ToScanOrder(TX_SIZE tx_size, TX_TYPE tx_type, tran_low_t *qcoeff_lexico,
                 tran_low_t *qcoeff_scan) {
  const int max_eob = av1_get_max_eob(tx_size);
  const SCAN_ORDER *const scan_order = get_scan(tx_size, tx_type);
  for (int i = 0; i < max_eob; ++i) {
    qcoeff_scan[i] = qcoeff_lexico[scan_order->scan[i]];
  }
}

// Reorders 'qcoeff_scan', which is in scan order (zigzag), into lexicographic
// order (row by row) in 'qcoeff_lexico'.
void ToLexicoOrder(TX_SIZE tx_size, TX_TYPE tx_type, tran_low_t *qcoeff_scan,
                   tran_low_t *qcoeff_lexico) {
  const int max_eob = av1_get_max_eob(tx_size);
  const SCAN_ORDER *const scan_order = get_scan(tx_size, tx_type);
  for (int i = 0; i < max_eob; ++i) {
    qcoeff_lexico[scan_order->scan[i]] = qcoeff_scan[i];
  }
}

// Runs coefficient dropout on 'qcoeff_scan'.
void Dropout(TX_SIZE tx_size, TX_TYPE tx_type, int dropout_num_before,
             int dropout_num_after, tran_low_t *qcoeff_scan) {
  tran_low_t qcoeff[MAX_TX_SQUARE];
  // qcoeff_scan is assumed to be in scan order, since tests are easier to
  // understand this way, but av1_dropout_qcoeff expects coeffs in lexico order
  // so we convert to lexico then back to scan afterwards.
  ToLexicoOrder(tx_size, tx_type, qcoeff_scan, qcoeff);

  const int max_eob = av1_get_max_eob(tx_size);
  const int kDequantFactor = 10;
  tran_low_t dqcoeff[MAX_TX_SQUARE];
  for (int i = 0; i < max_eob; ++i) {
    dqcoeff[i] = qcoeff[i] * kDequantFactor;
  }

  uint16_t eob = max_eob;
  while (eob > 0 && qcoeff_scan[eob - 1] == 0) --eob;

  MACROBLOCK mb;
  const int kPlane = 0;
  const int kBlock = 0;
  memset(&mb, 0, sizeof(mb));
  uint16_t eobs[] = { eob };
  mb.plane[kPlane].eobs = eobs;
  mb.plane[kPlane].qcoeff = qcoeff;
  mb.plane[kPlane].dqcoeff = dqcoeff;
  uint8_t txb_entropy_ctx[1];
  mb.plane[kPlane].txb_entropy_ctx = txb_entropy_ctx;

  av1_dropout_qcoeff_num(&mb, kPlane, kBlock, tx_size, tx_type,
                         dropout_num_before, dropout_num_after);

  ToScanOrder(tx_size, tx_type, qcoeff, qcoeff_scan);

  // Check updated eob value is valid.
  uint16_t new_eob = max_eob;
  while (new_eob > 0 && qcoeff_scan[new_eob - 1] == 0) --new_eob;
  EXPECT_EQ(new_eob, mb.plane[kPlane].eobs[0]);

  // Check dqcoeff is still valid.
  for (int i = 0; i < max_eob; ++i) {
    EXPECT_EQ(qcoeff[i] * kDequantFactor, dqcoeff[i]);
  }
}

void ExpectArrayEq(tran_low_t *actual, std::vector<tran_low_t> expected) {
  for (size_t i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(expected[i], actual[i]) << "Arrays differ at index " << i;
  }
}

static constexpr TX_TYPE kTxType = DCT_DCT;

TEST(DropoutTest, KeepsLargeCoeffs) {
  const TX_SIZE tx_size = TX_8X4;
  const uint32_t dropout_num_before = 4;
  const uint32_t dropout_num_after = 6;
  // Large isolated coeffs should be preserved.
  tran_low_t qcoeff_scan[] = { 0, 0, 0, 0, 0, 0, 42, 0,    // should be kept
                               0, 0, 0, 0, 0, 0, 0,  0,    //
                               0, 0, 0, 0, 0, 0, 0,  -30,  // should be kept
                               0, 0, 0, 0, 0, 0, 0,  0 };
  Dropout(tx_size, kTxType, dropout_num_before, dropout_num_after, qcoeff_scan);
  ExpectArrayEq(qcoeff_scan, { 0, 0, 0, 0, 0, 0, 42, 0,    //
                               0, 0, 0, 0, 0, 0, 0,  0,    //
                               0, 0, 0, 0, 0, 0, 0,  -30,  //
                               0, 0, 0, 0, 0, 0, 0,  0 });
}

TEST(DropoutTest, RemovesSmallIsolatedCoeffs) {
  const TX_SIZE tx_size = TX_8X4;
  const uint32_t dropout_num_before = 4;
  const uint32_t dropout_num_after = 6;
  // Small isolated coeffs should be removed.
  tran_low_t qcoeff_scan[] = { 0, 0, 0, 0, 1,  0, 0, 0,  // should be removed
                               0, 0, 0, 0, 0,  0, 0, 0,  //
                               0, 0, 0, 0, -2, 0, 0, 0,  // should be removed
                               0, 0, 0, 0, 0,  0, 0, 0 };
  Dropout(tx_size, kTxType, dropout_num_before, dropout_num_after, qcoeff_scan);
  ExpectArrayEq(qcoeff_scan, { 0, 0, 0, 0, 0, 0, 0, 0,  //
                               0, 0, 0, 0, 0, 0, 0, 0,  //
                               0, 0, 0, 0, 0, 0, 0, 0,  //
                               0, 0, 0, 0, 0, 0, 0, 0 });
}

TEST(DropoutTest, KeepsSmallCoeffsAmongLargeOnes) {
  const TX_SIZE tx_size = TX_8X4;
  const uint32_t dropout_num_before = 4;
  const uint32_t dropout_num_after = 6;
  // Small coeffs that are not isolated (not enough zeros before/after should be
  // kept).
  tran_low_t qcoeff_scan[] = {
    1, 0,  0, 0,  -5, 0, 0, -1,  // should be kept
    0, 0,  0, 10, 0,  0, 2, 0,   // should be kept
    0, 0,  0, 0,  0,  0, 0, 0,   //
    0, -2, 0, 0,  0,  0, 0, 0    // should be removed
  };                             // should be removed
  Dropout(tx_size, kTxType, dropout_num_before, dropout_num_after, qcoeff_scan);
  ExpectArrayEq(qcoeff_scan, { 1, 0, 0, 0,  -5, 0, 0, -1,  //
                               0, 0, 0, 10, 0,  0, 2, 0,   //
                               0, 0, 0, 0,  0,  0, 0, 0,   //
                               0, 0, 0, 0,  0,  0, 0, 0 });
}

TEST(DropoutTest, KeepsSmallCoeffsCloseToStartOrEnd) {
  const TX_SIZE tx_size = TX_8X4;
  const uint32_t dropout_num_before = 4;
  const uint32_t dropout_num_after = 6;
  // Small coeffs that are too close to the beginning or end of the block
  // should also be kept (not enough zeroes before/after).
  tran_low_t qcoeff_scan[] = { 0, 0, -1, 0,  0, 0, 0,  0,  // should be kept
                               0, 0, 0,  10, 0, 0, 0,  0,  // should be kept
                               0, 0, 0,  2,  0, 0, 0,  0,  // should be removed
                               0, 0, 0,  0,  0, 0, -1, 0 };  // should be kept
  Dropout(tx_size, kTxType, dropout_num_before, dropout_num_after, qcoeff_scan);
  ExpectArrayEq(qcoeff_scan, { 0, 0, -1, 0,  0, 0, 0,  0,  //
                               0, 0, 0,  10, 0, 0, 0,  0,  //
                               0, 0, 0,  0,  0, 0, 0,  0,  //
                               0, 0, 0,  0,  0, 0, -1, 0 });
}

TEST(DropoutTest, RemovesSmallClusterOfCoeffs) {
  const TX_SIZE tx_size = TX_8X4;
  const uint32_t dropout_num_before = 4;
  const uint32_t dropout_num_after = 6;
  // Small clusters (<= kDropoutContinuityMax) of small coeffs should be
  // removed.
  tran_low_t qcoeff_scan_two[] = {
    0, 0, 0, 0, 1, 0, 0, -1,  // should be removed
    0, 0, 0, 0, 0, 0, 0, 0,   //
    0, 0, 0, 0, 0, 0, 1, 0,   // should be removed
    0, 0, 0, 0, 0, 0, 0, 0
  };
  Dropout(tx_size, kTxType, dropout_num_before, dropout_num_after,
          qcoeff_scan_two);
  ExpectArrayEq(qcoeff_scan_two, { 0, 0, 0, 0, 0, 0, 0, 0,  //
                                   0, 0, 0, 0, 0, 0, 0, 0,  //
                                   0, 0, 0, 0, 0, 0, 0, 0,  //
                                   0, 0, 0, 0, 0, 0, 0, 0 });
}

TEST(DropoutTest, KeepsLargeClusterOfCoeffs) {
  const TX_SIZE tx_size = TX_8X4;
  const uint32_t dropout_num_before = 4;
  const uint32_t dropout_num_after = 6;
  // Large clusters (> kDropoutContinuityMax) of small coeffs should be kept.
  tran_low_t qcoeff_scan[] = { 0, 0, 0, 0, 1, 0,  1, -1,  // should be kept
                               0, 0, 0, 0, 0, 0,  0, 0,   //
                               0, 0, 0, 0, 0, -2, 0, 0,   // should be removed
                               0, 0, 0, 0, 0, 0,  0, 0 };
  Dropout(tx_size, kTxType, dropout_num_before, dropout_num_after, qcoeff_scan);
  ExpectArrayEq(qcoeff_scan, { 0, 0, 0, 0, 1, 0, 1, -1,  //
                               0, 0, 0, 0, 0, 0, 0, 0,   //
                               0, 0, 0, 0, 0, 0, 0, 0,   //
                               0, 0, 0, 0, 0, 0, 0, 0 });
}

TEST(DropoutTest, NumBeforeLargerThanNumAfter) {
  const TX_SIZE tx_size = TX_8X4;
  const uint32_t dropout_num_before = 4;
  const uint32_t dropout_num_after = 2;
  // The second coeff (-2) doesn't seem to meet the dropout_num_before
  // criteria. But since the first coeff (1) will be dropped, it will meet
  // the criteria and should be dropped too.
  tran_low_t qcoeff_scan[] = { 0,  0, 0, 0, 1, 0, 0, 0,  // should be removed
                               -2, 0, 0, 0, 0, 0, 0, 0,  // should be removed
                               0,  0, 0, 0, 0, 0, 0, 0,  //
                               0,  0, 0, 0, 0, 0, 0, 0 };
  Dropout(tx_size, kTxType, dropout_num_before, dropout_num_after, qcoeff_scan);
  ExpectArrayEq(qcoeff_scan, { 0, 0, 0, 0, 0, 0, 0, 0,  //
                               0, 0, 0, 0, 0, 0, 0, 0,  //
                               0, 0, 0, 0, 0, 0, 0, 0,  //
                               0, 0, 0, 0, 0, 0, 0, 0 });
}

// More complex test combining other test cases.
TEST(DropoutTest, ComplexTest) {
  const TX_SIZE tx_size = TX_8X8;
  const uint32_t dropout_num_before = 4;
  const uint32_t dropout_num_after = 2;
  tran_low_t qcoeff_scan[] = { 1, 12, 0,  0,   0, 0, 1,  0,   //
                               0, 0,  0,  -12, 0, 0, 0,  1,   //
                               0, 0,  -2, 0,   1, 0, 0,  1,   //
                               0, 0,  0,  0,   5, 0, -1, 0,   //
                               0, 0,  0,  1,   0, 0, 0,  -1,  //
                               0, 0,  0,  0,   2, 0, 0,  0,   //
                               0, 1,  0,  0,   0, 5, 0,  0,   //
                               0, 0,  1,  1,   0, 0, 0,  -2 };
  Dropout(tx_size, kTxType, dropout_num_before, dropout_num_after, qcoeff_scan);
  ExpectArrayEq(qcoeff_scan, { 1, 12, 0,  0,   0, 0, 0,  0,  //
                               0, 0,  0,  -12, 0, 0, 0,  1,  //
                               0, 0,  -2, 0,   1, 0, 0,  1,  //
                               0, 0,  0,  0,   5, 0, -1, 0,  //
                               0, 0,  0,  0,   0, 0, 0,  0,  //
                               0, 0,  0,  0,   0, 0, 0,  0,  //
                               0, 0,  0,  0,   0, 5, 0,  0,  //
                               0, 0,  0,  0,   0, 0, 0,  -2 });
}

}  // namespace
