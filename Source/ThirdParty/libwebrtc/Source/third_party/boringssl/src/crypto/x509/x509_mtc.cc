// Copyright 2026 The BoringSSL Authors
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

#include <openssl/asn1.h>
#include <openssl/base.h>
#include <openssl/bytestring.h>
#include <openssl/digest.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/nid.h>
#include <openssl/span.h>
#include <openssl/x509.h>

#include <assert.h>

#include <algorithm>
#include <optional>

#include "../bytestring/internal.h"
#include "../internal.h"
#include "./internal.h"


BSSL_NAMESPACE_BEGIN
namespace {

// Prefix for domain separation to denote a Merkle Tree leaf or interior node.
constexpr uint8_t kMTCLeafDomainSeparator[] = {0x00};
constexpr uint8_t kMTCNodeDomainSeparator[] = {0x01};
// DER encoding of AlgorithmIdentifier for alg-mtcProof with absent parameters.
//   SEQUENCE (10 bytes) {
//     OBJECT IDENTIFIER 1.3.6.1.5.5.7.6.67 (8 bytes)
//   }
constexpr uint8_t kMTCAlgorithmIdentifier[] = {
    0x30,  // SEQUENCE tag
    10,    // tag + len + OID bytes
    0x06,  // OBJECT tag
    8,     // OID length
    OBJ_ENC_alg_mtcProof,
};
// MTCLogEntryType enum value for tbs_cert_entry.
constexpr uint8_t kMTCLogEntryTypeTBSCertEntry[] = {0x00, 0x01};
// ASN.1 tag for encoded OCTET STRING.
constexpr uint8_t kEncodedOctetStringTag[] = {0x04};
// Prefix for domain separation used in CosignedMessage.
constexpr uint8_t kCosignedMessageLabel[12] = {'s', 'u', 'b', 't', 'r',  'e',
                                               'e', '/', 'v', '1', '\n', '\0'};
// String prefix used to construct ASCII representation of trust anchor IDs for
// CosignedMessage fields.
constexpr uint8_t kASCIIOIDPrefix[16] = {'o', 'i', 'd', '/', '1', '.',
                                         '3', '.', '6', '.', '1', '.',
                                         '4', '.', '1', '.'};
// DER encoded OID component found in trust anchor IDs for issuance logs.
constexpr uint8_t kEncOIDLogs = 0x00;
// draft-ietf-tls-trust-anchor-ids-06 limits trust anchor IDs to 32 bytes.
constexpr size_t kMaxTrustAnchorIdBytes = 32;

constexpr CBS_ASN1_TAG kTBSVersionTag =
    CBS_ASN1_CONSTRUCTED | CBS_ASN1_CONTEXT_SPECIFIC | 0;

// HashNode executes one hashing step in the evaluation of an inclusion proof.
void HashNode(Span<uint8_t> out, const EVP_MD *log_hash,
              Span<const uint8_t> left_child, Span<const uint8_t> right_child) {
  assert(out.size() == EVP_MD_size(log_hash));
  assert(left_child.size() == EVP_MD_size(log_hash));
  assert(right_child.size() == EVP_MD_size(log_hash));
  ScopedEVP_MD_CTX ctx;
  EVP_DigestInit_ex(ctx.get(), log_hash, nullptr);
  EVP_DigestUpdate(ctx.get(), kMTCNodeDomainSeparator,
                   sizeof(kMTCNodeDomainSeparator));
  EVP_DigestUpdate(ctx.get(), left_child.data(), left_child.size());
  EVP_DigestUpdate(ctx.get(), right_child.data(), right_child.size());
  EVP_DigestFinal_ex(ctx.get(), out.data(), nullptr);
}

// lsb returns whether the least-significant bit of `n` is set.
inline bool lsb(uint64_t n) { return n & 1; }

// is_mtc_proof returns whether `algor` is an mtcProof AlgorithmIdentifier.
bool is_mtc_proof(const X509_ALGOR *algor) {
  return OBJ_obj2nid(algor->algorithm) == NID_alg_mtcProof &&
         algor->parameter == nullptr;
}

// is_mtc_serial_valid returns whether `serial` is within
// (mtcMinSerial..mtcMaxSerial), which implies the log number is > 0.
bool is_mtc_serial_valid(uint64_t serial) {
  // mtcMinSerial is 2^48. mtcMaxSerial is 2^64-1, the largest number
  // representable as uint64_t, so is not checked explicitly.
  return serial >> 48;
}

// Returns the first hash value (of size `log_hash_size`) remaining in
// `inclusion_proof` and advances `inclusion_proof` past the returned value.
// Returns an empty span if there are no more hash values of the appropriate
// size.
Span<const uint8_t> GetNextValueFromInclusionProof(
    size_t log_hash_size, Span<const uint8_t> &inclusion_proof) {
  if (inclusion_proof.size() < log_hash_size) {
    return Span<const uint8_t>();
  }
  Span<const uint8_t> value = inclusion_proof.first(log_hash_size);
  inclusion_proof = inclusion_proof.subspan(log_hash_size);
  return value;
}

// Writes `data` to the digest in `ctx`, prepending an 8-bit length prefix.
bool DigestUpdateWithU8LengthPrefix(EVP_MD_CTX *ctx, Span<const uint8_t> data) {
  BSSL_CHECK(data.size() < (1u << 8));
  const uint8_t length_prefix = static_cast<uint8_t>(data.size());
  return EVP_DigestUpdate(ctx, &length_prefix, sizeof(length_prefix)) &&
         EVP_DigestUpdate(ctx, data.data(), data.size());
}

// Writes `data` to the digest in `ctx`, prepending a 16-bit length prefix.
bool DigestUpdateWithU16LengthPrefix(EVP_MD_CTX *ctx,
                                     Span<const uint8_t> data) {
  BSSL_CHECK(data.size() < (1u << 16));
  const uint8_t length_prefix[2] = {static_cast<uint8_t>(data.size() >> 8),
                                    static_cast<uint8_t>(data.size())};
  return EVP_DigestUpdate(ctx, length_prefix, sizeof(length_prefix)) &&
         EVP_DigestUpdate(ctx, data.data(), data.size());
}

// Reads the next ASN.1 element from `cbs` and writes it as-is to the digest in
// `ctx`, including ASN.1 header bytes.
bool DigestUpdateNextASN1Element(EVP_MD_CTX *ctx, CBS *cbs) {
  CBS element;
  if (!CBS_get_any_asn1_element(cbs, &element, /*out_tag=*/nullptr,
                                /*out_header_len=*/nullptr)) {
    return false;
  }
  return EVP_DigestUpdate(ctx, CBS_data(&element), CBS_len(&element));
}

// CheckMTCLogEntryExtensions returns whether `extensions` is a well-formed list
// of MTCLogEntryExtension structures, sorted in strictly ascending order by
// extension type (see section 5.2.1). No extension types are currently
// recognized, so the contents are otherwise ignored (see section 12.5).
bool CheckMTCLogEntryExtensions(CBS extensions) {
  std::optional<uint16_t> prev_type;
  while (CBS_len(&extensions) > 0) {
    uint16_t type;
    CBS data;
    if (!CBS_get_u16(&extensions, &type) ||
        !CBS_get_u16_length_prefixed(&extensions, &data)) {
      OPENSSL_PUT_ERROR(X509, X509_R_INVALID_MTC_PROOF);
      return false;
    }
    if (prev_type.has_value() && *prev_type >= type) {
      OPENSSL_PUT_ERROR(X509, X509_R_INVALID_MTC_PROOF);
      return false;
    }
    prev_type = type;
  }
  return true;
}

// MTCCACosigner represents a Merkle Tree CA and its corresponding CA cosigner
// (see draft-ietf-plants-merkle-tree-certs, section 5.5).
// Only id-pe-mtcCertificationAuthority-SHA256 is supported.
class MTCCACosigner {
 public:
  MTCCACosigner() = default;
  ~MTCCACosigner() = default;

