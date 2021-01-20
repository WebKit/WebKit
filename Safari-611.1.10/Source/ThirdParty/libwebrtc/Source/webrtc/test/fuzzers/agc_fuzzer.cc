/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "modules/audio_processing/audio_buffer.h"
#include "modules/audio_processing/gain_control_impl.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/numerics/safe_minmax.h"
#include "rtc_base/thread_annotations.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {
namespace {

void FillAudioBuffer(size_t sample_rate_hz,
                     test::FuzzDataHelper* fuzz_data,
                     AudioBuffer* buffer) {
  float* const* channels = buffer->channels_f();
  for (size_t i = 0; i < buffer->num_channels(); ++i) {
    for (size_t j = 0; j < buffer->num_frames(); ++j) {
      channels[i][j] =
          static_cast<float>(fuzz_data->ReadOrDefaultValue<int16_t>(0));
    }
  }

  if (sample_rate_hz != 16000) {
    buffer->SplitIntoFrequencyBands();
  }
}

// This function calls the GainControl functions that are overriden as private
// in GainControlInterface.
void FuzzGainControllerConfig(test::FuzzDataHelper* fuzz_data,
                              GainControl* gc) {
  GainControl::Mode modes[] = {GainControl::Mode::kAdaptiveAnalog,
                               GainControl::Mode::kAdaptiveDigital,
                               GainControl::Mode::kFixedDigital};
  GainControl::Mode mode = fuzz_data->SelectOneOf(modes);
  const bool enable_limiter = fuzz_data->ReadOrDefaultValue(true);
  // The values are capped to comply with the API of webrtc::GainControl.
  const int analog_level_min =
      rtc::SafeClamp<int>(fuzz_data->ReadOrDefaultValue<uint16_t>(0), 0, 65534);
  const int analog_level_max =
      rtc::SafeClamp<int>(fuzz_data->ReadOrDefaultValue<uint16_t>(65535),
                          analog_level_min + 1, 65535);
  const int stream_analog_level =
      rtc::SafeClamp<int>(fuzz_data->ReadOrDefaultValue<uint16_t>(30000),
                          analog_level_min, analog_level_max);
  const int gain =
      rtc::SafeClamp<int>(fuzz_data->ReadOrDefaultValue<int8_t>(30), -1, 100);
  const int target_level_dbfs =
      rtc::SafeClamp<int>(fuzz_data->ReadOrDefaultValue<int8_t>(15), -1, 35);

  gc->set_mode(mode);
  gc->enable_limiter(enable_limiter);
  if (mode == GainControl::Mode::kAdaptiveAnalog) {
    gc->set_analog_level_limits(analog_level_min, analog_level_max);
    gc->set_stream_analog_level(stream_analog_level);
  }
  gc->set_compression_gain_db(gain);
  gc->set_target_level_dbfs(target_level_dbfs);

  static_cast<void>(gc->mode());
  static_cast<void>(gc->analog_level_minimum());
  static_cast<void>(gc->analog_level_maximum());
  static_cast<void>(gc->stream_analog_level());
  static_cast<void>(gc->compression_gain_db());
  static_cast<void>(gc->stream_is_saturated());
  static_cast<void>(gc->target_level_dbfs());
  static_cast<void>(gc->is_limiter_enabled());
}

void FuzzGainController(test::FuzzDataHelper* fuzz_data, GainControlImpl* gci) {
  using Rate = ::webrtc::AudioProcessing::NativeRate;
  const Rate rate_kinds[] = {Rate::kSampleRate16kHz, Rate::kSampleRate32kHz,
                             Rate::kSampleRate48kHz};

  const auto sample_rate_hz =
      static_cast<size_t>(fuzz_data->SelectOneOf(rate_kinds));
  const size_t samples_per_frame = sample_rate_hz / 100;
  const size_t num_channels = fuzz_data->ReadOrDefaultValue(true) ? 2 : 1;

  gci->Initialize(num_channels, sample_rate_hz);
  FuzzGainControllerConfig(fuzz_data, gci);

  // The audio buffer is used for both capture and render.
  AudioBuffer audio(samples_per_frame, num_channels, samples_per_frame,
                    num_channels, samples_per_frame);

  std::vector<int16_t> packed_render_audio(samples_per_frame);

  while (fuzz_data->CanReadBytes(1)) {
    FillAudioBuffer(sample_rate_hz, fuzz_data, &audio);

    const bool stream_has_echo = fuzz_data->ReadOrDefaultValue(true);
    gci->AnalyzeCaptureAudio(audio);
    gci->ProcessCaptureAudio(&audio, stream_has_echo);

    FillAudioBuffer(sample_rate_hz, fuzz_data, &audio);

    gci->PackRenderAudioBuffer(audio, &packed_render_audio);
    gci->ProcessRenderAudio(packed_render_audio);
  }
}

}  // namespace

void FuzzOneInput(const uint8_t* data, size_t size) {
  if (size > 200000) {
    return;
  }
  test::FuzzDataHelper fuzz_data(rtc::ArrayView<const uint8_t>(data, size));
  auto gci = std::make_unique<GainControlImpl>();
  FuzzGainController(&fuzz_data, gci.get());
}
}  // namespace webrtc
