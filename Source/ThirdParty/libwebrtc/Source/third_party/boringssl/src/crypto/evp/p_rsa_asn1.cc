// Copyright 2006-2016 The OpenSSL Project Authors. All Rights Reserved.
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

#include <openssl/bn.h>
#include <openssl/bytestring.h>
#include <openssl/digest.h>
#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/rsa.h>
#include <openssl/span.h>

#include "../fipsmodule/rsa/internal.h"
#include "../rsa/internal.h"
#include "internal.h"


static int rsa_pub_encode(CBB *out, const EVP_PKEY *key) {
  // See RFC 3279, section 2.3.1.
  const RSA *rsa = reinterpret_cast<const RSA *>(key->pkey);
  CBB spki, algorithm, null, key_bitstring;
  if (!CBB_add_asn1(out, &spki, CBS_ASN1_SEQUENCE) ||
      !CBB_add_asn1(&spki, &algorithm, CBS_ASN1_SEQUENCE) ||
      !CBB_add_asn1_element(&algorithm, CBS_ASN1_OBJECT, rsa_asn1_meth.oid,
                            rsa_asn1_meth.oid_len) ||
      !CBB_add_asn1(&algorithm, &null, CBS_ASN1_NULL) ||
      !CBB_add_asn1(&spki, &key_bitstring, CBS_ASN1_BITSTRING) ||
      !CBB_add_u8(&key_bitstring, 0 /* padding */) ||
      !RSA_marshal_public_key(&key_bitstring, rsa) ||  //
      !CBB_flush(out)) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_ENCODE_ERROR);
    return 0;
  }

  return 1;
}

static evp_decode_result_t rsa_pub_decode(const EVP_PKEY_ALG *alg,
                                          EVP_PKEY *out, CBS *params,
                                          CBS *key) {
  // See RFC 3279, section 2.3.1.

  // The parameters must be NULL.
  CBS null;
  if (!CBS_get_asn1(params, &null, CBS_ASN1_NULL) || CBS_len(&null) != 0 ||
      CBS_len(params) != 0) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_DECODE_ERROR);
    return evp_decode_error;
  }

  bssl::UniquePtr<RSA> rsa(
      RSA_public_key_from_bytes(CBS_data(key), CBS_len(key)));
  if (rsa == nullptr) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_DECODE_ERROR);
    return evp_decode_error;
  }

  EVP_PKEY_assign_RSA(out, rsa.release());
  return evp_decode_ok;
}

static int rsa_pub_cmp(const EVP_PKEY *a, const EVP_PKEY *b) {
  // We currently assume that all |EVP_PKEY_RSA_PSS| keys have the same
  // parameters, so this vacuously compares parameters. If we ever support
  // multiple PSS parameter sets, we probably should compare them too. Note,
  // however, that OpenSSL does not compare parameters here.
  const RSA *a_rsa = reinterpret_cast<const RSA *>(a->pkey);
  const RSA *b_rsa = reinterpret_cast<const RSA *>(b->pkey);
  return BN_cmp(RSA_get0_n(b_rsa), RSA_get0_n(a_rsa)) == 0 &&
         BN_cmp(RSA_get0_e(b_rsa), RSA_get0_e(a_rsa)) == 0;
}

static int rsa_priv_encode(CBB *out, const EVP_PKEY *key) {
  const RSA *rsa = reinterpret_cast<const RSA *>(key->pkey);
  CBB pkcs8, algorithm, null, private_key;
  if (!CBB_add_asn1(out, &pkcs8, CBS_ASN1_SEQUENCE) ||
      !CBB_add_asn1_uint64(&pkcs8, 0 /* version */) ||
      !CBB_add_asn1(&pkcs8, &algorithm, CBS_ASN1_SEQUENCE) ||
      !CBB_add_asn1_element(&algorithm, CBS_ASN1_OBJECT, rsa_asn1_meth.oid,
                            rsa_asn1_meth.oid_len) ||
      !CBB_add_asn1(&algorithm, &null, CBS_ASN1_NULL) ||
      !CBB_add_asn1(&pkcs8, &private_key, CBS_ASN1_OCTETSTRING) ||
      !RSA_marshal_private_key(&private_key, rsa) ||  //
      !CBB_flush(out)) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_ENCODE_ERROR);
    return 0;
  }

  return 1;
}

