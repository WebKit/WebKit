/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/opensslidentity.h"

#include <memory>

// Must be included first before openssl headers.
#include "rtc_base/win32.h"  // NOLINT

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

#include "rtc_base/checks.h"
#include "rtc_base/helpers.h"
#include "rtc_base/logging.h"
#include "rtc_base/openssl.h"
#include "rtc_base/openssldigest.h"
#include "rtc_base/ptr_util.h"

namespace rtc {

// We could have exposed a myriad of parameters for the crypto stuff,
// but keeping it simple seems best.

// Random bits for certificate serial number
static const int SERIAL_RAND_BITS = 64;

// Generate a key pair. Caller is responsible for freeing the returned object.
static EVP_PKEY* MakeKey(const KeyParams& key_params) {
  RTC_LOG(LS_INFO) << "Making key pair";
  EVP_PKEY* pkey = EVP_PKEY_new();
  if (key_params.type() == KT_RSA) {
    int key_length = key_params.rsa_params().mod_size;
    BIGNUM* exponent = BN_new();
    RSA* rsa = RSA_new();
    if (!pkey || !exponent || !rsa ||
        !BN_set_word(exponent, key_params.rsa_params().pub_exp) ||
        !RSA_generate_key_ex(rsa, key_length, exponent, nullptr) ||
        !EVP_PKEY_assign_RSA(pkey, rsa)) {
      EVP_PKEY_free(pkey);
      BN_free(exponent);
      RSA_free(rsa);
      RTC_LOG(LS_ERROR) << "Failed to make RSA key pair";
      return nullptr;
    }
    // ownership of rsa struct was assigned, don't free it.
    BN_free(exponent);
  } else if (key_params.type() == KT_ECDSA) {
    if (key_params.ec_curve() == EC_NIST_P256) {
      EC_KEY* ec_key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);

      // Ensure curve name is included when EC key is serialized.
      // Without this call, OpenSSL versions before 1.1.0 will create
      // certificates that don't work for TLS.
      // This is a no-op for BoringSSL and OpenSSL 1.1.0+
      EC_KEY_set_asn1_flag(ec_key, OPENSSL_EC_NAMED_CURVE);

      if (!pkey || !ec_key || !EC_KEY_generate_key(ec_key) ||
          !EVP_PKEY_assign_EC_KEY(pkey, ec_key)) {
        EVP_PKEY_free(pkey);
        EC_KEY_free(ec_key);
        RTC_LOG(LS_ERROR) << "Failed to make EC key pair";
        return nullptr;
      }
      // ownership of ec_key struct was assigned, don't free it.
    } else {
      // Add generation of any other curves here.
      EVP_PKEY_free(pkey);
      RTC_LOG(LS_ERROR) << "ECDSA key requested for unknown curve";
      return nullptr;
    }
  } else {
    EVP_PKEY_free(pkey);
    RTC_LOG(LS_ERROR) << "Key type requested not understood";
    return nullptr;
  }

  RTC_LOG(LS_INFO) << "Returning key pair";
  return pkey;
}

