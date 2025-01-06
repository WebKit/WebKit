/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/ssl_stream_adapter.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "rtc_base/openssl_stream_adapter.h"
#include "rtc_base/ssl_identity.h"
#include "rtc_base/stream.h"

namespace rtc {

// Deprecated, prefer SrtpCryptoSuiteToName.
const char kCsAesCm128HmacSha1_80[] = "AES_CM_128_HMAC_SHA1_80";
const char kCsAesCm128HmacSha1_32[] = "AES_CM_128_HMAC_SHA1_32";
const char kCsAeadAes128Gcm[] = "AEAD_AES_128_GCM";
const char kCsAeadAes256Gcm[] = "AEAD_AES_256_GCM";

std::string SrtpCryptoSuiteToName(int crypto_suite) {
  switch (crypto_suite) {
    case kSrtpAes128CmSha1_80:
      return "AES_CM_128_HMAC_SHA1_80";
    case kSrtpAes128CmSha1_32:
      return "AES_CM_128_HMAC_SHA1_32";
    case kSrtpAeadAes128Gcm:
      return "AEAD_AES_128_GCM";
    case kSrtpAeadAes256Gcm:
      return "AEAD_AES_256_GCM";
    default:
      return std::string();
  }
}

bool GetSrtpKeyAndSaltLengths(int crypto_suite,
                              int* key_length,
                              int* salt_length) {
  switch (crypto_suite) {
    case kSrtpAes128CmSha1_32:
    case kSrtpAes128CmSha1_80:
      // SRTP_AES128_CM_HMAC_SHA1_32 and SRTP_AES128_CM_HMAC_SHA1_80 are defined
      // in RFC 5764 to use a 128 bits key and 112 bits salt for the cipher.
      *key_length = 16;
      *salt_length = 14;
      break;
    case kSrtpAeadAes128Gcm:
      // kSrtpAeadAes128Gcm is defined in RFC 7714 to use a 128 bits key and
      // a 96 bits salt for the cipher.
      *key_length = 16;
      *salt_length = 12;
      break;
    case kSrtpAeadAes256Gcm:
      // kSrtpAeadAes256Gcm is defined in RFC 7714 to use a 256 bits key and
      // a 96 bits salt for the cipher.
      *key_length = 32;
      *salt_length = 12;
      break;
    default:
      return false;
  }
  return true;
}

bool IsGcmCryptoSuite(int crypto_suite) {
  return (crypto_suite == kSrtpAeadAes256Gcm ||
          crypto_suite == kSrtpAeadAes128Gcm);
}

std::unique_ptr<SSLStreamAdapter> SSLStreamAdapter::Create(
    std::unique_ptr<StreamInterface> stream,
    absl::AnyInvocable<void(SSLHandshakeError)> handshake_error) {
  return std::make_unique<OpenSSLStreamAdapter>(std::move(stream),
                                                std::move(handshake_error));
}

bool SSLStreamAdapter::IsBoringSsl() {
  return OpenSSLStreamAdapter::IsBoringSsl();
}
bool SSLStreamAdapter::IsAcceptableCipher(int cipher, KeyType key_type) {
  return OpenSSLStreamAdapter::IsAcceptableCipher(cipher, key_type);
}
bool SSLStreamAdapter::IsAcceptableCipher(absl::string_view cipher,
                                          KeyType key_type) {
  return OpenSSLStreamAdapter::IsAcceptableCipher(cipher, key_type);
}

///////////////////////////////////////////////////////////////////////////////
// Test only settings
///////////////////////////////////////////////////////////////////////////////

void SSLStreamAdapter::EnableTimeCallbackForTesting() {
  OpenSSLStreamAdapter::EnableTimeCallbackForTesting();
}

///////////////////////////////////////////////////////////////////////////////

}  // namespace rtc
