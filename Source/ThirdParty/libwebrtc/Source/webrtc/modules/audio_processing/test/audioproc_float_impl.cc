/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/test/audioproc_float_impl.h"

#include <string.h>

#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/strings/string_view.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/audio_processing/test/aec_dump_based_simulator.h"
#include "modules/audio_processing/test/audio_processing_simulator.h"
#include "modules/audio_processing/test/wav_based_simulator.h"
#include "rtc_base/checks.h"
#include "rtc_base/strings/string_builder.h"
#include "system_wrappers/include/field_trial.h"

constexpr int kParameterNotSpecifiedValue = -10000;

ABSL_FLAG(std::string, dump_input, "", "Aec dump input filename");
ABSL_FLAG(std::string, dump_output, "", "Aec dump output filename");
ABSL_FLAG(std::string, i, "", "Forward stream input wav filename");
ABSL_FLAG(std::string, o, "", "Forward stream output wav filename");
ABSL_FLAG(std::string, ri, "", "Reverse stream input wav filename");
ABSL_FLAG(std::string, ro, "", "Reverse stream output wav filename");
ABSL_FLAG(std::string,
          artificial_nearend,
          "",
          "Artificial nearend wav filename");
ABSL_FLAG(std::string, linear_aec_output, "", "Linear AEC output wav filename");
ABSL_FLAG(int,
          output_num_channels,
          kParameterNotSpecifiedValue,
          "Number of forward stream output channels");
ABSL_FLAG(int,
          reverse_output_num_channels,
          kParameterNotSpecifiedValue,
          "Number of Reverse stream output channels");
ABSL_FLAG(int,
          output_sample_rate_hz,
          kParameterNotSpecifiedValue,
          "Forward stream output sample rate in Hz");
ABSL_FLAG(int,
          reverse_output_sample_rate_hz,
          kParameterNotSpecifiedValue,
          "Reverse stream output sample rate in Hz");
ABSL_FLAG(bool,
          fixed_interface,
          false,
          "Use the fixed interface when operating on wav files");
ABSL_FLAG(int,
          aec,
          kParameterNotSpecifiedValue,
          "Activate (1) or deactivate (0) the echo canceller");
ABSL_FLAG(int,
          aecm,
          kParameterNotSpecifiedValue,
          "Activate (1) or deactivate (0) the mobile echo controller");
ABSL_FLAG(int,
          ed,
          kParameterNotSpecifiedValue,
          "Activate (1) or deactivate (0) the residual echo detector");
ABSL_FLAG(std::string,
          ed_graph,
          "",
          "Output filename for graph of echo likelihood");
ABSL_FLAG(int,
          agc,
          kParameterNotSpecifiedValue,
          "Activate (1) or deactivate (0) the AGC");
ABSL_FLAG(int,
          agc2,
          kParameterNotSpecifiedValue,
          "Activate (1) or deactivate (0) the AGC2");
ABSL_FLAG(int,
          pre_amplifier,
          kParameterNotSpecifiedValue,
          "Activate (1) or deactivate(0) the pre amplifier");
ABSL_FLAG(
    int,
    capture_level_adjustment,
    kParameterNotSpecifiedValue,
    "Activate (1) or deactivate(0) the capture level adjustment functionality");
ABSL_FLAG(int,
          analog_mic_gain_emulation,
          kParameterNotSpecifiedValue,
          "Activate (1) or deactivate(0) the analog mic gain emulation in the "
          "production (non-test) code.");
ABSL_FLAG(int,
          hpf,
          kParameterNotSpecifiedValue,
          "Activate (1) or deactivate (0) the high-pass filter");
ABSL_FLAG(int,
          ns,
          kParameterNotSpecifiedValue,
          "Activate (1) or deactivate (0) the noise suppressor");
ABSL_FLAG(int,
          ts,
          kParameterNotSpecifiedValue,
          "Activate (1), deactivate (0) or activate the transient suppressor "
          "with continuous key events (2)");
ABSL_FLAG(int,
          analog_agc,
          kParameterNotSpecifiedValue,
          "Activate (1) or deactivate (0) the analog AGC");
ABSL_FLAG(int,
          vad,
          kParameterNotSpecifiedValue,
          "Activate (1) or deactivate (0) the voice activity detector");
ABSL_FLAG(int,
          le,
          kParameterNotSpecifiedValue,
          "Activate (1) or deactivate (0) the level estimator");
ABSL_FLAG(bool,
          all_default,
          false,
          "Activate all of the default components (will be overridden by any "
          "other settings)");
