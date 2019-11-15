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

#include <memory>
#include <vector>

#include "modules/pacing/paced_sender.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"

namespace webrtc {

class MockPacedSender : public PacedSender {
 public:
  MockPacedSender()
      : PacedSender(Clock::GetRealTimeClock(), nullptr, nullptr) {}
  MOCK_METHOD1(EnqueuePacket, void(std::unique_ptr<RtpPacketToSend> packet));
  MOCK_METHOD2(CreateProbeCluster, void(DataRate, int));
  MOCK_METHOD2(SetPacingRates, void(DataRate, DataRate));
  MOCK_CONST_METHOD0(OldestPacketWaitTime, TimeDelta());
  MOCK_CONST_METHOD0(QueueSizePackets, size_t());
  MOCK_CONST_METHOD0(ExpectedQueueTime, TimeDelta());
  MOCK_METHOD0(Process, void());
};

}  // namespace webrtc

#endif  // MODULES_PACING_MOCK_MOCK_PACED_SENDER_H_
