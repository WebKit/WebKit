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

#include <openssl/digest.h>
#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/params.h>

#include "../internal.h"
#include "../mem_internal.h"
#include "../params_internal.h"
#include "internal.h"


using namespace bssl;

static UniquePtr<EvpPkeyCtx> evp_pkey_ctx_new(
    EvpPkey *pkey, const EVP_PKEY_ALG *alg, const EVP_PKEY_CTX_METHOD *pmeth) {
  assert(pkey != nullptr || alg != nullptr);
  UniquePtr<EvpPkeyCtx> ret = MakeUnique<EvpPkeyCtx>();
  if (!ret) {
    return nullptr;
  }

  ret->pmeth = pmeth;
  ret->operation = EVP_PKEY_OP_UNDEFINED;
  ret->pkey = UpRef(pkey);

  if (pmeth->init && pmeth->init(ret.get(), alg) <= 0) {
    ret->pmeth = nullptr;  // Don't call |pmeth->cleanup|.
    return nullptr;
  }

  return ret;
}

EVP_PKEY_CTX *EVP_PKEY_CTX_new(EVP_PKEY *pkey, ENGINE *e) {
  auto *pkey_impl = FromOpaque(pkey);
  if (pkey_impl == nullptr || pkey_impl->ameth == nullptr) {
    OPENSSL_PUT_ERROR(EVP, ERR_R_PASSED_NULL_PARAMETER);
    return nullptr;
  }
  if (pkey_impl->pkey == nullptr) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_NO_KEY_SET);
    return nullptr;
  }

  const EVP_PKEY_CTX_METHOD *pkey_method = pkey_impl->ameth->pkey_method;
  if (pkey_method == nullptr) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_UNSUPPORTED_ALGORITHM);
    ERR_add_error_dataf("algorithm %d", pkey_impl->ameth->pkey_id);
    return nullptr;
  }

  return evp_pkey_ctx_new(pkey_impl, nullptr, pkey_method).release();
}

EVP_PKEY_CTX *EVP_PKEY_CTX_new_id(int id, ENGINE *e) {
  // |EVP_PKEY_RSA_PSS| is intentionally omitted from this list. These are types
  // that can be created without an |EVP_PKEY|, and we do not support
  // |EVP_PKEY_RSA_PSS| keygen.
  const EVP_PKEY_ALG *alg = nullptr;
  switch (id) {
    case EVP_PKEY_RSA:
      alg = EVP_pkey_rsa();
      break;
    case EVP_PKEY_EC:
      alg = evp_pkey_ec_no_curve();
      break;
    case EVP_PKEY_ED25519:
      alg = EVP_pkey_ed25519();
      break;
    case EVP_PKEY_X25519:
      alg = EVP_pkey_x25519();
      break;
    case EVP_PKEY_HKDF:
      alg = evp_pkey_hkdf();
      break;
    case EVP_PKEY_ML_DSA_44:
      alg = EVP_pkey_ml_dsa_44();
      break;
    case EVP_PKEY_ML_DSA_65:
      alg = EVP_pkey_ml_dsa_65();
      break;
    case EVP_PKEY_ML_DSA_87:
      alg = EVP_pkey_ml_dsa_87();
      break;
    case EVP_PKEY_ML_KEM_768:
      alg = EVP_pkey_ml_kem_768();
      break;
    case EVP_PKEY_ML_KEM_1024:
      alg = EVP_pkey_ml_kem_1024();
      break;
  }
  if (alg == nullptr || alg->pkey_method == nullptr) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_UNSUPPORTED_ALGORITHM);
    ERR_add_error_dataf("algorithm %d", id);
    return nullptr;
  }
  return evp_pkey_ctx_new_alg(alg).release();
}

UniquePtr<EvpPkeyCtx> bssl::evp_pkey_ctx_new_alg(const EVP_PKEY_ALG *alg) {
  return evp_pkey_ctx_new(nullptr, alg, alg->pkey_method);
}

EvpPkeyCtx::~EvpPkeyCtx() {
  if (pmeth && pmeth->cleanup) {
    pmeth->cleanup(this);
  }
}

void EVP_PKEY_CTX_free(EVP_PKEY_CTX *ctx) { Delete(FromOpaque(ctx)); }

EVP_PKEY_CTX *EVP_PKEY_CTX_dup(EVP_PKEY_CTX *ctx) {
  auto *impl = FromOpaque(ctx);

  if (!impl->pmeth || !impl->pmeth->copy) {
    return nullptr;
  }

  UniquePtr<EvpPkeyCtx> ret = MakeUnique<EvpPkeyCtx>();
  if (!ret) {
    return nullptr;
  }

  ret->pmeth = impl->pmeth;
  ret->operation = impl->operation;
  ret->pkey = UpRef(impl->pkey);
  ret->peerkey = UpRef(impl->peerkey);
  if (impl->pmeth->copy(ret.get(), impl) <= 0) {
    OPENSSL_PUT_ERROR(EVP, ERR_LIB_EVP);
    return nullptr;
  }

  return ret.release();
}

