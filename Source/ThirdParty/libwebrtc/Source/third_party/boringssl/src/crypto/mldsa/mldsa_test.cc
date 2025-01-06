/* Copyright (c) 2024, Google LLC
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

#include <openssl/mldsa.h>

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include <openssl/bytestring.h>
#include <openssl/mem.h>
#include <openssl/span.h>

#include "../test/file_test.h"
#include "../test/test_util.h"
#include "./internal.h"


namespace {

template <typename T>
std::vector<uint8_t> Marshal(int (*marshal_func)(CBB *, const T *),
                             const T *t) {
  bssl::ScopedCBB cbb;
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
  CBS cbs = bssl::MakeConstSpan(encoded_public_key);
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

TEST(MLDSATest, Basic) {
  std::vector<uint8_t> encoded_public_key(MLDSA65_PUBLIC_KEY_BYTES);
  auto priv = std::make_unique<MLDSA65_private_key>();
  uint8_t seed[MLDSA_SEED_BYTES];
  EXPECT_TRUE(
      MLDSA65_generate_key(encoded_public_key.data(), seed, priv.get()));

  std::vector<uint8_t> encoded_signature(MLDSA65_SIGNATURE_BYTES);
  static const uint8_t kMessage[] = {'H', 'e', 'l', 'l', 'o', ' ',
                                     'w', 'o', 'r', 'l', 'd'};
  static const uint8_t kContext[] = {'c', 't', 'x'};
  EXPECT_TRUE(MLDSA65_sign(encoded_signature.data(), priv.get(), kMessage,
                           sizeof(kMessage), kContext, sizeof(kContext)));

  auto pub = std::make_unique<MLDSA65_public_key>();
  CBS cbs = bssl::MakeConstSpan(encoded_public_key);
  ASSERT_TRUE(MLDSA65_parse_public_key(pub.get(), &cbs));

  EXPECT_EQ(MLDSA65_verify(pub.get(), encoded_signature.data(),
                           encoded_signature.size(), kMessage, sizeof(kMessage),
                           kContext, sizeof(kContext)),
            1);

  auto priv2 = std::make_unique<MLDSA65_private_key>();
  EXPECT_TRUE(MLDSA65_private_key_from_seed(priv2.get(), seed, sizeof(seed)));

  EXPECT_EQ(Bytes(Marshal(MLDSA65_marshal_private_key, priv.get())),
            Bytes(Marshal(MLDSA65_marshal_private_key, priv2.get())));
}

TEST(MLDSATest, SignatureIsRandomized) {
  std::vector<uint8_t> encoded_public_key(MLDSA65_PUBLIC_KEY_BYTES);
  auto priv = std::make_unique<MLDSA65_private_key>();
  uint8_t seed[MLDSA_SEED_BYTES];
  EXPECT_TRUE(
      MLDSA65_generate_key(encoded_public_key.data(), seed, priv.get()));

  auto pub = std::make_unique<MLDSA65_public_key>();
  CBS cbs = bssl::MakeConstSpan(encoded_public_key);
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
  CBS cbs = bssl::MakeConstSpan(encoded_public_key)
                .first(MLDSA65_PUBLIC_KEY_BYTES - 1);
  auto parsed_pub = std::make_unique<MLDSA65_public_key>();
  EXPECT_FALSE(MLDSA65_parse_public_key(parsed_pub.get(), &cbs));

  // Public key has the correct length.
  cbs = bssl::MakeConstSpan(encoded_public_key).first(MLDSA65_PUBLIC_KEY_BYTES);
  EXPECT_TRUE(MLDSA65_parse_public_key(parsed_pub.get(), &cbs));

  // Public key is 1 byte too long.
  cbs = bssl::MakeConstSpan(encoded_public_key);
  EXPECT_FALSE(MLDSA65_parse_public_key(parsed_pub.get(), &cbs));
}

TEST(MLDSATest, InvalidPrivateKeyEncodingLength) {
  std::vector<uint8_t> encoded_public_key(MLDSA65_PUBLIC_KEY_BYTES);
  auto priv = std::make_unique<MLDSA65_private_key>();
  uint8_t seed[MLDSA_SEED_BYTES];
  EXPECT_TRUE(
      MLDSA65_generate_key(encoded_public_key.data(), seed, priv.get()));

  CBB cbb;
  std::vector<uint8_t> malformed_private_key(MLDSA65_PRIVATE_KEY_BYTES + 1, 0);
  CBB_init_fixed(&cbb, malformed_private_key.data(), MLDSA65_PRIVATE_KEY_BYTES);
  ASSERT_TRUE(MLDSA65_marshal_private_key(&cbb, priv.get()));

  CBS cbs;
  auto parsed_priv = std::make_unique<MLDSA65_private_key>();

  // Private key is 1 byte too short.
  CBS_init(&cbs, malformed_private_key.data(), MLDSA65_PRIVATE_KEY_BYTES - 1);
  EXPECT_FALSE(MLDSA65_parse_private_key(parsed_priv.get(), &cbs));

  // Private key has the correct length.
  CBS_init(&cbs, malformed_private_key.data(), MLDSA65_PRIVATE_KEY_BYTES);
  EXPECT_TRUE(MLDSA65_parse_private_key(parsed_priv.get(), &cbs));

  // Private key is 1 byte too long.
  CBS_init(&cbs, malformed_private_key.data(), MLDSA65_PRIVATE_KEY_BYTES + 1);
  EXPECT_FALSE(MLDSA65_parse_private_key(parsed_priv.get(), &cbs));
}

static void MLDSASigGenTest(FileTest *t) {
  std::vector<uint8_t> private_key_bytes, msg, expected_signature;
  ASSERT_TRUE(t->GetBytes(&private_key_bytes, "sk"));
  ASSERT_TRUE(t->GetBytes(&msg, "message"));
  ASSERT_TRUE(t->GetBytes(&expected_signature, "signature"));

  auto priv = std::make_unique<MLDSA65_private_key>();
  CBS cbs;
  CBS_init(&cbs, private_key_bytes.data(), private_key_bytes.size());
  EXPECT_TRUE(MLDSA65_parse_private_key(priv.get(), &cbs));

  const uint8_t zero_randomizer[MLDSA_SIGNATURE_RANDOMIZER_BYTES] = {0};
  std::vector<uint8_t> signature(MLDSA65_SIGNATURE_BYTES);
  EXPECT_TRUE(MLDSA65_sign_internal(signature.data(), priv.get(), msg.data(),
                                    msg.size(), nullptr, 0, nullptr, 0,
                                    zero_randomizer));

  EXPECT_EQ(Bytes(signature), Bytes(expected_signature));

  auto pub = std::make_unique<MLDSA65_public_key>();
  ASSERT_TRUE(MLDSA65_public_from_private(pub.get(), priv.get()));
  EXPECT_TRUE(MLDSA65_verify_internal(pub.get(), signature.data(), msg.data(),
                                      msg.size(), nullptr, 0, nullptr, 0));
}

TEST(MLDSATest, SigGenTests) {
  FileTestGTest("crypto/mldsa/mldsa_nist_siggen_tests.txt", MLDSASigGenTest);
}

static void MLDSAKeyGenTest(FileTest *t) {
  std::vector<uint8_t> seed, expected_public_key, expected_private_key;
  ASSERT_TRUE(t->GetBytes(&seed, "seed"));
  ASSERT_TRUE(t->GetBytes(&expected_public_key, "pub"));
  ASSERT_TRUE(t->GetBytes(&expected_private_key, "priv"));

  std::vector<uint8_t> encoded_public_key(MLDSA65_PUBLIC_KEY_BYTES);
  auto priv = std::make_unique<MLDSA65_private_key>();
  ASSERT_TRUE(MLDSA65_generate_key_external_entropy(encoded_public_key.data(),
                                                    priv.get(), seed.data()));

  EXPECT_EQ(Bytes(encoded_public_key), Bytes(expected_public_key));
}

TEST(MLDSATest, KeyGenTests) {
  FileTestGTest("crypto/mldsa/mldsa_nist_keygen_tests.txt", MLDSAKeyGenTest);
}

template <typename PrivateKey, int (*ParsePrivateKey)(PrivateKey *, CBS *),
          size_t SignatureBytes,
          int (*SignInternal)(uint8_t *, const PrivateKey *, const uint8_t *,
                              size_t, const uint8_t *, size_t, const uint8_t *,
                              size_t, const uint8_t *)>
static void MLDSAWycheproofSignTest(FileTest *t) {
  std::vector<uint8_t> private_key_bytes, msg, expected_signature, context;
  ASSERT_TRUE(t->GetInstructionBytes(&private_key_bytes, "privateKey"));
  ASSERT_TRUE(t->GetBytes(&msg, "msg"));
  ASSERT_TRUE(t->GetBytes(&expected_signature, "sig"));
  if (t->HasAttribute("ctx")) {
    t->GetBytes(&context, "ctx");
  }
  std::string result;
  ASSERT_TRUE(t->GetAttribute(&result, "result"));
  t->IgnoreAttribute("flags");

  CBS cbs;
  CBS_init(&cbs, private_key_bytes.data(), private_key_bytes.size());
  auto priv = std::make_unique<PrivateKey>();
  const int priv_ok = ParsePrivateKey(priv.get(), &cbs);

  if (!priv_ok) {
    ASSERT_TRUE(result != "valid");
    return;
  }

  // Unfortunately we need to reimplement the context length check here because
  // we are using the internal function in order to pass in an all-zero
  // randomizer.
  if (context.size() > 255) {
    ASSERT_TRUE(result != "valid");
    return;
  }

  const uint8_t zero_randomizer[MLDSA_SIGNATURE_RANDOMIZER_BYTES] = {0};
  std::vector<uint8_t> signature(SignatureBytes);
  const uint8_t context_prefix[2] = {0, static_cast<uint8_t>(context.size())};
  EXPECT_TRUE(SignInternal(signature.data(), priv.get(), msg.data(), msg.size(),
                           context_prefix, sizeof(context_prefix),
                           context.data(), context.size(), zero_randomizer));

  EXPECT_EQ(Bytes(signature), Bytes(expected_signature));
}

TEST(MLDSATest, WycheproofSignTests65) {
  FileTestGTest(
      "third_party/wycheproof_testvectors/mldsa_65_standard_sign_test.txt",
      MLDSAWycheproofSignTest<MLDSA65_private_key, MLDSA65_parse_private_key,
                              MLDSA65_SIGNATURE_BYTES, MLDSA65_sign_internal>);
}

template <typename PublicKey, int (*ParsePublicKey)(PublicKey *, CBS *),
          int (*Verify)(const PublicKey *, const uint8_t *, size_t,
                        const uint8_t *, size_t, const uint8_t *, size_t)>
static void MLDSAWycheproofVerifyTest(FileTest *t) {
  std::vector<uint8_t> public_key_bytes, msg, signature, context;
  ASSERT_TRUE(t->GetInstructionBytes(&public_key_bytes, "publicKey"));
  ASSERT_TRUE(t->GetBytes(&msg, "msg"));
  ASSERT_TRUE(t->GetBytes(&signature, "sig"));
  if (t->HasAttribute("ctx")) {
    t->GetBytes(&context, "ctx");
  }
  std::string result, flags;
  ASSERT_TRUE(t->GetAttribute(&result, "result"));
  ASSERT_TRUE(t->GetAttribute(&flags, "flags"));

  CBS cbs;
  CBS_init(&cbs, public_key_bytes.data(), public_key_bytes.size());
  auto pub = std::make_unique<PublicKey>();
  const int pub_ok = ParsePublicKey(pub.get(), &cbs);

  if (!pub_ok) {
    EXPECT_EQ(flags, "IncorrectPublicKeyLength");
    return;
  }

  const int sig_ok =
      Verify(pub.get(), signature.data(), signature.size(), msg.data(),
             msg.size(), context.data(), context.size());
  if (!sig_ok) {
    EXPECT_EQ(result, "invalid");
  } else {
    EXPECT_EQ(result, "valid");
  }
}


TEST(MLDSATest, WycheproofVerifyTests65) {
  FileTestGTest(
      "third_party/wycheproof_testvectors/mldsa_65_standard_verify_test.txt",
      MLDSAWycheproofVerifyTest<MLDSA65_public_key, MLDSA65_parse_public_key,
                                MLDSA65_verify>);
}

}  // namespace
