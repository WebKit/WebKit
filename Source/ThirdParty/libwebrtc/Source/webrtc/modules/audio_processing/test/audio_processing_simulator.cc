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
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "api/audio/echo_canceller3_factory.h"
#include "common_audio/include/audio_util.h"
#include "modules/audio_processing/aec_dump/aec_dump_factory.h"
#include "modules/audio_processing/echo_cancellation_impl.h"
#include "modules/audio_processing/echo_control_mobile_impl.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/audio_processing/test/fake_recording_device.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/json.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/stringutils.h"

namespace webrtc {
namespace test {
namespace {

// Prints out the currently used AEC3 parameter values in JSON format.
void PrintAec3ParameterValues(const EchoCanceller3Config& cfg) {
  std::cout << "{";
  std::cout << "\"delay\": {";
  std::cout << "\"default_delay\": " << cfg.delay.default_delay << ",";
  std::cout << "\"down_sampling_factor\": " << cfg.delay.down_sampling_factor
            << ",";
  std::cout << "\"num_filters\": " << cfg.delay.num_filters << ",";
  std::cout << "\"api_call_jitter_blocks\": "
            << cfg.delay.api_call_jitter_blocks << ",";
  std::cout << "\"min_echo_path_delay_blocks\": "
            << cfg.delay.min_echo_path_delay_blocks << ",";
  std::cout << "\"delay_headroom_blocks\": " << cfg.delay.delay_headroom_blocks
            << ",";
  std::cout << "\"hysteresis_limit_1_blocks\": "
            << cfg.delay.hysteresis_limit_1_blocks << ",";
  std::cout << "\"hysteresis_limit_2_blocks\": "
            << cfg.delay.hysteresis_limit_2_blocks << ",";
  std::cout << "\"skew_hysteresis_blocks\": "
            << cfg.delay.skew_hysteresis_blocks << ",";
  std::cout << "\"fixed_capture_delay_samples\": "
            << cfg.delay.fixed_capture_delay_samples << ",";
  std::cout << "\"delay_estimate_smoothing\": "
            << cfg.delay.delay_estimate_smoothing << ",";
  std::cout << "\"delay_candidate_detection_threshold\": "
            << cfg.delay.delay_candidate_detection_threshold << ",";

  std::cout << "\"delay_selection_thresholds\": {";
  std::cout << "\"initial\": " << cfg.delay.delay_selection_thresholds.initial
            << ",";
  std::cout << "\"converged\": "
            << cfg.delay.delay_selection_thresholds.converged;
  std::cout << "}";

  std::cout << "},";

  std::cout << "\"filter\": {";
  std::cout << "\"main\": [";
  std::cout << cfg.filter.main.length_blocks << ",";
  std::cout << cfg.filter.main.leakage_converged << ",";
  std::cout << cfg.filter.main.leakage_diverged << ",";
  std::cout << cfg.filter.main.error_floor << ",";
  std::cout << cfg.filter.main.noise_gate;
  std::cout << "],";

  std::cout << "\"shadow\": [";
  std::cout << cfg.filter.shadow.length_blocks << ",";
  std::cout << cfg.filter.shadow.rate << ",";
  std::cout << cfg.filter.shadow.noise_gate;
  std::cout << "],";

  std::cout << "\"main_initial\": [";
  std::cout << cfg.filter.main_initial.length_blocks << ",";
  std::cout << cfg.filter.main_initial.leakage_converged << ",";
  std::cout << cfg.filter.main_initial.leakage_diverged << ",";
  std::cout << cfg.filter.main_initial.error_floor << ",";
  std::cout << cfg.filter.main_initial.noise_gate;
  std::cout << "],";

  std::cout << "\"shadow_initial\": [";
  std::cout << cfg.filter.shadow_initial.length_blocks << ",";
  std::cout << cfg.filter.shadow_initial.rate << ",";
  std::cout << cfg.filter.shadow_initial.noise_gate;
  std::cout << "],";

  std::cout << "\"config_change_duration_blocks\": "
            << cfg.filter.config_change_duration_blocks << ",";
  std::cout << "\"initial_state_seconds\": " << cfg.filter.initial_state_seconds
            << ",";
  std::cout << "\"conservative_initial_phase\": "
            << (cfg.filter.conservative_initial_phase ? "true" : "false")
            << ",";
  std::cout << "\"enable_shadow_filter_output_usage\": "
            << (cfg.filter.enable_shadow_filter_output_usage ? "true"
                                                             : "false");

  std::cout << "},";

  std::cout << "\"erle\": {";
  std::cout << "\"min\": " << cfg.erle.min << ",";
  std::cout << "\"max_l\": " << cfg.erle.max_l << ",";
  std::cout << "\"max_h\": " << cfg.erle.max_h << ",";
  std::cout << "\"onset_detection\": "
            << (cfg.erle.onset_detection ? "true" : "false");
  std::cout << "},";

  std::cout << "\"ep_strength\": {";
  std::cout << "\"lf\": " << cfg.ep_strength.lf << ",";
  std::cout << "\"mf\": " << cfg.ep_strength.mf << ",";
  std::cout << "\"hf\": " << cfg.ep_strength.hf << ",";
  std::cout << "\"default_len\": " << cfg.ep_strength.default_len << ",";
  std::cout << "\"reverb_based_on_render\": "
            << (cfg.ep_strength.reverb_based_on_render ? "true" : "false")
            << ",";
  std::cout << "\"echo_can_saturate\": "
            << (cfg.ep_strength.echo_can_saturate ? "true" : "false") << ",";
  std::cout << "\"bounded_erl\": "
            << (cfg.ep_strength.bounded_erl ? "true" : "false");

  std::cout << "},";

  std::cout << "\"gain_mask\": {";
  std::cout << "\"m0\": " << cfg.gain_mask.m0 << ",";
  std::cout << "\"m1\": " << cfg.gain_mask.m1 << ",";
  std::cout << "\"m2\": " << cfg.gain_mask.m2 << ",";
  std::cout << "\"m3\": " << cfg.gain_mask.m3 << ",";
  std::cout << "\"m5\": " << cfg.gain_mask.m5 << ",";
  std::cout << "\"m6\": " << cfg.gain_mask.m6 << ",";
  std::cout << "\"m7\": " << cfg.gain_mask.m7 << ",";
  std::cout << "\"m8\": " << cfg.gain_mask.m8 << ",";
  std::cout << "\"m9\": " << cfg.gain_mask.m9 << ",";
  std::cout << "\"gain_curve_offset\": " << cfg.gain_mask.gain_curve_offset
            << ",";
  std::cout << "\"gain_curve_slope\": " << cfg.gain_mask.gain_curve_slope
            << ",";
  std::cout << "\"temporal_masking_lf\": " << cfg.gain_mask.temporal_masking_lf
            << ",";
  std::cout << "\"temporal_masking_hf\": " << cfg.gain_mask.temporal_masking_hf
            << ",";
  std::cout << "\"temporal_masking_lf_bands\": "
            << cfg.gain_mask.temporal_masking_lf_bands;
  std::cout << "},";

  std::cout << "\"echo_audibility\": {";
  std::cout << "\"low_render_limit\": " << cfg.echo_audibility.low_render_limit
            << ",";
  std::cout << "\"normal_render_limit\": "
            << cfg.echo_audibility.normal_render_limit << ",";
  std::cout << "\"floor_power\": " << cfg.echo_audibility.floor_power << ",";
  std::cout << "\"audibility_threshold_lf\": "
            << cfg.echo_audibility.audibility_threshold_lf << ",";
  std::cout << "\"audibility_threshold_mf\": "
            << cfg.echo_audibility.audibility_threshold_mf << ",";
  std::cout << "\"audibility_threshold_hf\": "
            << cfg.echo_audibility.audibility_threshold_hf << ",";
  std::cout << "\"use_stationary_properties\": "
            << (cfg.echo_audibility.use_stationary_properties ? "true"
                                                              : "false")
            << ",";
  std::cout << "\"use_stationarity_properties_at_init\": "
            << (cfg.echo_audibility.use_stationarity_properties_at_init
                    ? "true"
                    : "false");
  std::cout << "},";

  std::cout << "\"render_levels\": {";
  std::cout << "\"active_render_limit\": "
            << cfg.render_levels.active_render_limit << ",";
  std::cout << "\"poor_excitation_render_limit\": "
            << cfg.render_levels.poor_excitation_render_limit << ",";
  std::cout << "\"poor_excitation_render_limit_ds8\": "
            << cfg.render_levels.poor_excitation_render_limit_ds8;
  std::cout << "},";

  std::cout << "\"echo_removal_control\": {";
  std::cout << "\"gain_rampup\": {";
  std::cout << "\"initial_gain\": "
            << cfg.echo_removal_control.gain_rampup.initial_gain << ",";
  std::cout << "\"first_non_zero_gain\": "
            << cfg.echo_removal_control.gain_rampup.first_non_zero_gain << ",";
  std::cout << "\"non_zero_gain_blocks\": "
            << cfg.echo_removal_control.gain_rampup.non_zero_gain_blocks << ",";
  std::cout << "\"full_gain_blocks\": "
            << cfg.echo_removal_control.gain_rampup.full_gain_blocks;
  std::cout << "},";
  std::cout << "\"has_clock_drift\": "
            << (cfg.echo_removal_control.has_clock_drift ? "true" : "false")
            << ",";
  std::cout << "\"linear_and_stable_echo_path\": "
            << (cfg.echo_removal_control.linear_and_stable_echo_path ? "true"
                                                                     : "false");

  std::cout << "},";

  std::cout << "\"echo_model\": {";
  std::cout << "\"noise_floor_hold\": " << cfg.echo_model.noise_floor_hold
            << ",";
  std::cout << "\"min_noise_floor_power\": "
            << cfg.echo_model.min_noise_floor_power << ",";
  std::cout << "\"stationary_gate_slope\": "
            << cfg.echo_model.stationary_gate_slope << ",";
  std::cout << "\"noise_gate_power\": " << cfg.echo_model.noise_gate_power
            << ",";
  std::cout << "\"noise_gate_slope\": " << cfg.echo_model.noise_gate_slope
            << ",";
  std::cout << "\"render_pre_window_size\": "
            << cfg.echo_model.render_pre_window_size << ",";
  std::cout << "\"render_post_window_size\": "
            << cfg.echo_model.render_post_window_size << ",";
  std::cout << "\"render_pre_window_size_init\": "
            << cfg.echo_model.render_pre_window_size_init << ",";
  std::cout << "\"render_post_window_size_init\": "
            << cfg.echo_model.render_post_window_size_init << ",";
  std::cout << "\"nonlinear_hold\": " << cfg.echo_model.nonlinear_hold << ",";
  std::cout << "\"nonlinear_release\": " << cfg.echo_model.nonlinear_release;
  std::cout << "},";

  std::cout << "\"suppressor\": {";
  std::cout << "\"nearend_average_blocks\": "
            << cfg.suppressor.nearend_average_blocks << ",";
  std::cout << "\"normal_tuning\": {";
  std::cout << "\"mask_lf\": [";
  std::cout << cfg.suppressor.normal_tuning.mask_lf.enr_transparent << ",";
  std::cout << cfg.suppressor.normal_tuning.mask_lf.enr_suppress << ",";
  std::cout << cfg.suppressor.normal_tuning.mask_lf.emr_transparent;
  std::cout << "],";
  std::cout << "\"mask_hf\": [";
  std::cout << cfg.suppressor.normal_tuning.mask_hf.enr_transparent << ",";
  std::cout << cfg.suppressor.normal_tuning.mask_hf.enr_suppress << ",";
  std::cout << cfg.suppressor.normal_tuning.mask_hf.emr_transparent;
  std::cout << "],";
  std::cout << "\"max_inc_factor\": "
            << cfg.suppressor.normal_tuning.max_inc_factor << ",";
  std::cout << "\"max_dec_factor_lf\": "
            << cfg.suppressor.normal_tuning.max_dec_factor_lf;
  std::cout << "},";
  std::cout << "\"nearend_tuning\": {";
  std::cout << "\"mask_lf\": [";
  std::cout << cfg.suppressor.nearend_tuning.mask_lf.enr_transparent << ",";
  std::cout << cfg.suppressor.nearend_tuning.mask_lf.enr_suppress << ",";
  std::cout << cfg.suppressor.nearend_tuning.mask_lf.emr_transparent;
  std::cout << "],";
  std::cout << "\"mask_hf\": [";
  std::cout << cfg.suppressor.nearend_tuning.mask_hf.enr_transparent << ",";
  std::cout << cfg.suppressor.nearend_tuning.mask_hf.enr_suppress << ",";
  std::cout << cfg.suppressor.nearend_tuning.mask_hf.emr_transparent;
  std::cout << "],";
  std::cout << "\"max_inc_factor\": "
            << cfg.suppressor.nearend_tuning.max_inc_factor << ",";
  std::cout << "\"max_dec_factor_lf\": "
            << cfg.suppressor.nearend_tuning.max_dec_factor_lf;
  std::cout << "},";
  std::cout << "\"dominant_nearend_detection\": {";
  std::cout << "\"enr_threshold\": "
            << cfg.suppressor.dominant_nearend_detection.enr_threshold << ",";
  std::cout << "\"snr_threshold\": "
            << cfg.suppressor.dominant_nearend_detection.snr_threshold << ",";
  std::cout << "\"hold_duration\": "
            << cfg.suppressor.dominant_nearend_detection.hold_duration << ",";
  std::cout << "\"trigger_threshold\": "
            << cfg.suppressor.dominant_nearend_detection.trigger_threshold;
  std::cout << "},";
  std::cout << "\"high_bands_suppression\": {";
  std::cout << "\"enr_threshold\": "
            << cfg.suppressor.high_bands_suppression.enr_threshold << ",";
  std::cout << "\"max_gain_during_echo\": "
            << cfg.suppressor.high_bands_suppression.max_gain_during_echo;
  std::cout << "},";
  std::cout << "\"floor_first_increase\": "
            << cfg.suppressor.floor_first_increase << ",";
  std::cout << "\"enforce_transparent\": "
            << (cfg.suppressor.enforce_transparent ? "true" : "false") << ",";
  std::cout << "\"enforce_empty_higher_bands\": "
            << (cfg.suppressor.enforce_empty_higher_bands ? "true" : "false");
  std::cout << "}";
  std::cout << "}";
  std::cout << std::endl;
}

// Class for parsing the AEC3 parameters from a JSON file and producing a config
// struct.
class Aec3ParametersParser {
 public:
  static EchoCanceller3Config Parse(const std::string& filename) {
    return Aec3ParametersParser().ParseFile(filename);
  }