  // Init() must be called before this object can be used.
  // Init parses parameters defining an MTC CA cosigner from `x509`, including
  // its MTCCertificationAuthority extension. `pkey` is the public key
  // previously parsed from `x509`. Returns true if successfully initialized an
  // MTCCACosigner representing a valid MTC CA, or false on error.
  bool Init(const X509 *x509, const EVP_PKEY *pkey) {
    // Get and parse the MTCCertificationAuthority extension (see section 5.5).
    int ext_index =
        X509_get_ext_by_NID(x509, NID_pe_mtcCertificationAuthority_SHA256, -1);
    const X509_EXTENSION *ext = X509_get_ext(x509, ext_index);
    if (ext == nullptr || !X509_EXTENSION_get_critical(ext)) {
      OPENSSL_PUT_ERROR(X509, X509_R_INVALID_MTC_CA);
      return false;
    }
    const ASN1_STRING *value = X509_EXTENSION_get_data(ext);
    CBS ext_value, seq;
    CBS_init(&ext_value, ASN1_STRING_get0_data(value),
             ASN1_STRING_length(value));
    if (!CBS_get_asn1(&ext_value, &seq, CBS_ASN1_SEQUENCE) ||  //
        CBS_len(&ext_value) != 0 ||                            //
        !x509_parse_algorithm(&seq, cosign_sigalg_.get()) ||   //
        !CBS_get_asn1_uint64(&seq, &min_serial_) ||            //
        !CBS_get_asn1_uint64(&seq, &max_serial_) ||            //
        CBS_len(&seq) != 0) {
      OPENSSL_PUT_ERROR(ASN1, ASN1_R_DECODE_ERROR);
      return false;
    }
    if (!is_mtc_serial_valid(min_serial_) ||
        !is_mtc_serial_valid(max_serial_)) {
      OPENSSL_PUT_ERROR(X509, X509_R_INVALID_MTC_CA);
      return false;
    }
    // Extract the CA ID from the subject (see section 5.1).
    const X509_NAME *subject = X509_get_subject_name(x509);
    int tai_index =
        X509_NAME_get_index_by_NID(subject, NID_rdna_trustAnchorID, -1);
    if (tai_index < 0 || X509_NAME_entry_count(subject) != 1) {
      OPENSSL_PUT_ERROR(X509, X509_R_INVALID_MTC_CA);
      return false;
    }
    const ASN1_STRING *tai =
        X509_NAME_ENTRY_get_data(X509_NAME_get_entry(subject, tai_index));
    // RELATIVE-OID has no dedicated ASN1_STRING representation, so the value
    // is stored as V_ASN1_OTHER containing the entire element.
    if (ASN1_STRING_type(tai) != V_ASN1_OTHER) {
      OPENSSL_PUT_ERROR(X509, X509_R_WRONG_TYPE);
      return false;
    }
    CBS tai_elem, tai_relative_oid;
    CBS_init(&tai_elem, ASN1_STRING_get0_data(tai), ASN1_STRING_length(tai));
    if (!CBS_get_asn1(&tai_elem, &tai_relative_oid, CBS_ASN1_RELATIVE_OID) ||
        CBS_len(&tai_elem) != 0 ||
        !CBS_is_valid_asn1_relative_oid(&tai_relative_oid)) {
      OPENSSL_PUT_ERROR(ASN1, ASN1_R_INVALID_OBJECT_ENCODING);
      return false;
    }
    if (CBS_len(&tai_relative_oid) > kMaxTrustAnchorIdBytes) {
      OPENSSL_PUT_ERROR(X509, X509_R_INVALID_MTC_CA);
      return false;
    }
    if (!ca_id_enc_oid_.CopyFrom(tai_relative_oid)) {
      OPENSSL_PUT_ERROR(X509, ERR_R_INTERNAL_ERROR);
      return false;
    }

    cosign_pubkey_.reset(EVP_PKEY_dup_ref(pkey));
    return true;
  }

