/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/p2p_constants.h"

#include <cstddef>
#include <cstdint>

#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"

namespace webrtc {

const char CN_AUDIO[] = "audio";
const char CN_VIDEO[] = "video";
const char CN_DATA[] = "data";
const char CN_OTHER[] = "main";

const char GROUP_TYPE_BUNDLE[] = "BUNDLE";

// Minimum ufrag length is 4 characters as per RFC5245.
const int ICE_UFRAG_LENGTH = 4;
// Minimum password length of 22 characters as per RFC5245. We chose 24 because
// some internal systems expect password to be multiple of 4.
const int ICE_PWD_LENGTH = 24;
const size_t ICE_UFRAG_MIN_LENGTH = 4;
const size_t ICE_PWD_MIN_LENGTH = 22;
const size_t ICE_UFRAG_MAX_LENGTH = 256;
const size_t ICE_PWD_MAX_LENGTH = 256;

// This is media-specific, so might belong
// somewhere like media/base/mediaconstants.h
const int ICE_CANDIDATE_COMPONENT_RTP = 1;
const int ICE_CANDIDATE_COMPONENT_RTCP = 2;
const int ICE_CANDIDATE_COMPONENT_DEFAULT = 1;

// From RFC 4145, SDP setup attribute values.
const char CONNECTIONROLE_ACTIVE_STR[] = "active";
const char CONNECTIONROLE_PASSIVE_STR[] = "passive";
const char CONNECTIONROLE_ACTPASS_STR[] = "actpass";
const char CONNECTIONROLE_HOLDCONN_STR[] = "holdconn";

const char LOCAL_TLD[] = ".local";

// When the socket is unwritable, we will use 10 Kbps (ignoring IP+UDP headers)
// for pinging. When the socket is writable, we will use only 1 Kbps because we
// don't want to degrade the quality on a modem.  These numbers should work well
// on a 28.8K modem, which is the slowest connection on which the voice quality
// is reasonable at all.
constexpr DataSize kStunPingPacketSize = DataSize::Bytes(60);
static_assert(kStrongPingInterval ==
              kStunPingPacketSize / DataRate::BitsPerSec(1'000));
static_assert(kWeakPingInterval ==
              kStunPingPacketSize / DataRate::BitsPerSec(10'000));

}  // namespace webrtc
