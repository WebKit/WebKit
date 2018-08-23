/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/test/audio_processing_simulator.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "api/audio/echo_canceller3_factory.h"
#include "common_audio/include/audio_util.h"
#include "modules/audio_processing/aec_dump/aec_dump_factory.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/audio_processing/test/fake_recording_device.h"
#include "rtc_base/checks.h"
#include "rtc_base/json.h"
#include "rtc_base/logging.h"
#include "rtc_base/stringutils.h"

namespace webrtc {
namespace test {
namespace {

void ReadParam(const Json::Value& root, std::string param_name, bool* param) {
  RTC_CHECK(param);
  bool v;
  if (rtc::GetBoolFromJsonObject(root, param_name, &v)) {
    *param = v;
    std::cout << param_name << ":" << (*param ? "true" : "false") << std::endl;
  }
}

void ReadParam(const Json::Value& root, std::string param_name, size_t* param) {
  RTC_CHECK(param);
  int v;
  if (rtc::GetIntFromJsonObject(root, param_name, &v)) {
    *param = v;
    std::cout << param_name << ":" << *param << std::endl;
  }
}

void ReadParam(const Json::Value& root, std::string param_name, int* param) {
  RTC_CHECK(param);
  int v;
  if (rtc::GetIntFromJsonObject(root, param_name, &v)) {
    *param = v;
    std::cout << param_name << ":" << *param << std::endl;
  }
}

void ReadParam(const Json::Value& root, std::string param_name, float* param) {
  RTC_CHECK(param);
  double v;
  if (rtc::GetDoubleFromJsonObject(root, param_name, &v)) {
    *param = static_cast<float>(v);
    std::cout << param_name << ":" << *param << std::endl;
  }
}

void ReadParam(const Json::Value& root,
               std::string param_name,
               EchoCanceller3Config::Filter::MainConfiguration* param) {
  RTC_CHECK(param);
  Json::Value json_array;
  if (rtc::GetValueFromJsonObject(root, param_name, &json_array)) {
    std::vector<double> v;
    rtc::JsonArrayToDoubleVector(json_array, &v);
    if (v.size() != 5) {
      std::cout << "Incorrect array size for " << param_name << std::endl;
      RTC_CHECK(false);
    }
    param->length_blocks = static_cast<size_t>(v[0]);
    param->leakage_converged = static_cast<float>(v[1]);
    param->leakage_diverged = static_cast<float>(v[2]);
    param->error_floor = static_cast<float>(v[3]);
    param->noise_gate = static_cast<float>(v[4]);

    std::cout << param_name << ":"
              << "[" << param->length_blocks << "," << param->leakage_converged
              << "," << param->leakage_diverged << "," << param->error_floor
              << "," << param->noise_gate << "]" << std::endl;
  }
}

void ReadParam(const Json::Value& root,
               std::string param_name,
               EchoCanceller3Config::Filter::ShadowConfiguration* param) {
  RTC_CHECK(param);
  Json::Value json_array;
  if (rtc::GetValueFromJsonObject(root, param_name, &json_array)) {
    std::vector<double> v;
    rtc::JsonArrayToDoubleVector(json_array, &v);
    if (v.size() != 3) {
      std::cout << "Incorrect array size for " << param_name << std::endl;
      RTC_CHECK(false);
    }
    param->length_blocks = static_cast<size_t>(v[0]);
    param->rate = static_cast<float>(v[1]);
    param->noise_gate = static_cast<float>(v[2]);
    std::cout << param_name << ":"
              << "[" << param->length_blocks << "," << param->rate << ","
              << param->noise_gate << "]" << std::endl;
  }
}

void ReadParam(const Json::Value& root,
               std::string param_name,
               EchoCanceller3Config::GainUpdates::GainChanges* param) {
  RTC_CHECK(param);
  Json::Value json_array;
  if (rtc::GetValueFromJsonObject(root, param_name, &json_array)) {
    std::vector<double> v;
    rtc::JsonArrayToDoubleVector(json_array, &v);
    if (v.size() != 6) {
      std::cout << "Incorrect array size for " << param_name << std::endl;
      RTC_CHECK(false);
    }
    param->max_inc = static_cast<float>(v[0]);
    param->max_dec = static_cast<float>(v[1]);
    param->rate_inc = static_cast<float>(v[2]);
    param->rate_dec = static_cast<float>(v[3]);
    param->min_inc = static_cast<float>(v[4]);
    param->min_dec = static_cast<float>(v[5]);

    std::cout << param_name << ":"
              << "[" << param->max_inc << "," << param->max_dec << ","
              << param->rate_inc << "," << param->rate_dec << ","
              << param->min_inc << "," << param->min_dec << "]" << std::endl;
  }
}

void ReadParam(const Json::Value& root,
               std::string param_name,
               EchoCanceller3Config::Suppressor::MaskingThresholds* param) {
  RTC_CHECK(param);
  Json::Value json_array;
  if (rtc::GetValueFromJsonObject(root, param_name, &json_array)) {
    std::vector<double> v;
    rtc::JsonArrayToDoubleVector(json_array, &v);
    if (v.size() != 3) {
      std::cout << "Incorrect array size for " << param_name << std::endl;
      RTC_CHECK(false);
    }
    param->enr_transparent = static_cast<float>(v[0]);
    param->enr_suppress = static_cast<float>(v[1]);
    param->emr_transparent = static_cast<float>(v[2]);

    std::cout << param_name << ":"
              << "[" << param->enr_transparent << "," << param->enr_suppress
              << "," << param->emr_transparent << "]" << std::endl;
  }
}

EchoCanceller3Config ParseAec3Parameters(const std::string& filename) {
  EchoCanceller3Config cfg;
  Json::Value root;
  std::string s;
  std::string json_string;
  std::ifstream f(filename.c_str());

  if (f.fail()) {
    std::cout << "Failed to open the file " << filename << std::endl;
    RTC_CHECK(false);
  }

  while (std::getline(f, s)) {
    json_string += s;
  }
  bool success = Json::Reader().parse(json_string, root);
  if (!success) {
    std::cout << "Incorrect JSON format:" << std::endl;
    std::cout << json_string << std::endl;
    RTC_CHECK(false);
  }

  std::cout << "AEC3 Parameters from JSON input:" << std::endl;
  Json::Value section;
  if (rtc::GetValueFromJsonObject(root, "delay", &section)) {
    ReadParam(section, "default_delay", &cfg.delay.default_delay);
    ReadParam(section, "down_sampling_factor", &cfg.delay.down_sampling_factor);
    ReadParam(section, "num_filters", &cfg.delay.num_filters);
    ReadParam(section, "api_call_jitter_blocks",
              &cfg.delay.api_call_jitter_blocks);
    ReadParam(section, "min_echo_path_delay_blocks",
              &cfg.delay.min_echo_path_delay_blocks);
    ReadParam(section, "delay_headroom_blocks",
              &cfg.delay.delay_headroom_blocks);
    ReadParam(section, "hysteresis_limit_1_blocks",
              &cfg.delay.hysteresis_limit_1_blocks);
    ReadParam(section, "hysteresis_limit_2_blocks",
              &cfg.delay.hysteresis_limit_2_blocks);
    ReadParam(section, "skew_hysteresis_blocks",
              &cfg.delay.skew_hysteresis_blocks);
  }

  if (rtc::GetValueFromJsonObject(root, "filter", &section)) {
    ReadParam(section, "main", &cfg.filter.main);
    ReadParam(section, "shadow", &cfg.filter.shadow);
    ReadParam(section, "main_initial", &cfg.filter.main_initial);
    ReadParam(section, "shadow_initial", &cfg.filter.shadow_initial);
  }

  if (rtc::GetValueFromJsonObject(root, "erle", &section)) {
    ReadParam(section, "min", &cfg.erle.min);
    ReadParam(section, "max_l", &cfg.erle.max_l);
    ReadParam(section, "max_h", &cfg.erle.max_h);
  }

  if (rtc::GetValueFromJsonObject(root, "ep_strength", &section)) {
    ReadParam(section, "lf", &cfg.ep_strength.lf);
    ReadParam(section, "mf", &cfg.ep_strength.mf);
    ReadParam(section, "hf", &cfg.ep_strength.hf);
    ReadParam(section, "default_len", &cfg.ep_strength.default_len);
    ReadParam(section, "reverb_based_on_render",
              &cfg.ep_strength.reverb_based_on_render);
    ReadParam(section, "echo_can_saturate", &cfg.ep_strength.echo_can_saturate);
    ReadParam(section, "bounded_erl", &cfg.ep_strength.bounded_erl);
  }

  if (rtc::GetValueFromJsonObject(root, "gain_mask", &section)) {
    ReadParam(section, "m1", &cfg.gain_mask.m1);
    ReadParam(section, "m2", &cfg.gain_mask.m2);
    ReadParam(section, "m3", &cfg.gain_mask.m3);
    ReadParam(section, "m5", &cfg.gain_mask.m5);
    ReadParam(section, "m6", &cfg.gain_mask.m6);
    ReadParam(section, "m7", &cfg.gain_mask.m7);
    ReadParam(section, "m8", &cfg.gain_mask.m8);
    ReadParam(section, "m9", &cfg.gain_mask.m9);

    ReadParam(section, "gain_curve_offset", &cfg.gain_mask.gain_curve_offset);
    ReadParam(section, "gain_curve_slope", &cfg.gain_mask.gain_curve_slope);
    ReadParam(section, "temporal_masking_lf",
              &cfg.gain_mask.temporal_masking_lf);
    ReadParam(section, "temporal_masking_hf",
              &cfg.gain_mask.temporal_masking_hf);
    ReadParam(section, "temporal_masking_lf_bands",
              &cfg.gain_mask.temporal_masking_lf_bands);
  }

  if (rtc::GetValueFromJsonObject(root, "echo_audibility", &section)) {
    ReadParam(section, "low_render_limit",
              &cfg.echo_audibility.low_render_limit);
    ReadParam(section, "normal_render_limit",
              &cfg.echo_audibility.normal_render_limit);

    ReadParam(section, "floor_power", &cfg.echo_audibility.floor_power);
    ReadParam(section, "audibility_threshold_lf",
              &cfg.echo_audibility.audibility_threshold_lf);
    ReadParam(section, "audibility_threshold_mf",
              &cfg.echo_audibility.audibility_threshold_mf);
    ReadParam(section, "audibility_threshold_hf",
              &cfg.echo_audibility.audibility_threshold_hf);
    ReadParam(section, "use_stationary_properties",
              &cfg.echo_audibility.use_stationary_properties);
  }

  if (rtc::GetValueFromJsonObject(root, "gain_updates", &section)) {
    ReadParam(section, "low_noise", &cfg.gain_updates.low_noise);
    ReadParam(section, "initial", &cfg.gain_updates.initial);
    ReadParam(section, "normal", &cfg.gain_updates.normal);
    ReadParam(section, "saturation", &cfg.gain_updates.saturation);
    ReadParam(section, "nonlinear", &cfg.gain_updates.nonlinear);
    ReadParam(section, "max_inc_factor", &cfg.gain_updates.max_inc_factor);
    ReadParam(section, "max_dec_factor_lf",
              &cfg.gain_updates.max_dec_factor_lf);
    ReadParam(section, "floor_first_increase",
              &cfg.gain_updates.floor_first_increase);
  }

  if (rtc::GetValueFromJsonObject(root, "echo_removal_control", &section)) {
    Json::Value subsection;
    if (rtc::GetValueFromJsonObject(section, "gain_rampup", &subsection)) {
      ReadParam(subsection, "initial_gain",
                &cfg.echo_removal_control.gain_rampup.initial_gain);
      ReadParam(subsection, "first_non_zero_gain",
                &cfg.echo_removal_control.gain_rampup.first_non_zero_gain);
      ReadParam(subsection, "non_zero_gain_blocks",
                &cfg.echo_removal_control.gain_rampup.non_zero_gain_blocks);
      ReadParam(subsection, "full_gain_blocks",
                &cfg.echo_removal_control.gain_rampup.full_gain_blocks);
    }
    ReadParam(section, "has_clock_drift",
              &cfg.echo_removal_control.has_clock_drift);
    ReadParam(section, "linear_and_stable_echo_path",
              &cfg.echo_removal_control.linear_and_stable_echo_path);
  }

  if (rtc::GetValueFromJsonObject(root, "echo_model", &section)) {
    Json::Value subsection;
    ReadParam(section, "noise_floor_hold", &cfg.echo_model.noise_floor_hold);
    ReadParam(section, "min_noise_floor_power",
              &cfg.echo_model.min_noise_floor_power);
    ReadParam(section, "stationary_gate_slope",
              &cfg.echo_model.stationary_gate_slope);
    ReadParam(section, "noise_gate_power", &cfg.echo_model.noise_gate_power);
    ReadParam(section, "noise_gate_slope", &cfg.echo_model.noise_gate_slope);
    ReadParam(section, "render_pre_window_size",
              &cfg.echo_model.render_pre_window_size);
    ReadParam(section, "render_post_window_size",
              &cfg.echo_model.render_post_window_size);
    ReadParam(section, "render_pre_window_size_init",
              &cfg.echo_model.render_pre_window_size_init);
    ReadParam(section, "render_post_window_size_init",
              &cfg.echo_model.render_post_window_size_init);
    ReadParam(section, "nonlinear_hold", &cfg.echo_model.nonlinear_hold);
    ReadParam(section, "nonlinear_release", &cfg.echo_model.nonlinear_release);
  }

  if (rtc::GetValueFromJsonObject(root, "suppressor", &section)) {
    ReadParam(section, "bands_with_reliable_coherence",
              &cfg.suppressor.bands_with_reliable_coherence);
    ReadParam(section, "nearend_average_blocks",
              &cfg.suppressor.nearend_average_blocks);
    ReadParam(section, "mask_lf", &cfg.suppressor.mask_lf);
    ReadParam(section, "mask_hf", &cfg.suppressor.mask_hf);
  }

  std::cout << std::endl;
  return cfg;
}

void CopyFromAudioFrame(const AudioFrame& src, ChannelBuffer<float>* dest) {
  RTC_CHECK_EQ(src.num_channels_, dest->num_channels());
  RTC_CHECK_EQ(src.samples_per_channel_, dest->num_frames());
  // Copy the data from the input buffer.
  std::vector<float> tmp(src.samples_per_channel_ * src.num_channels_);
  S16ToFloat(src.data(), tmp.size(), tmp.data());
  Deinterleave(tmp.data(), src.samples_per_channel_, src.num_channels_,
               dest->channels());
}

std::string GetIndexedOutputWavFilename(const std::string& wav_name,
                                        int counter) {
  std::stringstream ss;
  ss << wav_name.substr(0, wav_name.size() - 4) << "_" << counter
     << wav_name.substr(wav_name.size() - 4);
  return ss.str();
}

void WriteEchoLikelihoodGraphFileHeader(std::ofstream* output_file) {
  (*output_file) << "import numpy as np" << std::endl
                 << "import matplotlib.pyplot as plt" << std::endl
                 << "y = np.array([";
}

void WriteEchoLikelihoodGraphFileFooter(std::ofstream* output_file) {
  (*output_file) << "])" << std::endl
                 << "if __name__ == '__main__':" << std::endl
                 << "  x = np.arange(len(y))*.01" << std::endl
                 << "  plt.plot(x, y)" << std::endl
                 << "  plt.ylabel('Echo likelihood')" << std::endl
                 << "  plt.xlabel('Time (s)')" << std::endl
                 << "  plt.ylim([0,1])" << std::endl
                 << "  plt.show()" << std::endl;
}

}  // namespace

SimulationSettings::SimulationSettings() = default;
SimulationSettings::SimulationSettings(const SimulationSettings&) = default;
SimulationSettings::~SimulationSettings() = default;

void CopyToAudioFrame(const ChannelBuffer<float>& src, AudioFrame* dest) {
  RTC_CHECK_EQ(src.num_channels(), dest->num_channels_);
  RTC_CHECK_EQ(src.num_frames(), dest->samples_per_channel_);
  int16_t* dest_data = dest->mutable_data();
  for (size_t ch = 0; ch < dest->num_channels_; ++ch) {
    for (size_t sample = 0; sample < dest->samples_per_channel_; ++sample) {
      dest_data[sample * dest->num_channels_ + ch] =
          src.channels()[ch][sample] * 32767;
    }
  }
}

AudioProcessingSimulator::AudioProcessingSimulator(
    const SimulationSettings& settings,
    std::unique_ptr<AudioProcessingBuilder> ap_builder)
    : settings_(settings),
      ap_builder_(ap_builder ? std::move(ap_builder)
                             : absl::make_unique<AudioProcessingBuilder>()),
      analog_mic_level_(settings.initial_mic_level),
      fake_recording_device_(
          settings.initial_mic_level,
          settings_.simulate_mic_gain ? *settings.simulated_mic_kind : 0),
      worker_queue_("file_writer_task_queue") {
  if (settings_.ed_graph_output_filename &&
      !settings_.ed_graph_output_filename->empty()) {
    residual_echo_likelihood_graph_writer_.open(
        *settings_.ed_graph_output_filename);
    RTC_CHECK(residual_echo_likelihood_graph_writer_.is_open());
    WriteEchoLikelihoodGraphFileHeader(&residual_echo_likelihood_graph_writer_);
  }

  if (settings_.simulate_mic_gain)
    RTC_LOG(LS_VERBOSE) << "Simulating analog mic gain";
}

AudioProcessingSimulator::~AudioProcessingSimulator() {
  if (residual_echo_likelihood_graph_writer_.is_open()) {
    WriteEchoLikelihoodGraphFileFooter(&residual_echo_likelihood_graph_writer_);
    residual_echo_likelihood_graph_writer_.close();
  }
}

AudioProcessingSimulator::ScopedTimer::~ScopedTimer() {
  int64_t interval = rtc::TimeNanos() - start_time_;
  proc_time_->sum += interval;
  proc_time_->max = std::max(proc_time_->max, interval);
  proc_time_->min = std::min(proc_time_->min, interval);
}

void AudioProcessingSimulator::ProcessStream(bool fixed_interface) {
  // Optionally use the fake recording device to simulate analog gain.
  if (settings_.simulate_mic_gain) {
    if (settings_.aec_dump_input_filename) {
      // When the analog gain is simulated and an AEC dump is used as input, set
      // the undo level to |aec_dump_mic_level_| to virtually restore the
      // unmodified microphone signal level.
      fake_recording_device_.SetUndoMicLevel(aec_dump_mic_level_);
    }

    if (fixed_interface) {
      fake_recording_device_.SimulateAnalogGain(&fwd_frame_);
    } else {
      fake_recording_device_.SimulateAnalogGain(in_buf_.get());
    }

    // Notify the current mic level to AGC.
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 ap_->gain_control()->set_stream_analog_level(
                     fake_recording_device_.MicLevel()));
  } else {
    // Notify the current mic level to AGC.
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 ap_->gain_control()->set_stream_analog_level(
                     settings_.aec_dump_input_filename ? aec_dump_mic_level_
                                                       : analog_mic_level_));
  }

