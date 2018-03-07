/* Copyright (c) 2014, Google Inc.
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

#include <stdint.h>
#include <string.h>

#include <vector>

#include <gtest/gtest.h>

#include <openssl/aead.h>
#include <openssl/cipher.h>
#include <openssl/err.h>

#include "../fipsmodule/cipher/internal.h"
#include "../internal.h"
#include "../test/file_test.h"
#include "../test/test_util.h"


struct KnownAEAD {
  const char name[40];
  const EVP_AEAD *(*func)(void);
  const char *test_vectors;
  // limited_implementation indicates that tests that assume a generic AEAD
  // interface should not be performed. For example, the key-wrap AEADs only
  // handle inputs that are a multiple of eight bytes in length and the
  // SSLv3/TLS AEADs have the concept of “direction”.
  bool limited_implementation;
  // truncated_tags is true if the AEAD supports truncating tags to arbitrary
  // lengths.
  bool truncated_tags;
  // ad_len, if non-zero, is the required length of the AD.
  size_t ad_len;
};

static const struct KnownAEAD kAEADs[] = {
    {"AES_128_GCM", EVP_aead_aes_128_gcm, "aes_128_gcm_tests.txt", false, true,
     0},
    {"AES_128_GCM_NIST", EVP_aead_aes_128_gcm, "nist_cavp/aes_128_gcm.txt",
     false, true, 0},
    {"AES_256_GCM", EVP_aead_aes_256_gcm, "aes_256_gcm_tests.txt", false, true,
     0},
    {"AES_256_GCM_NIST", EVP_aead_aes_256_gcm, "nist_cavp/aes_256_gcm.txt",
     false, true, 0},
#if !defined(OPENSSL_SMALL)
    {"AES_128_GCM_SIV", EVP_aead_aes_128_gcm_siv, "aes_128_gcm_siv_tests.txt",
     false, false, 0},
    {"AES_256_GCM_SIV", EVP_aead_aes_256_gcm_siv, "aes_256_gcm_siv_tests.txt",
     false, false, 0},
#endif
    {"ChaCha20Poly1305", EVP_aead_chacha20_poly1305,
     "chacha20_poly1305_tests.txt", false, true, 0},
    {"AES_128_CBC_SHA1_TLS", EVP_aead_aes_128_cbc_sha1_tls,
     "aes_128_cbc_sha1_tls_tests.txt", true, false, 11},
    {"AES_128_CBC_SHA1_TLSImplicitIV",
     EVP_aead_aes_128_cbc_sha1_tls_implicit_iv,
     "aes_128_cbc_sha1_tls_implicit_iv_tests.txt", true, false, 11},
    {"AES_128_CBC_SHA256_TLS", EVP_aead_aes_128_cbc_sha256_tls,
     "aes_128_cbc_sha256_tls_tests.txt", true, false, 11},
    {"AES_256_CBC_SHA1_TLS", EVP_aead_aes_256_cbc_sha1_tls,
     "aes_256_cbc_sha1_tls_tests.txt", true, false, 11},
    {"AES_256_CBC_SHA1_TLSImplicitIV",
     EVP_aead_aes_256_cbc_sha1_tls_implicit_iv,
     "aes_256_cbc_sha1_tls_implicit_iv_tests.txt", true, false, 11},
    {"AES_256_CBC_SHA256_TLS", EVP_aead_aes_256_cbc_sha256_tls,
     "aes_256_cbc_sha256_tls_tests.txt", true, false, 11},
    {"AES_256_CBC_SHA384_TLS", EVP_aead_aes_256_cbc_sha384_tls,
     "aes_256_cbc_sha384_tls_tests.txt", true, false, 11},
    {"DES_EDE3_CBC_SHA1_TLS", EVP_aead_des_ede3_cbc_sha1_tls,
     "des_ede3_cbc_sha1_tls_tests.txt", true, false, 11},
    {"DES_EDE3_CBC_SHA1_TLSImplicitIV",
     EVP_aead_des_ede3_cbc_sha1_tls_implicit_iv,
     "des_ede3_cbc_sha1_tls_implicit_iv_tests.txt", true, false, 11},
    {"AES_128_CBC_SHA1_SSL3", EVP_aead_aes_128_cbc_sha1_ssl3,
     "aes_128_cbc_sha1_ssl3_tests.txt", true, false, 9},
    {"AES_256_CBC_SHA1_SSL3", EVP_aead_aes_256_cbc_sha1_ssl3,
     "aes_256_cbc_sha1_ssl3_tests.txt", true, false, 9},
    {"DES_EDE3_CBC_SHA1_SSL3", EVP_aead_des_ede3_cbc_sha1_ssl3,
     "des_ede3_cbc_sha1_ssl3_tests.txt", true, false, 9},
    {"AES_128_CTR_HMAC_SHA256", EVP_aead_aes_128_ctr_hmac_sha256,
     "aes_128_ctr_hmac_sha256.txt", false, true, 0},
    {"AES_256_CTR_HMAC_SHA256", EVP_aead_aes_256_ctr_hmac_sha256,
     "aes_256_ctr_hmac_sha256.txt", false, true, 0},
};

class PerAEADTest : public testing::TestWithParam<KnownAEAD> {
 public:
  const EVP_AEAD *aead() { return GetParam().func(); }
};

INSTANTIATE_TEST_CASE_P(, PerAEADTest, testing::ValuesIn(kAEADs),
                        [](const testing::TestParamInfo<KnownAEAD> &params)
                            -> std::string { return params.param.name; });

// Tests an AEAD against a series of test vectors from a file, using the
// FileTest format. As an example, here's a valid test case:
//
//   KEY: 5a19f3173586b4c42f8412f4d5a786531b3231753e9e00998aec12fda8df10e4
//   NONCE: 978105dfce667bf4
//   IN: 6a4583908d
//   AD: b654574932
//   CT: 5294265a60
//   TAG: 1d45758621762e061368e68868e2f929
TEST_P(PerAEADTest, TestVector) {
  std::string test_vectors = "crypto/cipher_extra/test/";
  test_vectors += GetParam().test_vectors;
  FileTestGTest(test_vectors.c_str(), [&](FileTest *t) {
    std::vector<uint8_t> key, nonce, in, ad, ct, tag;
    ASSERT_TRUE(t->GetBytes(&key, "KEY"));
    ASSERT_TRUE(t->GetBytes(&nonce, "NONCE"));
    ASSERT_TRUE(t->GetBytes(&in, "IN"));
    ASSERT_TRUE(t->GetBytes(&ad, "AD"));
    ASSERT_TRUE(t->GetBytes(&ct, "CT"));
    ASSERT_TRUE(t->GetBytes(&tag, "TAG"));
    size_t tag_len = tag.size();
    if (t->HasAttribute("TAG_LEN")) {
      // Legacy AEADs are MAC-then-encrypt and may include padding in the TAG
      // field. TAG_LEN contains the actual size of the digest in that case.
      std::string tag_len_str;
      ASSERT_TRUE(t->GetAttribute(&tag_len_str, "TAG_LEN"));
      tag_len = strtoul(tag_len_str.c_str(), nullptr, 10);
      ASSERT_TRUE(tag_len);
    }

    bssl::ScopedEVP_AEAD_CTX ctx;
    ASSERT_TRUE(EVP_AEAD_CTX_init_with_direction(
        ctx.get(), aead(), key.data(), key.size(), tag_len, evp_aead_seal));

    std::vector<uint8_t> out(in.size() + EVP_AEAD_max_overhead(aead()));
    if (!t->HasAttribute("NO_SEAL")) {
      size_t out_len;
      ASSERT_TRUE(EVP_AEAD_CTX_seal(ctx.get(), out.data(), &out_len, out.size(),
                                    nonce.data(), nonce.size(), in.data(),
                                    in.size(), ad.data(), ad.size()));
      out.resize(out_len);

      ASSERT_EQ(out.size(), ct.size() + tag.size());
      EXPECT_EQ(Bytes(ct), Bytes(out.data(), ct.size()));
      EXPECT_EQ(Bytes(tag), Bytes(out.data() + ct.size(), tag.size()));
    } else {
      out.resize(ct.size() + tag.size());
      OPENSSL_memcpy(out.data(), ct.data(), ct.size());
      OPENSSL_memcpy(out.data() + ct.size(), tag.data(), tag.size());
    }

    // The "stateful" AEADs for implementing pre-AEAD cipher suites need to be
    // reset after each operation.
    ctx.Reset();
    ASSERT_TRUE(EVP_AEAD_CTX_init_with_direction(
        ctx.get(), aead(), key.data(), key.size(), tag_len, evp_aead_open));

    std::vector<uint8_t> out2(out.size());
    size_t out2_len;
    int ret = EVP_AEAD_CTX_open(ctx.get(), out2.data(), &out2_len, out2.size(),
                                nonce.data(), nonce.size(), out.data(),
                                out.size(), ad.data(), ad.size());
    if (t->HasAttribute("FAILS")) {
      ASSERT_FALSE(ret) << "Decrypted bad data.";
      ERR_clear_error();
      return;
    }

    ASSERT_TRUE(ret) << "Failed to decrypt.";
    out2.resize(out2_len);
    EXPECT_EQ(Bytes(in), Bytes(out2));

    // The "stateful" AEADs for implementing pre-AEAD cipher suites need to be
    // reset after each operation.
    ctx.Reset();
    ASSERT_TRUE(EVP_AEAD_CTX_init_with_direction(
        ctx.get(), aead(), key.data(), key.size(), tag_len, evp_aead_open));

    // Garbage at the end isn't ignored.
    out.push_back(0);
    out2.resize(out.size());
    EXPECT_FALSE(EVP_AEAD_CTX_open(
        ctx.get(), out2.data(), &out2_len, out2.size(), nonce.data(),
        nonce.size(), out.data(), out.size(), ad.data(), ad.size()))
        << "Decrypted bad data with trailing garbage.";
    ERR_clear_error();

    // The "stateful" AEADs for implementing pre-AEAD cipher suites need to be
    // reset after each operation.
    ctx.Reset();
    ASSERT_TRUE(EVP_AEAD_CTX_init_with_direction(
        ctx.get(), aead(), key.data(), key.size(), tag_len, evp_aead_open));

    // Verify integrity is checked.
    out[0] ^= 0x80;
    out.resize(out.size() - 1);
    out2.resize(out.size());
    EXPECT_FALSE(EVP_AEAD_CTX_open(
        ctx.get(), out2.data(), &out2_len, out2.size(), nonce.data(),
        nonce.size(), out.data(), out.size(), ad.data(), ad.size()))
        << "Decrypted bad data with corrupted byte.";
    ERR_clear_error();
  });
}

TEST_P(PerAEADTest, TestExtraInput) {
  const KnownAEAD &aead_config = GetParam();
  if (!aead()->seal_scatter_supports_extra_in) {
    return;
  }

  const std::string test_vectors =
      "crypto/cipher_extra/test/" + std::string(aead_config.test_vectors);
  FileTestGTest(test_vectors.c_str(), [&](FileTest *t) {
    if (t->HasAttribute("NO_SEAL") ||
        t->HasAttribute("FAILS")) {
      t->SkipCurrent();
      return;
    }

    std::vector<uint8_t> key, nonce, in, ad, ct, tag;
    ASSERT_TRUE(t->GetBytes(&key, "KEY"));
    ASSERT_TRUE(t->GetBytes(&nonce, "NONCE"));
    ASSERT_TRUE(t->GetBytes(&in, "IN"));
    ASSERT_TRUE(t->GetBytes(&ad, "AD"));
    ASSERT_TRUE(t->GetBytes(&ct, "CT"));
    ASSERT_TRUE(t->GetBytes(&tag, "TAG"));

    bssl::ScopedEVP_AEAD_CTX ctx;
    ASSERT_TRUE(EVP_AEAD_CTX_init(ctx.get(), aead(), key.data(), key.size(),
                                  tag.size(), nullptr));
    std::vector<uint8_t> out_tag(EVP_AEAD_max_overhead(aead()) + in.size());
    std::vector<uint8_t> out(in.size());

    for (size_t extra_in_size = 0; extra_in_size < in.size(); extra_in_size++) {
      size_t tag_bytes_written;
      SCOPED_TRACE(extra_in_size);
      ASSERT_TRUE(EVP_AEAD_CTX_seal_scatter(
          ctx.get(), out.data(), out_tag.data(), &tag_bytes_written,
          out_tag.size(), nonce.data(), nonce.size(), in.data(),
          in.size() - extra_in_size, in.data() + in.size() - extra_in_size,
          extra_in_size, ad.data(), ad.size()));

      ASSERT_EQ(tag_bytes_written, extra_in_size + tag.size());

      memcpy(out.data() + in.size() - extra_in_size, out_tag.data(),
             extra_in_size);

      EXPECT_EQ(Bytes(ct), Bytes(out.data(), in.size()));
      EXPECT_EQ(Bytes(tag), Bytes(out_tag.data() + extra_in_size,
                                  tag_bytes_written - extra_in_size));
    }
  });
}

TEST_P(PerAEADTest, TestVectorScatterGather) {
  std::string test_vectors = "crypto/cipher_extra/test/";
  const KnownAEAD &aead_config = GetParam();
  test_vectors += aead_config.test_vectors;
  FileTestGTest(test_vectors.c_str(), [&](FileTest *t) {
    std::vector<uint8_t> key, nonce, in, ad, ct, tag;
    ASSERT_TRUE(t->GetBytes(&key, "KEY"));
    ASSERT_TRUE(t->GetBytes(&nonce, "NONCE"));
    ASSERT_TRUE(t->GetBytes(&in, "IN"));
    ASSERT_TRUE(t->GetBytes(&ad, "AD"));
    ASSERT_TRUE(t->GetBytes(&ct, "CT"));
    ASSERT_TRUE(t->GetBytes(&tag, "TAG"));
    size_t tag_len = tag.size();
    if (t->HasAttribute("TAG_LEN")) {
      // Legacy AEADs are MAC-then-encrypt and may include padding in the TAG
      // field. TAG_LEN contains the actual size of the digest in that case.
      std::string tag_len_str;
      ASSERT_TRUE(t->GetAttribute(&tag_len_str, "TAG_LEN"));
      tag_len = strtoul(tag_len_str.c_str(), nullptr, 10);
      ASSERT_TRUE(tag_len);
    }

    bssl::ScopedEVP_AEAD_CTX ctx;
    ASSERT_TRUE(EVP_AEAD_CTX_init_with_direction(
        ctx.get(), aead(), key.data(), key.size(), tag_len, evp_aead_seal));

    std::vector<uint8_t> out(in.size());
    std::vector<uint8_t> out_tag(EVP_AEAD_max_overhead(aead()));
    if (!t->HasAttribute("NO_SEAL")) {
      size_t out_tag_len;
      ASSERT_TRUE(EVP_AEAD_CTX_seal_scatter(
          ctx.get(), out.data(), out_tag.data(), &out_tag_len, out_tag.size(),
          nonce.data(), nonce.size(), in.data(), in.size(), nullptr, 0,
          ad.data(), ad.size()));
      out_tag.resize(out_tag_len);

      ASSERT_EQ(out.size(), ct.size());
      ASSERT_EQ(out_tag.size(), tag.size());
      EXPECT_EQ(Bytes(ct), Bytes(out.data(), ct.size()));
      EXPECT_EQ(Bytes(tag), Bytes(out_tag.data(), tag.size()));
    } else {
      out.resize(ct.size());
      out_tag.resize(tag.size());
      OPENSSL_memcpy(out.data(), ct.data(), ct.size());
      OPENSSL_memcpy(out_tag.data(), tag.data(), tag.size());
    }

    // The "stateful" AEADs for implementing pre-AEAD cipher suites need to be
    // reset after each operation.
    ctx.Reset();
    ASSERT_TRUE(EVP_AEAD_CTX_init_with_direction(
        ctx.get(), aead(), key.data(), key.size(), tag_len, evp_aead_open));

    std::vector<uint8_t> out2(out.size());
    int ret = EVP_AEAD_CTX_open_gather(
        ctx.get(), out2.data(), nonce.data(), nonce.size(), out.data(),
        out.size(), out_tag.data(), out_tag.size(), ad.data(), ad.size());

    // Skip decryption for AEADs that don't implement open_gather().
    if (!ret) {
      int err = ERR_peek_error();
      if (ERR_GET_LIB(err) == ERR_LIB_CIPHER &&
          ERR_GET_REASON(err) == CIPHER_R_CTRL_NOT_IMPLEMENTED) {
          t->SkipCurrent();
          return;
        }
    }

    if (t->HasAttribute("FAILS")) {
      ASSERT_FALSE(ret) << "Decrypted bad data";
      ERR_clear_error();
      return;
    }

    ASSERT_TRUE(ret) << "Failed to decrypt: "
                     << ERR_reason_error_string(ERR_get_error());
    EXPECT_EQ(Bytes(in), Bytes(out2));

    // The "stateful" AEADs for implementing pre-AEAD cipher suites need to be
    // reset after each operation.
    ctx.Reset();
    ASSERT_TRUE(EVP_AEAD_CTX_init_with_direction(
        ctx.get(), aead(), key.data(), key.size(), tag_len, evp_aead_open));

    // Garbage at the end isn't ignored.
    out_tag.push_back(0);
    out2.resize(out.size());
    EXPECT_FALSE(EVP_AEAD_CTX_open_gather(
        ctx.get(), out2.data(), nonce.data(), nonce.size(), out.data(),
        out.size(), out_tag.data(), out_tag.size(), ad.data(), ad.size()))
        << "Decrypted bad data with trailing garbage.";
    ERR_clear_error();

    // The "stateful" AEADs for implementing pre-AEAD cipher suites need to be
    // reset after each operation.
    ctx.Reset();
    ASSERT_TRUE(EVP_AEAD_CTX_init_with_direction(
        ctx.get(), aead(), key.data(), key.size(), tag_len, evp_aead_open));

    // Verify integrity is checked.
    out_tag[0] ^= 0x80;
    out_tag.resize(out_tag.size() - 1);
    out2.resize(out.size());
    EXPECT_FALSE(EVP_AEAD_CTX_open_gather(
        ctx.get(), out2.data(), nonce.data(), nonce.size(), out.data(),
        out.size(), out_tag.data(), out_tag.size(), ad.data(), ad.size()))
        << "Decrypted bad data with corrupted byte.";
    ERR_clear_error();

    ctx.Reset();
    ASSERT_TRUE(EVP_AEAD_CTX_init_with_direction(
        ctx.get(), aead(), key.data(), key.size(), tag_len, evp_aead_open));

    // Check edge case for tag length.
    EXPECT_FALSE(EVP_AEAD_CTX_open_gather(
        ctx.get(), out2.data(), nonce.data(), nonce.size(), out.data(),
        out.size(), out_tag.data(), 0, ad.data(), ad.size()))
        << "Decrypted bad data with corrupted byte.";
    ERR_clear_error();
  });
}

TEST_P(PerAEADTest, CleanupAfterInitFailure) {
  uint8_t key[EVP_AEAD_MAX_KEY_LENGTH];
  OPENSSL_memset(key, 0, sizeof(key));
  const size_t key_len = EVP_AEAD_key_length(aead());
  ASSERT_GE(sizeof(key), key_len);

  EVP_AEAD_CTX ctx;
  ASSERT_FALSE(EVP_AEAD_CTX_init(
      &ctx, aead(), key, key_len,
      9999 /* a silly tag length to trigger an error */, NULL /* ENGINE */));
  ERR_clear_error();

  // Running a second, failed _init should not cause a memory leak.
  ASSERT_FALSE(EVP_AEAD_CTX_init(
      &ctx, aead(), key, key_len,
      9999 /* a silly tag length to trigger an error */, NULL /* ENGINE */));
  ERR_clear_error();

  // Calling _cleanup on an |EVP_AEAD_CTX| after a failed _init should be a
  // no-op.
  EVP_AEAD_CTX_cleanup(&ctx);
}

