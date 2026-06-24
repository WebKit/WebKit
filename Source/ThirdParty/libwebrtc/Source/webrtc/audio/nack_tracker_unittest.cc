/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/nack_tracker.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "api/units/time_delta.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

constexpr int kSampleRateHz = 16000;
constexpr uint32_t kTimestampIncrement = 480;  // 30 ms.
constexpr TimeDelta kShortRoundTripTime = TimeDelta::Zero();
constexpr int kDefaultNackListSize = 200;

bool IsNackListCorrect(const std::vector<uint16_t>& nack_list,
                       const uint16_t* lost_sequence_numbers,
                       size_t num_lost_packets) {
  if (nack_list.size() != num_lost_packets)
    return false;

  if (num_lost_packets == 0)
    return true;

  for (size_t k = 0; k < nack_list.size(); ++k) {
    int seq_num = nack_list[k];
    bool seq_num_matched = false;
    for (size_t n = 0; n < num_lost_packets; ++n) {
      if (seq_num == lost_sequence_numbers[n]) {
        seq_num_matched = true;
        break;
      }
    }
    if (!seq_num_matched)
      return false;
  }
  return true;
}

}  // namespace

TEST(NackTrackerTest, EmptyListWhenNoPacketLoss) {
  NackTracker nack(kDefaultNackListSize);
  nack.UpdateSampleRate(kSampleRateHz);

  int seq_num = 1;
  uint32_t timestamp = 0;

  std::vector<uint16_t> nack_list;
  for (int n = 0; n < 100; n++) {
    nack.UpdateLastReceivedPacket(seq_num, timestamp);
    nack_list = nack.GetNackList(kShortRoundTripTime);
    seq_num++;
    timestamp += kTimestampIncrement;
    nack_list = nack.GetNackList(kShortRoundTripTime);
    EXPECT_TRUE(nack_list.empty());
  }
}

TEST(NackTrackerTest, LatePacketsMovedToNackThenNackListDoesNotChange) {
  const uint16_t kSequenceNumberLostPackets[] = {2, 3, 4, 5, 6, 7, 8, 9};
  static const int kNumAllLostPackets = sizeof(kSequenceNumberLostPackets) /
                                        sizeof(kSequenceNumberLostPackets[0]);

  for (int k = 0; k < 2; k++) {  // Two iteration with/without wrap around.
    NackTracker nack(kDefaultNackListSize);
    nack.UpdateSampleRate(kSampleRateHz);

    uint16_t sequence_num_lost_packets[kNumAllLostPackets];
    for (int n = 0; n < kNumAllLostPackets; n++) {
      sequence_num_lost_packets[n] =
          kSequenceNumberLostPackets[n] +
          k * 65531;  // Have wrap around in sequence numbers for |k == 1|.
    }
    uint16_t seq_num = sequence_num_lost_packets[0] - 1;

    uint32_t timestamp = 0;
    std::vector<uint16_t> nack_list;

    nack.UpdateLastReceivedPacket(seq_num, timestamp);
    nack_list = nack.GetNackList(kShortRoundTripTime);
    EXPECT_TRUE(nack_list.empty());

    seq_num = sequence_num_lost_packets[kNumAllLostPackets - 1] + 1;
    timestamp += kTimestampIncrement * (kNumAllLostPackets + 1);
    int num_lost_packets = std::max(0, kNumAllLostPackets);

    nack.UpdateLastReceivedPacket(seq_num, timestamp);
    nack_list = nack.GetNackList(kShortRoundTripTime);
    EXPECT_TRUE(IsNackListCorrect(nack_list, sequence_num_lost_packets,
                                  num_lost_packets));
    seq_num++;
    timestamp += kTimestampIncrement;
    num_lost_packets++;

    for (int n = 0; n < 20; ++n) {
      nack.UpdateLastReceivedPacket(seq_num, timestamp);
      nack_list = nack.GetNackList(kShortRoundTripTime);
      EXPECT_TRUE(IsNackListCorrect(nack_list, sequence_num_lost_packets,
                                    kNumAllLostPackets));
      seq_num++;
      timestamp += kTimestampIncrement;
    }
  }
}

