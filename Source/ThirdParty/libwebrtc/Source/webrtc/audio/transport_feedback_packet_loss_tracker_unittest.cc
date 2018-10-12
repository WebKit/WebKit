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

#include "audio/transport_feedback_packet_loss_tracker.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "rtc_base/checks.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

constexpr int64_t kDefaultSendIntervalMs = 10;
constexpr int64_t kDefaultMaxWindowSizeMs = 500 * kDefaultSendIntervalMs;

class TransportFeedbackPacketLossTrackerTest
    : public ::testing::TestWithParam<uint16_t> {
 public:
  TransportFeedbackPacketLossTrackerTest() = default;
  virtual ~TransportFeedbackPacketLossTrackerTest() = default;

 protected:
  void SendPackets(TransportFeedbackPacketLossTracker* tracker,
                   const std::vector<uint16_t>& sequence_numbers,
                   int64_t send_time_interval_ms,
                   bool validate_all = true) {
    RTC_CHECK_GE(send_time_interval_ms, 0);
    for (uint16_t sequence_number : sequence_numbers) {
      tracker->OnPacketAdded(sequence_number, time_ms_);
      if (validate_all) {
        tracker->Validate();
      }
      time_ms_ += send_time_interval_ms;
    }

    // We've either validated after each packet, or, for making sure the UT
    // doesn't run too long, we might validate only at the end of the range.
    if (!validate_all) {
      tracker->Validate();
    }
  }

  void SendPackets(TransportFeedbackPacketLossTracker* tracker,
                   uint16_t first_seq_num,
                   size_t num_of_packets,
                   int64_t send_time_interval_ms,
                   bool validate_all = true) {
    RTC_CHECK_GE(send_time_interval_ms, 0);
    std::vector<uint16_t> sequence_numbers(num_of_packets);
    std::iota(sequence_numbers.begin(), sequence_numbers.end(), first_seq_num);
    SendPackets(tracker, sequence_numbers, send_time_interval_ms, validate_all);
  }

  void AdvanceClock(int64_t time_delta_ms) {
    RTC_CHECK_GT(time_delta_ms, 0);
    time_ms_ += time_delta_ms;
  }

  void AddTransportFeedbackAndValidate(
      TransportFeedbackPacketLossTracker* tracker,
      uint16_t base_sequence_num,
      const std::vector<bool>& reception_status_vec) {
    // Any positive integer signals reception. kNotReceived signals loss.
    // Other values are just illegal.
    constexpr int64_t kArrivalTimeMs = 1234;

    std::vector<PacketFeedback> packet_feedback_vector;
    uint16_t seq_num = base_sequence_num;
    for (bool received : reception_status_vec) {
      packet_feedback_vector.emplace_back(PacketFeedback(
          received ? kArrivalTimeMs : PacketFeedback::kNotReceived, seq_num));
      ++seq_num;
    }

    tracker->OnPacketFeedbackVector(packet_feedback_vector);
    tracker->Validate();
  }

  // Checks that validty is as expected. If valid, checks also that
  // value is as expected.
  void ValidatePacketLossStatistics(
      const TransportFeedbackPacketLossTracker& tracker,
      absl::optional<float> expected_plr,
      absl::optional<float> expected_rplr) {
    // TODO(eladalon): Comparing the absl::optional<float> directly would have
    // given concise code, but less readable error messages. If we modify
    // the way absl::optional is printed, we can get rid of this.
    absl::optional<float> plr = tracker.GetPacketLossRate();
    EXPECT_EQ(static_cast<bool>(expected_plr), static_cast<bool>(plr));
    if (expected_plr && plr) {
      EXPECT_EQ(*expected_plr, *plr);
    }

    absl::optional<float> rplr = tracker.GetRecoverablePacketLossRate();
    EXPECT_EQ(static_cast<bool>(expected_rplr), static_cast<bool>(rplr));
    if (expected_rplr && rplr) {
      EXPECT_EQ(*expected_rplr, *rplr);
    }
  }

  uint16_t base_{GetParam()};

 private:
  int64_t time_ms_{0};
};

}  // namespace

