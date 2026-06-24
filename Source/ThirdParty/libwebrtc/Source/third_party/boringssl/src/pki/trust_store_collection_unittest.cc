// Copyright 2016 The Chromium Authors
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

#include "trust_store_collection.h"

#include <algorithm>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include <openssl/pool.h>

#include "test_helpers.h"
#include "trust_store_in_memory.h"

BSSL_NAMESPACE_BEGIN

namespace {

class TrustStoreCollectionTest : public testing::Test {
 public:
  void SetUp() override {
    ParsedCertificateList chain;
    ASSERT_TRUE(ReadCertChainFromFile(
        "testdata/verify_certificate_chain_unittest/key-rollover/oldchain.pem",
        &chain));

    ASSERT_EQ(3U, chain.size());
    target_ = chain[0];
    oldintermediate_ = chain[1];
    oldroot_ = chain[2];
    ASSERT_TRUE(target_);
    ASSERT_TRUE(oldintermediate_);
    ASSERT_TRUE(oldroot_);

    ASSERT_TRUE(
        ReadCertChainFromFile("testdata/verify_certificate_chain_unittest/"
                              "key-rollover/longrolloverchain.pem",
                              &chain));

    ASSERT_EQ(5U, chain.size());
    newintermediate_ = chain[1];
    newroot_ = chain[2];
    newrootrollover_ = chain[3];
    ASSERT_TRUE(newintermediate_);
    ASSERT_TRUE(newroot_);
    ASSERT_TRUE(newrootrollover_);
  }

 protected:
  std::shared_ptr<const ParsedCertificate> oldroot_;
  std::shared_ptr<const ParsedCertificate> newroot_;
  std::shared_ptr<const ParsedCertificate> newrootrollover_;

