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

#include <cstdint>
#include <vector>

#include <string.h>

#include <gtest/gtest.h>

#include <openssl/base.h>
#include <openssl/bytestring.h>
#include <openssl/mem.h>
#include <openssl/mlkem.h>

#include "../fipsmodule/bcm_interface.h"
#include "../fipsmodule/keccak/internal.h"
#include "../internal.h"
#include "../test/file_test.h"
#include "../test/test_util.h"
#include "../test/wycheproof_util.h"


BSSL_NAMESPACE_BEGIN
namespace {

// Arguments are templated to avoid needing to repeat the underlying function's
// type signature. We cannot use p_mldsa.cc's pattern because, on Windows,
// cross-dll function pointers are not constexpr.
#define TRAIT_METHOD(method_name, function_name) \
  template <typename... Args>                    \
  static auto method_name(Args... args) {        \
    return function_name(args...);               \
  }

#define MAKE_MLKEM_TRAITS(bits)                                                \
  struct MLKEM##bits##Traits {                                                 \
    using PublicKey = MLKEM##bits##_public_key;                                \
    using PrivateKey = MLKEM##bits##_private_key;                              \
                                                                               \
    static constexpr size_t kPublicKeyBytes = MLKEM##bits##_PUBLIC_KEY_BYTES;  \
    static constexpr size_t kPrivateKeyBytes =                                 \
        BCM_MLKEM##bits##_PRIVATE_KEY_BYTES;                                   \
    static constexpr size_t kCiphertextBytes = MLKEM##bits##_CIPHERTEXT_BYTES; \
                                                                               \
    TRAIT_METHOD(GenerateKey, MLKEM##bits##_generate_key)                      \
    TRAIT_METHOD(PrivateKeyFromSeed, MLKEM##bits##_private_key_from_seed)      \
    TRAIT_METHOD(PublicFromPrivate, MLKEM##bits##_public_from_private)         \
    TRAIT_METHOD(ParsePublicKey, MLKEM##bits##_parse_public_key)               \
    TRAIT_METHOD(MarshalPublicKey, MLKEM##bits##_marshal_public_key)           \
    TRAIT_METHOD(ParsePrivateKey, BCM_mlkem##bits##_parse_private_key)         \
    TRAIT_METHOD(MarshalPrivateKey, BCM_mlkem##bits##_marshal_private_key)     \
    TRAIT_METHOD(GenerateKeyExternalSeed,                                      \
                 BCM_mlkem##bits##_generate_key_external_seed)                 \
    TRAIT_METHOD(EncapExternalEntropy,                                         \
                 BCM_mlkem##bits##_encap_external_entropy)                     \
    TRAIT_METHOD(Encap, MLKEM##bits##_encap)                                   \
    TRAIT_METHOD(Decap, MLKEM##bits##_decap)                                   \
  };

MAKE_MLKEM_TRAITS(768)
MAKE_MLKEM_TRAITS(1024)

template <typename T>
std::vector<uint8_t> Marshal(int (*marshal_func)(CBB *, const T *),
                             const T *t) {
  ScopedCBB cbb;
  uint8_t *encoded;
  size_t encoded_len;
  if (!CBB_init(cbb.get(), 1) ||      //
      !marshal_func(cbb.get(), t) ||  //
      !CBB_finish(cbb.get(), &encoded, &encoded_len)) {
    abort();
  }

  std::vector<uint8_t> ret(encoded, encoded + encoded_len);
  OPENSSL_free(encoded);
  return ret;
}

template <typename T>
std::vector<uint8_t> Marshal(bcm_status (*marshal_func)(CBB *, const T *),
                             const T *t) {
  ScopedCBB cbb;
  uint8_t *encoded;
  size_t encoded_len;
  if (!CBB_init(cbb.get(), 1) ||                   //
      !bcm_success(marshal_func(cbb.get(), t)) ||  //
      !CBB_finish(cbb.get(), &encoded, &encoded_len)) {
    abort();
  }

  std::vector<uint8_t> ret(encoded, encoded + encoded_len);
  OPENSSL_free(encoded);
  return ret;
}

template <typename Traits>
void BasicTest() {
  // This function makes several ML-KEM keys, which runs up against stack
  // limits. Heap-allocate them instead.

  uint8_t encoded_public_key[Traits::kPublicKeyBytes];
  uint8_t seed[MLKEM_SEED_BYTES];
  auto priv = std::make_unique<typename Traits::PrivateKey>();
  Traits::GenerateKey(encoded_public_key, seed, priv.get());

  {
    auto priv2 = std::make_unique<typename Traits::PrivateKey>();
    ASSERT_TRUE(Traits::PrivateKeyFromSeed(priv2.get(), seed, sizeof(seed)));
    EXPECT_EQ(
        Bytes(Declassified(Marshal(Traits::MarshalPrivateKey, priv.get()))),
        Bytes(Declassified(Marshal(Traits::MarshalPrivateKey, priv2.get()))));
  }

  uint8_t first_two_bytes[2];
  OPENSSL_memcpy(first_two_bytes, encoded_public_key, sizeof(first_two_bytes));
  OPENSSL_memset(encoded_public_key, 0xff, sizeof(first_two_bytes));
  CBS encoded_public_key_cbs;
  CBS_init(&encoded_public_key_cbs, encoded_public_key,
           sizeof(encoded_public_key));
  auto pub = std::make_unique<typename Traits::PublicKey>();
  // Parsing should fail because the first coefficient is >= kPrime;
  ASSERT_FALSE(Traits::ParsePublicKey(pub.get(), &encoded_public_key_cbs));

  OPENSSL_memcpy(encoded_public_key, first_two_bytes, sizeof(first_two_bytes));
  CBS_init(&encoded_public_key_cbs, encoded_public_key,
           sizeof(encoded_public_key));
  ASSERT_TRUE(Traits::ParsePublicKey(pub.get(), &encoded_public_key_cbs));
  EXPECT_EQ(CBS_len(&encoded_public_key_cbs), 0u);

  EXPECT_EQ(Bytes(encoded_public_key),
            Bytes(Marshal(Traits::MarshalPublicKey, pub.get())));

  auto pub2 = std::make_unique<typename Traits::PublicKey>();
  Traits::PublicFromPrivate(pub2.get(), priv.get());
  EXPECT_EQ(Bytes(encoded_public_key),
            Bytes(Marshal(Traits::MarshalPublicKey, pub2.get())));

  std::vector<uint8_t> encoded_private_key(
      Marshal(Traits::MarshalPrivateKey, priv.get()));
  EXPECT_EQ(encoded_private_key.size(), size_t{Traits::kPrivateKeyBytes});

  // Parsing should fail if a coefficient is out of range.
  {
    std::vector<uint8_t> invalid = encoded_private_key;
    invalid[0] = 0xff;
    invalid[1] = 0xff;
    CBS cbs;
    CBS_init(&cbs, invalid.data(), invalid.size());
    auto priv2 = std::make_unique<typename Traits::PrivateKey>();
    ASSERT_FALSE(bcm_success(Traits::ParsePrivateKey(priv2.get(), &cbs)));
  }

  // Parsing should fail if the public key hash is wrong.
  {
    std::vector<uint8_t> invalid = encoded_private_key;
    // The final 64 bytes of the semi-expanded format are the 32-byte hash and
    // the 32-byte FO (Fujisaki-Okamoto) failure secret. Flip a bit in the hash.
    invalid[invalid.size() - 33] ^= 1;
    CBS cbs;
    CBS_init(&cbs, invalid.data(), invalid.size());
    auto priv2 = std::make_unique<typename Traits::PrivateKey>();
    ASSERT_FALSE(bcm_success(Traits::ParsePrivateKey(priv2.get(), &cbs)));
  }

  CBS cbs;
  CBS_init(&cbs, encoded_private_key.data(), encoded_private_key.size());
  auto priv2 = std::make_unique<typename Traits::PrivateKey>();
  ASSERT_TRUE(bcm_success(Traits::ParsePrivateKey(priv2.get(), &cbs)));
  EXPECT_EQ(
      Bytes(Declassified(encoded_private_key)),
      Bytes(Declassified(Marshal(Traits::MarshalPrivateKey, priv2.get()))));

  uint8_t ciphertext[Traits::kCiphertextBytes];
  uint8_t shared_secret1[MLKEM_SHARED_SECRET_BYTES];
  uint8_t shared_secret2[MLKEM_SHARED_SECRET_BYTES];
  Traits::Encap(ciphertext, shared_secret1, pub.get());
  ASSERT_TRUE(Traits::Decap(shared_secret2, ciphertext, sizeof(ciphertext),
                            priv.get()));
  EXPECT_EQ(Bytes(Declassified(shared_secret1)),
            Bytes(Declassified(shared_secret2)));
  ASSERT_TRUE(Traits::Decap(shared_secret2, ciphertext, sizeof(ciphertext),
                            priv2.get()));
  EXPECT_EQ(Bytes(Declassified(shared_secret1)),
            Bytes(Declassified(shared_secret2)));
}

TEST(MLKEMTest, Basic768) { BasicTest<MLKEM768Traits>(); }
TEST(MLKEMTest, Basic1024) { BasicTest<MLKEM1024Traits>(); }

template <typename Traits>
void MLKEMKeyGenFileTest(FileTest *t) {
  std::vector<uint8_t> expected_pub_key_bytes, seed, expected_priv_key_bytes;
  ASSERT_TRUE(t->GetBytes(&seed, "seed"));
  CONSTTIME_SECRET(seed.data(), seed.size());
  ASSERT_TRUE(t->GetBytes(&expected_pub_key_bytes, "public_key"));
  ASSERT_TRUE(t->GetBytes(&expected_priv_key_bytes, "private_key"));

  ASSERT_EQ(seed.size(), size_t{MLKEM_SEED_BYTES});

  std::vector<uint8_t> pub_key_bytes(Traits::kPublicKeyBytes);
  auto priv = std::make_unique<typename Traits::PrivateKey>();
  Traits::GenerateKeyExternalSeed(pub_key_bytes.data(), priv.get(),
                                  seed.data());
  const std::vector<uint8_t> priv_key_bytes(
      Marshal(Traits::MarshalPrivateKey, priv.get()));

  EXPECT_EQ(Bytes(pub_key_bytes), Bytes(expected_pub_key_bytes));
  EXPECT_EQ(Bytes(Declassified(priv_key_bytes)),
            Bytes(expected_priv_key_bytes));
}

TEST(MLKEMTest, KeyGen768TestVectors) {
  FileTestGTest("crypto/mlkem/mlkem768_keygen_tests.txt",
                MLKEMKeyGenFileTest<MLKEM768Traits>);
}

TEST(MLKEMTest, KeyGen1024TestVectors) {
  FileTestGTest("crypto/mlkem/mlkem1024_keygen_tests.txt",
                MLKEMKeyGenFileTest<MLKEM1024Traits>);
}

template <typename Traits>
void MLKEMNistKeyGenFileTest(FileTest *t) {
  std::vector<uint8_t> expected_pub_key_bytes, z, d, expected_priv_key_bytes;
  ASSERT_TRUE(t->GetBytes(&z, "z"));
  ASSERT_TRUE(t->GetBytes(&d, "d"));
  ASSERT_TRUE(t->GetBytes(&expected_pub_key_bytes, "ek"));
  ASSERT_TRUE(t->GetBytes(&expected_priv_key_bytes, "dk"));

  ASSERT_EQ(z.size(), size_t{MLKEM_SEED_BYTES} / 2);
  ASSERT_EQ(d.size(), size_t{MLKEM_SEED_BYTES} / 2);

  uint8_t seed[MLKEM_SEED_BYTES];
  OPENSSL_memcpy(&seed[0], d.data(), d.size());
  OPENSSL_memcpy(&seed[MLKEM_SEED_BYTES / 2], z.data(), z.size());
  std::vector<uint8_t> pub_key_bytes(Traits::kPublicKeyBytes);
  auto priv = std::make_unique<typename Traits::PrivateKey>();
  Traits::GenerateKeyExternalSeed(pub_key_bytes.data(), priv.get(), seed);
  const std::vector<uint8_t> priv_key_bytes(
      Marshal(Traits::MarshalPrivateKey, priv.get()));

  EXPECT_EQ(Bytes(pub_key_bytes), Bytes(expected_pub_key_bytes));
  EXPECT_EQ(Bytes(priv_key_bytes), Bytes(expected_priv_key_bytes));
}

TEST(MLKEMTest, NISTKeyGen768TestVectors) {
  FileTestGTest("crypto/mlkem/mlkem768_nist_keygen_tests.txt",
                MLKEMNistKeyGenFileTest<MLKEM768Traits>);
}

TEST(MLKEMTest, NISTKeyGen1024TestVectors) {
  FileTestGTest("crypto/mlkem/mlkem1024_nist_keygen_tests.txt",
                MLKEMNistKeyGenFileTest<MLKEM1024Traits>);
}

template <typename Traits>
void MLKEMEncapFileTest(FileTest *t) {
  std::vector<uint8_t> pub_key_bytes, entropy, expected_ciphertext,
      expected_shared_secret;
  ASSERT_TRUE(t->GetBytes(&entropy, "entropy"));
  CONSTTIME_SECRET(entropy.data(), entropy.size());
  ASSERT_TRUE(t->GetBytes(&pub_key_bytes, "public_key"));
  ASSERT_TRUE(t->GetBytes(&expected_ciphertext, "ciphertext"));
  ASSERT_TRUE(t->GetBytes(&expected_shared_secret, "shared_secret"));
  std::string result;
  ASSERT_TRUE(t->GetAttribute(&result, "result"));

  typename Traits::PublicKey pub_key;
  CBS pub_key_cbs;
  CBS_init(&pub_key_cbs, pub_key_bytes.data(), pub_key_bytes.size());
  const int parse_ok = Traits::ParsePublicKey(&pub_key, &pub_key_cbs);
  ASSERT_EQ(parse_ok, result == "pass");
  if (!parse_ok) {
    return;
  }

  uint8_t ciphertext[Traits::kCiphertextBytes];
  uint8_t shared_secret[MLKEM_SHARED_SECRET_BYTES];
  Traits::EncapExternalEntropy(ciphertext, shared_secret, &pub_key,
                               entropy.data());

  ASSERT_EQ(Bytes(expected_ciphertext), Bytes(ciphertext));
  ASSERT_EQ(Bytes(expected_shared_secret), Bytes(Declassified(shared_secret)));
}

TEST(MLKEMTest, Encap768TestVectors) {
  FileTestGTest("crypto/mlkem/mlkem768_encap_tests.txt",
                MLKEMEncapFileTest<MLKEM768Traits>);
}

TEST(MLKEMTest, Encap1024TestVectors) {
  FileTestGTest("crypto/mlkem/mlkem1024_encap_tests.txt",
                MLKEMEncapFileTest<MLKEM1024Traits>);
}

template <typename Traits>
void MLKEMDecapFileTest(FileTest *t) {
  std::vector<uint8_t> priv_key_bytes, ciphertext, expected_shared_secret;
  ASSERT_TRUE(t->GetBytes(&priv_key_bytes, "private_key"));
  ASSERT_TRUE(t->GetBytes(&ciphertext, "ciphertext"));
  ASSERT_TRUE(t->GetBytes(&expected_shared_secret, "shared_secret"));
  std::string result;
  ASSERT_TRUE(t->GetAttribute(&result, "result"));

  typename Traits::PrivateKey priv_key;
  CBS priv_key_cbs;
  CBS_init(&priv_key_cbs, priv_key_bytes.data(), priv_key_bytes.size());
  const int parse_ok =
      bcm_success(Traits::ParsePrivateKey(&priv_key, &priv_key_cbs));
  if (!parse_ok) {
    ASSERT_NE(result, "pass");
    return;
  }

  uint8_t shared_secret[MLKEM_SHARED_SECRET_BYTES];
  const int decap_ok = Traits::Decap(shared_secret, ciphertext.data(),
                                     ciphertext.size(), &priv_key);
  if (!decap_ok) {
    ASSERT_NE(result, "pass");
    return;
  }

  ASSERT_EQ(Bytes(expected_shared_secret), Bytes(shared_secret));
}

TEST(MLKEMTest, Decap768TestVectors) {
  FileTestGTest("crypto/mlkem/mlkem768_decap_tests.txt",
                MLKEMDecapFileTest<MLKEM768Traits>);
}

TEST(MLKEMTest, Decap1024TestVectors) {
  FileTestGTest("crypto/mlkem/mlkem1024_decap_tests.txt",
                MLKEMDecapFileTest<MLKEM1024Traits>);
}

template <typename Traits>
void MLKEMNistDecapFileTest(FileTest *t) {
  std::vector<uint8_t> ciphertext, expected_shared_secret, private_key_bytes;
  ASSERT_TRUE(t->GetBytes(&ciphertext, "c"));
  ASSERT_TRUE(t->GetBytes(&expected_shared_secret, "k"));
  ASSERT_TRUE(t->GetInstructionBytes(&private_key_bytes, "dk"));

  typename Traits::PrivateKey priv;
  CBS private_key_cbs;
  CBS_init(&private_key_cbs, private_key_bytes.data(),
           private_key_bytes.size());
  ASSERT_TRUE(bcm_success(Traits::ParsePrivateKey(&priv, &private_key_cbs)));

  uint8_t shared_secret[MLKEM_SHARED_SECRET_BYTES];
  ASSERT_TRUE(Traits::Decap(shared_secret, ciphertext.data(), ciphertext.size(),
                            &priv));

  ASSERT_EQ(Bytes(shared_secret), Bytes(expected_shared_secret));
}

TEST(MLKEMTest, NistDecap768TestVectors) {
  FileTestGTest("crypto/mlkem/mlkem768_nist_decap_tests.txt",
                MLKEMNistDecapFileTest<MLKEM768Traits>);
}

TEST(MLKEMTest, NistDecap1024TestVectors) {
  FileTestGTest("crypto/mlkem/mlkem1024_nist_decap_tests.txt",
                MLKEMNistDecapFileTest<MLKEM1024Traits>);
}

// Unoptimized builds are much slower, and iterative tests run ML-KEM many
// times. Disable them in unoptimized builds for now.
// https://crbug.com/479850443
#if (defined(__GNUC__) || defined(__clang__)) && !defined(__OPTIMIZE__)
#define DISABLE_IF_NOT_OPTIMIZED(t) DISABLED_##t
#else
#define DISABLE_IF_NOT_OPTIMIZED(t) t
#endif

template <typename Traits>
void IteratedTest(uint8_t out[32]) {
  BORINGSSL_keccak_st generate_st;
  BORINGSSL_keccak_init(&generate_st, boringssl_shake128);
  BORINGSSL_keccak_st results_st;
  BORINGSSL_keccak_init(&results_st, boringssl_shake128);

  auto priv = std::make_unique<typename Traits::PrivateKey>();
  auto pub = std::make_unique<typename Traits::PublicKey>();
  for (int i = 0; i < 10000; i++) {
    uint8_t seed[MLKEM_SEED_BYTES];
    BORINGSSL_keccak_squeeze(&generate_st, seed, sizeof(seed));
    uint8_t encoded_pub[Traits::kPublicKeyBytes];
    Traits::GenerateKeyExternalSeed(encoded_pub, priv.get(), seed);
    Traits::PublicFromPrivate(pub.get(), priv.get());

    BORINGSSL_keccak_absorb(&results_st, encoded_pub, sizeof(encoded_pub));
    const std::vector<uint8_t> encoded_priv(
        Marshal(Traits::MarshalPrivateKey, priv.get()));
    BORINGSSL_keccak_absorb(&results_st, encoded_priv.data(),
                            encoded_priv.size());

    uint8_t encap_entropy[BCM_MLKEM_ENCAP_ENTROPY];
    BORINGSSL_keccak_squeeze(&generate_st, encap_entropy,
                             sizeof(encap_entropy));
    uint8_t ciphertext[Traits::kCiphertextBytes];
    uint8_t shared_secret[MLKEM_SHARED_SECRET_BYTES];
    Traits::EncapExternalEntropy(ciphertext, shared_secret, pub.get(),
                                 encap_entropy);

    BORINGSSL_keccak_absorb(&results_st, ciphertext, sizeof(ciphertext));
    BORINGSSL_keccak_absorb(&results_st, shared_secret, sizeof(shared_secret));

    uint8_t invalid_ciphertext[Traits::kCiphertextBytes];
    BORINGSSL_keccak_squeeze(&generate_st, invalid_ciphertext,
                             sizeof(invalid_ciphertext));
    ASSERT_TRUE(Traits::Decap(shared_secret, invalid_ciphertext,
                              sizeof(invalid_ciphertext), priv.get()));

    BORINGSSL_keccak_absorb(&results_st, shared_secret, sizeof(shared_secret));
  }

  BORINGSSL_keccak_squeeze(&results_st, out, 32);
}

TEST(MLKEMTest, DISABLE_IF_NOT_OPTIMIZED(Iterate768)) {
  // The structure of this test is taken from
  // https://github.com/C2SP/CCTV/blob/main/ML-KEM/README.md?ref=words.filippo.io#accumulated-pq-crystals-vectors
  // but the final value has been updated to reflect the change from Kyber to
  // ML-KEM.
  uint8_t result[32];
  IteratedTest<MLKEM768Traits>(result);

  const uint8_t kExpected[32] = {
      0xf9, 0x59, 0xd1, 0x8d, 0x3d, 0x11, 0x80, 0x12, 0x14, 0x33, 0xbf,
      0x0e, 0x05, 0xf1, 0x1e, 0x79, 0x08, 0xcf, 0x9d, 0x03, 0xed, 0xc1,
      0x50, 0xb2, 0xb0, 0x7c, 0xb9, 0x0b, 0xef, 0x5b, 0xc1, 0xc1};
  EXPECT_EQ(Bytes(result), Bytes(kExpected));
}


TEST(MLKEMTest, DISABLE_IF_NOT_OPTIMIZED(Iterate1024)) {
  // The structure of this test is taken from
  // https://github.com/C2SP/CCTV/blob/main/ML-KEM/README.md?ref=words.filippo.io#accumulated-pq-crystals-vectors
  // but the final value has been updated to reflect the change from Kyber to
  // ML-KEM.
  uint8_t result[32];
  IteratedTest<MLKEM1024Traits>(result);

  const uint8_t kExpected[32] = {
      0xe3, 0xbf, 0x82, 0xb0, 0x13, 0x30, 0x7b, 0x2e, 0x9d, 0x47, 0xdd,
      0xe7, 0x91, 0xff, 0x6d, 0xfc, 0x82, 0xe6, 0x94, 0xe6, 0x38, 0x24,
      0x04, 0xab, 0xdb, 0x94, 0x8b, 0x90, 0x8b, 0x75, 0xba, 0xd5};
  EXPECT_EQ(Bytes(result), Bytes(kExpected));
}

template <typename Traits>
void MLKEMWycheproofKeyGenSeedTest(FileTest *t) {
  t->IgnoreInstruction("parameterSet");
  std::vector<uint8_t> seed, expected_ek, expected_dk;
  ASSERT_TRUE(t->GetBytes(&seed, "seed"));
  CONSTTIME_SECRET(seed.data(), seed.size());
  ASSERT_TRUE(t->GetBytes(&expected_ek, "ek"));
  ASSERT_TRUE(t->GetBytes(&expected_dk, "dk"));
  WycheproofResult result;
  ASSERT_TRUE(GetWycheproofResult(t, &result));
  bool expect_valid = result.IsValid();

  typename Traits::PrivateKey priv_key;
  if (!Traits::PrivateKeyFromSeed(&priv_key, seed.data(), seed.size())) {
    EXPECT_FALSE(expect_valid);
    return;
  }

  ASSERT_TRUE(expect_valid);

  const std::vector<uint8_t> computed_dk =
      Marshal(Traits::MarshalPrivateKey, &priv_key);
  EXPECT_EQ(Bytes(Declassified(computed_dk)), Bytes(expected_dk));

  typename Traits::PublicKey pub_key;
  Traits::PublicFromPrivate(&pub_key, &priv_key);
  const std::vector<uint8_t> computed_ek =
      Marshal(Traits::MarshalPublicKey, &pub_key);
  EXPECT_EQ(Bytes(computed_ek), Bytes(expected_ek));
}

TEST(MLKEMTest, WycheproofKeyGenSeed768) {
  FileTestGTest(
      "third_party/wycheproof_testvectors/mlkem_768_keygen_seed_test.txt",
      MLKEMWycheproofKeyGenSeedTest<MLKEM768Traits>);
}

TEST(MLKEMTest, WycheproofKeyGenSeed1024) {
  FileTestGTest(
      "third_party/wycheproof_testvectors/mlkem_1024_keygen_seed_test.txt",
      MLKEMWycheproofKeyGenSeedTest<MLKEM1024Traits>);
}

template <typename Traits>
void MLKEMWycheproofDecapTest(FileTest *t) {
  t->IgnoreInstruction("parameterSet");
  std::vector<uint8_t> seed, c, K;
  ASSERT_TRUE(t->GetBytes(&seed, "seed"));
  CONSTTIME_SECRET(seed.data(), seed.size());
  ASSERT_TRUE(t->GetBytes(&c, "c"));
  ASSERT_TRUE(t->GetBytes(&K, "K"));
  WycheproofResult result;
  ASSERT_TRUE(GetWycheproofResult(t, &result));
  bool expect_valid = result.IsValid();

  typename Traits::PrivateKey priv_key;
  if (!Traits::PrivateKeyFromSeed(&priv_key, seed.data(), seed.size())) {
    EXPECT_FALSE(expect_valid);
    return;
  }

  // Verify that the derived public key matches the one in the test case.
  if (t->HasAttribute("ek")) {
    std::vector<uint8_t> ek;
    ASSERT_TRUE(t->GetBytes(&ek, "ek"));
    typename Traits::PublicKey pub_key;
    Traits::PublicFromPrivate(&pub_key, &priv_key);
    EXPECT_EQ(Bytes(Marshal(Traits::MarshalPublicKey, &pub_key)), Bytes(ek));
  }

  uint8_t computed_K[MLKEM_SHARED_SECRET_BYTES];
  if (!Traits::Decap(computed_K, c.data(), c.size(), &priv_key)) {
    EXPECT_FALSE(expect_valid);
    return;
  }

  EXPECT_TRUE(expect_valid);
  EXPECT_EQ(Bytes(Declassified(computed_K)), Bytes(K));
}

TEST(MLKEMTest, WycheproofDecap768) {
  FileTestGTest("third_party/wycheproof_testvectors/mlkem_768_test.txt",
                MLKEMWycheproofDecapTest<MLKEM768Traits>);
}

TEST(MLKEMTest, WycheproofDecap1024) {
  FileTestGTest("third_party/wycheproof_testvectors/mlkem_1024_test.txt",
                MLKEMWycheproofDecapTest<MLKEM1024Traits>);
}

template <typename Traits>
void MLKEMWycheproofEncapTest(FileTest *t) {
  t->IgnoreInstruction("parameterSet");
  std::vector<uint8_t> ek, m, expected_c, expected_K;
  ASSERT_TRUE(t->GetBytes(&ek, "ek"));
  ASSERT_TRUE(t->GetBytes(&m, "m"));
  ASSERT_TRUE(t->GetBytes(&expected_c, "c"));
  ASSERT_TRUE(t->GetBytes(&expected_K, "K"));
  WycheproofResult result;
  ASSERT_TRUE(GetWycheproofResult(t, &result));
  bool expect_valid = result.IsValid();

  typename Traits::PublicKey pub_key;
  CBS cbs;
  CBS_init(&cbs, ek.data(), ek.size());
  if (!Traits::ParsePublicKey(&pub_key, &cbs) || CBS_len(&cbs) != 0) {
    EXPECT_FALSE(expect_valid);
    return;
  }

  ASSERT_TRUE(expect_valid);
  ASSERT_EQ(m.size(), size_t{BCM_MLKEM_ENCAP_ENTROPY});
  CONSTTIME_SECRET(m.data(), m.size());

  uint8_t computed_c[Traits::kCiphertextBytes];
  uint8_t computed_K[MLKEM_SHARED_SECRET_BYTES];
  Traits::EncapExternalEntropy(computed_c, computed_K, &pub_key, m.data());

  EXPECT_EQ(Bytes(computed_c), Bytes(expected_c));
  EXPECT_EQ(Bytes(Declassified(computed_K)), Bytes(expected_K));
}

TEST(MLKEMTest, WycheproofEncap768) {
  FileTestGTest("third_party/wycheproof_testvectors/mlkem_768_encaps_test.txt",
                MLKEMWycheproofEncapTest<MLKEM768Traits>);
}

TEST(MLKEMTest, WycheproofEncap1024) {
  FileTestGTest("third_party/wycheproof_testvectors/mlkem_1024_encaps_test.txt",
                MLKEMWycheproofEncapTest<MLKEM1024Traits>);
}

template <typename Traits>
void MLKEMWycheproofSemiExpandedDecapTest(FileTest *t) {
  t->IgnoreInstruction("parameterSet");
  std::vector<uint8_t> dk, ek, c, K;
  ASSERT_TRUE(t->GetBytes(&dk, "dk"));
  // We do not mark `dk` as `CONSTTIME_SECRET`. Only a portion of `dk` is
  // secret. Since we do not support the semi-expanded key outside of testing,
  // it is not worth accurately annotating this.
  ASSERT_TRUE(t->GetBytes(&ek, "ek"));
  ASSERT_TRUE(t->GetBytes(&c, "c"));

  bool has_K = t->HasAttribute("K");
  if (has_K) {
    ASSERT_TRUE(t->GetBytes(&K, "K"));
  }

  WycheproofResult result;
  ASSERT_TRUE(GetWycheproofResult(t, &result));
  bool expect_valid = result.IsValid();

  typename Traits::PrivateKey priv_key;
  CBS cbs;
  CBS_init(&cbs, dk.data(), dk.size());
  if (!bcm_success(Traits::ParsePrivateKey(&priv_key, &cbs)) ||
      CBS_len(&cbs) != 0) {
    EXPECT_FALSE(expect_valid);
    return;
  }

  // Verify that the derived public key matches the expected public key.
  typename Traits::PublicKey pub_key;
  Traits::PublicFromPrivate(&pub_key, &priv_key);
  EXPECT_EQ(Bytes(Marshal(Traits::MarshalPublicKey, &pub_key)), Bytes(ek));

  uint8_t computed_K[MLKEM_SHARED_SECRET_BYTES];
  const int decap_ok = Traits::Decap(computed_K, c.data(), c.size(), &priv_key);
  EXPECT_EQ(decap_ok, expect_valid ? 1 : 0);
  if (!decap_ok) {
    return;
  }

  ASSERT_TRUE(has_K);
  EXPECT_EQ(Bytes(Declassified(computed_K)), Bytes(K));
}

TEST(MLKEMTest, WycheproofSemiExpandedDecap768) {
  FileTestGTest(
      "third_party/wycheproof_testvectors/"
      "mlkem_768_semi_expanded_decaps_test.txt",
      MLKEMWycheproofSemiExpandedDecapTest<MLKEM768Traits>);
}

TEST(MLKEMTest, WycheproofSemiExpandedDecap1024) {
  FileTestGTest(
      "third_party/wycheproof_testvectors/"
      "mlkem_1024_semi_expanded_decaps_test.txt",
      MLKEMWycheproofSemiExpandedDecapTest<MLKEM1024Traits>);
}

TEST(MLKEMTest, Self) { ASSERT_TRUE(boringssl_self_test_mlkem()); }

TEST(MLKEMTest, PWCT) {
  auto pub768 = std::make_unique<uint8_t[]>(MLKEM768_PUBLIC_KEY_BYTES);
  auto priv768 = std::make_unique<MLKEM768_private_key>();
  ASSERT_EQ(
      BCM_mlkem768_generate_key_fips(pub768.get(), nullptr, priv768.get()),
      bcm_status::approved);

  auto pub1024 = std::make_unique<uint8_t[]>(MLKEM1024_PUBLIC_KEY_BYTES);
  auto priv1024 = std::make_unique<MLKEM1024_private_key>();
  ASSERT_EQ(
      BCM_mlkem1024_generate_key_fips(pub1024.get(), nullptr, priv1024.get()),
      bcm_status::approved);
}

TEST(MLKEMTest, NullptrArgumentsToCreate) {
  // For FIPS reasons, this should fail rather than crash.
  ASSERT_EQ(BCM_mlkem768_generate_key_fips(nullptr, nullptr, nullptr),
            bcm_status::failure);
  ASSERT_EQ(BCM_mlkem1024_generate_key_fips(nullptr, nullptr, nullptr),
            bcm_status::failure);
}

}  // namespace
BSSL_NAMESPACE_END
