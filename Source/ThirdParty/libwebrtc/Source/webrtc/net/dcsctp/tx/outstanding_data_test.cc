/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/tx/outstanding_data.h"

#include <vector>

#include "absl/types/optional.h"
#include "net/dcsctp/common/math.h"
#include "net/dcsctp/common/sequence_numbers.h"
#include "net/dcsctp/packet/chunk/data_chunk.h"
#include "net/dcsctp/packet/chunk/forward_tsn_chunk.h"
#include "net/dcsctp/public/types.h"
#include "net/dcsctp/testing/data_generator.h"
#include "net/dcsctp/testing/testing_macros.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace dcsctp {
namespace {
using ::testing::MockFunction;
using State = ::dcsctp::OutstandingData::State;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::Return;
using ::testing::StrictMock;

constexpr TimeMs kNow(42);

class OutstandingDataTest : public testing::Test {
 protected:
  OutstandingDataTest()
      : gen_(MID(42)),
        buf_(DataChunk::kHeaderSize,
             unwrapper_.Unwrap(TSN(10)),
             unwrapper_.Unwrap(TSN(9)),
             on_discard_.AsStdFunction()) {}

  UnwrappedTSN::Unwrapper unwrapper_;
  DataGenerator gen_;
  StrictMock<MockFunction<bool(IsUnordered, StreamID, MID)>> on_discard_;
  OutstandingData buf_;
};

TEST_F(OutstandingDataTest, HasInitialState) {
  EXPECT_TRUE(buf_.empty());
  EXPECT_EQ(buf_.outstanding_bytes(), 0u);
  EXPECT_EQ(buf_.outstanding_items(), 0u);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());
  EXPECT_EQ(buf_.last_cumulative_tsn_ack().Wrap(), TSN(9));
  EXPECT_EQ(buf_.next_tsn().Wrap(), TSN(10));
  EXPECT_EQ(buf_.highest_outstanding_tsn().Wrap(), TSN(9));
  EXPECT_THAT(buf_.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked)));
  EXPECT_FALSE(buf_.ShouldSendForwardTsn());
}

TEST_F(OutstandingDataTest, InsertChunk) {
  ASSERT_HAS_VALUE_AND_ASSIGN(UnwrappedTSN tsn,
                              buf_.Insert(gen_.Ordered({1}, "BE"), kNow));

  EXPECT_EQ(tsn.Wrap(), TSN(10));

  EXPECT_EQ(buf_.outstanding_bytes(), DataChunk::kHeaderSize + RoundUpTo4(1));
  EXPECT_EQ(buf_.outstanding_items(), 1u);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());
  EXPECT_EQ(buf_.last_cumulative_tsn_ack().Wrap(), TSN(9));
  EXPECT_EQ(buf_.next_tsn().Wrap(), TSN(11));
  EXPECT_EQ(buf_.highest_outstanding_tsn().Wrap(), TSN(10));
  EXPECT_THAT(buf_.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),
                          Pair(TSN(10), State::kInFlight)));
}

TEST_F(OutstandingDataTest, AcksSingleChunk) {
  buf_.Insert(gen_.Ordered({1}, "BE"), kNow);
  OutstandingData::AckInfo ack =
      buf_.HandleSack(unwrapper_.Unwrap(TSN(10)), {}, false);

  EXPECT_EQ(ack.bytes_acked, DataChunk::kHeaderSize + RoundUpTo4(1));
  EXPECT_EQ(ack.highest_tsn_acked.Wrap(), TSN(10));
  EXPECT_FALSE(ack.has_packet_loss);

  EXPECT_EQ(buf_.outstanding_bytes(), 0u);
  EXPECT_EQ(buf_.outstanding_items(), 0u);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());
  EXPECT_EQ(buf_.last_cumulative_tsn_ack().Wrap(), TSN(10));
  EXPECT_EQ(buf_.next_tsn().Wrap(), TSN(11));
  EXPECT_EQ(buf_.highest_outstanding_tsn().Wrap(), TSN(10));
  EXPECT_THAT(buf_.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(10), State::kAcked)));
}

