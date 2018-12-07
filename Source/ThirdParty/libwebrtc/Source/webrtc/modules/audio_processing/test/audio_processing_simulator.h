/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_TEST_AUDIO_PROCESSING_SIMULATOR_H_
#define MODULES_AUDIO_PROCESSING_TEST_AUDIO_PROCESSING_SIMULATOR_H_

#include <algorithm>
#include <fstream>
#include <limits>
#include <memory>
#include <string>

#include "absl/types/optional.h"
#include "common_audio/channel_buffer.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/audio_processing/test/fake_recording_device.h"
#include "modules/audio_processing/test/test_utils.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/timeutils.h"

namespace webrtc {
namespace test {

// Holds all the parameters available for controlling the simulation.
struct SimulationSettings {
  SimulationSettings();
  SimulationSettings(const SimulationSettings&);
  ~SimulationSettings();
  absl::optional<int> stream_delay;
  absl::optional<bool> use_stream_delay;
  absl::optional<int> stream_drift_samples;
  absl::optional<int> output_sample_rate_hz;
  absl::optional<int> output_num_channels;
  absl::optional<int> reverse_output_sample_rate_hz;
  absl::optional<int> reverse_output_num_channels;
  absl::optional<std::string> output_filename;
  absl::optional<std::string> reverse_output_filename;
  absl::optional<std::string> input_filename;
  absl::optional<std::string> reverse_input_filename;
  absl::optional<std::string> artificial_nearend_filename;
  absl::optional<bool> use_aec;
  absl::optional<bool> use_aecm;
  absl::optional<bool> use_ed;  // Residual Echo Detector.
  absl::optional<std::string> ed_graph_output_filename;
  absl::optional<bool> use_agc;
  absl::optional<bool> use_agc2;
  absl::optional<bool> use_pre_amplifier;
  absl::optional<bool> use_hpf;
  absl::optional<bool> use_ns;
  absl::optional<bool> use_ts;
  absl::optional<bool> use_ie;
  absl::optional<bool> use_vad;
  absl::optional<bool> use_le;
  absl::optional<bool> use_all;
  absl::optional<int> aec_suppression_level;
  absl::optional<bool> use_delay_agnostic;
  absl::optional<bool> use_extended_filter;
  absl::optional<bool> use_drift_compensation;
  absl::optional<bool> use_aec3;
  absl::optional<bool> use_experimental_agc;
  absl::optional<bool> use_experimental_agc_agc2_level_estimator;
  absl::optional<bool> experimental_agc_disable_digital_adaptive;
  absl::optional<bool> experimental_agc_analyze_before_aec;
  absl::optional<int> agc_mode;
  absl::optional<int> agc_target_level;
  absl::optional<bool> use_agc_limiter;
  absl::optional<int> agc_compression_gain;
  absl::optional<bool> agc2_use_adaptive_gain;
  float agc2_fixed_gain_db;
  AudioProcessing::Config::GainController2::LevelEstimator
      agc2_adaptive_level_estimator;
  float pre_amplifier_gain_factor;
  absl::optional<int> vad_likelihood;
  absl::optional<int> ns_level;
  absl::optional<bool> use_refined_adaptive_filter;
  int initial_mic_level;
  bool simulate_mic_gain = false;
  absl::optional<int> simulated_mic_kind;
  bool report_performance = false;
  bool report_bitexactness = false;
  bool use_verbose_logging = false;
  bool use_quiet_output = false;
  bool discard_all_settings_in_aecdump = true;
  absl::optional<std::string> aec_dump_input_filename;
  absl::optional<std::string> aec_dump_output_filename;
  bool fixed_interface = false;
  bool store_intermediate_output = false;
  bool print_aec3_parameter_values = false;
  bool dump_internal_data = false;
  absl::optional<std::string> dump_internal_data_output_dir;
  absl::optional<std::string> custom_call_order_filename;
  absl::optional<std::string> aec3_settings_filename;
};

// Holds a few statistics about a series of TickIntervals.
struct TickIntervalStats {
  TickIntervalStats() : min(std::numeric_limits<int64_t>::max()) {}
  int64_t sum;
  int64_t max;
  int64_t min;
};

// Copies samples present in a ChannelBuffer into an AudioFrame.
void CopyToAudioFrame(const ChannelBuffer<float>& src, AudioFrame* dest);

// Provides common functionality for performing audioprocessing simulations.
class AudioProcessingSimulator {
 public:
  static const int kChunksPerSecond = 1000 / AudioProcessing::kChunkSizeMs;

