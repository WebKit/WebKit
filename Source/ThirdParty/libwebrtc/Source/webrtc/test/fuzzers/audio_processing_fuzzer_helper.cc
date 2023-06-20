/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/fuzzers/audio_processing_fuzzer_helper.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "api/audio/audio_frame.h"
#include "modules/audio_processing/include/audio_frame_proxies.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace {
bool ValidForApm(float x) {
  return std::isfinite(x) && -1.0f <= x && x <= 1.0f;
}

void GenerateFloatFrame(test::FuzzDataHelper* fuzz_data,
                        int input_rate,
                        int num_channels,
                        float* const* float_frames) {
  const int samples_per_input_channel =
      AudioProcessing::GetFrameSize(input_rate);
  RTC_DCHECK_LE(samples_per_input_channel, 480);
  for (int i = 0; i < num_channels; ++i) {
    std::fill(float_frames[i], float_frames[i] + samples_per_input_channel, 0);
    const size_t read_bytes = sizeof(float) * samples_per_input_channel;
    if (fuzz_data->CanReadBytes(read_bytes)) {
      rtc::ArrayView<const uint8_t> byte_array =
          fuzz_data->ReadByteArray(read_bytes);
      memmove(float_frames[i], byte_array.begin(), read_bytes);
    }

    // Sanitize input.
    for (int j = 0; j < samples_per_input_channel; ++j) {
      if (!ValidForApm(float_frames[i][j])) {
        float_frames[i][j] = 0.f;
      }
    }
  }
}

void GenerateFixedFrame(test::FuzzDataHelper* fuzz_data,
                        int input_rate,
                        int num_channels,
                        AudioFrame* fixed_frame) {
  const int samples_per_input_channel =
      AudioProcessing::GetFrameSize(input_rate);

  fixed_frame->samples_per_channel_ = samples_per_input_channel;
  fixed_frame->sample_rate_hz_ = input_rate;
  fixed_frame->num_channels_ = num_channels;

  RTC_DCHECK_LE(samples_per_input_channel * num_channels,
                AudioFrame::kMaxDataSizeSamples);
  for (int i = 0; i < samples_per_input_channel * num_channels; ++i) {
    fixed_frame->mutable_data()[i] = fuzz_data->ReadOrDefaultValue<int16_t>(0);
  }
}
}  // namespace

void FuzzAudioProcessing(test::FuzzDataHelper* fuzz_data,
                         rtc::scoped_refptr<AudioProcessing> apm) {
  AudioFrame fixed_frame;
  // Normal usage is up to 8 channels. Allowing to fuzz one beyond this allows
  // us to catch implicit assumptions about normal usage.
  constexpr int kMaxNumChannels = 9;
  std::array<std::array<float, 480>, kMaxNumChannels> float_frames;
  std::array<float*, kMaxNumChannels> float_frame_ptrs;
  for (int i = 0; i < kMaxNumChannels; ++i) {
    float_frame_ptrs[i] = float_frames[i].data();
  }
  float* const* ptr_to_float_frames = &float_frame_ptrs[0];

  constexpr int kSampleRatesHz[] = {8000,  11025, 16000, 22050,
                                    32000, 44100, 48000};

  // We may run out of fuzz data in the middle of a loop iteration. In
  // that case, default values will be used for the rest of that
  // iteration.
  while (fuzz_data->CanReadBytes(1)) {
    const bool is_float = fuzz_data->ReadOrDefaultValue(true);
    // Decide input/output rate for this iteration.
    const int input_rate = fuzz_data->SelectOneOf(kSampleRatesHz);
    const int output_rate = fuzz_data->SelectOneOf(kSampleRatesHz);

    const uint8_t stream_delay = fuzz_data->ReadOrDefaultValue<uint8_t>(0);
    // API call needed for AECM to run.
    apm->set_stream_delay_ms(stream_delay);

    const bool key_pressed = fuzz_data->ReadOrDefaultValue(true);
    apm->set_stream_key_pressed(key_pressed);

    // Make the APM call depending on capture/render mode and float /
    // fix interface.
    const bool is_capture = fuzz_data->ReadOrDefaultValue(true);

    // Fill the arrays with audio samples from the data.
    int apm_return_code = AudioProcessing::Error::kNoError;
    if (is_float) {
      const int num_channels =
          fuzz_data->ReadOrDefaultValue<uint8_t>(1) % kMaxNumChannels;

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
      const int num_channels = fuzz_data->ReadOrDefaultValue(true) ? 2 : 1;
      GenerateFixedFrame(fuzz_data, input_rate, num_channels, &fixed_frame);

      if (is_capture) {
        apm_return_code = ProcessAudioFrame(apm.get(), &fixed_frame);
      } else {
        apm_return_code = ProcessReverseAudioFrame(apm.get(), &fixed_frame);
      }
    }

    // Cover stats gathering code paths.
    static_cast<void>(apm->GetStatistics(true /*has_remote_tracks*/));

    RTC_DCHECK_NE(apm_return_code, AudioProcessing::kBadDataLengthError);
  }
}
}  // namespace webrtc
