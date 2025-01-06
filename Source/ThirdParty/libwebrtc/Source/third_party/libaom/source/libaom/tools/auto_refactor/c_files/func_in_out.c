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

typedef struct XD {
  int u;
  int v;
} XD;

typedef struct RD {
  XD *xd;
  int u;
  int v;
} RD;

typedef struct VP9_COMP {
  int y;
  RD *rd;
  RD rd2;
  int arr[3];
  union {
    int z;
  };
  struct {
    int w;
  };
} VP9_COMP;

int sub_func(VP9_COMP *cpi, int b) {
  int d;
  cpi->y += 1;
  cpi->y -= b;
  d = cpi->y * 2;
  return d;
}

int func_id_forrest_show(VP9_COMP *cpi, int b) {
  int c = 2;
  int x = cpi->y + c * 2 + 1;
  int y;
  RD *rd = cpi->rd;
  y = cpi->rd->u;
  return x + y;
}

int func_link_id_chain_1(VP9_COMP *cpi) {
  RD *rd = cpi->rd;
  rd->u = 0;
}

int func_link_id_chain_2(VP9_COMP *cpi) {
  RD *rd = cpi->rd;
  XD *xd = rd->xd;
  xd->u = 0;
}

int func_assign_refer_status_1(VP9_COMP *cpi) { RD *rd = cpi->rd; }

int func_assign_refer_status_2(VP9_COMP *cpi) {
  RD *rd2;
  rd2 = cpi->rd;
}

int func_assign_refer_status_3(VP9_COMP *cpi) {
  int a;
  a = cpi->y;
}

int func_assign_refer_status_4(VP9_COMP *cpi) {
  int *b;
  b = &cpi->y;
}

int func_assign_refer_status_5(VP9_COMP *cpi) {
  RD *rd5;
  rd5 = &cpi->rd2;
}

int func_assign_refer_status_6(VP9_COMP *cpi, VP9_COMP *cpi2) {
  cpi->rd = cpi2->rd;
}

int func_assign_refer_status_7(VP9_COMP *cpi, VP9_COMP *cpi2) {
  cpi->arr[3] = 0;
}

int func_assign_refer_status_8(VP9_COMP *cpi, VP9_COMP *cpi2) {
  int x = cpi->arr[3];
}

int func_assign_refer_status_9(VP9_COMP *cpi) {
  {
    RD *rd = cpi->rd;
    { rd->u = 0; }
  }
}

int func_assign_refer_status_10(VP9_COMP *cpi) { cpi->arr[cpi->rd->u] = 0; }

int func_assign_refer_status_11(VP9_COMP *cpi) {
  RD *rd11 = &cpi->rd2;
  rd11->v = 1;
}

int func_assign_refer_status_12(VP9_COMP *cpi, VP9_COMP *cpi2) {
  *cpi->rd = *cpi2->rd;
}

int func_assign_refer_status_13(VP9_COMP *cpi) {
  cpi->z = 0;
  cpi->w = 0;
}

int func(VP9_COMP *cpi, int x) {
  int a;
  cpi->y = 4;
  a = 3 + cpi->y;
  a = a * x;
  cpi->y *= 4;
  RD *ref_rd = cpi->rd;
  ref_rd->u = 0;
  cpi->rd2.v = 1;
  cpi->rd->v = 1;
  RD *ref_rd2 = &cpi->rd2;
  RD **ref_rd3 = &(&cpi->rd2);
  int b = sub_func(cpi, a);
  cpi->rd->v++;
  return b;
}

int func_sub_call_1(VP9_COMP *cpi2, int x) { cpi2->y = 4; }

int func_call_1(VP9_COMP *cpi, int y) { func_sub_call_1(cpi, y); }

int func_sub_call_2(VP9_COMP *cpi2, RD *rd, int x) { rd->u = 0; }

int func_call_2(VP9_COMP *cpi, int y) { func_sub_call_2(cpi, &cpi->rd, y); }

int func_sub_call_3(VP9_COMP *cpi2, int x) {}

int func_call_3(VP9_COMP *cpi, int y) { func_sub_call_3(cpi, ++cpi->y); }

int func_sub_sub_call_4(VP9_COMP *cpi3, XD *xd) {
  cpi3->rd.u = 0;
  xd->u = 0;
}

int func_sub_call_4(VP9_COMP *cpi2, RD *rd) {
  func_sub_sub_call_4(cpi2, rd->xd);
}

int func_call_4(VP9_COMP *cpi, int y) { func_sub_call_4(cpi, &cpi->rd); }

int func_sub_call_5(VP9_COMP *cpi) {
  cpi->y = 2;
  func_call_5(cpi);
}

int func_call_5(VP9_COMP *cpi) { func_sub_call_5(cpi); }

int func_compound_1(VP9_COMP *cpi) {
  for (int i = 0; i < 10; ++i) {
    cpi->y++;
  }
}

int func_compound_2(VP9_COMP *cpi) {
  for (int i = 0; i < cpi->y; ++i) {
    cpi->rd->u = i;
  }
}

int func_compound_3(VP9_COMP *cpi) {
  int i = 3;
  while (i > 0) {
    cpi->rd->u = i;
    i--;
  }
}

int func_compound_4(VP9_COMP *cpi) {
  while (cpi->y-- >= 0) {
  }
}

int func_compound_5(VP9_COMP *cpi) {
  do {
  } while (cpi->y-- >= 0);
}

int func_compound_6(VP9_COMP *cpi) {
  for (int i = 0; i < 10; ++i) cpi->y--;
}

int main(void) {
  int x;
  VP9_COMP cpi;
  RD rd;
  cpi->rd = rd;
  func(&cpi, x);
}
