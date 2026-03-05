/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/crypto/crypto_options.h"

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "api/field_trials_view.h"
#include "rtc_base/checks.h"
#include "rtc_base/ssl_stream_adapter.h"

namespace webrtc {

CryptoOptions::CryptoOptions() {}

// static
CryptoOptions CryptoOptions::NoGcm() {
  CryptoOptions options;
  options.srtp.enable_gcm_crypto_suites = false;
  return options;
}

std::vector<int> CryptoOptions::GetSupportedDtlsSrtpCryptoSuites() const {
  std::vector<int> crypto_suites;
  // Note: kSrtpAes128CmSha1_80 is what is required to be supported (by
  // draft-ietf-rtcweb-security-arch), but kSrtpAes128CmSha1_32 is allowed as
  // well, and saves a few bytes per packet if it ends up selected.
  // As the cipher suite is potentially insecure, it will only be used if
  // enabled by both peers.
  if (srtp.enable_aes128_sha1_32_crypto_cipher) {
    crypto_suites.push_back(kSrtpAes128CmSha1_32);
  }
  if (srtp.enable_aes128_sha1_80_crypto_cipher) {
    crypto_suites.push_back(kSrtpAes128CmSha1_80);
  }

  // Note: GCM cipher suites are not the top choice since they increase the
  // packet size. In order to negotiate them the other side must not support
  // kSrtpAes128CmSha1_80.
  if (srtp.enable_gcm_crypto_suites) {
    crypto_suites.push_back(kSrtpAeadAes256Gcm);
    crypto_suites.push_back(kSrtpAeadAes128Gcm);
  }
  RTC_CHECK(!crypto_suites.empty());
  return crypto_suites;
}

bool CryptoOptions::operator==(const CryptoOptions& other) const {
  struct data_being_tested_for_equality {
    struct Srtp {
      bool enable_gcm_crypto_suites;
      bool enable_aes128_sha1_32_crypto_cipher;
      bool enable_aes128_sha1_80_crypto_cipher;
      bool enable_encrypted_rtp_header_extensions;
    } srtp;
    struct SFrame {
      bool require_frame_encryption;
    } sframe;
    EphemeralKeyExchangeCipherGroups ephemeral_key_exchange_cipher_groups;
  };
  static_assert(sizeof(data_being_tested_for_equality) == sizeof(*this),
                "Did you add something to CryptoOptions and forget to "
                "update operator==?");

  return srtp.enable_gcm_crypto_suites == other.srtp.enable_gcm_crypto_suites &&
         srtp.enable_aes128_sha1_32_crypto_cipher ==
             other.srtp.enable_aes128_sha1_32_crypto_cipher &&
         srtp.enable_aes128_sha1_80_crypto_cipher ==
             other.srtp.enable_aes128_sha1_80_crypto_cipher &&
         srtp.enable_encrypted_rtp_header_extensions ==
             other.srtp.enable_encrypted_rtp_header_extensions &&
         sframe.require_frame_encryption ==
             other.sframe.require_frame_encryption &&
         ephemeral_key_exchange_cipher_groups ==
             other.ephemeral_key_exchange_cipher_groups;
}

bool CryptoOptions::operator!=(const CryptoOptions& other) const {
  return !(*this == other);
}

CryptoOptions::EphemeralKeyExchangeCipherGroups::
    EphemeralKeyExchangeCipherGroups()
    : enabled_(SSLStreamAdapter::GetDefaultEphemeralKeyExchangeCipherGroups(
          /* field_trials= */ nullptr)) {}

bool CryptoOptions::EphemeralKeyExchangeCipherGroups::operator==(
    const CryptoOptions::EphemeralKeyExchangeCipherGroups& other) const {
  return enabled_ == other.enabled_;
}

std::set<uint16_t>
CryptoOptions::EphemeralKeyExchangeCipherGroups::GetSupported() {
  return SSLStreamAdapter::GetSupportedEphemeralKeyExchangeCipherGroups();
}

std::optional<std::string>
CryptoOptions::EphemeralKeyExchangeCipherGroups::GetName(uint16_t group_id) {
  return SSLStreamAdapter::GetEphemeralKeyExchangeCipherGroupName(group_id);
}

void CryptoOptions::EphemeralKeyExchangeCipherGroups::AddFirst(uint16_t group) {
  std::erase(enabled_, group);
  enabled_.insert(enabled_.begin(), group);
}

void CryptoOptions::EphemeralKeyExchangeCipherGroups::Update(
    const FieldTrialsView* field_trials,
    const std::vector<uint16_t>* disabled_groups) {
  // Note: assumption is that these lists contains few elements...so converting
  // to set<> is not worth it.
  std::vector<uint16_t> default_groups =
      SSLStreamAdapter::GetDefaultEphemeralKeyExchangeCipherGroups(
          field_trials);
  // Remove all disabled.
  if (disabled_groups) {
    std::erase_if(default_groups, [&](uint16_t val) {
      return absl::c_linear_search(*disabled_groups, val);
    });
    std::erase_if(enabled_, [&](uint16_t val) {
      return absl::c_linear_search(*disabled_groups, val);
    });
  }

  // Add those enabled by field-trials first.
  std::vector<uint16_t> current = std::move(enabled_);
  enabled_.clear();
  for (auto val : default_groups) {
    if (!absl::c_linear_search(current, val)) {
      enabled_.push_back(val);
    }
  }

  // Then re-add those present (unless already there).
  for (auto val : current) {
    if (!absl::c_linear_search(enabled_, val)) {
      enabled_.push_back(val);
    }
  }
}

}  // namespace webrtc
