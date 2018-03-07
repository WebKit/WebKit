/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_RTPTRANSPORTTESTUTIL_H_
#define PC_RTPTRANSPORTTESTUTIL_H_

#include "pc/rtptransportinternal.h"
#include "rtc_base/sigslot.h"

namespace webrtc {

class SignalPacketReceivedCounter : public sigslot::has_slots<> {
 public:
  explicit SignalPacketReceivedCounter(RtpTransportInternal* transport) {
    transport->SignalPacketReceived.connect(
        this, &SignalPacketReceivedCounter::OnPacketReceived);
  }
  int rtcp_count() const { return rtcp_count_; }
  int rtp_count() const { return rtp_count_; }

 private:
  void OnPacketReceived(bool rtcp,
                        rtc::CopyOnWriteBuffer*,
                        const rtc::PacketTime&) {
    if (rtcp) {
      ++rtcp_count_;
    } else {
      ++rtp_count_;
    }
  }
  int rtcp_count_ = 0;
  int rtp_count_ = 0;
};

}  // namespace webrtc

#endif  // PC_RTPTRANSPORTTESTUTIL_H_
