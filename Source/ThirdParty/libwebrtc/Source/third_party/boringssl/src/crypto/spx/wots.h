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

#ifndef OPENSSL_HEADER_CRYPTO_SPX_WOTS_H
#define OPENSSL_HEADER_CRYPTO_SPX_WOTS_H

#include <openssl/base.h>

#include "./params.h"

#if defined(__cplusplus)
extern "C" {
#endif


// Algorithm 5: Generate a WOTS+ public key.
void spx_wots_pk_gen(uint8_t *pk, const uint8_t sk_seed[SPX_N],
                     const uint8_t pub_seed[SPX_N], uint8_t addr[32]);

// Algorithm 6: Generate a WOTS+ signature on an n-byte message.
void spx_wots_sign(uint8_t *sig, const uint8_t msg[SPX_N],
                   const uint8_t sk_seed[SPX_N], const uint8_t pub_seed[SPX_N],
                   uint8_t addr[32]);

// Algorithm 7: Compute a WOTS+ public key from a message and its signature.
void spx_wots_pk_from_sig(uint8_t *pk, const uint8_t *sig, const uint8_t *msg,
                          const uint8_t pub_seed[SPX_N], uint8_t addr[32]);


#if defined(__cplusplus)
}  // extern C
#endif

#endif  // OPENSSL_HEADER_CRYPTO_SPX_WOTS_H
