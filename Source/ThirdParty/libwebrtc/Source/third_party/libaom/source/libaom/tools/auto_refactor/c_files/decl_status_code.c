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

int parse_decl_node_2() { int arr[3]; }

int parse_decl_node_3() { int *a; }

int parse_decl_node_4() { T1 t1[3]; }

int parse_decl_node_5() { T1 *t2[3]; }

int parse_decl_node_6() { T1 t3[3][3]; }

int main() {
  int a;
  T1 t1;
  struct S1 s1;
  T1 *t2;
}
