/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef CALL_RTCP_PACKET_SINK_INTERFACE_H_
#define CALL_RTCP_PACKET_SINK_INTERFACE_H_

#include "api/array_view.h"

namespace webrtc {

// This class represents a receiver of unparsed RTCP packets.
// TODO(eladalon): Replace this by demuxing over parsed rather than raw data.
// Whether this should be over an entire RTCP packet, or over RTCP blocks,
// is still under discussion.
class RtcpPacketSinkInterface {
 public:
  virtual ~RtcpPacketSinkInterface() = default;
  virtual void OnRtcpPacket(rtc::ArrayView<const uint8_t> packet) = 0;
};

}  // namespace webrtc

#endif  // CALL_RTCP_PACKET_SINK_INTERFACE_H_
