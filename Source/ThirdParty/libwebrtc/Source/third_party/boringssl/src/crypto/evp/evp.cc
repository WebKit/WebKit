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

#include <openssl/evp.h>

#include <assert.h>
#include <string.h>

#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/nid.h>

#include "../internal.h"
#include "../mem_internal.h"
#include "internal.h"


using namespace bssl;

// Node depends on |EVP_R_NOT_XOF_OR_INVALID_LENGTH|.
//
// TODO(davidben): Fix Node to not touch the error queue itself and remove this.
OPENSSL_DECLARE_ERROR_REASON(EVP, NOT_XOF_OR_INVALID_LENGTH)

// The HPKE module uses the EVP error namespace, but it lives in another
// directory.
OPENSSL_DECLARE_ERROR_REASON(EVP, EMPTY_PSK)

EVP_PKEY *EVP_PKEY_new() { return New<EvpPkey>(); }

EvpPkey::EvpPkey() : RefCounted(CheckSubClass()) {}

EvpPkey::~EvpPkey() { evp_pkey_set0(this, nullptr, nullptr); }

void EVP_PKEY_free(EVP_PKEY *pkey) {
  if (pkey == nullptr) {
    return;
  }

  auto *impl = FromOpaque(pkey);
  impl->DecRefInternal();
}

int EVP_PKEY_up_ref(EVP_PKEY *pkey) {
  auto *impl = FromOpaque(pkey);
  impl->UpRefInternal();
  return 1;
}

EVP_PKEY *EVP_PKEY_dup_ref(const EVP_PKEY *pkey) {
  auto pkey_ref = const_cast<EVP_PKEY *>(pkey);
  // We know that this call always returns one.
  EVP_PKEY_up_ref(pkey_ref);
  return pkey_ref;
}

int EVP_PKEY_is_opaque(const EVP_PKEY *pkey) {
  auto *impl = FromOpaque(pkey);
  if (impl->ameth && impl->ameth->pkey_opaque) {
    return impl->ameth->pkey_opaque(impl);
  }
  return 0;
}

int EVP_PKEY_eq(const EVP_PKEY *a, const EVP_PKEY *b) {
  // This also checks that |EVP_PKEY_id| matches.
  if (!EVP_PKEY_parameters_eq(a, b)) {
    return 0;
  }

  auto *a_impl = FromOpaque(a);
  auto *b_impl = FromOpaque(b);
  return a_impl->ameth != nullptr && a_impl->ameth->pub_equal != nullptr &&
         a_impl->pkey != nullptr && b_impl->pkey != nullptr &&
         a_impl->ameth->pub_equal(a_impl, b_impl);
}

int EVP_PKEY_cmp(const EVP_PKEY *a, const EVP_PKEY *b) {
  return EVP_PKEY_eq(a, b);
}

int EVP_PKEY_copy_parameters(EVP_PKEY *to, const EVP_PKEY *from) {
  auto *to_impl = FromOpaque(to);
  auto *from_impl = FromOpaque(from);

  if (EVP_PKEY_id(to_impl) == EVP_PKEY_NONE) {
    // TODO(crbug.com/42290409): This shouldn't leave |to| in a half-empty state
    // on error. The complexity here largely comes from parameterless DSA keys,
    // which we no longer support, so this function can probably be trimmed
    // down.
    evp_pkey_set0(to_impl, from_impl->ameth, nullptr);
  } else if (EVP_PKEY_id(to_impl) != EVP_PKEY_id(from_impl)) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_DIFFERENT_KEY_TYPES);
    return 0;
  }

  if (EVP_PKEY_missing_parameters(from_impl)) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_MISSING_PARAMETERS);
    return 0;
  }

  // Once set, parameters may not change.
  if (!EVP_PKEY_missing_parameters(to_impl)) {
    if (EVP_PKEY_parameters_eq(to_impl, from_impl) == 1) {
      return 1;
    }
    OPENSSL_PUT_ERROR(EVP, EVP_R_DIFFERENT_PARAMETERS);
    return 0;
  }

  if (from_impl->ameth && from_impl->ameth->param_copy) {
    return from_impl->ameth->param_copy(to_impl, from_impl);
  }

  // TODO(https://crbug.com/42290406): If the algorithm takes no parameters,
  // copying them should vacuously succeed. Better yet, simplify this whole
  // notion of parameter copying above.
  return 0;
}