ABSL_FLAG(int,
          analog_agc_disable_digital_adaptive,
          kParameterNotSpecifiedValue,
          "Force-deactivate (1) digital adaptation in "
          "experimental AGC. Digital adaptation is active by default (0).");
ABSL_FLAG(int,
          agc_mode,
          kParameterNotSpecifiedValue,
          "Specify the AGC mode (0-2)");
ABSL_FLAG(int,
          agc_target_level,
          kParameterNotSpecifiedValue,
          "Specify the AGC target level (0-31)");
ABSL_FLAG(int,
          agc_limiter,
          kParameterNotSpecifiedValue,
          "Activate (1) or deactivate (0) the level estimator");
ABSL_FLAG(int,
          agc_compression_gain,
          kParameterNotSpecifiedValue,
          "Specify the AGC compression gain (0-90)");
ABSL_FLAG(int,
          agc2_enable_adaptive_gain,
          kParameterNotSpecifiedValue,
          "Activate (1) or deactivate (0) the AGC2 adaptive gain");
ABSL_FLAG(float,
          agc2_fixed_gain_db,
          kParameterNotSpecifiedValue,
          "AGC2 fixed gain (dB) to apply");
ABSL_FLAG(std::string,
          agc2_adaptive_level_estimator,
          "RMS",
          "AGC2 adaptive digital level estimator to use [RMS, peak]");
ABSL_FLAG(float,
          pre_amplifier_gain_factor,
          kParameterNotSpecifiedValue,
          "Pre-amplifier gain factor (linear) to apply");
ABSL_FLAG(float,
          pre_gain_factor,
          kParameterNotSpecifiedValue,
          "Pre-gain factor (linear) to apply in the capture level adjustment");
ABSL_FLAG(float,
          post_gain_factor,
          kParameterNotSpecifiedValue,
          "Post-gain factor (linear) to apply in the capture level adjustment");
ABSL_FLAG(float,
          analog_mic_gain_emulation_initial_level,
          kParameterNotSpecifiedValue,
          "Emulated analog mic level to apply initially in the production "
          "(non-test) code.");
ABSL_FLAG(int,
          ns_level,
          kParameterNotSpecifiedValue,
          "Specify the NS level (0-3)");
ABSL_FLAG(int,
          ns_analysis_on_linear_aec_output,
          kParameterNotSpecifiedValue,
          "Specifies whether the noise suppression analysis is done on the "
          "linear AEC output");
ABSL_FLAG(int,
          maximum_internal_processing_rate,
          kParameterNotSpecifiedValue,
          "Set a maximum internal processing rate (32000 or 48000) to override "
          "the default rate");
ABSL_FLAG(int,
          stream_delay,
          kParameterNotSpecifiedValue,
          "Specify the stream delay in ms to use");
ABSL_FLAG(int,
          use_stream_delay,
          kParameterNotSpecifiedValue,
          "Activate (1) or deactivate (0) reporting the stream delay");
ABSL_FLAG(int,
          stream_drift_samples,
          kParameterNotSpecifiedValue,
          "Specify the number of stream drift samples to use");
ABSL_FLAG(int,
          initial_mic_level,
          100,
          "Initial mic level (0-255) for the analog mic gain simulation in the "
          "test code");
ABSL_FLAG(int,
          simulate_mic_gain,
          0,
          "Activate (1) or deactivate(0) the analog mic gain simulation in the "
          "test code");
ABSL_FLAG(int,
          multi_channel_render,
          kParameterNotSpecifiedValue,
          "Activate (1) or deactivate (0) multi-channel render processing in "
          "APM pipeline");
ABSL_FLAG(int,
          multi_channel_capture,
          kParameterNotSpecifiedValue,
          "Activate (1) or deactivate (0) multi-channel capture processing in "
          "APM pipeline");
ABSL_FLAG(int,
          simulated_mic_kind,
          kParameterNotSpecifiedValue,
          "Specify which microphone kind to use for microphone simulation");
ABSL_FLAG(int,
          frame_for_sending_capture_output_used_false,
          kParameterNotSpecifiedValue,
          "Capture frame index for sending a runtime setting for that the "
          "capture output is not used.");
ABSL_FLAG(int,
          frame_for_sending_capture_output_used_true,
          kParameterNotSpecifiedValue,
          "Capture frame index for sending a runtime setting for that the "
          "capture output is used.");
ABSL_FLAG(bool, performance_report, false, "Report the APM performance ");
ABSL_FLAG(std::string,
          performance_report_output_file,
          "",
          "Generate a CSV file with the API call durations");
