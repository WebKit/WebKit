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

#include <optional>
#include <vector>

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "net/dcsctp/common/internal_types.h"
#include "net/dcsctp/common/math.h"
#include "net/dcsctp/common/sequence_numbers.h"
#include "net/dcsctp/packet/chunk/data_chunk.h"
#include "net/dcsctp/packet/chunk/forward_tsn_chunk.h"
#include "net/dcsctp/packet/chunk/sack_chunk.h"
#include "net/dcsctp/public/types.h"
#include "net/dcsctp/testing/data_generator.h"
#include "net/dcsctp/testing/testing_macros.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace dcsctp {
namespace {
using ::testing::MockFunction;
using State = ::dcsctp::OutstandingData::State;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::Property;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::UnorderedElementsAre;
using ::webrtc::TimeDelta;
using ::webrtc::Timestamp;

constexpr Timestamp kNow = Timestamp::Millis(42);
constexpr OutgoingMessageId kMessageId = OutgoingMessageId(17);

class OutstandingDataTest : public testing::Test {
 protected:
  OutstandingDataTest()
      : gen_(MID(42)),
        buf_(DataChunk::kHeaderSize,
             unwrapper_.Unwrap(TSN(9)),
             on_discard_.AsStdFunction()) {}

  UnwrappedTSN::Unwrapper unwrapper_;
  DataGenerator gen_;
  StrictMock<MockFunction<bool(StreamID, OutgoingMessageId)>> on_discard_;
  OutstandingData buf_;
};

TEST_F(OutstandingDataTest, HasInitialState) {
  EXPECT_TRUE(buf_.empty());
  EXPECT_EQ(buf_.unacked_payload_bytes(), 0u);
  EXPECT_EQ(buf_.unacked_packet_bytes(), 0u);
  EXPECT_EQ(buf_.unacked_items(), 0u);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());
  EXPECT_EQ(buf_.last_cumulative_tsn_ack().Wrap(), TSN(9));
  EXPECT_EQ(buf_.next_tsn().Wrap(), TSN(10));
  EXPECT_EQ(buf_.highest_outstanding_tsn().Wrap(), TSN(9));
  EXPECT_THAT(buf_.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked)));
  EXPECT_FALSE(buf_.ShouldSendForwardTsn());
}

TEST_F(OutstandingDataTest, InsertChunk) {
  ASSERT_HAS_VALUE_AND_ASSIGN(
      UnwrappedTSN tsn, buf_.Insert(kMessageId, gen_.Ordered({1}, "BE"), kNow));

  EXPECT_EQ(tsn.Wrap(), TSN(10));

  EXPECT_EQ(buf_.unacked_payload_bytes(), 1u);
  EXPECT_EQ(buf_.unacked_items(), 1u);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());
  EXPECT_EQ(buf_.last_cumulative_tsn_ack().Wrap(), TSN(9));
  EXPECT_EQ(buf_.next_tsn().Wrap(), TSN(11));
  EXPECT_EQ(buf_.highest_outstanding_tsn().Wrap(), TSN(10));
  EXPECT_THAT(buf_.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),
                          Pair(TSN(10), State::kInFlight)));
}

TEST_F(OutstandingDataTest, AcksSingleChunk) {
  buf_.Insert(kMessageId, gen_.Ordered({1}, "BE"), kNow);
  OutstandingData::AckInfo ack =
      buf_.HandleSack(unwrapper_.Unwrap(TSN(10)), {}, false);

  EXPECT_EQ(ack.bytes_acked, DataChunk::kHeaderSize + RoundUpTo4(1));
  EXPECT_EQ(ack.highest_tsn_acked.Wrap(), TSN(10));
  EXPECT_FALSE(ack.has_packet_loss);

  EXPECT_EQ(buf_.unacked_payload_bytes(), 0u);
  EXPECT_EQ(buf_.unacked_items(), 0u);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());
  EXPECT_EQ(buf_.last_cumulative_tsn_ack().Wrap(), TSN(10));
  EXPECT_EQ(buf_.next_tsn().Wrap(), TSN(11));
  EXPECT_EQ(buf_.highest_outstanding_tsn().Wrap(), TSN(10));
  EXPECT_THAT(buf_.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(10), State::kAcked)));
}

