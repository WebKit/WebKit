/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/ice_transport_internal.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "api/candidate.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_error.h"
#include "p2p/base/p2p_constants.h"
#include "p2p/base/port.h"
#include "p2p/base/transport_description.h"
#include "rtc_base/checks.h"
#include "rtc_base/net_helper.h"

namespace webrtc {
namespace {

// RTCConfiguration uses kUndefined (-1) to indicate unset optional parameters.
std::optional<int> RTCConfigurationToIceConfigOptionalInt(
    int rtc_configuration_parameter) {
  if (rtc_configuration_parameter ==
      PeerConnectionInterface::RTCConfiguration::kUndefined) {
    return std::nullopt;
  }
  return rtc_configuration_parameter;
}

ContinualGatheringPolicy GetContinualGatheringPolicy(
    const PeerConnectionInterface::RTCConfiguration& config) {
  switch (config.continual_gathering_policy) {
    case PeerConnectionInterface::GATHER_ONCE:
      return GATHER_ONCE;
    case PeerConnectionInterface::GATHER_CONTINUALLY:
      return GATHER_CONTINUALLY;
    default:
      break;
  }
  RTC_DCHECK_NOTREACHED();
  return GATHER_ONCE;
}

}  // namespace

RTCError VerifyCandidate(const Candidate& cand) {
  // No address zero.
  if (cand.address().IsNil() || cand.address().IsAnyIP()) {
    return RTCError(RTCErrorType::INVALID_PARAMETER,
                    "candidate has address of zero");
  }

  // Disallow all ports below 1024, except for 80 and 443 on public addresses.
  int port = cand.address().port();
  if (cand.protocol() == TCP_PROTOCOL_NAME &&
      (cand.tcptype() == TCPTYPE_ACTIVE_STR || port == 0)) {
    // Expected for active-only candidates per
    // http://tools.ietf.org/html/rfc6544#section-4.5 so no error.
    // Libjingle clients emit port 0, in "active" mode.
    return RTCError::OK();
  }
  if (port < 1024) {
    if ((port != 80) && (port != 443)) {
      return RTCError(RTCErrorType::INVALID_PARAMETER,
                      "candidate has port below 1024, but not 80 or 443");
    }

    if (cand.address().IsPrivateIP()) {
      return RTCError(
          RTCErrorType::INVALID_PARAMETER,
          "candidate has port of 80 or 443 with private IP address");
    }
  }

  return RTCError::OK();
}

RTCError VerifyCandidates(const Candidates& candidates) {
  for (const Candidate& candidate : candidates) {
    RTCError error = VerifyCandidate(candidate);
    if (!error.ok())
      return error;
  }
  return RTCError::OK();
}

IceConfig::IceConfig() = default;

IceConfig::IceConfig(int receiving_timeout_ms,
                     int backup_connection_ping_interval,
                     ContinualGatheringPolicy gathering_policy,
                     bool prioritize_most_likely_candidate_pairs,
                     int stable_writable_connection_ping_interval_ms,
                     bool presume_writable_when_fully_relayed,
                     int regather_on_failed_networks_interval_ms,
                     int receiving_switching_delay_ms)
    : receiving_timeout(receiving_timeout_ms),
      backup_connection_ping_interval(backup_connection_ping_interval),
      continual_gathering_policy(gathering_policy),
      prioritize_most_likely_candidate_pairs(
          prioritize_most_likely_candidate_pairs),
      stable_writable_connection_ping_interval(
          stable_writable_connection_ping_interval_ms),
      presume_writable_when_fully_relayed(presume_writable_when_fully_relayed),
      regather_on_failed_networks_interval(
          regather_on_failed_networks_interval_ms),
      receiving_switching_delay(receiving_switching_delay_ms) {}

IceConfig::IceConfig(const PeerConnectionInterface::RTCConfiguration& config)
    : receiving_timeout(RTCConfigurationToIceConfigOptionalInt(
          config.ice_connection_receiving_timeout)),
      backup_connection_ping_interval(RTCConfigurationToIceConfigOptionalInt(
          config.ice_backup_candidate_pair_ping_interval)),
      continual_gathering_policy(GetContinualGatheringPolicy(config)),
      prioritize_most_likely_candidate_pairs(
          config.prioritize_most_likely_ice_candidate_pairs),
      stable_writable_connection_ping_interval(
          config.stable_writable_connection_ping_interval_ms),
      presume_writable_when_fully_relayed(
          config.presume_writable_when_fully_relayed),
      surface_ice_candidates_on_ice_transport_type_changed(
          config.surface_ice_candidates_on_ice_transport_type_changed),
      ice_check_interval_strong_connectivity(
          config.ice_check_interval_strong_connectivity),
      ice_check_interval_weak_connectivity(
          config.ice_check_interval_weak_connectivity),
      ice_check_min_interval(config.ice_check_min_interval),
      ice_unwritable_timeout(config.ice_unwritable_timeout),
      ice_unwritable_min_checks(config.ice_unwritable_min_checks),
      ice_inactive_timeout(config.ice_inactive_timeout),
      stun_keepalive_interval(config.stun_candidate_keepalive_interval),
      network_preference(config.network_preference) {}

IceConfig::~IceConfig() = default;

int IceConfig::receiving_timeout_or_default() const {
  return receiving_timeout.value_or(RECEIVING_TIMEOUT);
}
int IceConfig::backup_connection_ping_interval_or_default() const {
  return backup_connection_ping_interval.value_or(
      BACKUP_CONNECTION_PING_INTERVAL);
}
int IceConfig::stable_writable_connection_ping_interval_or_default() const {
  return stable_writable_connection_ping_interval.value_or(
      STRONG_AND_STABLE_WRITABLE_CONNECTION_PING_INTERVAL);
}
int IceConfig::regather_on_failed_networks_interval_or_default() const {
  return regather_on_failed_networks_interval.value_or(
      REGATHER_ON_FAILED_NETWORKS_INTERVAL);
}
int IceConfig::receiving_switching_delay_or_default() const {
  return receiving_switching_delay.value_or(RECEIVING_SWITCHING_DELAY);
}
int IceConfig::ice_check_interval_strong_connectivity_or_default() const {
  return ice_check_interval_strong_connectivity.value_or(STRONG_PING_INTERVAL);
}
int IceConfig::ice_check_interval_weak_connectivity_or_default() const {
  return ice_check_interval_weak_connectivity.value_or(WEAK_PING_INTERVAL);
}
int IceConfig::ice_check_min_interval_or_default() const {
  return ice_check_min_interval.value_or(-1);
}
int IceConfig::ice_unwritable_timeout_or_default() const {
  return ice_unwritable_timeout.value_or(CONNECTION_WRITE_CONNECT_TIMEOUT);
}
int IceConfig::ice_unwritable_min_checks_or_default() const {
  return ice_unwritable_min_checks.value_or(CONNECTION_WRITE_CONNECT_FAILURES);
}
int IceConfig::ice_inactive_timeout_or_default() const {
  return ice_inactive_timeout.value_or(CONNECTION_WRITE_TIMEOUT);
}
int IceConfig::stun_keepalive_interval_or_default() const {
  return stun_keepalive_interval.value_or(STUN_KEEPALIVE_INTERVAL);
}

RTCError IceConfig::IsValid() const {
  if (ice_check_interval_strong_connectivity_or_default() <
      ice_check_interval_weak_connectivity.value_or(WEAK_PING_INTERVAL)) {
    return RTCError(RTCErrorType::INVALID_PARAMETER,
                    "Ping interval of candidate pairs is shorter when ICE is "
                    "strongly connected than that when ICE is weakly "
                    "connected");
  }

  if (receiving_timeout_or_default() <
      std::max(ice_check_interval_strong_connectivity_or_default(),
               ice_check_min_interval_or_default())) {
    return RTCError(
        RTCErrorType::INVALID_PARAMETER,
        "Receiving timeout is shorter than the minimal ping interval.");
  }

  if (backup_connection_ping_interval_or_default() <
      ice_check_interval_strong_connectivity_or_default()) {
    return RTCError(RTCErrorType::INVALID_PARAMETER,
                    "Ping interval of backup candidate pairs is shorter than "
                    "that of general candidate pairs when ICE is strongly "
                    "connected");
  }

  if (stable_writable_connection_ping_interval_or_default() <
      ice_check_interval_strong_connectivity_or_default()) {
    return RTCError(RTCErrorType::INVALID_PARAMETER,
                    "Ping interval of stable and writable candidate pairs is "
                    "shorter than that of general candidate pairs when ICE is "
                    "strongly connected");
  }

  if (ice_unwritable_timeout_or_default() > ice_inactive_timeout_or_default()) {
    return RTCError(RTCErrorType::INVALID_PARAMETER,
                    "The timeout period for the writability state to become "
                    "UNRELIABLE is longer than that to become TIMEOUT.");
  }

  return RTCError::OK();
}

IceTransportInternal::IceTransportInternal() {}

IceTransportInternal::~IceTransportInternal() = default;

void IceTransportInternal::SetIceCredentials(absl::string_view ice_ufrag,
                                             absl::string_view ice_pwd) {
  SetIceParameters(IceParameters(ice_ufrag, ice_pwd, false));
}

void IceTransportInternal::SetRemoteIceCredentials(absl::string_view ice_ufrag,
                                                   absl::string_view ice_pwd) {
  SetRemoteIceParameters(IceParameters(ice_ufrag, ice_pwd, false));
}

void IceTransportInternal::AddGatheringStateCallback(
    const void* removal_tag,
    absl::AnyInvocable<void(IceTransportInternal*)> callback) {
  gathering_state_callback_list_.AddReceiver(removal_tag, std::move(callback));
}
void IceTransportInternal::RemoveGatheringStateCallback(
    const void* removal_tag) {
  gathering_state_callback_list_.RemoveReceivers(removal_tag);
}

}  // namespace webrtc