// Generate a self-signed certificate, with the public key from the
// given key pair. Caller is responsible for freeing the returned object.
static X509* MakeCertificate(EVP_PKEY* pkey, const SSLIdentityParams& params) {
  RTC_LOG(LS_INFO) << "Making certificate for " << params.common_name;
  X509* x509 = nullptr;
  BIGNUM* serial_number = nullptr;
  X509_NAME* name = nullptr;
  time_t epoch_off = 0;  // Time offset since epoch.

  if ((x509 = X509_new()) == nullptr)
    goto error;

  if (!X509_set_pubkey(x509, pkey))
    goto error;

  // serial number
  // temporary reference to serial number inside x509 struct
  ASN1_INTEGER* asn1_serial_number;
  if ((serial_number = BN_new()) == nullptr ||
      !BN_pseudo_rand(serial_number, SERIAL_RAND_BITS, 0, 0) ||
      (asn1_serial_number = X509_get_serialNumber(x509)) == nullptr ||
      !BN_to_ASN1_INTEGER(serial_number, asn1_serial_number))
    goto error;

  if (!X509_set_version(x509, 2L))  // version 3
    goto error;

  // There are a lot of possible components for the name entries. In
  // our P2P SSL mode however, the certificates are pre-exchanged
  // (through the secure XMPP channel), and so the certificate
  // identification is arbitrary. It can't be empty, so we set some
  // arbitrary common_name. Note that this certificate goes out in
  // clear during SSL negotiation, so there may be a privacy issue in
  // putting anything recognizable here.
  if ((name = X509_NAME_new()) == nullptr ||
      !X509_NAME_add_entry_by_NID(name, NID_commonName, MBSTRING_UTF8,
                                  (unsigned char*)params.common_name.c_str(),
                                  -1, -1, 0) ||
      !X509_set_subject_name(x509, name) || !X509_set_issuer_name(x509, name))
    goto error;

  if (!X509_time_adj(X509_get_notBefore(x509), params.not_before, &epoch_off) ||
      !X509_time_adj(X509_get_notAfter(x509), params.not_after, &epoch_off))
    goto error;

  if (!X509_sign(x509, pkey, EVP_sha256()))
    goto error;

  BN_free(serial_number);
  X509_NAME_free(name);
  RTC_LOG(LS_INFO) << "Returning certificate";
  return x509;

error:
  BN_free(serial_number);
  X509_NAME_free(name);
  X509_free(x509);
  return nullptr;
}

// This dumps the SSL error stack to the log.
static void LogSSLErrors(const std::string& prefix) {
  char error_buf[200];
  unsigned long err;

  while ((err = ERR_get_error()) != 0) {
    ERR_error_string_n(err, error_buf, sizeof(error_buf));
    RTC_LOG(LS_ERROR) << prefix << ": " << error_buf << "\n";
  }
}

OpenSSLKeyPair* OpenSSLKeyPair::Generate(const KeyParams& key_params) {
  EVP_PKEY* pkey = MakeKey(key_params);
  if (!pkey) {
    LogSSLErrors("Generating key pair");
    return nullptr;
  }
  return new OpenSSLKeyPair(pkey);
}

OpenSSLKeyPair* OpenSSLKeyPair::FromPrivateKeyPEMString(
    const std::string& pem_string) {
  BIO* bio = BIO_new_mem_buf(const_cast<char*>(pem_string.c_str()), -1);
  if (!bio) {
    RTC_LOG(LS_ERROR) << "Failed to create a new BIO buffer.";
    return nullptr;
  }
  BIO_set_mem_eof_return(bio, 0);
  EVP_PKEY* pkey =
      PEM_read_bio_PrivateKey(bio, nullptr, nullptr, const_cast<char*>("\0"));
  BIO_free(bio);  // Frees the BIO, but not the pointed-to string.
  if (!pkey) {
    RTC_LOG(LS_ERROR) << "Failed to create the private key from PEM string.";
    return nullptr;
  }
  if (EVP_PKEY_missing_parameters(pkey) != 0) {
    RTC_LOG(LS_ERROR)
        << "The resulting key pair is missing public key parameters.";
    EVP_PKEY_free(pkey);
    return nullptr;
  }
  return new OpenSSLKeyPair(pkey);
}

OpenSSLKeyPair::~OpenSSLKeyPair() {
  EVP_PKEY_free(pkey_);
}

OpenSSLKeyPair* OpenSSLKeyPair::GetReference() {
  AddReference();
  return new OpenSSLKeyPair(pkey_);
}

void OpenSSLKeyPair::AddReference() {
#if defined(OPENSSL_IS_BORINGSSL)
  EVP_PKEY_up_ref(pkey_);
#else
  CRYPTO_add(&pkey_->references, 1, CRYPTO_LOCK_EVP_PKEY);
#endif
}

