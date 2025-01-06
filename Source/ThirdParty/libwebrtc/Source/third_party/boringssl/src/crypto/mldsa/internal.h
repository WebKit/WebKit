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

#ifndef OPENSSL_HEADER_CRYPTO_MLDSA_INTERNAL_H
#define OPENSSL_HEADER_CRYPTO_MLDSA_INTERNAL_H

#include <openssl/base.h>
#include <openssl/mldsa.h>

#if defined(__cplusplus)
extern "C" {
#endif


// MLDSA_SIGNATURE_RANDOMIZER_BYTES is the number of bytes of uniformly
// random entropy necessary to generate a signature in randomized mode.
#define MLDSA_SIGNATURE_RANDOMIZER_BYTES 32


// ML-DSA-65

// MLDSA65_generate_key_external_entropy generates a public/private key pair
// using the given seed, writes the encoded public key to
// |out_encoded_public_key| and sets |out_private_key| to the private key.
// It returns 1 on success and 0 on failure.
OPENSSL_EXPORT int MLDSA65_generate_key_external_entropy(
    uint8_t out_encoded_public_key[MLDSA65_PUBLIC_KEY_BYTES],
    struct MLDSA65_private_key *out_private_key,
    const uint8_t entropy[MLDSA_SEED_BYTES]);

// MLDSA65_sign_internal signs |msg| using |private_key| and writes the
// signature to |out_encoded_signature|. The |context_prefix| and |context| are
// prefixed to the message, in that order, before signing. The |randomizer|
// value can be set to zero bytes in order to make a deterministic signature, or
// else filled with entropy for the usual |MLDSA_sign| behavior. It returns 1 on
// success and 0 on error.
OPENSSL_EXPORT int MLDSA65_sign_internal(
    uint8_t out_encoded_signature[MLDSA65_SIGNATURE_BYTES],
    const struct MLDSA65_private_key *private_key, const uint8_t *msg,
    size_t msg_len, const uint8_t *context_prefix, size_t context_prefix_len,
    const uint8_t *context, size_t context_len,
    const uint8_t randomizer[MLDSA_SIGNATURE_RANDOMIZER_BYTES]);

// MLDSA65_verify_internal verifies that |encoded_signature| is a valid
// signature of |msg| by |public_key|. The |context_prefix| and |context| are
// prefixed to the message before verification, in that order. It returns 1 on
// success and 0 on error.
OPENSSL_EXPORT int MLDSA65_verify_internal(
    const struct MLDSA65_public_key *public_key,
    const uint8_t encoded_signature[MLDSA65_SIGNATURE_BYTES],
    const uint8_t *msg, size_t msg_len, const uint8_t *context_prefix,
    size_t context_prefix_len, const uint8_t *context, size_t context_len);

// MLDSA65_marshal_private_key serializes |private_key| to |out| in the
// NIST format for ML-DSA-65 private keys. It returns 1 on success or 0
// on allocation error.
OPENSSL_EXPORT int MLDSA65_marshal_private_key(
    CBB *out, const struct MLDSA65_private_key *private_key);


#if defined(__cplusplus)
}  // extern C
#endif

#endif  // OPENSSL_HEADER_CRYPTO_MLDSA_INTERNAL_H
