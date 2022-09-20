/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/pacing/round_robin_packet_queue.h"

#include <utility>

#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "rtc_base/checks.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

constexpr uint32_t kDefaultSsrc = 123;
constexpr int kDefaultPayloadSize = 321;

std::unique_ptr<RtpPacketToSend> CreatePacket(RtpPacketMediaType type,
                                              uint16_t sequence_number) {
  auto packet = std::make_unique<RtpPacketToSend>(/*extensions=*/nullptr);
  packet->set_packet_type(type);
  packet->SetSsrc(kDefaultSsrc);
  packet->SetSequenceNumber(sequence_number);
  packet->SetPayloadSize(kDefaultPayloadSize);
  return packet;
}

}  // namespace

TEST(RoundRobinPacketQueueTest,
     PushAndPopUpdatesSizeInPacketsPerRtpPacketMediaType) {
  Timestamp now = Timestamp::Zero();
  RoundRobinPacketQueue queue(now);

  // Initially all sizes are zero.
  for (size_t i = 0; i < kNumMediaTypes; ++i) {
    EXPECT_EQ(queue.SizeInPacketsPerRtpPacketMediaType()[i], 0);
  }

  // Push packets.
  queue.Push(now, CreatePacket(RtpPacketMediaType::kAudio, 1));
  EXPECT_EQ(queue.SizeInPacketsPerRtpPacketMediaType()[static_cast<size_t>(
                RtpPacketMediaType::kAudio)],
            1);

  queue.Push(now, CreatePacket(RtpPacketMediaType::kVideo, 2));
  EXPECT_EQ(queue.SizeInPacketsPerRtpPacketMediaType()[static_cast<size_t>(
                RtpPacketMediaType::kVideo)],
            1);

  queue.Push(now, CreatePacket(RtpPacketMediaType::kRetransmission, 3));
  EXPECT_EQ(queue.SizeInPacketsPerRtpPacketMediaType()[static_cast<size_t>(
                RtpPacketMediaType::kRetransmission)],
            1);

  queue.Push(now, CreatePacket(RtpPacketMediaType::kForwardErrorCorrection, 4));
  EXPECT_EQ(queue.SizeInPacketsPerRtpPacketMediaType()[static_cast<size_t>(
                RtpPacketMediaType::kForwardErrorCorrection)],
            1);

  queue.Push(now, CreatePacket(RtpPacketMediaType::kPadding, 5));
  EXPECT_EQ(queue.SizeInPacketsPerRtpPacketMediaType()[static_cast<size_t>(
                RtpPacketMediaType::kPadding)],
            1);

  // Now all sizes are 1.
  for (size_t i = 0; i < kNumMediaTypes; ++i) {
    EXPECT_EQ(queue.SizeInPacketsPerRtpPacketMediaType()[i], 1);
  }

  // Popping happens in a priority order based on media type. This test does not
  // assert what this order is, only that the counter for the popped packet's
  // media type is decremented.
  for (size_t i = 0; i < kNumMediaTypes; ++i) {
    auto popped_packet = queue.Pop();
    EXPECT_EQ(queue.SizeInPacketsPerRtpPacketMediaType()[static_cast<size_t>(
                  popped_packet->packet_type().value())],
              0);
  }

  // We've popped all packets, so all sizes are zero.
  for (size_t i = 0; i < kNumMediaTypes; ++i) {
    EXPECT_EQ(queue.SizeInPacketsPerRtpPacketMediaType()[i], 0);
  }
}

}  // namespace webrtc