EVP_PKEY *EVP_PKEY_CTX_get0_pkey(EVP_PKEY_CTX *ctx) {
  auto *impl = FromOpaque(ctx);
  return impl->pkey.get();
}

int bssl::EVP_PKEY_CTX_ctrl(EVP_PKEY_CTX *ctx, int keytype, int optype, int cmd,
                            int p1, void *p2) {
  auto *impl = FromOpaque(ctx);
  if (!impl || !impl->pmeth || !impl->pmeth->ctrl) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_COMMAND_NOT_SUPPORTED);
    return 0;
  }
  if (keytype != -1 && impl->pmeth->pkey_id != keytype) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE);
    return 0;
  }

  if (impl->operation == EVP_PKEY_OP_UNDEFINED) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_NO_OPERATION_SET);
    return 0;
  }

  if (optype != -1 && !(impl->operation & optype)) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_INVALID_OPERATION);
    return 0;
  }

  return impl->pmeth->ctrl(impl, cmd, p1, p2);
}

int EVP_PKEY_sign_init(EVP_PKEY_CTX *ctx) {
  auto *impl = FromOpaque(ctx);
  if (!ctx || impl->pmeth == nullptr ||
      (impl->pmeth->sign == nullptr && impl->pmeth->sign_message == nullptr)) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE);
    return 0;
  }

  impl->operation = EVP_PKEY_OP_SIGN;
  return 1;
}

int EVP_PKEY_sign(EVP_PKEY_CTX *ctx, uint8_t *sig, size_t *sig_len,
                  const uint8_t *digest, size_t digest_len) {
  auto *impl = FromOpaque(ctx);
  if (!impl || !impl->pmeth || !impl->pmeth->sign) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE);
    return 0;
  }
  if (impl->operation != EVP_PKEY_OP_SIGN) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATION_NOT_INITIALIZED);
    return 0;
  }
  return impl->pmeth->sign(impl, sig, sig_len, digest, digest_len);
}

int EVP_PKEY_verify_init(EVP_PKEY_CTX *ctx) {
  auto *impl = FromOpaque(ctx);
  if (!impl || impl->pmeth == nullptr ||
      (impl->pmeth->verify == nullptr &&
       impl->pmeth->verify_message == nullptr)) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE);
    return 0;
  }
  impl->operation = EVP_PKEY_OP_VERIFY;
  return 1;
}

int EVP_PKEY_verify(EVP_PKEY_CTX *ctx, const uint8_t *sig, size_t sig_len,
                    const uint8_t *digest, size_t digest_len) {
  auto *impl = FromOpaque(ctx);
  if (!impl || !impl->pmeth || !impl->pmeth->verify) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE);
    return 0;
  }
  if (impl->operation != EVP_PKEY_OP_VERIFY) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATION_NOT_INITIALIZED);
    return 0;
  }
  return impl->pmeth->verify(impl, sig, sig_len, digest, digest_len);
}

int EVP_PKEY_encrypt_init(EVP_PKEY_CTX *ctx) {
  auto *impl = FromOpaque(ctx);
  if (!impl || !impl->pmeth || !impl->pmeth->encrypt) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE);
    return 0;
  }
  impl->operation = EVP_PKEY_OP_ENCRYPT;
  return 1;
}

int EVP_PKEY_encrypt(EVP_PKEY_CTX *ctx, uint8_t *out, size_t *outlen,
                     const uint8_t *in, size_t inlen) {
  auto *impl = FromOpaque(ctx);
  if (!impl || !impl->pmeth || !impl->pmeth->encrypt) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE);
    return 0;
  }
  if (impl->operation != EVP_PKEY_OP_ENCRYPT) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATION_NOT_INITIALIZED);
    return 0;
  }
  return impl->pmeth->encrypt(impl, out, outlen, in, inlen);
}

int EVP_PKEY_decrypt_init(EVP_PKEY_CTX *ctx) {
  auto *impl = FromOpaque(ctx);
  if (!impl || !impl->pmeth || !impl->pmeth->decrypt) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE);
    return 0;
  }
  impl->operation = EVP_PKEY_OP_DECRYPT;
  return 1;
}

