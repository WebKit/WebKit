/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_DTLSTRANSPORT_H_
#define P2P_BASE_DTLSTRANSPORT_H_

#include <memory>
#include <string>
#include <vector>

#include "p2p/base/dtlstransportinternal.h"
#include "p2p/base/icetransportinternal.h"
#include "rtc_base/buffer.h"
#include "rtc_base/bufferqueue.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/sslstreamadapter.h"
#include "rtc_base/stream.h"

namespace rtc {
class PacketTransportInternal;
}

namespace cricket {

// A bridge between a packet-oriented/transport-type interface on
// the bottom and a StreamInterface on the top.
class StreamInterfaceChannel : public rtc::StreamInterface {
 public:
  explicit StreamInterfaceChannel(IceTransportInternal* ice_transport);

  // Push in a packet; this gets pulled out from Read().
  bool OnPacketReceived(const char* data, size_t size);

  // Implementations of StreamInterface
  rtc::StreamState GetState() const override;
  void Close() override;
  rtc::StreamResult Read(void* buffer,
                         size_t buffer_len,
                         size_t* read,
                         int* error) override;
  rtc::StreamResult Write(const void* data,
                          size_t data_len,
                          size_t* written,
                          int* error) override;

 private:
  IceTransportInternal* ice_transport_;  // owned by DtlsTransport
  rtc::StreamState state_;
  rtc::BufferQueue packets_;

  RTC_DISALLOW_COPY_AND_ASSIGN(StreamInterfaceChannel);
};

// This class provides a DTLS SSLStreamAdapter inside a TransportChannel-style
// packet-based interface, wrapping an existing TransportChannel instance
// (e.g a P2PTransportChannel)
// Here's the way this works:
//
//   DtlsTransport {
//       SSLStreamAdapter* dtls_ {
//           StreamInterfaceChannel downward_ {
//               IceTransportInternal* ice_transport_;
//           }
//       }
//   }
//
//   - Data which comes into DtlsTransport from the underlying
//     ice_transport_ via OnReadPacket() is checked for whether it is DTLS
//     or not, and if it is, is passed to DtlsTransport::HandleDtlsPacket,
//     which pushes it into to downward_. dtls_ is listening for events on
//     downward_, so it immediately calls downward_->Read().
//
//   - Data written to DtlsTransport is passed either to downward_ or directly
//     to ice_transport_, depending on whether DTLS is negotiated and whether
//     the flags include PF_SRTP_BYPASS
//
//   - The SSLStreamAdapter writes to downward_->Write() which translates it
//     into packet writes on ice_transport_.
class DtlsTransport : public DtlsTransportInternal {
 public:
  // |ice_transport| is the ICE transport this DTLS transport is wrapping.
  //
  // |crypto_options| are the options used for the DTLS handshake. This affects
  // whether GCM crypto suites are negotiated.
  explicit DtlsTransport(IceTransportInternal* ice_transport,
                         const rtc::CryptoOptions& crypto_options);
  ~DtlsTransport() override;

  const rtc::CryptoOptions& crypto_options() const override;
  DtlsTransportState dtls_state() const override;
  const std::string& transport_name() const override;
  int component() const override;

  // Returns false if no local certificate was set, or if the peer doesn't
  // support DTLS.
  bool IsDtlsActive() const override;

  bool SetLocalCertificate(
      const rtc::scoped_refptr<rtc::RTCCertificate>& certificate) override;
  rtc::scoped_refptr<rtc::RTCCertificate> GetLocalCertificate() const override;

  bool SetRemoteFingerprint(const std::string& digest_alg,
                            const uint8_t* digest,
                            size_t digest_len) override;

  // Called to send a packet (via DTLS, if turned on).
  int SendPacket(const char* data,
                 size_t size,
                 const rtc::PacketOptions& options,
                 int flags) override;

  bool GetOption(rtc::Socket::Option opt, int* value) override;

  virtual bool SetSslMaxProtocolVersion(rtc::SSLProtocolVersion version);

  // Find out which DTLS-SRTP cipher was negotiated
  bool GetSrtpCryptoSuite(int* cipher) override;

  bool GetSslRole(rtc::SSLRole* role) const override;
  bool SetSslRole(rtc::SSLRole role) override;

