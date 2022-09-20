/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/tx/retransmission_queue.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/task_queue/task_queue_base.h"
#include "net/dcsctp/common/handover_testing.h"
#include "net/dcsctp/common/math.h"
#include "net/dcsctp/packet/chunk/data_chunk.h"
#include "net/dcsctp/packet/chunk/forward_tsn_chunk.h"
#include "net/dcsctp/packet/chunk/forward_tsn_common.h"
#include "net/dcsctp/packet/chunk/iforward_tsn_chunk.h"
#include "net/dcsctp/packet/chunk/sack_chunk.h"
#include "net/dcsctp/packet/data.h"
#include "net/dcsctp/public/dcsctp_options.h"
#include "net/dcsctp/socket/mock_dcsctp_socket_callbacks.h"
#include "net/dcsctp/testing/data_generator.h"
#include "net/dcsctp/testing/testing_macros.h"
#include "net/dcsctp/timer/fake_timeout.h"
#include "net/dcsctp/timer/timer.h"
#include "net/dcsctp/tx/mock_send_queue.h"
#include "net/dcsctp/tx/send_queue.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace dcsctp {
namespace {
using ::testing::MockFunction;
using State = ::dcsctp::RetransmissionQueue::State;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Pair;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

constexpr uint32_t kArwnd = 100000;
constexpr uint32_t kMaxMtu = 1191;

DcSctpOptions MakeOptions() {
  DcSctpOptions options;
  options.mtu = kMaxMtu;
  return options;
}

class RetransmissionQueueTest : public testing::Test {
 protected:
  RetransmissionQueueTest()
      : options_(MakeOptions()),
        gen_(MID(42)),
        timeout_manager_([this]() { return now_; }),
        timer_manager_([this](webrtc::TaskQueueBase::DelayPrecision precision) {
          return timeout_manager_.CreateTimeout(precision);
        }),
        timer_(timer_manager_.CreateTimer(
            "test/t3_rtx",
            []() { return absl::nullopt; },
            TimerOptions(options_.rto_initial))) {}

  std::function<SendQueue::DataToSend(TimeMs, size_t)> CreateChunk() {
    return [this](TimeMs now, size_t max_size) {
      return SendQueue::DataToSend(gen_.Ordered({1, 2, 3, 4}, "BE"));
    };
  }

  std::vector<TSN> GetTSNsForFastRetransmit(RetransmissionQueue& queue) {
    std::vector<TSN> tsns;
    for (const auto& elem : queue.GetChunksForFastRetransmit(10000)) {
      tsns.push_back(elem.first);
    }
    return tsns;
  }

  std::vector<TSN> GetSentPacketTSNs(RetransmissionQueue& queue) {
    std::vector<TSN> tsns;
    for (const auto& elem : queue.GetChunksToSend(now_, 10000)) {
      tsns.push_back(elem.first);
    }
    return tsns;
  }

  RetransmissionQueue CreateQueue(bool supports_partial_reliability = true,
                                  bool use_message_interleaving = false) {
    return RetransmissionQueue(
        "", &callbacks_, TSN(10), kArwnd, producer_, on_rtt_.AsStdFunction(),
        on_clear_retransmission_counter_.AsStdFunction(), *timer_, options_,
        supports_partial_reliability, use_message_interleaving);
  }

  std::unique_ptr<RetransmissionQueue> CreateQueueByHandover(
      RetransmissionQueue& queue) {
    EXPECT_EQ(queue.GetHandoverReadiness(), HandoverReadinessStatus());
    DcSctpSocketHandoverState state;
    queue.AddHandoverState(state);
    g_handover_state_transformer_for_test(&state);
    auto queue2 = std::make_unique<RetransmissionQueue>(
        "", &callbacks_, TSN(10), kArwnd, producer_, on_rtt_.AsStdFunction(),
        on_clear_retransmission_counter_.AsStdFunction(), *timer_, options_,
        /*supports_partial_reliability=*/true,
        /*use_message_interleaving=*/false);
    queue2->RestoreFromState(state);
    return queue2;
  }

  MockDcSctpSocketCallbacks callbacks_;
  DcSctpOptions options_;
  DataGenerator gen_;
  TimeMs now_ = TimeMs(0);
  FakeTimeoutManager timeout_manager_;
  TimerManager timer_manager_;
  NiceMock<MockFunction<void(DurationMs rtt_ms)>> on_rtt_;
  NiceMock<MockFunction<void()>> on_clear_retransmission_counter_;
  NiceMock<MockSendQueue> producer_;
  std::unique_ptr<Timer> timer_;
};

TEST_F(RetransmissionQueueTest, InitialAckedPrevTsn) {
  RetransmissionQueue queue = CreateQueue();
  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked)));
}

TEST_F(RetransmissionQueueTest, SendOneChunk) {
  RetransmissionQueue queue = CreateQueue();
  EXPECT_CALL(producer_, Produce)
      .WillOnce(CreateChunk())
      .WillRepeatedly([](TimeMs, size_t) { return absl::nullopt; });

  EXPECT_THAT(GetSentPacketTSNs(queue), testing::ElementsAre(TSN(10)));

  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),  //
                          Pair(TSN(10), State::kInFlight)));
}

TEST_F(RetransmissionQueueTest, SendOneChunkAndAck) {
  RetransmissionQueue queue = CreateQueue();
  EXPECT_CALL(producer_, Produce)
      .WillOnce(CreateChunk())
      .WillRepeatedly([](TimeMs, size_t) { return absl::nullopt; });

  EXPECT_THAT(GetSentPacketTSNs(queue), testing::ElementsAre(TSN(10)));

  queue.HandleSack(now_, SackChunk(TSN(10), kArwnd, {}, {}));

  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(10), State::kAcked)));
}

TEST_F(RetransmissionQueueTest, SendThreeChunksAndAckTwo) {
  RetransmissionQueue queue = CreateQueue();
  EXPECT_CALL(producer_, Produce)
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillRepeatedly([](TimeMs, size_t) { return absl::nullopt; });

  EXPECT_THAT(GetSentPacketTSNs(queue),
              testing::ElementsAre(TSN(10), TSN(11), TSN(12)));

  queue.HandleSack(now_, SackChunk(TSN(11), kArwnd, {}, {}));

  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(11), State::kAcked),  //
                          Pair(TSN(12), State::kInFlight)));
}

TEST_F(RetransmissionQueueTest, AckWithGapBlocksFromRFC4960Section334) {
  RetransmissionQueue queue = CreateQueue();
  EXPECT_CALL(producer_, Produce)
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillRepeatedly([](TimeMs, size_t) { return absl::nullopt; });

  EXPECT_THAT(GetSentPacketTSNs(queue),
              testing::ElementsAre(TSN(10), TSN(11), TSN(12), TSN(13), TSN(14),
                                   TSN(15), TSN(16), TSN(17)));

  queue.HandleSack(now_, SackChunk(TSN(12), kArwnd,
                                   {SackChunk::GapAckBlock(2, 3),
                                    SackChunk::GapAckBlock(5, 5)},
                                   {}));

  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(12), State::kAcked),   //
                          Pair(TSN(13), State::kNacked),  //
                          Pair(TSN(14), State::kAcked),   //
                          Pair(TSN(15), State::kAcked),   //
                          Pair(TSN(16), State::kNacked),  //
                          Pair(TSN(17), State::kAcked)));
}

