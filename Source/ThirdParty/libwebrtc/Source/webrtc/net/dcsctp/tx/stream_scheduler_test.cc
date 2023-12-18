/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/tx/stream_scheduler.h"

#include <vector>

#include "net/dcsctp/packet/sctp_packet.h"
#include "net/dcsctp/public/types.h"
#include "test/gmock.h"

namespace dcsctp {
namespace {
using ::testing::Return;
using ::testing::StrictMock;

constexpr size_t kMtu = 1000;
constexpr size_t kPayloadSize = 4;

MATCHER_P(HasDataWithMid, mid, "") {
  if (!arg.has_value()) {
    *result_listener << "There was no produced data";
    return false;
  }

  if (arg->data.mid != mid) {
    *result_listener << "the produced data had mid " << *arg->data.mid
                     << " and not the expected " << *mid;
    return false;
  }

  return true;
}

std::function<absl::optional<SendQueue::DataToSend>(TimeMs, size_t)>
CreateChunk(OutgoingMessageId message_id,
            StreamID sid,
            MID mid,
            size_t payload_size = kPayloadSize) {
  return [sid, mid, payload_size, message_id](TimeMs now, size_t max_size) {
    return SendQueue::DataToSend(
        message_id,
        Data(sid, SSN(0), mid, FSN(0), PPID(42),
             std::vector<uint8_t>(payload_size), Data::IsBeginning(true),
             Data::IsEnd(true), IsUnordered(true)));
  };
}

std::map<StreamID, size_t> GetPacketCounts(StreamScheduler& scheduler,
                                           size_t packets_to_generate) {
  std::map<StreamID, size_t> packet_counts;
  for (size_t i = 0; i < packets_to_generate; ++i) {
    absl::optional<SendQueue::DataToSend> data =
        scheduler.Produce(TimeMs(0), kMtu);
    if (data.has_value()) {
      ++packet_counts[data->data.stream_id];
    }
  }
  return packet_counts;
}

class MockStreamProducer : public StreamScheduler::StreamProducer {
 public:
  MOCK_METHOD(absl::optional<SendQueue::DataToSend>,
              Produce,
              (TimeMs, size_t),
              (override));
  MOCK_METHOD(size_t, bytes_to_send_in_next_message, (), (const, override));
};

class TestStream {
 public:
  TestStream(StreamScheduler& scheduler,
             StreamID stream_id,
             StreamPriority priority,
             size_t packet_size = kPayloadSize) {
    EXPECT_CALL(producer_, Produce)
        .WillRepeatedly(
            CreateChunk(OutgoingMessageId(0), stream_id, MID(0), packet_size));
    EXPECT_CALL(producer_, bytes_to_send_in_next_message)
        .WillRepeatedly(Return(packet_size));
    stream_ = scheduler.CreateStream(&producer_, stream_id, priority);
    stream_->MaybeMakeActive();
  }

  StreamScheduler::Stream& stream() { return *stream_; }

 private:
  StrictMock<MockStreamProducer> producer_;
  std::unique_ptr<StreamScheduler::Stream> stream_;
};

// A scheduler without active streams doesn't produce data.
TEST(StreamSchedulerTest, HasNoActiveStreams) {
  StreamScheduler scheduler("", kMtu);

  EXPECT_EQ(scheduler.Produce(TimeMs(0), kMtu), absl::nullopt);
}

// Stream properties can be set and retrieved
TEST(StreamSchedulerTest, CanSetAndGetStreamProperties) {
  StreamScheduler scheduler("", kMtu);

  StrictMock<MockStreamProducer> producer;
  auto stream =
      scheduler.CreateStream(&producer, StreamID(1), StreamPriority(2));

  EXPECT_EQ(stream->stream_id(), StreamID(1));
  EXPECT_EQ(stream->priority(), StreamPriority(2));

  stream->SetPriority(StreamPriority(0));
  EXPECT_EQ(stream->priority(), StreamPriority(0));
}

// A scheduler with a single stream produced packets from it.
TEST(StreamSchedulerTest, CanProduceFromSingleStream) {
  StreamScheduler scheduler("", kMtu);

  StrictMock<MockStreamProducer> producer;
  EXPECT_CALL(producer, Produce)
      .WillOnce(CreateChunk(OutgoingMessageId(0), StreamID(1), MID(0)));
  EXPECT_CALL(producer, bytes_to_send_in_next_message)
      .WillOnce(Return(kPayloadSize))  // When making active
      .WillOnce(Return(0));
  auto stream =
      scheduler.CreateStream(&producer, StreamID(1), StreamPriority(2));
  stream->MaybeMakeActive();

  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(0)));
  EXPECT_EQ(scheduler.Produce(TimeMs(0), kMtu), absl::nullopt);
}