// Sanity check on an empty window.
TEST_P(TransportFeedbackPacketLossTrackerTest, EmptyWindow) {
  TransportFeedbackPacketLossTracker tracker(kDefaultMaxWindowSizeMs, 5, 5);

  // PLR and RPLR reported as unknown before reception of first feedback.
  ValidatePacketLossStatistics(tracker, absl::nullopt, absl::nullopt);
}

// A feedback received for an empty window has no effect.
TEST_P(TransportFeedbackPacketLossTrackerTest, EmptyWindowFeedback) {
  TransportFeedbackPacketLossTracker tracker(kDefaultMaxWindowSizeMs, 3, 2);

  // Feedback doesn't correspond to any packets - ignored.
  AddTransportFeedbackAndValidate(&tracker, base_, {true, false, true});
  ValidatePacketLossStatistics(tracker, absl::nullopt, absl::nullopt);

  // After the packets are transmitted, acking them would have an effect.
  SendPackets(&tracker, base_, 3, kDefaultSendIntervalMs);
  AddTransportFeedbackAndValidate(&tracker, base_, {true, false, true});
  ValidatePacketLossStatistics(tracker, 1.0f / 3.0f, 0.5f);
}

// Sanity check on partially filled window.
TEST_P(TransportFeedbackPacketLossTrackerTest, PartiallyFilledWindow) {
  TransportFeedbackPacketLossTracker tracker(kDefaultMaxWindowSizeMs, 5, 4);

  // PLR unknown before minimum window size reached.
  // RPLR unknown before minimum pairs reached.
  // Expected window contents: [] -> [1001].
  SendPackets(&tracker, base_, 3, kDefaultSendIntervalMs);
  AddTransportFeedbackAndValidate(&tracker, base_, {true, false, false, true});
  ValidatePacketLossStatistics(tracker, absl::nullopt, absl::nullopt);
}

// Sanity check on minimum filled window - PLR known, RPLR unknown.
TEST_P(TransportFeedbackPacketLossTrackerTest, PlrMinimumFilledWindow) {
  TransportFeedbackPacketLossTracker tracker(kDefaultMaxWindowSizeMs, 5, 5);

  // PLR correctly calculated after minimum window size reached.
  // RPLR not necessarily known at that time (not if min-pairs not reached).
  // Expected window contents: [] -> [10011].
  SendPackets(&tracker, base_, 5, kDefaultSendIntervalMs);
  AddTransportFeedbackAndValidate(&tracker, base_,
                                  {true, false, false, true, true});
  ValidatePacketLossStatistics(tracker, 2.0f / 5.0f, absl::nullopt);
}

// Sanity check on minimum filled window - PLR unknown, RPLR known.
TEST_P(TransportFeedbackPacketLossTrackerTest, RplrMinimumFilledWindow) {
  TransportFeedbackPacketLossTracker tracker(kDefaultMaxWindowSizeMs, 6, 4);

  // RPLR correctly calculated after minimum pairs reached.
  // PLR not necessarily known at that time (not if min window not reached).
  // Expected window contents: [] -> [10011].
  SendPackets(&tracker, base_, 5, kDefaultSendIntervalMs);
  AddTransportFeedbackAndValidate(&tracker, base_,
                                  {true, false, false, true, true});
  ValidatePacketLossStatistics(tracker, absl::nullopt, 1.0f / 4.0f);
}

// If packets are sent close enough together that the clock reading for both
// is the same, that's handled properly.
TEST_P(TransportFeedbackPacketLossTrackerTest, SameSentTime) {
  TransportFeedbackPacketLossTracker tracker(kDefaultMaxWindowSizeMs, 3, 2);

  // Expected window contents: [] -> [101].
  SendPackets(&tracker, base_, 3, 0);  // Note: time interval = 0ms.
  AddTransportFeedbackAndValidate(&tracker, base_, {true, false, true});

  ValidatePacketLossStatistics(tracker, 1.0f / 3.0f, 0.5f);
}

