/* Copyright (c) 2020, Google Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#include <stdio.h>
#include <string.h>

#include <string>

#include <openssl/base.h>
#include <openssl/crypto.h>
#include <openssl/cipher.h>

#include <gtest/gtest.h>

// Test that OPENSSL_VERSION_NUMBER and OPENSSL_VERSION_TEXT are consistent.
// Node.js parses the version out of OPENSSL_VERSION_TEXT instead of using
// OPENSSL_VERSION_NUMBER.
TEST(CryptoTest, Version) {
  char expected[512];
  snprintf(expected, sizeof(expected), "OpenSSL %d.%d.%d ",
           OPENSSL_VERSION_NUMBER >> 28, (OPENSSL_VERSION_NUMBER >> 20) & 0xff,
           (OPENSSL_VERSION_NUMBER >> 12) & 0xff);
  EXPECT_EQ(expected,
            std::string(OPENSSL_VERSION_TEXT).substr(0, strlen(expected)));
}

#if defined(BORINGSSL_FIPS_COUNTERS)
TEST(CryptoTest, FIPSCountersEVP) {
  constexpr struct {
    const EVP_CIPHER *(*cipher)();
    fips_counter_t counter;
  } kTests[] = {
    {
        EVP_aes_128_gcm,
        fips_counter_evp_aes_128_gcm,
    },
    {
        EVP_aes_256_gcm,
        fips_counter_evp_aes_256_gcm,
    },
    {
        EVP_aes_128_ctr,
        fips_counter_evp_aes_128_ctr,
    },
    {
        EVP_aes_256_ctr,
        fips_counter_evp_aes_256_ctr,
    },
  };

  uint8_t key[EVP_MAX_KEY_LENGTH] = {0};
  uint8_t iv[EVP_MAX_IV_LENGTH] = {1};

  for (const auto& test : kTests) {
    const size_t before = FIPS_read_counter(test.counter);

    bssl::ScopedEVP_CIPHER_CTX ctx;
    ASSERT_TRUE(EVP_EncryptInit_ex(ctx.get(), test.cipher(), /*engine=*/nullptr,
                                   key, iv));
    ASSERT_GT(FIPS_read_counter(test.counter), before);
  }
}
#endif  // BORINGSSL_FIPS_COUNTERS
