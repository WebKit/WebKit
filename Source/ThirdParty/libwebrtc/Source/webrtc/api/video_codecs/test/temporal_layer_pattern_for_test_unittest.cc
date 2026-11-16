/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/test/temporal_layer_pattern_for_test.h"

#include <optional>
#include <vector>

#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::DoubleEq;
using ::testing::DoubleNear;
using ::testing::ElementsAre;
using ::testing::Eq;

constexpr int kManyBuffers = 8;

std::vector<TemporalLayerPatternForTest::FrameConfig> GenerateFrames(
    TemporalLayerPatternForTest& pattern,
    int num_frames) {
  std::vector<TemporalLayerPatternForTest::FrameConfig> frames;
  frames.reserve(num_frames);
  for (int i = 0; i < num_frames; ++i) {
    frames.push_back(pattern.NextFrameConfig());
  }
  return frames;
}

std::vector<int> TemporalIds(
    const std::vector<TemporalLayerPatternForTest::FrameConfig>& frames) {
  std::vector<int> temporal_ids;
  for (const TemporalLayerPatternForTest::FrameConfig& frame : frames) {
    temporal_ids.push_back(frame.temporal_id);
  }
  return temporal_ids;
}

TEST(TemporalLayerPatternTest, SingleLayerFormsChainOnBaseBuffer) {
  TemporalLayerPatternForTest pattern(
      /*num_temporal_layers=*/1, kManyBuffers,
      TemporalLayerPatternForTest::GeometricDistribution(1, 0.5));

  std::vector<TemporalLayerPatternForTest::FrameConfig> frames =
      GenerateFrames(pattern, 4);

  EXPECT_THAT(TemporalIds(frames), ElementsAre(0, 0, 0, 0));
  EXPECT_EQ(frames[0].reference_buffer, std::nullopt);
  for (const TemporalLayerPatternForTest::FrameConfig& frame : frames) {
    EXPECT_EQ(frame.update_buffer, 0);
    EXPECT_THAT(frame.rate_factor, DoubleEq(1.0));
  }
  EXPECT_EQ(frames[1].reference_buffer, 0);
  EXPECT_EQ(frames[2].reference_buffer, 0);
  EXPECT_EQ(frames[3].reference_buffer, 0);
}

TEST(TemporalLayerPatternTest, ThreeLayersRepeatDyadicPattern) {
  TemporalLayerPatternForTest pattern(
      /*num_temporal_layers=*/3, kManyBuffers,
      TemporalLayerPatternForTest::GeometricDistribution(3, 0.5));

  EXPECT_THAT(TemporalIds(GenerateFrames(pattern, 8)),
              ElementsAre(0, 2, 1, 2, 0, 2, 1, 2));
}

TEST(TemporalLayerPatternTest, ThreeLayersReferenceClosestLowerLayerFrame) {
  TemporalLayerPatternForTest pattern(
      /*num_temporal_layers=*/3, kManyBuffers,
      TemporalLayerPatternForTest::GeometricDistribution(3, 0.5));

  std::vector<TemporalLayerPatternForTest::FrameConfig> frames =
      GenerateFrames(pattern, 8);

  // The base layer forms a chain in buffer 0 and the middle layer stores its
  // frames in buffer 1. The topmost layer is never referenced and therefore
  // updates no buffer.
  EXPECT_THAT(frames[0].reference_buffer, Eq(std::nullopt));
  EXPECT_THAT(frames[0].update_buffer, Eq(0));  // T0
  EXPECT_THAT(frames[1].reference_buffer, Eq(0));
  EXPECT_THAT(frames[1].update_buffer, Eq(std::nullopt));  // T2
  EXPECT_THAT(frames[2].reference_buffer, Eq(0));
  EXPECT_THAT(frames[2].update_buffer, Eq(1));  // T1
  EXPECT_THAT(frames[3].reference_buffer, Eq(1));
  EXPECT_THAT(frames[3].update_buffer, Eq(std::nullopt));  // T2
  EXPECT_THAT(frames[4].reference_buffer, Eq(0));
  EXPECT_THAT(frames[4].update_buffer, Eq(0));  // T0
  EXPECT_THAT(frames[5].reference_buffer, Eq(0));
  EXPECT_THAT(frames[6].reference_buffer, Eq(0));
  EXPECT_THAT(frames[6].update_buffer, Eq(1));  // T1
  EXPECT_THAT(frames[7].reference_buffer, Eq(1));
}