// Additional reports update PLR and RPLR.
TEST_P(TransportFeedbackPacketLossTrackerTest, ExtendWindow) {
  TransportFeedbackPacketLossTracker tracker(kDefaultMaxWindowSizeMs, 5, 5);

  SendPackets(&tracker, base_, 25, kDefaultSendIntervalMs);

  // Expected window contents: [] -> [10011].
  AddTransportFeedbackAndValidate(&tracker, base_,
                                  {true, false, false, true, true});
  ValidatePacketLossStatistics(tracker, 2.0f / 5.0f, absl::nullopt);

  // Expected window contents: [10011] -> [1001110101].
  AddTransportFeedbackAndValidate(&tracker, base_ + 5,
                                  {true, false, true, false, true});
  ValidatePacketLossStatistics(tracker, 4.0f / 10.0f, 3.0f / 9.0f);

  // Expected window contents: [1001110101] -> [1001110101-GAP-10001].
  AddTransportFeedbackAndValidate(&tracker, base_ + 20,
                                  {true, false, false, false, true});
  ValidatePacketLossStatistics(tracker, 7.0f / 15.0f, 4.0f / 13.0f);
}

// Correct calculation with different packet lengths.
TEST_P(TransportFeedbackPacketLossTrackerTest, DifferentSentIntervals) {
  TransportFeedbackPacketLossTracker tracker(kDefaultMaxWindowSizeMs, 5, 4);

  int64_t frames[] = {20, 60, 120, 20, 60};
  for (size_t i = 0; i < sizeof(frames) / sizeof(frames[0]); i++) {
    SendPackets(&tracker, {static_cast<uint16_t>(base_ + i)}, frames[i]);
  }

  // Expected window contents: [] -> [10011].
  AddTransportFeedbackAndValidate(&tracker, base_,
                                  {true, false, false, true, true});
  ValidatePacketLossStatistics(tracker, 2.0f / 5.0f, 1.0f / 4.0f);
}

// The window retains information up to sent times that exceed the the max
// window size. The oldest packets get shifted out of window to make room
// for the newer ones.
TEST_P(TransportFeedbackPacketLossTrackerTest, MaxWindowSize) {
  TransportFeedbackPacketLossTracker tracker(4 * kDefaultSendIntervalMs, 5, 1);

  SendPackets(&tracker, base_, 6, kDefaultSendIntervalMs, true);

  // Up to the maximum time-span retained (first + 4 * kDefaultSendIntervalMs).
  // Expected window contents: [] -> [01001].
  AddTransportFeedbackAndValidate(&tracker, base_,
                                  {false, true, false, false, true});
  ValidatePacketLossStatistics(tracker, 3.0f / 5.0f, 2.0f / 4.0f);

  // After the maximum time-span, older entries are discarded to accommodate
  // newer ones.
  // Expected window contents: [01001] -> [10011].
  AddTransportFeedbackAndValidate(&tracker, base_ + 5, {true});
  ValidatePacketLossStatistics(tracker, 2.0f / 5.0f, 1.0f / 4.0f);
}

// All packets received.
TEST_P(TransportFeedbackPacketLossTrackerTest, AllReceived) {
  TransportFeedbackPacketLossTracker tracker(kDefaultMaxWindowSizeMs, 5, 4);

  // Expected window contents: [] -> [11111].
  SendPackets(&tracker, base_, 5, kDefaultSendIntervalMs);
  AddTransportFeedbackAndValidate(&tracker, base_,
                                  {true, true, true, true, true});
  ValidatePacketLossStatistics(tracker, 0.0f, 0.0f);
}

// All packets lost.
TEST_P(TransportFeedbackPacketLossTrackerTest, AllLost) {
  TransportFeedbackPacketLossTracker tracker(kDefaultMaxWindowSizeMs, 5, 4);

  // Note: The last packet in the feedback does not belong to the stream.
  // It's only there because we're not allowed to end a feedback with a loss.
  // Expected window contents: [] -> [00000].
  SendPackets(&tracker, base_, 5, kDefaultSendIntervalMs);
  AddTransportFeedbackAndValidate(&tracker, base_,
                                  {false, false, false, false, false, true});
  ValidatePacketLossStatistics(tracker, 1.0f, 0.0f);
}