  AudioProcessingSimulator(const SimulationSettings& settings,
                           std::unique_ptr<AudioProcessingBuilder> ap_builder);
  virtual ~AudioProcessingSimulator();

  // Processes the data in the input.
  virtual void Process() = 0;

  // Returns the execution time of all AudioProcessing calls.
  const TickIntervalStats& proc_time() const { return proc_time_; }

  // Reports whether the processed recording was bitexact.
  bool OutputWasBitexact() { return bitexact_output_; }

  size_t get_num_process_stream_calls() { return num_process_stream_calls_; }
  size_t get_num_reverse_process_stream_calls() {
    return num_reverse_process_stream_calls_;
  }

 protected:
  // RAII class for execution time measurement. Updates the provided
  // TickIntervalStats based on the time between ScopedTimer creation and
  // leaving the enclosing scope.
  class ScopedTimer {
   public:
    explicit ScopedTimer(TickIntervalStats* proc_time)
        : proc_time_(proc_time), start_time_(rtc::TimeNanos()) {}

    ~ScopedTimer();

   private:
    TickIntervalStats* const proc_time_;
    int64_t start_time_;
  };

  TickIntervalStats* mutable_proc_time() { return &proc_time_; }
  void ProcessStream(bool fixed_interface);
  void ProcessReverseStream(bool fixed_interface);
  void CreateAudioProcessor();
  void DestroyAudioProcessor();
  void SetupBuffersConfigsOutputs(int input_sample_rate_hz,
                                  int output_sample_rate_hz,
                                  int reverse_input_sample_rate_hz,
                                  int reverse_output_sample_rate_hz,
                                  int input_num_channels,
                                  int output_num_channels,
                                  int reverse_input_num_channels,
                                  int reverse_output_num_channels);

  const SimulationSettings settings_;
  std::unique_ptr<AudioProcessing> ap_;
  std::unique_ptr<AudioProcessingBuilder> ap_builder_;

  std::unique_ptr<ChannelBuffer<float>> in_buf_;
  std::unique_ptr<ChannelBuffer<float>> out_buf_;
  std::unique_ptr<ChannelBuffer<float>> reverse_in_buf_;
  std::unique_ptr<ChannelBuffer<float>> reverse_out_buf_;
  StreamConfig in_config_;
  StreamConfig out_config_;
  StreamConfig reverse_in_config_;
  StreamConfig reverse_out_config_;
  std::unique_ptr<ChannelBufferWavReader> buffer_reader_;
  std::unique_ptr<ChannelBufferWavReader> reverse_buffer_reader_;
  AudioFrame rev_frame_;
  AudioFrame fwd_frame_;
  bool bitexact_output_ = true;
  int aec_dump_mic_level_ = 0;

 private:
  void SetupOutput();

  size_t num_process_stream_calls_ = 0;
  size_t num_reverse_process_stream_calls_ = 0;
  size_t output_reset_counter_ = 0;
  std::unique_ptr<ChannelBufferWavWriter> buffer_writer_;
  std::unique_ptr<ChannelBufferWavWriter> reverse_buffer_writer_;
  TickIntervalStats proc_time_;
  std::ofstream residual_echo_likelihood_graph_writer_;
  int analog_mic_level_;
  FakeRecordingDevice fake_recording_device_;

  rtc::TaskQueue worker_queue_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(AudioProcessingSimulator);
};

}  // namespace test
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_TEST_AUDIO_PROCESSING_SIMULATOR_H_
