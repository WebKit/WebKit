/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_P2PCONSTANTS_H_
#define WEBRTC_P2P_BASE_P2PCONSTANTS_H_

#include <string>

namespace cricket {

// CN_ == "content name".  When we initiate a session, we choose the
// name, and when we receive a Gingle session, we provide default
// names (since Gingle has no content names).  But when we receive a
// Jingle call, the content name can be anything, so don't rely on
// these values being the same as the ones received.
extern const char CN_AUDIO[];
extern const char CN_VIDEO[];
extern const char CN_DATA[];
extern const char CN_OTHER[];

// GN stands for group name
extern const char GROUP_TYPE_BUNDLE[];

extern const int ICE_UFRAG_LENGTH;
extern const int ICE_PWD_LENGTH;
extern const size_t ICE_UFRAG_MIN_LENGTH;
extern const size_t ICE_PWD_MIN_LENGTH;
extern const size_t ICE_UFRAG_MAX_LENGTH;
extern const size_t ICE_PWD_MAX_LENGTH;

extern const int ICE_CANDIDATE_COMPONENT_RTP;
extern const int ICE_CANDIDATE_COMPONENT_RTCP;
extern const int ICE_CANDIDATE_COMPONENT_DEFAULT;

extern const char NS_JINGLE_RTP[];
extern const char NS_JINGLE_DRAFT_SCTP[];

// RFC 4145, SDP setup attribute values.
extern const char CONNECTIONROLE_ACTIVE_STR[];
extern const char CONNECTIONROLE_PASSIVE_STR[];
extern const char CONNECTIONROLE_ACTPASS_STR[];
extern const char CONNECTIONROLE_HOLDCONN_STR[];

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_P2PCONSTANTS_H_
