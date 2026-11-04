/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/acm2/acm_resampler.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "api/audio/audio_frame.h"
#include "api/audio/audio_view.h"
#include "test/gtest.h"

namespace webrtc {
namespace acm2 {

TEST(ResamplerHelperTest, MaybeResampleCheckForMaxSize) {
  ResamplerHelper resampler;
  AudioFrame audio_frame;

  // Create an audio frame that requires resampling from 48kHz to 96kHz
  // with a high number of channels (16) to exceed the buffer size.
  const int kCurrentSampleRateHz = 48000;
  const int kDesiredSampleRateHz = 96000;
  const size_t kChannels = 16;

  // 10 ms of data at 48kHz = 480 samples per channel.
  std::vector<int16_t> dummy_data(480 * 16, 0);
  audio_frame.UpdateFrame(0, dummy_data.data(), 480, kCurrentSampleRateHz,
                          AudioFrame::kNormalSpeech, AudioFrame::kVadActive,
                          kChannels);

  // The resampler prime path will attempt to allocate a buffer that is
  // kChannels * (kDesiredSampleRateHz / 100) = 16 * 960 = 15360 samples,
  // which exceeds AudioFrame::kMaxDataSizeSamples (7680).
  const bool resample_success =
      resampler.MaybeResample(kDesiredSampleRateHz, &audio_frame);

  // Verify that MaybeResample correctly detects the buffer size condition and
  // safely aborts the operation by returning false, muting the frame, and
  // capping the channel count to avoid a buffer overflow in the muted data
  // array.
  EXPECT_FALSE(resample_success);
  EXPECT_TRUE(audio_frame.muted());
  EXPECT_EQ(audio_frame.sample_rate_hz_, kDesiredSampleRateHz);
  EXPECT_EQ(audio_frame.num_channels_, AudioFrame::kMaxDataSizeSamples /
                                           audio_frame.samples_per_channel());
}

TEST(ResamplerHelperTest, MaybeResampleValidMaxSize) {
  ResamplerHelper resampler;
  AudioFrame audio_frame;

  // Ensure that resampling within the valid buffer size does not trigger the
  // muting behavior. We'll use a valid number of channels (e.g. 1) that will
  // not exceed the bounds.
  const int kCurrentSampleRateHz = 32000;
  const int kDesiredSampleRateHz = 48000;
  const size_t kChannels = 1;

  std::vector<int16_t> dummy_data(320 * 1, 1000);
  audio_frame.UpdateFrame(0, dummy_data.data(), 320, kCurrentSampleRateHz,
                          AudioFrame::kNormalSpeech, AudioFrame::kVadActive,
                          kChannels);

  const bool resample_success =
      resampler.MaybeResample(kDesiredSampleRateHz, &audio_frame);

  EXPECT_TRUE(resample_success);
  EXPECT_FALSE(audio_frame.muted());
  EXPECT_EQ(audio_frame.sample_rate_hz_, kDesiredSampleRateHz);
  EXPECT_EQ(audio_frame.num_channels_, kChannels);
}

namespace {

constexpr int kInputSampleRateHz = 16000;
constexpr size_t kInputSamplesPerChannel = kInputSampleRateHz / 100;
// Twice the input rate: the resampling ratio is exact in floating point, so
// the outputs of two resamplers can be compared sample by sample.
constexpr int kOutputSampleRateHz = 32000;
constexpr size_t kOutputSamplesPerChannel = kOutputSampleRateHz / 100;

// Sets `frame` to 10 ms of mono audio at `sample_rate_hz` with all samples
// equal to `value`, or to a muted frame if `value` is std::nullopt.
void SetFrame(std::optional<int16_t> value,
              int sample_rate_hz,
              AudioFrame* frame) {
  const size_t samples_per_channel =
      SampleRateToDefaultChannelSize(sample_rate_hz);
  const std::vector<int16_t> samples(samples_per_channel, value.value_or(0));
  frame->UpdateFrame(/*timestamp=*/0, value ? samples.data() : nullptr,
                     samples_per_channel, sample_rate_hz,
                     AudioFrame::kNormalSpeech, AudioFrame::kVadActive);
}

std::vector<int16_t> Samples(const AudioFrame& frame) {
  InterleavedView<const int16_t> samples = frame.data_view();
  return std::vector<int16_t>(samples.begin(), samples.end());
}

}  // namespace

TEST(ResamplerHelperTest, KeepsMutedFrameAfterMutedFrameMuted) {
  ResamplerHelper resampler;
  AudioFrame audio_frame;
  SetFrame(1000, kInputSampleRateHz, &audio_frame);
  ASSERT_TRUE(resampler.MaybeResample(kOutputSampleRateHz, &audio_frame));

  // The first muted frame after audio comes out unmuted: the resampled output
  // starts with the tail of that audio.
  SetFrame(std::nullopt, kInputSampleRateHz, &audio_frame);
  ASSERT_TRUE(resampler.MaybeResample(kOutputSampleRateHz, &audio_frame));
  EXPECT_FALSE(audio_frame.muted());
  EXPECT_NE(audio_frame.data_view()[0], 0);

  // The next muted frame would resample to silence, so it stays muted.
  SetFrame(std::nullopt, kInputSampleRateHz, &audio_frame);
  ASSERT_TRUE(resampler.MaybeResample(kOutputSampleRateHz, &audio_frame));
  EXPECT_TRUE(audio_frame.muted());
  EXPECT_EQ(audio_frame.sample_rate_hz_, kOutputSampleRateHz);
  EXPECT_EQ(audio_frame.samples_per_channel(), kOutputSamplesPerChannel);
}

// Keeping muted frames muted doesn't change the audio: the output is the same
// as that of a resampler that gets zeros instead of muted frames.
TEST(ResamplerHelperTest, KeepingMutedFramesMutedDoesNotChangeTheOutput) {
  // Larger frames than at kInputSampleRateHz. Resampling from this rate to
  // kOutputSampleRateHz is exact in floating point too.
  constexpr int kLargeFrameSampleRateHz = 48000;
  struct Step {
    bool muted;
    int desired_sample_rate_hz;
    int input_sample_rate_hz = kInputSampleRateHz;
  };
  constexpr Step kSteps[] = {
      {false, kOutputSampleRateHz},
      {true, kOutputSampleRateHz},
      {true, kOutputSampleRateHz},
      {false, kOutputSampleRateHz},
      // Muted without resampling, then resampling resumes while muted.
      {true, kInputSampleRateHz},
      {true, kOutputSampleRateHz},
      {false, kOutputSampleRateHz},
      // The same, but the audio before and after the muted frames comes in
      // larger frames than the muted frames.
      {false, kLargeFrameSampleRateHz, kLargeFrameSampleRateHz},
      {true, kInputSampleRateHz},
      {true, kOutputSampleRateHz},
      {false, kOutputSampleRateHz, kLargeFrameSampleRateHz},
  };
  ResamplerHelper resampler;
  ResamplerHelper reference;
  AudioFrame audio_frame;
  AudioFrame reference_frame;
  int step_index = 0;
  for (const Step& step : kSteps) {
    SCOPED_TRACE(step_index++);
    const int16_t value = step.muted ? 0 : 1000;
    SetFrame(step.muted ? std::nullopt : std::make_optional(value),
             step.input_sample_rate_hz, &audio_frame);
    SetFrame(value, step.input_sample_rate_hz, &reference_frame);
    ASSERT_TRUE(
        resampler.MaybeResample(step.desired_sample_rate_hz, &audio_frame));
    ASSERT_TRUE(
        reference.MaybeResample(step.desired_sample_rate_hz, &reference_frame));
    EXPECT_EQ(Samples(audio_frame), Samples(reference_frame));
  }
}

// A frame that MaybeResample() rejects doesn't reach the resampler, so a muted
// frame after it still gets the tail of the audio before it.
TEST(ResamplerHelperTest, MutedFrameAfterRejectedFrameGetsTheTail) {
  // 10 ms of kChannels at kTooHighSampleRateHz don't fit in an AudioFrame.
  constexpr size_t kChannels = 8;
  constexpr int kTooHighSampleRateHz = 192000;
  const std::vector<int16_t> audio(kInputSamplesPerChannel * kChannels, 1000);
  ResamplerHelper resampler;
  AudioFrame audio_frame;
  auto set_frame = [&](const int16_t* data) {
    audio_frame.UpdateFrame(/*timestamp=*/0, data, kInputSamplesPerChannel,
                            kInputSampleRateHz, AudioFrame::kNormalSpeech,
                            AudioFrame::kVadActive, kChannels);
  };
  set_frame(audio.data());
  ASSERT_TRUE(resampler.MaybeResample(kOutputSampleRateHz, &audio_frame));

  set_frame(/*data=*/nullptr);
  EXPECT_FALSE(resampler.MaybeResample(kTooHighSampleRateHz, &audio_frame));

  set_frame(/*data=*/nullptr);
  ASSERT_TRUE(resampler.MaybeResample(kOutputSampleRateHz, &audio_frame));
  EXPECT_FALSE(audio_frame.muted());
  EXPECT_NE(audio_frame.data_view()[0], 0);
}

}  // namespace acm2
}  // namespace webrtc
