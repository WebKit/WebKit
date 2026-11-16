/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/utility/cbr_layer_rate_tracker.h"

#include <array>
#include <optional>
#include <vector>

#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/video_codecs/video_encoder_builders.h"
#include "api/video_codecs/video_encoder_interface.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using FrameType = VideoEncoderInterface::FrameType;

constexpr DataRate kStreamBitrate = DataRate::KilobitsPerSec(600);
// The tracker does not look at the duration.
constexpr TimeDelta kFrameDuration = TimeDelta::Millis(33);

struct Frame {
  int spatial_id;
  int temporal_id;
  DataRate bitrate;
  bool is_keyframe;
};

// Feeds the tracker one temporal unit made up of `frames`.
void EncodeTemporalUnit(CbrLayerRateTracker& tracker,
                        const std::vector<Frame>& frames) {
  std::vector<VideoEncoderInterface::FrameEncodeSettings> settings;
  for (const Frame& frame : frames) {
    settings.push_back(FrameEncodeSettingsBuilder()
                           .SpatialId(frame.spatial_id)
                           .TemporalId(frame.temporal_id)
                           .CbrRateOptions(kFrameDuration, frame.bitrate)
                           .FrameType(frame.is_keyframe
                                          ? FrameType::kKeyframe
                                          : FrameType::kDeltaFrame)
                           .Build());
  }
  tracker.OnTemporalUnit(settings);
}

// Feeds the tracker a temporal unit holding a single frame.
void EncodeFrame(CbrLayerRateTracker& tracker,
                 int spatial_id,
                 int temporal_id,
                 DataRate bitrate,
                 bool is_keyframe) {
  EncodeTemporalUnit(tracker,
                     {{spatial_id, temporal_id, bitrate, is_keyframe}});
}

// The bitrate of temporal layer `tid` in a dyadic L1Tx pattern with
// `num_layers` temporal layers, where the per frame bit budget is halved for
// every step up the temporal layer stack and the stream as a whole targets
// `kStreamBitrate`. Matches
// `TemporalLayerPatternForTest::GeometricDistribution` with a ratio of 0.5.
//
// Every layer above the base one ends up with the same share of the bitrate,
// since its frames are twice as many but half as large as those of the layer
// below it. The base layer holds as many frames as the layer just above it,
// each twice the size, so it gets twice the share.
DataRate GeometricLayerBitrate(int tid, int num_layers) {
  return kStreamBitrate * ((tid == 0 ? 2.0 : 1.0) / (num_layers + 1));
}

// Feeds the tracker a keyframe followed by the first delta frame of a dyadic
// L1Tx pattern, which is all the priming needs.
void EncodeKeyframeAndFirstDeltaFrame(CbrLayerRateTracker& tracker,
                                      int num_layers) {
  EncodeFrame(tracker, /*spatial_id=*/0, /*temporal_id=*/0,
              GeometricLayerBitrate(0, num_layers), /*is_keyframe=*/true);
  EncodeFrame(tracker, /*spatial_id=*/0, /*temporal_id=*/num_layers - 1,
              GeometricLayerBitrate(num_layers - 1, num_layers),
              /*is_keyframe=*/false);
}

// A dyadic L1T3 pattern. Its first two frames are the ones
// `EncodeKeyframeAndFirstDeltaFrame` feeds, so a run of the pattern picks up
// at frame two.
constexpr std::array<int, 4> kL1T3Pattern = {0, 2, 1, 2};

int L1T3TemporalId(int frame) {
  return kL1T3Pattern[frame % kL1T3Pattern.size()];
}

// Feeds `num_frames` frames of the L1T3 pattern, starting at `first_frame`,
// with every layer allocated `scale` times its share of `kStreamBitrate`.
void EncodeL1T3Frames(CbrLayerRateTracker& tracker,
                      int first_frame,
                      int num_frames,
                      double scale = 1.0) {
  for (int frame = first_frame; frame < first_frame + num_frames; ++frame) {
    const int temporal_id = L1T3TemporalId(frame);
    EncodeFrame(tracker, /*spatial_id=*/0, temporal_id,
                GeometricLayerBitrate(temporal_id, 3) * scale,
                /*is_keyframe=*/false);
  }
}

// As `kL1T3Pattern`, for four temporal layers.
constexpr std::array<int, 8> kL1T4Pattern = {0, 3, 2, 3, 1, 3, 2, 3};

int L1T4TemporalId(int frame) {
  return kL1T4Pattern[frame % kL1T4Pattern.size()];
}

