// Copyright 2015-2016 The OpenSSL Project Authors. All Rights Reserved.
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

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <openssl/aes.h>
#include <openssl/cipher.h>
#include <openssl/err.h>
#include <openssl/nid.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/span.h>

#include "../internal.h"
#include "../test/file_test.h"
#include "../test/test_util.h"
#include "../test/wycheproof_util.h"
#include "./internal.h"


BSSL_NAMESPACE_BEGIN
namespace {

static const EVP_CIPHER *GetCipher(const std::string &name) {
  if (name == "DES-CBC") {
    return EVP_des_cbc();
  } else if (name == "DES-ECB") {
    return EVP_des_ecb();
  } else if (name == "DES-EDE") {
    return EVP_des_ede();
  } else if (name == "DES-EDE3") {
    return EVP_des_ede3();
  } else if (name == "DES-EDE-CBC") {
    return EVP_des_ede_cbc();
  } else if (name == "DES-EDE3-CBC") {
    return EVP_des_ede3_cbc();
  } else if (name == "RC4") {
    return EVP_rc4();
  } else if (name == "AES-128-ECB") {
    return EVP_aes_128_ecb();
  } else if (name == "AES-256-ECB") {
    return EVP_aes_256_ecb();
  } else if (name == "AES-128-CBC") {
    return EVP_aes_128_cbc();
  } else if (name == "AES-128-GCM") {
    return EVP_aes_128_gcm();
  } else if (name == "AES-128-OFB") {
    return EVP_aes_128_ofb();
  } else if (name == "AES-192-CBC") {
    return EVP_aes_192_cbc();
  } else if (name == "AES-192-CTR") {
    return EVP_aes_192_ctr();
  } else if (name == "AES-192-ECB") {
    return EVP_aes_192_ecb();
  } else if (name == "AES-192-GCM") {
    return EVP_aes_192_gcm();
  } else if (name == "AES-192-OFB") {
    return EVP_aes_192_ofb();
  } else if (name == "AES-256-CBC") {
    return EVP_aes_256_cbc();
  } else if (name == "AES-128-CTR") {
    return EVP_aes_128_ctr();
  } else if (name == "AES-256-CTR") {
    return EVP_aes_256_ctr();
  } else if (name == "AES-256-GCM") {
    return EVP_aes_256_gcm();
  } else if (name == "AES-256-OFB") {
    return EVP_aes_256_ofb();
  }
  return nullptr;
}

enum class Operation {
  // kBoth tests both encryption and decryption.
  kBoth,
  // kEncrypt tests encryption. The result of encryption should always
  // successfully decrypt, so this should only be used if the test file has a
  // matching decrypt-only vector.
  kEncrypt,
  // kDecrypt tests decryption. This should only be used if the test file has a
  // matching encrypt-only input, or if multiple ciphertexts are valid for
  // a given plaintext and this is a non-canonical ciphertext.
  kDecrypt,
  // kInvalidDecrypt tests decryption and expects it to fail, e.g. due to
  // invalid tag or padding.
  kInvalidDecrypt,
};

static const char *OperationToString(Operation op) {
  switch (op) {
    case Operation::kBoth:
      return "Both";
    case Operation::kEncrypt:
      return "Encrypt";
    case Operation::kDecrypt:
      return "Decrypt";
    case Operation::kInvalidDecrypt:
      return "InvalidDecrypt";
  }
  abort();
}

enum class API {
  // kCipher tests the EVP_Cipher() API. Arcane!
  kCipher,
  // kUnsized tests the EVP_CipherUpdate_ex() API. No size checking!
  kUnsized,
  // kSized tests the EVP_CipherUpdate_Ex2() API. With size checking!
  kSized,
};

static const char *APIToString(API api) {
  switch (api) {
    case API::kCipher:
      return "Cipher";
    case API::kUnsized:
      return "Unsized";
    case API::kSized:
      return "Sized";
  }
  abort();
}

// MaybeCopyCipherContext, if |copy| is true, replaces |*ctx| with a, hopefully
// equivalent, copy of it.
static bool MaybeCopyCipherContext(bool copy, UniquePtr<EVP_CIPHER_CTX> *ctx) {
  if (!copy) {
    return true;
  }
  UniquePtr<EVP_CIPHER_CTX> ctx2(EVP_CIPHER_CTX_new());
  if (!ctx2 || !EVP_CIPHER_CTX_copy(ctx2.get(), ctx->get())) {
    return false;
  }
  *ctx = std::move(ctx2);
  return true;
}

static void TestCipherAPI(const EVP_CIPHER *cipher, Operation op, bool padding,
                          bool copy, bool in_place, API api, size_t chunk_size,
                          Span<const uint8_t> key, Span<const uint8_t> iv,
                          Span<const uint8_t> plaintext,
                          Span<const uint8_t> ciphertext,
                          Span<const uint8_t> aad, Span<const uint8_t> tag) {
  SCOPED_TRACE(APIToString(api));

  bool encrypt = op == Operation::kEncrypt;
  bool is_custom_cipher =
      EVP_CIPHER_flags(cipher) & EVP_CIPH_FLAG_CUSTOM_CIPHER;
  Span<const uint8_t> in = encrypt ? plaintext : ciphertext;
  Span<const uint8_t> expected = encrypt ? ciphertext : plaintext;
  bool is_aead = EVP_CIPHER_mode(cipher) == EVP_CIPH_GCM_MODE;

  // Some |EVP_CIPHER|s take a variable-length key, and need to first be
  // configured with the key length, which requires configuring the cipher.
  UniquePtr<EVP_CIPHER_CTX> ctx(EVP_CIPHER_CTX_new());
  ASSERT_TRUE(ctx);
  ASSERT_TRUE(EVP_CipherInit_ex(ctx.get(), cipher, /*engine=*/nullptr,
                                /*key=*/nullptr, /*iv=*/nullptr,
                                encrypt ? 1 : 0));
  ASSERT_TRUE(EVP_CIPHER_CTX_set_key_length(ctx.get(), key.size()));
  if (!padding) {
    ASSERT_TRUE(EVP_CIPHER_CTX_set_padding(ctx.get(), 0));
  }

  // Configure the key.
  ASSERT_TRUE(MaybeCopyCipherContext(copy, &ctx));
  ASSERT_TRUE(EVP_CipherInit_ex(ctx.get(), /*cipher=*/nullptr,
                                /*engine=*/nullptr, key.data(), /*iv=*/nullptr,
                                /*enc=*/-1));

  // Configure the IV to run the actual operation. Callers that wish to use a
  // key for multiple, potentially concurrent, operations will likely copy at
  // this point. The |EVP_CIPHER_CTX| API uses the same type to represent a
  // pre-computed key schedule and a streaming operation.
  ASSERT_TRUE(MaybeCopyCipherContext(copy, &ctx));
  if (is_aead) {
    ASSERT_LE(iv.size(), size_t{INT_MAX});
    ASSERT_TRUE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IVLEN,
                                    static_cast<int>(iv.size()), nullptr));
    ASSERT_EQ(EVP_CIPHER_CTX_iv_length(ctx.get()), iv.size());
  } else {
    ASSERT_EQ(iv.size(), EVP_CIPHER_CTX_iv_length(ctx.get()));
  }
  ASSERT_TRUE(EVP_CipherInit_ex(ctx.get(), /*cipher=*/nullptr,
                                /*engine=*/nullptr,
                                /*key=*/nullptr, iv.data(), /*enc=*/-1));

  if (is_aead && !encrypt) {
    ASSERT_TRUE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_TAG,
                                    tag.size(),
                                    const_cast<uint8_t *>(tag.data())));
  }

  // Note: the deprecated |EVP_CIPHER|-based AEAD API is sensitive to whether
  // parameters are NULL, so it is important to skip the |in| and |aad|
  // |EVP_CipherUpdate| calls when empty.
  while (!aad.empty()) {
    size_t todo =
        chunk_size == 0 ? aad.size() : std::min(aad.size(), chunk_size);
    switch (api) {
      case API::kCipher:
        // AEADs always use the "custom cipher" return value convention. Passing
        // a null output pointer triggers the AAD logic.
        ASSERT_TRUE(is_custom_cipher);
        ASSERT_EQ(static_cast<int>(todo),
                  EVP_Cipher(ctx.get(), nullptr, aad.data(), todo));
        break;
      case API::kUnsized: {
        int len;
        ASSERT_TRUE(
            EVP_CipherUpdate(ctx.get(), nullptr, &len, aad.data(), todo));
        // Although it doesn't output anything, |EVP_CipherUpdate| should claim
        // to output the input length.
        EXPECT_EQ(len, static_cast<int>(todo));
        break;
      }
      case API::kSized: {
        ASSERT_TRUE(EVP_CipherUpdateAAD(ctx.get(), aad.data(), todo));
        break;
      }
    }
    aad = aad.subspan(todo);
  }

  // Set up the output buffer.
  size_t max_out = in.size();
  size_t block_size = EVP_CIPHER_CTX_block_size(ctx.get());
  if (block_size > 1 &&
      (EVP_CIPHER_CTX_flags(ctx.get()) & EVP_CIPH_NO_PADDING) == 0 &&
      EVP_CIPHER_CTX_encrypting(ctx.get())) {
    max_out += block_size - (max_out % block_size);
  }
  std::vector<uint8_t> result(max_out);
  if (in_place) {
    std::copy(in.begin(), in.end(), result.begin());
    in = Span(result).first(in.size());
  }

  size_t total = 0;
  int len;
  while (!in.empty()) {
    size_t todo = chunk_size == 0 ? in.size() : std::min(in.size(), chunk_size);
    EXPECT_LE(todo, static_cast<size_t>(INT_MAX));
    ASSERT_TRUE(MaybeCopyCipherContext(copy, &ctx));
    switch (api) {
      case API::kCipher:
        // |EVP_Cipher| sometimes returns the number of bytes written, or -1 on
        // error, and sometimes 1 or 0, implicitly writing |in_len| bytes.
        if (is_custom_cipher) {
          len = EVP_Cipher(ctx.get(), result.data() + total, in.data(), todo);
        } else {
          ASSERT_EQ(
              1, EVP_Cipher(ctx.get(), result.data() + total, in.data(), todo));
          len = static_cast<int>(todo);
        }
        break;
      case API::kUnsized:
        ASSERT_TRUE(EVP_CipherUpdate(ctx.get(), result.data() + total, &len,
                                     in.data(), static_cast<int>(todo)));
        break;
      case API::kSized: {
        size_t len_sz;
        ASSERT_TRUE(EVP_CipherUpdate_ex(ctx.get(), result.data() + total,
                                        &len_sz, result.size() - total,
                                        in.data(), static_cast<int>(todo)));
        len = static_cast<int>(len_sz);
        break;
      }
    }
    ASSERT_GE(len, 0);
    total += static_cast<size_t>(len);
    in = in.subspan(todo);
  }
  if (op == Operation::kInvalidDecrypt) {
    switch (api) {
      case API::kCipher:
        // Only the "custom cipher" return value convention can report failures.
        // Passing all nulls should act like |EVP_CipherFinal_ex|.
        ASSERT_TRUE(is_custom_cipher);
        EXPECT_EQ(-1, EVP_Cipher(ctx.get(), nullptr, nullptr, 0));
        break;
      case API::kSized:
        // Invalid padding and invalid tags all appear as a failed
        // |EVP_CipherFinal_ex|.
        EXPECT_FALSE(
            EVP_CipherFinal_ex(ctx.get(), result.data() + total, &len));
        break;
      case API::kUnsized: {
        size_t len_sz;
        EXPECT_FALSE(EVP_CipherFinal_ex2(ctx.get(), result.data() + total,
                                         &len_sz, result.size() - total));
        len = static_cast<int>(len_sz);
        break;
      }
    }
  } else {
    switch (api) {
      case API::kCipher:
        if (is_custom_cipher) {
          // Only the "custom cipher" convention has an |EVP_CipherFinal_ex|
          // equivalent.
          len = EVP_Cipher(ctx.get(), nullptr, nullptr, 0);
        } else {
          len = 0;
        }
        break;
      case API::kUnsized:
        ASSERT_TRUE(EVP_CipherFinal_ex(ctx.get(), result.data() + total, &len));
        break;
      case API::kSized: {
        size_t len_sz;
        ASSERT_TRUE(EVP_CipherFinal_ex2(ctx.get(), result.data() + total,
                                        &len_sz, result.size() - total));
        len = static_cast<int>(len_sz);
        break;
      }
    }
    ASSERT_GE(len, 0);
    total += static_cast<size_t>(len);
    result.resize(total);
    EXPECT_EQ(Bytes(expected), Bytes(result));
    if (encrypt && is_aead) {
      uint8_t rtag[16];
      ASSERT_LE(tag.size(), sizeof(rtag));
      ASSERT_TRUE(MaybeCopyCipherContext(copy, &ctx));
      ASSERT_TRUE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_GET_TAG,
                                      tag.size(), rtag));
      EXPECT_EQ(Bytes(tag), Bytes(rtag, tag.size()));
    }
  }
}