std::string OpenSSLKeyPair::PrivateKeyToPEMString() const {
  BIO* temp_memory_bio = BIO_new(BIO_s_mem());
  if (!temp_memory_bio) {
    RTC_LOG_F(LS_ERROR) << "Failed to allocate temporary memory bio";
    RTC_NOTREACHED();
    return "";
  }
  if (!PEM_write_bio_PrivateKey(temp_memory_bio, pkey_, nullptr, nullptr, 0,
                                nullptr, nullptr)) {
    RTC_LOG_F(LS_ERROR) << "Failed to write private key";
    BIO_free(temp_memory_bio);
    RTC_NOTREACHED();
    return "";
  }
  BIO_write(temp_memory_bio, "\0", 1);
  char* buffer;
  BIO_get_mem_data(temp_memory_bio, &buffer);
  std::string priv_key_str = buffer;
  BIO_free(temp_memory_bio);
  return priv_key_str;
}

std::string OpenSSLKeyPair::PublicKeyToPEMString() const {
  BIO* temp_memory_bio = BIO_new(BIO_s_mem());
  if (!temp_memory_bio) {
    RTC_LOG_F(LS_ERROR) << "Failed to allocate temporary memory bio";
    RTC_NOTREACHED();
    return "";
  }
  if (!PEM_write_bio_PUBKEY(temp_memory_bio, pkey_)) {
    RTC_LOG_F(LS_ERROR) << "Failed to write public key";
    BIO_free(temp_memory_bio);
    RTC_NOTREACHED();
    return "";
  }
  BIO_write(temp_memory_bio, "\0", 1);
  char* buffer;
  BIO_get_mem_data(temp_memory_bio, &buffer);
  std::string pub_key_str = buffer;
  BIO_free(temp_memory_bio);
  return pub_key_str;
}

bool OpenSSLKeyPair::operator==(const OpenSSLKeyPair& other) const {
  return EVP_PKEY_cmp(this->pkey_, other.pkey_) == 1;
}

bool OpenSSLKeyPair::operator!=(const OpenSSLKeyPair& other) const {
  return !(*this == other);
}

#if !defined(NDEBUG)
// Print a certificate to the log, for debugging.
static void PrintCert(X509* x509) {
  BIO* temp_memory_bio = BIO_new(BIO_s_mem());
  if (!temp_memory_bio) {
    RTC_LOG_F(LS_ERROR) << "Failed to allocate temporary memory bio";
    return;
  }
  X509_print_ex(temp_memory_bio, x509, XN_FLAG_SEP_CPLUS_SPC, 0);
  BIO_write(temp_memory_bio, "\0", 1);
  char* buffer;
  BIO_get_mem_data(temp_memory_bio, &buffer);
  RTC_LOG(LS_VERBOSE) << buffer;
  BIO_free(temp_memory_bio);
}
#endif

OpenSSLCertificate::OpenSSLCertificate(X509* x509) : x509_(x509) {
  AddReference();
}

OpenSSLCertificate* OpenSSLCertificate::Generate(
    OpenSSLKeyPair* key_pair,
    const SSLIdentityParams& params) {
  SSLIdentityParams actual_params(params);
  if (actual_params.common_name.empty()) {
    // Use a random string, arbitrarily 8chars long.
    actual_params.common_name = CreateRandomString(8);
  }
  X509* x509 = MakeCertificate(key_pair->pkey(), actual_params);
  if (!x509) {
    LogSSLErrors("Generating certificate");
    return nullptr;
  }
#if !defined(NDEBUG)
  PrintCert(x509);
#endif
  OpenSSLCertificate* ret = new OpenSSLCertificate(x509);
  X509_free(x509);
  return ret;
}

OpenSSLCertificate* OpenSSLCertificate::FromPEMString(
    const std::string& pem_string) {
  BIO* bio = BIO_new_mem_buf(const_cast<char*>(pem_string.c_str()), -1);
  if (!bio)
    return nullptr;
  BIO_set_mem_eof_return(bio, 0);
  X509* x509 =
      PEM_read_bio_X509(bio, nullptr, nullptr, const_cast<char*>("\0"));
  BIO_free(bio);  // Frees the BIO, but not the pointed-to string.

  if (!x509)
    return nullptr;

  OpenSSLCertificate* ret = new OpenSSLCertificate(x509);
  X509_free(x509);
  return ret;
}

