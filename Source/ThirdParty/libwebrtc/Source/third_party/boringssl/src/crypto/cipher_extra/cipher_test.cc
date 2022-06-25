/*
 * Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project.
 */
/* ====================================================================
 * Copyright (c) 2015 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 */

#include <limits.h>
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

#include "../test/file_test.h"
#include "../test/test_util.h"
#include "../test/wycheproof_util.h"
#include "./internal.h"


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

static bool DoCipher(EVP_CIPHER_CTX *ctx, std::vector<uint8_t> *out,
                     bssl::Span<const uint8_t> in, size_t chunk,
                     bool in_place) {
  size_t max_out = in.size();
  if ((EVP_CIPHER_CTX_flags(ctx) & EVP_CIPH_NO_PADDING) == 0 &&
      EVP_CIPHER_CTX_encrypting(ctx)) {
    unsigned block_size = EVP_CIPHER_CTX_block_size(ctx);
    max_out += block_size - (max_out % block_size);
  }
  out->resize(max_out);
  if (in_place) {
    std::copy(in.begin(), in.end(), out->begin());
    in = bssl::MakeConstSpan(out->data(), in.size());
  }

  size_t total = 0;
  int len;
  while (!in.empty()) {
    size_t todo = chunk == 0 ? in.size() : std::min(in.size(), chunk);
    EXPECT_LE(todo, static_cast<size_t>(INT_MAX));
    if (!EVP_CipherUpdate(ctx, out->data() + total, &len, in.data(),
                          static_cast<int>(todo))) {
      return false;
    }
    EXPECT_GE(len, 0);
    total += static_cast<size_t>(len);
    in = in.subspan(todo);
  }
  if (!EVP_CipherFinal_ex(ctx, out->data() + total, &len)) {
    return false;
  }
  EXPECT_GE(len, 0);
  total += static_cast<size_t>(len);
  out->resize(total);
  return true;
}

