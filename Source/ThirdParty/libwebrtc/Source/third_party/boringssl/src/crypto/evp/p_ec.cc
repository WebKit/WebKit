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

#include <string.h>

#include <openssl/bn.h>
#include <openssl/bytestring.h>
#include <openssl/digest.h>
#include <openssl/ec.h>
#include <openssl/ec_key.h>
#include <openssl/ecdh.h>
#include <openssl/ecdsa.h>
#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/nid.h>
#include <openssl/span.h>

#include "../ec/internal.h"
#include "../fipsmodule/ec/internal.h"
#include "../internal.h"
#include "../mem_internal.h"
#include "internal.h"


using namespace bssl;

namespace {

struct EVP_PKEY_ALG_EC : public EVP_PKEY_ALG {
  // ec_group returns the |EC_GROUP| for this algorithm.
  const EC_GROUP *(*ec_group)();
};

extern const EVP_PKEY_ASN1_METHOD ec_asn1_meth;
extern const EVP_PKEY_CTX_METHOD ec_pkey_meth;

static int eckey_pub_encode(CBB *out, const EvpPkey *key) {
  const EC_KEY *ec_key = reinterpret_cast<const EC_KEY *>(key->pkey);
  const EC_GROUP *group = EC_KEY_get0_group(ec_key);
  const EC_POINT *public_key = EC_KEY_get0_public_key(ec_key);

  // See RFC 5480, section 2.
  CBB spki, algorithm, key_bitstring;
  if (!CBB_add_asn1(out, &spki, CBS_ASN1_SEQUENCE) ||
      !CBB_add_asn1(&spki, &algorithm, CBS_ASN1_SEQUENCE) ||
      !CBB_add_asn1_element(&algorithm, CBS_ASN1_OBJECT, ec_asn1_meth.oid,
                            ec_asn1_meth.oid_len) ||
      !EC_KEY_marshal_curve_name(&algorithm, group) ||
      !CBB_add_asn1(&spki, &key_bitstring, CBS_ASN1_BITSTRING) ||
      !CBB_add_u8(&key_bitstring, 0 /* padding */) ||
      !EC_POINT_point2cbb(&key_bitstring, group, public_key,
                          POINT_CONVERSION_UNCOMPRESSED, nullptr) ||
      !CBB_flush(out)) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_ENCODE_ERROR);
    return 0;
  }

  return 1;
}

static bssl::evp_decode_result_t eckey_pub_decode(const EVP_PKEY_ALG *alg,
                                                  EvpPkey *out, CBS *params,
                                                  CBS *key) {
  const auto *ec_alg = static_cast<const EVP_PKEY_ALG_EC *>(alg);
  if (ec_alg->ec_group == nullptr) {
    return evp_decode_unsupported;
  }

  // See RFC 5480, section 2.

  // Check that |params| matches |alg|. Only the namedCurve form is allowed.
  const EC_GROUP *group = ec_alg->ec_group();
  if (ec_key_parse_curve_name(params, Span(&group, 1)) == nullptr) {
    if (ERR_equals(ERR_peek_last_error(), ERR_LIB_EC, EC_R_UNKNOWN_GROUP)) {
      ERR_clear_error();
      return evp_decode_unsupported;
    }
    OPENSSL_PUT_ERROR(EVP, EVP_R_DECODE_ERROR);
    return evp_decode_error;
  }
  if (CBS_len(params) != 0) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_DECODE_ERROR);
    return evp_decode_error;
  }

  UniquePtr<EC_KEY> eckey(EC_KEY_new());
  if (eckey == nullptr ||  //
      !EC_KEY_set_group(eckey.get(), group) ||
      !EC_KEY_oct2key(eckey.get(), CBS_data(key), CBS_len(key), nullptr)) {
    return evp_decode_error;
  }

  EVP_PKEY_assign_EC_KEY(out, eckey.release());
  return evp_decode_ok;
}

