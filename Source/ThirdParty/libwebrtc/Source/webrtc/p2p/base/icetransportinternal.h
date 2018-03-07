/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_ICETRANSPORTINTERNAL_H_
#define P2P_BASE_ICETRANSPORTINTERNAL_H_

#include <string>

#include "api/candidate.h"
#include "p2p/base/candidatepairinterface.h"
#include "p2p/base/jseptransport.h"
#include "p2p/base/packettransportinternal.h"
#include "p2p/base/transportdescription.h"
#include "rtc_base/stringencode.h"

namespace webrtc {
class MetricsObserverInterface;
}

namespace cricket {

// TODO(zhihuang): Replace this with
// PeerConnectionInterface::IceConnectionState.
enum class IceTransportState {
  STATE_INIT,
  STATE_CONNECTING,  // Will enter this state once a connection is created
  STATE_COMPLETED,
  STATE_FAILED
};

// TODO(zhihuang): Remove this once it's no longer used in
// remoting/protocol/libjingle_transport_factory.cc
enum IceProtocolType {
  ICEPROTO_RFC5245  // Standard RFC 5245 version of ICE.
};

// IceTransportInternal is an internal abstract class that does ICE.
// Once the public interface is supported,
// (https://www.w3.org/TR/webrtc/#rtcicetransport-interface)
// the IceTransportInterface will be split from this class.
class IceTransportInternal : public rtc::PacketTransportInternal {
 public:
  IceTransportInternal();
  ~IceTransportInternal() override;

  virtual IceTransportState GetState() const = 0;

  virtual int component() const = 0;

  virtual IceRole GetIceRole() const = 0;

  virtual void SetIceRole(IceRole role) = 0;

  virtual void SetIceTiebreaker(uint64_t tiebreaker) = 0;

  // TODO(zhihuang): Remove this once it's no longer called in
  // remoting/protocol/libjingle_transport_factory.cc
  virtual void SetIceProtocolType(IceProtocolType) {}

  virtual void SetIceCredentials(const std::string& ice_ufrag,
                                 const std::string& ice_pwd);

  virtual void SetRemoteIceCredentials(const std::string& ice_ufrag,
                                       const std::string& ice_pwd);

  // The ufrag and pwd in |ice_params| must be set
  // before candidate gathering can start.
  virtual void SetIceParameters(const IceParameters& ice_params) = 0;

  virtual void SetRemoteIceParameters(const IceParameters& ice_params) = 0;

  virtual void SetRemoteIceMode(IceMode mode) = 0;

  virtual void SetIceConfig(const IceConfig& config) = 0;

  // Start gathering candidates if not already started, or if an ICE restart
  // occurred.
  virtual void MaybeStartGathering() = 0;

  virtual void SetMetricsObserver(
      webrtc::MetricsObserverInterface* observer) = 0;

  virtual void AddRemoteCandidate(const Candidate& candidate) = 0;

  virtual void RemoveRemoteCandidate(const Candidate& candidate) = 0;

  virtual IceGatheringState gathering_state() const = 0;

  // Returns the current stats for this connection.
  virtual bool GetStats(ConnectionInfos* infos) = 0;

  // Returns RTT estimate over the currently active connection, or an empty
  // rtc::Optional if there is none.
  virtual rtc::Optional<int> GetRttEstimate() = 0;

  sigslot::signal1<IceTransportInternal*> SignalGatheringState;

  // Handles sending and receiving of candidates.
  sigslot::signal2<IceTransportInternal*, const Candidate&>
      SignalCandidateGathered;

  sigslot::signal2<IceTransportInternal*, const Candidates&>
      SignalCandidatesRemoved;

  // Deprecated by PacketTransportInternal::SignalNetworkRouteChanged.
  // This signal occurs when there is a change in the way that packets are
  // being routed, i.e. to a different remote location. The candidate
  // indicates where and how we are currently sending media.
  // TODO(zhihuang): Update the Chrome remoting to use the new
  // SignalNetworkRouteChanged.
  sigslot::signal2<IceTransportInternal*, const Candidate&> SignalRouteChange;

  // Invoked when there is conflict in the ICE role between local and remote
  // agents.
  sigslot::signal1<IceTransportInternal*> SignalRoleConflict;

  // Emitted whenever the transport state changed.
  sigslot::signal1<IceTransportInternal*> SignalStateChanged;

  // Invoked when the transport is being destroyed.
  sigslot::signal1<IceTransportInternal*> SignalDestroyed;
};

}  // namespace cricket

#endif  // P2P_BASE_ICETRANSPORTINTERNAL_H_
