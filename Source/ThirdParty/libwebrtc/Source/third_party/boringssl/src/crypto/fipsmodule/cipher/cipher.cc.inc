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

#include <openssl/cipher.h>

#include <assert.h>
#include <limits.h>
#include <string.h>

#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/nid.h>
#include <openssl/span.h>

#include "../../internal.h"
#include "../../mem_internal.h"
#include "../service_indicator/internal.h"
#include "internal.h"


using namespace bssl;

void EVP_CIPHER_CTX_init(EVP_CIPHER_CTX *ctx) {
  OPENSSL_memset(ctx, 0, sizeof(EVP_CIPHER_CTX));
}

EVP_CIPHER_CTX *EVP_CIPHER_CTX_new() {
  EVP_CIPHER_CTX *ctx = New<EVP_CIPHER_CTX>();
  if (ctx) {
    EVP_CIPHER_CTX_init(ctx);
  }
  return ctx;
}

int EVP_CIPHER_CTX_cleanup(EVP_CIPHER_CTX *c) {
  if (c->cipher != nullptr && c->cipher->cleanup) {
    c->cipher->cleanup(c);
  }
  OPENSSL_free(c->cipher_data);

  OPENSSL_memset(c, 0, sizeof(EVP_CIPHER_CTX));
  return 1;
}

void EVP_CIPHER_CTX_free(EVP_CIPHER_CTX *ctx) {
  if (ctx) {
    EVP_CIPHER_CTX_cleanup(ctx);
    Delete(ctx);
  }
}

int EVP_CIPHER_CTX_copy(EVP_CIPHER_CTX *out, const EVP_CIPHER_CTX *in) {
  if (in == nullptr || in->cipher == nullptr) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_INPUT_NOT_INITIALIZED);
    return 0;
  }

  if (in->poisoned) {
    OPENSSL_PUT_ERROR(CIPHER, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
    return 0;
  }

  EVP_CIPHER_CTX_cleanup(out);
  OPENSSL_memcpy(out, in, sizeof(EVP_CIPHER_CTX));

  if (in->cipher_data && in->cipher->ctx_size) {
    out->cipher_data = OPENSSL_memdup(in->cipher_data, in->cipher->ctx_size);
    if (!out->cipher_data) {
      out->cipher = nullptr;
      return 0;
    }
  }

  if (in->cipher->flags & EVP_CIPH_CUSTOM_COPY) {
    if (!in->cipher->ctrl((EVP_CIPHER_CTX *)in, EVP_CTRL_COPY, 0, out)) {
      out->cipher = nullptr;
      return 0;
    }
  }

  return 1;
}

int EVP_CIPHER_CTX_reset(EVP_CIPHER_CTX *ctx) {
  EVP_CIPHER_CTX_cleanup(ctx);
  EVP_CIPHER_CTX_init(ctx);
  return 1;
}

