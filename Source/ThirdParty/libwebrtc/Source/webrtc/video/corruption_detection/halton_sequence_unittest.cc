/*
 * Copyright 2024 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include "video/corruption_detection/halton_sequence.h"

#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::DoubleEq;
using ::testing::ElementsAre;

TEST(HaltonSequenceTest, ShouldGenerateBase2SequenceByDefault) {
  HaltonSequence halton_sequence;
  EXPECT_THAT(halton_sequence.GetNext(), ElementsAre(DoubleEq(0.0)));
  EXPECT_THAT(halton_sequence.GetNext(), ElementsAre(DoubleEq(1.0 / 2)));
  EXPECT_THAT(halton_sequence.GetNext(), ElementsAre(DoubleEq(1.0 / 4)));
  EXPECT_THAT(halton_sequence.GetNext(), ElementsAre(DoubleEq(3.0 / 4)));
  EXPECT_THAT(halton_sequence.GetNext(), ElementsAre(DoubleEq(1.0 / 8)));
  EXPECT_THAT(halton_sequence.GetNext(), ElementsAre(DoubleEq(5.0 / 8)));
  EXPECT_THAT(halton_sequence.GetNext(), ElementsAre(DoubleEq(3.0 / 8)));
}

TEST(HaltonSequenceTest,
     ShouldGenerateBase2Base3SequencesWhenCreatedAs2Dimensional) {
  HaltonSequence halton_sequence(2);
  EXPECT_THAT(halton_sequence.GetNext(),
              ElementsAre(DoubleEq(0.0), DoubleEq(0.0)));
  EXPECT_THAT(halton_sequence.GetNext(),
              ElementsAre(DoubleEq(1.0 / 2), DoubleEq(1.0 / 3)));
  EXPECT_THAT(halton_sequence.GetNext(),
              ElementsAre(DoubleEq(1.0 / 4), DoubleEq(2.0 / 3)));
  EXPECT_THAT(halton_sequence.GetNext(),
              ElementsAre(DoubleEq(3.0 / 4), DoubleEq(1.0 / 9)));
  EXPECT_THAT(halton_sequence.GetNext(),
              ElementsAre(DoubleEq(1.0 / 8), DoubleEq(4.0 / 9)));
  EXPECT_THAT(halton_sequence.GetNext(),
              ElementsAre(DoubleEq(5.0 / 8), DoubleEq(7.0 / 9)));
  EXPECT_THAT(halton_sequence.GetNext(),
              ElementsAre(DoubleEq(3.0 / 8), DoubleEq(2.0 / 9)));
}

TEST(HaltonSequenceTest, ShouldRestartSequenceWhenResetIsCalled) {
  HaltonSequence halton_sequence;
  EXPECT_THAT(halton_sequence.GetCurrentIndex(), 0);
  EXPECT_THAT(halton_sequence.GetNext(), ElementsAre(DoubleEq(0.0)));
  EXPECT_THAT(halton_sequence.GetCurrentIndex(), 1);
  EXPECT_THAT(halton_sequence.GetNext(), ElementsAre(DoubleEq(1.0 / 2)));
  EXPECT_THAT(halton_sequence.GetCurrentIndex(), 2);
  halton_sequence.Reset();
  EXPECT_THAT(halton_sequence.GetCurrentIndex(), 0);
  EXPECT_THAT(halton_sequence.GetNext(), ElementsAre(DoubleEq(0.0)));
}

TEST(HaltonSequenceTest, ShouldSetCurrentIndexWhenSetCurrentIndexIsCalled) {
  HaltonSequence halton_sequence;
  EXPECT_THAT(halton_sequence.GetCurrentIndex(), 0);
  halton_sequence.SetCurrentIndex(3);
  EXPECT_THAT(halton_sequence.GetCurrentIndex(), 3);
  EXPECT_THAT(halton_sequence.GetNext(), ElementsAre(DoubleEq(3.0 / 4)));
}

}  // namespace
}  // namespace webrtc
