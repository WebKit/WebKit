/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/p2ptransportchannel.h"

#include <algorithm>
#include <iterator>
#include <set>
#include <utility>

#include "api/candidate.h"
#include "logging/rtc_event_log/icelogger.h"
#include "p2p/base/candidatepairinterface.h"
#include "p2p/base/port.h"
#include "rtc_base/checks.h"
#include "rtc_base/crc32.h"
#include "rtc_base/logging.h"
#include "rtc_base/nethelper.h"
#include "rtc_base/nethelpers.h"
#include "rtc_base/stringencode.h"
#include "rtc_base/timeutils.h"
#include "system_wrappers/include/field_trial.h"
#include "system_wrappers/include/metrics.h"

namespace {

// The minimum improvement in RTT that justifies a switch.
const int kMinImprovement = 10;

bool IsRelayRelay(const cricket::Connection* conn) {
  return conn->local_candidate().type() == cricket::RELAY_PORT_TYPE &&
         conn->remote_candidate().type() == cricket::RELAY_PORT_TYPE;
}

bool IsUdp(cricket::Connection* conn) {
  return conn->local_candidate().relay_protocol() == cricket::UDP_PROTOCOL_NAME;
}

cricket::PortInterface::CandidateOrigin GetOrigin(cricket::PortInterface* port,
                                         cricket::PortInterface* origin_port) {
  if (!origin_port)
    return cricket::PortInterface::ORIGIN_MESSAGE;
  else if (port == origin_port)
    return cricket::PortInterface::ORIGIN_THIS_PORT;
  else
    return cricket::PortInterface::ORIGIN_OTHER_PORT;
}

// TODO(qingsi) Use an enum to replace the following constants for all
// comparision results.
static constexpr int a_is_better = 1;
static constexpr int b_is_better = -1;
static constexpr int a_and_b_equal = 0;

bool LocalCandidateUsesPreferredNetwork(
    const cricket::Connection* conn,
    absl::optional<rtc::AdapterType> network_preference) {
  rtc::AdapterType network_type = conn->port()->Network()->type();
  return network_preference.has_value() && (network_type == network_preference);
}

int CompareCandidatePairsByNetworkPreference(
    const cricket::Connection* a,
    const cricket::Connection* b,
    absl::optional<rtc::AdapterType> network_preference) {
  bool a_uses_preferred_network =
      LocalCandidateUsesPreferredNetwork(a, network_preference);
  bool b_uses_preferred_network =
      LocalCandidateUsesPreferredNetwork(b, network_preference);
  if (a_uses_preferred_network && !b_uses_preferred_network) {
    return a_is_better;
  } else if (!a_uses_preferred_network && b_uses_preferred_network) {
    return b_is_better;
  }
  return a_and_b_equal;
}

uint32_t GetWeakPingIntervalInFieldTrial() {
  uint32_t weak_ping_interval = ::strtoul(
      webrtc::field_trial::FindFullName("WebRTC-StunInterPacketDelay").c_str(),
      nullptr, 10);
  if (weak_ping_interval) {
    return static_cast<int>(weak_ping_interval);
  }
  return cricket::WEAK_PING_INTERVAL;
}

}  // unnamed namespace