 private:
  Aec3ParametersParser() = default;

  void ReadParam(const Json::Value& root,
                 std::string param_name,
                 bool* param) const {
    RTC_CHECK(param);
    bool v;
    if (rtc::GetBoolFromJsonObject(root, param_name, &v)) {
      *param = v;
    }
  }

  void ReadParam(const Json::Value& root,
                 std::string param_name,
                 size_t* param) const {
    RTC_CHECK(param);
    int v;
    if (rtc::GetIntFromJsonObject(root, param_name, &v)) {
      *param = v;
    }
  }

  void ReadParam(const Json::Value& root,
                 std::string param_name,
                 int* param) const {
    RTC_CHECK(param);
    int v;
    if (rtc::GetIntFromJsonObject(root, param_name, &v)) {
      *param = v;
    }
  }

  void ReadParam(const Json::Value& root,
                 std::string param_name,
                 float* param) const {
    RTC_CHECK(param);
    double v;
    if (rtc::GetDoubleFromJsonObject(root, param_name, &v)) {
      *param = static_cast<float>(v);
    }
  }

  void ReadParam(const Json::Value& root,
                 std::string param_name,
                 EchoCanceller3Config::Filter::MainConfiguration* param) const {
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
    }
  }

  void ReadParam(
      const Json::Value& root,
      std::string param_name,
      EchoCanceller3Config::Filter::ShadowConfiguration* param) const {
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
    }
  }

