/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "aom_dsp/bitwriter.h"
#include "aom_dsp/binary_codes_writer.h"
#include "aom_dsp/recenter.h"
#include "aom_ports/bitops.h"

// Encodes a value v in [0, n-1] quasi-uniformly
static void write_primitive_quniform(aom_writer *w, uint16_t n, uint16_t v) {
  if (n <= 1) return;
  const int l = get_msb(n) + 1;
  const int m = (1 << l) - n;
  if (v < m) {
    aom_write_literal(w, v, l - 1);
  } else {
    aom_write_literal(w, m + ((v - m) >> 1), l - 1);
    aom_write_bit(w, (v - m) & 1);
  }
}

static int count_primitive_quniform(uint16_t n, uint16_t v) {
  if (n <= 1) return 0;
  const int l = get_msb(n) + 1;
  const int m = (1 << l) - n;
  return v < m ? l - 1 : l;
}

// Finite subexponential code that codes a symbol v in [0, n-1] with parameter k
static void write_primitive_subexpfin(aom_writer *w, uint16_t n, uint16_t k,
                                      uint16_t v) {
  int i = 0;
  int mk = 0;
  while (1) {
    int b = (i ? k + i - 1 : k);
    int a = (1 << b);
    if (n <= mk + 3 * a) {
      write_primitive_quniform(w, n - mk, v - mk);
      break;
    } else {
      int t = (v >= mk + a);
      aom_write_bit(w, t);
      if (t) {
        i = i + 1;
        mk += a;
      } else {
        aom_write_literal(w, v - mk, b);
        break;
      }
    }
  }
}

static int count_primitive_subexpfin(uint16_t n, uint16_t k, uint16_t v) {
  int count = 0;
  int i = 0;
  int mk = 0;
  while (1) {
    int b = (i ? k + i - 1 : k);
    int a = (1 << b);
    if (n <= mk + 3 * a) {
      count += count_primitive_quniform(n - mk, v - mk);
      break;
    } else {
      int t = (v >= mk + a);
      count++;
      if (t) {
        i = i + 1;
        mk += a;
      } else {
        count += b;
        break;
      }
    }
  }
  return count;
}

// Finite subexponential code that codes a symbol v in [0, n-1] with parameter k
// based on a reference ref also in [0, n-1].
// Recenters symbol around r first and then uses a finite subexponential code.
void aom_write_primitive_refsubexpfin(aom_writer *w, uint16_t n, uint16_t k,
                                      uint16_t ref, uint16_t v) {
  write_primitive_subexpfin(w, n, k, recenter_finite_nonneg(n, ref, v));
}

int aom_count_primitive_refsubexpfin(uint16_t n, uint16_t k, uint16_t ref,
                                     uint16_t v) {
  return count_primitive_subexpfin(n, k, recenter_finite_nonneg(n, ref, v));
}

int aom_count_signed_primitive_refsubexpfin(uint16_t n, uint16_t k, int16_t ref,
                                            int16_t v) {
  ref += n - 1;
  v += n - 1;
  const uint16_t scaled_n = (n << 1) - 1;
  return aom_count_primitive_refsubexpfin(scaled_n, k, ref, v);
}