// The share of `stream_bitrate` that a geometric distribution over
// `num_layers` temporal layers gives to `temporal_id`, and the share it gives
// to all the layers up to and including it.
DataRate LayerShare(DataRate stream_bitrate, int temporal_id, int num_layers) {
  return stream_bitrate * ((temporal_id == 0 ? 2.0 : 1.0) / (num_layers + 1));
}

DataRate CumulativeShare(DataRate stream_bitrate,
                         int temporal_id,
                         int num_layers) {
  return stream_bitrate *
         (static_cast<double>(temporal_id + 2) / (num_layers + 1));
}

TEST(CbrLayerRateTrackerTest, SingleLayerReportsTheFullBitrate) {
  CbrLayerRateTracker tracker;
  EncodeFrame(tracker, /*spatial_id=*/0, /*temporal_id=*/0, kStreamBitrate,
              /*is_keyframe=*/true);
  EncodeFrame(tracker, /*spatial_id=*/0, /*temporal_id=*/0, kStreamBitrate,
              /*is_keyframe=*/false);

  EXPECT_EQ(tracker.allocation().num_temporal_layers, 1);
  EXPECT_EQ(tracker.allocation().framerate_factor[0], 1);
  EXPECT_EQ(tracker.allocation().bitrate[0][0], kStreamBitrate);
  EXPECT_EQ(tracker.allocation().SpatialLayerBitrate(0), kStreamBitrate);
}

TEST(CbrLayerRateTrackerTest, NothingIsKnownBeforeTheFirstFrame) {
  CbrLayerRateTracker tracker;

  EXPECT_EQ(tracker.allocation().num_temporal_layers, 1);
  EXPECT_EQ(tracker.allocation().framerate_factor[0], 1);
  EXPECT_EQ(tracker.allocation().SpatialLayerBitrate(0), DataRate::Zero());
}

TEST(CbrLayerRateTrackerTest, PrimesTwoLayersOnFirstFrameAfterKeyframe) {
  CbrLayerRateTracker tracker;
  EncodeKeyframeAndFirstDeltaFrame(tracker, /*num_layers=*/2);

  EXPECT_EQ(tracker.allocation().num_temporal_layers, 2);
  EXPECT_EQ(tracker.allocation().framerate_factor[0], 2);
  EXPECT_EQ(tracker.allocation().framerate_factor[1], 1);
  // The layers hold half of the frames each, and a base layer frame is twice
  // the size of a T1 frame, so the base layer accounts for two thirds of the
  // bitrate.
  EXPECT_NEAR(tracker.allocation().bitrate[0][0].kbps(), 400, 1);
  EXPECT_NEAR(tracker.allocation().SpatialLayerBitrate(0).kbps(),
              kStreamBitrate.kbps(), 1);
}

TEST(CbrLayerRateTrackerTest, PrimesThreeLayersOnFirstFrameAfterKeyframe) {
  CbrLayerRateTracker tracker;
  EncodeKeyframeAndFirstDeltaFrame(tracker, /*num_layers=*/3);

  EXPECT_EQ(tracker.allocation().num_temporal_layers, 3);
  EXPECT_EQ(tracker.allocation().framerate_factor[0], 4);
  EXPECT_EQ(tracker.allocation().framerate_factor[1], 2);
  EXPECT_EQ(tracker.allocation().framerate_factor[2], 1);
  EXPECT_NEAR(tracker.allocation().bitrate[0][0].kbps(), 300, 1);
  EXPECT_NEAR(tracker.allocation().bitrate[0][1].kbps(), 450, 1);
  EXPECT_NEAR(tracker.allocation().SpatialLayerBitrate(0).kbps(),
              kStreamBitrate.kbps(), 1);
}

TEST(CbrLayerRateTrackerTest, PrimesFourLayersOnFirstFrameAfterKeyframe) {
  CbrLayerRateTracker tracker;
  EncodeKeyframeAndFirstDeltaFrame(tracker, /*num_layers=*/4);

  EXPECT_EQ(tracker.allocation().num_temporal_layers, 4);
  EXPECT_EQ(tracker.allocation().framerate_factor[0], 8);
  EXPECT_EQ(tracker.allocation().framerate_factor[1], 4);
  EXPECT_EQ(tracker.allocation().framerate_factor[2], 2);
  EXPECT_EQ(tracker.allocation().framerate_factor[3], 1);
  EXPECT_NEAR(tracker.allocation().bitrate[0][0].kbps(), 240, 1);
  EXPECT_NEAR(tracker.allocation().bitrate[0][1].kbps(), 360, 1);
  EXPECT_NEAR(tracker.allocation().bitrate[0][2].kbps(), 480, 1);
  EXPECT_NEAR(tracker.allocation().SpatialLayerBitrate(0).kbps(),
              kStreamBitrate.kbps(), 1);
}