static evp_decode_result_t rsa_priv_decode(const EVP_PKEY_ALG *alg,
                                           EVP_PKEY *out, CBS *params,
                                           CBS *key) {
  // Per RFC 8017, A.1, the parameters have type NULL.
  CBS null;
  if (!CBS_get_asn1(params, &null, CBS_ASN1_NULL) || CBS_len(&null) != 0 ||
      CBS_len(params) != 0) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_DECODE_ERROR);
    return evp_decode_error;
  }

  bssl::UniquePtr<RSA> rsa(
      RSA_private_key_from_bytes(CBS_data(key), CBS_len(key)));
  if (rsa == nullptr) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_DECODE_ERROR);
    return evp_decode_error;
  }

  EVP_PKEY_assign_RSA(out, rsa.release());
  return evp_decode_ok;
}

static evp_decode_result_t rsa_decode_pss_params_sha256(CBS *params) {
  // For now, we only support the SHA-256 parameter set. If we want to support
  // more, we'll need to record a little more state in the |EVP_PKEY|.
  if (CBS_len(params) == 0) {
    return evp_decode_unsupported;
  }
  rsa_pss_params_t pss_params;
  if (!rsa_parse_pss_params(params, &pss_params,
                            /*allow_explicit_trailer=*/false) ||
      CBS_len(params) != 0) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_DECODE_ERROR);
    return evp_decode_error;
  }
  return pss_params == rsa_pss_sha256 ? evp_decode_ok : evp_decode_unsupported;
}

static int rsa_pub_encode_pss_sha256(CBB *out, const EVP_PKEY *key) {
  const RSA *rsa = reinterpret_cast<const RSA *>(key->pkey);
  CBB spki, algorithm, key_bitstring;
  if (!CBB_add_asn1(out, &spki, CBS_ASN1_SEQUENCE) ||
      !CBB_add_asn1(&spki, &algorithm, CBS_ASN1_SEQUENCE) ||
      !CBB_add_asn1_element(&algorithm, CBS_ASN1_OBJECT,
                            rsa_pss_sha256_asn1_meth.oid,
                            rsa_pss_sha256_asn1_meth.oid_len) ||
      !rsa_marshal_pss_params(&algorithm, rsa_pss_sha256) ||
      !CBB_add_asn1(&spki, &key_bitstring, CBS_ASN1_BITSTRING) ||
      !CBB_add_u8(&key_bitstring, 0 /* padding */) ||
      !RSA_marshal_public_key(&key_bitstring, rsa) ||  //
      !CBB_flush(out)) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_ENCODE_ERROR);
    return 0;
  }

  return 1;
}

static evp_decode_result_t rsa_pub_decode_pss_sha256(const EVP_PKEY_ALG *alg,
                                                     EVP_PKEY *out, CBS *params,
                                                     CBS *key) {
  evp_decode_result_t ret = rsa_decode_pss_params_sha256(params);
  if (ret != evp_decode_ok) {
    return ret;
  }

  bssl::UniquePtr<RSA> rsa(
      RSA_public_key_from_bytes(CBS_data(key), CBS_len(key)));
  if (rsa == nullptr) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_DECODE_ERROR);
    return evp_decode_error;
  }

  evp_pkey_set0(out, &rsa_pss_sha256_asn1_meth, rsa.release());
  return evp_decode_ok;
}

static int rsa_priv_encode_pss_sha256(CBB *out, const EVP_PKEY *key) {
  const RSA *rsa = reinterpret_cast<const RSA *>(key->pkey);
  CBB pkcs8, algorithm, private_key;
  if (!CBB_add_asn1(out, &pkcs8, CBS_ASN1_SEQUENCE) ||
      !CBB_add_asn1_uint64(&pkcs8, 0 /* version */) ||
      !CBB_add_asn1(&pkcs8, &algorithm, CBS_ASN1_SEQUENCE) ||
      !CBB_add_asn1_element(&algorithm, CBS_ASN1_OBJECT,
                            rsa_pss_sha256_asn1_meth.oid,
                            rsa_pss_sha256_asn1_meth.oid_len) ||
      !rsa_marshal_pss_params(&algorithm, rsa_pss_sha256) ||
      !CBB_add_asn1(&pkcs8, &private_key, CBS_ASN1_OCTETSTRING) ||
      !RSA_marshal_private_key(&private_key, rsa) ||  //
      !CBB_flush(out)) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_ENCODE_ERROR);
    return 0;
  }

  return 1;
}

static evp_decode_result_t rsa_priv_decode_pss_sha256(const EVP_PKEY_ALG *alg,
                                                      EVP_PKEY *out,
                                                      CBS *params, CBS *key) {
  evp_decode_result_t ret = rsa_decode_pss_params_sha256(params);
  if (ret != evp_decode_ok) {
    return ret;
  }

  bssl::UniquePtr<RSA> rsa(
      RSA_private_key_from_bytes(CBS_data(key), CBS_len(key)));
  if (rsa == nullptr) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_DECODE_ERROR);
    return evp_decode_error;
  }

  evp_pkey_set0(out, &rsa_pss_sha256_asn1_meth, rsa.release());
  return evp_decode_ok;
}