int EVP_PKEY_decrypt(EVP_PKEY_CTX *ctx, uint8_t *out, size_t *outlen,
                     const uint8_t *in, size_t inlen) {
  auto *impl = FromOpaque(ctx);
  if (!impl || !impl->pmeth || !impl->pmeth->decrypt) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE);
    return 0;
  }
  if (impl->operation != EVP_PKEY_OP_DECRYPT) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATION_NOT_INITIALIZED);
    return 0;
  }
  return impl->pmeth->decrypt(impl, out, outlen, in, inlen);
}

int EVP_PKEY_verify_recover_init(EVP_PKEY_CTX *ctx) {
  auto *impl = FromOpaque(ctx);
  if (!impl || !impl->pmeth || !impl->pmeth->verify_recover) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE);
    return 0;
  }
  impl->operation = EVP_PKEY_OP_VERIFYRECOVER;
  return 1;
}

int EVP_PKEY_verify_recover(EVP_PKEY_CTX *ctx, uint8_t *out, size_t *out_len,
                            const uint8_t *sig, size_t sig_len) {
  auto *impl = FromOpaque(ctx);
  if (!impl || !impl->pmeth || !impl->pmeth->verify_recover) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE);
    return 0;
  }
  if (impl->operation != EVP_PKEY_OP_VERIFYRECOVER) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATION_NOT_INITIALIZED);
    return 0;
  }
  return impl->pmeth->verify_recover(impl, out, out_len, sig, sig_len);
}

int EVP_PKEY_derive_init(EVP_PKEY_CTX *ctx) {
  auto *impl = FromOpaque(ctx);
  if (!impl || !impl->pmeth || !impl->pmeth->derive) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE);
    return 0;
  }
  impl->operation = EVP_PKEY_OP_DERIVE;
  return 1;
}

int EVP_PKEY_derive_set_peer(EVP_PKEY_CTX *ctx, EVP_PKEY *peer) {
  auto *impl = FromOpaque(ctx);
  if (!impl || !impl->pmeth ||
      !(impl->pmeth->derive || impl->pmeth->encrypt || impl->pmeth->decrypt) ||
      !impl->pmeth->ctrl) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE);
    return 0;
  }
  if (impl->operation != EVP_PKEY_OP_DERIVE &&
      impl->operation != EVP_PKEY_OP_ENCRYPT &&
      impl->operation != EVP_PKEY_OP_DECRYPT) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATION_NOT_INITIALIZED);
    return 0;
  }

  int ret = impl->pmeth->ctrl(impl, EVP_PKEY_CTRL_PEER_KEY, 0, peer);

  if (ret <= 0) {
    return 0;
  }

  if (ret == 2) {
    return 1;
  }

  if (!impl->pkey || !FromOpaque(peer)->pkey) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_NO_KEY_SET);
    return 0;
  }

  if (EVP_PKEY_id(impl->pkey.get()) != EVP_PKEY_id(peer)) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_DIFFERENT_KEY_TYPES);
    return 0;
  }

  if (!EVP_PKEY_missing_parameters(peer) &&
      !EVP_PKEY_parameters_eq(impl->pkey.get(), peer)) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_DIFFERENT_PARAMETERS);
    return 0;
  }

  impl->peerkey = UpRef(FromOpaque(peer));
  ret = impl->pmeth->ctrl(impl, EVP_PKEY_CTRL_PEER_KEY, 1, peer);
  if (ret <= 0) {
    impl->peerkey = nullptr;
    return 0;
  }

  return 1;
}

int EVP_PKEY_derive(EVP_PKEY_CTX *ctx, uint8_t *key, size_t *out_key_len) {
  auto *impl = FromOpaque(ctx);
  if (!impl || !impl->pmeth || !impl->pmeth->derive) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE);
    return 0;
  }
  if (impl->operation != EVP_PKEY_OP_DERIVE) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATION_NOT_INITIALIZED);
    return 0;
  }
  return impl->pmeth->derive(impl, key, out_key_len);
}

EVP_PKEY *EVP_PKEY_generate_from_alg(const EVP_PKEY_ALG *alg) {
  UniquePtr<EvpPkeyCtx> ctx = evp_pkey_ctx_new_alg(alg);
  EVP_PKEY *pkey = nullptr;
  if (ctx == nullptr ||                    //
      !EVP_PKEY_keygen_init(ctx.get()) ||  //
      !EVP_PKEY_keygen(ctx.get(), &pkey)) {
    return nullptr;
  }
  return pkey;
}

int EVP_PKEY_keygen_init(EVP_PKEY_CTX *ctx) {
  auto *impl = FromOpaque(ctx);
  if (!impl || !impl->pmeth || !impl->pmeth->keygen) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE);
    return 0;
  }
  impl->operation = EVP_PKEY_OP_KEYGEN;
  return 1;
}

