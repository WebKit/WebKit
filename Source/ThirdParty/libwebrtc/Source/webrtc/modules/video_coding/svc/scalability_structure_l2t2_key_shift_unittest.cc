/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/svc/scalability_structure_l2t2_key_shift.h"

#include <vector>

#include "api/array_view.h"
#include "api/transport/rtp/dependency_descriptor.h"
#include "common_video/generic_frame_descriptor/generic_frame_info.h"
#include "modules/video_coding/svc/scalability_structure_test_helpers.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::SizeIs;

// S1T1     3   7
//         /   /
// S1T0   1---5---9
//        |
// S0T1   |   4   8
//        |  /   /
// S0T0   0-2---6
// Time-> 0 1 2 3 4
TEST(ScalabilityStructureL2T2KeyShiftTest, DecodeTargetsAreEnabledByDefault) {
  ScalabilityStructureL2T2KeyShift structure;
  ScalabilityStructureWrapper wrapper(structure);
  std::vector<GenericFrameInfo> frames;
  wrapper.GenerateFrames(/*num_temporal_units=*/5, frames);
  ASSERT_THAT(frames, SizeIs(10));

  EXPECT_EQ(frames[0].spatial_id, 0);
  EXPECT_EQ(frames[1].spatial_id, 1);
  EXPECT_EQ(frames[2].spatial_id, 0);
  EXPECT_EQ(frames[3].spatial_id, 1);
  EXPECT_EQ(frames[4].spatial_id, 0);
  EXPECT_EQ(frames[5].spatial_id, 1);
  EXPECT_EQ(frames[6].spatial_id, 0);
  EXPECT_EQ(frames[7].spatial_id, 1);
  EXPECT_EQ(frames[8].spatial_id, 0);
  EXPECT_EQ(frames[9].spatial_id, 1);

  // spatial_id = 0 has the temporal shift.
  EXPECT_EQ(frames[0].temporal_id, 0);
  EXPECT_EQ(frames[2].temporal_id, 0);
  EXPECT_EQ(frames[4].temporal_id, 1);
  EXPECT_EQ(frames[6].temporal_id, 0);
  EXPECT_EQ(frames[8].temporal_id, 1);

  // spatial_id = 1 hasn't temporal shift.
  EXPECT_EQ(frames[1].temporal_id, 0);
  EXPECT_EQ(frames[3].temporal_id, 1);
  EXPECT_EQ(frames[5].temporal_id, 0);
  EXPECT_EQ(frames[7].temporal_id, 1);
  EXPECT_EQ(frames[9].temporal_id, 0);

  // Key frame diff.
  EXPECT_THAT(frames[0].frame_diffs, IsEmpty());
  EXPECT_THAT(frames[1].frame_diffs, ElementsAre(1));
  // S0T0 frame diffs
  EXPECT_THAT(frames[2].frame_diffs, ElementsAre(2));
  EXPECT_THAT(frames[6].frame_diffs, ElementsAre(4));
  // S1T0 frame diffs
  EXPECT_THAT(frames[5].frame_diffs, ElementsAre(4));
  EXPECT_THAT(frames[9].frame_diffs, ElementsAre(4));
  // T1 frames refer T0 frame of same spatial layer which is 2 frame ids away.
  EXPECT_THAT(frames[3].frame_diffs, ElementsAre(2));
  EXPECT_THAT(frames[4].frame_diffs, ElementsAre(2));
  EXPECT_THAT(frames[7].frame_diffs, ElementsAre(2));
  EXPECT_THAT(frames[8].frame_diffs, ElementsAre(2));
}

