/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_mixer/audio_frame_manipulator.h"

#include <algorithm>

#include "test/gtest.h"

namespace webrtc {
namespace {

void FillFrameWithConstants(size_t samples_per_channel,
                            size_t number_of_channels,
                            int16_t value,
                            AudioFrame* frame) {
  frame->num_channels_ = number_of_channels;
  frame->samples_per_channel_ = samples_per_channel;
  int16_t* frame_data = frame->mutable_data();
  std::fill(frame_data, frame_data + samples_per_channel * number_of_channels,
            value);
}
}  // namespace

TEST(AudioFrameManipulator, CompareForwardRampWithExpectedResultStereo) {
  constexpr int kSamplesPerChannel = 5;
  constexpr int kNumberOfChannels = 2;

  // Create a frame with values 5, 5, 5, ... and channels & samples as above.
  AudioFrame frame;
  FillFrameWithConstants(kSamplesPerChannel, kNumberOfChannels, 5, &frame);

  Ramp(0.0f, 1.0f, &frame);

  const int total_samples = kSamplesPerChannel * kNumberOfChannels;
  const int16_t expected_result[total_samples] = {0, 0, 1, 1, 2, 2, 3, 3, 4, 4};
  const int16_t* frame_data = frame.data();
  EXPECT_TRUE(
      std::equal(frame_data, frame_data + total_samples, expected_result));
}

TEST(AudioFrameManipulator, CompareBackwardRampWithExpectedResultMono) {
  constexpr int kSamplesPerChannel = 5;
  constexpr int kNumberOfChannels = 1;

  // Create a frame with values 5, 5, 5, ... and channels & samples as above.
  AudioFrame frame;
  FillFrameWithConstants(kSamplesPerChannel, kNumberOfChannels, 5, &frame);

  Ramp(1.0f, 0.0f, &frame);

  const int total_samples = kSamplesPerChannel * kNumberOfChannels;
  const int16_t expected_result[total_samples] = {5, 4, 3, 2, 1};
  const int16_t* frame_data = frame.data();
  EXPECT_TRUE(
      std::equal(frame_data, frame_data + total_samples, expected_result));
}

}  // namespace webrtc
