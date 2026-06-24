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

#include <openssl/bytestring.h>
#include <openssl/err.h>
#include <openssl/evp_errors.h>
#include <openssl/mem.h>
#include <openssl/xwing.h>

#include "../mem_internal.h"
#include "internal.h"


using namespace bssl;

namespace {

struct XWING_KEY {
  static constexpr bool kAllowUniquePtr = true;

  uint8_t pub[XWING_PUBLIC_KEY_BYTES];
  XWING_private_key priv;
  bool has_private;
};

extern const EVP_PKEY_ASN1_METHOD xwing_asn1_meth;
extern const EVP_PKEY_CTX_METHOD xwing_pkey_meth;

static void xwing_free(EvpPkey *pkey) {
  Delete(reinterpret_cast<XWING_KEY *>(pkey->pkey));
}

static bool xwing_pub_equal(const EvpPkey *a, const EvpPkey *b) {
  const XWING_KEY *a_key = reinterpret_cast<const XWING_KEY *>(a->pkey);
  const XWING_KEY *b_key = reinterpret_cast<const XWING_KEY *>(b->pkey);
  return OPENSSL_memcmp(a_key->pub, b_key->pub, XWING_PUBLIC_KEY_BYTES) == 0;
}

static bool xwing_pub_present(const EvpPkey *) { return true; }

static bool xwing_pub_copy(EvpPkey *out, const EvpPkey *pkey) {
  const XWING_KEY *pkey_xwing = reinterpret_cast<const XWING_KEY *>(pkey->pkey);
  auto public_copy = MakeUnique<XWING_KEY>();
  if (public_copy == nullptr) {
    return false;
  }
  OPENSSL_memcpy(public_copy->pub, pkey_xwing->pub, XWING_PUBLIC_KEY_BYTES);
  public_copy->has_private = false;
  evp_pkey_set0(out, pkey->ameth, public_copy.release());
  return true;
}

static bool xwing_priv_present(const EvpPkey *pk) {
  const XWING_KEY *key = reinterpret_cast<const XWING_KEY *>(pk->pkey);
  return key->has_private;
}

static int xwing_set_priv_seed(EvpPkey *pkey, const uint8_t *in, size_t len) {
  auto key = MakeUnique<XWING_KEY>();
  if (key == nullptr) {
    return 0;
  }
  CBS cbs;
  CBS_init(&cbs, in, len);
  if (!XWING_parse_private_key(&key->priv, &cbs) || CBS_len(&cbs) != 0 ||
      !XWING_public_from_private(key->pub, &key->priv)) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_DECODE_ERROR);
    return 0;
  }
  key->has_private = true;
  evp_pkey_set0(pkey, &xwing_asn1_meth, key.release());
  return 1;
}

static int xwing_get_priv_seed(const EvpPkey *pkey, uint8_t *out,
                               size_t *out_len) {
  if (out == nullptr) {
    *out_len = XWING_PRIVATE_KEY_BYTES;
    return 1;
  }
  const XWING_KEY *key = reinterpret_cast<const XWING_KEY *>(pkey->pkey);
  if (key == nullptr || !key->has_private) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_NOT_A_PRIVATE_KEY);
    return 0;
  }
  if (*out_len < XWING_PRIVATE_KEY_BYTES) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_BUFFER_TOO_SMALL);
    return 0;
  }
  CBB cbb;
  CBB_init_fixed(&cbb, out, XWING_PRIVATE_KEY_BYTES);
  if (!XWING_marshal_private_key(&cbb, &key->priv)) {
    return 0;
  }
  *out_len = CBB_len(&cbb);
  assert(*out_len == XWING_PRIVATE_KEY_BYTES);
  return 1;
}

static int xwing_set_pub_raw(EvpPkey *pkey, const uint8_t *in, size_t len) {
  if (len != XWING_PUBLIC_KEY_BYTES) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_DECODE_ERROR);
    return 0;
  }
  auto key = MakeUnique<XWING_KEY>();
  if (key == nullptr) {
    return 0;
  }
  OPENSSL_memcpy(key->pub, in, len);
  key->has_private = false;
  evp_pkey_set0(pkey, &xwing_asn1_meth, key.release());
  return 1;
}

static int xwing_get_pub_raw(const EvpPkey *pkey, uint8_t *out,
                             size_t *out_len) {
  if (out == nullptr) {
    *out_len = XWING_PUBLIC_KEY_BYTES;
    return 1;
  }
  if (*out_len < XWING_PUBLIC_KEY_BYTES) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_BUFFER_TOO_SMALL);
    return 0;
  }
  const XWING_KEY *key = reinterpret_cast<const XWING_KEY *>(pkey->pkey);
  OPENSSL_memcpy(out, key->pub, XWING_PUBLIC_KEY_BYTES);
  *out_len = XWING_PUBLIC_KEY_BYTES;
  return 1;
}

static int xwing_size(const EvpPkey *pkey) { return XWING_CIPHERTEXT_BYTES; }