int EVP_PKEY_missing_parameters(const EVP_PKEY *pkey) {
  auto *impl = FromOpaque(pkey);
  if (impl->ameth == nullptr) {
    return 0;  // EVP_PKEY_NONE is not parameterized, so nothing is missing.
  }
  if (impl->pkey == nullptr) {
    // This is an invalid, half-empty object. Report something is missing to
    // stop other parameter-based functions.
    return 1;
  }
  if (impl->ameth->param_missing) {
    return impl->ameth->param_missing(impl);
  }
  return 0;  // Not parameterized, so nothing is missing.
}

int EVP_PKEY_size(const EVP_PKEY *pkey) {
  auto *impl = FromOpaque(pkey);
  if (impl && impl->ameth && impl->ameth->pkey_size) {
    return impl->ameth->pkey_size(impl);
  }
  return 0;
}

int EVP_PKEY_bits(const EVP_PKEY *pkey) {
  auto *impl = FromOpaque(pkey);
  if (impl && impl->ameth && impl->ameth->pkey_bits) {
    return impl->ameth->pkey_bits(impl);
  }
  return 0;
}

int EVP_PKEY_id(const EVP_PKEY *pkey) {
  auto *impl = FromOpaque(pkey);
  return impl->ameth != nullptr ? impl->ameth->pkey_id : EVP_PKEY_NONE;
}

void bssl::evp_pkey_set0(EvpPkey *pkey, const EVP_PKEY_ASN1_METHOD *method,
                         void *pkey_data) {
  if (pkey->ameth && pkey->ameth->pkey_free) {
    pkey->ameth->pkey_free(pkey);
  }
  pkey->ameth = method;
  pkey->pkey = pkey_data;
}

int EVP_PKEY_type(int nid) {
  // In OpenSSL, this was used to map between type aliases. BoringSSL supports
  // no type aliases, so this function is just the identity.
  return nid;
}

int EVP_PKEY_assign(EVP_PKEY *pkey, int type, void *key) {
  // This function can only be used to assign RSA, DSA, EC, and DH keys. Other
  // key types have internal representations which are not exposed through the
  // public API.
  switch (type) {
    case EVP_PKEY_RSA:
      return EVP_PKEY_assign_RSA(pkey, reinterpret_cast<RSA *>(key));
    case EVP_PKEY_DSA:
      return EVP_PKEY_assign_DSA(pkey, reinterpret_cast<DSA *>(key));
    case EVP_PKEY_EC:
      return EVP_PKEY_assign_EC_KEY(pkey, reinterpret_cast<EC_KEY *>(key));
    case EVP_PKEY_DH:
      return EVP_PKEY_assign_DH(pkey, reinterpret_cast<DH *>(key));
  }

  OPENSSL_PUT_ERROR(EVP, EVP_R_UNSUPPORTED_ALGORITHM);
  ERR_add_error_dataf("algorithm %d", type);
  return 0;
}

int EVP_PKEY_set_type(EVP_PKEY *pkey, int type) {
  auto *impl = FromOpaque(pkey);
  if (impl && impl->pkey) {
    // Some callers rely on |pkey| getting cleared even if |type| is
    // unsupported, usually setting |type| to |EVP_PKEY_NONE|.
    evp_pkey_set0(impl, nullptr, nullptr);
  }

  // This function broadly isn't useful. It initializes |EVP_PKEY| for a type,
  // but forgets to put anything in the |pkey|. The one pattern where it does
  // anything is |EVP_PKEY_X25519|, where it's needed to make
  // |EVP_PKEY_set1_tls_encodedpoint| work, so we support only that.
  const EVP_PKEY_ALG *alg;
  if (type == EVP_PKEY_X25519) {
    alg = EVP_pkey_x25519();
  } else {
    OPENSSL_PUT_ERROR(EVP, EVP_R_UNSUPPORTED_ALGORITHM);
    ERR_add_error_dataf("algorithm %d", type);
    return 0;
  }

  if (impl) {
    evp_pkey_set0(impl, alg->method, nullptr);
  }

  return 1;
}

