/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/bitrateallocationstrategy.h"
#include "rtc_base/gunit.h"

namespace rtc {

std::vector<const rtc::BitrateAllocationStrategy::TrackConfig*>
MakeTrackConfigPtrsVector(
    const std::vector<BitrateAllocationStrategy::TrackConfig>& track_configs) {
  std::vector<const rtc::BitrateAllocationStrategy::TrackConfig*>
      track_config_ptrs(track_configs.size());
  int i = 0;
  for (const auto& c : track_configs) {
    track_config_ptrs[i++] = &c;
  }
  return track_config_ptrs;
}

TEST(BitrateAllocationStrategyTest, SetAllBitratesToMinimum) {
  const std::string audio_track_id = "audio_track";
  constexpr uint32_t min_audio_bitrate = 6000;
  constexpr uint32_t max_audio_bitrate = 64000;
  const std::string video_track_id = "video_track";
  constexpr uint32_t min_video_bitrate = 30000;
  constexpr uint32_t max_video_bitrate = 300000;
  constexpr uint32_t min_other_bitrate = 3000;
  constexpr uint32_t max_other_bitrate = 30000;

  std::vector<BitrateAllocationStrategy::TrackConfig> track_configs = {
      BitrateAllocationStrategy::TrackConfig(
          min_audio_bitrate, max_audio_bitrate, false, audio_track_id),
      BitrateAllocationStrategy::TrackConfig(
          min_video_bitrate, max_video_bitrate, false, video_track_id),
      BitrateAllocationStrategy::TrackConfig(min_other_bitrate,
                                             max_other_bitrate, false, "")};

  std::vector<uint32_t> allocations =
      BitrateAllocationStrategy::SetAllBitratesToMinimum(track_configs);
  EXPECT_EQ(min_audio_bitrate, allocations[0]);
  EXPECT_EQ(min_video_bitrate, allocations[1]);
  EXPECT_EQ(min_other_bitrate, allocations[2]);
}

TEST(BitrateAllocationStrategyTest, DistributeBitratesEvenly) {
  const std::string audio_track_id = "audio_track";
  constexpr uint32_t min_audio_bitrate = 16000;
  constexpr uint32_t max_audio_bitrate = 64000;
  const std::string video_track_id = "video_track";
  constexpr uint32_t min_video_bitrate = 30000;
  constexpr uint32_t max_video_bitrate = 300000;
  constexpr uint32_t min_other_bitrate = 3000;
  constexpr uint32_t max_other_bitrate = 30000;
  constexpr uint32_t available_bitrate = 52000;
  constexpr uint32_t even_bitrate_increase =
      (available_bitrate - min_audio_bitrate - min_video_bitrate -
       min_other_bitrate) /
      3;

  std::vector<BitrateAllocationStrategy::TrackConfig> track_configs = {
      BitrateAllocationStrategy::TrackConfig(
          min_audio_bitrate, max_audio_bitrate, false, audio_track_id),
      BitrateAllocationStrategy::TrackConfig(
          min_video_bitrate, max_video_bitrate, false, video_track_id),
      BitrateAllocationStrategy::TrackConfig(min_other_bitrate,
                                             max_other_bitrate, false, "")};

  std::vector<uint32_t> allocations =
      BitrateAllocationStrategy::DistributeBitratesEvenly(track_configs,
                                                          available_bitrate);
  EXPECT_EQ(min_audio_bitrate + even_bitrate_increase, allocations[0]);
  EXPECT_EQ(min_video_bitrate + even_bitrate_increase, allocations[1]);
  EXPECT_EQ(min_other_bitrate + even_bitrate_increase, allocations[2]);
}

std::vector<uint32_t> RunAudioPriorityAllocation(
    uint32_t sufficient_audio_bitrate,
    std::string audio_track_id,
    uint32_t min_audio_bitrate,
    uint32_t max_audio_bitrate,
    std::string video_track_id,
    uint32_t min_video_bitrate,
    uint32_t max_video_bitrate,
    uint32_t min_other_bitrate,
    uint32_t max_other_bitrate,
    uint32_t available_bitrate) {
  AudioPriorityBitrateAllocationStrategy allocation_strategy(
      audio_track_id, sufficient_audio_bitrate);
  std::vector<BitrateAllocationStrategy::TrackConfig> track_configs = {
      BitrateAllocationStrategy::TrackConfig(
          min_audio_bitrate, max_audio_bitrate, false, audio_track_id),
      BitrateAllocationStrategy::TrackConfig(
          min_video_bitrate, max_video_bitrate, false, video_track_id),
      BitrateAllocationStrategy::TrackConfig(min_other_bitrate,
                                             max_other_bitrate, false, "")};

  return allocation_strategy.AllocateBitrates(available_bitrate, track_configs);
}

// Test that when the available bitrate is less than the sum of the minimum
// bitrates, the minimum bitrate is allocated for each track.
TEST(AudioPriorityBitrateAllocationStrategyTest, MinAllocateBitrate) {
  constexpr uint32_t sufficient_audio_bitrate = 16000;
  const std::string audio_track_id = "audio_track";
  constexpr uint32_t min_audio_bitrate = 6000;
  constexpr uint32_t max_audio_bitrate = 64000;
  const std::string video_track_id = "video_track";
  constexpr uint32_t min_video_bitrate = 30000;
  constexpr uint32_t max_video_bitrate = 300000;
  constexpr uint32_t min_other_bitrate = 3000;
  constexpr uint32_t max_other_bitrate = 30000;
  constexpr uint32_t available_bitrate = 10000;

  std::vector<uint32_t> allocations = RunAudioPriorityAllocation(
      sufficient_audio_bitrate, audio_track_id, min_audio_bitrate,
      max_audio_bitrate, video_track_id, min_video_bitrate, max_video_bitrate,
      min_other_bitrate, max_other_bitrate, available_bitrate);
  EXPECT_EQ(min_audio_bitrate, allocations[0]);
  EXPECT_EQ(min_video_bitrate, allocations[1]);
  EXPECT_EQ(min_other_bitrate, allocations[2]);
}

// Test that when the available bitrate is more than the sum of the max
// bitrates, the max bitrate is allocated for each track.
TEST(AudioPriorityBitrateAllocationStrategyTest, MaxAllocateBitrate) {
  constexpr uint32_t sufficient_audio_bitrate = 16000;
  const std::string audio_track_id = "audio_track";
  constexpr uint32_t min_audio_bitrate = 6000;
  constexpr uint32_t max_audio_bitrate = 64000;
  const std::string video_track_id = "video_track";
  constexpr uint32_t min_video_bitrate = 30000;
  constexpr uint32_t max_video_bitrate = 300000;
  constexpr uint32_t min_other_bitrate = 3000;
  constexpr uint32_t max_other_bitrate = 30000;
  constexpr uint32_t available_bitrate = 400000;

  std::vector<uint32_t> allocations = RunAudioPriorityAllocation(
      sufficient_audio_bitrate, audio_track_id, min_audio_bitrate,
      max_audio_bitrate, video_track_id, min_video_bitrate, max_video_bitrate,
      min_other_bitrate, max_other_bitrate, available_bitrate);

  // TODO(bugs.webrtc.org/8541): Until the bug is fixed not audio streams will
  // get up to kTransmissionMaxBitrateMultiplier*max_bitrate
  constexpr uint32_t video_bitrate =
      (available_bitrate - max_audio_bitrate - max_other_bitrate * 2);
  EXPECT_EQ(max_audio_bitrate, allocations[0]);
  EXPECT_EQ(video_bitrate, allocations[1]);
  EXPECT_EQ(max_other_bitrate * 2, allocations[2]);
}

// Test that audio track will get up to sufficient bitrate before video and
// other bitrate will be allocated.
TEST(AudioPriorityBitrateAllocationStrategyTest, AudioPriorityAllocateBitrate) {
  constexpr uint32_t sufficient_audio_bitrate = 16000;
  const std::string audio_track_id = "audio_track";
  constexpr uint32_t min_audio_bitrate = 6000;
  constexpr uint32_t max_audio_bitrate = 64000;
  const std::string video_track_id = "video_track";
  constexpr uint32_t min_video_bitrate = 30000;
  constexpr uint32_t max_video_bitrate = 300000;
  constexpr uint32_t min_other_bitrate = 3000;
  constexpr uint32_t max_other_bitrate = 30000;
  constexpr uint32_t available_bitrate = 49000;

  std::vector<uint32_t> allocations = RunAudioPriorityAllocation(
      sufficient_audio_bitrate, audio_track_id, min_audio_bitrate,
      max_audio_bitrate, video_track_id, min_video_bitrate, max_video_bitrate,
      min_other_bitrate, max_other_bitrate, available_bitrate);
  EXPECT_EQ(sufficient_audio_bitrate, allocations[0]);
  EXPECT_EQ(min_video_bitrate, allocations[1]);
  EXPECT_EQ(min_other_bitrate, allocations[2]);
}

// Test that bitrate will be allocated evenly after sufficient audio bitrate is
// allocated.
TEST(AudioPriorityBitrateAllocationStrategyTest, EvenAllocateBitrate) {
  constexpr uint32_t sufficient_audio_bitrate = 16000;
  const std::string audio_track_id = "audio_track";
  constexpr uint32_t min_audio_bitrate = 6000;
  constexpr uint32_t max_audio_bitrate = 64000;
  const std::string video_track_id = "video_track";
  constexpr uint32_t min_video_bitrate = 30000;
  constexpr uint32_t max_video_bitrate = 300000;
  constexpr uint32_t min_other_bitrate = 3000;
  constexpr uint32_t max_other_bitrate = 30000;
  constexpr uint32_t available_bitrate = 52000;
  constexpr uint32_t even_bitrate_increase =
      (available_bitrate - sufficient_audio_bitrate - min_video_bitrate -
       min_other_bitrate) /
      3;

  std::vector<uint32_t> allocations = RunAudioPriorityAllocation(
      sufficient_audio_bitrate, audio_track_id, min_audio_bitrate,
      max_audio_bitrate, video_track_id, min_video_bitrate, max_video_bitrate,
      min_other_bitrate, max_other_bitrate, available_bitrate);
  EXPECT_EQ(sufficient_audio_bitrate + even_bitrate_increase, allocations[0]);
  EXPECT_EQ(min_video_bitrate + even_bitrate_increase, allocations[1]);
  EXPECT_EQ(min_other_bitrate + even_bitrate_increase, allocations[2]);
}

// Test that bitrate will be allocated to video after audio and other max
// allocation.
TEST(AudioPriorityBitrateAllocationStrategyTest, VideoAllocateBitrate) {
  constexpr uint32_t sufficient_audio_bitrate = 16000;
  const std::string audio_track_id = "audio_track";
  constexpr uint32_t min_audio_bitrate = 6000;
  constexpr uint32_t max_audio_bitrate = 64000;
  const std::string video_track_id = "video_track";
  constexpr uint32_t min_video_bitrate = 30000;
  constexpr uint32_t max_video_bitrate = 300000;
  constexpr uint32_t min_other_bitrate = 3000;
  constexpr uint32_t max_other_bitrate = 30000;
  constexpr uint32_t available_bitrate = 200000;
  constexpr uint32_t video_bitrate =
      available_bitrate - max_audio_bitrate - max_other_bitrate;

  std::vector<uint32_t> allocations = RunAudioPriorityAllocation(
      sufficient_audio_bitrate, audio_track_id, min_audio_bitrate,
      max_audio_bitrate, video_track_id, min_video_bitrate, max_video_bitrate,
      min_other_bitrate, max_other_bitrate, available_bitrate);
  EXPECT_EQ(max_audio_bitrate, allocations[0]);
  EXPECT_EQ(video_bitrate, allocations[1]);
  EXPECT_EQ(max_other_bitrate, allocations[2]);
}

}  // namespace rtc
