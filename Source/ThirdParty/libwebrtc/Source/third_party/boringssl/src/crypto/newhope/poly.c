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

#include <assert.h>
#include <string.h>

#include <openssl/aes.h>
#include <openssl/rand.h>

#include "internal.h"


extern uint16_t newhope_omegas_montgomery[];
extern uint16_t newhope_omegas_inv_montgomery[];

extern uint16_t newhope_psis_bitrev_montgomery[];
extern uint16_t newhope_psis_inv_montgomery[];

void NEWHOPE_POLY_frombytes(NEWHOPE_POLY* r, const uint8_t* a) {
  for (int i = 0; i < PARAM_N / 4; i++) {
    r->coeffs[4 * i + 0] =
        a[7 * i + 0] | (((uint16_t)a[7 * i + 1] & 0x3f) << 8);
    r->coeffs[4 * i + 1] = (a[7 * i + 1] >> 6) |
                           (((uint16_t)a[7 * i + 2]) << 2) |
                           (((uint16_t)a[7 * i + 3] & 0x0f) << 10);
    r->coeffs[4 * i + 2] = (a[7 * i + 3] >> 4) |
                           (((uint16_t)a[7 * i + 4]) << 4) |
                           (((uint16_t)a[7 * i + 5] & 0x03) << 12);
    r->coeffs[4 * i + 3] =
        (a[7 * i + 5] >> 2) | (((uint16_t)a[7 * i + 6]) << 6);
  }
}

void NEWHOPE_POLY_tobytes(uint8_t* r, const NEWHOPE_POLY* p) {
  uint16_t t0, t1, t2, t3, m;
  int16_t c;
  for (int i = 0; i < PARAM_N / 4; i++) {
    t0 = newhope_barrett_reduce(
        p->coeffs[4 * i + 0]); /* Make sure that coefficients
                          have only 14 bits */
    t1 = newhope_barrett_reduce(p->coeffs[4 * i + 1]);
    t2 = newhope_barrett_reduce(p->coeffs[4 * i + 2]);
    t3 = newhope_barrett_reduce(p->coeffs[4 * i + 3]);

    m = t0 - PARAM_Q;
    c = m;
    c >>= 15;
    t0 = m ^ ((t0 ^ m) & c); /* Make sure that coefficients are in [0,q] */

    m = t1 - PARAM_Q;
    c = m;
    c >>= 15;
    t1 = m ^ ((t1 ^ m) & c); /* <Make sure that coefficients are in [0,q] */

    m = t2 - PARAM_Q;
    c = m;
    c >>= 15;
    t2 = m ^ ((t2 ^ m) & c); /* <Make sure that coefficients are in [0,q] */

    m = t3 - PARAM_Q;
    c = m;
    c >>= 15;
    t3 = m ^ ((t3 ^ m) & c); /* Make sure that coefficients are in [0,q] */

    r[7 * i + 0] = t0 & 0xff;
    r[7 * i + 1] = (t0 >> 8) | (t1 << 6);
    r[7 * i + 2] = (t1 >> 2);
    r[7 * i + 3] = (t1 >> 10) | (t2 << 4);
    r[7 * i + 4] = (t2 >> 4);
    r[7 * i + 5] = (t2 >> 12) | (t3 << 2);
    r[7 * i + 6] = (t3 >> 6);
  }
}

void newhope_poly_uniform(NEWHOPE_POLY* a, const uint8_t* seed) {
/* The reference implementation uses SHAKE-128 here; this implementation uses
 * AES-CTR. Use half the seed for the initialization vector and half for the
 * key. */
#if SEED_LENGTH != 2 * AES_BLOCK_SIZE
#error "2 * seed length != AES_BLOCK_SIZE"
#endif
  uint8_t ivec[AES_BLOCK_SIZE];
  memcpy(ivec, &seed[SEED_LENGTH / 2], SEED_LENGTH / 2);
  AES_KEY key;
  AES_set_encrypt_key(seed, 8 * SEED_LENGTH / 2, &key);

  /* AES state. */
  uint8_t ecount[AES_BLOCK_SIZE];
  memset(ecount, 0, AES_BLOCK_SIZE);

  /* Encrypt a block of zeros just to get the random bytes. With luck, 2688
   * bytes is enough. */
  uint8_t buf[AES_BLOCK_SIZE * 168];
  memset(buf, 0, sizeof(buf));

  unsigned int block_num = 0;
  AES_ctr128_encrypt(buf, buf, sizeof(buf), &key, ivec, ecount, &block_num);

  size_t pos = 0, coeff_num = 0;
  while (coeff_num < PARAM_N) {
    /* Specialized for q = 12889 */
    uint16_t val = (buf[pos] | ((uint16_t)buf[pos + 1] << 8)) & 0x3fff;
    if (val < PARAM_Q) {
      a->coeffs[coeff_num++] = val;
    }

    pos += 2;
    if (pos > sizeof(buf) - 2) {
      memset(buf, 0, sizeof(buf));
      AES_ctr128_encrypt(buf, buf, sizeof(buf), &key, ivec, ecount, &block_num);
      pos = 0;
    }
  }
}

void NEWHOPE_POLY_noise(NEWHOPE_POLY* r) {
#if PARAM_K != 16
#error "poly_getnoise in poly.c only supports k=16"
#endif

  uint32_t tp[PARAM_N];

  /* The reference implementation calls ChaCha20 here. */
  RAND_bytes((uint8_t *) tp, sizeof(tp));

  for (size_t i = 0; i < PARAM_N; i++) {
    const uint32_t t = tp[i];

    uint32_t d = 0;
    for (size_t j = 0; j < 8; j++) {
      d += (t >> j) & 0x01010101;
    }

    const uint32_t a = ((d >> 8) & 0xff) + (d & 0xff);
    const uint32_t b = (d >> 24) + ((d >> 16) & 0xff);
    r->coeffs[i] = a + PARAM_Q - b;
  }
}

void newhope_poly_pointwise(NEWHOPE_POLY* r, const NEWHOPE_POLY* a,
                            const NEWHOPE_POLY* b) {
  for (size_t i = 0; i < PARAM_N; i++) {
    uint16_t t = newhope_montgomery_reduce(3186 * b->coeffs[i]);
    /* t is now in Montgomery domain */
    r->coeffs[i] = newhope_montgomery_reduce(a->coeffs[i] * t);
    /* r->coeffs[i] is back in normal domain */
  }
}

void newhope_poly_add(NEWHOPE_POLY* r, const NEWHOPE_POLY* a,
                      const NEWHOPE_POLY* b) {
  for (size_t i = 0; i < PARAM_N; i++) {
    r->coeffs[i] = newhope_barrett_reduce(a->coeffs[i] + b->coeffs[i]);
  }
}

void NEWHOPE_POLY_noise_ntt(NEWHOPE_POLY* r) {
  NEWHOPE_POLY_noise(r);
  /* Forward NTT transformation.  Because we're operating on a noise polynomial,
   * we can regard the bits as already reversed and skip the bit-reversal
   * step:
   *
   * newhope_bitrev_vector(r->coeffs); */
  newhope_mul_coefficients(r->coeffs, newhope_psis_bitrev_montgomery);
  newhope_ntt((uint16_t *) r->coeffs, newhope_omegas_montgomery);
}

void newhope_poly_invntt(NEWHOPE_POLY* r) {
  newhope_bitrev_vector(r->coeffs);
  newhope_ntt((uint16_t *) r->coeffs, newhope_omegas_inv_montgomery);
  newhope_mul_coefficients(r->coeffs, newhope_psis_inv_montgomery);
}
