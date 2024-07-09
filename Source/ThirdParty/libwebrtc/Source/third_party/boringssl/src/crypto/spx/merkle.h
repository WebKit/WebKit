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

#ifndef OPENSSL_HEADER_CRYPTO_SPX_MERKLE_H
#define OPENSSL_HEADER_CRYPTO_SPX_MERKLE_H

#include <openssl/base.h>

#include <sys/types.h>

#include "./params.h"

#if defined(__cplusplus)
extern "C" {
#endif


// Algorithm 8: Compute the root of a Merkle subtree of WOTS+ public keys.
void spx_treehash(uint8_t out_pk[SPX_N], const uint8_t sk_seed[SPX_N],
                  uint32_t i /*target node index*/,
                  uint32_t z /*target node height*/,
                  const uint8_t pk_seed[SPX_N], uint8_t addr[32]);

// Algorithm 9: Generate an XMSS signature.
void spx_xmss_sign(uint8_t *sig, const uint8_t msg[SPX_N], unsigned int idx,
                   const uint8_t sk_seed[SPX_N], const uint8_t pk_seed[SPX_N],
                   uint8_t addr[32]);

// Algorithm 10: Compute an XMSS public key from an XMSS signature.
void spx_xmss_pk_from_sig(uint8_t *root, const uint8_t *xmss_sig,
                          unsigned int idx, const uint8_t msg[SPX_N],
                          const uint8_t pk_seed[SPX_N], uint8_t addr[32]);

// Algorithm 11: Generate a hypertree signature.
void spx_ht_sign(uint8_t *sig, const uint8_t message[SPX_N], uint64_t idx_tree,
                 uint32_t idx_leaf, const uint8_t sk_seed[SPX_N],
                 const uint8_t pk_seed[SPX_N]);

// Algorithm 12: Verify a hypertree signature.
int spx_ht_verify(const uint8_t sig[SPX_D * SPX_XMSS_BYTES],
                  const uint8_t message[SPX_N], uint64_t idx_tree,
                  uint32_t idx_leaf, const uint8_t pk_root[SPX_N],
                  const uint8_t pk_seed[SPX_N]);


#if defined(__cplusplus)
}  // extern C
#endif

#endif  // OPENSSL_HEADER_CRYPTO_SPX_MERKLE_H