// Switches between two streams after every packet.
TEST(StreamSchedulerTest, WillRoundRobinBetweenStreams) {
  StreamScheduler scheduler("", kMtu);

  StrictMock<MockStreamProducer> producer1;
  EXPECT_CALL(producer1, Produce)
      .WillOnce(CreateChunk(OutgoingMessageId(0), StreamID(1), MID(100)))
      .WillOnce(CreateChunk(OutgoingMessageId(1), StreamID(1), MID(101)))
      .WillOnce(CreateChunk(OutgoingMessageId(2), StreamID(1), MID(102)));
  EXPECT_CALL(producer1, bytes_to_send_in_next_message)
      .WillOnce(Return(kPayloadSize))  // When making active
      .WillOnce(Return(kPayloadSize))
      .WillOnce(Return(kPayloadSize))
      .WillOnce(Return(0));
  auto stream1 =
      scheduler.CreateStream(&producer1, StreamID(1), StreamPriority(2));
  stream1->MaybeMakeActive();

  StrictMock<MockStreamProducer> producer2;
  EXPECT_CALL(producer2, Produce)
      .WillOnce(CreateChunk(OutgoingMessageId(4), StreamID(2), MID(200)))
      .WillOnce(CreateChunk(OutgoingMessageId(5), StreamID(2), MID(201)))
      .WillOnce(CreateChunk(OutgoingMessageId(6), StreamID(2), MID(202)));
  EXPECT_CALL(producer2, bytes_to_send_in_next_message)
      .WillOnce(Return(kPayloadSize))  // When making active
      .WillOnce(Return(kPayloadSize))
      .WillOnce(Return(kPayloadSize))
      .WillOnce(Return(0));
  auto stream2 =
      scheduler.CreateStream(&producer2, StreamID(2), StreamPriority(2));
  stream2->MaybeMakeActive();

  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(100)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(200)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(101)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(201)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(102)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(202)));
  EXPECT_EQ(scheduler.Produce(TimeMs(0), kMtu), absl::nullopt);
}

// Switches between two streams after every packet, but keeps producing from the
// same stream when a packet contains of multiple fragments.
TEST(StreamSchedulerTest, WillRoundRobinOnlyWhenFinishedProducingChunk) {
  StreamScheduler scheduler("", kMtu);

  StrictMock<MockStreamProducer> producer1;
  EXPECT_CALL(producer1, Produce)
      .WillOnce(CreateChunk(OutgoingMessageId(0), StreamID(1), MID(100)))
      .WillOnce([](...) {
        return SendQueue::DataToSend(
            OutgoingMessageId(1),
            Data(StreamID(1), SSN(0), MID(101), FSN(0), PPID(42),
                 std::vector<uint8_t>(4), Data::IsBeginning(true),
                 Data::IsEnd(false), IsUnordered(true)));
      })
      .WillOnce([](...) {
        return SendQueue::DataToSend(
            OutgoingMessageId(1),
            Data(StreamID(1), SSN(0), MID(101), FSN(0), PPID(42),
                 std::vector<uint8_t>(4), Data::IsBeginning(false),
                 Data::IsEnd(false), IsUnordered(true)));
      })
      .WillOnce([](...) {
        return SendQueue::DataToSend(
            OutgoingMessageId(1),
            Data(StreamID(1), SSN(0), MID(101), FSN(0), PPID(42),
                 std::vector<uint8_t>(4), Data::IsBeginning(false),
                 Data::IsEnd(true), IsUnordered(true)));
      })
      .WillOnce(CreateChunk(OutgoingMessageId(2), StreamID(1), MID(102)));
  EXPECT_CALL(producer1, bytes_to_send_in_next_message)
      .WillOnce(Return(kPayloadSize))  // When making active
      .WillOnce(Return(kPayloadSize))
      .WillOnce(Return(kPayloadSize))
      .WillOnce(Return(kPayloadSize))
      .WillOnce(Return(kPayloadSize))
      .WillOnce(Return(0));
  auto stream1 =
      scheduler.CreateStream(&producer1, StreamID(1), StreamPriority(2));
  stream1->MaybeMakeActive();

  StrictMock<MockStreamProducer> producer2;
  EXPECT_CALL(producer2, Produce)
      .WillOnce(CreateChunk(OutgoingMessageId(3), StreamID(2), MID(200)))
      .WillOnce(CreateChunk(OutgoingMessageId(4), StreamID(2), MID(201)))
      .WillOnce(CreateChunk(OutgoingMessageId(5), StreamID(2), MID(202)));
  EXPECT_CALL(producer2, bytes_to_send_in_next_message)
      .WillOnce(Return(kPayloadSize))  // When making active
      .WillOnce(Return(kPayloadSize))
      .WillOnce(Return(kPayloadSize))
      .WillOnce(Return(0));
  auto stream2 =
      scheduler.CreateStream(&producer2, StreamID(2), StreamPriority(2));
  stream2->MaybeMakeActive();

  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(100)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(200)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(101)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(101)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(101)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(201)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(102)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(202)));
  EXPECT_EQ(scheduler.Produce(TimeMs(0), kMtu), absl::nullopt);
}

