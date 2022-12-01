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

#include <assert.h>
#include <limits.h>
#include <stdlib.h>

#include "config/aom_config.h"

#include "aom_dsp/bitwriter_buffer.h"
#include "aom_dsp/recenter.h"
#include "aom_ports/bitops.h"

int aom_wb_is_byte_aligned(const struct aom_write_bit_buffer *wb) {
  return (wb->bit_offset % CHAR_BIT == 0);
}

uint32_t aom_wb_bytes_written(const struct aom_write_bit_buffer *wb) {
  return wb->bit_offset / CHAR_BIT + (wb->bit_offset % CHAR_BIT > 0);
}

void aom_wb_write_bit(struct aom_write_bit_buffer *wb, int bit) {
  const int off = (int)wb->bit_offset;
  const int p = off / CHAR_BIT;
  const int q = CHAR_BIT - 1 - off % CHAR_BIT;
  if (q == CHAR_BIT - 1) {
    // Zero next char and write bit
    wb->bit_buffer[p] = bit << q;
  } else {
    wb->bit_buffer[p] &= ~(1 << q);
    wb->bit_buffer[p] |= bit << q;
  }
  wb->bit_offset = off + 1;
}

void aom_wb_overwrite_bit(struct aom_write_bit_buffer *wb, int bit) {
  // Do not zero bytes but overwrite exisiting values
  const int off = (int)wb->bit_offset;
  const int p = off / CHAR_BIT;
  const int q = CHAR_BIT - 1 - off % CHAR_BIT;
  wb->bit_buffer[p] &= ~(1 << q);
  wb->bit_buffer[p] |= bit << q;
  wb->bit_offset = off + 1;
}

void aom_wb_write_literal(struct aom_write_bit_buffer *wb, int data, int bits) {
  assert(bits <= 31);
  int bit;
  for (bit = bits - 1; bit >= 0; bit--) aom_wb_write_bit(wb, (data >> bit) & 1);
}

void aom_wb_write_unsigned_literal(struct aom_write_bit_buffer *wb,
                                   uint32_t data, int bits) {
  assert(bits <= 32);
  int bit;
  for (bit = bits - 1; bit >= 0; bit--) aom_wb_write_bit(wb, (data >> bit) & 1);
}

void aom_wb_overwrite_literal(struct aom_write_bit_buffer *wb, int data,
                              int bits) {
  int bit;
  for (bit = bits - 1; bit >= 0; bit--)
    aom_wb_overwrite_bit(wb, (data >> bit) & 1);
}

void aom_wb_write_inv_signed_literal(struct aom_write_bit_buffer *wb, int data,
                                     int bits) {
  aom_wb_write_literal(wb, data, bits + 1);
}

void aom_wb_write_uvlc(struct aom_write_bit_buffer *wb, uint32_t v) {
  int64_t shift_val = ++v;
  int leading_zeroes = 1;

  assert(shift_val > 0);

  while (shift_val >>= 1) leading_zeroes += 2;

  aom_wb_write_literal(wb, 0, leading_zeroes >> 1);
  aom_wb_write_unsigned_literal(wb, v, (leading_zeroes + 1) >> 1);
}

static void wb_write_primitive_quniform(struct aom_write_bit_buffer *wb,
                                        uint16_t n, uint16_t v) {
  if (n <= 1) return;
  const int l = get_msb(n) + 1;
  const int m = (1 << l) - n;
  if (v < m) {
    aom_wb_write_literal(wb, v, l - 1);
  } else {
    aom_wb_write_literal(wb, m + ((v - m) >> 1), l - 1);
    aom_wb_write_bit(wb, (v - m) & 1);
  }
}

static void wb_write_primitive_subexpfin(struct aom_write_bit_buffer *wb,
                                         uint16_t n, uint16_t k, uint16_t v) {
  int i = 0;
  int mk = 0;
  while (1) {
    int b = (i ? k + i - 1 : k);
    int a = (1 << b);
    if (n <= mk + 3 * a) {
      wb_write_primitive_quniform(wb, n - mk, v - mk);
      break;
    } else {
      int t = (v >= mk + a);
      aom_wb_write_bit(wb, t);
      if (t) {
        i = i + 1;
        mk += a;
      } else {
        aom_wb_write_literal(wb, v - mk, b);
        break;
      }
    }
  }
}

static void wb_write_primitive_refsubexpfin(struct aom_write_bit_buffer *wb,
                                            uint16_t n, uint16_t k,
                                            uint16_t ref, uint16_t v) {
  wb_write_primitive_subexpfin(wb, n, k, recenter_finite_nonneg(n, ref, v));
}

void aom_wb_write_signed_primitive_refsubexpfin(struct aom_write_bit_buffer *wb,
                                                uint16_t n, uint16_t k,
                                                int16_t ref, int16_t v) {
  ref += n - 1;
  v += n - 1;
  const uint16_t scaled_n = (n << 1) - 1;
  wb_write_primitive_refsubexpfin(wb, scaled_n, k, ref, v);
}