TEST_P(PerAEADTest, TruncatedTags) {
  if (!GetParam().truncated_tags) {
    return;
  }

  uint8_t key[EVP_AEAD_MAX_KEY_LENGTH];
  OPENSSL_memset(key, 0, sizeof(key));
  const size_t key_len = EVP_AEAD_key_length(aead());
  ASSERT_GE(sizeof(key), key_len);

  uint8_t nonce[EVP_AEAD_MAX_NONCE_LENGTH];
  OPENSSL_memset(nonce, 0, sizeof(nonce));
  const size_t nonce_len = EVP_AEAD_nonce_length(aead());
  ASSERT_GE(sizeof(nonce), nonce_len);

  bssl::ScopedEVP_AEAD_CTX ctx;
  ASSERT_TRUE(EVP_AEAD_CTX_init(ctx.get(), aead(), key, key_len,
                                1 /* one byte tag */, NULL /* ENGINE */));

  const uint8_t plaintext[1] = {'A'};

  uint8_t ciphertext[128];
  size_t ciphertext_len;
  constexpr uint8_t kSentinel = 42;
  OPENSSL_memset(ciphertext, kSentinel, sizeof(ciphertext));

  ASSERT_TRUE(EVP_AEAD_CTX_seal(ctx.get(), ciphertext, &ciphertext_len,
                                sizeof(ciphertext), nonce, nonce_len, plaintext,
                                sizeof(plaintext), nullptr /* ad */, 0));

  for (size_t i = ciphertext_len; i < sizeof(ciphertext); i++) {
    // Sealing must not write past where it said it did.
    EXPECT_EQ(kSentinel, ciphertext[i])
        << "Sealing wrote off the end of the buffer.";
  }

  const size_t overhead_used = ciphertext_len - sizeof(plaintext);
  const size_t expected_overhead =
      1 + EVP_AEAD_max_overhead(aead()) - EVP_AEAD_max_tag_len(aead());
  EXPECT_EQ(overhead_used, expected_overhead)
      << "AEAD is probably ignoring request to truncate tags.";

  uint8_t plaintext2[sizeof(plaintext) + 16];
  OPENSSL_memset(plaintext2, kSentinel, sizeof(plaintext2));

  size_t plaintext2_len;
  ASSERT_TRUE(EVP_AEAD_CTX_open(
      ctx.get(), plaintext2, &plaintext2_len, sizeof(plaintext2), nonce,
      nonce_len, ciphertext, ciphertext_len, nullptr /* ad */, 0))
      << "Opening with truncated tag didn't work.";

  for (size_t i = plaintext2_len; i < sizeof(plaintext2); i++) {
    // Likewise, opening should also stay within bounds.
    EXPECT_EQ(kSentinel, plaintext2[i])
        << "Opening wrote off the end of the buffer.";
  }

  EXPECT_EQ(Bytes(plaintext), Bytes(plaintext2, plaintext2_len));
}