TEST_F(RetransmissionQueueTest, ResendPacketsWhenNackedThreeTimes) {
  RetransmissionQueue queue = CreateQueue();
  EXPECT_CALL(producer_, Produce)
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillRepeatedly([](TimeMs, size_t) { return absl::nullopt; });

  EXPECT_THAT(GetSentPacketTSNs(queue),
              testing::ElementsAre(TSN(10), TSN(11), TSN(12), TSN(13), TSN(14),
                                   TSN(15), TSN(16), TSN(17)));

  // Send more chunks, but leave some as gaps to force retransmission after
  // three NACKs.

  // Send 18
  EXPECT_CALL(producer_, Produce)
      .WillOnce(CreateChunk())
      .WillRepeatedly([](TimeMs, size_t) { return absl::nullopt; });
  EXPECT_THAT(GetSentPacketTSNs(queue), testing::ElementsAre(TSN(18)));

  // Ack 12, 14-15, 17-18
  queue.HandleSack(now_, SackChunk(TSN(12), kArwnd,
                                   {SackChunk::GapAckBlock(2, 3),
                                    SackChunk::GapAckBlock(5, 6)},
                                   {}));

  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(12), State::kAcked),   //
                          Pair(TSN(13), State::kNacked),  //
                          Pair(TSN(14), State::kAcked),   //
                          Pair(TSN(15), State::kAcked),   //
                          Pair(TSN(16), State::kNacked),  //
                          Pair(TSN(17), State::kAcked),   //
                          Pair(TSN(18), State::kAcked)));

  // Send 19
  EXPECT_CALL(producer_, Produce)
      .WillOnce(CreateChunk())
      .WillRepeatedly([](TimeMs, size_t) { return absl::nullopt; });
  EXPECT_THAT(GetSentPacketTSNs(queue), testing::ElementsAre(TSN(19)));

  // Ack 12, 14-15, 17-19
  queue.HandleSack(now_, SackChunk(TSN(12), kArwnd,
                                   {SackChunk::GapAckBlock(2, 3),
                                    SackChunk::GapAckBlock(5, 7)},
                                   {}));

  // Send 20
  EXPECT_CALL(producer_, Produce)
      .WillOnce(CreateChunk())
      .WillRepeatedly([](TimeMs, size_t) { return absl::nullopt; });
  EXPECT_THAT(GetSentPacketTSNs(queue), testing::ElementsAre(TSN(20)));

  // Ack 12, 14-15, 17-20
  queue.HandleSack(now_, SackChunk(TSN(12), kArwnd,
                                   {SackChunk::GapAckBlock(2, 3),
                                    SackChunk::GapAckBlock(5, 8)},
                                   {}));

  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(12), State::kAcked),              //
                          Pair(TSN(13), State::kToBeRetransmitted),  //
                          Pair(TSN(14), State::kAcked),              //
                          Pair(TSN(15), State::kAcked),              //
                          Pair(TSN(16), State::kToBeRetransmitted),  //
                          Pair(TSN(17), State::kAcked),              //
                          Pair(TSN(18), State::kAcked),              //
                          Pair(TSN(19), State::kAcked),              //
                          Pair(TSN(20), State::kAcked)));

  // This will trigger "fast retransmit" mode and only chunks 13 and 16 will be
  // resent right now. The send queue will not even be queried.
  EXPECT_CALL(producer_, Produce).Times(0);

  EXPECT_THAT(GetTSNsForFastRetransmit(queue),
              testing::ElementsAre(TSN(13), TSN(16)));

  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(12), State::kAcked),     //
                          Pair(TSN(13), State::kInFlight),  //
                          Pair(TSN(14), State::kAcked),     //
                          Pair(TSN(15), State::kAcked),     //
                          Pair(TSN(16), State::kInFlight),  //
                          Pair(TSN(17), State::kAcked),     //
                          Pair(TSN(18), State::kAcked),     //
                          Pair(TSN(19), State::kAcked),     //
                          Pair(TSN(20), State::kAcked)));
}

TEST_F(RetransmissionQueueTest, RestartsT3RtxOnRetransmitFirstOutstandingTSN) {
  // Verifies that if fast retransmit is retransmitting the first outstanding
  // TSN, it will also restart T3-RTX.
  RetransmissionQueue queue = CreateQueue();
  EXPECT_CALL(producer_, Produce)
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillRepeatedly([](TimeMs, size_t) { return absl::nullopt; });

  static constexpr TimeMs kStartTime(100000);
  now_ = kStartTime;

  EXPECT_THAT(GetSentPacketTSNs(queue),
              testing::ElementsAre(TSN(10), TSN(11), TSN(12)));

  // Ack 10, 12, after 100ms.
  now_ += DurationMs(100);
  queue.HandleSack(
      now_, SackChunk(TSN(10), kArwnd, {SackChunk::GapAckBlock(2, 2)}, {}));

  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(10), State::kAcked),   //
                          Pair(TSN(11), State::kNacked),  //
                          Pair(TSN(12), State::kAcked)));

  // Send 13
  EXPECT_CALL(producer_, Produce)
      .WillOnce(CreateChunk())
      .WillRepeatedly([](TimeMs, size_t) { return absl::nullopt; });
  EXPECT_THAT(GetSentPacketTSNs(queue), testing::ElementsAre(TSN(13)));

  // Ack 10, 12-13, after 100ms.
  now_ += DurationMs(100);
  queue.HandleSack(
      now_, SackChunk(TSN(10), kArwnd, {SackChunk::GapAckBlock(2, 3)}, {}));

  // Send 14
  EXPECT_CALL(producer_, Produce)
      .WillOnce(CreateChunk())
      .WillRepeatedly([](TimeMs, size_t) { return absl::nullopt; });
  EXPECT_THAT(GetSentPacketTSNs(queue), testing::ElementsAre(TSN(14)));

  // Ack 10, 12-14, after 100 ms.
  now_ += DurationMs(100);
  queue.HandleSack(
      now_, SackChunk(TSN(10), kArwnd, {SackChunk::GapAckBlock(2, 4)}, {}));

  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(10), State::kAcked),              //
                          Pair(TSN(11), State::kToBeRetransmitted),  //
                          Pair(TSN(12), State::kAcked),              //
                          Pair(TSN(13), State::kAcked),              //
                          Pair(TSN(14), State::kAcked)));

  // This will trigger "fast retransmit" mode and only chunks 13 and 16 will be
  // resent right now. The send queue will not even be queried.
  EXPECT_CALL(producer_, Produce).Times(0);

  EXPECT_THAT(GetTSNsForFastRetransmit(queue), testing::ElementsAre(TSN(11)));

  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(10), State::kAcked),     //
                          Pair(TSN(11), State::kInFlight),  //
                          Pair(TSN(12), State::kAcked),     //
                          Pair(TSN(13), State::kAcked),     //
                          Pair(TSN(14), State::kAcked)));

  // Verify that the timer was really restarted when fast-retransmitting. The
  // timeout is `options_.rto_initial`, so advance the time just before that.
  now_ += options_.rto_initial - DurationMs(1);
  EXPECT_FALSE(timeout_manager_.GetNextExpiredTimeout().has_value());

  // And ensure it really is running.
  now_ += DurationMs(1);
  ASSERT_HAS_VALUE_AND_ASSIGN(TimeoutID timeout,
                              timeout_manager_.GetNextExpiredTimeout());
  // An expired timeout has to be handled (asserts validate this).
  timer_manager_.HandleTimeout(timeout);
}

TEST_F(RetransmissionQueueTest, CanOnlyProduceTwoPacketsButWantsToSendThree) {
  RetransmissionQueue queue = CreateQueue();
  EXPECT_CALL(producer_, Produce)
      .WillOnce([this](TimeMs, size_t) {
        return SendQueue::DataToSend(gen_.Ordered({1, 2, 3, 4}, "BE"));
      })
      .WillOnce([this](TimeMs, size_t) {
        return SendQueue::DataToSend(gen_.Ordered({1, 2, 3, 4}, "BE"));
      })
      .WillRepeatedly([](TimeMs, size_t) { return absl::nullopt; });

  std::vector<std::pair<TSN, Data>> chunks_to_send =
      queue.GetChunksToSend(now_, 1000);
  EXPECT_THAT(chunks_to_send, ElementsAre(Pair(TSN(10), _), Pair(TSN(11), _)));

  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),      //
                          Pair(TSN(10), State::kInFlight),  //
                          Pair(TSN(11), State::kInFlight)));
}

TEST_F(RetransmissionQueueTest, RetransmitsOnT3Expiry) {
  RetransmissionQueue queue = CreateQueue();
  EXPECT_CALL(producer_, Produce)
      .WillOnce([this](TimeMs, size_t) {
        return SendQueue::DataToSend(gen_.Ordered({1, 2, 3, 4}, "BE"));
      })
      .WillRepeatedly([](TimeMs, size_t) { return absl::nullopt; });

  EXPECT_FALSE(queue.ShouldSendForwardTsn(now_));
  std::vector<std::pair<TSN, Data>> chunks_to_send =
      queue.GetChunksToSend(now_, 1000);
  EXPECT_THAT(chunks_to_send, ElementsAre(Pair(TSN(10), _)));
  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),  //
                          Pair(TSN(10), State::kInFlight)));

  // Will force chunks to be retransmitted
  queue.HandleT3RtxTimerExpiry();

  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),  //
                          Pair(TSN(10), State::kToBeRetransmitted)));

  EXPECT_FALSE(queue.ShouldSendForwardTsn(now_));

  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),  //
                          Pair(TSN(10), State::kToBeRetransmitted)));

  std::vector<std::pair<TSN, Data>> chunks_to_rtx =
      queue.GetChunksToSend(now_, 1000);
  EXPECT_THAT(chunks_to_rtx, ElementsAre(Pair(TSN(10), _)));
  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),  //
                          Pair(TSN(10), State::kInFlight)));
}

