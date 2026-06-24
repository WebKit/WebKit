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

#include "trust_store.h"

#include <cassert>
#include <cstring>
#include <optional>

#include <openssl/base.h>
#include <openssl/bytestring.h>
#include <openssl/mem.h>
#include <openssl/pool.h>

#include "merkle_tree.h"
#include "parse_certificate.h"
#include "parsed_certificate.h"
#include "string_util.h"

BSSL_NAMESPACE_BEGIN

namespace {

constexpr char kUnspecifiedStr[] = "UNSPECIFIED";
constexpr char kDistrustedStr[] = "DISTRUSTED";
constexpr char kTrustedAnchorStr[] = "TRUSTED_ANCHOR";
constexpr char kTrustedAnchorOrLeafStr[] = "TRUSTED_ANCHOR_OR_LEAF";
constexpr char kTrustedLeafStr[] = "TRUSTED_LEAF";

constexpr char kEnforceAnchorExpiry[] = "enforce_anchor_expiry";
constexpr char kEnforceAnchorConstraints[] = "enforce_anchor_constraints";
constexpr char kRequireAnchorBasicConstraints[] =
    "require_anchor_basic_constraints";
constexpr char kRequireLeafSelfsigned[] = "require_leaf_selfsigned";

}  // namespace

bool CertificateTrust::IsTrustAnchor() const {
  switch (type) {
    case CertificateTrustType::DISTRUSTED:
    case CertificateTrustType::UNSPECIFIED:
    case CertificateTrustType::TRUSTED_LEAF:
      return false;
    case CertificateTrustType::TRUSTED_ANCHOR:
    case CertificateTrustType::TRUSTED_ANCHOR_OR_LEAF:
      return true;
  }

  assert(0);  // NOTREACHED
  return false;
}

bool CertificateTrust::IsTrustLeaf() const {
  switch (type) {
    case CertificateTrustType::TRUSTED_LEAF:
    case CertificateTrustType::TRUSTED_ANCHOR_OR_LEAF:
      return true;
    case CertificateTrustType::DISTRUSTED:
    case CertificateTrustType::UNSPECIFIED:
    case CertificateTrustType::TRUSTED_ANCHOR:
      return false;
  }

  assert(0);  // NOTREACHED
  return false;
}

bool CertificateTrust::IsDistrusted() const {
  switch (type) {
    case CertificateTrustType::DISTRUSTED:
      return true;
    case CertificateTrustType::UNSPECIFIED:
    case CertificateTrustType::TRUSTED_ANCHOR:
    case CertificateTrustType::TRUSTED_ANCHOR_OR_LEAF:
    case CertificateTrustType::TRUSTED_LEAF:
      return false;
  }

  assert(0);  // NOTREACHED
  return false;
}

bool CertificateTrust::HasUnspecifiedTrust() const {
  switch (type) {
    case CertificateTrustType::UNSPECIFIED:
      return true;
    case CertificateTrustType::DISTRUSTED:
    case CertificateTrustType::TRUSTED_ANCHOR:
    case CertificateTrustType::TRUSTED_ANCHOR_OR_LEAF:
    case CertificateTrustType::TRUSTED_LEAF:
      return false;
  }

  assert(0);  // NOTREACHED
  return true;
}

std::string CertificateTrust::ToDebugString() const {
  std::string result;
  switch (type) {
    case CertificateTrustType::UNSPECIFIED:
      result = kUnspecifiedStr;
      break;
    case CertificateTrustType::DISTRUSTED:
      result = kDistrustedStr;
      break;
    case CertificateTrustType::TRUSTED_ANCHOR:
      result = kTrustedAnchorStr;
      break;
    case CertificateTrustType::TRUSTED_ANCHOR_OR_LEAF:
      result = kTrustedAnchorOrLeafStr;
      break;
    case CertificateTrustType::TRUSTED_LEAF:
      result = kTrustedLeafStr;
      break;
  }
  if (enforce_anchor_expiry) {
    result += '+';
    result += kEnforceAnchorExpiry;
  }
  if (enforce_anchor_constraints) {
    result += '+';
    result += kEnforceAnchorConstraints;
  }
  if (require_anchor_basic_constraints) {
    result += '+';
    result += kRequireAnchorBasicConstraints;
  }
  if (require_leaf_selfsigned) {
    result += '+';
    result += kRequireLeafSelfsigned;
  }
  return result;
}