  bool IsSerialInRange(uint64_t serial) const {
    bool ok = min_serial_ <= serial && serial <= max_serial_;
    assert(!ok || is_mtc_serial_valid(serial));
    return ok;
  }

  // Constructs CosignedMessage and computes signature.
  bool VerifyCACosignature(Span<const uint8_t> ca_cosignature,
                           uint64_t log_number, uint64_t subtree_start,
                           uint64_t subtree_end,
                           Span<const uint8_t> expected_subtree_hash) const {
    ScopedCBB cosigned_message;
    if (!CBB_init(cosigned_message.get(), 256) ||
        !CBB_add_bytes(cosigned_message.get(), kCosignedMessageLabel,
                       sizeof(kCosignedMessageLabel)) ||
        !WriteCosignerName(cosigned_message.get()) ||
        !CBB_add_u64(cosigned_message.get(), 0u) ||  // timestamp
        !WriteLogOrigin(cosigned_message.get(), log_number) ||
        !CBB_add_u64(cosigned_message.get(), subtree_start) ||
        !CBB_add_u64(cosigned_message.get(), subtree_end) ||
        !CBB_add_bytes(cosigned_message.get(), expected_subtree_hash.data(),
                       expected_subtree_hash.size())) {
      OPENSSL_PUT_ERROR(X509, ERR_R_INTERNAL_ERROR);
      return false;
    }
    return x509_verify_signature_bytes(cosign_sigalg_.get(), ca_cosignature,
                                       CBBAsSpan(cosigned_message.get()),
                                       cosign_pubkey_.get());
  }

