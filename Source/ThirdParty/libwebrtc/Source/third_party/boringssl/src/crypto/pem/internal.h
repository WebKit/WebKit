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

#ifndef OPENSSL_HEADER_CRYPTO_PEM_INTERNAL_H
#define OPENSSL_HEADER_CRYPTO_PEM_INTERNAL_H

#include <openssl/base.h>
#include <openssl/pem.h>

#include "../mem_internal.h"


BSSL_NAMESPACE_BEGIN

// These macros make the PEM_read/PEM_write functions easier to maintain and
// write. Now they are all implemented with either:
// IMPLEMENT_PEM_rw(...) or IMPLEMENT_PEM_rw_cb(...)

#define IMPLEMENT_PEM_read_fp(name, type, str, asn1)                         \
  static void *pem_read_##name##_d2i(void **x, const unsigned char **inp,    \
                                     long len) {                             \
    return d2i_##asn1((type **)x, inp, len);                                 \
  }                                                                          \
  OPENSSL_EXPORT type *PEM_read_##name(FILE *fp, type **x,                   \
                                       pem_password_cb *cb, void *u) {       \
    return (type *)PEM_ASN1_read(pem_read_##name##_d2i, str, fp, (void **)x, \
                                 cb, u);                                     \
  }

#define IMPLEMENT_PEM_write_fp(name, type, str, asn1)                        \
  static int pem_write_##name##_i2d(const void *x, unsigned char **outp) {   \
    return i2d_##asn1((const type *)x, outp);                                \
  }                                                                          \
  OPENSSL_EXPORT int PEM_write_##name(FILE *fp, const type *x) {             \
    return PEM_ASN1_write(pem_write_##name##_i2d, str, fp, x, NULL, NULL, 0, \
                          NULL, NULL);                                       \
  }

#define IMPLEMENT_PEM_write_cb_fp(name, type, str, asn1)                       \
  static int pem_write_##name##_i2d(const void *x, unsigned char **outp) {     \
    return i2d_##asn1((const type *)x, outp);                                  \
  }                                                                            \
  OPENSSL_EXPORT int PEM_write_##name(                                         \
      FILE *fp, const type *x, const EVP_CIPHER *enc,                          \
      const unsigned char *pass, int pass_len, pem_password_cb *cb, void *u) { \
    return PEM_ASN1_write(pem_write_##name##_i2d, str, fp, x, enc, pass,       \
                          pass_len, cb, u);                                    \
  }

#define IMPLEMENT_PEM_read_bio(name, type, str, asn1)                         \
  static void *pem_read_bio_##name##_d2i(void **x, const unsigned char **inp, \
                                         long len) {                          \
    return d2i_##asn1((type **)x, inp, len);                                  \
  }                                                                           \
  OPENSSL_EXPORT type *PEM_read_bio_##name(BIO *bp, type **x,                 \
                                           pem_password_cb *cb, void *u) {    \
    return (type *)PEM_ASN1_read_bio(pem_read_bio_##name##_d2i, str, bp,      \
                                     (void **)x, cb, u);                      \
  }

#define IMPLEMENT_PEM_write_bio(name, type, str, asn1)                         \
  static int pem_write_bio_##name##_i2d(const void *x, unsigned char **outp) { \
    return i2d_##asn1((const type *)x, outp);                                  \
  }                                                                            \
  OPENSSL_EXPORT int PEM_write_bio_##name(BIO *bp, const type *x) {            \
    return PEM_ASN1_write_bio(pem_write_bio_##name##_i2d, str, bp, x, NULL,    \
                              NULL, 0, NULL, NULL);                            \
  }

#define IMPLEMENT_PEM_write_cb_bio(name, type, str, asn1)                      \
  static int pem_write_bio_##name##_i2d(const void *x, unsigned char **outp) { \
    return i2d_##asn1((const type *)x, outp);                                  \
  }                                                                            \
  OPENSSL_EXPORT int PEM_write_bio_##name(                                     \
      BIO *bp, const type *x, const EVP_CIPHER *enc,                           \
      const unsigned char *pass, int pass_len, pem_password_cb *cb, void *u) { \
    return PEM_ASN1_write_bio(pem_write_bio_##name##_i2d, str, bp, x, enc,     \
                              pass, pass_len, cb, u);                          \
  }

#define IMPLEMENT_PEM_write(name, type, str, asn1) \
  IMPLEMENT_PEM_write_bio(name, type, str, asn1)   \
  IMPLEMENT_PEM_write_fp(name, type, str, asn1)

#define IMPLEMENT_PEM_write_cb(name, type, str, asn1) \
  IMPLEMENT_PEM_write_cb_bio(name, type, str, asn1)   \
  IMPLEMENT_PEM_write_cb_fp(name, type, str, asn1)

#define IMPLEMENT_PEM_read(name, type, str, asn1) \
  IMPLEMENT_PEM_read_bio(name, type, str, asn1)   \
  IMPLEMENT_PEM_read_fp(name, type, str, asn1)

#define IMPLEMENT_PEM_rw(name, type, str, asn1) \
  IMPLEMENT_PEM_read(name, type, str, asn1)     \
  IMPLEMENT_PEM_write(name, type, str, asn1)

#define IMPLEMENT_PEM_rw_cb(name, type, str, asn1) \
  IMPLEMENT_PEM_read(name, type, str, asn1)        \
  IMPLEMENT_PEM_write_cb(name, type, str, asn1)

// PEM_get_EVP_CIPHER_INFO decodes `header` as a PEM header block and writes the
// specified cipher and IV to `cipher`. It returns one on success and zero on
// error. `header` must be a NUL-terminated string. If `header` does not
// specify encryption, this function will return success and set
// `cipher->cipher` to NULL.
int PEM_get_EVP_CIPHER_INFO(const char *header, EVP_CIPHER_INFO *cipher);

// PEM_do_header decrypts `*len` bytes from `data` in-place according to the
// information in `cipher`. On success, it returns one and sets `*len` to the
// length of the plaintext. Otherwise, it returns zero. If `cipher` specifies
// encryption, the key is derived from a password returned from `callback`.
int PEM_do_header(const EVP_CIPHER_INFO *cipher, uint8_t *data, size_t *len,
                  pem_password_cb *callback, void *u);

// PEM_read_bio_inner differs from `PEM_read_bio` on the out pointer `len`
// so that it guarantee non-negativeness on this output and it takes in
// owned types.
int PEM_read_bio_inner(BIO *bp, bssl::UniquePtr<char> *name,
                       bssl::UniquePtr<char> *header,
                       bssl::Array<uint8_t> *data);

BSSL_NAMESPACE_END

#endif  // OPENSSL_HEADER_CRYPTO_PEM_INTERNAL_H