TEST_F(RetransmissionQueueTest, LimitedRetransmissionOnlyWithRfc3758Support) {
  RetransmissionQueue queue =
      CreateQueue(/*supports_partial_reliability=*/false);
  EXPECT_CALL(producer_, Produce)
      .WillOnce([this](TimeMs, size_t) {
        SendQueue::DataToSend dts(gen_.Ordered({1, 2, 3, 4}, "BE"));
        dts.max_retransmissions = MaxRetransmits(0);
        return dts;
      })
      .WillRepeatedly([](TimeMs, size_t) { return absl::nullopt; });

  EXPECT_FALSE(queue.ShouldSendForwardTsn(now_));
  std::vector<std::pair<TSN, Data>> chunks_to_send =
      queue.GetChunksToSend(now_, 1000);
  EXPECT_THAT(chunks_to_send, ElementsAre(Pair(TSN(10), _)));
  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),  //
                          Pair(TSN(10), State::kInFlight)));

  // Will force chunks to be retransmitted
  queue.HandleT3RtxTimerExpiry();

  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),  //
                          Pair(TSN(10), State::kToBeRetransmitted)));

  EXPECT_CALL(producer_, Discard(IsUnordered(false), StreamID(1), MID(42)))
      .Times(0);
  EXPECT_FALSE(queue.ShouldSendForwardTsn(now_));
}  // namespace dcsctp

TEST_F(RetransmissionQueueTest, LimitsRetransmissionsAsUdp) {
  RetransmissionQueue queue = CreateQueue();
  EXPECT_CALL(producer_, Produce)
      .WillOnce([this](TimeMs, size_t) {
        SendQueue::DataToSend dts(gen_.Ordered({1, 2, 3, 4}, "BE"));
        dts.max_retransmissions = MaxRetransmits(0);
        return dts;
      })
      .WillRepeatedly([](TimeMs, size_t) { return absl::nullopt; });

  EXPECT_FALSE(queue.ShouldSendForwardTsn(now_));
  std::vector<std::pair<TSN, Data>> chunks_to_send =
      queue.GetChunksToSend(now_, 1000);
  EXPECT_THAT(chunks_to_send, ElementsAre(Pair(TSN(10), _)));
  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),  //
                          Pair(TSN(10), State::kInFlight)));

  // Will force chunks to be retransmitted
  EXPECT_CALL(producer_, Discard(IsUnordered(false), StreamID(1), MID(42)))
      .Times(1);

  queue.HandleT3RtxTimerExpiry();

  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),  //
                          Pair(TSN(10), State::kAbandoned)));

  EXPECT_TRUE(queue.ShouldSendForwardTsn(now_));

  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),  //
                          Pair(TSN(10), State::kAbandoned)));

  std::vector<std::pair<TSN, Data>> chunks_to_rtx =
      queue.GetChunksToSend(now_, 1000);
  EXPECT_THAT(chunks_to_rtx, testing::IsEmpty());
  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),  //
                          Pair(TSN(10), State::kAbandoned)));
}

TEST_F(RetransmissionQueueTest, LimitsRetransmissionsToThreeSends) {
  RetransmissionQueue queue = CreateQueue();
  EXPECT_CALL(producer_, Produce)
      .WillOnce([this](TimeMs, size_t) {
        SendQueue::DataToSend dts(gen_.Ordered({1, 2, 3, 4}, "BE"));
        dts.max_retransmissions = MaxRetransmits(3);
        return dts;
      })
      .WillRepeatedly([](TimeMs, size_t) { return absl::nullopt; });

  EXPECT_FALSE(queue.ShouldSendForwardTsn(now_));
  std::vector<std::pair<TSN, Data>> chunks_to_send =
      queue.GetChunksToSend(now_, 1000);
  EXPECT_THAT(chunks_to_send, ElementsAre(Pair(TSN(10), _)));
  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),  //
                          Pair(TSN(10), State::kInFlight)));

  EXPECT_CALL(producer_, Discard(IsUnordered(false), StreamID(1), MID(42)))
      .Times(0);

  // Retransmission 1
  queue.HandleT3RtxTimerExpiry();
  EXPECT_FALSE(queue.ShouldSendForwardTsn(now_));
  EXPECT_THAT(queue.GetChunksToSend(now_, 1000), SizeIs(1));

  // Retransmission 2
  queue.HandleT3RtxTimerExpiry();
  EXPECT_FALSE(queue.ShouldSendForwardTsn(now_));
  EXPECT_THAT(queue.GetChunksToSend(now_, 1000), SizeIs(1));

  // Retransmission 3
  queue.HandleT3RtxTimerExpiry();
  EXPECT_FALSE(queue.ShouldSendForwardTsn(now_));
  EXPECT_THAT(queue.GetChunksToSend(now_, 1000), SizeIs(1));

  // Retransmission 4 - not allowed.
  EXPECT_CALL(producer_, Discard(IsUnordered(false), StreamID(1), MID(42)))
      .Times(1);
  queue.HandleT3RtxTimerExpiry();
  EXPECT_TRUE(queue.ShouldSendForwardTsn(now_));
  EXPECT_THAT(queue.GetChunksToSend(now_, 1000), IsEmpty());

  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),  //
                          Pair(TSN(10), State::kAbandoned)));
}

TEST_F(RetransmissionQueueTest, RetransmitsWhenSendBufferIsFullT3Expiry) {
  RetransmissionQueue queue = CreateQueue();
  static constexpr size_t kCwnd = 1200;
  queue.set_cwnd(kCwnd);
  EXPECT_EQ(queue.cwnd(), kCwnd);
  EXPECT_EQ(queue.outstanding_bytes(), 0u);
  EXPECT_EQ(queue.outstanding_items(), 0u);

  std::vector<uint8_t> payload(1000);
  EXPECT_CALL(producer_, Produce)
      .WillOnce([this, payload](TimeMs, size_t) {
        return SendQueue::DataToSend(gen_.Ordered(payload, "BE"));
      })
      .WillRepeatedly([](TimeMs, size_t) { return absl::nullopt; });

  std::vector<std::pair<TSN, Data>> chunks_to_send =
      queue.GetChunksToSend(now_, 1500);
  EXPECT_THAT(chunks_to_send, ElementsAre(Pair(TSN(10), _)));
  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),  //
                          Pair(TSN(10), State::kInFlight)));
  EXPECT_EQ(queue.outstanding_bytes(), payload.size() + DataChunk::kHeaderSize);
  EXPECT_EQ(queue.outstanding_items(), 1u);

  // Will force chunks to be retransmitted
  queue.HandleT3RtxTimerExpiry();

  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),  //
                          Pair(TSN(10), State::kToBeRetransmitted)));
  EXPECT_EQ(queue.outstanding_bytes(), 0u);
  EXPECT_EQ(queue.outstanding_items(), 0u);

  std::vector<std::pair<TSN, Data>> chunks_to_rtx =
      queue.GetChunksToSend(now_, 1500);
  EXPECT_THAT(chunks_to_rtx, ElementsAre(Pair(TSN(10), _)));
  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),  //
                          Pair(TSN(10), State::kInFlight)));
  EXPECT_EQ(queue.outstanding_bytes(), payload.size() + DataChunk::kHeaderSize);
  EXPECT_EQ(queue.outstanding_items(), 1u);
}

TEST_F(RetransmissionQueueTest, ProducesValidForwardTsn) {
  RetransmissionQueue queue = CreateQueue();
  EXPECT_CALL(producer_, Produce)
      .WillOnce([this](TimeMs, size_t) {
        SendQueue::DataToSend dts(gen_.Ordered({1, 2, 3, 4}, "B"));
        dts.max_retransmissions = MaxRetransmits(0);
        return dts;
      })
      .WillOnce([this](TimeMs, size_t) {
        SendQueue::DataToSend dts(gen_.Ordered({5, 6, 7, 8}, ""));
        dts.max_retransmissions = MaxRetransmits(0);
        return dts;
      })
      .WillOnce([this](TimeMs, size_t) {
        SendQueue::DataToSend dts(gen_.Ordered({9, 10, 11, 12}, ""));
        dts.max_retransmissions = MaxRetransmits(0);
        return dts;
      })
      .WillRepeatedly([](TimeMs, size_t) { return absl::nullopt; });

  // Send and ack first chunk (TSN 10)
  std::vector<std::pair<TSN, Data>> chunks_to_send =
      queue.GetChunksToSend(now_, 1000);
  EXPECT_THAT(chunks_to_send, ElementsAre(Pair(TSN(10), _), Pair(TSN(11), _),
                                          Pair(TSN(12), _)));
  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),      //
                          Pair(TSN(10), State::kInFlight),  //
                          Pair(TSN(11), State::kInFlight),  //
                          Pair(TSN(12), State::kInFlight)));

  // Chunk 10 is acked, but the remaining are lost
  queue.HandleSack(now_, SackChunk(TSN(10), kArwnd, {}, {}));

  EXPECT_CALL(producer_, Discard(IsUnordered(false), StreamID(1), MID(42)))
      .WillOnce(Return(true));

  queue.HandleT3RtxTimerExpiry();

  // NOTE: The TSN=13 represents the end fragment.
  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(10), State::kAcked),      //
                          Pair(TSN(11), State::kAbandoned),  //
                          Pair(TSN(12), State::kAbandoned),  //
                          Pair(TSN(13), State::kAbandoned)));

  EXPECT_TRUE(queue.ShouldSendForwardTsn(now_));

  ForwardTsnChunk forward_tsn = queue.CreateForwardTsn();
  EXPECT_EQ(forward_tsn.new_cumulative_tsn(), TSN(13));
  EXPECT_THAT(forward_tsn.skipped_streams(),
              UnorderedElementsAre(
                  ForwardTsnChunk::SkippedStream(StreamID(1), SSN(42))));
}

