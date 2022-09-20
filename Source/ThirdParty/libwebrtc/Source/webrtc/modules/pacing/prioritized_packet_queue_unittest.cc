/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/pacing/prioritized_packet_queue.h"

#include <utility>

#include "api/units/time_delta.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "rtc_base/checks.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

constexpr uint32_t kDefaultSsrc = 123;
constexpr int kDefaultPayloadSize = 789;

std::unique_ptr<RtpPacketToSend> CreatePacket(RtpPacketMediaType type,
                                              uint16_t sequence_number,
                                              uint32_t ssrc = kDefaultSsrc) {
  auto packet = std::make_unique<RtpPacketToSend>(/*extensions=*/nullptr);
  packet->set_packet_type(type);
  packet->SetSsrc(ssrc);
  packet->SetSequenceNumber(sequence_number);
  packet->SetPayloadSize(kDefaultPayloadSize);
  return packet;
}

}  // namespace

TEST(PrioritizedPacketQueue, ReturnsPacketsInPrioritizedOrder) {
  Timestamp now = Timestamp::Zero();
  PrioritizedPacketQueue queue(now);

  // Add packets in low to high packet order.
  queue.Push(now, CreatePacket(RtpPacketMediaType::kPadding, /*seq=*/1));
  queue.Push(now, CreatePacket(RtpPacketMediaType::kVideo, /*seq=*/2));
  queue.Push(now, CreatePacket(RtpPacketMediaType::kForwardErrorCorrection,
                               /*seq=*/3));
  queue.Push(now, CreatePacket(RtpPacketMediaType::kRetransmission, /*seq=*/4));
  queue.Push(now, CreatePacket(RtpPacketMediaType::kAudio, /*seq=*/5));

  // Packets should be returned in high to low order.
  EXPECT_EQ(queue.Pop()->SequenceNumber(), 5);
  EXPECT_EQ(queue.Pop()->SequenceNumber(), 4);
  // Video and FEC prioritized equally - but video was enqueued first.
  EXPECT_EQ(queue.Pop()->SequenceNumber(), 2);
  EXPECT_EQ(queue.Pop()->SequenceNumber(), 3);
  EXPECT_EQ(queue.Pop()->SequenceNumber(), 1);
}

TEST(PrioritizedPacketQueue, ReturnsEqualPrioPacketsInRoundRobinOrder) {
  Timestamp now = Timestamp::Zero();
  PrioritizedPacketQueue queue(now);

  // Insert video packets (prioritized equally), simulating a simulcast-type use
  // case.
  queue.Push(now,
             CreatePacket(RtpPacketMediaType::kVideo, /*seq=*/1, /*ssrc=*/100));

  queue.Push(now,
             CreatePacket(RtpPacketMediaType::kVideo, /*seq=*/2, /*ssrc=*/101));
  queue.Push(now,
             CreatePacket(RtpPacketMediaType::kVideo, /*seq=*/3, /*ssrc=*/101));

  queue.Push(now,
             CreatePacket(RtpPacketMediaType::kVideo, /*seq=*/4, /*ssrc=*/102));
  queue.Push(now,
             CreatePacket(RtpPacketMediaType::kVideo, /*seq=*/5, /*ssrc=*/102));
  queue.Push(now,
             CreatePacket(RtpPacketMediaType::kVideo, /*seq=*/6, /*ssrc=*/102));
  queue.Push(now,
             CreatePacket(RtpPacketMediaType::kVideo, /*seq=*/7, /*ssrc=*/102));

  // First packet from each SSRC.
  EXPECT_EQ(queue.Pop()->SequenceNumber(), 1);
  EXPECT_EQ(queue.Pop()->SequenceNumber(), 2);
  EXPECT_EQ(queue.Pop()->SequenceNumber(), 4);

  // Second packets from streams that have packets left.
  EXPECT_EQ(queue.Pop()->SequenceNumber(), 3);
  EXPECT_EQ(queue.Pop()->SequenceNumber(), 5);

  // Only packets from last stream remaining.
  EXPECT_EQ(queue.Pop()->SequenceNumber(), 6);
  EXPECT_EQ(queue.Pop()->SequenceNumber(), 7);
}

