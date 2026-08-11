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

#include "trust_store_in_memory.h"

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include <openssl/span.h>

#include "merkle_tree.h"
#include "test_helpers.h"
#include "trust_store.h"

BSSL_NAMESPACE_BEGIN
namespace {

class TrustStoreInMemoryTest : public testing::Test {
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

TEST_F(TrustStoreInMemoryTest, OneRootTrusted) {
  TrustStoreInMemory in_memory;
  in_memory.AddTrustAnchor(newroot_);

  // newroot_ is trusted.
  CertificateTrust trust = in_memory.GetTrust(newroot_.get());
  EXPECT_EQ(CertificateTrust::ForTrustAnchor().ToDebugString(),
            trust.ToDebugString());

  // oldroot_ is not.
  trust = in_memory.GetTrust(oldroot_.get());
  EXPECT_EQ(CertificateTrust::ForUnspecified().ToDebugString(),
            trust.ToDebugString());
}

TEST_F(TrustStoreInMemoryTest, DistrustBySPKI) {
  TrustStoreInMemory in_memory;
  in_memory.AddDistrustedCertificateBySPKI(
      std::string(BytesAsStringView(newroot_->tbs().spki_tlv)));

  // newroot_ is distrusted.
  CertificateTrust trust = in_memory.GetTrust(newroot_.get());
  EXPECT_EQ(CertificateTrust::ForDistrusted().ToDebugString(),
            trust.ToDebugString());

  // oldroot_ is unspecified.
  trust = in_memory.GetTrust(oldroot_.get());
  EXPECT_EQ(CertificateTrust::ForUnspecified().ToDebugString(),
            trust.ToDebugString());

  // newrootrollover_ is also distrusted because it has the same key.
  trust = in_memory.GetTrust(newrootrollover_.get());
  EXPECT_EQ(CertificateTrust::ForDistrusted().ToDebugString(),
            trust.ToDebugString());
}

TEST_F(TrustStoreInMemoryTest, DistrustBySPKIOverridesTrust) {
  TrustStoreInMemory in_memory;
  in_memory.AddTrustAnchor(newroot_);
  in_memory.AddDistrustedCertificateBySPKI(
      std::string(BytesAsStringView(newroot_->tbs().spki_tlv)));

  // newroot_ is distrusted.
  CertificateTrust trust = in_memory.GetTrust(newroot_.get());
  EXPECT_EQ(CertificateTrust::ForDistrusted().ToDebugString(),
            trust.ToDebugString());
}

TEST_F(TrustStoreInMemoryTest, IsEmptyClear) {
  TrustStoreInMemory in_memory;

  // Trust store is empty with nothing in it.
  EXPECT_TRUE(in_memory.IsEmpty());

  // After adding a classical trust anchor, it is no longer empty:
  in_memory.AddTrustAnchor(oldroot_);
  EXPECT_FALSE(in_memory.IsEmpty());

  // It is empty again after a call to Clear:
  in_memory.Clear();
  EXPECT_TRUE(in_memory.IsEmpty());

  // After adding an MTC root, it is no longer empty:
  static const uint8_t kValidLogId[] = {42};  // relative OID of 42
  TrustedSubtree a;
  a.range = Subtree{0, 4};
  std::vector<TrustedSubtree> valid_subtrees = {a};
  std::shared_ptr<MTCAnchor> valid_anchor =
      std::make_shared<MTCAnchor>(kValidLogId, MakeSpan(valid_subtrees));
  EXPECT_TRUE(valid_anchor->IsValid());
  EXPECT_TRUE(in_memory.AddMTCTrustAnchor(valid_anchor));
  EXPECT_FALSE(in_memory.IsEmpty());

  // It is empty again after a call to Clear:
  in_memory.Clear();
  EXPECT_TRUE(in_memory.IsEmpty());
}

TEST_F(TrustStoreInMemoryTest, MTCAnchors) {
  TrustStoreInMemory in_memory;

  // AddMTCTrustAnchor should fail if the MTCAnchor is invalid.
  static const uint8_t kValidLogId[] = {42};  // relative OID of 42
  TrustedSubtree a;
  a.range = Subtree{0, 4};
  TrustedSubtree b;
  b.range = Subtree{0, 6};
  TrustedSubtree c;
  c.range = Subtree{8, 9};
  std::vector<TrustedSubtree> valid_subtrees = {a, b, c};
  std::shared_ptr<MTCAnchor> valid_anchor =
      std::make_shared<MTCAnchor>(kValidLogId, MakeSpan(valid_subtrees));
  EXPECT_TRUE(valid_anchor->IsValid());
  EXPECT_EQ(valid_anchor->log_id(), kValidLogId);
  EXPECT_TRUE(in_memory.AddMTCTrustAnchor(valid_anchor));

  {
    // Attempting to add another MTCTrustAnchor with the same Log ID should fail
    TrustedSubtree d;
    d.range = Subtree{16, 17};
    std::vector<TrustedSubtree> subtrees = {d};
    std::shared_ptr<MTCAnchor> anchor =
        std::make_shared<MTCAnchor>(kValidLogId, MakeSpan(subtrees));
    EXPECT_TRUE(anchor->IsValid());
    EXPECT_FALSE(in_memory.AddMTCTrustAnchor(anchor));
  }

  {
    static const uint8_t kInvalidLogId[] = {
        255};  // The high bit is set, indicating this relative OID has more
               // bytes, but there are no more bytes.
    std::shared_ptr<MTCAnchor> invalid_anchor =
        std::make_shared<MTCAnchor>(kInvalidLogId, MakeSpan(valid_subtrees));
    EXPECT_FALSE(invalid_anchor->IsValid());
    EXPECT_FALSE(in_memory.AddMTCTrustAnchor(invalid_anchor));
  }

  {
    std::vector<TrustedSubtree> invalid_subtrees = {b, a, c};
    std::shared_ptr<MTCAnchor> invalid_anchor =
        std::make_shared<MTCAnchor>(kValidLogId, MakeSpan(invalid_subtrees));
    EXPECT_FALSE(invalid_anchor->IsValid());
    EXPECT_FALSE(in_memory.AddMTCTrustAnchor(invalid_anchor));
  }

  {
    TrustedSubtree subtree;
    subtree.range = Subtree{4, 9};
    std::vector<TrustedSubtree> invalid_subtrees = {subtree};
    std::shared_ptr<MTCAnchor> invalid_anchor =
        std::make_shared<MTCAnchor>(kValidLogId, MakeSpan(invalid_subtrees));
    EXPECT_FALSE(invalid_anchor->IsValid());
    EXPECT_FALSE(in_memory.AddMTCTrustAnchor(invalid_anchor));
  }
}

TEST_F(TrustStoreInMemoryTest, ContainsMTCAnchor) {
  TrustStoreInMemory in_memory1;
  TrustStoreInMemory in_memory2;

  static const uint8_t kValidLogId1[] = {42};  // relative OID of 42
  static const uint8_t kValidLogId2[] = {43};  // relative OID of 43
  std::vector<TrustedSubtree> subtrees;
  std::shared_ptr<MTCAnchor> anchor1 =
      std::make_shared<MTCAnchor>(kValidLogId1, MakeSpan(subtrees));
  std::shared_ptr<MTCAnchor> anchor1_dup =
      std::make_shared<MTCAnchor>(kValidLogId1, MakeSpan(subtrees));
  std::shared_ptr<MTCAnchor> anchor2 =
      std::make_shared<MTCAnchor>(kValidLogId2, MakeSpan(subtrees));
  std::shared_ptr<MTCAnchor> anchor2_dup =
      std::make_shared<MTCAnchor>(kValidLogId2, MakeSpan(subtrees));

  ASSERT_TRUE(in_memory1.AddMTCTrustAnchor(anchor1));
  ASSERT_TRUE(in_memory2.AddMTCTrustAnchor(anchor2));

  EXPECT_TRUE(in_memory1.ContainsMTCAnchor(anchor1.get()));
  EXPECT_TRUE(in_memory1.ContainsMTCAnchor(anchor1_dup.get()));
  EXPECT_FALSE(in_memory1.ContainsMTCAnchor(anchor2.get()));
  EXPECT_TRUE(in_memory2.ContainsMTCAnchor(anchor2.get()));
  EXPECT_TRUE(in_memory2.ContainsMTCAnchor(anchor2_dup.get()));
  EXPECT_FALSE(in_memory2.ContainsMTCAnchor(anchor1.get()));
}

}  // namespace
BSSL_NAMESPACE_END