TEST_F(RetransmissionQueueTest, ProducesValidForwardTsnWhenFullySent) {
  RetransmissionQueue queue = CreateQueue();
  EXPECT_CALL(producer_, Produce)
      .WillOnce([this](TimeMs, size_t) {
        SendQueue::DataToSend dts(gen_.Ordered({1, 2, 3, 4}, "B"));
        dts.max_retransmissions = MaxRetransmits(0);
        return dts;
      })
      .WillOnce([this](TimeMs, size_t) {
        SendQueue::DataToSend dts(gen_.Ordered({5, 6, 7, 8}, ""));
        dts.max_retransmissions = MaxRetransmits(0);
        return dts;
      })
      .WillOnce([this](TimeMs, size_t) {
        SendQueue::DataToSend dts(gen_.Ordered({9, 10, 11, 12}, "E"));
        dts.max_retransmissions = MaxRetransmits(0);
        return dts;
      })
      .WillRepeatedly([](TimeMs, size_t) { return absl::nullopt; });

  // Send and ack first chunk (TSN 10)
  std::vector<std::pair<TSN, Data>> chunks_to_send =
      queue.GetChunksToSend(now_, 1000);
  EXPECT_THAT(chunks_to_send, ElementsAre(Pair(TSN(10), _), Pair(TSN(11), _),
                                          Pair(TSN(12), _)));
  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),      //
                          Pair(TSN(10), State::kInFlight),  //
                          Pair(TSN(11), State::kInFlight),  //
                          Pair(TSN(12), State::kInFlight)));

  // Chunk 10 is acked, but the remaining are lost
  queue.HandleSack(now_, SackChunk(TSN(10), kArwnd, {}, {}));

  EXPECT_CALL(producer_, Discard(IsUnordered(false), StreamID(1), MID(42)))
      .WillOnce(Return(false));

  queue.HandleT3RtxTimerExpiry();

  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(10), State::kAcked),      //
                          Pair(TSN(11), State::kAbandoned),  //
                          Pair(TSN(12), State::kAbandoned)));

  EXPECT_TRUE(queue.ShouldSendForwardTsn(now_));

  ForwardTsnChunk forward_tsn = queue.CreateForwardTsn();
  EXPECT_EQ(forward_tsn.new_cumulative_tsn(), TSN(12));
  EXPECT_THAT(forward_tsn.skipped_streams(),
              UnorderedElementsAre(
                  ForwardTsnChunk::SkippedStream(StreamID(1), SSN(42))));
}

TEST_F(RetransmissionQueueTest, ProducesValidIForwardTsn) {
  RetransmissionQueue queue = CreateQueue(/*use_message_interleaving=*/true);
  EXPECT_CALL(producer_, Produce)
      .WillOnce([this](TimeMs, size_t) {
        DataGeneratorOptions opts;
        opts.stream_id = StreamID(1);
        SendQueue::DataToSend dts(gen_.Ordered({1, 2, 3, 4}, "B", opts));
        dts.max_retransmissions = MaxRetransmits(0);
        return dts;
      })
      .WillOnce([this](TimeMs, size_t) {
        DataGeneratorOptions opts;
        opts.stream_id = StreamID(2);
        SendQueue::DataToSend dts(gen_.Unordered({1, 2, 3, 4}, "B", opts));
        dts.max_retransmissions = MaxRetransmits(0);
        return dts;
      })
      .WillOnce([this](TimeMs, size_t) {
        DataGeneratorOptions opts;
        opts.stream_id = StreamID(3);
        SendQueue::DataToSend dts(gen_.Ordered({9, 10, 11, 12}, "B", opts));
        dts.max_retransmissions = MaxRetransmits(0);
        return dts;
      })
      .WillOnce([this](TimeMs, size_t) {
        DataGeneratorOptions opts;
        opts.stream_id = StreamID(4);
        SendQueue::DataToSend dts(gen_.Ordered({13, 14, 15, 16}, "B", opts));
        dts.max_retransmissions = MaxRetransmits(0);
        return dts;
      })
      .WillRepeatedly([](TimeMs, size_t) { return absl::nullopt; });

  std::vector<std::pair<TSN, Data>> chunks_to_send =
      queue.GetChunksToSend(now_, 1000);
  EXPECT_THAT(chunks_to_send, ElementsAre(Pair(TSN(10), _), Pair(TSN(11), _),
                                          Pair(TSN(12), _), Pair(TSN(13), _)));
  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),      //
                          Pair(TSN(10), State::kInFlight),  //
                          Pair(TSN(11), State::kInFlight),  //
                          Pair(TSN(12), State::kInFlight),  //
                          Pair(TSN(13), State::kInFlight)));

  // Chunk 13 is acked, but the remaining are lost
  queue.HandleSack(
      now_, SackChunk(TSN(9), kArwnd, {SackChunk::GapAckBlock(4, 4)}, {}));
  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),    //
                          Pair(TSN(10), State::kNacked),  //
                          Pair(TSN(11), State::kNacked),  //
                          Pair(TSN(12), State::kNacked),  //
                          Pair(TSN(13), State::kAcked)));

  EXPECT_CALL(producer_, Discard(IsUnordered(false), StreamID(1), MID(42)))
      .WillOnce(Return(true));
  EXPECT_CALL(producer_, Discard(IsUnordered(true), StreamID(2), MID(42)))
      .WillOnce(Return(true));
  EXPECT_CALL(producer_, Discard(IsUnordered(false), StreamID(3), MID(42)))
      .WillOnce(Return(true));

  queue.HandleT3RtxTimerExpiry();

  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),       //
                          Pair(TSN(10), State::kAbandoned),  //
                          Pair(TSN(11), State::kAbandoned),  //
                          Pair(TSN(12), State::kAbandoned),  //
                          Pair(TSN(13), State::kAcked),
                          // Representing end fragments of stream 1-3
                          Pair(TSN(14), State::kAbandoned),  //
                          Pair(TSN(15), State::kAbandoned),  //
                          Pair(TSN(16), State::kAbandoned)));

  EXPECT_TRUE(queue.ShouldSendForwardTsn(now_));

  IForwardTsnChunk forward_tsn1 = queue.CreateIForwardTsn();
  EXPECT_EQ(forward_tsn1.new_cumulative_tsn(), TSN(12));
  EXPECT_THAT(
      forward_tsn1.skipped_streams(),
      UnorderedElementsAre(IForwardTsnChunk::SkippedStream(
                               IsUnordered(false), StreamID(1), MID(42)),
                           IForwardTsnChunk::SkippedStream(
                               IsUnordered(true), StreamID(2), MID(42)),
                           IForwardTsnChunk::SkippedStream(
                               IsUnordered(false), StreamID(3), MID(42))));

  // When TSN 13 is acked, the placeholder "end fragments" must be skipped as
  // well.

  // A receiver is more likely to ack TSN 13, but do it incrementally.
  queue.HandleSack(now_, SackChunk(TSN(12), kArwnd, {}, {}));

  EXPECT_CALL(producer_, Discard).Times(0);
  EXPECT_FALSE(queue.ShouldSendForwardTsn(now_));

  queue.HandleSack(now_, SackChunk(TSN(13), kArwnd, {}, {}));
  EXPECT_TRUE(queue.ShouldSendForwardTsn(now_));

  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(13), State::kAcked),      //
                          Pair(TSN(14), State::kAbandoned),  //
                          Pair(TSN(15), State::kAbandoned),  //
                          Pair(TSN(16), State::kAbandoned)));

  IForwardTsnChunk forward_tsn2 = queue.CreateIForwardTsn();
  EXPECT_EQ(forward_tsn2.new_cumulative_tsn(), TSN(16));
  EXPECT_THAT(
      forward_tsn2.skipped_streams(),
      UnorderedElementsAre(IForwardTsnChunk::SkippedStream(
                               IsUnordered(false), StreamID(1), MID(42)),
                           IForwardTsnChunk::SkippedStream(
                               IsUnordered(true), StreamID(2), MID(42)),
                           IForwardTsnChunk::SkippedStream(
                               IsUnordered(false), StreamID(3), MID(42))));
}

