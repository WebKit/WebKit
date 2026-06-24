// Copyright 2014 The BoringSSL Authors
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

#include <openssl/aead.h>

#include <optional>

#include <assert.h>
#include <string.h>

#include <openssl/cipher.h>
#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/span.h>

#include "../../internal.h"
#include "../../mem_internal.h"
#include "internal.h"


using namespace bssl;

size_t EVP_AEAD_key_length(const EVP_AEAD *aead) { return aead->key_len; }

size_t EVP_AEAD_nonce_length(const EVP_AEAD *aead) { return aead->nonce_len; }

size_t EVP_AEAD_max_overhead(const EVP_AEAD *aead) { return aead->overhead; }

size_t EVP_AEAD_max_tag_len(const EVP_AEAD *aead) { return aead->max_tag_len; }

void EVP_AEAD_CTX_zero(EVP_AEAD_CTX *ctx) {
  OPENSSL_memset(ctx, 0, sizeof(EVP_AEAD_CTX));
}

EVP_AEAD_CTX *EVP_AEAD_CTX_new(const EVP_AEAD *aead, const uint8_t *key,
                               size_t key_len, size_t tag_len) {
  EVP_AEAD_CTX *ctx = New<EVP_AEAD_CTX>();
  if (!ctx) {
    return nullptr;
  }
  EVP_AEAD_CTX_zero(ctx);

  if (EVP_AEAD_CTX_init(ctx, aead, key, key_len, tag_len, nullptr)) {
    return ctx;
  }

  EVP_AEAD_CTX_free(ctx);
  return nullptr;
}

void EVP_AEAD_CTX_free(EVP_AEAD_CTX *ctx) {
  if (ctx == nullptr) {
    return;
  }
  EVP_AEAD_CTX_cleanup(ctx);
  Delete(ctx);
}

int EVP_AEAD_CTX_init(EVP_AEAD_CTX *ctx, const EVP_AEAD *aead,
                      const uint8_t *key, size_t key_len, size_t tag_len,
                      ENGINE *impl) {
  if (!aead->init) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_NO_DIRECTION_SET);
    ctx->aead = nullptr;
    return 0;
  }
  return EVP_AEAD_CTX_init_with_direction(ctx, aead, key, key_len, tag_len,
                                          evp_aead_open);
}

int EVP_AEAD_CTX_init_with_direction(EVP_AEAD_CTX *ctx, const EVP_AEAD *aead,
                                     const uint8_t *key, size_t key_len,
                                     size_t tag_len,
                                     enum evp_aead_direction_t dir) {
  if (key_len != aead->key_len) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_UNSUPPORTED_KEY_SIZE);
    ctx->aead = nullptr;
    return 0;
  }

  ctx->aead = aead;

  int ok;
  if (aead->init) {
    ok = aead->init(ctx, key, key_len, tag_len);
  } else {
    ok = aead->init_with_direction(ctx, key, key_len, tag_len, dir);
  }

  if (!ok) {
    ctx->aead = nullptr;
  }

  return ok;
}

void EVP_AEAD_CTX_cleanup(EVP_AEAD_CTX *ctx) {
  if (ctx->aead == nullptr) {
    return;
  }
  ctx->aead->cleanup(ctx);
  ctx->aead = nullptr;
}

// check_alias returns 1 if |out| is compatible with |in| and 0 otherwise. If
// |in| and |out| alias, we require that |in| == |out|.
static int check_alias(const uint8_t *in, size_t in_len, const uint8_t *out,
                       size_t out_len) {
  if (!buffers_alias(in, in_len, out, out_len)) {
    return 1;
  }

  return in == out;
}

int EVP_AEAD_CTX_seal(const EVP_AEAD_CTX *ctx, uint8_t *out, size_t *out_len,
                      size_t max_out_len, const uint8_t *nonce,
                      size_t nonce_len, const uint8_t *in, size_t in_len,
                      const uint8_t *ad, size_t ad_len) {
  bool ok = false;
  Cleanup cleanup([&] {
    if (!ok) {
      // In the event of an error, clear the output buffer so that a caller
      // that doesn't check the return value doesn't send raw data.
      OPENSSL_memset(out, 0, max_out_len);
      *out_len = 0;
    }
  });

  if (max_out_len < in_len) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BUFFER_TOO_SMALL);
    return 0;
  }

  CRYPTO_IOVEC iovec[1];
  iovec[0].in = in;
  iovec[0].out = out;
  iovec[0].len = in_len;
  CRYPTO_IVEC aadvec[1];
  aadvec[0].in = ad;
  aadvec[0].len = ad_len;
  if (!EVP_AEAD_CTX_sealv(ctx, iovec, 1, out + in_len, out_len,
                          max_out_len - in_len, nonce, nonce_len, aadvec, 1)) {
    *out_len = 0;
    return 0;
  }
  *out_len += in_len;
  ok = true;
  return 1;
}

