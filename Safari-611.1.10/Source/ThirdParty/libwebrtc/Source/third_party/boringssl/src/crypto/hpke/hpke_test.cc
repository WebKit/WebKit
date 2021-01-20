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

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <openssl/base.h>
#include <openssl/curve25519.h>
#include <openssl/digest.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/span.h>

#include "../test/file_test.h"
#include "../test/test_util.h"
#include "internal.h"


namespace bssl {
namespace {

enum class HPKEMode {
  kBase = 0,
  kPSK = 1,
};

// HPKETestVector corresponds to one array member in the published
// test-vectors.json.
class HPKETestVector {
 public:
  explicit HPKETestVector() = default;
  ~HPKETestVector() = default;

  bool ReadFromFileTest(FileTest *t);

  void Verify() const {
    ScopedEVP_HPKE_CTX sender_ctx;
    ScopedEVP_HPKE_CTX receiver_ctx;

    switch (mode_) {
      case HPKEMode::kBase:
        ASSERT_GT(secret_key_e_.size(), 0u);
        ASSERT_EQ(psk_.size(), 0u);
        ASSERT_EQ(psk_id_.size(), 0u);

        // Set up the sender.
        ASSERT_TRUE(EVP_HPKE_CTX_setup_base_s_x25519_for_test(
            sender_ctx.get(), kdf_id_, aead_id_, public_key_r_.data(),
            info_.data(), info_.size(), secret_key_e_.data(),
            public_key_e_.data()));

        // Set up the receiver.
        ASSERT_TRUE(EVP_HPKE_CTX_setup_base_r_x25519(
            receiver_ctx.get(), kdf_id_, aead_id_, public_key_e_.data(),
            public_key_r_.data(), secret_key_r_.data(), info_.data(),
            info_.size()));
        break;

      case HPKEMode::kPSK:
        ASSERT_GT(secret_key_e_.size(), 0u);
        ASSERT_GT(psk_.size(), 0u);
        ASSERT_GT(psk_id_.size(), 0u);

        // Set up the sender.
        ASSERT_TRUE(EVP_HPKE_CTX_setup_psk_s_x25519_for_test(
            sender_ctx.get(), kdf_id_, aead_id_, public_key_r_.data(),
            info_.data(), info_.size(), psk_.data(), psk_.size(),
            psk_id_.data(), psk_id_.size(), secret_key_e_.data(),
            public_key_e_.data()));

        // Set up the receiver.
        ASSERT_TRUE(EVP_HPKE_CTX_setup_psk_r_x25519(
            receiver_ctx.get(), kdf_id_, aead_id_, public_key_e_.data(),
            public_key_r_.data(), secret_key_r_.data(), info_.data(),
            info_.size(), psk_.data(), psk_.size(), psk_id_.data(),
            psk_id_.size()));
        break;
      default:
        FAIL() << "Unsupported mode";
        return;
    }

    VerifyEncryptions(sender_ctx.get(), receiver_ctx.get());
    VerifyExports(sender_ctx.get());
    VerifyExports(receiver_ctx.get());
  }

 private:
  void VerifyEncryptions(EVP_HPKE_CTX *sender_ctx,
                         EVP_HPKE_CTX *receiver_ctx) const {
    for (const Encryption &task : encryptions_) {
      std::vector<uint8_t> encrypted(task.plaintext.size() +
                                     EVP_HPKE_CTX_max_overhead(sender_ctx));
      size_t encrypted_len;
      ASSERT_TRUE(EVP_HPKE_CTX_seal(
          sender_ctx, encrypted.data(), &encrypted_len, encrypted.size(),
          task.plaintext.data(), task.plaintext.size(), task.aad.data(),
          task.aad.size()));

      ASSERT_EQ(Bytes(encrypted.data(), encrypted_len), Bytes(task.ciphertext));

      std::vector<uint8_t> decrypted(task.ciphertext.size());
      size_t decrypted_len;
      ASSERT_TRUE(EVP_HPKE_CTX_open(
          receiver_ctx, decrypted.data(), &decrypted_len, decrypted.size(),
          task.ciphertext.data(), task.ciphertext.size(), task.aad.data(),
          task.aad.size()));

      ASSERT_EQ(Bytes(decrypted.data(), decrypted_len), Bytes(task.plaintext));
    }
  }

