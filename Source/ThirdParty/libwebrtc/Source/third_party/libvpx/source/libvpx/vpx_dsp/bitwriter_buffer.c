/*
 *  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <limits.h>
#include <stdlib.h>

#include "./vpx_config.h"
#include "./bitwriter_buffer.h"

size_t vpx_wb_bytes_written(const struct vpx_write_bit_buffer *wb) {
  return wb->bit_offset / CHAR_BIT + (wb->bit_offset % CHAR_BIT > 0);
}

void vpx_wb_write_bit(struct vpx_write_bit_buffer *wb, int bit) {
  const int off = (int)wb->bit_offset;
  const int p = off / CHAR_BIT;
  const int q = CHAR_BIT - 1 - off % CHAR_BIT;
  if (q == CHAR_BIT - 1) {
    wb->bit_buffer[p] = bit << q;
  } else {
    wb->bit_buffer[p] &= ~(1 << q);
    wb->bit_buffer[p] |= bit << q;
  }
  wb->bit_offset = off + 1;
}

void vpx_wb_write_literal(struct vpx_write_bit_buffer *wb, int data, int bits) {
  int bit;
  for (bit = bits - 1; bit >= 0; bit--) vpx_wb_write_bit(wb, (data >> bit) & 1);
}

void vpx_wb_write_inv_signed_literal(struct vpx_write_bit_buffer *wb, int data,
                                     int bits) {
  vpx_wb_write_literal(wb, abs(data), bits);
  vpx_wb_write_bit(wb, data < 0);
}
