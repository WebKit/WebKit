/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/test/simulator_buffers.h"

#include "webrtc/base/checks.h"
#include "webrtc/modules/audio_processing/test/audio_buffer_tools.h"

namespace webrtc {
namespace test {

SimulatorBuffers::SimulatorBuffers(int render_input_sample_rate_hz,
                                   int capture_input_sample_rate_hz,
                                   int render_output_sample_rate_hz,
                                   int capture_output_sample_rate_hz,
                                   size_t num_render_input_channels,
                                   size_t num_capture_input_channels,
                                   size_t num_render_output_channels,
                                   size_t num_capture_output_channels) {
  Random rand_gen(42);
  CreateConfigAndBuffer(render_input_sample_rate_hz, num_render_input_channels,
                        &rand_gen, &render_input_buffer, &render_input_config,
                        &render_input, &render_input_samples);

  CreateConfigAndBuffer(render_output_sample_rate_hz,
                        num_render_output_channels, &rand_gen,
                        &render_output_buffer, &render_output_config,
                        &render_output, &render_output_samples);

  CreateConfigAndBuffer(capture_input_sample_rate_hz,
                        num_capture_input_channels, &rand_gen,
                        &capture_input_buffer, &capture_input_config,
                        &capture_input, &capture_input_samples);

  CreateConfigAndBuffer(capture_output_sample_rate_hz,
                        num_capture_output_channels, &rand_gen,
                        &capture_output_buffer, &capture_output_config,
                        &capture_output, &capture_output_samples);

  UpdateInputBuffers();
}

SimulatorBuffers::~SimulatorBuffers() = default;

void SimulatorBuffers::CreateConfigAndBuffer(
    int sample_rate_hz,
    size_t num_channels,
    Random* rand_gen,
    std::unique_ptr<AudioBuffer>* buffer,
    StreamConfig* config,
    std::vector<float*>* buffer_data,
    std::vector<float>* buffer_data_samples) {
  int samples_per_channel = rtc::CheckedDivExact(sample_rate_hz, 100);
  *config = StreamConfig(sample_rate_hz, num_channels, false);
  buffer->reset(new AudioBuffer(config->num_frames(), config->num_channels(),
                                config->num_frames(), config->num_channels(),
                                config->num_frames()));

  buffer_data_samples->resize(samples_per_channel * num_channels);
  for (auto& v : *buffer_data_samples) {
    v = rand_gen->Rand<float>();
  }

  buffer_data->resize(num_channels);
  for (size_t ch = 0; ch < num_channels; ++ch) {
    (*buffer_data)[ch] = &(*buffer_data_samples)[ch * samples_per_channel];
  }
}

void SimulatorBuffers::UpdateInputBuffers() {
  test::CopyVectorToAudioBuffer(capture_input_config, capture_input_samples,
                                capture_input_buffer.get());
  test::CopyVectorToAudioBuffer(render_input_config, render_input_samples,
                                render_input_buffer.get());
}

}  // namespace test
}  // namespace webrtc