TEST(PrioritizedPacketQueue, ReportsSizeInPackets) {
  PrioritizedPacketQueue queue(/*creation_time=*/Timestamp::Zero());
  EXPECT_EQ(queue.SizeInPackets(), 0);

  queue.Push(/*enqueue_time=*/Timestamp::Zero(),
             CreatePacket(RtpPacketMediaType::kVideo,
                          /*seq_no=*/1));
  EXPECT_EQ(queue.SizeInPackets(), 1);

  queue.Pop();
  EXPECT_EQ(queue.SizeInPackets(), 0);
}

TEST(PrioritizedPacketQueue, ReportsPayloadSize) {
  PrioritizedPacketQueue queue(/*creation_time=*/Timestamp::Zero());
  EXPECT_EQ(queue.SizeInPayloadBytes(), DataSize::Zero());

  queue.Push(/*enqueue_time=*/Timestamp::Zero(),
             CreatePacket(RtpPacketMediaType::kVideo,
                          /*seq_no=*/1));
  EXPECT_EQ(queue.SizeInPayloadBytes(), DataSize::Bytes(kDefaultPayloadSize));

  queue.Pop();
  EXPECT_EQ(queue.SizeInPayloadBytes(), DataSize::Zero());
}

TEST(PrioritizedPacketQueue, ReportsPaddingSize) {
  PrioritizedPacketQueue queue(/*creation_time=*/Timestamp::Zero());
  EXPECT_EQ(queue.SizeInPayloadBytes(), DataSize::Zero());
  static constexpr DataSize kPaddingSize = DataSize::Bytes(190);

  auto packet = std::make_unique<RtpPacketToSend>(/*extensions=*/nullptr);
  packet->set_packet_type(RtpPacketMediaType::kPadding);
  packet->SetSsrc(kDefaultSsrc);
  packet->SetSequenceNumber(/*seq=*/1);
  packet->SetPadding(kPaddingSize.bytes());
  queue.Push(/*enqueue_time=*/Timestamp::Zero(), std::move(packet));
  EXPECT_EQ(queue.SizeInPayloadBytes(), kPaddingSize);

  queue.Pop();
  EXPECT_EQ(queue.SizeInPayloadBytes(), DataSize::Zero());
}

TEST(PrioritizedPacketQueue, ReportsOldestEnqueueTime) {
  PrioritizedPacketQueue queue(/*creation_time=*/Timestamp::Zero());
  EXPECT_EQ(queue.OldestEnqueueTime(), Timestamp::MinusInfinity());

  // Add three packets, with the middle packet having higher prio.
  queue.Push(Timestamp::Millis(10),
             CreatePacket(RtpPacketMediaType::kPadding, /*seq=*/1));
  queue.Push(Timestamp::Millis(20),
             CreatePacket(RtpPacketMediaType::kVideo, /*seq=*/2));
  queue.Push(Timestamp::Millis(30),
             CreatePacket(RtpPacketMediaType::kPadding, /*seq=*/3));
  EXPECT_EQ(queue.OldestEnqueueTime(), Timestamp::Millis(10));

  queue.Pop();  // Pop packet with enqueue time 20.
  EXPECT_EQ(queue.OldestEnqueueTime(), Timestamp::Millis(10));

  queue.Pop();  // Pop packet with enqueue time 10.
  EXPECT_EQ(queue.OldestEnqueueTime(), Timestamp::Millis(30));

  queue.Pop();  // Pop packet with enqueue time 30, queue empty again.
  EXPECT_EQ(queue.OldestEnqueueTime(), Timestamp::MinusInfinity());
}

