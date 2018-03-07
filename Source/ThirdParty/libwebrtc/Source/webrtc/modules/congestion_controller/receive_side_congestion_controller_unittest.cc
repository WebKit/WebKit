/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/include/receive_side_congestion_controller.h"
#include "modules/pacing/packet_router.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"
#include "test/gtest.h"

using testing::_;
using testing::AtLeast;
using testing::NiceMock;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;

namespace webrtc {

namespace {

// Helper to convert some time format to resolution used in absolute send time
// header extension, rounded upwards. |t| is the time to convert, in some
// resolution. |denom| is the value to divide |t| by to get whole seconds,
// e.g. |denom| = 1000 if |t| is in milliseconds.
uint32_t AbsSendTime(int64_t t, int64_t denom) {
  return (((t << 18) + (denom >> 1)) / denom) & 0x00fffffful;
}

class MockPacketRouter : public PacketRouter {
 public:
  MOCK_METHOD2(OnReceiveBitrateChanged,
               void(const std::vector<uint32_t>& ssrcs, uint32_t bitrate));
};

const uint32_t kInitialBitrateBps = 60000;

}  // namespace

namespace test {

TEST(ReceiveSideCongestionControllerTest, OnReceivedPacketWithAbsSendTime) {
  StrictMock<MockPacketRouter> packet_router;
  SimulatedClock clock_(123456);

  ReceiveSideCongestionController controller(&clock_, &packet_router);

  size_t payload_size = 1000;
  RTPHeader header;
  header.ssrc = 0x11eb21c;
  header.extension.hasAbsoluteSendTime = true;

  std::vector<unsigned int> ssrcs;
  EXPECT_CALL(packet_router, OnReceiveBitrateChanged(_, _))
      .WillRepeatedly(SaveArg<0>(&ssrcs));

  for (int i = 0; i < 10; ++i) {
    clock_.AdvanceTimeMilliseconds((1000 * payload_size) / kInitialBitrateBps);
    int64_t now_ms = clock_.TimeInMilliseconds();
    header.extension.absoluteSendTime = AbsSendTime(now_ms, 1000);
    controller.OnReceivedPacket(now_ms, payload_size, header);
  }

  ASSERT_EQ(1u, ssrcs.size());
  EXPECT_EQ(header.ssrc, ssrcs[0]);
}

}  // namespace test
}  // namespace webrtc
