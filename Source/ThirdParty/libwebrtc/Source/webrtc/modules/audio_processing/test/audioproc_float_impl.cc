/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>

#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/audio_processing/test/aec_dump_based_simulator.h"
#include "modules/audio_processing/test/audio_processing_simulator.h"
#include "modules/audio_processing/test/audioproc_float_impl.h"
#include "modules/audio_processing/test/wav_based_simulator.h"
#include "rtc_base/checks.h"
#include "rtc_base/flags.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {
namespace test {
namespace {

const int kParameterNotSpecifiedValue = -10000;

const char kUsageDescription[] =
    "Usage: audioproc_f [options] -i <input.wav>\n"
    "                   or\n"
    "       audioproc_f [options] -dump_input <aec_dump>\n"
    "\n\n"
    "Command-line tool to simulate a call using the audio "
    "processing module, either based on wav files or "
    "protobuf debug dump recordings.\n";

WEBRTC_DEFINE_string(dump_input, "", "Aec dump input filename");
WEBRTC_DEFINE_string(dump_output, "", "Aec dump output filename");
WEBRTC_DEFINE_string(i, "", "Forward stream input wav filename");
WEBRTC_DEFINE_string(o, "", "Forward stream output wav filename");
WEBRTC_DEFINE_string(ri, "", "Reverse stream input wav filename");
WEBRTC_DEFINE_string(ro, "", "Reverse stream output wav filename");
WEBRTC_DEFINE_string(artificial_nearend, "", "Artificial nearend wav filename");
WEBRTC_DEFINE_int(output_num_channels,
                  kParameterNotSpecifiedValue,
                  "Number of forward stream output channels");
WEBRTC_DEFINE_int(reverse_output_num_channels,
                  kParameterNotSpecifiedValue,
                  "Number of Reverse stream output channels");
WEBRTC_DEFINE_int(output_sample_rate_hz,
                  kParameterNotSpecifiedValue,
                  "Forward stream output sample rate in Hz");
WEBRTC_DEFINE_int(reverse_output_sample_rate_hz,
                  kParameterNotSpecifiedValue,
                  "Reverse stream output sample rate in Hz");
WEBRTC_DEFINE_bool(fixed_interface,
                   false,
                   "Use the fixed interface when operating on wav files");
WEBRTC_DEFINE_int(aec,
                  kParameterNotSpecifiedValue,
                  "Activate (1) or deactivate(0) the echo canceller");
WEBRTC_DEFINE_int(aecm,
                  kParameterNotSpecifiedValue,
                  "Activate (1) or deactivate(0) the mobile echo controller");
WEBRTC_DEFINE_int(ed,
                  kParameterNotSpecifiedValue,
                  "Activate (1) or deactivate (0) the residual echo detector");
WEBRTC_DEFINE_string(ed_graph,
                     "",
                     "Output filename for graph of echo likelihood");
WEBRTC_DEFINE_int(agc,
                  kParameterNotSpecifiedValue,
                  "Activate (1) or deactivate(0) the AGC");
WEBRTC_DEFINE_int(agc2,
                  kParameterNotSpecifiedValue,
                  "Activate (1) or deactivate(0) the AGC2");
WEBRTC_DEFINE_int(pre_amplifier,
                  kParameterNotSpecifiedValue,
                  "Activate (1) or deactivate(0) the pre amplifier");
WEBRTC_DEFINE_int(hpf,
                  kParameterNotSpecifiedValue,
                  "Activate (1) or deactivate(0) the high-pass filter");
WEBRTC_DEFINE_int(ns,
                  kParameterNotSpecifiedValue,
                  "Activate (1) or deactivate(0) the noise suppressor");
WEBRTC_DEFINE_int(ts,
                  kParameterNotSpecifiedValue,
                  "Activate (1) or deactivate(0) the transient suppressor");
WEBRTC_DEFINE_int(vad,
                  kParameterNotSpecifiedValue,
                  "Activate (1) or deactivate(0) the voice activity detector");
WEBRTC_DEFINE_int(le,
                  kParameterNotSpecifiedValue,
                  "Activate (1) or deactivate(0) the level estimator");
WEBRTC_DEFINE_bool(
    all_default,
    false,
    "Activate all of the default components (will be overridden by any "
    "other settings)");
WEBRTC_DEFINE_int(aec_suppression_level,
                  kParameterNotSpecifiedValue,
                  "Set the aec suppression level (0-2)");
WEBRTC_DEFINE_int(delay_agnostic,
                  kParameterNotSpecifiedValue,
                  "Activate (1) or deactivate(0) the AEC delay agnostic mode");
WEBRTC_DEFINE_int(extended_filter,
                  kParameterNotSpecifiedValue,
                  "Activate (1) or deactivate(0) the AEC extended filter mode");
WEBRTC_DEFINE_int(
    aec3,
    kParameterNotSpecifiedValue,
    "Activate (1) or deactivate(0) the experimental AEC mode AEC3");
WEBRTC_DEFINE_int(experimental_agc,
                  kParameterNotSpecifiedValue,
                  "Activate (1) or deactivate(0) the experimental AGC");
WEBRTC_DEFINE_int(
    experimental_agc_disable_digital_adaptive,
    kParameterNotSpecifiedValue,
    "Force-deactivate (1) digital adaptation in "
    "experimental AGC. Digital adaptation is active by default (0).");
WEBRTC_DEFINE_int(experimental_agc_analyze_before_aec,
                  kParameterNotSpecifiedValue,
                  "Make level estimation happen before AEC"
                  " in the experimental AGC. After AEC is the default (0)");
WEBRTC_DEFINE_int(
    experimental_agc_agc2_level_estimator,
    kParameterNotSpecifiedValue,
    "AGC2 level estimation"
    " in the experimental AGC. AGC1 level estimation is the default (0)");
WEBRTC_DEFINE_int(
    refined_adaptive_filter,
    kParameterNotSpecifiedValue,
    "Activate (1) or deactivate(0) the refined adaptive filter functionality");
WEBRTC_DEFINE_int(agc_mode,
                  kParameterNotSpecifiedValue,
                  "Specify the AGC mode (0-2)");
WEBRTC_DEFINE_int(agc_target_level,
                  kParameterNotSpecifiedValue,
                  "Specify the AGC target level (0-31)");
WEBRTC_DEFINE_int(agc_limiter,
                  kParameterNotSpecifiedValue,
                  "Activate (1) or deactivate(0) the level estimator");
WEBRTC_DEFINE_int(agc_compression_gain,
                  kParameterNotSpecifiedValue,
                  "Specify the AGC compression gain (0-90)");
WEBRTC_DEFINE_float(agc2_enable_adaptive_gain,
                    kParameterNotSpecifiedValue,
                    "Activate (1) or deactivate(0) the AGC2 adaptive gain");
WEBRTC_DEFINE_float(agc2_fixed_gain_db, 0.f, "AGC2 fixed gain (dB) to apply");

std::vector<std::string> GetAgc2AdaptiveLevelEstimatorNames() {
  return {"RMS", "peak"};
}
WEBRTC_DEFINE_string(
    agc2_adaptive_level_estimator,
    "RMS",
    "AGC2 adaptive digital level estimator to use [RMS, peak]");

WEBRTC_DEFINE_float(pre_amplifier_gain_factor,
                    1.f,
                    "Pre-amplifier gain factor (linear) to apply");
WEBRTC_DEFINE_int(vad_likelihood,
                  kParameterNotSpecifiedValue,
                  "Specify the VAD likelihood (0-3)");
WEBRTC_DEFINE_int(ns_level,
                  kParameterNotSpecifiedValue,
                  "Specify the NS level (0-3)");
WEBRTC_DEFINE_int(stream_delay,
                  kParameterNotSpecifiedValue,
                  "Specify the stream delay in ms to use");
WEBRTC_DEFINE_int(use_stream_delay,
                  kParameterNotSpecifiedValue,
                  "Activate (1) or deactivate(0) reporting the stream delay");
WEBRTC_DEFINE_int(stream_drift_samples,
                  kParameterNotSpecifiedValue,
                  "Specify the number of stream drift samples to use");
WEBRTC_DEFINE_int(initial_mic_level, 100, "Initial mic level (0-255)");
WEBRTC_DEFINE_int(
    simulate_mic_gain,
    0,
    "Activate (1) or deactivate(0) the analog mic gain simulation");
WEBRTC_DEFINE_int(
    simulated_mic_kind,
    kParameterNotSpecifiedValue,
    "Specify which microphone kind to use for microphone simulation");
WEBRTC_DEFINE_bool(performance_report, false, "Report the APM performance ");
WEBRTC_DEFINE_bool(verbose, false, "Produce verbose output");
WEBRTC_DEFINE_bool(quiet,
                   false,
                   "Avoid producing information about the progress.");
WEBRTC_DEFINE_bool(bitexactness_report,
                   false,
                   "Report bitexactness for aec dump result reproduction");
WEBRTC_DEFINE_bool(discard_settings_in_aecdump,
                   false,
                   "Discard any config settings specified in the aec dump");
WEBRTC_DEFINE_bool(store_intermediate_output,
                   false,
                   "Creates new output files after each init");
WEBRTC_DEFINE_string(custom_call_order_file,
                     "",
                     "Custom process API call order file");
WEBRTC_DEFINE_bool(print_aec3_parameter_values,
                   false,
                   "Print parameter values used in AEC3 in JSON-format");
WEBRTC_DEFINE_string(aec3_settings,
                     "",
                     "File in JSON-format with custom AEC3 settings");
WEBRTC_DEFINE_bool(dump_data,
                   false,
                   "Dump internal data during the call (requires build flag)");
WEBRTC_DEFINE_string(dump_data_output_dir,
                     "",
                     "Internal data dump output directory");
WEBRTC_DEFINE_bool(help, false, "Print this message");

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
  if (FLAG_all_default) {
    settings.use_le = true;
    settings.use_vad = true;
    settings.use_ie = false;
    settings.use_ts = true;
    settings.use_ns = true;
    settings.use_hpf = true;
    settings.use_agc = true;
    settings.use_agc2 = false;
    settings.use_pre_amplifier = false;
    settings.use_aec = true;
    settings.use_aecm = false;
    settings.use_ed = false;
  }
  SetSettingIfSpecified(FLAG_dump_input, &settings.aec_dump_input_filename);
  SetSettingIfSpecified(FLAG_dump_output, &settings.aec_dump_output_filename);
  SetSettingIfSpecified(FLAG_i, &settings.input_filename);
  SetSettingIfSpecified(FLAG_o, &settings.output_filename);
  SetSettingIfSpecified(FLAG_ri, &settings.reverse_input_filename);
  SetSettingIfSpecified(FLAG_ro, &settings.reverse_output_filename);
  SetSettingIfSpecified(FLAG_artificial_nearend,
                        &settings.artificial_nearend_filename);
  SetSettingIfSpecified(FLAG_output_num_channels,
                        &settings.output_num_channels);
  SetSettingIfSpecified(FLAG_reverse_output_num_channels,
                        &settings.reverse_output_num_channels);
  SetSettingIfSpecified(FLAG_output_sample_rate_hz,
                        &settings.output_sample_rate_hz);
  SetSettingIfSpecified(FLAG_reverse_output_sample_rate_hz,
                        &settings.reverse_output_sample_rate_hz);
  SetSettingIfFlagSet(FLAG_aec, &settings.use_aec);
  SetSettingIfFlagSet(FLAG_aecm, &settings.use_aecm);
  SetSettingIfFlagSet(FLAG_ed, &settings.use_ed);
  SetSettingIfSpecified(FLAG_ed_graph, &settings.ed_graph_output_filename);
  SetSettingIfFlagSet(FLAG_agc, &settings.use_agc);
  SetSettingIfFlagSet(FLAG_agc2, &settings.use_agc2);
  SetSettingIfFlagSet(FLAG_pre_amplifier, &settings.use_pre_amplifier);
  SetSettingIfFlagSet(FLAG_hpf, &settings.use_hpf);
  SetSettingIfFlagSet(FLAG_ns, &settings.use_ns);
  SetSettingIfFlagSet(FLAG_ts, &settings.use_ts);
  SetSettingIfFlagSet(FLAG_vad, &settings.use_vad);
  SetSettingIfFlagSet(FLAG_le, &settings.use_le);
  SetSettingIfSpecified(FLAG_aec_suppression_level,
                        &settings.aec_suppression_level);
  SetSettingIfFlagSet(FLAG_delay_agnostic, &settings.use_delay_agnostic);
  SetSettingIfFlagSet(FLAG_extended_filter, &settings.use_extended_filter);
  SetSettingIfFlagSet(FLAG_refined_adaptive_filter,
                      &settings.use_refined_adaptive_filter);