TEST(NackTrackerTest, ArrivedPacketsAreRemovedFromNackList) {
  const uint16_t kSequenceNumberLostPackets[] = {2, 3, 4, 5, 6, 7, 8, 9};
  static const int kNumAllLostPackets = sizeof(kSequenceNumberLostPackets) /
                                        sizeof(kSequenceNumberLostPackets[0]);

  for (int k = 0; k < 2; ++k) {  // Two iteration with/without wrap around.
    NackTracker nack(kDefaultNackListSize);
    nack.UpdateSampleRate(kSampleRateHz);

    uint16_t sequence_num_lost_packets[kNumAllLostPackets];
    for (int n = 0; n < kNumAllLostPackets; ++n) {
      sequence_num_lost_packets[n] = kSequenceNumberLostPackets[n] +
                                     k * 65531;  // Wrap around for |k == 1|.
    }

    uint16_t seq_num = sequence_num_lost_packets[0] - 1;
    uint32_t timestamp = 0;

    nack.UpdateLastReceivedPacket(seq_num, timestamp);
    std::vector<uint16_t> nack_list = nack.GetNackList(kShortRoundTripTime);
    EXPECT_TRUE(nack_list.empty());

    size_t index_retransmitted_rtp = 0;
    uint32_t timestamp_retransmitted_rtp = timestamp + kTimestampIncrement;

    seq_num = sequence_num_lost_packets[kNumAllLostPackets - 1] + 1;
    timestamp += kTimestampIncrement * (kNumAllLostPackets + 1);
    size_t num_lost_packets = kNumAllLostPackets;
    for (int n = 0; n < kNumAllLostPackets; ++n) {
      // Number of lost packets does not change for the first
      // |kNackThreshold + 1| packets, one is added to the list and one is
      // removed. Thereafter, the list shrinks every iteration.
      if (n >= 1)
        num_lost_packets--;

      nack.UpdateLastReceivedPacket(seq_num, timestamp);
      nack_list = nack.GetNackList(kShortRoundTripTime);
      EXPECT_TRUE(IsNackListCorrect(
          nack_list, &sequence_num_lost_packets[index_retransmitted_rtp],
          num_lost_packets));
      seq_num++;
      timestamp += kTimestampIncrement;

      // Retransmission of a lost RTP.
      nack.UpdateLastReceivedPacket(
          sequence_num_lost_packets[index_retransmitted_rtp],
          timestamp_retransmitted_rtp);
      index_retransmitted_rtp++;
      timestamp_retransmitted_rtp += kTimestampIncrement;

      nack_list = nack.GetNackList(kShortRoundTripTime);
      EXPECT_TRUE(IsNackListCorrect(
          nack_list, &sequence_num_lost_packets[index_retransmitted_rtp],
          num_lost_packets - 1));  // One less lost packet in the list.
    }
    ASSERT_TRUE(nack_list.empty());
  }
}

TEST(NackTrackerTest, Reset) {
  NackTracker nack(kDefaultNackListSize);
  nack.UpdateSampleRate(kSampleRateHz);

  // Two consecutive packets to have a correct estimate of timestamp increase.
  uint16_t seq_num = 0;
  nack.UpdateLastReceivedPacket(seq_num, seq_num * kTimestampIncrement);
  seq_num++;
  nack.UpdateLastReceivedPacket(seq_num, seq_num * kTimestampIncrement);

  // Skip 10 packets (larger than NACK threshold).
  const int kNumLostPackets = 10;
  seq_num += kNumLostPackets + 1;
  nack.UpdateLastReceivedPacket(seq_num, seq_num * kTimestampIncrement);

  const size_t kExpectedListSize = kNumLostPackets;
  std::vector<uint16_t> nack_list = nack.GetNackList(kShortRoundTripTime);
  EXPECT_EQ(kExpectedListSize, nack_list.size());

  nack.Reset();
  nack_list = nack.GetNackList(kShortRoundTripTime);
  EXPECT_TRUE(nack_list.empty());
}