static void TestOperation(FileTest *t, const EVP_CIPHER *cipher, bool encrypt,
                          bool copy, bool in_place, size_t chunk_size,
                          const std::vector<uint8_t> &key,
                          const std::vector<uint8_t> &iv,
                          const std::vector<uint8_t> &plaintext,
                          const std::vector<uint8_t> &ciphertext,
                          const std::vector<uint8_t> &aad,
                          const std::vector<uint8_t> &tag) {
  const std::vector<uint8_t> *in, *out;
  if (encrypt) {
    in = &plaintext;
    out = &ciphertext;
  } else {
    in = &ciphertext;
    out = &plaintext;
  }

  bool is_aead = EVP_CIPHER_mode(cipher) == EVP_CIPH_GCM_MODE;

  bssl::ScopedEVP_CIPHER_CTX ctx1;
  ASSERT_TRUE(EVP_CipherInit_ex(ctx1.get(), cipher, nullptr, nullptr, nullptr,
                                encrypt ? 1 : 0));
  if (t->HasAttribute("IV")) {
    if (is_aead) {
      ASSERT_TRUE(EVP_CIPHER_CTX_ctrl(ctx1.get(), EVP_CTRL_AEAD_SET_IVLEN,
                                      iv.size(), 0));
    } else {
      ASSERT_EQ(iv.size(), EVP_CIPHER_CTX_iv_length(ctx1.get()));
    }
  }

  bssl::ScopedEVP_CIPHER_CTX ctx2;
  EVP_CIPHER_CTX *ctx = ctx1.get();
  if (copy) {
    ASSERT_TRUE(EVP_CIPHER_CTX_copy(ctx2.get(), ctx1.get()));
    ctx = ctx2.get();
  }

  if (is_aead && !encrypt) {
    ASSERT_TRUE(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, tag.size(),
                                    const_cast<uint8_t *>(tag.data())));
  }
  // The ciphers are run with no padding. For each of the ciphers we test, the
  // output size matches the input size.
  ASSERT_EQ(in->size(), out->size());
  ASSERT_TRUE(EVP_CIPHER_CTX_set_key_length(ctx, key.size()));
  ASSERT_TRUE(
      EVP_CipherInit_ex(ctx, nullptr, nullptr, key.data(), iv.data(), -1));
  // Note: the deprecated |EVP_CIPHER|-based AEAD API is sensitive to whether
  // parameters are NULL, so it is important to skip the |in| and |aad|
  // |EVP_CipherUpdate| calls when empty.
  if (!aad.empty()) {
    int unused;
    ASSERT_TRUE(
        EVP_CipherUpdate(ctx, nullptr, &unused, aad.data(), aad.size()));
  }
  ASSERT_TRUE(EVP_CIPHER_CTX_set_padding(ctx, 0));
  std::vector<uint8_t> result;
  ASSERT_TRUE(DoCipher(ctx, &result, *in, chunk_size, in_place));
  EXPECT_EQ(Bytes(*out), Bytes(result));
  if (encrypt && is_aead) {
    uint8_t rtag[16];
    ASSERT_LE(tag.size(), sizeof(rtag));
    ASSERT_TRUE(
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, tag.size(), rtag));
    EXPECT_EQ(Bytes(tag), Bytes(rtag, tag.size()));
  }

  // Additionally test low-level AES mode APIs. Skip runs where |copy| because
  // it does not apply.
  if (!copy) {
    int nid = EVP_CIPHER_nid(cipher);
    bool is_ctr = nid == NID_aes_128_ctr || nid == NID_aes_192_ctr ||
                  nid == NID_aes_256_ctr;
    bool is_cbc = nid == NID_aes_128_cbc || nid == NID_aes_192_cbc ||
                  nid == NID_aes_256_cbc;
    bool is_ofb = nid == NID_aes_128_ofb128 || nid == NID_aes_192_ofb128 ||
                  nid == NID_aes_256_ofb128;
    if (is_ctr || is_cbc || is_ofb) {
      AES_KEY aes;
      if (encrypt || !is_cbc) {
        ASSERT_EQ(0, AES_set_encrypt_key(key.data(), key.size() * 8, &aes));
      } else {
        ASSERT_EQ(0, AES_set_decrypt_key(key.data(), key.size() * 8, &aes));
      }

      // The low-level APIs all work in-place.
      bssl::Span<const uint8_t> input = *in;
      result.clear();
      if (in_place) {
        result = *in;
        input = result;
      } else {
        result.resize(out->size());
      }
      bssl::Span<uint8_t> output = bssl::MakeSpan(result);
      ASSERT_EQ(input.size(), output.size());

      // The low-level APIs all use block-size IVs.
      ASSERT_EQ(iv.size(), size_t{AES_BLOCK_SIZE});
      uint8_t ivec[AES_BLOCK_SIZE];
      OPENSSL_memcpy(ivec, iv.data(), iv.size());

      if (is_ctr) {
        unsigned num = 0;
        uint8_t ecount_buf[AES_BLOCK_SIZE];
        if (chunk_size == 0) {
          AES_ctr128_encrypt(input.data(), output.data(), input.size(), &aes,
                             ivec, ecount_buf, &num);
        } else {
          do {
            size_t todo = std::min(input.size(), chunk_size);
            AES_ctr128_encrypt(input.data(), output.data(), todo, &aes, ivec,
                               ecount_buf, &num);
            input = input.subspan(todo);
            output = output.subspan(todo);
          } while (!input.empty());
        }
        EXPECT_EQ(Bytes(*out), Bytes(result));
      } else if (is_cbc && chunk_size % AES_BLOCK_SIZE == 0) {
        // Note |AES_cbc_encrypt| requires block-aligned chunks.
        if (chunk_size == 0) {
          AES_cbc_encrypt(input.data(), output.data(), input.size(), &aes, ivec,
                          encrypt);
        } else {
          do {
            size_t todo = std::min(input.size(), chunk_size);
            AES_cbc_encrypt(input.data(), output.data(), todo, &aes, ivec,
                            encrypt);
            input = input.subspan(todo);
            output = output.subspan(todo);
          } while (!input.empty());
        }
        EXPECT_EQ(Bytes(*out), Bytes(result));
      } else if (is_ofb) {
        int num = 0;
        if (chunk_size == 0) {
          AES_ofb128_encrypt(input.data(), output.data(), input.size(), &aes,
                             ivec, &num);
        } else {
          do {
            size_t todo = std::min(input.size(), chunk_size);
            AES_ofb128_encrypt(input.data(), output.data(), todo, &aes, ivec,
                               &num);
            input = input.subspan(todo);
            output = output.subspan(todo);
          } while (!input.empty());
        }
        EXPECT_EQ(Bytes(*out), Bytes(result));
      }
    }
  }
}

