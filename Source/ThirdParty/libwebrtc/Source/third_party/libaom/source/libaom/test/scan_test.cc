/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "av1/common/scan.h"
#include "av1/common/txb_common.h"
#include "gtest/gtest.h"
#include "test/av1_txfm_test.h"

static int scan_test(const int16_t *scan, const int16_t *iscan, int si, int r,
                     int c, int h) {
  if (iscan[c * h + r] != si || scan[si] != c * h + r) {
    printf("r %d c %d ref_iscan %d iscan %d ref_scan %d scan %d\n", r, c, si,
           iscan[c * h + r], c * h + r, scan[si]);
    return 1;
  } else {
    return 0;
  }
}

static int scan_order_test(const SCAN_ORDER *scan_order, int w, int h,
                           SCAN_MODE mode) {
  const int16_t *scan = scan_order->scan;
  const int16_t *iscan = scan_order->iscan;
  int dim = w + h - 1;
  if (mode == SCAN_MODE_ZIG_ZAG) {
    int si = 0;
    for (int i = 0; i < dim; ++i) {
      if (i % 2 == 0) {
        for (int c = 0; c < w; ++c) {
          int r = i - c;
          if (r >= 0 && r < h) {
            if (scan_test(scan, iscan, si, r, c, h)) return 1;
            ++si;
          }
        }
      } else {
        for (int r = 0; r < h; ++r) {
          int c = i - r;
          if (c >= 0 && c < w) {
            if (scan_test(scan, iscan, si, r, c, h)) return 1;
            ++si;
          }
        }
      }
    }
  } else if (mode == SCAN_MODE_COL_DIAG) {
    int si = 0;
    for (int i = 0; i < dim; ++i) {
      for (int c = 0; c < w; ++c) {
        int r = i - c;
        if (r >= 0 && r < h) {
          if (scan_test(scan, iscan, si, r, c, h)) return 1;
          ++si;
        }
      }
    }
  } else if (mode == SCAN_MODE_ROW_DIAG) {
    int si = 0;
    for (int i = 0; i < dim; ++i) {
      for (int r = 0; r < h; ++r) {
        int c = i - r;
        if (c >= 0 && c < w) {
          if (scan_test(scan, iscan, si, r, c, h)) return 1;
          ++si;
        }
      }
    }
  } else if (mode == SCAN_MODE_ROW_1D) {
    int si = 0;
    for (int r = 0; r < h; ++r) {
      for (int c = 0; c < w; ++c) {
        if (scan_test(scan, iscan, si, r, c, h)) return 1;
        ++si;
      }
    }
  } else {
    assert(mode == SCAN_MODE_COL_1D);
    int si = 0;
    for (int c = 0; c < w; ++c) {
      for (int r = 0; r < h; ++r) {
        if (scan_test(scan, iscan, si, r, c, h)) return 1;
        ++si;
      }
    }
  }
  return 0;
}

TEST(Av1ScanTest, Dependency) {
  for (int tx_size = TX_4X4; tx_size < TX_SIZES_ALL; ++tx_size) {
    const int org_rows = tx_size_high[(TX_SIZE)tx_size];
    const int org_cols = tx_size_wide[(TX_SIZE)tx_size];
    const int rows = get_txb_high((TX_SIZE)tx_size);
    const int cols = get_txb_wide((TX_SIZE)tx_size);
    for (int tx_type = 0; tx_type < TX_TYPES; ++tx_type) {
      if (libaom_test::IsTxSizeTypeValid(static_cast<TX_SIZE>(tx_size),
                                         static_cast<TX_TYPE>(tx_type)) ==
          false) {
        continue;
      }
      SCAN_MODE scan_mode;
      TX_CLASS tx_class = tx_type_to_class[(TX_TYPE)tx_type];
      if (tx_class == TX_CLASS_2D) {
        if (rows == cols) {
          scan_mode = SCAN_MODE_ZIG_ZAG;
        } else if (rows > cols) {
          scan_mode = SCAN_MODE_ROW_DIAG;
        } else {
          scan_mode = SCAN_MODE_COL_DIAG;
        }
      } else if (tx_class == TX_CLASS_VERT) {
        scan_mode = SCAN_MODE_ROW_1D;
      } else {
        assert(tx_class == TX_CLASS_HORIZ);
        scan_mode = SCAN_MODE_COL_1D;
      }
      const SCAN_ORDER *scan_order =
          get_default_scan((TX_SIZE)tx_size, (TX_TYPE)tx_type);
      ASSERT_EQ(scan_order_test(scan_order, cols, rows, scan_mode), 0)
          << "scan mismatch tx_class " << tx_class << " tx_type " << tx_type
          << " tx_w " << org_cols << " tx_h " << org_rows << " scan_mode "
          << scan_mode << "\n";
    }
  }
}
