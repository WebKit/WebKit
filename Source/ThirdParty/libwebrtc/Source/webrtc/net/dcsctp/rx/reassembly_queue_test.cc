/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/rx/reassembly_queue.h"

#include <stddef.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <vector>

#include "api/array_view.h"
#include "net/dcsctp/common/handover_testing.h"
#include "net/dcsctp/packet/chunk/forward_tsn_chunk.h"
#include "net/dcsctp/packet/chunk/forward_tsn_common.h"
#include "net/dcsctp/packet/chunk/iforward_tsn_chunk.h"
#include "net/dcsctp/packet/data.h"
#include "net/dcsctp/public/dcsctp_message.h"
#include "net/dcsctp/public/types.h"
#include "net/dcsctp/testing/data_generator.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace dcsctp {
namespace {
using ::testing::ElementsAre;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

// The default maximum size of the Reassembly Queue.
static constexpr size_t kBufferSize = 10000;

static constexpr StreamID kStreamID(1);
static constexpr SSN kSSN(0);
static constexpr MID kMID(0);
static constexpr FSN kFSN(0);
static constexpr PPID kPPID(53);

static constexpr std::array<uint8_t, 4> kShortPayload = {1, 2, 3, 4};
static constexpr std::array<uint8_t, 4> kMessage2Payload = {5, 6, 7, 8};
static constexpr std::array<uint8_t, 6> kSixBytePayload = {1, 2, 3, 4, 5, 6};
static constexpr std::array<uint8_t, 8> kMediumPayload1 = {1, 2, 3, 4,
                                                           5, 6, 7, 8};
static constexpr std::array<uint8_t, 8> kMediumPayload2 = {9,  10, 11, 12,
                                                           13, 14, 15, 16};
static constexpr std::array<uint8_t, 16> kLongPayload = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

MATCHER_P3(SctpMessageIs, stream_id, ppid, expected_payload, "") {
  if (arg.stream_id() != stream_id) {
    *result_listener << "the stream_id is " << *arg.stream_id();
    return false;
  }

  if (arg.ppid() != ppid) {
    *result_listener << "the ppid is " << *arg.ppid();
    return false;
  }

  if (std::vector<uint8_t>(arg.payload().begin(), arg.payload().end()) !=
      std::vector<uint8_t>(expected_payload.begin(), expected_payload.end())) {
    *result_listener << "the payload is wrong";
    return false;
  }
  return true;
}

class ReassemblyQueueTest : public testing::Test {
 protected:
  ReassemblyQueueTest() {}
  DataGenerator gen_;
};

TEST_F(ReassemblyQueueTest, EmptyQueue) {
  ReassemblyQueue reasm("log: ", TSN(10), kBufferSize);
  EXPECT_FALSE(reasm.HasMessages());
  EXPECT_EQ(reasm.queued_bytes(), 0u);
}

TEST_F(ReassemblyQueueTest, SingleUnorderedChunkMessage) {
  ReassemblyQueue reasm("log: ", TSN(10), kBufferSize);
  reasm.Add(TSN(10), gen_.Unordered({1, 2, 3, 4}, "BE"));
  EXPECT_TRUE(reasm.HasMessages());
  EXPECT_THAT(reasm.FlushMessages(),
              ElementsAre(SctpMessageIs(kStreamID, kPPID, kShortPayload)));
  EXPECT_EQ(reasm.queued_bytes(), 0u);
}

TEST_F(ReassemblyQueueTest, LargeUnorderedChunkAllPermutations) {
  std::vector<uint32_t> tsns = {10, 11, 12, 13};
  rtc::ArrayView<const uint8_t> payload(kLongPayload);
  do {
    ReassemblyQueue reasm("log: ", TSN(10), kBufferSize);

    for (size_t i = 0; i < tsns.size(); i++) {
      auto span = payload.subview((tsns[i] - 10) * 4, 4);
      Data::IsBeginning is_beginning(tsns[i] == 10);
      Data::IsEnd is_end(tsns[i] == 13);

      reasm.Add(TSN(tsns[i]),
                Data(kStreamID, kSSN, kMID, kFSN, kPPID,
                     std::vector<uint8_t>(span.begin(), span.end()),
                     is_beginning, is_end, IsUnordered(false)));
      if (i < 3) {
        EXPECT_FALSE(reasm.HasMessages());
      } else {
        EXPECT_TRUE(reasm.HasMessages());
        EXPECT_THAT(reasm.FlushMessages(),
                    ElementsAre(SctpMessageIs(kStreamID, kPPID, kLongPayload)));
        EXPECT_EQ(reasm.queued_bytes(), 0u);
      }
    }
  } while (std::next_permutation(std::begin(tsns), std::end(tsns)));
}

TEST_F(ReassemblyQueueTest, SingleOrderedChunkMessage) {
  ReassemblyQueue reasm("log: ", TSN(10), kBufferSize);
  reasm.Add(TSN(10), gen_.Ordered({1, 2, 3, 4}, "BE"));
  EXPECT_EQ(reasm.queued_bytes(), 0u);
  EXPECT_TRUE(reasm.HasMessages());
  EXPECT_THAT(reasm.FlushMessages(),
              ElementsAre(SctpMessageIs(kStreamID, kPPID, kShortPayload)));
}

TEST_F(ReassemblyQueueTest, ManySmallOrderedMessages) {
  std::vector<uint32_t> tsns = {10, 11, 12, 13};
  rtc::ArrayView<const uint8_t> payload(kLongPayload);
  do {
    ReassemblyQueue reasm("log: ", TSN(10), kBufferSize);
    for (size_t i = 0; i < tsns.size(); i++) {
      auto span = payload.subview((tsns[i] - 10) * 4, 4);
      Data::IsBeginning is_beginning(true);
      Data::IsEnd is_end(true);

      SSN ssn(static_cast<uint16_t>(tsns[i] - 10));
      reasm.Add(TSN(tsns[i]),
                Data(kStreamID, ssn, kMID, kFSN, kPPID,
                     std::vector<uint8_t>(span.begin(), span.end()),
                     is_beginning, is_end, IsUnordered(false)));
    }
    EXPECT_THAT(
        reasm.FlushMessages(),
        ElementsAre(SctpMessageIs(kStreamID, kPPID, payload.subview(0, 4)),
                    SctpMessageIs(kStreamID, kPPID, payload.subview(4, 4)),
                    SctpMessageIs(kStreamID, kPPID, payload.subview(8, 4)),
                    SctpMessageIs(kStreamID, kPPID, payload.subview(12, 4))));
    EXPECT_EQ(reasm.queued_bytes(), 0u);
  } while (std::next_permutation(std::begin(tsns), std::end(tsns)));
}

TEST_F(ReassemblyQueueTest, RetransmissionInLargeOrdered) {
  ReassemblyQueue reasm("log: ", TSN(10), kBufferSize);
  reasm.Add(TSN(10), gen_.Ordered({1}, "B"));
  reasm.Add(TSN(12), gen_.Ordered({3}));
  reasm.Add(TSN(13), gen_.Ordered({4}));
  reasm.Add(TSN(14), gen_.Ordered({5}));
  reasm.Add(TSN(15), gen_.Ordered({6}));
  reasm.Add(TSN(16), gen_.Ordered({7}));
  reasm.Add(TSN(17), gen_.Ordered({8}));
  EXPECT_EQ(reasm.queued_bytes(), 7u);

  // lost and retransmitted
  reasm.Add(TSN(11), gen_.Ordered({2}));
  reasm.Add(TSN(18), gen_.Ordered({9}));
  reasm.Add(TSN(19), gen_.Ordered({10}));
  EXPECT_EQ(reasm.queued_bytes(), 10u);
  EXPECT_FALSE(reasm.HasMessages());

  reasm.Add(TSN(20), gen_.Ordered({11, 12, 13, 14, 15, 16}, "E"));
  EXPECT_TRUE(reasm.HasMessages());
  EXPECT_THAT(reasm.FlushMessages(),
              ElementsAre(SctpMessageIs(kStreamID, kPPID, kLongPayload)));
  EXPECT_EQ(reasm.queued_bytes(), 0u);
}

TEST_F(ReassemblyQueueTest, ForwardTSNRemoveUnordered) {
  ReassemblyQueue reasm("log: ", TSN(10), kBufferSize);
  reasm.Add(TSN(10), gen_.Unordered({1}, "B"));
  reasm.Add(TSN(12), gen_.Unordered({3}));
  reasm.Add(TSN(13), gen_.Unordered({4}, "E"));

  reasm.Add(TSN(14), gen_.Unordered({5}, "B"));
  reasm.Add(TSN(15), gen_.Unordered({6}));
  reasm.Add(TSN(17), gen_.Unordered({8}, "E"));
  EXPECT_EQ(reasm.queued_bytes(), 6u);

  EXPECT_FALSE(reasm.HasMessages());

  reasm.Handle(ForwardTsnChunk(TSN(13), {}));
  EXPECT_EQ(reasm.queued_bytes(), 3u);

  // The lost chunk comes, but too late.
  reasm.Add(TSN(11), gen_.Unordered({2}));
  EXPECT_FALSE(reasm.HasMessages());
  EXPECT_EQ(reasm.queued_bytes(), 3u);

  // The second lost chunk comes, message is assembled.
  reasm.Add(TSN(16), gen_.Unordered({7}));
  EXPECT_TRUE(reasm.HasMessages());
  EXPECT_EQ(reasm.queued_bytes(), 0u);
}

TEST_F(ReassemblyQueueTest, ForwardTSNRemoveOrdered) {
  ReassemblyQueue reasm("log: ", TSN(10), kBufferSize);
  reasm.Add(TSN(10), gen_.Ordered({1}, "B"));
  reasm.Add(TSN(12), gen_.Ordered({3}));
  reasm.Add(TSN(13), gen_.Ordered({4}, "E"));

  reasm.Add(TSN(14), gen_.Ordered({5}, "B"));
  reasm.Add(TSN(15), gen_.Ordered({6}));
  reasm.Add(TSN(16), gen_.Ordered({7}));
  reasm.Add(TSN(17), gen_.Ordered({8}, "E"));
  EXPECT_EQ(reasm.queued_bytes(), 7u);

  EXPECT_FALSE(reasm.HasMessages());

  reasm.Handle(ForwardTsnChunk(
      TSN(13), {ForwardTsnChunk::SkippedStream(kStreamID, kSSN)}));
  EXPECT_EQ(reasm.queued_bytes(), 0u);

  // The lost chunk comes, but too late.
  EXPECT_TRUE(reasm.HasMessages());
  EXPECT_THAT(reasm.FlushMessages(),
              ElementsAre(SctpMessageIs(kStreamID, kPPID, kMessage2Payload)));
}

TEST_F(ReassemblyQueueTest, ForwardTSNRemoveALotOrdered) {
  ReassemblyQueue reasm("log: ", TSN(10), kBufferSize);
  reasm.Add(TSN(10), gen_.Ordered({1}, "B"));
  reasm.Add(TSN(12), gen_.Ordered({3}));
  reasm.Add(TSN(13), gen_.Ordered({4}, "E"));

  reasm.Add(TSN(15), gen_.Ordered({5}, "B"));
  reasm.Add(TSN(16), gen_.Ordered({6}));
  reasm.Add(TSN(17), gen_.Ordered({7}));
  reasm.Add(TSN(18), gen_.Ordered({8}, "E"));
  EXPECT_EQ(reasm.queued_bytes(), 7u);

  EXPECT_FALSE(reasm.HasMessages());

  reasm.Handle(ForwardTsnChunk(
      TSN(13), {ForwardTsnChunk::SkippedStream(kStreamID, kSSN)}));
  EXPECT_EQ(reasm.queued_bytes(), 0u);

  // The lost chunk comes, but too late.
  EXPECT_TRUE(reasm.HasMessages());
  EXPECT_THAT(reasm.FlushMessages(),
              ElementsAre(SctpMessageIs(kStreamID, kPPID, kMessage2Payload)));
}

TEST_F(ReassemblyQueueTest, ShouldntDeliverMessagesBeforeInitialTsn) {
  ReassemblyQueue reasm("log: ", TSN(10), kBufferSize);
  reasm.Add(TSN(5), gen_.Unordered({1, 2, 3, 4}, "BE"));
  EXPECT_EQ(reasm.queued_bytes(), 0u);
  EXPECT_FALSE(reasm.HasMessages());
}

TEST_F(ReassemblyQueueTest, ShouldntRedeliverUnorderedMessages) {
  ReassemblyQueue reasm("log: ", TSN(10), kBufferSize);
  reasm.Add(TSN(10), gen_.Unordered({1, 2, 3, 4}, "BE"));
  EXPECT_EQ(reasm.queued_bytes(), 0u);
  EXPECT_TRUE(reasm.HasMessages());
  EXPECT_THAT(reasm.FlushMessages(),
              ElementsAre(SctpMessageIs(kStreamID, kPPID, kShortPayload)));
  reasm.Add(TSN(10), gen_.Unordered({1, 2, 3, 4}, "BE"));
  EXPECT_EQ(reasm.queued_bytes(), 0u);
  EXPECT_FALSE(reasm.HasMessages());
}

TEST_F(ReassemblyQueueTest, ShouldntRedeliverUnorderedMessagesReallyUnordered) {
  ReassemblyQueue reasm("log: ", TSN(10), kBufferSize);
  reasm.Add(TSN(10), gen_.Unordered({1, 2, 3, 4}, "B"));
  EXPECT_EQ(reasm.queued_bytes(), 4u);

  EXPECT_FALSE(reasm.HasMessages());

  reasm.Add(TSN(12), gen_.Unordered({1, 2, 3, 4}, "BE"));
  EXPECT_EQ(reasm.queued_bytes(), 4u);
  EXPECT_TRUE(reasm.HasMessages());

  EXPECT_THAT(reasm.FlushMessages(),
              ElementsAre(SctpMessageIs(kStreamID, kPPID, kShortPayload)));
  reasm.Add(TSN(12), gen_.Unordered({1, 2, 3, 4}, "BE"));
  EXPECT_EQ(reasm.queued_bytes(), 4u);
  EXPECT_FALSE(reasm.HasMessages());
}

TEST_F(ReassemblyQueueTest, ShouldntDeliverBeforeForwardedTsn) {
  ReassemblyQueue reasm("log: ", TSN(10), kBufferSize);
  reasm.Handle(ForwardTsnChunk(TSN(12), {}));

  reasm.Add(TSN(12), gen_.Unordered({1, 2, 3, 4}, "BE"));
  EXPECT_EQ(reasm.queued_bytes(), 0u);
  EXPECT_FALSE(reasm.HasMessages());
}

TEST_F(ReassemblyQueueTest, NotReadyForHandoverWhenDeliveredTsnsHaveGap) {
  ReassemblyQueue reasm("log: ", TSN(10), kBufferSize);
  reasm.Add(TSN(10), gen_.Unordered({1, 2, 3, 4}, "B"));
  EXPECT_FALSE(reasm.HasMessages());

  reasm.Add(TSN(12), gen_.Unordered({1, 2, 3, 4}, "BE"));
  EXPECT_TRUE(reasm.HasMessages());
  EXPECT_EQ(
      reasm.GetHandoverReadiness(),
      HandoverReadinessStatus()
          .Add(HandoverUnreadinessReason::kReassemblyQueueDeliveredTSNsGap)
          .Add(
              HandoverUnreadinessReason::kUnorderedStreamHasUnassembledChunks));

  EXPECT_THAT(reasm.FlushMessages(),
              ElementsAre(SctpMessageIs(kStreamID, kPPID, kShortPayload)));
  EXPECT_EQ(
      reasm.GetHandoverReadiness(),
      HandoverReadinessStatus()
          .Add(HandoverUnreadinessReason::kReassemblyQueueDeliveredTSNsGap)
          .Add(
              HandoverUnreadinessReason::kUnorderedStreamHasUnassembledChunks));

  reasm.Handle(ForwardTsnChunk(TSN(13), {}));
  EXPECT_EQ(reasm.GetHandoverReadiness(), HandoverReadinessStatus());
}

TEST_F(ReassemblyQueueTest, NotReadyForHandoverWhenResetStreamIsDeferred) {
  ReassemblyQueue reasm("log: ", TSN(10), kBufferSize);
  DataGeneratorOptions opts;
  opts.message_id = MID(0);
  reasm.Add(TSN(10), gen_.Ordered({1, 2, 3, 4}, "BE", opts));
  opts.message_id = MID(1);
  reasm.Add(TSN(11), gen_.Ordered({1, 2, 3, 4}, "BE", opts));
  EXPECT_THAT(reasm.FlushMessages(), SizeIs(2));

  reasm.ResetStreams(
      OutgoingSSNResetRequestParameter(
          ReconfigRequestSN(10), ReconfigRequestSN(3), TSN(13), {StreamID(1)}),
      TSN(11));
  EXPECT_EQ(reasm.GetHandoverReadiness(),
            HandoverReadinessStatus().Add(
                HandoverUnreadinessReason::kStreamResetDeferred));

  opts.message_id = MID(3);
  opts.ppid = PPID(3);
  reasm.Add(TSN(13), gen_.Ordered({1, 2, 3, 4}, "BE", opts));
  reasm.MaybeResetStreamsDeferred(TSN(11));

  opts.message_id = MID(2);
  opts.ppid = PPID(2);
  reasm.Add(TSN(13), gen_.Ordered({1, 2, 3, 4}, "BE", opts));
  reasm.MaybeResetStreamsDeferred(TSN(15));
  EXPECT_EQ(reasm.GetHandoverReadiness(),
            HandoverReadinessStatus().Add(
                HandoverUnreadinessReason::kReassemblyQueueDeliveredTSNsGap));

  EXPECT_THAT(reasm.FlushMessages(), SizeIs(2));
  EXPECT_EQ(reasm.GetHandoverReadiness(),
            HandoverReadinessStatus().Add(
                HandoverUnreadinessReason::kReassemblyQueueDeliveredTSNsGap));

  reasm.Handle(ForwardTsnChunk(TSN(15), {}));
  EXPECT_EQ(reasm.GetHandoverReadiness(), HandoverReadinessStatus());
}

TEST_F(ReassemblyQueueTest, HandoverInInitialState) {
  ReassemblyQueue reasm1("log: ", TSN(10), kBufferSize);

  EXPECT_EQ(reasm1.GetHandoverReadiness(), HandoverReadinessStatus());
  DcSctpSocketHandoverState state;
  reasm1.AddHandoverState(state);
  g_handover_state_transformer_for_test(&state);
  ReassemblyQueue reasm2("log: ", TSN(100), kBufferSize,
                         /*use_message_interleaving=*/false);
  reasm2.RestoreFromState(state);

  reasm2.Add(TSN(10), gen_.Ordered({1, 2, 3, 4}, "BE"));
  EXPECT_THAT(reasm2.FlushMessages(), SizeIs(1));
}

TEST_F(ReassemblyQueueTest, HandoverAfterHavingAssembedOneMessage) {
  ReassemblyQueue reasm1("log: ", TSN(10), kBufferSize);
  reasm1.Add(TSN(10), gen_.Ordered({1, 2, 3, 4}, "BE"));
  EXPECT_THAT(reasm1.FlushMessages(), SizeIs(1));

  EXPECT_EQ(reasm1.GetHandoverReadiness(), HandoverReadinessStatus());
  DcSctpSocketHandoverState state;
  reasm1.AddHandoverState(state);
  g_handover_state_transformer_for_test(&state);
  ReassemblyQueue reasm2("log: ", TSN(100), kBufferSize,
                         /*use_message_interleaving=*/false);
  reasm2.RestoreFromState(state);

  reasm2.Add(TSN(11), gen_.Ordered({1, 2, 3, 4}, "BE"));
  EXPECT_THAT(reasm2.FlushMessages(), SizeIs(1));
}

TEST_F(ReassemblyQueueTest, HandleInconsistentForwardTSN) {
  // Found when fuzzing.
  ReassemblyQueue reasm("log: ", TSN(10), kBufferSize);
  // Add TSN=43, SSN=7. Can't be reassembled as previous SSNs aren't known.
  reasm.Add(TSN(43), Data(kStreamID, SSN(7), MID(0), FSN(0), kPPID,
                          std::vector<uint8_t>(10), Data::IsBeginning(true),
                          Data::IsEnd(true), IsUnordered(false)));

  // Invalid, as TSN=44 have to have SSN>=7, but peer says 6.
  reasm.Handle(ForwardTsnChunk(
      TSN(44), {ForwardTsnChunk::SkippedStream(kStreamID, SSN(6))}));

  // Don't assemble SSN=7, as that TSN is skipped.
  EXPECT_FALSE(reasm.HasMessages());
}

TEST_F(ReassemblyQueueTest, SingleUnorderedChunkMessageInRfc8260) {
  ReassemblyQueue reasm("log: ", TSN(10), kBufferSize,
                        /*use_message_interleaving=*/true);
  reasm.Add(TSN(10), Data(StreamID(1), SSN(0), MID(0), FSN(0), kPPID,
                          {1, 2, 3, 4}, Data::IsBeginning(true),
                          Data::IsEnd(true), IsUnordered(true)));
  EXPECT_EQ(reasm.queued_bytes(), 0u);
  EXPECT_TRUE(reasm.HasMessages());
  EXPECT_THAT(reasm.FlushMessages(),
              ElementsAre(SctpMessageIs(kStreamID, kPPID, kShortPayload)));
}

TEST_F(ReassemblyQueueTest, TwoInterleavedChunks) {
  ReassemblyQueue reasm("log: ", TSN(10), kBufferSize,
                        /*use_message_interleaving=*/true);
  reasm.Add(TSN(10), Data(StreamID(1), SSN(0), MID(0), FSN(0), kPPID,
                          {1, 2, 3, 4}, Data::IsBeginning(true),
                          Data::IsEnd(false), IsUnordered(true)));
  reasm.Add(TSN(11), Data(StreamID(2), SSN(0), MID(0), FSN(0), kPPID,
                          {9, 10, 11, 12}, Data::IsBeginning(true),
                          Data::IsEnd(false), IsUnordered(true)));
  EXPECT_EQ(reasm.queued_bytes(), 8u);
  reasm.Add(TSN(12), Data(StreamID(1), SSN(0), MID(0), FSN(1), kPPID,
                          {5, 6, 7, 8}, Data::IsBeginning(false),
                          Data::IsEnd(true), IsUnordered(true)));
  EXPECT_EQ(reasm.queued_bytes(), 4u);
  reasm.Add(TSN(13), Data(StreamID(2), SSN(0), MID(0), FSN(1), kPPID,
                          {13, 14, 15, 16}, Data::IsBeginning(false),
                          Data::IsEnd(true), IsUnordered(true)));
  EXPECT_EQ(reasm.queued_bytes(), 0u);
  EXPECT_TRUE(reasm.HasMessages());
  EXPECT_THAT(reasm.FlushMessages(),
              ElementsAre(SctpMessageIs(StreamID(1), kPPID, kMediumPayload1),
                          SctpMessageIs(StreamID(2), kPPID, kMediumPayload2)));
}

TEST_F(ReassemblyQueueTest, UnorderedInterleavedMessagesAllPermutations) {
  std::vector<int> indexes = {0, 1, 2, 3, 4, 5};
  TSN tsns[] = {TSN(10), TSN(11), TSN(12), TSN(13), TSN(14), TSN(15)};
  StreamID stream_ids[] = {StreamID(1), StreamID(2), StreamID(1),
                           StreamID(1), StreamID(2), StreamID(2)};
  FSN fsns[] = {FSN(0), FSN(0), FSN(1), FSN(2), FSN(1), FSN(2)};
  rtc::ArrayView<const uint8_t> payload(kSixBytePayload);
  do {
    ReassemblyQueue reasm("log: ", TSN(10), kBufferSize,
                          /*use_message_interleaving=*/true);
    for (int i : indexes) {
      auto span = payload.subview(*fsns[i] * 2, 2);
      Data::IsBeginning is_beginning(fsns[i] == FSN(0));
      Data::IsEnd is_end(fsns[i] == FSN(2));
      reasm.Add(tsns[i], Data(stream_ids[i], SSN(0), MID(0), fsns[i], kPPID,
                              std::vector<uint8_t>(span.begin(), span.end()),
                              is_beginning, is_end, IsUnordered(true)));
    }
    EXPECT_TRUE(reasm.HasMessages());
    EXPECT_THAT(reasm.FlushMessages(),
                UnorderedElementsAre(
                    SctpMessageIs(StreamID(1), kPPID, kSixBytePayload),
                    SctpMessageIs(StreamID(2), kPPID, kSixBytePayload)));
    EXPECT_EQ(reasm.queued_bytes(), 0u);
  } while (std::next_permutation(std::begin(indexes), std::end(indexes)));
}

TEST_F(ReassemblyQueueTest, IForwardTSNRemoveALotOrdered) {
  ReassemblyQueue reasm("log: ", TSN(10), kBufferSize,
                        /*use_message_interleaving=*/true);
  reasm.Add(TSN(10), gen_.Ordered({1}, "B"));
  gen_.Ordered({2}, "");
  reasm.Add(TSN(12), gen_.Ordered({3}, ""));
  reasm.Add(TSN(13), gen_.Ordered({4}, "E"));
  reasm.Add(TSN(15), gen_.Ordered({5}, "B"));
  reasm.Add(TSN(16), gen_.Ordered({6}, ""));
  reasm.Add(TSN(17), gen_.Ordered({7}, ""));
  reasm.Add(TSN(18), gen_.Ordered({8}, "E"));

  ASSERT_FALSE(reasm.HasMessages());
  EXPECT_EQ(reasm.queued_bytes(), 7u);

  reasm.Handle(
      IForwardTsnChunk(TSN(13), {IForwardTsnChunk::SkippedStream(
                                    IsUnordered(false), kStreamID, MID(0))}));
  EXPECT_EQ(reasm.queued_bytes(), 0u);

  // The lost chunk comes, but too late.
  ASSERT_TRUE(reasm.HasMessages());
  EXPECT_THAT(reasm.FlushMessages(),
              ElementsAre(SctpMessageIs(kStreamID, kPPID, kMessage2Payload)));
}

}  // namespace
}  // namespace dcsctp