static bool eckey_pub_equal(const EvpPkey *a, const EvpPkey *b) {
  const EC_KEY *a_ec = reinterpret_cast<const EC_KEY *>(a->pkey);
  const EC_KEY *b_ec = reinterpret_cast<const EC_KEY *>(b->pkey);
  const EC_GROUP *group = EC_KEY_get0_group(b_ec);
  const EC_POINT *pa = EC_KEY_get0_public_key(a_ec),
                 *pb = EC_KEY_get0_public_key(b_ec);
  return EC_POINT_cmp(group, pa, pb, nullptr) == 0;
}

static bssl::evp_decode_result_t eckey_priv_decode(const EVP_PKEY_ALG *alg,
                                                   EvpPkey *out, CBS *params,
                                                   CBS *key) {
  const auto *ec_alg = static_cast<const EVP_PKEY_ALG_EC *>(alg);
  if (ec_alg->ec_group == nullptr) {
    return evp_decode_unsupported;
  }

  // See RFC 5915.
  const EC_GROUP *group = ec_alg->ec_group();
  if (ec_key_parse_parameters(params, Span(&group, 1)) == nullptr) {
    if (ERR_equals(ERR_peek_last_error(), ERR_LIB_EC, EC_R_UNKNOWN_GROUP)) {
      ERR_clear_error();
      return evp_decode_unsupported;
    }
    OPENSSL_PUT_ERROR(EVP, EVP_R_DECODE_ERROR);
    return evp_decode_error;
  }
  if (CBS_len(params) != 0) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_DECODE_ERROR);
    return evp_decode_error;
  }

  UniquePtr<EC_KEY> ec_key(ec_key_parse_private_key(key, group, {}));
  if (ec_key == nullptr || CBS_len(key) != 0) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_DECODE_ERROR);
    return evp_decode_error;
  }

  EVP_PKEY_assign_EC_KEY(out, ec_key.release());
  return evp_decode_ok;
}

static int eckey_priv_encode(CBB *out, const EvpPkey *key) {
  const EC_KEY *ec_key = reinterpret_cast<const EC_KEY *>(key->pkey);

  // Omit the redundant copy of the curve name. This contradicts RFC 5915 but
  // aligns with PKCS #11. SEC 1 only says they may be omitted if known by other
  // means. Both OpenSSL and NSS omit the redundant parameters, so we omit them
  // as well.
  unsigned enc_flags = EC_KEY_get_enc_flags(ec_key) | EC_PKEY_NO_PARAMETERS;

  // See RFC 5915.
  CBB pkcs8, algorithm, private_key;
  if (!CBB_add_asn1(out, &pkcs8, CBS_ASN1_SEQUENCE) ||
      !CBB_add_asn1_uint64(&pkcs8, 0 /* version */) ||
      !CBB_add_asn1(&pkcs8, &algorithm, CBS_ASN1_SEQUENCE) ||
      !CBB_add_asn1_element(&algorithm, CBS_ASN1_OBJECT, ec_asn1_meth.oid,
                            ec_asn1_meth.oid_len) ||
      !EC_KEY_marshal_curve_name(&algorithm, EC_KEY_get0_group(ec_key)) ||
      !CBB_add_asn1(&pkcs8, &private_key, CBS_ASN1_OCTETSTRING) ||
      !EC_KEY_marshal_private_key(&private_key, ec_key, enc_flags) ||
      !CBB_flush(out)) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_ENCODE_ERROR);
    return 0;
  }

  return 1;
}

static int eckey_set1_tls_encodedpoint(EvpPkey *pkey, const uint8_t *in,
                                       size_t len) {
  EC_KEY *ec_key = reinterpret_cast<EC_KEY *>(pkey->pkey);
  if (ec_key == nullptr) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_NO_KEY_SET);
    return 0;
  }

  return EC_KEY_oct2key(ec_key, in, len, nullptr);
}

static size_t eckey_get1_tls_encodedpoint(const EvpPkey *pkey,
                                          uint8_t **out_ptr) {
  const EC_KEY *ec_key = reinterpret_cast<const EC_KEY *>(pkey->pkey);
  if (ec_key == nullptr) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_NO_KEY_SET);
    return 0;
  }

  return EC_KEY_key2buf(ec_key, POINT_CONVERSION_UNCOMPRESSED, out_ptr,
                        nullptr);
}

