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

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <iterator>
#include <memory>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include <openssl/asn1.h>
#include <openssl/bytestring.h>
#include <openssl/crypto.h>
#include <openssl/digest.h>
#include <openssl/err.h>
#include <openssl/md4.h>
#include <openssl/md5.h>
#include <openssl/nid.h>
#include <openssl/obj.h>
#include <openssl/sha.h>

#include "../internal.h"
#include "../test/test_util.h"


BSSL_NAMESPACE_BEGIN
namespace {

struct MD {
  // name is the name of the digest.
  const char *name;
  // md_func is the digest to test.
  const EVP_MD *(*func)();
  // one_shot_func is the convenience one-shot version of the
  // digest.
  uint8_t *(*one_shot_func)(const uint8_t *, size_t, uint8_t *);
};

const MD md4 = {"MD4", &EVP_md4, nullptr};
const MD md5 = {"MD5", &EVP_md5, &MD5};
const MD sha1 = {"SHA1", &EVP_sha1, &SHA1};
const MD sha224 = {"SHA224", &EVP_sha224, &SHA224};
const MD sha256 = {"SHA256", &EVP_sha256, &SHA256};
const MD sha384 = {"SHA384", &EVP_sha384, &SHA384};
const MD sha512 = {"SHA512", &EVP_sha512, &SHA512};
const MD sha512_256 = {"SHA512-256", &EVP_sha512_256, &SHA512_256};
const MD md5_sha1 = {"MD5-SHA1", &EVP_md5_sha1, nullptr};
const MD blake2b256 = {"BLAKE2b-256", &EVP_blake2b256, nullptr};
const MD sha3_256 = {"SHA3-256", &EVP_sha3_256, nullptr};
const MD sha3_384 = {"SHA3-384", &EVP_sha3_384, nullptr};
const MD sha3_512 = {"SHA3-512", &EVP_sha3_512, nullptr};
const MD shake256 = {"SHAKE256", &EVP_shake256, nullptr};
const MD turboshake256 = {"TurboSHAKE256", &EVP_turboshake256, nullptr};

struct DigestTestVector {
  // md is the digest to test.
  const MD &md;
  // input is a NUL-terminated string to hash.
  const char *input;
  // repeat is the number of times to repeat input.
  size_t repeat;
  // expected_hex is the expected digest in hexadecimal.
  const char *expected_hex;
};

const DigestTestVector kTestVectors[] = {
    // MD4 tests, from RFC 1320. (crypto/md4 does not provide a
    // one-shot MD4 function.)
    {md4, "", 1, "31d6cfe0d16ae931b73c59d7e0c089c0"},
    {md4, "a", 1, "bde52cb31de33e46245e05fbdbd6fb24"},
    {md4, "abc", 1, "a448017aaf21d8525fc10ae87aa6729d"},
    {md4, "message digest", 1, "d9130a8164549fe818874806e1c7014b"},
    {md4, "abcdefghijklmnopqrstuvwxyz", 1, "d79e1c308aa5bbcdeea8ed63df412da9"},
    {md4, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", 1,
     "043f8582f241db351ce627e153e7f0e4"},
    {md4, "1234567890", 8, "e33b4ddc9c38f2199c3e7b164fcc0536"},

    // MD5 tests, from RFC 1321.
    {md5, "", 1, "d41d8cd98f00b204e9800998ecf8427e"},
    {md5, "a", 1, "0cc175b9c0f1b6a831c399e269772661"},
    {md5, "abc", 1, "900150983cd24fb0d6963f7d28e17f72"},
    {md5, "message digest", 1, "f96b697d7cb7938d525a2f31aaf161d0"},
    {md5, "abcdefghijklmnopqrstuvwxyz", 1, "c3fcd3d76192e4007dfb496cca67e13b"},
    {md5, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", 1,
     "d174ab98d277d9f5a5611c2c9f419d9f"},
    {md5, "1234567890", 8, "57edf4a22be3c955ac49da2e2107b67a"},

    // SHA-1 tests, from RFC 3174.
    {sha1, "abc", 1, "a9993e364706816aba3e25717850c26c9cd0d89d"},
    {sha1, "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 1,
     "84983e441c3bd26ebaae4aa1f95129e5e54670f1"},
    {sha1, "a", 1000000, "34aa973cd4c4daa4f61eeb2bdbad27316534016f"},
    {sha1, "0123456701234567012345670123456701234567012345670123456701234567",
     10, "dea356a2cddd90c7a7ecedc5ebb563934f460452"},

    // SHA-224 tests, from RFC 3874.
    {sha224, "abc", 1,
     "23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7"},
    {sha224, "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 1,
     "75388b16512776cc5dba5da1fd890150b0c6455cb4f58b1952522525"},
    {sha224, "a", 1000000,
     "20794655980c91d8bbb4c1ea97618a4bf03f42581948b2ee4ee7ad67"},

    // SHA-256 tests, from NIST.
    {sha256, "abc", 1,
     "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
    {sha256, "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 1,
     "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"},

    // SHA-384 tests, from NIST.
    {sha384, "abc", 1,
     "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed"
     "8086072ba1e7cc2358baeca134c825a7"},
    {sha384,
     "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmn"
     "hijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
     1,
     "09330c33f71147e83d192fc782cd1b4753111b173b3b05d22fa08086e3b0f712"
     "fcc7c71a557e2db966c3e9fa91746039"},

    // SHA-512 tests, from NIST.
    {sha512, "abc", 1,
     "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
     "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f"},
    {sha512,
     "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmn"
     "hijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
     1,
     "8e959b75dae313da8cf4f72814fc143f8f7779c6eb9f7fa17299aeadb6889018"
     "501d289e4900f7e4331b99dec4b5433ac7d329eeb6dd26545e96e55b874be909"},

    // SHA-512-256 tests, from
    // https://csrc.nist.gov/csrc/media/projects/cryptographic-standards-and-guidelines/documents/examples/sha512_256.pdf
    {sha512_256, "abc", 1,
     "53048e2681941ef99b2e29b76b4c7dabe4c2d0c634fc6d46e0e2f13107e7af23"},
    {sha512_256,
     "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopj"
     "klmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
     1, "3928e184fb8690f840da3988121d31be65cb9d3ef83ee6146feac861e19b563a"},

    // MD5-SHA1 tests.
    {md5_sha1, "abc", 1,
     "900150983cd24fb0d6963f7d28e17f72a9993e364706816aba3e25717850c26c9cd0d89"
     "d"},

    // BLAKE2b-256 tests.
    {blake2b256, "abc", 1,
     "bddd813c634239723171ef3fee98579b94964e3bb1cb3e427262c8c068d52319"},

    // SHA3-256 tests, from
    // https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Standards-and-Guidelines/documents/examples/SHA3-256_Msg0.pdf
    // https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Standards-and-Guidelines/documents/examples/SHA3-256_1600.pdf
    {sha3_256, "", 1,
     "a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a"},
    {sha3_256, "\xa3", 200,
     "79f38adec5c20307a98ef76e8324afbfd46cfd81b22e3973c65fa1bd9de31787"},

    // SHA3-384 tests, from
    // https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Standards-and-Guidelines/documents/examples/SHA3-384_Msg0.pdf
    // https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Standards-and-Guidelines/documents/examples/SHA3-384_1600.pdf
    {sha3_384, "", 1,
     "0c63a75b845e4f7d01107d852e4c2485c51a50aaaa94fc61995e71bbee983a2a"
     "c3713831264adb47fb6bd1e058d5f004"},
    {sha3_384, "\xa3", 200,
     "1881de2ca7e41ef95dc4732b8f5f002b189cc1e42b74168ed1732649ce1dbcdd"
     "76197a31fd55ee989f2d7050dd473e8f"},

    // SHA3-512 tests, from
    // https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Standards-and-Guidelines/documents/examples/SHA3-512_Msg0.pdf
    // https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Standards-and-Guidelines/documents/examples/SHA3-512_1600.pdf
    {sha3_512, "", 1,
     "a69f73cca23a9ac5c8b567dc185a756e97c982164fe25859e0d1dcc1475c80a6"
     "15b2123af1f5f94c11e3e9402c3ac558f500199d95b6d3e301758586281dcd26"},
    {sha3_512, "\xa3", 200,
     "e76dfad22084a8b1467fcf2ffa58361bec7628edf5f3fdc0e4805dc48caeeca8"
     "1b7c13c30adf52a3659584739a2df46be589c51ca1a4a8416df6545a1ce8ba00"},

    // SHAKE256 tests, from
    // https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Standards-and-Guidelines/documents/examples/SHAKE256_Msg0.pdf
    // https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Standards-and-Guidelines/documents/examples/SHAKE256_Msg1600.pdf
    {shake256, "", 1,
     "46b9dd2b0ba88d13233b3feb743eeb243fcd52ea62b81b82b50c27646ed5762f"},
    {shake256, "\xa3", 200,
     "cd8a920ed141aa0407a22d59288652e9d9f1a7ee0c1e7c1ca699424da84a904d"},

    // TurboSHAKE256 tests, from
    // https://www.rfc-editor.org/rfc/rfc9861.html#section-5
    {turboshake256, "", 1,
     "367a329dafea871c7802ec67f905ae13c57695dc2c6663c61035f59a18f8e7db"},
};

void CompareDigest(const DigestTestVector *test, const uint8_t *digest,
                   size_t digest_len) {
  EXPECT_EQ(test->expected_hex, EncodeHex(Span(digest, digest_len)));
}

void TestDigest(const DigestTestVector *test) {
  ScopedEVP_MD_CTX ctx;

  // Test the input provided.
  ASSERT_TRUE(EVP_DigestInit_ex(ctx.get(), test->md.func(), nullptr));
  for (size_t i = 0; i < test->repeat; i++) {
    ASSERT_TRUE(EVP_DigestUpdate(ctx.get(), test->input, strlen(test->input)));
  }
  auto digest = std::make_unique<uint8_t[]>(EVP_MD_size(test->md.func()));
  unsigned digest_len;
  ASSERT_TRUE(EVP_DigestFinal_ex(ctx.get(), digest.get(), &digest_len));
  CompareDigest(test, digest.get(), digest_len);

  // Test the input one character at a time.
  ASSERT_TRUE(EVP_DigestInit_ex(ctx.get(), test->md.func(), nullptr));
  ASSERT_TRUE(EVP_DigestUpdate(ctx.get(), nullptr, 0));
  for (size_t i = 0; i < test->repeat; i++) {
    for (const char *p = test->input; *p; p++) {
      ASSERT_TRUE(EVP_DigestUpdate(ctx.get(), p, 1));
    }
  }
  ASSERT_TRUE(EVP_DigestFinal_ex(ctx.get(), digest.get(), &digest_len));
  EXPECT_EQ(EVP_MD_size(test->md.func()), digest_len);
  CompareDigest(test, digest.get(), digest_len);

  // Test with unaligned input.
  ASSERT_TRUE(EVP_DigestInit_ex(ctx.get(), test->md.func(), nullptr));
  std::vector<char> unaligned(strlen(test->input) + 1);
  char *ptr = unaligned.data();
  if ((reinterpret_cast<uintptr_t>(ptr) & 1) == 0) {
    ptr++;
  }
  OPENSSL_memcpy(ptr, test->input, strlen(test->input));
  for (size_t i = 0; i < test->repeat; i++) {
    ASSERT_TRUE(EVP_DigestUpdate(ctx.get(), ptr, strlen(test->input)));
  }
  ASSERT_TRUE(EVP_DigestFinal_ex(ctx.get(), digest.get(), &digest_len));
  CompareDigest(test, digest.get(), digest_len);

  // Make a copy of the digest in the initial state.
  ASSERT_TRUE(EVP_DigestInit_ex(ctx.get(), test->md.func(), nullptr));
  ScopedEVP_MD_CTX copy;
  ASSERT_TRUE(EVP_MD_CTX_copy_ex(copy.get(), ctx.get()));
  for (size_t i = 0; i < test->repeat; i++) {
    ASSERT_TRUE(EVP_DigestUpdate(copy.get(), test->input, strlen(test->input)));
  }
  ASSERT_TRUE(EVP_DigestFinal_ex(copy.get(), digest.get(), &digest_len));
  CompareDigest(test, digest.get(), digest_len);

  // Make a copy of the digest with half the input provided.
  size_t half = strlen(test->input) / 2;
  ASSERT_TRUE(EVP_DigestUpdate(ctx.get(), test->input, half));
  ASSERT_TRUE(EVP_MD_CTX_copy_ex(copy.get(), ctx.get()));
  ASSERT_TRUE(EVP_DigestUpdate(copy.get(), test->input + half,
                               strlen(test->input) - half));
  for (size_t i = 1; i < test->repeat; i++) {
    ASSERT_TRUE(EVP_DigestUpdate(copy.get(), test->input, strlen(test->input)));
  }
  ASSERT_TRUE(EVP_DigestFinal_ex(copy.get(), digest.get(), &digest_len));
  CompareDigest(test, digest.get(), digest_len);

  // Move the digest from the initial state.
  ASSERT_TRUE(EVP_DigestInit_ex(ctx.get(), test->md.func(), nullptr));
  copy = std::move(ctx);
  for (size_t i = 0; i < test->repeat; i++) {
    ASSERT_TRUE(EVP_DigestUpdate(copy.get(), test->input, strlen(test->input)));
  }
  ASSERT_TRUE(EVP_DigestFinal_ex(copy.get(), digest.get(), &digest_len));
  CompareDigest(test, digest.get(), digest_len);

  // Move the digest with half the input provided.
  ASSERT_TRUE(EVP_DigestInit_ex(ctx.get(), test->md.func(), nullptr));
  ASSERT_TRUE(EVP_DigestUpdate(ctx.get(), test->input, half));
  copy = std::move(ctx);
  ASSERT_TRUE(EVP_DigestUpdate(copy.get(), test->input + half,
                               strlen(test->input) - half));
  for (size_t i = 1; i < test->repeat; i++) {
    ASSERT_TRUE(EVP_DigestUpdate(copy.get(), test->input, strlen(test->input)));
  }
  ASSERT_TRUE(EVP_DigestFinal_ex(copy.get(), digest.get(), &digest_len));
  CompareDigest(test, digest.get(), digest_len);

  // Test the one-shot function.
  if (test->md.one_shot_func && test->repeat == 1) {
    uint8_t *out = test->md.one_shot_func((const uint8_t *)test->input,
                                          strlen(test->input), digest.get());
    // One-shot functions return their supplied buffers.
    EXPECT_EQ(digest.get(), out);
    CompareDigest(test, digest.get(), EVP_MD_size(test->md.func()));
  }
}

TEST(DigestTest, TestVectors) {
  for (size_t i = 0; i < std::size(kTestVectors); i++) {
    SCOPED_TRACE(i);
    TestDigest(&kTestVectors[i]);
  }
}

TEST(DigestTest, Getters) {
  EXPECT_EQ(EVP_sha512(), EVP_get_digestbyname("RSA-SHA512"));
  EXPECT_EQ(EVP_sha512(), EVP_get_digestbyname("sha512WithRSAEncryption"));
  EXPECT_EQ(nullptr, EVP_get_digestbyname("nonsense"));
  EXPECT_EQ(EVP_sha512(), EVP_get_digestbyname("SHA512"));
  EXPECT_EQ(EVP_sha512(), EVP_get_digestbyname("sha512"));

  EXPECT_EQ(EVP_sha512(), EVP_get_digestbynid(NID_sha512));
  EXPECT_EQ(nullptr, EVP_get_digestbynid(NID_sha512WithRSAEncryption));
  EXPECT_EQ(nullptr, EVP_get_digestbynid(NID_undef));

  UniquePtr<ASN1_OBJECT> obj(OBJ_txt2obj("1.3.14.3.2.26", 0));
  ASSERT_TRUE(obj);
  EXPECT_EQ(EVP_sha1(), EVP_get_digestbyobj(obj.get()));
  EXPECT_EQ(EVP_md5_sha1(), EVP_get_digestbyobj(OBJ_nid2obj(NID_md5_sha1)));
  EXPECT_EQ(EVP_sha1(), EVP_get_digestbyobj(OBJ_nid2obj(NID_sha1)));
}

TEST(DigestTest, ASN1) {
  ScopedCBB cbb;
  ASSERT_TRUE(CBB_init(cbb.get(), 0));
  EXPECT_FALSE(EVP_marshal_digest_algorithm(cbb.get(), EVP_md5_sha1()));
  EXPECT_TRUE(ErrorsAreAndClear({{ERR_LIB_DIGEST, DIGEST_R_UNKNOWN_HASH}}));

  static const uint8_t kSHA256NullParam[] = {0x30, 0x0d, 0x06, 0x09, 0x60,
                                             0x86, 0x48, 0x01, 0x65, 0x03,
                                             0x04, 0x02, 0x01, 0x05, 0x00};
  static const uint8_t kSHA256NoParam[] = {0x30, 0x0b, 0x06, 0x09, 0x60,
                                           0x86, 0x48, 0x01, 0x65, 0x03,
                                           0x04, 0x02, 0x01};
  static const uint8_t kSHA256GarbageParam[] = {
      0x30, 0x0e, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
      0x65, 0x03, 0x04, 0x02, 0x01, 0x02, 0x01, 0x2a};

  // Serialize SHA-256, with and without NULL.
  cbb.Reset();
  ASSERT_TRUE(CBB_init(cbb.get(), 0));
  ASSERT_TRUE(EVP_marshal_digest_algorithm(cbb.get(), EVP_sha256()));
  EXPECT_EQ(Bytes(kSHA256NullParam),
            Bytes(CBB_data(cbb.get()), CBB_len(cbb.get())));
  cbb.Reset();
  ASSERT_TRUE(CBB_init(cbb.get(), 0));
  ASSERT_TRUE(EVP_marshal_digest_algorithm_no_params(cbb.get(), EVP_sha256()));
  EXPECT_EQ(Bytes(kSHA256NoParam),
            Bytes(CBB_data(cbb.get()), CBB_len(cbb.get())));

  // Parse SHA-256. Either absent or NULL parameters are tolerated for
  // compatibility.
  CBS cbs;
  CBS_init(&cbs, kSHA256NullParam, sizeof(kSHA256NullParam));
  EXPECT_EQ(EVP_sha256(), EVP_parse_digest_algorithm(&cbs));
  EXPECT_EQ(0u, CBS_len(&cbs));
  CBS_init(&cbs, kSHA256NoParam, sizeof(kSHA256NoParam));
  EXPECT_EQ(EVP_sha256(), EVP_parse_digest_algorithm(&cbs));
  EXPECT_EQ(0u, CBS_len(&cbs));

  // Garbage parameters are not.
  CBS_init(&cbs, kSHA256GarbageParam, sizeof(kSHA256GarbageParam));
  EXPECT_FALSE(EVP_parse_digest_algorithm(&cbs));
  EXPECT_TRUE(ErrorsAreAndClear({{ERR_LIB_DIGEST, DIGEST_R_DECODE_ERROR}}));
}

TEST(DigestTest, TransformBlocks) {
  uint8_t blocks[SHA256_CBLOCK * 10];
  for (size_t i = 0; i < sizeof(blocks); i++) {
    blocks[i] = i * 3;
  }

  SHA256_CTX ctx1;
  SHA256_Init(&ctx1);
  SHA256_Update(&ctx1, blocks, sizeof(blocks));

  SHA256_CTX ctx2;
  SHA256_Init(&ctx2);
  SHA256_TransformBlocks(ctx2.h, blocks, sizeof(blocks) / SHA256_CBLOCK);
  SHA256_TransformBlocks(ctx2.h, nullptr, 0);

  EXPECT_TRUE(0 == OPENSSL_memcmp(ctx1.h, ctx2.h, sizeof(ctx1.h)));
}

// |EVP_MD_CTX| is a caller-allocatable C struct. Some APIs are required to work
// when the struct is uninitialized.
TEST(DigestTest, Uninitialized) {
  // |EVP_MD_CTX_init| initializes its input from an arbitrary state.
  {
    EVP_MD_CTX ctx;
    EVP_MD_CTX_init(&ctx);
    EVP_DigestInit_ex(&ctx, EVP_sha256(), nullptr);
    EVP_MD_CTX_cleanup(&ctx);
  }

  // |EVP_DigestInit| internally calls |EVP_MD_CTX_init| and thus initializes it
  // from an arbitrary state.
  {
    EVP_MD_CTX ctx;
    EVP_DigestInit(&ctx, EVP_sha256());
    EVP_MD_CTX_cleanup(&ctx);
  }
}

TEST(DigestTest, XOF) {
  ScopedEVP_MD_CTX ctx;

  // Non-XOF digests should fail `EVP_DigestSqueeze`.
  ASSERT_TRUE(EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr));
  uint8_t out[64];
  EXPECT_FALSE(EVP_DigestSqueeze(ctx.get(), out, sizeof(out)));
  ERR_clear_error();

  // SHAKE256 512-bit test vectors from NIST:
  // https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Standards-and-Guidelines/documents/examples/SHAKE256_Msg0.pdf
  ASSERT_TRUE(EVP_DigestInit_ex(ctx.get(), EVP_shake256(), nullptr));
  ASSERT_TRUE(EVP_DigestSqueeze(ctx.get(), out, sizeof(out)));
  EXPECT_EQ(
      "46b9dd2b0ba88d13233b3feb743eeb243fcd52ea62b81b82b50c27646ed5762f"
      "d75dc4ddd8c0f200cb05019d67b592f6fc821c49479ab48640292eacb3b7c4be",
      EncodeHex(out));

  // https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Standards-and-Guidelines/documents/examples/SHAKE256_Msg1600.pdf
  ASSERT_TRUE(EVP_DigestInit_ex(ctx.get(), EVP_shake256(), nullptr));
  std::vector<uint8_t> input(200, 0xa3);
  ASSERT_TRUE(EVP_DigestUpdate(ctx.get(), input.data(), input.size()));
  ASSERT_TRUE(EVP_DigestSqueeze(ctx.get(), out, sizeof(out)));
  EXPECT_EQ(
      "cd8a920ed141aa0407a22d59288652e9d9f1a7ee0c1e7c1ca699424da84a904d"
      "2d700caae7396ece96604440577da4f3aa22aeb8857f961c4cd8e06f0ae6610b",
      EncodeHex(out));

  // TurboSHAKE256 512-bit test vector from RFC 9861, section 5.
  ASSERT_TRUE(EVP_DigestInit_ex(ctx.get(), EVP_turboshake256(), nullptr));
  ASSERT_TRUE(EVP_DigestSqueeze(ctx.get(), out, sizeof(out)));
  EXPECT_EQ(
      "367a329dafea871c7802ec67f905ae13c57695dc2c6663c61035f59a18f8e7db"
      "11edc0e12e91ea60eb6b32df06dd7f002fbafabb6e13ec1cc20d995547600db0",
      EncodeHex(out));
}

struct XOFTestVector {
  const char *name;
  const EVP_MD *(*md)();
  const std::string_view input_hex;
  const std::string_view expected_hex;
};

class XOFSqueeze : public testing::TestWithParam<XOFTestVector> {};

TEST_P(XOFSqueeze, Squeeze) {
  const auto &test = GetParam();

  ScopedEVP_MD_CTX ctx;
  std::vector<uint8_t> input;
  ASSERT_TRUE(DecodeHex(&input, test.input_hex));
  std::vector<uint8_t> expected;
  ASSERT_TRUE(DecodeHex(&expected, test.expected_hex));
  std::vector<uint8_t> out(expected.size());

  // One shot
  ASSERT_TRUE(EVP_DigestInit_ex(ctx.get(), test.md(), nullptr));
  ASSERT_TRUE(EVP_DigestUpdate(ctx.get(), input.data(), input.size()));
  ASSERT_TRUE(EVP_DigestSqueeze(ctx.get(), out.data(), out.size()));
  EXPECT_EQ(Bytes(out), Bytes(expected));

  // Absorb one at a time
  out.assign(expected.size(), 0);
  ASSERT_TRUE(EVP_DigestInit_ex(ctx.get(), test.md(), nullptr));
  for (const auto &byte : input) {
    ASSERT_TRUE(EVP_DigestUpdate(ctx.get(), &byte, 1));
  }
  ASSERT_TRUE(EVP_DigestSqueeze(ctx.get(), out.data(), out.size()));
  EXPECT_EQ(Bytes(out), Bytes(expected));

  // Squeeze one at a time
  out.assign(expected.size(), 0);
  ASSERT_TRUE(EVP_DigestInit_ex(ctx.get(), test.md(), nullptr));
  ASSERT_TRUE(EVP_DigestUpdate(ctx.get(), input.data(), input.size()));
  for (auto &byte : out) {
    ASSERT_TRUE(EVP_DigestSqueeze(ctx.get(), &byte, 1));
  }
  EXPECT_EQ(Bytes(out), Bytes(expected));
}

const XOFTestVector kSqueezeVectors[] = {
    {"SHAKE256_MSG", EVP_shake256,
     "e4baace59e93f0a59db1e7a9a3e6ba9de6be97e6ada3e8bc89e6a5b5e68192e6"
     "b2b3e6b299e998bfe583a7e7a587e982a3e794b1e4bb96e4b88de58fafe6809d"
     "e8adb0",
     "64feb2a33554eb5f27246625c20969bc2fb62e1f57b4a5426ea38ac9de186e96"
     "e28ec780c3869eca3f43d7d33d46d4582ff57a378733a0af272e7da58ec385c1"
     "93609d5a3a95d07819e3eb868b263321e248ff3bcc9bed506a1e8c507c8f953d"
     "acbe9911c261ab378dd08ccb6a9cee43fdd637a324dc37b8366fa692b9ea0e3e"
     "439bb85202c6b0141c6956604f5758ff3b1cd73977afe98c53d2352d987f2d48"
     "71e02afd919d39b72fba2bae479b8871b75a98c0bf8ec802f29ab88133a8379e"
     "3b8e4a70fd3bf5c4"},
    {"TURBOSHAKE256_MSG", EVP_turboshake256,
     "e58886e58e98e6af9be7b3b8e5bfbde5beaee7b98ae6b299e5a1b5e59f83e6b8"
     "bae6bca0e6a8a1e7b38ae980a1e5b7a1e9a088e887bee79eace681af",
     "3311577e7908dded934b818db25d18b578513e04199071f9b4f7b37761326fab"
     "15edbd103627c7cbcfbd32bba069488d7257f6c652831679ee050d6e0ad7807c"
     "cc746ccd6a3124696bd21d224d3cd588fb7c52637c5ae448a16eb156d5000482"
     "be97e26d6429e252f43e3f18fda98af2ad748f2814e7473b96e2bdc7252027be"
     "3310a49106e5eccedf9d08efc1172f25595a3624addd65c943cab15a5ab7059f"
     "929e6d7f8d3b938da5811ce385268c5a30f35d131fd96db6ab363cd681e1453b"
     "d237e182969d2376"}};
INSTANTIATE_TEST_SUITE_P(All, XOFSqueeze, testing::ValuesIn(kSqueezeVectors),
                         [](const testing::TestParamInfo<XOFTestVector> &v) {
                           return v.param.name;
                         });

}  // namespace
BSSL_NAMESPACE_END