// S1T0   1---4---7
//        |
// S0T1   |   3   6
//        |  /   /
// S0T0   0-2---5--
// Time-> 0 1 2 3 4
TEST(ScalabilityStructureL2T2KeyShiftTest, DisableS1T1Layer) {
  ScalabilityStructureL2T2KeyShift structure;
  structure.OnRatesUpdated(EnableTemporalLayers(/*s0=*/2, /*s1=*/1));
  ScalabilityStructureWrapper wrapper(structure);
  std::vector<GenericFrameInfo> frames;
  wrapper.GenerateFrames(/*num_temporal_units=*/5, frames);
  ASSERT_THAT(frames, SizeIs(8));

  EXPECT_EQ(frames[0].spatial_id, 0);
  EXPECT_EQ(frames[1].spatial_id, 1);
  EXPECT_EQ(frames[2].spatial_id, 0);
  EXPECT_EQ(frames[3].spatial_id, 0);
  EXPECT_EQ(frames[4].spatial_id, 1);
  EXPECT_EQ(frames[5].spatial_id, 0);
  EXPECT_EQ(frames[6].spatial_id, 0);
  EXPECT_EQ(frames[7].spatial_id, 1);

  // spatial_id = 0 has the temporal shift.
  EXPECT_EQ(frames[0].temporal_id, 0);
  EXPECT_EQ(frames[2].temporal_id, 0);
  EXPECT_EQ(frames[3].temporal_id, 1);
  EXPECT_EQ(frames[5].temporal_id, 0);
  EXPECT_EQ(frames[6].temporal_id, 1);

  // spatial_id = 1 has single temporal layer.
  EXPECT_EQ(frames[1].temporal_id, 0);
  EXPECT_EQ(frames[4].temporal_id, 0);
  EXPECT_EQ(frames[5].temporal_id, 0);
}

// S1T1     3  |
//         /   |
// S1T0   1---5+--7
//        |    |
// S0T1   |   4|
//        |  / |
// S0T0   0-2--+6---8
// Time-> 0 1 2 3 4 5
TEST(ScalabilityStructureL2T2KeyShiftTest, DisableT1LayersAfterFewFrames) {
  ScalabilityStructureL2T2KeyShift structure;
  ScalabilityStructureWrapper wrapper(structure);
  std::vector<GenericFrameInfo> frames;

  wrapper.GenerateFrames(/*num_temporal_units=*/3, frames);
  EXPECT_THAT(frames, SizeIs(6));
  structure.OnRatesUpdated(EnableTemporalLayers(/*s0=*/1, /*s1=*/1));
  wrapper.GenerateFrames(/*num_temporal_units=*/3, frames);
  ASSERT_THAT(frames, SizeIs(9));

  // Skip validation before T1 was disabled as that is covered by the test
  // where no layers are disabled.
  EXPECT_EQ(frames[6].spatial_id, 0);
  EXPECT_EQ(frames[7].spatial_id, 1);
  EXPECT_EQ(frames[8].spatial_id, 0);

  EXPECT_EQ(frames[6].temporal_id, 0);
  EXPECT_EQ(frames[7].temporal_id, 0);
  EXPECT_EQ(frames[8].temporal_id, 0);

  EXPECT_TRUE(wrapper.FrameReferencesAreValid(frames));
}

// S1T1     1   3
//         /   /
// S1T0   0---2
// Time-> 0 1 2 3 4 5
TEST(ScalabilityStructureL2T2KeyShiftTest, DisableS0FromTheStart) {
  ScalabilityStructureL2T2KeyShift structure;
  ScalabilityStructureWrapper wrapper(structure);
  std::vector<GenericFrameInfo> frames;

  structure.OnRatesUpdated(EnableTemporalLayers(/*s0=*/0, /*s1=*/2));
  wrapper.GenerateFrames(/*num_temporal_units=*/4, frames);
  EXPECT_THAT(frames, SizeIs(4));

  EXPECT_EQ(frames[0].spatial_id, 1);
  EXPECT_EQ(frames[1].spatial_id, 1);
  EXPECT_EQ(frames[2].spatial_id, 1);
  EXPECT_EQ(frames[3].spatial_id, 1);

  EXPECT_EQ(frames[0].temporal_id, 0);
  EXPECT_EQ(frames[1].temporal_id, 1);
  EXPECT_EQ(frames[2].temporal_id, 0);
  EXPECT_EQ(frames[3].temporal_id, 1);

  EXPECT_TRUE(wrapper.FrameReferencesAreValid(frames));
}