// Deactivates a stream before it has finished producing all packets.
TEST(StreamSchedulerTest, StreamsCanBeMadeInactive) {
  StreamScheduler scheduler("", kMtu);

  StrictMock<MockStreamProducer> producer1;
  EXPECT_CALL(producer1, Produce)
      .WillOnce(CreateChunk(OutgoingMessageId(0), StreamID(1), MID(100)))
      .WillOnce(CreateChunk(OutgoingMessageId(1), StreamID(1), MID(101)));
  EXPECT_CALL(producer1, bytes_to_send_in_next_message)
      .WillOnce(Return(kPayloadSize))  // When making active
      .WillOnce(Return(kPayloadSize))
      .WillOnce(Return(kPayloadSize));  // hints that there is a MID(2) coming.
  auto stream1 =
      scheduler.CreateStream(&producer1, StreamID(1), StreamPriority(2));
  stream1->MaybeMakeActive();

  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(100)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(101)));

  // ... but the stream is made inactive before it can be produced.
  stream1->MakeInactive();
  EXPECT_EQ(scheduler.Produce(TimeMs(0), kMtu), absl::nullopt);
}

// Resumes a paused stream - makes a stream active after inactivating it.
TEST(StreamSchedulerTest, SingleStreamCanBeResumed) {
  StreamScheduler scheduler("", kMtu);

  StrictMock<MockStreamProducer> producer1;
  // Callbacks are setup so that they hint that there is a MID(2) coming...
  EXPECT_CALL(producer1, Produce)
      .WillOnce(CreateChunk(OutgoingMessageId(0), StreamID(1), MID(100)))
      .WillOnce(CreateChunk(OutgoingMessageId(1), StreamID(1), MID(101)))
      .WillOnce(CreateChunk(OutgoingMessageId(2), StreamID(1), MID(102)));
  EXPECT_CALL(producer1, bytes_to_send_in_next_message)
      .WillOnce(Return(kPayloadSize))  // When making active
      .WillOnce(Return(kPayloadSize))
      .WillOnce(Return(kPayloadSize))
      .WillOnce(Return(kPayloadSize))  // When making active again
      .WillOnce(Return(0));
  auto stream1 =
      scheduler.CreateStream(&producer1, StreamID(1), StreamPriority(2));
  stream1->MaybeMakeActive();

  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(100)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(101)));

  stream1->MakeInactive();
  EXPECT_EQ(scheduler.Produce(TimeMs(0), kMtu), absl::nullopt);
  stream1->MaybeMakeActive();
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(102)));
  EXPECT_EQ(scheduler.Produce(TimeMs(0), kMtu), absl::nullopt);
}

