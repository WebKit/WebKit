/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "common/obudec.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_ports/mem_ops.h"
#include "av1/common/common.h"
#include "av1/common/obu_util.h"
#include "tools_common.h"

#define OBU_BUFFER_SIZE (500 * 1024)

#define OBU_HEADER_SIZE 1
#define OBU_EXTENSION_SIZE 1
#define OBU_MAX_LENGTH_FIELD_SIZE 8

#define OBU_MAX_HEADER_SIZE \
  (OBU_HEADER_SIZE + OBU_EXTENSION_SIZE + 2 * OBU_MAX_LENGTH_FIELD_SIZE)

#define OBU_DETECTION_SIZE \
  (OBU_HEADER_SIZE + OBU_EXTENSION_SIZE + 4 * OBU_MAX_LENGTH_FIELD_SIZE)

// Reads unsigned LEB128 integer and returns 0 upon successful read and decode.
// Stores raw bytes in 'value_buffer', length of the number in 'value_length',
// and decoded value in 'value'. If 'buffered' is true, it is buffered in the
// detect buffer first.
static int obudec_read_leb128(struct AvxInputContext *input_ctx,
                              uint8_t *value_buffer, size_t *value_length,
                              uint64_t *value, bool buffered) {
  if (!input_ctx || !value_buffer || !value_length || !value) return -1;
  size_t len;
  for (len = 0; len < OBU_MAX_LENGTH_FIELD_SIZE; ++len) {
    const size_t num_read =
        buffer_input(input_ctx, 1, &value_buffer[len], buffered);
    if (num_read == 0) {
      if (len == 0 && input_eof(input_ctx)) {
        *value_length = 0;
        return 0;
      }
      // Ran out of data before completing read of value.
      return -1;
    }
    if ((value_buffer[len] >> 7) == 0) {
      ++len;
      *value_length = len;
      break;
    }
  }

  return aom_uleb_decode(value_buffer, len, value, NULL);
}

// Reads OBU header from 'input_ctx'. The 'buffer_capacity' passed in must be
// large enough to store an OBU header with extension (2 bytes). Raw OBU data is
// written to 'obu_data', parsed OBU header values are written to 'obu_header',
// and total bytes read from file are written to 'bytes_read'. Returns 0 for
// success, and non-zero on failure. When end of file is reached, the return
// value is 0 and the 'bytes_read' value is set to 0. If 'buffered' is true, it
// is buffered in the detect buffer first.
static int obudec_read_obu_header(struct AvxInputContext *input_ctx,
                                  size_t buffer_capacity, int is_annexb,
                                  uint8_t *obu_data, ObuHeader *obu_header,
                                  size_t *bytes_read, bool buffered) {
  if (!input_ctx || buffer_capacity < (OBU_HEADER_SIZE + OBU_EXTENSION_SIZE) ||
      !obu_data || !obu_header || !bytes_read) {
    return -1;
  }
  *bytes_read = buffer_input(input_ctx, 1, obu_data, buffered);

  if (input_eof(input_ctx) && *bytes_read == 0) {
    return 0;
  } else if (*bytes_read != 1) {
    fprintf(stderr, "obudec: Failure reading OBU header.\n");
    return -1;
  }

  const int has_extension = (obu_data[0] >> 2) & 0x1;
  if (has_extension) {
    if (buffer_input(input_ctx, 1, &obu_data[1], buffered) != 1) {
      fprintf(stderr, "obudec: Failure reading OBU extension.");
      return -1;
    }
    ++*bytes_read;
  }

  size_t obu_bytes_parsed = 0;
  const aom_codec_err_t parse_result = aom_read_obu_header(
      obu_data, *bytes_read, &obu_bytes_parsed, obu_header, is_annexb);
  if (parse_result != AOM_CODEC_OK || *bytes_read != obu_bytes_parsed) {
    fprintf(stderr, "obudec: Error parsing OBU header.\n");
    return -1;
  }

  return 0;
}