  // Process the current audio frame.
  if (fixed_interface) {
    {
      const auto st = ScopedTimer(mutable_proc_time());
      RTC_CHECK_EQ(AudioProcessing::kNoError, ap_->ProcessStream(&fwd_frame_));
    }
    CopyFromAudioFrame(fwd_frame_, out_buf_.get());
  } else {
    const auto st = ScopedTimer(mutable_proc_time());
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 ap_->ProcessStream(in_buf_->channels(), in_config_,
                                    out_config_, out_buf_->channels()));
  }

  // Store the mic level suggested by AGC.
  // Note that when the analog gain is simulated and an AEC dump is used as
  // input, |analog_mic_level_| will not be used with set_stream_analog_level().
  analog_mic_level_ = ap_->gain_control()->stream_analog_level();
  if (settings_.simulate_mic_gain) {
    fake_recording_device_.SetMicLevel(analog_mic_level_);
  }

  if (buffer_writer_) {
    buffer_writer_->Write(*out_buf_);
  }

  if (residual_echo_likelihood_graph_writer_.is_open()) {
    auto stats = ap_->GetStatistics();
    residual_echo_likelihood_graph_writer_ << stats.residual_echo_likelihood
                                           << ", ";
  }

  ++num_process_stream_calls_;
}

void AudioProcessingSimulator::ProcessReverseStream(bool fixed_interface) {
  if (fixed_interface) {
    const auto st = ScopedTimer(mutable_proc_time());
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 ap_->ProcessReverseStream(&rev_frame_));
    CopyFromAudioFrame(rev_frame_, reverse_out_buf_.get());

  } else {
    const auto st = ScopedTimer(mutable_proc_time());
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 ap_->ProcessReverseStream(
                     reverse_in_buf_->channels(), reverse_in_config_,
                     reverse_out_config_, reverse_out_buf_->channels()));
  }

  if (reverse_buffer_writer_) {
    reverse_buffer_writer_->Write(*reverse_out_buf_);
  }

  ++num_reverse_process_stream_calls_;
}

