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

#include "ocsp.h"

#include <gtest/gtest.h>

#include <openssl/base64.h>
#include <openssl/pool.h>
#include <openssl/span.h>

#include "encode_values.h"
#include "parsed_certificate.h"
#include "signature_algorithm.h"
#include "string_util.h"
#include "test_helpers.h"

BSSL_NAMESPACE_BEGIN

namespace {

constexpr int64_t kOCSPAgeOneWeek = 7 * 24 * 60 * 60;

std::string GetFilePath(const std::string &file_name) {
  return std::string("testdata/ocsp_unittest/") + file_name;
}

std::shared_ptr<const ParsedCertificate> ParseCertificate(
    std::string_view data) {
  CertErrors errors;
  auto bytes = StringAsBytes(data);
  // TODO(crbug.com/533048005): Remove this option when
  // good_response_invalid_serial is removed.
  ParseCertificateOptions options;
  options.allow_invalid_serial_numbers = true;
  return ParsedCertificate::Create(
      bssl::UniquePtr<CRYPTO_BUFFER>(
          CRYPTO_BUFFER_new(bytes.data(), bytes.size(), nullptr)),
      options, &errors);
}

struct TestParams {
  const char *file_name;
  OCSPRevocationStatus expected_revocation_status;
  OCSPVerifyResult::ResponseStatus expected_response_status;
};

class CheckOCSPTest : public ::testing::TestWithParam<TestParams> {};

const TestParams kTestParams[] = {
    {"good_response.pem", OCSPRevocationStatus::GOOD,
     OCSPVerifyResult::PROVIDED},
    {"good_response_sha256.pem", OCSPRevocationStatus::GOOD,
     OCSPVerifyResult::PROVIDED},
    {"good_response_invalid_serial.pem", OCSPRevocationStatus::GOOD,
     OCSPVerifyResult::PROVIDED},
    {"no_response.pem", OCSPRevocationStatus::UNKNOWN,
     OCSPVerifyResult::NO_MATCHING_RESPONSE},
    {"malformed_request.pem", OCSPRevocationStatus::UNKNOWN,
     OCSPVerifyResult::ERROR_RESPONSE},
    {"bad_status.pem", OCSPRevocationStatus::UNKNOWN,
     OCSPVerifyResult::PARSE_RESPONSE_ERROR},
    {"bad_ocsp_type.pem", OCSPRevocationStatus::UNKNOWN,
     OCSPVerifyResult::PARSE_RESPONSE_ERROR},
    {"bad_signature.pem", OCSPRevocationStatus::UNKNOWN,
     OCSPVerifyResult::PROVIDED},
    {"ocsp_sign_direct.pem", OCSPRevocationStatus::GOOD,
     OCSPVerifyResult::PROVIDED},
    {"ocsp_sign_indirect.pem", OCSPRevocationStatus::GOOD,
     OCSPVerifyResult::PROVIDED},
    {"ocsp_sign_indirect_missing.pem", OCSPRevocationStatus::UNKNOWN,
     OCSPVerifyResult::PROVIDED},
    {"ocsp_sign_bad_indirect.pem", OCSPRevocationStatus::UNKNOWN,
     OCSPVerifyResult::PROVIDED},
    {"ocsp_sign_bad_indirect_critical_extension.pem",
     OCSPRevocationStatus::UNKNOWN, OCSPVerifyResult::PROVIDED},
    {"ocsp_sign_bad_indirect_wrong_issuer.pem", OCSPRevocationStatus::UNKNOWN,
     OCSPVerifyResult::PROVIDED},
    {"ocsp_sign_bad_indirect_expired.pem", OCSPRevocationStatus::UNKNOWN,
     OCSPVerifyResult::PROVIDED},
    {"ocsp_extra_certs.pem", OCSPRevocationStatus::GOOD,
     OCSPVerifyResult::PROVIDED},
    {"has_version.pem", OCSPRevocationStatus::GOOD, OCSPVerifyResult::PROVIDED},
    {"responder_name.pem", OCSPRevocationStatus::GOOD,
     OCSPVerifyResult::PROVIDED},
    {"responder_id.pem", OCSPRevocationStatus::GOOD,
     OCSPVerifyResult::PROVIDED},
    {"has_extension.pem", OCSPRevocationStatus::GOOD,
     OCSPVerifyResult::PROVIDED},
    {"good_response_next_update.pem", OCSPRevocationStatus::GOOD,
     OCSPVerifyResult::PROVIDED},
    {"revoke_response.pem", OCSPRevocationStatus::REVOKED,
     OCSPVerifyResult::PROVIDED},
    {"revoke_response_reason.pem", OCSPRevocationStatus::REVOKED,
     OCSPVerifyResult::PROVIDED},
    {"unknown_response.pem", OCSPRevocationStatus::UNKNOWN,
     OCSPVerifyResult::PROVIDED},
    {"multiple_response.pem", OCSPRevocationStatus::UNKNOWN,
     OCSPVerifyResult::PROVIDED},
    {"other_response.pem", OCSPRevocationStatus::UNKNOWN,
     OCSPVerifyResult::NO_MATCHING_RESPONSE},
    {"has_single_extension.pem", OCSPRevocationStatus::GOOD,
     OCSPVerifyResult::PROVIDED},
    {"has_critical_single_extension.pem", OCSPRevocationStatus::UNKNOWN,
     OCSPVerifyResult::UNHANDLED_CRITICAL_EXTENSION},
    {"has_critical_response_extension.pem", OCSPRevocationStatus::UNKNOWN,
     OCSPVerifyResult::UNHANDLED_CRITICAL_EXTENSION},
    {"has_critical_ct_extension.pem", OCSPRevocationStatus::GOOD,
     OCSPVerifyResult::PROVIDED},
    {"missing_response.pem", OCSPRevocationStatus::UNKNOWN,
     OCSPVerifyResult::NO_MATCHING_RESPONSE},
    {"stale_response.pem", OCSPRevocationStatus::UNKNOWN,
     OCSPVerifyResult::INVALID_DATE},
    {"future_response.pem", OCSPRevocationStatus::UNKNOWN,
     OCSPVerifyResult::INVALID_DATE},
    {"old_response.pem", OCSPRevocationStatus::UNKNOWN,
     OCSPVerifyResult::INVALID_DATE},
    {"produced_early_response.pem", OCSPRevocationStatus::UNKNOWN,
     OCSPVerifyResult::BAD_PRODUCED_AT},
    {"produced_late_response.pem", OCSPRevocationStatus::UNKNOWN,
     OCSPVerifyResult::BAD_PRODUCED_AT},
    {"invalid_response.pem", OCSPRevocationStatus::UNKNOWN,
     OCSPVerifyResult::PARSE_RESPONSE_ERROR},
    {"invalid_response_data.pem", OCSPRevocationStatus::UNKNOWN,
     OCSPVerifyResult::PARSE_RESPONSE_DATA_ERROR},
    {"multiple_response_good_revoked.pem", OCSPRevocationStatus::REVOKED,
     OCSPVerifyResult::PROVIDED},
    // An unparseable SingleResponse is currently ignored, so the result will be
    // NO_MATCHING_RESPONSE.
    {"good_response_invalid_status.pem", OCSPRevocationStatus::UNKNOWN,
     OCSPVerifyResult::NO_MATCHING_RESPONSE},
    {"revoke_response_invalid_status.pem", OCSPRevocationStatus::UNKNOWN,
     OCSPVerifyResult::NO_MATCHING_RESPONSE},
    {"unknown_response_invalid_status.pem", OCSPRevocationStatus::UNKNOWN,
     OCSPVerifyResult::NO_MATCHING_RESPONSE},
};

// Parameterised test name generator for tests depending on TestParams.
struct PrintTestName {
  std::string operator()(const testing::TestParamInfo<TestParams> &info) const {
    std::string_view name(info.param.file_name);
    // Strip ".pem" from the end as GTest names cannot contain period.
    name.remove_suffix(4);
    return std::string(name);
  }
};

INSTANTIATE_TEST_SUITE_P(All, CheckOCSPTest, ::testing::ValuesIn(kTestParams),
                         PrintTestName());

TEST_P(CheckOCSPTest, FromFile) {
  const TestParams &params = GetParam();

  std::string ocsp_data;
  std::string ca_data;
  std::string cert_data;
  std::string request_data;
  const PemBlockMapping mappings[] = {
      {"OCSP RESPONSE", &ocsp_data},
      {"CA CERTIFICATE", &ca_data},
      {"CERTIFICATE", &cert_data},
      {"OCSP REQUEST", &request_data},
  };

  ASSERT_TRUE(ReadTestDataFromPemFile(GetFilePath(params.file_name), mappings));

  // Mar 5 00:00:00 2017 GMT
  int64_t kVerifyTime = 1488672000;

  // Test that CheckOCSP() works.
  OCSPVerifyResult::ResponseStatus response_status;
  OCSPRevocationStatus revocation_status =
      CheckOCSP(ocsp_data, cert_data, ca_data, kVerifyTime, kOCSPAgeOneWeek,
                &response_status);

  EXPECT_EQ(params.expected_revocation_status, revocation_status);
  EXPECT_EQ(params.expected_response_status, response_status);

  // Check that CreateOCSPRequest() works.
  std::shared_ptr<const ParsedCertificate> cert = ParseCertificate(cert_data);
  ASSERT_TRUE(cert);

  std::shared_ptr<const ParsedCertificate> issuer = ParseCertificate(ca_data);
  ASSERT_TRUE(issuer);

  std::vector<uint8_t> encoded_request;
  ASSERT_TRUE(CreateOCSPRequest(cert.get(), issuer.get(), &encoded_request));

  EXPECT_EQ(der::Input(encoded_request),
            der::Input(StringAsBytes(request_data)));
}

struct TestDelegateParams {
  const char *file_name;
  std::set<SignatureAlgorithm> allowed_sig_algs;
  OCSPRevocationStatus expected_revocation_status;
  OCSPVerifyResult::ResponseStatus expected_response_status;
};

class CheckOCSPDelegateTest
    : public ::testing::TestWithParam<TestDelegateParams> {};

const TestDelegateParams kTestDelegateParams[] = {
    // Tests that the delegate is used for the policy on the OCSP response
    // signature algorithm.
    {"good_response.pem",
     {SignatureAlgorithm::kRsaPkcs1Sha1},
     OCSPRevocationStatus::GOOD,
     OCSPVerifyResult::PROVIDED},
    {"good_response.pem",
     {SignatureAlgorithm::kRsaPkcs1Sha256},
     OCSPRevocationStatus::UNKNOWN,
     OCSPVerifyResult::PROVIDED},
    {"good_response_sha256.pem",
     {SignatureAlgorithm::kRsaPkcs1Sha1},
     OCSPRevocationStatus::UNKNOWN,
     OCSPVerifyResult::PROVIDED},
    {"good_response_sha256.pem",
     {SignatureAlgorithm::kRsaPkcs1Sha256},
     OCSPRevocationStatus::GOOD,
     OCSPVerifyResult::PROVIDED},

    // Tests that the delegate is used for the policy on the authorized
    // responder verification.
    //
    // The ocsp_sign_indirect.pem uses SHA-1 for the OCSP response signature,
    // and SHA-256 for the authorized responder certificate's signature. If
    // both algorithms are allowed, it should verify successfully. If SHA-256
    // is not allowed, the authorized responder certificate should be rejected
    // during the certificate verification.
    {"ocsp_sign_indirect.pem",
     {SignatureAlgorithm::kRsaPkcs1Sha1, SignatureAlgorithm::kRsaPkcs1Sha256},
     OCSPRevocationStatus::GOOD,
     OCSPVerifyResult::PROVIDED},
    {"ocsp_sign_indirect.pem",
     {SignatureAlgorithm::kRsaPkcs1Sha1},
     OCSPRevocationStatus::UNKNOWN,
     OCSPVerifyResult::PROVIDED},
};

// Parameterised test name generator for tests depending on TestDelegateParams.
struct PrintTestDelegateName {
  std::string operator()(
      const testing::TestParamInfo<TestDelegateParams> &info) const {
    std::string_view file_name(info.param.file_name);
    // Strip ".pem" from the end as GTest names cannot contain period.
    file_name.remove_suffix(4);
    std::string name = std::string(file_name);
    for (SignatureAlgorithm sig_alg : info.param.allowed_sig_algs) {
      name += "SigAlg";
      name += std::to_string(static_cast<int>(sig_alg));
    }
    return name;
  }
};

INSTANTIATE_TEST_SUITE_P(All, CheckOCSPDelegateTest,
                         ::testing::ValuesIn(kTestDelegateParams),
                         PrintTestDelegateName());

// A delegate that only allows a specified set of signature algorithms.
//
// Derived from SimplePathBuilderDelegate just so that the test doesn't need to
// add no-op implementations of all the other methods on the
// VerifyCertificateChainDelegate interface. The actual
// IsSignatureAlgorithmAcceptable method is overridden so the DigestPolicy given
// to SimplePathBuilderDelegate doesn't actually matter.
class SigAlgCheckerDelegate : public SimplePathBuilderDelegate {
 public:
  SigAlgCheckerDelegate(std::set<SignatureAlgorithm> allowed_sig_algs)
      : SimplePathBuilderDelegate(
            1024, SimplePathBuilderDelegate::DigestPolicy::kStrong),
        allowed_sig_algs_(std::move(allowed_sig_algs)) {}