TEST_F(OutstandingDataTest, AcksPreviousChunkDoesntUpdate) {
  buf_.Insert(gen_.Ordered({1}, "BE"), kNow);
  buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), {}, false);

  EXPECT_EQ(buf_.outstanding_bytes(), DataChunk::kHeaderSize + RoundUpTo4(1));
  EXPECT_EQ(buf_.outstanding_items(), 1u);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());
  EXPECT_EQ(buf_.last_cumulative_tsn_ack().Wrap(), TSN(9));
  EXPECT_EQ(buf_.next_tsn().Wrap(), TSN(11));
  EXPECT_EQ(buf_.highest_outstanding_tsn().Wrap(), TSN(10));
  EXPECT_THAT(buf_.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),
                          Pair(TSN(10), State::kInFlight)));
}

TEST_F(OutstandingDataTest, AcksAndNacksWithGapAckBlocks) {
  buf_.Insert(gen_.Ordered({1}, "B"), kNow);
  buf_.Insert(gen_.Ordered({1}, "E"), kNow);

  std::vector<SackChunk::GapAckBlock> gab = {SackChunk::GapAckBlock(2, 2)};
  OutstandingData::AckInfo ack =
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab, false);
  EXPECT_EQ(ack.bytes_acked, DataChunk::kHeaderSize + RoundUpTo4(1));
  EXPECT_EQ(ack.highest_tsn_acked.Wrap(), TSN(11));
  EXPECT_FALSE(ack.has_packet_loss);

  EXPECT_EQ(buf_.outstanding_bytes(), 0u);
  EXPECT_EQ(buf_.outstanding_items(), 0u);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());
  EXPECT_EQ(buf_.last_cumulative_tsn_ack().Wrap(), TSN(9));
  EXPECT_EQ(buf_.next_tsn().Wrap(), TSN(12));
  EXPECT_EQ(buf_.highest_outstanding_tsn().Wrap(), TSN(11));
  EXPECT_THAT(buf_.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),    //
                          Pair(TSN(10), State::kNacked),  //
                          Pair(TSN(11), State::kAcked)));
}

TEST_F(OutstandingDataTest, NacksThreeTimesWithSameTsnDoesntRetransmit) {
  buf_.Insert(gen_.Ordered({1}, "B"), kNow);
  buf_.Insert(gen_.Ordered({1}, "E"), kNow);

  std::vector<SackChunk::GapAckBlock> gab1 = {SackChunk::GapAckBlock(2, 2)};
  EXPECT_FALSE(
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab1, false).has_packet_loss);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());

  EXPECT_FALSE(
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab1, false).has_packet_loss);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());

  EXPECT_FALSE(
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab1, false).has_packet_loss);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());

  EXPECT_THAT(buf_.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),    //
                          Pair(TSN(10), State::kNacked),  //
                          Pair(TSN(11), State::kAcked)));
}

TEST_F(OutstandingDataTest, NacksThreeTimesResultsInRetransmission) {
  buf_.Insert(gen_.Ordered({1}, "B"), kNow);
  buf_.Insert(gen_.Ordered({1}, ""), kNow);
  buf_.Insert(gen_.Ordered({1}, ""), kNow);
  buf_.Insert(gen_.Ordered({1}, "E"), kNow);

  std::vector<SackChunk::GapAckBlock> gab1 = {SackChunk::GapAckBlock(2, 2)};
  EXPECT_FALSE(
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab1, false).has_packet_loss);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());

  std::vector<SackChunk::GapAckBlock> gab2 = {SackChunk::GapAckBlock(2, 3)};
  EXPECT_FALSE(
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab2, false).has_packet_loss);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());

  std::vector<SackChunk::GapAckBlock> gab3 = {SackChunk::GapAckBlock(2, 4)};
  OutstandingData::AckInfo ack =
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab3, false);
  EXPECT_EQ(ack.bytes_acked, DataChunk::kHeaderSize + RoundUpTo4(1));
  EXPECT_EQ(ack.highest_tsn_acked.Wrap(), TSN(13));
  EXPECT_TRUE(ack.has_packet_loss);

  EXPECT_TRUE(buf_.has_data_to_be_retransmitted());

  EXPECT_THAT(buf_.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),               //
                          Pair(TSN(10), State::kToBeRetransmitted),  //
                          Pair(TSN(11), State::kAcked),              //
                          Pair(TSN(12), State::kAcked),              //
                          Pair(TSN(13), State::kAcked)));

  EXPECT_THAT(buf_.GetChunksToBeFastRetransmitted(1000),
              ElementsAre(Pair(TSN(10), _)));
  EXPECT_THAT(buf_.GetChunksToBeRetransmitted(1000), IsEmpty());
}

