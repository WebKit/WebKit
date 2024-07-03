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

#ifndef OPENSSL_HEADER_CRYPTO_SPX_UTIL_H
#define OPENSSL_HEADER_CRYPTO_SPX_UTIL_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif


// Encodes the integer value of input to out_len bytes in big-endian order.
// Note that input < 2^(8*out_len), as otherwise this function will truncate
// the least significant bytes of the integer representation.
void spx_uint64_to_len_bytes(uint8_t *output, size_t out_len, uint64_t input);

uint64_t spx_to_uint64(const uint8_t *input, size_t input_len);

// Compute the base 2^log2_b representation of X.
//
// As some of the parameter sets in https://eprint.iacr.org/2022/1725.pdf use
// a FORS height > 16 we use a uint32_t to store the output.
void spx_base_b(uint32_t *output, size_t out_len, const uint8_t *input,
                unsigned int log2_b);


#if defined(__cplusplus)
}  // extern C
#endif

#endif  // OPENSSL_HEADER_CRYPTO_SPX_UTIL_H
