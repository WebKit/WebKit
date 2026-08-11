// Copyright 2006-2019 The OpenSSL Project Authors. All Rights Reserved.
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
#include <stddef.h>
#include <stdint.h>

#include <openssl/bn.h>
#include <openssl/dh.h>
#include <openssl/err.h>
#include <openssl/mem.h>

#include "../internal.h"
#include "../mem_internal.h"
#include "internal.h"


using namespace bssl;

namespace {

extern const EVP_PKEY_CTX_METHOD dh_pkey_meth;

static void dh_free(EvpPkey *pkey) {
  DH_free(reinterpret_cast<DH *>(pkey->pkey));
  pkey->pkey = nullptr;
}

static int dh_size(const EvpPkey *pkey) {
  return DH_size(reinterpret_cast<const DH *>(pkey->pkey));
}

static int dh_bits(const EvpPkey *pkey) {
  return DH_bits(reinterpret_cast<const DH *>(pkey->pkey));
}

static int dh_param_missing(const EvpPkey *pkey) {
  const DH *dh = reinterpret_cast<const DH *>(pkey->pkey);
  return dh == nullptr || DH_get0_p(dh) == nullptr || DH_get0_g(dh) == nullptr;
}

static int dh_param_copy(EvpPkey *to, const EvpPkey *from) {
  if (dh_param_missing(from)) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_MISSING_PARAMETERS);
    return 0;
  }
  if (to->pkey == nullptr) {
    to->pkey = DH_new();
    if (to->pkey == nullptr) {
      return 0;
    }
  }

  const DH *dh = reinterpret_cast<DH *>(from->pkey);
  const BIGNUM *q_old = DH_get0_q(dh);
  BIGNUM *p = BN_dup(DH_get0_p(dh));
  BIGNUM *q = q_old == nullptr ? nullptr : BN_dup(q_old);
  BIGNUM *g = BN_dup(DH_get0_g(dh));
  if (p == nullptr || (q_old != nullptr && q == nullptr) || g == nullptr ||
      !DH_set0_pqg(reinterpret_cast<DH *>(to->pkey), p, q, g)) {
    BN_free(p);
    BN_free(q);
    BN_free(g);
    return 0;
  }

  // |DH_set0_pqg| took ownership of |p|, |q|, and |g|.
  return 1;
}

static bool dh_param_equal(const EvpPkey *a, const EvpPkey *b) {
  if (dh_param_missing(a) || dh_param_missing(b)) {
    return false;
  }

  // Matching OpenSSL, only compare p and g for PKCS#3-style Diffie-Hellman.
  // OpenSSL only checks q in X9.42-style Diffie-Hellman ("DHX").
  const DH *a_dh = reinterpret_cast<const DH *>(a->pkey);
  const DH *b_dh = reinterpret_cast<const DH *>(b->pkey);
  return BN_cmp(DH_get0_p(a_dh), DH_get0_p(b_dh)) == 0 &&
         BN_cmp(DH_get0_g(a_dh), DH_get0_g(b_dh)) == 0;
}

static bool dh_pub_equal(const EvpPkey *a, const EvpPkey *b) {
  if (!dh_param_equal(a, b)) {
    return false;
  }

  const DH *a_dh = reinterpret_cast<const DH *>(a->pkey);
  const DH *b_dh = reinterpret_cast<const DH *>(b->pkey);
  return BN_cmp(DH_get0_pub_key(a_dh), DH_get0_pub_key(b_dh)) == 0;
}

static bool dh_has_pub(const EvpPkey *pk) {
  const DH *pk_dh = reinterpret_cast<const DH *>(pk->pkey);
  return DH_get0_pub_key(pk_dh) != nullptr;
}