// Repeated reports are ignored.
TEST_P(TransportFeedbackPacketLossTrackerTest, ReportRepetition) {
  TransportFeedbackPacketLossTracker tracker(kDefaultMaxWindowSizeMs, 5, 4);

  SendPackets(&tracker, base_, 5, kDefaultSendIntervalMs);

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
  TransportFeedbackPacketLossTracker tracker(kDefaultMaxWindowSizeMs, 5, 1);

  SendPackets(&tracker, base_, 15, kDefaultSendIntervalMs);

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
  TransportFeedbackPacketLossTracker tracker(kDefaultMaxWindowSizeMs, 5, 4);

  SendPackets(&tracker, base_, 15, 10);

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
  TransportFeedbackPacketLossTracker tracker(200 * kDefaultSendIntervalMs, 5,
                                             1);

  SendPackets(&tracker, base_, 200, kDefaultSendIntervalMs);

  // Expected window contents: [] -> [10011].
  AddTransportFeedbackAndValidate(&tracker, base_,
                                  {true, false, false, true, true});
  ValidatePacketLossStatistics(tracker, 2.0f / 5.0f, 1.0f / 4.0f);

  // Expected window contents: [10011] -> [10011-GAP-101].
  AddTransportFeedbackAndValidate(&tracker, base_ + 100, {true, false, true});
  ValidatePacketLossStatistics(tracker, 3.0f / 8.0f, 2.0f / 6.0f);
}

// Moving a window, if it excludes some old acked messages, can leave
// in-window unacked messages intact, and ready to be used later.
TEST_P(TransportFeedbackPacketLossTrackerTest, MovedWindowRetainsRelevantInfo) {
  constexpr int64_t max_window_size_ms = 100;
  TransportFeedbackPacketLossTracker tracker(max_window_size_ms, 5, 1);

  // Note: All messages in this test are sent 1ms apart from each other.
  // Therefore, the delta in sequence numbers equals the timestamps delta.
  SendPackets(&tracker, base_, 4 * max_window_size_ms, 1);

  // Expected window contents: [] -> [10101].
  AddTransportFeedbackAndValidate(&tracker, base_,
                                  {true, false, true, false, true});
  ValidatePacketLossStatistics(tracker, 2.0f / 5.0f, 2.0f / 4.0f);

  // Expected window contents: [10101] -> [100011].
  const int64_t moved_oldest_acked = base_ + 2 * max_window_size_ms;
  const std::vector<bool> feedback = {true, false, false, false, true, true};
  AddTransportFeedbackAndValidate(&tracker, moved_oldest_acked, feedback);
  ValidatePacketLossStatistics(tracker, 3.0f / 6.0f, 1.0f / 5.0f);

  // Having acked |feedback.size()| starting with |moved_oldest_acked|, the
  // newest of the acked ones is now:
  const int64_t moved_newest_acked = moved_oldest_acked + feedback.size() - 1;

  // Messages that *are* more than the span-limit away from the newest
  // acked message *are* too old. Acking them would have no effect.
  AddTransportFeedbackAndValidate(
      &tracker, moved_newest_acked - max_window_size_ms - 1, {true});
  ValidatePacketLossStatistics(tracker, 3.0f / 6.0f, 1.0f / 5.0f);

  // Messages that are *not* more than the span-limit away from the newest
  // acked message are *not* too old. Acking them would have an effect.
  AddTransportFeedbackAndValidate(
      &tracker, moved_newest_acked - max_window_size_ms, {true});
  ValidatePacketLossStatistics(tracker, 3.0f / 7.0f, 1.0f / 5.0f);
}

// Inserting feedback into the middle of a window works correctly - can
// complete two pairs.
TEST_P(TransportFeedbackPacketLossTrackerTest, InsertionCompletesTwoPairs) {
  TransportFeedbackPacketLossTracker tracker(150 * kDefaultSendIntervalMs, 5,
                                             1);

  SendPackets(&tracker, base_, 15, kDefaultSendIntervalMs);

  // Expected window contents: [] -> [10111].
  AddTransportFeedbackAndValidate(&tracker, base_,
                                  {true, false, true, true, true});
  ValidatePacketLossStatistics(tracker, 1.0f / 5.0f, 1.0f / 4.0f);

  // Expected window contents: [10111] -> [10111-GAP-10101].
  AddTransportFeedbackAndValidate(&tracker, base_ + 7,
                                  {true, false, true, false, true});
  ValidatePacketLossStatistics(tracker, 3.0f / 10.0f, 3.0f / 8.0f);

  // Insert in between, closing the gap completely.
  // Expected window contents: [10111-GAP-10101] -> [101110110101].
  AddTransportFeedbackAndValidate(&tracker, base_ + 5, {false, true});
  ValidatePacketLossStatistics(tracker, 4.0f / 12.0f, 4.0f / 11.0f);
}

