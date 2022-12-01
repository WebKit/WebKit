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

typedef struct S {
  int x;
  int y;
  int z;
} S;

typedef struct T {
  S s;
} T;

int d(S *s) {
  ++s->x;
  s->x--;
  s->y = s->y + 1;
  int *c = &s->x;
  S ss;
  ss.x = 1;
  ss.x += 2;
  ss.z *= 2;
  return 0;
}
int b(S *s) {
  d(s);
  return 0;
}
int c(int x) {
  if (x) {
    c(x - 1);
  } else {
    S s;
    d(&s);
  }
  return 0;
}
int a(S *s) {
  b(s);
  c(1);
  return 0;
}
int e() {
  c(0);
  return 0;
}
int main() {
  int p = 3;
  S s;
  s.x = p + 1;
  s.y = 2;
  s.z = 3;
  a(&s);
  T t;
  t.s.x = 3;
}
