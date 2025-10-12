/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_RTP_TRANSCEIVER_DIRECTION_H_
#define API_RTP_TRANSCEIVER_DIRECTION_H_

namespace webrtc {

// https://w3c.github.io/webrtc-pc/#dom-rtcrtptransceiverdirection
enum class RtpTransceiverDirection {
  kSendRecv,
  kSendOnly,
  kRecvOnly,
  kInactive,
  kStopped,
};

template <typename Sink>
void AbslStringify(Sink& sink, RtpTransceiverDirection direction) {
  switch (direction) {
    case RtpTransceiverDirection::kSendRecv:
      sink.Append("sendrecv");
      break;
    case RtpTransceiverDirection::kSendOnly:
      sink.Append("send");
      break;
    case RtpTransceiverDirection::kRecvOnly:
      sink.Append("recv");
      break;
    case RtpTransceiverDirection::kInactive:
      sink.Append("inactive");
      break;
    case RtpTransceiverDirection::kStopped:
      sink.Append("stopped");
      break;
  }
}

}  // namespace webrtc

#endif  // API_RTP_TRANSCEIVER_DIRECTION_H_
