/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_OPENSSLIDENTITY_H_
#define RTC_BASE_OPENSSLIDENTITY_H_

#include <openssl/evp.h>
#include <openssl/x509.h>

#include <memory>
#include <string>

#include "rtc_base/checks.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/opensslcertificate.h"
#include "rtc_base/sslidentity.h"

typedef struct ssl_ctx_st SSL_CTX;

namespace rtc {

// OpenSSLKeyPair encapsulates an OpenSSL EVP_PKEY* keypair object,
// which is reference counted inside the OpenSSL library.
class OpenSSLKeyPair {
 public:
  explicit OpenSSLKeyPair(EVP_PKEY* pkey) : pkey_(pkey) {
    RTC_DCHECK(pkey_ != nullptr);
  }

  static OpenSSLKeyPair* Generate(const KeyParams& key_params);
  // Constructs a key pair from the private key PEM string. This must not result
  // in missing public key parameters. Returns null on error.
  static OpenSSLKeyPair* FromPrivateKeyPEMString(const std::string& pem_string);

  virtual ~OpenSSLKeyPair();

  virtual OpenSSLKeyPair* GetReference();

  EVP_PKEY* pkey() const { return pkey_; }
  std::string PrivateKeyToPEMString() const;
  std::string PublicKeyToPEMString() const;
  bool operator==(const OpenSSLKeyPair& other) const;
  bool operator!=(const OpenSSLKeyPair& other) const;

 private:
  void AddReference();

  EVP_PKEY* pkey_;

  RTC_DISALLOW_COPY_AND_ASSIGN(OpenSSLKeyPair);
};

// Holds a keypair and certificate together, and a method to generate
// them consistently.
class OpenSSLIdentity : public SSLIdentity {
 public:
  static OpenSSLIdentity* GenerateWithExpiration(const std::string& common_name,
                                                 const KeyParams& key_params,
                                                 time_t certificate_lifetime);
  static OpenSSLIdentity* GenerateForTest(const SSLIdentityParams& params);
  static SSLIdentity* FromPEMStrings(const std::string& private_key,
                                     const std::string& certificate);
  static SSLIdentity* FromPEMChainStrings(const std::string& private_key,
                                          const std::string& certificate_chain);
  ~OpenSSLIdentity() override;

  const OpenSSLCertificate& certificate() const override;
  const SSLCertChain& cert_chain() const override;
  OpenSSLIdentity* GetReference() const override;

  // Configure an SSL context object to use our key and certificate.
  bool ConfigureIdentity(SSL_CTX* ctx);

  std::string PrivateKeyToPEMString() const override;
  std::string PublicKeyToPEMString() const override;
  bool operator==(const OpenSSLIdentity& other) const;
  bool operator!=(const OpenSSLIdentity& other) const;

 private:
  OpenSSLIdentity(std::unique_ptr<OpenSSLKeyPair> key_pair,
                  std::unique_ptr<OpenSSLCertificate> certificate);
  OpenSSLIdentity(std::unique_ptr<OpenSSLKeyPair> key_pair,
                  std::unique_ptr<SSLCertChain> cert_chain);

  static OpenSSLIdentity* GenerateInternal(const SSLIdentityParams& params);

  std::unique_ptr<OpenSSLKeyPair> key_pair_;
  std::unique_ptr<SSLCertChain> cert_chain_;

  RTC_DISALLOW_COPY_AND_ASSIGN(OpenSSLIdentity);
};

}  // namespace rtc

#endif  // RTC_BASE_OPENSSLIDENTITY_H_
