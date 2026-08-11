// Copyright 1999-2016 The OpenSSL Project Authors. All Rights Reserved.
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

#include <stdio.h>

#include <openssl/asn1.h>
#include <openssl/asn1t.h>
#include <openssl/evp.h>
#include <openssl/obj.h>
#include <openssl/x509.h>

#include "../internal.h"
#include "internal.h"


// X509_CERT_AUX routines. These are used to encode additional user
// modifiable data about a certificate. This data is appended to the X509
// encoding when the *_X509_AUX routines are used. This means that the
// "traditional" X509 routines will simply ignore the extra data.

using namespace bssl;

BSSL_NAMESPACE_BEGIN

ASN1_SEQUENCE(X509_CERT_AUX) = {
    ASN1_SEQUENCE_OF_OPT(X509_CERT_AUX, trust, ASN1_OBJECT),
    ASN1_IMP_SEQUENCE_OF_OPT(X509_CERT_AUX, reject, ASN1_OBJECT, 0),
    ASN1_OPT(X509_CERT_AUX, alias, ASN1_UTF8STRING),
    ASN1_OPT(X509_CERT_AUX, keyid, ASN1_OCTET_STRING),
} ASN1_SEQUENCE_END(X509_CERT_AUX)

IMPLEMENT_ASN1_FUNCTIONS_const(X509_CERT_AUX)

BSSL_NAMESPACE_END

static X509_CERT_AUX *aux_get(X509Impl *x) {
  if (!x) {
    return nullptr;
  }
  if (!x->aux && !(x->aux = X509_CERT_AUX_new())) {
    return nullptr;
  }
  return x->aux;
}

int X509_alias_set1(X509 *x, const uint8_t *name, ossl_ssize_t len) {
  auto *impl = FromOpaque(x);
  X509_CERT_AUX *aux;
  // TODO(davidben): Empty aliases are not meaningful in PKCS#12, and the
  // getters cannot quite represent them. Also erase the object if |len| is
  // zero.
  if (!name) {
    if (!impl || !impl->aux || !impl->aux->alias) {
      return 1;
    }
    ASN1_UTF8STRING_free(impl->aux->alias);
    impl->aux->alias = nullptr;
    return 1;
  }
  if (!(aux = aux_get(impl))) {
    return 0;
  }
  if (!aux->alias && !(aux->alias = ASN1_UTF8STRING_new())) {
    return 0;
  }
  return ASN1_STRING_set(aux->alias, name, len);
}

int X509_keyid_set1(X509 *x, const uint8_t *id, ossl_ssize_t len) {
  auto *impl = FromOpaque(x);
  X509_CERT_AUX *aux;
  // TODO(davidben): Empty key IDs are not meaningful in PKCS#12, and the
  // getters cannot quite represent them. Also erase the object if |len| is
  // zero.
  if (!id) {
    if (!impl || !impl->aux || !impl->aux->keyid) {
      return 1;
    }
    ASN1_OCTET_STRING_free(impl->aux->keyid);
    impl->aux->keyid = nullptr;
    return 1;
  }
  if (!(aux = aux_get(impl))) {
    return 0;
  }
  if (!aux->keyid && !(aux->keyid = ASN1_OCTET_STRING_new())) {
    return 0;
  }
  return ASN1_STRING_set(aux->keyid, id, len);
}

const uint8_t *X509_alias_get0(const X509 *x, int *out_len) {
  auto *impl = FromOpaque(x);
  const ASN1_UTF8STRING *alias =
      impl->aux != nullptr ? impl->aux->alias : nullptr;
  if (out_len != nullptr) {
    *out_len = alias != nullptr ? alias->length : 0;
  }
  return alias != nullptr ? alias->data : nullptr;
}

const uint8_t *X509_keyid_get0(const X509 *x, int *out_len) {
  auto *impl = FromOpaque(x);
  const ASN1_OCTET_STRING *keyid =
      impl->aux != nullptr ? impl->aux->keyid : nullptr;
  if (out_len != nullptr) {
    *out_len = keyid != nullptr ? keyid->length : 0;
  }
  return keyid != nullptr ? keyid->data : nullptr;
}

int X509_add1_trust_object(X509 *x, const ASN1_OBJECT *obj) {
  auto *impl = FromOpaque(x);
  UniquePtr<ASN1_OBJECT> objtmp(OBJ_dup(obj));
  if (objtmp == nullptr) {
    return 0;
  }
  X509_CERT_AUX *aux = aux_get(impl);
  if (aux->trust == nullptr) {
    aux->trust = sk_ASN1_OBJECT_new_null();
    if (aux->trust == nullptr) {
      return 0;
    }
  }
  return PushToStack(aux->trust, std::move(objtmp));
}

int X509_add1_reject_object(X509 *x, const ASN1_OBJECT *obj) {
  auto *impl = FromOpaque(x);
  UniquePtr<ASN1_OBJECT> objtmp(OBJ_dup(obj));
  if (objtmp == nullptr) {
    return 0;
  }
  X509_CERT_AUX *aux = aux_get(impl);
  if (aux->reject == nullptr) {
    aux->reject = sk_ASN1_OBJECT_new_null();
    if (aux->reject == nullptr) {
      return 0;
    }
  }
  return PushToStack(aux->reject, std::move(objtmp));
}

void X509_trust_clear(X509 *x) {
  auto *impl = FromOpaque(x);
  if (impl->aux && impl->aux->trust) {
    sk_ASN1_OBJECT_pop_free(impl->aux->trust, ASN1_OBJECT_free);
    impl->aux->trust = nullptr;
  }
}

void X509_reject_clear(X509 *x) {
  auto *impl = FromOpaque(x);
  if (impl->aux && impl->aux->reject) {
    sk_ASN1_OBJECT_pop_free(impl->aux->reject, ASN1_OBJECT_free);
    impl->aux->reject = nullptr;
  }
}
