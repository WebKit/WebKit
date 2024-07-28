/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include <assert.h>

#include "av1/common/obu_util.h"

#include "aom_dsp/bitreader_buffer.h"

static aom_codec_err_t read_obu_size(const uint8_t *data,
                                     size_t bytes_available,
                                     size_t *const obu_size,
                                     size_t *const length_field_size) {
  uint64_t u_obu_size = 0;
  if (aom_uleb_decode(data, bytes_available, &u_obu_size, length_field_size) !=
      0) {
    return AOM_CODEC_CORRUPT_FRAME;
  }

  if (u_obu_size > UINT32_MAX) return AOM_CODEC_CORRUPT_FRAME;
  *obu_size = (size_t)u_obu_size;
  return AOM_CODEC_OK;
}

// Parses OBU header and stores values in 'header'.
static aom_codec_err_t read_obu_header(struct aom_read_bit_buffer *rb,
                                       int is_annexb, ObuHeader *header) {
  if (!rb || !header) return AOM_CODEC_INVALID_PARAM;

  const ptrdiff_t bit_buffer_byte_length = rb->bit_buffer_end - rb->bit_buffer;
  if (bit_buffer_byte_length < 1) return AOM_CODEC_CORRUPT_FRAME;

  header->size = 1;

  if (aom_rb_read_bit(rb) != 0) {
    // Forbidden bit. Must not be set.
    return AOM_CODEC_CORRUPT_FRAME;
  }

  header->type = (OBU_TYPE)aom_rb_read_literal(rb, 4);
  header->has_extension = aom_rb_read_bit(rb);
  header->has_size_field = aom_rb_read_bit(rb);

  if (!header->has_size_field && !is_annexb) {
    // section 5 obu streams must have obu_size field set.
    return AOM_CODEC_UNSUP_BITSTREAM;
  }

  // obu_reserved_1bit must be set to 0. The value is ignored by a decoder.
  aom_rb_read_bit(rb);

  if (header->has_extension) {
    if (bit_buffer_byte_length == 1) return AOM_CODEC_CORRUPT_FRAME;

    header->size += 1;
    header->temporal_layer_id = aom_rb_read_literal(rb, 3);
    header->spatial_layer_id = aom_rb_read_literal(rb, 2);
    // extension_header_reserved_3bits must be set to 0. The value is ignored by
    // a decoder.
    aom_rb_read_literal(rb, 3);
  } else {
    header->temporal_layer_id = 0;
    header->spatial_layer_id = 0;
  }

  return AOM_CODEC_OK;
}

aom_codec_err_t aom_read_obu_header(uint8_t *buffer, size_t buffer_length,
                                    size_t *consumed, ObuHeader *header,
                                    int is_annexb) {
  if (buffer_length < 1 || !consumed || !header) return AOM_CODEC_INVALID_PARAM;

  // TODO(tomfinegan): Set the error handler here and throughout this file, and
  // confirm parsing work done via aom_read_bit_buffer is successful.
  struct aom_read_bit_buffer rb = { buffer, buffer + buffer_length, 0, NULL,
                                    NULL };
  aom_codec_err_t parse_result = read_obu_header(&rb, is_annexb, header);
  if (parse_result == AOM_CODEC_OK) *consumed = header->size;
  return parse_result;
}

aom_codec_err_t aom_read_obu_header_and_size(const uint8_t *data,
                                             size_t bytes_available,
                                             int is_annexb,
                                             ObuHeader *obu_header,
                                             size_t *const payload_size,
                                             size_t *const bytes_read) {
  size_t length_field_size_obu = 0;
  size_t length_field_size_payload = 0;
  size_t obu_size = 0;
  aom_codec_err_t status;

  if (is_annexb) {
    // Size field comes before the OBU header, and includes the OBU header
    status =
        read_obu_size(data, bytes_available, &obu_size, &length_field_size_obu);

    if (status != AOM_CODEC_OK) return status;
  }

  struct aom_read_bit_buffer rb = { data + length_field_size_obu,
                                    data + bytes_available, 0, NULL, NULL };

  status = read_obu_header(&rb, is_annexb, obu_header);
  if (status != AOM_CODEC_OK) return status;

  if (!obu_header->has_size_field) {
    assert(is_annexb);
    // Derive the payload size from the data we've already read
    if (obu_size < obu_header->size) return AOM_CODEC_CORRUPT_FRAME;

    *payload_size = obu_size - obu_header->size;
  } else {
    // Size field comes after the OBU header, and is just the payload size
    status = read_obu_size(
        data + length_field_size_obu + obu_header->size,
        bytes_available - length_field_size_obu - obu_header->size,
        payload_size, &length_field_size_payload);
    if (status != AOM_CODEC_OK) return status;
  }

  *bytes_read =
      length_field_size_obu + obu_header->size + length_field_size_payload;
  return AOM_CODEC_OK;
}
