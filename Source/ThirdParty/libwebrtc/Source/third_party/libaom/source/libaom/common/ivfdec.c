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

#include "common/ivfdec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aom_ports/mem_ops.h"
#include "aom_ports/sanitizer.h"

static const char *IVF_SIGNATURE = "DKIF";

static void fix_framerate(int *num, int *den) {
  if (*den <= 0 || *den >= 1000000000 || *num <= 0 || *num >= 1000) {
    // framerate seems to be invalid, just default to 30fps.
    *num = 30;
    *den = 1;
  }
}

int file_is_ivf(struct AvxInputContext *input_ctx) {
  char raw_hdr[32];
  int is_ivf = 0;

  if (fread(raw_hdr, 1, 32, input_ctx->file) == 32) {
    if (memcmp(IVF_SIGNATURE, raw_hdr, 4) == 0) {
      is_ivf = 1;

      if (mem_get_le16(raw_hdr + 4) != 0) {
        fprintf(stderr,
                "Error: Unrecognized IVF version! This file may not"
                " decode properly.\n");
      }

      input_ctx->fourcc = mem_get_le32(raw_hdr + 8);
      input_ctx->width = mem_get_le16(raw_hdr + 12);
      input_ctx->height = mem_get_le16(raw_hdr + 14);
      input_ctx->framerate.numerator = mem_get_le32(raw_hdr + 16);
      input_ctx->framerate.denominator = mem_get_le32(raw_hdr + 20);
      fix_framerate(&input_ctx->framerate.numerator,
                    &input_ctx->framerate.denominator);
    }
  }

  if (!is_ivf) {
    rewind(input_ctx->file);
    input_ctx->detect.buf_read = 0;
  } else {
    input_ctx->detect.position = 4;
  }
  return is_ivf;
}

int ivf_read_frame(FILE *infile, uint8_t **buffer, size_t *bytes_read,
                   size_t *buffer_size, aom_codec_pts_t *pts) {
  char raw_header[IVF_FRAME_HDR_SZ] = { 0 };
  size_t frame_size = 0;

  if (fread(raw_header, IVF_FRAME_HDR_SZ, 1, infile) != 1) {
    if (!feof(infile)) fprintf(stderr, "Warning: Failed to read frame size\n");
  } else {
    frame_size = mem_get_le32(raw_header);

    if (frame_size > 256 * 1024 * 1024) {
      fprintf(stderr, "Warning: Read invalid frame size (%u)\n",
              (unsigned int)frame_size);
      frame_size = 0;
    }

    if (frame_size > *buffer_size) {
      uint8_t *new_buffer = (uint8_t *)realloc(*buffer, 2 * frame_size);

      if (new_buffer) {
        *buffer = new_buffer;
        *buffer_size = 2 * frame_size;
      } else {
        fprintf(stderr, "Warning: Failed to allocate compressed data buffer\n");
        frame_size = 0;
      }
    }

    if (pts) {
      *pts = mem_get_le32(&raw_header[4]);
      *pts += ((aom_codec_pts_t)mem_get_le32(&raw_header[8]) << 32);
    }
  }

  if (!feof(infile)) {
    ASAN_UNPOISON_MEMORY_REGION(*buffer, *buffer_size);
    if (fread(*buffer, 1, frame_size, infile) != frame_size) {
      fprintf(stderr, "Warning: Failed to read full frame\n");
      return 1;
    }

    ASAN_POISON_MEMORY_REGION(*buffer + frame_size, *buffer_size - frame_size);
    *bytes_read = frame_size;
    return 0;
  }

  return 1;
}