// NOTE: This implementation only functions correctly after InitializeSSL
// and before CleanupSSL.
bool OpenSSLCertificate::GetSignatureDigestAlgorithm(
    std::string* algorithm) const {
  int nid = OBJ_obj2nid(x509_->sig_alg->algorithm);
  switch (nid) {
    case NID_md5WithRSA:
    case NID_md5WithRSAEncryption:
      *algorithm = DIGEST_MD5;
      break;
    case NID_ecdsa_with_SHA1:
    case NID_dsaWithSHA1:
    case NID_dsaWithSHA1_2:
    case NID_sha1WithRSA:
    case NID_sha1WithRSAEncryption:
      *algorithm = DIGEST_SHA_1;
      break;
    case NID_ecdsa_with_SHA224:
    case NID_sha224WithRSAEncryption:
    case NID_dsa_with_SHA224:
      *algorithm = DIGEST_SHA_224;
      break;
    case NID_ecdsa_with_SHA256:
    case NID_sha256WithRSAEncryption:
    case NID_dsa_with_SHA256:
      *algorithm = DIGEST_SHA_256;
      break;
    case NID_ecdsa_with_SHA384:
    case NID_sha384WithRSAEncryption:
      *algorithm = DIGEST_SHA_384;
      break;
    case NID_ecdsa_with_SHA512:
    case NID_sha512WithRSAEncryption:
      *algorithm = DIGEST_SHA_512;
      break;
    default:
      // Unknown algorithm.  There are several unhandled options that are less
      // common and more complex.
      RTC_LOG(LS_ERROR) << "Unknown signature algorithm NID: " << nid;
      algorithm->clear();
      return false;
  }
  return true;
}

std::unique_ptr<SSLCertChain> OpenSSLCertificate::GetChain() const {
  return nullptr;
}

bool OpenSSLCertificate::ComputeDigest(const std::string& algorithm,
                                       unsigned char* digest,
                                       size_t size,
                                       size_t* length) const {
  return ComputeDigest(x509_, algorithm, digest, size, length);
}

bool OpenSSLCertificate::ComputeDigest(const X509* x509,
                                       const std::string& algorithm,
                                       unsigned char* digest,
                                       size_t size,
                                       size_t* length) {
  const EVP_MD* md;
  unsigned int n;

  if (!OpenSSLDigest::GetDigestEVP(algorithm, &md))
    return false;

  if (size < static_cast<size_t>(EVP_MD_size(md)))
    return false;

  X509_digest(x509, md, digest, &n);

  *length = n;

  return true;
}

OpenSSLCertificate::~OpenSSLCertificate() {
  X509_free(x509_);
}

OpenSSLCertificate* OpenSSLCertificate::GetReference() const {
  return new OpenSSLCertificate(x509_);
}

std::string OpenSSLCertificate::ToPEMString() const {
  BIO* bio = BIO_new(BIO_s_mem());
  if (!bio) {
    RTC_FATAL() << "unreachable code";
  }
  if (!PEM_write_bio_X509(bio, x509_)) {
    BIO_free(bio);
    RTC_FATAL() << "unreachable code";
  }
  BIO_write(bio, "\0", 1);
  char* buffer;
  BIO_get_mem_data(bio, &buffer);
  std::string ret(buffer);
  BIO_free(bio);
  return ret;
}

void OpenSSLCertificate::ToDER(Buffer* der_buffer) const {
  // In case of failure, make sure to leave the buffer empty.
  der_buffer->SetSize(0);

  // Calculates the DER representation of the certificate, from scratch.
  BIO* bio = BIO_new(BIO_s_mem());
  if (!bio) {
    RTC_FATAL() << "unreachable code";
  }
  if (!i2d_X509_bio(bio, x509_)) {
    BIO_free(bio);
    RTC_FATAL() << "unreachable code";
  }
  char* data;
  size_t length = BIO_get_mem_data(bio, &data);
  der_buffer->SetData(data, length);
  BIO_free(bio);
}

