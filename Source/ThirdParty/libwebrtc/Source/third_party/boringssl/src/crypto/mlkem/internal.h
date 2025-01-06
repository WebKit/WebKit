/* Copyright (c) 2023, Google Inc.
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

#ifndef OPENSSL_HEADER_CRYPTO_MLKEM_INTERNAL_H
#define OPENSSL_HEADER_CRYPTO_MLKEM_INTERNAL_H

#include <openssl/base.h>
#include <openssl/mlkem.h>

#if defined(__cplusplus)
extern "C" {
#endif


// MLKEM_ENCAP_ENTROPY is the number of bytes of uniformly random entropy
// necessary to encapsulate a secret. The entropy will be leaked to the
// decapsulating party.
#define MLKEM_ENCAP_ENTROPY 32

// MLKEM768_generate_key_external_seed is a deterministic function to create a
// pair of ML-KEM-768 keys, using the supplied seed. The seed needs to be
// uniformly random. This function is should only be used for tests, regular
// callers should use the non-deterministic |MLKEM768_generate_key| directly.
OPENSSL_EXPORT void MLKEM768_generate_key_external_seed(
    uint8_t out_encoded_public_key[MLKEM768_PUBLIC_KEY_BYTES],
    struct MLKEM768_private_key *out_private_key,
    const uint8_t seed[MLKEM_SEED_BYTES]);

// MLKEM768_encap_external_entropy behaves like |MLKEM768_encap|, but uses
// |MLKEM_ENCAP_ENTROPY| bytes of |entropy| for randomization. The decapsulating
// side will be able to recover |entropy| in full. This function should only be
// used for tests, regular callers should use the non-deterministic
// |MLKEM768_encap| directly.
OPENSSL_EXPORT void MLKEM768_encap_external_entropy(
    uint8_t out_ciphertext[MLKEM768_CIPHERTEXT_BYTES],
    uint8_t out_shared_secret[MLKEM_SHARED_SECRET_BYTES],
    const struct MLKEM768_public_key *public_key,
    const uint8_t entropy[MLKEM_ENCAP_ENTROPY]);

// MLKEM768_marshal_private_key serializes |private_key| to |out| in the
// NIST format for ML-KEM-768 private keys. It returns one on success or
// zero on allocation error. (Note that one can also save just the seed value
// produced by |MLKEM768_generate_key|, which is significantly smaller.)
OPENSSL_EXPORT int MLKEM768_marshal_private_key(
    CBB *out, const struct MLKEM768_private_key *private_key);

// MLKEM1024_generate_key_external_seed is a deterministic function to create a
// pair of ML-KEM-1024 keys, using the supplied seed. The seed needs to be
// uniformly random. This function is should only be used for tests, regular
// callers should use the non-deterministic |MLKEM1024_generate_key| directly.
OPENSSL_EXPORT void MLKEM1024_generate_key_external_seed(
    uint8_t out_encoded_public_key[MLKEM1024_PUBLIC_KEY_BYTES],
    struct MLKEM1024_private_key *out_private_key,
    const uint8_t seed[MLKEM_SEED_BYTES]);

// MLKEM1024_encap_external_entropy behaves like |MLKEM1024_encap|, but uses
// |MLKEM_ENCAP_ENTROPY| bytes of |entropy| for randomization. The
// decapsulating side will be able to recover |entropy| in full. This function
// should only be used for tests, regular callers should use the
// non-deterministic |MLKEM1024_encap| directly.
OPENSSL_EXPORT void MLKEM1024_encap_external_entropy(
    uint8_t out_ciphertext[MLKEM1024_CIPHERTEXT_BYTES],
    uint8_t out_shared_secret[MLKEM_SHARED_SECRET_BYTES],
    const struct MLKEM1024_public_key *public_key,
    const uint8_t entropy[MLKEM_ENCAP_ENTROPY]);

// MLKEM1024_marshal_private_key serializes |private_key| to |out| in the
// NIST format for ML-KEM-1024 private keys. It returns one on success or
// zero on allocation error. (Note that one can also save just the seed value
// produced by |MLKEM1024_generate_key|, which is significantly smaller.)
OPENSSL_EXPORT int MLKEM1024_marshal_private_key(
    CBB *out, const struct MLKEM1024_private_key *private_key);


#if defined(__cplusplus)
}
#endif

#endif  // OPENSSL_HEADER_CRYPTO_MLKEM_INTERNAL_H
