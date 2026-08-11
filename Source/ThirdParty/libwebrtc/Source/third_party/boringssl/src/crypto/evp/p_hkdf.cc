// Copyright 2022 The BoringSSL Authors
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

#include <openssl/err.h>
#include <openssl/hkdf.h>
#include <openssl/kdf.h>
#include <openssl/mem.h>
#include <openssl/span.h>

#include "../internal.h"
#include "../mem_internal.h"
#include "internal.h"


using namespace bssl;

namespace {

struct HKDF_PKEY_CTX {
  int mode = 0;
  const EVP_MD *md = nullptr;
  Array<uint8_t> key;
  Array<uint8_t> salt;
  Vector<uint8_t> info;
};

static int pkey_hkdf_init(EvpPkeyCtx *ctx, const EVP_PKEY_ALG *) {
  ctx->data = New<HKDF_PKEY_CTX>();
  return 1;
}

static int pkey_hkdf_copy(EvpPkeyCtx *dst, EvpPkeyCtx *src) {
  if (!pkey_hkdf_init(dst, nullptr)) {
    return 0;
  }

  HKDF_PKEY_CTX *hctx_dst = reinterpret_cast<HKDF_PKEY_CTX *>(dst->data);
  const HKDF_PKEY_CTX *hctx_src =
      reinterpret_cast<const HKDF_PKEY_CTX *>(src->data);
  hctx_dst->mode = hctx_src->mode;
  hctx_dst->md = hctx_src->md;

  if (!hctx_dst->key.CopyFrom(hctx_src->key) ||
      !hctx_dst->salt.CopyFrom(hctx_src->salt) ||
      !hctx_dst->info.CopyFrom(hctx_src->info)) {
    return 0;
  }

  return 1;
}

static void pkey_hkdf_cleanup(EvpPkeyCtx *ctx) {
  Delete(reinterpret_cast<HKDF_PKEY_CTX *>(ctx->data));
}

static int pkey_hkdf_derive(EvpPkeyCtx *ctx, uint8_t *out, size_t *out_len) {
  HKDF_PKEY_CTX *hctx = reinterpret_cast<HKDF_PKEY_CTX *>(ctx->data);
  if (hctx->md == nullptr) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_MISSING_PARAMETERS);
    return 0;
  }
  if (hctx->key.empty()) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_NO_KEY_SET);
    return 0;
  }

  if (out == nullptr) {
    if (hctx->mode == EVP_PKEY_HKDEF_MODE_EXTRACT_ONLY) {
      *out_len = EVP_MD_size(hctx->md);
    }
    // HKDF-Expand is variable-length and returns |*out_len| bytes. "Output" the
    // input length by leaving it alone.
    return 1;
  }

  switch (hctx->mode) {
    case EVP_PKEY_HKDEF_MODE_EXTRACT_AND_EXPAND:
      return HKDF(out, *out_len, hctx->md, hctx->key.data(), hctx->key.size(),
                  hctx->salt.data(), hctx->salt.size(), hctx->info.data(),
                  hctx->info.size());

    case EVP_PKEY_HKDEF_MODE_EXTRACT_ONLY:
      if (*out_len < EVP_MD_size(hctx->md)) {
        OPENSSL_PUT_ERROR(EVP, EVP_R_BUFFER_TOO_SMALL);
        return 0;
      }
      return HKDF_extract(out, out_len, hctx->md, hctx->key.data(),
                          hctx->key.size(), hctx->salt.data(),
                          hctx->salt.size());

    case EVP_PKEY_HKDEF_MODE_EXPAND_ONLY:
      return HKDF_expand(out, *out_len, hctx->md, hctx->key.data(),
                         hctx->key.size(), hctx->info.data(),
                         hctx->info.size());
  }
  OPENSSL_PUT_ERROR(EVP, ERR_R_INTERNAL_ERROR);
  return 0;
}

