// Copyright 2026 The BoringSSL Authors
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

#include <openssl/evp.h>

#include <stddef.h>
#include <stdint.h>

#include <openssl/err.h>

#include "../internal.h"
#include "internal.h"

using namespace bssl;

// Checks whether KEM function invocation is valid.
// `ciphertext_len` may be nullptr if it is not required to match.
// `secret_len` must always match.
static bool check_kem_invocation(const EVP_KEM *kem,
                                 const size_t *ciphertext_len,
                                 size_t secret_len,
                                 const bssl::EvpPkey *pkey_impl) {
  if (pkey_impl == nullptr || pkey_impl->pkey == nullptr ||
      pkey_impl->ameth == nullptr) {
    OPENSSL_PUT_ERROR(EVP, ERR_R_PASSED_NULL_PARAMETER);
    return false;
  }
  if (kem->pkey_id != EVP_PKEY_id(pkey_impl)) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_UNSUPPORTED_ALGORITHM);
    return false;
  }
  if (ciphertext_len && *ciphertext_len != kem->ciphertext_len) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_INVALID_CIPHERTEXT_LENGTH);
    return false;
  }
  if (secret_len != kem->secret_len) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_INVALID_SECRET_LENGTH);
    return false;
  }
  return true;
}

size_t EVP_KEM_ciphertext_len(const EVP_KEM *kem) {
  return kem->ciphertext_len;
}

size_t EVP_KEM_secret_len(const EVP_KEM *kem) {
  return kem->secret_len;
}

int EVP_KEM_encap(const EVP_KEM *kem, uint8_t *out_ciphertext,
                  size_t ciphertext_len, uint8_t *out_secret, size_t secret_len,
                  const EVP_PKEY *peer_key) {
  auto *pkey_impl = FromOpaque(peer_key);
  if (!check_kem_invocation(kem, &ciphertext_len, secret_len, pkey_impl)) {
    return 0;
  }
  return kem->encap(out_ciphertext, ciphertext_len, out_secret, secret_len,
                    pkey_impl);
}

int EVP_KEM_decap(const EVP_KEM *kem, uint8_t *out_secret, size_t secret_len,
                  const uint8_t *ciphertext, size_t ciphertext_len,
                  const EVP_PKEY *key) {
  auto *pkey_impl = FromOpaque(key);
  if (!check_kem_invocation(kem, nullptr, secret_len, pkey_impl)) {
    return 0;
  }
  return kem->decap(out_secret, secret_len, ciphertext, ciphertext_len,
                    pkey_impl);
}