void AudioProcessingSimulator::SetupBuffersConfigsOutputs(
    int input_sample_rate_hz,
    int output_sample_rate_hz,
    int reverse_input_sample_rate_hz,
    int reverse_output_sample_rate_hz,
    int input_num_channels,
    int output_num_channels,
    int reverse_input_num_channels,
    int reverse_output_num_channels) {
  in_config_ = StreamConfig(input_sample_rate_hz, input_num_channels);
  in_buf_.reset(new ChannelBuffer<float>(
      rtc::CheckedDivExact(input_sample_rate_hz, kChunksPerSecond),
      input_num_channels));

  reverse_in_config_ =
      StreamConfig(reverse_input_sample_rate_hz, reverse_input_num_channels);
  reverse_in_buf_.reset(new ChannelBuffer<float>(
      rtc::CheckedDivExact(reverse_input_sample_rate_hz, kChunksPerSecond),
      reverse_input_num_channels));

  out_config_ = StreamConfig(output_sample_rate_hz, output_num_channels);
  out_buf_.reset(new ChannelBuffer<float>(
      rtc::CheckedDivExact(output_sample_rate_hz, kChunksPerSecond),
      output_num_channels));

  reverse_out_config_ =
      StreamConfig(reverse_output_sample_rate_hz, reverse_output_num_channels);
  reverse_out_buf_.reset(new ChannelBuffer<float>(
      rtc::CheckedDivExact(reverse_output_sample_rate_hz, kChunksPerSecond),
      reverse_output_num_channels));

  fwd_frame_.sample_rate_hz_ = input_sample_rate_hz;
  fwd_frame_.samples_per_channel_ =
      rtc::CheckedDivExact(fwd_frame_.sample_rate_hz_, kChunksPerSecond);
  fwd_frame_.num_channels_ = input_num_channels;

  rev_frame_.sample_rate_hz_ = reverse_input_sample_rate_hz;
  rev_frame_.samples_per_channel_ =
      rtc::CheckedDivExact(rev_frame_.sample_rate_hz_, kChunksPerSecond);
  rev_frame_.num_channels_ = reverse_input_num_channels;

  if (settings_.use_verbose_logging) {
    rtc::LogMessage::LogToDebug(rtc::LS_VERBOSE);

    std::cout << "Sample rates:" << std::endl;
    std::cout << " Forward input: " << input_sample_rate_hz << std::endl;
    std::cout << " Forward output: " << output_sample_rate_hz << std::endl;
    std::cout << " Reverse input: " << reverse_input_sample_rate_hz
              << std::endl;
    std::cout << " Reverse output: " << reverse_output_sample_rate_hz
              << std::endl;
    std::cout << "Number of channels: " << std::endl;
    std::cout << " Forward input: " << input_num_channels << std::endl;
    std::cout << " Forward output: " << output_num_channels << std::endl;
    std::cout << " Reverse input: " << reverse_input_num_channels << std::endl;
    std::cout << " Reverse output: " << reverse_output_num_channels
              << std::endl;
  }

  SetupOutput();
}