static bool dh_pub_copy(EvpPkey *out, const EvpPkey *pk) {
  const DH *pk_dh = reinterpret_cast<const DH *>(pk->pkey);
  const BIGNUM *public_key = DH_get0_pub_key(pk_dh);
  if (public_key == nullptr) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_MISSING_PUBLIC_KEY);
    return false;
  }
  UniquePtr<DH> public_copy_dh(DHparams_dup(pk_dh));
  BIGNUM *public_key_copy = BN_dup(public_key);
  if (public_copy_dh == nullptr || public_key_copy == nullptr ||
      !DH_set0_key(public_copy_dh.get(), public_key_copy, nullptr)) {
    BN_free(public_key_copy);
    OPENSSL_PUT_ERROR(EVP, ERR_R_INTERNAL_ERROR);
    return false;
  }
  evp_pkey_set0(out, pk->ameth, public_copy_dh.release());
  return true;
}

static bool dh_has_priv(const EvpPkey *pk) {
  const DH *pk_dh = reinterpret_cast<const DH *>(pk->pkey);
  return DH_get0_priv_key(pk_dh) != nullptr;
}

static const EVP_PKEY_ASN1_METHOD dh_asn1_meth = {
    /*pkey_id=*/EVP_PKEY_DH,
    /*oid=*/{0},
    /*oid_len=*/0,
    /*pkey_method=*/&dh_pkey_meth,
    /*pub_decode=*/nullptr,
    /*pub_encode=*/nullptr,
    /*pub_equal=*/dh_pub_equal,
    /*pub_present=*/dh_has_pub,
    /*pub_dup=*/dh_pub_copy,
    /*priv_decode=*/nullptr,
    /*priv_encode=*/nullptr,
    /*priv_present=*/dh_has_priv,
    /*set_priv_raw=*/nullptr,
    /*set_priv_seed=*/nullptr,
    /*set_pub_raw=*/nullptr,
    /*get_priv_raw=*/nullptr,
    /*get_priv_seed=*/nullptr,
    /*get_pub_raw=*/nullptr,
    /*set1_tls_encodedpoint=*/nullptr,
    /*get1_tls_encodedpoint=*/nullptr,
    /*pkey_opaque=*/nullptr,
    /*pkey_size=*/dh_size,
    /*pkey_bits=*/dh_bits,
    /*param_missing=*/dh_param_missing,
    /*param_copy=*/dh_param_copy,
    /*param_equal=*/dh_param_equal,
    /*pkey_free=*/dh_free,
};

struct DH_PKEY_CTX {
  bool pad = false;
};

static int pkey_dh_init(EvpPkeyCtx *ctx, const EVP_PKEY_ALG *) {
  DH_PKEY_CTX *dctx = New<DH_PKEY_CTX>();
  if (dctx == nullptr) {
    return 0;
  }

  ctx->data = dctx;
  return 1;
}

static int pkey_dh_copy(EvpPkeyCtx *dst, EvpPkeyCtx *src) {
  if (!pkey_dh_init(dst, nullptr)) {
    return 0;
  }

  const DH_PKEY_CTX *sctx = reinterpret_cast<DH_PKEY_CTX *>(src->data);
  DH_PKEY_CTX *dctx = reinterpret_cast<DH_PKEY_CTX *>(dst->data);
  dctx->pad = sctx->pad;
  return 1;
}

static void pkey_dh_cleanup(EvpPkeyCtx *ctx) {
  Delete(reinterpret_cast<DH_PKEY_CTX *>(ctx->data));
}

static int pkey_dh_keygen(EvpPkeyCtx *ctx, EvpPkey *pkey) {
  DH *dh = DH_new();
  if (dh == nullptr || !EVP_PKEY_assign_DH(pkey, dh)) {
    DH_free(dh);
    return 0;
  }

  if (ctx->pkey != nullptr &&
      !EVP_PKEY_copy_parameters(pkey, ctx->pkey.get())) {
    return 0;
  }

  return DH_generate_key(dh);
}

