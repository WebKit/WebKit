/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/rate_control_settings.h"

#include <inttypes.h>
#include <stdio.h>

#include <string>

#include "absl/strings/match.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"

namespace webrtc {

namespace {

const int kDefaultAcceptedQueueMs = 350;

const int kDefaultMinPushbackTargetBitrateBps = 30000;

const char kCongestionWindowDefaultFieldTrialString[] =
    "QueueSize:350,MinBitrate:30000,DropFrame:true";

const char kUseBaseHeavyVp8Tl3RateAllocationFieldTrialName[] =
    "WebRTC-UseBaseHeavyVP8TL3RateAllocation";

}  // namespace

constexpr char CongestionWindowConfig::kKey[];

std::unique_ptr<StructParametersParser> CongestionWindowConfig::Parser() {
  return StructParametersParser::Create("QueueSize", &queue_size_ms,  //
                                        "MinBitrate", &min_bitrate_bps,
                                        "InitWin", &initial_data_window,
                                        "DropFrame", &drop_frame_only);
}

// static
CongestionWindowConfig CongestionWindowConfig::Parse(absl::string_view config) {
  CongestionWindowConfig res;
  res.Parser()->Parse(config);
  return res;
}

constexpr char VideoRateControlConfig::kKey[];

std::unique_ptr<StructParametersParser> VideoRateControlConfig::Parser() {
  // The empty comments ensures that each pair is on a separate line.
  return StructParametersParser::Create(
      "pacing_factor", &pacing_factor,                  //
      "alr_probing", &alr_probing,                      //
      "vp8_qp_max", &vp8_qp_max,                        //
      "vp8_min_pixels", &vp8_min_pixels,                //
      "trust_vp8", &trust_vp8,                          //
      "trust_vp9", &trust_vp9,                          //
      "bitrate_adjuster", &bitrate_adjuster,            //
      "adjuster_use_headroom", &adjuster_use_headroom,  //
      "vp8_s0_boost", &vp8_s0_boost,                    //
      "vp8_base_heavy_tl3_alloc", &vp8_base_heavy_tl3_alloc);
}

RateControlSettings::RateControlSettings(
    const FieldTrialsView& key_value_config) {
  std::string congestion_window_config =
      key_value_config.Lookup(CongestionWindowConfig::kKey);
  if (congestion_window_config.empty()) {
    congestion_window_config = kCongestionWindowDefaultFieldTrialString;
  }
  congestion_window_config_ =
      CongestionWindowConfig::Parse(congestion_window_config);
  video_config_.vp8_base_heavy_tl3_alloc = key_value_config.IsEnabled(
      kUseBaseHeavyVp8Tl3RateAllocationFieldTrialName);
  video_config_.Parser()->Parse(
      key_value_config.Lookup(VideoRateControlConfig::kKey));
}

RateControlSettings::~RateControlSettings() = default;
RateControlSettings::RateControlSettings(RateControlSettings&&) = default;

bool RateControlSettings::UseCongestionWindow() const {
  return static_cast<bool>(congestion_window_config_.queue_size_ms);
}

int64_t RateControlSettings::GetCongestionWindowAdditionalTimeMs() const {
  return congestion_window_config_.queue_size_ms.value_or(
      kDefaultAcceptedQueueMs);
}

bool RateControlSettings::UseCongestionWindowPushback() const {
  return congestion_window_config_.queue_size_ms &&
         congestion_window_config_.min_bitrate_bps;
}

bool RateControlSettings::UseCongestionWindowDropFrameOnly() const {
  return congestion_window_config_.drop_frame_only;
}

uint32_t RateControlSettings::CongestionWindowMinPushbackTargetBitrateBps()
    const {
  return congestion_window_config_.min_bitrate_bps.value_or(
      kDefaultMinPushbackTargetBitrateBps);
}

std::optional<DataSize> RateControlSettings::CongestionWindowInitialDataWindow()
    const {
  return congestion_window_config_.initial_data_window;
}

std::optional<double> RateControlSettings::GetPacingFactor() const {
  return video_config_.pacing_factor;
}

bool RateControlSettings::UseAlrProbing() const {
  return video_config_.alr_probing;
}

std::optional<int> RateControlSettings::LibvpxVp8QpMax() const {
  if (video_config_.vp8_qp_max &&
      (*video_config_.vp8_qp_max < 0 || *video_config_.vp8_qp_max > 63)) {
    RTC_LOG(LS_WARNING) << "Unsupported vp8_qp_max_ value, ignored.";
    return std::nullopt;
  }
  return video_config_.vp8_qp_max;
}

std::optional<int> RateControlSettings::LibvpxVp8MinPixels() const {
  if (video_config_.vp8_min_pixels && *video_config_.vp8_min_pixels < 1) {
    return std::nullopt;
  }
  return video_config_.vp8_min_pixels;
}

bool RateControlSettings::LibvpxVp8TrustedRateController() const {
  return video_config_.trust_vp8;
}

bool RateControlSettings::Vp8BoostBaseLayerQuality() const {
  return video_config_.vp8_s0_boost;
}

bool RateControlSettings::LibvpxVp9TrustedRateController() const {
  return video_config_.trust_vp9;
}

bool RateControlSettings::Vp8BaseHeavyTl3RateAllocation() const {
  return video_config_.vp8_base_heavy_tl3_alloc;
}

bool RateControlSettings::UseEncoderBitrateAdjuster() const {
  return video_config_.bitrate_adjuster;
}

bool RateControlSettings::BitrateAdjusterCanUseNetworkHeadroom() const {
  return video_config_.adjuster_use_headroom;
}

}  // namespace webrtc