TEST_F(OutstandingDataTest, NacksThreeTimesResultsInAbandoning) {
  static constexpr MaxRetransmits kMaxRetransmissions(0);
  buf_.Insert(gen_.Ordered({1}, "B"), kNow, kMaxRetransmissions);
  buf_.Insert(gen_.Ordered({1}, ""), kNow, kMaxRetransmissions);
  buf_.Insert(gen_.Ordered({1}, ""), kNow, kMaxRetransmissions);
  buf_.Insert(gen_.Ordered({1}, "E"), kNow, kMaxRetransmissions);

  std::vector<SackChunk::GapAckBlock> gab1 = {SackChunk::GapAckBlock(2, 2)};
  EXPECT_FALSE(
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab1, false).has_packet_loss);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());

  std::vector<SackChunk::GapAckBlock> gab2 = {SackChunk::GapAckBlock(2, 3)};
  EXPECT_FALSE(
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab2, false).has_packet_loss);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());

  EXPECT_CALL(on_discard_, Call(IsUnordered(false), StreamID(1), MID(42)))
      .WillOnce(Return(false));
  std::vector<SackChunk::GapAckBlock> gab3 = {SackChunk::GapAckBlock(2, 4)};
  OutstandingData::AckInfo ack =
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab3, false);
  EXPECT_EQ(ack.bytes_acked, DataChunk::kHeaderSize + RoundUpTo4(1));
  EXPECT_EQ(ack.highest_tsn_acked.Wrap(), TSN(13));
  EXPECT_TRUE(ack.has_packet_loss);

  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());
  EXPECT_EQ(buf_.next_tsn().Wrap(), TSN(14));
  EXPECT_THAT(buf_.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),       //
                          Pair(TSN(10), State::kAbandoned),  //
                          Pair(TSN(11), State::kAbandoned),  //
                          Pair(TSN(12), State::kAbandoned),  //
                          Pair(TSN(13), State::kAbandoned)));
}

TEST_F(OutstandingDataTest, NacksThreeTimesResultsInAbandoningWithPlaceholder) {
  static constexpr MaxRetransmits kMaxRetransmissions(0);
  buf_.Insert(gen_.Ordered({1}, "B"), kNow, kMaxRetransmissions);
  buf_.Insert(gen_.Ordered({1}, ""), kNow, kMaxRetransmissions);
  buf_.Insert(gen_.Ordered({1}, ""), kNow, kMaxRetransmissions);
  buf_.Insert(gen_.Ordered({1}, ""), kNow, kMaxRetransmissions);

  std::vector<SackChunk::GapAckBlock> gab1 = {SackChunk::GapAckBlock(2, 2)};
  EXPECT_FALSE(
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab1, false).has_packet_loss);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());

  std::vector<SackChunk::GapAckBlock> gab2 = {SackChunk::GapAckBlock(2, 3)};
  EXPECT_FALSE(
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab2, false).has_packet_loss);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());

  EXPECT_CALL(on_discard_, Call(IsUnordered(false), StreamID(1), MID(42)))
      .WillOnce(Return(true));
  std::vector<SackChunk::GapAckBlock> gab3 = {SackChunk::GapAckBlock(2, 4)};
  OutstandingData::AckInfo ack =
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab3, false);
  EXPECT_EQ(ack.bytes_acked, DataChunk::kHeaderSize + RoundUpTo4(1));
  EXPECT_EQ(ack.highest_tsn_acked.Wrap(), TSN(13));
  EXPECT_TRUE(ack.has_packet_loss);

  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());
  EXPECT_EQ(buf_.next_tsn().Wrap(), TSN(15));
  EXPECT_THAT(buf_.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),       //
                          Pair(TSN(10), State::kAbandoned),  //
                          Pair(TSN(11), State::kAbandoned),  //
                          Pair(TSN(12), State::kAbandoned),  //
                          Pair(TSN(13), State::kAbandoned),  //
                          Pair(TSN(14), State::kAbandoned)));
}

