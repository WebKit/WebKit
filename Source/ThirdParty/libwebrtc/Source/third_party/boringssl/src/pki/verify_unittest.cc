/* Copyright (c) 2023, Google Inc.
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

#include <string.h>

#include <optional>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <openssl/pki/verify.h>
#include <openssl/pki/verify_error.h>
#include <openssl/sha.h>

#include "test_helpers.h"

namespace bssl {

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
  // Set the |max_path_building_depth| explicitly to test the non-default case.
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

}  // namespace bssl