// Iterates between streams, where one is suddenly paused and later resumed.
TEST(StreamSchedulerTest, WillRoundRobinWithPausedStream) {
  StreamScheduler scheduler("", kMtu);

  StrictMock<MockStreamProducer> producer1;
  EXPECT_CALL(producer1, Produce)
      .WillOnce(CreateChunk(OutgoingMessageId(0), StreamID(1), MID(100)))
      .WillOnce(CreateChunk(OutgoingMessageId(1), StreamID(1), MID(101)))
      .WillOnce(CreateChunk(OutgoingMessageId(2), StreamID(1), MID(102)));
  EXPECT_CALL(producer1, bytes_to_send_in_next_message)
      .WillOnce(Return(kPayloadSize))  // When making active
      .WillOnce(Return(kPayloadSize))
      .WillOnce(Return(kPayloadSize))  // When making active
      .WillOnce(Return(kPayloadSize))
      .WillOnce(Return(0));
  auto stream1 =
      scheduler.CreateStream(&producer1, StreamID(1), StreamPriority(2));
  stream1->MaybeMakeActive();

  StrictMock<MockStreamProducer> producer2;
  EXPECT_CALL(producer2, Produce)
      .WillOnce(CreateChunk(OutgoingMessageId(3), StreamID(2), MID(200)))
      .WillOnce(CreateChunk(OutgoingMessageId(4), StreamID(2), MID(201)))
      .WillOnce(CreateChunk(OutgoingMessageId(5), StreamID(2), MID(202)));
  EXPECT_CALL(producer2, bytes_to_send_in_next_message)
      .WillOnce(Return(kPayloadSize))  // When making active
      .WillOnce(Return(kPayloadSize))
      .WillOnce(Return(kPayloadSize))
      .WillOnce(Return(0));
  auto stream2 =
      scheduler.CreateStream(&producer2, StreamID(2), StreamPriority(2));
  stream2->MaybeMakeActive();

  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(100)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(200)));
  stream1->MakeInactive();
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(201)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(202)));
  stream1->MaybeMakeActive();
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(101)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(102)));
  EXPECT_EQ(scheduler.Produce(TimeMs(0), kMtu), absl::nullopt);
}

// Verifies that packet counts are evenly distributed in round robin scheduling.
TEST(StreamSchedulerTest, WillDistributeRoundRobinPacketsEvenlyTwoStreams) {
  StreamScheduler scheduler("", kMtu);
  TestStream stream1(scheduler, StreamID(1), StreamPriority(1));
  TestStream stream2(scheduler, StreamID(2), StreamPriority(1));

  std::map<StreamID, size_t> packet_counts = GetPacketCounts(scheduler, 10);
  EXPECT_EQ(packet_counts[StreamID(1)], 5U);
  EXPECT_EQ(packet_counts[StreamID(2)], 5U);
}

// Verifies that packet counts are evenly distributed among active streams,
// where a stream is suddenly made inactive, two are added, and then the paused
// stream is resumed.
TEST(StreamSchedulerTest, WillDistributeEvenlyWithPausedAndAddedStreams) {
  StreamScheduler scheduler("", kMtu);
  TestStream stream1(scheduler, StreamID(1), StreamPriority(1));
  TestStream stream2(scheduler, StreamID(2), StreamPriority(1));

  std::map<StreamID, size_t> packet_counts = GetPacketCounts(scheduler, 10);
  EXPECT_EQ(packet_counts[StreamID(1)], 5U);
  EXPECT_EQ(packet_counts[StreamID(2)], 5U);

  stream2.stream().MakeInactive();

  TestStream stream3(scheduler, StreamID(3), StreamPriority(1));
  TestStream stream4(scheduler, StreamID(4), StreamPriority(1));

  std::map<StreamID, size_t> counts2 = GetPacketCounts(scheduler, 15);
  EXPECT_EQ(counts2[StreamID(1)], 5U);
  EXPECT_EQ(counts2[StreamID(2)], 0U);
  EXPECT_EQ(counts2[StreamID(3)], 5U);
  EXPECT_EQ(counts2[StreamID(4)], 5U);

  stream2.stream().MaybeMakeActive();

  std::map<StreamID, size_t> counts3 = GetPacketCounts(scheduler, 20);
  EXPECT_EQ(counts3[StreamID(1)], 5U);
  EXPECT_EQ(counts3[StreamID(2)], 5U);
  EXPECT_EQ(counts3[StreamID(3)], 5U);
  EXPECT_EQ(counts3[StreamID(4)], 5U);
}