TEST(NackTrackerTest, ListSizeAppliedFromBeginning) {
  const size_t kNackListSize = 10;
  for (int m = 0; m < 2; ++m) {
    uint16_t seq_num_offset = (m == 0) ? 0 : 65525;  // Wrap around if `m` is 1.
    NackTracker nack(kNackListSize);
    nack.UpdateSampleRate(kSampleRateHz);

    uint16_t seq_num = seq_num_offset;
    uint32_t timestamp = 0x12345678;
    nack.UpdateLastReceivedPacket(seq_num, timestamp);

    // Packet lost more than NACK-list size limit.
    uint16_t num_lost_packets = kNackListSize + 5;

    seq_num += num_lost_packets + 1;
    timestamp += (num_lost_packets + 1) * kTimestampIncrement;
    nack.UpdateLastReceivedPacket(seq_num, timestamp);

    std::vector<uint16_t> nack_list = nack.GetNackList(kShortRoundTripTime);
    EXPECT_EQ(kNackListSize, nack_list.size());
  }
}

TEST(NackTrackerTest, RoudTripTimeIsApplied) {
  const int kNackListSize = 200;
  NackTracker nack(kNackListSize);
  nack.UpdateSampleRate(kSampleRateHz);

  uint16_t seq_num = 0;
  uint32_t timestamp = 0x87654321;
  nack.UpdateLastReceivedPacket(seq_num, timestamp);

  // Packet lost more than NACK-list size limit.
  uint16_t kNumLostPackets = 5;

  seq_num += (1 + kNumLostPackets);
  timestamp += (1 + kNumLostPackets) * kTimestampIncrement;
  nack.UpdateLastReceivedPacket(seq_num, timestamp);

  // Expected time-to-play are:
  // kPacketSizeMs - 10, 2*kPacketSizeMs - 10, 3*kPacketSizeMs - 10, ...
  //
  // sequence number:  1,  2,  3,   4,   5
  // time-to-play:    20, 50, 80, 110, 140
  //
  std::vector<uint16_t> nack_list = nack.GetNackList(TimeDelta::Millis(930));
  EXPECT_THAT(nack_list, ElementsAre(4, 5));
}

TEST(NackTrackerTest, DoNotNackAfterDtx) {
  const int kNackListSize = 200;
  NackTracker nack(kNackListSize);
  nack.UpdateSampleRate(kSampleRateHz);
  uint16_t seq_num = 0;
  uint32_t timestamp = 0x87654321;
  nack.UpdateLastReceivedPacket(seq_num, timestamp);
  EXPECT_TRUE(nack.GetNackList(std::nullopt).empty());
  constexpr int kDtxPeriod = 400;
  nack.UpdateLastReceivedPacket(seq_num + 2,
                                timestamp + kDtxPeriod * kSampleRateHz / 1000);
  EXPECT_TRUE(nack.GetNackList(std::nullopt).empty());
}

TEST(NackTrackerTest, FixedDelayMode) {
  const int kNackListSize = 200;
  NackTracker nack(kNackListSize);
  nack.UpdateSampleRate(kSampleRateHz);
  uint16_t seq_num = 0;
  uint32_t timestamp = 0x87654321;
  nack.UpdateLastReceivedPacket(seq_num, timestamp);
  nack.UpdateLastReceivedPacket(seq_num + 2,
                                timestamp + 2 * kTimestampIncrement);
  EXPECT_THAT(nack.GetNackList(kShortRoundTripTime), ElementsAre(seq_num + 1));
  // The RTT is larger than the fixed delay, so no packets should be NACKed.
  EXPECT_THAT(nack.GetNackList(TimeDelta::Seconds(1)), IsEmpty());

  // Update the latest received packet such that the lost packet is older than
  // the fixed delay.
  nack.UpdateLastReceivedPacket(
      seq_num + 3, timestamp + 2 * kTimestampIncrement + kSampleRateHz);
  EXPECT_THAT(nack.GetNackList(kShortRoundTripTime), IsEmpty());
}

}  // namespace webrtc
