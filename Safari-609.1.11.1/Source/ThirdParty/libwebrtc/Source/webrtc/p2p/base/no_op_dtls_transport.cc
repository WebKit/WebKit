/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/no_op_dtls_transport.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "absl/memory/memory.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "logging/rtc_event_log/events/rtc_event_dtls_transport_state.h"
#include "logging/rtc_event_log/events/rtc_event_dtls_writable_state.h"
#include "p2p/base/packet_transport_internal.h"
#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/dscp.h"
#include "rtc_base/logging.h"
#include "rtc_base/message_queue.h"
#include "rtc_base/rtc_certificate.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/stream.h"
#include "rtc_base/thread.h"

namespace cricket {

NoOpDtlsTransport::NoOpDtlsTransport(
    IceTransportInternal* ice_transport,
    const webrtc::CryptoOptions& crypto_options)
    : crypto_options_(webrtc::CryptoOptions::NoGcm()),
      ice_transport_(ice_transport) {
  RTC_DCHECK(ice_transport_);
  ice_transport_->SignalWritableState.connect(
      this, &NoOpDtlsTransport::OnWritableState);
  ice_transport_->SignalReadyToSend.connect(this,
                                            &NoOpDtlsTransport::OnReadyToSend);
  ice_transport_->SignalReceivingState.connect(
      this, &NoOpDtlsTransport::OnReceivingState);
  ice_transport_->SignalNetworkRouteChanged.connect(
      this, &NoOpDtlsTransport::OnNetworkRouteChanged);
}

NoOpDtlsTransport::~NoOpDtlsTransport() {}
const webrtc::CryptoOptions& NoOpDtlsTransport::crypto_options() const {
  return crypto_options_;
}
DtlsTransportState NoOpDtlsTransport::dtls_state() const {
  return DTLS_TRANSPORT_CONNECTED;
}
int NoOpDtlsTransport::component() const {
  return kNoOpDtlsTransportComponent;
}
bool NoOpDtlsTransport::IsDtlsActive() const {
  return true;
}
bool NoOpDtlsTransport::GetDtlsRole(rtc::SSLRole* role) const {
  return false;
}
bool NoOpDtlsTransport::SetDtlsRole(rtc::SSLRole role) {
  return false;
}
bool NoOpDtlsTransport::GetSrtpCryptoSuite(int* cipher) {
  return false;
}
bool NoOpDtlsTransport::GetSslCipherSuite(int* cipher) {
  return false;
}
rtc::scoped_refptr<rtc::RTCCertificate> NoOpDtlsTransport::GetLocalCertificate()
    const {
  return rtc::scoped_refptr<rtc::RTCCertificate>();
}
bool NoOpDtlsTransport::SetLocalCertificate(
    const rtc::scoped_refptr<rtc::RTCCertificate>& certificate) {
  return false;
}
std::unique_ptr<rtc::SSLCertChain> NoOpDtlsTransport::GetRemoteSSLCertChain()
    const {
  return std::unique_ptr<rtc::SSLCertChain>();
}
bool NoOpDtlsTransport::ExportKeyingMaterial(const std::string& label,
                                             const uint8_t* context,
                                             size_t context_len,
                                             bool use_context,
                                             uint8_t* result,
                                             size_t result_len) {
  return false;
}
bool NoOpDtlsTransport::SetRemoteFingerprint(const std::string& digest_alg,
                                             const uint8_t* digest,
                                             size_t digest_len) {
  return true;
}
bool NoOpDtlsTransport::SetSslMaxProtocolVersion(
    rtc::SSLProtocolVersion version) {
  return true;
}
IceTransportInternal* NoOpDtlsTransport::ice_transport() {
  return ice_transport_;
}

void NoOpDtlsTransport::OnReadyToSend(rtc::PacketTransportInternal* transport) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  if (is_writable_) {
    SignalReadyToSend(this);
  }
}

void NoOpDtlsTransport::OnWritableState(
    rtc::PacketTransportInternal* transport) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  is_writable_ = ice_transport_->writable();
  if (is_writable_) {
    SignalWritableState(this);
  }
}
const std::string& NoOpDtlsTransport::transport_name() const {
  return ice_transport_->transport_name();
}
bool NoOpDtlsTransport::writable() const {
  return ice_transport_->writable();
}
bool NoOpDtlsTransport::receiving() const {
  return ice_transport_->receiving();
}
int NoOpDtlsTransport::SendPacket(const char* data,
                                  size_t len,
                                  const rtc::PacketOptions& options,
                                  int flags) {
  return 0;
}

int NoOpDtlsTransport::SetOption(rtc::Socket::Option opt, int value) {
  return ice_transport_->SetOption(opt, value);
}

int NoOpDtlsTransport::GetError() {
  return ice_transport_->GetError();
}

void NoOpDtlsTransport::OnNetworkRouteChanged(
    absl::optional<rtc::NetworkRoute> network_route) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  SignalNetworkRouteChanged(network_route);
}

void NoOpDtlsTransport::OnReceivingState(
    rtc::PacketTransportInternal* transport) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  SignalReceivingState(this);
}

}  // namespace cricket