// Reads OBU payload from 'input_ctx' and returns 0 for success when all payload
// bytes are read from the file. Payload data is written to 'obu_data', and
// actual bytes read added to 'bytes_read'. If 'buffered' is true, it is
// buffered in the detect buffer first.
static int obudec_read_obu_payload(struct AvxInputContext *input_ctx,
                                   size_t payload_length, uint8_t *obu_data,
                                   size_t *bytes_read, bool buffered) {
  if (!input_ctx || payload_length == 0 || !obu_data || !bytes_read) return -1;

  if (buffer_input(input_ctx, payload_length, obu_data, buffered) !=
      payload_length) {
    fprintf(stderr, "obudec: Failure reading OBU payload.\n");
    return -1;
  }

  *bytes_read += payload_length;
  return 0;
}

static int obudec_read_obu_header_and_size(
    struct AvxInputContext *input_ctx, size_t buffer_capacity, int is_annexb,
    uint8_t *buffer, size_t *bytes_read, size_t *payload_length,
    ObuHeader *obu_header, bool buffered) {
  const size_t kMinimumBufferSize = OBU_MAX_HEADER_SIZE;
  if (!input_ctx || !buffer || !bytes_read || !payload_length || !obu_header ||
      buffer_capacity < kMinimumBufferSize) {
    return -1;
  }

  size_t leb128_length_obu = 0;
  size_t leb128_length_payload = 0;
  uint64_t obu_size = 0;
  if (is_annexb) {
    if (obudec_read_leb128(input_ctx, &buffer[0], &leb128_length_obu, &obu_size,
                           buffered) != 0) {
      fprintf(stderr, "obudec: Failure reading OBU size length.\n");
      return -1;
    } else if (leb128_length_obu == 0) {
      *payload_length = 0;
      return 0;
    }
    if (obu_size > UINT32_MAX) {
      fprintf(stderr, "obudec: OBU payload length too large.\n");
      return -1;
    }
  }

  size_t header_size = 0;
  if (obudec_read_obu_header(input_ctx, buffer_capacity - leb128_length_obu,
                             is_annexb, buffer + leb128_length_obu, obu_header,
                             &header_size, buffered) != 0) {
    return -1;
  } else if (header_size == 0) {
    *payload_length = 0;
    return 0;
  }

  if (!obu_header->has_size_field) {
    assert(is_annexb);
    if (obu_size < header_size) {
      fprintf(stderr, "obudec: OBU size is too small.\n");
      return -1;
    }
    *payload_length = (size_t)obu_size - header_size;
  } else {
    uint64_t u64_payload_length = 0;
    if (obudec_read_leb128(input_ctx, &buffer[leb128_length_obu + header_size],
                           &leb128_length_payload, &u64_payload_length,
                           buffered) != 0) {
      fprintf(stderr, "obudec: Failure reading OBU payload length.\n");
      return -1;
    }
    if (u64_payload_length > UINT32_MAX) {
      fprintf(stderr, "obudec: OBU payload length too large.\n");
      return -1;
    }

    *payload_length = (size_t)u64_payload_length;
  }

  *bytes_read = leb128_length_obu + header_size + leb128_length_payload;
  return 0;
}

static int obudec_grow_buffer(size_t growth_amount, uint8_t **obu_buffer,
                              size_t *obu_buffer_capacity) {
  if (!*obu_buffer || !obu_buffer_capacity || growth_amount == 0) {
    return -1;
  }

  const size_t capacity = *obu_buffer_capacity;
  if (SIZE_MAX - growth_amount < capacity) {
    fprintf(stderr, "obudec: cannot grow buffer, capacity will roll over.\n");
    return -1;
  }

  const size_t new_capacity = capacity + growth_amount;

#if defined AOM_MAX_ALLOCABLE_MEMORY
  if (new_capacity > AOM_MAX_ALLOCABLE_MEMORY) {
    fprintf(stderr, "obudec: OBU size exceeds max alloc size.\n");
    return -1;
  }
#endif

  uint8_t *new_buffer = (uint8_t *)realloc(*obu_buffer, new_capacity);
  if (!new_buffer) {
    fprintf(stderr, "obudec: Failed to allocate compressed data buffer.\n");
    return -1;
  }

  *obu_buffer = new_buffer;
  *obu_buffer_capacity = new_capacity;
  return 0;
}

