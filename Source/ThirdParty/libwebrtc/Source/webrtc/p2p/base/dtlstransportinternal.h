/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_DTLSTRANSPORTINTERNAL_H_
#define WEBRTC_P2P_BASE_DTLSTRANSPORTINTERNAL_H_

#include <memory>
#include <string>
#include <vector>

#include "webrtc/base/sslstreamadapter.h"
#include "webrtc/base/stringencode.h"
#include "webrtc/p2p/base/icetransportinternal.h"
#include "webrtc/p2p/base/jseptransport.h"
#include "webrtc/p2p/base/packettransportinternal.h"

namespace cricket {

enum PacketFlags {
  PF_NORMAL = 0x00,       // A normal packet.
  PF_SRTP_BYPASS = 0x01,  // An encrypted SRTP packet; bypass any additional
                          // crypto provided by the transport (e.g. DTLS)
};

// DtlsTransportInternal is an internal interface that does DTLS.
// Once the public interface is supported,
// (https://www.w3.org/TR/webrtc/#rtcdtlstransport-interface)
// the DtlsTransportInterface will be split from this class.
class DtlsTransportInternal : public rtc::PacketTransportInternal {
 public:
  virtual ~DtlsTransportInternal() {}

  virtual DtlsTransportState dtls_state() const = 0;

  virtual const std::string& transport_name() const = 0;

  virtual int component() const = 0;

  virtual bool IsDtlsActive() const = 0;

  virtual bool GetSslRole(rtc::SSLRole* role) const = 0;

  virtual bool SetSslRole(rtc::SSLRole role) = 0;

  // Sets up the ciphers to use for DTLS-SRTP.
  virtual bool SetSrtpCryptoSuites(const std::vector<int>& ciphers) = 0;

  // Keep the original one for backward compatibility until all dependencies
  // move away. TODO(zhihuang): Remove this function.
  virtual bool SetSrtpCiphers(const std::vector<std::string>& ciphers) = 0;

  // Finds out which DTLS-SRTP cipher was negotiated.
  // TODO(zhihuang): Remove this once all dependencies implement this.
  virtual bool GetSrtpCryptoSuite(int* cipher) = 0;

  // Finds out which DTLS cipher was negotiated.
  // TODO(zhihuang): Remove this once all dependencies implement this.
  virtual bool GetSslCipherSuite(int* cipher) = 0;

  // Gets the local RTCCertificate used for DTLS.
  virtual rtc::scoped_refptr<rtc::RTCCertificate> GetLocalCertificate()
      const = 0;

  virtual bool SetLocalCertificate(
      const rtc::scoped_refptr<rtc::RTCCertificate>& certificate) = 0;

  // Gets a copy of the remote side's SSL certificate.
  virtual std::unique_ptr<rtc::SSLCertificate> GetRemoteSSLCertificate()
      const = 0;

  // Allows key material to be extracted for external encryption.
  virtual bool ExportKeyingMaterial(const std::string& label,
                                    const uint8_t* context,
                                    size_t context_len,
                                    bool use_context,
                                    uint8_t* result,
                                    size_t result_len) = 0;

  // Set DTLS remote fingerprint. Must be after local identity set.
  virtual bool SetRemoteFingerprint(const std::string& digest_alg,
                                    const uint8_t* digest,
                                    size_t digest_len) = 0;

  // Expose the underneath IceTransport.
  virtual IceTransportInternal* ice_transport() = 0;

  sigslot::signal2<DtlsTransportInternal*, DtlsTransportState> SignalDtlsState;

  // Emitted whenever the Dtls handshake failed on some transport channel.
  sigslot::signal1<rtc::SSLHandshakeError> SignalDtlsHandshakeError;

  // Debugging description of this transport.
  std::string debug_name() const override {
    return transport_name() + " " + rtc::ToString(component());
  }

 protected:
  DtlsTransportInternal() {}

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(DtlsTransportInternal);
};

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_DTLSTRANSPORTINTERNAL_H_
