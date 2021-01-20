/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/adaptation/video_stream_input_state_provider.h"

#include <utility>

#include "api/video_codecs/video_encoder.h"
#include "call/adaptation/encoder_settings.h"
#include "call/adaptation/test/fake_frame_rate_provider.h"
#include "test/gtest.h"

namespace webrtc {

TEST(VideoStreamInputStateProviderTest, DefaultValues) {
  FakeFrameRateProvider frame_rate_provider;
  VideoStreamInputStateProvider input_state_provider(&frame_rate_provider);
  VideoStreamInputState input_state = input_state_provider.InputState();
  EXPECT_EQ(false, input_state.has_input());
  EXPECT_EQ(absl::nullopt, input_state.frame_size_pixels());
  EXPECT_EQ(0, input_state.frames_per_second());
  EXPECT_EQ(VideoCodecType::kVideoCodecGeneric, input_state.video_codec_type());
  EXPECT_EQ(kDefaultMinPixelsPerFrame, input_state.min_pixels_per_frame());
}

TEST(VideoStreamInputStateProviderTest, ValuesSet) {
  FakeFrameRateProvider frame_rate_provider;
  VideoStreamInputStateProvider input_state_provider(&frame_rate_provider);
  input_state_provider.OnHasInputChanged(true);
  input_state_provider.OnFrameSizeObserved(42);
  frame_rate_provider.set_fps(123);
  VideoEncoder::EncoderInfo encoder_info;
  encoder_info.scaling_settings.min_pixels_per_frame = 1337;
  VideoEncoderConfig encoder_config;
  encoder_config.codec_type = VideoCodecType::kVideoCodecVP9;
  input_state_provider.OnEncoderSettingsChanged(EncoderSettings(
      std::move(encoder_info), std::move(encoder_config), VideoCodec()));
  VideoStreamInputState input_state = input_state_provider.InputState();
  EXPECT_EQ(true, input_state.has_input());
  EXPECT_EQ(42, input_state.frame_size_pixels());
  EXPECT_EQ(123, input_state.frames_per_second());
  EXPECT_EQ(VideoCodecType::kVideoCodecVP9, input_state.video_codec_type());
  EXPECT_EQ(1337, input_state.min_pixels_per_frame());
}

}  // namespace webrtc
