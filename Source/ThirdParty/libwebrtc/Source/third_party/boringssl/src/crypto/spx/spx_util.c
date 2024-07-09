/* Copyright (c) 2023, Google LLC
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#include <openssl/base.h>

#include <assert.h>

#include "./spx_util.h"

void spx_uint64_to_len_bytes(uint8_t *output, size_t out_len, uint64_t input) {
  for (size_t i = out_len; i > 0; --i) {
    output[i - 1] = input & 0xff;
    input = input >> 8;
  }
}

uint64_t spx_to_uint64(const uint8_t *input, size_t input_len) {
  uint64_t tmp = 0;
  for (size_t i = 0; i < input_len; ++i) {
    tmp = 256 * tmp + input[i];
  }
  return tmp;
}

void spx_base_b(uint32_t *output, size_t out_len, const uint8_t *input,
                unsigned int log2_b) {
  int in = 0;
  uint32_t out = 0;
  uint32_t bits = 0;
  uint32_t total = 0;
  uint32_t base = UINT32_C(1) << log2_b;

  for (out = 0; out < out_len; ++out) {
    while (bits < log2_b) {
      total = (total << 8) + input[in];
      in++;
      bits = bits + 8;
    }
    bits -= log2_b;
    output[out] = (total >> bits) % base;
  }
}