static int int_ec_size(const EvpPkey *pkey) {
  const EC_KEY *ec_key = reinterpret_cast<const EC_KEY *>(pkey->pkey);
  return ECDSA_size(ec_key);
}

static int ec_bits(const EvpPkey *pkey) {
  const EC_KEY *ec_key = reinterpret_cast<const EC_KEY *>(pkey->pkey);
  const EC_GROUP *group = EC_KEY_get0_group(ec_key);
  if (group == nullptr) {
    ERR_clear_error();
    return 0;
  }
  return EC_GROUP_order_bits(group);
}

static int ec_missing_parameters(const EvpPkey *pkey) {
  const EC_KEY *ec_key = reinterpret_cast<const EC_KEY *>(pkey->pkey);
  return ec_key == nullptr || EC_KEY_get0_group(ec_key) == nullptr;
}

static int ec_copy_parameters(EvpPkey *to, const EvpPkey *from) {
  const EC_KEY *from_key = reinterpret_cast<const EC_KEY *>(from->pkey);
  if (from_key == nullptr) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_NO_KEY_SET);
    return 0;
  }
  const EC_GROUP *group = EC_KEY_get0_group(from_key);
  if (group == nullptr) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_MISSING_PARAMETERS);
    return 0;
  }
  if (to->pkey == nullptr) {
    to->pkey = EC_KEY_new();
    if (to->pkey == nullptr) {
      return 0;
    }
  }
  return EC_KEY_set_group(reinterpret_cast<EC_KEY *>(to->pkey), group);
}

static bool ec_equal_parameters(const EvpPkey *a, const EvpPkey *b) {
  const EC_KEY *a_ec = reinterpret_cast<const EC_KEY *>(a->pkey);
  const EC_KEY *b_ec = reinterpret_cast<const EC_KEY *>(b->pkey);
  if (a_ec == nullptr || b_ec == nullptr) {
    return false;
  }
  const EC_GROUP *group_a = EC_KEY_get0_group(a_ec),
                 *group_b = EC_KEY_get0_group(b_ec);
  if (group_a == nullptr || group_b == nullptr) {
    return false;
  }
  // EC_GROUP_cmp returns zero on equality.
  return EC_GROUP_cmp(group_a, group_b, nullptr) == 0;
}

static void int_ec_free(EvpPkey *pkey) {
  EC_KEY_free(reinterpret_cast<EC_KEY *>(pkey->pkey));
  pkey->pkey = nullptr;
}

static int eckey_opaque(const EvpPkey *pkey) {
  const EC_KEY *ec_key = reinterpret_cast<const EC_KEY *>(pkey->pkey);
  return EC_KEY_is_opaque(ec_key);
}

static bool eckey_pub_present(const EvpPkey *pkey) {
  const EC_KEY *ec_key = reinterpret_cast<const EC_KEY *>(pkey->pkey);
  return EC_KEY_get0_public_key(ec_key) != nullptr;
}

static bool eckey_pub_copy(EvpPkey *out, const EvpPkey *pkey) {
  const EC_KEY *ec_key = reinterpret_cast<const EC_KEY *>(pkey->pkey);
  const EC_POINT *public_key = EC_KEY_get0_public_key(ec_key);
  if (public_key == nullptr) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_MISSING_PUBLIC_KEY);
    return false;
  }
  UniquePtr<EC_KEY> public_copy_ec_key(EC_KEY_new());
  if (public_copy_ec_key == nullptr ||
      !EC_KEY_set_group(public_copy_ec_key.get(), public_key->group) ||
      !EC_KEY_set_public_key(public_copy_ec_key.get(), public_key)) {
    OPENSSL_PUT_ERROR(EVP, ERR_R_INTERNAL_ERROR);
    return false;
  }
  evp_pkey_set0(out, pkey->ameth, public_copy_ec_key.release());
  return true;
}

static bool eckey_priv_present(const EvpPkey *pkey) {
  const EC_KEY *ec_key = reinterpret_cast<const EC_KEY *>(pkey->pkey);
  return EC_KEY_get0_private_key(ec_key) != nullptr;
}