// Sequence number gaps are not gaps in reception. However, gaps in reception
// are still possible, if a packet which WAS sent on the stream is not acked.
TEST_P(TransportFeedbackPacketLossTrackerTest, SanityGapsInSequenceNumbers) {
  TransportFeedbackPacketLossTracker tracker(50 * kDefaultSendIntervalMs, 5, 1);

  SendPackets(
      &tracker,
      {static_cast<uint16_t>(base_), static_cast<uint16_t>(base_ + 2),
       static_cast<uint16_t>(base_ + 4), static_cast<uint16_t>(base_ + 6),
       static_cast<uint16_t>(base_ + 8)},
      kDefaultSendIntervalMs);

  // Gaps in sequence numbers not considered as gaps in window, because  only
  // those sequence numbers which were associated with the stream count.
  // Expected window contents: [] -> [11011].
  AddTransportFeedbackAndValidate(
      // Note: Left packets belong to this stream, right ones ignored.
      &tracker, base_,
      {true, false, true, false, false, false, true, false, true, true});
  ValidatePacketLossStatistics(tracker, 1.0f / 5.0f, 1.0f / 4.0f);

  // Create gap by sending [base + 10] but not acking it.
  // Note: Acks for [base + 11] and [base + 13] ignored (other stream).
  // Expected window contents: [11011] -> [11011-GAP-01].
  SendPackets(
      &tracker,
      {static_cast<uint16_t>(base_ + 10), static_cast<uint16_t>(base_ + 12),
       static_cast<uint16_t>(base_ + 14)},
      kDefaultSendIntervalMs);
  AddTransportFeedbackAndValidate(&tracker, base_ + 11,
                                  {false, false, false, true, true});
  ValidatePacketLossStatistics(tracker, 2.0f / 7.0f, 2.0f / 5.0f);
}

// The window cannot span more than 0x8000 in sequence numbers, regardless
// of time stamps and ack/unacked status.
TEST_P(TransportFeedbackPacketLossTrackerTest, MaxUnackedPackets) {
  TransportFeedbackPacketLossTracker tracker(0x10000, 4, 1);

  SendPackets(&tracker, base_, 0x2000, 1, false);

  // Expected window contents: [] -> [10011].
  AddTransportFeedbackAndValidate(&tracker, base_,
                                  {true, false, false, true, true});
  ValidatePacketLossStatistics(tracker, 2.0f / 5.0f, 1.0f / 4.0f);

  // Sending more unacked packets, up to 0x7fff from the base, does not
  // move the window or discard any information.
  SendPackets(&tracker, static_cast<uint16_t>(base_ + 0x8000 - 0x2000), 0x2000,
              1, false);
  ValidatePacketLossStatistics(tracker, 2.0f / 5.0f, 1.0f / 4.0f);

  // Sending more unacked packets, up to 0x7fff from the base, does not
  // move the window or discard any information.
  // Expected window contents: [10011] -> [0011].
  SendPackets(&tracker, static_cast<uint16_t>(base_ + 0x8000), 1, 1);
  ValidatePacketLossStatistics(tracker, 2.0f / 4.0f, 1.0f / 3.0f);
}

// The window holds acked packets up until the difference in timestamps between
// the oldest and newest reaches the configured maximum. Once this maximum
// is exceeded, old packets are shifted out of window until the maximum is
// once again observed.
TEST_P(TransportFeedbackPacketLossTrackerTest, TimeDifferenceMaximumObserved) {
  constexpr int64_t max_window_size_ms = 500;
  TransportFeedbackPacketLossTracker tracker(max_window_size_ms, 3, 1);

  // Note: All messages in this test are sent 1ms apart from each other.
  // Therefore, the delta in sequence numbers equals the timestamps delta.

  // Baseline - window has acked messages.
  // Expected window contents: [] -> [01101].
  const std::vector<bool> feedback = {false, true, true, false, true};
  SendPackets(&tracker, base_, feedback.size(), 1);
  AddTransportFeedbackAndValidate(&tracker, base_, feedback);
  ValidatePacketLossStatistics(tracker, 2.0f / 5.0f, 2.0f / 4.0f);

  // Test - window base not moved.
  // Expected window contents: [01101] -> [011011].
  AdvanceClock(max_window_size_ms - feedback.size());
  SendPackets(&tracker, static_cast<uint16_t>(base_ + feedback.size()), 1, 1);
  AddTransportFeedbackAndValidate(
      &tracker, static_cast<uint16_t>(base_ + feedback.size()), {true});
  ValidatePacketLossStatistics(tracker, 2.0f / 6.0f, 2.0f / 5.0f);

  // Another packet, sent 1ms later, would already be too late. The window will
  // be moved, but only after the ACK is received.
  const uint16_t new_packet_seq_num =
      static_cast<uint16_t>(base_ + feedback.size() + 1);
  SendPackets(&tracker, {new_packet_seq_num}, 1);
  ValidatePacketLossStatistics(tracker, 2.0f / 6.0f, 2.0f / 5.0f);
  // Expected window contents: [011011] -> [110111].
  AddTransportFeedbackAndValidate(&tracker, new_packet_seq_num, {true});
  ValidatePacketLossStatistics(tracker, 1.0f / 6.0f, 1.0f / 5.0f);
}