static int xwing_bits(const EvpPkey *pkey) {
  return XWING_PUBLIC_KEY_BYTES * 8;
}

// X-Wing has no parameters to copy.
static int pkey_xwing_copy_ctx(EvpPkeyCtx *dst, EvpPkeyCtx *src) { return 1; }

static int pkey_xwing_keygen(EvpPkeyCtx *ctx, EvpPkey *pkey) {
  auto key = MakeUnique<XWING_KEY>();
  if (key == nullptr || !XWING_generate_key(key->pub, &key->priv)) {
    OPENSSL_PUT_ERROR(EVP, ERR_R_INTERNAL_ERROR);
    return 0;
  }
  key->has_private = true;
  evp_pkey_set0(pkey, &xwing_asn1_meth, key.release());
  return 1;
}

static int xwing_kem_encap(uint8_t *out_ciphertext, size_t ciphertext_len,
                           uint8_t *out_secret, size_t secret_len,
                           const EVP_PKEY *peer_key) {
  if (ciphertext_len != XWING_CIPHERTEXT_BYTES) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_INVALID_CIPHERTEXT_LENGTH);
    return 0;
  }
  if (secret_len != XWING_SHARED_SECRET_BYTES) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_INVALID_SECRET_LENGTH);
    return 0;
  }
  const XWING_KEY *peer_pubkey =
      reinterpret_cast<XWING_KEY *>(FromOpaque(peer_key)->pkey);
  return XWING_encap(out_ciphertext, out_secret, peer_pubkey->pub);
}

static int xwing_kem_decap(uint8_t *out_secret, size_t secret_len,
                           const uint8_t *ciphertext, size_t ciphertext_len,
                           const EVP_PKEY *key) {
  const XWING_KEY *priv = reinterpret_cast<XWING_KEY *>(FromOpaque(key)->pkey);
  if (priv == nullptr || !priv->has_private) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_NOT_A_PRIVATE_KEY);
    return 0;
  }
  if (secret_len != XWING_SHARED_SECRET_BYTES) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_INVALID_SECRET_LENGTH);
    return 0;
  }
  // XWING_decap does not accept wrong ciphertext lengths, so we must check for
  // the proper length here. For consistency, we don't add an error to the error
  // queue when a KEM decap fails due to incorrect ciphertext length.
  if (ciphertext_len != XWING_CIPHERTEXT_BYTES) {
    return 0;
  }
  return XWING_decap(out_secret, ciphertext, &priv->priv);
}

static const EVP_KEM xwing_evp_kem = {
    EVP_PKEY_XWING,             //
    XWING_CIPHERTEXT_BYTES,     //
    XWING_SHARED_SECRET_BYTES,  //
    &xwing_kem_encap,           //
    &xwing_kem_decap,           //
};

const EVP_PKEY_CTX_METHOD xwing_pkey_meth = {
    /*pkey_id=*/EVP_PKEY_XWING,
    /*init=*/nullptr,
    &pkey_xwing_copy_ctx,
    /*cleanup=*/nullptr,
    &pkey_xwing_keygen,
    /*sign=*/nullptr,
    /*sign_message=*/nullptr,
    /*verify=*/nullptr,
    /*verify_message=*/nullptr,
    /*verify_recover=*/nullptr,
    /*encrypt=*/nullptr,
    /*decrypt=*/nullptr,
    /*derive=*/nullptr,
    /*paramgen=*/nullptr,
    &KemAdapter<xwing_evp_kem>::EncapMethod,
    &KemAdapter<xwing_evp_kem>::DecapMethod,
    /*ctrl=*/nullptr,
};

const EVP_PKEY_ASN1_METHOD xwing_asn1_meth = {
    EVP_PKEY_XWING,
    /*oid=*/{},
    /*oid_len=*/0,
    &xwing_pkey_meth,

    /*pub_decode=*/nullptr,
    /*pub_encode=*/nullptr,
    &xwing_pub_equal,
    &xwing_pub_present,
    &xwing_pub_copy,

    /*priv_decode=*/nullptr,
    /*priv_encode=*/nullptr,
    &xwing_priv_present,

    /*set_priv_raw=*/nullptr,
    &xwing_set_priv_seed,
    &xwing_set_pub_raw,
    /*get_priv_raw=*/nullptr,
    &xwing_get_priv_seed,
    &xwing_get_pub_raw,

    /*set1_tls_encodedpoint=*/nullptr,
    /*get1_tls_encodedpoint=*/nullptr,
    /*pkey_opaque=*/nullptr,

    &xwing_size,
    &xwing_bits,

    /*param_missing=*/nullptr,
    /*param_copy=*/nullptr,
    /*param_equal=*/nullptr,

    /*pkey_free=*/&xwing_free,
};

}  // namespace

const EVP_PKEY_ALG *EVP_pkey_xwing() {
  static const EVP_PKEY_ALG kAlg = {&xwing_asn1_meth, &xwing_pkey_meth};
  return &kAlg;
}

const EVP_KEM *EVP_kem_xwing() { return &xwing_evp_kem; }