  void ReadParam(
      const Json::Value& root,
      std::string param_name,
      EchoCanceller3Config::Suppressor::MaskingThresholds* param) const {
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
    }
  }

  EchoCanceller3Config ParseFile(const std::string& filename) const {
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

    Json::Value section;
    if (rtc::GetValueFromJsonObject(root, "delay", &section)) {
      ReadParam(section, "default_delay", &cfg.delay.default_delay);
      ReadParam(section, "down_sampling_factor",
                &cfg.delay.down_sampling_factor);
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
      ReadParam(section, "fixed_capture_delay_samples",
                &cfg.delay.fixed_capture_delay_samples);
      ReadParam(section, "delay_estimate_smoothing",
                &cfg.delay.delay_estimate_smoothing);
      ReadParam(section, "delay_candidate_detection_threshold",
                &cfg.delay.delay_candidate_detection_threshold);

      Json::Value subsection;
      if (rtc::GetValueFromJsonObject(section, "delay_selection_thresholds",
                                      &subsection)) {
        ReadParam(subsection, "initial",
                  &cfg.delay.delay_selection_thresholds.initial);
        ReadParam(subsection, "converged",
                  &cfg.delay.delay_selection_thresholds.converged);
      }
    }

    if (rtc::GetValueFromJsonObject(root, "filter", &section)) {
      ReadParam(section, "main", &cfg.filter.main);
      ReadParam(section, "shadow", &cfg.filter.shadow);
      ReadParam(section, "main_initial", &cfg.filter.main_initial);
      ReadParam(section, "shadow_initial", &cfg.filter.shadow_initial);
      ReadParam(section, "config_change_duration_blocks",
                &cfg.filter.config_change_duration_blocks);
      ReadParam(section, "initial_state_seconds",
                &cfg.filter.initial_state_seconds);
      ReadParam(section, "conservative_initial_phase",
                &cfg.filter.conservative_initial_phase);
      ReadParam(section, "enable_shadow_filter_output_usage",
                &cfg.filter.enable_shadow_filter_output_usage);
    }

    if (rtc::GetValueFromJsonObject(root, "erle", &section)) {
      ReadParam(section, "min", &cfg.erle.min);
      ReadParam(section, "max_l", &cfg.erle.max_l);
      ReadParam(section, "max_h", &cfg.erle.max_h);
      ReadParam(section, "onset_detection", &cfg.erle.onset_detection);
    }

    if (rtc::GetValueFromJsonObject(root, "ep_strength", &section)) {
      ReadParam(section, "lf", &cfg.ep_strength.lf);
      ReadParam(section, "mf", &cfg.ep_strength.mf);
      ReadParam(section, "hf", &cfg.ep_strength.hf);
      ReadParam(section, "default_len", &cfg.ep_strength.default_len);
      ReadParam(section, "reverb_based_on_render",
                &cfg.ep_strength.reverb_based_on_render);
      ReadParam(section, "echo_can_saturate",
                &cfg.ep_strength.echo_can_saturate);
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
      ReadParam(section, "use_stationary_properties_at_init",
                &cfg.echo_audibility.use_stationarity_properties_at_init);
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
      ReadParam(section, "nonlinear_release",
                &cfg.echo_model.nonlinear_release);
    }

    Json::Value subsection;
    if (rtc::GetValueFromJsonObject(root, "suppressor", &section)) {
      ReadParam(section, "nearend_average_blocks",
                &cfg.suppressor.nearend_average_blocks);

      if (rtc::GetValueFromJsonObject(section, "normal_tuning", &subsection)) {
        ReadParam(subsection, "mask_lf", &cfg.suppressor.normal_tuning.mask_lf);
        ReadParam(subsection, "mask_hf", &cfg.suppressor.normal_tuning.mask_hf);
        ReadParam(subsection, "max_inc_factor",
                  &cfg.suppressor.normal_tuning.max_inc_factor);
        ReadParam(subsection, "max_dec_factor_lf",
                  &cfg.suppressor.normal_tuning.max_dec_factor_lf);
      }

      if (rtc::GetValueFromJsonObject(section, "nearend_tuning", &subsection)) {
        ReadParam(subsection, "mask_lf",
                  &cfg.suppressor.nearend_tuning.mask_lf);
        ReadParam(subsection, "mask_hf",
                  &cfg.suppressor.nearend_tuning.mask_hf);
        ReadParam(subsection, "max_inc_factor",
                  &cfg.suppressor.nearend_tuning.max_inc_factor);
        ReadParam(subsection, "max_dec_factor_lf",
                  &cfg.suppressor.nearend_tuning.max_dec_factor_lf);
      }

      if (rtc::GetValueFromJsonObject(section, "dominant_nearend_detection",
                                      &subsection)) {
        ReadParam(subsection, "enr_threshold",
                  &cfg.suppressor.dominant_nearend_detection.enr_threshold);
        ReadParam(subsection, "snr_threshold",
                  &cfg.suppressor.dominant_nearend_detection.snr_threshold);
        ReadParam(subsection, "hold_duration",
                  &cfg.suppressor.dominant_nearend_detection.hold_duration);
        ReadParam(subsection, "trigger_threshold",
                  &cfg.suppressor.dominant_nearend_detection.trigger_threshold);
      }

      if (rtc::GetValueFromJsonObject(section, "high_bands_suppression",
                                      &subsection)) {
        ReadParam(subsection, "enr_threshold",
                  &cfg.suppressor.high_bands_suppression.enr_threshold);
        ReadParam(subsection, "max_gain_during_echo",
                  &cfg.suppressor.high_bands_suppression.max_gain_during_echo);
      }

      ReadParam(section, "floor_first_increase",
                &cfg.suppressor.floor_first_increase);
      ReadParam(section, "enforce_transparent",
                &cfg.suppressor.enforce_transparent);
      ReadParam(section, "enforce_empty_higher_bands",
                &cfg.suppressor.enforce_empty_higher_bands);
    }

    std::cout << std::endl;
    return cfg;
  }
};

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
  rtc::StringBuilder ss;
  ss << wav_name.substr(0, wav_name.size() - 4) << "_" << counter
     << wav_name.substr(wav_name.size() - 4);
  return ss.Release();
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
  if (settings_.use_agc2) {
    apm_config.gain_controller2.enabled = *settings_.use_agc2;
    apm_config.gain_controller2.fixed_gain_db = settings_.agc2_fixed_gain_db;
    if (settings_.agc2_use_adaptive_gain) {
      apm_config.gain_controller2.adaptive_digital_mode =
          *settings_.agc2_use_adaptive_gain;
    }
  }
  if (settings_.use_pre_amplifier) {
    apm_config.pre_amplifier.enabled = *settings_.use_pre_amplifier;
    apm_config.pre_amplifier.fixed_gain_factor =
        settings_.pre_amplifier_gain_factor;
  }

  bool use_aec2 = settings_.use_aec && *settings_.use_aec;
  bool use_aec3 = settings_.use_aec3 && *settings_.use_aec3;
  bool use_aecm = settings_.use_aecm && *settings_.use_aecm;
  if (use_aec2 || use_aec3 || use_aecm) {
    apm_config.echo_canceller.enabled = true;
    apm_config.echo_canceller.mobile_mode = use_aecm;
  }

  if (settings_.use_aec3 && *settings_.use_aec3) {
    EchoCanceller3Config cfg;
    if (settings_.aec3_settings_filename) {
      if (settings_.use_verbose_logging) {
        std::cout << "Reading AEC3 Parameters from JSON input." << std::endl;
      }
      cfg = Aec3ParametersParser::Parse(*settings_.aec3_settings_filename);
    }
    echo_control_factory.reset(new EchoCanceller3Factory(cfg));

    if (settings_.print_aec3_parameter_values) {
      if (!settings_.use_quiet_output) {
        std::cout << "AEC3 settings:" << std::endl;
      }
      PrintAec3ParameterValues(cfg);
    }
  }

  if (settings_.use_drift_compensation && *settings_.use_drift_compensation) {
    RTC_LOG(LS_ERROR) << "Ignoring deprecated setting: AEC2 drift compensation";
  }
  if (settings_.aec_suppression_level) {
    auto level = static_cast<webrtc::EchoCancellationImpl::SuppressionLevel>(
        *settings_.aec_suppression_level);
    if (level ==
        webrtc::EchoCancellationImpl::SuppressionLevel::kLowSuppression) {
      RTC_LOG(LS_ERROR) << "Ignoring deprecated setting: AEC2 low suppression";
    } else {
      apm_config.echo_canceller.legacy_moderate_suppression_level =
          (level == webrtc::EchoCancellationImpl::SuppressionLevel::
                        kModerateSuppression);
    }
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
          *settings_.experimental_agc_disable_digital_adaptive,
      !!settings_.experimental_agc_analyze_before_aec &&
          *settings_.experimental_agc_analyze_before_aec));
  if (settings_.use_ed) {
    apm_config.residual_echo_detector.enabled = *settings_.use_ed;
  }

  RTC_CHECK(ap_builder_);
  ap_.reset((*ap_builder_)
                .SetEchoControlFactory(std::move(echo_control_factory))
                .Create(config));
  RTC_CHECK(ap_);

  ap_->ApplyConfig(apm_config);

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