static void TestCipher(FileTest *t) {
  std::string cipher_str;
  ASSERT_TRUE(t->GetAttribute(&cipher_str, "Cipher"));
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

  enum {
    kEncrypt,
    kDecrypt,
    kBoth,
  } operation = kBoth;
  if (t->HasAttribute("Operation")) {
    const std::string &str = t->GetAttributeOrDie("Operation");
    if (str == "ENCRYPT") {
      operation = kEncrypt;
    } else if (str == "DECRYPT") {
      operation = kDecrypt;
    } else {
      FAIL() << "Unknown operation: " << str;
    }
  }

  const std::vector<size_t> chunk_sizes = {0,  1,  2,  5,  7,  8,  9,  15, 16,
                                           17, 31, 32, 33, 63, 64, 65, 512};

  for (size_t chunk_size : chunk_sizes) {
    SCOPED_TRACE(chunk_size);
    for (bool copy : {false, true}) {
      SCOPED_TRACE(copy);
      for (bool in_place : {false, true}) {
        SCOPED_TRACE(in_place);
        // By default, both directions are run, unless overridden by the
        // operation.
        if (operation != kDecrypt) {
          SCOPED_TRACE("encrypt");
          TestOperation(t, cipher, true /* encrypt */, copy, in_place,
                        chunk_size, key, iv, plaintext, ciphertext, aad, tag);
        }

        if (operation != kEncrypt) {
          SCOPED_TRACE("decrypt");
          TestOperation(t, cipher, false /* decrypt */, copy, in_place,
                        chunk_size, key, iv, plaintext, ciphertext, aad, tag);
        }
      }
    }
  }
}

TEST(CipherTest, TestVectors) {
  FileTestGTest("crypto/cipher_extra/test/cipher_tests.txt", TestCipher);
}

TEST(CipherTest, CAVP_AES_128_CBC) {
  FileTestGTest("crypto/cipher_extra/test/nist_cavp/aes_128_cbc.txt",
                TestCipher);
}

TEST(CipherTest, CAVP_AES_128_CTR) {
  FileTestGTest("crypto/cipher_extra/test/nist_cavp/aes_128_ctr.txt",
                TestCipher);
}

TEST(CipherTest, CAVP_AES_192_CBC) {
  FileTestGTest("crypto/cipher_extra/test/nist_cavp/aes_192_cbc.txt",
                TestCipher);
}

TEST(CipherTest, CAVP_AES_192_CTR) {
  FileTestGTest("crypto/cipher_extra/test/nist_cavp/aes_192_ctr.txt",
                TestCipher);
}

TEST(CipherTest, CAVP_AES_256_CBC) {
  FileTestGTest("crypto/cipher_extra/test/nist_cavp/aes_256_cbc.txt",
                TestCipher);
}

TEST(CipherTest, CAVP_AES_256_CTR) {
  FileTestGTest("crypto/cipher_extra/test/nist_cavp/aes_256_ctr.txt",
                TestCipher);
}

TEST(CipherTest, CAVP_TDES_CBC) {
  FileTestGTest("crypto/cipher_extra/test/nist_cavp/tdes_cbc.txt", TestCipher);
}

TEST(CipherTest, CAVP_TDES_ECB) {
  FileTestGTest("crypto/cipher_extra/test/nist_cavp/tdes_ecb.txt", TestCipher);
}

TEST(CipherTest, WycheproofAESCBC) {
  FileTestGTest(
      "third_party/wycheproof_testvectors/aes_cbc_pkcs5_test.txt",
      [](FileTest *t) {
        t->IgnoreInstruction("type");
        t->IgnoreInstruction("ivSize");

        std::string key_size;
        ASSERT_TRUE(t->GetInstruction(&key_size, "keySize"));
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
        ASSERT_EQ(EVP_CIPHER_key_length(cipher), key.size());
        ASSERT_EQ(EVP_CIPHER_iv_length(cipher), iv.size());
        WycheproofResult result;
        ASSERT_TRUE(GetWycheproofResult(t, &result));

        bssl::ScopedEVP_CIPHER_CTX ctx;
        std::vector<uint8_t> out;
        const std::vector<size_t> chunk_sizes = {
            0, 1, 2, 5, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63, 64, 65, 512};
        for (size_t chunk : chunk_sizes) {
          SCOPED_TRACE(chunk);
          for (bool in_place : {false, true}) {
            SCOPED_TRACE(in_place);
            if (result.IsValid()) {
              ASSERT_TRUE(EVP_DecryptInit_ex(ctx.get(), cipher, nullptr,
                                             key.data(), iv.data()));
              ASSERT_TRUE(DoCipher(ctx.get(), &out, ct, chunk, in_place));
              EXPECT_EQ(Bytes(msg), Bytes(out));

              ASSERT_TRUE(EVP_EncryptInit_ex(ctx.get(), cipher, nullptr,
                                             key.data(), iv.data()));
              ASSERT_TRUE(DoCipher(ctx.get(), &out, msg, chunk, in_place));
              EXPECT_EQ(Bytes(ct), Bytes(out));
            } else {
              ASSERT_TRUE(EVP_DecryptInit_ex(ctx.get(), cipher, nullptr,
                                             key.data(), iv.data()));
              EXPECT_FALSE(DoCipher(ctx.get(), &out, ct, chunk, in_place));
            }
          }
        }
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
