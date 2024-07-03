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

#ifndef OPENSSL_HEADER_CRYPTO_SPX_FORS_H
#define OPENSSL_HEADER_CRYPTO_SPX_FORS_H

#include <openssl/base.h>

#include "./params.h"

#if defined(__cplusplus)
extern "C" {
#endif


// Algorithm 13: Generate a FORS private key value.
void spx_fors_sk_gen(uint8_t *fors_sk, uint32_t idx,
                     const uint8_t sk_seed[SPX_N], const uint8_t pk_seed[SPX_N],
                     uint8_t addr[32]);

// Algorithm 14: Compute the root of a Merkle subtree of FORS public values.
void spx_fors_treehash(uint8_t root_node[SPX_N], const uint8_t sk_seed[SPX_N],
                       uint32_t i /*target node index*/,
                       uint32_t z /*target node height*/,
                       const uint8_t pk_seed[SPX_N], uint8_t addr[32]);

// Algorithm 15: Generate a FORS signature.
void spx_fors_sign(uint8_t *fors_sig, const uint8_t message[SPX_FORS_MSG_BYTES],
                   const uint8_t sk_seed[SPX_N], const uint8_t pk_seed[SPX_N],
                   uint8_t addr[32]);

// Algorithm 16: Compute a FORS public key from a FORS signature.
void spx_fors_pk_from_sig(uint8_t *fors_pk,
                          const uint8_t fors_sig[SPX_FORS_BYTES],
                          const uint8_t message[SPX_FORS_MSG_BYTES],
                          const uint8_t pk_seed[SPX_N], uint8_t addr[32]);


#if defined(__cplusplus)
}  // extern C
#endif

#endif  // OPENSSL_HEADER_CRYPTO_SPX_FORS_H
