/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_P2P_CONSTANTS_H_
#define P2P_BASE_P2P_CONSTANTS_H_

#include <cstddef>
#include <cstdint>

#include "api/units/time_delta.h"
#include "rtc_base/system/plan_b_only.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

// CN_ == "content name".  When we initiate a session, we choose the
// name, and when we receive a Gingle session, we provide default
// names (since Gingle has no content names).  But when we receive a
// Jingle call, the content name can be anything, so don't rely on
// these values being the same as the ones received.
// Note: these were used in the deprecated "plan-b".
PLAN_B_ONLY extern const char CN_AUDIO[];
PLAN_B_ONLY extern const char CN_VIDEO[];
PLAN_B_ONLY extern const char CN_DATA[];
PLAN_B_ONLY extern const char CN_OTHER[];

extern const char GROUP_TYPE_BUNDLE[];

RTC_EXPORT extern const int ICE_UFRAG_LENGTH;
RTC_EXPORT extern const int ICE_PWD_LENGTH;
extern const size_t ICE_UFRAG_MIN_LENGTH;
extern const size_t ICE_PWD_MIN_LENGTH;
extern const size_t ICE_UFRAG_MAX_LENGTH;
extern const size_t ICE_PWD_MAX_LENGTH;

RTC_EXPORT extern const int ICE_CANDIDATE_COMPONENT_RTP;
RTC_EXPORT extern const int ICE_CANDIDATE_COMPONENT_RTCP;
RTC_EXPORT extern const int ICE_CANDIDATE_COMPONENT_DEFAULT;

// RFC 4145, SDP setup attribute values.
extern const char CONNECTIONROLE_ACTIVE_STR[];
extern const char CONNECTIONROLE_PASSIVE_STR[];
extern const char CONNECTIONROLE_ACTPASS_STR[];
extern const char CONNECTIONROLE_HOLDCONN_STR[];

// RFC 6762, the .local pseudo-top-level domain used for mDNS names.
extern const char LOCAL_TLD[];

// Most of the following constants are the default values of IceConfig
// paramters. See IceConfig for detailed definition.
//
// Default value IceConfig.ice_check_min_interval.
inline constexpr TimeDelta kMinCheckReceivingInterval = TimeDelta::Millis(50);
// Default value of IceConfig.receiving_timeout.
inline constexpr TimeDelta kReceivingTimeout = kMinCheckReceivingInterval * 50;
// The next two ping intervals are at the ICE transport level.
//
// kStrongPingInterval is applied when the selected connection is both
// writable and receiving.
//
// Default value of IceConfig.ice_check_interval_strong_connectivity.
inline constexpr TimeDelta kStrongPingInterval = TimeDelta::Millis(480);
// kWeakPingInterval is applied when the selected connection is either
// not writable or not receiving.
//
// Defaul value of IceConfig.ice_check_interval_weak_connectivity.
inline constexpr TimeDelta kWeakPingInterval = TimeDelta::Millis(48);
// The next two ping intervals are at the candidate pair level.
//
// Writable candidate pairs are pinged at a slower rate once they are stabilized
// and the channel is strongly connected.
inline constexpr TimeDelta kStrongAndStableWritableConnectionPingInterval =
    TimeDelta::Millis(2'500);
// Writable candidate pairs are pinged at a faster rate while the connections
// are stabilizing or the channel is weak.
inline constexpr TimeDelta kWeakOrStabilizingWritableConnectionPingInterval =
    TimeDelta::Millis(900);
// Default value of IceConfig.backup_connection_ping_interval
inline constexpr TimeDelta kBackupConnectionPingInterval =
    TimeDelta::Seconds(25);
// Defualt value of IceConfig.receiving_switching_delay.
inline constexpr TimeDelta kReceivingSwitchingDelay = TimeDelta::Seconds(1);
// Default value of IceConfig.regather_on_failed_networks_interval.
inline constexpr TimeDelta kRegatherOnFailedNetworksInterval =
    TimeDelta::Seconds(5 * 60);
// Default vaule of IceConfig.ice_unwritable_timeout.
inline constexpr TimeDelta kConnectionWriteConnectTimeout =
    TimeDelta::Seconds(5);
// Default vaule of IceConfig.ice_unwritable_min_checks.
inline constexpr int kConnectionWriteConnectFailures = 5;  // 5 pings
// Default value of IceConfig.ice_inactive_timeout;
inline constexpr TimeDelta kConnectionWriteTimeout = TimeDelta::Seconds(15);
// Default value of IceConfig.stun_keepalive_interval;
inline constexpr TimeDelta kStunKeepaliveInterval = TimeDelta::Seconds(10);

inline constexpr int kMinPingsAtWeakPingInterval = 3;

// The following constants are used at the candidate pair level to determine the
// state of a candidate pair.
//
// The timeout duration when a connection does not receive anything.
inline constexpr TimeDelta kWeakConnectionReceiveTimeout =
    TimeDelta::Millis(2'500);
// A connection will be declared dead if it has not received anything for this
// long.
inline constexpr TimeDelta kDeadConnectionReceiveTimeout =
    TimeDelta::Seconds(30);
// This is the length of time that we wait for a ping response to come back.
// There is no harm to keep this value high other than a small amount
// of increased memory, but in some networks (2G), we observe up to 60s RTTs.
inline constexpr TimeDelta kConnectionResponseTimeout = TimeDelta::Seconds(60);
// The minimum time we will wait before destroying a connection after creating
// it.
inline constexpr TimeDelta kMinConnectionLifetime = TimeDelta::Seconds(10);

// The type preference MUST be an integer from 0 to 126 inclusive.
// https://datatracker.ietf.org/doc/html/rfc5245#section-4.1.2.1
enum IcePriorityValue : uint8_t {
  ICE_TYPE_PREFERENCE_RELAY_TLS = 0,
  ICE_TYPE_PREFERENCE_RELAY_TCP = 1,
  ICE_TYPE_PREFERENCE_RELAY_DTLS = 2,
  ICE_TYPE_PREFERENCE_RELAY_UDP = 3,
  ICE_TYPE_PREFERENCE_PRFLX_TCP = 80,
  ICE_TYPE_PREFERENCE_HOST_TCP = 90,
  ICE_TYPE_PREFERENCE_SRFLX = 100,
  ICE_TYPE_PREFERENCE_PRFLX = 110,
  ICE_TYPE_PREFERENCE_HOST = 126
};

const int kMaxTurnUsernameLength = 509;  // RFC 8489 section 14.3

}  //  namespace webrtc


#endif  // P2P_BASE_P2P_CONSTANTS_H_