  // IsCACosigner returns whether `trust_anchor_id` equals the cosigner ID
  // of this CA's CA cosigner.
  bool IsCACosigner(Span<const uint8_t> trust_anchor_id) const {
    return trust_anchor_id == ca_id_enc_oid_;
  }

  const EVP_MD *log_hash() const { return EVP_sha256(); }

 private:
  // WriteTaiAscii concatenates the trust anchor ID prefix with the CA ID as a
  // relative OID to get the CA ID as a full OID, then appends any additional
  // OID components from `oid_suffix`, and writes the ASCII string
  // representation of the full trust anchor ID to `out`.
  bool WriteTaiAscii(CBB *out, Span<const uint64_t> oid_suffix = {}) const {
    // Trust anchor IDs such as the CA ID are limited to 32 bytes by spec, but
    // `oid_suffix` may add additional bytes.
    uint8_t id[2 * kMaxTrustAnchorIdBytes];
    CBB id_cbb;
    if (!CBB_init_fixed(&id_cbb, id, sizeof(id)) ||
        !CBB_add_bytes(&id_cbb, ca_id_enc_oid_.data(), ca_id_enc_oid_.size())) {
      return false;
    }
    for (const uint64_t component : oid_suffix) {
      if (!CBB_add_asn1_oid_component(&id_cbb, component)) {
        return false;
      }
    }
    return CBB_add_asn1_relative_oid_from_der_to_text(out, CBB_data(&id_cbb),
                                                      CBB_len(&id_cbb));
  }

  // WriteCosignerName writes the `cosigner_name` field for a CosignedMessage (a
  // prefixed ASCII representation of the CA ID) to `out`.
  bool WriteCosignerName(CBB *out) const {
    CBB cosigner_name;
    return CBB_add_u8_length_prefixed(out, &cosigner_name) &&
           CBB_add_bytes(&cosigner_name, kASCIIOIDPrefix,
                         sizeof(kASCIIOIDPrefix)) &&
           WriteTaiAscii(&cosigner_name) &&  //
           CBB_flush(out);
  }

