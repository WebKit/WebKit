// Copyright 2023 The BoringSSL Authors
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

#include <string.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <openssl/pki/verify.h>
#include <openssl/pki/verify_error.h>
#include <openssl/pool.h>
#include <openssl/sha2.h>

#include "encode_values.h"
#include "merkle_tree.h"
#include "parse_certificate.h"
#include "parsed_certificate.h"
#include "string_util.h"
#include "test_helpers.h"
#include "trust_store.h"
#include "trust_store_in_memory.h"

BSSL_NAMESPACE_BEGIN

static std::unique_ptr<VerifyTrustStore> MozillaRootStore() {
  std::string diagnostic;
  return VerifyTrustStore::FromDER(
             bssl::ReadTestFileToString(
                 "testdata/verify_unittest/mozilla_roots.der"),
             &diagnostic);
}

using ::testing::UnorderedElementsAre;

static std::string GetTestdata(std::string_view filename) {
  return bssl::ReadTestFileToString("testdata/verify_unittest/" +
                                    std::string(filename));
}

static ::testing::AssertionResult ReadTestCertPem(const std::string &file_name,
                                                  std::string *out_cert) {
  PemBlockMapping mappings[] = {
      {"CERTIFICATE", out_cert},
  };
  return ReadTestDataFromPemFile("testdata/verify_unittest/" + file_name,
                                 mappings);
}

TEST(VerifyTest, GoogleChain) {
  const std::string leaf = GetTestdata("google-leaf.der");
  const std::string intermediate1 = GetTestdata("google-intermediate1.der");
  const std::string intermediate2 = GetTestdata("google-intermediate2.der");
  CertificateVerifyOptions opts;
  opts.leaf_cert = leaf;
  opts.intermediates = {intermediate1, intermediate2};
  opts.time = 1499727444;
  std::unique_ptr<VerifyTrustStore> roots = MozillaRootStore();
  opts.trust_store = roots.get();

  VerifyError error;
  ASSERT_TRUE(CertificateVerify(opts, &error)) << error.DiagnosticString();

  opts.intermediates = {};
  EXPECT_FALSE(CertificateVerify(opts, &error));
  ASSERT_EQ(error.Code(), VerifyError::StatusCode::PATH_NOT_FOUND)
      << error.DiagnosticString();
}


TEST(VerifyTest, ExtraIntermediates) {
  const std::string leaf = GetTestdata("google-leaf.der");
  const std::string intermediate1 = GetTestdata("google-intermediate1.der");
  const std::string intermediate2 = GetTestdata("google-intermediate2.der");

  CertificateVerifyOptions opts;
  opts.leaf_cert = leaf;
  std::string diagnostic;
  const auto cert_pool_status = CertPool::FromCerts(
      {
          intermediate1,
          intermediate2,
      },
      &diagnostic);
  ASSERT_TRUE(cert_pool_status) << diagnostic;
  opts.extra_intermediates = cert_pool_status.get();
  opts.time = 1499727444;
  std::unique_ptr<VerifyTrustStore> roots = MozillaRootStore();
  opts.trust_store = roots.get();

  VerifyError error;
  ASSERT_TRUE(CertificateVerify(opts, &error)) << error.DiagnosticString();
}

TEST(VerifyTest, AllPaths) {
  const std::string leaf = GetTestdata("lencr-leaf.der");
  const std::string intermediate1 = GetTestdata("lencr-intermediate-r3.der");
  const std::string intermediate2 =
      GetTestdata("lencr-root-x1-cross-signed.der");
  const std::string root1 = GetTestdata("lencr-root-x1.der");
  const std::string root2 = GetTestdata("lencr-root-dst-x3.der");

  std::vector<std::string> expected_path1 = {leaf, intermediate1, root1};
  std::vector<std::string> expected_path2 = {leaf, intermediate1, intermediate2,
                                             root2};

  CertificateVerifyOptions opts;
  opts.leaf_cert = leaf;
  opts.intermediates = {intermediate1, intermediate2};
  opts.time = 1699404611;
  std::unique_ptr<VerifyTrustStore> roots = MozillaRootStore();
  opts.trust_store = roots.get();

  auto paths = CertificateVerifyAllPaths(opts);
  ASSERT_TRUE(paths);
  EXPECT_EQ(2U, paths.value().size());
  EXPECT_THAT(paths.value(),
              UnorderedElementsAre(expected_path1, expected_path2));
}

