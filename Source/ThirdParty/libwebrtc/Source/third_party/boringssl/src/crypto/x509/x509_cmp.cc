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

#include <string.h>

#include <openssl/asn1.h>
#include <openssl/digest.h>
#include <openssl/err.h>
#include <openssl/md5.h>
#include <openssl/mem.h>
#include <openssl/obj.h>
#include <openssl/sha.h>
#include <openssl/stack.h>
#include <openssl/x509.h>

#include "../internal.h"
#include "internal.h"


using namespace bssl;

int X509_issuer_name_cmp(const X509 *a, const X509 *b) {
  const auto *a_impl = FromOpaque(a);
  const auto *b_impl = FromOpaque(b);
  return X509_NAME_cmp(&a_impl->issuer, &b_impl->issuer);
}

int X509_subject_name_cmp(const X509 *a, const X509 *b) {
  const auto *a_impl = FromOpaque(a);
  const auto *b_impl = FromOpaque(b);
  return X509_NAME_cmp(&a_impl->subject, &b_impl->subject);
}

int X509_CRL_cmp(const X509_CRL *a, const X509_CRL *b) {
  return X509_NAME_cmp(a->crl->issuer, b->crl->issuer);
}

int X509_CRL_match(const X509_CRL *a, const X509_CRL *b) {
  return OPENSSL_memcmp(a->crl_hash, b->crl_hash, SHA256_DIGEST_LENGTH);
}

X509_NAME *X509_get_issuer_name(const X509 *a) {
  // This function is not const-correct for OpenSSL compatibility.
  const auto *impl = FromOpaque(a);
  return const_cast<X509Name *>(&impl->issuer);
}

uint32_t X509_issuer_name_hash(const X509 *x) {
  const auto *impl = FromOpaque(x);
  return X509_NAME_hash(&impl->issuer);
}

uint32_t X509_issuer_name_hash_old(const X509 *x) {
  const auto *impl = FromOpaque(x);
  return X509_NAME_hash_old(&impl->issuer);
}

X509_NAME *X509_get_subject_name(const X509 *a) {
  // This function is not const-correct for OpenSSL compatibility.
  const auto *impl = FromOpaque(a);
  return const_cast<X509Name *>(&impl->subject);
}

ASN1_INTEGER *X509_get_serialNumber(X509 *a) {
  auto *impl = FromOpaque(a);
  return &impl->serialNumber;
}

const ASN1_INTEGER *X509_get0_serialNumber(const X509 *x509) {
  const auto *impl = FromOpaque(x509);
  return &impl->serialNumber;
}

uint32_t X509_subject_name_hash(const X509 *x) {
  const auto *impl = FromOpaque(x);
  return X509_NAME_hash(&impl->subject);
}

uint32_t X509_subject_name_hash_old(const X509 *x) {
  const auto *impl = FromOpaque(x);
  return X509_NAME_hash_old(&impl->subject);
}

// Compare two certificates: they must be identical for this to work. NB:
// Although "cmp" operations are generally prototyped to take "const"
// arguments (eg. for use in STACKs), the way X509 handling is - these
// operations may involve ensuring the hashes are up-to-date and ensuring
// certain cert information is cached. So this is the point where the
// "depth-first" constification tree has to halt with an evil cast.
int X509_cmp(const X509 *a, const X509 *b) {
  const auto *a_impl = FromOpaque(a);
  const auto *b_impl = FromOpaque(b);
  // Fill in the |cert_hash| fields.
  //
  // TODO(davidben): This may fail, in which case the the hash will be all
  // zeros. This produces a consistent comparison (failures are sticky), but
  // not a good one. OpenSSL now returns -2, but this is not a consistent
  // comparison and may cause misbehaving sorts by transitivity. For now, we
  // retain the old OpenSSL behavior, which was to ignore the error. See
  // https://crbug.com/boringssl/355.
  x509v3_cache_extensions((X509Impl *)a_impl);
  x509v3_cache_extensions((X509Impl *)b_impl);

  return OPENSSL_memcmp(a_impl->cert_hash, b_impl->cert_hash,
                        SHA256_DIGEST_LENGTH);
}

