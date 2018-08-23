/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_AUDIO_ECHO_CANCELLER3_CONFIG_H_
#define API_AUDIO_ECHO_CANCELLER3_CONFIG_H_

#include <stddef.h>  // size_t

namespace webrtc {

// Configuration struct for EchoCanceller3
struct EchoCanceller3Config {
  EchoCanceller3Config();
  EchoCanceller3Config(const EchoCanceller3Config& e);
  struct Delay {
    size_t default_delay = 5;
    size_t down_sampling_factor = 4;
    size_t num_filters = 6;
    size_t api_call_jitter_blocks = 26;
    size_t min_echo_path_delay_blocks = 0;
    size_t delay_headroom_blocks = 2;
    size_t hysteresis_limit_1_blocks = 1;
    size_t hysteresis_limit_2_blocks = 1;
    size_t skew_hysteresis_blocks = 3;
  } delay;

  struct Filter {
    struct MainConfiguration {
      size_t length_blocks;
      float leakage_converged;
      float leakage_diverged;
      float error_floor;
      float noise_gate;
    };

    struct ShadowConfiguration {
      size_t length_blocks;
      float rate;
      float noise_gate;
    };

    MainConfiguration main = {13, 0.00005f, 0.01f, 0.1f, 20075344.f};
    ShadowConfiguration shadow = {13, 0.7f, 20075344.f};

    MainConfiguration main_initial = {12, 0.005f, 0.5f, 0.001f, 20075344.f};
    ShadowConfiguration shadow_initial = {12, 0.9f, 20075344.f};

    size_t config_change_duration_blocks = 250;
  } filter;

  struct Erle {
    float min = 1.f;
    float max_l = 4.f;
    float max_h = 1.5f;
  } erle;

  struct EpStrength {
    float lf = 1.f;
    float mf = 1.f;
    float hf = 1.f;
    float default_len = 0.88f;
    bool reverb_based_on_render = true;
    bool echo_can_saturate = true;
    bool bounded_erl = false;
  } ep_strength;

  struct Mask {
    Mask();
    Mask(const Mask& m);
    float m0 = 0.1f;
    float m1 = 0.01f;
    float m2 = 0.0001f;
    float m3 = 0.01f;
    float m5 = 0.01f;
    float m6 = 0.0001f;
    float m7 = 0.01f;
    float m8 = 0.0001f;
    float m9 = 0.1f;

    float gain_curve_offset = 1.45f;
    float gain_curve_slope = 5.f;
    float temporal_masking_lf = 0.9f;
    float temporal_masking_hf = 0.6f;
    size_t temporal_masking_lf_bands = 3;
  } gain_mask;

  struct EchoAudibility {
    float low_render_limit = 4 * 64.f;
    float normal_render_limit = 64.f;
    float floor_power = 2 * 64.f;
    float audibility_threshold_lf = 10;
    float audibility_threshold_mf = 10;
    float audibility_threshold_hf = 10;
    bool use_stationary_properties = true;
  } echo_audibility;

  struct RenderLevels {
    float active_render_limit = 100.f;
    float poor_excitation_render_limit = 150.f;
    float poor_excitation_render_limit_ds8 = 20.f;
  } render_levels;

  struct GainUpdates {
    struct GainChanges {
      float max_inc;
      float max_dec;
      float rate_inc;
      float rate_dec;
      float min_inc;
      float min_dec;
    };

    GainChanges low_noise = {2.f, 2.f, 1.4f, 1.4f, 1.1f, 1.1f};
    GainChanges initial = {2.f, 2.f, 1.5f, 1.5f, 1.2f, 1.2f};
    GainChanges normal = {2.f, 2.f, 1.5f, 1.5f, 1.2f, 1.2f};
    GainChanges saturation = {1.2f, 1.2f, 1.5f, 1.5f, 1.f, 1.f};
    GainChanges nonlinear = {1.5f, 1.5f, 1.2f, 1.2f, 1.1f, 1.1f};

    float max_inc_factor = 2.0f;
    float max_dec_factor_lf = 0.25f;
    float floor_first_increase = 0.00001f;
  } gain_updates;

  struct EchoRemovalControl {
    struct GainRampup {
      float initial_gain = 0.0f;
      float first_non_zero_gain = 0.001f;
      int non_zero_gain_blocks = 187;
      int full_gain_blocks = 312;
    } gain_rampup;
    bool has_clock_drift = false;
    bool linear_and_stable_echo_path = false;
  } echo_removal_control;

  struct EchoModel {
    EchoModel();
    EchoModel(const EchoModel& e);
    size_t noise_floor_hold = 50;
    float min_noise_floor_power = 1638400.f;
    float stationary_gate_slope = 10.f;
    float noise_gate_power = 27509.42f;
    float noise_gate_slope = 0.3f;
    size_t render_pre_window_size = 1;
    size_t render_post_window_size = 1;
    size_t render_pre_window_size_init = 10;
    size_t render_post_window_size_init = 10;
    float nonlinear_hold = 1;
    float nonlinear_release = 0.001f;
  } echo_model;

  struct Suppressor {
    size_t bands_with_reliable_coherence = 5;
    size_t nearend_average_blocks = 4;

    struct MaskingThresholds {
      float enr_transparent;
      float enr_suppress;
      float emr_transparent;
    };
    MaskingThresholds mask_lf = {.2f, .3f, .3f};
    MaskingThresholds mask_hf = {.07f, .1f, .3f};
  } suppressor;
};
}  // namespace webrtc

#endif  // API_AUDIO_ECHO_CANCELLER3_CONFIG_H_
