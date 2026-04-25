/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_DTLS_DTLS_TRANSPORT_INTERNAL_H_
#define P2P_DTLS_DTLS_TRANSPORT_INTERNAL_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

#include "absl/strings/string_view.h"
#include "api/dtls_transport_interface.h"
#include "api/rtc_error.h"
#include "api/scoped_refptr.h"
#include "p2p/base/ice_transport_internal.h"
#include "p2p/base/packet_transport_internal.h"
#include "rtc_base/buffer.h"
#include "rtc_base/callback_list.h"
#include "rtc_base/checks.h"
#include "rtc_base/rtc_certificate.h"
#include "rtc_base/ssl_certificate.h"
#include "rtc_base/ssl_stream_adapter.h"

namespace webrtc {

enum PacketFlags {
  PF_NORMAL = 0x00,       // A normal packet.
  PF_SRTP_BYPASS = 0x01,  // An encrypted SRTP packet; bypass any additional
                          // crypto provided by the transport (e.g. DTLS)
};

// DtlsTransportInternal is an internal interface that does DTLS, also
// negotiating SRTP crypto suites so that it may be used for DTLS-SRTP.
//
// Once the public interface is supported,
// (https://www.w3.org/TR/webrtc/#rtcdtlstransport-interface)
// the DtlsTransportInterface will be split from this class.
class DtlsTransportInternal : public PacketTransportInternal {
 public:
  ~DtlsTransportInternal() override;

  DtlsTransportInternal(const DtlsTransportInternal&) = delete;
  DtlsTransportInternal& operator=(const DtlsTransportInternal&) = delete;

  virtual DtlsTransportState dtls_state() const = 0;

  virtual int component() const = 0;

  virtual bool IsDtlsActive() const = 0;

  virtual bool GetDtlsRole(SSLRole* role) const = 0;

  virtual bool SetDtlsRole(SSLRole role) = 0;

  // Finds out which TLS/DTLS version is running.
  virtual bool GetSslVersionBytes(int* version) const = 0;
  // Return the the ID of the group used by the adapters most recently
  // completed handshake, or 0 if not applicable (e.g. before the handshake).
  virtual uint16_t GetSslGroupId() const = 0;
  // Finds out which DTLS-SRTP cipher was negotiated.
  // TODO(zhihuang): Remove this once all dependencies implement this.
  virtual bool GetSrtpCryptoSuite(int* cipher) const = 0;

  // Finds out which DTLS cipher was negotiated.
  // TODO(zhihuang): Remove this once all dependencies implement this.
  virtual bool GetSslCipherSuite(int* cipher) const = 0;
  virtual std::optional<absl::string_view> GetTlsCipherSuiteName() const = 0;

  // Find out which signature algorithm was used by the peer. Returns values
  // from
  // https://www.iana.org/assignments/tls-parameters/tls-parameters.xhtml#tls-signaturescheme
  // If not applicable, it returns zero.
  virtual uint16_t GetSslPeerSignatureAlgorithm() const = 0;

  virtual bool SetLocalCertificate(
      const scoped_refptr<RTCCertificate>& certificate) = 0;

  // Gets a copy of the remote side's SSL certificate chain.
  virtual std::unique_ptr<SSLCertChain> GetRemoteSSLCertChain() const = 0;

  // Allows key material to be extracted for external encryption.
  virtual bool AppendSrtpKeyingMaterial(
      ZeroOnFreeBuffer<uint8_t>& keying_material) = 0;

  // Older API. If not overridden in subclasses, will fail.
  [[deprecated]] virtual bool ExportSrtpKeyingMaterial(
      ZeroOnFreeBuffer<uint8_t>& keying_material) {
    RTC_DCHECK_NOTREACHED() << "Superseded by AppendSrtpKeyingMaterial";
    return false;
  }

  // Set DTLS remote fingerprint and role. Must be after local identity set.
  virtual RTCError SetRemoteParameters(absl::string_view digest_alg,
                                       const uint8_t* digest,
                                       size_t digest_len,
                                       std::optional<SSLRole> role) = 0;

  // Expose the underneath IceTransport.
  virtual IceTransportInternal* ice_transport() = 0;

  // F: void(DtlsTransportInternal*, const DtlsTransportState)
  template <typename F>
  void SubscribeDtlsTransportState(F&& callback) {
    dtls_transport_state_callback_list_.AddReceiver(std::forward<F>(callback));
  }

  template <typename F>
  void SubscribeDtlsTransportState(const void* id, F&& callback) {
    dtls_transport_state_callback_list_.AddReceiver(id,
                                                    std::forward<F>(callback));
  }
  // Unsubscribe the subscription with given id.
  void UnsubscribeDtlsTransportState(const void* id) {
    dtls_transport_state_callback_list_.RemoveReceivers(id);
  }

  void SendDtlsState(DtlsTransportInternal* transport,
                     DtlsTransportState state) {
    dtls_transport_state_callback_list_.Send(transport, state);
  }

  // Emitted whenever the Dtls handshake failed on some transport channel.
  // F: void(SSLHandshakeError)
  template <typename F>
  [[deprecated]] void SubscribeDtlsHandshakeError(F&& callback) {
    dtls_handshake_error_callback_list_.AddReceiver(std::forward<F>(callback));
  }
  template <typename F>
  void SubscribeDtlsHandshakeError(void* tag, F&& callback) {
    dtls_handshake_error_callback_list_.AddReceiver(tag,
                                                    std::forward<F>(callback));
  }

  void SendDtlsHandshakeError(SSLHandshakeError error) {
    dtls_handshake_error_callback_list_.Send(error);
  }

 protected:
  DtlsTransportInternal();

 private:
  CallbackList<const SSLHandshakeError> dtls_handshake_error_callback_list_;
  CallbackList<DtlsTransportInternal*, const DtlsTransportState>
      dtls_transport_state_callback_list_;
};

}  //  namespace webrtc


#endif  // P2P_DTLS_DTLS_TRANSPORT_INTERNAL_H_