  void VerifyExports(EVP_HPKE_CTX *ctx) const {
    for (const Export &task : exports_) {
      std::vector<uint8_t> exported_secret(task.exportLength);

      ASSERT_TRUE(EVP_HPKE_CTX_export(
          ctx, exported_secret.data(), exported_secret.size(),
          task.exportContext.data(), task.exportContext.size()));
      ASSERT_EQ(Bytes(exported_secret), Bytes(task.exportValue));
    }
  }

  struct Encryption {
    std::vector<uint8_t> aad;
    std::vector<uint8_t> ciphertext;
    std::vector<uint8_t> plaintext;
  };

  struct Export {
    std::vector<uint8_t> exportContext;
    size_t exportLength;
    std::vector<uint8_t> exportValue;
  };

  HPKEMode mode_;
  uint16_t kdf_id_;
  uint16_t aead_id_;
  std::vector<uint8_t> context_;
  std::vector<uint8_t> info_;
  std::vector<uint8_t> public_key_e_;
  std::vector<uint8_t> secret_key_e_;
  std::vector<uint8_t> public_key_r_;
  std::vector<uint8_t> secret_key_r_;
  std::vector<Encryption> encryptions_;
  std::vector<Export> exports_;
  std::vector<uint8_t> psk_;     // Empty when mode is not PSK.
  std::vector<uint8_t> psk_id_;  // Empty when mode is not PSK.
};

// Match FileTest's naming scheme for duplicated attribute names.
std::string BuildAttrName(const std::string &name, int iter) {
  return iter == 1 ? name : name + "/" + std::to_string(iter);
}

// Parses |s| as an unsigned integer of type T and writes the value to |out|.
// Returns true on success. If the integer value exceeds the maximum T value,
// returns false.
template <typename T>
bool ParseIntSafe(T *out, const std::string &s) {
  T value = 0;
  for (char c : s) {
    if (c < '0' || c > '9') {
      return false;
    }
    if (value > (std::numeric_limits<T>::max() - (c - '0')) / 10) {
      return false;
    }
    value = 10 * value + (c - '0');
  }
  *out = value;
  return true;
}

// Read the |key| attribute from |file_test| and convert it to an integer.
template <typename T>
bool FileTestReadInt(FileTest *file_test, T *out, const std::string &key) {
  std::string s;
  return file_test->GetAttribute(&s, key) && ParseIntSafe(out, s);
}


bool HPKETestVector::ReadFromFileTest(FileTest *t) {
  uint8_t mode_tmp;
  if (!FileTestReadInt(t, &mode_tmp, "mode")) {
    return false;
  }
  mode_ = static_cast<HPKEMode>(mode_tmp);

  if (!FileTestReadInt(t, &kdf_id_, "kdf_id") ||
      !FileTestReadInt(t, &aead_id_, "aead_id") ||
      !t->GetBytes(&info_, "info") ||
      !t->GetBytes(&secret_key_r_, "skRm") ||
      !t->GetBytes(&public_key_r_, "pkRm") ||
      !t->GetBytes(&secret_key_e_, "skEm") ||
      !t->GetBytes(&public_key_e_, "pkEm")) {
    return false;
  }

  if (mode_ == HPKEMode::kPSK) {
    if (!t->GetBytes(&psk_, "psk") ||
        !t->GetBytes(&psk_id_, "psk_id")) {
      return false;
    }
  }

  for (int i = 1; t->HasAttribute(BuildAttrName("aad", i)); i++) {
    Encryption encryption;
    if (!t->GetBytes(&encryption.aad, BuildAttrName("aad", i)) ||
        !t->GetBytes(&encryption.ciphertext, BuildAttrName("ciphertext", i)) ||
        !t->GetBytes(&encryption.plaintext, BuildAttrName("plaintext", i))) {
      return false;
    }
    encryptions_.push_back(std::move(encryption));
  }

  for (int i = 1; t->HasAttribute(BuildAttrName("exportContext", i)); i++) {
    Export exp;
    if (!t->GetBytes(&exp.exportContext, BuildAttrName("exportContext", i)) ||
        !FileTestReadInt(t, &exp.exportLength,
                         BuildAttrName("exportLength", i)) ||
        !t->GetBytes(&exp.exportValue, BuildAttrName("exportValue", i))) {
      return false;
    }
    exports_.push_back(std::move(exp));
  }
  return true;
}

}  // namespace

TEST(HPKETest, VerifyTestVectors) {
  FileTestGTest("crypto/hpke/hpke_test_vectors.txt", [](FileTest *t) {
    HPKETestVector test_vec;
    EXPECT_TRUE(test_vec.ReadFromFileTest(t));
    test_vec.Verify();
  });
}

// The test vectors used fixed sender ephemeral keys, while HPKE itself
// generates new keys for each context. Test this codepath by checking we can
// decrypt our own messages.
TEST(HPKETest, RoundTrip) {
  uint16_t kdf_ids[] = {EVP_HPKE_HKDF_SHA256, EVP_HPKE_HKDF_SHA384,
                        EVP_HPKE_HKDF_SHA512};
  uint16_t aead_ids[] = {EVP_HPKE_AEAD_AES_GCM_128, EVP_HPKE_AEAD_AES_GCM_256,
                         EVP_HPKE_AEAD_CHACHA20POLY1305};

  const uint8_t info_a[] = {1, 1, 2, 3, 5, 8};
  const uint8_t info_b[] = {42, 42, 42};
  const uint8_t ad_a[] = {1, 2, 4, 8, 16};
  const uint8_t ad_b[] = {7};
  Span<const uint8_t> info_values[] = {{nullptr, 0}, info_a, info_b};
  Span<const uint8_t> ad_values[] = {{nullptr, 0}, ad_a, ad_b};

  // Generate the receiver's keypair.
  uint8_t secret_key_r[X25519_PRIVATE_KEY_LEN];
  uint8_t public_key_r[X25519_PUBLIC_VALUE_LEN];
  X25519_keypair(public_key_r, secret_key_r);

  for (uint16_t kdf_id : kdf_ids) {
    for (uint16_t aead_id : aead_ids) {
      for (const Span<const uint8_t> &info : info_values) {
        for (const Span<const uint8_t> &ad : ad_values) {
          // Set up the sender.
          ScopedEVP_HPKE_CTX sender_ctx;
          uint8_t enc[X25519_PUBLIC_VALUE_LEN];
          ASSERT_TRUE(EVP_HPKE_CTX_setup_base_s_x25519(
              sender_ctx.get(), enc, kdf_id, aead_id, public_key_r, info.data(),
              info.size()));

          // Set up the receiver.
          ScopedEVP_HPKE_CTX receiver_ctx;
          ASSERT_TRUE(EVP_HPKE_CTX_setup_base_r_x25519(
              receiver_ctx.get(), kdf_id, aead_id, enc, public_key_r,
              secret_key_r, info.data(), info.size()));

          const char kCleartextPayload[] = "foobar";

          // Have sender encrypt message for the receiver.
          std::vector<uint8_t> ciphertext(
              sizeof(kCleartextPayload) +
              EVP_HPKE_CTX_max_overhead(sender_ctx.get()));
          size_t ciphertext_len;
          ASSERT_TRUE(EVP_HPKE_CTX_seal(
              sender_ctx.get(), ciphertext.data(), &ciphertext_len,
              ciphertext.size(),
              reinterpret_cast<const uint8_t *>(kCleartextPayload),
              sizeof(kCleartextPayload), ad.data(), ad.size()));

          // Have receiver decrypt the message.
          std::vector<uint8_t> cleartext(ciphertext.size());
          size_t cleartext_len;
          ASSERT_TRUE(EVP_HPKE_CTX_open(receiver_ctx.get(), cleartext.data(),
                                        &cleartext_len, cleartext.size(),
                                        ciphertext.data(), ciphertext_len,
                                        ad.data(), ad.size()));

          // Verify that decrypted message matches the original.
          ASSERT_EQ(Bytes(cleartext.data(), cleartext_len),
                    Bytes(kCleartextPayload, sizeof(kCleartextPayload)));
        }
      }
    }
  }
}

// Verify that the DH operations inside Encap() and Decap() both fail when the
// public key is on a small-order point in the curve.
TEST(HPKETest, X25519EncapSmallOrderPoint) {
  // Borrowed from X25519Test.SmallOrder.
  static const uint8_t kSmallOrderPoint[32] = {
      0xe0, 0xeb, 0x7a, 0x7c, 0x3b, 0x41, 0xb8, 0xae, 0x16, 0x56, 0xe3,
      0xfa, 0xf1, 0x9f, 0xc4, 0x6a, 0xda, 0x09, 0x8d, 0xeb, 0x9c, 0x32,
      0xb1, 0xfd, 0x86, 0x62, 0x05, 0x16, 0x5f, 0x49, 0xb8,
  };

  // Generate a valid keypair for the receiver.
  uint8_t secret_key_r[X25519_PRIVATE_KEY_LEN];
  uint8_t public_key_r[X25519_PUBLIC_VALUE_LEN];
  X25519_keypair(public_key_r, secret_key_r);

  uint16_t kdf_ids[] = {EVP_HPKE_HKDF_SHA256, EVP_HPKE_HKDF_SHA384,
                        EVP_HPKE_HKDF_SHA512};
  uint16_t aead_ids[] = {EVP_HPKE_AEAD_AES_GCM_128, EVP_HPKE_AEAD_AES_GCM_256,
                         EVP_HPKE_AEAD_CHACHA20POLY1305};

  for (uint16_t kdf_id : kdf_ids) {
    for (uint16_t aead_id : aead_ids) {
      // Set up the sender, passing in kSmallOrderPoint as |peer_public_value|.
      ScopedEVP_HPKE_CTX sender_ctx;
      uint8_t enc[X25519_PUBLIC_VALUE_LEN];
      ASSERT_FALSE(EVP_HPKE_CTX_setup_base_s_x25519(
          sender_ctx.get(), enc, kdf_id, aead_id, kSmallOrderPoint, nullptr,
          0));

      // Set up the receiver, passing in kSmallOrderPoint as |enc|.
      ScopedEVP_HPKE_CTX receiver_ctx;
      ASSERT_FALSE(EVP_HPKE_CTX_setup_base_r_x25519(
          receiver_ctx.get(), kdf_id, aead_id, kSmallOrderPoint, public_key_r,
          secret_key_r, nullptr, 0));
    }
  }
}

// Test that Seal() fails when the context has been initialized as a receiver.
TEST(HPKETest, ReceiverInvalidSeal) {
  const uint8_t kMockEnc[X25519_PUBLIC_VALUE_LEN] = {0xff};
  const char kCleartextPayload[] = "foobar";

  // Generate the receiver's keypair.
  uint8_t secret_key_r[X25519_PRIVATE_KEY_LEN];
  uint8_t public_key_r[X25519_PUBLIC_VALUE_LEN];
  X25519_keypair(public_key_r, secret_key_r);

  // Set up the receiver.
  ScopedEVP_HPKE_CTX receiver_ctx;
  ASSERT_TRUE(EVP_HPKE_CTX_setup_base_r_x25519(
      receiver_ctx.get(), EVP_HPKE_HKDF_SHA256, EVP_HPKE_AEAD_AES_GCM_128,
      kMockEnc, public_key_r, secret_key_r, nullptr, 0));

  // Call Seal() on the receiver.
  size_t ciphertext_len;
  uint8_t ciphertext[100];
  ASSERT_FALSE(EVP_HPKE_CTX_seal(
      receiver_ctx.get(), ciphertext, &ciphertext_len, sizeof(ciphertext),
      reinterpret_cast<const uint8_t *>(kCleartextPayload),
      sizeof(kCleartextPayload), nullptr, 0));
}

// Test that Open() fails when the context has been initialized as a sender.
TEST(HPKETest, SenderInvalidOpen) {
  const uint8_t kMockCiphertext[100] = {0xff};
  const size_t kMockCiphertextLen = 80;

  // Generate the receiver's keypair.
  uint8_t secret_key_r[X25519_PRIVATE_KEY_LEN];
  uint8_t public_key_r[X25519_PUBLIC_VALUE_LEN];
  X25519_keypair(public_key_r, secret_key_r);

  // Set up the sender.
  ScopedEVP_HPKE_CTX sender_ctx;
  uint8_t enc[X25519_PUBLIC_VALUE_LEN];
  ASSERT_TRUE(EVP_HPKE_CTX_setup_base_s_x25519(
      sender_ctx.get(), enc, EVP_HPKE_HKDF_SHA256, EVP_HPKE_AEAD_AES_GCM_128,
      public_key_r, nullptr, 0));

  // Call Open() on the sender.
  uint8_t cleartext[128];
  size_t cleartext_len;
  ASSERT_FALSE(EVP_HPKE_CTX_open(sender_ctx.get(), cleartext, &cleartext_len,
                                 sizeof(cleartext), kMockCiphertext,
                                 kMockCiphertextLen, nullptr, 0));
}

// Test that the PSK variants of Setup functions fail when any of the PSK inputs
// are empty.
TEST(HPKETest, EmptyPSK) {
  const uint8_t kMockEnc[X25519_PUBLIC_VALUE_LEN] = {0xff};
  const uint8_t kMockPSK[100] = {0xff};
  const bssl::Span<const uint8_t> kPSKValues[] = {
      {kMockPSK, sizeof(kMockPSK)},
      {nullptr, 0},
  };

  // Generate the receiver's keypair.
  uint8_t secret_key_r[X25519_PRIVATE_KEY_LEN];
  uint8_t public_key_r[X25519_PUBLIC_VALUE_LEN];
  X25519_keypair(public_key_r, secret_key_r);

  // Vary the PSK and PSKID inputs for the sender and receiver, trying all four
  // permutations of empty and nonempty inputs.

  for (const auto psk : kPSKValues) {
    for (const auto psk_id : kPSKValues) {
      const bool kExpectSuccess = psk.size() > 0 && psk_id.size() > 0;

      ASSERT_EQ(ERR_get_error(), 0u);

      ScopedEVP_HPKE_CTX sender_ctx;
      uint8_t enc[X25519_PUBLIC_VALUE_LEN];
      ASSERT_EQ(EVP_HPKE_CTX_setup_psk_s_x25519(
                    sender_ctx.get(), enc, EVP_HPKE_HKDF_SHA256,
                    EVP_HPKE_AEAD_AES_GCM_128, public_key_r, nullptr, 0,
                    psk.data(), psk.size(), psk_id.data(), psk_id.size()),
                kExpectSuccess);

      if (!kExpectSuccess) {
        uint32_t err = ERR_get_error();
        EXPECT_EQ(ERR_LIB_EVP, ERR_GET_LIB(err));
        EXPECT_EQ(EVP_R_EMPTY_PSK, ERR_GET_REASON(err));
      }
      ERR_clear_error();

      ScopedEVP_HPKE_CTX receiver_ctx;
      ASSERT_EQ(
          EVP_HPKE_CTX_setup_psk_r_x25519(
              receiver_ctx.get(), EVP_HPKE_HKDF_SHA256,
              EVP_HPKE_AEAD_AES_GCM_128, kMockEnc, public_key_r, secret_key_r,
              nullptr, 0, psk.data(), psk.size(), psk_id.data(), psk_id.size()),
          kExpectSuccess);

      if (!kExpectSuccess) {
        uint32_t err = ERR_get_error();
        EXPECT_EQ(ERR_LIB_EVP, ERR_GET_LIB(err));
        EXPECT_EQ(EVP_R_EMPTY_PSK, ERR_GET_REASON(err));
      }
      ERR_clear_error();
    }
  }
}

TEST(HPKETest, InternalParseIntSafe) {
  uint8_t u8 = 0xff;
  ASSERT_FALSE(ParseIntSafe(&u8, "-1"));

  ASSERT_TRUE(ParseIntSafe(&u8, "0"));
  ASSERT_EQ(u8, 0);

  ASSERT_TRUE(ParseIntSafe(&u8, "255"));
  ASSERT_EQ(u8, 255);

  ASSERT_FALSE(ParseIntSafe(&u8, "256"));

  uint16_t u16 = 0xffff;
  ASSERT_TRUE(ParseIntSafe(&u16, "257"));
  ASSERT_EQ(u16, 257);

  ASSERT_TRUE(ParseIntSafe(&u16, "65535"));
  ASSERT_EQ(u16, 65535);

  ASSERT_FALSE(ParseIntSafe(&u16, "65536"));
}


}  // namespace bssl