TEST_F(RetransmissionQueueTest, MeasureRTT) {
  RetransmissionQueue queue = CreateQueue(/*use_message_interleaving=*/true);
  EXPECT_CALL(producer_, Produce)
      .WillOnce([this](TimeMs, size_t) {
        SendQueue::DataToSend dts(gen_.Ordered({1, 2, 3, 4}, "B"));
        dts.max_retransmissions = MaxRetransmits(0);
        return dts;
      })
      .WillRepeatedly([](TimeMs, size_t) { return absl::nullopt; });

  std::vector<std::pair<TSN, Data>> chunks_to_send =
      queue.GetChunksToSend(now_, 1000);
  EXPECT_THAT(chunks_to_send, ElementsAre(Pair(TSN(10), _)));

  now_ = now_ + DurationMs(123);

  EXPECT_CALL(on_rtt_, Call(DurationMs(123))).Times(1);
  queue.HandleSack(now_, SackChunk(TSN(10), kArwnd, {}, {}));
}

TEST_F(RetransmissionQueueTest, ValidateCumTsnAtRest) {
  RetransmissionQueue queue = CreateQueue(/*use_message_interleaving=*/true);

  EXPECT_FALSE(queue.HandleSack(now_, SackChunk(TSN(8), kArwnd, {}, {})));
  EXPECT_TRUE(queue.HandleSack(now_, SackChunk(TSN(9), kArwnd, {}, {})));
  EXPECT_FALSE(queue.HandleSack(now_, SackChunk(TSN(10), kArwnd, {}, {})));
}

TEST_F(RetransmissionQueueTest, ValidateCumTsnAckOnInflightData) {
  RetransmissionQueue queue = CreateQueue();

  EXPECT_CALL(producer_, Produce)
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillRepeatedly([](TimeMs, size_t) { return absl::nullopt; });

  EXPECT_THAT(GetSentPacketTSNs(queue),
              testing::ElementsAre(TSN(10), TSN(11), TSN(12), TSN(13), TSN(14),
                                   TSN(15), TSN(16), TSN(17)));

  EXPECT_FALSE(queue.HandleSack(now_, SackChunk(TSN(8), kArwnd, {}, {})));
  EXPECT_TRUE(queue.HandleSack(now_, SackChunk(TSN(9), kArwnd, {}, {})));
  EXPECT_TRUE(queue.HandleSack(now_, SackChunk(TSN(10), kArwnd, {}, {})));
  EXPECT_TRUE(queue.HandleSack(now_, SackChunk(TSN(11), kArwnd, {}, {})));
  EXPECT_TRUE(queue.HandleSack(now_, SackChunk(TSN(12), kArwnd, {}, {})));
  EXPECT_TRUE(queue.HandleSack(now_, SackChunk(TSN(13), kArwnd, {}, {})));
  EXPECT_TRUE(queue.HandleSack(now_, SackChunk(TSN(14), kArwnd, {}, {})));
  EXPECT_TRUE(queue.HandleSack(now_, SackChunk(TSN(15), kArwnd, {}, {})));
  EXPECT_TRUE(queue.HandleSack(now_, SackChunk(TSN(16), kArwnd, {}, {})));
  EXPECT_TRUE(queue.HandleSack(now_, SackChunk(TSN(17), kArwnd, {}, {})));
  EXPECT_FALSE(queue.HandleSack(now_, SackChunk(TSN(18), kArwnd, {}, {})));
}

TEST_F(RetransmissionQueueTest, HandleGapAckBlocksMatchingNoInflightData) {
  RetransmissionQueue queue = CreateQueue();
  EXPECT_CALL(producer_, Produce)
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillRepeatedly([](TimeMs, size_t) { return absl::nullopt; });

  EXPECT_THAT(GetSentPacketTSNs(queue),
              testing::ElementsAre(TSN(10), TSN(11), TSN(12), TSN(13), TSN(14),
                                   TSN(15), TSN(16), TSN(17)));

  // Ack 9, 20-25. This is an invalid SACK, but should still be handled.
  queue.HandleSack(
      now_, SackChunk(TSN(9), kArwnd, {SackChunk::GapAckBlock(11, 16)}, {}));

  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),      //
                          Pair(TSN(10), State::kInFlight),  //
                          Pair(TSN(11), State::kInFlight),  //
                          Pair(TSN(12), State::kInFlight),  //
                          Pair(TSN(13), State::kInFlight),  //
                          Pair(TSN(14), State::kInFlight),  //
                          Pair(TSN(15), State::kInFlight),  //
                          Pair(TSN(16), State::kInFlight),  //
                          Pair(TSN(17), State::kInFlight)));
}

TEST_F(RetransmissionQueueTest, HandleInvalidGapAckBlocks) {
  RetransmissionQueue queue = CreateQueue();

  // Nothing produced - nothing in retransmission queue

  // Ack 9, 12-13
  queue.HandleSack(
      now_, SackChunk(TSN(9), kArwnd, {SackChunk::GapAckBlock(3, 4)}, {}));

  // Gap ack blocks are just ignore.
  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked)));
}

TEST_F(RetransmissionQueueTest, GapAckBlocksDoNotMoveCumTsnAck) {
  RetransmissionQueue queue = CreateQueue();
  EXPECT_CALL(producer_, Produce)
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillRepeatedly([](TimeMs, size_t) { return absl::nullopt; });

  EXPECT_THAT(GetSentPacketTSNs(queue),
              testing::ElementsAre(TSN(10), TSN(11), TSN(12), TSN(13), TSN(14),
                                   TSN(15), TSN(16), TSN(17)));

  // Ack 9, 10-14. This is actually an invalid ACK as the first gap can't be
  // adjacent to the cum-tsn-ack, but it's not strictly forbidden. However, the
  // cum-tsn-ack should not move, as the gap-ack-blocks are just advisory.
  queue.HandleSack(
      now_, SackChunk(TSN(9), kArwnd, {SackChunk::GapAckBlock(1, 5)}, {}));

  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),      //
                          Pair(TSN(10), State::kAcked),     //
                          Pair(TSN(11), State::kAcked),     //
                          Pair(TSN(12), State::kAcked),     //
                          Pair(TSN(13), State::kAcked),     //
                          Pair(TSN(14), State::kAcked),     //
                          Pair(TSN(15), State::kInFlight),  //
                          Pair(TSN(16), State::kInFlight),  //
                          Pair(TSN(17), State::kInFlight)));
}

TEST_F(RetransmissionQueueTest, StaysWithinAvailableSize) {
  RetransmissionQueue queue = CreateQueue();

  // See SctpPacketTest::ReturnsCorrectSpaceAvailableToStayWithinMTU for the
  // magic numbers in this test.
  EXPECT_CALL(producer_, Produce)
      .WillOnce([this](TimeMs, size_t size) {
        EXPECT_EQ(size, 1176 - DataChunk::kHeaderSize);

        std::vector<uint8_t> payload(183);
        return SendQueue::DataToSend(gen_.Ordered(payload, "BE"));
      })
      .WillOnce([this](TimeMs, size_t size) {
        EXPECT_EQ(size, 976 - DataChunk::kHeaderSize);

        std::vector<uint8_t> payload(957);
        return SendQueue::DataToSend(gen_.Ordered(payload, "BE"));
      });

  std::vector<std::pair<TSN, Data>> chunks_to_send =
      queue.GetChunksToSend(now_, 1188 - 12);
  EXPECT_THAT(chunks_to_send, ElementsAre(Pair(TSN(10), _), Pair(TSN(11), _)));
}

