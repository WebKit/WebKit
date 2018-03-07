/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_PACING_MOCK_MOCK_PACED_SENDER_H_
#define MODULES_PACING_MOCK_MOCK_PACED_SENDER_H_

#include <vector>

#include "modules/pacing/paced_sender.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"

namespace webrtc {

class MockPacedSender : public PacedSender {
 public:
  MockPacedSender()
      : PacedSender(Clock::GetRealTimeClock(), nullptr, nullptr) {}
  MOCK_METHOD6(SendPacket, bool(Priority priority,
                                uint32_t ssrc,
                                uint16_t sequence_number,
                                int64_t capture_time_ms,
                                size_t bytes,
                                bool retransmission));
  MOCK_METHOD1(CreateProbeCluster, void(int));
  MOCK_METHOD1(SetEstimatedBitrate, void(uint32_t));
  MOCK_CONST_METHOD0(QueueInMs, int64_t());
  MOCK_CONST_METHOD0(QueueInPackets, int());
  MOCK_CONST_METHOD0(ExpectedQueueTimeMs, int64_t());
  MOCK_CONST_METHOD0(GetApplicationLimitedRegionStartTime,
                     rtc::Optional<int64_t>());
  MOCK_METHOD0(Process, void());
};

}  // namespace webrtc

#endif  // MODULES_PACING_MOCK_MOCK_PACED_SENDER_H_
