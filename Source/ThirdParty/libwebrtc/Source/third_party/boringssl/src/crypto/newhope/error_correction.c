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

#include <string.h>

#include <openssl/base.h>
#include <openssl/rand.h>

#include "internal.h"


/* See paper for details on the error reconciliation */

static int32_t abs_32(int32_t v) {
  int32_t mask = v >> 31;
  return (v ^ mask) - mask;
}

static int32_t f(int32_t* v0, int32_t* v1, int32_t x) {
  int32_t xit, t, r, b;

  /* Next 6 lines compute t = x/PARAM_Q */
  b = x * 2730;
  t = b >> 25;
  b = x - t * 12289;
  b = 12288 - b;
  b >>= 31;
  t -= b;

  r = t & 1;
  xit = (t >> 1);
  *v0 = xit + r; /* v0 = round(x/(2*PARAM_Q)) */

  t -= 1;
  r = t & 1;
  *v1 = (t >> 1) + r;

  return abs_32(x - ((*v0) * 2 * PARAM_Q));
}

static int32_t g(int32_t x) {
  int32_t t, c, b;

  /* Next 6 lines compute t = x/(4*PARAM_Q); */
  b = x * 2730;
  t = b >> 27;
  b = x - t * 49156;
  b = 49155 - b;
  b >>= 31;
  t -= b;

  c = t & 1;
  t = (t >> 1) + c; /* t = round(x/(8*PARAM_Q)) */

  t *= 8 * PARAM_Q;

  return abs_32(t - x);
}

static int16_t LDDecode(int32_t xi0, int32_t xi1, int32_t xi2, int32_t xi3) {
  int32_t t;

  t = g(xi0);
  t += g(xi1);
  t += g(xi2);
  t += g(xi3);

  t -= 8 * PARAM_Q;
  t >>= 31;
  return t & 1;
}

void newhope_helprec(NEWHOPE_POLY* c, const NEWHOPE_POLY* v,
                     const uint8_t rand[32]) {
  int32_t v0[4], v1[4], v_tmp[4], k;
  uint8_t rbit;
  unsigned i;

  for (i = 0; i < 256; i++) {
    rbit = (rand[i >> 3] >> (i & 7)) & 1;

    k = f(v0 + 0, v1 + 0, 8 * v->coeffs[0 + i] + 4 * rbit);
    k += f(v0 + 1, v1 + 1, 8 * v->coeffs[256 + i] + 4 * rbit);
    k += f(v0 + 2, v1 + 2, 8 * v->coeffs[512 + i] + 4 * rbit);
    k += f(v0 + 3, v1 + 3, 8 * v->coeffs[768 + i] + 4 * rbit);

    k = (2 * PARAM_Q - 1 - k) >> 31;

    v_tmp[0] = ((~k) & v0[0]) ^ (k & v1[0]);
    v_tmp[1] = ((~k) & v0[1]) ^ (k & v1[1]);
    v_tmp[2] = ((~k) & v0[2]) ^ (k & v1[2]);
    v_tmp[3] = ((~k) & v0[3]) ^ (k & v1[3]);

    c->coeffs[0 + i] = (v_tmp[0] - v_tmp[3]) & 3;
    c->coeffs[256 + i] = (v_tmp[1] - v_tmp[3]) & 3;
    c->coeffs[512 + i] = (v_tmp[2] - v_tmp[3]) & 3;
    c->coeffs[768 + i] = (-k + 2 * v_tmp[3]) & 3;
  }
}

void newhope_reconcile(uint8_t* key, const NEWHOPE_POLY* v,
                       const NEWHOPE_POLY* c) {
  int i;
  int32_t tmp[4];

  memset(key, 0, NEWHOPE_KEY_LENGTH);

  for (i = 0; i < 256; i++) {
    tmp[0] = 16 * PARAM_Q + 8 * (int32_t)v->coeffs[0 + i] -
             PARAM_Q * (2 * c->coeffs[0 + i] + c->coeffs[768 + i]);
    tmp[1] = 16 * PARAM_Q + 8 * (int32_t)v->coeffs[256 + i] -
             PARAM_Q * (2 * c->coeffs[256 + i] + c->coeffs[768 + i]);
    tmp[2] = 16 * PARAM_Q + 8 * (int32_t)v->coeffs[512 + i] -
             PARAM_Q * (2 * c->coeffs[512 + i] + c->coeffs[768 + i]);
    tmp[3] = 16 * PARAM_Q + 8 * (int32_t)v->coeffs[768 + i] -
             PARAM_Q * (c->coeffs[768 + i]);

    key[i >> 3] |= LDDecode(tmp[0], tmp[1], tmp[2], tmp[3]) << (i & 7);
  }
}
