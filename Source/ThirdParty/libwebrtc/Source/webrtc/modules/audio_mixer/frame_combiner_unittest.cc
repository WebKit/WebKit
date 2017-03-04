/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_mixer/frame_combiner.h"

#include <numeric>
#include <sstream>
#include <string>

#include "webrtc/base/checks.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

namespace {
std::string ProduceDebugText(int sample_rate_hz,
                             int number_of_channels,
                             int number_of_sources) {
  std::ostringstream ss;
  ss << "Sample rate: " << sample_rate_hz << " ";
  ss << "Number of channels: " << number_of_channels << " ";
  ss << "Number of sources: " << number_of_sources;
  return ss.str();
}

AudioFrame frame1;
AudioFrame frame2;
AudioFrame audio_frame_for_mixing;

void SetUpFrames(int sample_rate_hz, int number_of_channels) {
  for (auto* frame : {&frame1, &frame2}) {
    frame->UpdateFrame(-1, 0, nullptr,
                       rtc::CheckedDivExact(sample_rate_hz, 100),
                       sample_rate_hz, AudioFrame::kNormalSpeech,
                       AudioFrame::kVadActive, number_of_channels);
  }
}
}  // namespace

TEST(FrameCombiner, BasicApiCallsLimiter) {
  FrameCombiner combiner(true);
  for (const int rate : {8000, 16000, 32000, 48000}) {
    for (const int number_of_channels : {1, 2}) {
      const std::vector<AudioFrame*> all_frames = {&frame1, &frame2};
      SetUpFrames(rate, number_of_channels);

      for (const int number_of_frames : {0, 1, 2}) {
        SCOPED_TRACE(
            ProduceDebugText(rate, number_of_channels, number_of_frames));
        const std::vector<AudioFrame*> frames_to_combine(
            all_frames.begin(), all_frames.begin() + number_of_frames);
        combiner.Combine(frames_to_combine, number_of_channels, rate,
                         &audio_frame_for_mixing);
      }
    }
  }
}

// No APM limiter means no AudioProcessing::NativeRate restriction
// on rate. The rate has to be divisible by 100 since we use
// 10 ms frames, though.
TEST(FrameCombiner, BasicApiCallsNoLimiter) {
  FrameCombiner combiner(false);
  for (const int rate : {8000, 10000, 11000, 32000, 44100}) {
    for (const int number_of_channels : {1, 2}) {
      const std::vector<AudioFrame*> all_frames = {&frame1, &frame2};
      SetUpFrames(rate, number_of_channels);

      for (const int number_of_frames : {0, 1, 2}) {
        SCOPED_TRACE(
            ProduceDebugText(rate, number_of_channels, number_of_frames));
        const std::vector<AudioFrame*> frames_to_combine(
            all_frames.begin(), all_frames.begin() + number_of_frames);
        combiner.Combine(frames_to_combine, number_of_channels, rate,
                         &audio_frame_for_mixing);
      }
    }
  }
}

TEST(FrameCombiner, CombiningZeroFramesShouldProduceSilence) {
  FrameCombiner combiner(false);
  for (const int rate : {8000, 10000, 11000, 32000, 44100}) {
    for (const int number_of_channels : {1, 2}) {
      SCOPED_TRACE(ProduceDebugText(rate, number_of_channels, 0));

      const std::vector<AudioFrame*> frames_to_combine;
      combiner.Combine(frames_to_combine, number_of_channels, rate,
                       &audio_frame_for_mixing);

      const std::vector<int16_t> mixed_data(
          audio_frame_for_mixing.data_,
          audio_frame_for_mixing.data_ + number_of_channels * rate / 100);

      const std::vector<int16_t> expected(number_of_channels * rate / 100, 0);
      EXPECT_EQ(mixed_data, expected);
    }
  }
}

TEST(FrameCombiner, CombiningOneFrameShouldNotChangeFrame) {
  FrameCombiner combiner(false);
  for (const int rate : {8000, 10000, 11000, 32000, 44100}) {
    for (const int number_of_channels : {1, 2}) {
      SCOPED_TRACE(ProduceDebugText(rate, number_of_channels, 1));

      SetUpFrames(rate, number_of_channels);
      std::iota(frame1.data_, frame1.data_ + number_of_channels * rate / 100,
                0);
      const std::vector<AudioFrame*> frames_to_combine = {&frame1};
      combiner.Combine(frames_to_combine, number_of_channels, rate,
                       &audio_frame_for_mixing);

      const std::vector<int16_t> mixed_data(
          audio_frame_for_mixing.data_,
          audio_frame_for_mixing.data_ + number_of_channels * rate / 100);

      std::vector<int16_t> expected(number_of_channels * rate / 100);
      std::iota(expected.begin(), expected.end(), 0);
      EXPECT_EQ(mixed_data, expected);
    }
  }
}

}  // namespace webrtc