// static
std::optional<CertificateTrust> CertificateTrust::FromDebugString(
    const std::string &trust_string) {
  std::vector<std::string_view> split =
      string_util::SplitString(trust_string, '+');

  if (split.empty()) {
    return std::nullopt;
  }

  CertificateTrust trust;

  if (string_util::IsEqualNoCase(split[0], kUnspecifiedStr)) {
    trust = CertificateTrust::ForUnspecified();
  } else if (string_util::IsEqualNoCase(split[0], kDistrustedStr)) {
    trust = CertificateTrust::ForDistrusted();
  } else if (string_util::IsEqualNoCase(split[0], kTrustedAnchorStr)) {
    trust = CertificateTrust::ForTrustAnchor();
  } else if (string_util::IsEqualNoCase(split[0], kTrustedAnchorOrLeafStr)) {
    trust = CertificateTrust::ForTrustAnchorOrLeaf();
  } else if (string_util::IsEqualNoCase(split[0], kTrustedLeafStr)) {
    trust = CertificateTrust::ForTrustedLeaf();
  } else {
    return std::nullopt;
  }

  for (auto i = ++split.begin(); i != split.end(); ++i) {
    if (string_util::IsEqualNoCase(*i, kEnforceAnchorExpiry)) {
      trust = trust.WithEnforceAnchorExpiry();
    } else if (string_util::IsEqualNoCase(*i, kEnforceAnchorConstraints)) {
      trust = trust.WithEnforceAnchorConstraints();
    } else if (string_util::IsEqualNoCase(*i, kRequireAnchorBasicConstraints)) {
      trust = trust.WithRequireAnchorBasicConstraints();
    } else if (string_util::IsEqualNoCase(*i, kRequireLeafSelfsigned)) {
      trust = trust.WithRequireLeafSelfSigned();
    } else {
      return std::nullopt;
    }
  }

  return trust;
}

MTCAnchor::MTCAnchor(bssl::Span<const uint8_t> log_id,
                     Span<const TrustedSubtree> trusted_subtrees)
    : log_id_(log_id.begin(), log_id.end()),
      trusted_subtrees_(trusted_subtrees.begin(), trusted_subtrees.end()) {
  CreateSyntheticCert(log_id);
}

bool MTCAnchor::IsValid() const {
  if (!synthetic_cert_) {
    return false;
  }
  Subtree min_subtree;
  for (const auto &subtree : trusted_subtrees_) {
    if (!subtree.range.IsValid() || subtree.range < min_subtree) {
      return false;
    }
    min_subtree = subtree.range;
  }
  return true;
}

der::Input MTCAnchor::NormalizedSubject() const {
  return synthetic_cert_->normalized_subject();
}

CertificateTrust MTCAnchor::CertTrust() const {
  CertificateTrust trust;
  trust.type = CertificateTrustType::TRUSTED_ANCHOR;
  return trust;
}

std::shared_ptr<const ParsedCertificate> MTCAnchor::AsCert() const {
  return synthetic_cert_;
}

std::optional<TreeHashConstSpan> MTCAnchor::SubtreeHash(
    Subtree target_range) const {
  auto it = std::lower_bound(
      trusted_subtrees_.begin(), trusted_subtrees_.end(), target_range,
      [](const TrustedSubtree &subtree, Subtree range) -> bool {
        return subtree.range < range;
      });
  if (it == trusted_subtrees_.end() || it->range != target_range) {
    return std::nullopt;
  }
  return it->hash;
}

