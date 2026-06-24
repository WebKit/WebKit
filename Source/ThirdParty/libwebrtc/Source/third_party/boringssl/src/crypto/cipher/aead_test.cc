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

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include <openssl/aead.h>
#include <openssl/cipher.h>
#include <openssl/err.h>
#include <openssl/span.h>

#include "../fipsmodule/cipher/internal.h"
#include "../internal.h"
#include "../test/abi_test.h"
#include "../test/file_test.h"
#include "../test/test_util.h"
#include "../test/wycheproof_util.h"
#include "internal.h"


BSSL_NAMESPACE_BEGIN
namespace {

// kLimitedImplementation indicates that tests that assume a generic AEAD
// interface should not be performed. For example, the key-wrap AEADs only
// handle inputs that are a multiple of eight bytes in length and the TLS CBC
// AEADs have the concept of “direction”.
constexpr uint32_t kLimitedImplementation = 1 << 0;
// kCanTruncateTags indicates that the AEAD supports truncatating tags to
// arbitrary lengths.
constexpr uint32_t kCanTruncateTags = 1 << 1;
// kVariableNonce indicates that the AEAD supports a variable-length nonce.
constexpr uint32_t kVariableNonce = 1 << 2;
// kNondeterministic indicates that the AEAD performs randomised encryption thus
// one cannot assume that encrypting the same data will result in the same
// ciphertext.
constexpr uint32_t kNondeterministic = 1 << 3;
// kVariableTag indicates that the AEAD outputs a variable length tag.
constexpr uint32_t kVariableTag = 1 << 4;
// kSkipIOVec indicates that the test vectors should be skipped when running
// iovec tests.
constexpr uint32_t kSkipIOVec = 1 << 5;

struct KnownAEAD {
  const char name[40];
  const EVP_AEAD *(*func)();
  const char *test_vectors;
  uint32_t flags;

  // Optional settings - leave out if not needed.
  std::optional<size_t> required_ad_length = std::nullopt;
  std::optional<size_t> minimum_tag_length = std::nullopt;

