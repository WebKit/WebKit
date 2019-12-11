/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_NO_OP_DTLS_TRANSPORT_H_
#define P2P_BASE_NO_OP_DTLS_TRANSPORT_H_

#include <memory>
#include <string>
#include <vector>

#include "api/crypto/crypto_options.h"
#include "p2p/base/dtls_transport_internal.h"
#include "p2p/base/ice_transport_internal.h"
#include "p2p/base/packet_transport_internal.h"
#include "rtc_base/buffer.h"
#include "rtc_base/buffer_queue.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/stream.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/thread_checker.h"

namespace cricket {

constexpr int kNoOpDtlsTransportComponent = -1;

// This implementation wraps a cricket::DtlsTransport, and takes
// ownership of it.
// The implementation does not perform any operations, except of being
// "connected". The purpose of this implementation is to disable RTP transport
// while MediaTransport is used.
//
// This implementation is only temporary. Long-term we will refactor and disable
// RTP transport entirely when MediaTransport is used. Always connected (after
// ICE), no-op, dtls transport. This is used when DTLS is disabled.
//
// MaybeCreateJsepTransport controller expects DTLS connection to send a
// 'connected' signal _after_ it is created (if it is created in a connected
// state, that would not be noticed by jsep transport controller). Therefore,
// the no-op dtls transport will wait for ICE event "writable", and then
// immediately report that it's connected (emulating 0-rtt connection).
//
// We could simply not set a dtls to active (not set a certificate on the DTLS),
// and it would use an underyling connection instead.
// However, when MediaTransport is used, we want to entirely disable
// dtls/srtp/rtp, in order to avoid multiplexing issues, such as "Failed to
// unprotect RTCP packet".
class NoOpDtlsTransport : public DtlsTransportInternal {
 public:
  NoOpDtlsTransport(IceTransportInternal* ice_transport,
                    const webrtc::CryptoOptions& crypto_options);

  ~NoOpDtlsTransport() override;
  const webrtc::CryptoOptions& crypto_options() const override;
  DtlsTransportState dtls_state() const override;
  int component() const override;
  bool IsDtlsActive() const override;
  bool GetDtlsRole(rtc::SSLRole* role) const override;
  bool SetDtlsRole(rtc::SSLRole role) override;
  bool GetSrtpCryptoSuite(int* cipher) override;
  bool GetSslCipherSuite(int* cipher) override;
  rtc::scoped_refptr<rtc::RTCCertificate> GetLocalCertificate() const override;
  bool SetLocalCertificate(
      const rtc::scoped_refptr<rtc::RTCCertificate>& certificate) override;
  std::unique_ptr<rtc::SSLCertChain> GetRemoteSSLCertChain() const override;
  bool ExportKeyingMaterial(const std::string& label,
                            const uint8_t* context,
                            size_t context_len,
                            bool use_context,
                            uint8_t* result,
                            size_t result_len) override;
  bool SetRemoteFingerprint(const std::string& digest_alg,
                            const uint8_t* digest,
                            size_t digest_len) override;
  bool SetSslMaxProtocolVersion(rtc::SSLProtocolVersion version) override;
  IceTransportInternal* ice_transport() override;

  const std::string& transport_name() const override;
  bool writable() const override;
  bool receiving() const override;

 private:
  void OnReadyToSend(rtc::PacketTransportInternal* transport);
  void OnWritableState(rtc::PacketTransportInternal* transport);
  void OnNetworkRouteChanged(absl::optional<rtc::NetworkRoute> network_route);
  void OnReceivingState(rtc::PacketTransportInternal* transport);

  int SendPacket(const char* data,
                 size_t len,
                 const rtc::PacketOptions& options,
                 int flags) override;
  int SetOption(rtc::Socket::Option opt, int value) override;
  int GetError() override;

  rtc::ThreadChecker thread_checker_;

  webrtc::CryptoOptions crypto_options_;
  IceTransportInternal* ice_transport_;
  bool is_writable_ = false;
};

}  // namespace cricket

#endif  // P2P_BASE_NO_OP_DTLS_TRANSPORT_H_
