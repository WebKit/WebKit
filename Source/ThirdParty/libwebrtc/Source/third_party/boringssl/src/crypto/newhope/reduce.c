/* Copyright (c) 2016, Google Inc.
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

#include "internal.h"


/* Incomplete-reduction routines; for details on allowed input ranges
 * and produced output ranges, see the description in the paper:
 * https://cryptojedi.org/papers/#newhope */

static const uint32_t kQInv = 12287; /* -inverse_mod(p,2^18) */
static const uint32_t kRLog = 18;

uint16_t newhope_montgomery_reduce(uint32_t a) {
  uint32_t u;

  u = (a * kQInv);
  u &= ((1 << kRLog) - 1);
  u *= PARAM_Q;
  a = a + u;
  return a >> 18;
}

uint16_t newhope_barrett_reduce(uint16_t a) {
  uint32_t u;

  u = ((uint32_t)a * 5) >> 16;
  u *= PARAM_Q;
  a -= u;
  return a;
}
