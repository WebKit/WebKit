/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "api/audio/audio_processing.h"
#include "modules/audio_processing/test/audio_processing_builder_for_testing.h"
#include "rtc_base/checks.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {
namespace {
constexpr int kMaxNumChannels = 2;
// APM supported max rate is 384000 Hz, using a limit slightly above lets the
// fuzzer exercise the handling of too high rates.
constexpr int kMaxSampleRateHz = 400000;
constexpr int kMaxSamplesPerChannel = kMaxSampleRateHz / 100;

void GenerateFloatFrame(test::FuzzDataHelper& fuzz_data,
                        int input_rate,
                        int num_channels,
                        float* const* float_frames) {
  const int samples_per_input_channel =
      AudioProcessing::GetFrameSize(input_rate);
  RTC_DCHECK_LE(samples_per_input_channel, kMaxSamplesPerChannel);
  for (int i = 0; i < num_channels; ++i) {
    float channel_value;
    fuzz_data.CopyTo<float>(&channel_value);
    std::fill(float_frames[i], float_frames[i] + samples_per_input_channel,
              channel_value);
  }
}

void GenerateFixedFrame(test::FuzzDataHelper& fuzz_data,
                        int input_rate,
                        int num_channels,
                        int16_t* fixed_frames) {
  const int samples_per_input_channel =
      AudioProcessing::GetFrameSize(input_rate);
  RTC_DCHECK_LE(samples_per_input_channel, kMaxSamplesPerChannel);
  // Write interleaved samples.
  for (int ch = 0; ch < num_channels; ++ch) {
    const int16_t channel_value = fuzz_data.ReadOrDefaultValue<int16_t>(0);
    for (int i = ch; i < samples_per_input_channel * num_channels;
         i += num_channels) {
      fixed_frames[i] = channel_value;
    }
  }
}

// No-op processor used to influence APM input/output pipeline decisions based
// on what submodules are present.
class NoopCustomProcessing : public CustomProcessing {
 public:
  NoopCustomProcessing() {}
  ~NoopCustomProcessing() override {}
  void Initialize(int sample_rate_hz, int num_channels) override {}
  void Process(AudioBuffer* audio) override {}
  std::string ToString() const override { return ""; }
  void SetRuntimeSetting(AudioProcessing::RuntimeSetting setting) override {}
};
}  // namespace

// This fuzzer is directed at fuzzing unexpected input and output sample rates
// of APM. For example, the sample rate 22050 Hz is processed by APM in frames
// of floor(22050/100) = 220 samples. This is not exactly 10 ms of audio
// content, and may break assumptions commonly made on the APM frame size.
void FuzzOneInput(const uint8_t* data, size_t size) {
  if (size > 100) {
    return;
  }
  test::FuzzDataHelper fuzz_data(rtc::ArrayView<const uint8_t>(data, size));

  std::unique_ptr<CustomProcessing> capture_processor =
      fuzz_data.ReadOrDefaultValue(true)
          ? std::make_unique<NoopCustomProcessing>()
          : nullptr;
  std::unique_ptr<CustomProcessing> render_processor =
      fuzz_data.ReadOrDefaultValue(true)
          ? std::make_unique<NoopCustomProcessing>()
          : nullptr;
  rtc::scoped_refptr<AudioProcessing> apm =
      AudioProcessingBuilderForTesting()
          .SetConfig({.pipeline = {.multi_channel_render = true,
                                   .multi_channel_capture = true}})
          .SetCapturePostProcessing(std::move(capture_processor))
          .SetRenderPreProcessing(std::move(render_processor))
          .Create();
  RTC_DCHECK(apm);

  std::array<int16_t, kMaxSamplesPerChannel * kMaxNumChannels> fixed_frame;
  std::array<std::array<float, kMaxSamplesPerChannel>, kMaxNumChannels>
      float_frames;
  std::array<float*, kMaxNumChannels> float_frame_ptrs;
  for (int i = 0; i < kMaxNumChannels; ++i) {
    float_frame_ptrs[i] = float_frames[i].data();
  }
  float* const* ptr_to_float_frames = &float_frame_ptrs[0];

  // Choose whether to fuzz the float or int16_t interfaces of APM.
  const bool is_float = fuzz_data.ReadOrDefaultValue(true);

  // We may run out of fuzz data in the middle of a loop iteration. In
  // that case, default values will be used for the rest of that
  // iteration.
  while (fuzz_data.CanReadBytes(1)) {
    // Decide input/output rate for this iteration.
    const int input_rate = static_cast<int>(
        fuzz_data.ReadOrDefaultValue<size_t>(8000) % kMaxSampleRateHz);
    const int output_rate = static_cast<int>(
        fuzz_data.ReadOrDefaultValue<size_t>(8000) % kMaxSampleRateHz);
    const int num_channels = fuzz_data.ReadOrDefaultValue(true) ? 2 : 1;

    // Since render and capture calls have slightly different reinitialization
    // procedures, we let the fuzzer choose the order.
    const bool is_capture = fuzz_data.ReadOrDefaultValue(true);

    int apm_return_code = AudioProcessing::Error::kNoError;
    if (is_float) {
      GenerateFloatFrame(fuzz_data, input_rate, num_channels,
                         ptr_to_float_frames);

      if (is_capture) {
        apm_return_code = apm->ProcessStream(
            ptr_to_float_frames, StreamConfig(input_rate, num_channels),
            StreamConfig(output_rate, num_channels), ptr_to_float_frames);
      } else {
        apm_return_code = apm->ProcessReverseStream(
            ptr_to_float_frames, StreamConfig(input_rate, num_channels),
            StreamConfig(output_rate, num_channels), ptr_to_float_frames);
      }
    } else {
      GenerateFixedFrame(fuzz_data, input_rate, num_channels,
                         fixed_frame.data());

      if (is_capture) {
        apm_return_code = apm->ProcessStream(
            fixed_frame.data(), StreamConfig(input_rate, num_channels),
            StreamConfig(output_rate, num_channels), fixed_frame.data());
      } else {
        apm_return_code = apm->ProcessReverseStream(
            fixed_frame.data(), StreamConfig(input_rate, num_channels),
            StreamConfig(output_rate, num_channels), fixed_frame.data());
      }
    }
    // APM may flag an error on unsupported audio formats, but should not crash.
    RTC_DCHECK(apm_return_code == AudioProcessing::kNoError ||
               apm_return_code == AudioProcessing::kBadSampleRateError);
  }
}

}  // namespace webrtc