void MTCAnchor::CreateSyntheticCert(bssl::Span<const uint8_t> log_id) {
  bssl::ScopedCBB cbb;
  CBB cert, tbs_cert, version, validity, subject_seq, subject_set, subject_log,
      signature;
  BSSL_CHECK(CBB_init(cbb.get(), 0));
  BSSL_CHECK(CBB_add_asn1(cbb.get(), &cert, CBS_ASN1_SEQUENCE));
  // Fill the TBSCertificate
  BSSL_CHECK(CBB_add_asn1(&cert, &tbs_cert, CBS_ASN1_SEQUENCE));
  // version
  BSSL_CHECK(
      CBB_add_asn1(&tbs_cert, &version,
                   CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 0));
  BSSL_CHECK(CBB_add_asn1_uint64(&version, 2));  // 2 is the enum value for v3
  // serialNumber
  BSSL_CHECK(CBB_add_asn1_uint64(&tbs_cert, 0));
  // signature
  BSSL_CHECK(CBB_add_asn1_element(&tbs_cert, CBS_ASN1_SEQUENCE, nullptr, 0));
  // issuer
  BSSL_CHECK(CBB_add_asn1_element(&tbs_cert, CBS_ASN1_SEQUENCE, nullptr, 0));
  // validity
  BSSL_CHECK(CBB_add_asn1(&tbs_cert, &validity, CBS_ASN1_SEQUENCE));
  static const uint8_t notBefore[] = "00000101000000Z";
  static const uint8_t notAfter[] = "99991231235960Z";
  BSSL_CHECK(CBB_add_asn1_element(&validity, CBS_ASN1_GENERALIZEDTIME,
                                  notBefore, sizeof(notBefore) - 1));
  BSSL_CHECK(CBB_add_asn1_element(&validity, CBS_ASN1_GENERALIZEDTIME, notAfter,
                                  sizeof(notAfter) - 1));

  // subject
  BSSL_CHECK(CBB_add_asn1(&tbs_cert, &subject_seq, CBS_ASN1_SEQUENCE));
  BSSL_CHECK(CBB_add_asn1(&subject_seq, &subject_set, CBS_ASN1_SET));
  BSSL_CHECK(CBB_add_asn1(&subject_set, &subject_log, CBS_ASN1_SEQUENCE));

  // Section 5.2: Use OID 1.3.6.1.4.1.44363.47.1 as the attribute type for the
  // log ID's name. Note that this is the early experimentation OID in the
  // draft rather than the real value of `id-rdna-trustAnchorID`.
  static uint8_t log_attr_oid[] = {0x2b, 0x06, 0x01, 0x04, 0x01,
                                   0x82, 0xda, 0x4b, 0x2f, 0x01};
  BSSL_CHECK(CBB_add_asn1_element(&subject_log, CBS_ASN1_OBJECT, log_attr_oid,
                                  sizeof(log_attr_oid)));

  // Section 5.2's note for initial experimentation also says to use UTF8String
  // to represent the attribute's value rather than RELATIVE-OID.

  //  Convert the relative OID `log_id` to a string. This can fail.
  CBS log_id_oid(log_id);
  bssl::UniquePtr<char> log_id_text(CBS_asn1_relative_oid_to_text(&log_id_oid));
  if (!log_id_text) {
    return;
  }
  BSSL_CHECK(
      CBB_add_asn1_element(&subject_log, CBS_ASN1_UTF8STRING,
                           reinterpret_cast<const uint8_t *>(log_id_text.get()),
                           strlen(log_id_text.get())));

  // subjectPublicKeyInfo
  BSSL_CHECK(CBB_add_asn1_element(&tbs_cert, CBS_ASN1_SEQUENCE, nullptr, 0));

  // finish the outer Certificate with the signatureAlgorithm and signature.
  BSSL_CHECK(CBB_add_asn1_element(&cert, CBS_ASN1_SEQUENCE, nullptr, 0));
  BSSL_CHECK(CBB_add_asn1(&cert, &signature, CBS_ASN1_BITSTRING));
  BSSL_CHECK(CBB_add_u8(&signature, 0));

  BSSL_CHECK(CBB_flush(cbb.get()));

  synthetic_cert_ = ParsedCertificate::Create(
      bssl::UniquePtr<CRYPTO_BUFFER>(
          CRYPTO_BUFFER_new(CBB_data(cbb.get()), CBB_len(cbb.get()), nullptr)),
      ParseCertificateOptions{}, nullptr);
}

TrustAnchor::TrustAnchor(CertificateTrust trust)
    : TrustAnchor(trust, nullptr) {}
TrustAnchor::TrustAnchor(CertificateTrust trust,
                         std::shared_ptr<const bssl::MTCAnchor> mtc_anchor)
    : trust_(trust), mtc_anchor_(mtc_anchor) {}

void TrustAnchor::OverrideCertTrust(CertificateTrust new_trust) {
  trust_ = new_trust;
}

CertificateTrust TrustAnchor::CertTrust() const { return trust_; }

std::shared_ptr<const MTCAnchor> TrustAnchor::MTCAnchor() const {
  return mtc_anchor_;
}

TrustStore::TrustStore() = default;

std::shared_ptr<const MTCAnchor> TrustStore::GetTrustedMTCIssuerOf(
    const ParsedCertificate *cert) {
  return nullptr;
}

void TrustStore::AsyncGetIssuersOf(const ParsedCertificate *cert,
                                   std::unique_ptr<Request> *out_req) {
  out_req->reset();
}

BSSL_NAMESPACE_END