int X509_NAME_cmp(const X509_NAME *a, const X509_NAME *b) {
  const X509_NAME_CACHE *a_cache = x509_name_get_cache(a);
  if (a_cache == nullptr) {
    return -2;
  }
  const X509_NAME_CACHE *b_cache = x509_name_get_cache(b);
  if (b_cache == nullptr) {
    return -2;
  }
  if (a_cache->canon.size() < b_cache->canon.size()) {
    return -1;
  }
  if (a_cache->canon.size() > b_cache->canon.size()) {
    return 1;
  }
  int ret = OPENSSL_memcmp(a_cache->canon.data(), b_cache->canon.data(),
                           a_cache->canon.size());
  // Canonicalize the return value so it is even possible to distinguish the
  // error case from a < b, though ideally we would not have an error case.
  if (ret < 0) {
    return -1;
  }
  if (ret > 0) {
    return 1;
  }
  return 0;
}

uint32_t X509_NAME_hash(const X509_NAME *x) {
  const X509_NAME_CACHE *cache = x509_name_get_cache(x);
  if (cache == nullptr) {
    return 0;
  }
  uint8_t md[SHA_DIGEST_LENGTH];
  SHA1(cache->canon.data(), cache->canon.size(), md);
  return CRYPTO_load_u32_le(md);
}

// I now DER encode the name and hash it.  Since I cache the DER encoding,
// this is reasonably efficient.

uint32_t X509_NAME_hash_old(const X509_NAME *x) {
  const X509_NAME_CACHE *cache = x509_name_get_cache(x);
  if (cache == nullptr) {
    return 0;
  }
  uint8_t md[MD5_DIGEST_LENGTH];
  MD5(cache->der.data(), cache->der.size(), md);
  return CRYPTO_load_u32_le(md);
}

X509 *X509_find_by_issuer_and_serial(const STACK_OF(X509) *sk,
                                     const X509_NAME *name,
                                     const ASN1_INTEGER *serial) {
  if (serial->type != V_ASN1_INTEGER && serial->type != V_ASN1_NEG_INTEGER) {
    return nullptr;
  }

  for (size_t i = 0; i < sk_X509_num(sk); i++) {
    X509 *x509 = sk_X509_value(sk, i);
    if (ASN1_INTEGER_cmp(X509_get0_serialNumber(x509), serial) == 0 &&
        X509_NAME_cmp(X509_get_issuer_name(x509), name) == 0) {
      return x509;
    }
  }
  return nullptr;
}

X509 *X509_find_by_subject(const STACK_OF(X509) *sk, const X509_NAME *name) {
  for (size_t i = 0; i < sk_X509_num(sk); i++) {
    X509 *x509 = sk_X509_value(sk, i);
    if (X509_NAME_cmp(X509_get_subject_name(x509), name) == 0) {
      return x509;
    }
  }
  return nullptr;
}

EVP_PKEY *X509_get0_pubkey(const X509 *x) {
  if (x == nullptr) {
    return nullptr;
  }
  auto *impl = FromOpaque(x);
  return X509_PUBKEY_get0(&impl->key);
}

EVP_PKEY *X509_get_pubkey(const X509 *x) {
  if (x == nullptr) {
    return nullptr;
  }
  auto *impl = FromOpaque(x);
  return X509_PUBKEY_get(&impl->key);
}

ASN1_BIT_STRING *X509_get0_pubkey_bitstr(const X509 *x) {
  if (!x) {
    return nullptr;
  }
  // This function is not const-correct for OpenSSL compatibility.
  auto *impl = FromOpaque(x);
  return const_cast<ASN1_BIT_STRING *>(&impl->key.public_key);
}

int X509_check_private_key(const X509 *x, const EVP_PKEY *k) {
  const EVP_PKEY *xk = X509_get0_pubkey(x);
  if (xk == nullptr) {
    return 0;
  }

  if (EVP_PKEY_eq(xk, k) == 1) {
    return 1;
  }

  if (EVP_PKEY_id(xk) != EVP_PKEY_id(k)) {
    OPENSSL_PUT_ERROR(X509, X509_R_KEY_TYPE_MISMATCH);
  } else {
    OPENSSL_PUT_ERROR(X509, X509_R_KEY_VALUES_MISMATCH);
  }
  return 0;
}

// Not strictly speaking an "up_ref" as a STACK doesn't have a reference
// count but it has the same effect by duping the STACK and upping the ref of
// each X509 structure.
STACK_OF(X509) *X509_chain_up_ref(STACK_OF(X509) *chain) {
  return sk_X509_deep_copy(chain, X509_dup_ref, X509_free);
}
