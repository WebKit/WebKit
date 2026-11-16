#include <doctest/doctest.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <sframe/sframe.h>

#include <header.h>

#include "common.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <map>       // for map
#include <stdexcept> // for invalid_argument
#include <string>    // for basic_string, operator==
#include <vector>

using namespace SFRAME_NAMESPACE;

TEST_CASE("SFrame Round-Trip")
{
  const auto rounds = 1 << 9;
  const auto kid = KeyID(0x42);
  const auto plaintext = from_hex("00010203");
  const std::map<CipherSuite, bytes> keys{
    { CipherSuite::AES_128_CTR_HMAC_SHA256_80,
      from_hex("000102030405060708090a0b0c0d0e0f") },
    { CipherSuite::AES_128_CTR_HMAC_SHA256_64,
      from_hex("101112131415161718191a1b1c1d1e1f") },
    { CipherSuite::AES_128_CTR_HMAC_SHA256_32,
      from_hex("202122232425262728292a2b2c2d2e2f") },
    { CipherSuite::AES_GCM_128_SHA256,
      from_hex("303132333435363738393a3b3c3d3e3f") },
    { CipherSuite::AES_GCM_256_SHA512,
      from_hex("404142434445464748494a4b4c4d4e4f"
               "505152535455565758595a5b5c5d5e5f") },
    { CipherSuite::AES_256_CTR_HMAC_SHA512_80,
      from_hex("606162636465666768696a6b6c6d6e6f") },
    { CipherSuite::AES_256_CTR_HMAC_SHA512_64,
      from_hex("707172737475767778797a7b7c7d7e7f") },
    { CipherSuite::AES_256_CTR_HMAC_SHA512_32,
      from_hex("808182838485868788898a8b8c8d8e8f") },
  };

  auto pt_out = bytes(plaintext.size());
  auto ct_out = bytes(plaintext.size() + Context::max_overhead);

  for (auto& pair : keys) {
    auto& suite = pair.first;
    auto& key = pair.second;

    auto send = Context(suite);
    send.add_key(kid, KeyUsage::protect, key).unwrap();

    auto recv = Context(suite);
    recv.add_key(kid, KeyUsage::unprotect, key).unwrap();

    for (int i = 0; i < rounds; i++) {
      auto encrypted =
        to_bytes(send.protect(kid, ct_out, plaintext, {}).unwrap());
      auto decrypted = to_bytes(recv.unprotect(pt_out, encrypted, {}).unwrap());
      CHECK(decrypted == plaintext);
    }
  }
}

// The MLS-based key derivation isn't covered by the RFC test vectors.  So we
// only have round-trip tests, not known-answer tests.
TEST_CASE("MLS Round-Trip")
{
  const auto epoch_bits = 2;
  const auto test_epochs = 1 << (epoch_bits + 1);
  const auto epoch_rounds = 10;
  const auto metadata = from_hex("00010203");
  const auto plaintext = from_hex("04050607");
  const auto sender_id_a = MLSContext::SenderID(0xA0A0A0A0);
  const auto sender_id_b = MLSContext::SenderID(0xA1A1A1A1);
  const std::vector<CipherSuite> suites{
    CipherSuite::AES_128_CTR_HMAC_SHA256_80,
    CipherSuite::AES_128_CTR_HMAC_SHA256_64,
    CipherSuite::AES_128_CTR_HMAC_SHA256_32,
    CipherSuite::AES_GCM_128_SHA256,
    CipherSuite::AES_GCM_256_SHA512,
    CipherSuite::AES_256_CTR_HMAC_SHA512_80,
    CipherSuite::AES_256_CTR_HMAC_SHA512_64,
    CipherSuite::AES_256_CTR_HMAC_SHA512_32,
  };

  auto pt_out = bytes(plaintext.size());
  auto ct_out = bytes(plaintext.size() + Context::max_overhead);

  for (auto& suite : suites) {
    auto member_a = MLSContext(suite, epoch_bits);
    auto member_b = MLSContext(suite, epoch_bits);

    for (MLSContext::EpochID epoch_id = 0; epoch_id < test_epochs; epoch_id++) {
      const auto sframe_epoch_secret = bytes(8, uint8_t(epoch_id));

      member_a.add_epoch(epoch_id, sframe_epoch_secret).unwrap();
      member_b.add_epoch(epoch_id, sframe_epoch_secret).unwrap();

      for (int i = 0; i < epoch_rounds; i++) {
        auto encrypted_ab =
          member_a.protect(epoch_id, sender_id_a, ct_out, plaintext, metadata)
            .unwrap();
        auto decrypted_ab =
          member_b.unprotect(pt_out, encrypted_ab, metadata).unwrap();
        CHECK(plaintext == to_bytes(decrypted_ab));

        auto encrypted_ba =
          member_b.protect(epoch_id, sender_id_b, ct_out, plaintext, metadata)
            .unwrap();
        auto decrypted_ba =
          member_a.unprotect(pt_out, encrypted_ba, metadata).unwrap();
        CHECK(plaintext == to_bytes(decrypted_ba));
      }
    }
  }
}

