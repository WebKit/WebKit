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

#ifndef BSSL_PKI_TRUST_STORE_H_
#define BSSL_PKI_TRUST_STORE_H_

#include <array>
#include <memory>
#include <optional>

#include <openssl/base.h>
#include <openssl/sha2.h>

#include "cert_issuer_source.h"
#include "merkle_tree.h"
#include "parsed_certificate.h"

BSSL_NAMESPACE_BEGIN

enum class CertificateTrustType {
  // This certificate is explicitly blocked (distrusted).
  DISTRUSTED,

  // The trustedness of this certificate is unknown (inherits trust from
  // its issuer).
  UNSPECIFIED,

  // This certificate is a trust anchor (as defined by RFC 5280).
  TRUSTED_ANCHOR,

  // This certificate can be used as a trust anchor (as defined by RFC 5280) or
  // a trusted leaf, depending on context.
  TRUSTED_ANCHOR_OR_LEAF,

  // This certificate is a directly trusted leaf.
  TRUSTED_LEAF,

  LAST = TRUSTED_ANCHOR
};

struct OPENSSL_EXPORT TrustedSubtree {
  Subtree range;
  std::array<uint8_t, SHA256_DIGEST_LENGTH> hash;
};

// Describes the level of trust in a certificate.
struct OPENSSL_EXPORT CertificateTrust {
  static constexpr CertificateTrust ForTrustAnchor() {
    CertificateTrust result;
    result.type = CertificateTrustType::TRUSTED_ANCHOR;
    return result;
  }

  static constexpr CertificateTrust ForTrustAnchorOrLeaf() {
    CertificateTrust result;
    result.type = CertificateTrustType::TRUSTED_ANCHOR_OR_LEAF;
    return result;
  }

  static constexpr CertificateTrust ForTrustedLeaf() {
    CertificateTrust result;
    result.type = CertificateTrustType::TRUSTED_LEAF;
    return result;
  }

  static constexpr CertificateTrust ForUnspecified() {
    CertificateTrust result;
    return result;
  }

  static constexpr CertificateTrust ForDistrusted() {
    CertificateTrust result;
    result.type = CertificateTrustType::DISTRUSTED;
    return result;
  }

  constexpr CertificateTrust WithEnforceAnchorExpiry(bool value = true) const {
    CertificateTrust result = *this;
    result.enforce_anchor_expiry = value;
    return result;
  }

  constexpr CertificateTrust WithEnforceAnchorConstraints(
      bool value = true) const {
    CertificateTrust result = *this;
    result.enforce_anchor_constraints = value;
    return result;
  }

  constexpr CertificateTrust WithRequireAnchorBasicConstraints(
      bool value = true) const {
    CertificateTrust result = *this;
    result.require_anchor_basic_constraints = value;
    return result;
  }

  constexpr CertificateTrust WithRequireLeafSelfSigned(
      bool value = true) const {
    CertificateTrust result = *this;
    result.require_leaf_selfsigned = value;
    return result;
  }

  bool IsTrustAnchor() const;
  bool IsTrustLeaf() const;
  bool IsDistrusted() const;
  bool HasUnspecifiedTrust() const;

  std::string ToDebugString() const;

  static std::optional<CertificateTrust> FromDebugString(
      const std::string &trust_string);

  // The overall type of trust.
  CertificateTrustType type = CertificateTrustType::UNSPECIFIED;

  // Optionally, enforce extra bits on trust anchors. If these are false, the
  // only fields in a trust anchor certificate that are meaningful are its
  // name and SPKI.
  bool enforce_anchor_expiry = false;
  bool enforce_anchor_constraints = false;
  // Require that X.509v3 trust anchors have a basicConstraints extension.
  // X.509v1 and X.509v2 trust anchors do not support basicConstraints and are
  // not affected.
  // Additionally, this setting only has effect if `enforce_anchor_constraints`
  // is true, which also requires that the extension assert CA=true.
  bool require_anchor_basic_constraints = false;

  // Optionally, require trusted leafs to be self-signed to be trusted.
  bool require_leaf_selfsigned = false;
};

class OPENSSL_EXPORT MTCAnchor {
 public:
  // Create an MTCAnchor for a trusted log with `log_id` containing the DER
  // encoding of the relative OID of the log's ID. The `trusted_subtrees` must
  // be sorted by their subtree ranges.
  MTCAnchor(Span<const uint8_t> log_id,
            Span<const TrustedSubtree> trusted_subtrees);

  // Returns whether this MTCAnchor represents a valid anchor. This function
  // exists because the c'tor inputs could be invalid.
  bool IsValid() const;