ABSL_FLAG(bool, verbose, false, "Produce verbose output");
ABSL_FLAG(bool,
          quiet,
          false,
          "Avoid producing information about the progress.");
ABSL_FLAG(bool,
          bitexactness_report,
          false,
          "Report bitexactness for aec dump result reproduction");
ABSL_FLAG(bool,
          discard_settings_in_aecdump,
          false,
          "Discard any config settings specified in the aec dump");
ABSL_FLAG(bool,
          store_intermediate_output,
          false,
          "Creates new output files after each init");
ABSL_FLAG(std::string,
          custom_call_order_file,
          "",
          "Custom process API call order file");
ABSL_FLAG(std::string,
          output_custom_call_order_file,
          "",
          "Generate custom process API call order file from AEC dump");
ABSL_FLAG(bool,
          print_aec_parameter_values,
          false,
          "Print parameter values used in AEC in JSON-format");
ABSL_FLAG(std::string,
          aec_settings,
          "",
          "File in JSON-format with custom AEC settings");
ABSL_FLAG(bool,
          dump_data,
          false,
          "Dump internal data during the call (requires build flag)");
ABSL_FLAG(std::string,
          dump_data_output_dir,
          "",
          "Internal data dump output directory");
ABSL_FLAG(int,
          dump_set_to_use,
          kParameterNotSpecifiedValue,
          "Specifies the dump set to use (if not all the dump sets will "
          "be used");
ABSL_FLAG(bool,
          analyze,
          false,
          "Only analyze the call setup behavior (no processing)");
ABSL_FLAG(float,
          dump_start_seconds,
          kParameterNotSpecifiedValue,
          "Start of when to dump data (seconds).");
ABSL_FLAG(float,
          dump_end_seconds,
          kParameterNotSpecifiedValue,
          "End of when to dump data (seconds).");
ABSL_FLAG(int,
          dump_start_frame,
          kParameterNotSpecifiedValue,
          "Start of when to dump data (frames).");
ABSL_FLAG(int,
          dump_end_frame,
          kParameterNotSpecifiedValue,
          "End of when to dump data (frames).");
ABSL_FLAG(int,
          init_to_process,
          kParameterNotSpecifiedValue,
          "Init index to process.");

ABSL_FLAG(bool,
          float_wav_output,
          false,
          "Produce floating point wav output files.");

ABSL_FLAG(std::string,
          force_fieldtrials,
          "",
          "Field trials control experimental feature code which can be forced. "
          "E.g. running with --force_fieldtrials=WebRTC-FooFeature/Enable/"
          " will assign the group Enable to field trial WebRTC-FooFeature.");

