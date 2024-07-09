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

#ifndef OPENSSL_HEADER_CRYPTO_KYBER_INTERNAL_H
#define OPENSSL_HEADER_CRYPTO_KYBER_INTERNAL_H

#include <openssl/base.h>
#include <openssl/experimental/kyber.h>

#if defined(__cplusplus)
extern "C" {
#endif


// KYBER_ENCAP_ENTROPY is the number of bytes of uniformly random entropy
// necessary to encapsulate a secret. The entropy will be leaked to the
// decapsulating party.
#define KYBER_ENCAP_ENTROPY 32

// KYBER_GENERATE_KEY_ENTROPY is the number of bytes of uniformly random entropy
// necessary to generate a key.
#define KYBER_GENERATE_KEY_ENTROPY 64

// KYBER_generate_key_external_entropy is a deterministic function to create a
// pair of Kyber768 keys, using the supplied entropy. The entropy needs to be
// uniformly random generated. This function is should only be used for tests,
// regular callers should use the non-deterministic |KYBER_generate_key|
// directly.
OPENSSL_EXPORT void KYBER_generate_key_external_entropy(
    uint8_t out_encoded_public_key[KYBER_PUBLIC_KEY_BYTES],
    struct KYBER_private_key *out_private_key,
    const uint8_t entropy[KYBER_GENERATE_KEY_ENTROPY]);

// KYBER_encap_external_entropy behaves like |KYBER_encap|, but uses
// |KYBER_ENCAP_ENTROPY| bytes of |entropy| for randomization. The decapsulating
// side will be able to recover |entropy| in full. This function should only be
// used for tests, regular callers should use the non-deterministic
// |KYBER_encap| directly.
OPENSSL_EXPORT void KYBER_encap_external_entropy(
    uint8_t out_ciphertext[KYBER_CIPHERTEXT_BYTES],
    uint8_t out_shared_secret[KYBER_SHARED_SECRET_BYTES],
    const struct KYBER_public_key *public_key,
    const uint8_t entropy[KYBER_ENCAP_ENTROPY]);

#if defined(__cplusplus)
}
#endif

#endif  // OPENSSL_HEADER_CRYPTO_KYBER_INTERNAL_H