static int pkey_hkdf_ctrl(EvpPkeyCtx *ctx, int type, int p1, void *p2) {
  HKDF_PKEY_CTX *hctx = reinterpret_cast<HKDF_PKEY_CTX *>(ctx->data);
  switch (type) {
    case EVP_PKEY_CTRL_HKDF_MODE:
      if (p1 != EVP_PKEY_HKDEF_MODE_EXTRACT_AND_EXPAND &&
          p1 != EVP_PKEY_HKDEF_MODE_EXTRACT_ONLY &&
          p1 != EVP_PKEY_HKDEF_MODE_EXPAND_ONLY) {
        OPENSSL_PUT_ERROR(EVP, EVP_R_INVALID_OPERATION);
        return 0;
      }
      hctx->mode = p1;
      return 1;
    case EVP_PKEY_CTRL_HKDF_MD:
      hctx->md = reinterpret_cast<const EVP_MD *>(p2);
      return 1;
    case EVP_PKEY_CTRL_HKDF_KEY: {
      const auto *key = reinterpret_cast<const Span<const uint8_t> *>(p2);
      return hctx->key.CopyFrom(*key);
    }
    case EVP_PKEY_CTRL_HKDF_SALT: {
      const auto *salt = reinterpret_cast<const Span<const uint8_t> *>(p2);
      return hctx->salt.CopyFrom(*salt);
    }
    case EVP_PKEY_CTRL_HKDF_INFO: {
      const auto *info = reinterpret_cast<const Span<const uint8_t> *>(p2);
      // |EVP_PKEY_CTX_add1_hkdf_info| appends to the info string, rather than
      // replacing it.
      return hctx->info.Append(*info);
    }
    default:
      OPENSSL_PUT_ERROR(EVP, EVP_R_COMMAND_NOT_SUPPORTED);
      return 0;
  }
}

const EVP_PKEY_CTX_METHOD hkdf_pkey_meth = {
    EVP_PKEY_HKDF,
    pkey_hkdf_init,
    pkey_hkdf_copy,
    pkey_hkdf_cleanup,
    /*keygen=*/nullptr,
    /*sign=*/nullptr,
    /*sign_message=*/nullptr,
    /*verify=*/nullptr,
    /*verify_message=*/nullptr,
    /*verify_recover=*/nullptr,
    /*encrypt=*/nullptr,
    /*decrypt=*/nullptr,
    pkey_hkdf_derive,
    /*paramgen=*/nullptr,
    /*encap=*/nullptr,
    /*decap=*/nullptr,
    pkey_hkdf_ctrl,
};

}  // namespace

const EVP_PKEY_ALG *bssl::evp_pkey_hkdf() {
  static const EVP_PKEY_ALG kAlg = {nullptr, &hkdf_pkey_meth};
  return &kAlg;
}

int EVP_PKEY_CTX_hkdf_mode(EVP_PKEY_CTX *ctx, int mode) {
  return EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_HKDF, EVP_PKEY_OP_DERIVE,
                           EVP_PKEY_CTRL_HKDF_MODE, mode, nullptr);
}

int EVP_PKEY_CTX_set_hkdf_md(EVP_PKEY_CTX *ctx, const EVP_MD *md) {
  return EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_HKDF, EVP_PKEY_OP_DERIVE,
                           EVP_PKEY_CTRL_HKDF_MD, 0, (void *)md);
}

int EVP_PKEY_CTX_set1_hkdf_key(EVP_PKEY_CTX *ctx, const uint8_t *key,
                               size_t key_len) {
  Span<const uint8_t> span(key, key_len);
  return EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_HKDF, EVP_PKEY_OP_DERIVE,
                           EVP_PKEY_CTRL_HKDF_KEY, 0, &span);
}

int EVP_PKEY_CTX_set1_hkdf_salt(EVP_PKEY_CTX *ctx, const uint8_t *salt,
                                size_t salt_len) {
  Span<const uint8_t> span(salt, salt_len);
  return EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_HKDF, EVP_PKEY_OP_DERIVE,
                           EVP_PKEY_CTRL_HKDF_SALT, 0, &span);
}

int EVP_PKEY_CTX_add1_hkdf_info(EVP_PKEY_CTX *ctx, const uint8_t *info,
                                size_t info_len) {
  Span<const uint8_t> span(info, info_len);
  return EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_HKDF, EVP_PKEY_OP_DERIVE,
                           EVP_PKEY_CTRL_HKDF_INFO, 0, &span);
}