static int obudec_read_one_obu(struct AvxInputContext *input_ctx,
                               uint8_t **obu_buffer, size_t obu_bytes_buffered,
                               size_t *obu_buffer_capacity, size_t *obu_length,
                               ObuHeader *obu_header, int is_annexb,
                               bool buffered) {
  if (!input_ctx || !(*obu_buffer) || !obu_buffer_capacity || !obu_length ||
      !obu_header) {
    return -1;
  }

  size_t bytes_read = 0;
  size_t obu_payload_length = 0;
  size_t available_buffer_capacity = *obu_buffer_capacity - obu_bytes_buffered;

  if (available_buffer_capacity < OBU_MAX_HEADER_SIZE) {
    if (obudec_grow_buffer(AOMMAX(*obu_buffer_capacity, OBU_MAX_HEADER_SIZE),
                           obu_buffer, obu_buffer_capacity) != 0) {
      *obu_length = bytes_read;
      return -1;
    }
    available_buffer_capacity +=
        AOMMAX(*obu_buffer_capacity, OBU_MAX_HEADER_SIZE);
  }

  const int status = obudec_read_obu_header_and_size(
      input_ctx, available_buffer_capacity, is_annexb,
      *obu_buffer + obu_bytes_buffered, &bytes_read, &obu_payload_length,
      obu_header, buffered);
  if (status < 0) return status;

  if (obu_payload_length > SIZE_MAX - bytes_read) return -1;

  if (obu_payload_length > 256 * 1024 * 1024) {
    fprintf(stderr, "obudec: Read invalid OBU size (%u)\n",
            (unsigned int)obu_payload_length);
    *obu_length = bytes_read + obu_payload_length;
    return -1;
  }

  if (bytes_read + obu_payload_length > available_buffer_capacity &&
      obudec_grow_buffer(AOMMAX(*obu_buffer_capacity, obu_payload_length),
                         obu_buffer, obu_buffer_capacity) != 0) {
    *obu_length = bytes_read + obu_payload_length;
    return -1;
  }

  if (obu_payload_length > 0 &&
      obudec_read_obu_payload(input_ctx, obu_payload_length,
                              *obu_buffer + obu_bytes_buffered + bytes_read,
                              &bytes_read, buffered) != 0) {
    return -1;
  }

  *obu_length = bytes_read;
  return 0;
}

