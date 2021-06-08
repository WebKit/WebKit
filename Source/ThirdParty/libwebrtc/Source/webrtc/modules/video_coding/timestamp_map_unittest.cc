/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/timestamp_map.h"

#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace video_coding {
namespace {
constexpr int kTimestampMapSize = 6;
constexpr int kTimestamp1 = 1;
constexpr int kTimestamp2 = 2;
constexpr int kNoExistingTimestamp3 = 3;
constexpr int kTimestamp4 = 4;
constexpr int kTimestamp5 = 5;
constexpr int kTimestamp6 = 6;
constexpr int kTimestamp7 = 7;
constexpr int64_t kRenderTime1 = 1000;
constexpr int64_t kRenderTime2 = 2000;
constexpr int64_t kRenderTime4 = 4000;
constexpr int64_t kRenderTime5 = 5000;
constexpr int64_t kRenderTime6 = 6000;
constexpr int64_t kRenderTime7 = 7000;
}  // namespace

class VcmTimestampMapTest : public ::testing::Test {
 protected:
  VcmTimestampMapTest() : _timestampMap(kTimestampMapSize) {}

  void SetUp() override {
    _timestampMap.Add(kTimestamp1, VCMFrameInformation({kRenderTime1}));
    _timestampMap.Add(kTimestamp2, VCMFrameInformation({kRenderTime2}));
    _timestampMap.Add(kTimestamp4, VCMFrameInformation({kRenderTime4}));
  }

  VCMTimestampMap _timestampMap;
};

TEST_F(VcmTimestampMapTest, PopExistingFrameInfo) {
  EXPECT_EQ(_timestampMap.Size(), 3u);
  auto frameInfo = _timestampMap.Pop(kTimestamp1);
  ASSERT_TRUE(frameInfo);
  EXPECT_EQ(frameInfo->renderTimeMs, kRenderTime1);
  frameInfo = _timestampMap.Pop(kTimestamp2);
  ASSERT_TRUE(frameInfo);
  EXPECT_EQ(frameInfo->renderTimeMs, kRenderTime2);
  frameInfo = _timestampMap.Pop(kTimestamp4);
  ASSERT_TRUE(frameInfo);
  EXPECT_EQ(frameInfo->renderTimeMs, kRenderTime4);
}

TEST_F(VcmTimestampMapTest, PopNonexistingClearsOlderFrameInfos) {
  auto frameInfo = _timestampMap.Pop(kNoExistingTimestamp3);
  EXPECT_FALSE(frameInfo);
  EXPECT_EQ(_timestampMap.Size(), 1u);
}

TEST_F(VcmTimestampMapTest, SizeIsIncrementedWhenAddingNewFrameInfo) {
  EXPECT_EQ(_timestampMap.Size(), 3u);
  _timestampMap.Add(kTimestamp5, VCMFrameInformation({kRenderTime5}));
  EXPECT_EQ(_timestampMap.Size(), 4u);
  _timestampMap.Add(kTimestamp6, VCMFrameInformation({kRenderTime6}));
  EXPECT_EQ(_timestampMap.Size(), 5u);
}

TEST_F(VcmTimestampMapTest, SizeIsDecreasedWhenPoppingFrameInfo) {
  EXPECT_EQ(_timestampMap.Size(), 3u);
  EXPECT_TRUE(_timestampMap.Pop(kTimestamp1));
  EXPECT_EQ(_timestampMap.Size(), 2u);
  EXPECT_TRUE(_timestampMap.Pop(kTimestamp2));
  EXPECT_EQ(_timestampMap.Size(), 1u);
  EXPECT_FALSE(_timestampMap.Pop(kNoExistingTimestamp3));
  EXPECT_EQ(_timestampMap.Size(), 1u);
  EXPECT_TRUE(_timestampMap.Pop(kTimestamp4));
  EXPECT_EQ(_timestampMap.Size(), 0u);
}

TEST_F(VcmTimestampMapTest, ClearEmptiesMap) {
  EXPECT_EQ(_timestampMap.Size(), 3u);
  _timestampMap.Clear();
  EXPECT_EQ(_timestampMap.Size(), 0u);
  // Clear empty map does nothing.
  _timestampMap.Clear();
  EXPECT_EQ(_timestampMap.Size(), 0u);
}

TEST_F(VcmTimestampMapTest, PopLastAddedClearsMap) {
  EXPECT_EQ(_timestampMap.Size(), 3u);
  EXPECT_TRUE(_timestampMap.Pop(kTimestamp4));
  EXPECT_EQ(_timestampMap.Size(), 0u);
}

TEST_F(VcmTimestampMapTest, LastAddedIsDiscardedIfMapGetsFull) {
  EXPECT_EQ(_timestampMap.Size(), 3u);
  _timestampMap.Add(kTimestamp5, VCMFrameInformation({kRenderTime5}));
  EXPECT_EQ(_timestampMap.Size(), 4u);
  _timestampMap.Add(kTimestamp6, VCMFrameInformation({kRenderTime6}));
  EXPECT_EQ(_timestampMap.Size(), 5u);
  _timestampMap.Add(kTimestamp7, VCMFrameInformation({kRenderTime7}));
  // Size is not incremented since the oldest element is discarded.
  EXPECT_EQ(_timestampMap.Size(), 5u);
  EXPECT_FALSE(_timestampMap.Pop(kTimestamp1));
  EXPECT_TRUE(_timestampMap.Pop(kTimestamp2));
  EXPECT_TRUE(_timestampMap.Pop(kTimestamp4));
  EXPECT_TRUE(_timestampMap.Pop(kTimestamp5));
  EXPECT_TRUE(_timestampMap.Pop(kTimestamp6));
  EXPECT_TRUE(_timestampMap.Pop(kTimestamp7));
  EXPECT_EQ(_timestampMap.Size(), 0u);
}

}  // namespace video_coding
}  // namespace webrtc