  // WriteLogOrigin writes the `log_origin` field for a CosignedMessage (a
  // prefixed ASCII representation of the CA's log ID for a given `log_number`)
  // to `out`.
  bool WriteLogOrigin(CBB *out, uint64_t log_number) const {
    const uint64_t log_number_relative_oid[2] = {kEncOIDLogs, log_number};
    CBB log_origin;
    return CBB_add_u8_length_prefixed(out, &log_origin) &&
           CBB_add_bytes(&log_origin, kASCIIOIDPrefix,
                         sizeof(kASCIIOIDPrefix)) &&
           WriteTaiAscii(&log_origin, Span(log_number_relative_oid)) &&
           CBB_flush(out);
  }

  // The CA ID, which is equal to the CA cosigner's cosigner ID (see sections
  // 5.1 and 5.4), in the form of a DER-encoded relative OID. This does not
  // include any framing, only the element's contents.
  Array<uint8_t> ca_id_enc_oid_;
  // Public key of the CA cosigner.
  UniquePtr<EVP_PKEY> cosign_pubkey_;
  // Signature algorithm for CA cosigner signatures.
  ScopedX509Algor cosign_sigalg_;
  // Min and max allowed serial number, both (2^48 .. 2^64-1).
  uint64_t min_serial_ = 0u;
  uint64_t max_serial_ = 0u;
};

}  // namespace

// Only id-pe-mtcCertificationAuthority-SHA256 is supported.
bool x509_is_merkle_tree_ca(const X509 *x509) {
  return X509_get_ext_by_NID(x509, NID_pe_mtcCertificationAuthority_SHA256,
                             -1) >= 0;
}

bool x509_evaluate_mtc_subtree_inclusion_proof(
    Span<uint8_t> out, const EVP_MD *log_hash,
    Span<const uint8_t> inclusion_proof, uint64_t index,
    Span<const uint8_t> entry_hash, uint64_t subtree_start,
    uint64_t subtree_end) {
  const size_t log_hash_size = EVP_MD_size(log_hash);
  if (out.size() != log_hash_size || entry_hash.size() != log_hash_size) {
    return false;
  }

  // Check that `subtree_start` and `subtree_end` define a valid subtree.
  if (subtree_start > subtree_end) {
    return false;
  }
  // The subtree must be aligned and not have a ragged left edge, i.e. the size
  // must not exceed the largest power of 2 that divides the start index.
  const uint64_t subtree_size = subtree_end - subtree_start;
  if (subtree_start != 0 &&
      // The expression `subtree_start & (~subtree_start + 1)` isolates the
      // lowest set bit of `subtree_start`.
      subtree_size > (subtree_start & (~subtree_start + 1))) {
    return false;
  }
  // Check that `index` is in range for the subtree.
  if (index < subtree_start || subtree_end <= index) {
    return false;
  }

  OPENSSL_memcpy(out.data(), entry_hash.data(), log_hash_size);

  // `fn` is the index of the entry if the subtree were re-numbered to start at
  // 0, and `sn` is what the last entry of such a re-numbered subtree would be.
  uint64_t fn = index - subtree_start;
  uint64_t sn = subtree_size - 1;
  while (!inclusion_proof.empty()) {
    Span<const uint8_t> p =
        GetNextValueFromInclusionProof(log_hash_size, inclusion_proof);
    if (p.empty()) {
      // Truncated hash in inclusion proof, or trailing data after last full
      // hash.
      return false;
    }
    assert(p.size() == log_hash_size);
    if (sn == 0) {
      // More hashes in the inclusion proof than expected.
      return false;
    }
    if (lsb(fn) || fn == sn) {
      HashNode(out, log_hash, /*left_child=*/p, /*right_child=*/out);
      while (!lsb(fn)) {
        fn >>= 1;
        sn >>= 1;
      }
    } else {
      HashNode(out, log_hash, /*left_child=*/out, /*right_child=*/p);
    }
    fn >>= 1;
    sn >>= 1;
  }

  if (sn != 0) {
    // Not enough hashes in inclusion proof.
    return false;
  }

  return true;
}