namespace cricket {

using webrtc::RTCErrorType;
using webrtc::RTCError;

bool IceCredentialsChanged(const std::string& old_ufrag,
                           const std::string& old_pwd,
                           const std::string& new_ufrag,
                           const std::string& new_pwd) {
  // The standard (RFC 5245 Section 9.1.1.1) says that ICE restarts MUST change
  // both the ufrag and password. However, section 9.2.1.1 says changing the
  // ufrag OR password indicates an ICE restart. So, to keep compatibility with
  // endpoints that only change one, we'll treat this as an ICE restart.
  return (old_ufrag != new_ufrag) || (old_pwd != new_pwd);
}

P2PTransportChannel::P2PTransportChannel(const std::string& transport_name,
                                         int component,
                                         PortAllocator* allocator)
    : P2PTransportChannel(transport_name, component, allocator, nullptr) {}

P2PTransportChannel::P2PTransportChannel(
    const std::string& transport_name,
    int component,
    PortAllocator* allocator,
    webrtc::AsyncResolverFactory* async_resolver_factory,
    webrtc::RtcEventLog* event_log)
    : transport_name_(transport_name),
      component_(component),
      allocator_(allocator),
      async_resolver_factory_(async_resolver_factory),
      network_thread_(rtc::Thread::Current()),
      incoming_only_(false),
      error_(0),
      sort_dirty_(false),
      remote_ice_mode_(ICEMODE_FULL),
      ice_role_(ICEROLE_UNKNOWN),
      tiebreaker_(0),
      gathering_state_(kIceGatheringNew),
      config_(RECEIVING_TIMEOUT,
              BACKUP_CONNECTION_PING_INTERVAL,
              GATHER_ONCE /* continual_gathering_policy */,
              false /* prioritize_most_likely_candidate_pairs */,
              STRONG_AND_STABLE_WRITABLE_CONNECTION_PING_INTERVAL,
              true /* presume_writable_when_fully_relayed */,
              REGATHER_ON_FAILED_NETWORKS_INTERVAL,
              RECEIVING_SWITCHING_DELAY) {
  weak_ping_interval_ = GetWeakPingIntervalInFieldTrial();
  // Validate IceConfig even for mostly built-in constant default values in case
  // we change them.
  RTC_DCHECK(ValidateIceConfig(config_).ok());
  webrtc::BasicRegatheringController::Config regathering_config(
      config_.regather_all_networks_interval_range,
      config_.regather_on_failed_networks_interval_or_default());
  regathering_controller_.reset(new webrtc::BasicRegatheringController(
      regathering_config, this, network_thread_));
  ice_event_log_.set_event_log(event_log);
}

P2PTransportChannel::~P2PTransportChannel() {
  for (auto& p : resolvers_) {
    p.resolver_->Destroy(false);
  }
  resolvers_.clear();
  RTC_DCHECK(network_thread_ == rtc::Thread::Current());
}

// Add the allocator session to our list so that we know which sessions
// are still active.
void P2PTransportChannel::AddAllocatorSession(
    std::unique_ptr<PortAllocatorSession> session) {
  RTC_DCHECK(network_thread_ == rtc::Thread::Current());

  session->set_generation(static_cast<uint32_t>(allocator_sessions_.size()));
  session->SignalPortReady.connect(this, &P2PTransportChannel::OnPortReady);
  session->SignalPortsPruned.connect(this, &P2PTransportChannel::OnPortsPruned);
  session->SignalCandidatesReady.connect(
      this, &P2PTransportChannel::OnCandidatesReady);
  session->SignalCandidatesRemoved.connect(
      this, &P2PTransportChannel::OnCandidatesRemoved);
  session->SignalCandidatesAllocationDone.connect(
      this, &P2PTransportChannel::OnCandidatesAllocationDone);
  if (!allocator_sessions_.empty()) {
    allocator_session()->PruneAllPorts();
  }
  allocator_sessions_.push_back(std::move(session));
  regathering_controller_->set_allocator_session(allocator_session());

  // We now only want to apply new candidates that we receive to the ports
  // created by this new session because these are replacing those of the
  // previous sessions.
  PruneAllPorts();
}

void P2PTransportChannel::AddConnection(Connection* connection) {
  connections_.push_back(connection);
  unpinged_connections_.insert(connection);
  connection->set_remote_ice_mode(remote_ice_mode_);
  connection->set_receiving_timeout(config_.receiving_timeout);
  connection->set_unwritable_timeout(config_.ice_unwritable_timeout);
  connection->set_unwritable_min_checks(config_.ice_unwritable_min_checks);
  connection->SignalReadPacket.connect(
      this, &P2PTransportChannel::OnReadPacket);
  connection->SignalReadyToSend.connect(
      this, &P2PTransportChannel::OnReadyToSend);
  connection->SignalStateChange.connect(
      this, &P2PTransportChannel::OnConnectionStateChange);
  connection->SignalDestroyed.connect(
      this, &P2PTransportChannel::OnConnectionDestroyed);
  connection->SignalNominated.connect(this, &P2PTransportChannel::OnNominated);

  had_connection_ = true;

  connection->set_ice_event_log(&ice_event_log_);
  LogCandidatePairConfig(connection,
                         webrtc::IceCandidatePairConfigType::kAdded);
}

// Determines whether we should switch the selected connection to
// |new_connection| based the writable/receiving state, the nomination state,
// and the last data received time. This prevents the controlled side from
// switching the selected connection too frequently when the controlling side
// is doing aggressive nominations. The precedence of the connection switching
// criteria is as follows:
// i) write/receiving/connected states
// ii) For controlled side,
//        a) nomination state,
//        b) last data received time.
// iii) Lower cost / higher priority.
// iv) rtt.
// To further prevent switching to high-cost networks, does not switch to
// a high-cost connection if it is not receiving.
// TODO(honghaiz): Stop the aggressive nomination on the controlling side and
// implement the ice-renomination option.
bool P2PTransportChannel::ShouldSwitchSelectedConnection(
    Connection* new_connection,
    bool* missed_receiving_unchanged_threshold) const {
  if (!ReadyToSend(new_connection) || selected_connection_ == new_connection) {
    return false;
  }

  if (selected_connection_ == nullptr) {
    return true;
  }

  // Do not switch to a connection that is not receiving if it is not on a
  // preferred network or it has higher cost because it may be just spuriously
  // better.
  int compare_a_b_by_networks = CompareCandidatePairNetworks(
      new_connection, selected_connection_, config_.network_preference);
  if (compare_a_b_by_networks == b_is_better && !new_connection->receiving()) {
    return false;
  }

  absl::optional<int64_t> receiving_unchanged_threshold(
      rtc::TimeMillis() - config_.receiving_switching_delay_or_default());
  int cmp = CompareConnections(selected_connection_, new_connection,
                               receiving_unchanged_threshold,
                               missed_receiving_unchanged_threshold);
  if (cmp != 0) {
    return cmp < 0;
  }

  // If everything else is the same, switch only if rtt has improved by
  // a margin.
  return new_connection->rtt() <= selected_connection_->rtt() - kMinImprovement;
}

bool P2PTransportChannel::MaybeSwitchSelectedConnection(
    Connection* new_connection,
    const std::string& reason) {
  bool missed_receiving_unchanged_threshold = false;
  if (ShouldSwitchSelectedConnection(new_connection,
                                     &missed_receiving_unchanged_threshold)) {
    RTC_LOG(LS_INFO) << "Switching selected connection due to: " << reason;
    SwitchSelectedConnection(new_connection);
    return true;
  }
  if (missed_receiving_unchanged_threshold &&
      config_.receiving_switching_delay_or_default()) {
    // If we do not switch to the connection because it missed the receiving
    // threshold, the new connection is in a better receiving state than the
    // currently selected connection. So we need to re-check whether it needs
    // to be switched at a later time.
    const std::string reason_to_sort =
        reason + " (after switching dampening interval)";
    invoker_.AsyncInvokeDelayed<void>(
        RTC_FROM_HERE, thread(),
        rtc::Bind(&P2PTransportChannel::SortConnectionsAndUpdateState, this,
                  reason_to_sort),
        config_.receiving_switching_delay_or_default());
  }
  return false;
}

void P2PTransportChannel::SetIceRole(IceRole ice_role) {
  RTC_DCHECK(network_thread_ == rtc::Thread::Current());
  if (ice_role_ != ice_role) {
    ice_role_ = ice_role;
    for (PortInterface* port : ports_) {
      port->SetIceRole(ice_role);
    }
    // Update role on pruned ports as well, because they may still have
    // connections alive that should be using the correct role.
    for (PortInterface* port : pruned_ports_) {
      port->SetIceRole(ice_role);
    }
  }
}

IceRole P2PTransportChannel::GetIceRole() const {
  return ice_role_;
}

void P2PTransportChannel::SetIceTiebreaker(uint64_t tiebreaker) {
  RTC_DCHECK(network_thread_ == rtc::Thread::Current());
  if (!ports_.empty() || !pruned_ports_.empty()) {
    RTC_LOG(LS_ERROR)
        << "Attempt to change tiebreaker after Port has been allocated.";
    return;
  }

  tiebreaker_ = tiebreaker;
}

IceTransportState P2PTransportChannel::GetState() const {
  return state_;
}

webrtc::IceTransportState P2PTransportChannel::GetIceTransportState() const {
  return standardized_state_;
}

const std::string& P2PTransportChannel::transport_name() const {
  return transport_name_;
}

int P2PTransportChannel::component() const {
  return component_;
}

bool P2PTransportChannel::writable() const {
  return writable_;
}

bool P2PTransportChannel::receiving() const {
  return receiving_;
}

IceGatheringState P2PTransportChannel::gathering_state() const {
  return gathering_state_;
}

absl::optional<int> P2PTransportChannel::GetRttEstimate() {
  if (selected_connection_ != nullptr
      && selected_connection_->rtt_samples() > 0) {
    return selected_connection_->rtt();
  } else {
    return absl::nullopt;
  }
}

// A channel is considered ICE completed once there is at most one active
// connection per network and at least one active connection.
IceTransportState P2PTransportChannel::ComputeState() const {
  if (!had_connection_) {
    return IceTransportState::STATE_INIT;
  }

  std::vector<Connection*> active_connections;
  for (Connection* connection : connections_) {
    if (connection->active()) {
      active_connections.push_back(connection);
    }
  }
  if (active_connections.empty()) {
    return IceTransportState::STATE_FAILED;
  }

  std::set<rtc::Network*> networks;
  for (Connection* connection : active_connections) {
    rtc::Network* network = connection->port()->Network();
    if (networks.find(network) == networks.end()) {
      networks.insert(network);
    } else {
      RTC_LOG(LS_VERBOSE) << ToString()
                          << ": Ice not completed yet for this channel as "
                          << network->ToString()
                          << " has more than 1 connection.";
      return IceTransportState::STATE_CONNECTING;
    }
  }

  ice_event_log_.DumpCandidatePairDescriptionToMemoryAsConfigEvents();
  return IceTransportState::STATE_COMPLETED;
}

// Compute the current RTCIceTransportState as described in
// https://www.w3.org/TR/webrtc/#dom-rtcicetransportstate
// TODO(bugs.webrtc.org/9308): Return IceTransportState::kDisconnected when it
// makes sense.
// TODO(bugs.webrtc.org/9218): Avoid prematurely signalling kFailed once we have
// implemented end-of-candidates signalling.
webrtc::IceTransportState P2PTransportChannel::ComputeIceTransportState()
    const {
  bool has_connection = false;
  for (Connection* connection : connections_) {
    if (connection->active()) {
      has_connection = true;
      break;
    }
  }

  switch (gathering_state_) {
    case kIceGatheringComplete:
      if (has_connection)
        return webrtc::IceTransportState::kCompleted;
      else
        return webrtc::IceTransportState::kFailed;
    case kIceGatheringNew:
      return webrtc::IceTransportState::kNew;
    case kIceGatheringGathering:
      if (has_connection)
        return webrtc::IceTransportState::kConnected;
      else
        return webrtc::IceTransportState::kChecking;
    default:
      RTC_NOTREACHED();
      return webrtc::IceTransportState::kFailed;
  }
}

void P2PTransportChannel::SetIceParameters(const IceParameters& ice_params) {
  RTC_DCHECK(network_thread_ == rtc::Thread::Current());
  RTC_LOG(LS_INFO) << "Set ICE ufrag: " << ice_params.ufrag
                   << " pwd: " << ice_params.pwd << " on transport "
                   << transport_name();
  ice_parameters_ = ice_params;
  // Note: Candidate gathering will restart when MaybeStartGathering is next
  // called.
}

void P2PTransportChannel::SetRemoteIceParameters(
    const IceParameters& ice_params) {
  RTC_DCHECK(network_thread_ == rtc::Thread::Current());
  RTC_LOG(LS_INFO) << "Received remote ICE parameters: ufrag="
                   << ice_params.ufrag << ", renomination "
                   << (ice_params.renomination ? "enabled" : "disabled");
  IceParameters* current_ice = remote_ice();
  if (!current_ice || *current_ice != ice_params) {
    // Keep the ICE credentials so that newer connections
    // are prioritized over the older ones.
    remote_ice_parameters_.push_back(ice_params);
  }

  // Update the pwd of remote candidate if needed.
  for (RemoteCandidate& candidate : remote_candidates_) {
    if (candidate.username() == ice_params.ufrag &&
        candidate.password().empty()) {
      candidate.set_password(ice_params.pwd);
    }
  }
  // We need to update the credentials and generation for any peer reflexive
  // candidates.
  for (Connection* conn : connections_) {
    conn->MaybeSetRemoteIceParametersAndGeneration(
        ice_params, static_cast<int>(remote_ice_parameters_.size() - 1));
  }
  // Updating the remote ICE candidate generation could change the sort order.
  RequestSortAndStateUpdate("remote candidate generation maybe changed");
}

void P2PTransportChannel::SetRemoteIceMode(IceMode mode) {
  remote_ice_mode_ = mode;
}

// TODO(qingsi): We apply the convention that setting a absl::optional parameter
// to null restores its default value in the implementation. However, some
// absl::optional parameters are only processed below if non-null, e.g.,
// regather_on_failed_networks_interval, and thus there is no way to restore the
// defaults. Fix this issue later for consistency.
void P2PTransportChannel::SetIceConfig(const IceConfig& config) {
  if (config_.continual_gathering_policy != config.continual_gathering_policy) {
    if (!allocator_sessions_.empty()) {
      RTC_LOG(LS_ERROR) << "Trying to change continual gathering policy "
                           "when gathering has already started!";
    } else {
      config_.continual_gathering_policy = config.continual_gathering_policy;
      RTC_LOG(LS_INFO) << "Set continual_gathering_policy to "
                       << config_.continual_gathering_policy;
    }
  }

  if (config_.backup_connection_ping_interval !=
      config.backup_connection_ping_interval) {
    config_.backup_connection_ping_interval =
        config.backup_connection_ping_interval;
    RTC_LOG(LS_INFO) << "Set backup connection ping interval to "
                     << config_.backup_connection_ping_interval_or_default()
                     << " milliseconds.";
  }
  if (config_.receiving_timeout != config.receiving_timeout) {
    config_.receiving_timeout = config.receiving_timeout;
    for (Connection* connection : connections_) {
      connection->set_receiving_timeout(config_.receiving_timeout);
    }
    RTC_LOG(LS_INFO) << "Set ICE receiving timeout to "
                     << config_.receiving_timeout_or_default()
                     << " milliseconds";
  }

  config_.prioritize_most_likely_candidate_pairs =
      config.prioritize_most_likely_candidate_pairs;
  RTC_LOG(LS_INFO) << "Set ping most likely connection to "
                   << config_.prioritize_most_likely_candidate_pairs;

  if (config_.stable_writable_connection_ping_interval !=
      config.stable_writable_connection_ping_interval) {
    config_.stable_writable_connection_ping_interval =
        config.stable_writable_connection_ping_interval;
    RTC_LOG(LS_INFO)
        << "Set stable_writable_connection_ping_interval to "
        << config_.stable_writable_connection_ping_interval_or_default();
  }

  if (config_.presume_writable_when_fully_relayed !=
      config.presume_writable_when_fully_relayed) {
    if (!connections_.empty()) {
      RTC_LOG(LS_ERROR) << "Trying to change 'presume writable' "
                           "while connections already exist!";
    } else {
      config_.presume_writable_when_fully_relayed =
          config.presume_writable_when_fully_relayed;
      RTC_LOG(LS_INFO) << "Set presume writable when fully relayed to "
                       << config_.presume_writable_when_fully_relayed;
    }
  }

  if (config_.regather_on_failed_networks_interval !=
      config.regather_on_failed_networks_interval) {
    config_.regather_on_failed_networks_interval =
        config.regather_on_failed_networks_interval;
    RTC_LOG(LS_INFO)
        << "Set regather_on_failed_networks_interval to "
        << config_.regather_on_failed_networks_interval_or_default();
  }

  if (config_.regather_all_networks_interval_range !=
      config.regather_all_networks_interval_range) {
    // Config validation is assumed to have already happened at the API layer.
    RTC_DCHECK(config.continual_gathering_policy != GATHER_ONCE);
    config_.regather_all_networks_interval_range =
        config.regather_all_networks_interval_range;
    RTC_LOG(LS_INFO) << "Set regather_all_networks_interval_range to "
                     << config.regather_all_networks_interval_range
                            .value_or(rtc::IntervalRange(-1, 0))
                            .ToString();
  }

  if (config_.receiving_switching_delay != config.receiving_switching_delay) {
    config_.receiving_switching_delay = config.receiving_switching_delay;
    RTC_LOG(LS_INFO) << "Set receiving_switching_delay to "
                     << config_.receiving_switching_delay_or_default();
  }

  if (config_.default_nomination_mode != config.default_nomination_mode) {
    config_.default_nomination_mode = config.default_nomination_mode;
    RTC_LOG(LS_INFO) << "Set default nomination mode to "
                     << static_cast<int>(config_.default_nomination_mode);
  }

  if (config_.ice_check_interval_strong_connectivity !=
      config.ice_check_interval_strong_connectivity) {
    config_.ice_check_interval_strong_connectivity =
        config.ice_check_interval_strong_connectivity;
    RTC_LOG(LS_INFO)
        << "Set strong ping interval to "
        << config_.ice_check_interval_strong_connectivity_or_default();
  }

  if (config_.ice_check_interval_weak_connectivity !=
      config.ice_check_interval_weak_connectivity) {
    config_.ice_check_interval_weak_connectivity =
        config.ice_check_interval_weak_connectivity;
    RTC_LOG(LS_INFO)
        << "Set weak ping interval to "
        << config_.ice_check_interval_weak_connectivity_or_default();
  }

  if (config_.ice_check_min_interval != config.ice_check_min_interval) {
    config_.ice_check_min_interval = config.ice_check_min_interval;
    RTC_LOG(LS_INFO) << "Set min ping interval to "
                     << config_.ice_check_min_interval_or_default();
  }

  if (config_.ice_unwritable_timeout != config.ice_unwritable_timeout) {
    config_.ice_unwritable_timeout = config.ice_unwritable_timeout;
    for (Connection* conn : connections_) {
      conn->set_unwritable_timeout(config_.ice_unwritable_timeout);
    }
    RTC_LOG(LS_INFO) << "Set unwritable timeout to "
                     << config_.ice_unwritable_timeout_or_default();
  }

  if (config_.ice_unwritable_min_checks != config.ice_unwritable_min_checks) {
    config_.ice_unwritable_min_checks = config.ice_unwritable_min_checks;
    for (Connection* conn : connections_) {
      conn->set_unwritable_min_checks(config_.ice_unwritable_min_checks);
    }
    RTC_LOG(LS_INFO) << "Set unwritable min checks to "
                     << config_.ice_unwritable_min_checks_or_default();
  }

  if (config_.network_preference != config.network_preference) {
    config_.network_preference = config.network_preference;
    RequestSortAndStateUpdate("network preference changed");
    RTC_LOG(LS_INFO) << "Set network preference to "
                     << (config_.network_preference.has_value()
                             ? config_.network_preference.value()
                             : -1);  // network_preference cannot be bound to
                                     // int with value_or.
  }

  // TODO(qingsi): Resolve the naming conflict of stun_keepalive_delay in
  // UDPPort and stun_keepalive_interval.
  if (config_.stun_keepalive_interval != config.stun_keepalive_interval) {
    config_.stun_keepalive_interval = config.stun_keepalive_interval;
    allocator_session()->SetStunKeepaliveIntervalForReadyPorts(
        config_.stun_keepalive_interval);
    RTC_LOG(LS_INFO) << "Set STUN keepalive interval to "
                     << config.stun_keepalive_interval_or_default();
  }

  webrtc::BasicRegatheringController::Config regathering_config(
      config_.regather_all_networks_interval_range,
      config_.regather_on_failed_networks_interval_or_default());
  regathering_controller_->SetConfig(regathering_config);

  RTC_DCHECK(ValidateIceConfig(config_).ok());
}

const IceConfig& P2PTransportChannel::config() const {
  return config_;
}

// TODO(qingsi): Add tests for the config validation starting from
// PeerConnection::SetConfiguration.
RTCError P2PTransportChannel::ValidateIceConfig(const IceConfig& config) {
  if (config.regather_all_networks_interval_range &&
      config.continual_gathering_policy == GATHER_ONCE) {
    return RTCError(RTCErrorType::INVALID_PARAMETER,
                    "regather_all_networks_interval_range specified but "
                    "continual gathering policy is GATHER_ONCE");
  }

  if (config.ice_check_interval_strong_connectivity_or_default() <
      config.ice_check_interval_weak_connectivity.value_or(
          GetWeakPingIntervalInFieldTrial())) {
    return RTCError(RTCErrorType::INVALID_PARAMETER,
                    "Ping interval of candidate pairs is shorter when ICE is "
                    "strongly connected than that when ICE is weakly "
                    "connected");
  }

  if (config.receiving_timeout_or_default() <
      std::max(config.ice_check_interval_strong_connectivity_or_default(),
               config.ice_check_min_interval_or_default())) {
    return RTCError(
        RTCErrorType::INVALID_PARAMETER,
        "Receiving timeout is shorter than the minimal ping interval.");
  }

  if (config.backup_connection_ping_interval_or_default() <
      config.ice_check_interval_strong_connectivity_or_default()) {
    return RTCError(RTCErrorType::INVALID_PARAMETER,
                    "Ping interval of backup candidate pairs is shorter than "
                    "that of general candidate pairs when ICE is strongly "
                    "connected");
  }

  if (config.stable_writable_connection_ping_interval_or_default() <
      config.ice_check_interval_strong_connectivity_or_default()) {
    return RTCError(RTCErrorType::INVALID_PARAMETER,
                    "Ping interval of stable and writable candidate pairs is "
                    "shorter than that of general candidate pairs when ICE is "
                    "strongly connected");
  }

  if (config.ice_unwritable_timeout_or_default() > CONNECTION_WRITE_TIMEOUT) {
    return RTCError(RTCErrorType::INVALID_PARAMETER,
                    "The timeout period for the writability state to become "
                    "UNRELIABLE is longer than that to become TIMEOUT.");
  }

  if (config.regather_all_networks_interval_range &&
      config.regather_all_networks_interval_range.value().min() < 0) {
    return RTCError(
        RTCErrorType::INVALID_RANGE,
        "The minimum regathering interval for all networks is negative.");
  }

  return RTCError::OK();
}

int P2PTransportChannel::check_receiving_interval() const {
  return std::max(MIN_CHECK_RECEIVING_INTERVAL,
                  config_.receiving_timeout_or_default() / 10);
}

void P2PTransportChannel::MaybeStartGathering() {
  if (ice_parameters_.ufrag.empty() || ice_parameters_.pwd.empty()) {
    RTC_LOG(LS_ERROR)
        << "Cannot gather candidates because ICE parameters are empty"
           " ufrag: " << ice_parameters_.ufrag
        << " pwd: " << ice_parameters_.pwd;
    return;
  }
  // Start gathering if we never started before, or if an ICE restart occurred.
  if (allocator_sessions_.empty() ||
      IceCredentialsChanged(allocator_sessions_.back()->ice_ufrag(),
                            allocator_sessions_.back()->ice_pwd(),
                            ice_parameters_.ufrag, ice_parameters_.pwd)) {
    if (gathering_state_ != kIceGatheringGathering) {
      gathering_state_ = kIceGatheringGathering;
      SignalGatheringState(this);
    }

    if (!allocator_sessions_.empty()) {
      IceRestartState state;
      if (writable()) {
        state = IceRestartState::CONNECTED;
      } else if (IsGettingPorts()) {
        state = IceRestartState::CONNECTING;
      } else {
        state = IceRestartState::DISCONNECTED;
      }
      RTC_HISTOGRAM_ENUMERATION("WebRTC.PeerConnection.IceRestartState",
                                static_cast<int>(state),
                                static_cast<int>(IceRestartState::MAX_VALUE));
    }

    // Time for a new allocator.
    std::unique_ptr<PortAllocatorSession> pooled_session =
        allocator_->TakePooledSession(transport_name(), component(),
                                      ice_parameters_.ufrag,
                                      ice_parameters_.pwd);
    if (pooled_session) {
      AddAllocatorSession(std::move(pooled_session));
      PortAllocatorSession* raw_pooled_session =
          allocator_sessions_.back().get();
      // Process the pooled session's existing candidates/ports, if they exist.
      OnCandidatesReady(raw_pooled_session,
                        raw_pooled_session->ReadyCandidates());
      for (PortInterface* port : allocator_sessions_.back()->ReadyPorts()) {
        OnPortReady(raw_pooled_session, port);
      }
      if (allocator_sessions_.back()->CandidatesAllocationDone()) {
        OnCandidatesAllocationDone(raw_pooled_session);
      }
    } else {
      AddAllocatorSession(allocator_->CreateSession(
          transport_name(), component(), ice_parameters_.ufrag,
          ice_parameters_.pwd));
      allocator_sessions_.back()->StartGettingPorts();
    }
  }
}

// A new port is available, attempt to make connections for it
void P2PTransportChannel::OnPortReady(PortAllocatorSession *session,
                                      PortInterface* port) {
  RTC_DCHECK(network_thread_ == rtc::Thread::Current());

  // Set in-effect options on the new port
  for (OptionMap::const_iterator it = options_.begin();
       it != options_.end();
       ++it) {
    int val = port->SetOption(it->first, it->second);
    if (val < 0) {
      // Errors are frequent, so use LS_INFO. bugs.webrtc.org/9221
      RTC_LOG(LS_INFO) << port->ToString() << ": SetOption(" << it->first
                       << ", " << it->second
                       << ") failed: " << port->GetError();
    }
  }

  // Remember the ports and candidates, and signal that candidates are ready.
  // The session will handle this, and send an initiate/accept/modify message
  // if one is pending.

  port->SetIceRole(ice_role_);
  port->SetIceTiebreaker(tiebreaker_);
  ports_.push_back(port);
  port->SignalUnknownAddress.connect(
      this, &P2PTransportChannel::OnUnknownAddress);
  port->SignalDestroyed.connect(this, &P2PTransportChannel::OnPortDestroyed);

  port->SignalRoleConflict.connect(
      this, &P2PTransportChannel::OnRoleConflict);
  port->SignalSentPacket.connect(this, &P2PTransportChannel::OnSentPacket);

  // Attempt to create a connection from this new port to all of the remote
  // candidates that we were given so far.

  std::vector<RemoteCandidate>::iterator iter;
  for (iter = remote_candidates_.begin(); iter != remote_candidates_.end();
       ++iter) {
    CreateConnection(port, *iter, iter->origin_port());
  }

  SortConnectionsAndUpdateState(
      "new candidate pairs created from a new local candidate");
}

// A new candidate is available, let listeners know
void P2PTransportChannel::OnCandidatesReady(
    PortAllocatorSession* session,
    const std::vector<Candidate>& candidates) {
  RTC_DCHECK(network_thread_ == rtc::Thread::Current());
  for (size_t i = 0; i < candidates.size(); ++i) {
    SignalCandidateGathered(this, candidates[i]);
  }
}

void P2PTransportChannel::OnCandidatesAllocationDone(
    PortAllocatorSession* session) {
  RTC_DCHECK(network_thread_ == rtc::Thread::Current());
  if (config_.gather_continually()) {
    RTC_LOG(LS_INFO) << "P2PTransportChannel: " << transport_name()
                     << ", component " << component()
                     << " gathering complete, but using continual "
                        "gathering so not changing gathering state.";
    return;
  }
  gathering_state_ = kIceGatheringComplete;
  RTC_LOG(LS_INFO) << "P2PTransportChannel: " << transport_name()
                   << ", component " << component() << " gathering complete";
  SignalGatheringState(this);
}

// Handle stun packets
void P2PTransportChannel::OnUnknownAddress(
    PortInterface* port,
    const rtc::SocketAddress& address, ProtocolType proto,
    IceMessage* stun_msg, const std::string &remote_username,
    bool port_muxed) {
  RTC_DCHECK(network_thread_ == rtc::Thread::Current());

  // Port has received a valid stun packet from an address that no Connection
  // is currently available for. See if we already have a candidate with the
  // address. If it isn't we need to create new candidate for it.
  //
  // TODO(qingsi): There is a caveat of the logic below if we have remote
  // candidates with hostnames. We could create a prflx candidate that is
  // identical to a host candidate that are currently in the process of name
  // resolution. We would not have a duplicate candidate since when adding the
  // resolved host candidate, FinishingAddingRemoteCandidate does
  // MaybeUpdatePeerReflexiveCandidate, and the prflx candidate would be updated
  // to a host candidate. As a result, for a brief moment we would have a prflx
  // candidate showing a private IP address, though we do not signal prflx
  // candidates to applications and we could obfuscate the IP addresses of prflx
  // candidates in P2PTransportChannel::GetStats. The difficulty of preventing
  // creating the prflx from the beginning is that we do not have a reliable way
  // to claim two candidates are identical without the address information. If
  // we always pause the addition of a prflx candidate when there is ongoing
  // name resolution and dedup after we have a resolved address, we run into the
  // risk of losing/delaying the addition of a non-identical candidate that
  // could be the only way to have a connection, if the resolution never
  // completes or is significantly delayed.
  const Candidate* candidate = nullptr;
  for (const Candidate& c : remote_candidates_) {
    if (c.username() == remote_username && c.address() == address &&
        c.protocol() == ProtoToString(proto)) {
      candidate = &c;
      break;
    }
  }

  uint32_t remote_generation = 0;
  std::string remote_password;
  // The STUN binding request may arrive after setRemoteDescription and before
  // adding remote candidate, so we need to set the password to the shared
  // password and set the generation if the user name matches.
  const IceParameters* ice_param =
      FindRemoteIceFromUfrag(remote_username, &remote_generation);
  // Note: if not found, the remote_generation will still be 0.
  if (ice_param != nullptr) {
    remote_password = ice_param->pwd;
  }

  Candidate remote_candidate;
  bool remote_candidate_is_new = (candidate == nullptr);
  if (!remote_candidate_is_new) {
    remote_candidate = *candidate;
  } else {
    // Create a new candidate with this address.
    // The priority of the candidate is set to the PRIORITY attribute
    // from the request.
    const StunUInt32Attribute* priority_attr =
        stun_msg->GetUInt32(STUN_ATTR_PRIORITY);
    if (!priority_attr) {
      RTC_LOG(LS_WARNING) << "P2PTransportChannel::OnUnknownAddress - "
                             "No STUN_ATTR_PRIORITY found in the "
                             "stun request message";
      port->SendBindingErrorResponse(stun_msg, address, STUN_ERROR_BAD_REQUEST,
                                     STUN_ERROR_REASON_BAD_REQUEST);
      return;
    }
    int remote_candidate_priority = priority_attr->value();

    uint16_t network_id = 0;
    uint16_t network_cost = 0;
    const StunUInt32Attribute* network_attr =
        stun_msg->GetUInt32(STUN_ATTR_NETWORK_INFO);
    if (network_attr) {
      uint32_t network_info = network_attr->value();
      network_id = static_cast<uint16_t>(network_info >> 16);
      network_cost = static_cast<uint16_t>(network_info);
    }

    // RFC 5245
    // If the source transport address of the request does not match any
    // existing remote candidates, it represents a new peer reflexive remote
    // candidate.
    remote_candidate = Candidate(
        component(), ProtoToString(proto), address, remote_candidate_priority,
        remote_username, remote_password, PRFLX_PORT_TYPE, remote_generation,
        "", network_id, network_cost);

    // From RFC 5245, section-7.2.1.3:
    // The foundation of the candidate is set to an arbitrary value, different
    // from the foundation for all other remote candidates.
    remote_candidate.set_foundation(
        rtc::ToString(rtc::ComputeCrc32(remote_candidate.id())));
  }

  // RFC5245, the agent constructs a pair whose local candidate is equal to
  // the transport address on which the STUN request was received, and a
  // remote candidate equal to the source transport address where the
  // request came from.

  // There shouldn't be an existing connection with this remote address.
  // When ports are muxed, this channel might get multiple unknown address
  // signals. In that case if the connection is already exists, we should
  // simply ignore the signal otherwise send server error.
  if (port->GetConnection(remote_candidate.address())) {
    if (port_muxed) {
      RTC_LOG(LS_INFO) << "Connection already exists for peer reflexive "
                          "candidate: " << remote_candidate.ToString();
      return;
    } else {
      RTC_NOTREACHED();
      port->SendBindingErrorResponse(stun_msg, address,
                                     STUN_ERROR_SERVER_ERROR,
                                     STUN_ERROR_REASON_SERVER_ERROR);
      return;
    }
  }

  Connection* connection =
      port->CreateConnection(remote_candidate, PortInterface::ORIGIN_THIS_PORT);
  if (!connection) {
    // This could happen in some scenarios. For example, a TurnPort may have
    // had a refresh request timeout, so it won't create connections.
    port->SendBindingErrorResponse(stun_msg, address, STUN_ERROR_SERVER_ERROR,
                                   STUN_ERROR_REASON_SERVER_ERROR);
    return;
  }

  RTC_LOG(LS_INFO) << "Adding connection from "
                   << (remote_candidate_is_new ? "peer reflexive"
                                               : "resurrected")
                   << " candidate: " << remote_candidate.ToString();
  AddConnection(connection);
  connection->HandleBindingRequest(stun_msg);

  // Update the list of connections since we just added another.  We do this
  // after sending the response since it could (in principle) delete the
  // connection in question.
  SortConnectionsAndUpdateState(
      "a new candidate pair created from an unknown remote address");
}

void P2PTransportChannel::OnRoleConflict(PortInterface* port) {
  SignalRoleConflict(this);  // STUN ping will be sent when SetRole is called
                             // from Transport.
}

const IceParameters* P2PTransportChannel::FindRemoteIceFromUfrag(
    const std::string& ufrag,
    uint32_t* generation) {
  const auto& params = remote_ice_parameters_;
  auto it = std::find_if(
      params.rbegin(), params.rend(),
      [ufrag](const IceParameters& param) { return param.ufrag == ufrag; });
  if (it == params.rend()) {
    // Not found.
    return nullptr;
  }
  *generation = params.rend() - it - 1;
  return &(*it);
}

void P2PTransportChannel::OnNominated(Connection* conn) {
  RTC_DCHECK(network_thread_ == rtc::Thread::Current());
  RTC_DCHECK(ice_role_ == ICEROLE_CONTROLLED);

  if (selected_connection_ == conn) {
    return;
  }

  // TODO(qingsi): RequestSortAndStateUpdate will eventually call
  // MaybeSwitchSelectedConnection again. Rewrite this logic.
  if (MaybeSwitchSelectedConnection(conn,
                                    "nomination on the controlled side")) {
    // Now that we have selected a connection, it is time to prune other
    // connections and update the read/write state of the channel.
    RequestSortAndStateUpdate("nomination on the controlled side");
  } else {
    RTC_LOG(LS_INFO)
        << "Not switching the selected connection on controlled side yet: "
        << conn->ToString();
  }
}

void P2PTransportChannel::ResolveHostnameCandidate(const Candidate& candidate) {
  if (!async_resolver_factory_) {
    RTC_LOG(LS_WARNING) << "Dropping ICE candidate with hostname address "
                        << "(no AsyncResolverFactory)";
    return;
  }

  rtc::AsyncResolverInterface* resolver = async_resolver_factory_->Create();
  resolvers_.emplace_back(candidate, resolver);
  resolver->SignalDone.connect(this, &P2PTransportChannel::OnCandidateResolved);
  resolver->Start(candidate.address());
  RTC_LOG(LS_INFO) << "Asynchronously resolving ICE candidate hostname "
                   << candidate.address().HostAsSensitiveURIString();
}

void P2PTransportChannel::AddRemoteCandidate(const Candidate& candidate) {
  RTC_DCHECK(network_thread_ == rtc::Thread::Current());

  uint32_t generation = GetRemoteCandidateGeneration(candidate);
  // If a remote candidate with a previous generation arrives, drop it.
  if (generation < remote_ice_generation()) {
    RTC_LOG(LS_WARNING) << "Dropping a remote candidate because its ufrag "
                        << candidate.username()
                        << " indicates it was for a previous generation.";
    return;
  }

  Candidate new_remote_candidate(candidate);
  new_remote_candidate.set_generation(generation);
  // ICE candidates don't need to have username and password set, but
  // the code below this (specifically, ConnectionRequest::Prepare in
  // port.cc) uses the remote candidates's username.  So, we set it
  // here.
  if (remote_ice()) {
    if (candidate.username().empty()) {
      new_remote_candidate.set_username(remote_ice()->ufrag);
    }
    if (new_remote_candidate.username() == remote_ice()->ufrag) {
      if (candidate.password().empty()) {
        new_remote_candidate.set_password(remote_ice()->pwd);
      }
    } else {
      // The candidate belongs to the next generation. Its pwd will be set
      // when the new remote ICE credentials arrive.
      RTC_LOG(LS_WARNING)
          << "A remote candidate arrives with an unknown ufrag: "
          << candidate.username();
    }
  }

  if (new_remote_candidate.address().IsUnresolvedIP()) {
    ResolveHostnameCandidate(new_remote_candidate);
    return;
  }

  FinishAddingRemoteCandidate(new_remote_candidate);
}

P2PTransportChannel::CandidateAndResolver::CandidateAndResolver(
    const Candidate& candidate,
    rtc::AsyncResolverInterface* resolver)
    : candidate_(candidate), resolver_(resolver) {}

P2PTransportChannel::CandidateAndResolver::~CandidateAndResolver() {}

void P2PTransportChannel::OnCandidateResolved(
    rtc::AsyncResolverInterface* resolver) {
  auto p = std::find_if(resolvers_.begin(), resolvers_.end(),
                        [resolver](const CandidateAndResolver& cr) {
                          return cr.resolver_ == resolver;
                        });
  if (p == resolvers_.end()) {
    RTC_LOG(LS_ERROR) << "Unexpected AsyncResolver signal";
    RTC_NOTREACHED();
    return;
  }
  Candidate candidate = p->candidate_;
  resolvers_.erase(p);
  AddRemoteCandidateWithResolver(candidate, resolver);
  invoker_.AsyncInvoke<void>(
      RTC_FROM_HERE, thread(),
      rtc::Bind(&rtc::AsyncResolverInterface::Destroy, resolver, false));
}

void P2PTransportChannel::AddRemoteCandidateWithResolver(
    Candidate candidate,
    rtc::AsyncResolverInterface* resolver) {
  if (resolver->GetError()) {
    RTC_LOG(LS_WARNING) << "Failed to resolve ICE candidate hostname "
                        << candidate.address().HostAsSensitiveURIString()
                        << " with error " << resolver->GetError();
    return;
  }

  rtc::SocketAddress resolved_address;
  // Prefer IPv6 to IPv4 if we have it (see RFC 5245 Section 15.1).
  // TODO(zstein): This won't work if we only have IPv4 locally but receive an
  // AAAA DNS record.
  bool have_address =
      resolver->GetResolvedAddress(AF_INET6, &resolved_address) ||
      resolver->GetResolvedAddress(AF_INET, &resolved_address);
  if (!have_address) {
    RTC_LOG(LS_INFO) << "ICE candidate hostname "
                     << candidate.address().HostAsSensitiveURIString()
                     << " could not be resolved";
    return;
  }

  RTC_LOG(LS_INFO) << "Resolved ICE candidate hostname "
                   << candidate.address().HostAsSensitiveURIString() << " to "
                   << resolved_address.ipaddr().ToSensitiveString();
  candidate.set_address(resolved_address);
  FinishAddingRemoteCandidate(candidate);
}

void P2PTransportChannel::FinishAddingRemoteCandidate(
    const Candidate& new_remote_candidate) {
  // If this candidate matches what was thought to be a peer reflexive
  // candidate, we need to update the candidate priority/etc.
  for (Connection* conn : connections_) {
    conn->MaybeUpdatePeerReflexiveCandidate(new_remote_candidate);
  }

  // Create connections to this remote candidate.
  CreateConnections(new_remote_candidate, NULL);

  // Resort the connections list, which may have new elements.
  SortConnectionsAndUpdateState(
      "new candidate pairs created from a new remote candidate");
}

void P2PTransportChannel::RemoveRemoteCandidate(
    const Candidate& cand_to_remove) {
  auto iter =
      std::remove_if(remote_candidates_.begin(), remote_candidates_.end(),
                     [cand_to_remove](const Candidate& candidate) {
                       return cand_to_remove.MatchesForRemoval(candidate);
                     });
  if (iter != remote_candidates_.end()) {
    RTC_LOG(LS_VERBOSE) << "Removed remote candidate "
                        << cand_to_remove.ToString();
    remote_candidates_.erase(iter, remote_candidates_.end());
  }
}

// Creates connections from all of the ports that we care about to the given
// remote candidate.  The return value is true if we created a connection from
// the origin port.
bool P2PTransportChannel::CreateConnections(const Candidate& remote_candidate,
                                            PortInterface* origin_port) {
  RTC_DCHECK(network_thread_ == rtc::Thread::Current());

  // If we've already seen the new remote candidate (in the current candidate
  // generation), then we shouldn't try creating connections for it.
  // We either already have a connection for it, or we previously created one
  // and then later pruned it. If we don't return, the channel will again
  // re-create any connections that were previously pruned, which will then
  // immediately be re-pruned, churning the network for no purpose.
  // This only applies to candidates received over signaling (i.e. origin_port
  // is NULL).
  if (!origin_port && IsDuplicateRemoteCandidate(remote_candidate)) {
    // return true to indicate success, without creating any new connections.
    return true;
  }

  // Add a new connection for this candidate to every port that allows such a
  // connection (i.e., if they have compatible protocols) and that does not
  // already have a connection to an equivalent candidate.  We must be careful
  // to make sure that the origin port is included, even if it was pruned,
  // since that may be the only port that can create this connection.
  bool created = false;
  std::vector<PortInterface *>::reverse_iterator it;
  for (it = ports_.rbegin(); it != ports_.rend(); ++it) {
    if (CreateConnection(*it, remote_candidate, origin_port)) {
      if (*it == origin_port)
        created = true;
    }
  }

  if ((origin_port != NULL) &&
      std::find(ports_.begin(), ports_.end(), origin_port) == ports_.end()) {
    if (CreateConnection(origin_port, remote_candidate, origin_port))
      created = true;
  }

  // Remember this remote candidate so that we can add it to future ports.
  RememberRemoteCandidate(remote_candidate, origin_port);

  return created;
}

// Setup a connection object for the local and remote candidate combination.
// And then listen to connection object for changes.
bool P2PTransportChannel::CreateConnection(PortInterface* port,
                                           const Candidate& remote_candidate,
                                           PortInterface* origin_port) {
  if (!port->SupportsProtocol(remote_candidate.protocol())) {
    return false;
  }
  // Look for an existing connection with this remote address.  If one is not
  // found or it is found but the existing remote candidate has an older
  // generation, then we can create a new connection for this address.
  Connection* connection = port->GetConnection(remote_candidate.address());
  if (connection == nullptr ||
      connection->remote_candidate().generation() <
          remote_candidate.generation()) {
    // Don't create a connection if this is a candidate we received in a
    // message and we are not allowed to make outgoing connections.
    PortInterface::CandidateOrigin origin = GetOrigin(port, origin_port);
    if (origin == PortInterface::ORIGIN_MESSAGE && incoming_only_) {
      return false;
    }
    Connection* connection = port->CreateConnection(remote_candidate, origin);
    if (!connection) {
      return false;
    }
    AddConnection(connection);
    RTC_LOG(LS_INFO) << ToString()
                     << ": Created connection with origin: " << origin
                     << ", total: " << connections_.size();
    return true;
  }

  // No new connection was created.
  // It is not legal to try to change any of the parameters of an existing
  // connection; however, the other side can send a duplicate candidate.
  if (!remote_candidate.IsEquivalent(connection->remote_candidate())) {
    RTC_LOG(INFO) << "Attempt to change a remote candidate."
                     " Existing remote candidate: "
                  << connection->remote_candidate().ToString()
                  << "New remote candidate: " << remote_candidate.ToString();
  }
  return false;
}

bool P2PTransportChannel::FindConnection(Connection* connection) const {
  std::vector<Connection*>::const_iterator citer =
      std::find(connections_.begin(), connections_.end(), connection);
  return citer != connections_.end();
}

uint32_t P2PTransportChannel::GetRemoteCandidateGeneration(
    const Candidate& candidate) {
  // If the candidate has a ufrag, use it to find the generation.
  if (!candidate.username().empty()) {
    uint32_t generation = 0;
    if (!FindRemoteIceFromUfrag(candidate.username(), &generation)) {
      // If the ufrag is not found, assume the next/future generation.
      generation = static_cast<uint32_t>(remote_ice_parameters_.size());
    }
    return generation;
  }
  // If candidate generation is set, use that.
  if (candidate.generation() > 0) {
    return candidate.generation();
  }
  // Otherwise, assume the generation from remote ice parameters.
  return remote_ice_generation();
}

// Check if remote candidate is already cached.
bool P2PTransportChannel::IsDuplicateRemoteCandidate(
    const Candidate& candidate) {
  for (size_t i = 0; i < remote_candidates_.size(); ++i) {
    if (remote_candidates_[i].IsEquivalent(candidate)) {
      return true;
    }
  }
  return false;
}

// Maintain our remote candidate list, adding this new remote one.
void P2PTransportChannel::RememberRemoteCandidate(
    const Candidate& remote_candidate, PortInterface* origin_port) {
  // Remove any candidates whose generation is older than this one.  The
  // presence of a new generation indicates that the old ones are not useful.
  size_t i = 0;
  while (i < remote_candidates_.size()) {
    if (remote_candidates_[i].generation() < remote_candidate.generation()) {
      RTC_LOG(INFO) << "Pruning candidate from old generation: "
                    << remote_candidates_[i].address().ToSensitiveString();
      remote_candidates_.erase(remote_candidates_.begin() + i);
    } else {
      i += 1;
    }
  }

  // Make sure this candidate is not a duplicate.
  if (IsDuplicateRemoteCandidate(remote_candidate)) {
    RTC_LOG(INFO) << "Duplicate candidate: " << remote_candidate.ToString();
    return;
  }

  // Try this candidate for all future ports.
  remote_candidates_.push_back(RemoteCandidate(remote_candidate, origin_port));
}

// Set options on ourselves is simply setting options on all of our available
// port objects.
int P2PTransportChannel::SetOption(rtc::Socket::Option opt, int value) {
  RTC_DCHECK(network_thread_ == rtc::Thread::Current());
  OptionMap::iterator it = options_.find(opt);
  if (it == options_.end()) {
    options_.insert(std::make_pair(opt, value));
  } else if (it->second == value) {
    return 0;
  } else {
    it->second = value;
  }

  for (PortInterface* port : ports_) {
    int val = port->SetOption(opt, value);
    if (val < 0) {
      // Because this also occurs deferred, probably no point in reporting an
      // error
      RTC_LOG(WARNING) << "SetOption(" << opt << ", " << value
                       << ") failed: " << port->GetError();
    }
  }
  return 0;
}

bool P2PTransportChannel::GetOption(rtc::Socket::Option opt, int* value) {
  RTC_DCHECK(network_thread_ == rtc::Thread::Current());

  const auto& found = options_.find(opt);
  if (found == options_.end()) {
    return false;
  }
  *value = found->second;
  return true;
}

int P2PTransportChannel::GetError() {
  return error_;
}

// Send data to the other side, using our selected connection.
int P2PTransportChannel::SendPacket(const char *data, size_t len,
                                    const rtc::PacketOptions& options,
                                    int flags) {
  RTC_DCHECK(network_thread_ == rtc::Thread::Current());
  if (flags != 0) {
    error_ = EINVAL;
    return -1;
  }
  // If we don't think the connection is working yet, return ENOTCONN
  // instead of sending a packet that will probably be dropped.
  if (!ReadyToSend(selected_connection_)) {
    error_ = ENOTCONN;
    return -1;
  }

  last_sent_packet_id_ = options.packet_id;
  rtc::PacketOptions modified_options(options);
  modified_options.info_signaled_after_sent.packet_type =
      rtc::PacketType::kData;
  int sent = selected_connection_->Send(data, len, modified_options);
  if (sent <= 0) {
    RTC_DCHECK(sent < 0);
    error_ = selected_connection_->GetError();
  }
  return sent;
}

bool P2PTransportChannel::GetStats(ConnectionInfos* candidate_pair_stats_list,
                                   CandidateStatsList* candidate_stats_list) {
  RTC_DCHECK(network_thread_ == rtc::Thread::Current());
  // Gather candidate and candidate pair stats.
  candidate_stats_list->clear();
  candidate_pair_stats_list->clear();

  if (!allocator_sessions_.empty()) {
    allocator_session()->GetCandidateStatsFromReadyPorts(candidate_stats_list);
  }

  // TODO(qingsi): Remove naming inconsistency for candidate pair/connection.
  for (Connection* connection : connections_) {
    ConnectionInfo candidate_pair_stats = connection->stats();
    candidate_pair_stats.best_connection = (selected_connection_ == connection);
    candidate_pair_stats_list->push_back(std::move(candidate_pair_stats));
    connection->set_reported(true);
  }

  return true;
}

absl::optional<rtc::NetworkRoute> P2PTransportChannel::network_route() const {
  return network_route_;
}

rtc::DiffServCodePoint P2PTransportChannel::DefaultDscpValue() const {
  OptionMap::const_iterator it = options_.find(rtc::Socket::OPT_DSCP);
  if (it == options_.end()) {
    return rtc::DSCP_NO_CHANGE;
  }
  return static_cast<rtc::DiffServCodePoint> (it->second);
}

// Monitor connection states.
void P2PTransportChannel::UpdateConnectionStates() {
  int64_t now = rtc::TimeMillis();

  // We need to copy the list of connections since some may delete themselves
  // when we call UpdateState.
  for (Connection* c : connections_) {
    c->UpdateState(now);
  }
}

// Prepare for best candidate sorting.
void P2PTransportChannel::RequestSortAndStateUpdate(
    const std::string& reason_to_sort) {
  if (!sort_dirty_) {
    invoker_.AsyncInvoke<void>(
        RTC_FROM_HERE, thread(),
        rtc::Bind(&P2PTransportChannel::SortConnectionsAndUpdateState, this,
                  reason_to_sort));
    sort_dirty_ = true;
  }
}

void P2PTransportChannel::MaybeStartPinging() {
  if (started_pinging_) {
    return;
  }

  int64_t now = rtc::TimeMillis();
  if (std::any_of(
          connections_.begin(), connections_.end(),
          [this, now](const Connection* c) { return IsPingable(c, now); })) {
    RTC_LOG(LS_INFO) << ToString()
                     << ": Have a pingable connection for the first time; "
                        "starting to ping.";
    invoker_.AsyncInvoke<void>(
        RTC_FROM_HERE, thread(),
        rtc::Bind(&P2PTransportChannel::CheckAndPing, this));
    regathering_controller_->Start();
    started_pinging_ = true;
  }
}

int P2PTransportChannel::CompareCandidatePairNetworks(
    const Connection* a,
    const Connection* b,
    absl::optional<rtc::AdapterType> network_preference) const {
  int compare_a_b_by_network_preference =
      CompareCandidatePairsByNetworkPreference(a, b,
                                               config_.network_preference);
  // The network preference has a higher precedence than the network cost.
  if (compare_a_b_by_network_preference != a_and_b_equal) {
    return compare_a_b_by_network_preference;
  }

  uint32_t a_cost = a->ComputeNetworkCost();
  uint32_t b_cost = b->ComputeNetworkCost();
  // Prefer lower network cost.
  if (a_cost < b_cost) {
    return a_is_better;
  }
  if (a_cost > b_cost) {
    return b_is_better;
  }
  return a_and_b_equal;
}

// Compare two connections based on their writing, receiving, and connected
// states.
int P2PTransportChannel::CompareConnectionStates(
    const Connection* a,
    const Connection* b,
    absl::optional<int64_t> receiving_unchanged_threshold,
    bool* missed_receiving_unchanged_threshold) const {
  // First, prefer a connection that's writable or presumed writable over
  // one that's not writable.
  bool a_writable = a->writable() || PresumedWritable(a);
  bool b_writable = b->writable() || PresumedWritable(b);
  if (a_writable && !b_writable) {
    return a_is_better;
  }
  if (!a_writable && b_writable) {
    return b_is_better;
  }

  // Sort based on write-state. Better states have lower values.
  if (a->write_state() < b->write_state()) {
    return a_is_better;
  }
  if (b->write_state() < a->write_state()) {
    return b_is_better;
  }

  // We prefer a receiving connection to a non-receiving, higher-priority
  // connection when sorting connections and choosing which connection to
  // switch to.
  if (a->receiving() && !b->receiving()) {
    return a_is_better;
  }
  if (!a->receiving() && b->receiving()) {
    if (!receiving_unchanged_threshold ||
        (a->receiving_unchanged_since() <= *receiving_unchanged_threshold &&
         b->receiving_unchanged_since() <= *receiving_unchanged_threshold)) {
      return b_is_better;
    }
    *missed_receiving_unchanged_threshold = true;
  }

  // WARNING: Some complexity here about TCP reconnecting.
  // When a TCP connection fails because of a TCP socket disconnecting, the
  // active side of the connection will attempt to reconnect for 5 seconds while
  // pretending to be writable (the connection is not set to the unwritable
  // state).  On the passive side, the connection also remains writable even
  // though it is disconnected, and a new connection is created when the active
  // side connects.  At that point, there are two TCP connections on the passive
  // side: 1. the old, disconnected one that is pretending to be writable, and
  // 2.  the new, connected one that is maybe not yet writable.  For purposes of
  // pruning, pinging, and selecting the selected connection, we want to treat
  // the new connection as "better" than the old one. We could add a method
  // called something like Connection::ImReallyBadEvenThoughImWritable, but that
  // is equivalent to the existing Connection::connected(), which we already
  // have. So, in code throughout this file, we'll check whether the connection
  // is connected() or not, and if it is not, treat it as "worse" than a
  // connected one, even though it's writable.  In the code below, we're doing
  // so to make sure we treat a new writable connection as better than an old
  // disconnected connection.

  // In the case where we reconnect TCP connections, the original best
  // connection is disconnected without changing to WRITE_TIMEOUT. In this case,
  // the new connection, when it becomes writable, should have higher priority.
  if (a->write_state() == Connection::STATE_WRITABLE &&
      b->write_state() == Connection::STATE_WRITABLE) {
    if (a->connected() && !b->connected()) {
      return a_is_better;
    }
    if (!a->connected() && b->connected()) {
      return b_is_better;
    }
  }
  return 0;
}

// Compares two connections based only on the candidate and network information.
// Returns positive if |a| is better than |b|.
int P2PTransportChannel::CompareConnectionCandidates(
    const Connection* a,
    const Connection* b) const {
  int compare_a_b_by_networks =
      CompareCandidatePairNetworks(a, b, config_.network_preference);
  if (compare_a_b_by_networks != a_and_b_equal) {
    return compare_a_b_by_networks;
  }

  // Compare connection priority. Lower values get sorted last.
  if (a->priority() > b->priority()) {
    return a_is_better;
  }
  if (a->priority() < b->priority()) {
    return b_is_better;
  }

  // If we're still tied at this point, prefer a younger generation.
  // (Younger generation means a larger generation number).
  int cmp = (a->remote_candidate().generation() + a->port()->generation()) -
            (b->remote_candidate().generation() + b->port()->generation());
  if (cmp != 0) {
    return cmp;
  }

  // A periodic regather (triggered by the regather_all_networks_interval_range)
  // will produce candidates that appear the same but would use a new port. We
  // want to use the new candidates and purge the old candidates as they come
  // in, so use the fact that the old ports get pruned immediately to rank the
  // candidates with an active port/remote candidate higher.
  bool a_pruned = IsPortPruned(a->port()) ||
      IsRemoteCandidatePruned(a->remote_candidate());
  bool b_pruned = IsPortPruned(b->port()) ||
      IsRemoteCandidatePruned(b->remote_candidate());
  if (!a_pruned && b_pruned) {
    return a_is_better;
  }
  if (a_pruned && !b_pruned) {
    return b_is_better;
  }

  // Otherwise, must be equal
  return 0;
}

bool P2PTransportChannel::IsPortPruned(const Port* port) const {
  return std::find(ports_.begin(), ports_.end(), port) == ports_.end();
}

bool P2PTransportChannel::IsRemoteCandidatePruned(const Candidate& cand) const {
  return std::find(remote_candidates_.begin(), remote_candidates_.end(), cand)
      == remote_candidates_.end();
}

int P2PTransportChannel::CompareConnections(
    const Connection* a,
    const Connection* b,
    absl::optional<int64_t> receiving_unchanged_threshold,
    bool* missed_receiving_unchanged_threshold) const {
  RTC_CHECK(a != nullptr);
  RTC_CHECK(b != nullptr);

  // We prefer to switch to a writable and receiving connection over a
  // non-writable or non-receiving connection, even if the latter has
  // been nominated by the controlling side.
  int state_cmp = CompareConnectionStates(a, b, receiving_unchanged_threshold,
                                          missed_receiving_unchanged_threshold);
  if (state_cmp != 0) {
    return state_cmp;
  }

  if (ice_role_ == ICEROLE_CONTROLLED) {
    // Compare the connections based on the nomination states and the last data
    // received time if this is on the controlled side.
    if (a->remote_nomination() > b->remote_nomination()) {
      return a_is_better;
    }
    if (a->remote_nomination() < b->remote_nomination()) {
      return b_is_better;
    }

    if (a->last_data_received() > b->last_data_received()) {
      return a_is_better;
    }
    if (a->last_data_received() < b->last_data_received()) {
      return b_is_better;
    }
  }

  // Compare the network cost and priority.
  return CompareConnectionCandidates(a, b);
}

bool P2PTransportChannel::PresumedWritable(const Connection* conn) const {
  return (conn->write_state() == Connection::STATE_WRITE_INIT &&
          config_.presume_writable_when_fully_relayed &&
          conn->local_candidate().type() == RELAY_PORT_TYPE &&
          (conn->remote_candidate().type() == RELAY_PORT_TYPE ||
           conn->remote_candidate().type() == PRFLX_PORT_TYPE));
}

// Sort the available connections to find the best one.  We also monitor
// the number of available connections and the current state.
void P2PTransportChannel::SortConnectionsAndUpdateState(
    const std::string& reason_to_sort) {
  RTC_DCHECK(network_thread_ == rtc::Thread::Current());

  // Make sure the connection states are up-to-date since this affects how they
  // will be sorted.
  UpdateConnectionStates();

  // Any changes after this point will require a re-sort.
  sort_dirty_ = false;

  // Find the best alternative connection by sorting.  It is important to note
  // that amongst equal preference, writable connections, this will choose the
  // one whose estimated latency is lowest.  So it is the only one that we
  // need to consider switching to.
  // TODO(honghaiz): Don't sort;  Just use std::max_element in the right places.
  std::stable_sort(connections_.begin(), connections_.end(),
                   [this](const Connection* a, const Connection* b) {
                     int cmp = CompareConnections(a, b, absl::nullopt, nullptr);
                     if (cmp != 0) {
                       return cmp > 0;
                     }
                     // Otherwise, sort based on latency estimate.
                     return a->rtt() < b->rtt();
                   });

  RTC_LOG(LS_VERBOSE) << "Sorting " << connections_.size()
                      << " available connections";
  for (size_t i = 0; i < connections_.size(); ++i) {
    RTC_LOG(LS_VERBOSE) << connections_[i]->ToString();
  }

  Connection* top_connection =
      (connections_.size() > 0) ? connections_[0] : nullptr;

  // If necessary, switch to the new choice. Note that |top_connection| doesn't
  // have to be writable to become the selected connection although it will
  // have higher priority if it is writable.
  MaybeSwitchSelectedConnection(top_connection, reason_to_sort);

  // The controlled side can prune only if the selected connection has been
  // nominated because otherwise it may prune the connection that will be
  // selected by the controlling side.
  // TODO(honghaiz): This is not enough to prevent a connection from being
  // pruned too early because with aggressive nomination, the controlling side
  // will nominate every connection until it becomes writable.
  if (ice_role_ == ICEROLE_CONTROLLING ||
      (selected_connection_ && selected_connection_->nominated())) {
    PruneConnections();
  }

  // Check if all connections are timedout.
  bool all_connections_timedout = true;
  for (size_t i = 0; i < connections_.size(); ++i) {
    if (connections_[i]->write_state() != Connection::STATE_WRITE_TIMEOUT) {
      all_connections_timedout = false;
      break;
    }
  }

  // Now update the writable state of the channel with the information we have
  // so far.
  if (all_connections_timedout) {
    HandleAllTimedOut();
  }

  // Update the state of this channel.
  UpdateState();

  // Also possibly start pinging.
  // We could start pinging if:
  // * The first connection was created.
  // * ICE credentials were provided.
  // * A TCP connection became connected.
  MaybeStartPinging();
}

std::map<rtc::Network*, Connection*>
P2PTransportChannel::GetBestConnectionByNetwork() const {
  // |connections_| has been sorted, so the first one in the list on a given
  // network is the best connection on the network, except that the selected
  // connection is always the best connection on the network.
  std::map<rtc::Network*, Connection*> best_connection_by_network;
  if (selected_connection_) {
    best_connection_by_network[selected_connection_->port()->Network()] =
        selected_connection_;
  }
  // TODO(honghaiz): Need to update this if |connections_| are not sorted.
  for (Connection* conn : connections_) {
    rtc::Network* network = conn->port()->Network();
    // This only inserts when the network does not exist in the map.
    best_connection_by_network.insert(std::make_pair(network, conn));
  }
  return best_connection_by_network;
}

std::vector<Connection*>
P2PTransportChannel::GetBestWritableConnectionPerNetwork() const {
  std::vector<Connection*> connections;
  for (auto kv : GetBestConnectionByNetwork()) {
    Connection* conn = kv.second;
    if (conn->writable() && conn->connected()) {
      connections.push_back(conn);
    }
  }
  return connections;
}

void P2PTransportChannel::PruneConnections() {
  // We can prune any connection for which there is a connected, writable
  // connection on the same network with better or equal priority.  We leave
  // those with better priority just in case they become writable later (at
  // which point, we would prune out the current selected connection).  We leave
  // connections on other networks because they may not be using the same
  // resources and they may represent very distinct paths over which we can
  // switch. If |best_conn_on_network| is not connected, we may be reconnecting
  // a TCP connection and should not prune connections in this network.
  // See the big comment in CompareConnectionStates.
  //
  // An exception is made for connections on an "any address" network, meaning
  // not bound to any specific network interface. We don't want to keep one of
  // these alive as a backup, since it could be using the same network
  // interface as the higher-priority, selected candidate pair.
  auto best_connection_by_network = GetBestConnectionByNetwork();
  for (Connection* conn : connections_) {
    Connection* best_conn = selected_connection_;
    if (!rtc::IPIsAny(conn->port()->Network()->ip())) {
      // If the connection is bound to a specific network interface (not an
      // "any address" network), compare it against the best connection for
      // that network interface rather than the best connection overall. This
      // ensures that at least one connection per network will be left
      // unpruned.
      best_conn = best_connection_by_network[conn->port()->Network()];
    }
    // Do not prune connections if the connection being compared against is
    // weak. Otherwise, it may delete connections prematurely.
    if (best_conn && conn != best_conn && !best_conn->weak() &&
        CompareConnectionCandidates(best_conn, conn) >= 0) {
      conn->Prune();
    }
  }
}

// Change the selected connection, and let listeners know.
void P2PTransportChannel::SwitchSelectedConnection(Connection* conn) {
  // Note: if conn is NULL, the previous |selected_connection_| has been
  // destroyed, so don't use it.
  Connection* old_selected_connection = selected_connection_;
  selected_connection_ = conn;
  LogCandidatePairConfig(conn, webrtc::IceCandidatePairConfigType::kSelected);
  network_route_.reset();
  if (old_selected_connection) {
    old_selected_connection->set_selected(false);
  }
  if (selected_connection_) {
    ++nomination_;
    selected_connection_->set_selected(true);
    if (old_selected_connection) {
      RTC_LOG(LS_INFO) << ToString()
                       << ": Previous selected connection: "
                       << old_selected_connection->ToString();
    }
    RTC_LOG(LS_INFO) << ToString()
                     << ": New selected connection: "
                     << selected_connection_->ToString();
    SignalRouteChange(this, selected_connection_->remote_candidate());
    // This is a temporary, but safe fix to webrtc issue 5705.
    // TODO(honghaiz): Make all ENOTCONN error routed through the transport
    // channel so that it knows whether the media channel is allowed to
    // send; then it will only signal ready-to-send if the media channel
    // has been disallowed to send.
    if (selected_connection_->writable() ||
        PresumedWritable(selected_connection_)) {
      SignalReadyToSend(this);
    }

    network_route_.emplace(rtc::NetworkRoute());
    network_route_->connected = ReadyToSend(selected_connection_);
    network_route_->local_network_id =
        selected_connection_->local_candidate().network_id();
    network_route_->remote_network_id =
        selected_connection_->remote_candidate().network_id();
    network_route_->last_sent_packet_id = last_sent_packet_id_;
    network_route_->packet_overhead =
        GetIpOverhead(
            selected_connection_->local_candidate().address().family()) +
        GetProtocolOverhead(selected_connection_->local_candidate().protocol());
  } else {
    RTC_LOG(LS_INFO) << ToString()
                     << ": No selected connection";
  }

  SignalNetworkRouteChanged(network_route_);
}

// Warning: UpdateState should eventually be called whenever a connection
// is added, deleted, or the write state of any connection changes so that the
// transport controller will get the up-to-date channel state. However it
// should not be called too often; in the case that multiple connection states
// change, it should be called after all the connection states have changed. For
// example, we call this at the end of SortConnectionsAndUpdateState.
void P2PTransportChannel::UpdateState() {
  IceTransportState state = ComputeState();
  webrtc::IceTransportState current_standardized_state =
      ComputeIceTransportState();

  if (state_ != state) {
    RTC_LOG(LS_INFO) << ToString()
                     << ": Transport channel state changed from "
                     << static_cast<int>(state_) << " to "
                     << static_cast<int>(state);
    // Check that the requested transition is allowed. Note that
    // P2PTransportChannel does not (yet) implement a direct mapping of the ICE
    // states from the standard; the difference is covered by
    // TransportController and PeerConnection.
    switch (state_) {
      case IceTransportState::STATE_INIT:
        // TODO(deadbeef): Once we implement end-of-candidates signaling,
        // we shouldn't go from INIT to COMPLETED.
        RTC_DCHECK(state == IceTransportState::STATE_CONNECTING ||
                   state == IceTransportState::STATE_COMPLETED);
        break;
      case IceTransportState::STATE_CONNECTING:
        RTC_DCHECK(state == IceTransportState::STATE_COMPLETED ||
                   state == IceTransportState::STATE_FAILED);
        break;
      case IceTransportState::STATE_COMPLETED:
        // TODO(deadbeef): Once we implement end-of-candidates signaling,
        // we shouldn't go from COMPLETED to CONNECTING.
        // Though we *can* go from COMPlETED to FAILED, if consent expires.
        RTC_DCHECK(state == IceTransportState::STATE_CONNECTING ||
                   state == IceTransportState::STATE_FAILED);
        break;
      case IceTransportState::STATE_FAILED:
        // TODO(deadbeef): Once we implement end-of-candidates signaling,
        // we shouldn't go from FAILED to CONNECTING or COMPLETED.
        RTC_DCHECK(state == IceTransportState::STATE_CONNECTING ||
                   state == IceTransportState::STATE_COMPLETED);
        break;
      default:
        RTC_NOTREACHED();
        break;
    }
    state_ = state;
    SignalStateChanged(this);
  }

  if (standardized_state_ != current_standardized_state) {
    standardized_state_ = current_standardized_state;
    SignalIceTransportStateChanged(this);
  }
  // If our selected connection is "presumed writable" (TURN-TURN with no
  // CreatePermission required), act like we're already writable to the upper
  // layers, so they can start media quicker.
  bool writable =
      selected_connection_ && (selected_connection_->writable() ||
                               PresumedWritable(selected_connection_));
  set_writable(writable);

  bool receiving = false;
  for (const Connection* connection : connections_) {
    if (connection->receiving()) {
      receiving = true;
      break;
    }
  }
  set_receiving(receiving);
}

void P2PTransportChannel::MaybeStopPortAllocatorSessions() {
  if (!IsGettingPorts()) {
    return;
  }

  for (const auto& session : allocator_sessions_) {
    if (session->IsStopped()) {
      continue;
    }
    // If gathering continually, keep the last session running so that
    // it can gather candidates if the networks change.
    if (config_.gather_continually() && session == allocator_sessions_.back()) {
      session->ClearGettingPorts();
    } else {
      session->StopGettingPorts();
    }
  }
}

// If all connections timed out, delete them all.
void P2PTransportChannel::HandleAllTimedOut() {
  for (Connection* connection : connections_) {
    connection->Destroy();
  }
}

bool P2PTransportChannel::weak() const {
  return !selected_connection_ || selected_connection_->weak();
}

bool P2PTransportChannel::ReadyToSend(Connection* connection) const {
  // Note that we allow sending on an unreliable connection, because it's
  // possible that it became unreliable simply due to bad chance.
  // So this shouldn't prevent attempting to send media.
  return connection != nullptr &&
         (connection->writable() ||
          connection->write_state() == Connection::STATE_WRITE_UNRELIABLE ||
          PresumedWritable(connection));
}

// Handle queued up check-and-ping request
void P2PTransportChannel::CheckAndPing() {
  // Make sure the states of the connections are up-to-date (since this affects
  // which ones are pingable).
  UpdateConnectionStates();
  // When the selected connection is not receiving or not writable, or any
  // active connection has not been pinged enough times, use the weak ping
  // interval.
  bool need_more_pings_at_weak_interval = std::any_of(
      connections_.begin(), connections_.end(), [](Connection* conn) {
        return conn->active() &&
               conn->num_pings_sent() < MIN_PINGS_AT_WEAK_PING_INTERVAL;
      });
  int ping_interval = (weak() || need_more_pings_at_weak_interval)
                          ? weak_ping_interval()
                          : strong_ping_interval();
  if (rtc::TimeMillis() >= last_ping_sent_ms_ + ping_interval) {
    Connection* conn = FindNextPingableConnection();
    if (conn) {
      PingConnection(conn);
      MarkConnectionPinged(conn);
    }
  }
  int delay = std::min(ping_interval, check_receiving_interval());
  invoker_.AsyncInvokeDelayed<void>(
      RTC_FROM_HERE, thread(),
      rtc::Bind(&P2PTransportChannel::CheckAndPing, this), delay);
}

// A connection is considered a backup connection if the channel state
// is completed, the connection is not the selected connection and it is active.
bool P2PTransportChannel::IsBackupConnection(const Connection* conn) const {
  return state_ == IceTransportState::STATE_COMPLETED &&
         conn != selected_connection_ && conn->active();
}

// Is the connection in a state for us to even consider pinging the other side?
// We consider a connection pingable even if it's not connected because that's
// how a TCP connection is kicked into reconnecting on the active side.
bool P2PTransportChannel::IsPingable(const Connection* conn,
                                     int64_t now) const {
  const Candidate& remote = conn->remote_candidate();
  // We should never get this far with an empty remote ufrag.
  RTC_DCHECK(!remote.username().empty());
  if (remote.username().empty() || remote.password().empty()) {
    // If we don't have an ICE ufrag and pwd, there's no way we can ping.
    return false;
  }

  // A failed connection will not be pinged.
  if (conn->state() == IceCandidatePairState::FAILED) {
    return false;
  }

  // An never connected connection cannot be written to at all, so pinging is
  // out of the question. However, if it has become WRITABLE, it is in the
  // reconnecting state so ping is needed.
  if (!conn->connected() && !conn->writable()) {
    return false;
  }

  // If the channel is weakly connected, ping all connections.
  if (weak()) {
    return true;
  }

  // Always ping active connections regardless whether the channel is completed
  // or not, but backup connections are pinged at a slower rate.
  if (IsBackupConnection(conn)) {
    return conn->rtt_samples() == 0 ||
           (now >= conn->last_ping_response_received() +
                       config_.backup_connection_ping_interval_or_default());
  }
  // Don't ping inactive non-backup connections.
  if (!conn->active()) {
    return false;
  }

  // Do ping unwritable, active connections.
  if (!conn->writable()) {
    return true;
  }

  // Ping writable, active connections if it's been long enough since the last
  // ping.
  return WritableConnectionPastPingInterval(conn, now);
}

bool P2PTransportChannel::WritableConnectionPastPingInterval(
    const Connection* conn,
    int64_t now) const {
  int interval = CalculateActiveWritablePingInterval(conn, now);
  return conn->last_ping_sent() + interval <= now;
}

int P2PTransportChannel::CalculateActiveWritablePingInterval(
    const Connection* conn,
    int64_t now) const {
  // Ping each connection at a higher rate at least
  // MIN_PINGS_AT_WEAK_PING_INTERVAL times.
  if (conn->num_pings_sent() < MIN_PINGS_AT_WEAK_PING_INTERVAL) {
    return weak_ping_interval();
  }

  int stable_interval =
      config_.stable_writable_connection_ping_interval_or_default();
  int weak_or_stablizing_interval = std::min(
      stable_interval, WEAK_OR_STABILIZING_WRITABLE_CONNECTION_PING_INTERVAL);
  // If the channel is weak or the connection is not stable yet, use the
  // weak_or_stablizing_interval.
  return (!weak() && conn->stable(now)) ? stable_interval
                                        : weak_or_stablizing_interval;
}

// Returns the next pingable connection to ping.
Connection* P2PTransportChannel::FindNextPingableConnection() {
  int64_t now = rtc::TimeMillis();

  // Rule 1: Selected connection takes priority over non-selected ones.
  if (selected_connection_ && selected_connection_->connected() &&
      selected_connection_->writable() &&
      WritableConnectionPastPingInterval(selected_connection_, now)) {
    return selected_connection_;
  }

  // Rule 2: If the channel is weak, we need to find a new writable and
  // receiving connection, probably on a different network. If there are lots of
  // connections, it may take several seconds between two pings for every
  // non-selected connection. This will cause the receiving state of those
  // connections to be false, and thus they won't be selected. This is
  // problematic for network fail-over. We want to make sure at least one
  // connection per network is pinged frequently enough in order for it to be
  // selectable. So we prioritize one connection per network.
  // Rule 2.1: Among such connections, pick the one with the earliest
  // last-ping-sent time.
  if (weak()) {
    auto selectable_connections = GetBestWritableConnectionPerNetwork();
    std::vector<Connection*> pingable_selectable_connections;
    std::copy_if(selectable_connections.begin(), selectable_connections.end(),
                 std::back_inserter(pingable_selectable_connections),
                 [this, now](Connection* conn) {
                   return WritableConnectionPastPingInterval(conn, now);
                 });
    auto iter = std::min_element(pingable_selectable_connections.begin(),
                                 pingable_selectable_connections.end(),
                                 [](Connection* conn1, Connection* conn2) {
                                   return conn1->last_ping_sent() <
                                          conn2->last_ping_sent();
                                 });
    if (iter != pingable_selectable_connections.end()) {
      return *iter;
    }
  }

  // Rule 3: Triggered checks have priority over non-triggered connections.
  // Rule 3.1: Among triggered checks, oldest takes precedence.
  Connection* oldest_triggered_check =
      FindOldestConnectionNeedingTriggeredCheck(now);
  if (oldest_triggered_check) {
    return oldest_triggered_check;
  }

  // Rule 4: Unpinged connections have priority over pinged ones.
  RTC_CHECK(connections_.size() ==
            pinged_connections_.size() + unpinged_connections_.size());
  // If there are unpinged and pingable connections, only ping those.
  // Otherwise, treat everything as unpinged.
  // TODO(honghaiz): Instead of adding two separate vectors, we can add a state
  // "pinged" to filter out unpinged connections.
  if (std::find_if(unpinged_connections_.begin(), unpinged_connections_.end(),
                   [this, now](Connection* conn) {
                     return this->IsPingable(conn, now);
                   }) == unpinged_connections_.end()) {
    unpinged_connections_.insert(pinged_connections_.begin(),
                                 pinged_connections_.end());
    pinged_connections_.clear();
  }

  // Among un-pinged pingable connections, "more pingable" takes precedence.
  std::vector<Connection*> pingable_connections;
  std::copy_if(unpinged_connections_.begin(), unpinged_connections_.end(),
               std::back_inserter(pingable_connections),
               [this, now](Connection* conn) { return IsPingable(conn, now); });
  auto iter =
      std::max_element(pingable_connections.begin(), pingable_connections.end(),
                       [this](Connection* conn1, Connection* conn2) {
                         // Some implementations of max_element compare an
                         // element with itself.
                         if (conn1 == conn2) {
                           return false;
                         }
                         return MorePingable(conn1, conn2) == conn2;
                       });
  if (iter != pingable_connections.end()) {
    return *iter;
  }
  return nullptr;
}

void P2PTransportChannel::MarkConnectionPinged(Connection* conn) {
  if (conn && pinged_connections_.insert(conn).second) {
    unpinged_connections_.erase(conn);
  }
}

// Apart from sending ping from |conn| this method also updates
// |use_candidate_attr| and |nomination| flags. One of the flags is set to
// nominate |conn| if this channel is in CONTROLLING.
void P2PTransportChannel::PingConnection(Connection* conn) {
  bool use_candidate_attr = false;
  uint32_t nomination = 0;
  if (ice_role_ == ICEROLE_CONTROLLING) {
    bool renomination_supported = ice_parameters_.renomination &&
                                  !remote_ice_parameters_.empty() &&
                                  remote_ice_parameters_.back().renomination;
    if (renomination_supported) {
      nomination = GetNominationAttr(conn);
    } else {
      use_candidate_attr =
          GetUseCandidateAttr(conn, config_.default_nomination_mode);
    }
  }
  conn->set_nomination(nomination);
  conn->set_use_candidate_attr(use_candidate_attr);
  last_ping_sent_ms_ = rtc::TimeMillis();
  conn->Ping(last_ping_sent_ms_);
}

uint32_t P2PTransportChannel::GetNominationAttr(Connection* conn) const {
  return (conn == selected_connection_) ? nomination_ : 0;
}

// Nominate a connection based on the NominationMode.
bool P2PTransportChannel::GetUseCandidateAttr(Connection* conn,
                                              NominationMode mode) const {
  switch (mode) {
    case NominationMode::REGULAR:
      // TODO(honghaiz): Implement regular nomination.
      return false;
    case NominationMode::AGGRESSIVE:
      if (remote_ice_mode_ == ICEMODE_LITE) {
        return GetUseCandidateAttr(conn, NominationMode::REGULAR);
      }
      return true;
    case NominationMode::SEMI_AGGRESSIVE: {
      // Nominate if
      // a) Remote is in FULL ICE AND
      //    a.1) |conn| is the selected connection OR
      //    a.2) there is no selected connection OR
      //    a.3) the selected connection is unwritable OR
      //    a.4) |conn| has higher priority than selected_connection.
      // b) Remote is in LITE ICE AND
      //    b.1) |conn| is the selected_connection AND
      //    b.2) |conn| is writable.
      bool selected = conn == selected_connection_;
      if (remote_ice_mode_ == ICEMODE_LITE) {
        return selected && conn->writable();
      }
      bool better_than_selected =
          !selected_connection_ || !selected_connection_->writable() ||
          CompareConnectionCandidates(selected_connection_, conn) < 0;
      return selected || better_than_selected;
    }
    default:
      RTC_NOTREACHED();
      return false;
  }
}

// When a connection's state changes, we need to figure out who to use as
// the selected connection again.  It could have become usable, or become
// unusable.
void P2PTransportChannel::OnConnectionStateChange(Connection* connection) {
  RTC_DCHECK(network_thread_ == rtc::Thread::Current());

  // May stop the allocator session when at least one connection becomes
  // strongly connected after starting to get ports and the local candidate of
  // the connection is at the latest generation. It is not enough to check
  // that the connection becomes weakly connected because the connection may be
  // changing from (writable, receiving) to (writable, not receiving).
  bool strongly_connected = !connection->weak();
  bool latest_generation = connection->local_candidate().generation() >=
                           allocator_session()->generation();
  if (strongly_connected && latest_generation) {
    MaybeStopPortAllocatorSessions();
  }

  // We have to unroll the stack before doing this because we may be changing
  // the state of connections while sorting.
  RequestSortAndStateUpdate("candidate pair state changed");
}

// When a connection is removed, edit it out, and then update our best
// connection.
void P2PTransportChannel::OnConnectionDestroyed(Connection* connection) {
  RTC_DCHECK(network_thread_ == rtc::Thread::Current());

  // Note: the previous selected_connection_ may be destroyed by now, so don't
  // use it.

  // Remove this connection from the list.
  std::vector<Connection*>::iterator iter =
      std::find(connections_.begin(), connections_.end(), connection);
  RTC_DCHECK(iter != connections_.end());
  pinged_connections_.erase(*iter);
  unpinged_connections_.erase(*iter);
  connections_.erase(iter);

  RTC_LOG(LS_INFO) << ToString() << ": Removed connection " << connection
                   << " (" << connections_.size() << " remaining)";

  // If this is currently the selected connection, then we need to pick a new
  // one. The call to SortConnectionsAndUpdateState will pick a new one. It
  // looks at the current selected connection in order to avoid switching
  // between fairly similar ones. Since this connection is no longer an option,
  // we can just set selected to nullptr and re-choose a best assuming that
  // there was no selected connection.
  if (selected_connection_ == connection) {
    RTC_LOG(LS_INFO) << "Selected connection destroyed. Will choose a new one.";
    SwitchSelectedConnection(nullptr);
    RequestSortAndStateUpdate("selected candidate pair destroyed");
  } else {
    // If a non-selected connection was destroyed, we don't need to re-sort but
    // we do need to update state, because we could be switching to "failed" or
    // "completed".
    UpdateState();
  }
}

// When a port is destroyed, remove it from our list of ports to use for
// connection attempts.
void P2PTransportChannel::OnPortDestroyed(PortInterface* port) {
  RTC_DCHECK(network_thread_ == rtc::Thread::Current());

  ports_.erase(std::remove(ports_.begin(), ports_.end(), port), ports_.end());
  pruned_ports_.erase(
      std::remove(pruned_ports_.begin(), pruned_ports_.end(), port),
      pruned_ports_.end());
  RTC_LOG(INFO) << "Removed port because it is destroyed: " << ports_.size()
                << " remaining";
}

void P2PTransportChannel::OnPortsPruned(
    PortAllocatorSession* session,
    const std::vector<PortInterface*>& ports) {
  RTC_DCHECK(network_thread_ == rtc::Thread::Current());
  for (PortInterface* port : ports) {
    if (PrunePort(port)) {
      RTC_LOG(INFO) << "Removed port: " << port->ToString() << " "
                    << ports_.size() << " remaining";
    }
  }
}

void P2PTransportChannel::OnCandidatesRemoved(
    PortAllocatorSession* session,
    const std::vector<Candidate>& candidates) {
  RTC_DCHECK(network_thread_ == rtc::Thread::Current());
  // Do not signal candidate removals if continual gathering is not enabled, or
  // if this is not the last session because an ICE restart would have signaled
  // the remote side to remove all candidates in previous sessions.
  if (!config_.gather_continually() || session != allocator_session()) {
    return;
  }

  std::vector<Candidate> candidates_to_remove;
  for (Candidate candidate : candidates) {
    candidate.set_transport_name(transport_name());
    candidates_to_remove.push_back(candidate);
  }
  SignalCandidatesRemoved(this, candidates_to_remove);
}

void P2PTransportChannel::PruneAllPorts() {
  pruned_ports_.insert(pruned_ports_.end(), ports_.begin(), ports_.end());
  ports_.clear();
}

bool P2PTransportChannel::PrunePort(PortInterface* port) {
  auto it = std::find(ports_.begin(), ports_.end(), port);
  // Don't need to do anything if the port has been deleted from the port list.
  if (it == ports_.end()) {
    return false;
  }
  ports_.erase(it);
  pruned_ports_.push_back(port);
  return true;
}

// We data is available, let listeners know
void P2PTransportChannel::OnReadPacket(Connection* connection,
                                       const char* data,
                                       size_t len,
                                       int64_t packet_time_us) {
  RTC_DCHECK(network_thread_ == rtc::Thread::Current());

  // Do not deliver, if packet doesn't belong to the correct transport channel.
  if (!FindConnection(connection))
    return;

  // Let the client know of an incoming packet
  SignalReadPacket(this, data, len, packet_time_us, 0);

  // May need to switch the sending connection based on the receiving media path
  // if this is the controlled side.
  if (ice_role_ == ICEROLE_CONTROLLED) {
    MaybeSwitchSelectedConnection(connection, "data received");
  }
}

void P2PTransportChannel::OnSentPacket(const rtc::SentPacket& sent_packet) {
  RTC_DCHECK(network_thread_ == rtc::Thread::Current());

  SignalSentPacket(this, sent_packet);
}

void P2PTransportChannel::OnReadyToSend(Connection* connection) {
  if (connection == selected_connection_ && writable()) {
    SignalReadyToSend(this);
  }
}

// Find "triggered checks".  We ping first those connections that have
// received a ping but have not sent a ping since receiving it
// (last_ping_received > last_ping_sent).  But we shouldn't do
// triggered checks if the connection is already writable.
Connection* P2PTransportChannel::FindOldestConnectionNeedingTriggeredCheck(
    int64_t now) {
  Connection* oldest_needing_triggered_check = nullptr;
  for (auto* conn : connections_) {
    if (!IsPingable(conn, now)) {
      continue;
    }
    bool needs_triggered_check =
        (!conn->writable() &&
         conn->last_ping_received() > conn->last_ping_sent());
    if (needs_triggered_check &&
        (!oldest_needing_triggered_check ||
         (conn->last_ping_received() <
          oldest_needing_triggered_check->last_ping_received()))) {
      oldest_needing_triggered_check = conn;
    }
  }

  if (oldest_needing_triggered_check) {
    RTC_LOG(LS_INFO) << "Selecting connection for triggered check: "
                     << oldest_needing_triggered_check->ToString();
  }
  return oldest_needing_triggered_check;
}

Connection* P2PTransportChannel::MostLikelyToWork(Connection* conn1,
                                                  Connection* conn2) {
  bool rr1 = IsRelayRelay(conn1);
  bool rr2 = IsRelayRelay(conn2);
  if (rr1 && !rr2) {
    return conn1;
  } else if (rr2 && !rr1) {
    return conn2;
  } else if (rr1 && rr2) {
    bool udp1 = IsUdp(conn1);
    bool udp2 = IsUdp(conn2);
    if (udp1 && !udp2) {
      return conn1;
    } else if (udp2 && udp1) {
      return conn2;
    }
  }
  return nullptr;
}

Connection* P2PTransportChannel::LeastRecentlyPinged(Connection* conn1,
                                                     Connection* conn2) {
  if (conn1->last_ping_sent() < conn2->last_ping_sent()) {
    return conn1;
  }
  if (conn1->last_ping_sent() > conn2->last_ping_sent()) {
    return conn2;
  }
  return nullptr;
}

Connection* P2PTransportChannel::MorePingable(Connection* conn1,
                                              Connection* conn2) {
  RTC_DCHECK(conn1 != conn2);
  if (config_.prioritize_most_likely_candidate_pairs) {
    Connection* most_likely_to_work_conn = MostLikelyToWork(conn1, conn2);
    if (most_likely_to_work_conn) {
      return most_likely_to_work_conn;
    }
  }

  Connection* least_recently_pinged_conn = LeastRecentlyPinged(conn1, conn2);
  if (least_recently_pinged_conn) {
    return least_recently_pinged_conn;
  }

  // During the initial state when nothing has been pinged yet, return the first
  // one in the ordered |connections_|.
  return *(std::find_if(connections_.begin(), connections_.end(),
                        [conn1, conn2](Connection* conn) {
                          return conn == conn1 || conn == conn2;
                        }));
}

void P2PTransportChannel::set_writable(bool writable) {
  if (writable_ == writable) {
    return;
  }
  RTC_LOG(LS_VERBOSE) << ToString()
                      << ": Changed writable_ to " << writable;
  writable_ = writable;
  if (writable_) {
    SignalReadyToSend(this);
  }
  SignalWritableState(this);
}

void P2PTransportChannel::set_receiving(bool receiving) {
  if (receiving_ == receiving) {
    return;
  }
  receiving_ = receiving;
  SignalReceivingState(this);
}

void P2PTransportChannel::LogCandidatePairConfig(
    Connection* conn,
    webrtc::IceCandidatePairConfigType type) {
  if (conn == nullptr) {
    return;
  }
  ice_event_log_.LogCandidatePairConfig(type, conn->id(),
                                        conn->ToLogDescription());
}

}  // namespace cricket