TEST_P(PerAEADTest, AliasedBuffers) {
  if (GetParam().limited_implementation) {
    return;
  }

  const size_t key_len = EVP_AEAD_key_length(aead());
  const size_t nonce_len = EVP_AEAD_nonce_length(aead());
  const size_t max_overhead = EVP_AEAD_max_overhead(aead());

  std::vector<uint8_t> key(key_len, 'a');
  bssl::ScopedEVP_AEAD_CTX ctx;
  ASSERT_TRUE(EVP_AEAD_CTX_init(ctx.get(), aead(), key.data(), key_len,
                                EVP_AEAD_DEFAULT_TAG_LENGTH, nullptr));

  static const uint8_t kPlaintext[260] =
      "testing123456testing123456testing123456testing123456testing123456testing"
      "123456testing123456testing123456testing123456testing123456testing123456t"
      "esting123456testing123456testing123456testing123456testing123456testing1"
      "23456testing123456testing123456testing12345";
  const std::vector<size_t> offsets = {
      0,  1,  2,  8,  15, 16,  17,  31,  32,  33,  63,
      64, 65, 95, 96, 97, 127, 128, 129, 255, 256, 257,
  };

  std::vector<uint8_t> nonce(nonce_len, 'b');
  std::vector<uint8_t> valid_encryption(sizeof(kPlaintext) + max_overhead);
  size_t valid_encryption_len;
  ASSERT_TRUE(EVP_AEAD_CTX_seal(
      ctx.get(), valid_encryption.data(), &valid_encryption_len,
      sizeof(kPlaintext) + max_overhead, nonce.data(), nonce_len, kPlaintext,
      sizeof(kPlaintext), nullptr, 0))
      << "EVP_AEAD_CTX_seal failed with disjoint buffers.";

  // Test with out != in which we expect to fail.
  std::vector<uint8_t> buffer(2 + valid_encryption_len);
  uint8_t *in = buffer.data() + 1;
  uint8_t *out1 = buffer.data();
  uint8_t *out2 = buffer.data() + 2;

  OPENSSL_memcpy(in, kPlaintext, sizeof(kPlaintext));
  size_t out_len;
  EXPECT_FALSE(EVP_AEAD_CTX_seal(
      ctx.get(), out1 /* in - 1 */, &out_len, sizeof(kPlaintext) + max_overhead,
      nonce.data(), nonce_len, in, sizeof(kPlaintext), nullptr, 0));
  EXPECT_FALSE(EVP_AEAD_CTX_seal(
      ctx.get(), out2 /* in + 1 */, &out_len, sizeof(kPlaintext) + max_overhead,
      nonce.data(), nonce_len, in, sizeof(kPlaintext), nullptr, 0));
  ERR_clear_error();

  OPENSSL_memcpy(in, valid_encryption.data(), valid_encryption_len);
  EXPECT_FALSE(EVP_AEAD_CTX_open(ctx.get(), out1 /* in - 1 */, &out_len,
                                 valid_encryption_len, nonce.data(), nonce_len,
                                 in, valid_encryption_len, nullptr, 0));
  EXPECT_FALSE(EVP_AEAD_CTX_open(ctx.get(), out2 /* in + 1 */, &out_len,
                                 valid_encryption_len, nonce.data(), nonce_len,
                                 in, valid_encryption_len, nullptr, 0));
  ERR_clear_error();

  // Test with out == in, which we expect to work.
  OPENSSL_memcpy(in, kPlaintext, sizeof(kPlaintext));

  ASSERT_TRUE(EVP_AEAD_CTX_seal(ctx.get(), in, &out_len,
                                sizeof(kPlaintext) + max_overhead, nonce.data(),
                                nonce_len, in, sizeof(kPlaintext), nullptr, 0));
  EXPECT_EQ(Bytes(valid_encryption.data(), valid_encryption_len),
            Bytes(in, out_len));

  OPENSSL_memcpy(in, valid_encryption.data(), valid_encryption_len);
  ASSERT_TRUE(EVP_AEAD_CTX_open(ctx.get(), in, &out_len, valid_encryption_len,
                                nonce.data(), nonce_len, in,
                                valid_encryption_len, nullptr, 0));
  EXPECT_EQ(Bytes(kPlaintext), Bytes(in, out_len));
}