TEST_CASE("MLS Round-Trip with context")
{
  const auto epoch_bits = 4;
  const auto test_epochs = 1 << (epoch_bits + 1);
  const auto epoch_rounds = 10;
  const auto metadata = from_hex("00010203");
  const auto plaintext = from_hex("04050607");
  const auto sender_id_a = MLSContext::SenderID(0xA0A0A0A0);
  const auto sender_id_b = MLSContext::SenderID(0xA1A1A1A1);
  const auto sender_id_bits = size_t(32);
  const auto context_id_0 = 0xB0B0;
  const auto context_id_1 = 0xB1B1;

  const std::vector<CipherSuite> suites{
    CipherSuite::AES_128_CTR_HMAC_SHA256_80,
    CipherSuite::AES_128_CTR_HMAC_SHA256_64,
    CipherSuite::AES_128_CTR_HMAC_SHA256_32,
    CipherSuite::AES_GCM_128_SHA256,
    CipherSuite::AES_GCM_256_SHA512,
    CipherSuite::AES_256_CTR_HMAC_SHA512_80,
    CipherSuite::AES_256_CTR_HMAC_SHA512_64,
    CipherSuite::AES_256_CTR_HMAC_SHA512_32,
  };

  auto pt_out = bytes(plaintext.size());
  auto ct_out_1 = bytes(plaintext.size() + Context::max_overhead);
  auto ct_out_0 = bytes(plaintext.size() + Context::max_overhead);

  for (auto& suite : suites) {
    auto member_a_0 = MLSContext(suite, epoch_bits);
    auto member_a_1 = MLSContext(suite, epoch_bits);
    auto member_b = MLSContext(suite, epoch_bits);

    for (MLSContext::EpochID epoch_id = 0; epoch_id < test_epochs; epoch_id++) {
      const auto sframe_epoch_secret = bytes(8, uint8_t(epoch_id));

      member_a_0.add_epoch(epoch_id, sframe_epoch_secret, sender_id_bits)
        .unwrap();
      member_a_1.add_epoch(epoch_id, sframe_epoch_secret, sender_id_bits)
        .unwrap();
      member_b.add_epoch(epoch_id, sframe_epoch_secret).unwrap();

      for (int i = 0; i < epoch_rounds; i++) {
        auto encrypted_ab_0 = member_a_0
                                .protect(epoch_id,
                                         sender_id_a,
                                         context_id_0,
                                         ct_out_0,
                                         plaintext,
                                         metadata)
                                .unwrap();
        auto decrypted_ab_0 = to_bytes(
          member_b.unprotect(pt_out, encrypted_ab_0, metadata).unwrap());
        CHECK(plaintext == decrypted_ab_0);

        auto encrypted_ab_1 = member_a_1
                                .protect(epoch_id,
                                         sender_id_a,
                                         context_id_1,
                                         ct_out_1,
                                         plaintext,
                                         metadata)
                                .unwrap();
        auto decrypted_ab_1 = to_bytes(
          member_b.unprotect(pt_out, encrypted_ab_1, metadata).unwrap());
        CHECK(plaintext == decrypted_ab_1);

        CHECK(to_bytes(encrypted_ab_0) != to_bytes(encrypted_ab_1));

        auto encrypted_ba =
          member_b.protect(epoch_id, sender_id_b, ct_out_0, plaintext, metadata)
            .unwrap();
        auto decrypted_ba_0 = to_bytes(
          member_a_0.unprotect(pt_out, encrypted_ba, metadata).unwrap());
        auto decrypted_ba_1 = to_bytes(
          member_a_1.unprotect(pt_out, encrypted_ba, metadata).unwrap());
        CHECK(plaintext == decrypted_ba_0);
        CHECK(plaintext == decrypted_ba_1);
      }
    }
  }
}