TEST(VerifyTest, DepthLimit) {
  const std::string leaf = GetTestdata("google-leaf.der");
  const std::string intermediate1 = GetTestdata("google-intermediate1.der");
  const std::string intermediate2 = GetTestdata("google-intermediate2.der");
  CertificateVerifyOptions opts;
  opts.leaf_cert = leaf;
  opts.intermediates = {intermediate1, intermediate2};
  opts.time = 1499727444;
  // Set the `max_path_building_depth` explicitly to test the non-default case.
  // Depth of 5 is enough to successfully find a path.
  opts.max_path_building_depth = 5;
  std::unique_ptr<VerifyTrustStore> roots = MozillaRootStore();
  opts.trust_store = roots.get();

  VerifyError error;
  ASSERT_TRUE(CertificateVerify(opts, &error)) << error.DiagnosticString();

  // Depth of 2 is not enough to find a path.
  opts.max_path_building_depth = 2;
  EXPECT_FALSE(CertificateVerify(opts, &error));
  ASSERT_EQ(error.Code(), VerifyError::StatusCode::PATH_DEPTH_LIMIT_REACHED)
      << error.DiagnosticString();
}

TEST(VerifyTest, MldsaAlgorithms) {
  std::string root, intermediate, leaf;
  ASSERT_TRUE(ReadTestCertPem("mldsa-root.pem", &root));
  ASSERT_TRUE(ReadTestCertPem("mldsa-intermediate.pem", &intermediate));
  ASSERT_TRUE(ReadTestCertPem("mldsa-leaf.pem", &leaf));

  std::string diagnostic;
  std::unique_ptr<VerifyTrustStore> trust_store =
      VerifyTrustStore::FromDER(root, &diagnostic);
  ASSERT_TRUE(trust_store) << diagnostic;

  CertificateVerifyOptions opts;
  opts.leaf_cert = leaf;
  opts.intermediates = {intermediate};
  opts.trust_store = trust_store.get();
  // April 6, 2026 (00:00Z) is the time used to generate the test certs; use
  // that time for verification.
  opts.time = 1775458800;

  VerifyError error;
  ASSERT_TRUE(CertificateVerify(opts, &error)) << error.DiagnosticString();
}

class VerifyMTCTest : public ::testing::Test {
 public:
  VerifyMTCTest() = default;

  void SetUp() override {
    ASSERT_TRUE(ReadTestCertPem("mtc-leaf.pem", &generic_cert_));
    ASSERT_TRUE(ReadTestCertPem("mtc-leaf-bitflip.pem", &bitflip_cert_));
    ASSERT_TRUE(ReadTestCertPem("mtc-leaf-unused-bit.pem", &unused_bit_cert_));
    ASSERT_TRUE(ReadTestCertPem("mtc-leaf-b.pem", &leaf_b_));
    ASSERT_TRUE(ReadTestCertPem("mtc-leaf-c.pem", &leaf_c_));

    ASSERT_TRUE(
        CreateTrustedSubtree("Rrynt7BBSfI4WMZ1u1+XOJSNaWnYOdUDjn7VbdF+kQY=", 8,
                             13, &generic_cert_subtree_));
    ASSERT_TRUE(
        CreateTrustedSubtree("S92aoQXoNnSPJ37X1zY5InskJPTpzUs6LRr3TOwInvo=", 8,
                             16, &leaf_b_subtree_));
    ASSERT_TRUE(
        CreateTrustedSubtree("FxyVwc4letskl3WVKXWqlPBvUZsl5NiD5sW7Wr50k+4=", 16,
                             24, &leaf_c_subtree_));
  }

