/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/p2p/quic/quictransport.h"

#include "webrtc/base/checks.h"
#include "webrtc/p2p/base/p2ptransportchannel.h"

namespace cricket {

QuicTransport::QuicTransport(
    const std::string& name,
    PortAllocator* allocator,
    const rtc::scoped_refptr<rtc::RTCCertificate>& certificate)
    : Transport(name, allocator), local_certificate_(certificate) {}

QuicTransport::~QuicTransport() {
  DestroyAllChannels();
}

void QuicTransport::SetLocalCertificate(
    const rtc::scoped_refptr<rtc::RTCCertificate>& certificate) {
  local_certificate_ = certificate;
}
bool QuicTransport::GetLocalCertificate(
    rtc::scoped_refptr<rtc::RTCCertificate>* certificate) {
  if (!local_certificate_) {
    return false;
  }
  *certificate = local_certificate_;
  return true;
}

bool QuicTransport::ApplyLocalTransportDescription(
    TransportChannelImpl* channel,
    std::string* error_desc) {
  rtc::SSLFingerprint* local_fp =
      local_description()->identity_fingerprint.get();
  if (!VerifyCertificateFingerprint(local_certificate_.get(), local_fp,
                                    error_desc)) {
    return false;
  }
  if (!channel->SetLocalCertificate(local_certificate_)) {
    return BadTransportDescription("Failed to set local identity.", error_desc);
  }
  return Transport::ApplyLocalTransportDescription(channel, error_desc);
}

bool QuicTransport::NegotiateTransportDescription(ContentAction action,
                                                  std::string* error_desc) {
  if (!local_description() || !remote_description()) {
    const std::string msg =
        "Local and Remote description must be set before "
        "transport descriptions are negotiated";
    return BadTransportDescription(msg, error_desc);
  }
  rtc::SSLFingerprint* local_fp =
      local_description()->identity_fingerprint.get();
  rtc::SSLFingerprint* remote_fp =
      remote_description()->identity_fingerprint.get();
  if (!local_fp || !remote_fp) {
    return BadTransportDescription("Fingerprints must be supplied for QUIC.",
                                   error_desc);
  }
  remote_fingerprint_.reset(new rtc::SSLFingerprint(*remote_fp));
  if (!NegotiateRole(action, &local_role_, error_desc)) {
    return false;
  }
  // Now run the negotiation for the Transport class.
  return Transport::NegotiateTransportDescription(action, error_desc);
}

QuicTransportChannel* QuicTransport::CreateTransportChannel(int component) {
  P2PTransportChannel* ice_channel =
      new P2PTransportChannel(name(), component, port_allocator());
  return new QuicTransportChannel(ice_channel);
}

void QuicTransport::DestroyTransportChannel(TransportChannelImpl* channel) {
  delete channel;
}

bool QuicTransport::GetSslRole(rtc::SSLRole* ssl_role) const {
  RTC_DCHECK(ssl_role != NULL);
  *ssl_role = local_role_;
  return true;
}

bool QuicTransport::ApplyNegotiatedTransportDescription(
    TransportChannelImpl* channel,
    std::string* error_desc) {
  // Set ssl role and remote fingerprint. These are required for QUIC setup.
  if (!channel->SetSslRole(local_role_)) {
    return BadTransportDescription("Failed to set ssl role for the channel.",
                                   error_desc);
  }
  // Apply remote fingerprint.
  if (!channel->SetRemoteFingerprint(
          remote_fingerprint_->algorithm,
          reinterpret_cast<const uint8_t*>(remote_fingerprint_->digest.data()),
          remote_fingerprint_->digest.size())) {
    return BadTransportDescription("Failed to apply remote fingerprint.",
                                   error_desc);
  }
  return Transport::ApplyNegotiatedTransportDescription(channel, error_desc);
}

}  // namespace cricket