namespace webrtc {
namespace test {
namespace {

const char kUsageDescription[] =
    "Usage: audioproc_f [options] -i <input.wav>\n"
    "                   or\n"
    "       audioproc_f [options] -dump_input <aec_dump>\n"
    "\n\n"
    "Command-line tool to simulate a call using the audio "
    "processing module, either based on wav files or "
    "protobuf debug dump recordings.\n";

std::vector<std::string> GetAgc2AdaptiveLevelEstimatorNames() {
  return {"RMS", "peak"};
}

void SetSettingIfSpecified(const std::string& value,
                           absl::optional<std::string>* parameter) {
  if (value.compare("") != 0) {
    *parameter = value;
  }
}

void SetSettingIfSpecified(int value, absl::optional<int>* parameter) {
  if (value != kParameterNotSpecifiedValue) {
    *parameter = value;
  }
}

void SetSettingIfSpecified(float value, absl::optional<float>* parameter) {
  constexpr float kFloatParameterNotSpecifiedValue =
      kParameterNotSpecifiedValue;
  if (value != kFloatParameterNotSpecifiedValue) {
    *parameter = value;
  }
}

void SetSettingIfFlagSet(int32_t flag, absl::optional<bool>* parameter) {
  if (flag == 0) {
    *parameter = false;
  } else if (flag == 1) {
    *parameter = true;
  }
}

AudioProcessing::Config::GainController2::LevelEstimator
MapAgc2AdaptiveLevelEstimator(absl::string_view name) {
  if (name.compare("RMS") == 0) {
    return AudioProcessing::Config::GainController2::LevelEstimator::kRms;
  }
  if (name.compare("peak") == 0) {
    return AudioProcessing::Config::GainController2::LevelEstimator::kPeak;
  }
  auto concat_strings =
      [](const std::vector<std::string>& strings) -> std::string {
    rtc::StringBuilder ss;
    for (const auto& s : strings) {
      ss << " " << s;
    }
    return ss.Release();
  };
  RTC_CHECK(false)
      << "Invalid value for agc2_adaptive_level_estimator, valid options:"
      << concat_strings(GetAgc2AdaptiveLevelEstimatorNames()) << ".";
}

SimulationSettings CreateSettings() {
  SimulationSettings settings;
  if (absl::GetFlag(FLAGS_all_default)) {
    settings.use_le = true;
    settings.use_vad = true;
    settings.use_ts = true;
    settings.use_analog_agc = true;
    settings.use_ns = true;
    settings.use_hpf = true;
    settings.use_agc = true;
    settings.use_agc2 = false;
    settings.use_pre_amplifier = false;
    settings.use_aec = true;
    settings.use_aecm = false;
    settings.use_ed = false;
  }
  SetSettingIfSpecified(absl::GetFlag(FLAGS_dump_input),
                        &settings.aec_dump_input_filename);
  SetSettingIfSpecified(absl::GetFlag(FLAGS_dump_output),
                        &settings.aec_dump_output_filename);
  SetSettingIfSpecified(absl::GetFlag(FLAGS_i), &settings.input_filename);
  SetSettingIfSpecified(absl::GetFlag(FLAGS_o), &settings.output_filename);
  SetSettingIfSpecified(absl::GetFlag(FLAGS_ri),
                        &settings.reverse_input_filename);
  SetSettingIfSpecified(absl::GetFlag(FLAGS_ro),
                        &settings.reverse_output_filename);
  SetSettingIfSpecified(absl::GetFlag(FLAGS_artificial_nearend),
                        &settings.artificial_nearend_filename);
  SetSettingIfSpecified(absl::GetFlag(FLAGS_linear_aec_output),
                        &settings.linear_aec_output_filename);
  SetSettingIfSpecified(absl::GetFlag(FLAGS_output_num_channels),
                        &settings.output_num_channels);
  SetSettingIfSpecified(absl::GetFlag(FLAGS_reverse_output_num_channels),
                        &settings.reverse_output_num_channels);
  SetSettingIfSpecified(absl::GetFlag(FLAGS_output_sample_rate_hz),
                        &settings.output_sample_rate_hz);
  SetSettingIfSpecified(absl::GetFlag(FLAGS_reverse_output_sample_rate_hz),
                        &settings.reverse_output_sample_rate_hz);
  SetSettingIfFlagSet(absl::GetFlag(FLAGS_aec), &settings.use_aec);
  SetSettingIfFlagSet(absl::GetFlag(FLAGS_aecm), &settings.use_aecm);
  SetSettingIfFlagSet(absl::GetFlag(FLAGS_ed), &settings.use_ed);
  SetSettingIfSpecified(absl::GetFlag(FLAGS_ed_graph),
                        &settings.ed_graph_output_filename);
  SetSettingIfFlagSet(absl::GetFlag(FLAGS_agc), &settings.use_agc);
  SetSettingIfFlagSet(absl::GetFlag(FLAGS_agc2), &settings.use_agc2);
  SetSettingIfFlagSet(absl::GetFlag(FLAGS_pre_amplifier),
                      &settings.use_pre_amplifier);
  SetSettingIfFlagSet(absl::GetFlag(FLAGS_capture_level_adjustment),
                      &settings.use_capture_level_adjustment);
  SetSettingIfFlagSet(absl::GetFlag(FLAGS_analog_mic_gain_emulation),
                      &settings.use_analog_mic_gain_emulation);
  SetSettingIfFlagSet(absl::GetFlag(FLAGS_hpf), &settings.use_hpf);
  SetSettingIfFlagSet(absl::GetFlag(FLAGS_ns), &settings.use_ns);
  SetSettingIfSpecified(absl::GetFlag(FLAGS_ts), &settings.use_ts);
  SetSettingIfFlagSet(absl::GetFlag(FLAGS_analog_agc),
                      &settings.use_analog_agc);
  SetSettingIfFlagSet(absl::GetFlag(FLAGS_vad), &settings.use_vad);
  SetSettingIfFlagSet(absl::GetFlag(FLAGS_le), &settings.use_le);
  SetSettingIfFlagSet(absl::GetFlag(FLAGS_analog_agc_disable_digital_adaptive),
                      &settings.analog_agc_disable_digital_adaptive);
  SetSettingIfSpecified(absl::GetFlag(FLAGS_agc_mode), &settings.agc_mode);
  SetSettingIfSpecified(absl::GetFlag(FLAGS_agc_target_level),
                        &settings.agc_target_level);
  SetSettingIfFlagSet(absl::GetFlag(FLAGS_agc_limiter),
                      &settings.use_agc_limiter);
  SetSettingIfSpecified(absl::GetFlag(FLAGS_agc_compression_gain),
                        &settings.agc_compression_gain);
  SetSettingIfFlagSet(absl::GetFlag(FLAGS_agc2_enable_adaptive_gain),
                      &settings.agc2_use_adaptive_gain);

  SetSettingIfSpecified(absl::GetFlag(FLAGS_agc2_fixed_gain_db),
                        &settings.agc2_fixed_gain_db);
  settings.agc2_adaptive_level_estimator = MapAgc2AdaptiveLevelEstimator(
      absl::GetFlag(FLAGS_agc2_adaptive_level_estimator));
  SetSettingIfSpecified(absl::GetFlag(FLAGS_pre_amplifier_gain_factor),
                        &settings.pre_amplifier_gain_factor);
  SetSettingIfSpecified(absl::GetFlag(FLAGS_pre_gain_factor),
                        &settings.pre_gain_factor);
  SetSettingIfSpecified(absl::GetFlag(FLAGS_post_gain_factor),
                        &settings.post_gain_factor);
  SetSettingIfSpecified(
      absl::GetFlag(FLAGS_analog_mic_gain_emulation_initial_level),
      &settings.analog_mic_gain_emulation_initial_level);
  SetSettingIfSpecified(absl::GetFlag(FLAGS_ns_level), &settings.ns_level);
  SetSettingIfFlagSet(absl::GetFlag(FLAGS_ns_analysis_on_linear_aec_output),
                      &settings.ns_analysis_on_linear_aec_output);
  SetSettingIfSpecified(absl::GetFlag(FLAGS_maximum_internal_processing_rate),
                        &settings.maximum_internal_processing_rate);
  SetSettingIfSpecified(absl::GetFlag(FLAGS_stream_delay),
                        &settings.stream_delay);
  SetSettingIfFlagSet(absl::GetFlag(FLAGS_use_stream_delay),
                      &settings.use_stream_delay);
  SetSettingIfSpecified(absl::GetFlag(FLAGS_custom_call_order_file),
                        &settings.call_order_input_filename);
  SetSettingIfSpecified(absl::GetFlag(FLAGS_output_custom_call_order_file),
                        &settings.call_order_output_filename);
  SetSettingIfSpecified(absl::GetFlag(FLAGS_aec_settings),
                        &settings.aec_settings_filename);
  settings.initial_mic_level = absl::GetFlag(FLAGS_initial_mic_level);
  SetSettingIfFlagSet(absl::GetFlag(FLAGS_multi_channel_render),
                      &settings.multi_channel_render);
  SetSettingIfFlagSet(absl::GetFlag(FLAGS_multi_channel_capture),
                      &settings.multi_channel_capture);
  settings.simulate_mic_gain = absl::GetFlag(FLAGS_simulate_mic_gain);
  SetSettingIfSpecified(absl::GetFlag(FLAGS_simulated_mic_kind),
                        &settings.simulated_mic_kind);
  SetSettingIfSpecified(
      absl::GetFlag(FLAGS_frame_for_sending_capture_output_used_false),
      &settings.frame_for_sending_capture_output_used_false);
  SetSettingIfSpecified(
      absl::GetFlag(FLAGS_frame_for_sending_capture_output_used_true),
      &settings.frame_for_sending_capture_output_used_true);
  settings.report_performance = absl::GetFlag(FLAGS_performance_report);
  SetSettingIfSpecified(absl::GetFlag(FLAGS_performance_report_output_file),
                        &settings.performance_report_output_filename);
  settings.use_verbose_logging = absl::GetFlag(FLAGS_verbose);
  settings.use_quiet_output = absl::GetFlag(FLAGS_quiet);
  settings.report_bitexactness = absl::GetFlag(FLAGS_bitexactness_report);
  settings.discard_all_settings_in_aecdump =
      absl::GetFlag(FLAGS_discard_settings_in_aecdump);
  settings.fixed_interface = absl::GetFlag(FLAGS_fixed_interface);
  settings.store_intermediate_output =
      absl::GetFlag(FLAGS_store_intermediate_output);
  settings.print_aec_parameter_values =
      absl::GetFlag(FLAGS_print_aec_parameter_values);
  settings.dump_internal_data = absl::GetFlag(FLAGS_dump_data);
  SetSettingIfSpecified(absl::GetFlag(FLAGS_dump_data_output_dir),
                        &settings.dump_internal_data_output_dir);
  SetSettingIfSpecified(absl::GetFlag(FLAGS_dump_set_to_use),
                        &settings.dump_set_to_use);
  settings.wav_output_format = absl::GetFlag(FLAGS_float_wav_output)
                                   ? WavFile::SampleFormat::kFloat
                                   : WavFile::SampleFormat::kInt16;

  settings.analysis_only = absl::GetFlag(FLAGS_analyze);

  SetSettingIfSpecified(absl::GetFlag(FLAGS_dump_start_frame),
                        &settings.dump_start_frame);
  SetSettingIfSpecified(absl::GetFlag(FLAGS_dump_end_frame),
                        &settings.dump_end_frame);

  constexpr int kFramesPerSecond = 100;
  absl::optional<float> start_seconds;
  SetSettingIfSpecified(absl::GetFlag(FLAGS_dump_start_seconds),
                        &start_seconds);
  if (start_seconds) {
    settings.dump_start_frame = *start_seconds * kFramesPerSecond;
  }

  absl::optional<float> end_seconds;
  SetSettingIfSpecified(absl::GetFlag(FLAGS_dump_end_seconds), &end_seconds);
  if (end_seconds) {
    settings.dump_end_frame = *end_seconds * kFramesPerSecond;
  }

  SetSettingIfSpecified(absl::GetFlag(FLAGS_init_to_process),
                        &settings.init_to_process);

  return settings;
}

void ReportConditionalErrorAndExit(bool condition, const std::string& message) {
  if (condition) {
    std::cerr << message << std::endl;
    exit(1);
  }
}

void PerformBasicParameterSanityChecks(
    const SimulationSettings& settings,
    bool pre_constructed_ap_provided,
    bool pre_constructed_ap_builder_provided) {
  if (settings.input_filename || settings.reverse_input_filename) {
    ReportConditionalErrorAndExit(
        !!settings.aec_dump_input_filename,
        "Error: The aec dump file cannot be specified "
        "together with input wav files!\n");

    ReportConditionalErrorAndExit(
        !!settings.aec_dump_input_string,
        "Error: The aec dump input string cannot be specified "
        "together with input wav files!\n");

    ReportConditionalErrorAndExit(!!settings.artificial_nearend_filename,
                                  "Error: The artificial nearend cannot be "
                                  "specified together with input wav files!\n");

    ReportConditionalErrorAndExit(!settings.input_filename,
                                  "Error: When operating at wav files, the "
                                  "input wav filename must be "
                                  "specified!\n");

    ReportConditionalErrorAndExit(
        settings.reverse_output_filename && !settings.reverse_input_filename,
        "Error: When operating at wav files, the reverse input wav filename "
        "must be specified if the reverse output wav filename is specified!\n");
  } else {
    ReportConditionalErrorAndExit(
        !settings.aec_dump_input_filename && !settings.aec_dump_input_string,
        "Error: Either the aec dump input file, the wav "
        "input file or the aec dump input string must be specified!\n");
    ReportConditionalErrorAndExit(
        settings.aec_dump_input_filename && settings.aec_dump_input_string,
        "Error: The aec dump input file cannot be specified together with the "
        "aec dump input string!\n");
  }

  ReportConditionalErrorAndExit(settings.use_aec && !(*settings.use_aec) &&
                                    settings.linear_aec_output_filename,
                                "Error: The linear AEC ouput filename cannot "
                                "be specified without the AEC being active");

  ReportConditionalErrorAndExit(
      settings.use_aec && *settings.use_aec && settings.use_aecm &&
          *settings.use_aecm,
      "Error: The AEC and the AECM cannot be activated at the same time!\n");

  ReportConditionalErrorAndExit(
      settings.output_sample_rate_hz && *settings.output_sample_rate_hz <= 0,
      "Error: --output_sample_rate_hz must be positive!\n");

  ReportConditionalErrorAndExit(
      settings.reverse_output_sample_rate_hz &&
          settings.output_sample_rate_hz &&
          *settings.output_sample_rate_hz <= 0,
      "Error: --reverse_output_sample_rate_hz must be positive!\n");

  ReportConditionalErrorAndExit(
      settings.output_num_channels && *settings.output_num_channels <= 0,
      "Error: --output_num_channels must be positive!\n");

  ReportConditionalErrorAndExit(
      settings.reverse_output_num_channels &&
          *settings.reverse_output_num_channels <= 0,
      "Error: --reverse_output_num_channels must be positive!\n");

  ReportConditionalErrorAndExit(
      settings.agc_target_level && ((*settings.agc_target_level) < 0 ||
                                    (*settings.agc_target_level) > 31),
      "Error: --agc_target_level must be specified between 0 and 31.\n");

  ReportConditionalErrorAndExit(
      settings.agc_compression_gain && ((*settings.agc_compression_gain) < 0 ||
                                        (*settings.agc_compression_gain) > 90),
      "Error: --agc_compression_gain must be specified between 0 and 90.\n");

  ReportConditionalErrorAndExit(
      settings.agc2_fixed_gain_db && ((*settings.agc2_fixed_gain_db) < 0 ||
                                      (*settings.agc2_fixed_gain_db) > 90),
      "Error: --agc2_fixed_gain_db must be specified between 0 and 90.\n");

  ReportConditionalErrorAndExit(
      settings.ns_level &&
          ((*settings.ns_level) < 0 || (*settings.ns_level) > 3),
      "Error: --ns_level must be specified between 0 and 3.\n");

  ReportConditionalErrorAndExit(
      settings.report_bitexactness && !settings.aec_dump_input_filename,
      "Error: --bitexactness_report can only be used when operating on an "
      "aecdump\n");

  ReportConditionalErrorAndExit(
      settings.call_order_input_filename && settings.aec_dump_input_filename,
      "Error: --custom_call_order_file cannot be used when operating on an "
      "aecdump\n");

  ReportConditionalErrorAndExit(
      (settings.initial_mic_level < 0 || settings.initial_mic_level > 255),
      "Error: --initial_mic_level must be specified between 0 and 255.\n");

  ReportConditionalErrorAndExit(
      settings.simulated_mic_kind && !settings.simulate_mic_gain,
      "Error: --simulated_mic_kind cannot be specified when mic simulation is "
      "disabled\n");

  ReportConditionalErrorAndExit(
      !settings.simulated_mic_kind && settings.simulate_mic_gain,
      "Error: --simulated_mic_kind must be specified when mic simulation is "
      "enabled\n");

  auto valid_wav_name = [](const std::string& wav_file_name) {
    if (wav_file_name.size() < 5) {
      return false;
    }
    if ((wav_file_name.compare(wav_file_name.size() - 4, 4, ".wav") == 0) ||
        (wav_file_name.compare(wav_file_name.size() - 4, 4, ".WAV") == 0)) {
      return true;
    }
    return false;
  };

  ReportConditionalErrorAndExit(
      settings.input_filename && (!valid_wav_name(*settings.input_filename)),
      "Error: --i must be a valid .wav file name.\n");

  ReportConditionalErrorAndExit(
      settings.output_filename && (!valid_wav_name(*settings.output_filename)),
      "Error: --o must be a valid .wav file name.\n");

  ReportConditionalErrorAndExit(
      settings.reverse_input_filename &&
          (!valid_wav_name(*settings.reverse_input_filename)),
      "Error: --ri must be a valid .wav file name.\n");

  ReportConditionalErrorAndExit(
      settings.reverse_output_filename &&
          (!valid_wav_name(*settings.reverse_output_filename)),
      "Error: --ro must be a valid .wav file name.\n");

  ReportConditionalErrorAndExit(
      settings.artificial_nearend_filename &&
          !valid_wav_name(*settings.artificial_nearend_filename),
      "Error: --artifical_nearend must be a valid .wav file name.\n");

  ReportConditionalErrorAndExit(
      settings.linear_aec_output_filename &&
          (!valid_wav_name(*settings.linear_aec_output_filename)),
      "Error: --linear_aec_output must be a valid .wav file name.\n");

  ReportConditionalErrorAndExit(
      WEBRTC_APM_DEBUG_DUMP == 0 && settings.dump_internal_data,
      "Error: --dump_data cannot be set without proper build support.\n");

  ReportConditionalErrorAndExit(settings.init_to_process &&
                                    *settings.init_to_process != 1 &&
                                    !settings.aec_dump_input_filename,
                                "Error: --init_to_process must be set to 1 for "
                                "wav-file based simulations.\n");

  ReportConditionalErrorAndExit(
      !settings.init_to_process &&
          (settings.dump_start_frame || settings.dump_end_frame),
      "Error: --init_to_process must be set when specifying a start and/or end "
      "frame for when to dump internal data.\n");

  ReportConditionalErrorAndExit(
      !settings.dump_internal_data &&
          settings.dump_internal_data_output_dir.has_value(),
      "Error: --dump_data_output_dir cannot be set without --dump_data.\n");

  ReportConditionalErrorAndExit(
      !settings.aec_dump_input_filename &&
          settings.call_order_output_filename.has_value(),
      "Error: --output_custom_call_order_file needs an AEC dump input file.\n");

  ReportConditionalErrorAndExit(
      (!settings.use_pre_amplifier || !(*settings.use_pre_amplifier)) &&
          settings.pre_amplifier_gain_factor.has_value(),
      "Error: --pre_amplifier_gain_factor needs --pre_amplifier to be "
      "specified and set.\n");

  ReportConditionalErrorAndExit(
      pre_constructed_ap_provided && pre_constructed_ap_builder_provided,
      "Error: The AudioProcessing and the AudioProcessingBuilder cannot both "
      "be specified at the same time.\n");

  ReportConditionalErrorAndExit(
      settings.aec_settings_filename && pre_constructed_ap_provided,
      "Error: The aec_settings_filename cannot be specified when a "
      "pre-constructed audio processing object is provided.\n");

  ReportConditionalErrorAndExit(
      settings.aec_settings_filename && pre_constructed_ap_provided,
      "Error: The print_aec_parameter_values cannot be set when a "
      "pre-constructed audio processing object is provided.\n");

  if (settings.linear_aec_output_filename && pre_constructed_ap_provided) {
    std::cout << "Warning: For the linear AEC output to be stored, this must "
                 "be configured in the AEC that is part of the provided "
                 "AudioProcessing object."
              << std::endl;
  }
}

int RunSimulation(rtc::scoped_refptr<AudioProcessing> audio_processing,
                  std::unique_ptr<AudioProcessingBuilder> ap_builder,
                  int argc,
                  char* argv[],
                  absl::string_view input_aecdump,
                  std::vector<float>* processed_capture_samples) {
  std::vector<char*> args = absl::ParseCommandLine(argc, argv);
  if (args.size() != 1) {
    printf("%s", kUsageDescription);
    return 1;
  }
  // InitFieldTrialsFromString stores the char*, so the char array must
  // outlive the application.
  const std::string field_trials = absl::GetFlag(FLAGS_force_fieldtrials);
  webrtc::field_trial::InitFieldTrialsFromString(field_trials.c_str());

  SimulationSettings settings = CreateSettings();
  if (!input_aecdump.empty()) {
    settings.aec_dump_input_string = input_aecdump;
    settings.processed_capture_samples = processed_capture_samples;
    RTC_CHECK(settings.processed_capture_samples);
  }
  PerformBasicParameterSanityChecks(settings, !!audio_processing, !!ap_builder);
  std::unique_ptr<AudioProcessingSimulator> processor;

  if (settings.aec_dump_input_filename || settings.aec_dump_input_string) {
    processor.reset(new AecDumpBasedSimulator(
        settings, std::move(audio_processing), std::move(ap_builder)));
  } else {
    processor.reset(new WavBasedSimulator(settings, std::move(audio_processing),
                                          std::move(ap_builder)));
  }

  if (settings.analysis_only) {
    processor->Analyze();
  } else {
    processor->Process();
  }

  if (settings.report_performance) {
    processor->GetApiCallStatistics().PrintReport();
  }
  if (settings.performance_report_output_filename) {
    processor->GetApiCallStatistics().WriteReportToFile(
        *settings.performance_report_output_filename);
  }

  if (settings.report_bitexactness && settings.aec_dump_input_filename) {
    if (processor->OutputWasBitexact()) {
      std::cout << "The processing was bitexact.";
    } else {
      std::cout << "The processing was not bitexact.";
    }
  }

  return 0;
}

}  // namespace

int AudioprocFloatImpl(rtc::scoped_refptr<AudioProcessing> audio_processing,
                       int argc,
                       char* argv[]) {
  return RunSimulation(
      std::move(audio_processing), /*ap_builder=*/nullptr, argc, argv,
      /*input_aecdump=*/"", /*processed_capture_samples=*/nullptr);
}

int AudioprocFloatImpl(std::unique_ptr<AudioProcessingBuilder> ap_builder,
                       int argc,
                       char* argv[],
                       absl::string_view input_aecdump,
                       std::vector<float>* processed_capture_samples) {
  return RunSimulation(/*audio_processing=*/nullptr, std::move(ap_builder),
                       argc, argv, input_aecdump, processed_capture_samples);
}

}  // namespace test
}  // namespace webrtc
