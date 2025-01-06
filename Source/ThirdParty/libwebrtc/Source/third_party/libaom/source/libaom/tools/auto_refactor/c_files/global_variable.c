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

extern const int global_a[13];

const int global_b = 0;

typedef struct S1 {
  int x;
} T1;

struct S3 {
  int x;
} s3;

int func_global_1(int *a) {
  *a = global_a[3];
  return 0;
}
