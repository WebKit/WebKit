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

#include "aom_dsp/bitreader.h"

int aom_reader_init(aom_reader *r, const uint8_t *buffer, size_t size) {
  if (size && !buffer) {
    return 1;
  }
  r->buffer_end = buffer + size;
  r->buffer = buffer;
  od_ec_dec_init(&r->ec, buffer, (uint32_t)size);
#if CONFIG_ACCOUNTING
  r->accounting = NULL;
#endif
  return 0;
}

const uint8_t *aom_reader_find_begin(aom_reader *r) { return r->buffer; }

const uint8_t *aom_reader_find_end(aom_reader *r) { return r->buffer_end; }

uint32_t aom_reader_tell(const aom_reader *r) { return od_ec_dec_tell(&r->ec); }

uint32_t aom_reader_tell_frac(const aom_reader *r) {
  return od_ec_dec_tell_frac(&r->ec);
}

int aom_reader_has_overflowed(const aom_reader *r) {
  const uint32_t tell_bits = aom_reader_tell(r);
  const uint32_t tell_bytes = (tell_bits + 7) >> 3;
  return ((ptrdiff_t)tell_bytes > r->buffer_end - r->buffer);
}