static void TestLowLevelAPI(const EVP_CIPHER *cipher, Operation op,
                            bool in_place, size_t chunk_size,
                            Span<const uint8_t> key, Span<const uint8_t> iv,
                            Span<const uint8_t> plaintext,
                            Span<const uint8_t> ciphertext) {
  bool encrypt = op == Operation::kEncrypt;
  Span<const uint8_t> in = encrypt ? plaintext : ciphertext;
  Span<const uint8_t> expected = encrypt ? ciphertext : plaintext;
  int nid = EVP_CIPHER_nid(cipher);
  bool is_ctr = nid == NID_aes_128_ctr || nid == NID_aes_192_ctr ||
                nid == NID_aes_256_ctr;
  bool is_cbc = nid == NID_aes_128_cbc || nid == NID_aes_192_cbc ||
                nid == NID_aes_256_cbc;
  bool is_ofb = nid == NID_aes_128_ofb128 || nid == NID_aes_192_ofb128 ||
                nid == NID_aes_256_ofb128;
  if (!is_ctr && !is_cbc && !is_ofb) {
    return;
  }

  // Invalid ciphertexts are not possible in any of the ciphers where this API
  // applies.
  ASSERT_NE(op, Operation::kInvalidDecrypt);

  AES_KEY aes;
  if (encrypt || !is_cbc) {
    ASSERT_EQ(0, AES_set_encrypt_key(key.data(), key.size() * 8, &aes));
  } else {
    ASSERT_EQ(0, AES_set_decrypt_key(key.data(), key.size() * 8, &aes));
  }

  std::vector<uint8_t> result;
  if (in_place) {
    result.assign(in.begin(), in.end());
  } else {
    result.resize(expected.size());
  }
  Span<uint8_t> out = Span(result);
  // Input and output sizes for all the low-level APIs should match.
  ASSERT_EQ(in.size(), out.size());

  // The low-level APIs all use block-size IVs.
  ASSERT_EQ(iv.size(), size_t{AES_BLOCK_SIZE});
  uint8_t ivec[AES_BLOCK_SIZE];
  OPENSSL_memcpy(ivec, iv.data(), iv.size());

  if (is_ctr) {
    unsigned num = 0;
    uint8_t ecount_buf[AES_BLOCK_SIZE];
    if (chunk_size == 0) {
      AES_ctr128_encrypt(in.data(), out.data(), in.size(), &aes, ivec,
                         ecount_buf, &num);
    } else {
      do {
        size_t todo = std::min(in.size(), chunk_size);
        AES_ctr128_encrypt(in.data(), out.data(), todo, &aes, ivec, ecount_buf,
                           &num);
        in = in.subspan(todo);
        out = out.subspan(todo);
      } while (!in.empty());
    }
    EXPECT_EQ(Bytes(expected), Bytes(result));
  } else if (is_cbc && chunk_size % AES_BLOCK_SIZE == 0) {
    // Note |AES_cbc_encrypt| requires block-aligned chunks.
    if (chunk_size == 0) {
      AES_cbc_encrypt(in.data(), out.data(), in.size(), &aes, ivec, encrypt);
    } else {
      do {
        size_t todo = std::min(in.size(), chunk_size);
        AES_cbc_encrypt(in.data(), out.data(), todo, &aes, ivec, encrypt);
        in = in.subspan(todo);
        out = out.subspan(todo);
      } while (!in.empty());
    }
    EXPECT_EQ(Bytes(expected), Bytes(result));
  } else if (is_ofb) {
    int num = 0;
    if (chunk_size == 0) {
      AES_ofb128_encrypt(in.data(), out.data(), in.size(), &aes, ivec, &num);
    } else {
      do {
        size_t todo = std::min(in.size(), chunk_size);
        AES_ofb128_encrypt(in.data(), out.data(), todo, &aes, ivec, &num);
        in = in.subspan(todo);
        out = out.subspan(todo);
      } while (!in.empty());
    }
    EXPECT_EQ(Bytes(expected), Bytes(result));
  }
}