int EVP_CipherInit_ex(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher,
                      ENGINE *engine, const uint8_t *key, const uint8_t *iv,
                      int enc) {
  if (enc == -1) {
    enc = ctx->encrypt;
  } else {
    if (enc) {
      enc = 1;
    }
    ctx->encrypt = enc;
  }

  if (cipher) {
    // Ensure a context left from last time is cleared (the previous check
    // attempted to avoid this if the same ENGINE and EVP_CIPHER could be
    // used).
    if (ctx->cipher) {
      EVP_CIPHER_CTX_cleanup(ctx);
      // Restore encrypt and flags
      ctx->encrypt = enc;
    }

    ctx->cipher = cipher;
    if (ctx->cipher->ctx_size) {
      ctx->cipher_data = OPENSSL_malloc(ctx->cipher->ctx_size);
      if (!ctx->cipher_data) {
        ctx->cipher = nullptr;
        return 0;
      }
    } else {
      ctx->cipher_data = nullptr;
    }

    ctx->key_len = cipher->key_len;
    ctx->flags = 0;

    if (ctx->cipher->flags & EVP_CIPH_CTRL_INIT) {
      if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_INIT, 0, nullptr)) {
        ctx->cipher = nullptr;
        OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_INITIALIZATION_ERROR);
        return 0;
      }
    }
  } else if (!ctx->cipher) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_NO_CIPHER_SET);
    return 0;
  }

  // we assume block size is a power of 2 in *cryptUpdate
  assert(ctx->cipher->block_size == 1 || ctx->cipher->block_size == 8 ||
         ctx->cipher->block_size == 16);

  if (!(EVP_CIPHER_CTX_flags(ctx) & EVP_CIPH_CUSTOM_IV)) {
    switch (EVP_CIPHER_CTX_mode(ctx)) {
      case EVP_CIPH_STREAM_CIPHER:
      case EVP_CIPH_ECB_MODE:
        break;

      case EVP_CIPH_CFB_MODE:
        ctx->num = 0;
        [[fallthrough]];

      case EVP_CIPH_CBC_MODE:
        assert(EVP_CIPHER_CTX_iv_length(ctx) <= sizeof(ctx->iv));
        if (iv) {
          OPENSSL_memcpy(ctx->oiv, iv, EVP_CIPHER_CTX_iv_length(ctx));
        }
        OPENSSL_memcpy(ctx->iv, ctx->oiv, EVP_CIPHER_CTX_iv_length(ctx));
        break;

      case EVP_CIPH_CTR_MODE:
      case EVP_CIPH_OFB_MODE:
        ctx->num = 0;
        // Don't reuse IV for CTR mode
        if (iv) {
          OPENSSL_memcpy(ctx->iv, iv, EVP_CIPHER_CTX_iv_length(ctx));
        }
        break;

      default:
        return 0;
    }
  }

  if (key || (ctx->cipher->flags & EVP_CIPH_ALWAYS_CALL_INIT)) {
    if (!ctx->cipher->init(ctx, key, iv, enc)) {
      return 0;
    }
  }

  ctx->buf_len = 0;
  ctx->final_used = 0;
  // Clear the poisoned flag to permit reuse of a CTX that previously had a
  // failed operation.
  ctx->poisoned = 0;
  return 1;
}

int EVP_EncryptInit_ex(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher,
                       ENGINE *impl, const uint8_t *key, const uint8_t *iv) {
  return EVP_CipherInit_ex(ctx, cipher, impl, key, iv, 1);
}

int EVP_DecryptInit_ex(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher,
                       ENGINE *impl, const uint8_t *key, const uint8_t *iv) {
  return EVP_CipherInit_ex(ctx, cipher, impl, key, iv, 0);
}

// block_remainder returns the number of bytes to remove from |len| to get a
// multiple of |ctx|'s block size.
static size_t block_remainder(const EVP_CIPHER_CTX *ctx, size_t len) {
  // |block_size| must be a power of two.
  assert(ctx->cipher->block_size != 0);
  assert((ctx->cipher->block_size & (ctx->cipher->block_size - 1)) == 0);
  return len & (ctx->cipher->block_size - 1);
}

int EVP_EncryptUpdate(EVP_CIPHER_CTX *ctx, uint8_t *out, int *out_len,
                      const uint8_t *in, int in_len) {
  *out_len = 0;
  if (in_len < 0) {
    OPENSSL_PUT_ERROR(CIPHER, ERR_R_OVERFLOW);
    return 0;
  }
  size_t in_len_sz = static_cast<size_t>(in_len);
  size_t out_len_sz;
  if ((ctx->cipher->flags & EVP_CIPH_FLAG_CUSTOM_CIPHER) && out == nullptr) {
    if (!EVP_CipherUpdateAAD(ctx, in, in_len_sz)) {
      return 0;
    }
    out_len_sz = in_len_sz;  // Even though no output was written!
  } else {
    // in_len_sz is < INT_MAX which is no more than half of SIZE_MAX.
    size_t max_out_len =
        std::min(in_len_sz + ctx->cipher->block_size - 1, size_t{INT_MAX});
    if (!EVP_EncryptUpdate_ex(ctx, out, &out_len_sz, max_out_len, in,
                              in_len_sz)) {
      return 0;
    }
  }
  *out_len = static_cast<int>(out_len_sz);
  return 1;
}

