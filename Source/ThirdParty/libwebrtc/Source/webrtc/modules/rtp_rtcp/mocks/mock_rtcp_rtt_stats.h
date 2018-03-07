/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_MOCKS_MOCK_RTCP_RTT_STATS_H_
#define MODULES_RTP_RTCP_MOCKS_MOCK_RTCP_RTT_STATS_H_

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "test/gmock.h"

namespace webrtc {

class MockRtcpRttStats : public RtcpRttStats {
 public:
  MOCK_METHOD1(OnRttUpdate, void(int64_t rtt));
  MOCK_CONST_METHOD0(LastProcessedRtt, int64_t());
};
}  // namespace webrtc
#endif  // MODULES_RTP_RTCP_MOCKS_MOCK_RTCP_RTT_STATS_H_
