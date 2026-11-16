// Copyright 2018 The BoringSSL Authors
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

#ifndef OPENSSL_HEADER_CRYPTO_FIPSMODULE_TLS_INTERNAL_H
#define OPENSSL_HEADER_CRYPTO_FIPSMODULE_TLS_INTERNAL_H

#include <openssl/base.h>


BSSL_NAMESPACE_BEGIN

// CRYPTO_tls13_hkdf_expand_label computes the TLS 1.3 KDF function of the same
// name. See https://www.rfc-editor.org/rfc/rfc8446#section-7.1.
OPENSSL_EXPORT int CRYPTO_tls13_hkdf_expand_label(
    uint8_t *out, size_t out_len, const EVP_MD *digest,  //
    const uint8_t *secret, size_t secret_len,            //
    const uint8_t *label, size_t label_len,              //
    const uint8_t *hash, size_t hash_len);

BSSL_NAMESPACE_END

#endif  // OPENSSL_HEADER_CRYPTO_FIPSMODULE_TLS_INTERNAL_H
