/*
 * Copyright (c) 2021, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

typedef struct S1 {
  int x;
} T1;

struct S3 {
  int x;
};

typedef struct {
  int x;
  struct S3 s3;
} T4;

typedef union U5 {
  int x;
  double y;
} T5;

typedef struct S6 {
  struct {
    int x;
  };
  union {
    int y;
    int z;
  };
} T6;

typedef struct S7 {
  struct {
    int x;
  } y;
  union {
    int w;
  } z;
} T7;

int main(void) {}