TEST(TemporalLayerPatternTest, HighLayersFallBackToLastStoredFrame) {
  // Four layers would need three buffers, only two are available so the frames
  // of layer two are no longer stored.
  TemporalLayerPatternForTest pattern(
      /*num_temporal_layers=*/4, /*num_reference_buffers=*/2,
      TemporalLayerPatternForTest::GeometricDistribution(4, 0.5));

  std::vector<TemporalLayerPatternForTest::FrameConfig> frames =
      GenerateFrames(pattern, 9);

  EXPECT_THAT(TemporalIds(frames), ElementsAre(0, 3, 2, 3, 1, 3, 2, 3, 0));
  for (const TemporalLayerPatternForTest::FrameConfig& frame : frames) {
    if (frame.update_buffer.has_value()) {
      EXPECT_LT(*frame.update_buffer, 2);
    }
    if (frame.reference_buffer.has_value()) {
      EXPECT_LT(*frame.reference_buffer, 2);
    }
  }

  // Only the two lowest layers are stored.
  EXPECT_THAT(frames[0].update_buffer, Eq(0));             // T0
  EXPECT_THAT(frames[4].update_buffer, Eq(1));             // T1
  EXPECT_THAT(frames[2].update_buffer, Eq(std::nullopt));  // T2
  EXPECT_THAT(frames[1].update_buffer, Eq(std::nullopt));  // T3

  // Frames of the unstored layers reference the most recently stored frame
  // rather than the closest preceding frame of a lower layer.
  EXPECT_THAT(frames[1].reference_buffer, Eq(0));  // T3, after T0.
  EXPECT_THAT(frames[2].reference_buffer, Eq(0));  // T2, after T0.
  EXPECT_THAT(frames[3].reference_buffer, Eq(0));  // T3, T2 was not stored.
  EXPECT_THAT(frames[5].reference_buffer, Eq(1));  // T3, after T1.
  EXPECT_THAT(frames[6].reference_buffer, Eq(1));  // T2, after T1.
  EXPECT_THAT(frames[7].reference_buffer, Eq(1));  // T3, T2 was not stored.
  EXPECT_THAT(frames[8].reference_buffer, Eq(0));  // T0 chain is unaffected.
}

TEST(TemporalLayerPatternTest, SingleBufferMakesAllLayersReferenceBaseLayer) {
  TemporalLayerPatternForTest pattern(
      /*num_temporal_layers=*/3, /*num_reference_buffers=*/1,
      TemporalLayerPatternForTest::GeometricDistribution(3, 0.5));

  std::vector<TemporalLayerPatternForTest::FrameConfig> frames =
      GenerateFrames(pattern, 4);

  EXPECT_THAT(frames[0].update_buffer, Eq(0));
  for (const TemporalLayerPatternForTest::FrameConfig& frame : frames) {
    EXPECT_THAT(frame.reference_buffer.value_or(0), Eq(0));
  }
  EXPECT_THAT(frames[1].update_buffer, Eq(std::nullopt));
  EXPECT_THAT(frames[2].update_buffer, Eq(std::nullopt));
  EXPECT_THAT(frames[3].update_buffer, Eq(std::nullopt));
}

TEST(TemporalLayerPatternTest, GeometricDistributionScalesPerFrameBudget) {
  // With a ratio of one half each layer gets half the per frame budget of the
  // layer below it.
  TemporalLayerPatternForTest pattern(
      /*num_temporal_layers=*/3, kManyBuffers,
      TemporalLayerPatternForTest::GeometricDistribution(3, 0.5));

  EXPECT_THAT(pattern.frame_budget_factor(0), DoubleEq(2.0));
  EXPECT_THAT(pattern.frame_budget_factor(1), DoubleEq(1.0));
  EXPECT_THAT(pattern.frame_budget_factor(2), DoubleEq(0.5));

  // The layers hold 1, 1 and 2 of the four frames of a group of pictures, so
  // the base layer ends up with half of the bitrate and the other two with a
  // quarter each.
  EXPECT_THAT(pattern.rate_factor(0), DoubleEq(0.5));
  EXPECT_THAT(pattern.rate_factor(1), DoubleEq(0.25));
  EXPECT_THAT(pattern.rate_factor(2), DoubleEq(0.25));
}