  bool CreateTrustedSubtree(const std::string &hash_b64, uint64_t start,
                            uint64_t end, TrustedSubtree *out_subtree) const {
    std::string subtree_hash;
    if (!string_util::Base64Decode(hash_b64, &subtree_hash)) {
      return false;
    }
    if (subtree_hash.size() != out_subtree->hash.size()) {
      return false;
    }
    memcpy(out_subtree->hash.data(), subtree_hash.data(), subtree_hash.size());
    out_subtree->range = Subtree{start, end};
    return true;
  }

  std::shared_ptr<const ParsedCertificate> CertFromString(
      const std::string &cert) const {
    UniquePtr<CRYPTO_BUFFER> cert_buf(CRYPTO_BUFFER_new(
        reinterpret_cast<const uint8_t *>(cert.data()), cert.size(), nullptr));
    return ParsedCertificate::Create(std::move(cert_buf),
                                     ParseCertificateOptions{}, nullptr);
  }

  bool PrepareOptsForVerify(const std::string &cert,
                            const VerifyTrustStore *trust_store,
                            CertificateVerifyOptions *out_opts) const {
    out_opts->leaf_cert = cert;
    std::shared_ptr<const ParsedCertificate> parsed_cert = CertFromString(cert);
    if (!parsed_cert) {
      return false;
    }
    // out_opts->time is a std::optional<int64_t>. If we write directly to
    // *out_opts->time, the std::optional will still be a std::nullopt.
    int64_t time;
    if (!der::GeneralizedTimeToPosixTime(parsed_cert->tbs().validity_not_before,
                                         &time)) {
      return false;
    }
    out_opts->time = time;
    out_opts->trust_store = trust_store;
    return true;
  }

  std::unique_ptr<VerifyTrustStore> EmptyTrustStore() const {
    return VerifyTrustStore::FromDER("", nullptr);
  }

 protected:
  std::string generic_cert_;
  std::string bitflip_cert_;
  std::string unused_bit_cert_;
  std::string leaf_b_;
  std::string leaf_c_;

  // the trusted subtree for [8, 13) that is used in `generic_cert_`,
  // `bitflip_cert_`.
  TrustedSubtree generic_cert_subtree_;
  // Subtree for `leaf_b_` which overlaps with `generic_cert_subtree_`.
  TrustedSubtree leaf_b_subtree_;
  // Subtree for `leaf_c_` which does not overlap with any other subtrees.
  TrustedSubtree leaf_c_subtree_;

  // Relative OID encoding of 32473.1, the log ID used for the MTC Anchor that
  // issued the test MTCs in this test fixture.
  static constexpr uint8_t kAnchorLogId[] = {0x81, 0xfd, 0x59, 0x01};
  static constexpr uint8_t kAnchorLogIdBitflip[] = {0x81, 0xfd, 0x59, 0x00};
};

TEST_F(VerifyMTCTest, SignaturelessMTC) {
  // Configure the trust store to trust the MTC anchor with the landmark subtree
  // for `generic_cert_`.
  std::unique_ptr<VerifyTrustStore> trust_store = EmptyTrustStore();
  std::vector<TrustedSubtree> trusted_subtrees = {generic_cert_subtree_};
  auto mtc_anchor = std::make_shared<MTCAnchor>(MakeSpan(kAnchorLogId),
                                                MakeSpan(trusted_subtrees));
  ASSERT_TRUE(trust_store->trust_store->AddMTCTrustAnchor(mtc_anchor));

  CertificateVerifyOptions opts;
  VerifyError error;
  ASSERT_TRUE(PrepareOptsForVerify(generic_cert_, trust_store.get(), &opts));
  EXPECT_TRUE(CertificateVerify(opts, &error)) << error.DiagnosticString();
}