int file_is_obu(struct ObuDecInputContext *obu_ctx) {
  if (!obu_ctx || !obu_ctx->avx_ctx) return 0;

  struct AvxInputContext *avx_ctx = obu_ctx->avx_ctx;
  uint8_t detect_buf[OBU_DETECTION_SIZE] = { 0 };
  const int is_annexb = obu_ctx->is_annexb;
  size_t payload_length = 0;
  ObuHeader obu_header;
  memset(&obu_header, 0, sizeof(obu_header));
  size_t length_of_unit_size = 0;
  size_t annexb_header_length = 0;
  uint64_t unit_size = 0;

  if (is_annexb) {
    // read the size of first temporal unit
    if (obudec_read_leb128(avx_ctx, &detect_buf[0], &length_of_unit_size,
                           &unit_size, /*buffered=*/true) != 0) {
      fprintf(stderr, "obudec: Failure reading temporal unit header\n");
      rewind_detect(avx_ctx);
      return 0;
    }

    // read the size of first frame unit
    if (obudec_read_leb128(avx_ctx, &detect_buf[length_of_unit_size],
                           &annexb_header_length, &unit_size,
                           /*buffered=*/true) != 0) {
      fprintf(stderr, "obudec: Failure reading frame unit header\n");
      rewind_detect(avx_ctx);
      return 0;
    }
    annexb_header_length += length_of_unit_size;
  }

  size_t bytes_read = 0;
  if (obudec_read_obu_header_and_size(
          avx_ctx, OBU_DETECTION_SIZE - annexb_header_length, is_annexb,
          &detect_buf[annexb_header_length], &bytes_read, &payload_length,
          &obu_header, /*buffered=*/true) != 0) {
    fprintf(stderr, "obudec: Failure reading first OBU.\n");
    rewind_detect(avx_ctx);
    return 0;
  }

  if (is_annexb) {
    bytes_read += annexb_header_length;
  }

  if (obu_header.type != OBU_TEMPORAL_DELIMITER &&
      obu_header.type != OBU_SEQUENCE_HEADER) {
    rewind_detect(avx_ctx);
    return 0;
  }

  if (obu_header.has_size_field) {
    if (obu_header.type == OBU_TEMPORAL_DELIMITER && payload_length != 0) {
      fprintf(
          stderr,
          "obudec: Invalid OBU_TEMPORAL_DELIMITER payload length (non-zero).");
      rewind_detect(avx_ctx);
      return 0;
    }
  } else if (!is_annexb) {
    fprintf(stderr, "obudec: OBU size fields required, cannot decode input.\n");
    rewind_detect(avx_ctx);
    return 0;
  }

  // Appears that input is valid Section 5 AV1 stream.
  obu_ctx->buffer = (uint8_t *)malloc(OBU_BUFFER_SIZE);
  if (!obu_ctx->buffer) {
    fprintf(stderr, "Out of memory.\n");
    rewind_detect(avx_ctx);
    return 0;
  }
  obu_ctx->buffer_capacity = OBU_BUFFER_SIZE;

  memcpy(obu_ctx->buffer, &detect_buf[0], bytes_read);
  obu_ctx->bytes_buffered = bytes_read;
  // If the first OBU is a SEQUENCE_HEADER, then it will have a payload.
  // We need to read this in so that our buffer only contains complete OBUs.
  if (payload_length > 0) {
    if (payload_length > (obu_ctx->buffer_capacity - bytes_read)) {
      fprintf(stderr, "obudec: First OBU's payload is too large\n");
      rewind_detect(avx_ctx);
      obudec_free(obu_ctx);
      return 0;
    }

    size_t payload_bytes = 0;
    const int status = obudec_read_obu_payload(
        avx_ctx, payload_length, &obu_ctx->buffer[bytes_read], &payload_bytes,
        /*buffered=*/false);
    if (status < 0) {
      rewind_detect(avx_ctx);
      obudec_free(obu_ctx);
      return 0;
    }
    obu_ctx->bytes_buffered += payload_bytes;
  }
  return 1;
}

