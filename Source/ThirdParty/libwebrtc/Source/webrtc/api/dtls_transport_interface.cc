/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/dtls_transport_interface.h"

#include <memory>
#include <optional>
#include <utility>

#include "rtc_base/ssl_certificate.h"

namespace webrtc {

DtlsTransportInformation::DtlsTransportInformation()
    : state_(DtlsTransportState::kNew) {}

DtlsTransportInformation::DtlsTransportInformation(DtlsTransportState state)
    : state_(state) {}

DtlsTransportInformation::DtlsTransportInformation(
    DtlsTransportState state,
    std::optional<DtlsTransportTlsRole> role,
    std::optional<int> tls_version,
    std::optional<int> ssl_cipher_suite,
    std::optional<int> srtp_cipher_suite,
    std::unique_ptr<SSLCertChain> remote_ssl_certificates,
    std::optional<int> ssl_group_id)
    : state_(state),
      role_(role),
      tls_version_(tls_version),
      ssl_cipher_suite_(ssl_cipher_suite),
      srtp_cipher_suite_(srtp_cipher_suite),
      remote_ssl_certificates_(std::move(remote_ssl_certificates)),
      ssl_group_id_(ssl_group_id) {}

// Deprecated version
DtlsTransportInformation::DtlsTransportInformation(
    DtlsTransportState state,
    std::optional<int> tls_version,
    std::optional<int> ssl_cipher_suite,
    std::optional<int> srtp_cipher_suite,
    std::unique_ptr<SSLCertChain> remote_ssl_certificates)
    : state_(state),
      role_(std::nullopt),
      tls_version_(tls_version),
      ssl_cipher_suite_(ssl_cipher_suite),
      srtp_cipher_suite_(srtp_cipher_suite),
      remote_ssl_certificates_(std::move(remote_ssl_certificates)) {}

DtlsTransportInformation::DtlsTransportInformation(
    const DtlsTransportInformation& c)
    : state_(c.state()),
      role_(c.role_),
      tls_version_(c.tls_version_),
      ssl_cipher_suite_(c.ssl_cipher_suite_),
      srtp_cipher_suite_(c.srtp_cipher_suite_),
      remote_ssl_certificates_(c.remote_ssl_certificates()
                                   ? c.remote_ssl_certificates()->Clone()
                                   : nullptr),
      ssl_group_id_(c.ssl_group_id_) {}

DtlsTransportInformation& DtlsTransportInformation::operator=(
    const DtlsTransportInformation& c) {
  state_ = c.state();
  role_ = c.role_;
  tls_version_ = c.tls_version_;
  ssl_cipher_suite_ = c.ssl_cipher_suite_;
  srtp_cipher_suite_ = c.srtp_cipher_suite_;
  remote_ssl_certificates_ = c.remote_ssl_certificates()
                                 ? c.remote_ssl_certificates()->Clone()
                                 : nullptr;
  ssl_group_id_ = c.ssl_group_id_;
  return *this;
}

}  // namespace webrtc
