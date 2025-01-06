/* Copyright (c) 2021, Google Inc.
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

#ifndef OPENSSL_HEADER_CRYPTO_FIPSMODULE_ECDSA_INTERNAL_H
#define OPENSSL_HEADER_CRYPTO_FIPSMODULE_ECDSA_INTERNAL_H

#include <openssl/base.h>

#include "../ec/internal.h"

#if defined(__cplusplus)
extern "C" {
#endif


// ECDSA_MAX_FIXED_LEN is the maximum length of an ECDSA signature in the
// fixed-width, big-endian format from IEEE P1363.
#define ECDSA_MAX_FIXED_LEN (2 * EC_MAX_BYTES)

// ecdsa_sign_fixed behaves like |ECDSA_sign| but uses the fixed-width,
// big-endian format from IEEE P1363.
int ecdsa_sign_fixed(const uint8_t *digest, size_t digest_len, uint8_t *sig,
                     size_t *out_sig_len, size_t max_sig_len,
                     const EC_KEY *key);

// ecdsa_sign_fixed_with_nonce_for_known_answer_test behaves like
// |ecdsa_sign_fixed| but takes a caller-supplied nonce. This function is used
// as part of known-answer tests in the FIPS module.
int ecdsa_sign_fixed_with_nonce_for_known_answer_test(
    const uint8_t *digest, size_t digest_len, uint8_t *sig, size_t *out_sig_len,
    size_t max_sig_len, const EC_KEY *key, const uint8_t *nonce,
    size_t nonce_len);

// ecdsa_verify_fixed behaves like |ECDSA_verify| but uses the fixed-width,
// big-endian format from IEEE P1363.
int ecdsa_verify_fixed(const uint8_t *digest, size_t digest_len,
                       const uint8_t *sig, size_t sig_len, const EC_KEY *key);

// ecdsa_verify_fixed_no_self_test behaves like ecdsa_verify_fixed, but doesn't
// try to run the self-test first. This is for use in the self tests themselves,
// to prevent an infinite loop.
int ecdsa_verify_fixed_no_self_test(const uint8_t *digest, size_t digest_len,
                                    const uint8_t *sig, size_t sig_len,
                                    const EC_KEY *key);


#if defined(__cplusplus)
}
#endif

#endif  // OPENSSL_HEADER_CRYPTO_FIPSMODULE_ECDSA_INTERNAL_H
