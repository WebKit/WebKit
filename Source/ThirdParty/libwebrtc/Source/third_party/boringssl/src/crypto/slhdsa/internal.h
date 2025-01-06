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

#ifndef OPENSSL_HEADER_CRYPTO_SLHDSA_INTERNAL_H
#define OPENSSL_HEADER_CRYPTO_SLHDSA_INTERNAL_H

#include <openssl/slhdsa.h>

#include "params.h"

#if defined(__cplusplus)
extern "C" {
#endif


// SLHDSA_SHA2_128S_generate_key_from_seed generates an SLH-DSA-SHA2-128s key
// pair from a 48-byte seed and writes the result to |out_public_key| and
// |out_secret_key|.
OPENSSL_EXPORT void SLHDSA_SHA2_128S_generate_key_from_seed(
    uint8_t out_public_key[SLHDSA_SHA2_128S_PUBLIC_KEY_BYTES],
    uint8_t out_secret_key[SLHDSA_SHA2_128S_PRIVATE_KEY_BYTES],
    const uint8_t seed[3 * SLHDSA_SHA2_128S_N]);

// SLHDSA_SHA2_128S_sign_internal acts like |SLHDSA_SHA2_128S_sign| but
// accepts an explicit entropy input, which can be PK.seed (bytes 32..48 of
// the private key) to generate deterministic signatures. It also takes the
// input message in three parts so that the "internal" version of the signing
// function, from section 9.2, can be implemented. The |header| argument may be
// NULL to omit it.
OPENSSL_EXPORT void SLHDSA_SHA2_128S_sign_internal(
    uint8_t out_signature[SLHDSA_SHA2_128S_SIGNATURE_BYTES],
    const uint8_t secret_key[SLHDSA_SHA2_128S_PRIVATE_KEY_BYTES],
    const uint8_t header[SLHDSA_M_PRIME_HEADER_LEN], const uint8_t *context,
    size_t context_len, const uint8_t *msg, size_t msg_len,
    const uint8_t entropy[SLHDSA_SHA2_128S_N]);

// SLHDSA_SHA2_128S_verify_internal acts like |SLHDSA_SHA2_128S_verify| but
// takes the input message in three parts so that the "internal" version of the
// verification function, from section 9.3, can be implemented. The |header|
// argument may be NULL to omit it.
OPENSSL_EXPORT int SLHDSA_SHA2_128S_verify_internal(
    const uint8_t *signature, size_t signature_len,
    const uint8_t public_key[SLHDSA_SHA2_128S_PUBLIC_KEY_BYTES],
    const uint8_t header[SLHDSA_M_PRIME_HEADER_LEN], const uint8_t *context,
    size_t context_len, const uint8_t *msg, size_t msg_len);


#if defined(__cplusplus)
}  // extern C
#endif

#endif  // OPENSSL_HEADER_CRYPTO_SLHDSA_INTERNAL_H
