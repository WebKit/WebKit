/* Copyright (c) 2024, Google LLC
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

#ifndef OPENSSL_HEADER_CRYPTO_SLHDSA_MERKLE_H
#define OPENSSL_HEADER_CRYPTO_SLHDSA_MERKLE_H

#include <openssl/base.h>

#include <sys/types.h>

#include "./params.h"

#if defined(__cplusplus)
extern "C" {
#endif


// Implements Algorithm 9: xmss_node function (page 23)
void slhdsa_treehash(uint8_t out_pk[SLHDSA_SHA2_128S_N],
                     const uint8_t sk_seed[SLHDSA_SHA2_128S_N],
                     uint32_t i /*target node index*/,
                     uint32_t z /*target node height*/,
                     const uint8_t pk_seed[SLHDSA_SHA2_128S_N],
                     uint8_t addr[32]);

// Implements Algorithm 10: xmss_sign function (page 24)
void slhdsa_xmss_sign(uint8_t sig[SLHDSA_SHA2_128S_XMSS_BYTES],
                      const uint8_t msg[SLHDSA_SHA2_128S_N], unsigned int idx,
                      const uint8_t sk_seed[SLHDSA_SHA2_128S_N],
                      const uint8_t pk_seed[SLHDSA_SHA2_128S_N],
                      uint8_t addr[32]);

// Implements Algorithm 11: xmss_pkFromSig function (page 25)
void slhdsa_xmss_pk_from_sig(
    uint8_t root[SLHDSA_SHA2_128S_N],
    const uint8_t xmss_sig[SLHDSA_SHA2_128S_XMSS_BYTES], unsigned int idx,
    const uint8_t msg[SLHDSA_SHA2_128S_N],
    const uint8_t pk_seed[SLHDSA_SHA2_128S_N], uint8_t addr[32]);

// Implements Algorithm 12: ht_sign function (page 27)
void slhdsa_ht_sign(
    uint8_t sig[SLHDSA_SHA2_128S_D * SLHDSA_SHA2_128S_XMSS_BYTES],
    const uint8_t message[SLHDSA_SHA2_128S_N], uint64_t idx_tree,
    uint32_t idx_leaf, const uint8_t sk_seed[SLHDSA_SHA2_128S_N],
    const uint8_t pk_seed[SLHDSA_SHA2_128S_N]);

// Implements Algorithm 13: ht_verify function (page 28)
int slhdsa_ht_verify(
    const uint8_t sig[SLHDSA_SHA2_128S_D * SLHDSA_SHA2_128S_XMSS_BYTES],
    const uint8_t message[SLHDSA_SHA2_128S_N], uint64_t idx_tree,
    uint32_t idx_leaf, const uint8_t pk_root[SLHDSA_SHA2_128S_N],
    const uint8_t pk_seed[SLHDSA_SHA2_128S_N]);


#if defined(__cplusplus)
}  // extern C
#endif

#endif  // OPENSSL_HEADER_CRYPTO_SLHDSA_MERKLE_H