int EVP_AEAD_CTX_seal_scatter(const EVP_AEAD_CTX *ctx, uint8_t *out,
                              uint8_t *out_tag, size_t *out_tag_len,
                              size_t max_out_tag_len, const uint8_t *nonce,
                              size_t nonce_len, const uint8_t *in,
                              size_t in_len, const uint8_t *extra_in,
                              size_t extra_in_len, const uint8_t *ad,
                              size_t ad_len) {
  bool ok = false;
  Cleanup cleanup([&] {
    if (!ok) {
      // In the event of an error, clear the output buffer so that a caller
      // that doesn't check the return value doesn't send raw data.
      OPENSSL_memset(out, 0, in_len);
      OPENSSL_memset(out_tag, 0, max_out_tag_len);
      *out_tag_len = 0;
    }
  });

  // |out_tag| contains both the encryption of |extra_in| and the tag.
  Span<uint8_t> out_tag_span(out_tag, max_out_tag_len);
  if (out_tag_span.size() < extra_in_len) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BUFFER_TOO_SMALL);
    return 0;
  }
  Span<uint8_t> extra_out = out_tag_span.first(extra_in_len);
  Span<uint8_t> tag_only_out = out_tag_span.subspan(extra_in_len);

  CRYPTO_IOVEC iovec[2];
  iovec[0].in = in;
  iovec[0].out = out;
  iovec[0].len = in_len;
  iovec[1].in = extra_in;
  iovec[1].out = extra_out.data();
  iovec[1].len = extra_in_len;
  CRYPTO_IVEC aadvec[1];
  aadvec[0].in = ad;
  aadvec[0].len = ad_len;
  if (!EVP_AEAD_CTX_sealv(ctx, iovec, extra_in_len ? 2 : 1, tag_only_out.data(),
                          out_tag_len, tag_only_out.size(), nonce, nonce_len,
                          aadvec, 1)) {
    *out_tag_len = 0;
    return 0;
  }
  *out_tag_len += extra_in_len;
  ok = true;
  return 1;
}

static bool check_iovec_internal_alias(Span<const CRYPTO_IOVEC> iovecs) {
  for (size_t i = 0; i < iovecs.size(); ++i) {
    // Same index check.
    if (!check_alias(iovecs[i].in, iovecs[i].len, iovecs[i].out,
                     iovecs[i].len)) {
      return false;
    }
#if !defined(NDEBUG)
    // Unrealistic cases; they'd be harmful but also extremely unlikely anyone
    // will ever get those wrong. Thus skip them in release builds.
    for (size_t j = i + 1; j < iovecs.size(); ++j) {
      if (buffers_alias(iovecs[i].in, iovecs[i].len,  //
                        iovecs[j].out, iovecs[j].len) ||
          buffers_alias(iovecs[i].out, iovecs[i].len,  //
                        iovecs[j].in, iovecs[j].len) ||
          buffers_alias(iovecs[i].out, iovecs[i].len,  //
                        iovecs[j].out, iovecs[j].len)) {
        return false;
      }
    }
#endif
  }
  return true;
}

#if !defined(NDEBUG)
static bool check_ivec_buf_alias(Span<const CRYPTO_IVEC> ivecs,
                                 const uint8_t *buf, size_t buf_len) {
  for (const CRYPTO_IVEC &ivec : ivecs) {
    if (buffers_alias(ivec.in, ivec.len, buf, buf_len)) {
      return false;
    }
  }
  return true;
}

static bool check_iovec_out_ivec_alias(Span<const CRYPTO_IOVEC> iovecs,
                                       Span<const CRYPTO_IVEC> ivecs) {
  for (const CRYPTO_IOVEC &iovec : iovecs) {
    if (!check_ivec_buf_alias(ivecs, iovec.out, iovec.len)) {
      return false;
    }
  }
  return true;
}

static bool check_iovec_buf_alias(Span<const CRYPTO_IOVEC> iovecs,
                                  const uint8_t *buf, size_t buf_len) {
  for (const CRYPTO_IOVEC &iovec : iovecs) {
    if (buffers_alias(iovec.in, iovec.len, buf, buf_len)) {
      return false;
    }
    if (buffers_alias(iovec.out, iovec.len, buf, buf_len)) {
      return false;
    }
  }
  return true;
}

