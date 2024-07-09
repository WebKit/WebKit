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

#ifndef OPENSSL_HEADER_CRYPTO_SPX_THASH_H
#define OPENSSL_HEADER_CRYPTO_SPX_THASH_H

#include <openssl/base.h>

#include "./params.h"

#if defined(__cplusplus)
extern "C" {
#endif


// Implements F: a hash function takes an n-byte message as input and produces
// an n-byte output.
void spx_thash_f(uint8_t *output, const uint8_t input[SPX_N],
                 const uint8_t pk_seed[SPX_N], uint8_t addr[32]);

// Implements H: a hash function takes a 2*n-byte message as input and produces
// an n-byte output.
void spx_thash_h(uint8_t *output, const uint8_t input[2 * SPX_N],
                 const uint8_t pk_seed[SPX_N], uint8_t addr[32]);

// Implements Hmsg: a hash function used to generate the digest of the message
// to be signed.
void spx_thash_hmsg(uint8_t *output, const uint8_t r[SPX_N],
                    const uint8_t pk_seed[SPX_N], const uint8_t pk_root[SPX_N],
                    const uint8_t *msg, size_t msg_len);

// Implements PRF: a pseudo-random function that is used to generate the secret
// values in WOTS+ and FORS private keys.
void spx_thash_prf(uint8_t *output, const uint8_t pk_seed[SPX_N],
                   const uint8_t sk_seed[SPX_N], uint8_t addr[32]);

// Implements PRF: a pseudo-random function that is used to generate the
// randomizer r for the randomized hashing of the message to be signed. values
// in WOTS+ and FORS private keys.
void spx_thash_prfmsg(uint8_t *output, const uint8_t sk_prf[SPX_N],
                      const uint8_t opt_rand[SPX_N], const uint8_t *msg,
                      size_t msg_len);

// Implements Tl: a hash function that maps an l*n-byte message to an n-byte
// message.
void spx_thash_tl(uint8_t *output, const uint8_t input[SPX_WOTS_BYTES],
                  const uint8_t pk_seed[SPX_N], uint8_t addr[32]);

// Implements Tk: a hash function that maps a k*n-byte message to an n-byte
// message.
void spx_thash_tk(uint8_t *output, const uint8_t input[SPX_FORS_TREES * SPX_N],
                  const uint8_t pk_seed[SPX_N], uint8_t addr[32]);


#if defined(__cplusplus)
}  // extern C
#endif

#endif  // OPENSSL_HEADER_CRYPTO_SPX_THASH_H