// Degrades to fair queuing with streams having identical priority.
TEST(StreamSchedulerTest, WillDoFairQueuingWithSamePriority) {
  StreamScheduler scheduler("", kMtu);
  scheduler.EnableMessageInterleaving(true);

  constexpr size_t kSmallPacket = 30;
  constexpr size_t kLargePacket = 70;

  StrictMock<MockStreamProducer> callback1;
  EXPECT_CALL(callback1, Produce)
      .WillOnce(CreateChunk(OutgoingMessageId(0), StreamID(1), MID(100),
                            kSmallPacket))
      .WillOnce(CreateChunk(OutgoingMessageId(1), StreamID(1), MID(101),
                            kSmallPacket))
      .WillOnce(CreateChunk(OutgoingMessageId(2), StreamID(1), MID(102),
                            kSmallPacket));
  EXPECT_CALL(callback1, bytes_to_send_in_next_message)
      .WillOnce(Return(kSmallPacket))  // When making active
      .WillOnce(Return(kSmallPacket))
      .WillOnce(Return(kSmallPacket))
      .WillOnce(Return(0));
  auto stream1 =
      scheduler.CreateStream(&callback1, StreamID(1), StreamPriority(2));
  stream1->MaybeMakeActive();

  StrictMock<MockStreamProducer> callback2;
  EXPECT_CALL(callback2, Produce)
      .WillOnce(CreateChunk(OutgoingMessageId(3), StreamID(2), MID(200),
                            kLargePacket))
      .WillOnce(CreateChunk(OutgoingMessageId(4), StreamID(2), MID(201),
                            kLargePacket))
      .WillOnce(CreateChunk(OutgoingMessageId(5), StreamID(2), MID(202),
                            kLargePacket));
  EXPECT_CALL(callback2, bytes_to_send_in_next_message)
      .WillOnce(Return(kLargePacket))  // When making active
      .WillOnce(Return(kLargePacket))
      .WillOnce(Return(kLargePacket))
      .WillOnce(Return(0));
  auto stream2 =
      scheduler.CreateStream(&callback2, StreamID(2), StreamPriority(2));
  stream2->MaybeMakeActive();

  // t = 30
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(100)));
  // t = 60
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(101)));
  // t = 70
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(200)));
  // t = 90
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(102)));
  // t = 140
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(201)));
  // t = 210
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(202)));
  EXPECT_EQ(scheduler.Produce(TimeMs(0), kMtu), absl::nullopt);
}

// Will do weighted fair queuing with three streams having different priority.
TEST(StreamSchedulerTest, WillDoWeightedFairQueuingSameSizeDifferentPriority) {
  StreamScheduler scheduler("", kMtu);
  scheduler.EnableMessageInterleaving(true);

  StrictMock<MockStreamProducer> callback1;
  EXPECT_CALL(callback1, Produce)
      .WillOnce(CreateChunk(OutgoingMessageId(0), StreamID(1), MID(100)))
      .WillOnce(CreateChunk(OutgoingMessageId(1), StreamID(1), MID(101)))
      .WillOnce(CreateChunk(OutgoingMessageId(2), StreamID(1), MID(102)));
  EXPECT_CALL(callback1, bytes_to_send_in_next_message)
      .WillOnce(Return(kPayloadSize))  // When making active
      .WillOnce(Return(kPayloadSize))
      .WillOnce(Return(kPayloadSize))
      .WillOnce(Return(0));
  // Priority 125 -> allowed to produce every 1000/125 ~= 80 time units.
  auto stream1 =
      scheduler.CreateStream(&callback1, StreamID(1), StreamPriority(125));
  stream1->MaybeMakeActive();

  StrictMock<MockStreamProducer> callback2;
  EXPECT_CALL(callback2, Produce)
      .WillOnce(CreateChunk(OutgoingMessageId(3), StreamID(2), MID(200)))
      .WillOnce(CreateChunk(OutgoingMessageId(4), StreamID(2), MID(201)))
      .WillOnce(CreateChunk(OutgoingMessageId(5), StreamID(2), MID(202)));
  EXPECT_CALL(callback2, bytes_to_send_in_next_message)
      .WillOnce(Return(kPayloadSize))  // When making active
      .WillOnce(Return(kPayloadSize))
      .WillOnce(Return(kPayloadSize))
      .WillOnce(Return(0));
  // Priority 200 -> allowed to produce every 1000/200 ~= 50 time units.
  auto stream2 =
      scheduler.CreateStream(&callback2, StreamID(2), StreamPriority(200));
  stream2->MaybeMakeActive();

  StrictMock<MockStreamProducer> callback3;
  EXPECT_CALL(callback3, Produce)
      .WillOnce(CreateChunk(OutgoingMessageId(6), StreamID(3), MID(300)))
      .WillOnce(CreateChunk(OutgoingMessageId(7), StreamID(3), MID(301)))
      .WillOnce(CreateChunk(OutgoingMessageId(8), StreamID(3), MID(302)));
  EXPECT_CALL(callback3, bytes_to_send_in_next_message)
      .WillOnce(Return(kPayloadSize))  // When making active
      .WillOnce(Return(kPayloadSize))
      .WillOnce(Return(kPayloadSize))
      .WillOnce(Return(0));
  // Priority 500 -> allowed to produce every 1000/500 ~= 20 time units.
  auto stream3 =
      scheduler.CreateStream(&callback3, StreamID(3), StreamPriority(500));
  stream3->MaybeMakeActive();

  // t ~= 20
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(300)));
  // t ~= 40
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(301)));
  // t ~= 50
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(200)));
  // t ~= 60
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(302)));
  // t ~= 80
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(100)));
  // t ~= 100
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(201)));
  // t ~= 150
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(202)));
  // t ~= 160
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(101)));
  // t ~= 240
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(102)));
  EXPECT_EQ(scheduler.Produce(TimeMs(0), kMtu), absl::nullopt);
}