template <typename F>
static int WrapWithPoison(EVP_CIPHER_CTX *ctx, F f) {
  if (ctx->poisoned) {
    // |ctx| has been left in an indeterminate state by a previous failed
    // operation. Do not allow proceeding.
    OPENSSL_PUT_ERROR(CIPHER, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
    return 0;
  }
  if (!f()) {
    // Functions using |WrapWithPoison| may leave |ctx| in an indeterminate
    // state. Mark the object as poisoned.
    ctx->poisoned = 1;
    return 0;
  }
  return 1;
}

static int EVP_EncryptUpdate_ex_internal(EVP_CIPHER_CTX *ctx, uint8_t *out,
                                         size_t *out_len, size_t max_out_len,
                                         const uint8_t *in, size_t in_len) {
  *out_len = 0;

  // Ciphers that use blocks may write up to |block_size| extra bytes. Ensure
  // the output does not overflow |*out_len|.
  Span<const uint8_t> in_span(in, in_len);
  size_t block_size = ctx->cipher->block_size;

  if (in_span.empty()) {
    return 1;
  }

  size_t buf_len = ctx->buf_len;
  assert(block_size <= sizeof(ctx->buf));
  Span<uint8_t> out_span(out, max_out_len);
  if (buf_len != 0) {
    if (block_size - buf_len > in_span.size()) {
      CopyToPrefix(in_span, Span(ctx->buf).subspan(buf_len));
      ctx->buf_len += in_span.size();
      return 1;
    } else {
      size_t j = block_size - buf_len;
      CopyToPrefix(in_span.first(j), Span(ctx->buf).subspan(buf_len));
      if (out_span.size() < block_size) {
        OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BUFFER_TOO_SMALL);
        return 0;
      }
      if (!ctx->cipher->cipher_update(ctx, out_span.data(), ctx->buf,
                                      block_size)) {
        return 0;
      }
      in_span = in_span.subspan(j);
      out_span = out_span.subspan(block_size);
    }
  }

  size_t whole_blocks = in_span.size() - block_remainder(ctx, in_span.size());
  if (whole_blocks > 0) {
    if (out_span.size() < whole_blocks) {
      OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BUFFER_TOO_SMALL);
      return 0;
    }
    if (!ctx->cipher->cipher_update(ctx, out_span.data(), in_span.data(),
                                    whole_blocks)) {
      return 0;
    }
    in_span = in_span.subspan(whole_blocks);
    out_span = out_span.subspan(whole_blocks);
  }

  assert(in_span.size() < block_size);
  CopyToPrefix(in_span, ctx->buf);
  ctx->buf_len = in_span.size();

  *out_len = max_out_len - out_span.size();
  return 1;
}

int EVP_EncryptUpdate_ex(EVP_CIPHER_CTX *ctx, uint8_t *out, size_t *out_len,
                         size_t max_out_len, const uint8_t *in, size_t in_len) {
  *out_len = 0;
  return WrapWithPoison(ctx, [&] {
    return EVP_EncryptUpdate_ex_internal(ctx, out, out_len, max_out_len, in,
                                         in_len);
  });
}

int EVP_EncryptFinal_ex(EVP_CIPHER_CTX *ctx, uint8_t *out, int *out_len) {
  size_t out_len_sz;
  int ret =
      EVP_EncryptFinal_ex2(ctx, out, &out_len_sz, ctx->cipher->block_size);
  static_assert(EVP_MAX_BLOCK_LENGTH <= INT_MAX);
  *out_len = static_cast<int>(out_len_sz);
  return ret;
}