void OpenSSLCertificate::AddReference() const {
  RTC_DCHECK(x509_ != nullptr);
#if defined(OPENSSL_IS_BORINGSSL)
  X509_up_ref(x509_);
#else
  CRYPTO_add(&x509_->references, 1, CRYPTO_LOCK_X509);
#endif
}

bool OpenSSLCertificate::operator==(const OpenSSLCertificate& other) const {
  return X509_cmp(x509_, other.x509_) == 0;
}

bool OpenSSLCertificate::operator!=(const OpenSSLCertificate& other) const {
  return !(*this == other);
}

// Documented in sslidentity.h.
int64_t OpenSSLCertificate::CertificateExpirationTime() const {
  ASN1_TIME* expire_time = X509_get_notAfter(x509_);
  bool long_format;

  if (expire_time->type == V_ASN1_UTCTIME) {
    long_format = false;
  } else if (expire_time->type == V_ASN1_GENERALIZEDTIME) {
    long_format = true;
  } else {
    return -1;
  }

  return ASN1TimeToSec(expire_time->data, expire_time->length, long_format);
}

OpenSSLIdentity::OpenSSLIdentity(
    std::unique_ptr<OpenSSLKeyPair> key_pair,
    std::unique_ptr<OpenSSLCertificate> certificate)
    : key_pair_(std::move(key_pair)) {
  RTC_DCHECK(key_pair_ != nullptr);
  RTC_DCHECK(certificate != nullptr);
  std::vector<std::unique_ptr<SSLCertificate>> certs;
  certs.push_back(std::move(certificate));
  cert_chain_.reset(new SSLCertChain(std::move(certs)));
}

OpenSSLIdentity::OpenSSLIdentity(std::unique_ptr<OpenSSLKeyPair> key_pair,
                                 std::unique_ptr<SSLCertChain> cert_chain)
    : key_pair_(std::move(key_pair)), cert_chain_(std::move(cert_chain)) {
  RTC_DCHECK(key_pair_ != nullptr);
  RTC_DCHECK(cert_chain_ != nullptr);
}

OpenSSLIdentity::~OpenSSLIdentity() = default;

OpenSSLIdentity* OpenSSLIdentity::GenerateInternal(
    const SSLIdentityParams& params) {
  std::unique_ptr<OpenSSLKeyPair> key_pair(
      OpenSSLKeyPair::Generate(params.key_params));
  if (key_pair) {
    std::unique_ptr<OpenSSLCertificate> certificate(
        OpenSSLCertificate::Generate(key_pair.get(), params));
    if (certificate != nullptr)
      return new OpenSSLIdentity(std::move(key_pair), std::move(certificate));
  }
  RTC_LOG(LS_INFO) << "Identity generation failed";
  return nullptr;
}

OpenSSLIdentity* OpenSSLIdentity::GenerateWithExpiration(
    const std::string& common_name,
    const KeyParams& key_params,
    time_t certificate_lifetime) {
  SSLIdentityParams params;
  params.key_params = key_params;
  params.common_name = common_name;
  time_t now = time(nullptr);
  params.not_before = now + kCertificateWindowInSeconds;
  params.not_after = now + certificate_lifetime;
  if (params.not_before > params.not_after)
    return nullptr;
  return GenerateInternal(params);
}

OpenSSLIdentity* OpenSSLIdentity::GenerateForTest(
    const SSLIdentityParams& params) {
  return GenerateInternal(params);
}