enum class SizedAPIFailCase { kNever, kUpdate1, kUpdate2, kFinal };

static const char *SizedAPIFailCaseToString(SizedAPIFailCase fail_case) {
  switch (fail_case) {
    case SizedAPIFailCase::kNever:
      return "Never";
    case SizedAPIFailCase::kUpdate1:
      return "Update1";
    case SizedAPIFailCase::kUpdate2:
      return "Update2";
    case SizedAPIFailCase::kFinal:
      return "Final";
  }
  abort();
}

static size_t RoundDown(size_t n, size_t block_size) {
  return n / block_size * block_size;
}

static void TestSizedAPIRangeChecks(const EVP_CIPHER *cipher, Operation op,
                                    bool padding, Span<const uint8_t> key,
                                    Span<const uint8_t> iv,
                                    Span<const uint8_t> plaintext,
                                    Span<const uint8_t> ciphertext,
                                    Span<const uint8_t> aad,
                                    Span<const uint8_t> tag) {
  bool encrypt = op == Operation::kEncrypt;
  Span<const uint8_t> in = encrypt ? plaintext : ciphertext;
  Span<const uint8_t> expected = encrypt ? ciphertext : plaintext;
  bool is_aead = EVP_CIPHER_mode(cipher) == EVP_CIPH_GCM_MODE;

  // Some |EVP_CIPHER|s take a variable-length key, and need to first be
  // configured with the key length, which requires configuring the cipher.
  UniquePtr<EVP_CIPHER_CTX> ctx(EVP_CIPHER_CTX_new());
  ASSERT_TRUE(ctx);
  ASSERT_TRUE(EVP_CipherInit_ex(ctx.get(), cipher, /*engine=*/nullptr,
                                /*key=*/nullptr, /*iv=*/nullptr,
                                encrypt ? 1 : 0));
  ASSERT_TRUE(EVP_CIPHER_CTX_set_key_length(ctx.get(), key.size()));
  if (!padding) {
    ASSERT_TRUE(EVP_CIPHER_CTX_set_padding(ctx.get(), 0));
  }

  // Configure the key.
  ASSERT_TRUE(EVP_CipherInit_ex(ctx.get(), /*cipher=*/nullptr,
                                /*engine=*/nullptr, key.data(),
                                /*iv=*/nullptr,
                                /*enc=*/-1));

  // Configure the IV to run the actual operation. Callers that wish to use
  // a key for multiple, potentially concurrent, operations will likely copy
  // at this point. The |EVP_CIPHER_CTX| API uses the same type to represent
  // a pre-computed key schedule and a streaming operation.
  if (is_aead) {
    ASSERT_LE(iv.size(), size_t{INT_MAX});
    ASSERT_TRUE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IVLEN,
                                    static_cast<int>(iv.size()), nullptr));
    ASSERT_EQ(EVP_CIPHER_CTX_iv_length(ctx.get()), iv.size());
  } else {
    ASSERT_EQ(iv.size(), EVP_CIPHER_CTX_iv_length(ctx.get()));
  }
  ASSERT_TRUE(EVP_CipherInit_ex(ctx.get(), /*cipher=*/nullptr,
                                /*engine=*/nullptr,
                                /*key=*/nullptr, iv.data(), /*enc=*/-1));

  if (is_aead) {
    if (!encrypt) {
      ASSERT_TRUE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_TAG,
                                      tag.size(),
                                      const_cast<uint8_t *>(tag.data())));
    }
    ASSERT_TRUE(EVP_CipherUpdateAAD(ctx.get(), aad.data(), aad.size()));
  }

  size_t block_size = EVP_CIPHER_block_size(cipher);
  for (size_t len_to_split = 0; len_to_split <= in.size(); ++len_to_split) {
    SCOPED_TRACE(len_to_split);
    for (SizedAPIFailCase fail_case :
         {SizedAPIFailCase::kNever, SizedAPIFailCase::kUpdate1,
          SizedAPIFailCase::kUpdate2, SizedAPIFailCase::kFinal}) {
      SCOPED_TRACE(SizedAPIFailCaseToString(fail_case));

      UniquePtr<EVP_CIPHER_CTX> thisctx(EVP_CIPHER_CTX_new());
      ASSERT_TRUE(thisctx);
      ASSERT_TRUE(EVP_CIPHER_CTX_copy(thisctx.get(), ctx.get()));

      size_t update_size = RoundDown(
          (padding && !encrypt)
              // When decrypting, the final block will not be output right away.
              ? std::max(in.size(), size_t{1}) - 1
              : in.size(),
          block_size);
      Span<const uint8_t> in1 = in.first(len_to_split);
      size_t update1_size = RoundDown(
          (padding && !encrypt)
              // When decrypting, the final block will not be output right away.
              ? std::max(in1.size(), size_t{1}) - 1
              : in1.size(),
          block_size);
      size_t update2_size = update_size - update1_size;
      Span<const uint8_t> in2 = in.subspan(len_to_split);
      size_t final_size = expected.size() - update1_size - update2_size;

      // Now, finally, the cipher is ready to be abused.
      size_t len;
      if (fail_case == SizedAPIFailCase::kUpdate1 && update1_size > 0) {
        std::vector<uint8_t> buf(update1_size - 1);
        EXPECT_FALSE(EVP_CipherUpdate_ex(thisctx.get(), buf.data(), &len,
                                         buf.size(), in1.data(), in1.size()));
        EXPECT_EQ(len, size_t{0});
        continue;
      } else {
        std::vector<uint8_t> buf(update1_size);
        EXPECT_TRUE(EVP_CipherUpdate_ex(thisctx.get(), buf.data(), &len,
                                        buf.size(), in1.data(), in1.size()));
        EXPECT_EQ(len, update1_size);
        EXPECT_EQ(Bytes(expected.first(update1_size)), Bytes(buf));
      }
      if (fail_case == SizedAPIFailCase::kUpdate2 && update2_size > 0) {
        std::vector<uint8_t> buf(update2_size - 1);
        EXPECT_FALSE(EVP_CipherUpdate_ex(thisctx.get(), buf.data(), &len,
                                         buf.size(), in2.data(), in2.size()));
        EXPECT_EQ(len, size_t{0});
        continue;
      } else {
        std::vector<uint8_t> buf(update2_size);
        EXPECT_TRUE(EVP_CipherUpdate_ex(thisctx.get(), buf.data(), &len,
                                        buf.size(), in2.data(), in2.size()));
        EXPECT_EQ(len, update2_size);
        EXPECT_EQ(Bytes(expected.subspan(update1_size, update2_size)),
                  Bytes(buf));
      }
      if (fail_case == SizedAPIFailCase::kFinal && final_size > 0) {
        std::vector<uint8_t> buf(final_size - 1);
        EXPECT_FALSE(
            EVP_CipherFinal_ex2(thisctx.get(), buf.data(), &len, buf.size()));
        EXPECT_EQ(len, size_t{0});
        continue;
      } else {
        std::vector<uint8_t> buf(final_size);
        EXPECT_TRUE(
            EVP_CipherFinal_ex2(thisctx.get(), buf.data(), &len, buf.size()));
        EXPECT_EQ(len, final_size);
        EXPECT_EQ(Bytes(expected.subspan(update1_size + update2_size)),
                  Bytes(buf));
      }
      if (encrypt && is_aead) {
        uint8_t rtag[16];
        ASSERT_LE(tag.size(), sizeof(rtag));
        ASSERT_TRUE(EVP_CIPHER_CTX_ctrl(thisctx.get(), EVP_CTRL_AEAD_GET_TAG,
                                        tag.size(), rtag));
        EXPECT_EQ(Bytes(tag), Bytes(rtag, tag.size()));
      }
    }
  }
}