TEST_F(OutstandingDataTest, AcksPreviousChunkDoesntUpdate) {
  buf_.Insert(kMessageId, gen_.Ordered({1}, "BE"), kNow);
  buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), {}, false);

  EXPECT_EQ(buf_.unacked_payload_bytes(), 1u);
  EXPECT_EQ(buf_.unacked_items(), 1u);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());
  EXPECT_EQ(buf_.last_cumulative_tsn_ack().Wrap(), TSN(9));
  EXPECT_EQ(buf_.next_tsn().Wrap(), TSN(11));
  EXPECT_EQ(buf_.highest_outstanding_tsn().Wrap(), TSN(10));
  EXPECT_THAT(buf_.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),
                          Pair(TSN(10), State::kInFlight)));
}

TEST_F(OutstandingDataTest, AcksAndNacksWithGapAckBlocks) {
  buf_.Insert(kMessageId, gen_.Ordered({1}, "B"), kNow);
  buf_.Insert(kMessageId, gen_.Ordered({1}, "E"), kNow);

  std::vector<SackChunk::GapAckBlock> gab = {SackChunk::GapAckBlock(2, 2)};
  OutstandingData::AckInfo ack =
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab, false);
  EXPECT_EQ(ack.bytes_acked, DataChunk::kHeaderSize + RoundUpTo4(1));
  EXPECT_EQ(ack.highest_tsn_acked.Wrap(), TSN(11));
  EXPECT_FALSE(ack.has_packet_loss);

  // TSN (10) is still outstanding.
  EXPECT_EQ(buf_.unacked_payload_bytes(), 1u);
  EXPECT_EQ(buf_.unacked_items(), 1u);
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
  buf_.Insert(kMessageId, gen_.Ordered({1}, "B"), kNow);
  buf_.Insert(kMessageId, gen_.Ordered({1}, "E"), kNow);

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
  buf_.Insert(kMessageId, gen_.Ordered({1}, "B"), kNow);
  buf_.Insert(kMessageId, gen_.Ordered({1}, ""), kNow);
  buf_.Insert(kMessageId, gen_.Ordered({1}, ""), kNow);
  buf_.Insert(kMessageId, gen_.Ordered({1}, "E"), kNow);

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
  buf_.Insert(kMessageId, gen_.Ordered({1}, "B"), kNow, kMaxRetransmissions);
  buf_.Insert(kMessageId, gen_.Ordered({1}, ""), kNow, kMaxRetransmissions);
  buf_.Insert(kMessageId, gen_.Ordered({1}, ""), kNow, kMaxRetransmissions);
  buf_.Insert(kMessageId, gen_.Ordered({1}, "E"), kNow, kMaxRetransmissions);

  std::vector<SackChunk::GapAckBlock> gab1 = {SackChunk::GapAckBlock(2, 2)};
  EXPECT_FALSE(
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab1, false).has_packet_loss);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());

  std::vector<SackChunk::GapAckBlock> gab2 = {SackChunk::GapAckBlock(2, 3)};
  EXPECT_FALSE(
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab2, false).has_packet_loss);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());

  EXPECT_CALL(on_discard_, Call(StreamID(1), kMessageId))
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
  buf_.Insert(kMessageId, gen_.Ordered({1}, "B"), kNow, kMaxRetransmissions);
  buf_.Insert(kMessageId, gen_.Ordered({1}, ""), kNow, kMaxRetransmissions);
  buf_.Insert(kMessageId, gen_.Ordered({1}, ""), kNow, kMaxRetransmissions);
  buf_.Insert(kMessageId, gen_.Ordered({1}, ""), kNow, kMaxRetransmissions);

  std::vector<SackChunk::GapAckBlock> gab1 = {SackChunk::GapAckBlock(2, 2)};
  EXPECT_FALSE(
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab1, false).has_packet_loss);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());

  std::vector<SackChunk::GapAckBlock> gab2 = {SackChunk::GapAckBlock(2, 3)};
  EXPECT_FALSE(
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab2, false).has_packet_loss);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());

  EXPECT_CALL(on_discard_, Call(StreamID(1), kMessageId))
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
  static constexpr Timestamp kExpiresAt = kNow + TimeDelta::Millis(1);
  EXPECT_TRUE(buf_.Insert(kMessageId, gen_.Ordered({1}, "B"), kNow,
                          MaxRetransmits::NoLimit(), kExpiresAt)
                  .has_value());
  EXPECT_TRUE(buf_.Insert(kMessageId, gen_.Ordered({1}, ""),
                          kNow + TimeDelta::Millis(0),
                          MaxRetransmits::NoLimit(), kExpiresAt)
                  .has_value());

  EXPECT_CALL(on_discard_, Call(StreamID(1), kMessageId))
      .WillOnce(Return(false));
  EXPECT_FALSE(buf_.Insert(kMessageId, gen_.Ordered({1}, "E"),
                           kNow + TimeDelta::Millis(1),
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
  buf_.Insert(kMessageId, gen_.Ordered({1}, "B"), kNow, kMaxRetransmissions);
  buf_.Insert(kMessageId, gen_.Ordered({1}, ""), kNow, kMaxRetransmissions);
  buf_.Insert(kMessageId, gen_.Ordered({1}, "E"), kNow, kMaxRetransmissions);

  EXPECT_CALL(on_discard_, Call(StreamID(1), kMessageId))
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
  buf_.Insert(kMessageId, gen_.Ordered({1}, "B"), kNow);
  buf_.Insert(kMessageId, gen_.Ordered({1}, ""), kNow);
  buf_.Insert(kMessageId, gen_.Ordered({1}, ""), kNow);
  buf_.Insert(kMessageId, gen_.Ordered({1}, ""), kNow);
  buf_.Insert(kMessageId, gen_.Ordered({1}, ""), kNow);
  buf_.Insert(kMessageId, gen_.Ordered({1}, ""), kNow);
  buf_.Insert(kMessageId, gen_.Ordered({1}, ""), kNow);
  buf_.Insert(kMessageId, gen_.Ordered({1}, "E"), kNow);

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
  buf_.Insert(kMessageId, gen_.Ordered({1}, "BE"), kNow);
  buf_.Insert(kMessageId, gen_.Ordered({1}, "BE"), kNow + TimeDelta::Millis(1));
  buf_.Insert(kMessageId, gen_.Ordered({1}, "BE"), kNow + TimeDelta::Millis(2));

  static constexpr TimeDelta kDuration = TimeDelta::Millis(123);
  TimeDelta duration =
      buf_.MeasureRTT(kNow + kDuration, unwrapper_.Unwrap(TSN(11)));

  EXPECT_EQ(duration, kDuration - TimeDelta::Millis(1));
}

TEST_F(OutstandingDataTest, MustRetransmitBeforeGettingNackedAgain) {
  // This test case verifies that a chunk that has been nacked, and scheduled to
  // be retransmitted, doesn't get nacked again until it has been actually sent
  // on the wire.

  static constexpr MaxRetransmits kOneRetransmission(1);
  for (int tsn = 10; tsn <= 20; ++tsn) {
    buf_.Insert(kMessageId,
                gen_.Ordered({1}, tsn == 10   ? "B"
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

  EXPECT_CALL(on_discard_, Call(StreamID(1), kMessageId))
      .WillOnce(Return(false));

  std::vector<SackChunk::GapAckBlock> gab9 = {SackChunk::GapAckBlock(2, 10)};
  OutstandingData::AckInfo ack3 =
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab9, false);

  EXPECT_TRUE(ack3.has_packet_loss);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());
}

TEST_F(OutstandingDataTest, LifecyleReturnsAckedItemsInAckInfo) {
  buf_.Insert(OutgoingMessageId(1), gen_.Ordered({1}, "BE"), kNow,
              MaxRetransmits::NoLimit(), Timestamp::PlusInfinity(),
              LifecycleId(42));
  buf_.Insert(OutgoingMessageId(2), gen_.Ordered({1}, "BE"), kNow,
              MaxRetransmits::NoLimit(), Timestamp::PlusInfinity(),
              LifecycleId(43));
  buf_.Insert(OutgoingMessageId(3), gen_.Ordered({1}, "BE"), kNow,
              MaxRetransmits::NoLimit(), Timestamp::PlusInfinity(),
              LifecycleId(44));

  OutstandingData::AckInfo ack1 =
      buf_.HandleSack(unwrapper_.Unwrap(TSN(11)), {}, false);

  EXPECT_THAT(ack1.acked_lifecycle_ids,
              ElementsAre(LifecycleId(42), LifecycleId(43)));

  OutstandingData::AckInfo ack2 =
      buf_.HandleSack(unwrapper_.Unwrap(TSN(12)), {}, false);

  EXPECT_THAT(ack2.acked_lifecycle_ids, ElementsAre(LifecycleId(44)));
}

TEST_F(OutstandingDataTest, LifecycleReturnsAbandonedNackedThreeTimes) {
  buf_.Insert(kMessageId, gen_.Ordered({1}, "B"), kNow, MaxRetransmits(0));
  buf_.Insert(kMessageId, gen_.Ordered({1}, ""), kNow, MaxRetransmits(0));
  buf_.Insert(kMessageId, gen_.Ordered({1}, ""), kNow, MaxRetransmits(0));
  buf_.Insert(kMessageId, gen_.Ordered({1}, "E"), kNow, MaxRetransmits(0),
              Timestamp::PlusInfinity(), LifecycleId(42));

  std::vector<SackChunk::GapAckBlock> gab1 = {SackChunk::GapAckBlock(2, 2)};
  EXPECT_FALSE(
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab1, false).has_packet_loss);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());

  std::vector<SackChunk::GapAckBlock> gab2 = {SackChunk::GapAckBlock(2, 3)};
  EXPECT_FALSE(
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab2, false).has_packet_loss);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());

  std::vector<SackChunk::GapAckBlock> gab3 = {SackChunk::GapAckBlock(2, 4)};
  EXPECT_CALL(on_discard_, Call(StreamID(1), kMessageId))
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
  buf_.Insert(kMessageId, gen_.Ordered({1}, "B"), kNow, MaxRetransmits(0));
  buf_.Insert(kMessageId, gen_.Ordered({1}, ""), kNow, MaxRetransmits(0));
  buf_.Insert(kMessageId, gen_.Ordered({1}, ""), kNow, MaxRetransmits(0));
  buf_.Insert(kMessageId, gen_.Ordered({1}, "E"), kNow, MaxRetransmits(0),
              Timestamp::PlusInfinity(), LifecycleId(42));

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
  EXPECT_CALL(on_discard_, Call(StreamID(1), kMessageId))
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

