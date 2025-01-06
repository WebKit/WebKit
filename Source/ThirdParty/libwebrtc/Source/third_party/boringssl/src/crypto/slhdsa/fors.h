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

#ifndef OPENSSL_HEADER_CRYPTO_SLHDSA_FORS_H
#define OPENSSL_HEADER_CRYPTO_SLHDSA_FORS_H

#include "./params.h"

#if defined(__cplusplus)
extern "C" {
#endif


// Implements Algorithm 14: fors_skGen function (page 29)
void slhdsa_fors_sk_gen(uint8_t fors_sk[SLHDSA_SHA2_128S_N], uint32_t idx,
                        const uint8_t sk_seed[SLHDSA_SHA2_128S_N],
                        const uint8_t pk_seed[SLHDSA_SHA2_128S_N],
                        uint8_t addr[32]);

// Implements Algorithm 15: fors_node function (page 30)
void slhdsa_fors_treehash(uint8_t root_node[SLHDSA_SHA2_128S_N],
                          const uint8_t sk_seed[SLHDSA_SHA2_128S_N],
                          uint32_t i /*target node index*/,
                          uint32_t z /*target node height*/,
                          const uint8_t pk_seed[SLHDSA_SHA2_128S_N],
                          uint8_t addr[32]);

// Implements Algorithm 16: fors_sign function (page 31)
void slhdsa_fors_sign(uint8_t fors_sig[SLHDSA_SHA2_128S_FORS_BYTES],
                      const uint8_t message[SLHDSA_SHA2_128S_FORS_MSG_BYTES],
                      const uint8_t sk_seed[SLHDSA_SHA2_128S_N],
                      const uint8_t pk_seed[SLHDSA_SHA2_128S_N],
                      uint8_t addr[32]);

// Implements Algorithm 17: fors_pkFromSig function (page 32)
void slhdsa_fors_pk_from_sig(
    uint8_t fors_pk[SLHDSA_SHA2_128S_N],
    const uint8_t fors_sig[SLHDSA_SHA2_128S_FORS_BYTES],
    const uint8_t message[SLHDSA_SHA2_128S_FORS_MSG_BYTES],
    const uint8_t pk_seed[SLHDSA_SHA2_128S_N], uint8_t addr[32]);


#if defined(__cplusplus)
}  // extern C
#endif

#endif  // OPENSSL_HEADER_CRYPTO_SLHDSA_FORS_H
