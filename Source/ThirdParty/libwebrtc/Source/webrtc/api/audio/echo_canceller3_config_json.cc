/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/audio/echo_canceller3_config_json.h"

#include <string>
#include <vector>

#include "rtc_base/logging.h"
#include "rtc_base/strings/json.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {
namespace {
void ReadParam(const Json::Value& root, std::string param_name, bool* param) {
  RTC_DCHECK(param);
  bool v;
  if (rtc::GetBoolFromJsonObject(root, param_name, &v)) {
    *param = v;
  }
}

void ReadParam(const Json::Value& root, std::string param_name, size_t* param) {
  RTC_DCHECK(param);
  int v;
  if (rtc::GetIntFromJsonObject(root, param_name, &v)) {
    RTC_DCHECK_GE(v, 0);
    *param = v;
  }
}

void ReadParam(const Json::Value& root, std::string param_name, int* param) {
  RTC_DCHECK(param);
  int v;
  if (rtc::GetIntFromJsonObject(root, param_name, &v)) {
    *param = v;
  }
}

void ReadParam(const Json::Value& root, std::string param_name, float* param) {
  RTC_DCHECK(param);
  double v;
  if (rtc::GetDoubleFromJsonObject(root, param_name, &v)) {
    *param = static_cast<float>(v);
  }
}

void ReadParam(const Json::Value& root,
               std::string param_name,
               EchoCanceller3Config::Filter::MainConfiguration* param) {
  RTC_DCHECK(param);
  Json::Value json_array;
  if (rtc::GetValueFromJsonObject(root, param_name, &json_array)) {
    std::vector<double> v;
    rtc::JsonArrayToDoubleVector(json_array, &v);
    if (v.size() != 6) {
      RTC_LOG(LS_ERROR) << "Incorrect array size for " << param_name;
      return;
    }
    param->length_blocks = static_cast<size_t>(v[0]);
    param->leakage_converged = static_cast<float>(v[1]);
    param->leakage_diverged = static_cast<float>(v[2]);
    param->error_floor = static_cast<float>(v[3]);
    param->error_ceil = static_cast<float>(v[4]);
    param->noise_gate = static_cast<float>(v[5]);
  }
}

void ReadParam(const Json::Value& root,
               std::string param_name,
               EchoCanceller3Config::Filter::ShadowConfiguration* param) {
  RTC_DCHECK(param);
  Json::Value json_array;
  if (rtc::GetValueFromJsonObject(root, param_name, &json_array)) {
    std::vector<double> v;
    rtc::JsonArrayToDoubleVector(json_array, &v);
    if (v.size() != 3) {
      RTC_LOG(LS_ERROR) << "Incorrect array size for " << param_name;
      return;
    }
    param->length_blocks = static_cast<size_t>(v[0]);
    param->rate = static_cast<float>(v[1]);
    param->noise_gate = static_cast<float>(v[2]);
  }
}

void ReadParam(const Json::Value& root,
               std::string param_name,
               EchoCanceller3Config::Suppressor::MaskingThresholds* param) {
  RTC_DCHECK(param);
  Json::Value json_array;
  if (rtc::GetValueFromJsonObject(root, param_name, &json_array)) {
    std::vector<double> v;
    rtc::JsonArrayToDoubleVector(json_array, &v);
    if (v.size() != 3) {
      RTC_LOG(LS_ERROR) << "Incorrect array size for " << param_name;
      return;
    }
    param->enr_transparent = static_cast<float>(v[0]);
    param->enr_suppress = static_cast<float>(v[1]);
    param->emr_transparent = static_cast<float>(v[2]);
  }
}
}  // namespace