  std::shared_ptr<const ParsedCertificate> target_;
  std::shared_ptr<const ParsedCertificate> oldintermediate_;
  std::shared_ptr<const ParsedCertificate> newintermediate_;
};

// Collection contains no stores, should return no results.
TEST_F(TrustStoreCollectionTest, NoStores) {
  ParsedCertificateList issuers;

  TrustStoreCollection collection;
  collection.SyncGetIssuersOf(target_.get(), &issuers);

  EXPECT_TRUE(issuers.empty());
}

// Collection contains only one store.
TEST_F(TrustStoreCollectionTest, OneStore) {
  ParsedCertificateList issuers;

  TrustStoreCollection collection;
  TrustStoreInMemory in_memory;
  in_memory.AddTrustAnchor(newroot_);
  collection.AddTrustStore(&in_memory);
  collection.SyncGetIssuersOf(newintermediate_.get(), &issuers);

  ASSERT_EQ(1U, issuers.size());
  EXPECT_EQ(newroot_.get(), issuers[0].get());

  // newroot_ is trusted.
  CertificateTrust trust = collection.GetTrust(newroot_.get());
  EXPECT_EQ(CertificateTrust::ForTrustAnchor().ToDebugString(),
            trust.ToDebugString());

  // oldroot_ is not.
  trust = collection.GetTrust(oldroot_.get());
  EXPECT_EQ(CertificateTrust::ForUnspecified().ToDebugString(),
            trust.ToDebugString());
}

// SyncGetIssuersOf() should append to its output parameters rather than assign
// them.
TEST_F(TrustStoreCollectionTest, OutputVectorsAppendedTo) {
  ParsedCertificateList issuers;

  // Populate the out-parameter with some values.
  issuers.resize(3);

  TrustStoreCollection collection;
  TrustStoreInMemory in_memory;
  in_memory.AddTrustAnchor(newroot_);
  collection.AddTrustStore(&in_memory);
  collection.SyncGetIssuersOf(newintermediate_.get(), &issuers);

  ASSERT_EQ(4U, issuers.size());
  EXPECT_EQ(newroot_.get(), issuers[3].get());

  // newroot_ is trusted.
  CertificateTrust trust = collection.GetTrust(newroot_.get());
  EXPECT_EQ(CertificateTrust::ForTrustAnchor().ToDebugString(),
            trust.ToDebugString());

  // newrootrollover_ is not.
  trust = collection.GetTrust(newrootrollover_.get());
  EXPECT_EQ(CertificateTrust::ForUnspecified().ToDebugString(),
            trust.ToDebugString());
}

// Collection contains two stores.
TEST_F(TrustStoreCollectionTest, TwoStores) {
  ParsedCertificateList issuers;

  TrustStoreCollection collection;
  TrustStoreInMemory in_memory1;
  TrustStoreInMemory in_memory2;
  in_memory1.AddTrustAnchor(newroot_);
  in_memory2.AddTrustAnchor(oldroot_);
  collection.AddTrustStore(&in_memory1);
  collection.AddTrustStore(&in_memory2);
  collection.SyncGetIssuersOf(newintermediate_.get(), &issuers);

  ASSERT_EQ(2U, issuers.size());
  EXPECT_EQ(newroot_.get(), issuers[0].get());
  EXPECT_EQ(oldroot_.get(), issuers[1].get());

  // newroot_ is trusted.
  CertificateTrust trust = collection.GetTrust(newroot_.get());
  EXPECT_EQ(CertificateTrust::ForTrustAnchor().ToDebugString(),
            trust.ToDebugString());

  // oldroot_ is trusted.
  trust = collection.GetTrust(oldroot_.get());
  EXPECT_EQ(CertificateTrust::ForTrustAnchor().ToDebugString(),
            trust.ToDebugString());

  // newrootrollover_ is not.
  trust = collection.GetTrust(newrootrollover_.get());
  EXPECT_EQ(CertificateTrust::ForUnspecified().ToDebugString(),
            trust.ToDebugString());
}

// Collection contains two stores. The certificate is marked as trusted in one,
// but distrusted in the other.
TEST_F(TrustStoreCollectionTest, DistrustTakesPriority) {
  ParsedCertificateList issuers;

  TrustStoreCollection collection;
  TrustStoreInMemory in_memory1;
  TrustStoreInMemory in_memory2;

  // newroot_ is trusted in store1, distrusted in store2.
  in_memory1.AddTrustAnchor(newroot_);
  in_memory2.AddDistrustedCertificateForTest(newroot_);

  // oldintermediate is distrusted in store1, trusted in store2.
  in_memory1.AddDistrustedCertificateForTest(oldintermediate_);
  in_memory2.AddTrustAnchor(oldintermediate_);

  collection.AddTrustStore(&in_memory1);
  collection.AddTrustStore(&in_memory2);

  // newroot_ is distrusted..
  CertificateTrust trust = collection.GetTrust(newroot_.get());
  EXPECT_EQ(CertificateTrust::ForDistrusted().ToDebugString(),
            trust.ToDebugString());

  // oldintermediate_ is distrusted.
  trust = collection.GetTrust(oldintermediate_.get());
  EXPECT_EQ(CertificateTrust::ForDistrusted().ToDebugString(),
            trust.ToDebugString());

  // newrootrollover_ is unspecified.
  trust = collection.GetTrust(newrootrollover_.get());
  EXPECT_EQ(CertificateTrust::ForUnspecified().ToDebugString(),
            trust.ToDebugString());
}

class TrustStoreCollectionMtcTest : public testing::Test {
 public:
  void SetUp() override {
    ParsedCertificateList chain;
    ASSERT_TRUE(
        ReadCertChainFromFile("testdata/verify_unittest/mtc-leaf.pem", &chain));

    ASSERT_EQ(1U, chain.size());
    mtc_leaf_ = chain[0];
    ASSERT_TRUE(mtc_leaf_);

    // Create modified versions of `mtc_leaf_` with different issuers. These
    // don't need to be able to verify, we just need certs with different
    // issuers to test the issuer lookup in GetTrustedMTCIssuerOf.
    constexpr char kLogId2Str[] = "32473.2";
    mtc_leaf2_ = ModifyLeafWithIssuerLogId(kLogId2Str);
    ASSERT_TRUE(mtc_leaf2_);

    constexpr char kLogId3Str[] = "32473.3";
    mtc_leaf3_ = ModifyLeafWithIssuerLogId(kLogId3Str);
    ASSERT_TRUE(mtc_leaf3_);
  }

