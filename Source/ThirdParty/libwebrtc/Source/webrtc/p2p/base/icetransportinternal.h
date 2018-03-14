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
#include <vector>

#include "api/candidate.h"
#include "p2p/base/candidatepairinterface.h"
#include "p2p/base/packettransportinternal.h"
#include "p2p/base/port.h"
#include "p2p/base/transportdescription.h"
#include "rtc_base/stringencode.h"

namespace webrtc {
class MetricsObserverInterface;
}

namespace cricket {

typedef std::vector<Candidate> Candidates;

enum IceConnectionState {
  kIceConnectionConnecting = 0,
  kIceConnectionFailed,
  kIceConnectionConnected,  // Writable, but still checking one or more
                            // connections
  kIceConnectionCompleted,
};

// TODO(deadbeef): Unify with PeerConnectionInterface::IceConnectionState
// once /talk/ and /webrtc/ are combined, and also switch to ENUM_NAME naming
// style.
enum IceGatheringState {
  kIceGatheringNew = 0,
  kIceGatheringGathering,
  kIceGatheringComplete,
};

enum ContinualGatheringPolicy {
  // All port allocator sessions will stop after a writable connection is found.
  GATHER_ONCE = 0,
  // The most recent port allocator session will keep on running.
  GATHER_CONTINUALLY,
  // The most recent port allocator session will keep on running, and it will
  // try to recover connectivity if the channel becomes disconnected.
  GATHER_CONTINUALLY_AND_RECOVER,
};

// ICE Nomination mode.
enum class NominationMode {
  REGULAR,         // Nominate once per ICE restart (Not implemented yet).
  AGGRESSIVE,      // Nominate every connection except that it will behave as if
                   // REGULAR when the remote is an ICE-LITE endpoint.
  SEMI_AGGRESSIVE  // Our current implementation of the nomination algorithm.
                   // The details are described in P2PTransportChannel.
};

// Information about ICE configuration.
// TODO(deadbeef): Use rtc::Optional to represent unset values, instead of
// -1.
struct IceConfig {
  // The ICE connection receiving timeout value in milliseconds.
  int receiving_timeout = -1;
  // Time interval in milliseconds to ping a backup connection when the ICE
  // channel is strongly connected.
  int backup_connection_ping_interval = -1;

  ContinualGatheringPolicy continual_gathering_policy = GATHER_ONCE;

  bool gather_continually() const {
    return continual_gathering_policy == GATHER_CONTINUALLY ||
           continual_gathering_policy == GATHER_CONTINUALLY_AND_RECOVER;
  }

  // Whether we should prioritize Relay/Relay candidate when nothing
  // is writable yet.
  bool prioritize_most_likely_candidate_pairs = false;

  // Writable connections are pinged at a slower rate once stablized.
  int stable_writable_connection_ping_interval = -1;

  // If set to true, this means the ICE transport should presume TURN-to-TURN
  // candidate pairs will succeed, even before a binding response is received.
  bool presume_writable_when_fully_relayed = false;

  // Interval to check on all networks and to perform ICE regathering on any
  // active network having no connection on it.
  rtc::Optional<int> regather_on_failed_networks_interval;

  // Interval to perform ICE regathering on all networks
  // The delay in milliseconds is sampled from the uniform distribution [a, b]
  rtc::Optional<rtc::IntervalRange> regather_all_networks_interval_range;

  // The time period in which we will not switch the selected connection
  // when a new connection becomes receiving but the selected connection is not
  // in case that the selected connection may become receiving soon.
  rtc::Optional<int> receiving_switching_delay;

  // TODO(honghaiz): Change the default to regular nomination.
  // Default nomination mode if the remote does not support renomination.
  NominationMode default_nomination_mode = NominationMode::SEMI_AGGRESSIVE;

  // ICE checks (STUN pings) will not be sent at higher rate (lower interval)
  // than this, no matter what other settings there are.
  // Measure in milliseconds.
  rtc::Optional<int> ice_check_min_interval;

  IceConfig();
  IceConfig(int receiving_timeout_ms,
            int backup_connection_ping_interval,
            ContinualGatheringPolicy gathering_policy,
            bool prioritize_most_likely_candidate_pairs,
            int stable_writable_connection_ping_interval_ms,
            bool presume_writable_when_fully_relayed,
            int regather_on_failed_networks_interval_ms,
            int receiving_switching_delay_ms);
  ~IceConfig();
};

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