static void TestCipher(const EVP_CIPHER *cipher, Operation input_op,
                       bool padding, Span<const uint8_t> key,
                       Span<const uint8_t> iv, Span<const uint8_t> plaintext,
                       Span<const uint8_t> ciphertext, Span<const uint8_t> aad,
                       Span<const uint8_t> tag) {
  size_t block_size = EVP_CIPHER_block_size(cipher);
  std::vector<Operation> ops = {Operation::kEncrypt, Operation::kDecrypt};
  if (input_op != Operation::kBoth) {
    ops = {input_op};
  }
  for (Operation op : ops) {
    SCOPED_TRACE(OperationToString(op));
    // Zero indicates a single-shot API.
    static const size_t kChunkSizes[] = {0,  1,  2,  5,  7,  8,  9,  15, 16,
                                         17, 31, 32, 33, 63, 64, 65, 512};
    for (size_t chunk_size : kChunkSizes) {
      SCOPED_TRACE(chunk_size);
      if (chunk_size > plaintext.size() && chunk_size > ciphertext.size() &&
          chunk_size > aad.size()) {
        continue;
      }
      for (bool in_place : {false, true}) {
        SCOPED_TRACE(in_place);
        for (bool copy : {false, true}) {
          SCOPED_TRACE(copy);
          TestCipherAPI(cipher, op, padding, copy, in_place, API::kSized,
                        chunk_size, key, iv, plaintext, ciphertext, aad, tag);
          TestCipherAPI(cipher, op, padding, copy, in_place, API::kUnsized,
                        chunk_size, key, iv, plaintext, ciphertext, aad, tag);
          if (!padding && chunk_size % block_size == 0) {
            TestCipherAPI(cipher, op, padding, copy, in_place, API::kCipher,
                          chunk_size, key, iv, plaintext, ciphertext, aad, tag);
          }
        }
        if (!padding) {
          TestLowLevelAPI(cipher, op, in_place, chunk_size, key, iv, plaintext,
                          ciphertext);
        }
      }
    }
    if (op != Operation::kInvalidDecrypt) {
      TestSizedAPIRangeChecks(cipher, op, padding, key, iv, plaintext,
                              ciphertext, aad, tag);
    }
  }
}

static void CipherFileTest(FileTest *t) {
  std::string cipher_str;
  ASSERT_TRUE(t->GetAttribute(&cipher_str, "Cipher"));
  SCOPED_TRACE(cipher_str);
  const EVP_CIPHER *cipher = GetCipher(cipher_str);
  ASSERT_TRUE(cipher);

  std::vector<uint8_t> key, iv, plaintext, ciphertext, aad, tag;
  ASSERT_TRUE(t->GetBytes(&key, "Key"));
  ASSERT_TRUE(t->GetBytes(&plaintext, "Plaintext"));
  ASSERT_TRUE(t->GetBytes(&ciphertext, "Ciphertext"));
  if (EVP_CIPHER_iv_length(cipher) > 0) {
    ASSERT_TRUE(t->GetBytes(&iv, "IV"));
  }
  if (EVP_CIPHER_mode(cipher) == EVP_CIPH_GCM_MODE) {
    ASSERT_TRUE(t->GetBytes(&aad, "AAD"));
    ASSERT_TRUE(t->GetBytes(&tag, "Tag"));
  }

  Operation op = Operation::kBoth;
  if (t->HasAttribute("Operation")) {
    const std::string &str = t->GetAttributeOrDie("Operation");
    if (str == "Encrypt" || str == "ENCRYPT") {
      op = Operation::kEncrypt;
    } else if (str == "Decrypt" || str == "DECRYPT") {
      op = Operation::kDecrypt;
    } else if (str == "InvalidDecrypt") {
      op = Operation::kInvalidDecrypt;
    } else {
      FAIL() << "Unknown operation: " << str;
    }
  }

  TestCipher(cipher, op, /*padding=*/false, key, iv, plaintext, ciphertext, aad,
             tag);
}

TEST(CipherTest, TestVectors) {
  FileTestGTest("crypto/cipher/test/cipher_tests.txt", CipherFileTest);
}

TEST(CipherTest, CAVP_AES_128_CBC) {
  FileTestGTest("crypto/cipher/test/nist_cavp/aes_128_cbc.txt", CipherFileTest);
}

TEST(CipherTest, CAVP_AES_128_CTR) {
  FileTestGTest("crypto/cipher/test/nist_cavp/aes_128_ctr.txt", CipherFileTest);
}

TEST(CipherTest, CAVP_AES_192_CBC) {
  FileTestGTest("crypto/cipher/test/nist_cavp/aes_192_cbc.txt", CipherFileTest);
}

TEST(CipherTest, CAVP_AES_192_CTR) {
  FileTestGTest("crypto/cipher/test/nist_cavp/aes_192_ctr.txt", CipherFileTest);
}

TEST(CipherTest, CAVP_AES_256_CBC) {
  FileTestGTest("crypto/cipher/test/nist_cavp/aes_256_cbc.txt", CipherFileTest);
}

TEST(CipherTest, CAVP_AES_256_CTR) {
  FileTestGTest("crypto/cipher/test/nist_cavp/aes_256_ctr.txt", CipherFileTest);
}

TEST(CipherTest, CAVP_TDES_CBC) {
  FileTestGTest("crypto/cipher/test/nist_cavp/tdes_cbc.txt", CipherFileTest);
}

TEST(CipherTest, CAVP_TDES_ECB) {
  FileTestGTest("crypto/cipher/test/nist_cavp/tdes_ecb.txt", CipherFileTest);
}

TEST(CipherTest, WycheproofAESCBC) {
  FileTestGTest("third_party/wycheproof_testvectors/aes_cbc_pkcs5_test.txt",
                [](FileTest *t) {
                  t->IgnoreInstruction("type");
                  t->IgnoreInstruction("ivSize");

                  std::string key_size;
                  ASSERT_TRUE(t->GetInstruction(&key_size, "keySize"));
                  SCOPED_TRACE(std::string{"aes_"} + key_size +
                               std::string{"_cbc"});
                  const EVP_CIPHER *cipher;
                  switch (atoi(key_size.c_str())) {
                    case 128:
                      cipher = EVP_aes_128_cbc();
                      break;
                    case 192:
                      cipher = EVP_aes_192_cbc();
                      break;
                    case 256:
                      cipher = EVP_aes_256_cbc();
                      break;
                    default:
                      FAIL() << "Unsupported key size: " << key_size;
                  }

                  std::vector<uint8_t> key, iv, msg, ct;
                  ASSERT_TRUE(t->GetBytes(&key, "key"));
                  ASSERT_TRUE(t->GetBytes(&iv, "iv"));
                  ASSERT_TRUE(t->GetBytes(&msg, "msg"));
                  ASSERT_TRUE(t->GetBytes(&ct, "ct"));
                  WycheproofResult result;
                  ASSERT_TRUE(GetWycheproofResult(t, &result));
                  TestCipher(cipher,
                             result.IsValid() ? Operation::kBoth
                                              : Operation::kInvalidDecrypt,
                             /*padding=*/true, key, iv, msg, ct, /*aad=*/{},
                             /*tag=*/{});
                });
}

