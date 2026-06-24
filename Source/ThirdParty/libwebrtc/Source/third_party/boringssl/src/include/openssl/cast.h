// Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef OPENSSL_HEADER_CAST_H
#define OPENSSL_HEADER_CAST_H

#include <openssl/base.h>   // IWYU pragma: export

#if defined(__cplusplus)
extern "C" {
#endif


// CAST-128.
//
// CAST-128 (RFC 2144), also known as CAST5, is a legacy block cipher with
// 64-bit blocks. It is deprecated and retained for backwards compatibility
// only. The implementation is not hardened against side channels and may leak
// secrets via timing or cache side channels.
//
// Use a modern cipher, such as AES-GCM or ChaCha20-Poly1305, instead.


#define CAST_ENCRYPT 1
#define CAST_DECRYPT 0

#define CAST_BLOCK 8
#define CAST_KEY_LENGTH 16

typedef struct cast_key_st {
  uint32_t data[32];
  int short_key;  // Use reduced rounds for short key
} CAST_KEY;

// CAST_set_key initializes `key` from `len` bytes of key material starting from
// `data`. CAST-128 keys are between 5 and 16 bytes long. If `len` is greater
// than 16, `data` is truncated and only the first 16 bytes are processed. If
// `len` is less than 5, it is internally zero-padded.
OPENSSL_EXPORT void CAST_set_key(CAST_KEY *key, size_t len,
                                 const uint8_t *data);

// CAST_ecb_encrypt encrypts (or decrypts, if `enc` is `CAST_DECRYPT`) a single
// 8-byte block from `in` to `out`, using `key`.
OPENSSL_EXPORT void CAST_ecb_encrypt(const uint8_t in[CAST_BLOCK],
                                     uint8_t out[CAST_BLOCK],
                                     const CAST_KEY *key, int enc);

// CAST_encrypt encrypts an 8-byte block from `data` in-place with `key`. An
// 8-byte block is represented in this function as two 32-bit integers,
// containing the first and second four bytes in big-endian order.
OPENSSL_EXPORT void CAST_encrypt(uint32_t data[2], const CAST_KEY *key);

// CAST_decrypt decrypts an 8-byte block from `data` in-place with `key`. An
// 8-byte block is represented in this function as two 32-bit integers,
// containing the first and second four bytes in big-endian order.
OPENSSL_EXPORT void CAST_decrypt(uint32_t data[2], const CAST_KEY *key);

// CAST_cbc_encrypt encrypts (or decrypts, if `enc` is `CAST_DECRYPT`) `length`
// bytes from `in` to `out` with CAST-128 in CBC mode. `length` must be a
// multiple of 8. The IV is taken from `iv`. When the function completes, the IV
// for the next block is written to `iv`.
OPENSSL_EXPORT void CAST_cbc_encrypt(const uint8_t *in, uint8_t *out,
                                     size_t length, const CAST_KEY *ks,
                                     uint8_t iv[8], int enc);

// CAST_cfb64_encrypt encrypts (or decrypts, if `enc` is `CAST_DECRYPT`)
// `length` bytes from `in` to `out` with CAST-128 in CFB-64 mode. `length` must
// be a multiple of 8. On the first call, `*num` should be zero and `ivec` the
// IV. On exit, this function will write state to `ivec` and `*num` to resume an
// encryption or decryption operation if the buffers are not contiguous.
OPENSSL_EXPORT void CAST_cfb64_encrypt(const uint8_t *in, uint8_t *out,
                                       size_t length, const CAST_KEY *schedule,
                                       uint8_t ivec[8], int *num, int enc);


#if defined(__cplusplus)
}  // extern C
#endif

#endif  // OPENSSL_HEADER_CAST_H
