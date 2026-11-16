/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/frame_dependencies_calculator.h"

#include "common_video/generic_frame_descriptor/generic_frame_info.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

constexpr CodecBufferUsage ReferenceAndUpdate(int id) {
  return CodecBufferUsage(id, /*referenced=*/true, /*updated=*/true);
}
constexpr CodecBufferUsage Reference(int id) {
  return CodecBufferUsage(id, /*referenced=*/true, /*updated=*/false);
}
constexpr CodecBufferUsage Update(int id) {
  return CodecBufferUsage(id, /*referenced=*/false, /*updated=*/true);
}

TEST(FrameDependenciesCalculatorTest, SingleLayer) {
  CodecBufferUsage pattern[] = {ReferenceAndUpdate(0)};
  FrameDependenciesCalculator calculator;

  EXPECT_THAT(calculator.FromBuffersUsage(/*frame_id=*/1, pattern), IsEmpty());
  EXPECT_THAT(calculator.FromBuffersUsage(/*frame_id=*/3, pattern),
              ElementsAre(1));
  EXPECT_THAT(calculator.FromBuffersUsage(/*frame_id=*/6, pattern),
              ElementsAre(3));
}

TEST(FrameDependenciesCalculatorTest, TwoTemporalLayers) {
  // Shortened 4-frame pattern:
  // T1:  2---4   6---8 ...
  //      /   /   /   /
  // T0: 1---3---5---7 ...
  CodecBufferUsage pattern0[] = {ReferenceAndUpdate(0)};
  CodecBufferUsage pattern1[] = {Reference(0), Update(1)};
  CodecBufferUsage pattern2[] = {ReferenceAndUpdate(0)};
  CodecBufferUsage pattern3[] = {Reference(0), Reference(1)};
  FrameDependenciesCalculator calculator;

  EXPECT_THAT(calculator.FromBuffersUsage(/*frame_id=*/1, pattern0), IsEmpty());
  EXPECT_THAT(calculator.FromBuffersUsage(/*frame_id=*/2, pattern1),
              ElementsAre(1));
  EXPECT_THAT(calculator.FromBuffersUsage(/*frame_id=*/3, pattern2),
              ElementsAre(1));
  EXPECT_THAT(calculator.FromBuffersUsage(/*frame_id=*/4, pattern3),
              UnorderedElementsAre(2, 3));
  EXPECT_THAT(calculator.FromBuffersUsage(/*frame_id=*/5, pattern0),
              ElementsAre(3));
  EXPECT_THAT(calculator.FromBuffersUsage(/*frame_id=*/6, pattern1),
              ElementsAre(5));
  EXPECT_THAT(calculator.FromBuffersUsage(/*frame_id=*/7, pattern2),
              ElementsAre(5));
  EXPECT_THAT(calculator.FromBuffersUsage(/*frame_id=*/8, pattern3),
              UnorderedElementsAre(6, 7));
}

TEST(FrameDependenciesCalculatorTest, ThreeTemporalLayers4FramePattern) {
  // T2:   2---4   6---8 ...
  //      /   /   /   /
  // T1:  |  3    |  7   ...
  //      /_/     /_/
  // T0: 1-------5-----  ...
  CodecBufferUsage pattern0[] = {ReferenceAndUpdate(0)};
  CodecBufferUsage pattern1[] = {Reference(0), Update(2)};
  CodecBufferUsage pattern2[] = {Reference(0), Update(1)};
  CodecBufferUsage pattern3[] = {Reference(0), Reference(1), Reference(2)};
  FrameDependenciesCalculator calculator;

  EXPECT_THAT(calculator.FromBuffersUsage(/*frame_id=*/1, pattern0), IsEmpty());
  EXPECT_THAT(calculator.FromBuffersUsage(/*frame_id=*/2, pattern1),
              ElementsAre(1));
  EXPECT_THAT(calculator.FromBuffersUsage(/*frame_id=*/3, pattern2),
              ElementsAre(1));
  // Note that frame#4 references buffer#0 that is updated by frame#1,
  // yet there is no direct dependency from frame#4 to frame#1.
  EXPECT_THAT(calculator.FromBuffersUsage(/*frame_id=*/4, pattern3),
              UnorderedElementsAre(2, 3));
  EXPECT_THAT(calculator.FromBuffersUsage(/*frame_id=*/5, pattern0),
              ElementsAre(1));
  EXPECT_THAT(calculator.FromBuffersUsage(/*frame_id=*/6, pattern1),
              ElementsAre(5));
}

TEST(FrameDependenciesCalculatorTest, SimulcastWith2Layers) {
  // S1: 2---4---6-  ...
  //
  // S0: 1---3---5-  ...
  CodecBufferUsage pattern0[] = {ReferenceAndUpdate(0)};
  CodecBufferUsage pattern1[] = {ReferenceAndUpdate(1)};
  FrameDependenciesCalculator calculator;

  EXPECT_THAT(calculator.FromBuffersUsage(/*frame_id=*/1, pattern0), IsEmpty());
  EXPECT_THAT(calculator.FromBuffersUsage(/*frame_id=*/2, pattern1), IsEmpty());
  EXPECT_THAT(calculator.FromBuffersUsage(/*frame_id=*/3, pattern0),
              ElementsAre(1));
  EXPECT_THAT(calculator.FromBuffersUsage(/*frame_id=*/4, pattern1),
              ElementsAre(2));
  EXPECT_THAT(calculator.FromBuffersUsage(/*frame_id=*/5, pattern0),
              ElementsAre(3));
  EXPECT_THAT(calculator.FromBuffersUsage(/*frame_id=*/6, pattern1),
              ElementsAre(4));
}

}  // namespace
}  // namespace webrtc