TEST_P(TransportFeedbackPacketLossTrackerTest, RepeatedSeqNumResetsWindow) {
  TransportFeedbackPacketLossTracker tracker(50 * kDefaultSendIntervalMs, 2, 1);

  // Baseline - window has acked messages.
  // Expected window contents: [] -> [01101].
  SendPackets(&tracker, base_, 5, kDefaultSendIntervalMs);
  AddTransportFeedbackAndValidate(&tracker, base_,
                                  {false, true, true, false, true});
  ValidatePacketLossStatistics(tracker, 2.0f / 5.0f, 2.0f / 4.0f);

  // A reset occurs.
  SendPackets(&tracker, {static_cast<uint16_t>(base_ + 2)},
              kDefaultSendIntervalMs);
  ValidatePacketLossStatistics(tracker, absl::nullopt, absl::nullopt);
}

// The window is reset by the sending of a packet which is 0x8000 or more
// away from the newest packet acked/unacked packet.
TEST_P(TransportFeedbackPacketLossTrackerTest,
       SendAfterLongSuspensionResetsWindow) {
  TransportFeedbackPacketLossTracker tracker(50 * kDefaultSendIntervalMs, 2, 1);

  // Baseline - window has acked messages.
  // Expected window contents: [] -> [01101].
  SendPackets(&tracker, base_, 5, kDefaultSendIntervalMs);
  AddTransportFeedbackAndValidate(&tracker, base_,
                                  {false, true, true, false, true});
  ValidatePacketLossStatistics(tracker, 2.0f / 5.0f, 2.0f / 4.0f);

  // A reset occurs.
  SendPackets(&tracker, {static_cast<uint16_t>(base_ + 5 + 0x8000)},
              kDefaultSendIntervalMs);
  ValidatePacketLossStatistics(tracker, absl::nullopt, absl::nullopt);
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST(TransportFeedbackPacketLossTrackerTest, InvalidConfigMaxWindowSize) {
  EXPECT_DEATH(TransportFeedbackPacketLossTracker tracker(0, 20, 10), "");
}

TEST(TransportFeedbackPacketLossTrackerTest, InvalidConfigPlrMinAcked) {
  EXPECT_DEATH(TransportFeedbackPacketLossTracker tracker(5000, 0, 10), "");
}

TEST(TransportFeedbackPacketLossTrackerTest, InvalidConfigRplrMinPairs) {
  EXPECT_DEATH(TransportFeedbackPacketLossTracker tracker(5000, 20, 0), "");
}

TEST(TransportFeedbackPacketLossTrackerTest, TimeCantFlowBackwards) {
  TransportFeedbackPacketLossTracker tracker(5000, 2, 1);
  tracker.OnPacketAdded(100, 0);
  tracker.OnPacketAdded(101, 2);
  EXPECT_DEATH(tracker.OnPacketAdded(102, 1), "");
}
#endif

// All tests are run multiple times with various baseline sequence number,
// to weed out potential bugs with wrap-around handling.
constexpr uint16_t kBases[] = {0x0000, 0x3456, 0xc032, 0xfffe};

INSTANTIATE_TEST_CASE_P(_,
                        TransportFeedbackPacketLossTrackerTest,
                        testing::ValuesIn(kBases));

}  // namespace webrtc