static int EVP_EncryptFinal_ex2_internal(EVP_CIPHER_CTX *ctx, uint8_t *out,
                                         size_t *out_len, size_t max_out_len) {
  *out_len = 0;

  size_t block_size = ctx->cipher->block_size;
  assert(block_size <= sizeof(ctx->buf));
  if (block_size == 1) {
    if (ctx->cipher->flags & EVP_CIPH_FLAG_CUSTOM_CIPHER) {
      return ctx->cipher->cipher_final(ctx);
    }
    return 1;
  }

  size_t buf_len = ctx->buf_len;
  if (ctx->flags & EVP_CIPH_NO_PADDING) {
    if (buf_len) {
      OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_DATA_NOT_MULTIPLE_OF_BLOCK_LENGTH);
      return 0;
    }
    return 1;
  }

  size_t padding = block_size - buf_len;
  for (size_t i = buf_len; i < block_size; i++) {
    ctx->buf[i] = padding;
  }

  Span<uint8_t> out_span(out, max_out_len);
  if (out_span.size() < block_size) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BUFFER_TOO_SMALL);
    return 0;
  }
  if (!ctx->cipher->cipher_update(ctx, out_span.data(), ctx->buf, block_size)) {
    return 0;
  }
  out_span = out_span.subspan(block_size);

  *out_len = max_out_len - out_span.size();
  return 1;
}

int EVP_EncryptFinal_ex2(EVP_CIPHER_CTX *ctx, uint8_t *out, size_t *out_len,
                         size_t max_out_len) {
  *out_len = 0;
  return WrapWithPoison(ctx, [&] {
    if (!EVP_EncryptFinal_ex2_internal(ctx, out, out_len, max_out_len)) {
      return 0;
    }
    EVP_Cipher_verify_service_indicator(ctx);
    return 1;
  });
}

int EVP_DecryptUpdate(EVP_CIPHER_CTX *ctx, uint8_t *out, int *out_len,
                      const uint8_t *in, int in_len) {
  *out_len = 0;
  if (in_len < 0) {
    OPENSSL_PUT_ERROR(CIPHER, ERR_R_OVERFLOW);
    return 0;
  }
  size_t in_len_sz = static_cast<size_t>(in_len);
  size_t out_len_sz;
  if ((ctx->cipher->flags & EVP_CIPH_FLAG_CUSTOM_CIPHER) && out == nullptr) {
    if (!EVP_CipherUpdateAAD(ctx, in, in_len_sz)) {
      return 0;
    }
    out_len_sz = in_len_sz;
  } else {
    // in_len_sz is < INT_MAX which is no more than half of SIZE_MAX.
    size_t max_out_len =
        std::min(in_len_sz + ctx->cipher->block_size - 1, size_t{INT_MAX});
    if (!EVP_DecryptUpdate_ex(ctx, out, &out_len_sz, max_out_len, in,
                              in_len_sz)) {
      return 0;
    }
  }
  *out_len = static_cast<int>(out_len_sz);
  return 1;
}