TEST_F(OutstandingDataTest, ExpiresChunkBeforeItIsInserted) {
  static constexpr TimeMs kExpiresAt = kNow + DurationMs(1);
  EXPECT_TRUE(buf_.Insert(gen_.Ordered({1}, "B"), kNow,
                          MaxRetransmits::NoLimit(), kExpiresAt)
                  .has_value());
  EXPECT_TRUE(buf_.Insert(gen_.Ordered({1}, ""), kNow + DurationMs(0),
                          MaxRetransmits::NoLimit(), kExpiresAt)
                  .has_value());

  EXPECT_CALL(on_discard_, Call(IsUnordered(false), StreamID(1), MID(42)))
      .WillOnce(Return(false));
  EXPECT_FALSE(buf_.Insert(gen_.Ordered({1}, "E"), kNow + DurationMs(1),
                           MaxRetransmits::NoLimit(), kExpiresAt)
                   .has_value());

  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());
  EXPECT_EQ(buf_.last_cumulative_tsn_ack().Wrap(), TSN(9));
  EXPECT_EQ(buf_.next_tsn().Wrap(), TSN(13));
  EXPECT_EQ(buf_.highest_outstanding_tsn().Wrap(), TSN(12));
  EXPECT_THAT(buf_.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),       //
                          Pair(TSN(10), State::kAbandoned),  //
                          Pair(TSN(11), State::kAbandoned),
                          Pair(TSN(12), State::kAbandoned)));
}

TEST_F(OutstandingDataTest, CanGenerateForwardTsn) {
  static constexpr MaxRetransmits kMaxRetransmissions(0);
  buf_.Insert(gen_.Ordered({1}, "B"), kNow, kMaxRetransmissions);
  buf_.Insert(gen_.Ordered({1}, ""), kNow, kMaxRetransmissions);
  buf_.Insert(gen_.Ordered({1}, "E"), kNow, kMaxRetransmissions);

  EXPECT_CALL(on_discard_, Call(IsUnordered(false), StreamID(1), MID(42)))
      .WillOnce(Return(false));
  buf_.NackAll();

  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());
  EXPECT_THAT(buf_.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),       //
                          Pair(TSN(10), State::kAbandoned),  //
                          Pair(TSN(11), State::kAbandoned),
                          Pair(TSN(12), State::kAbandoned)));

  EXPECT_TRUE(buf_.ShouldSendForwardTsn());
  ForwardTsnChunk chunk = buf_.CreateForwardTsn();
  EXPECT_EQ(chunk.new_cumulative_tsn(), TSN(12));
}

TEST_F(OutstandingDataTest, AckWithGapBlocksFromRFC4960Section334) {
  buf_.Insert(gen_.Ordered({1}, "B"), kNow);
  buf_.Insert(gen_.Ordered({1}, ""), kNow);
  buf_.Insert(gen_.Ordered({1}, ""), kNow);
  buf_.Insert(gen_.Ordered({1}, ""), kNow);
  buf_.Insert(gen_.Ordered({1}, ""), kNow);
  buf_.Insert(gen_.Ordered({1}, ""), kNow);
  buf_.Insert(gen_.Ordered({1}, ""), kNow);
  buf_.Insert(gen_.Ordered({1}, "E"), kNow);

  EXPECT_THAT(buf_.GetChunkStatesForTesting(),
              testing::ElementsAre(Pair(TSN(9), State::kAcked),      //
                                   Pair(TSN(10), State::kInFlight),  //
                                   Pair(TSN(11), State::kInFlight),  //
                                   Pair(TSN(12), State::kInFlight),  //
                                   Pair(TSN(13), State::kInFlight),  //
                                   Pair(TSN(14), State::kInFlight),  //
                                   Pair(TSN(15), State::kInFlight),  //
                                   Pair(TSN(16), State::kInFlight),  //
                                   Pair(TSN(17), State::kInFlight)));

  std::vector<SackChunk::GapAckBlock> gab = {SackChunk::GapAckBlock(2, 3),
                                             SackChunk::GapAckBlock(5, 5)};
  buf_.HandleSack(unwrapper_.Unwrap(TSN(12)), gab, false);

  EXPECT_THAT(buf_.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(12), State::kAcked),   //
                          Pair(TSN(13), State::kNacked),  //
                          Pair(TSN(14), State::kAcked),   //
                          Pair(TSN(15), State::kAcked),   //
                          Pair(TSN(16), State::kNacked),  //
                          Pair(TSN(17), State::kAcked)));
}

