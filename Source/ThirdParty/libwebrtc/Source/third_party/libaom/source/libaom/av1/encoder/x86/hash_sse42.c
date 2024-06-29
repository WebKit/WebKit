/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <stdint.h>
#include <smmintrin.h>

#include "config/av1_rtcd.h"

// Byte-boundary alignment issues
#define ALIGN_SIZE 8
#define ALIGN_MASK (ALIGN_SIZE - 1)

#define CALC_CRC(op, crc, type, buf, len) \
  while ((len) >= sizeof(type)) {         \
    (crc) = op((crc), *(type *)(buf));    \
    (len) -= sizeof(type);                \
    buf += sizeof(type);                  \
  }

/**
 * Calculates 32-bit CRC for the input buffer
 * polynomial is 0x11EDC6F41
 * @return A 32-bit unsigned integer representing the CRC
 */
uint32_t av1_get_crc32c_value_sse4_2(void *crc_calculator, uint8_t *p,
                                     size_t len) {
  (void)crc_calculator;
  const uint8_t *buf = p;
  uint32_t crc = 0xFFFFFFFF;

  // Align the input to the word boundary
  for (; (len > 0) && ((intptr_t)buf & ALIGN_MASK); len--, buf++) {
    crc = _mm_crc32_u8(crc, *buf);
  }

#ifdef __x86_64__
  uint64_t crc64 = crc;
  CALC_CRC(_mm_crc32_u64, crc64, uint64_t, buf, len)
  crc = (uint32_t)crc64;
#endif
  CALC_CRC(_mm_crc32_u32, crc, uint32_t, buf, len)
  CALC_CRC(_mm_crc32_u16, crc, uint16_t, buf, len)
  CALC_CRC(_mm_crc32_u8, crc, uint8_t, buf, len)
  return (crc ^ 0xFFFFFFFF);
}
