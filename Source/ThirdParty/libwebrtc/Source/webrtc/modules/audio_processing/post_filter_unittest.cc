/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/post_filter.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <vector>

#include "api/array_view.h"
#include "api/audio/audio_processing.h"
#include "modules/audio_processing/audio_buffer.h"
#include "modules/audio_processing/test/audio_buffer_tools.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::FloatNear;
using ::testing::Le;
using ::testing::NotNull;

// Process one frame of data via the AudioBuffer interface and produce the
// output.
std::vector<float> ProcessOneFrameAsAudioBuffer(
    const std::vector<float>& frame_input,
    const StreamConfig& stream_config,
    PostFilter* filter) {
  AudioBuffer audio_buffer(
      stream_config.sample_rate_hz(), stream_config.num_channels(),
      stream_config.sample_rate_hz(), stream_config.num_channels(),
      stream_config.sample_rate_hz(), stream_config.num_channels());

  test::CopyVectorToAudioBuffer(stream_config, frame_input, &audio_buffer);
  filter->Process(audio_buffer);
  std::vector<float> frame_output;
  test::ExtractVectorFromAudioBuffer(stream_config, &audio_buffer,
                                     &frame_output);
  return frame_output;
}

float ComputePower(ArrayView<const float> audio) {
  double energy = 0.0;
  std::for_each(audio.begin(), audio.end(),
                [&energy](float x) { energy += x * x; });
  return energy / audio.size();
}

float PowerDecibelToLinear(float db) {
  return std::powf(10.0, db / 10.0);
}
}  // namespace

class PostFilterCreateTest : public ::testing::TestWithParam<int> {};

TEST_P(PostFilterCreateTest, CreateOnlyFor48k) {
  const int sample_rate_hz = GetParam();
  std::unique_ptr<PostFilter> filter =
      PostFilter::CreateIfNeeded(sample_rate_hz, /*num_channels=*/2);
  if (sample_rate_hz == 48000) {
    EXPECT_NE(filter, nullptr);
  } else {
    EXPECT_EQ(filter, nullptr);
  }
}

INSTANTIATE_TEST_SUITE_P(
    _,
    PostFilterCreateTest,
    ::testing::Values(8000, 16000, 24000, 32000, 44100, 48000));

TEST(PostFilterTest, Tone19p8kHzSignalAttenuation48k) {
  constexpr int sample_rate_hz = 48000;

  std::unique_ptr<PostFilter> filter =
      PostFilter::CreateIfNeeded(sample_rate_hz, 1);
  ASSERT_THAT(filter, NotNull());

  const StreamConfig stream_config(sample_rate_hz, 1);

  constexpr int num_frames = sample_rate_hz * 10 / 1000;  // 10ms;
  constexpr double tone_frequency = 19800;                // Hz

  const double phase_increment =
      tone_frequency * 2.0 * std::numbers::pi / sample_rate_hz;
  double phase = 0.0;

  std::vector<float> audio_input(num_frames);

  // Prime the filter to avoid initial state effect.
  std::generate(audio_input.begin(), audio_input.end(),
                [&phase, phase_increment]() {
                  phase += phase_increment;
                  return std::sinf(phase);
                });
  ProcessOneFrameAsAudioBuffer(audio_input, stream_config, filter.get());

  for (int n = 0; n < 20; ++n) {
    std::generate(audio_input.begin(), audio_input.end(),
                  [&phase, phase_increment]() {
                    phase += phase_increment;
                    return std::sinf(phase);
                  });
    const float input_power = ComputePower(audio_input);

    std::vector<float> audio_output =
        ProcessOneFrameAsAudioBuffer(audio_input, stream_config, filter.get());

    float output_power = ComputePower(audio_output);
    EXPECT_THAT(output_power / input_power, Le(PowerDecibelToLinear(-20.0)));
  }
}

TEST(PostFilterTest, Tone17kHzSignalNoAttenuation48k) {
  constexpr int sample_rate_hz = 48000;

  std::unique_ptr<PostFilter> filter =
      PostFilter::CreateIfNeeded(sample_rate_hz, 1);
  ASSERT_THAT(filter, NotNull());

  const StreamConfig stream_config(sample_rate_hz, 1);

  constexpr int num_frames = sample_rate_hz * 10 / 1000;  // 10ms;
  constexpr double tone_frequency = 16800;                // Hz

  const double phase_increment =
      tone_frequency * 2.0 * std::numbers::pi / sample_rate_hz;
  double phase = 0.0;

  std::vector<float> audio_input(num_frames);

  // Prime the filter to avoid initial state effect.
  std::generate(audio_input.begin(), audio_input.end(),
                [&phase, phase_increment]() {
                  phase += phase_increment;
                  return std::sinf(phase);
                });
  ProcessOneFrameAsAudioBuffer(audio_input, stream_config, filter.get());

  for (int n = 0; n < 20; ++n) {
    std::generate(audio_input.begin(), audio_input.end(),
                  [&phase, phase_increment]() {
                    phase += phase_increment;
                    return std::sinf(phase);
                  });
    const float input_power = ComputePower(audio_input);

    std::vector<float> audio_output =
        ProcessOneFrameAsAudioBuffer(audio_input, stream_config, filter.get());

    float output_power = ComputePower(audio_output);
    EXPECT_THAT(10 * std::log10f(output_power / input_power),
                FloatNear(0.0, 0.5f));
  }
}
}  // namespace webrtc
