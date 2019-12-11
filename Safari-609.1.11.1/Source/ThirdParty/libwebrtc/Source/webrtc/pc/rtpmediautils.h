/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_RTPMEDIAUTILS_H_
#define PC_RTPMEDIAUTILS_H_

#include "api/rtptransceiverinterface.h"

namespace webrtc {

// Returns the RtpTransceiverDirection that satisfies specified send and receive
// conditions.
RtpTransceiverDirection RtpTransceiverDirectionFromSendRecv(bool send,
                                                            bool recv);

// Returns true only if the direction will send media.
bool RtpTransceiverDirectionHasSend(RtpTransceiverDirection direction);

// Returns true only if the direction will receive media.
bool RtpTransceiverDirectionHasRecv(RtpTransceiverDirection direction);

// Returns the RtpTransceiverDirection which is the reverse of the given
// direction.
RtpTransceiverDirection RtpTransceiverDirectionReversed(
    RtpTransceiverDirection direction);

// Returns the RtpTransceiverDirection with its send component set to |send|.
RtpTransceiverDirection RtpTransceiverDirectionWithSendSet(
    RtpTransceiverDirection direction,
    bool send = true);

// Returns the RtpTransceiverDirection with its recv component set to |recv|.
RtpTransceiverDirection RtpTransceiverDirectionWithRecvSet(
    RtpTransceiverDirection direction,
    bool recv = true);

// Returns an unspecified string representation of the given direction.
const char* RtpTransceiverDirectionToString(RtpTransceiverDirection direction);

#ifdef UNIT_TEST
inline std::ostream& operator<<(  // no-presubmit-check TODO(webrtc:8982)
    std::ostream& os,             // no-presubmit-check TODO(webrtc:8982)
    RtpTransceiverDirection direction) {
  return os << RtpTransceiverDirectionToString(direction);
}
#endif  // UNIT_TEST

}  // namespace webrtc

#endif  // PC_RTPMEDIAUTILS_H_
