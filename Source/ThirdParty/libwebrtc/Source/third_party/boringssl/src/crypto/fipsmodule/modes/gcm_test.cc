/* ====================================================================
 * Copyright (c) 2008 The OpenSSL Project.  All rights reserved.
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
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
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
 * ==================================================================== */

#include <stdio.h>
#include <string.h>

#include <vector>

#include <gtest/gtest.h>

#include <openssl/aes.h>

#include "internal.h"
#include "../../test/file_test.h"
#include "../../test/test_util.h"


TEST(GCMTest, TestVectors) {
  FileTestGTest("crypto/fipsmodule/modes/gcm_tests.txt", [](FileTest *t) {
    std::vector<uint8_t> key, plaintext, additional_data, nonce, ciphertext,
        tag;
    ASSERT_TRUE(t->GetBytes(&key, "Key"));
    ASSERT_TRUE(t->GetBytes(&plaintext, "Plaintext"));
    ASSERT_TRUE(t->GetBytes(&additional_data, "AdditionalData"));
    ASSERT_TRUE(t->GetBytes(&nonce, "Nonce"));
    ASSERT_TRUE(t->GetBytes(&ciphertext, "Ciphertext"));
    ASSERT_TRUE(t->GetBytes(&tag, "Tag"));

    ASSERT_EQ(plaintext.size(), ciphertext.size());
    ASSERT_TRUE(key.size() == 16 || key.size() == 24 || key.size() == 32);
    ASSERT_EQ(16u, tag.size());

    std::vector<uint8_t> out(plaintext.size());
    AES_KEY aes_key;
    ASSERT_EQ(0, AES_set_encrypt_key(key.data(), key.size() * 8, &aes_key));

    GCM128_CONTEXT ctx;
    OPENSSL_memset(&ctx, 0, sizeof(ctx));
    CRYPTO_gcm128_init_key(&ctx.gcm_key, &aes_key, AES_encrypt, 0);
    CRYPTO_gcm128_setiv(&ctx, &aes_key, nonce.data(), nonce.size());
    if (!additional_data.empty()) {
      CRYPTO_gcm128_aad(&ctx, additional_data.data(), additional_data.size());
    }
    if (!plaintext.empty()) {
      CRYPTO_gcm128_encrypt(&ctx, &aes_key, plaintext.data(), out.data(),
                            plaintext.size());
    }
    ASSERT_TRUE(CRYPTO_gcm128_finish(&ctx, tag.data(), tag.size()));
    EXPECT_EQ(Bytes(ciphertext), Bytes(out));

    CRYPTO_gcm128_setiv(&ctx, &aes_key, nonce.data(), nonce.size());
    OPENSSL_memset(out.data(), 0, out.size());
    if (!additional_data.empty()) {
      CRYPTO_gcm128_aad(&ctx, additional_data.data(), additional_data.size());
    }
    if (!ciphertext.empty()) {
      CRYPTO_gcm128_decrypt(&ctx, &aes_key, ciphertext.data(), out.data(),
                            ciphertext.size());
    }
    ASSERT_TRUE(CRYPTO_gcm128_finish(&ctx, tag.data(), tag.size()));
    EXPECT_EQ(Bytes(plaintext), Bytes(out));
  });
}

TEST(GCMTest, ByteSwap) {
  EXPECT_EQ(0x04030201u, CRYPTO_bswap4(0x01020304u));
  EXPECT_EQ(UINT64_C(0x0807060504030201),
            CRYPTO_bswap8(UINT64_C(0x0102030405060708)));
}