TEST_F(VerifyMTCTest, ExplicitlyTrustedLeaf) {
  // Configure the trust store to directly trust the `generic_cert_` leaf.
  std::unique_ptr<VerifyTrustStore> trust_store = EmptyTrustStore();
  trust_store->trust_store->AddCertificate(CertFromString(generic_cert_),
                                           CertificateTrust::ForTrustedLeaf());

  CertificateVerifyOptions opts;
  VerifyError error;
  ASSERT_TRUE(PrepareOptsForVerify(generic_cert_, trust_store.get(), &opts));
  EXPECT_TRUE(CertificateVerify(opts, &error)) << error.DiagnosticString();
}

TEST_F(VerifyMTCTest, ExplicitlyDistrustedLeaf) {
  // Configure the trust store to trust the MTC anchor for `generic_cert_`, but
  // also to explicitly distrust that leaf.
  std::unique_ptr<VerifyTrustStore> trust_store = EmptyTrustStore();
  trust_store->trust_store->AddCertificate(CertFromString(generic_cert_),
                                           CertificateTrust::ForDistrusted());
  std::vector<TrustedSubtree> trusted_subtrees = {generic_cert_subtree_};
  auto mtc_anchor = std::make_shared<MTCAnchor>(MakeSpan(kAnchorLogId),
                                                MakeSpan(trusted_subtrees));
  ASSERT_TRUE(trust_store->trust_store->AddMTCTrustAnchor(mtc_anchor));

  CertificateVerifyOptions opts;
  VerifyError error;
  ASSERT_TRUE(PrepareOptsForVerify(generic_cert_, trust_store.get(), &opts));
  EXPECT_FALSE(CertificateVerify(opts, &error)) << error.DiagnosticString();
  EXPECT_EQ(error.Code(), VerifyError::StatusCode::PATH_NOT_FOUND);
}

TEST_F(VerifyMTCTest, WrongProof) {
  std::unique_ptr<VerifyTrustStore> trust_store = EmptyTrustStore();
  std::vector<TrustedSubtree> trusted_subtrees = {generic_cert_subtree_};
  auto mtc_anchor = std::make_shared<MTCAnchor>(MakeSpan(kAnchorLogId),
                                                MakeSpan(trusted_subtrees));
  ASSERT_TRUE(trust_store->trust_store->AddMTCTrustAnchor(mtc_anchor));

  CertificateVerifyOptions opts;
  VerifyError error;
  ASSERT_TRUE(PrepareOptsForVerify(bitflip_cert_, trust_store.get(), &opts));
  EXPECT_FALSE(CertificateVerify(opts, &error)) << error.DiagnosticString();
  EXPECT_EQ(error.Code(),
            VerifyError::StatusCode::CERTIFICATE_INVALID_SIGNATURE);
}

TEST_F(VerifyMTCTest, UnusedBit) {
  std::unique_ptr<VerifyTrustStore> trust_store = EmptyTrustStore();
  std::vector<TrustedSubtree> trusted_subtrees = {generic_cert_subtree_};
  auto mtc_anchor = std::make_shared<MTCAnchor>(MakeSpan(kAnchorLogId),
                                                MakeSpan(trusted_subtrees));
  ASSERT_TRUE(trust_store->trust_store->AddMTCTrustAnchor(mtc_anchor));

  CertificateVerifyOptions opts;
  VerifyError error;
  ASSERT_TRUE(PrepareOptsForVerify(unused_bit_cert_, trust_store.get(), &opts));
  EXPECT_FALSE(CertificateVerify(opts, &error)) << error.DiagnosticString();
  EXPECT_EQ(error.Code(),
            VerifyError::StatusCode::CERTIFICATE_INVALID_SIGNATURE);
}