// S1T1     3  |6   8
//         /   /   /
// S1T0   1---5+--7
//        |    |
// S0T1   |   4|
//        |  / |
// S0T0   0-2  |
// Time-> 0 1 2 3 4 5
TEST(ScalabilityStructureL2T2KeyShiftTest, DisableS0AfterFewFrames) {
  ScalabilityStructureL2T2KeyShift structure;
  ScalabilityStructureWrapper wrapper(structure);
  std::vector<GenericFrameInfo> frames;

  wrapper.GenerateFrames(/*num_temporal_units=*/3, frames);
  EXPECT_THAT(frames, SizeIs(6));
  structure.OnRatesUpdated(EnableTemporalLayers(/*s0=*/0, /*s1=*/2));
  wrapper.GenerateFrames(/*num_temporal_units=*/3, frames);
  ASSERT_THAT(frames, SizeIs(9));

  // Expect frame[6] is delta frame.
  EXPECT_THAT(frames[6].frame_diffs, ElementsAre(1));
  // Skip validation before S0 was disabled as that should be covered by
  // test where no layers are disabled.
  EXPECT_EQ(frames[6].spatial_id, 1);
  EXPECT_EQ(frames[7].spatial_id, 1);
  EXPECT_EQ(frames[8].spatial_id, 1);

  EXPECT_EQ(frames[6].temporal_id, 1);
  EXPECT_EQ(frames[7].temporal_id, 0);
  EXPECT_EQ(frames[8].temporal_id, 1);

  EXPECT_TRUE(wrapper.FrameReferencesAreValid(frames));
}

// S1T1     3| |  8
//         / | | /
// S1T0   1  | |6
//        |  | ||
// S0T1   |  |4||
//        |  / ||
// S0T0   0-2| |5-7
// Time-> 0 1 2 3 4 5
TEST(ScalabilityStructureL2T2KeyShiftTest, ReenableS1TriggersKeyFrame) {
  ScalabilityStructureL2T2KeyShift structure;
  ScalabilityStructureWrapper wrapper(structure);
  std::vector<GenericFrameInfo> frames;

  wrapper.GenerateFrames(/*num_temporal_units=*/2, frames);
  EXPECT_THAT(frames, SizeIs(4));

  structure.OnRatesUpdated(EnableTemporalLayers(/*s0=*/2, /*s1=*/0));
  wrapper.GenerateFrames(/*num_temporal_units=*/1, frames);
  EXPECT_THAT(frames, SizeIs(5));

  structure.OnRatesUpdated(EnableTemporalLayers(/*s0=*/2, /*s1=*/2));
  wrapper.GenerateFrames(/*num_temporal_units=*/2, frames);
  ASSERT_THAT(frames, SizeIs(9));

  EXPECT_THAT(frames[4].spatial_id, 0);
  EXPECT_THAT(frames[4].temporal_id, 1);

  // Expect frame[5] to be a key frame.
  EXPECT_TRUE(wrapper.FrameReferencesAreValid(
      rtc::MakeArrayView(frames.data() + 5, 4)));

  EXPECT_THAT(frames[5].spatial_id, 0);
  EXPECT_THAT(frames[6].spatial_id, 1);
  EXPECT_THAT(frames[7].spatial_id, 0);
  EXPECT_THAT(frames[8].spatial_id, 1);

  // S0 should do temporal shift after the key frame.
  EXPECT_THAT(frames[5].temporal_id, 0);
  EXPECT_THAT(frames[7].temporal_id, 0);

  // No temporal shift for the top spatial layer.
  EXPECT_THAT(frames[6].temporal_id, 0);
  EXPECT_THAT(frames[8].temporal_id, 1);
}

TEST(ScalabilityStructureL2T2KeyShiftTest, EnableOnlyS0T0FromTheStart) {
  ScalabilityStructureL2T2KeyShift structure;
  ScalabilityStructureWrapper wrapper(structure);
  std::vector<GenericFrameInfo> frames;

  structure.OnRatesUpdated(EnableTemporalLayers(/*s0=*/1, /*s1=*/0));
  wrapper.GenerateFrames(/*num_temporal_units=*/3, frames);
  ASSERT_THAT(frames, SizeIs(3));

  EXPECT_EQ(frames[0].spatial_id, 0);
  EXPECT_EQ(frames[1].spatial_id, 0);
  EXPECT_EQ(frames[2].spatial_id, 0);

  EXPECT_EQ(frames[0].temporal_id, 0);
  EXPECT_EQ(frames[1].temporal_id, 0);
  EXPECT_EQ(frames[2].temporal_id, 0);

  EXPECT_TRUE(wrapper.FrameReferencesAreValid(frames));
}