TEST(CbrLayerRateTrackerTest, SpatialLayersShareTheTemporalStructure) {
  CbrLayerRateTracker tracker;
  // Two spatial layers, the upper one with twice the bitrate of the lower one,
  // in a dyadic L1T2 pattern.
  for (int frame = 0; frame < 16; ++frame) {
    const int temporal_id = frame % 2;
    const DataRate bitrate = GeometricLayerBitrate(temporal_id, 2);
    EncodeTemporalUnit(
        tracker, {{0, temporal_id, bitrate / 3, /*is_keyframe=*/frame == 0},
                  {1, temporal_id, bitrate * 2 / 3, /*is_keyframe=*/false}});
  }

  EXPECT_EQ(tracker.allocation().num_temporal_layers, 2);
  EXPECT_EQ(tracker.allocation().framerate_factor[0], 2);
  EXPECT_EQ(tracker.allocation().framerate_factor[1], 1);
  EXPECT_NEAR(tracker.allocation().SpatialLayerBitrate(0).kbps(),
              kStreamBitrate.kbps() / 3, 5);
  EXPECT_NEAR(tracker.allocation().SpatialLayerBitrate(1).kbps(),
              2 * kStreamBitrate.kbps() / 3, 5);
}

TEST(CbrLayerRateTrackerTest, ConvergesOnANonGeometricDistribution) {
  CbrLayerRateTracker tracker;
  // A distribution where the per frame bit budget decreases linearly with the
  // temporal id instead of geometrically, so the layers above the base one do
  // not end up with equal shares and the priming does not predict it. With the
  // frames of an L1T3 pattern distributed 1:1:2 over the layers and per frame
  // budgets of 3:2:1, the shares come out as 3:2:2.
  const std::array<DataRate, 3> kLayerBitrates = {
      kStreamBitrate * 3 / 7,
      kStreamBitrate * 2 / 7,
      kStreamBitrate * 2 / 7,
  };
  constexpr std::array<int, 4> kPattern = {0, 2, 1, 2};

  EncodeFrame(tracker, 0, 0, kLayerBitrates[0], /*is_keyframe=*/true);
  for (int frame = 1; frame < 40; ++frame) {
    const int temporal_id = kPattern[frame % 4];
    EncodeFrame(tracker, 0, temporal_id, kLayerBitrates[temporal_id],
                /*is_keyframe=*/false);
  }

  EXPECT_EQ(tracker.allocation().framerate_factor[0], 4);
  EXPECT_EQ(tracker.allocation().framerate_factor[1], 2);
  EXPECT_EQ(tracker.allocation().framerate_factor[2], 1);
  EXPECT_NEAR(tracker.allocation().bitrate[0][0].kbps(),
              kStreamBitrate.kbps() * 3 / 7, 10);
  EXPECT_NEAR(tracker.allocation().bitrate[0][1].kbps(),
              kStreamBitrate.kbps() * 5 / 7, 10);
  EXPECT_NEAR(tracker.allocation().SpatialLayerBitrate(0).kbps(),
              kStreamBitrate.kbps(), 10);
}

TEST(CbrLayerRateTrackerTest, FollowsAChangeOfTheStreamBitrate) {
  CbrLayerRateTracker tracker;
  EncodeKeyframeAndFirstDeltaFrame(tracker, /*num_layers=*/2);

  // The stream bitrate is halved, keeping the same distribution over the
  // temporal layers. Redistributing the same shape over a new bitrate is
  // picked up as soon as every layer has been seen once.
  for (int frame = 0; frame < 4; ++frame) {
    const int temporal_id = frame % 2;
    EncodeFrame(tracker, 0, temporal_id,
                GeometricLayerBitrate(temporal_id, 2) / 2,
                /*is_keyframe=*/false);
  }

  EXPECT_NEAR(tracker.allocation().SpatialLayerBitrate(0).kbps(),
              kStreamBitrate.kbps() / 2, 5);
  EXPECT_NEAR(tracker.allocation().bitrate[0][0].kbps(), 200, 5);
}

TEST(CbrLayerRateTrackerTest, SingleLayerFollowsTheBitrateImmediately) {
  CbrLayerRateTracker tracker;
  EncodeFrame(tracker, /*spatial_id=*/0, /*temporal_id=*/0, kStreamBitrate,
              /*is_keyframe=*/true);

  // With a single temporal layer every frame states the bitrate of the whole
  // stream, so there is nothing to average over and no reason to lag behind.
  const DataRate kLowerBitrate = kStreamBitrate / 5;
  EncodeFrame(tracker, /*spatial_id=*/0, /*temporal_id=*/0, kLowerBitrate,
              /*is_keyframe=*/false);

  EXPECT_EQ(tracker.allocation().SpatialLayerBitrate(0), kLowerBitrate);
}