TEST_F(VerifyMTCTest, WrongLogID) {
  // Trust the correct subtree for `generic_cert_` but with the wrong log ID.
  // Verifying the cert should fail because even though the proof evaluates to a
  // valid hash, the hash is for the wrong issuer.
  std::unique_ptr<VerifyTrustStore> trust_store = EmptyTrustStore();
  std::vector<TrustedSubtree> trusted_subtrees = {generic_cert_subtree_};
  auto mtc_anchor = std::make_shared<MTCAnchor>(MakeSpan(kAnchorLogIdBitflip),
                                                MakeSpan(trusted_subtrees));
  ASSERT_TRUE(trust_store->trust_store->AddMTCTrustAnchor(mtc_anchor));

  CertificateVerifyOptions opts;
  VerifyError error;
  ASSERT_TRUE(PrepareOptsForVerify(generic_cert_, trust_store.get(), &opts));
  EXPECT_FALSE(CertificateVerify(opts, &error)) << error.DiagnosticString();
  EXPECT_EQ(error.Code(), VerifyError::StatusCode::PATH_NOT_FOUND);
}

TEST_F(VerifyMTCTest, ExpiredMTC) {
  // Configure the trust store to trust the MTC anchor with the landmark subtree
  // for `generic_cert_`.
  std::unique_ptr<VerifyTrustStore> trust_store = EmptyTrustStore();
  std::vector<TrustedSubtree> trusted_subtrees = {generic_cert_subtree_};
  auto mtc_anchor = std::make_shared<MTCAnchor>(MakeSpan(kAnchorLogId),
                                                MakeSpan(trusted_subtrees));
  ASSERT_TRUE(trust_store->trust_store->AddMTCTrustAnchor(mtc_anchor));

  CertificateVerifyOptions opts;
  opts.trust_store = trust_store.get();
  opts.leaf_cert = generic_cert_;
  std::shared_ptr<const ParsedCertificate> parsed_cert =
      CertFromString(generic_cert_);
  ASSERT_TRUE(parsed_cert);
  int64_t time;
  ASSERT_TRUE(der::GeneralizedTimeToPosixTime(
      parsed_cert->tbs().validity_not_after, &time));
  // set verify time to 1 second after the cert's validity period:
  opts.time = time + 1;
  VerifyError error;
  EXPECT_FALSE(CertificateVerify(opts, &error)) << error.DiagnosticString();
  EXPECT_EQ(error.Code(), VerifyError::StatusCode::CERTIFICATE_EXPIRED);
}

TEST_F(VerifyMTCTest, TrustStoreConfiguration) {
  // Test that an MTC isn't trusted if there's no MTCAnchor set on the trust
  // store.
  std::unique_ptr<VerifyTrustStore> trust_store = EmptyTrustStore();

  CertificateVerifyOptions opts;
  VerifyError error;
  ASSERT_TRUE(PrepareOptsForVerify(generic_cert_, trust_store.get(), &opts));
  EXPECT_FALSE(CertificateVerify(opts, &error)) << error.DiagnosticString();
  EXPECT_EQ(error.Code(), VerifyError::StatusCode::PATH_NOT_FOUND);
}

TEST_F(VerifyMTCTest, BadMTCAnchorHash) {
  // Test that an MTC isn't trusted if the MTCAnchor's TrustedSubtree has the
  // wrong hash. Configure the trust store to trust the MTC anchor with the
  // landmark subtree for `generic_cert_`.
  std::unique_ptr<VerifyTrustStore> trust_store = EmptyTrustStore();
  std::vector<TrustedSubtree> trusted_subtrees = {generic_cert_subtree_};
  trusted_subtrees[0].hash[0] ^= 1;
  auto mtc_anchor = std::make_shared<MTCAnchor>(MakeSpan(kAnchorLogId),
                                                MakeSpan(trusted_subtrees));
  ASSERT_TRUE(trust_store->trust_store->AddMTCTrustAnchor(mtc_anchor));

  CertificateVerifyOptions opts;
  VerifyError error;
  ASSERT_TRUE(PrepareOptsForVerify(generic_cert_, trust_store.get(), &opts));
  EXPECT_FALSE(CertificateVerify(opts, &error)) << error.DiagnosticString();
  EXPECT_EQ(error.Code(),
            VerifyError::StatusCode::CERTIFICATE_INVALID_SIGNATURE);
}