static bool check_iovec_out_buf_alias(Span<const CRYPTO_IOVEC> iovecs,
                                      const uint8_t *buf, size_t buf_len) {
  for (const CRYPTO_IOVEC &iovec : iovecs) {
    if (buffers_alias(iovec.out, iovec.len, buf, buf_len)) {
      return false;
    }
  }
  return true;
}
#endif

static bool check_iovec_alias(Span<const CRYPTO_IOVEC> iovecs,
                              Span<const CRYPTO_IVEC> aadvecs,
                              const uint8_t *out, size_t out_len,
                              const uint8_t *in1, size_t in1_len,
                              const uint8_t *in2, size_t in2_len) {
  return
#if !defined(NDEBUG)
      // Unrealistic cases; they'd be harmful but also extremely unlikely anyone
      // will ever get those wrong. Thus skip them in release builds.
      //
      // iovec.out <-> aadvec.
      check_iovec_out_ivec_alias(iovecs, aadvecs) &&
      // iovec <-> out.
      check_iovec_buf_alias(iovecs, out, out_len) &&
      // iovec.out <-> in1.
      check_iovec_out_buf_alias(iovecs, in1, in1_len) &&
      // iovec.out <-> in2.
      check_iovec_out_buf_alias(iovecs, in2, in2_len) &&
      // aadvec <-> out.
      check_ivec_buf_alias(aadvecs, out, out_len) &&
      // out <-> in1.
      !buffers_alias(out, out_len, in1, in1_len) &&
      // out <-> in2.
      !buffers_alias(out, out_len, in2, in2_len) &&
#endif
      // iovec <-> iovec.
      check_iovec_internal_alias(iovecs);
}

static void clear_iovec(Span<const CRYPTO_IOVEC> iovecs) {
  for (const CRYPTO_IOVEC &iovec : iovecs) {
    OPENSSL_memset(iovec.out, 0, iovec.len);
  }
}

int EVP_AEAD_CTX_sealv(const EVP_AEAD_CTX *ctx, const CRYPTO_IOVEC *iovec,
                       size_t num_iovec, uint8_t *out_tag, size_t *out_tag_len,
                       size_t max_out_tag_len, const uint8_t *nonce,
                       size_t nonce_len, const CRYPTO_IVEC *aadvec,
                       size_t num_aadvec) {
  Span<const CRYPTO_IOVEC> iovecs(iovec, num_iovec);
  Span<const CRYPTO_IVEC> aadvecs(aadvec, num_aadvec);

  bool ok = false;
  Cleanup cleanup([&] {
    if (!ok) {
      // In the event of an error, clear the output buffer so that a caller
      // that doesn't check the return value doesn't send raw data.
      clear_iovec(iovecs);
      OPENSSL_memset(out_tag, 0, max_out_tag_len);
      *out_tag_len = 0;
    }
  });

  if (!bssl::iovec::IsValid(iovecs) || !bssl::iovec::IsValid(aadvecs)) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_TOO_LARGE);
    return 0;
  }

  // Enforce aliasing rules: no output may alias any input, with the one
  // exception that an iovec member's |in| and |out| pointers may be identical
  // for in-place operation.
  if (!check_iovec_alias(iovecs, aadvecs, out_tag, max_out_tag_len, nonce,
                         nonce_len, nullptr, 0)) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_OUTPUT_ALIASES_INPUT);
    return 0;
  }

  if (!ctx->aead->sealv) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_CTRL_NOT_IMPLEMENTED);
    return 0;
  }

  if (ctx->aead->sealv(ctx, iovecs, Span(out_tag, max_out_tag_len), out_tag_len,
                       Span(nonce, nonce_len), aadvecs)) {
    ok = true;
    return 1;
  }

  return 0;
}

