/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/dtls_srtp_transport.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "api/dtls_transport_interface.h"
#include "api/field_trials_view.h"
#include "p2p/base/packet_transport_internal.h"
#include "p2p/dtls/dtls_transport_internal.h"
#include "pc/srtp_transport.h"
#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/ssl_stream_adapter.h"

namespace webrtc {
namespace {
void ValidateAndLogTransport(DtlsTransportInternal* rtp_dtls_transport,
                             DtlsTransportInternal* old_rtcp_dtls_transport,
                             DtlsTransportInternal* rtcp_dtls_transport,
                             bool is_srtp_active) {
  if (rtcp_dtls_transport && rtcp_dtls_transport != old_rtcp_dtls_transport) {
    // This would only be possible if using BUNDLE but not rtcp-mux, which isn't
    // allowed according to the BUNDLE spec.
    RTC_CHECK(!is_srtp_active)
        << "Setting RTCP for DTLS/SRTP after the DTLS is active "
           "should never happen.";
  }
  if (rtcp_dtls_transport && rtp_dtls_transport) {
    RTC_DCHECK_EQ(rtcp_dtls_transport->transport_name(),
                  rtp_dtls_transport->transport_name());
  }
  if (rtcp_dtls_transport) {
    RTC_LOG(LS_INFO) << "Setting RTCP Transport on "
                     << rtcp_dtls_transport->transport_name() << " transport "
                     << rtcp_dtls_transport;
  }
  if (rtp_dtls_transport) {
    RTC_LOG(LS_INFO) << "Setting RTP Transport on "
                     << rtp_dtls_transport->transport_name() << " transport "
                     << rtp_dtls_transport;
  }
}
}  // namespace

DtlsSrtpTransport::DtlsSrtpTransport(bool rtcp_mux_enabled,
                                     const FieldTrialsView& field_trials)
    : SrtpTransport(rtcp_mux_enabled, field_trials) {}

void DtlsSrtpTransport::SetDtlsTransports(DtlsTransportInternal* rtp_dtls,
                                          DtlsTransportInternal* rtcp_dtls) {
  ValidateAndLogTransport(rtp_dtls, rtcp_dtls_transport(), rtcp_dtls,
                          IsSrtpActive());

  bool rtp_changed = MaybeUnsubscribe(rtp_dtls_transport(), rtp_dtls);
  bool rtcp_changed = MaybeUnsubscribe(rtcp_dtls_transport(), rtcp_dtls);

  // Now pass the RTP transport to RtpTransport.
  SetRtpPacketTransport(rtp_dtls);
  SetRtcpPacketTransport(rtcp_dtls);

  if (rtp_changed) {
    SetupDtlsTransport(rtp_dtls, /*is_rtcp=*/false);
  }
  if (rtcp_changed) {
    SetupDtlsTransport(rtcp_dtls, /*is_rtcp=*/true);
  }
}

void DtlsSrtpTransport::SetDtlsTransportsOwned(
    std::unique_ptr<DtlsTransportInternal> rtp_dtls,
    std::unique_ptr<DtlsTransportInternal> rtcp_dtls) {
  ValidateAndLogTransport(rtp_dtls.get(), rtcp_dtls_transport(),
                          rtcp_dtls.get(), IsSrtpActive());

  bool rtp_changed = MaybeUnsubscribe(rtp_dtls_transport(), rtp_dtls.get());
  bool rtcp_changed = MaybeUnsubscribe(rtcp_dtls_transport(), rtcp_dtls.get());

  // Pass the RTP transport to RtpTransport and ownership of
  // rtcp_dtls_transport.
  SetRtpPacketTransportOwned(std::move(rtp_dtls));
  SetRtcpPacketTransportOwned(std::move(rtcp_dtls));

  if (rtp_changed) {
    SetupDtlsTransport(rtp_dtls_transport(), /*is_rtcp=*/false);
  }

  if (rtcp_changed) {
    SetupDtlsTransport(rtcp_dtls_transport(), /*is_rtcp=*/true);
  }
}

void DtlsSrtpTransport::SetRtcpMuxEnabled(bool enable) {
  SrtpTransport::SetRtcpMuxEnabled(enable);
  if (enable) {
    MaybeSetupDtlsSrtp();
  }
}

void DtlsSrtpTransport::UpdateSendEncryptedHeaderExtensionIds(
    const std::vector<int>& send_extension_ids) {
  if (send_extension_ids_ == send_extension_ids) {
    return;
  }
  send_extension_ids_.emplace(send_extension_ids);
  if (DtlsHandshakeCompleted()) {
    // Reset the crypto parameters to update the send extension IDs.
    SetupRtpDtlsSrtp();
  }
}

void DtlsSrtpTransport::UpdateRecvEncryptedHeaderExtensionIds(
    const std::vector<int>& recv_extension_ids) {
  if (recv_extension_ids_ == recv_extension_ids) {
    return;
  }
  recv_extension_ids_.emplace(recv_extension_ids);
  if (DtlsHandshakeCompleted()) {
    // Reset the crypto parameters to update the receive extension IDs.
    SetupRtpDtlsSrtp();
  }
}

bool DtlsSrtpTransport::IsDtlsActive() {
  auto rtcp_dtls_transport_ptr =
      rtcp_mux_enabled() ? nullptr : rtcp_dtls_transport();
  return rtp_dtls_transport() && rtp_dtls_transport()->IsDtlsActive() &&
         (!rtcp_dtls_transport_ptr || rtcp_dtls_transport_ptr->IsDtlsActive());
}

bool DtlsSrtpTransport::IsDtlsConnected() {
  auto rtcp_dtls_transport_ptr =
      rtcp_mux_enabled() ? nullptr : rtcp_dtls_transport();
  return rtp_dtls_transport() &&
         rtp_dtls_transport()->dtls_state() == DtlsTransportState::kConnected &&
         (!rtcp_dtls_transport_ptr || rtcp_dtls_transport_ptr->dtls_state() ==
                                          DtlsTransportState::kConnected);
}

bool DtlsSrtpTransport::IsDtlsWritable() {
  auto rtcp_packet_transport =
      rtcp_mux_enabled() ? nullptr : rtcp_dtls_transport();
  return rtp_dtls_transport() && rtp_dtls_transport()->writable() &&
         (!rtcp_packet_transport || rtcp_packet_transport->writable());
}

bool DtlsSrtpTransport::DtlsHandshakeCompleted() {
  return IsDtlsActive() && IsDtlsConnected();
}

void DtlsSrtpTransport::MaybeSetupDtlsSrtp() {
  if (IsSrtpActive() || !IsDtlsWritable()) {
    return;
  }

  SetupRtpDtlsSrtp();

  if (!rtcp_mux_enabled() && rtcp_dtls_transport()) {
    SetupRtcpDtlsSrtp();
  }
}

void DtlsSrtpTransport::SetupRtpDtlsSrtp() {
  // Use an empty encrypted header extension ID vector if not set. This could
  // happen when the DTLS handshake is completed before processing the
  // Offer/Answer which contains the encrypted header extension IDs.
  std::vector<int> send_extension_ids;
  std::vector<int> recv_extension_ids;
  if (send_extension_ids_) {
    send_extension_ids = *send_extension_ids_;
  }
  if (recv_extension_ids_) {
    recv_extension_ids = *recv_extension_ids_;
  }

  int selected_crypto_suite;
  ZeroOnFreeBuffer<uint8_t> send_key;
  ZeroOnFreeBuffer<uint8_t> recv_key;

  if (!ExtractParams(rtp_dtls_transport(), &selected_crypto_suite, &send_key,
                     &recv_key) ||
      !SetRtpParams(selected_crypto_suite, send_key, send_extension_ids,
                    selected_crypto_suite, recv_key, recv_extension_ids)) {
    RTC_LOG(LS_WARNING) << "DTLS-SRTP key installation for RTP failed";
  }
}

void DtlsSrtpTransport::SetupRtcpDtlsSrtp() {
  // Return if the DTLS-SRTP is active because the encrypted header extension
  // IDs don't need to be updated for RTCP and the crypto params don't need to
  // be reset.
  if (IsSrtpActive()) {
    return;
  }

  std::vector<int> send_extension_ids;
  std::vector<int> recv_extension_ids;
  if (send_extension_ids_) {
    send_extension_ids = *send_extension_ids_;
  }
  if (recv_extension_ids_) {
    recv_extension_ids = *recv_extension_ids_;
  }

  int selected_crypto_suite;
  ZeroOnFreeBuffer<uint8_t> rtcp_send_key;
  ZeroOnFreeBuffer<uint8_t> rtcp_recv_key;
  if (!ExtractParams(rtcp_dtls_transport(), &selected_crypto_suite,
                     &rtcp_send_key, &rtcp_recv_key) ||
      !SetRtcpParams(selected_crypto_suite, rtcp_send_key, send_extension_ids,
                     selected_crypto_suite, rtcp_recv_key,
                     recv_extension_ids)) {
    RTC_LOG(LS_WARNING) << "DTLS-SRTP key installation for RTCP failed";
  }
}

bool DtlsSrtpTransport::ExtractParams(DtlsTransportInternal* dtls_transport,
                                      int* selected_crypto_suite,
                                      ZeroOnFreeBuffer<uint8_t>* send_key,
                                      ZeroOnFreeBuffer<uint8_t>* recv_key) {
  if (!dtls_transport || !dtls_transport->IsDtlsActive()) {
    return false;
  }

  if (!dtls_transport->GetSrtpCryptoSuite(selected_crypto_suite)) {
    RTC_LOG(LS_ERROR) << "No DTLS-SRTP selected crypto suite";
    return false;
  }

  int key_len;
  int salt_len;
  if (!GetSrtpKeyAndSaltLengths((*selected_crypto_suite), &key_len,
                                &salt_len)) {
    RTC_LOG(LS_ERROR) << "Unknown DTLS-SRTP crypto suite"
                      << selected_crypto_suite;
    return false;
  }

  RTC_LOG(LS_INFO) << "Extracting keys from transport: "
                   << dtls_transport->transport_name();

  // RFC 5705 exporter using the RFC 5764 parameters
  ZeroOnFreeBuffer<uint8_t> dtls_buffer;
  if (!dtls_transport->AppendSrtpKeyingMaterial(dtls_buffer)) {
    RTC_LOG(LS_ERROR) << "DTLS-SRTP key export failed";
    RTC_DCHECK_NOTREACHED();  // This should never happen
    return false;
  }
  // Verify that key material size is as expected.
  RTC_DCHECK_EQ(dtls_buffer.size(),
                static_cast<size_t>(2 * key_len + 2 * salt_len));

  // Sync up the keys with the DTLS-SRTP interface
  // https://datatracker.ietf.org/doc/html/rfc5764#section-4.2
  // The keying material is in the format:
  // client_write_key|server_write_key|client_write_salt|server_write_salt
  ZeroOnFreeBuffer<uint8_t> client_write_key(&dtls_buffer[0], key_len,
                                             key_len + salt_len);
  ZeroOnFreeBuffer<uint8_t> server_write_key(&dtls_buffer[key_len], key_len,
                                             key_len + salt_len);
  client_write_key.AppendData(&dtls_buffer[key_len + key_len], salt_len);
  server_write_key.AppendData(&dtls_buffer[key_len + key_len + salt_len],
                              salt_len);

  SSLRole role;
  if (!dtls_transport->GetDtlsRole(&role)) {
    RTC_LOG(LS_WARNING) << "Failed to get the DTLS role.";
    return false;
  }

  if (role == SSL_SERVER) {
    *send_key = std::move(server_write_key);
    *recv_key = std::move(client_write_key);
  } else {
    *send_key = std::move(client_write_key);
    *recv_key = std::move(server_write_key);
  }
  return true;
}

void DtlsSrtpTransport::SetupDtlsTransport(
    DtlsTransportInternal* dtls_transport,
    bool is_rtcp) {
  if (dtls_transport) {
    dtls_transport->SubscribeDtlsTransportState(
        this,
        [this](DtlsTransportInternal* transport, DtlsTransportState state) {
          OnDtlsState(transport, state);
        });
    // Set the initial state.
    OnDtlsState(dtls_transport, dtls_transport->dtls_state());
  } else {
    // When the transport is removed, we usually reset the SRTP parameters.
    // However, if the RTCP transport is removed because we are enabling RTCP
    // muxing, we should not reset the parameters because the SRTP session
    // will be maintained by the RTP transport.
    if (is_rtcp && rtcp_mux_enabled()) {
      return;
    }
    OnDtlsState(nullptr, DtlsTransportState::kNew);
  }
}

bool DtlsSrtpTransport::MaybeUnsubscribe(DtlsTransportInternal* old_transport,
                                         DtlsTransportInternal* new_transport) {
  if (old_transport && old_transport != new_transport) {
    old_transport->UnsubscribeDtlsTransportState(this);
  }
  return old_transport != new_transport;
}

void DtlsSrtpTransport::OnDtlsState(DtlsTransportInternal* transport,
                                    DtlsTransportState state) {
  RTC_DCHECK(transport == rtp_dtls_transport() ||
             transport == rtcp_dtls_transport());

  if (on_dtls_state_change_) {
    on_dtls_state_change_();
  }

  if (state != DtlsTransportState::kConnected) {
    ResetParams();
  }

  MaybeSetupDtlsSrtp();
}

void DtlsSrtpTransport::OnWritableState(
    PacketTransportInternal* packet_transport) {
  MaybeSetupDtlsSrtp();
}

void DtlsSrtpTransport::SetOnDtlsStateChange(
    absl::AnyInvocable<void()> callback) {
  on_dtls_state_change_ = std::move(callback);
}
}  // namespace webrtc
