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

#ifndef OPENSSL_HEADER_MD5_H
#define OPENSSL_HEADER_MD5_H

#include <openssl/base.h>   // IWYU pragma: export

#if defined(__cplusplus)
extern "C" {
#endif


// MD5.


// MD5_CBLOCK is the block size of MD5.
#define MD5_CBLOCK 64

// MD5_DIGEST_LENGTH is the length of an MD5 digest.
#define MD5_DIGEST_LENGTH 16

// MD5_STATE_MAX_BYTES is the maximum length, in bytes, of a marshaled `MD5_CTX`
// state.
#define MD5_STATE_MAX_BYTES (16 + 8 + MD5_CBLOCK - 1)

// MD5_Init initialises `md5` and returns one.
OPENSSL_EXPORT int MD5_Init(MD5_CTX *md5);

// MD5_Update adds `len` bytes from `data` to `md5` and returns one.
OPENSSL_EXPORT int MD5_Update(MD5_CTX *md5, const void *data, size_t len);

// MD5_Final adds the final padding to `md5` and writes the resulting digest to
// `out`, which must have at least `MD5_DIGEST_LENGTH` bytes of space. It
// returns one.
OPENSSL_EXPORT int MD5_Final(uint8_t out[MD5_DIGEST_LENGTH], MD5_CTX *md5);

// MD5 writes the digest of `len` bytes from `data` to `out` and returns `out`.
// There must be at least `MD5_DIGEST_LENGTH` bytes of space in `out`.
OPENSSL_EXPORT uint8_t *MD5(const uint8_t *data, size_t len,
                            uint8_t out[MD5_DIGEST_LENGTH]);

// MD5_marshal_state serializes the intermediate hash state in `md5` into `out`,
// sets `*out_len` to the number of bytes written (at most
// `MD5_STATE_MAX_BYTES`), and returns one. `md5` is not modified. The
// serialized format is guaranteed to be backward- and forward-compatible across
// versions of BoringSSL.
OPENSSL_EXPORT int MD5_marshal_state(const MD5_CTX *md5,
                                     uint8_t out[MD5_STATE_MAX_BYTES],
                                     size_t *out_len);

// MD5_parse_state deserializes `in_len` bytes from `in` (in the format produced
// by `MD5_marshal_state`) into `md5` and returns one. If `in` is malformed, it
// returns zero and leaves `md5` unmodified. The serialized format is guaranteed
// to be backward- and forward-compatible across versions of BoringSSL.
OPENSSL_EXPORT int MD5_parse_state(MD5_CTX *md5, const uint8_t *in,
                                   size_t in_len);

// MD5_Transform is a low-level function that performs a single, MD5 block
// transformation using the state from `md5` and 64 bytes from `block`.
OPENSSL_EXPORT void MD5_Transform(MD5_CTX *md5,
                                  const uint8_t block[MD5_CBLOCK]);

struct md5_state_st {
  uint32_t h[4];
  uint32_t Nl, Nh;
  uint8_t data[MD5_CBLOCK];
  unsigned num;
};


#if defined(__cplusplus)
}  // extern C
#endif

#endif  // OPENSSL_HEADER_MD5_H