int EVP_AEAD_CTX_open(const EVP_AEAD_CTX *ctx, uint8_t *out, size_t *out_len,
                      size_t max_out_len, const uint8_t *nonce,
                      size_t nonce_len, const uint8_t *in, size_t in_len,
                      const uint8_t *ad, size_t ad_len) {
  bool ok = false;
  Cleanup cleanup([&] {
    if (!ok) {
      // In the event of an error, clear the output buffer so that a caller
      // that doesn't check the return value doesn't try and process bad
      // data.
      OPENSSL_memset(out, 0, max_out_len);
      *out_len = 0;
    }
  });

  if (ctx->tag_len) {
    // If the tag length is known, the caller only needs to provide enough
    // space for in_len - tag_len.
    if (in_len < ctx->tag_len) {
      OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BAD_DECRYPT);
      return 0;
    }
    size_t plaintext_len = in_len - ctx->tag_len;
    if (max_out_len < plaintext_len) {
      OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BUFFER_TOO_SMALL);
      return 0;
    }

    CRYPTO_IOVEC iovec[1];
    iovec[0].in = in;
    iovec[0].out = out;
    iovec[0].len = plaintext_len;
    CRYPTO_IVEC aadvec[1];
    aadvec[0].in = ad;
    aadvec[0].len = ad_len;
    if (!EVP_AEAD_CTX_openv_detached(ctx, iovec, 1, nonce, nonce_len,
                                     in + plaintext_len, ctx->tag_len, aadvec,
                                     1)) {
      return 0;
    }
    *out_len = plaintext_len;
    ok = true;
    return 1;
  }

  if (max_out_len < in_len) {
    // Variable tag length AEADs need to be able to decrypt the entire
    // plaintext before they can split it up. So the caller has to provide
    // sufficient max_out_len for temporary data.
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BUFFER_TOO_SMALL);
    return 0;
  }
  CRYPTO_IOVEC iovec[1];
  iovec[0].in = in;
  iovec[0].out = out;
  iovec[0].len = in_len;
  CRYPTO_IVEC aadvec[1];
  aadvec[0].in = ad;
  aadvec[0].len = ad_len;
  if (!EVP_AEAD_CTX_openv(ctx, iovec, 1, out_len, nonce, nonce_len, aadvec,
                          1)) {
    return 0;
  }
  ok = true;
  return 1;
}

int EVP_AEAD_CTX_open_gather(const EVP_AEAD_CTX *ctx, uint8_t *out,
                             const uint8_t *nonce, size_t nonce_len,
                             const uint8_t *in, size_t in_len,
                             const uint8_t *in_tag, size_t in_tag_len,
                             const uint8_t *ad, size_t ad_len) {
  bool ok = false;
  Cleanup cleanup([&] {
    if (!ok) {
      // In the event of an error, clear the output buffer so that a caller
      // that doesn't check the return value doesn't try and process bad
      // data.
      OPENSSL_memset(out, 0, in_len);
    }
  });

  CRYPTO_IOVEC iovec[1];
  iovec[0].in = in;
  iovec[0].out = out;
  iovec[0].len = in_len;
  CRYPTO_IVEC aadvec[1];
  aadvec[0].in = ad;
  aadvec[0].len = ad_len;
  if (!EVP_AEAD_CTX_openv_detached(ctx, iovec, 1, nonce, nonce_len, in_tag,
                                   in_tag_len, aadvec, 1)) {
    return 0;
  }
  ok = true;
  return 1;
}

int EVP_AEAD_CTX_openv(const EVP_AEAD_CTX *ctx, const CRYPTO_IOVEC *iovec,
                       size_t num_iovec, size_t *out_total_bytes,
                       const uint8_t *nonce, size_t nonce_len,
                       const CRYPTO_IVEC *aadvec, size_t num_aadvec) {
  Span<const CRYPTO_IOVEC> iovecs(iovec, num_iovec);
  Span<const CRYPTO_IVEC> aadvecs(aadvec, num_aadvec);

  bool ok = false;
  Cleanup cleanup([&] {
    if (!ok) {
      // In the event of an error, clear the output buffer so that a caller
      // that doesn't check the return value doesn't try and process bad
      // data.
      clear_iovec(iovecs);
      *out_total_bytes = 0;
    }
  });

  if (!bssl::iovec::IsValid(iovecs) || !bssl::iovec::IsValid(aadvecs)) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_TOO_LARGE);
    return 0;
  }

  // Enforce aliasing rules: no output may alias any input, with the one
  // exception that an iovec member's |in| and |out| pointers may be identical
  // for in-place operation.
  if (!check_iovec_alias(iovecs, aadvecs, nullptr, 0, nonce, nonce_len, nullptr,
                         0)) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_OUTPUT_ALIASES_INPUT);
    return 0;
  }

  if (!ctx->aead->openv) {
    if (ctx->tag_len && ctx->aead->openv_detached) {
      // Try with a detached tag.
      InplaceVector<CRYPTO_IOVEC, CRYPTO_IOVEC_MAX> detached_iovecs;
      detached_iovecs.CopyFrom(iovecs);

      uint8_t tagbuf[EVP_AEAD_MAX_OVERHEAD];
      std::optional<Span<const uint8_t>> tag = bssl::iovec::GetAndRemoveSuffix(
          Span(tagbuf).first(ctx->tag_len), Span(detached_iovecs));

      if (!tag.has_value()) {  // I.e. no |ctx->tag_len| bytes available.
        OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BAD_DECRYPT);
        return 0;
      }

      if (ctx->aead->openv_detached(ctx, detached_iovecs,
                                    Span(nonce, nonce_len), *tag, aadvecs)) {
        ok = true;
        *out_total_bytes = bssl::iovec::TotalLength(Span(detached_iovecs));
        return 1;
      }
      return 0;
    }

    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_CTRL_NOT_IMPLEMENTED);
    return 0;
  }

  if (ctx->aead->openv(ctx, iovecs, out_total_bytes, Span(nonce, nonce_len),
                       aadvecs)) {
    ok = true;
    return 1;
  }

  return 0;
}