SSLIdentity* OpenSSLIdentity::FromPEMStrings(const std::string& private_key,
                                             const std::string& certificate) {
  std::unique_ptr<OpenSSLCertificate> cert(
      OpenSSLCertificate::FromPEMString(certificate));
  if (!cert) {
    RTC_LOG(LS_ERROR) << "Failed to create OpenSSLCertificate from PEM string.";
    return nullptr;
  }

  std::unique_ptr<OpenSSLKeyPair> key_pair(
      OpenSSLKeyPair::FromPrivateKeyPEMString(private_key));
  if (!key_pair) {
    RTC_LOG(LS_ERROR) << "Failed to create key pair from PEM string.";
    return nullptr;
  }

  return new OpenSSLIdentity(std::move(key_pair), std::move(cert));
}

SSLIdentity* OpenSSLIdentity::FromPEMChainStrings(
    const std::string& private_key,
    const std::string& certificate_chain) {
  BIO* bio =
      BIO_new_mem_buf(certificate_chain.data(), certificate_chain.size());
  if (!bio)
    return nullptr;
  BIO_set_mem_eof_return(bio, 0);
  std::vector<std::unique_ptr<SSLCertificate>> certs;
  while (true) {
    X509* x509 =
        PEM_read_bio_X509(bio, nullptr, nullptr, const_cast<char*>("\0"));
    if (x509 == nullptr) {
      uint32_t err = ERR_peek_error();
      if (ERR_GET_LIB(err) == ERR_LIB_PEM &&
          ERR_GET_REASON(err) == PEM_R_NO_START_LINE) {
        break;
      }
      RTC_LOG(LS_ERROR) << "Failed to parse certificate from PEM string.";
      BIO_free(bio);
      return nullptr;
    }
    certs.emplace_back(new OpenSSLCertificate(x509));
    X509_free(x509);
  }
  BIO_free(bio);
  if (certs.empty()) {
    RTC_LOG(LS_ERROR) << "Found no certificates in PEM string.";
    return nullptr;
  }

  std::unique_ptr<OpenSSLKeyPair> key_pair(
      OpenSSLKeyPair::FromPrivateKeyPEMString(private_key));
  if (!key_pair) {
    RTC_LOG(LS_ERROR) << "Failed to create key pair from PEM string.";
    return nullptr;
  }

  return new OpenSSLIdentity(std::move(key_pair),
                             MakeUnique<SSLCertChain>(std::move(certs)));
}

const OpenSSLCertificate& OpenSSLIdentity::certificate() const {
  return *static_cast<const OpenSSLCertificate*>(&cert_chain_->Get(0));
}

OpenSSLIdentity* OpenSSLIdentity::GetReference() const {
  return new OpenSSLIdentity(WrapUnique(key_pair_->GetReference()),
                             WrapUnique(cert_chain_->Copy()));
}

bool OpenSSLIdentity::ConfigureIdentity(SSL_CTX* ctx) {
  // 1 is the documented success return code.
  const OpenSSLCertificate* cert = &certificate();
  if (SSL_CTX_use_certificate(ctx, cert->x509()) != 1 ||
      SSL_CTX_use_PrivateKey(ctx, key_pair_->pkey()) != 1) {
    LogSSLErrors("Configuring key and certificate");
    return false;
  }
  // If a chain is available, use it.
  for (size_t i = 1; i < cert_chain_->GetSize(); ++i) {
    cert = static_cast<const OpenSSLCertificate*>(&cert_chain_->Get(i));
    if (SSL_CTX_add1_chain_cert(ctx, cert->x509()) != 1) {
      LogSSLErrors("Configuring intermediate certificate");
      return false;
    }
  }

  return true;
}

std::string OpenSSLIdentity::PrivateKeyToPEMString() const {
  return key_pair_->PrivateKeyToPEMString();
}

std::string OpenSSLIdentity::PublicKeyToPEMString() const {
  return key_pair_->PublicKeyToPEMString();
}

bool OpenSSLIdentity::operator==(const OpenSSLIdentity& other) const {
  return *this->key_pair_ == *other.key_pair_ &&
         this->certificate() == other.certificate();
}

bool OpenSSLIdentity::operator!=(const OpenSSLIdentity& other) const {
  return !(*this == other);
}

}  // namespace rtc