TEST(CbrLayerRateTrackerTest, ConvergesOnAShiftedKeyPattern) {
  // In L2T2_KEY_SHIFT the spatial layers run antiphase: within a temporal unit
  // they are on different temporal layers, and the base layer of the lower
  // spatial layer holds two frames in a row right after the keyframe. The
  // frame after the keyframe is therefore not on the topmost layer and the
  // priming does not kick in, leaving the cadence to be learned.
  //
  // t=0: S0T0, S1T0    t=1: S0T0, S1T1    t=2: S0T1, S1T0    ...
  CbrLayerRateTracker tracker;
  const DataRate kLowerBitrate = kStreamBitrate / 3;
  const DataRate kUpperBitrate = kStreamBitrate * 2 / 3;
  EncodeTemporalUnit(tracker,
                     {{0, 0, kLowerBitrate * 2 / 3, /*is_keyframe=*/true},
                      {1, 0, kUpperBitrate * 2 / 3, /*is_keyframe=*/false}});

  for (int temporal_unit = 1; temporal_unit <= 16; ++temporal_unit) {
    const int lower_temporal_id = temporal_unit % 2 == 1 ? 0 : 1;
    const int upper_temporal_id = 1 - lower_temporal_id;
    EncodeTemporalUnit(
        tracker, {{0, lower_temporal_id,
                   kLowerBitrate * (lower_temporal_id == 0 ? 2.0 / 3 : 1.0 / 3),
                   /*is_keyframe=*/false},
                  {1, upper_temporal_id,
                   kUpperBitrate * (upper_temporal_id == 0 ? 2.0 / 3 : 1.0 / 3),
                   /*is_keyframe=*/false}});

    // Both layers have been seen twice after four temporal units, which is all
    // it takes to measure how often their frames occur.
    if (temporal_unit >= 4) {
      EXPECT_EQ(tracker.allocation().framerate_factor[0], 2)
          << " at temporal unit " << temporal_unit;
    }
  }

  EXPECT_EQ(tracker.allocation().num_temporal_layers, 2);
  EXPECT_EQ(tracker.allocation().framerate_factor[1], 1);
  EXPECT_NEAR(tracker.allocation().SpatialLayerBitrate(0).kbps(),
              kLowerBitrate.kbps(), 5);
  EXPECT_NEAR(tracker.allocation().SpatialLayerBitrate(1).kbps(),
              kUpperBitrate.kbps(), 5);
}

TEST(CbrLayerRateTrackerTest, ConvergesOnANonDyadicCadence) {
  // A pattern where only every third frame belongs to the base layer. The
  // priming assumes a dyadic pattern, in which the base layer holds every
  // other frame, so the cadence has to be corrected from there.
  CbrLayerRateTracker tracker;
  // The base layer frames are twice the size of the ones above them but only
  // half as many, so the two layers end up with the same bitrate.
  const DataRate kLayerBitrate = kStreamBitrate / 2;

  EncodeFrame(tracker, /*spatial_id=*/0, /*temporal_id=*/0, kLayerBitrate,
              /*is_keyframe=*/true);
  for (int frame = 1; frame <= 40; ++frame) {
    EncodeFrame(tracker, /*spatial_id=*/0,
                /*temporal_id=*/frame % 3 == 0 ? 0 : 1, kLayerBitrate,
                /*is_keyframe=*/false);

    // Measured: the cadence estimate settles on the correct value by the
    // twentieth frame, less than a second of video, and stays there. The
    // frames of the upper layer come in pairs, so the interval between them
    // alternates between one and two and the estimate has to average the two
    // out before it can be trusted to the nearest integer.
    if (frame >= 20) {
      EXPECT_EQ(tracker.allocation().framerate_factor[0], 3)
          << " at frame " << frame;
    }
  }

  EXPECT_EQ(tracker.allocation().num_temporal_layers, 2);
  EXPECT_EQ(tracker.allocation().framerate_factor[1], 1);
  EXPECT_NEAR(tracker.allocation().bitrate[0][0].kbps(), kLayerBitrate.kbps(),
              5);
  EXPECT_NEAR(tracker.allocation().SpatialLayerBitrate(0).kbps(),
              kStreamBitrate.kbps(), 5);
}