TEST_F(OutstandingDataTest, MeasureRTT) {
  buf_.Insert(gen_.Ordered({1}, "BE"), kNow);
  buf_.Insert(gen_.Ordered({1}, "BE"), kNow + DurationMs(1));
  buf_.Insert(gen_.Ordered({1}, "BE"), kNow + DurationMs(2));

  static constexpr DurationMs kDuration(123);
  ASSERT_HAS_VALUE_AND_ASSIGN(
      DurationMs duration,
      buf_.MeasureRTT(kNow + kDuration, unwrapper_.Unwrap(TSN(11))));

  EXPECT_EQ(duration, kDuration - DurationMs(1));
}

TEST_F(OutstandingDataTest, MustRetransmitBeforeGettingNackedAgain) {
  // This test case verifies that a chunk that has been nacked, and scheduled to
  // be retransmitted, doesn't get nacked again until it has been actually sent
  // on the wire.

  static constexpr MaxRetransmits kOneRetransmission(1);
  for (int tsn = 10; tsn <= 20; ++tsn) {
    buf_.Insert(gen_.Ordered({1}, tsn == 10   ? "B"
                                  : tsn == 20 ? "E"
                                              : ""),
                kNow, kOneRetransmission);
  }

  std::vector<SackChunk::GapAckBlock> gab1 = {SackChunk::GapAckBlock(2, 2)};
  EXPECT_FALSE(
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab1, false).has_packet_loss);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());

  std::vector<SackChunk::GapAckBlock> gab2 = {SackChunk::GapAckBlock(2, 3)};
  EXPECT_FALSE(
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab2, false).has_packet_loss);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());

  std::vector<SackChunk::GapAckBlock> gab3 = {SackChunk::GapAckBlock(2, 4)};
  OutstandingData::AckInfo ack =
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab3, false);
  EXPECT_TRUE(ack.has_packet_loss);
  EXPECT_TRUE(buf_.has_data_to_be_retransmitted());

  // Don't call GetChunksToBeRetransmitted yet - simulate that the congestion
  // window doesn't allow it to be retransmitted yet. It does however get more
  // SACKs indicating packet loss.

  std::vector<SackChunk::GapAckBlock> gab4 = {SackChunk::GapAckBlock(2, 5)};
  EXPECT_FALSE(
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab4, false).has_packet_loss);
  EXPECT_TRUE(buf_.has_data_to_be_retransmitted());

  std::vector<SackChunk::GapAckBlock> gab5 = {SackChunk::GapAckBlock(2, 6)};
  EXPECT_FALSE(
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab5, false).has_packet_loss);
  EXPECT_TRUE(buf_.has_data_to_be_retransmitted());

  std::vector<SackChunk::GapAckBlock> gab6 = {SackChunk::GapAckBlock(2, 7)};
  OutstandingData::AckInfo ack2 =
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab6, false);

  EXPECT_FALSE(ack2.has_packet_loss);
  EXPECT_TRUE(buf_.has_data_to_be_retransmitted());

  // Now it's retransmitted.
  EXPECT_THAT(buf_.GetChunksToBeFastRetransmitted(1000),
              ElementsAre(Pair(TSN(10), _)));
  EXPECT_THAT(buf_.GetChunksToBeRetransmitted(1000), IsEmpty());

  // And obviously lost, as it will get NACKed and abandoned.
  std::vector<SackChunk::GapAckBlock> gab7 = {SackChunk::GapAckBlock(2, 8)};
  EXPECT_FALSE(
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab7, false).has_packet_loss);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());

  std::vector<SackChunk::GapAckBlock> gab8 = {SackChunk::GapAckBlock(2, 9)};
  EXPECT_FALSE(
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab8, false).has_packet_loss);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());

  EXPECT_CALL(on_discard_, Call(IsUnordered(false), StreamID(1), MID(42)))
      .WillOnce(Return(false));

  std::vector<SackChunk::GapAckBlock> gab9 = {SackChunk::GapAckBlock(2, 10)};
  OutstandingData::AckInfo ack3 =
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab9, false);

  EXPECT_TRUE(ack3.has_packet_loss);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());
}