EVP_PKEY *EVP_PKEY_from_raw_private_key(const EVP_PKEY_ALG *alg,
                                        const uint8_t *in, size_t len) {
  if (alg->method->set_priv_raw == nullptr) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_UNSUPPORTED_ALGORITHM);
    return nullptr;
  }
  UniquePtr<EvpPkey> ret(FromOpaque(EVP_PKEY_new()));
  if (ret == nullptr || !alg->method->set_priv_raw(ret.get(), in, len)) {
    return nullptr;
  }
  return ret.release();
}

EVP_PKEY *EVP_PKEY_from_private_seed(const EVP_PKEY_ALG *alg, const uint8_t *in,
                                     size_t len) {
  if (alg->method->set_priv_seed == nullptr) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_UNSUPPORTED_ALGORITHM);
    return nullptr;
  }
  UniquePtr<EvpPkey> ret(FromOpaque(EVP_PKEY_new()));
  if (ret == nullptr || !alg->method->set_priv_seed(ret.get(), in, len)) {
    return nullptr;
  }
  return ret.release();
}

EVP_PKEY *EVP_PKEY_from_raw_public_key(const EVP_PKEY_ALG *alg,
                                       const uint8_t *in, size_t len) {
  if (alg->method->set_pub_raw == nullptr) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_UNSUPPORTED_ALGORITHM);
    return nullptr;
  }
  UniquePtr<EvpPkey> ret(FromOpaque(EVP_PKEY_new()));
  if (ret == nullptr || !alg->method->set_pub_raw(ret.get(), in, len)) {
    return nullptr;
  }
  return ret.release();
}

EVP_PKEY *EVP_PKEY_new_raw_private_key(int type, ENGINE *unused,
                                       const uint8_t *in, size_t len) {
  for (const EVP_PKEY_ALG *alg : GetDefaultEVPAlgorithms()) {
    if (alg->method && alg->method->pkey_id == type) {
      return EVP_PKEY_from_raw_private_key(alg, in, len);
    }
  }
  OPENSSL_PUT_ERROR(EVP, EVP_R_UNSUPPORTED_ALGORITHM);
  return nullptr;
}

EVP_PKEY *EVP_PKEY_new_raw_public_key(int type, ENGINE *unused,
                                      const uint8_t *in, size_t len) {
  for (const EVP_PKEY_ALG *alg : GetDefaultEVPAlgorithms()) {
    if (alg->method && alg->method->pkey_id == type) {
      return EVP_PKEY_from_raw_public_key(alg, in, len);
    }
  }
  OPENSSL_PUT_ERROR(EVP, EVP_R_UNSUPPORTED_ALGORITHM);
  return nullptr;
}

int EVP_PKEY_get_raw_private_key(const EVP_PKEY *pkey, uint8_t *out,
                                 size_t *out_len) {
  auto *impl = FromOpaque(pkey);

  if (impl->ameth == nullptr || impl->ameth->get_priv_raw == nullptr) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE);
    return 0;
  }

  return impl->ameth->get_priv_raw(impl, out, out_len);
}

int EVP_PKEY_get_private_seed(const EVP_PKEY *pkey, uint8_t *out,
                              size_t *out_len) {
  auto *impl = FromOpaque(pkey);

  if (impl->ameth == nullptr || impl->ameth->get_priv_seed == nullptr) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE);
    return 0;
  }

  return impl->ameth->get_priv_seed(impl, out, out_len);
}

int EVP_PKEY_get_raw_public_key(const EVP_PKEY *pkey, uint8_t *out,
                                size_t *out_len) {
  auto *impl = FromOpaque(pkey);

  if (impl->ameth == nullptr || impl->ameth->get_pub_raw == nullptr) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE);
    return 0;
  }

  return impl->ameth->get_pub_raw(impl, out, out_len);
}

int EVP_PKEY_parameters_eq(const EVP_PKEY *a, const EVP_PKEY *b) {
  if (EVP_PKEY_id(a) != EVP_PKEY_id(b)) {
    return 0;
  }

  auto *a_impl = FromOpaque(a);
  auto *b_impl = FromOpaque(b);
  if (a_impl->ameth && a_impl->ameth->param_equal) {
    return a_impl->ameth->param_equal(a_impl, b_impl);
  }
  // If the algorithm does not use parameters, the two null value compare as
  // vacuously equal.
  return 1;
}

int EVP_PKEY_cmp_parameters(const EVP_PKEY *a, const EVP_PKEY *b) {
  return EVP_PKEY_parameters_eq(a, b);
}

