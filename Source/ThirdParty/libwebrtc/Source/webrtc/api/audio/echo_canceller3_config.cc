/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/audio/echo_canceller3_config.h"

namespace webrtc {

EchoCanceller3Config::EchoCanceller3Config() = default;
EchoCanceller3Config::EchoCanceller3Config(const EchoCanceller3Config& e) =
    default;
EchoCanceller3Config::Delay::Delay() = default;
EchoCanceller3Config::Delay::Delay(const EchoCanceller3Config::Delay& e) =
    default;

EchoCanceller3Config::Mask::Mask() = default;
EchoCanceller3Config::Mask::Mask(const EchoCanceller3Config::Mask& m) = default;

EchoCanceller3Config::EchoModel::EchoModel() = default;
EchoCanceller3Config::EchoModel::EchoModel(
    const EchoCanceller3Config::EchoModel& e) = default;

EchoCanceller3Config::Suppressor::Suppressor() = default;
EchoCanceller3Config::Suppressor::Suppressor(
    const EchoCanceller3Config::Suppressor& e) = default;

EchoCanceller3Config::Suppressor::MaskingThresholds::MaskingThresholds(
    float enr_transparent,
    float enr_suppress,
    float emr_transparent)
    : enr_transparent(enr_transparent),
      enr_suppress(enr_suppress),
      emr_transparent(emr_transparent) {}
EchoCanceller3Config::Suppressor::Suppressor::MaskingThresholds::
    MaskingThresholds(
        const EchoCanceller3Config::Suppressor::MaskingThresholds& e) = default;

EchoCanceller3Config::Suppressor::Tuning::Tuning(MaskingThresholds mask_lf,
                                                 MaskingThresholds mask_hf,
                                                 float max_inc_factor,
                                                 float max_dec_factor_lf)
    : mask_lf(mask_lf),
      mask_hf(mask_hf),
      max_inc_factor(max_inc_factor),
      max_dec_factor_lf(max_dec_factor_lf) {}
EchoCanceller3Config::Suppressor::Tuning::Tuning(
    const EchoCanceller3Config::Suppressor::Tuning& e) = default;

}  // namespace webrtc