// The layer the caller happens to state a new allocation on first must not
// matter: the change is carried over to the layers that have not reported yet,
// so the cumulative bitrate is right for the very next frame either way. The
// three tests below place the change on each of the layers in turn.
TEST(CbrLayerRateTrackerTest, CarriesAChangeSeenOnTheBaseLayerOver) {
  CbrLayerRateTracker tracker;
  EncodeKeyframeAndFirstDeltaFrame(tracker, /*num_layers=*/3);
  // Run the pattern until every layer has stated a bitrate of its own. The
  // frame that follows belongs to the base layer.
  EncodeL1T3Frames(tracker, /*first_frame=*/2, /*num_frames=*/6);
  ASSERT_EQ(L1T3TemporalId(8), 0);

  EncodeL1T3Frames(tracker, /*first_frame=*/8, /*num_frames=*/1, /*scale=*/0.5);

  EXPECT_EQ(tracker.allocation().bitrate[0][0],
            GeometricLayerBitrate(0, 3) / 2);
  EXPECT_EQ(tracker.allocation().SpatialLayerBitrate(0), kStreamBitrate / 2);
}

TEST(CbrLayerRateTrackerTest, CarriesAChangeSeenOnAMiddleLayerOver) {
  CbrLayerRateTracker tracker;
  EncodeKeyframeAndFirstDeltaFrame(tracker, /*num_layers=*/3);
  EncodeL1T3Frames(tracker, /*first_frame=*/2, /*num_frames=*/4);
  ASSERT_EQ(L1T3TemporalId(6), 1);

  EncodeL1T3Frames(tracker, /*first_frame=*/6, /*num_frames=*/1, /*scale=*/0.5);

  EXPECT_EQ(tracker.allocation().bitrate[0][0],
            GeometricLayerBitrate(0, 3) / 2);
  EXPECT_EQ(tracker.allocation().SpatialLayerBitrate(0), kStreamBitrate / 2);
}

TEST(CbrLayerRateTrackerTest, CarriesAChangeSeenOnTheTopLayerOver) {
  CbrLayerRateTracker tracker;
  EncodeKeyframeAndFirstDeltaFrame(tracker, /*num_layers=*/3);
  EncodeL1T3Frames(tracker, /*first_frame=*/2, /*num_frames=*/5);
  ASSERT_EQ(L1T3TemporalId(7), 2);

  EncodeL1T3Frames(tracker, /*first_frame=*/7, /*num_frames=*/1, /*scale=*/0.5);

  EXPECT_EQ(tracker.allocation().bitrate[0][0],
            GeometricLayerBitrate(0, 3) / 2);
  EXPECT_EQ(tracker.allocation().SpatialLayerBitrate(0), kStreamBitrate / 2);
}

TEST(CbrLayerRateTrackerTest, DoesNotCountACarriedOverChangeTwice) {
  CbrLayerRateTracker tracker;
  EncodeKeyframeAndFirstDeltaFrame(tracker, /*num_layers=*/3);
  EncodeL1T3Frames(tracker, /*first_frame=*/2, /*num_frames=*/4);

  // The allocation is halved, and halved again before the layers that were
  // only assumed to follow along have stated anything themselves. Each of them
  // then restates what was already assumed, which must leave the estimates
  // where they are.
  EncodeL1T3Frames(tracker, /*first_frame=*/6, /*num_frames=*/1, /*scale=*/0.5);
  EncodeL1T3Frames(tracker, /*first_frame=*/7, /*num_frames=*/1,
                   /*scale=*/0.25);
  EXPECT_EQ(tracker.allocation().SpatialLayerBitrate(0), kStreamBitrate / 4);

  EncodeL1T3Frames(tracker, /*first_frame=*/8, /*num_frames=*/4,
                   /*scale=*/0.25);
  EXPECT_EQ(tracker.allocation().bitrate[0][0],
            GeometricLayerBitrate(0, 3) / 4);
  EXPECT_EQ(tracker.allocation().SpatialLayerBitrate(0), kStreamBitrate / 4);
}

TEST(CbrLayerRateTrackerTest, SettlesOnANewDistribution) {
  CbrLayerRateTracker tracker;
  EncodeKeyframeAndFirstDeltaFrame(tracker, /*num_layers=*/3);
  EncodeL1T3Frames(tracker, /*first_frame=*/2, /*num_frames=*/6);

  // The stream bitrate is kept, but distributed 3:2:2 rather than 2:1:1. The
  // first layer to state its new share is taken to speak for the others, which
  // is wrong here, so a layer that was carried along is only put right the
  // next time it states a bitrate of its own.
  const std::array<DataRate, 3> kNewBitrates = {
      kStreamBitrate * 3 / 7,
      kStreamBitrate * 2 / 7,
      kStreamBitrate * 2 / 7,
  };
  // Splitting the stream bitrate in sevenths does not come out even, so the
  // layers are what the total is held against.
  const DataRate kNewStreamBitrate =
      kNewBitrates[0] + kNewBitrates[1] + kNewBitrates[2];
  for (int frame = 8; frame < 16; ++frame) {
    const int temporal_id = L1T3TemporalId(frame);
    EncodeFrame(tracker, 0, temporal_id, kNewBitrates[temporal_id],
                /*is_keyframe=*/false);

    // The base layer, the last to be heard from a second time, settles on the
    // frame that follows a full pattern.
    if (frame >= 12) {
      EXPECT_EQ(tracker.allocation().bitrate[0][0], kNewBitrates[0])
          << " at frame " << frame;
      EXPECT_EQ(tracker.allocation().SpatialLayerBitrate(0), kNewStreamBitrate)
          << " at frame " << frame;
    }
  }

  EXPECT_EQ(tracker.allocation().bitrate[0][1],
            kNewBitrates[0] + kNewBitrates[1]);
}