TEST(CipherTest, SHA1WithSecretSuffix) {
  uint8_t buf[SHA_CBLOCK * 4];
  RAND_bytes(buf, sizeof(buf));
  // Hashing should run in time independent of the bytes.
  CONSTTIME_SECRET(buf, sizeof(buf));

  // Exhaustively testing interesting cases in this function is cubic in the
  // block size, so we test in 3-byte increments.
  constexpr size_t kSkip = 3;
  // This value should be less than 8 to test the edge case when the 8-byte
  // length wraps to the next block.
  static_assert(kSkip < 8, "kSkip is too large");

  // |EVP_sha1_final_with_secret_suffix| is sensitive to the public length of
  // the partial block previously hashed. In TLS, this is the HMAC prefix, the
  // header, and the public minimum padding length.
  for (size_t prefix = 0; prefix < SHA_CBLOCK; prefix += kSkip) {
    SCOPED_TRACE(prefix);
    // The first block is treated differently, so we run with up to three
    // blocks of length variability.
    for (size_t max_len = 0; max_len < 3 * SHA_CBLOCK; max_len += kSkip) {
      SCOPED_TRACE(max_len);
      for (size_t len = 0; len <= max_len; len += kSkip) {
        SCOPED_TRACE(len);

        uint8_t expected[SHA_DIGEST_LENGTH];
        SHA1(buf, prefix + len, expected);
        CONSTTIME_DECLASSIFY(expected, sizeof(expected));

        // Make a copy of the secret length to avoid interfering with the loop.
        size_t secret_len = len;
        CONSTTIME_SECRET(&secret_len, sizeof(secret_len));

        SHA_CTX ctx;
        SHA1_Init(&ctx);
        SHA1_Update(&ctx, buf, prefix);
        uint8_t computed[SHA_DIGEST_LENGTH];
        ASSERT_TRUE(EVP_sha1_final_with_secret_suffix(
            &ctx, computed, buf + prefix, secret_len, max_len));

        CONSTTIME_DECLASSIFY(computed, sizeof(computed));
        EXPECT_EQ(Bytes(expected), Bytes(computed));
      }
    }
  }
}

TEST(CipherTest, SHA256WithSecretSuffix) {
  uint8_t buf[SHA256_CBLOCK * 4];
  RAND_bytes(buf, sizeof(buf));
  // Hashing should run in time independent of the bytes.
  CONSTTIME_SECRET(buf, sizeof(buf));

  // Exhaustively testing interesting cases in this function is cubic in the
  // block size, so we test in 3-byte increments.
  constexpr size_t kSkip = 3;
  // This value should be less than 8 to test the edge case when the 8-byte
  // length wraps to the next block.
  static_assert(kSkip < 8, "kSkip is too large");

  // |EVP_sha256_final_with_secret_suffix| is sensitive to the public length of
  // the partial block previously hashed. In TLS, this is the HMAC prefix, the
  // header, and the public minimum padding length.
  for (size_t prefix = 0; prefix < SHA256_CBLOCK; prefix += kSkip) {
    SCOPED_TRACE(prefix);
    // The first block is treated differently, so we run with up to three
    // blocks of length variability.
    for (size_t max_len = 0; max_len < 3 * SHA256_CBLOCK; max_len += kSkip) {
      SCOPED_TRACE(max_len);
      for (size_t len = 0; len <= max_len; len += kSkip) {
        SCOPED_TRACE(len);

        uint8_t expected[SHA256_DIGEST_LENGTH];
        SHA256(buf, prefix + len, expected);
        CONSTTIME_DECLASSIFY(expected, sizeof(expected));

        // Make a copy of the secret length to avoid interfering with the loop.
        size_t secret_len = len;
        CONSTTIME_SECRET(&secret_len, sizeof(secret_len));

        SHA256_CTX ctx;
        SHA256_Init(&ctx);
        SHA256_Update(&ctx, buf, prefix);
        uint8_t computed[SHA256_DIGEST_LENGTH];
        ASSERT_TRUE(EVP_sha256_final_with_secret_suffix(
            &ctx, computed, buf + prefix, secret_len, max_len));

        CONSTTIME_DECLASSIFY(computed, sizeof(computed));
        EXPECT_EQ(Bytes(expected), Bytes(computed));
      }
    }
  }
}

TEST(CipherTest, GetCipher) {
  const EVP_CIPHER *cipher = EVP_get_cipherbynid(NID_aes_128_gcm);
  ASSERT_TRUE(cipher);
  EXPECT_EQ(NID_aes_128_gcm, EVP_CIPHER_nid(cipher));

  cipher = EVP_get_cipherbyname("aes-128-gcm");
  ASSERT_TRUE(cipher);
  EXPECT_EQ(NID_aes_128_gcm, EVP_CIPHER_nid(cipher));

  cipher = EVP_get_cipherbyname("AES-128-GCM");
  ASSERT_TRUE(cipher);
  EXPECT_EQ(NID_aes_128_gcm, EVP_CIPHER_nid(cipher));

  // We support a tcpdump-specific alias for 3DES.
  cipher = EVP_get_cipherbyname("3des");
  ASSERT_TRUE(cipher);
  EXPECT_EQ(NID_des_ede3_cbc, EVP_CIPHER_nid(cipher));
}

