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
#include "net/dcsctp/packet/chunk/data_chunk.h"
#include "net/dcsctp/packet/chunk/forward_tsn_chunk.h"
#include "net/dcsctp/packet/chunk/forward_tsn_common.h"
#include "net/dcsctp/packet/chunk/iforward_tsn_chunk.h"
#include "net/dcsctp/packet/chunk/sack_chunk.h"
#include "net/dcsctp/packet/data.h"
#include "net/dcsctp/public/dcsctp_options.h"
#include "net/dcsctp/testing/data_generator.h"
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
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

constexpr uint32_t kArwnd = 100000;
constexpr uint32_t kMaxMtu = 1191;

class RetransmissionQueueTest : public testing::Test {
 protected:
  RetransmissionQueueTest()
      : gen_(MID(42)),
        timeout_manager_([this]() { return now_; }),
        timer_manager_([this]() { return timeout_manager_.CreateTimeout(); }),
        timer_(timer_manager_.CreateTimer(
            "test/t3_rtx",
            []() { return absl::nullopt; },
            TimerOptions(DurationMs(0)))) {}

  std::function<SendQueue::DataToSend(TimeMs, size_t)> CreateChunk() {
    return [this](TimeMs now, size_t max_size) {
      return SendQueue::DataToSend(gen_.Ordered({1, 2, 3, 4}, "BE"));
    };
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
    DcSctpOptions options;
    options.mtu = kMaxMtu;
    return RetransmissionQueue(
        "", TSN(10), kArwnd, producer_, on_rtt_.AsStdFunction(),
        on_outgoing_message_buffer_empty_.AsStdFunction(),
        on_clear_retransmission_counter_.AsStdFunction(), *timer_, options,
        supports_partial_reliability, use_message_interleaving);
  }

  DataGenerator gen_;
  TimeMs now_ = TimeMs(0);
  FakeTimeoutManager timeout_manager_;
  TimerManager timer_manager_;
  NiceMock<MockFunction<void(DurationMs rtt_ms)>> on_rtt_;
  NiceMock<MockFunction<void()>> on_outgoing_message_buffer_empty_;
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

  EXPECT_THAT(GetSentPacketTSNs(queue), testing::ElementsAre(TSN(13), TSN(16)));

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
        dts.max_retransmissions = 0;
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
        dts.max_retransmissions = 0;
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
      .Times(1);

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
        dts.max_retransmissions = 3;
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
  queue.HandleT3RtxTimerExpiry();
  EXPECT_CALL(producer_, Discard(IsUnordered(false), StreamID(1), MID(42)))
      .Times(1);
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

  // Will force chunks to be retransmitted
  queue.HandleT3RtxTimerExpiry();

  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),  //
                          Pair(TSN(10), State::kToBeRetransmitted)));
  EXPECT_EQ(queue.outstanding_bytes(), 0u);

  std::vector<std::pair<TSN, Data>> chunks_to_rtx =
      queue.GetChunksToSend(now_, 1500);
  EXPECT_THAT(chunks_to_rtx, ElementsAre(Pair(TSN(10), _)));
  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),  //
                          Pair(TSN(10), State::kInFlight)));
  EXPECT_EQ(queue.outstanding_bytes(), payload.size() + DataChunk::kHeaderSize);
}

TEST_F(RetransmissionQueueTest, ProducesValidForwardTsn) {
  RetransmissionQueue queue = CreateQueue();
  EXPECT_CALL(producer_, Produce)
      .WillOnce([this](TimeMs, size_t) {
        SendQueue::DataToSend dts(gen_.Ordered({1, 2, 3, 4}, "B"));
        dts.max_retransmissions = 0;
        return dts;
      })
      .WillOnce([this](TimeMs, size_t) {
        SendQueue::DataToSend dts(gen_.Ordered({5, 6, 7, 8}, ""));
        dts.max_retransmissions = 0;
        return dts;
      })
      .WillOnce([this](TimeMs, size_t) {
        SendQueue::DataToSend dts(gen_.Ordered({9, 10, 11, 12}, ""));
        dts.max_retransmissions = 0;
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
  queue.HandleT3RtxTimerExpiry();

  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(10), State::kAcked),              //
                          Pair(TSN(11), State::kToBeRetransmitted),  //
                          Pair(TSN(12), State::kToBeRetransmitted)));

  EXPECT_CALL(producer_, Discard(IsUnordered(false), StreamID(1), MID(42)))
      .Times(1);
  EXPECT_TRUE(queue.ShouldSendForwardTsn(now_));

  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(10), State::kAcked),      //
                          Pair(TSN(11), State::kAbandoned),  //
                          Pair(TSN(12), State::kAbandoned)));

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
        dts.max_retransmissions = 0;
        return dts;
      })
      .WillOnce([this](TimeMs, size_t) {
        DataGeneratorOptions opts;
        opts.stream_id = StreamID(2);
        SendQueue::DataToSend dts(gen_.Unordered({1, 2, 3, 4}, "B", opts));
        dts.max_retransmissions = 0;
        return dts;
      })
      .WillOnce([this](TimeMs, size_t) {
        DataGeneratorOptions opts;
        opts.stream_id = StreamID(3);
        SendQueue::DataToSend dts(gen_.Ordered({9, 10, 11, 12}, "B", opts));
        dts.max_retransmissions = 0;
        return dts;
      })
      .WillOnce([this](TimeMs, size_t) {
        DataGeneratorOptions opts;
        opts.stream_id = StreamID(4);
        SendQueue::DataToSend dts(gen_.Ordered({13, 14, 15, 16}, "B", opts));
        dts.max_retransmissions = 0;
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

  queue.HandleT3RtxTimerExpiry();

  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),               //
                          Pair(TSN(10), State::kToBeRetransmitted),  //
                          Pair(TSN(11), State::kToBeRetransmitted),  //
                          Pair(TSN(12), State::kToBeRetransmitted),  //
                          Pair(TSN(13), State::kAcked)));

  EXPECT_CALL(producer_, Discard(IsUnordered(false), StreamID(1), MID(42)))
      .Times(1);
  EXPECT_CALL(producer_, Discard(IsUnordered(true), StreamID(2), MID(42)))
      .Times(1);
  EXPECT_CALL(producer_, Discard(IsUnordered(false), StreamID(3), MID(42)))
      .Times(1);
  EXPECT_TRUE(queue.ShouldSendForwardTsn(now_));

  EXPECT_THAT(queue.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),       //
                          Pair(TSN(10), State::kAbandoned),  //
                          Pair(TSN(11), State::kAbandoned),  //
                          Pair(TSN(12), State::kAbandoned),  //
                          Pair(TSN(13), State::kAcked)));

  IForwardTsnChunk forward_tsn = queue.CreateIForwardTsn();
  EXPECT_EQ(forward_tsn.new_cumulative_tsn(), TSN(12));
  EXPECT_THAT(
      forward_tsn.skipped_streams(),
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
        dts.max_retransmissions = 0;
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

}  // namespace
}  // namespace dcsctp
