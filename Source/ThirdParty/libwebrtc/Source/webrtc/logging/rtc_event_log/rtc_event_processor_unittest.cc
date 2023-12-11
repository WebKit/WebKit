/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/rtc_event_processor.h"

#include <stddef.h>

#include <cstdint>
#include <initializer_list>
#include <limits>

#include "absl/memory/memory.h"
#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "rtc_base/checks.h"
#include "rtc_base/random.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

namespace {
LoggedStartEvent CreateEvent(int64_t time_ms, int64_t utc_time_ms) {
  return LoggedStartEvent(Timestamp::Millis(time_ms),
                          Timestamp::Millis(utc_time_ms));
}

std::vector<LoggedStartEvent> CreateEventList(
    std::initializer_list<int64_t> timestamp_list) {
  std::vector<LoggedStartEvent> v;
  for (int64_t timestamp_ms : timestamp_list) {
    v.emplace_back(Timestamp::Millis(timestamp_ms));
  }
  return v;
}

std::vector<std::vector<LoggedStartEvent>>
CreateRandomEventLists(size_t num_lists, size_t num_elements, uint64_t seed) {
  Random prng(seed);
  std::vector<std::vector<LoggedStartEvent>> lists(num_lists);
  for (size_t elem = 0; elem < num_elements; elem++) {
    uint32_t i = prng.Rand(0u, num_lists - 1);
    int64_t timestamp_ms = elem;
    lists[i].emplace_back(Timestamp::Millis(timestamp_ms));
  }
  return lists;
}

LoggedRtpPacket CreateRtpPacket(int64_t time_ms,
                                uint32_t ssrc,
                                absl::optional<uint16_t> transport_seq_num) {
  RTPHeader header;
  header.ssrc = ssrc;
  header.timestamp = static_cast<uint32_t>(time_ms);
  header.paddingLength = 0;
  header.headerLength = 20;
  header.extension.hasTransportSequenceNumber = transport_seq_num.has_value();
  if (transport_seq_num.has_value()) {
    header.extension.transportSequenceNumber = transport_seq_num.value();
  }
  return LoggedRtpPacket(Timestamp::Millis(time_ms), header, 20, 1000);
}

}  // namespace

TEST(RtcEventProcessor, NoList) {
  RtcEventProcessor processor;
  processor.ProcessEventsInOrder();  // Don't crash but do nothing.
}

TEST(RtcEventProcessor, EmptyList) {
  auto not_called = [](LoggedStartEvent /*elem*/) { EXPECT_TRUE(false); };
  std::vector<LoggedStartEvent> events;
  RtcEventProcessor processor;

  processor.AddEvents(events, not_called);
  processor.ProcessEventsInOrder();  // Don't crash but do nothing.
}

TEST(RtcEventProcessor, OneList) {
  std::vector<LoggedStartEvent> result;
  auto f = [&result](LoggedStartEvent elem) { result.push_back(elem); };

  std::vector<LoggedStartEvent> events(CreateEventList({1, 2, 3, 4}));
  RtcEventProcessor processor;
  processor.AddEvents(events, f);
  processor.ProcessEventsInOrder();

  std::vector<int64_t> expected_results{1, 2, 3, 4};
  ASSERT_EQ(result.size(), expected_results.size());
  for (size_t i = 0; i < expected_results.size(); i++) {
    EXPECT_EQ(result[i].log_time_ms(), expected_results[i]);
  }
}