// Test the AES-GCM EVP_CIPHER's internal IV management APIs. OpenSSH uses these
// APIs.
TEST(CipherTest, GCMIncrementingIV) {
  const EVP_CIPHER *kCipher = EVP_aes_128_gcm();
  static const uint8_t kKey[16] = {0, 1, 2,  3,  4,  5,  6,  7,
                                   8, 9, 10, 11, 12, 13, 14, 15};
  static const uint8_t kInput[] = {'h', 'e', 'l', 'l', 'o'};

  auto expect_iv = [&](EVP_CIPHER_CTX *ctx, Span<const uint8_t> iv, bool enc) {
    // Make a reference ciphertext.
    ScopedEVP_CIPHER_CTX ref;
    ASSERT_TRUE(EVP_EncryptInit_ex(ref.get(), kCipher, /*impl=*/nullptr,
                                   kKey, /*iv=*/nullptr));
    ASSERT_TRUE(EVP_CIPHER_CTX_ctrl(ref.get(), EVP_CTRL_AEAD_SET_IVLEN,
                                    static_cast<int>(iv.size()), nullptr));
    ASSERT_TRUE(EVP_EncryptInit_ex(ref.get(), /*cipher=*/nullptr,
                                   /*impl=*/nullptr, /*key=*/nullptr,
                                   iv.data()));
    uint8_t ciphertext[sizeof(kInput)];
    int ciphertext_len;
    ASSERT_TRUE(EVP_EncryptUpdate(ref.get(), ciphertext, &ciphertext_len,
                                  kInput, sizeof(kInput)));
    int extra_len;
    ASSERT_TRUE(EVP_EncryptFinal_ex(ref.get(), nullptr, &extra_len));
    ASSERT_EQ(extra_len, 0);
    uint8_t tag[16];
    ASSERT_TRUE(EVP_CIPHER_CTX_ctrl(ref.get(), EVP_CTRL_AEAD_GET_TAG,
                                    sizeof(tag), tag));

    if (enc) {
      uint8_t actual[sizeof(kInput)];
      int actual_len;
      ASSERT_TRUE(
          EVP_EncryptUpdate(ctx, actual, &actual_len, kInput, sizeof(kInput)));
      ASSERT_TRUE(EVP_EncryptFinal_ex(ctx, nullptr, &extra_len));
      ASSERT_EQ(extra_len, 0);
      EXPECT_EQ(Bytes(actual, actual_len), Bytes(ciphertext, ciphertext_len));
      uint8_t actual_tag[16];
      ASSERT_TRUE(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG,
                                      sizeof(actual_tag), actual_tag));
      EXPECT_EQ(Bytes(actual_tag), Bytes(tag));
    } else {
      ASSERT_TRUE(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, sizeof(tag),
                                      const_cast<uint8_t *>(tag)));
      uint8_t actual[sizeof(kInput)];
      int actual_len;
      ASSERT_TRUE(EVP_DecryptUpdate(ctx, actual, &actual_len, ciphertext,
                                    sizeof(ciphertext)));
      ASSERT_TRUE(EVP_DecryptFinal_ex(ctx, nullptr, &extra_len));
      ASSERT_EQ(extra_len, 0);
      EXPECT_EQ(Bytes(actual, actual_len), Bytes(kInput));
    }
  };

  {
    // Passing in a fixed IV length of -1 sets the whole IV, but then configures
    // |EVP_CIPHER_CTX| to increment the bottom 8 bytes of the IV.
    static const uint8_t kIV1[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    static const uint8_t kIV2[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 13};
    static const uint8_t kIV3[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 14};
    static const uint8_t kIV4[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 15};

    ScopedEVP_CIPHER_CTX ctx;
    ASSERT_TRUE(EVP_EncryptInit_ex(ctx.get(), kCipher, /*impl=*/nullptr, kKey,
                                   /*iv=*/nullptr));
    ASSERT_TRUE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IV_FIXED, -1,
                                    const_cast<uint8_t *>(kIV1)));

    // EVP_CTRL_GCM_IV_GEN both configures and returns the IV.
    uint8_t iv[12];
    ASSERT_TRUE(
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_IV_GEN, sizeof(iv), iv));
    EXPECT_EQ(Bytes(iv), Bytes(kIV1));
    ASSERT_NO_FATAL_FAILURE(expect_iv(ctx.get(), kIV1, /*enc=*/true));

    // Continuing to run EVP_CTRL_GCM_IV_GEN should increment the IV.
    ASSERT_TRUE(
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_IV_GEN, sizeof(iv), iv));
    EXPECT_EQ(Bytes(iv), Bytes(kIV2));
    ASSERT_NO_FATAL_FAILURE(expect_iv(ctx.get(), kIV2, /*enc=*/true));

    // Passing in a shorter length outputs the suffix portion.
    uint8_t suffix[8];
    ASSERT_TRUE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_IV_GEN,
                                    sizeof(suffix), suffix));
    EXPECT_EQ(Bytes(suffix), Bytes(Span(kIV3).last(sizeof(suffix))));
    ASSERT_NO_FATAL_FAILURE(expect_iv(ctx.get(), kIV3, /*enc=*/true));

    // A length of -1 returns the whole IV.
    ASSERT_TRUE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_IV_GEN, -1, iv));
    EXPECT_EQ(Bytes(iv), Bytes(kIV4));
    ASSERT_NO_FATAL_FAILURE(expect_iv(ctx.get(), kIV4, /*enc=*/true));
  }

  {
    // Similar to the above, but for decrypting.
    static const uint8_t kIV1[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    static const uint8_t kIV2[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 13};

    ScopedEVP_CIPHER_CTX ctx;
    ASSERT_TRUE(EVP_DecryptInit_ex(ctx.get(), kCipher, /*impl=*/nullptr, kKey,
                                   /*iv=*/nullptr));
    ASSERT_TRUE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IV_FIXED, -1,
                                    const_cast<uint8_t *>(kIV1)));

    uint8_t iv[12];
    ASSERT_TRUE(
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_IV_GEN, sizeof(iv), iv));
    EXPECT_EQ(Bytes(iv), Bytes(kIV1));
    ASSERT_NO_FATAL_FAILURE(expect_iv(ctx.get(), kIV1, /*enc=*/false));

    ASSERT_TRUE(
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_IV_GEN, sizeof(iv), iv));
    EXPECT_EQ(Bytes(iv), Bytes(kIV2));
    ASSERT_NO_FATAL_FAILURE(expect_iv(ctx.get(), kIV2, /*enc=*/false));
  }

  {
    // Test that only the bottom 8 bytes are used as a counter.
    static const uint8_t kIV1[12] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                     0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    static const uint8_t kIV2[12] = {0xff, 0xff, 0xff, 0xff, 0x00, 0x00,
                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t kIV3[12] = {0xff, 0xff, 0xff, 0xff, 0x00, 0x00,
                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x01};

    ScopedEVP_CIPHER_CTX ctx;
    ASSERT_TRUE(EVP_EncryptInit_ex(ctx.get(), kCipher, /*impl=*/nullptr, kKey,
                                   /*iv=*/nullptr));
    ASSERT_TRUE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IV_FIXED, -1,
                                    const_cast<uint8_t *>(kIV1)));

    uint8_t iv[12];
    ASSERT_TRUE(
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_IV_GEN, sizeof(iv), iv));
    EXPECT_EQ(Bytes(iv), Bytes(kIV1));
    ASSERT_NO_FATAL_FAILURE(expect_iv(ctx.get(), kIV1, /*enc=*/true));

    ASSERT_TRUE(
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_IV_GEN, sizeof(iv), iv));
    EXPECT_EQ(Bytes(iv), Bytes(kIV2));
    ASSERT_NO_FATAL_FAILURE(expect_iv(ctx.get(), kIV2, /*enc=*/true));

    ASSERT_TRUE(
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_IV_GEN, sizeof(iv), iv));
    EXPECT_EQ(Bytes(iv), Bytes(kIV3));
    ASSERT_NO_FATAL_FAILURE(expect_iv(ctx.get(), kIV3, /*enc=*/true));
  }

  {
    // Test with a longer IV length.
    static const uint8_t kIV1[16] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                     0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                     0xff, 0xff, 0xff, 0xff};
    static const uint8_t kIV2[16] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                     0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
                                     0x00, 0x00, 0x00, 0x00};
    static const uint8_t kIV3[16] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                     0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
                                     0x00, 0x00, 0x00, 0x01};

    ScopedEVP_CIPHER_CTX ctx;
    ASSERT_TRUE(EVP_EncryptInit_ex(ctx.get(), kCipher, /*impl=*/nullptr, kKey,
                                   /*iv=*/nullptr));
    ASSERT_TRUE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IVLEN,
                                    sizeof(kIV1), nullptr));
    ASSERT_TRUE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IV_FIXED, -1,
                                    const_cast<uint8_t *>(kIV1)));

    uint8_t iv[16];
    ASSERT_TRUE(
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_IV_GEN, sizeof(iv), iv));
    EXPECT_EQ(Bytes(iv), Bytes(kIV1));
    ASSERT_NO_FATAL_FAILURE(expect_iv(ctx.get(), kIV1, /*enc=*/true));

    ASSERT_TRUE(
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_IV_GEN, sizeof(iv), iv));
    EXPECT_EQ(Bytes(iv), Bytes(kIV2));
    ASSERT_NO_FATAL_FAILURE(expect_iv(ctx.get(), kIV2, /*enc=*/true));

    ASSERT_TRUE(
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_IV_GEN, sizeof(iv), iv));
    EXPECT_EQ(Bytes(iv), Bytes(kIV3));
    ASSERT_NO_FATAL_FAILURE(expect_iv(ctx.get(), kIV3, /*enc=*/true));
  }

  {
    // When decrypting, callers are expected to configure the fixed half and
    // invocation half separately. The two will get stitched together into the
    // final IV.
    const uint8_t kIV[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

    ScopedEVP_CIPHER_CTX ctx;
    ASSERT_TRUE(EVP_DecryptInit_ex(ctx.get(), kCipher, /*impl=*/nullptr, kKey,
                                   /*iv=*/nullptr));
    ASSERT_TRUE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IV_FIXED, 4,
                                    const_cast<uint8_t *>(kIV)));
    ASSERT_TRUE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IV_INV, 8,
                                    const_cast<uint8_t *>(kIV + 4)));
    // EVP_CTRL_GCM_SET_IV_INV is sufficient to configure the IV. There is no
    // need to call EVP_CTRL_GCM_IV_GEN.
    ASSERT_NO_FATAL_FAILURE(expect_iv(ctx.get(), kIV, /*enc=*/false));
  }

  {
    // Stitching together a decryption IV that exceeds the standard IV length.
    const uint8_t kIV[16] = {1, 2,  3,  4,  5,  6,  7,  8,
                             9, 10, 11, 12, 13, 14, 15, 16};

    ScopedEVP_CIPHER_CTX ctx;
    ASSERT_TRUE(EVP_DecryptInit_ex(ctx.get(), kCipher, /*impl=*/nullptr, kKey,
                                   /*iv=*/nullptr));
    ASSERT_TRUE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IVLEN,
                                    sizeof(kIV), nullptr));

    ASSERT_TRUE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IV_FIXED, 4,
                                    const_cast<uint8_t *>(kIV)));
    ASSERT_TRUE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IV_INV, 12,
                                    const_cast<uint8_t *>(kIV + 4)));
    // EVP_CTRL_GCM_SET_IV_INV is sufficient to configure the IV. There is no
    // need to call EVP_CTRL_GCM_IV_GEN.
    ASSERT_NO_FATAL_FAILURE(expect_iv(ctx.get(), kIV, /*enc=*/false));
  }

  {
    // Fixed IVs must be at least 4 bytes and admit at least an 8 byte counter.
    const uint8_t kIV[16] = {1, 2,  3,  4,  5,  6,  7,  8,
                             9, 10, 11, 12, 13, 14, 15, 16};

    ScopedEVP_CIPHER_CTX ctx;
    ASSERT_TRUE(EVP_DecryptInit_ex(ctx.get(), kCipher, /*impl=*/nullptr, kKey,
                                   /*iv=*/nullptr));

    // This means the default IV length only allows a 4/8 split.
    EXPECT_FALSE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IV_FIXED, 0,
                                     const_cast<uint8_t *>(kIV)));
    EXPECT_FALSE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IV_FIXED, 3,
                                     const_cast<uint8_t *>(kIV)));
    EXPECT_TRUE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IV_FIXED, 4,
                                    const_cast<uint8_t *>(kIV)));
    EXPECT_FALSE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IV_FIXED, 5,
                                     const_cast<uint8_t *>(kIV)));
    EXPECT_FALSE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IV_FIXED, 16,
                                     const_cast<uint8_t *>(kIV)));

    // A longer IV allows a wider range.
    ASSERT_TRUE(
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IVLEN, 16, nullptr));
    EXPECT_FALSE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IV_FIXED, 0,
                                     const_cast<uint8_t *>(kIV)));
    EXPECT_FALSE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IV_FIXED, 3,
                                     const_cast<uint8_t *>(kIV)));
    EXPECT_TRUE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IV_FIXED, 4,
                                    const_cast<uint8_t *>(kIV)));
    EXPECT_TRUE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IV_FIXED, 6,
                                    const_cast<uint8_t *>(kIV)));
    EXPECT_TRUE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IV_FIXED, 8,
                                    const_cast<uint8_t *>(kIV)));
    EXPECT_FALSE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IV_FIXED, 9,
                                     const_cast<uint8_t *>(kIV)));
    EXPECT_FALSE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IV_FIXED, 16,
                                     const_cast<uint8_t *>(kIV)));
  }

  {
    // When encrypting, setting a fixed IV randomizes the counter portion.
    const uint8_t kFixedIV[4] = {1, 2, 3, 4};
    ScopedEVP_CIPHER_CTX ctx;
    ASSERT_TRUE(EVP_EncryptInit_ex(ctx.get(), kCipher, /*impl=*/nullptr, kKey,
                                   /*iv=*/nullptr));
    ASSERT_TRUE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IV_FIXED,
                                    sizeof(kFixedIV),
                                    const_cast<uint8_t *>(kFixedIV)));
    uint8_t counter[8];
    ASSERT_TRUE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_IV_GEN,
                                    sizeof(counter), counter));

    uint8_t iv[12];
    memcpy(iv, kFixedIV, sizeof(kFixedIV));
    memcpy(iv + sizeof(kFixedIV), counter, sizeof(counter));
    ASSERT_NO_FATAL_FAILURE(expect_iv(ctx.get(), iv, /*enc=*/true));

    // The counter continues to act as a counter.
    uint8_t counter2[8];
    ASSERT_TRUE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_IV_GEN,
                                    sizeof(counter2), counter2));
    EXPECT_EQ(CRYPTO_load_u64_be(counter2), CRYPTO_load_u64_be(counter) + 1);
    memcpy(iv + sizeof(kFixedIV), counter2, sizeof(counter2));
    ASSERT_NO_FATAL_FAILURE(expect_iv(ctx.get(), iv, /*enc=*/true));
  }

  {
    // Same as above, but with a larger IV.
    const uint8_t kFixedIV[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    ScopedEVP_CIPHER_CTX ctx;
    ASSERT_TRUE(EVP_EncryptInit_ex(ctx.get(), kCipher, /*impl=*/nullptr, kKey,
                                   /*iv=*/nullptr));
    ASSERT_TRUE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IVLEN,
                                    sizeof(kFixedIV) + 8, nullptr));
    ASSERT_TRUE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IV_FIXED,
                                    sizeof(kFixedIV),
                                    const_cast<uint8_t *>(kFixedIV)));
    uint8_t counter[8];
    ASSERT_TRUE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_IV_GEN,
                                    sizeof(counter), counter));

    uint8_t iv[16];
    memcpy(iv, kFixedIV, sizeof(kFixedIV));
    memcpy(iv + sizeof(kFixedIV), counter, sizeof(counter));
    ASSERT_NO_FATAL_FAILURE(expect_iv(ctx.get(), iv, /*enc=*/true));

    // The counter continues to act as a counter.
    uint8_t counter2[8];
    ASSERT_TRUE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_IV_GEN,
                                    sizeof(counter2), counter2));
    EXPECT_EQ(CRYPTO_load_u64_be(counter2), CRYPTO_load_u64_be(counter) + 1);
    memcpy(iv + sizeof(kFixedIV), counter2, sizeof(counter2));
    ASSERT_NO_FATAL_FAILURE(expect_iv(ctx.get(), iv, /*enc=*/true));
  }
}