TEST_CASE("MLS Failure after Purge")
{
  const auto suite = CipherSuite::AES_GCM_128_SHA256;
  const auto epoch_bits = 2;
  const auto metadata = from_hex("00010203");
  const auto plaintext = from_hex("04050607");
  const auto sender_id_a = MLSContext::SenderID(0xA0A0A0A0);
  const auto sframe_epoch_secret_1 = bytes(32, 1);
  const auto sframe_epoch_secret_2 = bytes(32, 2);

  auto pt_out = bytes(plaintext.size());
  auto ct_out = bytes(plaintext.size() + Context::max_overhead);

  auto member_a = MLSContext(suite, epoch_bits);
  auto member_b = MLSContext(suite, epoch_bits);

  // Install epoch 1 and create a cipihertext
  const auto epoch_id_1 = MLSContext::EpochID(1);
  member_a.add_epoch(epoch_id_1, sframe_epoch_secret_1).unwrap();
  member_b.add_epoch(epoch_id_1, sframe_epoch_secret_1).unwrap();

  const auto enc_ab_1 =
    member_a.protect(epoch_id_1, sender_id_a, ct_out, plaintext, metadata)
      .unwrap();
  const auto enc_ab_1_data = to_bytes(enc_ab_1);

  // Install epoch 2
  const auto epoch_id_2 = MLSContext::EpochID(2);
  member_a.add_epoch(epoch_id_2, sframe_epoch_secret_2).unwrap();
  member_b.add_epoch(epoch_id_2, sframe_epoch_secret_2).unwrap();

  // Purge epoch 1 and verify failure
  member_a.purge_before(epoch_id_2);
  member_b.purge_before(epoch_id_2);

  CHECK(member_a.protect(epoch_id_1, sender_id_a, ct_out, plaintext, metadata)
          .error()
          .type() == SFrameErrorType::invalid_parameter_error);
  CHECK(member_b.unprotect(pt_out, enc_ab_1_data, metadata).error().type() ==
        SFrameErrorType::unknown_key_id_error);

  const auto enc_ab_2 =
    member_a.protect(epoch_id_2, sender_id_a, ct_out, plaintext, metadata)
      .unwrap();
  const auto dec_ab_2 = member_b.unprotect(pt_out, enc_ab_2, metadata).unwrap();
  CHECK(plaintext == to_bytes(dec_ab_2));
}

TEST_CASE("SFrame Context Remove Key")
{
  const auto suite = CipherSuite::AES_GCM_128_SHA256;
  const auto kid = KeyID(0x07);
  const auto key = from_hex("000102030405060708090a0b0c0d0e0f");
  const auto plaintext = from_hex("00010203");
  const auto metadata = bytes{};

  auto pt_out = bytes(plaintext.size());
  auto ct_out = bytes(plaintext.size() + Context::max_overhead);

  auto sender = Context(suite);
  auto receiver = Context(suite);
  sender.add_key(kid, KeyUsage::protect, key).unwrap();
  receiver.add_key(kid, KeyUsage::unprotect, key).unwrap();

  // Protect and unprotect succeed before removal
  auto encrypted =
    to_bytes(sender.protect(kid, ct_out, plaintext, metadata).unwrap());
  auto decrypted =
    to_bytes(receiver.unprotect(pt_out, encrypted, metadata).unwrap());
  CHECK(decrypted == plaintext);

  // Remove sender key and verify protect fails
  sender.remove_key(kid);
  CHECK(sender.protect(kid, ct_out, plaintext, metadata).error().type() ==
        SFrameErrorType::unknown_key_id_error);

  // Remove receiver key and verify unprotect fails
  receiver.remove_key(kid);
  CHECK(receiver.unprotect(pt_out, encrypted, metadata).error().type() ==
        SFrameErrorType::unknown_key_id_error);

  // Re-add keys and verify round-trip works again
  sender.add_key(kid, KeyUsage::protect, key).unwrap();
  receiver.add_key(kid, KeyUsage::unprotect, key).unwrap();

  encrypted =
    to_bytes(sender.protect(kid, ct_out, plaintext, metadata).unwrap());
  decrypted =
    to_bytes(receiver.unprotect(pt_out, encrypted, metadata).unwrap());
  CHECK(decrypted == plaintext);
}