TEST(RtcEventProcessor, MergeTwoLists) {
  std::vector<LoggedStartEvent> result;
  auto f = [&result](LoggedStartEvent elem) { result.push_back(elem); };

  std::vector<LoggedStartEvent> events1(CreateEventList({1, 2, 4, 7, 8, 9}));
  std::vector<LoggedStartEvent> events2(CreateEventList({3, 5, 6, 10}));
  RtcEventProcessor processor;
  processor.AddEvents(events1, f);
  processor.AddEvents(events2, f);
  processor.ProcessEventsInOrder();

  std::vector<int64_t> expected_results{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  ASSERT_EQ(result.size(), expected_results.size());
  for (size_t i = 0; i < expected_results.size(); i++) {
    EXPECT_EQ(result[i].log_time_ms(), expected_results[i]);
  }
}

TEST(RtcEventProcessor, MergeTwoListsWithDuplicatedElements) {
  std::vector<LoggedStartEvent> result;
  auto f = [&result](LoggedStartEvent elem) { result.push_back(elem); };

  std::vector<LoggedStartEvent> events1(CreateEventList({1, 2, 2, 3, 5, 5}));
  std::vector<LoggedStartEvent> events2(CreateEventList({1, 3, 4, 4}));
  RtcEventProcessor processor;
  processor.AddEvents(events1, f);
  processor.AddEvents(events2, f);
  processor.ProcessEventsInOrder();

  std::vector<int64_t> expected_results{1, 1, 2, 2, 3, 3, 4, 4, 5, 5};
  ASSERT_EQ(result.size(), expected_results.size());
  for (size_t i = 0; i < expected_results.size(); i++) {
    EXPECT_EQ(result[i].log_time_ms(), expected_results[i]);
  }
}

TEST(RtcEventProcessor, MergeManyLists) {
  std::vector<LoggedStartEvent> result;
  auto f = [&result](LoggedStartEvent elem) { result.push_back(elem); };

  constexpr size_t kNumLists = 5;
  constexpr size_t kNumElems = 30;
  constexpr uint64_t kSeed = 0xF3C6B91F;
  std::vector<std::vector<LoggedStartEvent>> lists(
      CreateRandomEventLists(kNumLists, kNumElems, kSeed));
  RTC_DCHECK_EQ(lists.size(), kNumLists);
  RtcEventProcessor processor;
  for (const auto& list : lists) {
    processor.AddEvents(list, f);
  }
  processor.ProcessEventsInOrder();

  std::vector<int64_t> expected_results(kNumElems);
  std::iota(expected_results.begin(), expected_results.end(), 0);
  ASSERT_EQ(result.size(), expected_results.size());
  for (size_t i = 0; i < expected_results.size(); i++) {
    EXPECT_EQ(result[i].log_time_ms(), expected_results[i]);
  }
}

TEST(RtcEventProcessor, DifferentTypes) {
  std::vector<int64_t> result;
  auto f1 = [&result](LoggedStartEvent elem) {
    result.push_back(elem.log_time_ms());
  };
  auto f2 = [&result](LoggedStopEvent elem) {
    result.push_back(elem.log_time_ms());
  };

  std::vector<LoggedStartEvent> events1{LoggedStartEvent(Timestamp::Millis(2))};
  std::vector<LoggedStopEvent> events2{LoggedStopEvent(Timestamp::Millis(1))};
  RtcEventProcessor processor;
  processor.AddEvents(events1, f1);
  processor.AddEvents(events2, f2);
  processor.ProcessEventsInOrder();

  std::vector<int64_t> expected_results{1, 2};
  ASSERT_EQ(result.size(), expected_results.size());
  for (size_t i = 0; i < expected_results.size(); i++) {
    EXPECT_EQ(result[i], expected_results[i]);
  }
}

TEST(RtcEventProcessor, IncomingPacketBeforeOutgoingFeedback) {
  EXPECT_LT(TieBreaker<LoggedRtpPacketIncoming>::type_order,
            TieBreaker<LoggedRtcpPacketOutgoing>::type_order);
}

TEST(RtcEventProcessor, PacketWrapperTypesOrderedAsRtp) {
  EXPECT_EQ(TieBreaker<LoggedRtpPacketIncoming>::type_order,
            TieBreaker<LoggedRtpPacket>::type_order(
                PacketDirection::kIncomingPacket));
  EXPECT_EQ(TieBreaker<LoggedRtpPacketOutgoing>::type_order,
            TieBreaker<LoggedRtpPacket>::type_order(
                PacketDirection::kOutgoingPacket));

  EXPECT_EQ(TieBreaker<LoggedRtpPacketIncoming>::type_order,
            TieBreaker<LoggedPacketInfo>::type_order(
                PacketDirection::kIncomingPacket));
  EXPECT_EQ(TieBreaker<LoggedRtpPacketOutgoing>::type_order,
            TieBreaker<LoggedPacketInfo>::type_order(
                PacketDirection::kOutgoingPacket));
}

TEST(RtcEventProcessor, IncomingFeedbackBeforeBwe) {
  EXPECT_LT(TieBreaker<LoggedRtcpPacketIncoming>::type_order,
            TieBreaker<LoggedBweProbeSuccessEvent>::type_order);
  EXPECT_LT(TieBreaker<LoggedRtcpPacketIncoming>::type_order,
            TieBreaker<LoggedRemoteEstimateEvent>::type_order);
  EXPECT_LT(TieBreaker<LoggedRtcpPacketIncoming>::type_order,
            TieBreaker<LoggedBweProbeSuccessEvent>::type_order);
  EXPECT_LT(TieBreaker<LoggedRtcpPacketIncoming>::type_order,
            TieBreaker<LoggedBweProbeFailureEvent>::type_order);
  EXPECT_LT(TieBreaker<LoggedRtcpPacketIncoming>::type_order,
            TieBreaker<LoggedBweDelayBasedUpdate>::type_order);
  EXPECT_LT(TieBreaker<LoggedRtcpPacketIncoming>::type_order,
            TieBreaker<LoggedBweLossBasedUpdate>::type_order);
  EXPECT_LT(TieBreaker<LoggedRtcpPacketIncoming>::type_order,
            TieBreaker<LoggedBweProbeClusterCreatedEvent>::type_order);
}

TEST(RtcEventProcessor, RtpPacketsInTransportSeqNumOrder) {
  std::vector<LoggedRtpPacket> ssrc_1234{
      CreateRtpPacket(1, 1234, absl::nullopt),
      CreateRtpPacket(1, 1234, absl::nullopt)};
  std::vector<LoggedRtpPacket> ssrc_2345{CreateRtpPacket(1, 2345, 2),
                                         CreateRtpPacket(1, 2345, 3),
                                         CreateRtpPacket(1, 2345, 6)};
  std::vector<LoggedRtpPacket> ssrc_3456{CreateRtpPacket(1, 3456, 1),
                                         CreateRtpPacket(1, 3456, 4),
                                         CreateRtpPacket(1, 3456, 5)};

  // Store SSRC and transport sequence number for each processed packet.
  std::vector<std::pair<uint32_t, absl::optional<uint16_t>>> results;
  auto get_packet = [&results](const LoggedRtpPacket& packet) {
    absl::optional<uint16_t> transport_seq_num;
    if (packet.header.extension.hasTransportSequenceNumber)
      transport_seq_num = packet.header.extension.transportSequenceNumber;
    results.emplace_back(packet.header.ssrc, transport_seq_num);
  };

  RtcEventProcessor processor;
  processor.AddEvents(ssrc_1234, get_packet, PacketDirection::kIncomingPacket);
  processor.AddEvents(ssrc_2345, get_packet, PacketDirection::kIncomingPacket);
  processor.AddEvents(ssrc_3456, get_packet, PacketDirection::kIncomingPacket);
  processor.ProcessEventsInOrder();

  std::vector<std::pair<uint32_t, absl::optional<uint16_t>>> expected{
      {1234, absl::nullopt},
      {1234, absl::nullopt},
      {3456, 1},
      {2345, 2},
      {2345, 3},
      {3456, 4},
      {3456, 5},
      {2345, 6}};
  EXPECT_THAT(results, testing::ElementsAreArray(expected));
}

TEST(RtcEventProcessor, TransportSeqNumOrderHandlesWrapAround) {
  std::vector<LoggedRtpPacket> ssrc_1234{
      CreateRtpPacket(0, 1234, std::numeric_limits<uint16_t>::max() - 1),
      CreateRtpPacket(1, 1234, 1), CreateRtpPacket(1, 1234, 2)};
  std::vector<LoggedRtpPacket> ssrc_2345{
      CreateRtpPacket(1, 2345, std::numeric_limits<uint16_t>::max()),
      CreateRtpPacket(1, 2345, 0), CreateRtpPacket(1, 2345, 3)};

  // Store SSRC and transport sequence number for each processed packet.
  std::vector<std::pair<uint32_t, absl::optional<uint16_t>>> results;
  auto get_packet = [&results](const LoggedRtpPacket& packet) {
    absl::optional<uint16_t> transport_seq_num;
    if (packet.header.extension.hasTransportSequenceNumber)
      transport_seq_num = packet.header.extension.transportSequenceNumber;
    results.emplace_back(packet.header.ssrc, transport_seq_num);
  };

  RtcEventProcessor processor;
  processor.AddEvents(ssrc_1234, get_packet, PacketDirection::kOutgoingPacket);
  processor.AddEvents(ssrc_2345, get_packet, PacketDirection::kOutgoingPacket);
  processor.ProcessEventsInOrder();

  std::vector<std::pair<uint32_t, absl::optional<uint16_t>>> expected{
      {1234, std::numeric_limits<uint16_t>::max() - 1},
      {2345, std::numeric_limits<uint16_t>::max()},
      {2345, 0},
      {1234, 1},
      {1234, 2},
      {2345, 3}};
  EXPECT_THAT(results, testing::ElementsAreArray(expected));
}

TEST(RtcEventProcessor, InsertionOrderIfNoTransportSeqNum) {
  std::vector<LoggedStartEvent> events1{CreateEvent(1, 222)};
  std::vector<LoggedStartEvent> events2{CreateEvent(1, 111)};
  std::vector<LoggedStartEvent> events3{CreateEvent(1, 333)};

  std::vector<int64_t> results;
  auto get_utc_time = [&results](const LoggedStartEvent& elem) {
    results.push_back(elem.utc_time().ms());
  };

  RtcEventProcessor processor;
  processor.AddEvents(events1, get_utc_time);
  processor.AddEvents(events2, get_utc_time);
  processor.AddEvents(events3, get_utc_time);
  processor.ProcessEventsInOrder();

  EXPECT_THAT(results, testing::ElementsAreArray({222, 111, 333}));
}

}  // namespace webrtc
