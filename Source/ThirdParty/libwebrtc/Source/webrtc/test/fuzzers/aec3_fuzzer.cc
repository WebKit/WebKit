/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "absl/types/optional.h"
#include "modules/audio_processing/aec3/echo_canceller3.h"
#include "modules/audio_processing/audio_buffer.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {
namespace {
using SampleRate = ::webrtc::AudioProcessing::NativeRate;

void PrepareAudioBuffer(int sample_rate_hz,
                        test::FuzzDataHelper* fuzz_data,
                        AudioBuffer* buffer) {
  float* const* channels = buffer->channels_f();
  for (size_t i = 0; i < buffer->num_channels(); ++i) {
    for (size_t j = 0; j < buffer->num_frames(); ++j) {
      channels[i][j] =
          static_cast<float>(fuzz_data->ReadOrDefaultValue<int16_t>(0));
    }
  }
  if (sample_rate_hz == 32000 || sample_rate_hz == 48000) {
    buffer->SplitIntoFrequencyBands();
  }
}

}  // namespace

void FuzzOneInput(const uint8_t* data, size_t size) {
  if (size > 200000) {
    return;
  }

  test::FuzzDataHelper fuzz_data(rtc::ArrayView<const uint8_t>(data, size));

  constexpr int kSampleRates[] = {16000, 32000, 48000};
  const int sample_rate_hz =
      static_cast<size_t>(fuzz_data.SelectOneOf(kSampleRates));

  constexpr int kMaxNumChannels = 9;
  const size_t num_render_channels =
      1 + fuzz_data.ReadOrDefaultValue<uint8_t>(0) % (kMaxNumChannels - 1);
  const size_t num_capture_channels =
      1 + fuzz_data.ReadOrDefaultValue<uint8_t>(0) % (kMaxNumChannels - 1);

  EchoCanceller3 aec3(EchoCanceller3Config(),
                      /*multichannel_config=*/absl::nullopt, sample_rate_hz,
                      num_render_channels, num_capture_channels);

  AudioBuffer capture_audio(sample_rate_hz, num_capture_channels,
                            sample_rate_hz, num_capture_channels,
                            sample_rate_hz, num_capture_channels);
  AudioBuffer render_audio(sample_rate_hz, num_render_channels, sample_rate_hz,
                           num_render_channels, sample_rate_hz,
                           num_render_channels);

  // Fuzz frames while there is still fuzzer data.
  while (fuzz_data.BytesLeft() > 0) {
    bool is_capture = fuzz_data.ReadOrDefaultValue(true);
    bool level_changed = fuzz_data.ReadOrDefaultValue(true);
    if (is_capture) {
      PrepareAudioBuffer(sample_rate_hz, &fuzz_data, &capture_audio);
      aec3.ProcessCapture(&capture_audio, level_changed);
    } else {
      PrepareAudioBuffer(sample_rate_hz, &fuzz_data, &render_audio);
      aec3.AnalyzeRender(&render_audio);
    }
  }
}
}  // namespace webrtc