static int EVP_DecryptUpdate_ex_internal(EVP_CIPHER_CTX *ctx, uint8_t *out,
                                         size_t *out_len, size_t max_out_len,
                                         const uint8_t *in, size_t in_len) {
  *out_len = 0;

  // Ciphers that use blocks may write up to |block_size| extra bytes. Ensure
  // the output does not overflow |*out_len|.
  Span<const uint8_t> in_span(in, in_len);
  size_t block_size = ctx->cipher->block_size;

  if (in_span.empty()) {
    return 1;
  }

  Span<uint8_t> out_span(out, max_out_len);
  if (ctx->flags & EVP_CIPH_NO_PADDING) {
    // Use the shared block handling logic from encryption.
    return EVP_EncryptUpdate_ex_internal(ctx, out_span.data(), out_len,
                                         out_span.size(), in_span.data(),
                                         in_span.size());
  }

  assert(block_size <= sizeof(ctx->final));
  if (ctx->final_used) {
    if (out_span.size() < block_size) {
      OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BUFFER_TOO_SMALL);
      return 0;
    }
    CopyToPrefix(Span(ctx->final).first(block_size), out_span);
    ctx->final_used = 0;
    out_span = out_span.subspan(block_size);
  }

  // Use the shared block handling logic from encryption.
  if (block_size > 1 &&
      block_remainder(ctx, ctx->buf_len + in_span.size()) == 0) {
    // Decryption would end on a block boundary. In this case, although we
    // can decrypt up to the block boundary, we cannot output the final
    // plaintext block yet. It may be the final block, with padding to
    // remove.
    //
    // Instead, output all but the final block's decryption, then decrypt the
    // final block into ctx->final, to be processed later.

    // NOTE: Not _really_ necessary, but let's try aligning the second
    // EVP_EncryptUpdate_ex call to a block boundary to mess with the buffer
    // less.
    size_t head = in_span.size() > block_size ? in_span.size() - block_size : 0;
    size_t head_out_len;
    if (!EVP_EncryptUpdate_ex_internal(ctx, out_span.data(), &head_out_len,
                                       out_span.size(), in_span.data(), head)) {
      return 0;
    }
    in_span = in_span.subspan(head);
    out_span = out_span.subspan(head_out_len);
    size_t final_size;
    if (!EVP_EncryptUpdate_ex_internal(ctx, ctx->final, &final_size,
                                       sizeof(ctx->final), in_span.data(),
                                       in_span.size())) {
      return 0;
    }
    ctx->final_used = 1;
    assert(final_size == block_size);
    assert(ctx->buf_len == 0);
  } else {
    // Buffer will be non-empty.
    size_t written_out_len;
    if (!EVP_EncryptUpdate_ex_internal(ctx, out_span.data(), &written_out_len,
                                       out_span.size(), in_span.data(),
                                       in_span.size())) {
      return 0;
    }
    assert(block_size == 1 || ctx->buf_len != 0);
    out_span = out_span.subspan(written_out_len);
  }

  *out_len = max_out_len - out_span.size();
  return 1;
}

int EVP_DecryptUpdate_ex(EVP_CIPHER_CTX *ctx, uint8_t *out, size_t *out_len,
                         size_t max_out_len, const uint8_t *in, size_t in_len) {
  return WrapWithPoison(ctx, [&] {
    return EVP_DecryptUpdate_ex_internal(ctx, out, out_len, max_out_len, in,
                                         in_len);
  });
}

int EVP_DecryptFinal_ex(EVP_CIPHER_CTX *ctx, uint8_t *out, int *out_len) {
  size_t out_len_sz;
  int ret =
      EVP_DecryptFinal_ex2(ctx, out, &out_len_sz, ctx->cipher->block_size);
  static_assert(EVP_MAX_BLOCK_LENGTH <= INT_MAX);
  *out_len = static_cast<int>(out_len_sz);
  return ret;
}

