/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_QUIC_QUICTRANSPORT_H_
#define WEBRTC_P2P_QUIC_QUICTRANSPORT_H_

#include <string>
#include <map>
#include <memory>

#include "webrtc/p2p/base/jseptransport.h"
#include "webrtc/p2p/quic/quictransportchannel.h"

namespace cricket {

class P2PTransportChannel;
class PortAllocator;

// TODO(deadbeef): To get QUIC working with TransportController again, would
// need to merge this class with Transport (or make separate DTLS/QUIC
// subclasses). The only difference between the two (as of typing this) is that
// the QUIC channel *requires* a fingerprint, whereas the DTLS channel can
// operate in a passthrough mode when SDES is used.
class QuicTransport : public Transport {
 public:
  QuicTransport(const std::string& name,
                PortAllocator* allocator,
                const rtc::scoped_refptr<rtc::RTCCertificate>& certificate);

  ~QuicTransport() override;

  // Transport overrides.
  void SetLocalCertificate(
      const rtc::scoped_refptr<rtc::RTCCertificate>& certificate) override;
  bool GetLocalCertificate(
      rtc::scoped_refptr<rtc::RTCCertificate>* certificate) override;
  bool SetSslMaxProtocolVersion(rtc::SSLProtocolVersion version) override {
    return true;  // Not needed by QUIC
  }
  bool GetSslRole(rtc::SSLRole* ssl_role) const override;

 protected:
  // Transport overrides.
  QuicTransportChannel* CreateTransportChannel(int component) override;
  void DestroyTransportChannel(TransportChannelImpl* channel) override;
  bool ApplyLocalTransportDescription(TransportChannelImpl* channel,
                                      std::string* error_desc) override;
  bool NegotiateTransportDescription(ContentAction action,
                                     std::string* error_desc) override;
  bool ApplyNegotiatedTransportDescription(TransportChannelImpl* channel,
                                           std::string* error_desc) override;

 private:
  rtc::scoped_refptr<rtc::RTCCertificate> local_certificate_;
  rtc::SSLRole local_role_ = rtc::SSL_CLIENT;
  std::unique_ptr<rtc::SSLFingerprint> remote_fingerprint_;
};

}  // namespace cricket

#endif  // WEBRTC_P2P_QUIC_QUICTRANSPORT_H_