TEST_F(RetransmissionQueueTest, AccountsNackedAbandonedChunksAsNotOutstanding) {
  RetransmissionQueue queue = CreateQueue();
  EXPECT_CALL(producer_, Produce)
      .WillOnce([this](TimeMs, size_t) {
        SendQueue::DataToSend dts(gen_.Ordered({1, 2, 3, 4}, "B"));
        dts.max_retransmissions = MaxRetransmits(0);
        return dts;
      })
      .WillOnce([this](TimeMs, size_t) {
        SendQueue::DataToSend dts(gen_.Ordered({5, 6, 7, 8}, ""));
        dts.max_retransmissions = MaxRetransmits(0);
        return dts;
      })
      .WillOnce([this](TimeMs, size_t) {
        SendQueue::DataToSend dts(gen_.Ordered({9, 10, 11, 12}, ""));
        dts.max_retransmissions = MaxRetransmits(0);
        return dts;
      })
      .WillRepeatedly([](TimeMs, size_t) { return absl::nullopt; });

  // Send and ack first chunk (TSN 10)
  std::vector<std::pair<TSN, Data>> chunks_to_send =
      queue.GetChunksToSend(now_, 1000);
  EXPECT_THAT(chunks_to_send, ElementsAre(Pair(TSN(10), _), Pair(TSN(11), _),
                                          Pair(TSN(12), _)));
  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),      //
                          Pair(TSN(10), State::kInFlight),  //
                          Pair(TSN(11), State::kInFlight),  //
                          Pair(TSN(12), State::kInFlight)));
  EXPECT_EQ(queue.outstanding_bytes(), (16 + 4) * 3u);
  EXPECT_EQ(queue.outstanding_items(), 3u);

  // Mark the message as lost.
  EXPECT_CALL(producer_, Discard(IsUnordered(false), StreamID(1), MID(42)))
      .Times(1);
  queue.HandleT3RtxTimerExpiry();

  EXPECT_TRUE(queue.ShouldSendForwardTsn(now_));

  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),       //
                          Pair(TSN(10), State::kAbandoned),  //
                          Pair(TSN(11), State::kAbandoned),  //
                          Pair(TSN(12), State::kAbandoned)));
  EXPECT_EQ(queue.outstanding_bytes(), 0u);
  EXPECT_EQ(queue.outstanding_items(), 0u);

  // Now ACK those, one at a time.
  queue.HandleSack(now_, SackChunk(TSN(10), kArwnd, {}, {}));
  EXPECT_EQ(queue.outstanding_bytes(), 0u);
  EXPECT_EQ(queue.outstanding_items(), 0u);

  queue.HandleSack(now_, SackChunk(TSN(11), kArwnd, {}, {}));
  EXPECT_EQ(queue.outstanding_bytes(), 0u);
  EXPECT_EQ(queue.outstanding_items(), 0u);

  queue.HandleSack(now_, SackChunk(TSN(12), kArwnd, {}, {}));
  EXPECT_EQ(queue.outstanding_bytes(), 0u);
  EXPECT_EQ(queue.outstanding_items(), 0u);
}

TEST_F(RetransmissionQueueTest, ExpireFromSendQueueWhenPartiallySent) {
  RetransmissionQueue queue = CreateQueue();
  DataGeneratorOptions options;
  options.stream_id = StreamID(17);
  options.message_id = MID(42);
  TimeMs test_start = now_;
  EXPECT_CALL(producer_, Produce)
      .WillOnce([&](TimeMs, size_t) {
        SendQueue::DataToSend dts(gen_.Ordered({1, 2, 3, 4}, "B", options));
        dts.expires_at = TimeMs(test_start + DurationMs(10));
        return dts;
      })
      .WillOnce([&](TimeMs, size_t) {
        SendQueue::DataToSend dts(gen_.Ordered({5, 6, 7, 8}, "", options));
        dts.expires_at = TimeMs(test_start + DurationMs(10));
        return dts;
      })
      .WillRepeatedly([](TimeMs, size_t) { return absl::nullopt; });

  std::vector<std::pair<TSN, Data>> chunks_to_send =
      queue.GetChunksToSend(now_, 24);
  EXPECT_THAT(chunks_to_send, ElementsAre(Pair(TSN(10), _)));

  EXPECT_CALL(producer_, Discard(IsUnordered(false), StreamID(17), MID(42)))
      .WillOnce(Return(true));
  now_ += DurationMs(100);

  EXPECT_THAT(queue.GetChunksToSend(now_, 24), IsEmpty());

  EXPECT_THAT(
      queue.GetChunkStatesForTesting(),
      ElementsAre(Pair(TSN(9), State::kAcked),         // Initial TSN
                  Pair(TSN(10), State::kAbandoned),    // Produced
                  Pair(TSN(11), State::kAbandoned),    // Produced and expired
                  Pair(TSN(12), State::kAbandoned)));  // Placeholder end
}

TEST_F(RetransmissionQueueTest, LimitsRetransmissionsOnlyWhenNackedThreeTimes) {
  RetransmissionQueue queue = CreateQueue();
  EXPECT_CALL(producer_, Produce)
      .WillOnce([this](TimeMs, size_t) {
        SendQueue::DataToSend dts(gen_.Ordered({1, 2, 3, 4}, "BE"));
        dts.max_retransmissions = MaxRetransmits(0);
        return dts;
      })
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillRepeatedly([](TimeMs, size_t) { return absl::nullopt; });

  EXPECT_FALSE(queue.ShouldSendForwardTsn(now_));

  std::vector<std::pair<TSN, Data>> chunks_to_send =
      queue.GetChunksToSend(now_, 1000);
  EXPECT_THAT(chunks_to_send, ElementsAre(Pair(TSN(10), _), Pair(TSN(11), _),
                                          Pair(TSN(12), _), Pair(TSN(13), _)));
  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),      //
                          Pair(TSN(10), State::kInFlight),  //
                          Pair(TSN(11), State::kInFlight),  //
                          Pair(TSN(12), State::kInFlight),  //
                          Pair(TSN(13), State::kInFlight)));

  EXPECT_FALSE(queue.ShouldSendForwardTsn(now_));

  EXPECT_CALL(producer_, Discard(IsUnordered(false), StreamID(1), MID(42)))
      .Times(0);

  queue.HandleSack(
      now_, SackChunk(TSN(9), kArwnd, {SackChunk::GapAckBlock(2, 2)}, {}));

  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),      //
                          Pair(TSN(10), State::kNacked),    //
                          Pair(TSN(11), State::kAcked),     //
                          Pair(TSN(12), State::kInFlight),  //
                          Pair(TSN(13), State::kInFlight)));

  EXPECT_FALSE(queue.ShouldSendForwardTsn(now_));

  queue.HandleSack(
      now_, SackChunk(TSN(9), kArwnd, {SackChunk::GapAckBlock(2, 3)}, {}));

  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),    //
                          Pair(TSN(10), State::kNacked),  //
                          Pair(TSN(11), State::kAcked),   //
                          Pair(TSN(12), State::kAcked),   //
                          Pair(TSN(13), State::kInFlight)));

  EXPECT_FALSE(queue.ShouldSendForwardTsn(now_));

  EXPECT_CALL(producer_, Discard(IsUnordered(false), StreamID(1), MID(42)))
      .WillOnce(Return(false));
  queue.HandleSack(
      now_, SackChunk(TSN(9), kArwnd, {SackChunk::GapAckBlock(2, 4)}, {}));

  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),       //
                          Pair(TSN(10), State::kAbandoned),  //
                          Pair(TSN(11), State::kAcked),      //
                          Pair(TSN(12), State::kAcked),      //
                          Pair(TSN(13), State::kAcked)));

  EXPECT_TRUE(queue.ShouldSendForwardTsn(now_));
}

