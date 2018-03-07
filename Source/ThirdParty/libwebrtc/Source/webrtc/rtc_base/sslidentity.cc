/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Handling of certificates and keypairs for SSLStreamAdapter's peer mode.
#include "rtc_base/sslidentity.h"

#include <ctime>
#include <string>
#include <utility>

#include "rtc_base/base64.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/opensslidentity.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/sslfingerprint.h"

namespace rtc {

const char kPemTypeCertificate[] = "CERTIFICATE";
const char kPemTypeRsaPrivateKey[] = "RSA PRIVATE KEY";
const char kPemTypeEcPrivateKey[] = "EC PRIVATE KEY";

SSLCertificateStats::SSLCertificateStats(
    std::string&& fingerprint,
    std::string&& fingerprint_algorithm,
    std::string&& base64_certificate,
    std::unique_ptr<SSLCertificateStats>&& issuer)
    : fingerprint(std::move(fingerprint)),
      fingerprint_algorithm(std::move(fingerprint_algorithm)),
      base64_certificate(std::move(base64_certificate)),
      issuer(std::move(issuer)) {
}

SSLCertificateStats::~SSLCertificateStats() {
}

std::unique_ptr<SSLCertificateStats> SSLCertificate::GetStats() const {
  // We have a certificate and optionally a chain of certificates. This forms a
  // linked list, starting with |this|, then the first element of |chain| and
  // ending with the last element of |chain|. The "issuer" of a certificate is
  // the next certificate in the chain. Stats are produced for each certificate
  // in the list. Here, the "issuer" is the issuer's stats.
  std::unique_ptr<SSLCertChain> chain = GetChain();
  std::unique_ptr<SSLCertificateStats> issuer;
  if (chain) {
    // The loop runs in reverse so that the |issuer| is known before the
    // |cert|'s stats.
    for (ptrdiff_t i = chain->GetSize() - 1; i >= 0; --i) {
      const SSLCertificate* cert = &chain->Get(i);
      issuer = cert->GetStats(std::move(issuer));
    }
  }
  return GetStats(std::move(issuer));
}

std::unique_ptr<SSLCertificate> SSLCertificate::GetUniqueReference() const {
  return WrapUnique(GetReference());
}

std::unique_ptr<SSLCertificateStats> SSLCertificate::GetStats(
    std::unique_ptr<SSLCertificateStats> issuer) const {
  // TODO(bemasc): Move this computation to a helper class that caches these
  // values to reduce CPU use in |StatsCollector::GetStats|. This will require
  // adding a fast |SSLCertificate::Equals| to detect certificate changes.
  std::string digest_algorithm;
  if (!GetSignatureDigestAlgorithm(&digest_algorithm))
    return nullptr;

  // |SSLFingerprint::Create| can fail if the algorithm returned by
  // |SSLCertificate::GetSignatureDigestAlgorithm| is not supported by the
  // implementation of |SSLCertificate::ComputeDigest|. This currently happens
  // with MD5- and SHA-224-signed certificates when linked to libNSS.
  std::unique_ptr<SSLFingerprint> ssl_fingerprint(
      SSLFingerprint::Create(digest_algorithm, this));
  if (!ssl_fingerprint)
    return nullptr;
  std::string fingerprint = ssl_fingerprint->GetRfc4572Fingerprint();

  Buffer der_buffer;
  ToDER(&der_buffer);
  std::string der_base64;
  Base64::EncodeFromArray(der_buffer.data(), der_buffer.size(), &der_base64);

  return std::unique_ptr<SSLCertificateStats>(new SSLCertificateStats(
      std::move(fingerprint),
      std::move(digest_algorithm),
      std::move(der_base64),
      std::move(issuer)));
}

KeyParams::KeyParams(KeyType key_type) {
  if (key_type == KT_ECDSA) {
    type_ = KT_ECDSA;
    params_.curve = EC_NIST_P256;
  } else if (key_type == KT_RSA) {
    type_ = KT_RSA;
    params_.rsa.mod_size = kRsaDefaultModSize;
    params_.rsa.pub_exp = kRsaDefaultExponent;
  } else {
    RTC_NOTREACHED();
  }
}

// static
KeyParams KeyParams::RSA(int mod_size, int pub_exp) {
  KeyParams kt(KT_RSA);
  kt.params_.rsa.mod_size = mod_size;
  kt.params_.rsa.pub_exp = pub_exp;
  return kt;
}

// static
KeyParams KeyParams::ECDSA(ECCurve curve) {
  KeyParams kt(KT_ECDSA);
  kt.params_.curve = curve;
  return kt;
}

bool KeyParams::IsValid() const {
  if (type_ == KT_RSA) {
    return (params_.rsa.mod_size >= kRsaMinModSize &&
            params_.rsa.mod_size <= kRsaMaxModSize &&
            params_.rsa.pub_exp > params_.rsa.mod_size);
  } else if (type_ == KT_ECDSA) {
    return (params_.curve == EC_NIST_P256);
  }
  return false;
}

RSAParams KeyParams::rsa_params() const {
  RTC_DCHECK(type_ == KT_RSA);
  return params_.rsa;
}

ECCurve KeyParams::ec_curve() const {
  RTC_DCHECK(type_ == KT_ECDSA);
  return params_.curve;
}

KeyType IntKeyTypeFamilyToKeyType(int key_type_family) {
  return static_cast<KeyType>(key_type_family);
}

bool SSLIdentity::PemToDer(const std::string& pem_type,
                           const std::string& pem_string,
                           std::string* der) {
  // Find the inner body. We need this to fulfill the contract of
  // returning pem_length.
  size_t header = pem_string.find("-----BEGIN " + pem_type + "-----");
  if (header == std::string::npos)
    return false;

  size_t body = pem_string.find("\n", header);
  if (body == std::string::npos)
    return false;

  size_t trailer = pem_string.find("-----END " + pem_type + "-----");
  if (trailer == std::string::npos)
    return false;

  std::string inner = pem_string.substr(body + 1, trailer - (body + 1));

  *der = Base64::Decode(inner, Base64::DO_PARSE_WHITE |
                        Base64::DO_PAD_ANY |
                        Base64::DO_TERM_BUFFER);
  return true;
}

std::string SSLIdentity::DerToPem(const std::string& pem_type,
                                  const unsigned char* data,
                                  size_t length) {
  std::stringstream result;

  result << "-----BEGIN " << pem_type << "-----\n";

  std::string b64_encoded;
  Base64::EncodeFromArray(data, length, &b64_encoded);

  // Divide the Base-64 encoded data into 64-character chunks, as per
  // 4.3.2.4 of RFC 1421.
  static const size_t kChunkSize = 64;
  size_t chunks = (b64_encoded.size() + (kChunkSize - 1)) / kChunkSize;
  for (size_t i = 0, chunk_offset = 0; i < chunks;
       ++i, chunk_offset += kChunkSize) {
    result << b64_encoded.substr(chunk_offset, kChunkSize);
    result << "\n";
  }

  result << "-----END " << pem_type << "-----\n";

  return result.str();
}

SSLCertChain::SSLCertChain(std::vector<std::unique_ptr<SSLCertificate>> certs)
    : certs_(std::move(certs)) {}

SSLCertChain::SSLCertChain(const std::vector<SSLCertificate*>& certs) {
  RTC_DCHECK(!certs.empty());
  certs_.resize(certs.size());
  std::transform(
      certs.begin(), certs.end(), certs_.begin(),
      [](const SSLCertificate* cert) -> std::unique_ptr<SSLCertificate> {
        return cert->GetUniqueReference();
      });
}

SSLCertChain::SSLCertChain(const SSLCertificate* cert) {
  certs_.push_back(cert->GetUniqueReference());
}

SSLCertChain::~SSLCertChain() {}

SSLCertChain* SSLCertChain::Copy() const {
  std::vector<std::unique_ptr<SSLCertificate>> new_certs(certs_.size());
  std::transform(certs_.begin(), certs_.end(), new_certs.begin(),
                 [](const std::unique_ptr<SSLCertificate>& cert)
                     -> std::unique_ptr<SSLCertificate> {
                   return cert->GetUniqueReference();
                 });
  return new SSLCertChain(std::move(new_certs));
}

// static
SSLCertificate* SSLCertificate::FromPEMString(const std::string& pem_string) {
  return OpenSSLCertificate::FromPEMString(pem_string);
}

// static
SSLIdentity* SSLIdentity::GenerateWithExpiration(const std::string& common_name,
                                                 const KeyParams& key_params,
                                                 time_t certificate_lifetime) {
  return OpenSSLIdentity::GenerateWithExpiration(common_name, key_params,
                                                 certificate_lifetime);
}

// static
SSLIdentity* SSLIdentity::Generate(const std::string& common_name,
                                   const KeyParams& key_params) {
  return OpenSSLIdentity::GenerateWithExpiration(
      common_name, key_params, kDefaultCertificateLifetimeInSeconds);
}

// static
SSLIdentity* SSLIdentity::Generate(const std::string& common_name,
                                   KeyType key_type) {
  return OpenSSLIdentity::GenerateWithExpiration(
      common_name, KeyParams(key_type), kDefaultCertificateLifetimeInSeconds);
}

SSLIdentity* SSLIdentity::GenerateForTest(const SSLIdentityParams& params) {
  return OpenSSLIdentity::GenerateForTest(params);
}

// static
SSLIdentity* SSLIdentity::FromPEMStrings(const std::string& private_key,
                                         const std::string& certificate) {
  return OpenSSLIdentity::FromPEMStrings(private_key, certificate);
}

// static
SSLIdentity* SSLIdentity::FromPEMChainStrings(
    const std::string& private_key,
    const std::string& certificate_chain) {
  return OpenSSLIdentity::FromPEMChainStrings(private_key, certificate_chain);
}

bool operator==(const SSLIdentity& a, const SSLIdentity& b) {
  return static_cast<const OpenSSLIdentity&>(a) ==
         static_cast<const OpenSSLIdentity&>(b);
}
bool operator!=(const SSLIdentity& a, const SSLIdentity& b) {
  return !(a == b);
}

// Read |n| bytes from ASN1 number string at *|pp| and return the numeric value.
// Update *|pp| and *|np| to reflect number of read bytes.
static inline int ASN1ReadInt(const unsigned char** pp, size_t* np, size_t n) {
  const unsigned char* p = *pp;
  int x = 0;
  for (size_t i = 0; i < n; i++)
    x = 10 * x + p[i] - '0';
  *pp = p + n;
  *np = *np - n;
  return x;
}

int64_t ASN1TimeToSec(const unsigned char* s, size_t length, bool long_format) {
  size_t bytes_left = length;

  // Make sure the string ends with Z.  Doing it here protects the strspn call
  // from running off the end of the string in Z's absense.
  if (length == 0 || s[length - 1] != 'Z')
    return -1;

  // Make sure we only have ASCII digits so that we don't need to clutter the
  // code below and ASN1ReadInt with error checking.
  size_t n = strspn(reinterpret_cast<const char*>(s), "0123456789");
  if (n + 1 != length)
    return -1;

  int year;

  // Read out ASN1 year, in either 2-char "UTCTIME" or 4-char "GENERALIZEDTIME"
  // format.  Both format use UTC in this context.
  if (long_format) {
    // ASN1 format: yyyymmddhh[mm[ss[.fff]]]Z where the Z is literal, but
    // RFC 5280 requires us to only support exactly yyyymmddhhmmssZ.

    if (bytes_left < 11)
      return -1;

    year = ASN1ReadInt(&s, &bytes_left, 4);
    year -= 1900;
  } else {
    // ASN1 format: yymmddhhmm[ss]Z where the Z is literal, but RFC 5280
    // requires us to only support exactly yymmddhhmmssZ.

    if (bytes_left < 9)
      return -1;

    year = ASN1ReadInt(&s, &bytes_left, 2);
    if (year < 50)  // Per RFC 5280 4.1.2.5.1
      year += 100;
  }

  std::tm tm;
  tm.tm_year = year;

  // Read out remaining ASN1 time data and store it in |tm| in documented
  // std::tm format.
  tm.tm_mon = ASN1ReadInt(&s, &bytes_left, 2) - 1;
  tm.tm_mday = ASN1ReadInt(&s, &bytes_left, 2);
  tm.tm_hour = ASN1ReadInt(&s, &bytes_left, 2);
  tm.tm_min = ASN1ReadInt(&s, &bytes_left, 2);
  tm.tm_sec = ASN1ReadInt(&s, &bytes_left, 2);

  if (bytes_left != 1) {
    // Now just Z should remain.  Its existence was asserted above.
    return -1;
  }

  return TmToSeconds(tm);
}

}  // namespace rtc
