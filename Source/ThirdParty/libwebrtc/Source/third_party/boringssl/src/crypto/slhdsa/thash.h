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

#ifndef OPENSSL_HEADER_CRYPTO_SLHDSA_THASH_H
#define OPENSSL_HEADER_CRYPTO_SLHDSA_THASH_H

#include "./params.h"

#if defined(__cplusplus)
extern "C" {
#endif


// Implements PRF_msg: a pseudo-random function that is used to generate the
// randomizer r for the randomized hashing of the message to be signed.
// (Section 4.1, page 11)
void slhdsa_thash_prfmsg(uint8_t output[SLHDSA_SHA2_128S_N],
                         const uint8_t sk_prf[SLHDSA_SHA2_128S_N],
                         const uint8_t opt_rand[SLHDSA_SHA2_128S_N],
                         const uint8_t header[SLHDSA_M_PRIME_HEADER_LEN],
                         const uint8_t *ctx, size_t ctx_len, const uint8_t *msg,
                         size_t msg_len);

// Implements H_msg: a hash function used to generate the digest of the message
// to be signed. (Section 4.1, page 11)
void slhdsa_thash_hmsg(uint8_t output[SLHDSA_SHA2_128S_DIGEST_SIZE],
                       const uint8_t r[SLHDSA_SHA2_128S_N],
                       const uint8_t pk_seed[SLHDSA_SHA2_128S_N],
                       const uint8_t pk_root[SLHDSA_SHA2_128S_N],
                       const uint8_t header[SLHDSA_M_PRIME_HEADER_LEN],
                       const uint8_t *ctx, size_t ctx_len, const uint8_t *msg,
                       size_t msg_len);

// Implements PRF: a pseudo-random function that is used to generate the secret
// values in WOTS+ and FORS private keys. (Section 4.1, page 11)
void slhdsa_thash_prf(uint8_t output[SLHDSA_SHA2_128S_N],
                      const uint8_t pk_seed[SLHDSA_SHA2_128S_N],
                      const uint8_t sk_seed[SLHDSA_SHA2_128S_N],
                      uint8_t addr[32]);

// Implements T_l: a hash function that maps an l*n-byte message to an n-byte
// message. Used for WOTS+ public key compression. (Section 4.1, page 11)
void slhdsa_thash_tl(uint8_t output[SLHDSA_SHA2_128S_N],
                     const uint8_t input[SLHDSA_SHA2_128S_WOTS_BYTES],
                     const uint8_t pk_seed[SLHDSA_SHA2_128S_N],
                     uint8_t addr[32]);

// Implements H: a hash function that takes a 2*n-byte message as input and
// produces an n-byte output. (Section 4.1, page 11)
void slhdsa_thash_h(uint8_t output[SLHDSA_SHA2_128S_N],
                    const uint8_t input[2 * SLHDSA_SHA2_128S_N],
                    const uint8_t pk_seed[SLHDSA_SHA2_128S_N],
                    uint8_t addr[32]);

// Implements F: a hash function that takes an n-byte message as input and
// produces an n-byte output. (Section 4.1, page 11)
void slhdsa_thash_f(uint8_t output[SLHDSA_SHA2_128S_N],
                    const uint8_t input[SLHDSA_SHA2_128S_N],
                    const uint8_t pk_seed[SLHDSA_SHA2_128S_N],
                    uint8_t addr[32]);

// Implements T_k: a hash function that maps a k*n-byte message to an n-byte
// message. Used for FORS public key compression. (Section 4.1, page 11)
void slhdsa_thash_tk(
    uint8_t output[SLHDSA_SHA2_128S_N],
    const uint8_t input[SLHDSA_SHA2_128S_FORS_TREES * SLHDSA_SHA2_128S_N],
    const uint8_t pk_seed[SLHDSA_SHA2_128S_N], uint8_t addr[32]);


#if defined(__cplusplus)
}  // extern C
#endif

#endif  // OPENSSL_HEADER_CRYPTO_SLHDSA_THASH_H