  SetSettingIfFlagSet(FLAG_aec3, &settings.use_aec3);
  SetSettingIfFlagSet(FLAG_experimental_agc, &settings.use_experimental_agc);
  SetSettingIfFlagSet(FLAG_experimental_agc_disable_digital_adaptive,
                      &settings.experimental_agc_disable_digital_adaptive);
  SetSettingIfFlagSet(FLAG_experimental_agc_analyze_before_aec,
                      &settings.experimental_agc_analyze_before_aec);
  SetSettingIfFlagSet(FLAG_experimental_agc_agc2_level_estimator,
                      &settings.use_experimental_agc_agc2_level_estimator);
  SetSettingIfSpecified(FLAG_agc_mode, &settings.agc_mode);
  SetSettingIfSpecified(FLAG_agc_target_level, &settings.agc_target_level);
  SetSettingIfFlagSet(FLAG_agc_limiter, &settings.use_agc_limiter);
  SetSettingIfSpecified(FLAG_agc_compression_gain,
                        &settings.agc_compression_gain);
  SetSettingIfFlagSet(FLAG_agc2_enable_adaptive_gain,
                      &settings.agc2_use_adaptive_gain);
  settings.agc2_fixed_gain_db = FLAG_agc2_fixed_gain_db;
  settings.agc2_adaptive_level_estimator =
      MapAgc2AdaptiveLevelEstimator(FLAG_agc2_adaptive_level_estimator);
  settings.pre_amplifier_gain_factor = FLAG_pre_amplifier_gain_factor;
  SetSettingIfSpecified(FLAG_vad_likelihood, &settings.vad_likelihood);
  SetSettingIfSpecified(FLAG_ns_level, &settings.ns_level);
  SetSettingIfSpecified(FLAG_stream_delay, &settings.stream_delay);
  SetSettingIfFlagSet(FLAG_use_stream_delay, &settings.use_stream_delay);
  SetSettingIfSpecified(FLAG_stream_drift_samples,
                        &settings.stream_drift_samples);
  SetSettingIfSpecified(FLAG_custom_call_order_file,
                        &settings.custom_call_order_filename);
  SetSettingIfSpecified(FLAG_aec3_settings, &settings.aec3_settings_filename);
  settings.initial_mic_level = FLAG_initial_mic_level;
  settings.simulate_mic_gain = FLAG_simulate_mic_gain;
  SetSettingIfSpecified(FLAG_simulated_mic_kind, &settings.simulated_mic_kind);
  settings.report_performance = FLAG_performance_report;
  settings.use_verbose_logging = FLAG_verbose;
  settings.use_quiet_output = FLAG_quiet;
  settings.report_bitexactness = FLAG_bitexactness_report;
  settings.discard_all_settings_in_aecdump = FLAG_discard_settings_in_aecdump;
  settings.fixed_interface = FLAG_fixed_interface;
  settings.store_intermediate_output = FLAG_store_intermediate_output;
  settings.print_aec3_parameter_values = FLAG_print_aec3_parameter_values;
  settings.dump_internal_data = FLAG_dump_data;
  SetSettingIfSpecified(FLAG_dump_data_output_dir,
                        &settings.dump_internal_data_output_dir);

