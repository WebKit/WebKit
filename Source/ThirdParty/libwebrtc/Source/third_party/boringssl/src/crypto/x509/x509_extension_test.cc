// Copyright 2025 The BoringSSL Authors
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

#include <openssl/x509.h>

#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include <openssl/asn1.h>
#include <openssl/obj.h>
#include <openssl/span.h>
#include <openssl/x509v3.h>

#include "../test/der_trailing_data.h"
#include "../test/test_util.h"

BSSL_NAMESPACE_BEGIN
namespace {

static std::string ASN1ObjectToString(const ASN1_OBJECT *obj) {
  char buf[128];
  if (OBJ_obj2txt(buf, sizeof(buf), obj, /*always_return_oid=*/0) < 0) {
    ADD_FAILURE() << "OBJ_obj2txt failed";
    return "ERROR";
  }
  return buf;
}

static Span<const uint8_t> ASN1StringAsBytes(const ASN1_STRING *str) {
  return Span(ASN1_STRING_get0_data(str), ASN1_STRING_length(str));
}

static std::string_view ASN1StringAsView(const ASN1_STRING *str) {
  return BytesAsStringView(ASN1StringAsBytes(str));
}

static std::string ASN1StringToUTF8(const ASN1_STRING *str) {
  uint8_t *utf8;
  int utf8_len = ASN1_STRING_to_UTF8(&utf8, str);
  if (utf8_len < 0) {
    ADD_FAILURE() << "ASN1_STRING_to_UTF8 failed";
    return "ERROR";
  }
  UniquePtr<uint8_t> free_utf8(utf8);
  return std::string(utf8, utf8 + utf8_len);
}

TEST(X509ExtensionTest, ParseCertificatePolicies) {
  // A sample input with a few policies and qualifiers to exercise the parser:
  static const uint8_t kTestPolicies[] = {
      0x30, 0x82, 0x01, 0xca, 0x30, 0x0f, 0x06, 0x0d, 0x2a, 0x86, 0x48, 0x86,
      0xf7, 0x12, 0x04, 0x01, 0x84, 0xb7, 0x09, 0x02, 0x01, 0x30, 0x82, 0x01,
      0xb5, 0x06, 0x0d, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x12, 0x04, 0x01, 0x84,
      0xb7, 0x09, 0x02, 0x02, 0x30, 0x82, 0x01, 0xa2, 0x30, 0x1f, 0x06, 0x08,
      0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x02, 0x01, 0x16, 0x13, 0x68, 0x74,
      0x74, 0x70, 0x73, 0x3a, 0x2f, 0x2f, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c,
      0x65, 0x2e, 0x63, 0x6f, 0x6d, 0x30, 0x0c, 0x06, 0x08, 0x2b, 0x06, 0x01,
      0x05, 0x05, 0x07, 0x02, 0x02, 0x30, 0x00, 0x30, 0x27, 0x06, 0x08, 0x2b,
      0x06, 0x01, 0x05, 0x05, 0x07, 0x02, 0x02, 0x30, 0x1b, 0x30, 0x19, 0x0c,
      0x0c, 0x6f, 0x72, 0x67, 0x61, 0x6e, 0x69, 0x7a, 0x61, 0x74, 0x69, 0x6f,
      0x6e, 0x30, 0x09, 0x02, 0x01, 0x01, 0x02, 0x01, 0x02, 0x02, 0x01, 0x03,
      0x30, 0x1a, 0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x02, 0x02,
      0x30, 0x0e, 0x0c, 0x0c, 0x65, 0x78, 0x70, 0x6c, 0x69, 0x63, 0x69, 0x74,
      0x54, 0x65, 0x78, 0x74, 0x30, 0x35, 0x06, 0x08, 0x2b, 0x06, 0x01, 0x05,
      0x05, 0x07, 0x02, 0x02, 0x30, 0x29, 0x30, 0x19, 0x0c, 0x0c, 0x6f, 0x72,
      0x67, 0x61, 0x6e, 0x69, 0x7a, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x30, 0x09,
      0x02, 0x01, 0x01, 0x02, 0x01, 0x02, 0x02, 0x01, 0x03, 0x0c, 0x0c, 0x65,
      0x78, 0x70, 0x6c, 0x69, 0x63, 0x69, 0x74, 0x54, 0x65, 0x78, 0x74, 0x30,
      0x35, 0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x02, 0x02, 0x30,
      0x29, 0x30, 0x19, 0x16, 0x0c, 0x6f, 0x72, 0x67, 0x61, 0x6e, 0x69, 0x7a,
      0x61, 0x74, 0x69, 0x6f, 0x6e, 0x30, 0x09, 0x02, 0x01, 0x01, 0x02, 0x01,
      0x02, 0x02, 0x01, 0x03, 0x16, 0x0c, 0x65, 0x78, 0x70, 0x6c, 0x69, 0x63,
      0x69, 0x74, 0x54, 0x65, 0x78, 0x74, 0x30, 0x35, 0x06, 0x08, 0x2b, 0x06,
      0x01, 0x05, 0x05, 0x07, 0x02, 0x02, 0x30, 0x29, 0x30, 0x19, 0x1a, 0x0c,
      0x6f, 0x72, 0x67, 0x61, 0x6e, 0x69, 0x7a, 0x61, 0x74, 0x69, 0x6f, 0x6e,
      0x30, 0x09, 0x02, 0x01, 0x01, 0x02, 0x01, 0x02, 0x02, 0x01, 0x03, 0x1a,
      0x0c, 0x65, 0x78, 0x70, 0x6c, 0x69, 0x63, 0x69, 0x74, 0x54, 0x65, 0x78,
      0x74, 0x30, 0x4d, 0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x02,
      0x02, 0x30, 0x41, 0x30, 0x25, 0x1e, 0x18, 0x00, 0x6f, 0x00, 0x72, 0x00,
      0x67, 0x00, 0x61, 0x00, 0x6e, 0x00, 0x69, 0x00, 0x7a, 0x00, 0x61, 0x00,
      0x74, 0x00, 0x69, 0x00, 0x6f, 0x00, 0x6e, 0x30, 0x09, 0x02, 0x01, 0x01,
      0x02, 0x01, 0x02, 0x02, 0x01, 0x03, 0x1e, 0x18, 0x00, 0x65, 0x00, 0x78,
      0x00, 0x70, 0x00, 0x6c, 0x00, 0x69, 0x00, 0x63, 0x00, 0x69, 0x00, 0x74,
      0x00, 0x54, 0x00, 0x65, 0x00, 0x78, 0x00, 0x74, 0x30, 0x11, 0x06, 0x0d,
      0x2a, 0x86, 0x48, 0x86, 0xf7, 0x12, 0x04, 0x01, 0x84, 0xb7, 0x09, 0x02,
      0x02, 0x16, 0x00, 0x30, 0x12, 0x06, 0x0d, 0x2a, 0x86, 0x48, 0x86, 0xf7,
      0x12, 0x04, 0x01, 0x84, 0xb7, 0x09, 0x02, 0x02, 0x01, 0x01, 0xff, 0x30,
      0x11, 0x06, 0x0d, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x12, 0x04, 0x01, 0x84,
      0xb7, 0x09, 0x02, 0x02, 0x80, 0x00};
  const uint8_t *inp = kTestPolicies;
  UniquePtr<CERTIFICATEPOLICIES> policies(
      d2i_CERTIFICATEPOLICIES(nullptr, &inp, sizeof(kTestPolicies)));
  ASSERT_TRUE(policies);
  EXPECT_EQ(inp, kTestPolicies + sizeof(kTestPolicies));

  ASSERT_EQ(sk_POLICYINFO_num(policies.get()), 2u);

  // The first policy has no qualifiers.
  const POLICYINFO *policy = sk_POLICYINFO_value(policies.get(), 0);
  EXPECT_EQ(ASN1ObjectToString(policy->policyid),
            "1.2.840.113554.4.1.72585.2.1");
  EXPECT_EQ(policy->qualifiers, nullptr);

  // The second policy has a wide range of qualifiers, to exercise the encoding.
  policy = sk_POLICYINFO_value(policies.get(), 1);
  EXPECT_EQ(ASN1ObjectToString(policy->policyid),
            "1.2.840.113554.4.1.72585.2.2");
  EXPECT_EQ(sk_POLICYQUALINFO_num(policy->qualifiers), 11u);

  // Sample id-qt-cps
  const POLICYQUALINFO *qualifier =
      sk_POLICYQUALINFO_value(policy->qualifiers, 0);
  EXPECT_EQ(ASN1ObjectToString(qualifier->pqualid), "Policy Qualifier CPS");
  EXPECT_EQ(qualifier->d.cpsuri->type, V_ASN1_IA5STRING);
  EXPECT_EQ(ASN1StringAsView(qualifier->d.cpsuri), "https://example.com");

  // Sample id-qt-unotice with both optional fields omitted.
  qualifier = sk_POLICYQUALINFO_value(policy->qualifiers, 1);
  EXPECT_EQ(ASN1ObjectToString(qualifier->pqualid),
            "Policy Qualifier User Notice");
  EXPECT_EQ(qualifier->d.usernotice->noticeref, nullptr);
  EXPECT_EQ(qualifier->d.usernotice->exptext, nullptr);

  // Sample id-qt-unotice with noticeRef.
  qualifier = sk_POLICYQUALINFO_value(policy->qualifiers, 2);
  EXPECT_EQ(ASN1ObjectToString(qualifier->pqualid),
            "Policy Qualifier User Notice");
  EXPECT_EQ(qualifier->d.usernotice->noticeref->organization->type,
            V_ASN1_UTF8STRING);
  EXPECT_EQ(ASN1StringAsView(qualifier->d.usernotice->noticeref->organization),
            "organization");
  EXPECT_EQ(sk_ASN1_INTEGER_num(qualifier->d.usernotice->noticeref->noticenos),
            3u);
  EXPECT_EQ(qualifier->d.usernotice->exptext, nullptr);

  // Sample id-qt-unotice with explicitText.
  qualifier = sk_POLICYQUALINFO_value(policy->qualifiers, 3);
  EXPECT_EQ(ASN1ObjectToString(qualifier->pqualid),
            "Policy Qualifier User Notice");
  EXPECT_EQ(qualifier->d.usernotice->noticeref, nullptr);
  EXPECT_EQ(qualifier->d.usernotice->exptext->type, V_ASN1_UTF8STRING);
  EXPECT_EQ(ASN1StringAsView(qualifier->d.usernotice->exptext), "explicitText");

  // Sample id-qt-unotice with both.
  qualifier = sk_POLICYQUALINFO_value(policy->qualifiers, 4);
  EXPECT_EQ(ASN1ObjectToString(qualifier->pqualid),
            "Policy Qualifier User Notice");
  EXPECT_EQ(qualifier->d.usernotice->noticeref->organization->type,
            V_ASN1_UTF8STRING);
  EXPECT_EQ(ASN1StringAsView(qualifier->d.usernotice->noticeref->organization),
            "organization");
  EXPECT_EQ(sk_ASN1_INTEGER_num(qualifier->d.usernotice->noticeref->noticenos),
            3u);
  EXPECT_EQ(qualifier->d.usernotice->exptext->type, V_ASN1_UTF8STRING);
  EXPECT_EQ(ASN1StringAsView(qualifier->d.usernotice->exptext), "explicitText");

  // Variations with all the allowed DisplayText string types.
  qualifier = sk_POLICYQUALINFO_value(policy->qualifiers, 5);
  EXPECT_EQ(ASN1ObjectToString(qualifier->pqualid),
            "Policy Qualifier User Notice");
  EXPECT_EQ(qualifier->d.usernotice->noticeref->organization->type,
            V_ASN1_IA5STRING);
  EXPECT_EQ(ASN1StringAsView(qualifier->d.usernotice->noticeref->organization),
            "organization");
  EXPECT_EQ(sk_ASN1_INTEGER_num(qualifier->d.usernotice->noticeref->noticenos),
            3u);
  EXPECT_EQ(qualifier->d.usernotice->exptext->type, V_ASN1_IA5STRING);
  EXPECT_EQ(ASN1StringAsView(qualifier->d.usernotice->exptext), "explicitText");

  qualifier = sk_POLICYQUALINFO_value(policy->qualifiers, 6);
  EXPECT_EQ(ASN1ObjectToString(qualifier->pqualid),
            "Policy Qualifier User Notice");
  EXPECT_EQ(qualifier->d.usernotice->noticeref->organization->type,
            V_ASN1_VISIBLESTRING);
  EXPECT_EQ(ASN1StringAsView(qualifier->d.usernotice->noticeref->organization),
            "organization");
  EXPECT_EQ(sk_ASN1_INTEGER_num(qualifier->d.usernotice->noticeref->noticenos),
            3u);
  EXPECT_EQ(qualifier->d.usernotice->exptext->type, V_ASN1_VISIBLESTRING);
  EXPECT_EQ(ASN1StringAsView(qualifier->d.usernotice->exptext), "explicitText");

  qualifier = sk_POLICYQUALINFO_value(policy->qualifiers, 7);
  EXPECT_EQ(ASN1ObjectToString(qualifier->pqualid),
            "Policy Qualifier User Notice");
  EXPECT_EQ(qualifier->d.usernotice->noticeref->organization->type,
            V_ASN1_BMPSTRING);
  EXPECT_EQ(ASN1StringToUTF8(qualifier->d.usernotice->noticeref->organization),
            "organization");
  EXPECT_EQ(sk_ASN1_INTEGER_num(qualifier->d.usernotice->noticeref->noticenos),
            3u);
  EXPECT_EQ(qualifier->d.usernotice->exptext->type, V_ASN1_BMPSTRING);
  EXPECT_EQ(ASN1StringToUTF8(qualifier->d.usernotice->exptext), "explicitText");

  // A custom qualifier should be parsed as an ANY type.
  qualifier = sk_POLICYQUALINFO_value(policy->qualifiers, 8);
  EXPECT_EQ(ASN1ObjectToString(qualifier->pqualid),
            "1.2.840.113554.4.1.72585.2.2");
  EXPECT_EQ(qualifier->d.other->type, V_ASN1_IA5STRING);
  EXPECT_EQ(ASN1StringAsView(qualifier->d.other->value.ia5string), "");

  qualifier = sk_POLICYQUALINFO_value(policy->qualifiers, 9);
  EXPECT_EQ(ASN1ObjectToString(qualifier->pqualid),
            "1.2.840.113554.4.1.72585.2.2");
  EXPECT_EQ(qualifier->d.other->type, V_ASN1_BOOLEAN);
  EXPECT_EQ(qualifier->d.other->value.boolean, ASN1_BOOLEAN_TRUE);

  qualifier = sk_POLICYQUALINFO_value(policy->qualifiers, 10);
  EXPECT_EQ(ASN1ObjectToString(qualifier->pqualid),
            "1.2.840.113554.4.1.72585.2.2");
  EXPECT_EQ(qualifier->d.other->type, V_ASN1_OTHER);
  EXPECT_EQ(Bytes(ASN1StringAsBytes(qualifier->d.other->value.asn1_string)),
            Bytes(std::vector<uint8_t>{0x80, 0x00}));

  // The object should roundtrip to the original input.
  uint8_t *der = nullptr;
  int der_len = i2d_CERTIFICATEPOLICIES(policies.get(), &der);
  EXPECT_GT(der_len, 0);
  UniquePtr<uint8_t> free_der(der);
  EXPECT_EQ(Bytes(der, der_len), Bytes(kTestPolicies));

  // Trailing data should be rejected.
  TestDERTrailingData(
      kTestPolicies, [](Span<const uint8_t> rewritten, size_t n) {
        SCOPED_TRACE(n);
        SCOPED_TRACE(EncodeHex(rewritten));
        const uint8_t *p = rewritten.data();
        EXPECT_FALSE(UniquePtr<CERTIFICATEPOLICIES>(
            d2i_CERTIFICATEPOLICIES(nullptr, &p, rewritten.size())));
      });

  // Test additional invalid inputs.
  const std::vector<uint8_t> kInvalidInputs[] = {
      // The parser should attempt to parse the value for recognized types and
      // reject if invalid:
      // A qualifier of type id-qt-unotice with a UTF8String instead of
      // UserNotice.
      {0x30, 0x25, 0x30, 0x23, 0x06, 0x0d, 0x2a, 0x86, 0x48, 0x86,
       0xf7, 0x12, 0x04, 0x01, 0x84, 0xb7, 0x09, 0x02, 0x01, 0x30,
       0x12, 0x30, 0x10, 0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05,
       0x07, 0x02, 0x02, 0x0c, 0x04, 0x6e, 0x6f, 0x70, 0x65},
      // A qualifier of type id-qt-cps with a UTF8String instead of IA5String.
      {0x30, 0x25, 0x30, 0x23, 0x06, 0x0d, 0x2a, 0x86, 0x48, 0x86,
       0xf7, 0x12, 0x04, 0x01, 0x84, 0xb7, 0x09, 0x02, 0x01, 0x30,
       0x12, 0x30, 0x10, 0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05,
       0x07, 0x02, 0x01, 0x0c, 0x04, 0x6e, 0x6f, 0x70, 0x65},
  };
  for (const auto &in : kInvalidInputs) {
    SCOPED_TRACE(EncodeHex(in));
    const uint8_t *p = in.data();
    EXPECT_FALSE(UniquePtr<CERTIFICATEPOLICIES>(
        d2i_CERTIFICATEPOLICIES(nullptr, &p, in.size())));
    EXPECT_TRUE(ErrorsAreAndClear({{ERR_LIB_ASN1, std::nullopt}}));
  }
}

TEST(X509ExtensionTest, ParseCRLDistributionPoints) {
  // CRL distribution points are very complex. This input is a sequence of
  // three CRL distribution points that try to exercise various cases.
  static const uint8_t kInput[] = {
      0x30, 0x81, 0x87, 0x30, 0x28, 0xa0, 0x26, 0xa0, 0x24, 0x86, 0x10, 0x68,
      0x74, 0x74, 0x70, 0x3a, 0x2f, 0x2f, 0x61, 0x2e, 0x65, 0x78, 0x61, 0x6d,
      0x70, 0x6c, 0x65, 0x86, 0x10, 0x68, 0x74, 0x74, 0x70, 0x3a, 0x2f, 0x2f,
      0x62, 0x2e, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x30, 0x30, 0xa0,
      0x0f, 0xa1, 0x0d, 0x30, 0x0b, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c, 0x04,
      0x54, 0x65, 0x73, 0x74, 0x81, 0x02, 0x05, 0x60, 0xa2, 0x19, 0xa4, 0x17,
      0x30, 0x15, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c,
      0x0a, 0x43, 0x52, 0x4c, 0x20, 0x49, 0x73, 0x73, 0x75, 0x65, 0x72, 0x30,
      0x29, 0xa2, 0x27, 0x86, 0x12, 0x68, 0x74, 0x74, 0x70, 0x3a, 0x2f, 0x2f,
      0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x2e, 0x63, 0x6f, 0x6d, 0x82,
      0x0b, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x2e, 0x63, 0x6f, 0x6d,
      0x87, 0x04, 0x7f, 0x00, 0x00, 0x01};
  const uint8_t *inp = kInput;
  UniquePtr<CRL_DIST_POINTS> crldp(
      d2i_CRL_DIST_POINTS(nullptr, &inp, sizeof(kInput)));
  ASSERT_TRUE(crldp);
  EXPECT_EQ(inp, kInput + sizeof(kInput));

  ASSERT_EQ(sk_DIST_POINT_num(crldp.get()), 3u);

  // A distribution point with two URIs.
  const DIST_POINT *dp1 = sk_DIST_POINT_value(crldp.get(), 0);
  ASSERT_NE(dp1->distpoint, nullptr);
  ASSERT_EQ(dp1->distpoint->type, 0);  // fullName
  ASSERT_NE(dp1->distpoint->name.fullname, nullptr);
  ASSERT_EQ(sk_GENERAL_NAME_num(dp1->distpoint->name.fullname), 2u);

  const GENERAL_NAME *gn1_1 =
      sk_GENERAL_NAME_value(dp1->distpoint->name.fullname, 0);
  ASSERT_EQ(gn1_1->type, GEN_URI);
  EXPECT_EQ(ASN1StringAsView(gn1_1->d.uniformResourceIdentifier),
            "http://a.example");

  const GENERAL_NAME *gn1_2 =
      sk_GENERAL_NAME_value(dp1->distpoint->name.fullname, 1);
  ASSERT_EQ(gn1_2->type, GEN_URI);
  EXPECT_EQ(ASN1StringAsView(gn1_2->d.uniformResourceIdentifier),
            "http://b.example");

  EXPECT_EQ(dp1->reasons, nullptr);
  EXPECT_EQ(dp1->CRLissuer, nullptr);

  // A distribution point relative to the CRL issuer, some reason flags, and a
  // CRL issuer
  const DIST_POINT *dp2 = sk_DIST_POINT_value(crldp.get(), 1);
  ASSERT_NE(dp2->distpoint, nullptr);
  ASSERT_EQ(dp2->distpoint->type, 1);  // relativename
  ASSERT_NE(dp2->distpoint->name.relativename, nullptr);
  ASSERT_EQ(sk_X509_NAME_ENTRY_num(dp2->distpoint->name.relativename), 1u);

  const X509_NAME_ENTRY *entry =
      sk_X509_NAME_ENTRY_value(dp2->distpoint->name.relativename, 0);
  EXPECT_EQ(OBJ_obj2nid(X509_NAME_ENTRY_get_object(entry)), NID_commonName);
  EXPECT_EQ(ASN1StringAsView(X509_NAME_ENTRY_get_data(entry)), "Test");

  ASSERT_NE(dp2->reasons, nullptr);
  UniquePtr<ASN1_BIT_STRING> expected_reasons(ASN1_BIT_STRING_new());
  ASSERT_TRUE(expected_reasons);
  ASSERT_TRUE(ASN1_BIT_STRING_set_bit(expected_reasons.get(),
                                      CRL_REASON_KEY_COMPROMISE, 1));
  ASSERT_TRUE(ASN1_BIT_STRING_set_bit(expected_reasons.get(),
                                      CRL_REASON_CA_COMPROMISE, 1));
  EXPECT_EQ(ASN1_STRING_cmp(dp2->reasons, expected_reasons.get()), 0);

  ASSERT_NE(dp2->CRLissuer, nullptr);
  ASSERT_EQ(sk_GENERAL_NAME_num(dp2->CRLissuer), 1u);
  const GENERAL_NAME *issuer_gn = sk_GENERAL_NAME_value(dp2->CRLissuer, 0);
  ASSERT_EQ(issuer_gn->type, GEN_DIRNAME);
  ASSERT_NE(issuer_gn->d.directoryName, nullptr);
  ASSERT_EQ(X509_NAME_entry_count(issuer_gn->d.directoryName), 1);
  const X509_NAME_ENTRY *issuer_entry =
      X509_NAME_get_entry(issuer_gn->d.directoryName, 0);
  EXPECT_EQ(OBJ_obj2nid(X509_NAME_ENTRY_get_object(issuer_entry)),
            NID_commonName);
  EXPECT_EQ(ASN1StringAsView(X509_NAME_ENTRY_get_data(issuer_entry)),
            "CRL Issuer");

  // Only a CRL issuer with various general names.
  const DIST_POINT *dp3 = sk_DIST_POINT_value(crldp.get(), 2);
  EXPECT_EQ(dp3->distpoint, nullptr);
  EXPECT_EQ(dp3->reasons, nullptr);
  ASSERT_NE(dp3->CRLissuer, nullptr);
  ASSERT_EQ(sk_GENERAL_NAME_num(dp3->CRLissuer), 3u);

  const GENERAL_NAME *gn3_1 = sk_GENERAL_NAME_value(dp3->CRLissuer, 0);
  ASSERT_EQ(gn3_1->type, GEN_URI);
  EXPECT_EQ(ASN1StringAsView(gn3_1->d.uniformResourceIdentifier),
            "http://example.com");

  const GENERAL_NAME *gn3_2 = sk_GENERAL_NAME_value(dp3->CRLissuer, 1);
  ASSERT_EQ(gn3_2->type, GEN_DNS);
  EXPECT_EQ(ASN1StringAsView(gn3_2->d.dNSName), "example.com");

  const GENERAL_NAME *gn3_3 = sk_GENERAL_NAME_value(dp3->CRLissuer, 2);
  ASSERT_EQ(gn3_3->type, GEN_IPADD);
  EXPECT_EQ(Bytes(ASN1StringAsBytes(gn3_3->d.iPAddress)),
            Bytes(std::vector<uint8_t>{127, 0, 0, 1}));

  // The object should roundtrip to the original input.
  uint8_t *der = nullptr;
  int der_len = i2d_CRL_DIST_POINTS(crldp.get(), &der);
  EXPECT_GT(der_len, 0);
  UniquePtr<uint8_t> free_der(der);
  EXPECT_EQ(Bytes(der, der_len), Bytes(kInput));

  // Trailing data should be rejected.
  TestDERTrailingData(kInput, [](Span<const uint8_t> rewritten, size_t n) {
    SCOPED_TRACE(n);
    SCOPED_TRACE(EncodeHex(rewritten));
    const uint8_t *p = rewritten.data();
    EXPECT_FALSE(UniquePtr<CRL_DIST_POINTS>(
        d2i_CRL_DIST_POINTS(nullptr, &p, rewritten.size())));
  });
}

}  // namespace
BSSL_NAMESPACE_END