TEST_F(OutstandingDataTest, GeneratesForwardTsnUntilNextStreamResetTsn) {
  // This test generates:
  // * Stream 1: TSN 10, 11, 12 <RESET>
  // * Stream 2: TSN 13, 14 <RESET>
  // * Stream 3: TSN 15, 16
  //
  // Then it expires chunk 12-15, and ensures that the generated FORWARD-TSN
  // only includes up till TSN 12 until the cum ack TSN has reached 12, and then
  // 13 and 14 are included, and then after the cum ack TSN has reached 14, then
  // 15 is included.
  //
  // What it shouldn't do, is to generate a FORWARD-TSN directly at the start
  // with new TSN=15, and setting [(sid=1, ssn=44), (sid=2, ssn=46),
  // (sid=3, ssn=47)], because that will confuse the receiver at TSN=17,
  // receiving SID=1, SSN=0 (it's reset!), expecting SSN to be 45.
  constexpr DataGeneratorOptions kStream1 = {.stream_id = StreamID(1)};
  constexpr DataGeneratorOptions kStream2 = {.stream_id = StreamID(2)};
  constexpr DataGeneratorOptions kStream3 = {.stream_id = StreamID(3)};
  constexpr MaxRetransmits kNoRtx = MaxRetransmits(0);
  EXPECT_CALL(on_discard_, Call).WillRepeatedly(Return(false));

  // TSN 10-12
  buf_.Insert(OutgoingMessageId(0), gen_.Ordered({1}, "BE", kStream1), kNow,
              kNoRtx);
  buf_.Insert(OutgoingMessageId(1), gen_.Ordered({1}, "BE", kStream1), kNow,
              kNoRtx);
  buf_.Insert(OutgoingMessageId(2), gen_.Ordered({1}, "BE", kStream1), kNow,
              kNoRtx);

  buf_.BeginResetStreams();

  // TSN 13, 14
  buf_.Insert(OutgoingMessageId(3), gen_.Ordered({1}, "BE", kStream2), kNow,
              kNoRtx);
  buf_.Insert(OutgoingMessageId(4), gen_.Ordered({1}, "BE", kStream2), kNow,
              kNoRtx);

  buf_.BeginResetStreams();

  // TSN 15, 16
  buf_.Insert(OutgoingMessageId(5), gen_.Ordered({1}, "BE", kStream3), kNow,
              kNoRtx);
  buf_.Insert(OutgoingMessageId(6), gen_.Ordered({1}, "BE", kStream3), kNow);

  EXPECT_FALSE(buf_.ShouldSendForwardTsn());

  buf_.HandleSack(unwrapper_.Unwrap(TSN(11)), {}, false);
  buf_.NackAll();
  EXPECT_THAT(buf_.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(11), State::kAcked),      //
                          Pair(TSN(12), State::kAbandoned),  //
                          Pair(TSN(13), State::kAbandoned),  //
                          Pair(TSN(14), State::kAbandoned),  //
                          Pair(TSN(15), State::kAbandoned),  //
                          Pair(TSN(16), State::kToBeRetransmitted)));

  EXPECT_TRUE(buf_.ShouldSendForwardTsn());
  EXPECT_THAT(
      buf_.CreateForwardTsn(),
      AllOf(Property(&ForwardTsnChunk::new_cumulative_tsn, TSN(12)),
            Property(&ForwardTsnChunk::skipped_streams,
                     UnorderedElementsAre(ForwardTsnChunk::SkippedStream(
                         StreamID(1), SSN(44))))));

  // Ack 12, allowing a FORWARD-TSN that spans to TSN=14 to be created.
  buf_.HandleSack(unwrapper_.Unwrap(TSN(12)), {}, false);
  EXPECT_TRUE(buf_.ShouldSendForwardTsn());
  EXPECT_THAT(
      buf_.CreateForwardTsn(),
      AllOf(Property(&ForwardTsnChunk::new_cumulative_tsn, TSN(14)),
            Property(&ForwardTsnChunk::skipped_streams,
                     UnorderedElementsAre(ForwardTsnChunk::SkippedStream(
                         StreamID(2), SSN(46))))));

  // Ack 13, allowing a FORWARD-TSN that spans to TSN=14 to be created.
  buf_.HandleSack(unwrapper_.Unwrap(TSN(13)), {}, false);
  EXPECT_TRUE(buf_.ShouldSendForwardTsn());
  EXPECT_THAT(
      buf_.CreateForwardTsn(),
      AllOf(Property(&ForwardTsnChunk::new_cumulative_tsn, TSN(14)),
            Property(&ForwardTsnChunk::skipped_streams,
                     UnorderedElementsAre(ForwardTsnChunk::SkippedStream(
                         StreamID(2), SSN(46))))));

  // Ack 14, allowing a FORWARD-TSN that spans to TSN=15 to be created.
  buf_.HandleSack(unwrapper_.Unwrap(TSN(14)), {}, false);
  EXPECT_TRUE(buf_.ShouldSendForwardTsn());
  EXPECT_THAT(
      buf_.CreateForwardTsn(),
      AllOf(Property(&ForwardTsnChunk::new_cumulative_tsn, TSN(15)),
            Property(&ForwardTsnChunk::skipped_streams,
                     UnorderedElementsAre(ForwardTsnChunk::SkippedStream(
                         StreamID(3), SSN(47))))));

  buf_.HandleSack(unwrapper_.Unwrap(TSN(15)), {}, false);
  EXPECT_FALSE(buf_.ShouldSendForwardTsn());
}

TEST_F(OutstandingDataTest, TreatsUnackedPayloadBytesDifferentFromPacketBytes) {
  buf_.Insert(kMessageId, gen_.Ordered({1}, "BE"), kNow);

  EXPECT_EQ(buf_.unacked_payload_bytes(), 1u);
  EXPECT_EQ(buf_.unacked_packet_bytes(),
            DataChunk::kHeaderSize + RoundUpTo4(1));
  EXPECT_EQ(buf_.unacked_items(), 1u);

  buf_.Insert(kMessageId, gen_.Ordered({1}, "BE"), kNow);
  EXPECT_EQ(buf_.unacked_payload_bytes(), 2u);
  EXPECT_EQ(buf_.unacked_packet_bytes(),
            2 * (DataChunk::kHeaderSize + RoundUpTo4(1)));
  EXPECT_EQ(buf_.unacked_items(), 2u);
}

}  // namespace
}  // namespace dcsctp