TEST_CASE("SFrame Context Remove Key - Nonexistent Key")
{
  const auto suite = CipherSuite::AES_GCM_128_SHA256;

  auto ctx = Context(suite);

  // Removing a key that was never added should not throw
  CHECK_NOTHROW(ctx.remove_key(KeyID(0x99)));
}

TEST_CASE("SFrame Unknown Key")
{
  const auto suite = CipherSuite::AES_GCM_128_SHA256;
  const auto kid = KeyID(0x42);
  const auto unknown_kid = KeyID(0x43);
  const auto key = from_hex("000102030405060708090a0b0c0d0e0f");
  const auto plaintext = from_hex("00010203");
  const auto metadata = bytes{};

  auto pt_out = bytes(plaintext.size());
  auto ct_out = bytes(plaintext.size() + Context::max_overhead);

  auto sender = Context(suite);
  sender.add_key(kid, KeyUsage::protect, key).unwrap();

  // Protecting with a key ID that was never added fails with
  // unknown_key_id_error
  CHECK(
    sender.protect(unknown_kid, ct_out, plaintext, metadata).error().type() ==
    SFrameErrorType::unknown_key_id_error);

  // Produce a valid ciphertext whose header references `kid`
  auto encrypted =
    to_bytes(sender.protect(kid, ct_out, plaintext, metadata).unwrap());

  // A receiver that doesn't know `kid` fails to unprotect with
  // unknown_key_id_error
  auto receiver = Context(suite);
  CHECK(receiver.unprotect(pt_out, encrypted, metadata).error().type() ==
        SFrameErrorType::unknown_key_id_error);
}

TEST_CASE("SFrame Unknown Key Error Reports Key ID")
{
  const auto suite = CipherSuite::AES_GCM_128_SHA256;
  const auto kid = KeyID(0x42);
  const auto unknown_kid = KeyID(0x43);
  const auto key = from_hex("000102030405060708090a0b0c0d0e0f");
  const auto plaintext = from_hex("00010203");
  const auto metadata = bytes{};

  auto pt_out = bytes(plaintext.size());
  auto ct_out = bytes(plaintext.size() + Context::max_overhead);

  auto sender = Context(suite);
  sender.add_key(kid, KeyUsage::protect, key).unwrap();

  // Protecting with a key ID never added: error must name that key ID
  auto protect_err =
    sender.protect(unknown_kid, ct_out, plaintext, metadata).error();
  CHECK(protect_err.type() == SFrameErrorType::unknown_key_id_error);
  CHECK(protect_err.key_id().has_value());
  CHECK(protect_err.key_id().value() == unknown_kid);

  // Produce a ciphertext whose header embeds kid
  auto encrypted =
    to_bytes(sender.protect(kid, ct_out, plaintext, metadata).unwrap());

  // Receiver with no keys: error must name the key ID parsed from the
  // ciphertext
  auto receiver = Context(suite);
  auto unprotect_err = receiver.unprotect(pt_out, encrypted, metadata).error();
  CHECK(unprotect_err.type() == SFrameErrorType::unknown_key_id_error);
  CHECK(unprotect_err.key_id().has_value());
  CHECK(unprotect_err.key_id().value() == kid);
}