TEST_F(OutstandingDataTest, CanAbandonChunksMarkedForFastRetransmit) {
  // This test is a bit convoluted, and can't really happen with a well behaving
  // client, but this was found by fuzzers. This test will verify that a message
  // that was both marked as "to be fast retransmitted" and "abandoned" at the
  // same time doesn't cause any consistency issues.

  // Add chunks 10-14, but chunk 11 has zero retransmissions. When chunk 10 and
  // 11 are NACKed three times, chunk 10 will be marked for retransmission, but
  // chunk 11 will be abandoned, which also abandons chunk 10, as it's part of
  // the same message.
  buf_.Insert(gen_.Ordered({1}, "B"), kNow);                    // 10
  buf_.Insert(gen_.Ordered({1}, ""), kNow, MaxRetransmits(0));  // 11
  buf_.Insert(gen_.Ordered({1}, ""), kNow);                     // 12
  buf_.Insert(gen_.Ordered({1}, ""), kNow);                     // 13
  buf_.Insert(gen_.Ordered({1}, "E"), kNow);                    // 14

  // ACK 9, 12
  std::vector<SackChunk::GapAckBlock> gab1 = {SackChunk::GapAckBlock(3, 3)};
  EXPECT_FALSE(
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab1, false).has_packet_loss);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());

  // ACK 9, 12, 13
  std::vector<SackChunk::GapAckBlock> gab2 = {SackChunk::GapAckBlock(3, 4)};
  EXPECT_FALSE(
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab2, false).has_packet_loss);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());

  EXPECT_CALL(on_discard_, Call(IsUnordered(false), StreamID(1), MID(42)))
      .WillOnce(Return(false));

  // ACK 9, 12, 13, 14
  std::vector<SackChunk::GapAckBlock> gab3 = {SackChunk::GapAckBlock(3, 5)};
  OutstandingData::AckInfo ack =
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab3, false);
  EXPECT_TRUE(ack.has_packet_loss);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());
  EXPECT_THAT(buf_.GetChunksToBeFastRetransmitted(1000), IsEmpty());
  EXPECT_THAT(buf_.GetChunksToBeRetransmitted(1000), IsEmpty());
}

TEST_F(OutstandingDataTest, LifecyleReturnsAckedItemsInAckInfo) {
  buf_.Insert(gen_.Ordered({1}, "BE"), kNow, MaxRetransmits::NoLimit(),
              TimeMs::InfiniteFuture(), LifecycleId(42));
  buf_.Insert(gen_.Ordered({1}, "BE"), kNow, MaxRetransmits::NoLimit(),
              TimeMs::InfiniteFuture(), LifecycleId(43));
  buf_.Insert(gen_.Ordered({1}, "BE"), kNow, MaxRetransmits::NoLimit(),
              TimeMs::InfiniteFuture(), LifecycleId(44));

  OutstandingData::AckInfo ack1 =
      buf_.HandleSack(unwrapper_.Unwrap(TSN(11)), {}, false);

  EXPECT_THAT(ack1.acked_lifecycle_ids,
              ElementsAre(LifecycleId(42), LifecycleId(43)));

  OutstandingData::AckInfo ack2 =
      buf_.HandleSack(unwrapper_.Unwrap(TSN(12)), {}, false);

  EXPECT_THAT(ack2.acked_lifecycle_ids, ElementsAre(LifecycleId(44)));
}

