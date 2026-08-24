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
#include <string.h>

#include <openssl/asn1.h>
#include <openssl/asn1t.h>
#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/obj.h>
#include <openssl/x509.h>

#include "internal.h"


using namespace bssl;

static STACK_OF(CONF_VALUE) *i2v_AUTHORITY_KEYID(
    const X509V3_EXT_METHOD *method, void *ext, STACK_OF(CONF_VALUE) *extlist);
static void *v2i_AUTHORITY_KEYID(const X509V3_EXT_METHOD *method,
                                 const X509V3_CTX *ctx,
                                 const STACK_OF(CONF_VALUE) *values);

const X509V3_EXT_METHOD bssl::v3_akey_id = {
    NID_authority_key_identifier,
    X509V3_EXT_MULTILINE,
    ASN1_ITEM_ref(AUTHORITY_KEYID),
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    i2v_AUTHORITY_KEYID,
    v2i_AUTHORITY_KEYID,
    nullptr,
    nullptr,
    nullptr,
};

static STACK_OF(CONF_VALUE) *i2v_AUTHORITY_KEYID(
    const X509V3_EXT_METHOD *method, void *ext, STACK_OF(CONF_VALUE) *extlist) {
  const AUTHORITY_KEYID *akeyid =
      reinterpret_cast<const AUTHORITY_KEYID *>(ext);
  int extlist_was_null = extlist == nullptr;
  if (akeyid->keyid) {
    char *tmp = x509v3_bytes_to_hex(akeyid->keyid->data, akeyid->keyid->length);
    int ok = tmp != nullptr && X509V3_add_value("keyid", tmp, &extlist);
    OPENSSL_free(tmp);
    if (!ok) {
      goto err;
    }
  }
  if (akeyid->issuer) {
    STACK_OF(CONF_VALUE) *tmpextlist =
        i2v_GENERAL_NAMES(nullptr, akeyid->issuer, extlist);
    if (tmpextlist == nullptr) {
      goto err;
    }
    extlist = tmpextlist;
  }
  if (akeyid->serial) {
    if (!X509V3_add_value_int("serial", akeyid->serial, &extlist)) {
      goto err;
    }
  }
  return extlist;

err:
  if (extlist_was_null) {
    sk_CONF_VALUE_pop_free(extlist, X509V3_conf_free);
  }
  return nullptr;
}

// Currently two options:
//
// - keyid: Use the issuer's subject key ID. The value 'always' means it's an
//   error if the issuer certificate doesn't have one.
//
// - issuer: Use the issuer's issuer and serial number. The default is to only
//   use this if the key ID is not present. The value 'always' means it's always
//   included.
static void *v2i_AUTHORITY_KEYID(const X509V3_EXT_METHOD *method,
                                 const X509V3_CTX *ctx,
                                 const STACK_OF(CONF_VALUE) *values) {
  enum Option { kOff = 0, kOn = 1, kAlways = 2 };
  Option use_key_id = kOff, use_issuer = kOff;
  for (const CONF_VALUE *cnf : values) {
    if (!strcmp(cnf->name, "keyid")) {
      use_key_id = kOn;
      if (cnf->value && !strcmp(cnf->value, "always")) {
        use_key_id = kAlways;
      }
    } else if (!strcmp(cnf->name, "issuer")) {
      use_issuer = kOn;
      if (cnf->value && !strcmp(cnf->value, "always")) {
        use_issuer = kAlways;
      }
    } else {
      OPENSSL_PUT_ERROR(X509V3, X509V3_R_UNKNOWN_OPTION);
      ERR_add_error_data(2, "name=", cnf->name);
      return nullptr;
    }
  }

  if (!ctx || !ctx->issuer_cert) {
    if (ctx && (ctx->flags == X509V3_CTX_TEST)) {
      return AUTHORITY_KEYID_new();
    }
    OPENSSL_PUT_ERROR(X509V3, X509V3_R_NO_ISSUER_CERTIFICATE);
    return nullptr;
  }

  UniquePtr<ASN1_OCTET_STRING> key_id;
  if (use_key_id != kOff) {
    int critical;
    key_id.reset(static_cast<ASN1_OCTET_STRING *>(X509_get_ext_d2i(
        ctx->issuer_cert, NID_subject_key_identifier, &critical, nullptr)));
    if (key_id == nullptr && critical != -1) {
      return nullptr;  // Syntax error in the extension.
    }
    if (use_key_id == kAlways && key_id == nullptr) {
      OPENSSL_PUT_ERROR(X509V3, X509V3_R_UNABLE_TO_GET_ISSUER_KEYID);
      return nullptr;
    }
  }

  UniquePtr<ASN1_INTEGER> serial;
  UniquePtr<GENERAL_NAMES> issuer_gens;
  if ((use_issuer == kOn && key_id == nullptr) || use_issuer == kAlways) {
    UniquePtr<X509_NAME> issuer_name(
        X509_NAME_dup(X509_get_issuer_name(ctx->issuer_cert)));
    serial.reset(ASN1_INTEGER_dup(X509_get0_serialNumber(ctx->issuer_cert)));
    if (issuer_name == nullptr || serial == nullptr) {
      OPENSSL_PUT_ERROR(X509V3, X509V3_R_UNABLE_TO_GET_ISSUER_DETAILS);
      return nullptr;
    }
    // AKID wraps the issuer name in a GeneralNames structure.
    UniquePtr<GENERAL_NAME> gen(GENERAL_NAME_new());
    if (gen == nullptr) {
      return nullptr;
    }
    gen->type = GEN_DIRNAME;
    gen->d.directoryName = issuer_name.release();
    issuer_gens.reset(sk_GENERAL_NAME_new_null());
    if (issuer_gens == nullptr ||
        !PushToStack(issuer_gens.get(), std::move(gen))) {
      return nullptr;
    }
  }

  UniquePtr<AUTHORITY_KEYID> akid(AUTHORITY_KEYID_new());
  if (akid == nullptr) {
    return nullptr;
  }
  akid->issuer = issuer_gens.release();
  akid->serial = serial.release();
  akid->keyid = key_id.release();
  return akid.release();
}