TEST(CbrLayerRateTrackerTest, DoesNotReadARepeatedBitrateAsAChange) {
  CbrLayerRateTracker tracker;
  EncodeKeyframeAndFirstDeltaFrame(tracker, /*num_layers=*/3);
  EncodeL1T3Frames(tracker, /*first_frame=*/2, /*num_frames=*/6);

  // Only the base layer is given more, which the tracker cannot tell from the
  // start of a change of the whole allocation. The layers above it then keep
  // restating what they had, and a restatement says nothing, so the estimates
  // must settle rather than swing back and forth between the two readings.
  const DataRate kNewBaseBitrate = GeometricLayerBitrate(0, 3) * 1.5;
  const DataRate kNewStreamBitrate = kNewBaseBitrate +
                                     GeometricLayerBitrate(1, 3) +
                                     GeometricLayerBitrate(2, 3);
  for (int frame = 8; frame < 40; ++frame) {
    const int temporal_id = L1T3TemporalId(frame);
    EncodeFrame(tracker, 0, temporal_id,
                temporal_id == 0 ? kNewBaseBitrate
                                 : GeometricLayerBitrate(temporal_id, 3),
                /*is_keyframe=*/false);

    // One pattern is enough for every layer to have been heard from.
    if (frame >= 12) {
      EXPECT_EQ(tracker.allocation().bitrate[0][0], kNewBaseBitrate)
          << " at frame " << frame;
      EXPECT_EQ(tracker.allocation().SpatialLayerBitrate(0), kNewStreamBitrate)
          << " at frame " << frame;
    }
  }
}

TEST(CbrLayerRateTrackerTest, DoesNotReadADepartureFromAGuessAsAChange) {
  CbrLayerRateTracker tracker;
  // The priming guesses what the layers above the base one hold. The first
  // frame of such a layer states the real value, which is not a change of the
  // allocation however far off the guess was, so the other layers must be left
  // alone.
  EncodeKeyframeAndFirstDeltaFrame(tracker, /*num_layers=*/3);
  const DataRate kMiddleLayerBitrate = GeometricLayerBitrate(1, 3) * 4;

  EncodeFrame(tracker, /*spatial_id=*/0, /*temporal_id=*/1, kMiddleLayerBitrate,
              /*is_keyframe=*/false);

  EXPECT_EQ(tracker.allocation().bitrate[0][0], GeometricLayerBitrate(0, 3));
  EXPECT_EQ(tracker.allocation().bitrate[0][1],
            GeometricLayerBitrate(0, 3) + kMiddleLayerBitrate);
}

TEST(CbrLayerRateTrackerTest, KeepsTheSpatialLayersApart) {
  // Two spatial layers in an L1T2 pattern, the lower one at 300 kbps and the
  // upper one at 600 kbps, both split 2:1 between their temporal layers.
  constexpr DataRate kLowerStreamBitrate = DataRate::KilobitsPerSec(300);
  constexpr DataRate kUpperStreamBitrate = DataRate::KilobitsPerSec(600);
  CbrLayerRateTracker tracker;
  for (int frame = 0; frame < 4; ++frame) {
    const double share = frame % 2 == 0 ? 2.0 / 3 : 1.0 / 3;
    EncodeTemporalUnit(tracker, {{0, frame % 2, kLowerStreamBitrate * share,
                                  /*is_keyframe=*/frame == 0},
                                 {1, frame % 2, kUpperStreamBitrate * share,
                                  /*is_keyframe=*/false}});
  }

  // Halving the lower spatial layer says nothing about the upper one, which
  // is left out of this temporal unit.
  EncodeFrame(tracker, /*spatial_id=*/0, /*temporal_id=*/0,
              kLowerStreamBitrate * (2.0 / 3) / 2, /*is_keyframe=*/false);

  EXPECT_EQ(tracker.allocation().SpatialLayerBitrate(0),
            kLowerStreamBitrate / 2);
  EXPECT_EQ(tracker.allocation().SpatialLayerBitrate(1), kUpperStreamBitrate);
}