int EVP_PKEY_keygen(EVP_PKEY_CTX *ctx, EVP_PKEY **out_pkey) {
  auto *impl = FromOpaque(ctx);
  if (!impl || !impl->pmeth || !impl->pmeth->keygen) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE);
    return 0;
  }
  if (impl->operation != EVP_PKEY_OP_KEYGEN) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATION_NOT_INITIALIZED);
    return 0;
  }

  if (!out_pkey) {
    return 0;
  }

  if (!*out_pkey) {
    *out_pkey = EVP_PKEY_new();
    if (!*out_pkey) {
      OPENSSL_PUT_ERROR(EVP, ERR_LIB_EVP);
      return 0;
    }
  }

  if (!impl->pmeth->keygen(impl, FromOpaque(*out_pkey))) {
    EVP_PKEY_free(*out_pkey);
    *out_pkey = nullptr;
    return 0;
  }
  return 1;
}

int EVP_PKEY_paramgen_init(EVP_PKEY_CTX *ctx) {
  auto *impl = FromOpaque(ctx);
  if (!impl || !impl->pmeth || !impl->pmeth->paramgen) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE);
    return 0;
  }
  impl->operation = EVP_PKEY_OP_PARAMGEN;
  return 1;
}

int EVP_PKEY_paramgen(EVP_PKEY_CTX *ctx, EVP_PKEY **out_pkey) {
  auto *impl = FromOpaque(ctx);
  if (!impl || !impl->pmeth || !impl->pmeth->paramgen) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE);
    return 0;
  }
  if (impl->operation != EVP_PKEY_OP_PARAMGEN) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATION_NOT_INITIALIZED);
    return 0;
  }

  if (!out_pkey) {
    return 0;
  }

  if (!*out_pkey) {
    *out_pkey = EVP_PKEY_new();
    if (!*out_pkey) {
      OPENSSL_PUT_ERROR(EVP, ERR_LIB_EVP);
      return 0;
    }
  }

  if (!impl->pmeth->paramgen(impl, FromOpaque(*out_pkey))) {
    EVP_PKEY_free(*out_pkey);
    *out_pkey = nullptr;
    return 0;
  }
  return 1;
}

int EVP_PKEY_encapsulate_init(EVP_PKEY_CTX *ctx, const OSSL_PARAM *params) {
  if (params != nullptr && !IsEndParam(*params)) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_INVALID_PARAMETERS);
    return 0;
  }
  auto *impl = FromOpaque(ctx);
  if (!impl || !impl->pmeth || !impl->pmeth->encap) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE);
    return 0;
  }
  impl->operation = EVP_PKEY_OP_ENCAPSULATE;
  return 1;
}

int EVP_PKEY_encapsulate(EVP_PKEY_CTX *ctx, uint8_t *out_ciphertext,
                         size_t *out_ciphertext_len, uint8_t *out_secret,
                         size_t *out_secret_len) {
  auto *impl = FromOpaque(ctx);
  if (!impl || !impl->pmeth || !impl->pmeth->encap) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE);
    return 0;
  }
  if (impl->operation != EVP_PKEY_OP_ENCAPSULATE) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATION_NOT_INITIALIZED);
    return 0;
  }
  if (!impl->pkey) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_NO_KEY_SET);
    return 0;
  }
  return impl->pmeth->encap(impl, out_ciphertext, out_ciphertext_len,
                            out_secret, out_secret_len);
}

int EVP_PKEY_decapsulate_init(EVP_PKEY_CTX *ctx, const OSSL_PARAM *params) {
  if (params != nullptr && !IsEndParam(*params)) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_INVALID_PARAMETERS);
    return 0;
  }
  auto *impl = FromOpaque(ctx);
  if (!impl || !impl->pmeth || !impl->pmeth->decap) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE);
    return 0;
  }
  impl->operation = EVP_PKEY_OP_DECAPSULATE;
  return 1;
}

int EVP_PKEY_decapsulate(EVP_PKEY_CTX *ctx, uint8_t *out_secret,
                         size_t *out_secret_len, const uint8_t *ciphertext,
                         size_t ciphertext_len) {
  auto *impl = FromOpaque(ctx);
  if (!impl || !impl->pmeth || !impl->pmeth->decap) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE);
    return 0;
  }
  if (impl->operation != EVP_PKEY_OP_DECAPSULATE) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATION_NOT_INITIALIZED);
    return 0;
  }
  if (!impl->pkey) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_NO_KEY_SET);
    return 0;
  }
  return impl->pmeth->decap(impl, out_secret, out_secret_len, ciphertext,
                            ciphertext_len);
}