TEST_P(PerAEADTest, UnalignedInput) {
  alignas(64) uint8_t key[EVP_AEAD_MAX_KEY_LENGTH + 1];
  alignas(64) uint8_t nonce[EVP_AEAD_MAX_NONCE_LENGTH + 1];
  alignas(64) uint8_t plaintext[32 + 1];
  alignas(64) uint8_t ad[32 + 1];
  OPENSSL_memset(key, 'K', sizeof(key));
  OPENSSL_memset(nonce, 'N', sizeof(nonce));
  OPENSSL_memset(plaintext, 'P', sizeof(plaintext));
  OPENSSL_memset(ad, 'A', sizeof(ad));
  const size_t key_len = EVP_AEAD_key_length(aead());
  ASSERT_GE(sizeof(key) - 1, key_len);
  const size_t nonce_len = EVP_AEAD_nonce_length(aead());
  ASSERT_GE(sizeof(nonce) - 1, nonce_len);
  const size_t ad_len =
      GetParam().ad_len != 0 ? GetParam().ad_len : sizeof(ad) - 1;
  ASSERT_GE(sizeof(ad) - 1, ad_len);

  // Encrypt some input.
  bssl::ScopedEVP_AEAD_CTX ctx;
  ASSERT_TRUE(EVP_AEAD_CTX_init_with_direction(
      ctx.get(), aead(), key + 1, key_len, EVP_AEAD_DEFAULT_TAG_LENGTH,
      evp_aead_seal));
  alignas(64) uint8_t ciphertext[sizeof(plaintext) + EVP_AEAD_MAX_OVERHEAD];
  size_t ciphertext_len;
  ASSERT_TRUE(EVP_AEAD_CTX_seal(ctx.get(), ciphertext + 1, &ciphertext_len,
                                sizeof(ciphertext) - 1, nonce + 1, nonce_len,
                                plaintext + 1, sizeof(plaintext) - 1, ad + 1,
                                ad_len));

  // It must successfully decrypt.
  alignas(64) uint8_t out[sizeof(ciphertext)];
  ctx.Reset();
  ASSERT_TRUE(EVP_AEAD_CTX_init_with_direction(
      ctx.get(), aead(), key + 1, key_len, EVP_AEAD_DEFAULT_TAG_LENGTH,
      evp_aead_open));
  size_t out_len;
  ASSERT_TRUE(EVP_AEAD_CTX_open(ctx.get(), out + 1, &out_len, sizeof(out) - 1,
                                nonce + 1, nonce_len, ciphertext + 1,
                                ciphertext_len, ad + 1, ad_len));
  EXPECT_EQ(Bytes(plaintext + 1, sizeof(plaintext) - 1),
            Bytes(out + 1, out_len));
}

