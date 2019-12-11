/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/transport/test/create_feedback_generator.h"
#include "test/gtest.h"

namespace webrtc {
TEST(FeedbackGeneratorTest, ReportsFeedbackForSentPackets) {
  size_t kPacketSize = 1000;
  auto gen = CreateFeedbackGenerator(FeedbackGenerator::Config());
  for (int i = 0; i < 10; ++i) {
    gen->SendPacket(kPacketSize);
    gen->Sleep(TimeDelta::ms(50));
  }
  auto feedback_list = gen->PopFeedback();
  EXPECT_GT(feedback_list.size(), 0u);
  for (const auto& feedback : feedback_list) {
    EXPECT_GT(feedback.packet_feedbacks.size(), 0u);
    for (const auto& packet : feedback.packet_feedbacks) {
      EXPECT_EQ(packet.sent_packet.size.bytes<size_t>(), kPacketSize);
    }
  }
}
}  // namespace webrtc