int x509_verify_mtc(const X509 *x509, const EVP_PKEY *pkey,
                    const X509 *issuer) {
  MTCCACosigner issuer_mtc_ca;
  if (!issuer_mtc_ca.Init(issuer, pkey)) {
    return 0;
  }

  auto *impl = FromOpaque(x509);
  if (!is_mtc_proof(impl->sig_alg.get())) {
    OPENSSL_PUT_ERROR(X509, X509_R_UNSUPPORTED_ALGORITHM);
    return 0;
  }
  if (X509_ALGOR_cmp(impl->sig_alg.get(), impl->tbs_sig_alg.get())) {
    OPENSSL_PUT_ERROR(X509, X509_R_SIGNATURE_ALGORITHM_MISMATCH);
    return 0;
  }

  if (impl->signature->type == V_ASN1_BIT_STRING &&
      ASN1_BIT_STRING_unused_bits(impl->signature.get()) != 0) {
    OPENSSL_PUT_ERROR(X509, X509_R_INVALID_BIT_STRING_BITS_LEFT);
    return 0;
  }

  CBS mtc_proof;
  CBS_init(&mtc_proof, ASN1_STRING_get0_data(impl->signature.get()),
           ASN1_STRING_length(impl->signature.get()));

  CBS extensions, inclusion_proof, signatures;
  uint64_t subtree_start, subtree_end;
  if (!CBS_get_u16_length_prefixed(&mtc_proof, &extensions) ||
      !CBS_get_u48(&mtc_proof, &subtree_start) ||
      !CBS_get_u48(&mtc_proof, &subtree_end) ||
      !CBS_get_u16_length_prefixed(&mtc_proof, &inclusion_proof) ||
      !CBS_get_u24_length_prefixed(&mtc_proof, &signatures) ||
      CBS_len(&mtc_proof) != 0) {
    OPENSSL_PUT_ERROR(ASN1, ASN1_R_DECODE_ERROR);
    return 0;
  }
  if (!CheckMTCLogEntryExtensions(extensions)) {
    return 0;
  }

  // Iterate through the subtree signatures array to check that they conform to
  // spec, and identify the subtree signature from the CA cosigner.
  CBS cosigner_id, signature;
  std::optional<Span<const uint8_t>> prev_cosigner_id;
  std::optional<Span<const uint8_t>> ca_cosignature;
  while (CBS_len(&signatures) > 0) {
    if (!CBS_get_u8_length_prefixed(&signatures, &cosigner_id) ||
        CBS_len(&cosigner_id) == 0 ||
        !CBS_get_u16_length_prefixed(&signatures, &signature)) {
      OPENSSL_PUT_ERROR(ASN1, ASN1_R_DECODE_ERROR);
      return 0;
    }
    Span<const uint8_t> cosigner_id_span = cosigner_id;
    // Cosignatures must be ordered by increasing cosigner ID length
    // (lexicographical order if same length), and must not contain duplicate
    // cosigner IDs.
    if (prev_cosigner_id.has_value()) {
      if (prev_cosigner_id->size() > cosigner_id_span.size()) {
        OPENSSL_PUT_ERROR(X509, X509_R_INVALID_MTC_PROOF);
        return 0;
      }
      if (prev_cosigner_id->size() == cosigner_id_span.size() &&
          !std::lexicographical_compare(
              prev_cosigner_id->begin(), prev_cosigner_id->end(),
              cosigner_id_span.begin(), cosigner_id_span.end())) {
        OPENSSL_PUT_ERROR(X509, X509_R_INVALID_MTC_PROOF);
        return 0;
      }
    }
    prev_cosigner_id = cosigner_id_span;
    if (issuer_mtc_ca.IsCACosigner(cosigner_id_span)) {
      ca_cosignature = signature;
    }
  }
  if (!ca_cosignature.has_value()) {
    OPENSSL_PUT_ERROR(X509, X509_R_INVALID_MTC_PROOF);
    return 0;
  }

  CBS tbs_seq, tbs;
  Array<uint8_t> tbs_scratch;
  if (!x509_get_or_marshal_tbs_cert(&tbs_seq, &tbs_scratch, x509) ||
      !CBS_get_asn1(&tbs_seq, &tbs, CBS_ASN1_SEQUENCE)) {
    OPENSSL_PUT_ERROR(ASN1, ASN1_R_DECODE_ERROR);
    return 0;
  }

  CBS version_field;
  if (CBS_peek_asn1_tag(&tbs, kTBSVersionTag)) {
    if (!CBS_get_asn1_element(&tbs, &version_field, kTBSVersionTag)) {
      OPENSSL_PUT_ERROR(ASN1, ASN1_R_DECODE_ERROR);
      return 0;
    }
  } else {
    CBS_init(&version_field, nullptr, 0);
  }

  // Check serial number is well-formed and within CA's range. (Caller is
  // responsible for checking serial number against other revoked ranges.)
  uint64_t serial;
  if (!CBS_get_asn1_uint64(&tbs, &serial)) {
    OPENSSL_PUT_ERROR(ASN1, ASN1_R_DECODE_ERROR);
    return 0;
  }
  if (!issuer_mtc_ca.IsSerialInRange(serial)) {
    OPENSSL_PUT_ERROR(X509, X509_R_INVALID_PARAMETER);
    return 0;
  }
  // `index` is the least significant 48 bits of `serial`.
  const uint64_t index = serial & ((uint64_t{1} << 48) - 1);
  const uint64_t log_number = serial >> 48;
  assert(is_mtc_serial_valid(serial));

  // The TBSCertificate's `signature` field must match the expected MTC
  // signature algorithm identifier.
  CBS tbs_sigalg;
  if (!CBS_get_asn1_element(&tbs, &tbs_sigalg, CBS_ASN1_SEQUENCE) ||
      !CBS_mem_equal(&tbs_sigalg, kMTCAlgorithmIdentifier,
                     sizeof(kMTCAlgorithmIdentifier))) {
    OPENSSL_PUT_ERROR(X509, X509_R_SIGNATURE_ALGORITHM_MISMATCH);
    return 0;
  }

  // Compute `entry_hash` in a single pass without storing the full
  // TBSCertificateLogEntry or MTCLogEntry.
  ScopedEVP_MD_CTX entry_hash_ctx;
  if (!EVP_DigestInit_ex(entry_hash_ctx.get(), issuer_mtc_ca.log_hash(),
                         nullptr) ||
      !EVP_DigestUpdate(entry_hash_ctx.get(), kMTCLeafDomainSeparator,
                        sizeof(kMTCLeafDomainSeparator)) ||
      // MTCLogEntry `extensions` field (comes directly from the MTCProof).
      !DigestUpdateWithU16LengthPrefix(entry_hash_ctx.get(), extensions) ||
      // MTCLogEntryType enum value for a tbs_cert_entry.
      !EVP_DigestUpdate(entry_hash_ctx.get(), kMTCLogEntryTypeTBSCertEntry,
                        sizeof(kMTCLogEntryTypeTBSCertEntry)) ||
      // Contents of TBSCertificateLogEntry follow (excluding the initial
      // identifier and length octets):
      !EVP_DigestUpdate(entry_hash_ctx.get(), CBS_data(&version_field),
                        CBS_len(&version_field)) ||
      !DigestUpdateNextASN1Element(entry_hash_ctx.get(), &tbs) ||  // issuer
      !DigestUpdateNextASN1Element(entry_hash_ctx.get(), &tbs) ||  // validity
      !DigestUpdateNextASN1Element(entry_hash_ctx.get(), &tbs)) {  // subject
    OPENSSL_PUT_ERROR(X509, ERR_R_INTERNAL_ERROR);
    return 0;
  }

  // Hash the TBSCertificate's `subjectPublicKeyInfo` field.
  CBS spki;
  if (!CBS_get_asn1_element(&tbs, &spki, CBS_ASN1_SEQUENCE)) {
    OPENSSL_PUT_ERROR(ASN1, ASN1_R_DECODE_ERROR);
    return 0;
  }
  uint8_t spki_digest[EVP_MAX_MD_SIZE];
  unsigned int spki_digest_len;
  // The short-form DER length can only encode lengths up to 127 (0x7f). The MTC
  // spec permits longer hash lengths up to 255 (0xff), which would require a
  // long-form length, but no `EVP_MD` currently outputs more than 127 bytes, so
  // reject them here instead of supporting both short and long encoding forms.
  if (!EVP_Digest(CBS_data(&spki), CBS_len(&spki), spki_digest,
                  &spki_digest_len, issuer_mtc_ca.log_hash(), nullptr) ||
      spki_digest_len == 0 || spki_digest_len > 0x7f) {
    OPENSSL_PUT_ERROR(X509, ERR_R_INTERNAL_ERROR);
    return 0;
  }

  // Add the AlgorithmIdentifier from the subjectPublicKeyInfo.
  CBS spki_contents;
  if (!CBS_get_asn1(&spki, &spki_contents, CBS_ASN1_SEQUENCE)) {
    OPENSSL_PUT_ERROR(ASN1, ASN1_R_DECODE_ERROR);
    return 0;
  }
  if (!DigestUpdateNextASN1Element(entry_hash_ctx.get(), &spki_contents) ||
      //`subjectPublicKeyInfoHash` field.
      !EVP_DigestUpdate(entry_hash_ctx.get(), &kEncodedOctetStringTag,
                        sizeof(kEncodedOctetStringTag)) ||
      !DigestUpdateWithU8LengthPrefix(entry_hash_ctx.get(),
                                      Span(spki_digest, spki_digest_len)) ||
      // Write the remainder of the TBSCertificate contents octets to the hash.
      // This includes the optional fields `issuerUniqueID`, `subjectUniqueID`,
      // and `extensions`.
      !EVP_DigestUpdate(entry_hash_ctx.get(), CBS_data(&tbs), CBS_len(&tbs))) {
    OPENSSL_PUT_ERROR(X509, ERR_R_INTERNAL_ERROR);
    return 0;
  }

  uint8_t entry_hash[EVP_MAX_MD_SIZE];
  unsigned entry_hash_len;
  if (!EVP_DigestFinal_ex(entry_hash_ctx.get(), entry_hash, &entry_hash_len)) {
    OPENSSL_PUT_ERROR(X509, ERR_R_INTERNAL_ERROR);
    return 0;
  }

  InplaceVector<uint8_t, EVP_MAX_MD_SIZE> expected_subtree_hash;
  expected_subtree_hash.ResizeForOverwrite(
      EVP_MD_size(issuer_mtc_ca.log_hash()));

  if (!x509_evaluate_mtc_subtree_inclusion_proof(
          Span(expected_subtree_hash), issuer_mtc_ca.log_hash(),
          inclusion_proof, index, Span(entry_hash, entry_hash_len),
          subtree_start, subtree_end)) {
    OPENSSL_PUT_ERROR(X509, X509_R_INVALID_MTC_PROOF);
    return 0;
  }

  return issuer_mtc_ca.VerifyCACosignature(ca_cosignature.value(), log_number,
                                           subtree_start, subtree_end,
                                           Span(expected_subtree_hash));
}

BSSL_NAMESPACE_END