  Span<const uint8_t> log_id() const { return log_id_; }
  // TODO(nharper): Move this function to TrustAnchor.
  der::Input NormalizedSubject() const;
  // TODO(nharper): Remove this function in favor of TrustAnchor's version.
  CertificateTrust CertTrust() const;
  // TODO(nharper): Move this function to TrustAnchor.
  std::shared_ptr<const ParsedCertificate> AsCert() const;
  std::optional<TreeHashConstSpan> SubtreeHash(Subtree target_range) const;

 private:
  void CreateSyntheticCert(Span<const uint8_t> log_id);

  std::vector<uint8_t> log_id_;
  std::shared_ptr<const ParsedCertificate> synthetic_cert_;
  std::vector<TrustedSubtree> trusted_subtrees_;
};

// A TrustAnchor contains information about how a trust anchor is trusted and
// what is trusted. It should be used in place of a (ParsedCertificate,
// CertificateTrust) tuple as some trust information, e.g. MTC anchors, is not
// representable by such a tuple.
//
// TODO(nharper): This class is the first step of a large refactor to stop
// representing TrustAnchors as ParsedCertificates. Prior to the introduction of
// Merkle Tree Certs, the information about a trust anchor was split between
// a ParsedCertificate (containing the anchor's Subject name and public key) and
// a CertificateTrust struct (informing when/how the trust anchor is trusted, as
// well as whether any additional constraints should be applied). With the
// introduction of Merkle Tree Certs, some of this information about how to use
// an MTC trust anchor fits in neither place. The eventual goal is that the
// TrustAnchor class is the single place that contains all information about a
// trust anchor, rather than that information being split across multiple
// objects. This will allow a further goal of removing the dependency of
// requiring that a trust anchor be representable as a ParsedCertificate: MTC
// trust anchors only have a ParsedCertificate accessor for compatibility
// reasons, and while draft-ietf-lamps-x509-alg-none improves the efficiency of
// storing trust anchors in X.509 certs, this refactor will allow further
// improvements for classical (non-MTC) roots.
//
// See also internal design doc:
// https://docs.google.com/document/d/1wqHAmZqtF8oJzNObFBm41lzDt2JWRhjM32A3z6ampJE/edit
class OPENSSL_EXPORT TrustAnchor {
 public:
  // Creates a default TrustAnchor with an unspecified CertificateTrust.
  TrustAnchor() = default;

  // Creates a TrustAnchor with no associated MTCAnchor.
  explicit TrustAnchor(CertificateTrust trust);
  TrustAnchor(CertificateTrust trust,
              std::shared_ptr<const MTCAnchor> mtc_anchor);

  // TODO(nharper): Temporarily add ParsedCertificate member and accessor.

  // TODO(nharper): Add accessors to get information about the trust anchor that
  // is currently found in the ParsedCertificate:
  // - `der::Input NormalizedSubject() const;`
  // - `UniquePtr<EVP_PKEY> PublicKey() const;`

  // Modifies the CertificateTrust for this TrustAnchor. Should only be called
  // by path_builder.cc.
  //
  // TODO(nharper): Remove this function as part of the larger TrustAnchor
  // refactor.
  void OverrideCertTrust(CertificateTrust new_trust);
  CertificateTrust CertTrust() const;
  std::shared_ptr<const bssl::MTCAnchor> MTCAnchor() const;

 private:
  CertificateTrust trust_;
  std::shared_ptr<const bssl::MTCAnchor> mtc_anchor_ = nullptr;
};

// Interface for finding intermediates / trust anchors, and testing the
// trustedness of certificates.
class OPENSSL_EXPORT TrustStore : public CertIssuerSource {
 public:
  TrustStore();

  TrustStore(const TrustStore &) = delete;
  TrustStore &operator=(const TrustStore &) = delete;

  // Returns the trusted of `cert`, which must be non-null.
  virtual CertificateTrust GetTrust(const ParsedCertificate *cert) = 0;

  // Returns the TrustAnchor that issued `cert`, if one exists.
  //
  // TODO(nharper): Make this a pure virtual function once all TrustStore
  // implementations have implemented this.
  virtual std::shared_ptr<const MTCAnchor> GetTrustedMTCIssuerOf(
      const ParsedCertificate *cert);

  // Disable async issuers for TrustStore, as it isn't needed.
  void AsyncGetIssuersOf(const ParsedCertificate *cert,
                         std::unique_ptr<Request> *out_req) final;
};

BSSL_NAMESPACE_END

#endif  // BSSL_PKI_TRUST_STORE_H_
