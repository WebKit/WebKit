/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/fuzzers/audio_processing_fuzzer.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "modules/audio_processing/include/audio_processing.h"
#include "modules/include/module_common_types.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace {
size_t ByteToNativeRate(uint8_t data) {
  using Rate = AudioProcessing::NativeRate;
  switch (data % 4) {
    case 0:
      return static_cast<size_t>(Rate::kSampleRate8kHz);
    case 1:
      return static_cast<size_t>(Rate::kSampleRate16kHz);
    case 2:
      return static_cast<size_t>(Rate::kSampleRate32kHz);
    default:
      return static_cast<size_t>(Rate::kSampleRate48kHz);
  }
}

template <class T>
bool ParseSequence(size_t size,
                   const uint8_t** data,
                   size_t* remaining_size,
                   T* result_data) {
  const size_t data_size_bytes = sizeof(T) * size;
  if (data_size_bytes > *remaining_size) {
    return false;
  }

  std::copy(*data, *data + data_size_bytes,
            reinterpret_cast<uint8_t*>(result_data));

  *data += data_size_bytes;
  *remaining_size -= data_size_bytes;
  return true;
}

void FuzzAudioProcessing(const uint8_t* data,
                         size_t size,
                         bool is_float,
                         AudioProcessing* apm) {
  AudioFrame fixed_frame;
  std::array<float, 480> float_frame{};
  float* const first_channel = &float_frame[0];

  while (size > 0) {
    // Decide input/output rate for this iteration.
    const auto input_rate_byte = ParseByte(&data, &size);
    const auto output_rate_byte = ParseByte(&data, &size);
    if (!input_rate_byte || !output_rate_byte) {
      return;
    }
    const auto input_rate_hz = ByteToNativeRate(*input_rate_byte);
    const auto output_rate_hz = ByteToNativeRate(*output_rate_byte);

    const size_t samples_per_input_channel =
        rtc::CheckedDivExact(input_rate_hz, 100ul);
    fixed_frame.samples_per_channel_ = samples_per_input_channel;
    fixed_frame.sample_rate_hz_ = input_rate_hz;

    // Two channels breaks AEC3.
    fixed_frame.num_channels_ = 1;

    // Fill the arrays with audio samples from the data.
    if (is_float) {
      if (!ParseSequence(samples_per_input_channel, &data, &size,
                         &float_frame[0])) {
        return;
      }
    } else if (!ParseSequence(samples_per_input_channel, &data, &size,
                              fixed_frame.mutable_data())) {
      return;
    }

    // Filter obviously wrong values like inf/nan and values that will
    // lead to inf/nan in calculations. 1e6 leads to DCHECKS failing.
    for (auto& x : float_frame) {
      if (!std::isnormal(x) || std::abs(x) > 1e5) {
        x = 0;
      }
    }

    // Make the APM call depending on capture/render mode and float /
    // fix interface.
    const auto is_capture = ParseBool(&data, &size);
    if (!is_capture) {
      return;
    }
    if (*is_capture) {
      auto apm_return_code =
          is_float ? (apm->ProcessStream(
                         &first_channel, StreamConfig(input_rate_hz, 1),
                         StreamConfig(output_rate_hz, 1), &first_channel))
                   : (apm->ProcessStream(&fixed_frame));
      RTC_DCHECK_NE(apm_return_code, AudioProcessing::kBadDataLengthError);
    } else {
      auto apm_return_code =
          is_float ? (apm->ProcessReverseStream(
                         &first_channel, StreamConfig(input_rate_hz, 1),
                         StreamConfig(output_rate_hz, 1), &first_channel))
                   : (apm->ProcessReverseStream(&fixed_frame));
      RTC_DCHECK_NE(apm_return_code, AudioProcessing::kBadDataLengthError);
    }
  }
}

}  // namespace

rtc::Optional<bool> ParseBool(const uint8_t** data, size_t* remaining_size) {
  if (1 > *remaining_size) {
    return rtc::nullopt;
  }
  auto res = (**data) % 2;
  *data += 1;
  *remaining_size -= 1;
  return res;
}

rtc::Optional<uint8_t> ParseByte(const uint8_t** data, size_t* remaining_size) {
  if (1 > *remaining_size) {
    return rtc::nullopt;
  }
  auto res = **data;
  *data += 1;
  *remaining_size -= 1;
  return res;
}

void FuzzAudioProcessing(const uint8_t* data,
                         size_t size,
                         std::unique_ptr<AudioProcessing> apm) {
  const auto is_float = ParseBool(&data, &size);
  if (!is_float) {
    return;
  }

  FuzzAudioProcessing(data, size, *is_float, apm.get());
}
}  // namespace webrtc
