/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/icetransportinternal.h"

namespace cricket {

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

IceConfig::~IceConfig() = default;

IceTransportInternal::IceTransportInternal() = default;

IceTransportInternal::~IceTransportInternal() = default;

void IceTransportInternal::SetIceCredentials(const std::string& ice_ufrag,
                                             const std::string& ice_pwd) {
  SetIceParameters(IceParameters(ice_ufrag, ice_pwd, false));
}

void IceTransportInternal::SetRemoteIceCredentials(const std::string& ice_ufrag,
                                                   const std::string& ice_pwd) {
  SetRemoteIceParameters(IceParameters(ice_ufrag, ice_pwd, false));
}

}  // namespace cricket
