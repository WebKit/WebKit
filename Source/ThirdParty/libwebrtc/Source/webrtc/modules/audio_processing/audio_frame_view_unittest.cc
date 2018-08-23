/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/include/audio_frame_view.h"

#include "modules/audio_processing/audio_buffer.h"
#include "test/gtest.h"

namespace webrtc {
TEST(AudioFrameTest, ConstructFromAudioBuffer) {
  constexpr int kSampleRateHz = 48000;
  constexpr int kNumChannels = 2;
  constexpr float kFloatConstant = 1272.f;
  constexpr float kIntConstant = 17252;
  const webrtc::StreamConfig stream_config(kSampleRateHz, kNumChannels, false);
  webrtc::AudioBuffer buffer(
      stream_config.num_frames(), stream_config.num_channels(),
      stream_config.num_frames(), stream_config.num_channels(),
      stream_config.num_frames());

  AudioFrameView<float> non_const_view(
      buffer.channels_f(), buffer.num_channels(), buffer.num_frames());
  // Modification is allowed.
  non_const_view.channel(0)[0] = kFloatConstant;
  EXPECT_EQ(buffer.channels_f()[0][0], kFloatConstant);

  AudioFrameView<const float> const_view(
      buffer.channels_f(), buffer.num_channels(), buffer.num_frames());
  // Modification is not allowed.
  // const_view.channel(0)[0] = kFloatConstant;

  // Assignment is allowed.
  AudioFrameView<const float> other_const_view = non_const_view;
  static_cast<void>(other_const_view);

  // But not the other way. The following will fail:
  // non_const_view = other_const_view;

  AudioFrameView<int16_t> non_const_int16_view(
      buffer.channels(), buffer.num_channels(), buffer.num_frames());
  non_const_int16_view.channel(0)[0] = kIntConstant;
  EXPECT_EQ(buffer.channels()[0][0], kIntConstant);
}
}  // namespace webrtc