static int EVP_DecryptFinal_ex2_internal(EVP_CIPHER_CTX *ctx,
                                         unsigned char *out, size_t *out_len,
                                         size_t max_out_len) {
  *out_len = 0;

  size_t block_size = ctx->cipher->block_size;
  assert(block_size <= sizeof(ctx->buf));
  if (block_size == 1) {
    if (ctx->cipher->flags & EVP_CIPH_FLAG_CUSTOM_CIPHER) {
      return ctx->cipher->cipher_final(ctx);
    }
    return 1;
  }

  size_t buf_len = ctx->buf_len;
  if (ctx->flags & EVP_CIPH_NO_PADDING) {
    if (buf_len) {
      OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_DATA_NOT_MULTIPLE_OF_BLOCK_LENGTH);
      return 0;
    }
    return 1;
  }

  if (buf_len || !ctx->final_used) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_WRONG_FINAL_BLOCK_LENGTH);
    return 0;
  }
  assert(block_size <= sizeof(ctx->final));

  // The following assumes that the ciphertext has been authenticated.
  // Otherwise it provides a padding oracle.
  size_t padding = ctx->final[block_size - 1];
  if (padding == 0 || padding > block_size) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BAD_DECRYPT);
    return 0;
  }

  for (size_t i = block_size - padding; i < block_size; i++) {
    if (ctx->final[i] != padding) {
      OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BAD_DECRYPT);
      return 0;
    }
  }

  Span<uint8_t> out_span(out, max_out_len);
  size_t payload = ctx->cipher->block_size - padding;
  if (out_span.size() < payload) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BUFFER_TOO_SMALL);
    return 0;
  }
  CopyToPrefix(Span(ctx->final).first(payload), out_span);
  out_span = out_span.subspan(payload);

  *out_len = max_out_len - out_span.size();
  return 1;
}

int EVP_DecryptFinal_ex2(EVP_CIPHER_CTX *ctx, unsigned char *out,
                         size_t *out_len, size_t max_out_len) {
  *out_len = 0;
  return WrapWithPoison(ctx, [&] {
    if (!EVP_DecryptFinal_ex2_internal(ctx, out, out_len, max_out_len)) {
      return 0;
    }
    EVP_Cipher_verify_service_indicator(ctx);
    return 1;
  });
}

int EVP_Cipher(EVP_CIPHER_CTX *ctx, uint8_t *out, const uint8_t *in,
               size_t in_len) {
  const int kError =
      (ctx->cipher->flags & EVP_CIPH_FLAG_CUSTOM_CIPHER) ? -1 : 0;

  if ((ctx->cipher->flags & EVP_CIPH_FLAG_CUSTOM_CIPHER) &&
      in_len > size_t{INT_MAX}) {
    // Can't represent the return value? That'd be bad.
    OPENSSL_PUT_ERROR(CIPHER, ERR_R_OVERFLOW);
    return kError;
  }

  size_t out_len;
  if ((ctx->cipher->flags & EVP_CIPH_FLAG_CUSTOM_CIPHER) && in == nullptr) {
    if (!ctx->cipher->cipher_final(ctx)) {
      return kError;
    }
    out_len = 0;
  } else if ((ctx->cipher->flags & EVP_CIPH_FLAG_CUSTOM_CIPHER) &&
             out == nullptr) {
    if (!ctx->cipher->update_aad(ctx, in, in_len)) {
      return kError;
    }
    out_len = in_len;  // Yes, even though no output was written!
  } else {
    if (!ctx->cipher->cipher_update(ctx, out, in, in_len)) {
      return kError;
    }
    out_len = in_len;
  }

  // |EVP_CIPH_FLAG_CUSTOM_CIPHER| never sets the FIPS indicator via
  // |EVP_Cipher| because it's complicated whether the operation has completed
  // or not. E.g. AES-GCM with a non-NULL |in| argument hasn't completed an
  // operation. Callers should use the |EVP_AEAD| API or, at least,
  // |EVP_CipherUpdate| etc.
  //
  // This call can't be pushed into |EVP_Cipher_verify_service_indicator|
  // because whether |ret| indicates success or not depends on whether
  // |EVP_CIPH_FLAG_CUSTOM_CIPHER| is set. (This unreasonable, but matches
  // OpenSSL.)
  if (!(ctx->cipher->flags & EVP_CIPH_FLAG_CUSTOM_CIPHER)) {
    EVP_Cipher_verify_service_indicator(ctx);
  }

  // Custom ciphers return byte count; regular ciphers return boolean.
  if (ctx->cipher->flags & EVP_CIPH_FLAG_CUSTOM_CIPHER) {
    return static_cast<int>(out_len);
  }
  return 1;
}