TEST_CASE("MLS Remove Epoch")
{
  const auto suite = CipherSuite::AES_GCM_128_SHA256;
  const auto epoch_bits = 2;
  const auto metadata = from_hex("00010203");
  const auto plaintext = from_hex("04050607");
  const auto sender_id = MLSContext::SenderID(0xA0A0A0A0);
  const auto sframe_epoch_secret_1 = bytes(32, 1);
  const auto sframe_epoch_secret_2 = bytes(32, 2);

  auto pt_out = bytes(plaintext.size());
  auto ct_out = bytes(plaintext.size() + Context::max_overhead);

  auto member_a = MLSContext(suite, epoch_bits);
  auto member_b = MLSContext(suite, epoch_bits);

  // Install epoch 1 and verify round-trip
  const auto epoch_id_1 = MLSContext::EpochID(1);
  member_a.add_epoch(epoch_id_1, sframe_epoch_secret_1);
  member_b.add_epoch(epoch_id_1, sframe_epoch_secret_1);

  auto enc =
    member_a.protect(epoch_id_1, sender_id, ct_out, plaintext, metadata)
      .unwrap();
  auto enc_data = to_bytes(enc);
  auto dec = to_bytes(member_b.unprotect(pt_out, enc_data, metadata).unwrap());
  CHECK(plaintext == dec);

  // Install epoch 2
  const auto epoch_id_2 = MLSContext::EpochID(2);
  member_a.add_epoch(epoch_id_2, sframe_epoch_secret_2);
  member_b.add_epoch(epoch_id_2, sframe_epoch_secret_2);

  // Remove only epoch 1 (not purge_before) and verify it fails
  member_a.remove_epoch(epoch_id_1);
  member_b.remove_epoch(epoch_id_1);

  CHECK(member_a.protect(epoch_id_1, sender_id, ct_out, plaintext, metadata)
          .error()
          .type() == SFrameErrorType::invalid_parameter_error);
  CHECK(member_b.unprotect(pt_out, enc_data, metadata).error().type() ==
        SFrameErrorType::unknown_key_id_error);

  // Epoch 2 should still work
  enc = member_a.protect(epoch_id_2, sender_id, ct_out, plaintext, metadata)
          .unwrap();
  dec = to_bytes(member_b.unprotect(pt_out, enc, metadata).unwrap());
  CHECK(plaintext == dec);

  // Re-add epoch 1 with the same secret and verify it works again
  member_a.add_epoch(epoch_id_1, sframe_epoch_secret_1);
  member_b.add_epoch(epoch_id_1, sframe_epoch_secret_1);

  enc = member_a.protect(epoch_id_1, sender_id, ct_out, plaintext, metadata)
          .unwrap();
  dec = to_bytes(member_b.unprotect(pt_out, enc, metadata).unwrap());
  CHECK(plaintext == dec);
}

TEST_CASE("SFrame Tamper Detection")
{
  const auto kid = KeyID(0x42);
  const auto base_key = from_hex("000102030405060708090a0b0c0d0e0f"
                                 "101112131415161718191a1b1c1d1e1f");
  const auto plaintext = from_hex("00010203");
  const auto metadata = from_hex("0405060708");
  const std::vector<CipherSuite> suites{
    CipherSuite::AES_128_CTR_HMAC_SHA256_80,
    CipherSuite::AES_128_CTR_HMAC_SHA256_64,
    CipherSuite::AES_128_CTR_HMAC_SHA256_32,
    CipherSuite::AES_GCM_128_SHA256,
    CipherSuite::AES_GCM_256_SHA512,
    CipherSuite::AES_256_CTR_HMAC_SHA512_80,
    CipherSuite::AES_256_CTR_HMAC_SHA512_64,
    CipherSuite::AES_256_CTR_HMAC_SHA512_32,
  };

  auto pt_out = bytes(plaintext.size());
  auto ct_out = bytes(plaintext.size() + Context::max_overhead);

  for (const auto& suite : suites) {
    auto send = Context(suite);
    send.add_key(kid, KeyUsage::protect, base_key).unwrap();

    auto recv = Context(suite);
    recv.add_key(kid, KeyUsage::unprotect, base_key).unwrap();

    const auto encrypted =
      to_bytes(send.protect(kid, ct_out, plaintext, metadata).unwrap());
    CHECK(to_bytes(recv.unprotect(pt_out, encrypted, metadata).unwrap()) ==
          plaintext);

    const auto header_size = Header::parse(encrypted).unwrap().size();

    // A bit flipped in the authentication tag is detected.  This is the case
    // that a round-trip test cannot catch if the tag is truncated to the wrong
    // length, since both sides would truncate identically.
    auto bad_tag = encrypted;
    bad_tag.back() ^= 0x01;
    CHECK(recv.unprotect(pt_out, bad_tag, metadata).error().type() ==
          SFrameErrorType::authentication_error);

    auto bad_body = encrypted;
    bad_body.at(header_size) ^= 0x01;
    CHECK(recv.unprotect(pt_out, bad_body, metadata).error().type() ==
          SFrameErrorType::authentication_error);

    // Metadata is authenticated as part of the AAD, but not transmitted
    auto bad_metadata = metadata;
    bad_metadata.back() ^= 0x01;
    CHECK(recv.unprotect(pt_out, encrypted, bad_metadata).error().type() ==
          SFrameErrorType::authentication_error);

    auto truncated = encrypted;
    truncated.pop_back();
    CHECK(recv.unprotect(pt_out, truncated, metadata).error().type() ==
          SFrameErrorType::authentication_error);
  }
}