int obudec_read_temporal_unit(struct ObuDecInputContext *obu_ctx,
                              uint8_t **buffer, size_t *bytes_read,
                              size_t *buffer_size) {
  FILE *f = obu_ctx->avx_ctx->file;
  if (!f) return -1;

  *buffer_size = 0;
  *bytes_read = 0;

  if (input_eof(obu_ctx->avx_ctx)) {
    return 1;
  }

  size_t tu_size;
  size_t obu_size = 0;
  size_t length_of_temporal_unit_size = 0;
  uint8_t tuheader[OBU_MAX_LENGTH_FIELD_SIZE] = { 0 };

  if (obu_ctx->is_annexb) {
    uint64_t size = 0;

    if (obu_ctx->bytes_buffered == 0) {
      if (obudec_read_leb128(obu_ctx->avx_ctx, &tuheader[0],
                             &length_of_temporal_unit_size, &size,
                             /*buffered=*/false) != 0) {
        fprintf(stderr, "obudec: Failure reading temporal unit header\n");
        return -1;
      }
      if (size == 0 && input_eof(obu_ctx->avx_ctx)) {
        return 1;
      }
    } else {
      // temporal unit size was already stored in buffer
      if (aom_uleb_decode(obu_ctx->buffer, obu_ctx->bytes_buffered, &size,
                          &length_of_temporal_unit_size) != 0) {
        fprintf(stderr, "obudec: Failure reading temporal unit header\n");
        return -1;
      }
    }

    if (size > UINT32_MAX || size + length_of_temporal_unit_size > UINT32_MAX) {
      fprintf(stderr, "obudec: TU too large.\n");
      return -1;
    }

    size += length_of_temporal_unit_size;
    tu_size = (size_t)size;
  } else {
    while (1) {
      ObuHeader obu_header;
      memset(&obu_header, 0, sizeof(obu_header));

      if (obudec_read_one_obu(obu_ctx->avx_ctx, &obu_ctx->buffer,
                              obu_ctx->bytes_buffered,
                              &obu_ctx->buffer_capacity, &obu_size, &obu_header,
                              0, /*buffered=*/false) != 0) {
        fprintf(stderr, "obudec: read_one_obu failed in TU loop\n");
        return -1;
      }

      if (obu_header.type == OBU_TEMPORAL_DELIMITER || obu_size == 0) {
        tu_size = obu_ctx->bytes_buffered;
        break;
      } else {
        obu_ctx->bytes_buffered += obu_size;
      }
    }
  }

#if defined AOM_MAX_ALLOCABLE_MEMORY
  if (tu_size > AOM_MAX_ALLOCABLE_MEMORY) {
    fprintf(stderr, "obudec: Temporal Unit size exceeds max alloc size.\n");
    return -1;
  }
#endif
  if (tu_size > 0) {
    uint8_t *new_buffer = (uint8_t *)realloc(*buffer, tu_size);
    if (!new_buffer) {
      free(*buffer);
      fprintf(stderr, "obudec: Out of memory.\n");
      return -1;
    }
    *buffer = new_buffer;
  }
  *bytes_read = tu_size;
  *buffer_size = tu_size;

  if (!obu_ctx->is_annexb) {
    memcpy(*buffer, obu_ctx->buffer, tu_size);

    // At this point, (obu_ctx->buffer + obu_ctx->bytes_buffered + obu_size)
    // points to the end of the buffer.
    memmove(obu_ctx->buffer, obu_ctx->buffer + obu_ctx->bytes_buffered,
            obu_size);
    obu_ctx->bytes_buffered = obu_size;
  } else {
    if (!input_eof(obu_ctx->avx_ctx)) {
      size_t data_size;
      size_t offset;
      if (!obu_ctx->bytes_buffered) {
        data_size = tu_size - length_of_temporal_unit_size;
        memcpy(*buffer, &tuheader[0], length_of_temporal_unit_size);
        offset = length_of_temporal_unit_size;
      } else {
        const size_t copy_size = AOMMIN(obu_ctx->bytes_buffered, tu_size);
        memcpy(*buffer, obu_ctx->buffer, copy_size);
        offset = copy_size;
        data_size = tu_size - copy_size;
        obu_ctx->bytes_buffered -= copy_size;
      }

      if (read_from_input(obu_ctx->avx_ctx, data_size, *buffer + offset) !=
          data_size) {
        fprintf(stderr, "obudec: Failed to read full temporal unit\n");
        return -1;
      }
    }
  }
  return 0;
}

void obudec_free(struct ObuDecInputContext *obu_ctx) {
  free(obu_ctx->buffer);
  obu_ctx->buffer = NULL;
  obu_ctx->buffer_capacity = 0;
  obu_ctx->bytes_buffered = 0;
}