int EVP_CipherUpdate(EVP_CIPHER_CTX *ctx, uint8_t *out, int *out_len,
                     const uint8_t *in, int in_len) {
  if (ctx->encrypt) {
    return EVP_EncryptUpdate(ctx, out, out_len, in, in_len);
  } else {
    return EVP_DecryptUpdate(ctx, out, out_len, in, in_len);
  }
}

int EVP_CipherUpdate_ex(EVP_CIPHER_CTX *ctx, uint8_t *out, size_t *out_len,
                        size_t max_out_len, const uint8_t *in, size_t in_len) {
  if (ctx->encrypt) {
    return EVP_EncryptUpdate_ex(ctx, out, out_len, max_out_len, in, in_len);
  } else {
    return EVP_DecryptUpdate_ex(ctx, out, out_len, max_out_len, in, in_len);
  }
}

int EVP_CipherUpdateAAD(EVP_CIPHER_CTX *ctx, const uint8_t *in, size_t in_len) {
  return WrapWithPoison(ctx, [&] {
    if (!(ctx->cipher->flags & EVP_CIPH_FLAG_CUSTOM_CIPHER)) {
      OPENSSL_PUT_ERROR(CIPHER, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
      return 0;
    }
    return ctx->cipher->update_aad(ctx, in, in_len);
  });
}

int EVP_CipherFinal_ex(EVP_CIPHER_CTX *ctx, uint8_t *out, int *out_len) {
  if (ctx->encrypt) {
    return EVP_EncryptFinal_ex(ctx, out, out_len);
  } else {
    return EVP_DecryptFinal_ex(ctx, out, out_len);
  }
}

int EVP_CipherFinal_ex2(EVP_CIPHER_CTX *ctx, uint8_t *out, size_t *out_len,
                        size_t max_out_len) {
  if (ctx->encrypt) {
    return EVP_EncryptFinal_ex2(ctx, out, out_len, max_out_len);
  } else {
    return EVP_DecryptFinal_ex2(ctx, out, out_len, max_out_len);
  }
}

const EVP_CIPHER *EVP_CIPHER_CTX_cipher(const EVP_CIPHER_CTX *ctx) {
  return ctx->cipher;
}

int EVP_CIPHER_CTX_nid(const EVP_CIPHER_CTX *ctx) { return ctx->cipher->nid; }

int EVP_CIPHER_CTX_encrypting(const EVP_CIPHER_CTX *ctx) {
  return ctx->encrypt;
}

unsigned EVP_CIPHER_CTX_block_size(const EVP_CIPHER_CTX *ctx) {
  return ctx->cipher->block_size;
}

unsigned EVP_CIPHER_CTX_key_length(const EVP_CIPHER_CTX *ctx) {
  return ctx->key_len;
}

unsigned EVP_CIPHER_CTX_iv_length(const EVP_CIPHER_CTX *ctx) {
  if (EVP_CIPHER_mode(ctx->cipher) == EVP_CIPH_GCM_MODE) {
    int length;
    int res = EVP_CIPHER_CTX_ctrl((EVP_CIPHER_CTX *)ctx, EVP_CTRL_GET_IVLEN, 0,
                                  &length);
    // EVP_CIPHER_CTX_ctrl returning an error should be impossible under this
    // circumstance. If it somehow did, fallback to the static cipher iv_len.
    if (res == 1) {
      return length;
    }
  }
  return ctx->cipher->iv_len;
}

void *EVP_CIPHER_CTX_get_app_data(const EVP_CIPHER_CTX *ctx) {
  return ctx->app_data;
}

void EVP_CIPHER_CTX_set_app_data(EVP_CIPHER_CTX *ctx, void *data) {
  ctx->app_data = data;
}

uint32_t EVP_CIPHER_CTX_flags(const EVP_CIPHER_CTX *ctx) {
  return ctx->cipher->flags & ~EVP_CIPH_MODE_MASK;
}

uint32_t EVP_CIPHER_CTX_mode(const EVP_CIPHER_CTX *ctx) {
  return ctx->cipher->flags & EVP_CIPH_MODE_MASK;
}

int EVP_CIPHER_CTX_ctrl(EVP_CIPHER_CTX *ctx, int command, int arg, void *ptr) {
  int ret;
  if (!ctx->cipher) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_NO_CIPHER_SET);
    return 0;
  }

  if (!ctx->cipher->ctrl) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_CTRL_NOT_IMPLEMENTED);
    return 0;
  }

  ret = ctx->cipher->ctrl(ctx, command, arg, ptr);
  if (ret == -1) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_CTRL_OPERATION_NOT_IMPLEMENTED);
    return 0;
  }

  return ret;
}