// S1T1     3|
//         / |
// S1T0   1  |
//        |  |
// S0T1   |  |
//        |  |
// S0T0   0-2+4-5-6
// Time-> 0 1 2 3 4
TEST(ScalabilityStructureL2T2KeyShiftTest, EnableOnlyS0T0AfterFewFrames) {
  ScalabilityStructureL2T2KeyShift structure;
  ScalabilityStructureWrapper wrapper(structure);
  std::vector<GenericFrameInfo> frames;

  wrapper.GenerateFrames(/*num_temporal_units=*/2, frames);
  EXPECT_THAT(frames, SizeIs(4));
  structure.OnRatesUpdated(EnableTemporalLayers(/*s0=*/1, /*s1=*/0));
  wrapper.GenerateFrames(/*num_temporal_units=*/3, frames);
  ASSERT_THAT(frames, SizeIs(7));

  EXPECT_EQ(frames[4].spatial_id, 0);
  EXPECT_EQ(frames[5].spatial_id, 0);
  EXPECT_EQ(frames[6].spatial_id, 0);

  EXPECT_EQ(frames[4].temporal_id, 0);
  EXPECT_EQ(frames[5].temporal_id, 0);
  EXPECT_EQ(frames[6].temporal_id, 0);

  EXPECT_TRUE(wrapper.FrameReferencesAreValid(frames));
}

TEST(ScalabilityStructureL2T2KeyShiftTest, EnableOnlyS1T0FromTheStart) {
  ScalabilityStructureL2T2KeyShift structure;
  ScalabilityStructureWrapper wrapper(structure);
  std::vector<GenericFrameInfo> frames;

  structure.OnRatesUpdated(EnableTemporalLayers(/*s0=*/0, /*s1=*/1));
  wrapper.GenerateFrames(/*num_temporal_units=*/3, frames);
  ASSERT_THAT(frames, SizeIs(3));

  EXPECT_EQ(frames[0].spatial_id, 1);
  EXPECT_EQ(frames[1].spatial_id, 1);
  EXPECT_EQ(frames[2].spatial_id, 1);

  EXPECT_EQ(frames[0].temporal_id, 0);
  EXPECT_EQ(frames[1].temporal_id, 0);
  EXPECT_EQ(frames[2].temporal_id, 0);

  EXPECT_TRUE(wrapper.FrameReferencesAreValid(frames));
}

// S1T1     3|
//         / |
// S1T0   1--+4-5-6
//        |  |
// S0T1   |  |
//        |  |
// S0T0   0-2|
// Time-> 0 1 2 3 4
TEST(ScalabilityStructureL2T2KeyShiftTest, EnableOnlyS1T0AfterFewFrames) {
  ScalabilityStructureL2T2KeyShift structure;
  ScalabilityStructureWrapper wrapper(structure);
  std::vector<GenericFrameInfo> frames;

  wrapper.GenerateFrames(/*num_temporal_units=*/2, frames);
  EXPECT_THAT(frames, SizeIs(4));
  structure.OnRatesUpdated(EnableTemporalLayers(/*s0=*/0, /*s1=*/1));
  wrapper.GenerateFrames(/*num_temporal_units=*/3, frames);
  ASSERT_THAT(frames, SizeIs(7));

  EXPECT_EQ(frames[4].spatial_id, 1);
  EXPECT_EQ(frames[5].spatial_id, 1);
  EXPECT_EQ(frames[6].spatial_id, 1);

  EXPECT_EQ(frames[4].temporal_id, 0);
  EXPECT_EQ(frames[5].temporal_id, 0);
  EXPECT_EQ(frames[6].temporal_id, 0);

  EXPECT_TRUE(wrapper.FrameReferencesAreValid(frames));
}

}  // namespace
}  // namespace webrtc