int EVP_PKEY_CTX_set_signature_md(EVP_PKEY_CTX *ctx, const EVP_MD *md) {
  return EVP_PKEY_CTX_ctrl(ctx, -1, EVP_PKEY_OP_TYPE_SIG, EVP_PKEY_CTRL_MD, 0,
                           (void *)md);
}

int EVP_PKEY_CTX_get_signature_md(EVP_PKEY_CTX *ctx, const EVP_MD **out_md) {
  return EVP_PKEY_CTX_ctrl(ctx, -1, EVP_PKEY_OP_TYPE_SIG, EVP_PKEY_CTRL_GET_MD,
                           0, (void *)out_md);
}

int EVP_PKEY_CTX_set1_signature_context_string(EVP_PKEY_CTX *ctx,
                                               const uint8_t *context,
                                               size_t context_len) {
  Span<const uint8_t> context_string(context, context_len);
  return EVP_PKEY_CTX_ctrl(ctx, -1, EVP_PKEY_OP_TYPE_SIG,
                           EVP_PKEY_CTRL_SIGNATURE_CONTEXT_STRING, 0,
                           &context_string);
}

void *EVP_PKEY_get0(const EVP_PKEY *pkey) {
  // Node references, but never calls this function, so for now we return NULL.
  // If other projects require complete support, call |EVP_PKEY_get0_RSA|, etc.,
  // rather than reading |pkey->pkey| directly. This avoids problems if our
  // internal representation does not match the type the caller expects from
  // OpenSSL.
  return nullptr;
}

void OpenSSL_add_all_algorithms() {}

void OPENSSL_add_all_algorithms_conf() {}

void OpenSSL_add_all_ciphers() {}

void OpenSSL_add_all_digests() {}

void EVP_cleanup() {}

int EVP_default_properties_is_fips_enabled(OSSL_LIB_CTX *libctx) {
  return FIPS_mode();
}

int EVP_PKEY_set1_tls_encodedpoint(EVP_PKEY *pkey, const uint8_t *in,
                                   size_t len) {
  auto *impl = FromOpaque(pkey);

  if (impl->ameth == nullptr || impl->ameth->set1_tls_encodedpoint == nullptr) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE);
    return 0;
  }

  return impl->ameth->set1_tls_encodedpoint(impl, in, len);
}

size_t EVP_PKEY_get1_tls_encodedpoint(const EVP_PKEY *pkey, uint8_t **out_ptr) {
  auto *impl = FromOpaque(pkey);

  if (impl->ameth == nullptr || impl->ameth->get1_tls_encodedpoint == nullptr) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE);
    return 0;
  }

  return impl->ameth->get1_tls_encodedpoint(impl, out_ptr);
}

int EVP_PKEY_base_id(const EVP_PKEY *pkey) {
  // OpenSSL has two notions of key type because it supports multiple OIDs for
  // the same algorithm: NID_rsa vs NID_rsaEncryption and five distinct spelling
  // of DSA. We do not support these, so the base ID is simply the ID.
  return EVP_PKEY_id(pkey);
}

int EVP_PKEY_has_public(const EVP_PKEY *pkey) {
  auto *impl = FromOpaque(pkey);
  if (impl == nullptr || impl->ameth == nullptr || impl->pkey == nullptr ||
      impl->ameth->pub_present == nullptr) {
    return 0;
  }
  return impl->ameth->pub_present(impl);
}

int EVP_PKEY_has_private(const EVP_PKEY *pkey) {
  auto *impl = FromOpaque(pkey);
  if (impl == nullptr || impl->ameth == nullptr || impl->pkey == nullptr ||
      impl->ameth->priv_present == nullptr) {
    return 0;
  }
  return impl->ameth->priv_present(impl);
}

EVP_PKEY *EVP_PKEY_copy_public(const EVP_PKEY *pkey) {
  auto *impl = FromOpaque(pkey);
  if (impl == nullptr || impl->ameth == nullptr || impl->pkey == nullptr ||
      impl->ameth->pub_copy == nullptr) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE);
    return nullptr;
  }
  UniquePtr<EvpPkey> ret(FromOpaque(EVP_PKEY_new()));
  if (ret == nullptr || !impl->ameth->pub_copy(ret.get(), impl)) {
    return nullptr;
  }
  return ret.release();
}