static int pkey_dh_derive(EvpPkeyCtx *ctx, uint8_t *out, size_t *out_len) {
  DH_PKEY_CTX *dctx = reinterpret_cast<DH_PKEY_CTX *>(ctx->data);
  if (ctx->pkey == nullptr || ctx->peerkey == nullptr) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_KEYS_NOT_SET);
    return 0;
  }

  DH *our_key = reinterpret_cast<DH *>(ctx->pkey->pkey);
  DH *peer_key = reinterpret_cast<DH *>(ctx->peerkey->pkey);
  if (our_key == nullptr || peer_key == nullptr) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_KEYS_NOT_SET);
    return 0;
  }

  const BIGNUM *pub_key = DH_get0_pub_key(peer_key);
  if (pub_key == nullptr) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_KEYS_NOT_SET);
    return 0;
  }

  if (out == nullptr) {
    *out_len = DH_size(our_key);
    return 1;
  }

  if (*out_len < (size_t)DH_size(our_key)) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_BUFFER_TOO_SMALL);
    return 0;
  }

  int ret = dctx->pad ? DH_compute_key_padded(out, pub_key, our_key)
                      : DH_compute_key(out, pub_key, our_key);
  if (ret < 0) {
    return 0;
  }

  assert(ret <= DH_size(our_key));
  *out_len = (size_t)ret;
  return 1;
}

static int pkey_dh_ctrl(EvpPkeyCtx *ctx, int type, int p1, void *p2) {
  DH_PKEY_CTX *dctx = reinterpret_cast<DH_PKEY_CTX *>(ctx->data);
  switch (type) {
    case EVP_PKEY_CTRL_PEER_KEY:
      // |EVP_PKEY_derive_set_peer| requires the key implement this command,
      // even if it is a no-op.
      return 1;

    case EVP_PKEY_CTRL_DH_PAD:
      dctx->pad = p1;
      return 1;

    default:
      OPENSSL_PUT_ERROR(EVP, EVP_R_COMMAND_NOT_SUPPORTED);
      return 0;
  }
}

const EVP_PKEY_CTX_METHOD dh_pkey_meth = {
    /*pkey_id=*/EVP_PKEY_DH,
    /*init=*/pkey_dh_init,
    /*copy=*/pkey_dh_copy,
    /*cleanup=*/pkey_dh_cleanup,
    /*keygen=*/pkey_dh_keygen,
    /*sign=*/nullptr,
    /*sign_message=*/nullptr,
    /*verify=*/nullptr,
    /*verify_message=*/nullptr,
    /*verify_recover=*/nullptr,
    /*encrypt=*/nullptr,
    /*decrypt=*/nullptr,
    /*derive=*/pkey_dh_derive,
    /*paramgen=*/nullptr,
    /*encap=*/nullptr,
    /*decap=*/nullptr,
    /*ctrl=*/pkey_dh_ctrl,
};

}  // namespace

int EVP_PKEY_set1_DH(EVP_PKEY *pkey, DH *key) {
  if (EVP_PKEY_assign_DH(pkey, key)) {
    DH_up_ref(key);
    return 1;
  }
  return 0;
}

int EVP_PKEY_assign_DH(EVP_PKEY *pkey, DH *key) {
  if (key == nullptr) {
    return 0;
  }
  evp_pkey_set0(FromOpaque(pkey), &dh_asn1_meth, key);
  return 1;
}

DH *EVP_PKEY_get0_DH(const EVP_PKEY *pkey) {
  if (EVP_PKEY_id(pkey) != EVP_PKEY_DH) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_EXPECTING_A_DH_KEY);
    return nullptr;
  }
  return reinterpret_cast<DH *>(const_cast<EvpPkey *>(FromOpaque(pkey))->pkey);
}

DH *EVP_PKEY_get1_DH(const EVP_PKEY *pkey) {
  DH *dh = EVP_PKEY_get0_DH(pkey);
  if (dh != nullptr) {
    DH_up_ref(dh);
  }
  return dh;
}

int EVP_PKEY_CTX_set_dh_pad(EVP_PKEY_CTX *ctx, int pad) {
  return EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_DH, EVP_PKEY_OP_DERIVE,
                           EVP_PKEY_CTRL_DH_PAD, pad, nullptr);
}