int EVP_CIPHER_CTX_set_padding(EVP_CIPHER_CTX *ctx, int pad) {
  if (pad) {
    ctx->flags &= ~EVP_CIPH_NO_PADDING;
  } else {
    ctx->flags |= EVP_CIPH_NO_PADDING;
  }
  return 1;
}

int EVP_CIPHER_CTX_set_key_length(EVP_CIPHER_CTX *c, unsigned key_len) {
  if (c->key_len == key_len) {
    return 1;
  }

  if (key_len == 0 || !(c->cipher->flags & EVP_CIPH_VARIABLE_LENGTH)) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_INVALID_KEY_LENGTH);
    return 0;
  }

  c->key_len = key_len;
  return 1;
}

int EVP_CIPHER_nid(const EVP_CIPHER *cipher) { return cipher->nid; }

unsigned EVP_CIPHER_block_size(const EVP_CIPHER *cipher) {
  return cipher->block_size;
}

unsigned EVP_CIPHER_key_length(const EVP_CIPHER *cipher) {
  return cipher->key_len;
}

unsigned EVP_CIPHER_iv_length(const EVP_CIPHER *cipher) {
  return cipher->iv_len;
}

uint32_t EVP_CIPHER_flags(const EVP_CIPHER *cipher) {
  return cipher->flags & ~EVP_CIPH_MODE_MASK;
}

uint32_t EVP_CIPHER_mode(const EVP_CIPHER *cipher) {
  return cipher->flags & EVP_CIPH_MODE_MASK;
}

int EVP_CipherInit(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher,
                   const uint8_t *key, const uint8_t *iv, int enc) {
  if (cipher) {
    EVP_CIPHER_CTX_init(ctx);
  }
  return EVP_CipherInit_ex(ctx, cipher, nullptr, key, iv, enc);
}

int EVP_EncryptInit(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher,
                    const uint8_t *key, const uint8_t *iv) {
  return EVP_CipherInit(ctx, cipher, key, iv, 1);
}

int EVP_DecryptInit(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher,
                    const uint8_t *key, const uint8_t *iv) {
  return EVP_CipherInit(ctx, cipher, key, iv, 0);
}

int EVP_CipherFinal(EVP_CIPHER_CTX *ctx, uint8_t *out, int *out_len) {
  return EVP_CipherFinal_ex(ctx, out, out_len);
}

int EVP_EncryptFinal(EVP_CIPHER_CTX *ctx, uint8_t *out, int *out_len) {
  return EVP_EncryptFinal_ex(ctx, out, out_len);
}

int EVP_DecryptFinal(EVP_CIPHER_CTX *ctx, uint8_t *out, int *out_len) {
  return EVP_DecryptFinal_ex(ctx, out, out_len);
}

int EVP_add_cipher_alias(const char *a, const char *b) { return 1; }

void EVP_CIPHER_CTX_set_flags(const EVP_CIPHER_CTX *ctx, uint32_t flags) {}