const EVP_PKEY_ASN1_METHOD ec_asn1_meth = {
    EVP_PKEY_EC,
    // 1.2.840.10045.2.1
    {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01},
    7,

    &ec_pkey_meth,

    eckey_pub_decode,
    eckey_pub_encode,
    eckey_pub_equal,
    eckey_pub_present,
    eckey_pub_copy,

    eckey_priv_decode,
    eckey_priv_encode,
    eckey_priv_present,

    /*set_priv_raw=*/nullptr,
    /*set_priv_seed=*/nullptr,
    /*set_pub_raw=*/nullptr,
    /*get_priv_raw=*/nullptr,
    /*get_priv_seed=*/nullptr,
    /*get_pub_raw=*/nullptr,
    eckey_set1_tls_encodedpoint,
    eckey_get1_tls_encodedpoint,

    eckey_opaque,

    int_ec_size,
    ec_bits,

    ec_missing_parameters,
    ec_copy_parameters,
    ec_equal_parameters,

    int_ec_free,
};

struct EC_PKEY_CTX {
  // message digest
  const EVP_MD *md = nullptr;
  const EC_GROUP *gen_group = nullptr;
};

static int pkey_ec_init(EvpPkeyCtx *ctx, const EVP_PKEY_ALG *alg) {
  EC_PKEY_CTX *dctx = New<EC_PKEY_CTX>();
  if (!dctx) {
    return 0;
  }

  const auto *ec_alg = static_cast<const EVP_PKEY_ALG_EC *>(alg);
  if (ec_alg != nullptr && ec_alg->ec_group != nullptr) {
    dctx->gen_group = ec_alg->ec_group();
  }

  ctx->data = dctx;
  return 1;
}

static int pkey_ec_copy(EvpPkeyCtx *dst, EvpPkeyCtx *src) {
  if (!pkey_ec_init(dst, nullptr)) {
    return 0;
  }

  const EC_PKEY_CTX *sctx = reinterpret_cast<EC_PKEY_CTX *>(src->data);
  EC_PKEY_CTX *dctx = reinterpret_cast<EC_PKEY_CTX *>(dst->data);
  dctx->md = sctx->md;
  dctx->gen_group = sctx->gen_group;
  return 1;
}

static void pkey_ec_cleanup(EvpPkeyCtx *ctx) {
  Delete(reinterpret_cast<EC_PKEY_CTX *>(ctx->data));
}

static int pkey_ec_sign(EvpPkeyCtx *ctx, uint8_t *sig, size_t *siglen,
                        const uint8_t *tbs, size_t tbslen) {
  const EC_KEY *ec = reinterpret_cast<EC_KEY *>(ctx->pkey->pkey);
  if (!sig) {
    *siglen = ECDSA_size(ec);
    return 1;
  } else if (*siglen < (size_t)ECDSA_size(ec)) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_BUFFER_TOO_SMALL);
    return 0;
  }

  unsigned int sltmp;
  if (!ECDSA_sign(0, tbs, tbslen, sig, &sltmp, ec)) {
    return 0;
  }
  *siglen = (size_t)sltmp;
  return 1;
}

static int pkey_ec_verify(EvpPkeyCtx *ctx, const uint8_t *sig, size_t siglen,
                          const uint8_t *tbs, size_t tbslen) {
  const EC_KEY *ec_key = reinterpret_cast<EC_KEY *>(ctx->pkey->pkey);
  return ECDSA_verify(0, tbs, tbslen, sig, siglen, ec_key);
}

static int pkey_ec_derive(EvpPkeyCtx *ctx, uint8_t *key, size_t *keylen) {
  if (!ctx->pkey || !ctx->peerkey) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_KEYS_NOT_SET);
    return 0;
  }

  const EC_KEY *eckey = reinterpret_cast<EC_KEY *>(ctx->pkey->pkey);
  if (!key) {
    const EC_GROUP *group;
    group = EC_KEY_get0_group(eckey);
    *keylen = (EC_GROUP_get_degree(group) + 7) / 8;
    return 1;
  }

  const EC_KEY *eckey_peer = reinterpret_cast<EC_KEY *>(ctx->peerkey->pkey);
  const EC_POINT *pubkey = EC_KEY_get0_public_key(eckey_peer);

  // NB: unlike PKCS#3 DH, if *outlen is less than maximum size this is
  // not an error, the result is truncated.
  size_t outlen = *keylen;
  int ret = ECDH_compute_key(key, outlen, pubkey, eckey, nullptr);
  if (ret < 0) {
    return 0;
  }
  *keylen = ret;
  return 1;
}