TEST_F(VerifyMTCTest, SubtreeRangesMatch) {
  // generic_cert_ and leaf_b_ have proofs to subtree ranges with the same start
  // but different ends. Check that CertificateVerify only succeeds if the trust
  // store has the right MTCAnchor.
  std::vector<TrustedSubtree> trusted_subtrees_a = {generic_cert_subtree_};
  std::vector<TrustedSubtree> trusted_subtrees_b = {leaf_b_subtree_};

  auto mtc_anchor_a = std::make_shared<MTCAnchor>(MakeSpan(kAnchorLogId),
                                                  MakeSpan(trusted_subtrees_a));
  auto mtc_anchor_b = std::make_shared<MTCAnchor>(MakeSpan(kAnchorLogId),
                                                  MakeSpan(trusted_subtrees_b));

  std::unique_ptr<VerifyTrustStore> trust_store_a = EmptyTrustStore();
  ASSERT_TRUE(trust_store_a->trust_store->AddMTCTrustAnchor(mtc_anchor_a));
  {
    CertificateVerifyOptions opts;
    VerifyError error;
    ASSERT_TRUE(
        PrepareOptsForVerify(generic_cert_, trust_store_a.get(), &opts));
    EXPECT_TRUE(CertificateVerify(opts, &error)) << error.DiagnosticString();
  }
  {
    CertificateVerifyOptions opts;
    VerifyError error;
    ASSERT_TRUE(PrepareOptsForVerify(leaf_b_, trust_store_a.get(), &opts));
    EXPECT_FALSE(CertificateVerify(opts, &error)) << error.DiagnosticString();
  }

  std::unique_ptr<VerifyTrustStore> trust_store_b = EmptyTrustStore();
  ASSERT_TRUE(trust_store_b->trust_store->AddMTCTrustAnchor(mtc_anchor_b));
  {
    CertificateVerifyOptions opts;
    VerifyError error;
    ASSERT_TRUE(PrepareOptsForVerify(leaf_b_, trust_store_b.get(), &opts));
    EXPECT_TRUE(CertificateVerify(opts, &error)) << error.DiagnosticString();
  }
  {
    CertificateVerifyOptions opts;
    VerifyError error;
    ASSERT_TRUE(
        PrepareOptsForVerify(generic_cert_, trust_store_b.get(), &opts));
    EXPECT_FALSE(CertificateVerify(opts, &error)) << error.DiagnosticString();
  }
}

TEST_F(VerifyMTCTest, MultipleSubtrees) {
  std::vector<TrustedSubtree> subtrees = {generic_cert_subtree_,
                                          leaf_b_subtree_, leaf_c_subtree_};
  auto mtc_anchor =
      std::make_shared<MTCAnchor>(MakeSpan(kAnchorLogId), MakeSpan(subtrees));
  std::unique_ptr<VerifyTrustStore> trust_store = EmptyTrustStore();
  ASSERT_TRUE(trust_store->trust_store->AddMTCTrustAnchor(mtc_anchor));

  // Check that generic_cert_, leaf_b_, and leaf_c_ are all trusted.
  std::vector<std::string> leafs = {generic_cert_, leaf_b_, leaf_c_};
  for (size_t i = 0; i < leafs.size(); i++) {
    SCOPED_TRACE(testing::Message() << "Leaf " << i);
    CertificateVerifyOptions opts;
    VerifyError error;
    ASSERT_TRUE(PrepareOptsForVerify(leafs[i], trust_store.get(), &opts));
    EXPECT_TRUE(CertificateVerify(opts, &error)) << error.DiagnosticString();
  }
}

BSSL_NAMESPACE_END
