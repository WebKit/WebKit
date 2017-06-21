/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_CALL_RTX_RECEIVE_STREAM_H_
#define WEBRTC_CALL_RTX_RECEIVE_STREAM_H_

#include <map>

#include "webrtc/call/rtp_packet_sink_interface.h"

namespace webrtc {

class RtxReceiveStream : public RtpPacketSinkInterface {
 public:
  RtxReceiveStream(RtpPacketSinkInterface* media_sink,
                   std::map<int, int> rtx_payload_type_map,
                   uint32_t media_ssrc);
  ~RtxReceiveStream() override;
  // RtpPacketSinkInterface.
  void OnRtpPacket(const RtpPacketReceived& packet) override;

 private:
  RtpPacketSinkInterface* const media_sink_;
  // Mapping rtx_payload_type_map_[rtx] = associated.
  const std::map<int, int> rtx_payload_type_map_;
  // TODO(nisse): Ultimately, the media receive stream shouldn't care about the
  // ssrc, and we should delete this.
  const uint32_t media_ssrc_;
};

}  // namespace webrtc

#endif  // WEBRTC_CALL_RTX_RECEIVE_STREAM_H_
