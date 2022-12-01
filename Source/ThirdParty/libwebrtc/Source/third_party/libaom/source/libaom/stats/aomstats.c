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

#include "stats/aomstats.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "aom_dsp/aom_dsp_common.h"
#include "common/tools_common.h"

int stats_open_file(stats_io_t *stats, const char *fpf, int pass) {
  int res;
  stats->pass = pass;

  if (pass == 0) {
    stats->file = fopen(fpf, "wb");
    stats->buf.sz = 0;
    stats->buf.buf = NULL;
    res = (stats->file != NULL);
  } else {
    size_t nbytes;

    stats->file = fopen(fpf, "rb");

    if (stats->file == NULL) fatal("First-pass stats file does not exist!");

    if (fseek(stats->file, 0, SEEK_END))
      fatal("First-pass stats file must be seekable!");

    stats->buf.sz = stats->buf_alloc_sz = ftell(stats->file);
    rewind(stats->file);

    stats->buf.buf = malloc(stats->buf_alloc_sz);

    if (!stats->buf.buf)
      fatal("Failed to allocate first-pass stats buffer (%u bytes)",
            (unsigned int)stats->buf_alloc_sz);

    nbytes = fread(stats->buf.buf, 1, stats->buf.sz, stats->file);
    res = (nbytes == stats->buf.sz);
  }

  return res;
}

int stats_open_mem(stats_io_t *stats, int pass) {
  int res;
  stats->pass = pass;

  if (!pass) {
    stats->buf.sz = 0;
    stats->buf_alloc_sz = 64 * 1024;
    stats->buf.buf = malloc(stats->buf_alloc_sz);
  }

  stats->buf_ptr = stats->buf.buf;
  res = (stats->buf.buf != NULL);
  return res;
}

void stats_close(stats_io_t *stats, int last_pass) {
  if (stats->file) {
    if (stats->pass == last_pass) {
      free(stats->buf.buf);
    }

    fclose(stats->file);
    stats->file = NULL;
  } else {
    if (stats->pass == last_pass) free(stats->buf.buf);
  }
}

void stats_write(stats_io_t *stats, const void *pkt, size_t len) {
  if (stats->file) {
    (void)fwrite(pkt, 1, len, stats->file);
    return;
  }
  assert(stats->buf.sz <= stats->buf_alloc_sz);
  assert(0 < stats->buf_alloc_sz);
  if (stats->buf.sz + len > stats->buf_alloc_sz) {
    // Grow by a factor of 1.5 each time, for amortized constant time.
    // Also make sure there is enough room for the data.
    size_t new_sz = AOMMAX((3 * stats->buf_alloc_sz) / 2, stats->buf.sz + len);
    char *new_ptr = realloc(stats->buf.buf, new_sz);

    if (new_ptr) {
      stats->buf_ptr = new_ptr + (stats->buf_ptr - (char *)stats->buf.buf);
      stats->buf.buf = new_ptr;
      stats->buf_alloc_sz = new_sz;
    } else {
      fatal("Failed to realloc firstpass stats buffer.");
    }
  }

  memcpy(stats->buf_ptr, pkt, len);
  stats->buf.sz += len;
  stats->buf_ptr += len;
}

aom_fixed_buf_t stats_get(stats_io_t *stats) { return stats->buf; }
