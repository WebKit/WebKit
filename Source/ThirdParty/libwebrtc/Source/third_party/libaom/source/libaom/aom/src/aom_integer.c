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

#include "aom/aom_integer.h"

static const size_t kMaximumLeb128Size = 8;
static const uint8_t kLeb128ByteMask = 0x7f;  // Binary: 01111111

// Disallow values larger than 32-bits to ensure consistent behavior on 32 and
// 64 bit targets: value is typically used to determine buffer allocation size
// when decoded.
static const uint64_t kMaximumLeb128Value = UINT32_MAX;

size_t aom_uleb_size_in_bytes(uint64_t value) {
  size_t size = 0;
  do {
    ++size;
  } while ((value >>= 7) != 0);
  return size;
}

int aom_uleb_decode(const uint8_t *buffer, size_t available, uint64_t *value,
                    size_t *length) {
  if (buffer && value) {
    *value = 0;
    for (size_t i = 0; i < kMaximumLeb128Size && i < available; ++i) {
      const uint8_t decoded_byte = *(buffer + i) & kLeb128ByteMask;
      *value |= ((uint64_t)decoded_byte) << (i * 7);
      if ((*(buffer + i) >> 7) == 0) {
        if (length) {
          *length = i + 1;
        }

        // Fail on values larger than 32-bits to ensure consistent behavior on
        // 32 and 64 bit targets: value is typically used to determine buffer
        // allocation size.
        if (*value > UINT32_MAX) return -1;

        return 0;
      }
    }
  }

  // If we get here, either the buffer/value pointers were invalid,
  // or we ran over the available space
  return -1;
}

int aom_uleb_encode(uint64_t value, size_t available, uint8_t *coded_value,
                    size_t *coded_size) {
  const size_t leb_size = aom_uleb_size_in_bytes(value);
  if (value > kMaximumLeb128Value || leb_size > kMaximumLeb128Size ||
      leb_size > available || !coded_value || !coded_size) {
    return -1;
  }

  for (size_t i = 0; i < leb_size; ++i) {
    uint8_t byte = value & 0x7f;
    value >>= 7;

    if (value != 0) byte |= 0x80;  // Signal that more bytes follow.

    *(coded_value + i) = byte;
  }

  *coded_size = leb_size;
  return 0;
}

int aom_uleb_encode_fixed_size(uint64_t value, size_t available,
                               size_t pad_to_size, uint8_t *coded_value,
                               size_t *coded_size) {
  if (value > kMaximumLeb128Value || !coded_value || !coded_size ||
      available < pad_to_size || pad_to_size > kMaximumLeb128Size) {
    return -1;
  }
  const uint64_t limit = 1ULL << (7 * pad_to_size);
  if (value >= limit) {
    // Can't encode 'value' within 'pad_to_size' bytes
    return -1;
  }

  for (size_t i = 0; i < pad_to_size; ++i) {
    uint8_t byte = value & 0x7f;
    value >>= 7;

    if (i < pad_to_size - 1) byte |= 0x80;  // Signal that more bytes follow.

    *(coded_value + i) = byte;
  }

  assert(value == 0);

  *coded_size = pad_to_size;
  return 0;
}
