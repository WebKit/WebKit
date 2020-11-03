/* Copyright (c) 2020, Google Inc.
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

#ifndef OPENSSL_HEADER_EC_EXTRA_INTERNAL_H
#define OPENSSL_HEADER_EC_EXTRA_INTERNAL_H

#include <openssl/ec.h>

#include "../fipsmodule/ec/internal.h"

#if defined(__cplusplus)
extern "C" {
#endif


// Hash-to-curve.
//
// The following functions implement primitives from
// draft-irtf-cfrg-hash-to-curve. The |dst| parameter in each function is the
// domain separation tag and must be unique for each protocol and between the
// |hash_to_curve| and |hash_to_scalar| variants. See section 3.1 of the spec
// for additional guidance on this parameter.

// ec_hash_to_curve_p384_xmd_sha512_sswu_draft07 hashes |msg| to a point on
// |group| and writes the result to |out|, implementing the
// P384_XMD:SHA-512_SSWU_RO_ suite from draft-irtf-cfrg-hash-to-curve-07. It
// returns one on success and zero on error.
OPENSSL_EXPORT int ec_hash_to_curve_p384_xmd_sha512_sswu_draft07(
    const EC_GROUP *group, EC_RAW_POINT *out, const uint8_t *dst,
    size_t dst_len, const uint8_t *msg, size_t msg_len);

// ec_hash_to_scalar_p384_xmd_sha512_draft07 hashes |msg| to a scalar on |group|
// and writes the result to |out|, using the hash_to_field operation from the
// P384_XMD:SHA-512_SSWU_RO_ suite from draft-irtf-cfrg-hash-to-curve-07, but
// generating a value modulo the group order rather than a field element.
OPENSSL_EXPORT int ec_hash_to_scalar_p384_xmd_sha512_draft07(
    const EC_GROUP *group, EC_SCALAR *out, const uint8_t *dst, size_t dst_len,
    const uint8_t *msg, size_t msg_len);


#if defined(__cplusplus)
}  // extern C
#endif

#endif  // OPENSSL_HEADER_EC_EXTRA_INTERNAL_H
