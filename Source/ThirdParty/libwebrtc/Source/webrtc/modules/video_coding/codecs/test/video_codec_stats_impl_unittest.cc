/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/video_codec_stats_impl.h"

#include <tuple>

#include "absl/types/optional.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {

namespace {
using ::testing::Return;
using ::testing::Values;
using Filter = VideoCodecStats::Filter;
using Frame = VideoCodecStatsImpl::Frame;
using Stream = VideoCodecStats::Stream;
}  // namespace

TEST(VideoCodecStatsImpl, AddAndGetFrame) {
  VideoCodecStatsImpl stats;
  stats.AddFrame({.timestamp_rtp = 0, .spatial_idx = 0});
  stats.AddFrame({.timestamp_rtp = 0, .spatial_idx = 1});
  stats.AddFrame({.timestamp_rtp = 1, .spatial_idx = 0});

  Frame* fs = stats.GetFrame(/*timestamp_rtp=*/0, /*spatial_idx=*/0);
  ASSERT_NE(fs, nullptr);
  EXPECT_EQ(fs->timestamp_rtp, 0u);
  EXPECT_EQ(fs->spatial_idx, 0);

  fs = stats.GetFrame(/*timestamp_rtp=*/0, /*spatial_idx=*/1);
  ASSERT_NE(fs, nullptr);
  EXPECT_EQ(fs->timestamp_rtp, 0u);
  EXPECT_EQ(fs->spatial_idx, 1);

  fs = stats.GetFrame(/*timestamp_rtp=*/1, /*spatial_idx=*/0);
  ASSERT_NE(fs, nullptr);
  EXPECT_EQ(fs->timestamp_rtp, 1u);
  EXPECT_EQ(fs->spatial_idx, 0);

  fs = stats.GetFrame(/*timestamp_rtp=*/1, /*spatial_idx=*/1);
  EXPECT_EQ(fs, nullptr);
}

class VideoCodecStatsImplSlicingTest
    : public ::testing::TestWithParam<std::tuple<Filter, std::vector<int>>> {};

TEST_P(VideoCodecStatsImplSlicingTest, Slice) {
  Filter filter = std::get<0>(GetParam());
  std::vector<int> expected_frames = std::get<1>(GetParam());
  std::vector<VideoCodecStats::Frame> frames = {
      {.frame_num = 0, .timestamp_rtp = 0, .spatial_idx = 0, .temporal_idx = 0},
      {.frame_num = 0, .timestamp_rtp = 0, .spatial_idx = 1, .temporal_idx = 0},
      {.frame_num = 1, .timestamp_rtp = 1, .spatial_idx = 0, .temporal_idx = 1},
      {.frame_num = 1,
       .timestamp_rtp = 1,
       .spatial_idx = 1,
       .temporal_idx = 1}};

  VideoCodecStatsImpl stats;
  stats.AddFrame(frames[0]);
  stats.AddFrame(frames[1]);
  stats.AddFrame(frames[2]);
  stats.AddFrame(frames[3]);

  std::vector<VideoCodecStats::Frame> slice = stats.Slice(filter);
  ASSERT_EQ(slice.size(), expected_frames.size());
  for (size_t i = 0; i < expected_frames.size(); ++i) {
    Frame& expected = frames[expected_frames[i]];
    EXPECT_EQ(slice[i].frame_num, expected.frame_num);
    EXPECT_EQ(slice[i].timestamp_rtp, expected.timestamp_rtp);
    EXPECT_EQ(slice[i].spatial_idx, expected.spatial_idx);
    EXPECT_EQ(slice[i].temporal_idx, expected.temporal_idx);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    VideoCodecStatsImplSlicingTest,
    ::testing::Values(
        std::make_tuple(Filter{}, std::vector<int>{0, 1, 2, 3}),
        std::make_tuple(Filter{.first_frame = 1}, std::vector<int>{2, 3}),
        std::make_tuple(Filter{.last_frame = 0}, std::vector<int>{0, 1}),
        std::make_tuple(Filter{.spatial_idx = 0}, std::vector<int>{0, 2}),
        std::make_tuple(Filter{.temporal_idx = 1},
                        std::vector<int>{0, 1, 2, 3})));

TEST(VideoCodecStatsImpl, AggregateBitrate) {
  std::vector<VideoCodecStats::Frame> frames = {
      {.frame_num = 0,
       .timestamp_rtp = 0,
       .frame_size = DataSize::Bytes(1000),
       .target_bitrate = DataRate::BytesPerSec(1000)},
      {.frame_num = 1,
       .timestamp_rtp = 90000,
       .frame_size = DataSize::Bytes(2000),
       .target_bitrate = DataRate::BytesPerSec(1000)}};

  Stream stream = VideoCodecStatsImpl().Aggregate(frames);
  EXPECT_EQ(stream.encoded_bitrate_kbps.GetAverage(), 12.0);
  EXPECT_EQ(stream.bitrate_mismatch_pct.GetAverage(), 50.0);
}

TEST(VideoCodecStatsImpl, AggregateFramerate) {
  std::vector<VideoCodecStats::Frame> frames = {
      {.frame_num = 0,
       .timestamp_rtp = 0,
       .frame_size = DataSize::Bytes(1),
       .target_framerate = Frequency::Hertz(1)},
      {.frame_num = 1,
       .timestamp_rtp = 90000,
       .frame_size = DataSize::Zero(),
       .target_framerate = Frequency::Hertz(1)}};

  Stream stream = VideoCodecStatsImpl().Aggregate(frames);
  EXPECT_EQ(stream.encoded_framerate_fps.GetAverage(), 0.5);
  EXPECT_EQ(stream.framerate_mismatch_pct.GetAverage(), -50.0);
}

TEST(VideoCodecStatsImpl, AggregateTransmissionTime) {
  std::vector<VideoCodecStats::Frame> frames = {
      {.frame_num = 0,
       .timestamp_rtp = 0,
       .frame_size = DataSize::Bytes(2),
       .target_bitrate = DataRate::BytesPerSec(1)},
      {.frame_num = 1,
       .timestamp_rtp = 90000,
       .frame_size = DataSize::Bytes(3),
       .target_bitrate = DataRate::BytesPerSec(1)}};

  Stream stream = VideoCodecStatsImpl().Aggregate(frames);
  ASSERT_EQ(stream.transmission_time_ms.NumSamples(), 2);
  ASSERT_EQ(stream.transmission_time_ms.GetSamples()[0], 2000);
  ASSERT_EQ(stream.transmission_time_ms.GetSamples()[1], 4000);
}

}  // namespace test
}  // namespace webrtc
