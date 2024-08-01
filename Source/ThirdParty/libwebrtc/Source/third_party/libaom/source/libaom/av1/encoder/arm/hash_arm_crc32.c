/*
 * Copyright (c) 2022, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#else
#include <arm_acle.h>
#endif

#include <stddef.h>
#include <stdint.h>

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#define CRC_LOOP(op, crc, type, buf, len) \
  while ((len) >= sizeof(type)) {         \
    (crc) = op((crc), *(type *)(buf));    \
    (len) -= sizeof(type);                \
    buf += sizeof(type);                  \
  }

#define CRC_SINGLE(op, crc, type, buf, len) \
  if ((len) >= sizeof(type)) {              \
    (crc) = op((crc), *(type *)(buf));      \
    (len) -= sizeof(type);                  \
    buf += sizeof(type);                    \
  }

/* Return 32-bit CRC for the input buffer.
 * Polynomial is 0x1EDC6F41.
 */

uint32_t av1_get_crc32c_value_arm_crc32(void *crc_calculator, uint8_t *p,
                                        size_t len) {
  (void)crc_calculator;
  const uint8_t *buf = p;
  uint32_t crc = 0xFFFFFFFF;

#if !AOM_ARCH_AARCH64
  // Align input to 8-byte boundary (only necessary for 32-bit builds.)
  while (len && ((uintptr_t)buf & 7)) {
    crc = __crc32cb(crc, *buf++);
    len--;
  }
#endif

  CRC_LOOP(__crc32cd, crc, uint64_t, buf, len)
  CRC_SINGLE(__crc32cw, crc, uint32_t, buf, len)
  CRC_SINGLE(__crc32ch, crc, uint16_t, buf, len)
  CRC_SINGLE(__crc32cb, crc, uint8_t, buf, len)

  return ~crc;
}