  std::shared_ptr<const ParsedCertificate> ModifyLeafWithIssuerLogId(
      std::string_view new_log_id) {
    // The log_id encoded in the issuer name of mtc-leaf.pem.
    constexpr std::string_view log_id_str = "32473.1";

    if (new_log_id.size() != log_id_str.size()) {
      ADD_FAILURE() << "invalid replacement string";
      return nullptr;
    }

    std::vector<uint8_t> leaf_der2(mtc_leaf_->der_cert().begin(),
                                   mtc_leaf_->der_cert().end());

    // find the log_id_str in leaf_der2 and replace the bytes with
    // new log id.
    auto it = std::search(leaf_der2.begin(), leaf_der2.end(),
                          log_id_str.begin(), log_id_str.end());
    if (it == leaf_der2.end()) {
      ADD_FAILURE() << "log id not found";
      return nullptr;
    }
    std::copy(new_log_id.begin(), new_log_id.end(), it);

    CertErrors errors;
    std::shared_ptr<const ParsedCertificate> mtc_leaf2 =
        ParsedCertificate::Create(
            bssl::UniquePtr<CRYPTO_BUFFER>(
                CRYPTO_BUFFER_new(leaf_der2.data(), leaf_der2.size(), nullptr)),
            {}, &errors);
    EXPECT_TRUE(mtc_leaf2) << errors.ToDebugString();
    return mtc_leaf2;
  }

 protected:
  std::shared_ptr<const ParsedCertificate> mtc_leaf_;
  std::shared_ptr<const ParsedCertificate> mtc_leaf2_;
  std::shared_ptr<const ParsedCertificate> mtc_leaf3_;
};

TEST_F(TrustStoreCollectionMtcTest, MtcNoStores) {
  TrustStoreCollection collection;
  EXPECT_EQ(nullptr, collection.GetTrustedMTCIssuerOf(mtc_leaf_.get()));
}

TEST_F(TrustStoreCollectionMtcTest, MtcTwoStores) {
  constexpr uint8_t kLogid1[] = {0x81, 0xfd, 0x59, 0x01};
  constexpr uint8_t kLogid2[] = {0x81, 0xfd, 0x59, 0x02};
  constexpr uint8_t kLogid3[] = {0x81, 0xfd, 0x59, 0x03};

  Span<const TrustedSubtree> trusted_subtrees;
  std::shared_ptr<const MTCAnchor> mtc_anchor1 =
      std::make_shared<MTCAnchor>(MakeSpan(kLogid1), trusted_subtrees);
  std::shared_ptr<const MTCAnchor> mtc_anchor2 =
      std::make_shared<MTCAnchor>(MakeSpan(kLogid2), trusted_subtrees);
  std::shared_ptr<const MTCAnchor> mtc_anchor3 =
      std::make_shared<MTCAnchor>(MakeSpan(kLogid3), trusted_subtrees);

  TrustStoreCollection collection;
  TrustStoreInMemory in_memory1;
  TrustStoreInMemory in_memory2;
  ASSERT_TRUE(in_memory1.AddMTCTrustAnchor(mtc_anchor1));
  ASSERT_TRUE(in_memory2.AddMTCTrustAnchor(mtc_anchor2));
  collection.AddTrustStore(&in_memory1);
  collection.AddTrustStore(&in_memory2);

  EXPECT_EQ(mtc_anchor1, collection.GetTrustedMTCIssuerOf(mtc_leaf_.get()));
  EXPECT_EQ(mtc_anchor2, collection.GetTrustedMTCIssuerOf(mtc_leaf2_.get()));
  EXPECT_EQ(nullptr, collection.GetTrustedMTCIssuerOf(mtc_leaf3_.get()));
}

}  // namespace

BSSL_NAMESPACE_END