void AudioProcessingSimulator::SetupOutput() {
  if (settings_.output_filename) {
    std::string filename;
    if (settings_.store_intermediate_output) {
      filename = GetIndexedOutputWavFilename(*settings_.output_filename,
                                             output_reset_counter_);
    } else {
      filename = *settings_.output_filename;
    }

    std::unique_ptr<WavWriter> out_file(
        new WavWriter(filename, out_config_.sample_rate_hz(),
                      static_cast<size_t>(out_config_.num_channels())));
    buffer_writer_.reset(new ChannelBufferWavWriter(std::move(out_file)));
  }

  if (settings_.reverse_output_filename) {
    std::string filename;
    if (settings_.store_intermediate_output) {
      filename = GetIndexedOutputWavFilename(*settings_.reverse_output_filename,
                                             output_reset_counter_);
    } else {
      filename = *settings_.reverse_output_filename;
    }

    std::unique_ptr<WavWriter> reverse_out_file(
        new WavWriter(filename, reverse_out_config_.sample_rate_hz(),
                      static_cast<size_t>(reverse_out_config_.num_channels())));
    reverse_buffer_writer_.reset(
        new ChannelBufferWavWriter(std::move(reverse_out_file)));
  }

  ++output_reset_counter_;
}

void AudioProcessingSimulator::DestroyAudioProcessor() {
  if (settings_.aec_dump_output_filename) {
    ap_->DetachAecDump();
  }
}