TEST(CipherTest, SetIVLengthResets) {
  const EVP_CIPHER *kCipher = EVP_aes_128_gcm();
  static const uint8_t kKey[16] = {};
  static const uint8_t kIV[12] = {};

  {
    // Set an IV...
    ScopedEVP_CIPHER_CTX ctx;
    ASSERT_TRUE(EVP_EncryptInit_ex(ctx.get(), kCipher, /*impl=*/nullptr,
                                   /*key=*/nullptr, kIV));
    // But then change the length, invalidating the IV.
    ASSERT_TRUE(
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IVLEN, 64, nullptr));
    // Configuring the key should not be enough...
    ASSERT_TRUE(EVP_EncryptInit_ex(ctx.get(), /*cipher=*/nullptr,
                                   /*impl=*/nullptr, kKey,
                                   /*iv=*/nullptr));
    // ...to perform any operation.
    uint8_t in[1] = {0};
    uint8_t out[1];
    int out_len;
    EXPECT_FALSE(EVP_EncryptUpdate(ctx.get(), out, &out_len, in, sizeof(in)));
  }

  {
    // Changing IV length should also reset the unusual IV generation feature.
    ScopedEVP_CIPHER_CTX ctx;
    ASSERT_TRUE(EVP_EncryptInit_ex(ctx.get(), kCipher, /*impl=*/nullptr, kKey,
                                   /*iv=*/nullptr));
    ASSERT_TRUE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IV_FIXED, 4,
                                    const_cast<uint8_t *>(kIV)));
    ASSERT_TRUE(
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IVLEN, 64, nullptr));
    uint8_t counter[8];
    EXPECT_FALSE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_IV_GEN,
                                     sizeof(counter), counter));
  }

  {
    // Set an IV...
    ScopedEVP_CIPHER_CTX ctx;
    ASSERT_TRUE(EVP_EncryptInit_ex(ctx.get(), kCipher, /*impl=*/nullptr,
                                   /*key=*/nullptr, kIV));
    // ...and then perform a no-op IV length change.
    ASSERT_TRUE(
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IVLEN, 12, nullptr));
    // This time the IV is left alone. Some applications expect this to work.
    ASSERT_TRUE(EVP_EncryptInit_ex(ctx.get(), /*cipher=*/nullptr,
                                   /*impl=*/nullptr, kKey,
                                   /*iv=*/nullptr));
    uint8_t in[1] = {0};
    uint8_t out[1];
    int out_len;
    EXPECT_TRUE(EVP_EncryptUpdate(ctx.get(), out, &out_len, in, sizeof(in)));
  }

  {
    // If GCM IV length is less than 8, SET_IV_FIXED(-1) is allowed to succeed,
    // but a subsequent EVP_CTRL_GCM_IV_GEN call must fail rather than
    // underflow.
    ScopedEVP_CIPHER_CTX ctx;
    ASSERT_TRUE(EVP_EncryptInit_ex(ctx.get(), kCipher, /*impl=*/nullptr, kKey,
                                   /*iv=*/nullptr));
    ASSERT_TRUE(
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IVLEN, 7, nullptr));
    ASSERT_TRUE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IV_FIXED, -1,
                                    const_cast<uint8_t *>(kIV)));
    uint8_t counter[8];
    EXPECT_FALSE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_IV_GEN,
                                     sizeof(counter), counter));
  }
}

