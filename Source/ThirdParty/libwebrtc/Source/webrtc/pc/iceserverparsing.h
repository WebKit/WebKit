/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_PC_ICESERVERPARSING_H_
#define WEBRTC_PC_ICESERVERPARSING_H_

#include <vector>

#include "webrtc/api/peerconnectioninterface.h"
#include "webrtc/api/rtcerror.h"

namespace webrtc {

// Parses the URLs for each server in |servers| to build |stun_servers| and
// |turn_servers|. Can return SYNTAX_ERROR if the URL is malformed, or
// INVALID_PARAMETER if a TURN server is missing |username| or |password|.
//
// Intended to be used to convert/validate the servers passed into a
// PeerConnection through RTCConfiguration.
RTCErrorType ParseIceServers(
    const PeerConnectionInterface::IceServers& servers,
    cricket::ServerAddresses* stun_servers,
    std::vector<cricket::RelayServerConfig>* turn_servers);

}  // namespace webrtc

#endif  // WEBRTC_PC_ICESERVERPARSING_H_