  return settings;
}

void ReportConditionalErrorAndExit(bool condition, const std::string& message) {
  if (condition) {
    std::cerr << message << std::endl;
    exit(1);
  }
}

void PerformBasicParameterSanityChecks(const SimulationSettings& settings) {
  if (settings.input_filename || settings.reverse_input_filename) {
    ReportConditionalErrorAndExit(!!settings.aec_dump_input_filename,
                                  "Error: The aec dump cannot be specified "
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
    ReportConditionalErrorAndExit(!settings.aec_dump_input_filename,
                                  "Error: Either the aec dump or the wav "
                                  "input files must be specified!\n");
  }

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

  ReportConditionalErrorAndExit(settings.aec_suppression_level &&
                                    ((*settings.aec_suppression_level) < 1 ||
                                     (*settings.aec_suppression_level) > 2),
                                "Error: --aec_suppression_level must be "
                                "specified between 1 and 2. 0 is "
                                "deprecated.\n");

  ReportConditionalErrorAndExit(
      settings.agc_target_level && ((*settings.agc_target_level) < 0 ||
                                    (*settings.agc_target_level) > 31),
      "Error: --agc_target_level must be specified between 0 and 31.\n");

  ReportConditionalErrorAndExit(
      settings.agc_compression_gain && ((*settings.agc_compression_gain) < 0 ||
                                        (*settings.agc_compression_gain) > 90),
      "Error: --agc_compression_gain must be specified between 0 and 90.\n");

  ReportConditionalErrorAndExit(
      settings.use_agc2 && *settings.use_agc2 &&
          ((settings.agc2_fixed_gain_db) < 0 ||
           (settings.agc2_fixed_gain_db) > 90),
      "Error: --agc2_fixed_gain_db must be specified between 0 and 90.\n");

  ReportConditionalErrorAndExit(
      settings.vad_likelihood &&
          ((*settings.vad_likelihood) < 0 || (*settings.vad_likelihood) > 3),
      "Error: --vad_likelihood must be specified between 0 and 3.\n");

  ReportConditionalErrorAndExit(
      settings.ns_level &&
          ((*settings.ns_level) < 0 || (*settings.ns_level) > 3),
      "Error: --ns_level must be specified between 0 and 3.\n");

  ReportConditionalErrorAndExit(
      settings.report_bitexactness && !settings.aec_dump_input_filename,
      "Error: --bitexactness_report can only be used when operating on an "
      "aecdump\n");

  ReportConditionalErrorAndExit(
      settings.custom_call_order_filename && settings.aec_dump_input_filename,
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
      WEBRTC_APM_DEBUG_DUMP == 0 && settings.dump_internal_data,
      "Error: --dump_data cannot be set without proper build support.\n");

  ReportConditionalErrorAndExit(
      !settings.dump_internal_data &&
          settings.dump_internal_data_output_dir.has_value(),
      "Error: --dump_data_output_dir cannot be set without --dump_data.\n");
}

}  // namespace

int AudioprocFloatImpl(std::unique_ptr<AudioProcessingBuilder> ap_builder,
                       int argc,
                       char* argv[]) {
  if (rtc::FlagList::SetFlagsFromCommandLine(&argc, argv, true) || FLAG_help ||
      argc != 1) {
    printf("%s", kUsageDescription);
    if (FLAG_help) {
      rtc::FlagList::Print(nullptr, false);
      return 0;
    }
    return 1;
  }

  SimulationSettings settings = CreateSettings();
  PerformBasicParameterSanityChecks(settings);
  std::unique_ptr<AudioProcessingSimulator> processor;

  if (settings.aec_dump_input_filename) {
    processor.reset(new AecDumpBasedSimulator(settings, std::move(ap_builder)));
  } else {
    processor.reset(new WavBasedSimulator(settings, std::move(ap_builder)));
  }

  processor->Process();

  if (settings.report_performance) {
    const auto& proc_time = processor->proc_time();
    int64_t exec_time_us = proc_time.sum / rtc::kNumNanosecsPerMicrosec;
    std::cout << std::endl
              << "Execution time: " << exec_time_us * 1e-6 << " s, File time: "
              << processor->get_num_process_stream_calls() * 1.f /
                     AudioProcessingSimulator::kChunksPerSecond
              << std::endl
              << "Time per fwd stream chunk (mean, max, min): " << std::endl
              << exec_time_us * 1.f / processor->get_num_process_stream_calls()
              << " us, " << 1.f * proc_time.max / rtc::kNumNanosecsPerMicrosec
              << " us, " << 1.f * proc_time.min / rtc::kNumNanosecsPerMicrosec
              << " us" << std::endl;
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

}  // namespace test
}  // namespace webrtc