  // Find out which DTLS cipher was negotiated
  bool GetSslCipherSuite(int* cipher) override;

  // Once DTLS has been established, this method retrieves the certificate in
  // use by the remote peer, for use in external identity verification.
  std::unique_ptr<rtc::SSLCertificate> GetRemoteSSLCertificate() const override;

  // Once DTLS has established (i.e., this ice_transport is writable), this
  // method extracts the keys negotiated during the DTLS handshake, for use in
  // external encryption. DTLS-SRTP uses this to extract the needed SRTP keys.
  // See the SSLStreamAdapter documentation for info on the specific parameters.
  bool ExportKeyingMaterial(const std::string& label,
                            const uint8_t* context,
                            size_t context_len,
                            bool use_context,
                            uint8_t* result,
                            size_t result_len) override;

  IceTransportInternal* ice_transport() override;

  // For informational purposes. Tells if the DTLS handshake has finished.
  // This may be true even if writable() is false, if the remote fingerprint
  // has not yet been verified.
  bool IsDtlsConnected();

  bool receiving() const override;
  bool writable() const override;

  int GetError() override;

  rtc::Optional<rtc::NetworkRoute> network_route() const override;

  int SetOption(rtc::Socket::Option opt, int value) override;

  std::string ToString() const {
    const char RECEIVING_ABBREV[2] = {'_', 'R'};
    const char WRITABLE_ABBREV[2] = {'_', 'W'};
    std::stringstream ss;
    ss << "DtlsTransport[" << transport_name_ << "|" << component_ << "|"
       << RECEIVING_ABBREV[receiving()] << WRITABLE_ABBREV[writable()] << "]";
    return ss.str();
  }

 private:
  void OnWritableState(rtc::PacketTransportInternal* transport);
  void OnReadPacket(rtc::PacketTransportInternal* transport,
                    const char* data,
                    size_t size,
                    const rtc::PacketTime& packet_time,
                    int flags);
  void OnSentPacket(rtc::PacketTransportInternal* transport,
                    const rtc::SentPacket& sent_packet);
  void OnReadyToSend(rtc::PacketTransportInternal* transport);
  void OnReceivingState(rtc::PacketTransportInternal* transport);
  void OnDtlsEvent(rtc::StreamInterface* stream_, int sig, int err);
  void OnNetworkRouteChanged(rtc::Optional<rtc::NetworkRoute> network_route);
  bool SetupDtls();
  void MaybeStartDtls();
  bool HandleDtlsPacket(const char* data, size_t size);
  void OnDtlsHandshakeError(rtc::SSLHandshakeError error);
  void ConfigureHandshakeTimeout();

  void set_receiving(bool receiving);
  void set_writable(bool writable);
  // Sets the DTLS state, signaling if necessary.
  void set_dtls_state(DtlsTransportState state);

  std::string transport_name_;
  int component_;
  DtlsTransportState dtls_state_ = DTLS_TRANSPORT_NEW;
  rtc::Thread* network_thread_;  // Everything should occur on this thread.
  // Underlying ice_transport, not owned by this class.
  IceTransportInternal* const ice_transport_;
  std::unique_ptr<rtc::SSLStreamAdapter> dtls_;  // The DTLS stream
  StreamInterfaceChannel*
      downward_;  // Wrapper for ice_transport_, owned by dtls_.
  std::vector<int> srtp_ciphers_;  // SRTP ciphers to use with DTLS.
  bool dtls_active_ = false;
  rtc::scoped_refptr<rtc::RTCCertificate> local_certificate_;
  rtc::SSLRole ssl_role_;
  rtc::SSLProtocolVersion ssl_max_version_;
  rtc::CryptoOptions crypto_options_;
  rtc::Buffer remote_fingerprint_value_;
  std::string remote_fingerprint_algorithm_;

  // Cached DTLS ClientHello packet that was received before we started the
  // DTLS handshake. This could happen if the hello was received before the
  // ice transport became writable, or before a remote fingerprint was received.
  rtc::Buffer cached_client_hello_;

  bool receiving_ = false;
  bool writable_ = false;

  RTC_DISALLOW_COPY_AND_ASSIGN(DtlsTransport);
};

}  // namespace cricket

#endif  // P2P_BASE_DTLSTRANSPORT_H_
