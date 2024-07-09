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

#ifndef OPENSSL_HEADER_SPX_H
#define OPENSSL_HEADER_SPX_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif


#if defined(OPENSSL_UNSTABLE_EXPERIMENTAL_SPX)
// This header implements experimental, draft versions of not-yet-standardized
// primitives. When the standard is complete, these functions will be removed
// and replaced with the final, incompatible standard version. They are
// available now for short-lived experiments, but must not be deployed anywhere
// durable, such as a long-lived key store. To use these functions define
// OPENSSL_UNSTABLE_EXPERIMENTAL_SPX

// SPX_N is the number of bytes in the hash output
#define SPX_N 16

// SPX_PUBLIC_KEY_BYTES is the nNumber of bytes in the public key of
// SPHINCS+-SHA2-128s
#define SPX_PUBLIC_KEY_BYTES 32

// SPX_SECRET_KEY_BYTES is the number of bytes in the private key of
// SPHINCS+-SHA2-128s
#define SPX_SECRET_KEY_BYTES 64

// SPX_SIGNATURE_BYTES is the number of bytes in a signature of
// SPHINCS+-SHA2-128s
#define SPX_SIGNATURE_BYTES 7856

// SPX_generate_key generates a SPHINCS+-SHA2-128s key pair and writes the
// result to |out_public_key| and |out_secret_key|.
// Private key: SK.seed || SK.prf || PK.seed || PK.root
// Public key: PK.seed || PK.root
OPENSSL_EXPORT void SPX_generate_key(
    uint8_t out_public_key[SPX_PUBLIC_KEY_BYTES],
    uint8_t out_secret_key[SPX_SECRET_KEY_BYTES]);

// SPX_generate_key_from_seed generates a SPHINCS+-SHA2-128s key pair from a
// 48-byte seed and writes the result to |out_public_key| and |out_secret_key|.
// Secret key: SK.seed || SK.prf || PK.seed || PK.root
// Public key: PK.seed || PK.root
OPENSSL_EXPORT void SPX_generate_key_from_seed(
    uint8_t out_public_key[SPX_PUBLIC_KEY_BYTES],
    uint8_t out_secret_key[SPX_SECRET_KEY_BYTES],
    const uint8_t seed[3 * SPX_N]);

// SPX_sign generates a SPHINCS+-SHA2-128s signature over |msg| or length
// |msg_len| using |secret_key| and writes the output to |out_signature|.
//
// if |randomized| is 0, deterministic signing is performed, otherwise,
// non-deterministic signing is performed.
OPENSSL_EXPORT void SPX_sign(
    uint8_t out_snignature[SPX_SIGNATURE_BYTES],
    const uint8_t secret_key[SPX_SECRET_KEY_BYTES], const uint8_t *msg,
    size_t msg_len, int randomized);

// SPX_verify verifies a SPHINCS+-SHA2-128s signature in |signature| over |msg|
// or length |msg_len| using |public_key|. 1 is returned if the signature
// matches, 0 otherwise.
OPENSSL_EXPORT int SPX_verify(
    const uint8_t signature[SPX_SIGNATURE_BYTES],
    const uint8_t public_key[SPX_SECRET_KEY_BYTES], const uint8_t *msg,
    size_t msg_len);

#endif //OPENSSL_UNSTABLE_EXPERIMENTAL_SPX


#if defined(__cplusplus)
}  // extern C
#endif

#endif  // OPENSSL_HEADER_SPX_H
