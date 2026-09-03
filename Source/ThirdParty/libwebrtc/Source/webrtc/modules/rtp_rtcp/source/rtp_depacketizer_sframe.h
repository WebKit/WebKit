/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTP_DEPACKETIZER_SFRAME_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_DEPACKETIZER_SFRAME_H_

#include <memory>

#include "api/rtc_error.h"
#include "modules/rtp_rtcp/source/sframe_rtp_packet_received.h"

namespace webrtc {

class RtpPacketReceived;

// Parses the SFrame RTP payload descriptor from the given `packet`.
// Consumes the first byte of the payload, returns a SframeRtpPacketReceived
// carrying the parsed SframeDescriptor and a copy of the packet whose payload
// has had the descriptor byte stripped.  Returns an error if `packet` is not
// a valid SFrame packet.
RTCErrorOr<std::unique_ptr<SframeRtpPacketReceived>>
ParseSframeRtpPacketOrError(const RtpPacketReceived& packet);

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_RTP_DEPACKETIZER_SFRAME_H_