// The two simulations below drive the tracker the way a congestion controller
// would drive a stream with temporal layers, which the encoder level tests do
// not cover: those run a single temporal layer, where the bitrate of the layer
// and of the stream are the same thing.
TEST(CbrLayerRateTrackerTest, FollowsAStagedBitrateSweep) {
  // The bitrate profile of the encoder level ChangingBitrateTargetVga test,
  // over an L1T3 pattern at 30 fps.
  constexpr int kNumLayers = 3;
  CbrLayerRateTracker tracker;
  int frame = 0;

  auto encode = [&](DataRate stream_bitrate, int num_frames) {
    for (int i = 0; i < num_frames; ++i, ++frame) {
      const int temporal_id = L1T3TemporalId(frame);
      EncodeFrame(tracker, /*spatial_id=*/0, temporal_id,
                  LayerShare(stream_bitrate, temporal_id, kNumLayers),
                  /*is_keyframe=*/frame == 0);

      // Every step of the sweep scales the whole allocation, so the frame at
      // hand states everything there is to know about the change and the
      // bitrate it is encoded against is right away.
      EXPECT_EQ(tracker.allocation().bitrate[0][temporal_id],
                CumulativeShare(stream_bitrate, temporal_id, kNumLayers))
          << " at frame " << frame << " of T" << temporal_id;
    }
  };

  encode(DataRate::KilobitsPerSec(500), 60);
  encode(DataRate::KilobitsPerSec(100), 30);
  for (int kbps = 150; kbps < 500; kbps += 50) {
    encode(DataRate::KilobitsPerSec(kbps), 6);
  }
  encode(DataRate::KilobitsPerSec(500), 30);

  EXPECT_EQ(tracker.allocation().SpatialLayerBitrate(0),
            DataRate::KilobitsPerSec(500));
}

TEST(CbrLayerRateTrackerTest, RecoversOnceTheTargetHoldsStill) {
  // A target that moves on every single frame, which is more than a congestion
  // controller would ask for, denies the tracker the one thing that tells a
  // change of the whole allocation from a change of the distribution: a layer
  // restating the bitrate it already had. Whatever the stream does while a
  // layer waits for its first turn is then never accounted for, and the skew
  // that leaves behind, measured at up to 10% of the cumulative bitrate,
  // stays until the target settles.
  constexpr int kNumLayers = 4;
  CbrLayerRateTracker tracker;
  auto stream_bitrate_at = [](int frame) {
    // A sawtooth between 300 and 900 kbps in steps of 30, which divides evenly
    // into fifths and keeps the layer bitrates whole.
    return DataRate::KilobitsPerSec(300 + 30 * (frame % 21));
  };

  EncodeFrame(tracker, /*spatial_id=*/0, /*temporal_id=*/0,
              LayerShare(stream_bitrate_at(0), 0, kNumLayers),
              /*is_keyframe=*/true);
  EncodeFrame(tracker, /*spatial_id=*/0, /*temporal_id=*/kNumLayers - 1,
              LayerShare(stream_bitrate_at(1), kNumLayers - 1, kNumLayers),
              /*is_keyframe=*/false);
  for (int frame = 2; frame < 40; ++frame) {
    const int temporal_id = L1T4TemporalId(frame);
    EncodeFrame(tracker,
                /*spatial_id=*/0, temporal_id,
                LayerShare(stream_bitrate_at(frame), temporal_id, kNumLayers),
                /*is_keyframe=*/false);
  }

  // A layer restating what it holds is what puts the estimates right, so they
  // are all in order again once the pattern has come around.
  constexpr DataRate kHeldBitrate = DataRate::KilobitsPerSec(600);
  for (int frame = 40; frame < 80; ++frame) {
    const int temporal_id = L1T4TemporalId(frame);
    EncodeFrame(tracker, /*spatial_id=*/0, temporal_id,
                LayerShare(kHeldBitrate, temporal_id, kNumLayers),
                /*is_keyframe=*/false);

    if (frame >= 40 + 1 * static_cast<int>(kL1T4Pattern.size())) {
      EXPECT_EQ(tracker.allocation().bitrate[0][temporal_id],
                CumulativeShare(kHeldBitrate, temporal_id, kNumLayers))
          << " at frame " << frame << " of T" << temporal_id;
    }
  }
}