static int pkey_ec_ctrl(EvpPkeyCtx *ctx, int type, int p1, void *p2) {
  EC_PKEY_CTX *dctx = reinterpret_cast<EC_PKEY_CTX *>(ctx->data);

  switch (type) {
    case EVP_PKEY_CTRL_MD: {
      const EVP_MD *md = reinterpret_cast<const EVP_MD *>(p2);
      int md_type = EVP_MD_type(md);
      if (md_type != NID_sha1 && md_type != NID_sha224 &&
          md_type != NID_sha256 && md_type != NID_sha384 &&
          md_type != NID_sha512) {
        OPENSSL_PUT_ERROR(EVP, EVP_R_INVALID_DIGEST_TYPE);
        return 0;
      }
      dctx->md = md;
      return 1;
    }

    case EVP_PKEY_CTRL_GET_MD:
      *(const EVP_MD **)p2 = dctx->md;
      return 1;

    case EVP_PKEY_CTRL_PEER_KEY:
      // Default behaviour is OK
      return 1;

    case EVP_PKEY_CTRL_EC_PARAMGEN_GROUP: {
      dctx->gen_group = static_cast<const EC_GROUP *>(p2);
      return 1;
    }

    default:
      OPENSSL_PUT_ERROR(EVP, EVP_R_COMMAND_NOT_SUPPORTED);
      return 0;
  }
}

static int pkey_ec_keygen(EvpPkeyCtx *ctx, EvpPkey *pkey) {
  EC_PKEY_CTX *dctx = reinterpret_cast<EC_PKEY_CTX *>(ctx->data);
  const EC_GROUP *group = dctx->gen_group;
  if (group == nullptr) {
    if (ctx->pkey == nullptr) {
      OPENSSL_PUT_ERROR(EVP, EVP_R_NO_PARAMETERS_SET);
      return 0;
    }
    group = EC_KEY_get0_group(reinterpret_cast<EC_KEY *>(ctx->pkey->pkey));
  }
  EC_KEY *ec = EC_KEY_new();
  if (ec == nullptr || !EC_KEY_set_group(ec, group) ||
      !EC_KEY_generate_key(ec)) {
    EC_KEY_free(ec);
    return 0;
  }
  EVP_PKEY_assign_EC_KEY(pkey, ec);
  return 1;
}

static int pkey_ec_paramgen(EvpPkeyCtx *ctx, EvpPkey *pkey) {
  EC_PKEY_CTX *dctx = reinterpret_cast<EC_PKEY_CTX *>(ctx->data);
  if (dctx->gen_group == nullptr) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_NO_PARAMETERS_SET);
    return 0;
  }
  EC_KEY *ec = EC_KEY_new();
  if (ec == nullptr || !EC_KEY_set_group(ec, dctx->gen_group)) {
    EC_KEY_free(ec);
    return 0;
  }
  EVP_PKEY_assign_EC_KEY(pkey, ec);
  return 1;
}

const EVP_PKEY_CTX_METHOD ec_pkey_meth = {
    EVP_PKEY_EC,
    pkey_ec_init,
    pkey_ec_copy,
    pkey_ec_cleanup,
    pkey_ec_keygen,
    pkey_ec_sign,
    nullptr /* sign_message */,
    pkey_ec_verify,
    nullptr /* verify_message */,
    nullptr /* verify_recover */,
    nullptr /* encrypt */,
    nullptr /* decrypt */,
    pkey_ec_derive,
    pkey_ec_paramgen,
    /*encap=*/nullptr,
    /*decap=*/nullptr,
    pkey_ec_ctrl,
};

}  // namespace

