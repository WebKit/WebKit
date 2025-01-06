/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>

#include "config/aom_config.h"

#include "aom_dsp/bitreader_buffer.h"
#include "aom_dsp/recenter.h"
#include "aom_ports/bitops.h"

size_t aom_rb_bytes_read(const struct aom_read_bit_buffer *rb) {
  return (rb->bit_offset + 7) >> 3;
}

int aom_rb_read_bit(struct aom_read_bit_buffer *rb) {
  const uint32_t off = rb->bit_offset;
  const uint32_t p = off >> 3;
  const int q = 7 - (int)(off & 0x7);
  if (rb->bit_buffer + p < rb->bit_buffer_end) {
    const int bit = (rb->bit_buffer[p] >> q) & 1;
    rb->bit_offset = off + 1;
    return bit;
  } else {
    if (rb->error_handler) rb->error_handler(rb->error_handler_data);
    return 0;
  }
}

int aom_rb_read_literal(struct aom_read_bit_buffer *rb, int bits) {
  assert(bits <= 31);
  int value = 0, bit;
  for (bit = bits - 1; bit >= 0; bit--) value |= aom_rb_read_bit(rb) << bit;
  return value;
}

#if CONFIG_AV1_DECODER
uint32_t aom_rb_read_unsigned_literal(struct aom_read_bit_buffer *rb,
                                      int bits) {
  assert(bits <= 32);
  uint32_t value = 0;
  int bit;
  for (bit = bits - 1; bit >= 0; bit--)
    value |= (uint32_t)aom_rb_read_bit(rb) << bit;
  return value;
}

int aom_rb_read_inv_signed_literal(struct aom_read_bit_buffer *rb, int bits) {
  const int nbits = sizeof(unsigned) * 8 - bits - 1;
  const unsigned value = (unsigned)aom_rb_read_literal(rb, bits + 1) << nbits;
  return ((int)value) >> nbits;
}
#endif  // CONFIG_AV1_DECODER

uint32_t aom_rb_read_uvlc(struct aom_read_bit_buffer *rb) {
  int leading_zeros = 0;
  while (leading_zeros < 32 && !aom_rb_read_bit(rb)) ++leading_zeros;
  // Maximum 32 bits.
  if (leading_zeros == 32) return UINT32_MAX;
  const uint32_t base = (1u << leading_zeros) - 1;
  const uint32_t value = aom_rb_read_literal(rb, leading_zeros);
  return base + value;
}

#if CONFIG_AV1_DECODER
static uint16_t aom_rb_read_primitive_quniform(struct aom_read_bit_buffer *rb,
                                               uint16_t n) {
  if (n <= 1) return 0;
  const int l = get_msb(n) + 1;
  const int m = (1 << l) - n;
  const int v = aom_rb_read_literal(rb, l - 1);
  return v < m ? v : (v << 1) - m + aom_rb_read_bit(rb);
}

static uint16_t aom_rb_read_primitive_subexpfin(struct aom_read_bit_buffer *rb,
                                                uint16_t n, uint16_t k) {
  int i = 0;
  int mk = 0;

  while (1) {
    int b = (i ? k + i - 1 : k);
    int a = (1 << b);

    if (n <= mk + 3 * a) {
      return aom_rb_read_primitive_quniform(rb, n - mk) + mk;
    }

    if (!aom_rb_read_bit(rb)) {
      return aom_rb_read_literal(rb, b) + mk;
    }

    i = i + 1;
    mk += a;
  }

  assert(0);
  return 0;
}

static uint16_t aom_rb_read_primitive_refsubexpfin(
    struct aom_read_bit_buffer *rb, uint16_t n, uint16_t k, uint16_t ref) {
  return inv_recenter_finite_nonneg(n, ref,
                                    aom_rb_read_primitive_subexpfin(rb, n, k));
}

int16_t aom_rb_read_signed_primitive_refsubexpfin(
    struct aom_read_bit_buffer *rb, uint16_t n, uint16_t k, int16_t ref) {
  ref += n - 1;
  const uint16_t scaled_n = (n << 1) - 1;
  return aom_rb_read_primitive_refsubexpfin(rb, scaled_n, k, ref) - n + 1;
}
#endif  // CONFIG_AV1_DECODER
