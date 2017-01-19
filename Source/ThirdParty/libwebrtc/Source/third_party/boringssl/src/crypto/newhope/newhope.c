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

#include <openssl/mem.h>
#include <openssl/rand.h>

#include "internal.h"


NEWHOPE_POLY *NEWHOPE_POLY_new(void) {
  return (NEWHOPE_POLY *)OPENSSL_malloc(sizeof(NEWHOPE_POLY));
}

void NEWHOPE_POLY_free(NEWHOPE_POLY *p) { OPENSSL_free(p); }

/* Encodes reconciliation data from |c| into |r|. */
static void encode_rec(const NEWHOPE_POLY *c, uint8_t *r) {
  int i;
  for (i = 0; i < PARAM_N / 4; i++) {
    r[i] = c->coeffs[4 * i] | (c->coeffs[4 * i + 1] << 2) |
           (c->coeffs[4 * i + 2] << 4) | (c->coeffs[4 * i + 3] << 6);
  }
}

/* Decodes reconciliation data from |r| into |c|. */
static void decode_rec(const uint8_t *r, NEWHOPE_POLY *c) {
  int i;
  for (i = 0; i < PARAM_N / 4; i++) {
    c->coeffs[4 * i + 0] = r[i] & 0x03;
    c->coeffs[4 * i + 1] = (r[i] >> 2) & 0x03;
    c->coeffs[4 * i + 2] = (r[i] >> 4) & 0x03;
    c->coeffs[4 * i + 3] = (r[i] >> 6);
  }
}

void NEWHOPE_offer(uint8_t *offermsg, NEWHOPE_POLY *s) {
  NEWHOPE_POLY_noise_ntt(s);

  /* The first part of the offer message is the seed, which compactly encodes
   * a. */
  NEWHOPE_POLY a;
  uint8_t *seed = &offermsg[NEWHOPE_POLY_LENGTH];
  RAND_bytes(seed, SEED_LENGTH);
  newhope_poly_uniform(&a, seed);

  NEWHOPE_POLY e;
  NEWHOPE_POLY_noise_ntt(&e);

  /* The second part of the offer message is the polynomial pk = a*s+e */
  NEWHOPE_POLY pk;
  NEWHOPE_offer_computation(&pk, s,  &e, &a);
  NEWHOPE_POLY_tobytes(offermsg, &pk);
}

int NEWHOPE_accept(uint8_t key[SHA256_DIGEST_LENGTH],
                   uint8_t acceptmsg[NEWHOPE_ACCEPTMSG_LENGTH],
                   const uint8_t offermsg[NEWHOPE_OFFERMSG_LENGTH],
                   size_t msg_len) {
  if (msg_len != NEWHOPE_OFFERMSG_LENGTH) {
    return 0;
  }

  /* Decode the |offermsg|, generating the same |a| as the peer, from the peer's
   * seed. */
  NEWHOPE_POLY pk, a;
  NEWHOPE_offer_frommsg(&pk, &a, offermsg);

  /* Generate noise polynomials used to generate our key. */
  NEWHOPE_POLY sp, ep, epp;
  NEWHOPE_POLY_noise_ntt(&sp);
  NEWHOPE_POLY_noise_ntt(&ep);
  NEWHOPE_POLY_noise(&epp);  /* intentionally not NTT */

  /* Generate random bytes used for reconciliation. (The reference
   * implementation calls ChaCha20 here.) */
  uint8_t rand[32];
  RAND_bytes(rand, sizeof(rand));

  /* Encode |bp| and |c| as the |acceptmsg|. */
  NEWHOPE_POLY bp, c;
  uint8_t k[NEWHOPE_KEY_LENGTH];
  NEWHOPE_accept_computation(k, &bp, &c, &sp, &ep, &epp, rand, &pk, &a);
  NEWHOPE_POLY_tobytes(acceptmsg, &bp);
  encode_rec(&c, &acceptmsg[NEWHOPE_POLY_LENGTH]);

  SHA256_CTX ctx;
  if (!SHA256_Init(&ctx) ||
      !SHA256_Update(&ctx, k, NEWHOPE_KEY_LENGTH) ||
      !SHA256_Final(key, &ctx)) {
    return 0;
  }

  return 1;
}

int NEWHOPE_finish(uint8_t key[SHA256_DIGEST_LENGTH], const NEWHOPE_POLY *sk,
                   const uint8_t acceptmsg[NEWHOPE_ACCEPTMSG_LENGTH],
                   size_t msg_len) {
  if (msg_len != NEWHOPE_ACCEPTMSG_LENGTH) {
    return 0;
  }

  /* Decode the accept message into |bp| and |c|. */
  NEWHOPE_POLY bp, c;
  NEWHOPE_POLY_frombytes(&bp, acceptmsg);
  decode_rec(&acceptmsg[NEWHOPE_POLY_LENGTH], &c);

  uint8_t k[NEWHOPE_KEY_LENGTH];
  NEWHOPE_finish_computation(k, sk, &bp, &c);
  SHA256_CTX ctx;
  if (!SHA256_Init(&ctx) ||
      !SHA256_Update(&ctx, k, NEWHOPE_KEY_LENGTH) ||
      !SHA256_Final(key, &ctx)) {
    return 0;
  }

  return 1;
}

void NEWHOPE_offer_computation(NEWHOPE_POLY *out_pk,
                               const NEWHOPE_POLY *s, const NEWHOPE_POLY *e,
                               const NEWHOPE_POLY *a) {
  NEWHOPE_POLY r;
  newhope_poly_pointwise(&r, s, a);
  newhope_poly_add(out_pk, e, &r);
}

void NEWHOPE_accept_computation(
    uint8_t k[NEWHOPE_KEY_LENGTH], NEWHOPE_POLY *bp,
    NEWHOPE_POLY *reconciliation,
    const NEWHOPE_POLY *sp, const NEWHOPE_POLY *ep, const NEWHOPE_POLY *epp,
    const uint8_t rand[32],
    const NEWHOPE_POLY *pk, const NEWHOPE_POLY *a) {
  /* bp = a*s' + e' */
  newhope_poly_pointwise(bp, a, sp);
  newhope_poly_add(bp, bp, ep);

  /* v = pk * s' + e'' */
  NEWHOPE_POLY v;
  newhope_poly_pointwise(&v, pk, sp);
  newhope_poly_invntt(&v);
  newhope_poly_add(&v, &v, epp);
  newhope_helprec(reconciliation, &v, rand);
  newhope_reconcile(k, &v, reconciliation);
}

void NEWHOPE_finish_computation(uint8_t k[NEWHOPE_KEY_LENGTH],
                                const NEWHOPE_POLY *sk, const NEWHOPE_POLY *bp,
                                const NEWHOPE_POLY *reconciliation) {
  NEWHOPE_POLY v;
  newhope_poly_pointwise(&v, sk, bp);
  newhope_poly_invntt(&v);
  newhope_reconcile(k, &v, reconciliation);
}

void NEWHOPE_offer_frommsg(NEWHOPE_POLY *out_pk, NEWHOPE_POLY *out_a,
                           const uint8_t offermsg[NEWHOPE_OFFERMSG_LENGTH]) {
  NEWHOPE_POLY_frombytes(out_pk, offermsg);
  const uint8_t *seed = offermsg + NEWHOPE_POLY_LENGTH;
  newhope_poly_uniform(out_a, seed);
}
