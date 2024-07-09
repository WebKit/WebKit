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

#include <optional>
#include <string>
#include <string_view>

#include <openssl/pki/certificate.h>
#include <gmock/gmock.h>

#include "string_util.h"
#include "test_helpers.h"

TEST(CertificateTest, FromPEM) {
  std::string diagnostic;
  std::unique_ptr<bssl::Certificate> cert(
      bssl::Certificate::FromPEM("nonsense", &diagnostic));
  EXPECT_FALSE(cert);

  cert = bssl::Certificate::FromPEM(bssl::ReadTestFileToString(
      "testdata/verify_unittest/self-issued.pem"), &diagnostic);
  EXPECT_TRUE(cert);
}

TEST(CertificateTest, IsSelfIssued) {
  std::string diagnostic;
  const std::string leaf =
      bssl::ReadTestFileToString("testdata/verify_unittest/google-leaf.der");
  std::unique_ptr<bssl::Certificate> leaf_cert(
      bssl::Certificate::FromDER(bssl::StringAsBytes(leaf), &diagnostic));
  EXPECT_TRUE(leaf_cert);
  EXPECT_FALSE(leaf_cert->IsSelfIssued());

  const std::string self_issued =
      bssl::ReadTestFileToString("testdata/verify_unittest/self-issued.pem");
  std::unique_ptr<bssl::Certificate> self_issued_cert(
      bssl::Certificate::FromPEM(self_issued, &diagnostic));
  EXPECT_TRUE(self_issued_cert);
  EXPECT_TRUE(self_issued_cert->IsSelfIssued());
}

TEST(CertificateTest, Validity) {
  std::string diagnostic;
  const std::string leaf =
      bssl::ReadTestFileToString("testdata/verify_unittest/google-leaf.der");
  std::unique_ptr<bssl::Certificate> cert(
      bssl::Certificate::FromDER(bssl::StringAsBytes(leaf), &diagnostic));
  EXPECT_TRUE(cert);

  bssl::Certificate::Validity validity = cert->GetValidity();
  EXPECT_EQ(validity.not_before, 1498644466);
  EXPECT_EQ(validity.not_after, 1505899620);
}

TEST(CertificateTest, SerialNumber) {
  std::string diagnostic;
  const std::string leaf =
      bssl::ReadTestFileToString("testdata/verify_unittest/google-leaf.der");
  std::unique_ptr<bssl::Certificate> cert(
      bssl::Certificate::FromDER(bssl::StringAsBytes(leaf), &diagnostic));
  EXPECT_TRUE(cert);

  EXPECT_EQ(bssl::string_util::HexEncode(cert->GetSerialNumber()),
            "0118F044A8F31892");
}