void Aec3ConfigFromJsonString(absl::string_view json_string,
                              EchoCanceller3Config* config,
                              bool* parsing_successful) {
  RTC_DCHECK(config);
  RTC_DCHECK(parsing_successful);
  EchoCanceller3Config& cfg = *config;
  cfg = EchoCanceller3Config();
  *parsing_successful = true;

  Json::Value root;
  bool success = Json::Reader().parse(std::string(json_string), root);
  if (!success) {
    RTC_LOG(LS_ERROR) << "Incorrect JSON format: " << json_string;
    *parsing_successful = false;
    return;
  }

  Json::Value aec3_root;
  success = rtc::GetValueFromJsonObject(root, "aec3", &aec3_root);
  if (!success) {
    RTC_LOG(LS_ERROR) << "Missing AEC3 config field: " << json_string;
    *parsing_successful = false;
    return;
  }

  Json::Value section;
  if (rtc::GetValueFromJsonObject(root, "buffering", &section)) {
    ReadParam(section, "use_new_render_buffering",
              &cfg.buffering.use_new_render_buffering);
    ReadParam(section, "excess_render_detection_interval_blocks",
              &cfg.buffering.excess_render_detection_interval_blocks);
    ReadParam(section, "max_allowed_excess_render_blocks",
              &cfg.buffering.max_allowed_excess_render_blocks);
  }

  if (rtc::GetValueFromJsonObject(aec3_root, "delay", &section)) {
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

  if (rtc::GetValueFromJsonObject(aec3_root, "filter", &section)) {
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

  if (rtc::GetValueFromJsonObject(aec3_root, "erle", &section)) {
    ReadParam(section, "min", &cfg.erle.min);
    ReadParam(section, "max_l", &cfg.erle.max_l);
    ReadParam(section, "max_h", &cfg.erle.max_h);
    ReadParam(section, "onset_detection", &cfg.erle.onset_detection);
    ReadParam(section, "num_sections", &cfg.erle.num_sections);
  }

  if (rtc::GetValueFromJsonObject(aec3_root, "ep_strength", &section)) {
    ReadParam(section, "lf", &cfg.ep_strength.lf);
    ReadParam(section, "mf", &cfg.ep_strength.mf);
    ReadParam(section, "hf", &cfg.ep_strength.hf);
    ReadParam(section, "default_len", &cfg.ep_strength.default_len);
    ReadParam(section, "reverb_based_on_render",
              &cfg.ep_strength.reverb_based_on_render);
    ReadParam(section, "echo_can_saturate", &cfg.ep_strength.echo_can_saturate);
    ReadParam(section, "bounded_erl", &cfg.ep_strength.bounded_erl);
  }

  if (rtc::GetValueFromJsonObject(aec3_root, "echo_audibility", &section)) {
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
    ReadParam(section, "use_stationarity_properties_at_init",
              &cfg.echo_audibility.use_stationarity_properties_at_init);
  }

  if (rtc::GetValueFromJsonObject(aec3_root, "render_levels", &section)) {
    ReadParam(section, "active_render_limit",
              &cfg.render_levels.active_render_limit);
    ReadParam(section, "poor_excitation_render_limit",
              &cfg.render_levels.poor_excitation_render_limit);
    ReadParam(section, "poor_excitation_render_limit_ds8",
              &cfg.render_levels.poor_excitation_render_limit_ds8);
  }

  if (rtc::GetValueFromJsonObject(aec3_root, "echo_removal_control",
                                  &section)) {
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

  if (rtc::GetValueFromJsonObject(aec3_root, "echo_model", &section)) {
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

  Json::Value subsection;
  if (rtc::GetValueFromJsonObject(aec3_root, "suppressor", &section)) {
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
      ReadParam(subsection, "mask_lf", &cfg.suppressor.nearend_tuning.mask_lf);
      ReadParam(subsection, "mask_hf", &cfg.suppressor.nearend_tuning.mask_hf);
      ReadParam(subsection, "max_inc_factor",
                &cfg.suppressor.nearend_tuning.max_inc_factor);
      ReadParam(subsection, "max_dec_factor_lf",
                &cfg.suppressor.nearend_tuning.max_dec_factor_lf);
    }

    if (rtc::GetValueFromJsonObject(section, "dominant_nearend_detection",
                                    &subsection)) {
      ReadParam(subsection, "enr_threshold",
                &cfg.suppressor.dominant_nearend_detection.enr_threshold);
      ReadParam(subsection, "enr_exit_threshold",
                &cfg.suppressor.dominant_nearend_detection.enr_exit_threshold);
      ReadParam(subsection, "snr_threshold",
                &cfg.suppressor.dominant_nearend_detection.snr_threshold);
      ReadParam(subsection, "hold_duration",
                &cfg.suppressor.dominant_nearend_detection.hold_duration);
      ReadParam(subsection, "trigger_threshold",
                &cfg.suppressor.dominant_nearend_detection.trigger_threshold);
      ReadParam(
          subsection, "use_during_initial_phase",
          &cfg.suppressor.dominant_nearend_detection.use_during_initial_phase);
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
}

EchoCanceller3Config Aec3ConfigFromJsonString(absl::string_view json_string) {
  EchoCanceller3Config cfg;
  bool not_used;
  Aec3ConfigFromJsonString(json_string, &cfg, &not_used);
  return cfg;
}

std::string Aec3ConfigToJsonString(const EchoCanceller3Config& config) {
  rtc::StringBuilder ost;
  ost << "{";
  ost << "\"aec3\": {";
  ost << "\"delay\": {";
  ost << "\"default_delay\": " << config.delay.default_delay << ",";
  ost << "\"down_sampling_factor\": " << config.delay.down_sampling_factor
      << ",";
  ost << "\"num_filters\": " << config.delay.num_filters << ",";
  ost << "\"api_call_jitter_blocks\": " << config.delay.api_call_jitter_blocks
      << ",";
  ost << "\"min_echo_path_delay_blocks\": "
      << config.delay.min_echo_path_delay_blocks << ",";
  ost << "\"delay_headroom_blocks\": " << config.delay.delay_headroom_blocks
      << ",";
  ost << "\"hysteresis_limit_1_blocks\": "
      << config.delay.hysteresis_limit_1_blocks << ",";
  ost << "\"hysteresis_limit_2_blocks\": "
      << config.delay.hysteresis_limit_2_blocks << ",";
  ost << "\"skew_hysteresis_blocks\": " << config.delay.skew_hysteresis_blocks
      << ",";
  ost << "\"fixed_capture_delay_samples\": "
      << config.delay.fixed_capture_delay_samples << ",";
  ost << "\"delay_estimate_smoothing\": "
      << config.delay.delay_estimate_smoothing << ",";
  ost << "\"delay_candidate_detection_threshold\": "
      << config.delay.delay_candidate_detection_threshold << ",";

  ost << "\"delay_selection_thresholds\": {";
  ost << "\"initial\": " << config.delay.delay_selection_thresholds.initial
      << ",";
  ost << "\"converged\": " << config.delay.delay_selection_thresholds.converged;
  ost << "}";

  ost << "},";

  ost << "\"filter\": {";
  ost << "\"main\": [";
  ost << config.filter.main.length_blocks << ",";
  ost << config.filter.main.leakage_converged << ",";
  ost << config.filter.main.leakage_diverged << ",";
  ost << config.filter.main.error_floor << ",";
  ost << config.filter.main.error_ceil << ",";
  ost << config.filter.main.noise_gate;
  ost << "],";

  ost << "\"shadow\": [";
  ost << config.filter.shadow.length_blocks << ",";
  ost << config.filter.shadow.rate << ",";
  ost << config.filter.shadow.noise_gate;
  ost << "],";

  ost << "\"main_initial\": [";
  ost << config.filter.main_initial.length_blocks << ",";
  ost << config.filter.main_initial.leakage_converged << ",";
  ost << config.filter.main_initial.leakage_diverged << ",";
  ost << config.filter.main_initial.error_floor << ",";
  ost << config.filter.main_initial.error_ceil << ",";
  ost << config.filter.main_initial.noise_gate;
  ost << "],";

  ost << "\"shadow_initial\": [";
  ost << config.filter.shadow_initial.length_blocks << ",";
  ost << config.filter.shadow_initial.rate << ",";
  ost << config.filter.shadow_initial.noise_gate;
  ost << "],";

  ost << "\"config_change_duration_blocks\": "
      << config.filter.config_change_duration_blocks << ",";
  ost << "\"initial_state_seconds\": " << config.filter.initial_state_seconds
      << ",";
  ost << "\"conservative_initial_phase\": "
      << (config.filter.conservative_initial_phase ? "true" : "false") << ",";
  ost << "\"enable_shadow_filter_output_usage\": "
      << (config.filter.enable_shadow_filter_output_usage ? "true" : "false");

  ost << "},";

  ost << "\"erle\": {";
  ost << "\"min\": " << config.erle.min << ",";
  ost << "\"max_l\": " << config.erle.max_l << ",";
  ost << "\"max_h\": " << config.erle.max_h << ",";
  ost << "\"onset_detection\": "
      << (config.erle.onset_detection ? "true" : "false") << ",";
  ost << "\"num_sections\": " << config.erle.num_sections;
  ost << "},";

  ost << "\"ep_strength\": {";
  ost << "\"lf\": " << config.ep_strength.lf << ",";
  ost << "\"mf\": " << config.ep_strength.mf << ",";
  ost << "\"hf\": " << config.ep_strength.hf << ",";
  ost << "\"default_len\": " << config.ep_strength.default_len << ",";
  ost << "\"reverb_based_on_render\": "
      << (config.ep_strength.reverb_based_on_render ? "true" : "false") << ",";
  ost << "\"echo_can_saturate\": "
      << (config.ep_strength.echo_can_saturate ? "true" : "false") << ",";
  ost << "\"bounded_erl\": "
      << (config.ep_strength.bounded_erl ? "true" : "false");

  ost << "},";

  ost << "\"echo_audibility\": {";
  ost << "\"low_render_limit\": " << config.echo_audibility.low_render_limit
      << ",";
  ost << "\"normal_render_limit\": "
      << config.echo_audibility.normal_render_limit << ",";
  ost << "\"floor_power\": " << config.echo_audibility.floor_power << ",";
  ost << "\"audibility_threshold_lf\": "
      << config.echo_audibility.audibility_threshold_lf << ",";
  ost << "\"audibility_threshold_mf\": "
      << config.echo_audibility.audibility_threshold_mf << ",";
  ost << "\"audibility_threshold_hf\": "
      << config.echo_audibility.audibility_threshold_hf << ",";
  ost << "\"use_stationary_properties\": "
      << (config.echo_audibility.use_stationary_properties ? "true" : "false")
      << ",";
  ost << "\"use_stationarity_properties_at_init\": "
      << (config.echo_audibility.use_stationarity_properties_at_init ? "true"
                                                                     : "false");
  ost << "},";

  ost << "\"render_levels\": {";
  ost << "\"active_render_limit\": " << config.render_levels.active_render_limit
      << ",";
  ost << "\"poor_excitation_render_limit\": "
      << config.render_levels.poor_excitation_render_limit << ",";
  ost << "\"poor_excitation_render_limit_ds8\": "
      << config.render_levels.poor_excitation_render_limit_ds8;
  ost << "},";

  ost << "\"echo_removal_control\": {";
  ost << "\"gain_rampup\": {";
  ost << "\"initial_gain\": "
      << config.echo_removal_control.gain_rampup.initial_gain << ",";
  ost << "\"first_non_zero_gain\": "
      << config.echo_removal_control.gain_rampup.first_non_zero_gain << ",";
  ost << "\"non_zero_gain_blocks\": "
      << config.echo_removal_control.gain_rampup.non_zero_gain_blocks << ",";
  ost << "\"full_gain_blocks\": "
      << config.echo_removal_control.gain_rampup.full_gain_blocks;
  ost << "},";
  ost << "\"has_clock_drift\": "
      << (config.echo_removal_control.has_clock_drift ? "true" : "false")
      << ",";
  ost << "\"linear_and_stable_echo_path\": "
      << (config.echo_removal_control.linear_and_stable_echo_path ? "true"
                                                                  : "false");

  ost << "},";

  ost << "\"echo_model\": {";
  ost << "\"noise_floor_hold\": " << config.echo_model.noise_floor_hold << ",";
  ost << "\"min_noise_floor_power\": "
      << config.echo_model.min_noise_floor_power << ",";
  ost << "\"stationary_gate_slope\": "
      << config.echo_model.stationary_gate_slope << ",";
  ost << "\"noise_gate_power\": " << config.echo_model.noise_gate_power << ",";
  ost << "\"noise_gate_slope\": " << config.echo_model.noise_gate_slope << ",";
  ost << "\"render_pre_window_size\": "
      << config.echo_model.render_pre_window_size << ",";
  ost << "\"render_post_window_size\": "
      << config.echo_model.render_post_window_size << ",";
  ost << "\"render_pre_window_size_init\": "
      << config.echo_model.render_pre_window_size_init << ",";
  ost << "\"render_post_window_size_init\": "
      << config.echo_model.render_post_window_size_init << ",";
  ost << "\"nonlinear_hold\": " << config.echo_model.nonlinear_hold << ",";
  ost << "\"nonlinear_release\": " << config.echo_model.nonlinear_release;
  ost << "},";

  ost << "\"suppressor\": {";
  ost << "\"nearend_average_blocks\": "
      << config.suppressor.nearend_average_blocks << ",";
  ost << "\"normal_tuning\": {";
  ost << "\"mask_lf\": [";
  ost << config.suppressor.normal_tuning.mask_lf.enr_transparent << ",";
  ost << config.suppressor.normal_tuning.mask_lf.enr_suppress << ",";
  ost << config.suppressor.normal_tuning.mask_lf.emr_transparent;
  ost << "],";
  ost << "\"mask_hf\": [";
  ost << config.suppressor.normal_tuning.mask_hf.enr_transparent << ",";
  ost << config.suppressor.normal_tuning.mask_hf.enr_suppress << ",";
  ost << config.suppressor.normal_tuning.mask_hf.emr_transparent;
  ost << "],";
  ost << "\"max_inc_factor\": "
      << config.suppressor.normal_tuning.max_inc_factor << ",";
  ost << "\"max_dec_factor_lf\": "
      << config.suppressor.normal_tuning.max_dec_factor_lf;
  ost << "},";
  ost << "\"nearend_tuning\": {";
  ost << "\"mask_lf\": [";
  ost << config.suppressor.nearend_tuning.mask_lf.enr_transparent << ",";
  ost << config.suppressor.nearend_tuning.mask_lf.enr_suppress << ",";
  ost << config.suppressor.nearend_tuning.mask_lf.emr_transparent;
  ost << "],";
  ost << "\"mask_hf\": [";
  ost << config.suppressor.nearend_tuning.mask_hf.enr_transparent << ",";
  ost << config.suppressor.nearend_tuning.mask_hf.enr_suppress << ",";
  ost << config.suppressor.nearend_tuning.mask_hf.emr_transparent;
  ost << "],";
  ost << "\"max_inc_factor\": "
      << config.suppressor.nearend_tuning.max_inc_factor << ",";
  ost << "\"max_dec_factor_lf\": "
      << config.suppressor.nearend_tuning.max_dec_factor_lf;
  ost << "},";
  ost << "\"dominant_nearend_detection\": {";
  ost << "\"enr_threshold\": "
      << config.suppressor.dominant_nearend_detection.enr_threshold << ",";
  ost << "\"enr_exit_threshold\": "
      << config.suppressor.dominant_nearend_detection.enr_exit_threshold << ",";
  ost << "\"snr_threshold\": "
      << config.suppressor.dominant_nearend_detection.snr_threshold << ",";
  ost << "\"hold_duration\": "
      << config.suppressor.dominant_nearend_detection.hold_duration << ",";
  ost << "\"trigger_threshold\": "
      << config.suppressor.dominant_nearend_detection.trigger_threshold << ",";
  ost << "\"use_during_initial_phase\": "
      << config.suppressor.dominant_nearend_detection.use_during_initial_phase;
  ost << "},";
  ost << "\"high_bands_suppression\": {";
  ost << "\"enr_threshold\": "
      << config.suppressor.high_bands_suppression.enr_threshold << ",";
  ost << "\"max_gain_during_echo\": "
      << config.suppressor.high_bands_suppression.max_gain_during_echo;
  ost << "},";
  ost << "\"floor_first_increase\": " << config.suppressor.floor_first_increase
      << ",";
  ost << "\"enforce_transparent\": "
      << (config.suppressor.enforce_transparent ? "true" : "false") << ",";
  ost << "\"enforce_empty_higher_bands\": "
      << (config.suppressor.enforce_empty_higher_bands ? "true" : "false");
  ost << "}";
  ost << "}";
  ost << "}";

  return ost.Release();
}
}  // namespace webrtc
