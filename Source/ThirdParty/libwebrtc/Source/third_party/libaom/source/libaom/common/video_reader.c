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
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "aom_ports/mem_ops.h"
#include "common/ivfdec.h"
#include "common/obudec.h"
#include "common/tools_common.h"
#include "common/video_reader.h"
#include "common/webmdec.h"

struct AvxVideoReaderStruct {
  AvxVideoInfo info;
  struct AvxInputContext input_ctx;
  struct ObuDecInputContext obu_ctx;
  struct WebmInputContext webm_ctx;
  uint8_t *buffer;
  size_t buffer_size;
  size_t frame_size;
  aom_codec_pts_t pts;
};

AvxVideoReader *aom_video_reader_open(const char *filename) {
  AvxVideoReader *reader = NULL;
  const bool using_file = strcmp(filename, "-") != 0;
  FILE *const file =
      using_file ? fopen(filename, "rb") : set_binary_mode(stdin);
  if (!file) return NULL;  // Can't open file

  reader = (AvxVideoReader *)calloc(1, sizeof(*reader));
  if (!reader) {
    fclose(file);
    return NULL;  // Can't allocate AvxVideoReader
  }

  reader->input_ctx.filename = filename;
  reader->input_ctx.file = file;
  reader->obu_ctx.avx_ctx = &reader->input_ctx;
  reader->obu_ctx.is_annexb = 1;

  // TODO(https://crbug.com/aomedia/1706): webm type does not support reading
  // from stdin yet, and file_is_webm is not using the detect buffer when
  // determining the type. Therefore it should only be checked when using a file
  // and needs to be checked prior to other types.
  if (false) {
#if CONFIG_WEBM_IO
  } else if (using_file &&
             file_is_webm(&reader->webm_ctx, &reader->input_ctx)) {
    reader->input_ctx.file_type = FILE_TYPE_WEBM;
    reader->info.codec_fourcc = reader->input_ctx.fourcc;
    reader->info.frame_width = reader->input_ctx.width;
    reader->info.frame_height = reader->input_ctx.height;
#endif
  } else if (file_is_ivf(&reader->input_ctx)) {
    reader->input_ctx.file_type = FILE_TYPE_IVF;
    reader->info.codec_fourcc = reader->input_ctx.fourcc;
    reader->info.frame_width = reader->input_ctx.width;
    reader->info.frame_height = reader->input_ctx.height;
  } else if (file_is_obu(&reader->obu_ctx)) {
    reader->input_ctx.file_type = FILE_TYPE_OBU;
    // assume AV1
    reader->info.codec_fourcc = AV1_FOURCC;
    reader->info.is_annexb = reader->obu_ctx.is_annexb;
  } else {
    fclose(file);
    free(reader);
    return NULL;  // Unknown file type
  }

  return reader;
}

void aom_video_reader_close(AvxVideoReader *reader) {
  if (reader) {
    fclose(reader->input_ctx.file);
    if (reader->input_ctx.file_type == FILE_TYPE_OBU) {
      obudec_free(&reader->obu_ctx);
    }
    free(reader->buffer);
    free(reader);
  }
}

int aom_video_reader_read_frame(AvxVideoReader *reader) {
  if (reader->input_ctx.file_type == FILE_TYPE_IVF) {
    return !ivf_read_frame(&reader->input_ctx, &reader->buffer,
                           &reader->frame_size, &reader->buffer_size,
                           &reader->pts);
  } else if (reader->input_ctx.file_type == FILE_TYPE_OBU) {
    return !obudec_read_temporal_unit(&reader->obu_ctx, &reader->buffer,
                                      &reader->frame_size,
                                      &reader->buffer_size);
#if CONFIG_WEBM_IO
  } else if (reader->input_ctx.file_type == FILE_TYPE_WEBM) {
    return !webm_read_frame(&reader->webm_ctx, &reader->buffer,
                            &reader->frame_size, &reader->buffer_size);
#endif
  } else {
    assert(0);
    return 0;
  }
}

const uint8_t *aom_video_reader_get_frame(AvxVideoReader *reader,
                                          size_t *size) {
  if (size) *size = reader->frame_size;

  return reader->buffer;
}

int64_t aom_video_reader_get_frame_pts(AvxVideoReader *reader) {
  return (int64_t)reader->pts;
}

FILE *aom_video_reader_get_file(AvxVideoReader *reader) {
  return reader->input_ctx.file;
}

const AvxVideoInfo *aom_video_reader_get_info(AvxVideoReader *reader) {
  return &reader->info;
}

void aom_video_reader_set_fourcc(AvxVideoReader *reader, uint32_t fourcc) {
  reader->info.codec_fourcc = fourcc;
}