TEST(CbrLayerRateTrackerTest, PrimesAllSpatialLayersOnFirstUnitAfterKeyframe) {
  // L2T3: the temporal unit after the keyframe is on the topmost temporal
  // layer in both spatial layers, so both are primed.
  CbrLayerRateTracker tracker;
  EncodeTemporalUnit(tracker, {{0, 0, GeometricLayerBitrate(0, 3) / 3,
                                /*is_keyframe=*/true},
                               {1, 0, GeometricLayerBitrate(0, 3) * 2 / 3,
                                /*is_keyframe=*/false}});
  EncodeTemporalUnit(tracker, {{0, 2, GeometricLayerBitrate(2, 3) / 3,
                                /*is_keyframe=*/false},
                               {1, 2, GeometricLayerBitrate(2, 3) * 2 / 3,
                                /*is_keyframe=*/false}});

  const CumulativeCbrAllocation& allocation = tracker.allocation();
  EXPECT_EQ(allocation.num_temporal_layers, 3);
  EXPECT_EQ(allocation.framerate_factor[0], 4);
  EXPECT_EQ(allocation.framerate_factor[1], 2);
  EXPECT_EQ(allocation.framerate_factor[2], 1);
  EXPECT_NEAR(allocation.bitrate[0][0].kbps(), 100, 1);
  EXPECT_NEAR(allocation.bitrate[0][1].kbps(), 150, 1);
  EXPECT_NEAR(allocation.SpatialLayerBitrate(0).kbps(), 200, 1);
  EXPECT_NEAR(allocation.bitrate[1][0].kbps(), 200, 1);
  EXPECT_NEAR(allocation.bitrate[1][1].kbps(), 300, 1);
  EXPECT_NEAR(allocation.SpatialLayerBitrate(1).kbps(), 400, 1);
  EXPECT_NEAR(allocation.TotalBitrate().kbps(), kStreamBitrate.kbps(), 2);
}

TEST(CbrLayerRateTrackerTest, AbsentSpatialLayersKeepTheirBitrate) {
  // Three spatial layers where a spatial layer being present means that all
  // layers above it are present too, and the lowest present one follows the
  // dyadic cadence {0, 2, 1, 2}. All frames are on the base temporal layer, so
  // the spatial layers simply run at different frame rates.
  constexpr std::array<int, 4> kLowestLayer = {0, 2, 1, 2};
  const std::array<DataRate, 3> kLayerBitrates = {
      DataRate::KilobitsPerSec(50), DataRate::KilobitsPerSec(100),
      DataRate::KilobitsPerSec(150)};
  CbrLayerRateTracker tracker;

  std::optional<CumulativeCbrAllocation> previous;
  for (int temporal_unit = 0; temporal_unit < 16; ++temporal_unit) {
    std::vector<Frame> frames;
    for (int sid = kLowestLayer[temporal_unit % 4]; sid < 3; ++sid) {
      frames.push_back({sid, 0, kLayerBitrates[sid],
                        /*is_keyframe=*/temporal_unit == 0 && sid == 0});
    }
    EncodeTemporalUnit(tracker, frames);

    // Once every layer has been heard from, the allocation must not move
    // whichever layers the temporal unit holds, or encoders that reconfigure
    // on every change would do so for nothing.
    if (temporal_unit >= 1) {
      ASSERT_TRUE(previous.has_value());
      EXPECT_EQ(tracker.allocation(), *previous)
          << " at temporal unit " << temporal_unit;
    }
    previous = tracker.allocation();
  }

  const CumulativeCbrAllocation& allocation = tracker.allocation();
  EXPECT_EQ(allocation.num_temporal_layers, 1);
  for (int sid = 0; sid < 3; ++sid) {
    EXPECT_EQ(allocation.SpatialLayerBitrate(sid), kLayerBitrates[sid]);
    for (int tid = 0; tid < CumulativeCbrAllocation::kMaxTemporalLayers;
         ++tid) {
      EXPECT_EQ(allocation.bitrate[sid][tid], kLayerBitrates[sid]);
    }
  }
  EXPECT_EQ(allocation.TotalBitrate(), DataRate::KilobitsPerSec(300));
}

TEST(CbrLayerRateTrackerTest, IgnoresFramesWithoutCbr) {
  CbrLayerRateTracker tracker;
  std::vector<VideoEncoderInterface::FrameEncodeSettings> settings;
  settings.push_back(FrameEncodeSettingsBuilder()
                         .SpatialId(0)
                         .TemporalId(0)
                         .CqpRateOptions(/*target_qp=*/30)
                         .FrameType(FrameType::kKeyframe)
                         .Build());
  tracker.OnTemporalUnit(settings);

  EXPECT_EQ(tracker.allocation(), CumulativeCbrAllocation());
}

}  // namespace
}  // namespace webrtc
