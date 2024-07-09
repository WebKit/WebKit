// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BSSL_PKI_CERTIFICATE_POLICIES_H_
#define BSSL_PKI_CERTIFICATE_POLICIES_H_

#include <stdint.h>
#include <vector>


#include <optional>
#include "input.h"

namespace bssl {

class CertErrors;

// Returns the DER-encoded OID, without tag or length, of the anyPolicy
// certificate policy defined in RFC 5280 section 4.2.1.4.
inline constexpr uint8_t kAnyPolicyOid[] = {0x55, 0x1D, 0x20, 0x00};

// From RFC 5280:
//
//     id-ce-inhibitAnyPolicy OBJECT IDENTIFIER ::=  { id-ce 54 }
//
// In dotted notation: 2.5.29.54
inline constexpr uint8_t kInhibitAnyPolicyOid[] = {0x55, 0x1d, 0x36};

// From RFC 5280:
//
//     id-ce-policyMappings OBJECT IDENTIFIER ::=  { id-ce 33 }
//
// In dotted notation: 2.5.29.33
inline constexpr uint8_t kPolicyMappingsOid[] = {0x55, 0x1d, 0x21};

// -- policyQualifierIds for Internet policy qualifiers
//
// id-qt          OBJECT IDENTIFIER ::=  { id-pkix 2 }
// id-qt-cps      OBJECT IDENTIFIER ::=  { id-qt 1 }
//
// In dotted decimal form: 1.3.6.1.5.5.7.2.1
inline constexpr uint8_t kCpsPointerId[] = {0x2b, 0x06, 0x01, 0x05,
                                            0x05, 0x07, 0x02, 0x01};

// id-qt-unotice  OBJECT IDENTIFIER ::=  { id-qt 2 }
//
// In dotted decimal form: 1.3.6.1.5.5.7.2.2
inline constexpr uint8_t kUserNoticeId[] = {0x2b, 0x06, 0x01, 0x05,
                                            0x05, 0x07, 0x02, 0x02};

struct PolicyQualifierInfo {
  der::Input qualifier_oid;
  der::Input qualifier;
};

struct OPENSSL_EXPORT PolicyInformation {
  PolicyInformation();
  ~PolicyInformation();
  PolicyInformation(const PolicyInformation &);
  PolicyInformation(PolicyInformation &&);

  der::Input policy_oid;
  std::vector<PolicyQualifierInfo> policy_qualifiers;
};

// Parses a certificatePolicies extension and stores the policy information
// |*policies|, in the order presented in |extension_value|.
//
// Returns true on success. On failure returns false and may add errors to
// |errors|, which must be non-null.
//
// The values in |policies| are only valid as long as |extension_value| is (as
// it references data).
OPENSSL_EXPORT bool ParseCertificatePoliciesExtension(
    der::Input extension_value, std::vector<PolicyInformation> *policies,
    CertErrors *errors);

// Parses a certificatePolicies extension and stores the policy OIDs in
// |*policy_oids|, in sorted order.
//
// If policyQualifiers for User Notice or CPS are present then they are
// ignored (RFC 5280 section 4.2.1.4 says "optional qualifiers, which MAY
// be present, are not expected to change the definition of the policy."
//
// If a policy qualifier other than User Notice/CPS is present, parsing
// will fail if |fail_parsing_unknown_qualifier_oids| was set to true,
// otherwise the unrecognized qualifiers wil be skipped and not parsed
// any further.
//
// Returns true on success. On failure returns false and may add errors to
// |errors|, which must be non-null.
//
// The values in |policy_oids| are only valid as long as |extension_value| is
// (as it references data).
OPENSSL_EXPORT bool ParseCertificatePoliciesExtensionOids(
    der::Input extension_value, bool fail_parsing_unknown_qualifier_oids,
    std::vector<der::Input> *policy_oids, CertErrors *errors);

struct ParsedPolicyConstraints {
  std::optional<uint8_t> require_explicit_policy;

  std::optional<uint8_t> inhibit_policy_mapping;
};

// Parses a PolicyConstraints SEQUENCE as defined by RFC 5280. Returns true on
// success, and sets |out|.
[[nodiscard]] OPENSSL_EXPORT bool ParsePolicyConstraints(
    der::Input policy_constraints_tlv, ParsedPolicyConstraints *out);

// Parses an InhibitAnyPolicy as defined by RFC 5280. Returns num certs on
// success, or empty if parser fails.
[[nodiscard]] OPENSSL_EXPORT std::optional<uint8_t> ParseInhibitAnyPolicy(
    der::Input inhibit_any_policy_tlv);

struct ParsedPolicyMapping {
  der::Input issuer_domain_policy;
  der::Input subject_domain_policy;
};

// Parses a PolicyMappings SEQUENCE as defined by RFC 5280. Returns true on
// success, and sets |mappings|.
[[nodiscard]] OPENSSL_EXPORT bool ParsePolicyMappings(
    der::Input policy_mappings_tlv, std::vector<ParsedPolicyMapping> *mappings);

}  // namespace bssl

#endif  // BSSL_PKI_CERTIFICATE_POLICIES_H_