// Will do weighted fair queuing with three streams having different priority
// and sending different payload sizes.
TEST(StreamSchedulerTest, WillDoWeightedFairQueuingDifferentSizeAndPriority) {
  StreamScheduler scheduler("", kMtu);
  scheduler.EnableMessageInterleaving(true);

  constexpr size_t kSmallPacket = 20;
  constexpr size_t kMediumPacket = 50;
  constexpr size_t kLargePacket = 70;

  // Stream with priority = 125 -> inverse weight ~=80
  StrictMock<MockStreamProducer> callback1;
  EXPECT_CALL(callback1, Produce)
      // virtual finish time ~ 0 + 50 * 80 = 4000
      .WillOnce(CreateChunk(OutgoingMessageId(0), StreamID(1), MID(100),
                            kMediumPacket))
      // virtual finish time ~ 4000 + 20 * 80 = 5600
      .WillOnce(CreateChunk(OutgoingMessageId(1), StreamID(1), MID(101),
                            kSmallPacket))
      // virtual finish time ~ 5600 + 70 * 80 = 11200
      .WillOnce(CreateChunk(OutgoingMessageId(2), StreamID(1), MID(102),
                            kLargePacket));
  EXPECT_CALL(callback1, bytes_to_send_in_next_message)
      .WillOnce(Return(kMediumPacket))  // When making active
      .WillOnce(Return(kSmallPacket))
      .WillOnce(Return(kLargePacket))
      .WillOnce(Return(0));
  auto stream1 =
      scheduler.CreateStream(&callback1, StreamID(1), StreamPriority(125));
  stream1->MaybeMakeActive();

  // Stream with priority = 200 -> inverse weight ~=50
  StrictMock<MockStreamProducer> callback2;
  EXPECT_CALL(callback2, Produce)
      // virtual finish time ~ 0 + 50 * 50 = 2500
      .WillOnce(CreateChunk(OutgoingMessageId(3), StreamID(2), MID(200),
                            kMediumPacket))
      // virtual finish time ~ 2500 + 70 * 50 = 6000
      .WillOnce(CreateChunk(OutgoingMessageId(4), StreamID(2), MID(201),
                            kLargePacket))
      // virtual finish time ~ 6000 + 20 * 50 = 7000
      .WillOnce(CreateChunk(OutgoingMessageId(5), StreamID(2), MID(202),
                            kSmallPacket));
  EXPECT_CALL(callback2, bytes_to_send_in_next_message)
      .WillOnce(Return(kMediumPacket))  // When making active
      .WillOnce(Return(kLargePacket))
      .WillOnce(Return(kSmallPacket))
      .WillOnce(Return(0));
  auto stream2 =
      scheduler.CreateStream(&callback2, StreamID(2), StreamPriority(200));
  stream2->MaybeMakeActive();

  // Stream with priority = 500 -> inverse weight ~=20
  StrictMock<MockStreamProducer> callback3;
  EXPECT_CALL(callback3, Produce)
      // virtual finish time ~ 0 + 20 * 20 = 400
      .WillOnce(CreateChunk(OutgoingMessageId(6), StreamID(3), MID(300),
                            kSmallPacket))
      // virtual finish time ~ 400 + 50 * 20 = 1400
      .WillOnce(CreateChunk(OutgoingMessageId(7), StreamID(3), MID(301),
                            kMediumPacket))
      // virtual finish time ~ 1400 + 70 * 20 = 2800
      .WillOnce(CreateChunk(OutgoingMessageId(8), StreamID(3), MID(302),
                            kLargePacket));
  EXPECT_CALL(callback3, bytes_to_send_in_next_message)
      .WillOnce(Return(kSmallPacket))  // When making active
      .WillOnce(Return(kMediumPacket))
      .WillOnce(Return(kLargePacket))
      .WillOnce(Return(0));
  auto stream3 =
      scheduler.CreateStream(&callback3, StreamID(3), StreamPriority(500));
  stream3->MaybeMakeActive();

  // t ~= 400
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(300)));
  // t ~= 1400
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(301)));
  // t ~= 2500
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(200)));
  // t ~= 2800
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(302)));
  // t ~= 4000
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(100)));
  // t ~= 5600
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(101)));
  // t ~= 6000
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(201)));
  // t ~= 7000
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(202)));
  // t ~= 11200
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(102)));
  EXPECT_EQ(scheduler.Produce(TimeMs(0), kMtu), absl::nullopt);
}
TEST(StreamSchedulerTest, WillDistributeWFQPacketsInTwoStreamsByPriority) {
  // A simple test with two streams of different priority, but sending packets
  // of identical size. Verifies that the ratio of sent packets represent their
  // priority.
  StreamScheduler scheduler("", kMtu);
  scheduler.EnableMessageInterleaving(true);

  TestStream stream1(scheduler, StreamID(1), StreamPriority(100), kPayloadSize);
  TestStream stream2(scheduler, StreamID(2), StreamPriority(200), kPayloadSize);

  std::map<StreamID, size_t> packet_counts = GetPacketCounts(scheduler, 15);
  EXPECT_EQ(packet_counts[StreamID(1)], 5U);
  EXPECT_EQ(packet_counts[StreamID(2)], 10U);
}