TEST_F(OutstandingDataTest, LifecycleReturnsAbandonedNackedThreeTimes) {
  buf_.Insert(gen_.Ordered({1}, "B"), kNow, MaxRetransmits(0));
  buf_.Insert(gen_.Ordered({1}, ""), kNow, MaxRetransmits(0));
  buf_.Insert(gen_.Ordered({1}, ""), kNow, MaxRetransmits(0));
  buf_.Insert(gen_.Ordered({1}, "E"), kNow, MaxRetransmits(0),
              TimeMs::InfiniteFuture(), LifecycleId(42));

  std::vector<SackChunk::GapAckBlock> gab1 = {SackChunk::GapAckBlock(2, 2)};
  EXPECT_FALSE(
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab1, false).has_packet_loss);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());

  std::vector<SackChunk::GapAckBlock> gab2 = {SackChunk::GapAckBlock(2, 3)};
  EXPECT_FALSE(
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab2, false).has_packet_loss);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());

  std::vector<SackChunk::GapAckBlock> gab3 = {SackChunk::GapAckBlock(2, 4)};
  EXPECT_CALL(on_discard_, Call(IsUnordered(false), StreamID(1), MID(42)))
      .WillOnce(Return(false));
  OutstandingData::AckInfo ack1 =
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab3, false);
  EXPECT_TRUE(ack1.has_packet_loss);
  EXPECT_THAT(ack1.abandoned_lifecycle_ids, IsEmpty());

  // This will generate a FORWARD-TSN, which is acked
  EXPECT_TRUE(buf_.ShouldSendForwardTsn());
  ForwardTsnChunk chunk = buf_.CreateForwardTsn();
  EXPECT_EQ(chunk.new_cumulative_tsn(), TSN(13));

  OutstandingData::AckInfo ack2 =
      buf_.HandleSack(unwrapper_.Unwrap(TSN(13)), {}, false);
  EXPECT_FALSE(ack2.has_packet_loss);
  EXPECT_THAT(ack2.abandoned_lifecycle_ids, ElementsAre(LifecycleId(42)));
}

TEST_F(OutstandingDataTest, LifecycleReturnsAbandonedAfterT3rtxExpired) {
  buf_.Insert(gen_.Ordered({1}, "B"), kNow, MaxRetransmits(0));
  buf_.Insert(gen_.Ordered({1}, ""), kNow, MaxRetransmits(0));
  buf_.Insert(gen_.Ordered({1}, ""), kNow, MaxRetransmits(0));
  buf_.Insert(gen_.Ordered({1}, "E"), kNow, MaxRetransmits(0),
              TimeMs::InfiniteFuture(), LifecycleId(42));

  EXPECT_THAT(buf_.GetChunkStatesForTesting(),
              testing::ElementsAre(Pair(TSN(9), State::kAcked),      //
                                   Pair(TSN(10), State::kInFlight),  //
                                   Pair(TSN(11), State::kInFlight),  //
                                   Pair(TSN(12), State::kInFlight),  //
                                   Pair(TSN(13), State::kInFlight)));

  std::vector<SackChunk::GapAckBlock> gab1 = {SackChunk::GapAckBlock(2, 4)};
  EXPECT_FALSE(
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab1, false).has_packet_loss);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());

  EXPECT_THAT(buf_.GetChunkStatesForTesting(),
              testing::ElementsAre(Pair(TSN(9), State::kAcked),    //
                                   Pair(TSN(10), State::kNacked),  //
                                   Pair(TSN(11), State::kAcked),   //
                                   Pair(TSN(12), State::kAcked),   //
                                   Pair(TSN(13), State::kAcked)));

  // T3-rtx triggered.
  EXPECT_CALL(on_discard_, Call(IsUnordered(false), StreamID(1), MID(42)))
      .WillOnce(Return(false));
  buf_.NackAll();

  EXPECT_THAT(buf_.GetChunkStatesForTesting(),
              testing::ElementsAre(Pair(TSN(9), State::kAcked),       //
                                   Pair(TSN(10), State::kAbandoned),  //
                                   Pair(TSN(11), State::kAbandoned),  //
                                   Pair(TSN(12), State::kAbandoned),  //
                                   Pair(TSN(13), State::kAbandoned)));

  // This will generate a FORWARD-TSN, which is acked
  EXPECT_TRUE(buf_.ShouldSendForwardTsn());
  ForwardTsnChunk chunk = buf_.CreateForwardTsn();
  EXPECT_EQ(chunk.new_cumulative_tsn(), TSN(13));

  OutstandingData::AckInfo ack2 =
      buf_.HandleSack(unwrapper_.Unwrap(TSN(13)), {}, false);
  EXPECT_FALSE(ack2.has_packet_loss);
  EXPECT_THAT(ack2.abandoned_lifecycle_ids, ElementsAre(LifecycleId(42)));
}
}  // namespace
}  // namespace dcsctp
