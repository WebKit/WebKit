/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <limits>
#include <memory>
#include <numeric>
#include <vector>

#include "webrtc/base/checks.h"
#include "webrtc/base/mod_ops.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"
#include "webrtc/voice_engine/transport_feedback_packet_loss_tracker.h"

using webrtc::rtcp::TransportFeedback;

namespace webrtc {

namespace {

class TransportFeedbackPacketLossTrackerTest
    : public ::testing::TestWithParam<uint16_t> {
 public:
  TransportFeedbackPacketLossTrackerTest() = default;
  virtual ~TransportFeedbackPacketLossTrackerTest() = default;

 protected:
  uint16_t base_{GetParam()};

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(TransportFeedbackPacketLossTrackerTest);
};

void SendPackets(TransportFeedbackPacketLossTracker* tracker,
                 const std::vector<uint16_t>& seq_nums,
                 bool validate_all = true) {
  for (uint16_t seq_num : seq_nums) {
    tracker->OnPacketAdded(seq_num);
    if (validate_all) {
      tracker->Validate();
    }
  }

  // We've either validated after each packet, or, for making sure the UT
  // doesn't run too long, we might validate only at the end of the range.
  if (!validate_all) {
    tracker->Validate();
  }
}

void SendPacketRange(TransportFeedbackPacketLossTracker* tracker,
                     uint16_t first_seq_num,
                     size_t num_of_seq_nums,
                     bool validate_all = true) {
  std::vector<uint16_t> seq_nums(num_of_seq_nums);
  std::iota(seq_nums.begin(), seq_nums.end(), first_seq_num);
  SendPackets(tracker, seq_nums, validate_all);
}

void AddTransportFeedbackAndValidate(
    TransportFeedbackPacketLossTracker* tracker,
    uint16_t base_sequence_num,
    const std::vector<bool>& reception_status_vec) {
  const int64_t kBaseTimeUs = 1234;  // Irrelevant to this test.
  TransportFeedback test_feedback;
  test_feedback.SetBase(base_sequence_num, kBaseTimeUs);
  uint16_t sequence_num = base_sequence_num;
  for (bool status : reception_status_vec) {
    if (status)
      test_feedback.AddReceivedPacket(sequence_num, kBaseTimeUs);
    ++sequence_num;
  }

  // TransportFeedback imposes some limitations on what constitutes a legal
  // status vector. For instance, the vector cannot terminate in a lost
  // packet. Make sure all limitations are abided by.
  RTC_CHECK_EQ(base_sequence_num, test_feedback.GetBaseSequence());
  const auto& vec = test_feedback.GetStatusVector();
  RTC_CHECK_EQ(reception_status_vec.size(), vec.size());
  for (size_t i = 0; i < reception_status_vec.size(); i++) {
    RTC_CHECK_EQ(reception_status_vec[i],
                 vec[i] != TransportFeedback::StatusSymbol::kNotReceived);
  }

  tracker->OnReceivedTransportFeedback(test_feedback);
  tracker->Validate();
}

// Checks that validty is as expected. If valid, checks also that
// value is as expected.
void ValidatePacketLossStatistics(
    const TransportFeedbackPacketLossTracker& tracker,
    rtc::Optional<float> expected_plr,
    rtc::Optional<float> expected_rplr) {
  // Comparing the rtc::Optional<float> directly would have given concise code,
  // but less readable error messages.
  rtc::Optional<float> plr = tracker.GetPacketLossRate();
  EXPECT_EQ(static_cast<bool>(expected_plr), static_cast<bool>(plr));
  if (expected_plr && plr) {
    EXPECT_EQ(*expected_plr, *plr);
  }

  rtc::Optional<float> rplr = tracker.GetRecoverablePacketLossRate();
  EXPECT_EQ(static_cast<bool>(expected_rplr), static_cast<bool>(rplr));
  if (expected_rplr && rplr) {
    EXPECT_EQ(*expected_rplr, *rplr);
  }
}

// Convenience function for when both are valid, and explicitly stating
// the rtc::Optional<float> constructor is just cumbersome.
void ValidatePacketLossStatistics(
    const TransportFeedbackPacketLossTracker& tracker,
    float expected_plr,
    float expected_rplr) {
  ValidatePacketLossStatistics(tracker,
                               rtc::Optional<float>(expected_plr),
                               rtc::Optional<float>(expected_rplr));
}

}  // namespace

// Sanity check on an empty window.
TEST(TransportFeedbackPacketLossTrackerTest, EmptyWindow) {
  TransportFeedbackPacketLossTracker tracker(10, 5, 5);

  // PLR and RPLR reported as unknown before reception of first feedback.
  ValidatePacketLossStatistics(tracker,
                               rtc::Optional<float>(),
                               rtc::Optional<float>());
}

// A feedback received for an empty window has no effect.
TEST_P(TransportFeedbackPacketLossTrackerTest, EmptyWindowFeedback) {
  TransportFeedbackPacketLossTracker tracker(3, 3, 2);

  // Feedback doesn't correspond to any packets - ignored.
  AddTransportFeedbackAndValidate(&tracker, base_, {true, false, true});
  ValidatePacketLossStatistics(tracker,
                               rtc::Optional<float>(),
                               rtc::Optional<float>());

  // After the packets are transmitted, acking them would have an effect.
  SendPacketRange(&tracker, base_, 3);
  AddTransportFeedbackAndValidate(&tracker, base_, {true, false, true});
  ValidatePacketLossStatistics(tracker, 1.0f / 3.0f, 0.5f);
}

// Sanity check on partially filled window.
TEST_P(TransportFeedbackPacketLossTrackerTest, PartiallyFilledWindow) {
  TransportFeedbackPacketLossTracker tracker(10, 5, 4);

  // PLR unknown before minimum window size reached.
  // RPLR unknown before minimum pairs reached.
  // Expected window contents: [] -> [1001].
  SendPacketRange(&tracker, base_, 3);
  AddTransportFeedbackAndValidate(&tracker, base_, {true, false, false, true});
  ValidatePacketLossStatistics(tracker,
                               rtc::Optional<float>(),
                               rtc::Optional<float>());
}

// Sanity check on minimum filled window - PLR known, RPLR unknown.
TEST_P(TransportFeedbackPacketLossTrackerTest, PlrMinimumFilledWindow) {
  TransportFeedbackPacketLossTracker tracker(10, 5, 5);

  // PLR correctly calculated after minimum window size reached.
  // RPLR not necessarily known at that time (not if min-pairs not reached).
  // Expected window contents: [] -> [10011].
  SendPacketRange(&tracker, base_, 5);
  AddTransportFeedbackAndValidate(&tracker, base_,
                                  {true, false, false, true, true});
  ValidatePacketLossStatistics(tracker,
                               rtc::Optional<float>(2.0f / 5.0f),
                               rtc::Optional<float>());
}

// Sanity check on minimum filled window - PLR unknown, RPLR known.
TEST_P(TransportFeedbackPacketLossTrackerTest, RplrMinimumFilledWindow) {
  TransportFeedbackPacketLossTracker tracker(10, 6, 4);

  // RPLR correctly calculated after minimum pairs reached.
  // PLR not necessarily known at that time (not if min window not reached).
  // Expected window contents: [] -> [10011].
  SendPacketRange(&tracker, base_, 5);
  AddTransportFeedbackAndValidate(&tracker, base_,
                                  {true, false, false, true, true});
  ValidatePacketLossStatistics(tracker,
                               rtc::Optional<float>(),
                               rtc::Optional<float>(1.0f / 4.0f));
}

// Additional reports update PLR and RPLR.
TEST_P(TransportFeedbackPacketLossTrackerTest, ExtendWindow) {
  TransportFeedbackPacketLossTracker tracker(20, 5, 5);

  SendPacketRange(&tracker, base_, 25);

  // Expected window contents: [] -> [10011].
  AddTransportFeedbackAndValidate(&tracker, base_,
                                  {true, false, false, true, true});
  ValidatePacketLossStatistics(tracker,
                               rtc::Optional<float>(2.0f / 5.0f),
                               rtc::Optional<float>());

  // Expected window contents: [10011] -> [1001110101].
  AddTransportFeedbackAndValidate(&tracker, base_ + 5,
                                  {true, false, true, false, true});
  ValidatePacketLossStatistics(tracker, 4.0f / 10.0f, 3.0f / 9.0f);

  // Expected window contents: [1001110101] -> [1001110101-GAP-10001].
  AddTransportFeedbackAndValidate(&tracker, base_ + 20,
                                  {true, false, false, false, true});
  ValidatePacketLossStatistics(tracker, 7.0f / 15.0f, 4.0f / 13.0f);
}

// Sanity - all packets correctly received.
TEST_P(TransportFeedbackPacketLossTrackerTest, AllReceived) {
  TransportFeedbackPacketLossTracker tracker(10, 5, 4);

  // Expected window contents: [] -> [11111].
  SendPacketRange(&tracker, base_, 5);
  AddTransportFeedbackAndValidate(&tracker, base_,
                                  {true, true, true, true, true});
  ValidatePacketLossStatistics(tracker, 0.0f, 0.0f);
}

// Sanity - all packets lost.
TEST_P(TransportFeedbackPacketLossTrackerTest, AllLost) {
  TransportFeedbackPacketLossTracker tracker(10, 5, 4);

  // Expected window contents: [] -> [00000].
  SendPacketRange(&tracker, base_, 5);
  // Note: Last acked packet (the received one) does not belong to the stream,
  // and is only there to make sure the feedback message is legal.
  AddTransportFeedbackAndValidate(&tracker, base_,
                                  {false, false, false, false, false, true});
  ValidatePacketLossStatistics(tracker, 1.0f, 0.0f);
}

// Repeated reports are ignored.
TEST_P(TransportFeedbackPacketLossTrackerTest, ReportRepetition) {
  TransportFeedbackPacketLossTracker tracker(10, 5, 4);

  SendPacketRange(&tracker, base_, 5);

  // Expected window contents: [] -> [10011].
  AddTransportFeedbackAndValidate(&tracker, base_,
                                  {true, false, false, true, true});
  ValidatePacketLossStatistics(tracker, 2.0f / 5.0f, 1.0f / 4.0f);

  // Repeat entire previous feedback
  // Expected window contents: [10011] -> [10011].
  AddTransportFeedbackAndValidate(&tracker, base_,
                                  {true, false, false, true, true});
  ValidatePacketLossStatistics(tracker, 2.0f / 5.0f, 1.0f / 4.0f);
}

// Report overlap.
TEST_P(TransportFeedbackPacketLossTrackerTest, ReportOverlap) {
  TransportFeedbackPacketLossTracker tracker(10, 5, 1);

  SendPacketRange(&tracker, base_, 15);

  // Expected window contents: [] -> [10011].
  AddTransportFeedbackAndValidate(&tracker, base_,
                                  {true, false, false, true, true});
  ValidatePacketLossStatistics(tracker, 2.0f / 5.0f, 1.0f / 4.0f);

  // Expected window contents: [10011] -> [1001101].
  AddTransportFeedbackAndValidate(&tracker, base_ + 3,
                                  {true, true, false, true});
  ValidatePacketLossStatistics(tracker, 3.0f / 7.0f, 2.0f / 6.0f);
}

// Report conflict.
TEST_P(TransportFeedbackPacketLossTrackerTest, ReportConflict) {
  TransportFeedbackPacketLossTracker tracker(10, 5, 4);

  SendPacketRange(&tracker, base_, 15);

  // Expected window contents: [] -> [01001].
  AddTransportFeedbackAndValidate(&tracker, base_,
                                  {false, true, false, false, true});
  ValidatePacketLossStatistics(tracker, 3.0f / 5.0f, 2.0f / 4.0f);

  // Expected window contents: [01001] -> [11101].
  // While false->true will be applied, true -> false will be ignored.
  AddTransportFeedbackAndValidate(&tracker, base_, {true, false, true});
  ValidatePacketLossStatistics(tracker, 1.0f / 5.0f, 1.0f / 4.0f);
}

// Skipped packets treated as unknown (not lost).
TEST_P(TransportFeedbackPacketLossTrackerTest, SkippedPackets) {
  TransportFeedbackPacketLossTracker tracker(10, 5, 1);

  SendPacketRange(&tracker, base_, 200);

  // Expected window contents: [] -> [10011].
  AddTransportFeedbackAndValidate(&tracker, base_,
                                  {true, false, false, true, true});
  ValidatePacketLossStatistics(tracker, 2.0f / 5.0f, 1.0f / 4.0f);

  // Expected window contents: [10011] -> [10011-GAP-101].
  AddTransportFeedbackAndValidate(&tracker, base_ + 100, {true, false, true});
  ValidatePacketLossStatistics(tracker, 3.0f / 8.0f, 2.0f / 6.0f);
}

// The window retains information up to the configured max-window-size, but
// starts discarding after that. (Sent packets are not counted.)
TEST_P(TransportFeedbackPacketLossTrackerTest, MaxWindowSize) {
  TransportFeedbackPacketLossTracker tracker(10, 10, 1);

  SendPacketRange(&tracker, base_, 200);

  // Up to max-window-size retained.
  // Expected window contents: [] -> [1010100001].
  AddTransportFeedbackAndValidate(
      &tracker, base_,
      {true, false, true, false, true, false, false, false, false, true});
  ValidatePacketLossStatistics(tracker, 6.0f / 10.0f, 3.0f / 9.0f);

  // After max-window-size, older entries discarded to accommodate newer ones.
  // Expected window contents: [1010100001] -> [0000110111].
  AddTransportFeedbackAndValidate(&tracker, base_ + 10,
                                  {true, false, true, true, true});
  ValidatePacketLossStatistics(tracker, 5.0f / 10.0f, 2.0f / 9.0f);
}

// Inserting a feedback into the middle of a full window works correctly.
TEST_P(TransportFeedbackPacketLossTrackerTest, InsertIntoMiddle) {
  TransportFeedbackPacketLossTracker tracker(10, 5, 1);

  SendPacketRange(&tracker, base_, 300);

  // Expected window contents: [] -> [10101].
  AddTransportFeedbackAndValidate(&tracker, base_,
                                  {true, false, true, false, true});
  ValidatePacketLossStatistics(tracker, 2.0f / 5.0f, 2.0f / 4.0f);

  // Expected window contents: [10101] -> [10101-GAP-10001].
  AddTransportFeedbackAndValidate(&tracker, base_ + 100,
                                  {true, false, false, false, true});
  ValidatePacketLossStatistics(tracker, 5.0f / 10.0f, 3.0f / 8.0f);

  // Insert into the middle of this full window - it discards the older data.
  // Expected window contents: [10101-GAP-10001] -> [11111-GAP-10001].
  AddTransportFeedbackAndValidate(&tracker, base_ + 50,
                                  {true, true, true, true, true});
  ValidatePacketLossStatistics(tracker, 3.0f / 10.0f, 1.0f / 8.0f);
}

// Inserting feedback into the middle of a full window works correctly - can
// complete two pairs.
TEST_P(TransportFeedbackPacketLossTrackerTest, InsertionCompletesTwoPairs) {
  TransportFeedbackPacketLossTracker tracker(15, 5, 1);

  SendPacketRange(&tracker, base_, 300);

  // Expected window contents: [] -> [10111].
  AddTransportFeedbackAndValidate(&tracker, base_,
                                  {true, false, true, true, true});
  ValidatePacketLossStatistics(tracker, 1.0f / 5.0f, 1.0f / 4.0f);

  // Expected window contents: [10111] -> [10111-GAP-10101].
  AddTransportFeedbackAndValidate(&tracker, base_ + 7,
                                  {true, false, true, false, true});
  ValidatePacketLossStatistics(tracker, 3.0f / 10.0f, 3.0f / 8.0f);

  // Insert in between, closing the gap completely.
  // Expected window contents: [10111-GAP-10101] -> [101111010101].
  AddTransportFeedbackAndValidate(&tracker, base_ + 5, {false, true});
  ValidatePacketLossStatistics(tracker, 4.0f / 12.0f, 4.0f / 11.0f);
}

// The window can meaningfully hold up to 0x8000 SENT packets (of which only
// up to max-window acked messages will be kept and regarded).
TEST_P(TransportFeedbackPacketLossTrackerTest, SecondQuadrant) {
  TransportFeedbackPacketLossTracker tracker(20, 5, 1);

  SendPacketRange(&tracker, base_, 0x8000, false);

  // Expected window contents: [] -> [10011].
  AddTransportFeedbackAndValidate(&tracker, base_,
                                  {true, false, false, true, true});
  ValidatePacketLossStatistics(tracker, 2.0f / 5.0f, 1.0f / 4.0f);

  // Window *does* get updated with inputs from quadrant #2.
  // Expected window contents: [10011] -> [100111].
  AddTransportFeedbackAndValidate(&tracker, base_ + 0x4321, {true});
  ValidatePacketLossStatistics(tracker, 2.0f / 6.0f, 1.0f / 4.0f);

  // Correct recognition of quadrant #2: up to, but not including, base_ +
  // 0x8000
  // Expected window contents: [100111] -> [1001111].
  AddTransportFeedbackAndValidate(&tracker, base_ + 0x7fff, {true});
  ValidatePacketLossStatistics(tracker, 2.0f / 7.0f, 1.0f / 4.0f);
}

// After the base has moved due to insertion into the third quadrant, it is
// still possible to get feedbacks in the middle of the window and obtain the
// correct PLR and RPLR. Insertion into the middle before the max window size
// has been achieved does not cause older packets to be dropped.
TEST_P(TransportFeedbackPacketLossTrackerTest, InsertIntoMiddleAfterBaseMoved) {
  TransportFeedbackPacketLossTracker tracker(20, 5, 1);

  SendPacketRange(&tracker, base_, 20);
  SendPacketRange(&tracker, base_ + 0x5000, 20);

  // Expected window contents: [] -> [1001101].
  AddTransportFeedbackAndValidate(
      &tracker, base_, {true, false, false, true, true, false, true});
  ValidatePacketLossStatistics(tracker, 3.0f / 7.0f, 2.0f / 6.0f);

  // Expected window contents: [1001101] -> [101-GAP-1001].
  SendPacketRange(&tracker, base_ + 0x8000, 4);
  AddTransportFeedbackAndValidate(&tracker, base_ + 0x8000,
                                  {true, false, false, true});
  ValidatePacketLossStatistics(tracker, 3.0f / 7.0f, 2.0f / 5.0f);

  // Inserting into the middle still works after the base has shifted.
  // Expected window contents:
  // [101-GAP-1001] -> [101-GAP-100101-GAP-1001]
  AddTransportFeedbackAndValidate(&tracker, base_ + 0x5000,
                                  {true, false, false, true, false, true});
  ValidatePacketLossStatistics(tracker, 6.0f / 13.0f, 4.0f / 10.0f);

  // The base can keep moving after inserting into the middle.
  // Expected window contents:
  // [101-GAP-100101-GAP-1001] -> [1-GAP-100101-GAP-100111].
  SendPacketRange(&tracker, base_ + 0x8000 + 4, 2);
  AddTransportFeedbackAndValidate(&tracker, base_ + 0x8000 + 4, {true, true});
  ValidatePacketLossStatistics(tracker, 5.0f / 13.0f, 3.0f / 10.0f);
}

// After moving the base of the window, the max window size is still observed.
TEST_P(TransportFeedbackPacketLossTrackerTest,
       MaxWindowObservedAfterBaseMoved) {
  TransportFeedbackPacketLossTracker tracker(15, 10, 1);

  // Expected window contents: [] -> [1001110101].
  SendPacketRange(&tracker, base_, 10);
  AddTransportFeedbackAndValidate(
      &tracker, base_,
      {true, false, false, true, true, true, false, true, false, true});
  ValidatePacketLossStatistics(tracker, 4.0f / 10.0f, 3.0f / 9.0f);

  // Create gap (on both sides).
  SendPacketRange(&tracker, base_ + 0x4000, 20);

  // Expected window contents: [1001110101] -> [1110101-GAP-101].
  SendPacketRange(&tracker, base_ + 0x8000, 3);
  AddTransportFeedbackAndValidate(&tracker, base_ + 0x8000,
                                  {true, false, true});
  ValidatePacketLossStatistics(tracker, 3.0f / 10.0f, 3.0f / 8.0f);

  // Push into middle until max window is reached. The gap is NOT completed.
  // Expected window contents:
  // [1110101-GAP-101] -> [1110101-GAP-10001-GAP-101]
  AddTransportFeedbackAndValidate(&tracker, base_ + 0x4000 + 2,
                                  {true, false, false, false, true});
  ValidatePacketLossStatistics(tracker, 6.0f / 15.0f, 4.0f / 12.0f);

  // Pushing new packets into the middle would discard older packets.
  // Expected window contents:
  // [1110101-GAP-10001-GAP-101] -> [0101-GAP-10001101-GAP-101]
  AddTransportFeedbackAndValidate(&tracker, base_ + 0x4000 + 2 + 5,
                                  {true, false, true});
  ValidatePacketLossStatistics(tracker, 7.0f / 15.0f, 5.0f / 12.0f);
}

// A packet with a new enough sequence number might shift enough old feedbacks
// out  window, that we'd go back to an unknown PLR and RPLR.
TEST_P(TransportFeedbackPacketLossTrackerTest, NewPacketMovesWindowBase) {
  TransportFeedbackPacketLossTracker tracker(20, 5, 3);

  SendPacketRange(&tracker, base_, 50);
  SendPacketRange(&tracker, base_ + 0x4000 - 1, 6);  // Gap

  // Expected window contents: [] -> [1001110101].
  AddTransportFeedbackAndValidate(
      &tracker, base_,
      {true, false, false, true, true, true, false, true, false, true});
  ValidatePacketLossStatistics(tracker, 4.0f / 10.0f, 3.0f / 9.0f);

  // A new sent packet with a new enough sequence number could shift enough
  // acked packets out of window, that we'd go back to an unknown PLR
  // and RPLR. This *doesn't* // necessarily mean all of the old ones
  // were discarded, though.
  // Expected window contents: [1001110101] -> [01].
  SendPacketRange(&tracker, base_ + 0x8006, 2);
  ValidatePacketLossStatistics(tracker,
                               rtc::Optional<float>(),  // Still invalid.
                               rtc::Optional<float>());

  // Even if those messages are acked, we'd still might be in unknown PLR
  // and RPLR, because we might have shifted more packets out of the window
  // than we have inserted.
  // Expected window contents: [01] -> [01-GAP-11].
  AddTransportFeedbackAndValidate(&tracker, base_ + 0x8006, {true, true});
  ValidatePacketLossStatistics(tracker,
                               rtc::Optional<float>(),  // Still invalid.
                               rtc::Optional<float>());

  // Inserting in the middle shows that though some of the elements were
  // ejected, some were retained.
  // Expected window contents: [01-GAP-11] -> [01-GAP-1001-GAP-11].
  AddTransportFeedbackAndValidate(&tracker, base_ + 0x4000,
                                  {true, false, false, true});
  ValidatePacketLossStatistics(tracker, 3.0f / 8.0f, 2.0f / 5.0f);
}

// Sequence number gaps are not gaps in reception. However, gaps in reception
// are still possible, if a packet which WAS sent on the stream is not acked.
TEST_P(TransportFeedbackPacketLossTrackerTest, SanityGapsInSequenceNumbers) {
  TransportFeedbackPacketLossTracker tracker(20, 5, 1);

  SendPackets(&tracker, {static_cast<uint16_t>(base_),
                         static_cast<uint16_t>(base_ + 2),
                         static_cast<uint16_t>(base_ + 4),
                         static_cast<uint16_t>(base_ + 6),
                         static_cast<uint16_t>(base_ + 8)});

  // Gaps in sequence numbers not considered as gaps in window, because  only
  // those sequence numbers which were associated with the stream count.
  // Expected window contents: [] -> [11011].
  AddTransportFeedbackAndValidate(
      // Note: Left packets belong to this stream, odd ones ignored.
      &tracker, base_, {true, false,
                        true, false,
                        false, false,
                        true, false,
                        true, true});
  ValidatePacketLossStatistics(tracker, 1.0f / 5.0f, 1.0f / 4.0f);

  // Create gap by sending [base + 10] but not acking it.
  // Note: Acks for [base + 11] and [base + 13] ignored (other stream).
  // Expected window contents: [11011] -> [11011-GAP-01].
  SendPackets(&tracker, {static_cast<uint16_t>(base_ + 10),
                         static_cast<uint16_t>(base_ + 12),
                         static_cast<uint16_t>(base_ + 14)});
  AddTransportFeedbackAndValidate(&tracker, base_ + 11,
                                  {false, false, false, true, true});
  ValidatePacketLossStatistics(tracker, 2.0f / 7.0f, 2.0f / 5.0f);
}

// Sending a packet which is less than 0x8000 away from the baseline does
// not move the window.
TEST_P(TransportFeedbackPacketLossTrackerTest,
       UnackedInWindowDoesNotMoveWindow) {
  TransportFeedbackPacketLossTracker tracker(5, 3, 1);

  // Baseline - window has acked messages.
  // Expected window contents: [] -> [10101].
  SendPacketRange(&tracker, base_, 5);
  AddTransportFeedbackAndValidate(&tracker, base_,
                                  {true, false, true, false, true});
  ValidatePacketLossStatistics(tracker, 2.0f / 5.0f, 2.0f / 4.0f);

  // Test - window not moved.
  // Expected window contents: [10101] -> [10101].
  SendPackets(&tracker, {static_cast<uint16_t>(base_ + 0x7fff)});
  ValidatePacketLossStatistics(tracker, 2.0f / 5.0f, 2.0f / 4.0f);
}

// Sending a packet which is at least 0x8000 away from the baseline, but not
// 0x8000 or more away from the newest packet in the window, moves the window,
// but does not reset it.
TEST_P(TransportFeedbackPacketLossTrackerTest, UnackedOutOfWindowMovesWindow) {
  TransportFeedbackPacketLossTracker tracker(5, 3, 1);

  // Baseline - window has acked messages.
  // Expected window contents: [] -> [10101].
  SendPacketRange(&tracker, base_, 5);
  AddTransportFeedbackAndValidate(&tracker, base_,
                                  {true, false, true, false, true});
  ValidatePacketLossStatistics(tracker, 2.0f / 5.0f, 2.0f / 4.0f);

  // 0x8000 from baseline, but only 0x7ffc from newest - window moved.
  // Expected window contents: [10101] -> [0101].
  SendPackets(&tracker, {static_cast<uint16_t>(base_ + 0x8000)});
  ValidatePacketLossStatistics(tracker, 2.0f / 4.0f, 2.0f / 3.0f);
}

// TODO(elad.alon): More tests possible here, but a CL is in the pipeline which
// significantly changes the entire class's operation (makes the window
// time-based), so no sense in writing complicated UTs which will be replaced
// very soon.

// The window is reset by the sending of a packet which is 0x8000 or more
// away from the newest packet.
TEST_P(TransportFeedbackPacketLossTrackerTest, WindowResetAfterLongNoSend) {
  TransportFeedbackPacketLossTracker tracker(10, 2, 1);

  // Baseline - window has acked messages.
  // Expected window contents: [] -> [1-GAP-10101].
  SendPacketRange(&tracker, base_, 1);
  SendPacketRange(&tracker, base_ + 0x7fff - 4, 5);
  AddTransportFeedbackAndValidate(&tracker, base_, {true});
  AddTransportFeedbackAndValidate(&tracker, base_ + 0x7fff - 4,
                                  {true, false, true, false, true});
  ValidatePacketLossStatistics(tracker, 2.0f / 6.0f, 2.0f / 5.0f);

  // Sent packet too new - the entire window is reset.
  // Expected window contents: [1-GAP-10101] -> [].
  SendPacketRange(&tracker, base_ + 0x7fff + 0x8000, 1);
  ValidatePacketLossStatistics(tracker,
                               rtc::Optional<float>(),
                               rtc::Optional<float>());

  // To show it was really reset, prove show that acking the sent packet
  // still leaves us at unknown, because no acked (or unacked) packets were
  // left in the window.
  // Expected window contents: [] -> [1].
  AddTransportFeedbackAndValidate(&tracker, base_ + 0x7fff + 0x8000, {true});
  ValidatePacketLossStatistics(tracker,
                               rtc::Optional<float>(),
                               rtc::Optional<float>());
}

// All tests are run multiple times with various baseline sequence number,
// to weed out potential bugs with wrap-around handling.
constexpr uint16_t kBases[] = {0x0000, 0x3456, 0xc032, 0xfffe};

INSTANTIATE_TEST_CASE_P(_,
                        TransportFeedbackPacketLossTrackerTest,
                        testing::ValuesIn(kBases));

}  // namespace webrtc
