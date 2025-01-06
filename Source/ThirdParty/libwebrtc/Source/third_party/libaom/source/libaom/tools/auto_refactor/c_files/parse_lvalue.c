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

typedef struct RD {
  int u;
  int v;
  int arr[3];
} RD;

typedef struct VP9_COMP {
  int y;
  RD *rd;
  RD rd2;
  RD rd3[2];
} VP9_COMP;

int parse_lvalue_2(VP9_COMP *cpi) { RD *rd2 = &cpi->rd2; }

int func(VP9_COMP *cpi, int x) {
  cpi->rd->u = 0;

  int y;
  y = 0;

  cpi->rd2.v = 0;

  cpi->rd->arr[2] = 0;

  cpi->rd3[1]->arr[2] = 0;

  return 0;
}

int main(void) {
  int x = 0;
  VP9_COMP cpi;
  func(&cpi, x);
}
