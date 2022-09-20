/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/rx/traditional_reassembly_streams.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "net/dcsctp/common/handover_testing.h"
#include "net/dcsctp/common/sequence_numbers.h"
#include "net/dcsctp/packet/chunk/forward_tsn_chunk.h"
#include "net/dcsctp/packet/chunk/forward_tsn_common.h"
#include "net/dcsctp/packet/data.h"
#include "net/dcsctp/rx/reassembly_streams.h"
#include "net/dcsctp/testing/data_generator.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace dcsctp {
namespace {
using ::testing::ElementsAre;
using ::testing::MockFunction;
using ::testing::NiceMock;
using ::testing::Property;

class TraditionalReassemblyStreamsTest : public testing::Test {
 protected:
  UnwrappedTSN tsn(uint32_t value) { return tsn_.Unwrap(TSN(value)); }

  TraditionalReassemblyStreamsTest() {}
  DataGenerator gen_;
  UnwrappedTSN::Unwrapper tsn_;
};

TEST_F(TraditionalReassemblyStreamsTest,
       AddUnorderedMessageReturnsCorrectSize) {
  NiceMock<MockFunction<ReassemblyStreams::OnAssembledMessage>> on_assembled;

  TraditionalReassemblyStreams streams("", on_assembled.AsStdFunction());

  EXPECT_EQ(streams.Add(tsn(1), gen_.Unordered({1}, "B")), 1);
  EXPECT_EQ(streams.Add(tsn(2), gen_.Unordered({2, 3, 4})), 3);
  EXPECT_EQ(streams.Add(tsn(3), gen_.Unordered({5, 6})), 2);
  // Adding the end fragment should make it empty again.
  EXPECT_EQ(streams.Add(tsn(4), gen_.Unordered({7}, "E")), -6);
}

TEST_F(TraditionalReassemblyStreamsTest,
       AddSimpleOrderedMessageReturnsCorrectSize) {
  NiceMock<MockFunction<ReassemblyStreams::OnAssembledMessage>> on_assembled;

  TraditionalReassemblyStreams streams("", on_assembled.AsStdFunction());

  EXPECT_EQ(streams.Add(tsn(1), gen_.Ordered({1}, "B")), 1);
  EXPECT_EQ(streams.Add(tsn(2), gen_.Ordered({2, 3, 4})), 3);
  EXPECT_EQ(streams.Add(tsn(3), gen_.Ordered({5, 6})), 2);
  EXPECT_EQ(streams.Add(tsn(4), gen_.Ordered({7}, "E")), -6);
}

TEST_F(TraditionalReassemblyStreamsTest,
       AddMoreComplexOrderedMessageReturnsCorrectSize) {
  NiceMock<MockFunction<ReassemblyStreams::OnAssembledMessage>> on_assembled;

  TraditionalReassemblyStreams streams("", on_assembled.AsStdFunction());

  EXPECT_EQ(streams.Add(tsn(1), gen_.Ordered({1}, "B")), 1);
  Data late = gen_.Ordered({2, 3, 4});
  EXPECT_EQ(streams.Add(tsn(3), gen_.Ordered({5, 6})), 2);
  EXPECT_EQ(streams.Add(tsn(4), gen_.Ordered({7}, "E")), 1);

  EXPECT_EQ(streams.Add(tsn(5), gen_.Ordered({1}, "BE")), 1);
  EXPECT_EQ(streams.Add(tsn(6), gen_.Ordered({5, 6}, "B")), 2);
  EXPECT_EQ(streams.Add(tsn(7), gen_.Ordered({7}, "E")), 1);
  EXPECT_EQ(streams.Add(tsn(2), std::move(late)), -8);
}

TEST_F(TraditionalReassemblyStreamsTest,
       DeleteUnorderedMessageReturnsCorrectSize) {
  NiceMock<MockFunction<ReassemblyStreams::OnAssembledMessage>> on_assembled;

  TraditionalReassemblyStreams streams("", on_assembled.AsStdFunction());

  EXPECT_EQ(streams.Add(tsn(1), gen_.Unordered({1}, "B")), 1);
  EXPECT_EQ(streams.Add(tsn(2), gen_.Unordered({2, 3, 4})), 3);
  EXPECT_EQ(streams.Add(tsn(3), gen_.Unordered({5, 6})), 2);

  EXPECT_EQ(streams.HandleForwardTsn(tsn(3), {}), 6u);
}

TEST_F(TraditionalReassemblyStreamsTest,
       DeleteSimpleOrderedMessageReturnsCorrectSize) {
  NiceMock<MockFunction<ReassemblyStreams::OnAssembledMessage>> on_assembled;

  TraditionalReassemblyStreams streams("", on_assembled.AsStdFunction());

  EXPECT_EQ(streams.Add(tsn(1), gen_.Ordered({1}, "B")), 1);
  EXPECT_EQ(streams.Add(tsn(2), gen_.Ordered({2, 3, 4})), 3);
  EXPECT_EQ(streams.Add(tsn(3), gen_.Ordered({5, 6})), 2);

  ForwardTsnChunk::SkippedStream skipped[] = {
      ForwardTsnChunk::SkippedStream(StreamID(1), SSN(0))};
  EXPECT_EQ(streams.HandleForwardTsn(tsn(3), skipped), 6u);
}

TEST_F(TraditionalReassemblyStreamsTest,
       DeleteManyOrderedMessagesReturnsCorrectSize) {
  NiceMock<MockFunction<ReassemblyStreams::OnAssembledMessage>> on_assembled;

  TraditionalReassemblyStreams streams("", on_assembled.AsStdFunction());

  EXPECT_EQ(streams.Add(tsn(1), gen_.Ordered({1}, "B")), 1);
  gen_.Ordered({2, 3, 4});
  EXPECT_EQ(streams.Add(tsn(3), gen_.Ordered({5, 6})), 2);
  EXPECT_EQ(streams.Add(tsn(4), gen_.Ordered({7}, "E")), 1);

  EXPECT_EQ(streams.Add(tsn(5), gen_.Ordered({1}, "BE")), 1);
  EXPECT_EQ(streams.Add(tsn(6), gen_.Ordered({5, 6}, "B")), 2);
  EXPECT_EQ(streams.Add(tsn(7), gen_.Ordered({7}, "E")), 1);

  // Expire all three messages
  ForwardTsnChunk::SkippedStream skipped[] = {
      ForwardTsnChunk::SkippedStream(StreamID(1), SSN(2))};
  EXPECT_EQ(streams.HandleForwardTsn(tsn(8), skipped), 8u);
}

TEST_F(TraditionalReassemblyStreamsTest,
       DeleteOrderedMessageDelivesTwoReturnsCorrectSize) {
  NiceMock<MockFunction<ReassemblyStreams::OnAssembledMessage>> on_assembled;

  TraditionalReassemblyStreams streams("", on_assembled.AsStdFunction());

  EXPECT_EQ(streams.Add(tsn(1), gen_.Ordered({1}, "B")), 1);
  gen_.Ordered({2, 3, 4});
  EXPECT_EQ(streams.Add(tsn(3), gen_.Ordered({5, 6})), 2);
  EXPECT_EQ(streams.Add(tsn(4), gen_.Ordered({7}, "E")), 1);

  EXPECT_EQ(streams.Add(tsn(5), gen_.Ordered({1}, "BE")), 1);
  EXPECT_EQ(streams.Add(tsn(6), gen_.Ordered({5, 6}, "B")), 2);
  EXPECT_EQ(streams.Add(tsn(7), gen_.Ordered({7}, "E")), 1);

  // The first ordered message expire, and the following two are delivered.
  ForwardTsnChunk::SkippedStream skipped[] = {
      ForwardTsnChunk::SkippedStream(StreamID(1), SSN(0))};
  EXPECT_EQ(streams.HandleForwardTsn(tsn(4), skipped), 8u);
}

TEST_F(TraditionalReassemblyStreamsTest, NoStreamsCanBeHandedOver) {
  NiceMock<MockFunction<ReassemblyStreams::OnAssembledMessage>> on_assembled;

  TraditionalReassemblyStreams streams1("", on_assembled.AsStdFunction());
  EXPECT_TRUE(streams1.GetHandoverReadiness().IsReady());

  DcSctpSocketHandoverState state;
  streams1.AddHandoverState(state);
  g_handover_state_transformer_for_test(&state);
  TraditionalReassemblyStreams streams2("", on_assembled.AsStdFunction());
  streams2.RestoreFromState(state);

  EXPECT_EQ(streams2.Add(tsn(1), gen_.Ordered({1}, "B")), 1);
  EXPECT_EQ(streams2.Add(tsn(2), gen_.Ordered({2, 3, 4})), 3);
  EXPECT_EQ(streams2.Add(tsn(1), gen_.Unordered({1}, "B")), 1);
  EXPECT_EQ(streams2.Add(tsn(2), gen_.Unordered({2, 3, 4})), 3);
}

TEST_F(TraditionalReassemblyStreamsTest,
       OrderedStreamsCanBeHandedOverWhenNoUnassembledChunksExist) {
  NiceMock<MockFunction<ReassemblyStreams::OnAssembledMessage>> on_assembled;

  TraditionalReassemblyStreams streams1("", on_assembled.AsStdFunction());

  EXPECT_EQ(streams1.Add(tsn(1), gen_.Ordered({1}, "B")), 1);
  EXPECT_EQ(streams1.GetHandoverReadiness(),
            HandoverReadinessStatus(
                HandoverUnreadinessReason::kOrderedStreamHasUnassembledChunks));
  EXPECT_EQ(streams1.Add(tsn(2), gen_.Ordered({2, 3, 4})), 3);
  EXPECT_EQ(streams1.GetHandoverReadiness(),
            HandoverReadinessStatus(
                HandoverUnreadinessReason::kOrderedStreamHasUnassembledChunks));
  EXPECT_EQ(streams1.Add(tsn(3), gen_.Ordered({5, 6})), 2);
  EXPECT_EQ(streams1.GetHandoverReadiness(),
            HandoverReadinessStatus(
                HandoverUnreadinessReason::kOrderedStreamHasUnassembledChunks));

  ForwardTsnChunk::SkippedStream skipped[] = {
      ForwardTsnChunk::SkippedStream(StreamID(1), SSN(0))};
  EXPECT_EQ(streams1.HandleForwardTsn(tsn(3), skipped), 6u);
  EXPECT_TRUE(streams1.GetHandoverReadiness().IsReady());

  DcSctpSocketHandoverState state;
  streams1.AddHandoverState(state);
  g_handover_state_transformer_for_test(&state);
  TraditionalReassemblyStreams streams2("", on_assembled.AsStdFunction());
  streams2.RestoreFromState(state);
  EXPECT_EQ(streams2.Add(tsn(4), gen_.Ordered({7})), 1);
}

TEST_F(TraditionalReassemblyStreamsTest,
       UnorderedStreamsCanBeHandedOverWhenNoUnassembledChunksExist) {
  NiceMock<MockFunction<ReassemblyStreams::OnAssembledMessage>> on_assembled;

  TraditionalReassemblyStreams streams1("", on_assembled.AsStdFunction());

  EXPECT_EQ(streams1.Add(tsn(1), gen_.Unordered({1}, "B")), 1);
  EXPECT_EQ(
      streams1.GetHandoverReadiness(),
      HandoverReadinessStatus(
          HandoverUnreadinessReason::kUnorderedStreamHasUnassembledChunks));
  EXPECT_EQ(streams1.Add(tsn(2), gen_.Unordered({2, 3, 4})), 3);
  EXPECT_EQ(
      streams1.GetHandoverReadiness(),
      HandoverReadinessStatus(
          HandoverUnreadinessReason::kUnorderedStreamHasUnassembledChunks));
  EXPECT_EQ(streams1.Add(tsn(3), gen_.Unordered({5, 6})), 2);
  EXPECT_EQ(
      streams1.GetHandoverReadiness(),
      HandoverReadinessStatus(
          HandoverUnreadinessReason::kUnorderedStreamHasUnassembledChunks));

  EXPECT_EQ(streams1.HandleForwardTsn(tsn(3), {}), 6u);
  EXPECT_TRUE(streams1.GetHandoverReadiness().IsReady());

  DcSctpSocketHandoverState state;
  streams1.AddHandoverState(state);
  g_handover_state_transformer_for_test(&state);
  TraditionalReassemblyStreams streams2("", on_assembled.AsStdFunction());
  streams2.RestoreFromState(state);
  EXPECT_EQ(streams2.Add(tsn(4), gen_.Unordered({7})), 1);
}

TEST_F(TraditionalReassemblyStreamsTest, CanDeleteFirstOrderedMessage) {
  NiceMock<MockFunction<ReassemblyStreams::OnAssembledMessage>> on_assembled;
  EXPECT_CALL(on_assembled,
              Call(ElementsAre(tsn(2)),
                   Property(&DcSctpMessage::payload, ElementsAre(2, 3, 4))));

  TraditionalReassemblyStreams streams("", on_assembled.AsStdFunction());

  // Not received, SID=1. TSN=1, SSN=0
  gen_.Ordered({1}, "BE");
  // And deleted (SID=1, TSN=1, SSN=0)
  ForwardTsnChunk::SkippedStream skipped[] = {
      ForwardTsnChunk::SkippedStream(StreamID(1), SSN(0))};
  EXPECT_EQ(streams.HandleForwardTsn(tsn(1), skipped), 0u);

  // Receive SID=1, TSN=2, SSN=1
  EXPECT_EQ(streams.Add(tsn(2), gen_.Ordered({2, 3, 4}, "BE")), 0);
}

}  // namespace
}  // namespace dcsctp
