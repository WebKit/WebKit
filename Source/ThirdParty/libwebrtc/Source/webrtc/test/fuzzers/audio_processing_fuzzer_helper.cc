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
#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace {
bool ValidForApm(float x) {
  return std::isfinite(x) && -1.0f <= x && x <= 1.0f;
}

void GenerateFloatFrame(test::FuzzDataHelper* fuzz_data,
                        size_t input_rate,
                        size_t num_channels,
                        float* const* float_frames) {
  const size_t samples_per_input_channel =
      rtc::CheckedDivExact(input_rate, static_cast<size_t>(100));
  RTC_DCHECK_LE(samples_per_input_channel, 480);
  for (size_t i = 0; i < num_channels; ++i) {
    std::fill(float_frames[i], float_frames[i] + samples_per_input_channel, 0);
    const size_t read_bytes = sizeof(float) * samples_per_input_channel;
    if (fuzz_data->CanReadBytes(read_bytes)) {
      rtc::ArrayView<const uint8_t> byte_array =
          fuzz_data->ReadByteArray(read_bytes);
      memmove(float_frames[i], byte_array.begin(), read_bytes);
    }

    // Sanitize input.
    for (size_t j = 0; j < samples_per_input_channel; ++j) {
      if (!ValidForApm(float_frames[i][j])) {
        float_frames[i][j] = 0.f;
      }
    }
  }
}

void GenerateFixedFrame(test::FuzzDataHelper* fuzz_data,
                        size_t input_rate,
                        size_t num_channels,
                        AudioFrame* fixed_frame) {
  const size_t samples_per_input_channel =
      rtc::CheckedDivExact(input_rate, static_cast<size_t>(100));
  fixed_frame->samples_per_channel_ = samples_per_input_channel;
  fixed_frame->sample_rate_hz_ = input_rate;
  fixed_frame->num_channels_ = num_channels;

  RTC_DCHECK_LE(samples_per_input_channel * num_channels,
                AudioFrame::kMaxDataSizeSamples);
  for (size_t i = 0; i < samples_per_input_channel * num_channels; ++i) {
    fixed_frame->mutable_data()[i] = fuzz_data->ReadOrDefaultValue<int16_t>(0);
  }
}
}  // namespace

void FuzzAudioProcessing(test::FuzzDataHelper* fuzz_data,
                         std::unique_ptr<AudioProcessing> apm) {
  AudioFrame fixed_frame;
  std::array<float, 480> float_frame1;
  std::array<float, 480> float_frame2;
  std::array<float* const, 2> float_frame_ptrs = {
      &float_frame1[0], &float_frame2[0],
  };
  float* const* ptr_to_float_frames = &float_frame_ptrs[0];

  using Rate = AudioProcessing::NativeRate;
  const Rate rate_kinds[] = {Rate::kSampleRate8kHz, Rate::kSampleRate16kHz,
                             Rate::kSampleRate32kHz, Rate::kSampleRate48kHz};

  // We may run out of fuzz data in the middle of a loop iteration. In
  // that case, default values will be used for the rest of that
  // iteration.
  while (fuzz_data->CanReadBytes(1)) {
    const bool is_float = fuzz_data->ReadOrDefaultValue(true);
    // Decide input/output rate for this iteration.
    const auto input_rate =
        static_cast<size_t>(fuzz_data->SelectOneOf(rate_kinds));
    const auto output_rate =
        static_cast<size_t>(fuzz_data->SelectOneOf(rate_kinds));

    const int num_channels = fuzz_data->ReadOrDefaultValue(true) ? 2 : 1;
    const uint8_t stream_delay = fuzz_data->ReadOrDefaultValue<uint8_t>(0);

    // API call needed for AEC-2 and AEC-m to run.
    apm->set_stream_delay_ms(stream_delay);

    const bool key_pressed = fuzz_data->ReadOrDefaultValue(true);
    apm->set_stream_key_pressed(key_pressed);

    // Make the APM call depending on capture/render mode and float /
    // fix interface.
    const bool is_capture = fuzz_data->ReadOrDefaultValue(true);

    // Fill the arrays with audio samples from the data.
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
            ptr_to_float_frames, StreamConfig(input_rate, 1),
            StreamConfig(output_rate, 1), ptr_to_float_frames);
      }
    } else {
      GenerateFixedFrame(fuzz_data, input_rate, num_channels, &fixed_frame);

      if (is_capture) {
        apm_return_code = apm->ProcessStream(&fixed_frame);
      } else {
        apm_return_code = apm->ProcessReverseStream(&fixed_frame);
      }
    }

    // Make calls to stats gathering functions to cover these
    // codeways.
    static_cast<void>(apm->GetStatistics());
    static_cast<void>(apm->GetStatistics(true));
    static_cast<void>(apm->UpdateHistogramsOnCallEnd());

    RTC_DCHECK_NE(apm_return_code, AudioProcessing::kBadDataLengthError);
  }
}
}  // namespace webrtc