static int rsa_opaque(const EVP_PKEY *pkey) {
  const RSA *rsa = reinterpret_cast<const RSA *>(pkey->pkey);
  return RSA_is_opaque(rsa);
}

static int int_rsa_size(const EVP_PKEY *pkey) {
  const RSA *rsa = reinterpret_cast<const RSA *>(pkey->pkey);
  return RSA_size(rsa);
}

static int rsa_bits(const EVP_PKEY *pkey) {
  const RSA *rsa = reinterpret_cast<const RSA *>(pkey->pkey);
  return RSA_bits(rsa);
}

static void int_rsa_free(EVP_PKEY *pkey) {
  RSA_free(reinterpret_cast<RSA *>(pkey->pkey));
  pkey->pkey = NULL;
}

const EVP_PKEY_ASN1_METHOD rsa_asn1_meth = {
    EVP_PKEY_RSA,
    // 1.2.840.113549.1.1.1
    {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01},
    9,

    &rsa_pkey_meth,

    rsa_pub_decode,
    rsa_pub_encode,
    rsa_pub_cmp,

    rsa_priv_decode,
    rsa_priv_encode,

    /*set_priv_raw=*/nullptr,
    /*set_pub_raw=*/nullptr,
    /*get_priv_raw=*/nullptr,
    /*get_pub_raw=*/nullptr,
    /*set1_tls_encodedpoint=*/nullptr,
    /*get1_tls_encodedpoint=*/nullptr,

    rsa_opaque,

    int_rsa_size,
    rsa_bits,

    /*param_missing=*/nullptr,
    /*param_copy=*/nullptr,
    /*param_cmp=*/nullptr,

    int_rsa_free,
};

const EVP_PKEY_ASN1_METHOD rsa_pss_sha256_asn1_meth = {
    EVP_PKEY_RSA_PSS,
    // 1.2.840.113549.1.1.10
    {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0a},
    9,

    &rsa_pss_sha256_pkey_meth,

    rsa_pub_decode_pss_sha256,
    rsa_pub_encode_pss_sha256,
    rsa_pub_cmp,

    rsa_priv_decode_pss_sha256,
    rsa_priv_encode_pss_sha256,

    /*set_priv_raw=*/nullptr,
    /*set_pub_raw=*/nullptr,
    /*get_priv_raw=*/nullptr,
    /*get_pub_raw=*/nullptr,
    /*set1_tls_encodedpoint=*/nullptr,
    /*get1_tls_encodedpoint=*/nullptr,

    rsa_opaque,

    int_rsa_size,
    rsa_bits,

    /*param_missing=*/nullptr,
    /*param_copy=*/nullptr,
    /*param_cmp=*/nullptr,

    int_rsa_free,
};


const EVP_PKEY_ALG *EVP_pkey_rsa(void) {
  static const EVP_PKEY_ALG kAlg = {
      /*method=*/&rsa_asn1_meth,
      /*ec_group=*/nullptr,
  };
  return &kAlg;
}

const EVP_PKEY_ALG *EVP_pkey_rsa_pss_sha256(void) {
  static const EVP_PKEY_ALG kAlg = {
      /*method=*/&rsa_pss_sha256_asn1_meth,
      /*ec_group=*/nullptr,
  };
  return &kAlg;
}

int EVP_PKEY_set1_RSA(EVP_PKEY *pkey, RSA *key) {
  if (EVP_PKEY_assign_RSA(pkey, key)) {
    RSA_up_ref(key);
    return 1;
  }
  return 0;
}

int EVP_PKEY_assign_RSA(EVP_PKEY *pkey, RSA *key) {
  if (key == nullptr) {
    return 0;
  }
  evp_pkey_set0(pkey, &rsa_asn1_meth, key);
  return 1;
}

RSA *EVP_PKEY_get0_RSA(const EVP_PKEY *pkey) {
  int pkey_id = EVP_PKEY_id(pkey);
  if (pkey_id != EVP_PKEY_RSA && pkey_id != EVP_PKEY_RSA_PSS) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_EXPECTING_AN_RSA_KEY);
    return NULL;
  }
  return reinterpret_cast<RSA *>(pkey->pkey);
}

RSA *EVP_PKEY_get1_RSA(const EVP_PKEY *pkey) {
  RSA *rsa = EVP_PKEY_get0_RSA(pkey);
  if (rsa != NULL) {
    RSA_up_ref(rsa);
  }
  return rsa;
}