TEST(StreamSchedulerTest, WillDistributeWFQPacketsInFourStreamsByPriority) {
  // Same as `WillDistributeWFQPacketsInTwoStreamsByPriority` but with more
  // streams.
  StreamScheduler scheduler("", kMtu);
  scheduler.EnableMessageInterleaving(true);

  TestStream stream1(scheduler, StreamID(1), StreamPriority(100), kPayloadSize);
  TestStream stream2(scheduler, StreamID(2), StreamPriority(200), kPayloadSize);
  TestStream stream3(scheduler, StreamID(3), StreamPriority(300), kPayloadSize);
  TestStream stream4(scheduler, StreamID(4), StreamPriority(400), kPayloadSize);

  std::map<StreamID, size_t> packet_counts = GetPacketCounts(scheduler, 50);
  EXPECT_EQ(packet_counts[StreamID(1)], 5U);
  EXPECT_EQ(packet_counts[StreamID(2)], 10U);
  EXPECT_EQ(packet_counts[StreamID(3)], 15U);
  EXPECT_EQ(packet_counts[StreamID(4)], 20U);
}

TEST(StreamSchedulerTest, WillDistributeFromTwoStreamsFairly) {
  // A simple test with two streams of different priority, but sending packets
  // of different size. Verifies that the ratio of total packet payload
  // represent their priority.
  // In this example,
  // * stream1 has priority 100 and sends packets of size 8
  // * stream2 has priority 400 and sends packets of size 4
  // With round robin, stream1 would get twice as many payload bytes on the wire
  // as stream2, but with WFQ and a 4x priority increase, stream2 should 4x as
  // many payload bytes on the wire. That translates to stream2 getting 8x as
  // many packets on the wire as they are half as large.
  StreamScheduler scheduler("", kMtu);
  // Enable WFQ scheduler.
  scheduler.EnableMessageInterleaving(true);

  TestStream stream1(scheduler, StreamID(1), StreamPriority(100),
                     /*packet_size=*/8);
  TestStream stream2(scheduler, StreamID(2), StreamPriority(400),
                     /*packet_size=*/4);

  std::map<StreamID, size_t> packet_counts = GetPacketCounts(scheduler, 90);
  EXPECT_EQ(packet_counts[StreamID(1)], 10U);
  EXPECT_EQ(packet_counts[StreamID(2)], 80U);
}