int EVP_AEAD_CTX_openv_detached(const EVP_AEAD_CTX *ctx,
                                const CRYPTO_IOVEC *iovec, size_t num_iovec,
                                const uint8_t *nonce, size_t nonce_len,
                                const uint8_t *in_tag, size_t in_tag_len,
                                const CRYPTO_IVEC *aadvec, size_t num_aadvec) {
  Span<const CRYPTO_IOVEC> iovecs(iovec, num_iovec);
  Span<const CRYPTO_IVEC> aadvecs(aadvec, num_aadvec);

  bool ok = false;
  Cleanup cleanup([&] {
    if (!ok) {
      // In the event of an error, clear the output buffer so that a caller
      // that doesn't check the return value doesn't try and process bad
      // data.
      clear_iovec(iovecs);
    }
  });

  if (!bssl::iovec::IsValid(iovecs) || !bssl::iovec::IsValid(aadvecs)) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_TOO_LARGE);
    return 0;
  }
  if (in_tag_len > EVP_AEAD_MAX_OPEN_OVERHEAD) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_UNSUPPORTED_TAG_SIZE);
    return 0;
  }

  // Enforce aliasing rules: no output may alias any input, with the one
  // exception that an iovec member's |in| and |out| pointers may be identical
  // for in-place operation.
  if (!check_iovec_alias(iovecs, aadvecs, nullptr, 0, nonce, nonce_len, in_tag,
                         in_tag_len)) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_OUTPUT_ALIASES_INPUT);
    return 0;
  }

  if (!ctx->aead->openv_detached) {
    // AEADs with variable overhead may provide openv instead of openv_detached.
    // While one might call openv and then, on success, discard the result if
    // the length was wrong, this requires callers to predict the plaintext
    // length first. We do not expect callers to do this, especially in the TLS
    // CBC construction, where this length is sensitive to the Lucky 13 attack.
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_CTRL_NOT_IMPLEMENTED);
    return 0;
  }

  if (ctx->aead->openv_detached(ctx, iovecs, Span(nonce, nonce_len),
                                Span(in_tag, in_tag_len), aadvecs)) {
    ok = true;
    return 1;
  }

  return 0;
}

const EVP_AEAD *EVP_AEAD_CTX_aead(const EVP_AEAD_CTX *ctx) { return ctx->aead; }

int EVP_AEAD_CTX_get_iv(const EVP_AEAD_CTX *ctx, const uint8_t **out_iv,
                        size_t *out_len) {
  if (ctx->aead->get_iv == nullptr) {
    OPENSSL_PUT_ERROR(CIPHER, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
    return 0;
  }

  return ctx->aead->get_iv(ctx, out_iv, out_len);
}

int EVP_AEAD_CTX_tag_len(const EVP_AEAD_CTX *ctx, size_t *out_tag_len,
                         const size_t in_len, const size_t extra_in_len) {
  size_t tag_len;
  if (ctx->aead->tag_len) {
    if (in_len + extra_in_len < in_len) {
      OPENSSL_PUT_ERROR(CIPHER, ERR_R_OVERFLOW);
      *out_tag_len = 0;
      return 0;
    }
    tag_len = ctx->aead->tag_len(ctx, in_len + extra_in_len);
  } else {
    tag_len = ctx->tag_len;
  }

  if (extra_in_len + tag_len < extra_in_len) {
    OPENSSL_PUT_ERROR(CIPHER, ERR_R_OVERFLOW);
    *out_tag_len = 0;
    return 0;
  }
  *out_tag_len = extra_in_len + tag_len;
  return 1;
}