TEST(TemporalLayerPatternTest, GeometricDistributionWithRatioOneIsFlat) {
  TemporalLayerPatternForTest pattern(
      /*num_temporal_layers=*/4, kManyBuffers,
      TemporalLayerPatternForTest::GeometricDistribution(4, 1.0));

  // Every frame gets the same budget, so a layer's share of the bitrate is
  // just its share of the frames.
  for (int tid = 0; tid < 4; ++tid) {
    EXPECT_THAT(pattern.frame_budget_factor(tid), DoubleEq(1.0)) << tid;
  }
  EXPECT_THAT(pattern.rate_factor(0), DoubleEq(0.125));
  EXPECT_THAT(pattern.rate_factor(1), DoubleEq(0.125));
  EXPECT_THAT(pattern.rate_factor(2), DoubleEq(0.25));
  EXPECT_THAT(pattern.rate_factor(3), DoubleEq(0.5));
}

TEST(TemporalLayerPatternTest, LinearDistributionScalesPerFrameBudget) {
  TemporalLayerPatternForTest pattern(
      /*num_temporal_layers=*/3, kManyBuffers,
      TemporalLayerPatternForTest::LinearDistribution(3));

  // Per frame budgets of 3, 2 and 1 units, normalized over the seven units of
  // a group of pictures and scaled by its four frames.
  EXPECT_THAT(pattern.frame_budget_factor(0), DoubleNear(12.0 / 7, 1e-9));
  EXPECT_THAT(pattern.frame_budget_factor(1), DoubleNear(8.0 / 7, 1e-9));
  EXPECT_THAT(pattern.frame_budget_factor(2), DoubleNear(4.0 / 7, 1e-9));

  EXPECT_THAT(pattern.rate_factor(0), DoubleNear(3.0 / 7, 1e-9));
  EXPECT_THAT(pattern.rate_factor(1), DoubleNear(2.0 / 7, 1e-9));
  EXPECT_THAT(pattern.rate_factor(2), DoubleNear(2.0 / 7, 1e-9));
}

TEST(TemporalLayerPatternTest, RateFactorsPreserveNominalBitrate) {
  for (int num_temporal_layers = 1; num_temporal_layers <= 5;
       ++num_temporal_layers) {
    for (double ratio : {1.0, 0.7, 0.5}) {
      TemporalLayerPatternForTest geometric(
          num_temporal_layers, kManyBuffers,
          TemporalLayerPatternForTest::GeometricDistribution(
              num_temporal_layers, ratio));
      TemporalLayerPatternForTest linear(
          num_temporal_layers, kManyBuffers,
          TemporalLayerPatternForTest::LinearDistribution(num_temporal_layers));

      const int num_frames = 4 * (1 << (num_temporal_layers - 1));
      for (TemporalLayerPatternForTest* pattern : {&geometric, &linear}) {
        // The layers split the stream bitrate between them.
        double total_rate_factor = 0.0;
        for (int tid = 0; tid < num_temporal_layers; ++tid) {
          total_rate_factor += pattern->rate_factor(tid);
        }
        EXPECT_THAT(total_rate_factor, DoubleNear(1.0, 1e-9))
            << "num_temporal_layers=" << num_temporal_layers
            << " ratio=" << ratio;

        // Since all frames share the nominal duration, the bits handed out
        // over a whole number of groups of pictures must add up to what the
        // nominal bitrate allows over the same number of frames.
        double total_budget_factor = 0.0;
        for (const TemporalLayerPatternForTest::FrameConfig& frame :
             GenerateFrames(*pattern, num_frames)) {
          total_budget_factor +=
              pattern->frame_budget_factor(frame.temporal_id);
        }
        EXPECT_THAT(total_budget_factor, DoubleNear(num_frames, 1e-9))
            << "num_temporal_layers=" << num_temporal_layers
            << " ratio=" << ratio;
      }
    }
  }
}

}  // namespace
}  // namespace webrtc
