/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_DTLSTRANSPORT_H_
#define WEBRTC_P2P_BASE_DTLSTRANSPORT_H_

#include <memory>

#include "webrtc/p2p/base/dtlstransportchannel.h"
#include "webrtc/p2p/base/transport.h"

namespace rtc {
class SSLIdentity;
}

namespace cricket {

class PortAllocator;

// Base should be a descendant of cricket::Transport and have a constructor
// that takes a transport name and PortAllocator.
//
// Everything in this class should be called on the network thread.
template<class Base>
class DtlsTransport : public Base {
 public:
  DtlsTransport(const std::string& name,
                PortAllocator* allocator,
                const rtc::scoped_refptr<rtc::RTCCertificate>& certificate)
      : Base(name, allocator),
        certificate_(certificate),
        secure_role_(rtc::SSL_CLIENT),
        ssl_max_version_(rtc::SSL_PROTOCOL_DTLS_12) {}

  ~DtlsTransport() {
    Base::DestroyAllChannels();
  }

  void SetLocalCertificate(
      const rtc::scoped_refptr<rtc::RTCCertificate>& certificate) override {
    certificate_ = certificate;
  }
  bool GetLocalCertificate(
      rtc::scoped_refptr<rtc::RTCCertificate>* certificate) override {
    if (!certificate_)
      return false;

    *certificate = certificate_;
    return true;
  }

  bool SetSslMaxProtocolVersion(rtc::SSLProtocolVersion version) override {
    ssl_max_version_ = version;
    return true;
  }

  bool ApplyLocalTransportDescription(TransportChannelImpl* channel,
                                      std::string* error_desc) override {
    rtc::SSLFingerprint* local_fp =
        Base::local_description()->identity_fingerprint.get();

    if (!local_fp) {
      certificate_ = nullptr;
    } else if (!Base::VerifyCertificateFingerprint(certificate_.get(), local_fp,
                                                   error_desc)) {
      return false;
    }

    if (!channel->SetLocalCertificate(certificate_)) {
      return BadTransportDescription("Failed to set local identity.",
                                     error_desc);
    }

    // Apply the description in the base class.
    return Base::ApplyLocalTransportDescription(channel, error_desc);
  }

  bool NegotiateTransportDescription(ContentAction local_role,
                                     std::string* error_desc) override {
    if (!Base::local_description() || !Base::remote_description()) {
      const std::string msg = "Local and Remote description must be set before "
                              "transport descriptions are negotiated";
      return BadTransportDescription(msg, error_desc);
    }
    rtc::SSLFingerprint* local_fp =
        Base::local_description()->identity_fingerprint.get();
    rtc::SSLFingerprint* remote_fp =
        Base::remote_description()->identity_fingerprint.get();
    if (remote_fp && local_fp) {
      remote_fingerprint_.reset(new rtc::SSLFingerprint(*remote_fp));
      if (!Base::NegotiateRole(local_role, &secure_role_, error_desc)) {
        return false;
      }
    } else if (local_fp && (local_role == CA_ANSWER)) {
      return BadTransportDescription(
          "Local fingerprint supplied when caller didn't offer DTLS.",
          error_desc);
    } else {
      // We are not doing DTLS
      remote_fingerprint_.reset(new rtc::SSLFingerprint("", nullptr, 0));
    }
    // Now run the negotiation for the base class.
    return Base::NegotiateTransportDescription(local_role, error_desc);
  }

  DtlsTransportChannelWrapper* CreateTransportChannel(int component) override {
    DtlsTransportChannelWrapper* channel = new DtlsTransportChannelWrapper(
        Base::CreateTransportChannel(component));
    channel->SetSslMaxProtocolVersion(ssl_max_version_);
    return channel;
  }

  void DestroyTransportChannel(TransportChannelImpl* channel) override {
    // Kind of ugly, but this lets us do the exact inverse of the create.
    DtlsTransportChannelWrapper* dtls_channel =
        static_cast<DtlsTransportChannelWrapper*>(channel);
    TransportChannelImpl* base_channel = dtls_channel->channel();
    delete dtls_channel;
    Base::DestroyTransportChannel(base_channel);
  }

  bool GetSslRole(rtc::SSLRole* ssl_role) const override {
    ASSERT(ssl_role != NULL);
    *ssl_role = secure_role_;
    return true;
  }

 private:
  bool ApplyNegotiatedTransportDescription(TransportChannelImpl* channel,
                                           std::string* error_desc) override {
    // Set ssl role. Role must be set before fingerprint is applied, which
    // initiates DTLS setup.
    if (!channel->SetSslRole(secure_role_)) {
      return BadTransportDescription("Failed to set ssl role for the channel.",
                                     error_desc);
    }
    // Apply remote fingerprint.
    if (!channel->SetRemoteFingerprint(remote_fingerprint_->algorithm,
                                       reinterpret_cast<const uint8_t*>(
                                           remote_fingerprint_->digest.data()),
                                       remote_fingerprint_->digest.size())) {
      return BadTransportDescription("Failed to apply remote fingerprint.",
                                     error_desc);
    }
    return Base::ApplyNegotiatedTransportDescription(channel, error_desc);
  }

  rtc::scoped_refptr<rtc::RTCCertificate> certificate_;
  rtc::SSLRole secure_role_;
  rtc::SSLProtocolVersion ssl_max_version_;
  std::unique_ptr<rtc::SSLFingerprint> remote_fingerprint_;
};

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_DTLSTRANSPORT_H_
