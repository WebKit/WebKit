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

#include "av1/encoder/hash.h"

#include <stddef.h>

#include "config/av1_rtcd.h"

/* CRC-32C (iSCSI) polynomial in reversed bit order. */
#define POLY 0x82f63b78

/* Construct table for software CRC-32C calculation. */
void av1_crc32c_calculator_init(CRC32C *p_crc32c) {
  uint32_t crc;

  for (int n = 0; n < 256; n++) {
    crc = n;
    crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
    crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
    crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
    crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
    crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
    crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
    crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
    crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
    p_crc32c->table[0][n] = crc;
  }
  for (int n = 0; n < 256; n++) {
    crc = p_crc32c->table[0][n];
    for (int k = 1; k < 8; k++) {
      crc = p_crc32c->table[0][crc & 0xff] ^ (crc >> 8);
      p_crc32c->table[k][n] = crc;
    }
  }
}

/* Table-driven software version as a fall-back.  This is about 15 times slower
 than using the hardware instructions.  This assumes little-endian integers,
 as is the case on Intel processors that the assembler code here is for. */
uint32_t av1_get_crc32c_value_c(void *c, const uint8_t *buf, size_t len) {
  const uint8_t *next = buf;
  uint64_t crc;
  CRC32C *p = (CRC32C *)c;
  crc = 0 ^ 0xffffffff;
  while (len && ((uintptr_t)next & 7) != 0) {
    crc = p->table[0][(crc ^ *next++) & 0xff] ^ (crc >> 8);
    len--;
  }
  while (len >= 8) {
    crc ^= *(uint64_t *)next;
    crc = p->table[7][crc & 0xff] ^ p->table[6][(crc >> 8) & 0xff] ^
          p->table[5][(crc >> 16) & 0xff] ^ p->table[4][(crc >> 24) & 0xff] ^
          p->table[3][(crc >> 32) & 0xff] ^ p->table[2][(crc >> 40) & 0xff] ^
          p->table[1][(crc >> 48) & 0xff] ^ p->table[0][crc >> 56];
    next += 8;
    len -= 8;
  }
  while (len) {
    crc = p->table[0][(crc ^ *next++) & 0xff] ^ (crc >> 8);
    len--;
  }
  return (uint32_t)crc ^ 0xffffffff;
}
