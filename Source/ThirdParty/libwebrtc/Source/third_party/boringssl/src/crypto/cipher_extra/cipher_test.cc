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

#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <openssl/cipher.h>
#include <openssl/err.h>

#include "../test/file_test.h"
#include "../test/test_util.h"


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

static void TestOperation(FileTest *t, const EVP_CIPHER *cipher, bool encrypt,
                          size_t chunk_size, const std::vector<uint8_t> &key,
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

  bssl::ScopedEVP_CIPHER_CTX ctx;
  ASSERT_TRUE(EVP_CipherInit_ex(ctx.get(), cipher, nullptr, nullptr, nullptr,
                         encrypt ? 1 : 0));
  if (t->HasAttribute("IV")) {
    if (is_aead) {
      ASSERT_TRUE(
          EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, iv.size(), 0));
    } else {
      ASSERT_EQ(iv.size(), EVP_CIPHER_CTX_iv_length(ctx.get()));
    }
  }
  if (is_aead && !encrypt) {
    ASSERT_TRUE(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, tag.size(),
                                    const_cast<uint8_t *>(tag.data())));
  }
  // The ciphers are run with no padding. For each of the ciphers we test, the
  // output size matches the input size.
  std::vector<uint8_t> result(in->size());
  ASSERT_EQ(in->size(), out->size());
  int unused, result_len1 = 0, result_len2;
  ASSERT_TRUE(EVP_CIPHER_CTX_set_key_length(ctx.get(), key.size()));
  ASSERT_TRUE(EVP_CipherInit_ex(ctx.get(), nullptr, nullptr, key.data(),
                                iv.data(), -1));
  // Note: the deprecated |EVP_CIPHER|-based AES-GCM API is sensitive to whether
  // parameters are NULL, so it is important to skip the |in| and |aad|
  // |EVP_CipherUpdate| calls when empty.
  if (!aad.empty()) {
    ASSERT_TRUE(
        EVP_CipherUpdate(ctx.get(), nullptr, &unused, aad.data(), aad.size()));
  }
  ASSERT_TRUE(EVP_CIPHER_CTX_set_padding(ctx.get(), 0));
  if (chunk_size != 0) {
    for (size_t i = 0; i < in->size();) {
      size_t todo = chunk_size;
      if (i + todo > in->size()) {
        todo = in->size() - i;
      }

      int len;
      ASSERT_TRUE(EVP_CipherUpdate(ctx.get(), result.data() + result_len1, &len,
                                   in->data() + i, todo));
      result_len1 += len;
      i += todo;
    }
  } else if (!in->empty()) {
    ASSERT_TRUE(EVP_CipherUpdate(ctx.get(), result.data(), &result_len1,
                                 in->data(), in->size()));
  }
  ASSERT_TRUE(
      EVP_CipherFinal_ex(ctx.get(), result.data() + result_len1, &result_len2));
  result.resize(result_len1 + result_len2);
  EXPECT_EQ(Bytes(*out), Bytes(result));
  if (encrypt && is_aead) {
    uint8_t rtag[16];
    ASSERT_LE(tag.size(), sizeof(rtag));
    ASSERT_TRUE(
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, tag.size(), rtag));
    EXPECT_EQ(Bytes(tag), Bytes(rtag, tag.size()));
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
    // By default, both directions are run, unless overridden by the operation.
    if (operation != kDecrypt) {
      SCOPED_TRACE("encrypt");
      TestOperation(t, cipher, true /* encrypt */, chunk_size, key, iv,
                    plaintext, ciphertext, aad, tag);
    }

    if (operation != kEncrypt) {
      SCOPED_TRACE("decrypt");
      TestOperation(t, cipher, false /* decrypt */, chunk_size, key, iv,
                    plaintext, ciphertext, aad, tag);
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
