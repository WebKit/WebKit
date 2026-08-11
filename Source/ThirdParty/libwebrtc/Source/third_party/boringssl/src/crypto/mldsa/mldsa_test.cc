// Copyright 2024 The BoringSSL Authors
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

#include <openssl/mldsa.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <openssl/bytestring.h>
#include <openssl/mem.h>
#include <openssl/span.h>

#include "../bytestring/internal.h"
#include "../fipsmodule/bcm_interface.h"
#include "../internal.h"
#include "../test/file_test.h"
#include "../test/test_util.h"
#include "../test/wycheproof_util.h"


BSSL_NAMESPACE_BEGIN
namespace {

// Arguments are templated to avoid needing to repeat the underlying function's
// type signature. We cannot use p_mldsa.cc's pattern because, on Windows,
// cross-dll function pointers are not constexpr.
#define TRAIT_METHOD(method_name, function_name)       \
  template <typename... Args>                          \
  static auto method_name(Args... args) {              \
    return function_name(std::forward<Args>(args)...); \
  }

#define MAKE_MLDSA_TRAITS(kl)                                               \
  struct MLDSA##kl##Traits {                                                \
    using PublicKey = MLDSA##kl##_public_key;                               \
    using PrivateKey = MLDSA##kl##_private_key;                             \
    using Prehash = MLDSA##kl##_prehash;                                    \
                                                                            \
    static constexpr size_t kPublicKeyBytes = MLDSA##kl##_PUBLIC_KEY_BYTES; \
    static constexpr size_t kSignatureBytes = MLDSA##kl##_SIGNATURE_BYTES;  \
    TRAIT_METHOD(PrivateKeyFromSeed, MLDSA##kl##_private_key_from_seed)     \
    TRAIT_METHOD(ParsePrivateKey, BCM_mldsa##kl##_parse_private_key)        \
    TRAIT_METHOD(MarshalPrivateKey, BCM_mldsa##kl##_marshal_private_key)    \
    TRAIT_METHOD(GenerateKey, MLDSA##kl##_generate_key)                     \
    TRAIT_METHOD(GenerateKeyExternalEntropy,                                \
                 BCM_mldsa##kl##_generate_key_external_entropy)             \
    TRAIT_METHOD(Sign, MLDSA##kl##_sign)                                    \
    TRAIT_METHOD(SignInternal, BCM_mldsa##kl##_sign_internal)               \
    TRAIT_METHOD(SignMuInternal, BCM_mldsa##kl##_sign_mu_internal)          \
                                                                            \
    TRAIT_METHOD(PublicFromPrivate, MLDSA##kl##_public_from_private)        \
    TRAIT_METHOD(ParsePublicKey, MLDSA##kl##_parse_public_key)              \
    TRAIT_METHOD(MarshalPublicKey, MLDSA##kl##_marshal_public_key)          \
    TRAIT_METHOD(Verify, MLDSA##kl##_verify)                                \
    TRAIT_METHOD(VerifyInternal, BCM_mldsa##kl##_verify_internal)           \
                                                                            \
    TRAIT_METHOD(PrehashInit, MLDSA##kl##_prehash_init)                     \
    TRAIT_METHOD(PrehashUpdate, MLDSA##kl##_prehash_update)                 \
    TRAIT_METHOD(PrehashFinalize, MLDSA##kl##_prehash_finalize)             \
  };

MAKE_MLDSA_TRAITS(44)
MAKE_MLDSA_TRAITS(65)
MAKE_MLDSA_TRAITS(87)

template <typename T>
std::vector<uint8_t> Marshal(bcm_status (*marshal_func)(CBB *, const T *),
                             const T *t) {
  ScopedCBB cbb;
  uint8_t *encoded;
  size_t encoded_len;
  if (!CBB_init(cbb.get(), 1) ||                             //
      marshal_func(cbb.get(), t) != bcm_status::approved ||  //
      !CBB_finish(cbb.get(), &encoded, &encoded_len)) {
    abort();
  }

  std::vector<uint8_t> ret(encoded, encoded + encoded_len);
  OPENSSL_free(encoded);
  return ret;
}

// This test is very slow, so it is disabled by default.
TEST(MLDSATest, DISABLED_BitFlips) {
  std::vector<uint8_t> encoded_public_key(MLDSA65_PUBLIC_KEY_BYTES);
  auto priv = std::make_unique<MLDSA65_private_key>();
  uint8_t seed[MLDSA_SEED_BYTES];
  EXPECT_TRUE(
      MLDSA65_generate_key(encoded_public_key.data(), seed, priv.get()));

  std::vector<uint8_t> encoded_signature(MLDSA65_SIGNATURE_BYTES);
  static const uint8_t kMessage[] = {'H', 'e', 'l', 'l', 'o', ' ',
                                     'w', 'o', 'r', 'l', 'd'};
  EXPECT_TRUE(MLDSA65_sign(encoded_signature.data(), priv.get(), kMessage,
                           sizeof(kMessage), nullptr, 0));

  auto pub = std::make_unique<MLDSA65_public_key>();
  CBS cbs = CBS(encoded_public_key);
  ASSERT_TRUE(MLDSA65_parse_public_key(pub.get(), &cbs));

  EXPECT_EQ(MLDSA65_verify(pub.get(), encoded_signature.data(),
                           encoded_signature.size(), kMessage, sizeof(kMessage),
                           nullptr, 0),
            1);

  for (size_t i = 0; i < MLDSA65_SIGNATURE_BYTES; i++) {
    for (int j = 0; j < 8; j++) {
      encoded_signature[i] ^= 1 << j;
      EXPECT_EQ(MLDSA65_verify(pub.get(), encoded_signature.data(),
                               encoded_signature.size(), kMessage,
                               sizeof(kMessage), nullptr, 0),
                0)
          << "Bit flip in signature at byte " << i << " bit " << j
          << " didn't cause a verification failure";
      encoded_signature[i] ^= 1 << j;
    }
  }
}

template <typename Traits>
void MLDSABasicTest() {
  std::vector<uint8_t> encoded_public_key(Traits::kPublicKeyBytes);
  auto priv = std::make_unique<typename Traits::PrivateKey>();
  uint8_t seed[MLDSA_SEED_BYTES];
  EXPECT_TRUE(Traits::GenerateKey(encoded_public_key.data(), seed, priv.get()));

  const std::vector<uint8_t> encoded_private_key =
      Marshal(Traits::MarshalPrivateKey, priv.get());
  CBS cbs = CBS(encoded_private_key);
  EXPECT_TRUE(bcm_success(Traits::ParsePrivateKey(priv.get(), &cbs)));

  std::vector<uint8_t> encoded_signature(Traits::kSignatureBytes);
  static const uint8_t kMessage[] = {'H', 'e', 'l', 'l', 'o', ' ',
                                     'w', 'o', 'r', 'l', 'd'};
  static const uint8_t kContext[] = {'c', 't', 'x'};
  EXPECT_TRUE(Traits::Sign(encoded_signature.data(), priv.get(), kMessage,
                           sizeof(kMessage), kContext, sizeof(kContext)));

  auto pub = std::make_unique<typename Traits::PublicKey>();
  cbs = CBS(encoded_public_key);
  ASSERT_TRUE(Traits::ParsePublicKey(pub.get(), &cbs));

  EXPECT_EQ(Traits::Verify(pub.get(), encoded_signature.data(),
                           encoded_signature.size(), kMessage, sizeof(kMessage),
                           kContext, sizeof(kContext)),
            1);

  auto priv2 = std::make_unique<typename Traits::PrivateKey>();
  EXPECT_TRUE(Traits::PrivateKeyFromSeed(priv2.get(), seed, sizeof(seed)));

  EXPECT_EQ(
      Bytes(Declassified(Marshal(Traits::MarshalPrivateKey, priv.get()))),
      Bytes(Declassified(Marshal(Traits::MarshalPrivateKey, priv2.get()))));
}

TEST(MLDSATest, Basic44) { MLDSABasicTest<MLDSA44Traits>(); }
TEST(MLDSATest, Basic65) { MLDSABasicTest<MLDSA65Traits>(); }
TEST(MLDSATest, Basic87) { MLDSABasicTest<MLDSA87Traits>(); }

TEST(MLDSATest, SignatureIsRandomized) {
  std::vector<uint8_t> encoded_public_key(MLDSA65_PUBLIC_KEY_BYTES);
  auto priv = std::make_unique<MLDSA65_private_key>();
  uint8_t seed[MLDSA_SEED_BYTES];
  EXPECT_TRUE(
      MLDSA65_generate_key(encoded_public_key.data(), seed, priv.get()));

  auto pub = std::make_unique<MLDSA65_public_key>();
  CBS cbs = CBS(encoded_public_key);
  ASSERT_TRUE(MLDSA65_parse_public_key(pub.get(), &cbs));

  std::vector<uint8_t> encoded_signature1(MLDSA65_SIGNATURE_BYTES);
  std::vector<uint8_t> encoded_signature2(MLDSA65_SIGNATURE_BYTES);
  static const uint8_t kMessage[] = {'H', 'e', 'l', 'l', 'o', ' ',
                                     'w', 'o', 'r', 'l', 'd'};
  EXPECT_TRUE(MLDSA65_sign(encoded_signature1.data(), priv.get(), kMessage,
                           sizeof(kMessage), nullptr, 0));
  EXPECT_TRUE(MLDSA65_sign(encoded_signature2.data(), priv.get(), kMessage,
                           sizeof(kMessage), nullptr, 0));

  EXPECT_NE(Bytes(encoded_signature1), Bytes(encoded_signature2));

  // Even though the signatures are different, they both verify.
  EXPECT_EQ(MLDSA65_verify(pub.get(), encoded_signature1.data(),
                           encoded_signature1.size(), kMessage,
                           sizeof(kMessage), nullptr, 0),
            1);
  EXPECT_EQ(MLDSA65_verify(pub.get(), encoded_signature2.data(),
                           encoded_signature2.size(), kMessage,
                           sizeof(kMessage), nullptr, 0),
            1);
}

TEST(MLDSATest, PrehashedSignatureVerifies) {
  std::vector<uint8_t> encoded_public_key(MLDSA65_PUBLIC_KEY_BYTES);
  auto priv = std::make_unique<MLDSA65_private_key>();
  uint8_t seed[MLDSA_SEED_BYTES];
  EXPECT_TRUE(
      MLDSA65_generate_key(encoded_public_key.data(), seed, priv.get()));

  auto pub = std::make_unique<MLDSA65_public_key>();
  CBS cbs = CBS(encoded_public_key);
  ASSERT_TRUE(MLDSA65_parse_public_key(pub.get(), &cbs));

  std::vector<uint8_t> encoded_signature(MLDSA65_SIGNATURE_BYTES);
  static const uint8_t kMessage[] = {'H', 'e', 'l', 'l', 'o', ' ',
                                     'w', 'o', 'r', 'l', 'd'};

  MLDSA65_prehash prehash_state;
  EXPECT_TRUE(MLDSA65_prehash_init(&prehash_state, pub.get(), nullptr, 0));
  MLDSA65_prehash_update(&prehash_state, kMessage, sizeof(kMessage));
  uint8_t representative[MLDSA_MU_BYTES];
  MLDSA65_prehash_finalize(representative, &prehash_state);
  EXPECT_TRUE(MLDSA65_sign_message_representative(encoded_signature.data(),
                                                  priv.get(), representative));

  EXPECT_EQ(MLDSA65_verify(pub.get(), encoded_signature.data(),
                           encoded_signature.size(), kMessage, sizeof(kMessage),
                           nullptr, 0),
            1);

  // Updating in multiple chunks also works.
  for (size_t i = 0; i <= sizeof(kMessage); ++i) {
    for (size_t j = i; j <= sizeof(kMessage); ++j) {
      EXPECT_TRUE(MLDSA65_prehash_init(&prehash_state, pub.get(), nullptr, 0));
      MLDSA65_prehash_update(&prehash_state, kMessage, i);
      MLDSA65_prehash_update(&prehash_state, kMessage + i, j - i);
      MLDSA65_prehash_update(&prehash_state, kMessage + j,
                             sizeof(kMessage) - j);
      MLDSA65_prehash_finalize(representative, &prehash_state);
      EXPECT_TRUE(MLDSA65_sign_message_representative(
          encoded_signature.data(), priv.get(), representative));

      EXPECT_EQ(MLDSA65_verify(pub.get(), encoded_signature.data(),
                               encoded_signature.size(), kMessage,
                               sizeof(kMessage), nullptr, 0),
                1);
    }
  }
}

TEST(MLDSATest, SignatureVerifiesFromPrehash) {
  std::vector<uint8_t> encoded_public_key(MLDSA65_PUBLIC_KEY_BYTES);
  auto priv = std::make_unique<MLDSA65_private_key>();
  uint8_t seed[MLDSA_SEED_BYTES];
  EXPECT_TRUE(
      MLDSA65_generate_key(encoded_public_key.data(), seed, priv.get()));

  auto pub = std::make_unique<MLDSA65_public_key>();
  CBS cbs = CBS(encoded_public_key);
  ASSERT_TRUE(MLDSA65_parse_public_key(pub.get(), &cbs));

  std::vector<uint8_t> encoded_signature(MLDSA65_SIGNATURE_BYTES);
  static const uint8_t kMessage[] = {'H', 'e', 'l', 'l', 'o', ' ',
                                     'w', 'o', 'r', 'l', 'd'};

  EXPECT_TRUE(MLDSA65_sign(encoded_signature.data(), priv.get(), kMessage,
                           sizeof(kMessage), nullptr, 0));

  MLDSA65_prehash prehash_state;
  EXPECT_TRUE(MLDSA65_prehash_init(&prehash_state, pub.get(), nullptr, 0));
  MLDSA65_prehash_update(&prehash_state, kMessage, sizeof(kMessage));
  uint8_t representative[MLDSA_MU_BYTES];
  MLDSA65_prehash_finalize(representative, &prehash_state);
  EXPECT_EQ(MLDSA65_verify_message_representative(
                pub.get(), encoded_signature.data(), encoded_signature.size(),
                representative),
            1);

  // Updating in multiple chunks also works.
  for (size_t i = 0; i <= sizeof(kMessage); ++i) {
    for (size_t j = i; j <= sizeof(kMessage); ++j) {
      EXPECT_TRUE(MLDSA65_prehash_init(&prehash_state, pub.get(), nullptr, 0));
      MLDSA65_prehash_update(&prehash_state, kMessage, i);
      MLDSA65_prehash_update(&prehash_state, kMessage + i, j - i);
      MLDSA65_prehash_update(&prehash_state, kMessage + j,
                             sizeof(kMessage) - j);
      MLDSA65_prehash_finalize(representative, &prehash_state);
      EXPECT_EQ(MLDSA65_verify_message_representative(
                    pub.get(), encoded_signature.data(),
                    encoded_signature.size(), representative),
                1);
    }
  }
}

TEST(MLDSATest, PublicFromPrivateIsConsistent) {
  std::vector<uint8_t> encoded_public_key(MLDSA65_PUBLIC_KEY_BYTES);
  auto priv = std::make_unique<MLDSA65_private_key>();
  uint8_t seed[MLDSA_SEED_BYTES];
  EXPECT_TRUE(
      MLDSA65_generate_key(encoded_public_key.data(), seed, priv.get()));

  auto pub = std::make_unique<MLDSA65_public_key>();
  EXPECT_TRUE(MLDSA65_public_from_private(pub.get(), priv.get()));

  std::vector<uint8_t> encoded_public_key2(MLDSA65_PUBLIC_KEY_BYTES);

  CBB cbb;
  CBB_init_fixed(&cbb, encoded_public_key2.data(), encoded_public_key2.size());
  ASSERT_TRUE(MLDSA65_marshal_public_key(&cbb, pub.get()));

  EXPECT_EQ(Bytes(encoded_public_key2), Bytes(encoded_public_key));
}

TEST(MLDSATest, InvalidPublicKeyEncodingLength) {
  // Encode a public key with a trailing 0 at the end.
  std::vector<uint8_t> encoded_public_key(MLDSA65_PUBLIC_KEY_BYTES + 1);
  auto priv = std::make_unique<MLDSA65_private_key>();
  uint8_t seed[MLDSA_SEED_BYTES];
  EXPECT_TRUE(
      MLDSA65_generate_key(encoded_public_key.data(), seed, priv.get()));

  // Public key is 1 byte too short.
  CBS cbs = CBS(Span(encoded_public_key).first(MLDSA65_PUBLIC_KEY_BYTES - 1));
  auto parsed_pub = std::make_unique<MLDSA65_public_key>();
  EXPECT_FALSE(MLDSA65_parse_public_key(parsed_pub.get(), &cbs));

  // Public key has the correct length.
  cbs = CBS(Span(encoded_public_key).first(MLDSA65_PUBLIC_KEY_BYTES));
  EXPECT_TRUE(MLDSA65_parse_public_key(parsed_pub.get(), &cbs));

  // Public key is 1 byte too long.
  cbs = CBS(encoded_public_key);
  EXPECT_FALSE(MLDSA65_parse_public_key(parsed_pub.get(), &cbs));
}

TEST(MLDSATest, InvalidPrivateKeyEncodingLength) {
  std::vector<uint8_t> encoded_public_key(MLDSA65_PUBLIC_KEY_BYTES);
  auto priv = std::make_unique<MLDSA65_private_key>();
  uint8_t seed[MLDSA_SEED_BYTES];
  EXPECT_TRUE(bcm_success(
      BCM_mldsa65_generate_key(encoded_public_key.data(), seed, priv.get())));

  CBB cbb;
  std::vector<uint8_t> malformed_private_key(BCM_MLDSA65_PRIVATE_KEY_BYTES + 1,
                                             0);
  CBB_init_fixed(&cbb, malformed_private_key.data(),
                 BCM_MLDSA65_PRIVATE_KEY_BYTES);
  ASSERT_TRUE(bcm_success(BCM_mldsa65_marshal_private_key(&cbb, priv.get())));

  CBS cbs;
  auto parsed_priv = std::make_unique<MLDSA65_private_key>();

  // Private key is 1 byte too short.
  CBS_init(&cbs, malformed_private_key.data(),
           BCM_MLDSA65_PRIVATE_KEY_BYTES - 1);
  EXPECT_FALSE(
      bcm_success(BCM_mldsa65_parse_private_key(parsed_priv.get(), &cbs)));

  // Private key has the correct length.
  CBS_init(&cbs, malformed_private_key.data(), BCM_MLDSA65_PRIVATE_KEY_BYTES);
  EXPECT_TRUE(
      bcm_success(BCM_mldsa65_parse_private_key(parsed_priv.get(), &cbs)));

  // Private key is 1 byte too long.
  CBS_init(&cbs, malformed_private_key.data(),
           BCM_MLDSA65_PRIVATE_KEY_BYTES + 1);
  EXPECT_FALSE(
      bcm_success(BCM_mldsa65_parse_private_key(parsed_priv.get(), &cbs)));
}

template <typename Traits>
void MLDSASigGenTest(FileTest *t) {
  std::vector<uint8_t> private_key_bytes, msg, expected_signature;
  ASSERT_TRUE(t->GetBytes(&private_key_bytes, "sk"));
  ASSERT_TRUE(t->GetBytes(&msg, "message"));
  ASSERT_TRUE(t->GetBytes(&expected_signature, "signature"));

  auto priv = std::make_unique<typename Traits::PrivateKey>();
  CBS cbs;
  CBS_init(&cbs, private_key_bytes.data(), private_key_bytes.size());
  EXPECT_TRUE(bcm_success(Traits::ParsePrivateKey(priv.get(), &cbs)));

  const uint8_t zero_randomizer[BCM_MLDSA_SIGNATURE_RANDOMIZER_BYTES] = {0};
  std::vector<uint8_t> signature(Traits::kSignatureBytes);
  EXPECT_TRUE(bcm_success(
      Traits::SignInternal(signature.data(), priv.get(), msg.data(), msg.size(),
                           nullptr, 0, nullptr, 0, zero_randomizer)));

  EXPECT_EQ(Bytes(signature), Bytes(expected_signature));

  auto pub = std::make_unique<typename Traits::PublicKey>();
  ASSERT_TRUE(Traits::PublicFromPrivate(pub.get(), priv.get()));
  EXPECT_TRUE(bcm_success(Traits::VerifyInternal(pub.get(), signature.data(),
                                                 msg.data(), msg.size(),
                                                 nullptr, 0, nullptr, 0)));
}

TEST(MLDSATest, SigGenTests44) {
  FileTestGTest("crypto/mldsa/mldsa_nist_siggen_44_tests.txt",
                MLDSASigGenTest<MLDSA44Traits>);
}

TEST(MLDSATest, SigGenTests65) {
  FileTestGTest("crypto/mldsa/mldsa_nist_siggen_65_tests.txt",
                MLDSASigGenTest<MLDSA65Traits>);
}

TEST(MLDSATest, SigGenTests87) {
  FileTestGTest("crypto/mldsa/mldsa_nist_siggen_87_tests.txt",
                MLDSASigGenTest<MLDSA87Traits>);
}

template <typename Traits>
void MLDSAKeyGenTest(FileTest *t) {
  std::vector<uint8_t> seed, expected_public_key, expected_private_key;
  ASSERT_TRUE(t->GetBytes(&seed, "seed"));
  CONSTTIME_SECRET(seed.data(), seed.size());
  ASSERT_TRUE(t->GetBytes(&expected_public_key, "pub"));
  ASSERT_TRUE(t->GetBytes(&expected_private_key, "priv"));

  std::vector<uint8_t> encoded_public_key(Traits::kPublicKeyBytes);
  auto priv = std::make_unique<typename Traits::PrivateKey>();
  ASSERT_TRUE(bcm_success(Traits::GenerateKeyExternalEntropy(
      encoded_public_key.data(), priv.get(), seed.data())));

  const std::vector<uint8_t> encoded_private_key =
      Marshal(Traits::MarshalPrivateKey, priv.get());

  EXPECT_EQ(Bytes(encoded_public_key), Bytes(expected_public_key));
  EXPECT_EQ(Bytes(Declassified(encoded_private_key)),
            Bytes(expected_private_key));
}

TEST(MLDSATest, KeyGenTests44) {
  FileTestGTest("crypto/mldsa/mldsa_nist_keygen_44_tests.txt",
                MLDSAKeyGenTest<MLDSA44Traits>);
}

TEST(MLDSATest, KeyGenTests65) {
  FileTestGTest("crypto/mldsa/mldsa_nist_keygen_65_tests.txt",
                MLDSAKeyGenTest<MLDSA65Traits>);
}

TEST(MLDSATest, KeyGenTests87) {
  FileTestGTest("crypto/mldsa/mldsa_nist_keygen_87_tests.txt",
                MLDSAKeyGenTest<MLDSA87Traits>);
}

template <typename Traits>
void MLDSAWycheproofSignCommon(FileTest *t, typename Traits::PrivateKey *priv) {
  std::vector<uint8_t> public_key, msg, mu, sig, context;
  ASSERT_TRUE(t->GetInstructionBytes(&public_key, "publicKey"));
  ASSERT_TRUE(t->GetBytes(&sig, "sig"));
  if (t->HasAttribute("ctx")) {
    t->GetBytes(&context, "ctx");
  }
  WycheproofResult result;
  ASSERT_TRUE(GetWycheproofResult(t, &result));
  bool expect_valid = result.IsValid();

  // The provided public key should match.
  auto pub = std::make_unique<typename Traits::PublicKey>();
  ASSERT_TRUE(Traits::PublicFromPrivate(pub.get(), priv));
  ScopedCBB pub_cbb;
  ASSERT_TRUE(CBB_init(pub_cbb.get(), Traits::kPublicKeyBytes));
  ASSERT_TRUE(Traits::MarshalPublicKey(pub_cbb.get(), pub.get()));
  EXPECT_EQ(Bytes(CBBAsSpan(pub_cbb.get())), Bytes(public_key));

  // Unfortunately we need to reimplement the context length check here because
  // we are using the internal function in order to pass in an all-zero
  // randomizer.
  if (context.size() > 255) {
    EXPECT_FALSE(expect_valid);
    t->IgnoreAttribute("msg");
    return;
  }

  // At this point, there are more signing error conditions.
  ASSERT_TRUE(expect_valid);

  // All tests provide mu.
  ASSERT_TRUE(t->GetBytes(&mu, "mu"));
  const uint8_t zero_randomizer[BCM_MLDSA_SIGNATURE_RANDOMIZER_BYTES] = {0};
  std::vector<uint8_t> computed_sig(Traits::kSignatureBytes);
  ASSERT_EQ(mu.size(), size_t{MLDSA_MU_BYTES});
  EXPECT_TRUE(bcm_success(Traits::SignMuInternal(computed_sig.data(), priv,
                                                 mu.data(), zero_randomizer)));
  EXPECT_EQ(Bytes(computed_sig), Bytes(sig));

  // Some tests provide the input message.
  if (t->HasAttribute("msg")) {
    ASSERT_TRUE(t->GetBytes(&msg, "msg"));
    const uint8_t context_prefix[2] = {0, static_cast<uint8_t>(context.size())};
    EXPECT_TRUE(bcm_success(
        Traits::SignInternal(computed_sig.data(), priv, msg.data(), msg.size(),
                             context_prefix, sizeof(context_prefix),
                             context.data(), context.size(), zero_randomizer)));
    EXPECT_EQ(Bytes(computed_sig), Bytes(sig));

    typename Traits::Prehash state;
    ASSERT_TRUE(
        Traits::PrehashInit(&state, pub.get(), context.data(), context.size()));
    Traits::PrehashUpdate(&state, msg.data(), msg.size());
    uint8_t computed_mu[MLDSA_MU_BYTES];
    Traits::PrehashFinalize(computed_mu, &state);
    EXPECT_EQ(Bytes(computed_mu), Bytes(mu));
  }
}

void IgnoreWycheproofSignatureAttributes(FileTest *t) {
  // When the private key is bad, Wycheproof sometimes still provides signature
  // inputs, but we don't get far enough to load them.
  t->IgnoreInstruction("publicKey");
  t->IgnoreAttribute("ctx");
  t->IgnoreAttribute("msg");
  t->IgnoreAttribute("mu");
  t->IgnoreAttribute("sig");
}

template <typename Traits>
void MLDSAWycheproofSignTest(FileTest *t) {
  std::vector<uint8_t> private_key_bytes;
  ASSERT_TRUE(t->GetInstructionBytes(&private_key_bytes, "privateKey"));
  WycheproofResult result;
  ASSERT_TRUE(GetWycheproofResult(t, &result));

  CBS cbs;
  CBS_init(&cbs, private_key_bytes.data(), private_key_bytes.size());
  auto priv = std::make_unique<typename Traits::PrivateKey>();
  if (!bcm_success(Traits::ParsePrivateKey(priv.get(), &cbs))) {
    EXPECT_FALSE(result.IsValid());
    IgnoreWycheproofSignatureAttributes(t);
    return;
  }

  MLDSAWycheproofSignCommon<Traits>(t, priv.get());
}

TEST(MLDSATest, WycheproofSignTests44) {
  FileTestGTest(
      "third_party/wycheproof_testvectors/mldsa_44_sign_noseed_test.txt",
      MLDSAWycheproofSignTest<MLDSA44Traits>);
}

TEST(MLDSATest, WycheproofSignTests65) {
  FileTestGTest(
      "third_party/wycheproof_testvectors/mldsa_65_sign_noseed_test.txt",
      MLDSAWycheproofSignTest<MLDSA65Traits>);
}

TEST(MLDSATest, WycheproofSignTests87) {
  FileTestGTest(
      "third_party/wycheproof_testvectors/mldsa_87_sign_noseed_test.txt",
      MLDSAWycheproofSignTest<MLDSA87Traits>);
}

template <typename Traits>
void MLDSASigGenFromSeedTest(FileTest *t) {
  std::vector<uint8_t> private_seed;
  ASSERT_TRUE(t->GetInstructionBytes(&private_seed, "privateSeed"));
  WycheproofResult result;
  ASSERT_TRUE(GetWycheproofResult(t, &result));
  t->IgnoreInstruction("privateKeyPkcs8");

  auto priv = std::make_unique<typename Traits::PrivateKey>();
  if (!Traits::PrivateKeyFromSeed(priv.get(), private_seed.data(),
                                  private_seed.size())) {
    EXPECT_FALSE(result.IsValid());
    IgnoreWycheproofSignatureAttributes(t);
    return;
  }

  MLDSAWycheproofSignCommon<Traits>(t, priv.get());
}

TEST(MLDSATest, WycheproofSignWithSeedTests44) {
  FileTestGTest(
      "third_party/wycheproof_testvectors/mldsa_44_sign_seed_test.txt",
      MLDSASigGenFromSeedTest<MLDSA44Traits>);
}

TEST(MLDSATest, WycheproofSignWithSeedTests65) {
  FileTestGTest(
      "third_party/wycheproof_testvectors/mldsa_65_sign_seed_test.txt",
      MLDSASigGenFromSeedTest<MLDSA65Traits>);
}

TEST(MLDSATest, WycheproofSignWithSeedTests87) {
  FileTestGTest(
      "third_party/wycheproof_testvectors/mldsa_87_sign_seed_test.txt",
      MLDSASigGenFromSeedTest<MLDSA87Traits>);
}

template <typename Traits>
void MLDSAWycheproofVerifyTest(FileTest *t) {
  std::vector<uint8_t> public_key_bytes, msg, signature, context;
  t->IgnoreInstruction("publicKeyDer");
  ASSERT_TRUE(t->GetInstructionBytes(&public_key_bytes, "publicKey"));
  ASSERT_TRUE(t->GetBytes(&msg, "msg"));
  ASSERT_TRUE(t->GetBytes(&signature, "sig"));
  if (t->HasAttribute("ctx")) {
    t->GetBytes(&context, "ctx");
  }
  WycheproofResult result;
  ASSERT_TRUE(GetWycheproofResult(t, &result));
  bool expect_valid = result.IsValid();

  CBS cbs;
  CBS_init(&cbs, public_key_bytes.data(), public_key_bytes.size());
  auto pub = std::make_unique<typename Traits::PublicKey>();
  if (!Traits::ParsePublicKey(pub.get(), &cbs)) {
    EXPECT_FALSE(expect_valid);
    return;
  }

  const int sig_ok =
      Traits::Verify(pub.get(), signature.data(), signature.size(), msg.data(),
                     msg.size(), context.data(), context.size());
  EXPECT_EQ(sig_ok, expect_valid ? 1 : 0);
}

TEST(MLDSATest, WycheproofVerifyTests65) {
  FileTestGTest("third_party/wycheproof_testvectors/mldsa_65_verify_test.txt",
                MLDSAWycheproofVerifyTest<MLDSA65Traits>);
}

TEST(MLDSATest, WycheproofVerifyTests87) {
  FileTestGTest("third_party/wycheproof_testvectors/mldsa_87_verify_test.txt",
                MLDSAWycheproofVerifyTest<MLDSA87Traits>);
}

TEST(MLDSATest, WycheproofVerifyTests44) {
  FileTestGTest("third_party/wycheproof_testvectors/mldsa_44_verify_test.txt",
                MLDSAWycheproofVerifyTest<MLDSA44Traits>);
}

TEST(MLDSATest, Self) { ASSERT_TRUE(boringssl_self_test_mldsa()); }

TEST(MLDSATest, PWCT) {
  uint8_t seed[MLDSA_SEED_BYTES];

  auto pub65 = std::make_unique<uint8_t[]>(MLDSA65_PUBLIC_KEY_BYTES);
  auto priv65 = std::make_unique<MLDSA65_private_key>();
  ASSERT_EQ(BCM_mldsa65_generate_key_fips(pub65.get(), seed, priv65.get()),
            bcm_status::approved);

  auto pub87 = std::make_unique<uint8_t[]>(MLDSA87_PUBLIC_KEY_BYTES);
  auto priv87 = std::make_unique<MLDSA87_private_key>();
  ASSERT_EQ(BCM_mldsa87_generate_key_fips(pub87.get(), seed, priv87.get()),
            bcm_status::approved);

  auto pub44 = std::make_unique<uint8_t[]>(MLDSA44_PUBLIC_KEY_BYTES);
  auto priv44 = std::make_unique<MLDSA44_private_key>();
  ASSERT_EQ(BCM_mldsa44_generate_key_fips(pub44.get(), seed, priv44.get()),
            bcm_status::approved);
}

TEST(MLDSATest, NullptrArgumentsToCreate) {
  // For FIPS reasons, this should fail rather than crash.
  ASSERT_EQ(BCM_mldsa65_generate_key_fips(nullptr, nullptr, nullptr),
            bcm_status::failure);
  ASSERT_EQ(BCM_mldsa87_generate_key_fips(nullptr, nullptr, nullptr),
            bcm_status::failure);
  ASSERT_EQ(BCM_mldsa44_generate_key_fips(nullptr, nullptr, nullptr),
            bcm_status::failure);
  ASSERT_EQ(
      BCM_mldsa65_generate_key_external_entropy_fips(nullptr, nullptr, nullptr),
      bcm_status::failure);
  ASSERT_EQ(
      BCM_mldsa87_generate_key_external_entropy_fips(nullptr, nullptr, nullptr),
      bcm_status::failure);
  ASSERT_EQ(
      BCM_mldsa44_generate_key_external_entropy_fips(nullptr, nullptr, nullptr),
      bcm_status::failure);
}

}  // namespace
BSSL_NAMESPACE_END