  std::string TestVectorPath() const {
    return std::string("crypto/cipher/test/") + test_vectors;
  }
};

static const struct KnownAEAD kAEADs[] = {
    {"AES_128_GCM", EVP_aead_aes_128_gcm, "aes_128_gcm_tests.txt",
     kCanTruncateTags | kVariableNonce},

    {"AES_192_GCM", EVP_aead_aes_192_gcm, "aes_192_gcm_tests.txt",
     kCanTruncateTags | kVariableNonce},

    {"AES_256_GCM", EVP_aead_aes_256_gcm, "aes_256_gcm_tests.txt",
     kCanTruncateTags | kVariableNonce},

    // A set of 31,000 test vectors imported from NIST. We skip iovec tests
    // because multiplicatively testing these vectors against different iovec
    // splits takes a very long time and is of low value. Instead, we assume
    // that our normal test vectors provide sufficient iovec coverage, and just
    // run them through the non-iovec APIs to ensure the overall AEAD
    // implementation matches.
    {"AES_128_GCM_NIST", EVP_aead_aes_128_gcm, "nist_cavp/aes_128_gcm.txt",
     kCanTruncateTags | kVariableNonce | kSkipIOVec},
    {"AES_256_GCM_NIST", EVP_aead_aes_256_gcm, "nist_cavp/aes_256_gcm.txt",
     kCanTruncateTags | kVariableNonce | kSkipIOVec},

    {"AES_128_GCM_SIV", EVP_aead_aes_128_gcm_siv, "aes_128_gcm_siv_tests.txt",
     0},

    {"AES_256_GCM_SIV", EVP_aead_aes_256_gcm_siv, "aes_256_gcm_siv_tests.txt",
     0},

    {"AES_128_GCM_RandomNonce", EVP_aead_aes_128_gcm_randnonce,
     "aes_128_gcm_randnonce_tests.txt", kNondeterministic | kCanTruncateTags,
     /*required_ad_length=*/std::nullopt, /*minimum_tag_length=*/13},

    {"AES_256_GCM_RandomNonce", EVP_aead_aes_256_gcm_randnonce,
     "aes_256_gcm_randnonce_tests.txt", kNondeterministic | kCanTruncateTags,
     /*required_ad_length=*/std::nullopt, /*minimum_tag_length=*/13},

    {"ChaCha20Poly1305", EVP_aead_chacha20_poly1305,
     "chacha20_poly1305_tests.txt", kCanTruncateTags},

    {"XChaCha20Poly1305", EVP_aead_xchacha20_poly1305,
     "xchacha20_poly1305_tests.txt", kCanTruncateTags},

    {"AES_128_CBC_SHA1_TLS", EVP_aead_aes_128_cbc_sha1_tls,
     "aes_128_cbc_sha1_tls_tests.txt", kLimitedImplementation | kVariableTag,
     /*required_ad_length=*/11},

    {"AES_128_CBC_SHA1_TLSImplicitIV",
     EVP_aead_aes_128_cbc_sha1_tls_implicit_iv,
     "aes_128_cbc_sha1_tls_implicit_iv_tests.txt",
     kLimitedImplementation | kVariableTag, /*required_ad_length=*/11},

    {"AES_256_CBC_SHA1_TLS", EVP_aead_aes_256_cbc_sha1_tls,
     "aes_256_cbc_sha1_tls_tests.txt", kLimitedImplementation | kVariableTag,
     /*required_ad_length=*/11},

    {"AES_256_CBC_SHA1_TLSImplicitIV",
     EVP_aead_aes_256_cbc_sha1_tls_implicit_iv,
     "aes_256_cbc_sha1_tls_implicit_iv_tests.txt",
     kLimitedImplementation | kVariableTag, /*required_ad_length=*/11},

    {"DES_EDE3_CBC_SHA1_TLS", EVP_aead_des_ede3_cbc_sha1_tls,
     "des_ede3_cbc_sha1_tls_tests.txt", kLimitedImplementation | kVariableTag,
     /*required_ad_length=*/11},

    {"DES_EDE3_CBC_SHA1_TLSImplicitIV",
     EVP_aead_des_ede3_cbc_sha1_tls_implicit_iv,
     "des_ede3_cbc_sha1_tls_implicit_iv_tests.txt",
     kLimitedImplementation | kVariableTag, /*required_ad_length=*/11},

    {"AES_128_CBC_SHA256_TLS", EVP_aead_aes_128_cbc_sha256_tls,
     "aes_128_cbc_sha256_tls_tests.txt", kLimitedImplementation | kVariableTag,
     /*required_ad_length=*/11},

    {"AES_128_CTR_HMAC_SHA256", EVP_aead_aes_128_ctr_hmac_sha256,
     "aes_128_ctr_hmac_sha256.txt", kCanTruncateTags},

    {"AES_256_CTR_HMAC_SHA256", EVP_aead_aes_256_ctr_hmac_sha256,
     "aes_256_ctr_hmac_sha256.txt", kCanTruncateTags},

    {"AES_128_CCM_BLUETOOTH", EVP_aead_aes_128_ccm_bluetooth,
     "aes_128_ccm_bluetooth_tests.txt", 0},

    {"AES_128_CCM_BLUETOOTH_8", EVP_aead_aes_128_ccm_bluetooth_8,
     "aes_128_ccm_bluetooth_8_tests.txt", 0},

    {"AES_128_CCM_Matter", EVP_aead_aes_128_ccm_matter,
     "aes_128_ccm_matter_tests.txt", 0},

    {"AES_128_EAX", EVP_aead_aes_128_eax, "aes_128_eax_test.txt",
     kVariableNonce},

    {"AES_256_EAX", EVP_aead_aes_256_eax, "aes_256_eax_test.txt",
     kVariableNonce},
};

class PerAEADTest : public testing::TestWithParam<KnownAEAD> {
 public:
  const EVP_AEAD *aead() { return GetParam().func(); }
};

INSTANTIATE_TEST_SUITE_P(All, PerAEADTest, testing::ValuesIn(kAEADs),
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
  FileTestGTest(GetParam().TestVectorPath().c_str(), [&](FileTest *t) {
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

    ScopedEVP_AEAD_CTX ctx;
    ASSERT_TRUE(EVP_AEAD_CTX_init_with_direction(
        ctx.get(), aead(), key.data(), key.size(), tag_len, evp_aead_seal));

    std::vector<uint8_t> out(in.size() + EVP_AEAD_max_overhead(aead()));
    if (!t->HasAttribute("NO_SEAL") &&
        !(GetParam().flags & kNondeterministic)) {
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
  FileTestGTest(GetParam().TestVectorPath().c_str(), [&](FileTest *t) {
    if (t->HasAttribute("NO_SEAL") ||  //
        t->HasAttribute("FAILS") ||    //
        (aead_config.flags & kNondeterministic)) {
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

    size_t tag_len = tag.size();
    if (t->HasAttribute("TAG_LEN")) {
      // Legacy AEADs are MAC-then-encrypt and may include padding in the TAG
      // field. TAG_LEN contains the actual size of the digest in that case.
      std::string tag_len_str;
      ASSERT_TRUE(t->GetAttribute(&tag_len_str, "TAG_LEN"));
      tag_len = strtoul(tag_len_str.c_str(), nullptr, 10);
      ASSERT_TRUE(tag_len);
    }

    std::vector<uint8_t> out_tag(EVP_AEAD_max_overhead(aead()) + in.size());
    std::vector<uint8_t> out(in.size());

    for (size_t extra_in_size = 0; extra_in_size < in.size(); extra_in_size++) {
      SCOPED_TRACE(extra_in_size);

      ScopedEVP_AEAD_CTX ctx;
      ASSERT_TRUE(EVP_AEAD_CTX_init_with_direction(
          ctx.get(), aead(), key.data(), key.size(), tag_len, evp_aead_seal));
      size_t tag_bytes_written;
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

      // Bounds on the tag output should be checked.
      {
        std::vector<uint8_t> tag_buf(extra_in_size == 0 ? 0
                                                        : extra_in_size - 1);
        EXPECT_FALSE(EVP_AEAD_CTX_seal_scatter(
            ctx.get(), out.data(), tag_buf.data(), &tag_bytes_written,
            tag_buf.size(), nonce.data(), nonce.size(), in.data(),
            in.size() - extra_in_size, in.data() + in.size() - extra_in_size,
            extra_in_size, ad.data(), ad.size()));
      }
      {
        std::vector<uint8_t> tag_buf(extra_in_size + tag.size() - 1);
        EXPECT_FALSE(EVP_AEAD_CTX_seal_scatter(
            ctx.get(), out.data(), tag_buf.data(), &tag_bytes_written,
            tag_buf.size(), nonce.data(), nonce.size(), in.data(),
            in.size() - extra_in_size, in.data() + in.size() - extra_in_size,
            extra_in_size, ad.data(), ad.size()));
      }
    }
  });
}

TEST_P(PerAEADTest, TestVectorScatterGather) {
  const KnownAEAD &aead_config = GetParam();
  FileTestGTest(aead_config.TestVectorPath().c_str(), [&](FileTest *t) {
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

    ScopedEVP_AEAD_CTX ctx;
    ASSERT_TRUE(EVP_AEAD_CTX_init_with_direction(
        ctx.get(), aead(), key.data(), key.size(), tag_len, evp_aead_seal));

    std::vector<uint8_t> out(in.size());
    std::vector<uint8_t> out_tag(EVP_AEAD_max_overhead(aead()));
    if (!t->HasAttribute("NO_SEAL") &&
        !(aead_config.flags & kNondeterministic)) {
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
    if (!ret && ERR_equals(ERR_peek_error(), ERR_LIB_CIPHER,
                           CIPHER_R_CTRL_NOT_IMPLEMENTED)) {
      t->SkipCurrent();
      return;
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
    ASSERT_EQ(out2.size(), out.size());
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
    ASSERT_EQ(out2.size(), out.size());
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

class TestIOVecs {
 public:
  TestIOVecs() = default;

  TestIOVecs(TestIOVecs &&) = default;
  TestIOVecs &operator=(TestIOVecs &&) = default;

  TestIOVecs(const TestIOVecs &other) { *this = other.Clone(); }

  TestIOVecs &operator=(const TestIOVecs &other) {
    return (*this = other.Clone());
  }

  static TestIOVecs Split(Span<const uint8_t> inp, std::vector<size_t> splits,
                          bool in_place) {
    TestIOVecs ret;
    for (size_t i = 0; i <= splits.size(); i++) {
      size_t start = i == 0 ? 0 : splits[i - 1];
      size_t end = i == splits.size() ? inp.size() : splits[i];
      assert(start <= end);
      ret.Append(inp.subspan(start, end - start), in_place);
    }
    return ret;
  }

  Span<const CRYPTO_IOVEC> iovecs() const { return iovecs_; }

  Span<const CRYPTO_IVEC> ivecs() const { return ivecs_; }

  void Append(Span<const uint8_t> inp, bool in_place = false) {
    CRYPTO_IOVEC iovec;
    iovec.len = inp.size();
    buffers_.emplace_back(inp.begin(), inp.end());
    iovec.in = buffers_.back().data();
    if (!in_place) {
      buffers_.emplace_back(inp.size(), 'X');
    }
    iovec.out = buffers_.back().data();
    iovecs_.push_back(iovec);

    CRYPTO_IVEC ivec;
    ivec.len = iovec.len;
    ivec.in = iovec.in;
    ivecs_.push_back(ivec);
  }

  std::vector<uint8_t> Output() const {
    std::vector<uint8_t> ret;
    for (const auto &iovec : iovecs_) {
      ret.insert(ret.end(), iovec.out, iovec.out + iovec.len);
    }
    return ret;
  }

  TestIOVecs Clone() const {
    TestIOVecs out;
    for (const CRYPTO_IOVEC &iovec : iovecs_) {
      out.Append(Span(iovec.in, iovec.len),
                 /*in_place=*/iovec.in == iovec.out);
    }
    return out;
  }

 private:
  std::vector<CRYPTO_IOVEC> iovecs_;
  std::vector<CRYPTO_IVEC> ivecs_;
  std::vector<std::vector<uint8_t>> buffers_;
};

std::vector<std::vector<size_t>> InterestingSplitsForLength(size_t length,
                                                            size_t block_size) {
  std::set<std::set<size_t>> ideas;  // Sort and dedup.

  const size_t second_block_start = block_size;
  const size_t unaligned_at_start = 1;
  const size_t unaligned_at_end =
      (length % block_size == 1) ? length - 2 : length - 1;
  const size_t last_block_start = (length - 1) / block_size * block_size;

  // 1 chunk.
  ideas.insert(std::set<size_t>{});

  // 2 chunks.
  ideas.insert({0});
  ideas.insert({unaligned_at_start});
  ideas.insert({second_block_start});
  ideas.insert({last_block_start});
  ideas.insert({unaligned_at_end});
  ideas.insert({length});

  // 3 chunks.

  // Only one nonempty chunk.
  ideas.insert({unaligned_at_start, unaligned_at_start});

  // Try something useful.
  ideas.insert({unaligned_at_start, last_block_start});
  ideas.insert({second_block_start, last_block_start});
  ideas.insert({unaligned_at_start, unaligned_at_end});
  ideas.insert({second_block_start, unaligned_at_end});

  // Convert to vector.
  std::vector<std::vector<size_t>> out;
  for (const auto &idea : ideas) {
    // Skip anything out of range.
    bool ok = true;
    for (size_t pos : idea) {
      if (pos > length) {
        ok = false;
        break;
      }
    }
    if (!ok) {
      continue;
    }
    out.push_back(std::vector<size_t>(idea.begin(), idea.end()));
  }
  return out;
}

std::string FormatSplits(const std::vector<size_t> &splits) {
  std::ostringstream tracebuf;
  for (size_t i = 0; i < splits.size(); ++i) {
    if (i != 0) {
      tracebuf << ", ";
    }
    tracebuf << splits[i];
  }
  return tracebuf.str();
}

void RunSealvTests(const KnownAEAD &aead_config, bool in_place) {
  if (aead_config.flags & kSkipIOVec) {
    return;
  }

  FileTestGTest(aead_config.TestVectorPath().c_str(), [&](FileTest *t) {
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

    if (t->HasAttribute("NO_SEAL") || (aead_config.flags & kNondeterministic)) {
      t->SkipCurrent();
      return;
    }

    for (const auto &adsplits :
         InterestingSplitsForLength(ad.size(), /*block_size=*/16)) {
      SCOPED_TRACE(FormatSplits(adsplits));
      TestIOVecs advecs = TestIOVecs::Split(ad, adsplits, in_place);
      ScopedEVP_AEAD_CTX ctx;
      for (const auto &splits :
           InterestingSplitsForLength(in.size(), /*block_size=*/16)) {
        if (!adsplits.empty() && !splits.empty()) {
          // No need to test both with split AAD and split iovec. Each split
          // on its own should already hit everything interesting.
          continue;
        }
        SCOPED_TRACE(FormatSplits(splits));

        ctx.Reset();
        ASSERT_TRUE(EVP_AEAD_CTX_init_with_direction(
            ctx.get(), aead_config.func(), key.data(), key.size(), tag_len,
            evp_aead_seal));
        TestIOVecs iovecs = TestIOVecs::Split(in, splits, in_place);
        std::vector<uint8_t> out_tag(EVP_AEAD_max_overhead(aead_config.func()));
        size_t out_tag_len;
        ASSERT_TRUE(EVP_AEAD_CTX_sealv(
            ctx.get(), iovecs.iovecs().data(), iovecs.iovecs().size(),
            out_tag.data(), &out_tag_len, out_tag.size(), nonce.data(),
            nonce.size(), advecs.ivecs().data(), advecs.ivecs().size()));
        out_tag.resize(out_tag_len);
        EXPECT_EQ(Bytes(ct), Bytes(iovecs.Output()));
        EXPECT_EQ(Bytes(tag), Bytes(out_tag));
      }
    }
  });
}

void RunOpenvDetachedTests(const KnownAEAD &aead_config, bool in_place) {
  if (aead_config.flags & (kVariableTag | kSkipIOVec)) {
    // openv_detached is not supported for variable-length AEADs.
    return;
  }

  FileTestGTest(aead_config.TestVectorPath().c_str(), [&](FileTest *t) {
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
    t->IgnoreAttribute("NO_SEAL");

    for (const auto &adsplits :
         InterestingSplitsForLength(ad.size(), /*block_size=*/16)) {
      SCOPED_TRACE(FormatSplits(adsplits));
      TestIOVecs advecs = TestIOVecs::Split(ad, adsplits, in_place);
      ScopedEVP_AEAD_CTX ctx;
      for (const auto &splits :
           InterestingSplitsForLength(ct.size(), /*block_size=*/16)) {
        if (!adsplits.empty() && !splits.empty()) {
          // No need to test both with split AAD and split iovec. Each split
          // on its own should already hit everything interesting.
          continue;
        }
        SCOPED_TRACE(FormatSplits(splits));

        // The "stateful" AEADs for implementing pre-AEAD cipher suites need
        // to be reset after each operation.
        ctx.Reset();
        ASSERT_TRUE(EVP_AEAD_CTX_init_with_direction(
            ctx.get(), aead_config.func(), key.data(), key.size(), tag_len,
            evp_aead_open));

        TestIOVecs iovecs = TestIOVecs::Split(ct, splits, in_place);
        int ret = EVP_AEAD_CTX_openv_detached(
            ctx.get(), iovecs.iovecs().data(), iovecs.iovecs().size(),
            nonce.data(), nonce.size(), tag.data(), tag.size(),
            advecs.ivecs().data(), advecs.ivecs().size());

        if (t->HasAttribute("FAILS")) {
          ASSERT_FALSE(ret) << "Decrypted bad data";
          ERR_clear_error();
          continue;
        }

        ASSERT_TRUE(ret) << "Failed to decrypt: "
                         << ERR_reason_error_string(ERR_get_error());
        EXPECT_EQ(Bytes(in), Bytes(iovecs.Output()));

        // The "stateful" AEADs for implementing pre-AEAD cipher suites need
        // to be reset after each operation.
        ctx.Reset();
        ASSERT_TRUE(EVP_AEAD_CTX_init_with_direction(
            ctx.get(), aead_config.func(), key.data(), key.size(), tag_len,
            evp_aead_open));

        // Garbage at the end isn't ignored.
        std::vector<uint8_t> bad_tag = tag;
        bad_tag.push_back(0);
        iovecs = TestIOVecs::Split(ct, splits, in_place);
        EXPECT_FALSE(EVP_AEAD_CTX_openv_detached(
            ctx.get(), iovecs.iovecs().data(), iovecs.iovecs().size(),
            nonce.data(), nonce.size(), bad_tag.data(), bad_tag.size(),
            advecs.ivecs().data(), advecs.ivecs().size()))
            << "Decrypted bad data with trailing garbage.";
        ERR_clear_error();

        // The "stateful" AEADs for implementing pre-AEAD cipher suites need
        // to be reset after each operation.
        ctx.Reset();
        ASSERT_TRUE(EVP_AEAD_CTX_init_with_direction(
            ctx.get(), aead_config.func(), key.data(), key.size(), tag_len,
            evp_aead_open));

        // Verify integrity is checked.
        bad_tag = tag;
        bad_tag[0] ^= 0x80;
        iovecs = TestIOVecs::Split(ct, splits, in_place);
        EXPECT_FALSE(EVP_AEAD_CTX_openv_detached(
            ctx.get(), iovecs.iovecs().data(), iovecs.iovecs().size(),
            nonce.data(), nonce.size(), bad_tag.data(), bad_tag.size(),
            advecs.ivecs().data(), advecs.ivecs().size()))
            << "Decrypted bad data with corrupted byte.";
        ERR_clear_error();

        ctx.Reset();
        ASSERT_TRUE(EVP_AEAD_CTX_init_with_direction(
            ctx.get(), aead_config.func(), key.data(), key.size(), tag_len,
            evp_aead_open));

        // Check a zero-length tag is rejected.
        iovecs = TestIOVecs::Split(ct, splits, in_place);
        EXPECT_FALSE(EVP_AEAD_CTX_openv_detached(
            ctx.get(), iovecs.iovecs().data(), iovecs.iovecs().size(),
            nonce.data(), nonce.size(), nullptr, 0, advecs.ivecs().data(),
            advecs.ivecs().size()))
            << "Decrypted bad data with zero-length tag.";
        ERR_clear_error();
      }
    }
  });
}

void RunOpenvTests(const KnownAEAD &aead_config, bool in_place) {
  if (aead_config.flags & kSkipIOVec) {
    return;
  }

  FileTestGTest(aead_config.TestVectorPath().c_str(), [&](FileTest *t) {
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
    t->IgnoreAttribute("NO_SEAL");

    std::vector<uint8_t> combined = ct;
    combined.insert(combined.end(), tag.begin(), tag.end());

    for (const auto &adsplits :
         InterestingSplitsForLength(ad.size(), /*block_size=*/16)) {
      SCOPED_TRACE(FormatSplits(adsplits));
      TestIOVecs advecs = TestIOVecs::Split(ad, adsplits, in_place);
      ScopedEVP_AEAD_CTX ctx;
      for (const auto &splits :
           InterestingSplitsForLength(combined.size(), /*block_size=*/16)) {
        if (!adsplits.empty() && !splits.empty()) {
          // No need to test both with split AAD and split iovec. Each split
          // on its own should already hit everything interesting.
          continue;
        }
        SCOPED_TRACE(FormatSplits(splits));

        // The "stateful" AEADs for implementing pre-AEAD cipher suites need
        // to be reset after each operation.
        ctx.Reset();
        ASSERT_TRUE(EVP_AEAD_CTX_init_with_direction(
            ctx.get(), aead_config.func(), key.data(), key.size(), tag_len,
            evp_aead_open));

        size_t plaintext_len;
        TestIOVecs iovecs = TestIOVecs::Split(combined, splits, in_place);
        int ret = EVP_AEAD_CTX_openv(
            ctx.get(), iovecs.iovecs().data(), iovecs.iovecs().size(),
            &plaintext_len, nonce.data(), nonce.size(), advecs.ivecs().data(),
            advecs.ivecs().size());

        if (t->HasAttribute("FAILS")) {
          ASSERT_FALSE(ret) << "Decrypted bad data";
          ERR_clear_error();
          continue;
        }

        ASSERT_TRUE(ret) << "Failed to decrypt: "
                         << ERR_reason_error_string(ERR_get_error());
        std::vector<uint8_t> out = iovecs.Output();
        out.resize(plaintext_len);
        EXPECT_EQ(Bytes(in), Bytes(out));

        // The "stateful" AEADs for implementing pre-AEAD cipher suites need
        // to be reset after each operation.
        ctx.Reset();
        ASSERT_TRUE(EVP_AEAD_CTX_init_with_direction(
            ctx.get(), aead_config.func(), key.data(), key.size(), tag_len,
            evp_aead_open));

        // Garbage at the end isn't ignored.
        std::vector<uint8_t> combined_wrecked(combined);
        combined_wrecked.push_back(0);
        iovecs = TestIOVecs::Split(combined_wrecked, splits, in_place);
        EXPECT_FALSE(EVP_AEAD_CTX_openv(
            ctx.get(), iovecs.iovecs().data(), iovecs.iovecs().size(),
            &plaintext_len, nonce.data(), nonce.size(), advecs.ivecs().data(),
            advecs.ivecs().size()))
            << "Decrypted bad data with trailing garbage.";
        ERR_clear_error();

        // The "stateful" AEADs for implementing pre-AEAD cipher suites need
        // to be reset after each operation.
        ctx.Reset();
        ASSERT_TRUE(EVP_AEAD_CTX_init_with_direction(
            ctx.get(), aead_config.func(), key.data(), key.size(), tag_len,
            evp_aead_open));

        // Verify integrity is checked by changing the last byte.
        combined_wrecked = combined;
        combined_wrecked.back() ^= 0x80;
        iovecs = TestIOVecs::Split(combined_wrecked, splits, in_place);
        EXPECT_FALSE(EVP_AEAD_CTX_openv(
            ctx.get(), iovecs.iovecs().data(), iovecs.iovecs().size(),
            &plaintext_len, nonce.data(), nonce.size(), advecs.ivecs().data(),
            advecs.ivecs().size()))
            << "Decrypted bad data with corrupted byte.";
        ERR_clear_error();

        ctx.Reset();
        ASSERT_TRUE(EVP_AEAD_CTX_init_with_direction(
            ctx.get(), aead_config.func(), key.data(), key.size(), tag_len,
            evp_aead_open));

        // Check edge case for tag length.
        combined_wrecked = combined;
        combined_wrecked.pop_back();
        std::vector<size_t> splits_wrecked = splits;
        for (size_t &split : splits_wrecked) {
          split = std::min(split, combined_wrecked.size());
        }
        iovecs = TestIOVecs::Split(combined_wrecked, splits_wrecked, in_place);
        EXPECT_FALSE(EVP_AEAD_CTX_openv(
            ctx.get(), iovecs.iovecs().data(), iovecs.iovecs().size(),
            &plaintext_len, nonce.data(), nonce.size(), advecs.ivecs().data(),
            advecs.ivecs().size()))
            << "Decrypted bad data with corrupted byte.";
        ERR_clear_error();
      }
    }
  });
}

TEST_P(PerAEADTest, TestSealv) {
  RunSealvTests(GetParam(), /*in_place=*/false);
}

TEST_P(PerAEADTest, TestOpenv) {
  RunOpenvTests(GetParam(), /*in_place=*/false);
}

TEST_P(PerAEADTest, TestOpenvDetached) {
  RunOpenvDetachedTests(GetParam(), /*in_place=*/false);
}

TEST_P(PerAEADTest, TestSealvInPlace) {
  RunSealvTests(GetParam(), /*in_place=*/true);
}

TEST_P(PerAEADTest, TestOpenvInPlace) {
  RunOpenvTests(GetParam(), /*in_place=*/true);
}

TEST_P(PerAEADTest, TestOpenvDetachedInPlace) {
  RunOpenvDetachedTests(GetParam(), /*in_place=*/true);
}


TEST_P(PerAEADTest, CleanupAfterInitFailure) {
  uint8_t key[EVP_AEAD_MAX_KEY_LENGTH];
  OPENSSL_memset(key, 0, sizeof(key));
  const size_t key_len = EVP_AEAD_key_length(aead());
  ASSERT_GE(sizeof(key), key_len);

  EVP_AEAD_CTX ctx;
  ASSERT_FALSE(EVP_AEAD_CTX_init(
      &ctx, aead(), key, key_len,
      9999 /* a silly tag length to trigger an error */, nullptr /* ENGINE */));
  ERR_clear_error();

  // Running a second, failed _init should not cause a memory leak.
  ASSERT_FALSE(EVP_AEAD_CTX_init(
      &ctx, aead(), key, key_len,
      9999 /* a silly tag length to trigger an error */, nullptr /* ENGINE */));
  ERR_clear_error();

  // Calling _cleanup on an |EVP_AEAD_CTX| after a failed _init should be a
  // no-op.
  EVP_AEAD_CTX_cleanup(&ctx);
}

TEST_P(PerAEADTest, TruncatedTags) {
  uint8_t key[EVP_AEAD_MAX_KEY_LENGTH];
  OPENSSL_memset(key, 0, sizeof(key));
  const size_t key_len = EVP_AEAD_key_length(aead());
  ASSERT_GE(sizeof(key), key_len);

  uint8_t nonce[EVP_AEAD_MAX_NONCE_LENGTH];
  OPENSSL_memset(nonce, 0, sizeof(nonce));
  const size_t nonce_len = EVP_AEAD_nonce_length(aead());
  ASSERT_GE(sizeof(nonce), nonce_len);

  static const uint8_t ad[32] = {0};
  const size_t ad_len = GetParam().required_ad_length.value_or(16);
  ASSERT_LE(ad_len, sizeof(ad));

  size_t tag_len = GetParam().minimum_tag_length.value_or(1);
  if (!(GetParam().flags & kCanTruncateTags)) {
    // Can't truncate. Still worth running the tests to ensure memory
    // correctness.
    tag_len = EVP_AEAD_max_tag_len(aead());
  }

  const uint8_t plaintext[1] = {'A'};

  uint8_t ciphertext[128];
  size_t ciphertext_len;
  constexpr uint8_t kSentinel = 42;
  OPENSSL_memset(ciphertext, kSentinel, sizeof(ciphertext));

  const size_t expected_overhead =
      tag_len + EVP_AEAD_max_overhead(aead()) - EVP_AEAD_max_tag_len(aead());
  size_t expected_ciphertext_len = sizeof(plaintext) + expected_overhead;

  ScopedEVP_AEAD_CTX ctx;
  ASSERT_TRUE(EVP_AEAD_CTX_init_with_direction(ctx.get(), aead(), key, key_len,
                                               tag_len, evp_aead_seal));

  if (EVP_AEAD_CTX_seal(ctx.get(), ciphertext, &ciphertext_len,
                        expected_ciphertext_len - 1, nonce, nonce_len,
                        plaintext, sizeof(plaintext), ad, ad_len)) {
    // Never write more bytes than the caller said is available.
    ASSERT_LE(ciphertext_len, expected_ciphertext_len - 1);
    for (size_t i = ciphertext_len; i < sizeof(ciphertext); i++) {
      // Sealing must not write past where it said it did.
      EXPECT_EQ(kSentinel, ciphertext[i])
          << "Sealing wrote off the end of the buffer.";
    }
    ASSERT_TRUE(GetParam().flags & kLimitedImplementation)
        << "Got a shorter ciphertext with shorter-than-expected AEAD length, "
           "even though this AEAD is meant to be full featured and should "
           "respect the tag length initially provided to init perfectly.";
  }

  OPENSSL_memset(ciphertext, kSentinel, sizeof(ciphertext));

  ctx.Reset();
  ASSERT_TRUE(EVP_AEAD_CTX_init_with_direction(ctx.get(), aead(), key, key_len,
                                               tag_len, evp_aead_seal));

  ASSERT_TRUE(EVP_AEAD_CTX_seal(ctx.get(), ciphertext, &ciphertext_len,
                                expected_ciphertext_len, nonce, nonce_len,
                                plaintext, sizeof(plaintext), ad, ad_len));
  // Never write more bytes than the caller said is available.
  ASSERT_LE(ciphertext_len, expected_ciphertext_len);

  for (size_t i = ciphertext_len; i < sizeof(ciphertext); i++) {
    // Sealing must not write past where it said it did.
    EXPECT_EQ(kSentinel, ciphertext[i])
        << "Sealing wrote off the end of the buffer.";
  }

  if (!(GetParam().flags & kLimitedImplementation)) {
    EXPECT_EQ(ciphertext_len, expected_ciphertext_len)
        << "AEAD is probably ignoring request to truncate tags.";
  }

  uint8_t plaintext2[sizeof(plaintext) + 64];
  OPENSSL_memset(plaintext2, kSentinel, sizeof(plaintext2));

  ctx.Reset();
  ASSERT_TRUE(EVP_AEAD_CTX_init_with_direction(ctx.get(), aead(), key, key_len,
                                               tag_len, evp_aead_open));

  size_t plaintext2_len;
  ASSERT_TRUE(EVP_AEAD_CTX_open(ctx.get(), plaintext2, &plaintext2_len,
                                sizeof(plaintext2), nonce, nonce_len,
                                ciphertext, ciphertext_len, ad, ad_len))
      << "Opening with truncated tag didn't work.";

  size_t max_touched_len = plaintext2_len;
  if (GetParam().flags & kLimitedImplementation) {
    // Limited AEADs may use additional buffer space up to the ciphertext
    // length, provided it fits in the plaintext buffer.
    max_touched_len = std::max(max_touched_len, ciphertext_len);
  }

  for (size_t i = max_touched_len; i < sizeof(plaintext2); i++) {
    // Likewise, opening should also stay within bounds.
    EXPECT_EQ(kSentinel, plaintext2[i])
        << "Opening wrote off the end of the buffer.";
  }

  EXPECT_EQ(Bytes(plaintext), Bytes(plaintext2, plaintext2_len));
}

TEST_P(PerAEADTest, AliasedBuffers) {
  if (GetParam().flags & kLimitedImplementation) {
    return;
  }

  const size_t key_len = EVP_AEAD_key_length(aead());
  const size_t nonce_len = EVP_AEAD_nonce_length(aead());
  const size_t max_overhead = EVP_AEAD_max_overhead(aead());

  std::vector<uint8_t> key(key_len, 'a');
  ScopedEVP_AEAD_CTX ctx;
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

  if (!(GetParam().flags & kNondeterministic)) {
    EXPECT_EQ(Bytes(valid_encryption.data(), valid_encryption_len),
              Bytes(in, out_len));
  }

  OPENSSL_memcpy(in, valid_encryption.data(), valid_encryption_len);
  ASSERT_TRUE(EVP_AEAD_CTX_open(ctx.get(), in, &out_len, valid_encryption_len,
                                nonce.data(), nonce_len, in,
                                valid_encryption_len, nullptr, 0));
  EXPECT_EQ(Bytes(kPlaintext), Bytes(in, out_len));
}

TEST_P(PerAEADTest, UnalignedInput) {
  alignas(16) uint8_t key[EVP_AEAD_MAX_KEY_LENGTH + 1];
  alignas(16) uint8_t nonce[EVP_AEAD_MAX_NONCE_LENGTH + 1];
  alignas(16) uint8_t plaintext[32 + 1];
  alignas(16) uint8_t ad[32 + 1];
  OPENSSL_memset(key, 'K', sizeof(key));
  OPENSSL_memset(nonce, 'N', sizeof(nonce));
  OPENSSL_memset(plaintext, 'P', sizeof(plaintext));
  OPENSSL_memset(ad, 'A', sizeof(ad));
  const size_t key_len = EVP_AEAD_key_length(aead());
  ASSERT_GE(sizeof(key) - 1, key_len);
  const size_t nonce_len = EVP_AEAD_nonce_length(aead());
  ASSERT_GE(sizeof(nonce) - 1, nonce_len);
  const size_t ad_len = GetParam().required_ad_length.value_or(sizeof(ad) - 1);
  ASSERT_GE(sizeof(ad) - 1, ad_len);

  // Encrypt some input.
  ScopedEVP_AEAD_CTX ctx;
  ASSERT_TRUE(EVP_AEAD_CTX_init_with_direction(
      ctx.get(), aead(), key + 1, key_len, EVP_AEAD_DEFAULT_TAG_LENGTH,
      evp_aead_seal));
  alignas(16) uint8_t ciphertext[sizeof(plaintext) + EVP_AEAD_MAX_OVERHEAD];
  size_t ciphertext_len;
  ASSERT_TRUE(EVP_AEAD_CTX_seal(ctx.get(), ciphertext + 1, &ciphertext_len,
                                sizeof(ciphertext) - 1, nonce + 1, nonce_len,
                                plaintext + 1, sizeof(plaintext) - 1, ad + 1,
                                ad_len));

  // It must successfully decrypt.
  alignas(16) uint8_t out[sizeof(ciphertext)];
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
  uint8_t key[EVP_AEAD_MAX_KEY_LENGTH];
  OPENSSL_memset(key, 'K', sizeof(key));

  ScopedEVP_AEAD_CTX ctx;
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

TEST_P(PerAEADTest, InvalidNonceLength) {
  size_t valid_nonce_len = EVP_AEAD_nonce_length(aead());
  std::vector<size_t> nonce_lens;
  if (valid_nonce_len != 0) {
    // Other than the implicit IV TLS "AEAD"s, none of our AEADs allow empty
    // nonces. In particular, although AES-GCM was incorrectly specified with
    // variable-length nonces, it does not allow the empty nonce.
    nonce_lens.push_back(0);
  }
  if (!(GetParam().flags & kVariableNonce)) {
    nonce_lens.push_back(valid_nonce_len + 1);
    if (valid_nonce_len != 0) {
      nonce_lens.push_back(valid_nonce_len - 1);
    }
  }

  static const uint8_t kZeros[EVP_AEAD_MAX_KEY_LENGTH] = {0};
  const size_t ad_len = GetParam().required_ad_length.value_or(16);
  ASSERT_LE(ad_len, sizeof(kZeros));

  for (size_t nonce_len : nonce_lens) {
    SCOPED_TRACE(nonce_len);
    uint8_t buf[256];
    size_t len;
    std::vector<uint8_t> nonce(nonce_len);
    ScopedEVP_AEAD_CTX ctx;
    ASSERT_TRUE(EVP_AEAD_CTX_init_with_direction(
        ctx.get(), aead(), kZeros, EVP_AEAD_key_length(aead()),
        EVP_AEAD_DEFAULT_TAG_LENGTH, evp_aead_seal));

    EXPECT_FALSE(EVP_AEAD_CTX_seal(ctx.get(), buf, &len, sizeof(buf),
                                   nonce.data(), nonce.size(), nullptr /* in */,
                                   0, kZeros /* ad */, ad_len));
    uint32_t err = ERR_get_error();
    // TODO(davidben): Merge these errors. https://crbug.com/boringssl/129.
    if (!ErrorEquals(err, ERR_LIB_CIPHER, CIPHER_R_UNSUPPORTED_NONCE_SIZE)) {
      EXPECT_TRUE(
          ErrorEquals(err, ERR_LIB_CIPHER, CIPHER_R_INVALID_NONCE_SIZE));
    }

    ctx.Reset();
    ASSERT_TRUE(EVP_AEAD_CTX_init_with_direction(
        ctx.get(), aead(), kZeros, EVP_AEAD_key_length(aead()),
        EVP_AEAD_DEFAULT_TAG_LENGTH, evp_aead_open));
    EXPECT_FALSE(EVP_AEAD_CTX_open(ctx.get(), buf, &len, sizeof(buf),
                                   nonce.data(), nonce.size(), kZeros /* in */,
                                   sizeof(kZeros), kZeros /* ad */, ad_len));
    err = ERR_get_error();
    if (!ErrorEquals(err, ERR_LIB_CIPHER, CIPHER_R_UNSUPPORTED_NONCE_SIZE)) {
      EXPECT_TRUE(
          ErrorEquals(err, ERR_LIB_CIPHER, CIPHER_R_INVALID_NONCE_SIZE));
    }
  }
}

#if defined(SUPPORTS_ABI_TEST)
// CHECK_ABI can't pass enums, i.e. |evp_aead_seal| and |evp_aead_open|. Thus
// these two wrappers.
static int aead_ctx_init_for_seal(EVP_AEAD_CTX *ctx, const EVP_AEAD *aead,
                                  const uint8_t *key, size_t key_len) {
  return EVP_AEAD_CTX_init_with_direction(ctx, aead, key, key_len, 0,
                                          evp_aead_seal);
}

static int aead_ctx_init_for_open(EVP_AEAD_CTX *ctx, const EVP_AEAD *aead,
                                  const uint8_t *key, size_t key_len) {
  return EVP_AEAD_CTX_init_with_direction(ctx, aead, key, key_len, 0,
                                          evp_aead_open);
}

// CHECK_ABI can pass, at most, eight arguments. Thus these wrappers that
// figure out the output length from the input length, and take the nonce length
// from the configuration of the AEAD.
static int aead_ctx_seal(EVP_AEAD_CTX *ctx, uint8_t *out_ciphertext,
                         size_t *out_ciphertext_len, const uint8_t *nonce,
                         const uint8_t *plaintext, size_t plaintext_len,
                         const uint8_t *ad, size_t ad_len) {
  const size_t nonce_len = EVP_AEAD_nonce_length(EVP_AEAD_CTX_aead(ctx));
  return EVP_AEAD_CTX_seal(ctx, out_ciphertext, out_ciphertext_len,
                           plaintext_len + EVP_AEAD_MAX_OVERHEAD, nonce,
                           nonce_len, plaintext, plaintext_len, ad, ad_len);
}

static int aead_ctx_open(EVP_AEAD_CTX *ctx, uint8_t *out_plaintext,
                         size_t *out_plaintext_len, const uint8_t *nonce,
                         const uint8_t *ciphertext, size_t ciphertext_len,
                         const uint8_t *ad, size_t ad_len) {
  const size_t nonce_len = EVP_AEAD_nonce_length(EVP_AEAD_CTX_aead(ctx));
  return EVP_AEAD_CTX_open(ctx, out_plaintext, out_plaintext_len,
                           ciphertext_len, nonce, nonce_len, ciphertext,
                           ciphertext_len, ad, ad_len);
}

TEST_P(PerAEADTest, ABI) {
  uint8_t key[EVP_AEAD_MAX_KEY_LENGTH];
  OPENSSL_memset(key, 'K', sizeof(key));
  const size_t key_len = EVP_AEAD_key_length(aead());
  ASSERT_LE(key_len, sizeof(key));

  ScopedEVP_AEAD_CTX ctx_seal;
  ASSERT_TRUE(
      CHECK_ABI(aead_ctx_init_for_seal, ctx_seal.get(), aead(), key, key_len));

  ScopedEVP_AEAD_CTX ctx_open;
  ASSERT_TRUE(
      CHECK_ABI(aead_ctx_init_for_open, ctx_open.get(), aead(), key, key_len));

  alignas(2) uint8_t plaintext[512];
  OPENSSL_memset(plaintext, 'P', sizeof(plaintext));

  alignas(2) uint8_t ad_buf[512];
  OPENSSL_memset(ad_buf, 'A', sizeof(ad_buf));
  const uint8_t *const ad = ad_buf + 1;
  const size_t ad_len =
      GetParam().required_ad_length.value_or(sizeof(ad_buf) - 1);
  ASSERT_LE(ad_len, sizeof(ad_buf) - 1);

  uint8_t nonce[EVP_AEAD_MAX_NONCE_LENGTH];
  OPENSSL_memset(nonce, 'N', sizeof(nonce));
  const size_t nonce_len = EVP_AEAD_nonce_length(aead());
  ASSERT_LE(nonce_len, sizeof(nonce));

  alignas(2) uint8_t ciphertext[sizeof(plaintext) + EVP_AEAD_MAX_OVERHEAD + 1];
  size_t ciphertext_len;
  // Knock plaintext, ciphertext, and AD off alignment and give odd lengths for
  // plaintext and AD. This hopefully triggers any edge-cases in the assembly.
  ASSERT_TRUE(CHECK_ABI(aead_ctx_seal, ctx_seal.get(), ciphertext + 1,
                        &ciphertext_len, nonce, plaintext + 1,
                        sizeof(plaintext) - 1, ad, ad_len));

  alignas(2) uint8_t plaintext2[sizeof(ciphertext) + 1];
  size_t plaintext2_len;
  ASSERT_TRUE(CHECK_ABI(aead_ctx_open, ctx_open.get(), plaintext2 + 1,
                        &plaintext2_len, nonce, ciphertext + 1, ciphertext_len,
                        ad, ad_len));

  EXPECT_EQ(Bytes(plaintext + 1, sizeof(plaintext) - 1),
            Bytes(plaintext2 + 1, plaintext2_len));
}

TEST(ChaChaPoly1305Test, ABI) {
  if (!chacha20_poly1305_asm_capable()) {
    return;
  }

  auto buf = std::make_unique<uint8_t[]>(1024);
  for (size_t len = 0; len <= 1024; len += 5) {
    SCOPED_TRACE(len);
    union chacha20_poly1305_open_data open_ctx = {};
#if defined(OPENSSL_X86_64)
    CHECK_ABI(chacha20_poly1305_open_sse41, buf.get(), buf.get(), len,
              buf.get(), len % 128, &open_ctx);
    if (CRYPTO_is_AVX2_capable() && CRYPTO_is_BMI2_capable()) {
      CHECK_ABI(chacha20_poly1305_open_avx2, buf.get(), buf.get(), len,
                buf.get(), len % 128, &open_ctx);
    }
#else
    CHECK_ABI(chacha20_poly1305_open, buf.get(), buf.get(), len, buf.get(),
              len % 128, &open_ctx);
#endif
  }

  for (size_t len = 0; len <= 1024; len += 5) {
    SCOPED_TRACE(len);
    union chacha20_poly1305_seal_data seal_ctx = {};
#if defined(OPENSSL_X86_64)
    CHECK_ABI(chacha20_poly1305_seal_sse41, buf.get(), buf.get(), len,
              buf.get(), len % 128, &seal_ctx);
    if (CRYPTO_is_AVX2_capable() && CRYPTO_is_BMI2_capable()) {
      CHECK_ABI(chacha20_poly1305_seal_avx2, buf.get(), buf.get(), len,
                buf.get(), len % 128, &seal_ctx);
    }
#else
    CHECK_ABI(chacha20_poly1305_seal, buf.get(), buf.get(), len, buf.get(),
              len % 128, &seal_ctx);
#endif
  }
}
#endif  // SUPPORTS_ABI_TEST

TEST(AEADTest, AESCCMLargeAD) {
  static const std::vector<uint8_t> kKey(16, 'A');
  static const std::vector<uint8_t> kNonce(13, 'N');
  static const std::vector<uint8_t> kAD(65536, 'D');
  static const std::vector<uint8_t> kPlaintext = {
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
      0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
  static const std::vector<uint8_t> kCiphertext = {
      0xa2, 0x12, 0x3f, 0x0b, 0x07, 0xd5, 0x02, 0xff,
      0xa9, 0xcd, 0xa0, 0xf3, 0x69, 0x1c, 0x49, 0x0c};
  static const std::vector<uint8_t> kTag = {0x4a, 0x31, 0x82, 0x96};

  // Test AES-128-CCM-Bluetooth.
  ScopedEVP_AEAD_CTX ctx;
  ASSERT_TRUE(EVP_AEAD_CTX_init(ctx.get(), EVP_aead_aes_128_ccm_bluetooth(),
                                kKey.data(), kKey.size(),
                                EVP_AEAD_DEFAULT_TAG_LENGTH, nullptr));

  std::vector<uint8_t> out(kCiphertext.size() + kTag.size());
  size_t out_len;
  EXPECT_TRUE(EVP_AEAD_CTX_seal(ctx.get(), out.data(), &out_len, out.size(),
                                kNonce.data(), kNonce.size(), kPlaintext.data(),
                                kPlaintext.size(), kAD.data(), kAD.size()));

  ASSERT_EQ(out_len, kCiphertext.size() + kTag.size());
  EXPECT_EQ(Bytes(kCiphertext), Bytes(out.data(), kCiphertext.size()));
  EXPECT_EQ(Bytes(kTag), Bytes(out.data() + kCiphertext.size(), kTag.size()));

  EXPECT_TRUE(EVP_AEAD_CTX_open(ctx.get(), out.data(), &out_len, out.size(),
                                kNonce.data(), kNonce.size(), out.data(),
                                out.size(), kAD.data(), kAD.size()));

  ASSERT_EQ(out_len, kPlaintext.size());
  EXPECT_EQ(Bytes(kPlaintext), Bytes(out.data(), kPlaintext.size()));
}

static void RunWycheproofTestCase(FileTest *t, const EVP_AEAD *aead) {
  t->IgnoreInstruction("ivSize");

  std::vector<uint8_t> aad, ct, iv, key, msg, tag;
  ASSERT_TRUE(t->GetBytes(&aad, "aad"));
  ASSERT_TRUE(t->GetBytes(&ct, "ct"));
  ASSERT_TRUE(t->GetBytes(&iv, "iv"));
  ASSERT_TRUE(t->GetBytes(&key, "key"));
  ASSERT_TRUE(t->GetBytes(&msg, "msg"));
  ASSERT_TRUE(t->GetBytes(&tag, "tag"));
  std::string tag_size_str;
  ASSERT_TRUE(t->GetInstruction(&tag_size_str, "tagSize"));
  size_t tag_size = static_cast<size_t>(atoi(tag_size_str.c_str()));
  ASSERT_EQ(0u, tag_size % 8);
  tag_size /= 8;
  WycheproofResult result;
  ASSERT_TRUE(GetWycheproofResult(t, &result));

  std::vector<uint8_t> ct_and_tag = ct;
  ct_and_tag.insert(ct_and_tag.end(), tag.begin(), tag.end());

  ScopedEVP_AEAD_CTX ctx;
  ASSERT_TRUE(EVP_AEAD_CTX_init(ctx.get(), aead, key.data(), key.size(),
                                tag_size, nullptr));
  std::vector<uint8_t> out(msg.size());
  size_t out_len;
  // Wycheproof tags small AES-GCM IVs as "acceptable" and otherwise does not
  // use it in AEADs. Any AES-GCM IV that isn't 96 bits is absurd, but our API
  // supports those, so we treat SmallIv tests as valid.
  if (result.IsValid({"SmallIv"})) {
    // Decryption should succeed.
    ASSERT_TRUE(EVP_AEAD_CTX_open(ctx.get(), out.data(), &out_len, out.size(),
                                  iv.data(), iv.size(), ct_and_tag.data(),
                                  ct_and_tag.size(), aad.data(), aad.size()));
    EXPECT_EQ(Bytes(msg), Bytes(out.data(), out_len));

    // Decryption in-place should succeed.
    out = ct_and_tag;
    ASSERT_TRUE(EVP_AEAD_CTX_open(ctx.get(), out.data(), &out_len, out.size(),
                                  iv.data(), iv.size(), out.data(), out.size(),
                                  aad.data(), aad.size()));
    EXPECT_EQ(Bytes(msg), Bytes(out.data(), out_len));

    // AEADs are deterministic, so encryption should produce the same result.
    out.resize(ct_and_tag.size());
    ASSERT_TRUE(EVP_AEAD_CTX_seal(ctx.get(), out.data(), &out_len, out.size(),
                                  iv.data(), iv.size(), msg.data(), msg.size(),
                                  aad.data(), aad.size()));
    EXPECT_EQ(Bytes(ct_and_tag), Bytes(out.data(), out_len));

    // Encrypt in-place.
    out = msg;
    out.resize(ct_and_tag.size());
    ASSERT_TRUE(EVP_AEAD_CTX_seal(ctx.get(), out.data(), &out_len, out.size(),
                                  iv.data(), iv.size(), out.data(), msg.size(),
                                  aad.data(), aad.size()));
    EXPECT_EQ(Bytes(ct_and_tag), Bytes(out.data(), out_len));
  } else {
    // Decryption should fail.
    EXPECT_FALSE(EVP_AEAD_CTX_open(ctx.get(), out.data(), &out_len, out.size(),
                                   iv.data(), iv.size(), ct_and_tag.data(),
                                   ct_and_tag.size(), aad.data(), aad.size()));

    // Decryption in-place should also fail.
    out = ct_and_tag;
    EXPECT_FALSE(EVP_AEAD_CTX_open(ctx.get(), out.data(), &out_len, out.size(),
                                   iv.data(), iv.size(), out.data(), out.size(),
                                   aad.data(), aad.size()));
  }
}

TEST(AEADTest, WycheproofAESGCMSIV) {
  FileTestGTest("third_party/wycheproof_testvectors/aes_gcm_siv_test.txt",
                [](FileTest *t) {
                  std::string key_size_str;
                  ASSERT_TRUE(t->GetInstruction(&key_size_str, "keySize"));
                  const EVP_AEAD *aead;
                  switch (atoi(key_size_str.c_str())) {
                    case 128:
                      aead = EVP_aead_aes_128_gcm_siv();
                      break;
                    case 256:
                      aead = EVP_aead_aes_256_gcm_siv();
                      break;
                    default:
                      FAIL() << "Unknown key size: " << key_size_str;
                  }

                  RunWycheproofTestCase(t, aead);
                });
}

TEST(AEADTest, WycheproofAESGCM) {
  FileTestGTest("third_party/wycheproof_testvectors/aes_gcm_test.txt",
                [](FileTest *t) {
                  std::string key_size_str;
                  ASSERT_TRUE(t->GetInstruction(&key_size_str, "keySize"));
                  const EVP_AEAD *aead;
                  switch (atoi(key_size_str.c_str())) {
                    case 128:
                      aead = EVP_aead_aes_128_gcm();
                      break;
                    case 192:
                      aead = EVP_aead_aes_192_gcm();
                      break;
                    case 256:
                      aead = EVP_aead_aes_256_gcm();
                      break;
                    default:
                      FAIL() << "Unknown key size: " << key_size_str;
                  }

                  RunWycheproofTestCase(t, aead);
                });
}

TEST(AEADTest, WycheproofChaCha20Poly1305) {
  FileTestGTest("third_party/wycheproof_testvectors/chacha20_poly1305_test.txt",
                [](FileTest *t) {
                  t->IgnoreInstruction("keySize");
                  RunWycheproofTestCase(t, EVP_aead_chacha20_poly1305());
                });
}

TEST(AEADTest, WycheproofXChaCha20Poly1305) {
  FileTestGTest(
      "third_party/wycheproof_testvectors/xchacha20_poly1305_test.txt",
      [](FileTest *t) {
        t->IgnoreInstruction("keySize");
        RunWycheproofTestCase(t, EVP_aead_xchacha20_poly1305());
      });
}

TEST(AEADTest, WycheproofAESEAX) {
  FileTestGTest(
      "third_party/wycheproof_testvectors/aes_eax_test.txt", [](FileTest *t) {
        std::string key_size_str;
        ASSERT_TRUE(t->GetInstruction(&key_size_str, "keySize"));
        const EVP_AEAD *aead;
        switch (atoi(key_size_str.c_str())) {
          case 128:
            aead = EVP_aead_aes_128_eax();
            break;
          case 256:
            aead = EVP_aead_aes_256_eax();
            break;
          default:
            t->SkipCurrent();
            GTEST_SKIP() << "Unsupported key size: " << key_size_str;
        }

        std::string nonce_size_str;
        ASSERT_TRUE(t->GetInstruction(&nonce_size_str, "ivSize"));
        // Skip tests with invalid nonce size.
        if (nonce_size_str != "96" && nonce_size_str != "128") {
          t->SkipCurrent();
          GTEST_SKIP() << "Unsupported nonce size: " << nonce_size_str;
        }

        RunWycheproofTestCase(t, aead);
      });
}

TEST(AEADTest, FreeNull) { EVP_AEAD_CTX_free(nullptr); }

TEST(AEADTest, ForEachBlockRange) {
  auto ebg13 = [](const uint8_t *in, uint8_t *out, size_t len) {
    while (len > 0) {
      if ((*in >= 'A' && *in <= 'M') || (*in >= 'a' && *in <= 'm')) {
        *out = *in + 13;
      } else if ((*in >= 'N' && *in <= 'Z') || (*in >= 'n' && *in <= 'z')) {
        *out = *in - 13;
      } else {
        *out = *in;
      }
      --len;
      ++in;
      ++out;
    }
  };

  for (const std::string in_str : {
           "",
           "A",
           "Abcdefghijklmno",
           "Abcdefghijklmnop",
           "AbcdefghijklmnopA",
           "AbcdefghijklmnopAbcdefghijklmnop",
           "BRLOGENSHFEGLE doesn't lol. The quick brown fox jumped over the "
           "lazy sleeping dog's back then sat on a brute-force attack.",
       }) {
    SCOPED_TRACE(in_str);

    Span<const uint8_t> in_span = StringAsBytes(in_str);
    std::vector<uint8_t> want(in_str.size(), 'X');
    ebg13(in_span.data(), want.data(), in_span.size());
    std::string_view want_str = BytesAsStringView(want);

    for (const auto &splits :
         InterestingSplitsForLength(in_span.size(), /*block_size=*/16)) {
      SCOPED_TRACE(FormatSplits(splits));
      TestIOVecs iovecs =
          TestIOVecs::Split(in_span, splits, /*in_place=*/false);
      int final_calls = 0;
      size_t bytes_processed = 0;
      bssl::iovec::ForEachBlockRange<16, /*WriteOut=*/true>(
          iovecs.iovecs(),
          [&](const uint8_t *in, uint8_t *out, size_t len) {
            EXPECT_GE(len, size_t{1});
            EXPECT_EQ(len % size_t{16}, size_t{0});
            ebg13(in, out, len);
            bytes_processed += len;
            return true;
          },
          [&](const uint8_t *in, uint8_t *out, size_t len) {
            if (in_span.size() != 0) {
              EXPECT_GE(len, size_t{1});
            }
            ebg13(in, out, len);
            ++final_calls;
            bytes_processed += len;
            return true;
          });
      EXPECT_EQ(final_calls, 1);
      EXPECT_EQ(bytes_processed, in_str.size());
      auto output = iovecs.Output();
      std::string output_str(reinterpret_cast<char *>(output.data()),
                             output.size());
      EXPECT_EQ(output_str, want_str);
    }
  }
}

}  // namespace
BSSL_NAMESPACE_END
