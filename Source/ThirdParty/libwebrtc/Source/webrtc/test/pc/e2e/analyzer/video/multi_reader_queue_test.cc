/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/video/multi_reader_queue.h"

#include "absl/types/optional.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(MultiReaderQueueTest, EmptyQueueEmptyForAllHeads) {
  MultiReaderQueue<int> queue = MultiReaderQueue<int>(/*readers_count=*/10);
  EXPECT_EQ(queue.size(), 0lu);
  for (int i = 0; i < 10; ++i) {
    EXPECT_TRUE(queue.IsEmpty(/*reader=*/i));
    EXPECT_EQ(queue.size(/*reader=*/i), 0lu);
    EXPECT_FALSE(queue.PopFront(/*reader=*/i).has_value());
    EXPECT_FALSE(queue.Front(/*reader=*/i).has_value());
  }
}

TEST(MultiReaderQueueTest, SizeIsEqualForAllHeadsAfterAddOnly) {
  MultiReaderQueue<int> queue = MultiReaderQueue<int>(/*readers_count=*/10);
  queue.PushBack(1);
  queue.PushBack(2);
  queue.PushBack(3);
  EXPECT_EQ(queue.size(), 3lu);
  for (int i = 0; i < 10; ++i) {
    EXPECT_FALSE(queue.IsEmpty(/*reader=*/i));
    EXPECT_EQ(queue.size(/*reader=*/i), 3lu);
  }
}

TEST(MultiReaderQueueTest, SizeIsCorrectAfterRemoveFromOnlyOneHead) {
  MultiReaderQueue<int> queue = MultiReaderQueue<int>(/*readers_count=*/10);
  for (int i = 0; i < 5; ++i) {
    queue.PushBack(i);
  }
  EXPECT_EQ(queue.size(), 5lu);
  // Removing elements from queue #0
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(queue.size(/*reader=*/0), static_cast<size_t>(5 - i));
    EXPECT_EQ(queue.PopFront(/*reader=*/0), absl::optional<int>(i));
    for (int j = 1; j < 10; ++j) {
      EXPECT_EQ(queue.size(/*reader=*/j), 5lu);
    }
  }
  EXPECT_EQ(queue.size(/*reader=*/0), 0lu);
  EXPECT_TRUE(queue.IsEmpty(/*reader=*/0));
}

TEST(MultiReaderQueueTest, SingleHeadOneAddOneRemove) {
  MultiReaderQueue<int> queue = MultiReaderQueue<int>(/*readers_count=*/1);
  queue.PushBack(1);
  EXPECT_EQ(queue.size(), 1lu);
  EXPECT_TRUE(queue.Front(/*reader=*/0).has_value());
  EXPECT_EQ(queue.Front(/*reader=*/0).value(), 1);
  absl::optional<int> value = queue.PopFront(/*reader=*/0);
  EXPECT_TRUE(value.has_value());
  EXPECT_EQ(value.value(), 1);
  EXPECT_EQ(queue.size(), 0lu);
  EXPECT_TRUE(queue.IsEmpty(/*reader=*/0));
}

TEST(MultiReaderQueueTest, SingleHead) {
  MultiReaderQueue<size_t> queue =
      MultiReaderQueue<size_t>(/*readers_count=*/1);
  for (size_t i = 0; i < 10; ++i) {
    queue.PushBack(i);
    EXPECT_EQ(queue.size(), i + 1);
  }
  for (size_t i = 0; i < 10; ++i) {
    EXPECT_EQ(queue.Front(/*reader=*/0), absl::optional<size_t>(i));
    EXPECT_EQ(queue.PopFront(/*reader=*/0), absl::optional<size_t>(i));
    EXPECT_EQ(queue.size(), 10 - i - 1);
  }
}

TEST(MultiReaderQueueTest, ThreeHeadsAddAllRemoveAllPerHead) {
  MultiReaderQueue<size_t> queue =
      MultiReaderQueue<size_t>(/*readers_count=*/3);
  for (size_t i = 0; i < 10; ++i) {
    queue.PushBack(i);
    EXPECT_EQ(queue.size(), i + 1);
  }
  for (size_t i = 0; i < 10; ++i) {
    absl::optional<size_t> value = queue.PopFront(/*reader=*/0);
    EXPECT_EQ(queue.size(), 10lu);
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), i);
  }
  for (size_t i = 0; i < 10; ++i) {
    absl::optional<size_t> value = queue.PopFront(/*reader=*/1);
    EXPECT_EQ(queue.size(), 10lu);
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), i);
  }
  for (size_t i = 0; i < 10; ++i) {
    absl::optional<size_t> value = queue.PopFront(/*reader=*/2);
    EXPECT_EQ(queue.size(), 10 - i - 1);
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), i);
  }
}

