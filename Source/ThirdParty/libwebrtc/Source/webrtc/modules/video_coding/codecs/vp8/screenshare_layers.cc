/* Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/vp8/screenshare_layers.h"

#include <stdlib.h>

#include <algorithm>
#include <memory>

#include "modules/video_coding/include/video_codec_interface.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {
namespace {
static const int kOneSecond90Khz = 90000;
static const int kMinTimeBetweenSyncs = kOneSecond90Khz * 2;
static const int kMaxTimeBetweenSyncs = kOneSecond90Khz * 4;
static const int kQpDeltaThresholdForSync = 8;
static const int kMinBitrateKbpsForQpBoost = 500;
}  // namespace

const double ScreenshareLayers::kMaxTL0FpsReduction = 2.5;
const double ScreenshareLayers::kAcceptableTargetOvershoot = 2.0;

constexpr int ScreenshareLayers::kMaxNumTemporalLayers;

// Always emit a frame with certain interval, even if bitrate targets have
// been exceeded. This prevents needless keyframe requests.
const int ScreenshareLayers::kMaxFrameIntervalMs = 2750;

ScreenshareLayers::ScreenshareLayers(int num_temporal_layers, Clock* clock)
    : clock_(clock),
      number_of_temporal_layers_(
          std::min(kMaxNumTemporalLayers, num_temporal_layers)),
      active_layer_(-1),
      last_timestamp_(-1),
      last_sync_timestamp_(-1),
      last_emitted_tl0_timestamp_(-1),
      last_frame_time_ms_(-1),
      min_qp_(-1),
      max_qp_(-1),
      max_debt_bytes_(0),
      encode_framerate_(1000.0f, 1000.0f),  // 1 second window, second scale.
      bitrate_updated_(false),
      checker_(TemporalLayersChecker::CreateTemporalLayersChecker(
          Vp8TemporalLayersType::kBitrateDynamic,
          num_temporal_layers)) {
  RTC_CHECK_GT(number_of_temporal_layers_, 0);
  RTC_CHECK_LE(number_of_temporal_layers_, kMaxNumTemporalLayers);
}

ScreenshareLayers::~ScreenshareLayers() {
  UpdateHistograms();
}

bool ScreenshareLayers::SupportsEncoderFrameDropping() const {
  // Frame dropping is handled internally by this class.
  return false;
}

Vp8TemporalLayers::FrameConfig ScreenshareLayers::UpdateLayerConfig(
    uint32_t timestamp) {
  auto it = pending_frame_configs_.find(timestamp);
  if (it != pending_frame_configs_.end()) {
    // Drop and re-encode, reuse the previous config.
    return it->second;
  }

  if (number_of_temporal_layers_ <= 1) {
    // No flags needed for 1 layer screenshare.
    // TODO(pbos): Consider updating only last, and not all buffers.
    Vp8TemporalLayers::FrameConfig tl_config(
        kReferenceAndUpdate, kReferenceAndUpdate, kReferenceAndUpdate);
    pending_frame_configs_[timestamp] = tl_config;
    return tl_config;
  }

  const int64_t now_ms = clock_->TimeInMilliseconds();

  int64_t unwrapped_timestamp = time_wrap_handler_.Unwrap(timestamp);
  int64_t ts_diff;
  if (last_timestamp_ == -1) {
    ts_diff = kOneSecond90Khz / capture_framerate_.value_or(*target_framerate_);
  } else {
    ts_diff = unwrapped_timestamp - last_timestamp_;
  }

  if (target_framerate_) {
    // If input frame rate exceeds target frame rate, either over a one second
    // averaging window, or if frame interval is below 90% of desired value,
    // drop frame.
    if (encode_framerate_.Rate(now_ms).value_or(0) > *target_framerate_)
      return Vp8TemporalLayers::FrameConfig(kNone, kNone, kNone);

    // Primarily check if frame interval is too short using frame timestamps,
    // as if they are correct they won't be affected by queuing in webrtc.
    const int64_t expected_frame_interval_90khz =
        kOneSecond90Khz / *target_framerate_;
    if (last_timestamp_ != -1 && ts_diff > 0) {
      if (ts_diff < 85 * expected_frame_interval_90khz / 100) {
        return Vp8TemporalLayers::FrameConfig(kNone, kNone, kNone);
      }
    } else {
      // Timestamps looks off, use realtime clock here instead.
      const int64_t expected_frame_interval_ms = 1000 / *target_framerate_;
      if (last_frame_time_ms_ != -1 &&
          now_ms - last_frame_time_ms_ <
              (85 * expected_frame_interval_ms) / 100) {
        return Vp8TemporalLayers::FrameConfig(kNone, kNone, kNone);
      }
    }
  }

  if (stats_.first_frame_time_ms_ == -1)
    stats_.first_frame_time_ms_ = now_ms;

  // Make sure both frame droppers leak out bits.
  layers_[0].UpdateDebt(ts_diff / 90);
  layers_[1].UpdateDebt(ts_diff / 90);
  last_timestamp_ = timestamp;
  last_frame_time_ms_ = now_ms;

  TemporalLayerState layer_state = TemporalLayerState::kDrop;

  if (active_layer_ == -1 ||
      layers_[active_layer_].state != TemporalLayer::State::kDropped) {
    if (last_emitted_tl0_timestamp_ != -1 &&
        (unwrapped_timestamp - last_emitted_tl0_timestamp_) / 90 >
            kMaxFrameIntervalMs) {
      // Too long time has passed since the last frame was emitted, cancel
      // enough debt to allow a single frame.
      layers_[0].debt_bytes_ = max_debt_bytes_ - 1;
    }
    if (layers_[0].debt_bytes_ > max_debt_bytes_) {
      // Must drop TL0, encode TL1 instead.
      if (layers_[1].debt_bytes_ > max_debt_bytes_) {
        // Must drop both TL0 and TL1.
        active_layer_ = -1;
      } else {
        active_layer_ = 1;
      }
    } else {
      active_layer_ = 0;
    }
  }

  switch (active_layer_) {
    case 0:
      layer_state = TemporalLayerState::kTl0;
      last_emitted_tl0_timestamp_ = unwrapped_timestamp;
      break;
    case 1:
      if (layers_[1].state != TemporalLayer::State::kDropped) {
        if (TimeToSync(unwrapped_timestamp) ||
            layers_[1].state == TemporalLayer::State::kKeyFrame) {
          last_sync_timestamp_ = unwrapped_timestamp;
          layer_state = TemporalLayerState::kTl1Sync;
        } else {
          layer_state = TemporalLayerState::kTl1;
        }
      } else {
        layer_state = last_sync_timestamp_ == unwrapped_timestamp
                          ? TemporalLayerState::kTl1Sync
                          : TemporalLayerState::kTl1;
      }
      break;
    case -1:
      layer_state = TemporalLayerState::kDrop;
      ++stats_.num_dropped_frames_;
      break;
    default:
      RTC_NOTREACHED();
  }

  Vp8TemporalLayers::FrameConfig tl_config;
  // TODO(pbos): Consider referencing but not updating the 'alt' buffer for all
  // layers.
  switch (layer_state) {
    case TemporalLayerState::kDrop:
      tl_config = Vp8TemporalLayers::FrameConfig(kNone, kNone, kNone);
      break;
    case TemporalLayerState::kTl0:
      // TL0 only references and updates 'last'.
      tl_config =
          Vp8TemporalLayers::FrameConfig(kReferenceAndUpdate, kNone, kNone);
      tl_config.packetizer_temporal_idx = 0;
      break;
    case TemporalLayerState::kTl1:
      // TL1 references both 'last' and 'golden' but only updates 'golden'.
      tl_config = Vp8TemporalLayers::FrameConfig(kReference,
                                                 kReferenceAndUpdate, kNone);
      tl_config.packetizer_temporal_idx = 1;
      break;
    case TemporalLayerState::kTl1Sync:
      // Predict from only TL0 to allow participants to switch to the high
      // bitrate stream. Updates 'golden' so that TL1 can continue to refer to
      // and update 'golden' from this point on.
      tl_config = Vp8TemporalLayers::FrameConfig(kReference, kUpdate, kNone);
      tl_config.packetizer_temporal_idx = 1;
      break;
  }

  tl_config.layer_sync = layer_state == TemporalLayerState::kTl1Sync;
  pending_frame_configs_[timestamp] = tl_config;
  return tl_config;
}

void ScreenshareLayers::OnRatesUpdated(
    const std::vector<uint32_t>& bitrates_bps,
    int framerate_fps) {
  RTC_DCHECK_GT(framerate_fps, 0);
  RTC_DCHECK_GE(bitrates_bps.size(), 1);
  RTC_DCHECK_LE(bitrates_bps.size(), 2);

  // |bitrates_bps| uses individual rates per layer, but we want to use the
  // accumulated rate here.
  uint32_t tl0_kbps = bitrates_bps[0] / 1000;
  uint32_t tl1_kbps = tl0_kbps;
  if (bitrates_bps.size() > 1) {
    tl1_kbps += bitrates_bps[1] / 1000;
  }

  if (!target_framerate_) {
    // First OnRatesUpdated() is called during construction, with the
    // configured targets as parameters.
    target_framerate_ = framerate_fps;
    capture_framerate_ = target_framerate_;
    bitrate_updated_ = true;
  } else {
    if ((capture_framerate_ &&
         framerate_fps != static_cast<int>(*capture_framerate_)) ||
        (tl0_kbps != layers_[0].target_rate_kbps_) ||
        (tl1_kbps != layers_[1].target_rate_kbps_)) {
      bitrate_updated_ = true;
    }

    if (framerate_fps < 0) {
      capture_framerate_.reset();
    } else {
      capture_framerate_ = framerate_fps;
    }
  }

  layers_[0].target_rate_kbps_ = tl0_kbps;
  layers_[1].target_rate_kbps_ = tl1_kbps;
}

void ScreenshareLayers::OnEncodeDone(uint32_t rtp_timestamp,
                                     size_t size_bytes,
                                     bool is_keyframe,
                                     int qp,
                                     CodecSpecificInfoVP8* vp8_info) {
  if (size_bytes == 0) {
    layers_[active_layer_].state = TemporalLayer::State::kDropped;
    ++stats_.num_overshoots_;
    return;
  }

  absl::optional<FrameConfig> frame_config;
  auto it = pending_frame_configs_.find(rtp_timestamp);
  if (it != pending_frame_configs_.end()) {
    frame_config = it->second;
    pending_frame_configs_.erase(it);

    if (checker_) {
      RTC_DCHECK(checker_->CheckTemporalConfig(is_keyframe, *frame_config));
    }
  }

  if (number_of_temporal_layers_ == 1) {
    vp8_info->temporalIdx = kNoTemporalIdx;
    vp8_info->layerSync = false;
  } else {
    int64_t unwrapped_timestamp = time_wrap_handler_.Unwrap(rtp_timestamp);
    if (frame_config) {
      vp8_info->temporalIdx = frame_config->packetizer_temporal_idx;
      vp8_info->layerSync = frame_config->layer_sync;
    } else {
      // Frame requested to be dropped, but was not. Fall back to base-layer.
      vp8_info->temporalIdx = 0;
      vp8_info->layerSync = false;
    }
    if (is_keyframe) {
      vp8_info->temporalIdx = 0;
      last_sync_timestamp_ = unwrapped_timestamp;
      vp8_info->layerSync = true;
      layers_[0].state = TemporalLayer::State::kKeyFrame;
      layers_[1].state = TemporalLayer::State::kKeyFrame;
      active_layer_ = 1;
    }
  }

  encode_framerate_.Update(1, clock_->TimeInMilliseconds());

  if (number_of_temporal_layers_ == 1)
    return;

  RTC_DCHECK_NE(-1, active_layer_);
  if (layers_[active_layer_].state == TemporalLayer::State::kDropped) {
    layers_[active_layer_].state = TemporalLayer::State::kQualityBoost;
  }

  if (qp != -1)
    layers_[active_layer_].last_qp = qp;

  if (active_layer_ == 0) {
    layers_[0].debt_bytes_ += size_bytes;
    layers_[1].debt_bytes_ += size_bytes;
    ++stats_.num_tl0_frames_;
    stats_.tl0_target_bitrate_sum_ += layers_[0].target_rate_kbps_;
    stats_.tl0_qp_sum_ += qp;
  } else if (active_layer_ == 1) {
    layers_[1].debt_bytes_ += size_bytes;
    ++stats_.num_tl1_frames_;
    stats_.tl1_target_bitrate_sum_ += layers_[1].target_rate_kbps_;
    stats_.tl1_qp_sum_ += qp;
  }
}

bool ScreenshareLayers::TimeToSync(int64_t timestamp) const {
  RTC_DCHECK_EQ(1, active_layer_);
  RTC_DCHECK_NE(-1, layers_[0].last_qp);
  if (layers_[1].last_qp == -1) {
    // First frame in TL1 should only depend on TL0 since there are no
    // previous frames in TL1.
    return true;
  }

  RTC_DCHECK_NE(-1, last_sync_timestamp_);
  int64_t timestamp_diff = timestamp - last_sync_timestamp_;
  if (timestamp_diff > kMaxTimeBetweenSyncs) {
    // After a certain time, force a sync frame.
    return true;
  } else if (timestamp_diff < kMinTimeBetweenSyncs) {
    // If too soon from previous sync frame, don't issue a new one.
    return false;
  }
  // Issue a sync frame if difference in quality between TL0 and TL1 isn't too
  // large.
  if (layers_[0].last_qp - layers_[1].last_qp < kQpDeltaThresholdForSync)
    return true;
  return false;
}

uint32_t ScreenshareLayers::GetCodecTargetBitrateKbps() const {
  uint32_t target_bitrate_kbps = layers_[0].target_rate_kbps_;

  if (number_of_temporal_layers_ > 1) {
    // Calculate a codec target bitrate. This may be higher than TL0, gaining
    // quality at the expense of frame rate at TL0. Constraints:
    // - TL0 frame rate no less than framerate / kMaxTL0FpsReduction.
    // - Target rate * kAcceptableTargetOvershoot should not exceed TL1 rate.
    target_bitrate_kbps =
        std::min(layers_[0].target_rate_kbps_ * kMaxTL0FpsReduction,
                 layers_[1].target_rate_kbps_ / kAcceptableTargetOvershoot);
  }

  return std::max(layers_[0].target_rate_kbps_, target_bitrate_kbps);
}

bool ScreenshareLayers::UpdateConfiguration(Vp8EncoderConfig* cfg) {
  if (min_qp_ == -1 || max_qp_ == -1) {
    // Store the valid qp range. This must not change during the lifetime of
    // this class.
    min_qp_ = cfg->rc_min_quantizer;
    max_qp_ = cfg->rc_max_quantizer;
  }

  bool cfg_updated = false;
  uint32_t target_bitrate_kbps = GetCodecTargetBitrateKbps();

  // TODO(sprang): We _really_ need to make an overhaul of this class. :(
  // If we're dropping frames in order to meet a target framerate, adjust the
  // bitrate assigned to the encoder so the total average bitrate is correct.
  float encoder_config_bitrate_kbps = target_bitrate_kbps;
  if (target_framerate_ && capture_framerate_ &&
      *target_framerate_ < *capture_framerate_) {
    encoder_config_bitrate_kbps *=
        static_cast<float>(*capture_framerate_) / *target_framerate_;
  }

  if (bitrate_updated_ ||
      cfg->rc_target_bitrate != encoder_config_bitrate_kbps) {
    cfg->rc_target_bitrate = encoder_config_bitrate_kbps;

    // Don't reconfigure qp limits during quality boost frames.
    if (active_layer_ == -1 ||
        layers_[active_layer_].state != TemporalLayer::State::kQualityBoost) {
      // After a dropped frame, a frame with max qp will be encoded and the
      // quality will then ramp up from there. To boost the speed of recovery,
      // encode the next frame with lower max qp, if there is sufficient
      // bandwidth to do so without causing excessive delay.
      // TL0 is the most important to improve since the errors in this layer
      // will propagate to TL1.
      // Currently, reduce max qp by 20% for TL0 and 15% for TL1.
      if (layers_[1].target_rate_kbps_ >= kMinBitrateKbpsForQpBoost) {
        layers_[0].enhanced_max_qp =
            min_qp_ + (((max_qp_ - min_qp_) * 80) / 100);
        layers_[1].enhanced_max_qp =
            min_qp_ + (((max_qp_ - min_qp_) * 85) / 100);
      } else {
        layers_[0].enhanced_max_qp = -1;
        layers_[1].enhanced_max_qp = -1;
      }
    }

    if (capture_framerate_) {
      int avg_frame_size =
          (target_bitrate_kbps * 1000) / (8 * *capture_framerate_);
      // Allow max debt to be the size of a single optimal frame.
      // TODO(sprang): Determine if this needs to be adjusted by some factor.
      // (Lower values may cause more frame drops, higher may lead to queuing
      // delays.)
      max_debt_bytes_ = avg_frame_size;
    }

    bitrate_updated_ = false;
    cfg_updated = true;
  }

  // Don't try to update boosts state if not active yet.
  if (active_layer_ == -1)
    return cfg_updated;

  if (max_qp_ == -1 || number_of_temporal_layers_ <= 1)
    return cfg_updated;

  // If layer is in the quality boost state (following a dropped frame), update
  // the configuration with the adjusted (lower) qp and set the state back to
  // normal.
  unsigned int adjusted_max_qp = max_qp_;  // Set the normal max qp.
  if (layers_[active_layer_].state == TemporalLayer::State::kQualityBoost) {
    if (layers_[active_layer_].enhanced_max_qp != -1) {
      // Bitrate is high enough for quality boost, update max qp.
      adjusted_max_qp = layers_[active_layer_].enhanced_max_qp;
    }
    // Regardless of qp, reset the boost state for the next frame.
    layers_[active_layer_].state = TemporalLayer::State::kNormal;
  }

  if (adjusted_max_qp == cfg->rc_max_quantizer)
    return cfg_updated;

  cfg->rc_max_quantizer = adjusted_max_qp;
  cfg_updated = true;

  return cfg_updated;
}

void ScreenshareLayers::TemporalLayer::UpdateDebt(int64_t delta_ms) {
  uint32_t debt_reduction_bytes = target_rate_kbps_ * delta_ms / 8;
  if (debt_reduction_bytes >= debt_bytes_) {
    debt_bytes_ = 0;
  } else {
    debt_bytes_ -= debt_reduction_bytes;
  }
}

void ScreenshareLayers::UpdateHistograms() {
  if (stats_.first_frame_time_ms_ == -1)
    return;
  int64_t duration_sec =
      (clock_->TimeInMilliseconds() - stats_.first_frame_time_ms_ + 500) / 1000;
  if (duration_sec >= metrics::kMinRunTimeInSeconds) {
    RTC_HISTOGRAM_COUNTS_10000(
        "WebRTC.Video.Screenshare.Layer0.FrameRate",
        (stats_.num_tl0_frames_ + (duration_sec / 2)) / duration_sec);
    RTC_HISTOGRAM_COUNTS_10000(
        "WebRTC.Video.Screenshare.Layer1.FrameRate",
        (stats_.num_tl1_frames_ + (duration_sec / 2)) / duration_sec);
    int total_frames = stats_.num_tl0_frames_ + stats_.num_tl1_frames_;
    RTC_HISTOGRAM_COUNTS_10000(
        "WebRTC.Video.Screenshare.FramesPerDrop",
        (stats_.num_dropped_frames_ == 0
             ? 0
             : total_frames / stats_.num_dropped_frames_));
    RTC_HISTOGRAM_COUNTS_10000(
        "WebRTC.Video.Screenshare.FramesPerOvershoot",
        (stats_.num_overshoots_ == 0 ? 0
                                     : total_frames / stats_.num_overshoots_));
    if (stats_.num_tl0_frames_ > 0) {
      RTC_HISTOGRAM_COUNTS_10000("WebRTC.Video.Screenshare.Layer0.Qp",
                                 stats_.tl0_qp_sum_ / stats_.num_tl0_frames_);
      RTC_HISTOGRAM_COUNTS_10000(
          "WebRTC.Video.Screenshare.Layer0.TargetBitrate",
          stats_.tl0_target_bitrate_sum_ / stats_.num_tl0_frames_);
    }
    if (stats_.num_tl1_frames_ > 0) {
      RTC_HISTOGRAM_COUNTS_10000("WebRTC.Video.Screenshare.Layer1.Qp",
                                 stats_.tl1_qp_sum_ / stats_.num_tl1_frames_);
      RTC_HISTOGRAM_COUNTS_10000(
          "WebRTC.Video.Screenshare.Layer1.TargetBitrate",
          stats_.tl1_target_bitrate_sum_ / stats_.num_tl1_frames_);
    }
  }
}
}  // namespace webrtc
