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

#include <string.h>
#include "aom_dsp/bitwriter.h"

void aom_start_encode(aom_writer *w, uint8_t *source) {
  w->buffer = source;
  w->pos = 0;
  od_ec_enc_init(&w->ec, 62025);
}

int aom_stop_encode(aom_writer *w) {
  int nb_bits;
  uint32_t bytes;
  unsigned char *data;
  data = od_ec_enc_done(&w->ec, &bytes);
  nb_bits = od_ec_enc_tell(&w->ec);
  memcpy(w->buffer, data, bytes);
  w->pos = bytes;
  od_ec_enc_clear(&w->ec);
  return nb_bits;
}

int aom_tell_size(aom_writer *w) {
  const int nb_bits = od_ec_enc_tell(&w->ec);
  return nb_bits;
}