TEST(PrioritizedPacketQueue, ReportsAverageQueueTime) {
  PrioritizedPacketQueue queue(/*creation_time=*/Timestamp::Zero());
  EXPECT_EQ(queue.AverageQueueTime(), TimeDelta::Zero());

  // Add three packets, with the middle packet having higher prio.
  queue.Push(Timestamp::Millis(10),
             CreatePacket(RtpPacketMediaType::kPadding, /*seq=*/1));
  queue.Push(Timestamp::Millis(20),
             CreatePacket(RtpPacketMediaType::kVideo, /*seq=*/2));
  queue.Push(Timestamp::Millis(30),
             CreatePacket(RtpPacketMediaType::kPadding, /*seq=*/3));

  queue.UpdateAverageQueueTime(Timestamp::Millis(40));
  // Packets have waited 30, 20, 10 ms -> average = 20ms.
  EXPECT_EQ(queue.AverageQueueTime(), TimeDelta::Millis(20));

  queue.Pop();  // Pop packet with enqueue time 20.
  EXPECT_EQ(queue.AverageQueueTime(), TimeDelta::Millis(20));

  queue.Pop();  // Pop packet with enqueue time 10.
  EXPECT_EQ(queue.AverageQueueTime(), TimeDelta::Millis(10));

  queue.Pop();  // Pop packet with enqueue time 30, queue empty again.
  EXPECT_EQ(queue.AverageQueueTime(), TimeDelta::Zero());
}

TEST(PrioritizedPacketQueue, SubtractsPusedTimeFromAverageQueueTime) {
  PrioritizedPacketQueue queue(/*creation_time=*/Timestamp::Zero());
  EXPECT_EQ(queue.AverageQueueTime(), TimeDelta::Zero());

  // Add a packet and then enable paused state.
  queue.Push(Timestamp::Millis(100),
             CreatePacket(RtpPacketMediaType::kPadding, /*seq=*/1));
  queue.SetPauseState(true, Timestamp::Millis(600));
  EXPECT_EQ(queue.AverageQueueTime(), TimeDelta::Millis(500));

  // Enqueue a packet 500ms into the paused state. Queue time of
  // original packet is still seen as 500ms and new one has 0ms giving
  // an average of 250ms.
  queue.Push(Timestamp::Millis(1100),
             CreatePacket(RtpPacketMediaType::kVideo, /*seq=*/2));
  EXPECT_EQ(queue.AverageQueueTime(), TimeDelta::Millis(250));

  // Unpause some time later, queue time still unchanged.
  queue.SetPauseState(false, Timestamp::Millis(1600));
  EXPECT_EQ(queue.AverageQueueTime(), TimeDelta::Millis(250));

  // Update queue time 500ms after pause state ended.
  queue.UpdateAverageQueueTime(Timestamp::Millis(2100));
  EXPECT_EQ(queue.AverageQueueTime(), TimeDelta::Millis(750));
}

TEST(PrioritizedPacketQueue, ReportsLeadingAudioEnqueueTime) {
  PrioritizedPacketQueue queue(/*creation_time=*/Timestamp::Zero());
  EXPECT_EQ(queue.LeadingAudioPacketEnqueueTime(), Timestamp::MinusInfinity());

  queue.Push(Timestamp::Millis(10),
             CreatePacket(RtpPacketMediaType::kVideo, /*seq=*/1));
  EXPECT_EQ(queue.LeadingAudioPacketEnqueueTime(), Timestamp::MinusInfinity());

  queue.Push(Timestamp::Millis(20),
             CreatePacket(RtpPacketMediaType::kAudio, /*seq=*/2));

  EXPECT_EQ(queue.LeadingAudioPacketEnqueueTime(), Timestamp::Millis(20));

  queue.Pop();  // Pop audio packet.
  EXPECT_EQ(queue.LeadingAudioPacketEnqueueTime(), Timestamp::MinusInfinity());
}

TEST(PrioritizedPacketQueue,
     PushAndPopUpdatesSizeInPacketsPerRtpPacketMediaType) {
  Timestamp now = Timestamp::Zero();
  PrioritizedPacketQueue queue(now);

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