// EVP_CIPHER's buffer management for AES-GCM's variable-length IV is messy.
// Ensure it is correct even if the caller encrypts in a contrived way.
TEST(CipherTest, GCMRepeatedlyChangeIVLength) {
  const uint8_t kKey[16] = {0, 1, 2,  3,  4,  5,  6,  7,
                            8, 9, 10, 11, 12, 13, 14, 15};
  const uint8_t kInput[] = {'h', 'e', 'l', 'l', 'o'};
  const std::vector<int> kLengthSequences[] = {
      // Keep making it bigger, all allocated.
      {64, 128, 256},
      // Keep making it bigger, all within the built-in buffer.
      {12, 13, 14, 15, 16},
      // Make it smaller, all allocated.
      {256, 128, 64},
      // Make it smaller, all within the built-in buffer.
      {12, 11, 10, 9},
      // As we go down and back up, don't lose track of which buffer it is.
      {256, 128, 64, 32, 16, 8, 4, 8, 16},
      {256, 128, 64, 32, 16, 8, 4, 8, 16, 32, 64, 128},
  };
  for (const auto &length_seq : kLengthSequences) {
    SCOPED_TRACE(testing::PrintToString(length_seq));

    // Compute the expected output.
    UniquePtr<EVP_CIPHER_CTX> ctx(EVP_CIPHER_CTX_new());
    ASSERT_TRUE(ctx);
    ASSERT_TRUE(EVP_EncryptInit_ex(ctx.get(), EVP_aes_128_gcm(),
                                   /*impl=*/nullptr, kKey,
                                   /*iv=*/nullptr));
    ASSERT_TRUE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IVLEN,
                                    length_seq.back(), nullptr));
    std::vector<uint8_t> iv(length_seq.back());
    ASSERT_TRUE(EVP_EncryptInit_ex(ctx.get(), /*cipher=*/nullptr,
                                   /*impl=*/nullptr, /*key=*/nullptr,
                                   iv.data()));
    uint8_t expected_ciphertext[sizeof(kInput)];
    size_t len;
    ASSERT_TRUE(EVP_EncryptUpdate_ex(ctx.get(), expected_ciphertext, &len,
                                     sizeof(expected_ciphertext), kInput,
                                     sizeof(kInput)));
    ASSERT_EQ(len, sizeof(expected_ciphertext));
    ASSERT_TRUE(EVP_EncryptFinal_ex2(ctx.get(), nullptr, &len, 0));
    uint8_t expected_tag[12];
    ASSERT_TRUE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_GET_TAG,
                                    sizeof(expected_tag), expected_tag));

    for (bool copy : {false, true}) {
      SCOPED_TRACE(copy);

      ctx.reset(EVP_CIPHER_CTX_new());
      ASSERT_TRUE(ctx);
      ASSERT_TRUE(EVP_EncryptInit_ex(ctx.get(), EVP_aes_128_gcm(),
                                     /*impl=*/nullptr, kKey,
                                     /*iv=*/nullptr));
      ASSERT_TRUE(MaybeCopyCipherContext(copy, &ctx));
      // Simulate a caller that cannot make up its mind about the IV length.
      for (int iv_len : length_seq) {
        ASSERT_TRUE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IVLEN,
                                        iv_len, nullptr));
        ASSERT_TRUE(MaybeCopyCipherContext(copy, &ctx));
      }
      // Finish the encryption.
      ASSERT_TRUE(EVP_EncryptInit_ex(ctx.get(), /*cipher=*/nullptr,
                                     /*impl=*/nullptr, /*key=*/nullptr,
                                     iv.data()));
      ASSERT_TRUE(MaybeCopyCipherContext(copy, &ctx));
      uint8_t ciphertext[sizeof(kInput)];
      ASSERT_TRUE(EVP_EncryptUpdate_ex(ctx.get(), ciphertext, &len,
                                       sizeof(ciphertext), kInput,
                                       sizeof(kInput)));
      ASSERT_TRUE(MaybeCopyCipherContext(copy, &ctx));
      ASSERT_EQ(len, sizeof(ciphertext));
      ASSERT_TRUE(EVP_EncryptFinal_ex2(ctx.get(), nullptr, &len, 0));
      ASSERT_TRUE(MaybeCopyCipherContext(copy, &ctx));
      uint8_t tag[12];
      ASSERT_TRUE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_GET_TAG,
                                      sizeof(tag), tag));

      // The answer should have been the same.
      EXPECT_EQ(Bytes(ciphertext), Bytes(expected_ciphertext));
      EXPECT_EQ(Bytes(tag), Bytes(expected_tag));
    }
  }
}

TEST(CipherTest, RC2KeyLengthOverflow) {
  const EVP_CIPHER *cipher = EVP_rc2_cbc();
  UniquePtr<EVP_CIPHER_CTX> ctx(EVP_CIPHER_CTX_new());
  ASSERT_TRUE(ctx);

  ASSERT_TRUE(EVP_DecryptInit_ex(ctx.get(), cipher, nullptr, nullptr, nullptr));

  if (EVP_CIPHER_CTX_set_key_length(ctx.get(), 0x80000000)) {
    std::vector<uint8_t> key(128, 0);
    // This should not crash.
    EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr, key.data(), nullptr);
  }
}

}  // namespace
BSSL_NAMESPACE_END