  bool IsSignatureAlgorithmAcceptable(SignatureAlgorithm signature_algorithm,
                                      CertErrors *errors) override {
    return allowed_sig_algs_.find(signature_algorithm) !=
           allowed_sig_algs_.end();
  }

 private:
  std::set<SignatureAlgorithm> allowed_sig_algs_;
};


TEST_P(CheckOCSPDelegateTest, FromFile) {
  const TestDelegateParams &params = GetParam();

  SigAlgCheckerDelegate delegate(params.allowed_sig_algs);

  std::string ocsp_data;
  std::string ca_data;
  std::string cert_data;
  std::string request_data;
  const PemBlockMapping mappings[] = {
      {"OCSP RESPONSE", &ocsp_data},
      {"CA CERTIFICATE", &ca_data},
      {"CERTIFICATE", &cert_data},
      {"OCSP REQUEST", &request_data},
  };

  ASSERT_TRUE(ReadTestDataFromPemFile(GetFilePath(params.file_name), mappings));

  // Mar 5 00:00:00 2017 GMT
  int64_t kVerifyTime = 1488672000;

  std::shared_ptr<const ParsedCertificate> cert = ParseCertificate(cert_data);
  ASSERT_TRUE(cert);
  std::shared_ptr<const ParsedCertificate> issuer = ParseCertificate(ca_data);
  ASSERT_TRUE(issuer);


  // Test that CheckOCSP() works.
  OCSPVerifyResult::ResponseStatus response_status;
  OCSPRevocationStatus revocation_status =
      CheckOCSP(ocsp_data, cert, issuer, kVerifyTime, kOCSPAgeOneWeek,
                &delegate, &response_status);

  EXPECT_EQ(params.expected_revocation_status, revocation_status);
  EXPECT_EQ(params.expected_response_status, response_status);
}

std::string_view kGetURLTestParams[] = {
    "http://www.example.com/",
    "http://www.example.com/path/",
    "http://www.example.com/path",
    "http://www.example.com/path?query"
    "http://user:pass@www.example.com/path?query",
};

class CreateOCSPGetURLTest : public ::testing::TestWithParam<std::string_view> {
};

INSTANTIATE_TEST_SUITE_P(All, CreateOCSPGetURLTest,
                         ::testing::ValuesIn(kGetURLTestParams));

TEST_P(CreateOCSPGetURLTest, Basic) {
  std::string ca_data;
  std::string cert_data;
  std::string request_data;
  const PemBlockMapping mappings[] = {
      {"CA CERTIFICATE", &ca_data},
      {"CERTIFICATE", &cert_data},
      {"OCSP REQUEST", &request_data},
  };

  // Load one of the test files. (Doesn't really matter which one as
  // constructing the DER is tested elsewhere).
  ASSERT_TRUE(
      ReadTestDataFromPemFile(GetFilePath("good_response.pem"), mappings));

  std::shared_ptr<const ParsedCertificate> cert = ParseCertificate(cert_data);
  ASSERT_TRUE(cert);

  std::shared_ptr<const ParsedCertificate> issuer = ParseCertificate(ca_data);
  ASSERT_TRUE(issuer);

  std::optional<std::string> url =
      CreateOCSPGetURL(cert.get(), issuer.get(), GetParam());
  ASSERT_TRUE(url);

  // Try to extract the encoded data and compare against `request_data`.
  //
  // A known answer output test would be better as this just reverses the logic
  // from the implementation file.
  std::string b64 = url->substr(GetParam().size() + 1);

  // Hex un-escape the data.
  b64 = bssl::string_util::FindAndReplace(b64, "%2B", "+");
  b64 = bssl::string_util::FindAndReplace(b64, "%2F", "/");
  b64 = bssl::string_util::FindAndReplace(b64, "%3D", "=");

  // Base64 decode the data.
  size_t len;
  EXPECT_TRUE(EVP_DecodedLength(&len, b64.size()));
  std::vector<uint8_t> decoded(len);
  EXPECT_TRUE(EVP_DecodeBase64(decoded.data(), &len, len,
                               reinterpret_cast<const uint8_t *>(b64.data()),
                               b64.size()));
  std::string decoded_string(decoded.begin(), decoded.begin() + len);

  EXPECT_EQ(request_data, decoded_string);
}

}  // namespace

BSSL_NAMESPACE_END