TEST_F(RetransmissionQueueTest, AbandonsRtxLimit2WhenNackedNineTimes) {
  // This is a fairly long test.
  RetransmissionQueue queue = CreateQueue();
  EXPECT_CALL(producer_, Produce)
      .WillOnce([this](TimeMs, size_t) {
        SendQueue::DataToSend dts(gen_.Ordered({1, 2, 3, 4}, "BE"));
        dts.max_retransmissions = MaxRetransmits(2);
        return dts;
      })
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillRepeatedly([](TimeMs, size_t) { return absl::nullopt; });

  EXPECT_FALSE(queue.ShouldSendForwardTsn(now_));

  std::vector<std::pair<TSN, Data>> chunks_to_send =
      queue.GetChunksToSend(now_, 1000);
  EXPECT_THAT(chunks_to_send,
              ElementsAre(Pair(TSN(10), _), Pair(TSN(11), _), Pair(TSN(12), _),
                          Pair(TSN(13), _), Pair(TSN(14), _), Pair(TSN(15), _),
                          Pair(TSN(16), _), Pair(TSN(17), _), Pair(TSN(18), _),
                          Pair(TSN(19), _)));

  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),      //
                          Pair(TSN(10), State::kInFlight),  //
                          Pair(TSN(11), State::kInFlight),  //
                          Pair(TSN(12), State::kInFlight),  //
                          Pair(TSN(13), State::kInFlight),  //
                          Pair(TSN(14), State::kInFlight),  //
                          Pair(TSN(15), State::kInFlight),  //
                          Pair(TSN(16), State::kInFlight),  //
                          Pair(TSN(17), State::kInFlight),  //
                          Pair(TSN(18), State::kInFlight),  //
                          Pair(TSN(19), State::kInFlight)));

  EXPECT_CALL(producer_, Discard(IsUnordered(false), StreamID(1), MID(42)))
      .Times(0);

  // Ack TSN [11 to 13] - three nacks for TSN(10), which will retransmit it.
  for (int tsn = 11; tsn <= 13; ++tsn) {
    queue.HandleSack(
        now_,
        SackChunk(TSN(9), kArwnd, {SackChunk::GapAckBlock(2, (tsn - 9))}, {}));
  }

  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),               //
                          Pair(TSN(10), State::kToBeRetransmitted),  //
                          Pair(TSN(11), State::kAcked),              //
                          Pair(TSN(12), State::kAcked),              //
                          Pair(TSN(13), State::kAcked),              //
                          Pair(TSN(14), State::kInFlight),           //
                          Pair(TSN(15), State::kInFlight),           //
                          Pair(TSN(16), State::kInFlight),           //
                          Pair(TSN(17), State::kInFlight),           //
                          Pair(TSN(18), State::kInFlight),           //
                          Pair(TSN(19), State::kInFlight)));

  EXPECT_THAT(queue.GetChunksForFastRetransmit(1000),
              ElementsAre(Pair(TSN(10), _)));

  // Ack TSN [14 to 16] - three more nacks - second and last retransmission.
  for (int tsn = 14; tsn <= 16; ++tsn) {
    queue.HandleSack(
        now_,
        SackChunk(TSN(9), kArwnd, {SackChunk::GapAckBlock(2, (tsn - 9))}, {}));
  }

  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),               //
                          Pair(TSN(10), State::kToBeRetransmitted),  //
                          Pair(TSN(11), State::kAcked),              //
                          Pair(TSN(12), State::kAcked),              //
                          Pair(TSN(13), State::kAcked),              //
                          Pair(TSN(14), State::kAcked),              //
                          Pair(TSN(15), State::kAcked),              //
                          Pair(TSN(16), State::kAcked),              //
                          Pair(TSN(17), State::kInFlight),           //
                          Pair(TSN(18), State::kInFlight),           //
                          Pair(TSN(19), State::kInFlight)));

  EXPECT_THAT(queue.GetChunksToSend(now_, 1000), ElementsAre(Pair(TSN(10), _)));

  // Ack TSN [17 to 18]
  for (int tsn = 17; tsn <= 18; ++tsn) {
    queue.HandleSack(
        now_,
        SackChunk(TSN(9), kArwnd, {SackChunk::GapAckBlock(2, (tsn - 9))}, {}));
  }

  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),    //
                          Pair(TSN(10), State::kNacked),  //
                          Pair(TSN(11), State::kAcked),   //
                          Pair(TSN(12), State::kAcked),   //
                          Pair(TSN(13), State::kAcked),   //
                          Pair(TSN(14), State::kAcked),   //
                          Pair(TSN(15), State::kAcked),   //
                          Pair(TSN(16), State::kAcked),   //
                          Pair(TSN(17), State::kAcked),   //
                          Pair(TSN(18), State::kAcked),   //
                          Pair(TSN(19), State::kInFlight)));

  EXPECT_FALSE(queue.ShouldSendForwardTsn(now_));

  // Ack TSN 19 - three more nacks for TSN 10, no more retransmissions.
  EXPECT_CALL(producer_, Discard(IsUnordered(false), StreamID(1), MID(42)))
      .WillOnce(Return(false));
  queue.HandleSack(
      now_, SackChunk(TSN(9), kArwnd, {SackChunk::GapAckBlock(2, 10)}, {}));

  EXPECT_THAT(queue.GetChunksToSend(now_, 1000), IsEmpty());

  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),       //
                          Pair(TSN(10), State::kAbandoned),  //
                          Pair(TSN(11), State::kAcked),      //
                          Pair(TSN(12), State::kAcked),      //
                          Pair(TSN(13), State::kAcked),      //
                          Pair(TSN(14), State::kAcked),      //
                          Pair(TSN(15), State::kAcked),      //
                          Pair(TSN(16), State::kAcked),      //
                          Pair(TSN(17), State::kAcked),      //
                          Pair(TSN(18), State::kAcked),      //
                          Pair(TSN(19), State::kAcked)));

  EXPECT_TRUE(queue.ShouldSendForwardTsn(now_));
}

TEST_F(RetransmissionQueueTest, CwndRecoversWhenAcking) {
  RetransmissionQueue queue = CreateQueue();
  static constexpr size_t kCwnd = 1200;
  queue.set_cwnd(kCwnd);
  EXPECT_EQ(queue.cwnd(), kCwnd);

  std::vector<uint8_t> payload(1000);
  EXPECT_CALL(producer_, Produce)
      .WillOnce([this, payload](TimeMs, size_t) {
        return SendQueue::DataToSend(gen_.Ordered(payload, "BE"));
      })
      .WillRepeatedly([](TimeMs, size_t) { return absl::nullopt; });

  std::vector<std::pair<TSN, Data>> chunks_to_send =
      queue.GetChunksToSend(now_, 1500);
  EXPECT_THAT(chunks_to_send, ElementsAre(Pair(TSN(10), _)));
  size_t serialized_size = payload.size() + DataChunk::kHeaderSize;
  EXPECT_EQ(queue.outstanding_bytes(), serialized_size);

  queue.HandleSack(now_, SackChunk(TSN(10), kArwnd, {}, {}));

  EXPECT_EQ(queue.cwnd(), kCwnd + serialized_size);
}

// Verifies that it doesn't produce tiny packets, when getting close to
// the full congestion window.
TEST_F(RetransmissionQueueTest, OnlySendsLargePacketsOnLargeCongestionWindow) {
  RetransmissionQueue queue = CreateQueue();
  size_t intial_cwnd = options_.avoid_fragmentation_cwnd_mtus * options_.mtu;
  queue.set_cwnd(intial_cwnd);
  EXPECT_EQ(queue.cwnd(), intial_cwnd);

  // Fill the congestion window almost - leaving 500 bytes.
  size_t chunk_size = intial_cwnd - 500;
  EXPECT_CALL(producer_, Produce)
      .WillOnce([chunk_size, this](TimeMs, size_t) {
        return SendQueue::DataToSend(
            gen_.Ordered(std::vector<uint8_t>(chunk_size), "BE"));
      })
      .WillRepeatedly([](TimeMs, size_t) { return absl::nullopt; });

  EXPECT_TRUE(queue.can_send_data());
  std::vector<std::pair<TSN, Data>> chunks_to_send =
      queue.GetChunksToSend(now_, 10000);
  EXPECT_THAT(chunks_to_send, ElementsAre(Pair(TSN(10), _)));

  // To little space left - will not send more.
  EXPECT_FALSE(queue.can_send_data());

  // But when the first chunk is acked, it will continue.
  queue.HandleSack(now_, SackChunk(TSN(10), kArwnd, {}, {}));

  EXPECT_TRUE(queue.can_send_data());
  EXPECT_EQ(queue.outstanding_bytes(), 0u);
  EXPECT_EQ(queue.cwnd(), intial_cwnd + kMaxMtu);
}

TEST_F(RetransmissionQueueTest, AllowsSmallFragmentsOnSmallCongestionWindow) {
  RetransmissionQueue queue = CreateQueue();
  size_t intial_cwnd =
      options_.avoid_fragmentation_cwnd_mtus * options_.mtu - 1;
  queue.set_cwnd(intial_cwnd);
  EXPECT_EQ(queue.cwnd(), intial_cwnd);

  // Fill the congestion window almost - leaving 500 bytes.
  size_t chunk_size = intial_cwnd - 500;
  EXPECT_CALL(producer_, Produce)
      .WillOnce([chunk_size, this](TimeMs, size_t) {
        return SendQueue::DataToSend(
            gen_.Ordered(std::vector<uint8_t>(chunk_size), "BE"));
      })
      .WillRepeatedly([](TimeMs, size_t) { return absl::nullopt; });

  EXPECT_TRUE(queue.can_send_data());
  std::vector<std::pair<TSN, Data>> chunks_to_send =
      queue.GetChunksToSend(now_, 10000);
  EXPECT_THAT(chunks_to_send, ElementsAre(Pair(TSN(10), _)));

  // With congestion window under limit, allow small packets to be created.
  EXPECT_TRUE(queue.can_send_data());
}

TEST_F(RetransmissionQueueTest, ReadyForHandoverWhenHasNoOutstandingData) {
  RetransmissionQueue queue = CreateQueue();
  EXPECT_CALL(producer_, Produce)
      .WillOnce(CreateChunk())
      .WillRepeatedly([](TimeMs, size_t) { return absl::nullopt; });

  EXPECT_THAT(GetSentPacketTSNs(queue), SizeIs(1));
  EXPECT_EQ(
      queue.GetHandoverReadiness(),
      HandoverReadinessStatus(
          HandoverUnreadinessReason::kRetransmissionQueueOutstandingData));

  queue.HandleSack(now_, SackChunk(TSN(10), kArwnd, {}, {}));
  EXPECT_EQ(queue.GetHandoverReadiness(), HandoverReadinessStatus());
}

