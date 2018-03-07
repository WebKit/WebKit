/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_MOCKS_MOCK_RECOVERED_PACKET_RECEIVER_H_
#define MODULES_RTP_RTCP_MOCKS_MOCK_RECOVERED_PACKET_RECEIVER_H_

#include "modules/rtp_rtcp/include/flexfec_receiver.h"
#include "rtc_base/basictypes.h"
#include "test/gmock.h"

namespace webrtc {

class MockRecoveredPacketReceiver : public RecoveredPacketReceiver {
 public:
  MOCK_METHOD2(OnRecoveredPacket, void(const uint8_t* packet, size_t length));
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_MOCKS_MOCK_RECOVERED_PACKET_RECEIVER_H_
