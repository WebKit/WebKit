/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/timing/simulator/test/encoded_frame_generators.h"

#include "test/create_test_environment.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc::video_timing_simulator {
namespace {

using ::testing::Eq;

TEST(SingleLayerEncodedFrameGeneratorTest, ProducesNonLayeredReferences) {
  SingleLayerEncodedFrameGenerator generator(CreateTestEnvironment());
  auto keyframe = generator.NextEncodedFrame();
  EXPECT_TRUE(keyframe->is_keyframe());
  for (int i = 1; i < 8; ++i) {
    auto delta_frame = generator.NextEncodedFrame();
    EXPECT_FALSE(delta_frame->is_keyframe());
    EXPECT_THAT(delta_frame->num_references, Eq(1));
    EXPECT_THAT(delta_frame->references[0], Eq(i - 1));
  }
}

TEST(TemporalLayersEncodedFrameGenerator, ProducesLayeredReferences) {
  TemporalLayersEncodedFrameGenerator generator(CreateTestEnvironment());

  auto keyframe = generator.NextEncodedFrame();
  EXPECT_TRUE(keyframe->is_keyframe());

  auto tl2a = generator.NextEncodedFrame();
  EXPECT_FALSE(tl2a->is_keyframe());
  EXPECT_THAT(tl2a->references[0], Eq(0));

  auto tl1 = generator.NextEncodedFrame();
  EXPECT_FALSE(tl1->is_keyframe());
  EXPECT_THAT(tl1->references[0], Eq(0));

  auto tl2b = generator.NextEncodedFrame();
  EXPECT_FALSE(tl2b->is_keyframe());
  EXPECT_THAT(tl2b->references[0], Eq(2));

  auto tl0_next_gop = generator.NextEncodedFrame();
  EXPECT_FALSE(tl0_next_gop->is_keyframe());
  EXPECT_THAT(tl0_next_gop->references[0], Eq(0));
}

}  // namespace
}  // namespace webrtc::video_timing_simulator