TEST_F(RetransmissionQueueTest, ReadyForHandoverWhenNothingToRetransmit) {
  RetransmissionQueue queue = CreateQueue();
  EXPECT_CALL(producer_, Produce)
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillRepeatedly([](TimeMs, size_t) { return absl::nullopt; });
  EXPECT_THAT(GetSentPacketTSNs(queue), SizeIs(8));
  EXPECT_EQ(
      queue.GetHandoverReadiness(),
      HandoverReadinessStatus(
          HandoverUnreadinessReason::kRetransmissionQueueOutstandingData));

  // Send more chunks, but leave some chunks unacked to force retransmission
  // after three NACKs.

  // Send 18
  EXPECT_CALL(producer_, Produce)
      .WillOnce(CreateChunk())
      .WillRepeatedly([](TimeMs, size_t) { return absl::nullopt; });
  EXPECT_THAT(GetSentPacketTSNs(queue), SizeIs(1));

  // Ack 12, 14-15, 17-18
  queue.HandleSack(now_, SackChunk(TSN(12), kArwnd,
                                   {SackChunk::GapAckBlock(2, 3),
                                    SackChunk::GapAckBlock(5, 6)},
                                   {}));

  // Send 19
  EXPECT_CALL(producer_, Produce)
      .WillOnce(CreateChunk())
      .WillRepeatedly([](TimeMs, size_t) { return absl::nullopt; });
  EXPECT_THAT(GetSentPacketTSNs(queue), SizeIs(1));

  // Ack 12, 14-15, 17-19
  queue.HandleSack(now_, SackChunk(TSN(12), kArwnd,
                                   {SackChunk::GapAckBlock(2, 3),
                                    SackChunk::GapAckBlock(5, 7)},
                                   {}));

  // Send 20
  EXPECT_CALL(producer_, Produce)
      .WillOnce(CreateChunk())
      .WillRepeatedly([](TimeMs, size_t) { return absl::nullopt; });
  EXPECT_THAT(GetSentPacketTSNs(queue), SizeIs(1));

  // Ack 12, 14-15, 17-20
  // This will trigger "fast retransmit" mode and only chunks 13 and 16 will be
  // resent right now. The send queue will not even be queried.
  queue.HandleSack(now_, SackChunk(TSN(12), kArwnd,
                                   {SackChunk::GapAckBlock(2, 3),
                                    SackChunk::GapAckBlock(5, 8)},
                                   {}));
  EXPECT_EQ(
      queue.GetHandoverReadiness(),
      HandoverReadinessStatus()
          .Add(HandoverUnreadinessReason::kRetransmissionQueueOutstandingData)
          .Add(HandoverUnreadinessReason::kRetransmissionQueueFastRecovery)
          .Add(HandoverUnreadinessReason::kRetransmissionQueueNotEmpty));

  // Send "fast retransmit" mode chunks
  EXPECT_CALL(producer_, Produce).Times(0);
  EXPECT_THAT(GetTSNsForFastRetransmit(queue), SizeIs(2));
  EXPECT_EQ(
      queue.GetHandoverReadiness(),
      HandoverReadinessStatus()
          .Add(HandoverUnreadinessReason::kRetransmissionQueueOutstandingData)
          .Add(HandoverUnreadinessReason::kRetransmissionQueueFastRecovery));

  // Ack 20 to confirm the retransmission
  queue.HandleSack(now_, SackChunk(TSN(20), kArwnd, {}, {}));
  EXPECT_EQ(queue.GetHandoverReadiness(), HandoverReadinessStatus());
}

TEST_F(RetransmissionQueueTest, HandoverTest) {
  RetransmissionQueue queue = CreateQueue();
  EXPECT_CALL(producer_, Produce)
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillRepeatedly([](TimeMs, size_t) { return absl::nullopt; });
  EXPECT_THAT(GetSentPacketTSNs(queue), SizeIs(2));
  queue.HandleSack(now_, SackChunk(TSN(11), kArwnd, {}, {}));

  std::unique_ptr<RetransmissionQueue> handedover_queue =
      CreateQueueByHandover(queue);

  EXPECT_CALL(producer_, Produce)
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillOnce(CreateChunk())
      .WillRepeatedly([](TimeMs, size_t) { return absl::nullopt; });
  EXPECT_THAT(GetSentPacketTSNs(*handedover_queue),
              testing::ElementsAre(TSN(12), TSN(13), TSN(14)));

  handedover_queue->HandleSack(now_, SackChunk(TSN(13), kArwnd, {}, {}));
  EXPECT_THAT(handedover_queue->GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(13), State::kAcked),  //
                          Pair(TSN(14), State::kInFlight)));
}

TEST_F(RetransmissionQueueTest, CanAlwaysSendOnePacket) {
  RetransmissionQueue queue = CreateQueue();

  // A large payload - enough to not fit two DATA in same packet.
  size_t mtu = RoundDownTo4(options_.mtu);
  std::vector<uint8_t> payload(mtu - 100);

  EXPECT_CALL(producer_, Produce)
      .WillOnce([this, payload](TimeMs, size_t) {
        return SendQueue::DataToSend(gen_.Ordered(payload, "B"));
      })
      .WillOnce([this, payload](TimeMs, size_t) {
        return SendQueue::DataToSend(gen_.Ordered(payload, ""));
      })
      .WillOnce([this, payload](TimeMs, size_t) {
        return SendQueue::DataToSend(gen_.Ordered(payload, ""));
      })
      .WillOnce([this, payload](TimeMs, size_t) {
        return SendQueue::DataToSend(gen_.Ordered(payload, ""));
      })
      .WillOnce([this, payload](TimeMs, size_t) {
        return SendQueue::DataToSend(gen_.Ordered(payload, "E"));
      })
      .WillRepeatedly([](TimeMs, size_t) { return absl::nullopt; });

  // Produce all chunks and put them in the retransmission queue.
  std::vector<std::pair<TSN, Data>> chunks_to_send =
      queue.GetChunksToSend(now_, 5 * mtu);
  EXPECT_THAT(chunks_to_send,
              ElementsAre(Pair(TSN(10), _), Pair(TSN(11), _), Pair(TSN(12), _),
                          Pair(TSN(13), _), Pair(TSN(14), _)));
  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),      //
                          Pair(TSN(10), State::kInFlight),  //
                          Pair(TSN(11), State::kInFlight),  //
                          Pair(TSN(12), State::kInFlight),
                          Pair(TSN(13), State::kInFlight),
                          Pair(TSN(14), State::kInFlight)));

  // Ack 12, and report an empty receiver window (the peer obviously has a
  // tiny receive window).
  queue.HandleSack(
      now_, SackChunk(TSN(9), /*rwnd=*/0, {SackChunk::GapAckBlock(3, 3)}, {}));

  // Force TSN 10 to be retransmitted.
  queue.HandleT3RtxTimerExpiry();

  // Even if the receiver window is empty, it will allow TSN 10 to be sent.
  EXPECT_THAT(queue.GetChunksToSend(now_, mtu), ElementsAre(Pair(TSN(10), _)));

  // But not more than that, as there now is outstanding data.
  EXPECT_THAT(queue.GetChunksToSend(now_, mtu), IsEmpty());

  // Don't ack any new data, and still have receiver window zero.
  queue.HandleSack(
      now_, SackChunk(TSN(9), /*rwnd=*/0, {SackChunk::GapAckBlock(3, 3)}, {}));

  // There is in-flight data, so new data should not be allowed to be send since
  // the receiver window is full.
  EXPECT_THAT(queue.GetChunksToSend(now_, mtu), IsEmpty());

  // Ack that packet (no more in-flight data), but still report an empty
  // receiver window.
  queue.HandleSack(
      now_, SackChunk(TSN(10), /*rwnd=*/0, {SackChunk::GapAckBlock(2, 2)}, {}));

  // Then TSN 11 can be sent, as there is no in-flight data.
  EXPECT_THAT(queue.GetChunksToSend(now_, mtu), ElementsAre(Pair(TSN(11), _)));
  EXPECT_THAT(queue.GetChunksToSend(now_, mtu), IsEmpty());

  // Ack and recover the receiver window
  queue.HandleSack(now_, SackChunk(TSN(12), /*rwnd=*/5 * mtu, {}, {}));

  // That will unblock sending remaining chunks.
  EXPECT_THAT(queue.GetChunksToSend(now_, mtu), ElementsAre(Pair(TSN(13), _)));
  EXPECT_THAT(queue.GetChunksToSend(now_, mtu), ElementsAre(Pair(TSN(14), _)));
  EXPECT_THAT(queue.GetChunksToSend(now_, mtu), IsEmpty());
}

}  // namespace
}  // namespace dcsctp