TEST(MultiReaderQueueTest, ThreeHeadsAddAllRemoveAll) {
  MultiReaderQueue<size_t> queue =
      MultiReaderQueue<size_t>(/*readers_count=*/3);
  for (size_t i = 0; i < 10; ++i) {
    queue.PushBack(i);
    EXPECT_EQ(queue.size(), i + 1);
  }
  for (size_t i = 0; i < 10; ++i) {
    absl::optional<size_t> value1 = queue.PopFront(/*reader=*/0);
    absl::optional<size_t> value2 = queue.PopFront(/*reader=*/1);
    absl::optional<size_t> value3 = queue.PopFront(/*reader=*/2);
    EXPECT_EQ(queue.size(), 10 - i - 1);
    ASSERT_TRUE(value1.has_value());
    ASSERT_TRUE(value2.has_value());
    ASSERT_TRUE(value3.has_value());
    EXPECT_EQ(value1.value(), i);
    EXPECT_EQ(value2.value(), i);
    EXPECT_EQ(value3.value(), i);
  }
}

TEST(MultiReaderQueueTest, AddReaderSeeElementsOnlyFromReaderToCopy) {
  MultiReaderQueue<size_t> queue =
      MultiReaderQueue<size_t>(/*readers_count=*/2);
  for (size_t i = 0; i < 10; ++i) {
    queue.PushBack(i);
  }
  for (size_t i = 0; i < 5; ++i) {
    queue.PopFront(0);
  }

  queue.AddReader(/*reader=*/2, /*reader_to_copy=*/0);

  EXPECT_EQ(queue.readers_count(), 3lu);
  for (size_t i = 5; i < 10; ++i) {
    absl::optional<size_t> value = queue.PopFront(/*reader=*/2);
    EXPECT_EQ(queue.size(/*reader=*/2), 10 - i - 1);
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), i);
  }
}

TEST(MultiReaderQueueTest, AddReaderWithoutReaderToCopySeeFullQueue) {
  MultiReaderQueue<size_t> queue =
      MultiReaderQueue<size_t>(/*readers_count=*/2);
  for (size_t i = 0; i < 10; ++i) {
    queue.PushBack(i);
  }
  for (size_t i = 0; i < 5; ++i) {
    queue.PopFront(/*reader=*/0);
  }

  queue.AddReader(/*reader=*/2);

  EXPECT_EQ(queue.readers_count(), 3lu);
  for (size_t i = 0; i < 10; ++i) {
    absl::optional<size_t> value = queue.PopFront(/*reader=*/2);
    EXPECT_EQ(queue.size(/*reader=*/2), 10 - i - 1);
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), i);
  }
}

TEST(MultiReaderQueueTest, RemoveReaderWontChangeOthers) {
  MultiReaderQueue<size_t> queue =
      MultiReaderQueue<size_t>(/*readers_count=*/2);
  for (size_t i = 0; i < 10; ++i) {
    queue.PushBack(i);
  }
  EXPECT_EQ(queue.size(/*reader=*/1), 10lu);

  queue.RemoveReader(0);

  EXPECT_EQ(queue.readers_count(), 1lu);
  EXPECT_EQ(queue.size(/*reader=*/1), 10lu);
}

TEST(MultiReaderQueueTest, RemoveLastReaderMakesQueueEmpty) {
  MultiReaderQueue<size_t> queue =
      MultiReaderQueue<size_t>(/*readers_count=*/1);
  for (size_t i = 0; i < 10; ++i) {
    queue.PushBack(i);
  }
  EXPECT_EQ(queue.size(), 10lu);

  queue.RemoveReader(0);

  EXPECT_EQ(queue.size(), 0lu);
  EXPECT_EQ(queue.readers_count(), 0lu);
}

}  // namespace
}  // namespace webrtc