TEST_P(PerAEADTest, Overflow) {
  alignas(64) uint8_t key[EVP_AEAD_MAX_KEY_LENGTH];
  OPENSSL_memset(key, 'K', sizeof(key));

  bssl::ScopedEVP_AEAD_CTX ctx;
  const size_t max_tag_len = EVP_AEAD_max_tag_len(aead());
  ASSERT_TRUE(EVP_AEAD_CTX_init_with_direction(ctx.get(), aead(), key,
                                               EVP_AEAD_key_length(aead()),
                                               max_tag_len, evp_aead_seal));

  uint8_t plaintext[1] = {0};
  uint8_t ciphertext[1024] = {0};
  size_t ciphertext_len;
  // The AEAD must not overflow when calculating the ciphertext length.
  ASSERT_FALSE(EVP_AEAD_CTX_seal(
      ctx.get(), ciphertext, &ciphertext_len, sizeof(ciphertext), nullptr, 0,
      plaintext, std::numeric_limits<size_t>::max() - max_tag_len + 1, nullptr,
      0));
  ERR_clear_error();

  // (Can't test the scatter interface because it'll attempt to zero the output
  // buffer on error and the primary output buffer is implicitly the same size
  // as the input.)
}

// Test that EVP_aead_aes_128_gcm and EVP_aead_aes_256_gcm reject empty nonces.
// AES-GCM is not defined for those.
TEST(AEADTest, AESGCMEmptyNonce) {
  static const uint8_t kZeros[32] = {0};

  // Test AES-128-GCM.
  uint8_t buf[16];
  size_t len;
  bssl::ScopedEVP_AEAD_CTX ctx;
  ASSERT_TRUE(EVP_AEAD_CTX_init(ctx.get(), EVP_aead_aes_128_gcm(), kZeros, 16,
                                EVP_AEAD_DEFAULT_TAG_LENGTH, nullptr));

  EXPECT_FALSE(EVP_AEAD_CTX_seal(ctx.get(), buf, &len, sizeof(buf),
                                 nullptr /* nonce */, 0, nullptr /* in */, 0,
                                 nullptr /* ad */, 0));
  uint32_t err = ERR_get_error();
  EXPECT_EQ(ERR_LIB_CIPHER, ERR_GET_LIB(err));
  EXPECT_EQ(CIPHER_R_INVALID_NONCE_SIZE, ERR_GET_REASON(err));

  EXPECT_FALSE(EVP_AEAD_CTX_open(ctx.get(), buf, &len, sizeof(buf),
                                 nullptr /* nonce */, 0, kZeros /* in */,
                                 sizeof(kZeros), nullptr /* ad */, 0));
  err = ERR_get_error();
  EXPECT_EQ(ERR_LIB_CIPHER, ERR_GET_LIB(err));
  EXPECT_EQ(CIPHER_R_INVALID_NONCE_SIZE, ERR_GET_REASON(err));

  // Test AES-256-GCM.
  ctx.Reset();
  ASSERT_TRUE(EVP_AEAD_CTX_init(ctx.get(), EVP_aead_aes_256_gcm(), kZeros, 32,
                                EVP_AEAD_DEFAULT_TAG_LENGTH, nullptr));

  EXPECT_FALSE(EVP_AEAD_CTX_seal(ctx.get(), buf, &len, sizeof(buf),
                                 nullptr /* nonce */, 0, nullptr /* in */, 0,
                                 nullptr /* ad */, 0));
  err = ERR_get_error();
  EXPECT_EQ(ERR_LIB_CIPHER, ERR_GET_LIB(err));
  EXPECT_EQ(CIPHER_R_INVALID_NONCE_SIZE, ERR_GET_REASON(err));

  EXPECT_FALSE(EVP_AEAD_CTX_open(ctx.get(), buf, &len, sizeof(buf),
                                 nullptr /* nonce */, 0, kZeros /* in */,
                                 sizeof(kZeros), nullptr /* ad */, 0));
  err = ERR_get_error();
  EXPECT_EQ(ERR_LIB_CIPHER, ERR_GET_LIB(err));
  EXPECT_EQ(CIPHER_R_INVALID_NONCE_SIZE, ERR_GET_REASON(err));
}
