/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_TRANSPORTCHANNELIMPL_H_
#define P2P_BASE_TRANSPORTCHANNELIMPL_H_

#include <string>

#include "p2p/base/icetransportinternal.h"
#include "p2p/base/transportchannel.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {
class MetricsObserverInterface;
}

namespace cricket {

class Candidate;

// Base class for real implementations of TransportChannel.  This includes some
// methods called only by Transport, which do not need to be exposed to the
// client.
class TransportChannelImpl : public TransportChannel {
 public:
  explicit TransportChannelImpl(const std::string& transport_name,
                                int component)
      : TransportChannel(transport_name, component) {}

  // For ICE channels.
  virtual IceRole GetIceRole() const = 0;
  virtual void SetIceRole(IceRole role) = 0;
  virtual void SetIceTiebreaker(uint64_t tiebreaker) = 0;
  // TODO(pthatcher): Remove this once it's no longer called in
  // remoting/protocol/libjingle_transport_factory.cc
  virtual void SetIceProtocolType(IceProtocolType type) {}
  // TODO(honghaiz): Remove this once the call in chromoting is removed.
  virtual void SetIceCredentials(const std::string& ice_ufrag,
                                 const std::string& ice_pwd) {
    SetIceParameters(IceParameters(ice_ufrag, ice_pwd, false));
  }
  // TODO(honghaiz): Remove this once the call in chromoting is removed.
  virtual void SetRemoteIceCredentials(const std::string& ice_ufrag,
                                       const std::string& ice_pwd) {
    SetRemoteIceParameters(IceParameters(ice_ufrag, ice_pwd, false));
  }

  // SetIceParameters only needs to be implemented by the ICE transport
  // channels. Non-ICE transport channels should pass them down to the inner
  // ICE transport channel. The ufrag and pwd in |ice_params| must be set
  // before candidate gathering can start.
  virtual void SetIceParameters(const IceParameters& ice_params) = 0;
  // SetRemoteIceParameters only needs to be implemented by the ICE transport
  // channels. Non-ICE transport channels should pass them down to the inner
  // ICE transport channel.
  virtual void SetRemoteIceParameters(const IceParameters& ice_params) = 0;

  // SetRemoteIceMode must be implemented only by the ICE transport channels.
  virtual void SetRemoteIceMode(IceMode mode) = 0;

  virtual void SetIceConfig(const IceConfig& config) = 0;

  // Start gathering candidates if not already started, or if an ICE restart
  // occurred.
  virtual void MaybeStartGathering() = 0;

  virtual void SetMetricsObserver(
      webrtc::MetricsObserverInterface* observer) = 0;

  sigslot::signal1<TransportChannelImpl*> SignalGatheringState;

  // Handles sending and receiving of candidates.  The Transport
  // receives the candidates and may forward them to the relevant
  // channel.
  //
  // Note: Since candidates are delivered asynchronously to the
  // channel, they cannot return an error if the message is invalid.
  // It is assumed that the Transport will have checked validity
  // before forwarding.
  sigslot::signal2<TransportChannelImpl*, const Candidate&>
      SignalCandidateGathered;
  sigslot::signal2<TransportChannelImpl*, const Candidates&>
      SignalCandidatesRemoved;
  virtual void AddRemoteCandidate(const Candidate& candidate) = 0;
  virtual void RemoveRemoteCandidate(const Candidate& candidate) = 0;

  virtual IceGatheringState gathering_state() const = 0;

  // DTLS methods
  virtual bool SetLocalCertificate(
      const rtc::scoped_refptr<rtc::RTCCertificate>& certificate) = 0;

  // Set DTLS Remote fingerprint. Must be after local identity set.
  virtual bool SetRemoteFingerprint(const std::string& digest_alg,
                                    const uint8_t* digest,
                                    size_t digest_len) = 0;

  virtual bool SetSslRole(rtc::SSLRole role) = 0;

  // Invoked when there is conflict in the ICE role between local and remote
  // agents.
  sigslot::signal1<TransportChannelImpl*> SignalRoleConflict;

  // Emitted whenever the transport channel state changed.
  sigslot::signal1<TransportChannelImpl*> SignalStateChanged;

  // Emitted whenever the Dtls handshake failed on some transport channel.
  sigslot::signal1<rtc::SSLHandshakeError> SignalDtlsHandshakeError;

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(TransportChannelImpl);
};

}  // namespace cricket

#endif  // P2P_BASE_TRANSPORTCHANNELIMPL_H_
