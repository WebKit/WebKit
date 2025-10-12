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

#ifndef OPENSSL_HEADER_CRYPTO_RSA_INTERNAL_H
#define OPENSSL_HEADER_CRYPTO_RSA_INTERNAL_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif


int RSA_padding_check_PKCS1_OAEP_mgf1(uint8_t *out, size_t *out_len,
                                      size_t max_out, const uint8_t *from,
                                      size_t from_len, const uint8_t *param,
                                      size_t param_len, const EVP_MD *md,
                                      const EVP_MD *mgf1md);

enum rsa_pss_params_t {
  // RSA-PSS using SHA-256, MGF1 with SHA-256, salt length 32.
  rsa_pss_sha256,
  // RSA-PSS using SHA-384, MGF1 with SHA-384, salt length 48.
  rsa_pss_sha384,
  // RSA-PSS using SHA-512, MGF1 with SHA-512, salt length 64.
  rsa_pss_sha512,
};

// rsa_pss_params_get_md returns the hash function used with |params|. This also
// specifies the MGF-1 hash and the salt length because we do not support other
// configurations.
const EVP_MD *rsa_pss_params_get_md(rsa_pss_params_t params);

// rsa_marshal_pss_params marshals |params| as a DER-encoded RSASSA-PSS-params
// (RFC 4055). It returns one on success and zero on error.
int rsa_marshal_pss_params(CBB *cbb, rsa_pss_params_t params);

// rsa_marshal_pss_params decodes a DER-encoded RSASSA-PSS-params
// (RFC 4055). It returns one on success and zero on error. On success, it sets
// |*out| to the result. If |allow_explicit_trailer| is non-zero, an explicit
// encoding of the trailerField is allowed, although it is not valid DER.
int rsa_parse_pss_params(CBS *cbs, rsa_pss_params_t *out,
                         int allow_explicit_trailer);


#if defined(__cplusplus)
}  // extern C
#endif

#endif  // OPENSSL_HEADER_CRYPTO_RSA_INTERNAL_H
