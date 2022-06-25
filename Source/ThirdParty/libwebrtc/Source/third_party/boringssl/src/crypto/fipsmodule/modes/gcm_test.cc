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
#include <openssl/cpu.h>

#include "../../test/abi_test.h"
#include "../../test/file_test.h"
#include "../../test/test_util.h"
#include "../aes/internal.h"
#include "internal.h"


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

    std::vector<uint8_t> got_tag(tag.size());
    CRYPTO_gcm128_tag(&ctx, got_tag.data(), got_tag.size());
    EXPECT_EQ(Bytes(tag), Bytes(got_tag));
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

#if defined(SUPPORTS_ABI_TEST) && !defined(OPENSSL_NO_ASM)
TEST(GCMTest, ABI) {
  static const uint64_t kH[2] = {
      UINT64_C(0x66e94bd4ef8a2c3b),
      UINT64_C(0x884cfa59ca342b2e),
  };
  static const size_t kBlockCounts[] = {1, 2, 3, 4, 7, 8, 15, 16, 31, 32};
  uint8_t buf[16 * 32];
  OPENSSL_memset(buf, 42, sizeof(buf));

  uint64_t X[2] = {
      UINT64_C(0x0388dace60b6a392),
      UINT64_C(0xf328c2b971b2fe78),
  };

  alignas(16) u128 Htable[16];
#if defined(GHASH_ASM_X86) || defined(GHASH_ASM_X86_64)
  if (gcm_ssse3_capable()) {
    CHECK_ABI_SEH(gcm_init_ssse3, Htable, kH);
    CHECK_ABI_SEH(gcm_gmult_ssse3, X, Htable);
    for (size_t blocks : kBlockCounts) {
      CHECK_ABI_SEH(gcm_ghash_ssse3, X, Htable, buf, 16 * blocks);
    }
  }

  if (crypto_gcm_clmul_enabled()) {
    CHECK_ABI_SEH(gcm_init_clmul, Htable, kH);
    CHECK_ABI_SEH(gcm_gmult_clmul, X, Htable);
    for (size_t blocks : kBlockCounts) {
      CHECK_ABI_SEH(gcm_ghash_clmul, X, Htable, buf, 16 * blocks);
    }

#if defined(GHASH_ASM_X86_64)
    if (((OPENSSL_ia32cap_get()[1] >> 22) & 0x41) == 0x41) {  // AVX+MOVBE
      CHECK_ABI_SEH(gcm_init_avx, Htable, kH);
      CHECK_ABI_SEH(gcm_gmult_avx, X, Htable);
      for (size_t blocks : kBlockCounts) {
        CHECK_ABI_SEH(gcm_ghash_avx, X, Htable, buf, 16 * blocks);
      }

      if (hwaes_capable()) {
        AES_KEY aes_key;
        static const uint8_t kKey[16] = {0};

        // aesni_gcm_* makes assumptions about |GCM128_CONTEXT|'s layout.
        GCM128_CONTEXT gcm;
        memset(&gcm, 0, sizeof(gcm));
        memcpy(&gcm.gcm_key.H, kH, sizeof(kH));
        memcpy(&gcm.gcm_key.Htable, Htable, sizeof(Htable));
        memcpy(&gcm.Xi, X, sizeof(X));
        uint8_t iv[16] = {0};

        aes_hw_set_encrypt_key(kKey, 128, &aes_key);
        for (size_t blocks : kBlockCounts) {
          CHECK_ABI(aesni_gcm_encrypt, buf, buf, blocks * 16, &aes_key, iv,
                    gcm.Xi.u);
          CHECK_ABI(aesni_gcm_encrypt, buf, buf, blocks * 16 + 7, &aes_key, iv,
                    gcm.Xi.u);
        }
        aes_hw_set_decrypt_key(kKey, 128, &aes_key);
        for (size_t blocks : kBlockCounts) {
          CHECK_ABI(aesni_gcm_decrypt, buf, buf, blocks * 16, &aes_key, iv,
                    gcm.Xi.u);
          CHECK_ABI(aesni_gcm_decrypt, buf, buf, blocks * 16 + 7, &aes_key, iv,
                    gcm.Xi.u);
        }
      }
    }
#endif  // GHASH_ASM_X86_64
  }
#endif  // GHASH_ASM_X86 || GHASH_ASM_X86_64

#if defined(GHASH_ASM_ARM)
  if (gcm_neon_capable()) {
    CHECK_ABI(gcm_init_neon, Htable, kH);
    CHECK_ABI(gcm_gmult_neon, X, Htable);
    for (size_t blocks : kBlockCounts) {
      CHECK_ABI(gcm_ghash_neon, X, Htable, buf, 16 * blocks);
    }
  }

  if (gcm_pmull_capable()) {
    CHECK_ABI(gcm_init_v8, Htable, kH);
    CHECK_ABI(gcm_gmult_v8, X, Htable);
    for (size_t blocks : kBlockCounts) {
      CHECK_ABI(gcm_ghash_v8, X, Htable, buf, 16 * blocks);
    }
  }
#endif  // GHASH_ASM_ARM

#if defined(GHASH_ASM_PPC64LE)
  if (CRYPTO_is_PPC64LE_vcrypto_capable()) {
    CHECK_ABI(gcm_init_p8, Htable, kH);
    CHECK_ABI(gcm_gmult_p8, X, Htable);
    for (size_t blocks : kBlockCounts) {
      CHECK_ABI(gcm_ghash_p8, X, Htable, buf, 16 * blocks);
    }
  }
#endif  // GHASH_ASM_PPC64LE
}
#endif  // SUPPORTS_ABI_TEST && !OPENSSL_NO_ASM
