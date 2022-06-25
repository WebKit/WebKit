/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc/clipping_predictor_level_buffer.h"

#include <algorithm>

#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::Eq;
using ::testing::Optional;

class ClippingPredictorLevelBufferParametrization
    : public ::testing::TestWithParam<int> {
 protected:
  int capacity() const { return GetParam(); }
};

TEST_P(ClippingPredictorLevelBufferParametrization, CheckEmptyBufferSize) {
  ClippingPredictorLevelBuffer buffer(capacity());
  EXPECT_EQ(buffer.Capacity(), std::max(capacity(), 1));
  EXPECT_EQ(buffer.Size(), 0);
}

TEST_P(ClippingPredictorLevelBufferParametrization, CheckHalfEmptyBufferSize) {
  ClippingPredictorLevelBuffer buffer(capacity());
  for (int i = 0; i < buffer.Capacity() / 2; ++i) {
    buffer.Push({2, 4});
  }
  EXPECT_EQ(buffer.Capacity(), std::max(capacity(), 1));
  EXPECT_EQ(buffer.Size(), std::max(capacity(), 1) / 2);
}

TEST_P(ClippingPredictorLevelBufferParametrization, CheckFullBufferSize) {
  ClippingPredictorLevelBuffer buffer(capacity());
  for (int i = 0; i < buffer.Capacity(); ++i) {
    buffer.Push({2, 4});
  }
  EXPECT_EQ(buffer.Capacity(), std::max(capacity(), 1));
  EXPECT_EQ(buffer.Size(), std::max(capacity(), 1));
}

TEST_P(ClippingPredictorLevelBufferParametrization, CheckLargeBufferSize) {
  ClippingPredictorLevelBuffer buffer(capacity());
  for (int i = 0; i < 2 * buffer.Capacity(); ++i) {
    buffer.Push({2, 4});
  }
  EXPECT_EQ(buffer.Capacity(), std::max(capacity(), 1));
  EXPECT_EQ(buffer.Size(), std::max(capacity(), 1));
}

TEST_P(ClippingPredictorLevelBufferParametrization, CheckSizeAfterReset) {
  ClippingPredictorLevelBuffer buffer(capacity());
  buffer.Push({1, 1});
  buffer.Push({1, 1});
  buffer.Reset();
  EXPECT_EQ(buffer.Capacity(), std::max(capacity(), 1));
  EXPECT_EQ(buffer.Size(), 0);
  buffer.Push({1, 1});
  EXPECT_EQ(buffer.Capacity(), std::max(capacity(), 1));
  EXPECT_EQ(buffer.Size(), 1);
}

INSTANTIATE_TEST_SUITE_P(ClippingPredictorLevelBufferTest,
                         ClippingPredictorLevelBufferParametrization,
                         ::testing::Values(-1, 0, 1, 123));

TEST(ClippingPredictorLevelBufferTest, CheckMetricsAfterFullBuffer) {
  ClippingPredictorLevelBuffer buffer(/*capacity=*/2);
  buffer.Push({1, 2});
  buffer.Push({3, 6});
  EXPECT_THAT(buffer.ComputePartialMetrics(/*delay=*/0, /*num_items=*/1),
              Optional(Eq(ClippingPredictorLevelBuffer::Level{3, 6})));
  EXPECT_THAT(buffer.ComputePartialMetrics(/*delay=*/1, /*num_items=*/1),
              Optional(Eq(ClippingPredictorLevelBuffer::Level{1, 2})));
  EXPECT_THAT(buffer.ComputePartialMetrics(/*delay=*/0, /*num_items=*/2),
              Optional(Eq(ClippingPredictorLevelBuffer::Level{2, 6})));
}

TEST(ClippingPredictorLevelBufferTest, CheckMetricsAfterPushBeyondCapacity) {
  ClippingPredictorLevelBuffer buffer(/*capacity=*/2);
  buffer.Push({1, 1});
  buffer.Push({3, 6});
  buffer.Push({5, 10});
  buffer.Push({7, 14});
  buffer.Push({6, 12});
  EXPECT_THAT(buffer.ComputePartialMetrics(/*delay=*/0, /*num_items=*/1),
              Optional(Eq(ClippingPredictorLevelBuffer::Level{6, 12})));
  EXPECT_THAT(buffer.ComputePartialMetrics(/*delay=*/1, /*num_items=*/1),
              Optional(Eq(ClippingPredictorLevelBuffer::Level{7, 14})));
  EXPECT_THAT(buffer.ComputePartialMetrics(/*delay=*/0, /*num_items=*/2),
              Optional(Eq(ClippingPredictorLevelBuffer::Level{6.5f, 14})));
}

TEST(ClippingPredictorLevelBufferTest, CheckMetricsAfterTooFewItems) {
  ClippingPredictorLevelBuffer buffer(/*capacity=*/4);
  buffer.Push({1, 2});
  buffer.Push({3, 6});
  EXPECT_EQ(buffer.ComputePartialMetrics(/*delay=*/0, /*num_items=*/3),
            absl::nullopt);
  EXPECT_EQ(buffer.ComputePartialMetrics(/*delay=*/2, /*num_items=*/1),
            absl::nullopt);
}

TEST(ClippingPredictorLevelBufferTest, CheckMetricsAfterReset) {
  ClippingPredictorLevelBuffer buffer(/*capacity=*/2);
  buffer.Push({1, 2});
  buffer.Reset();
  buffer.Push({5, 10});
  buffer.Push({7, 14});
  EXPECT_THAT(buffer.ComputePartialMetrics(/*delay=*/0, /*num_items=*/1),
              Optional(Eq(ClippingPredictorLevelBuffer::Level{7, 14})));
  EXPECT_THAT(buffer.ComputePartialMetrics(/*delay=*/0, /*num_items=*/2),
              Optional(Eq(ClippingPredictorLevelBuffer::Level{6, 14})));
  EXPECT_THAT(buffer.ComputePartialMetrics(/*delay=*/1, /*num_items=*/1),
              Optional(Eq(ClippingPredictorLevelBuffer::Level{5, 10})));
}

}  // namespace
}  // namespace webrtc