TEST(StreamSchedulerTest, WillDistributeFromFourStreamsFairly) {
  // Same as `WillDistributeWeightedFairFromTwoStreamsFairly` but more
  // complicated.
  StreamScheduler scheduler("", kMtu);
  // Enable WFQ scheduler.
  scheduler.EnableMessageInterleaving(true);

  TestStream stream1(scheduler, StreamID(1), StreamPriority(100),
                     /*packet_size=*/10);
  TestStream stream2(scheduler, StreamID(2), StreamPriority(200),
                     /*packet_size=*/10);
  TestStream stream3(scheduler, StreamID(3), StreamPriority(200),
                     /*packet_size=*/20);
  TestStream stream4(scheduler, StreamID(4), StreamPriority(400),
                     /*packet_size=*/30);

  std::map<StreamID, size_t> packet_counts = GetPacketCounts(scheduler, 80);
  // 15 packets * 10 bytes = 150 bytes at priority 100.
  EXPECT_EQ(packet_counts[StreamID(1)], 15U);
  // 30 packets * 10 bytes = 300 bytes at priority 200.
  EXPECT_EQ(packet_counts[StreamID(2)], 30U);
  // 15 packets * 20 bytes = 300 bytes at priority 200.
  EXPECT_EQ(packet_counts[StreamID(3)], 15U);
  // 20 packets * 30 bytes = 600 bytes at priority 400.
  EXPECT_EQ(packet_counts[StreamID(4)], 20U);
}

// Sending large messages with small MTU will fragment the messages and produce
// a first fragment not larger than the MTU, and will then not first send from
// the stream with the smallest message, as their first fragment will be equally
// small for both streams. See `LargeMessageWithLargeMtu` for the same test, but
// with a larger MTU.
TEST(StreamSchedulerTest, SendLargeMessageWithSmallMtu) {
  StreamScheduler scheduler(
      "", 100 + SctpPacket::kHeaderSize + IDataChunk::kHeaderSize);
  scheduler.EnableMessageInterleaving(true);

  StrictMock<MockStreamProducer> producer1;
  EXPECT_CALL(producer1, Produce)
      .WillOnce(CreateChunk(OutgoingMessageId(0), StreamID(1), MID(0), 100))
      .WillOnce(CreateChunk(OutgoingMessageId(1), StreamID(1), MID(0), 100));
  EXPECT_CALL(producer1, bytes_to_send_in_next_message)
      .WillOnce(Return(200))  // When making active
      .WillOnce(Return(100))
      .WillOnce(Return(0));
  auto stream1 =
      scheduler.CreateStream(&producer1, StreamID(1), StreamPriority(1));
  stream1->MaybeMakeActive();

  StrictMock<MockStreamProducer> producer2;
  EXPECT_CALL(producer2, Produce)
      .WillOnce(CreateChunk(OutgoingMessageId(2), StreamID(2), MID(1), 100))
      .WillOnce(CreateChunk(OutgoingMessageId(3), StreamID(2), MID(1), 50));
  EXPECT_CALL(producer2, bytes_to_send_in_next_message)
      .WillOnce(Return(150))  // When making active
      .WillOnce(Return(50))
      .WillOnce(Return(0));
  auto stream2 =
      scheduler.CreateStream(&producer2, StreamID(2), StreamPriority(1));
  stream2->MaybeMakeActive();
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(0)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(1)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(1)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(0)));
  EXPECT_EQ(scheduler.Produce(TimeMs(0), kMtu), absl::nullopt);
}

// Sending large messages with large MTU will not fragment messages and will
// send the message first from the stream that has the smallest message.
TEST(StreamSchedulerTest, SendLargeMessageWithLargeMtu) {
  StreamScheduler scheduler(
      "", 200 + SctpPacket::kHeaderSize + IDataChunk::kHeaderSize);
  scheduler.EnableMessageInterleaving(true);

  StrictMock<MockStreamProducer> producer1;
  EXPECT_CALL(producer1, Produce)
      .WillOnce(CreateChunk(OutgoingMessageId(0), StreamID(1), MID(0), 200));
  EXPECT_CALL(producer1, bytes_to_send_in_next_message)
      .WillOnce(Return(200))  // When making active
      .WillOnce(Return(0));
  auto stream1 =
      scheduler.CreateStream(&producer1, StreamID(1), StreamPriority(1));
  stream1->MaybeMakeActive();

  StrictMock<MockStreamProducer> producer2;
  EXPECT_CALL(producer2, Produce)
      .WillOnce(CreateChunk(OutgoingMessageId(1), StreamID(2), MID(1), 150));
  EXPECT_CALL(producer2, bytes_to_send_in_next_message)
      .WillOnce(Return(150))  // When making active
      .WillOnce(Return(0));
  auto stream2 =
      scheduler.CreateStream(&producer2, StreamID(2), StreamPriority(1));
  stream2->MaybeMakeActive();
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(1)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(0)));
  EXPECT_EQ(scheduler.Produce(TimeMs(0), kMtu), absl::nullopt);
}

}  // namespace
}  // namespace dcsctp
