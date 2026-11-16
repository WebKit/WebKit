#include <doctest/doctest.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sframe/sframe.h>

#include <crypto.h>
#include <header.h>
#include <sframe/result.h>

#include "common.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace SFRAME_NAMESPACE;
using nlohmann::json;

struct HexBytes
{
  bytes data;

  operator input_bytes() const { return data; }
};

static bool
equal_bytes(const HexBytes& hex, input_bytes other)
{
  return hex.data.size() == other.size() &&
         std::equal(hex.data.begin(), hex.data.end(), other.begin());
}

static bool
operator==(const HexBytes& hex, input_bytes other)
{
  return equal_bytes(hex, other);
}

static bool
operator==(input_bytes other, const HexBytes& hex)
{
  return hex == other;
}

static bool
operator==(const HexBytes& hex, output_bytes other)
{
  return equal_bytes(hex, input_bytes(other.data(), other.size()));
}

static bool
operator==(output_bytes other, const HexBytes& hex)
{
  return hex == other;
}

static void
from_json(const json& j, HexBytes& b)
{
  const auto hex = j.get<std::string>();

  if (hex.length() % 2 == 1) {
    throw std::invalid_argument("Odd-length hex string");
  }

  const auto len = hex.length() / 2;
  b.data.resize(len);
  for (size_t i = 0; i < len; i += 1) {
    const std::string byte = hex.substr(2 * i, 2);
    b.data.at(i) = static_cast<uint8_t>(strtol(byte.c_str(), nullptr, 16));
  }
}

static void
to_json(json& /* j */, const HexBytes& /* p */)
{
  // Included just so that macros work
}

struct HeaderTestVector
{
  uint64_t kid;
  uint64_t ctr;
  HexBytes encoded;

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(HeaderTestVector, kid, ctr, encoded)

  void verify() const
  {
    // Decode
    const auto decoded = Header::parse(encoded).unwrap();
    REQUIRE(decoded.key_id == kid);
    REQUIRE(decoded.counter == ctr);
    REQUIRE(decoded.size() == encoded.data.size());
    REQUIRE(decoded.encoded() == encoded);

    // Encode
    const auto to_encode = Header{ kid, ctr };
    REQUIRE(to_encode.encoded() == encoded);
  }
};

struct AesCtrHmacTestVector
{
  CipherSuite cipher_suite;
  HexBytes key;
  HexBytes enc_key;
  HexBytes auth_key;
  HexBytes nonce;
  HexBytes aad;
  HexBytes pt;
  HexBytes ct;

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(AesCtrHmacTestVector,
                                 cipher_suite,
                                 key,
                                 enc_key,
                                 auth_key,
                                 nonce,
                                 aad,
                                 pt,
                                 ct)

  void verify() const
  {
    // Seal
    auto ciphertext = bytes(ct.data.size());
    const auto ct_out =
      seal(cipher_suite, key, nonce, ciphertext, aad, pt).unwrap();
    REQUIRE(ct_out == ct);

    // Open
    auto plaintext = bytes(pt.data.size());
    const auto pt_out =
      open(cipher_suite, key, nonce, plaintext, aad, ct).unwrap();
    REQUIRE(pt_out == pt);
  }
};

struct SFrameTestVector
{
  CipherSuite cipher_suite;
  uint64_t kid;
  uint64_t ctr;
  HexBytes base_key;
  HexBytes sframe_key_label;
  HexBytes sframe_salt_label;
  HexBytes sframe_secret;
  HexBytes metadata;
  HexBytes nonce;
  HexBytes aad;
  HexBytes pt;
  HexBytes ct;

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(SFrameTestVector,
                                 cipher_suite,
                                 kid,
                                 ctr,
                                 base_key,
                                 sframe_key_label,
                                 sframe_salt_label,
                                 sframe_secret,
                                 metadata,
                                 nonce,
                                 aad,
                                 pt,
                                 ct)

  void verify() const
  {
    // Protect
    auto send_ctx = Context(cipher_suite);
    send_ctx.add_key(kid, KeyUsage::protect, base_key).unwrap();

    auto ct_data = owned_bytes<128>();
    auto next_ctr = uint64_t(0);
    while (next_ctr < ctr) {
      send_ctx.protect(kid, ct_data, pt, metadata).unwrap();
      next_ctr += 1;
    }

    const auto ct_out = send_ctx.protect(kid, ct_data, pt, metadata).unwrap();

    const auto act_ct_hex = to_hex(ct_out);
    const auto exp_ct_hex = to_hex(ct);
    REQUIRE(act_ct_hex == exp_ct_hex);

    REQUIRE(ct_out == ct);

    // Unprotect
    auto recv_ctx = Context(cipher_suite);
    recv_ctx.add_key(kid, KeyUsage::unprotect, base_key).unwrap();

    auto pt_data = owned_bytes<128>();
    auto pt_out = recv_ctx.unprotect(pt_data, ct, metadata).unwrap();
    REQUIRE(pt_out == pt);
  }
};

struct TestVectors
{
  std::vector<HeaderTestVector> header;
  std::vector<AesCtrHmacTestVector> aes_ctr_hmac;
  std::vector<SFrameTestVector> sframe;

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(TestVectors, header, aes_ctr_hmac, sframe)
};

struct TestVectorTest
{
  static std::unique_ptr<TestVectors> vectors;

  TestVectorTest()
  {
    if (vectors == nullptr) {
      const auto vectors_json = json::parse(std::ifstream("test-vectors.json"));
      vectors = std::make_unique<TestVectors>(vectors_json.get<TestVectors>());
    }
  }
};

std::unique_ptr<TestVectors> TestVectorTest::vectors = nullptr;

TEST_CASE_FIXTURE(TestVectorTest, "Header Test Vectors")
{
  for (const auto& tv : vectors->header) {
    tv.verify();
  }
}

TEST_CASE_FIXTURE(TestVectorTest, "AES-CTR-HMAC Test Vectors")
{
  for (const auto& tv : vectors->aes_ctr_hmac) {
    tv.verify();
  }
}

TEST_CASE_FIXTURE(TestVectorTest, "SFrame Test Vectors")
{
  for (const auto& tv : vectors->sframe) {
    tv.verify();
  }
}