void AudioProcessingSimulator::CreateAudioProcessor() {
  Config config;
  AudioProcessing::Config apm_config;
  std::unique_ptr<EchoControlFactory> echo_control_factory;
  if (settings_.use_ts) {
    config.Set<ExperimentalNs>(new ExperimentalNs(*settings_.use_ts));
  }
  if (settings_.use_ie) {
    config.Set<Intelligibility>(new Intelligibility(*settings_.use_ie));
  }
  if (settings_.use_agc2) {
    apm_config.gain_controller2.enabled = *settings_.use_agc2;
    apm_config.gain_controller2.fixed_gain_db = settings_.agc2_fixed_gain_db;
  }
  if (settings_.use_pre_amplifier) {
    apm_config.pre_amplifier.enabled = *settings_.use_pre_amplifier;
    apm_config.pre_amplifier.fixed_gain_factor =
        settings_.pre_amplifier_gain_factor;
  }

  if (settings_.use_aec3 && *settings_.use_aec3) {
    EchoCanceller3Config cfg;
    if (settings_.aec3_settings_filename) {
      cfg = ParseAec3Parameters(*settings_.aec3_settings_filename);
    }
    echo_control_factory.reset(new EchoCanceller3Factory(cfg));
  }
  if (settings_.use_hpf) {
    apm_config.high_pass_filter.enabled = *settings_.use_hpf;
  }

  if (settings_.use_refined_adaptive_filter) {
    config.Set<RefinedAdaptiveFilter>(
        new RefinedAdaptiveFilter(*settings_.use_refined_adaptive_filter));
  }
  config.Set<ExtendedFilter>(new ExtendedFilter(
      !settings_.use_extended_filter || *settings_.use_extended_filter));
  config.Set<DelayAgnostic>(new DelayAgnostic(!settings_.use_delay_agnostic ||
                                              *settings_.use_delay_agnostic));
  config.Set<ExperimentalAgc>(new ExperimentalAgc(
      !settings_.use_experimental_agc || *settings_.use_experimental_agc,
      !!settings_.use_experimental_agc_agc2_level_estimator &&
          *settings_.use_experimental_agc_agc2_level_estimator,
      !!settings_.experimental_agc_disable_digital_adaptive &&
          *settings_.experimental_agc_disable_digital_adaptive));
  if (settings_.use_ed) {
    apm_config.residual_echo_detector.enabled = *settings_.use_ed;
  }

  RTC_CHECK(ap_builder_);
  ap_.reset((*ap_builder_)
                .SetEchoControlFactory(std::move(echo_control_factory))
                .Create(config));
  RTC_CHECK(ap_);

  ap_->ApplyConfig(apm_config);

  if (settings_.use_aec) {
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 ap_->echo_cancellation()->Enable(*settings_.use_aec));
  }
  if (settings_.use_aecm) {
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 ap_->echo_control_mobile()->Enable(*settings_.use_aecm));
  }
  if (settings_.use_agc) {
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 ap_->gain_control()->Enable(*settings_.use_agc));
  }
  if (settings_.use_ns) {
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 ap_->noise_suppression()->Enable(*settings_.use_ns));
  }
  if (settings_.use_le) {
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 ap_->level_estimator()->Enable(*settings_.use_le));
  }
  if (settings_.use_vad) {
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 ap_->voice_detection()->Enable(*settings_.use_vad));
  }
  if (settings_.use_agc_limiter) {
    RTC_CHECK_EQ(AudioProcessing::kNoError, ap_->gain_control()->enable_limiter(
                                                *settings_.use_agc_limiter));
  }
  if (settings_.agc_target_level) {
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 ap_->gain_control()->set_target_level_dbfs(
                     *settings_.agc_target_level));
  }
  if (settings_.agc_compression_gain) {
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 ap_->gain_control()->set_compression_gain_db(
                     *settings_.agc_compression_gain));
  }
  if (settings_.agc_mode) {
    RTC_CHECK_EQ(
        AudioProcessing::kNoError,
        ap_->gain_control()->set_mode(
            static_cast<webrtc::GainControl::Mode>(*settings_.agc_mode)));
  }

  if (settings_.use_drift_compensation) {
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 ap_->echo_cancellation()->enable_drift_compensation(
                     *settings_.use_drift_compensation));
  }

  if (settings_.aec_suppression_level) {
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 ap_->echo_cancellation()->set_suppression_level(
                     static_cast<webrtc::EchoCancellation::SuppressionLevel>(
                         *settings_.aec_suppression_level)));
  }

  if (settings_.aecm_routing_mode) {
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 ap_->echo_control_mobile()->set_routing_mode(
                     static_cast<webrtc::EchoControlMobile::RoutingMode>(
                         *settings_.aecm_routing_mode)));
  }

  if (settings_.use_aecm_comfort_noise) {
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 ap_->echo_control_mobile()->enable_comfort_noise(
                     *settings_.use_aecm_comfort_noise));
  }

  if (settings_.vad_likelihood) {
    RTC_CHECK_EQ(AudioProcessing::kNoError,
                 ap_->voice_detection()->set_likelihood(
                     static_cast<webrtc::VoiceDetection::Likelihood>(
                         *settings_.vad_likelihood)));
  }
  if (settings_.ns_level) {
    RTC_CHECK_EQ(
        AudioProcessing::kNoError,
        ap_->noise_suppression()->set_level(
            static_cast<NoiseSuppression::Level>(*settings_.ns_level)));
  }

  if (settings_.use_ts) {
    ap_->set_stream_key_pressed(*settings_.use_ts);
  }

  if (settings_.aec_dump_output_filename) {
    ap_->AttachAecDump(AecDumpFactory::Create(
        *settings_.aec_dump_output_filename, -1, &worker_queue_));
  }
}

}  // namespace test
}  // namespace webrtc
