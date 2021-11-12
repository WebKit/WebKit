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
#include <numeric>

#include "absl/memory/memory.h"
#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "rtc_base/checks.h"
#include "rtc_base/random.h"
#include "test/gtest.h"

namespace webrtc {

namespace {
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

}  // namespace webrtc
