/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_INCLUDE_MOCK_MOCK_SEND_SIDE_CONGESTION_CONTROLLER_H_
#define MODULES_CONGESTION_CONTROLLER_INCLUDE_MOCK_MOCK_SEND_SIDE_CONGESTION_CONTROLLER_H_

#include "modules/congestion_controller/include/send_side_congestion_controller.h"
#include "test/gmock.h"

namespace webrtc {
namespace test {

class MockSendSideCongestionController : public SendSideCongestionController {
 public:
  MockSendSideCongestionController(const Clock* clock,
                                   RtcEventLog* event_log,
                                   PacedSender* paced_sender)
      : SendSideCongestionController(clock,
                                     nullptr /* observer */,
                                     event_log,
                                     paced_sender) {}

  MOCK_METHOD3(SetBweBitrates,
               void(int min_bitrate_bps,
                    int start_bitrate_bps,
                    int max_bitrate_bps));
};

}  // namespace test
}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_INCLUDE_MOCK_MOCK_SEND_SIDE_CONGESTION_CONTROLLER_H_