const EVP_PKEY_ALG *EVP_pkey_ec_p224() {
  static const EVP_PKEY_ALG_EC kAlg = {{&ec_asn1_meth, &ec_pkey_meth},
                                       &EC_group_p224};
  return &kAlg;
}

const EVP_PKEY_ALG *EVP_pkey_ec_p256() {
  static const EVP_PKEY_ALG_EC kAlg = {{&ec_asn1_meth, &ec_pkey_meth},
                                       &EC_group_p256};
  return &kAlg;
}

const EVP_PKEY_ALG *EVP_pkey_ec_p384() {
  static const EVP_PKEY_ALG_EC kAlg = {{&ec_asn1_meth, &ec_pkey_meth},
                                       &EC_group_p384};
  return &kAlg;
}

const EVP_PKEY_ALG *EVP_pkey_ec_p521() {
  static const EVP_PKEY_ALG_EC kAlg = {{&ec_asn1_meth, &ec_pkey_meth},
                                       &EC_group_p521};
  return &kAlg;
}

const EVP_PKEY_ALG *bssl::evp_pkey_ec_no_curve() {
  static const EVP_PKEY_ALG_EC kAlg = {{&ec_asn1_meth, &ec_pkey_meth}, nullptr};
  return &kAlg;
}

int EVP_PKEY_set1_EC_KEY(EVP_PKEY *pkey, EC_KEY *key) {
  if (EVP_PKEY_assign_EC_KEY(pkey, key)) {
    EC_KEY_up_ref(key);
    return 1;
  }
  return 0;
}

int EVP_PKEY_assign_EC_KEY(EVP_PKEY *pkey, EC_KEY *key) {
  if (key == nullptr) {
    return 0;
  }
  evp_pkey_set0(FromOpaque(pkey), &ec_asn1_meth, key);
  return 1;
}

EC_KEY *EVP_PKEY_get0_EC_KEY(const EVP_PKEY *pkey) {
  if (EVP_PKEY_id(pkey) != EVP_PKEY_EC) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_EXPECTING_A_EC_KEY);
    return nullptr;
  }
  return reinterpret_cast<EC_KEY *>(FromOpaque(pkey)->pkey);
}

EC_KEY *EVP_PKEY_get1_EC_KEY(const EVP_PKEY *pkey) {
  EC_KEY *ec_key = EVP_PKEY_get0_EC_KEY(pkey);
  if (ec_key != nullptr) {
    EC_KEY_up_ref(ec_key);
  }
  return ec_key;
}

int EVP_PKEY_get_ec_curve_nid(const EVP_PKEY *pkey) {
  const EC_KEY *ec_key = EVP_PKEY_get0_EC_KEY(pkey);
  if (ec_key == nullptr) {
    return NID_undef;
  }
  const EC_GROUP *group = EC_KEY_get0_group(ec_key);
  if (group == nullptr) {
    return NID_undef;
  }
  return EC_GROUP_get_curve_name(group);
}

int EVP_PKEY_get_ec_point_conv_form(const EVP_PKEY *pkey) {
  const EC_KEY *ec_key = EVP_PKEY_get0_EC_KEY(pkey);
  if (ec_key == nullptr) {
    return 0;
  }
  return EC_KEY_get_conv_form(ec_key);
}

int EVP_PKEY_CTX_set_ec_paramgen_curve_nid(EVP_PKEY_CTX *ctx, int nid) {
  const EC_GROUP *group = EC_GROUP_new_by_curve_name(nid);
  if (group == nullptr) {
    return 0;
  }
  return EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_EC, EVP_PKEY_OP_TYPE_GEN,
                           EVP_PKEY_CTRL_EC_PARAMGEN_GROUP, 0,
                           const_cast<EC_GROUP *>(group));
}

int EVP_PKEY_CTX_set_ec_param_enc(EVP_PKEY_CTX *ctx, int encoding) {
  // BoringSSL only supports named curve syntax.
  if (encoding != OPENSSL_EC_NAMED_CURVE) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_INVALID_PARAMETERS);
    return 0;
  }
  return 1;
}
