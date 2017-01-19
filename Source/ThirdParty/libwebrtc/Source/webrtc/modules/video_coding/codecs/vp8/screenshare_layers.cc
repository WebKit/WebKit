/* Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/

#include "webrtc/modules/video_coding/codecs/vp8/screenshare_layers.h"

#include <stdlib.h>

#include <algorithm>

#include "webrtc/base/checks.h"
#include "vpx/vpx_encoder.h"
#include "vpx/vp8cx.h"
#include "webrtc/modules/video_coding/include/video_codec_interface.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/system_wrappers/include/metrics.h"

namespace webrtc {

static const int kOneSecond90Khz = 90000;
static const int kMinTimeBetweenSyncs = kOneSecond90Khz * 5;
static const int kMaxTimeBetweenSyncs = kOneSecond90Khz * 10;
static const int kQpDeltaThresholdForSync = 8;

const double ScreenshareLayers::kMaxTL0FpsReduction = 2.5;
const double ScreenshareLayers::kAcceptableTargetOvershoot = 2.0;

// Since this is TL0 we only allow updating and predicting from the LAST
// reference frame.
const int ScreenshareLayers::kTl0Flags =
    VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_REF_GF |
    VP8_EFLAG_NO_REF_ARF;

// Allow predicting from both TL0 and TL1.
const int ScreenshareLayers::kTl1Flags =
    VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST;

// Allow predicting from only TL0 to allow participants to switch to the high
// bitrate stream. This means predicting only from the LAST reference frame, but
// only updating GF to not corrupt TL0.
const int ScreenshareLayers::kTl1SyncFlags =
    VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_UPD_ARF |
    VP8_EFLAG_NO_UPD_LAST;

// Always emit a frame with certain interval, even if bitrate targets have
// been exceeded.
const int ScreenshareLayers::kMaxFrameIntervalMs = 2000;

ScreenshareLayers::ScreenshareLayers(int num_temporal_layers,
                                     uint8_t initial_tl0_pic_idx,
                                     Clock* clock)
    : clock_(clock),
      number_of_temporal_layers_(num_temporal_layers),
      last_base_layer_sync_(false),
      tl0_pic_idx_(initial_tl0_pic_idx),
      active_layer_(-1),
      last_timestamp_(-1),
      last_sync_timestamp_(-1),
      last_emitted_tl0_timestamp_(-1),
      min_qp_(-1),
      max_qp_(-1),
      max_debt_bytes_(0),
      frame_rate_(-1) {
  RTC_CHECK_GT(num_temporal_layers, 0);
  RTC_CHECK_LE(num_temporal_layers, 2);
}

ScreenshareLayers::~ScreenshareLayers() {
  UpdateHistograms();
}

int ScreenshareLayers::CurrentLayerId() const {
  // Codec does not use temporal layers for screenshare.
  return 0;
}

int ScreenshareLayers::EncodeFlags(uint32_t timestamp) {
  if (number_of_temporal_layers_ <= 1) {
    // No flags needed for 1 layer screenshare.
    return 0;
  }

  if (stats_.first_frame_time_ms_ == -1)
    stats_.first_frame_time_ms_ = clock_->TimeInMilliseconds();

  int64_t unwrapped_timestamp = time_wrap_handler_.Unwrap(timestamp);
  int flags = 0;
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
      flags = kTl0Flags;
      last_emitted_tl0_timestamp_ = unwrapped_timestamp;
      break;
    case 1:
      if (TimeToSync(unwrapped_timestamp)) {
        last_sync_timestamp_ = unwrapped_timestamp;
        flags = kTl1SyncFlags;
      } else {
        flags = kTl1Flags;
      }
      break;
    case -1:
      flags = -1;
      ++stats_.num_dropped_frames_;
      break;
    default:
      flags = -1;
      RTC_NOTREACHED();
  }

  int64_t ts_diff;
  if (last_timestamp_ == -1) {
    ts_diff = kOneSecond90Khz / (frame_rate_ <= 0 ? 5 : frame_rate_);
  } else {
    ts_diff = unwrapped_timestamp - last_timestamp_;
  }
  // Make sure both frame droppers leak out bits.
  layers_[0].UpdateDebt(ts_diff / 90);
  layers_[1].UpdateDebt(ts_diff / 90);
  last_timestamp_ = timestamp;
  return flags;
}

bool ScreenshareLayers::ConfigureBitrates(int bitrate_kbps,
                                          int max_bitrate_kbps,
                                          int framerate,
                                          vpx_codec_enc_cfg_t* cfg) {
  layers_[0].target_rate_kbps_ = bitrate_kbps;
  layers_[1].target_rate_kbps_ = max_bitrate_kbps;

  int target_bitrate_kbps = bitrate_kbps;

  if (cfg != nullptr) {
    if (number_of_temporal_layers_ > 1) {
      // Calculate a codec target bitrate. This may be higher than TL0, gaining
      // quality at the expense of frame rate at TL0. Constraints:
      // - TL0 frame rate no less than framerate / kMaxTL0FpsReduction.
      // - Target rate * kAcceptableTargetOvershoot should not exceed TL1 rate.
      target_bitrate_kbps =
          std::min(bitrate_kbps * kMaxTL0FpsReduction,
                   max_bitrate_kbps / kAcceptableTargetOvershoot);

      cfg->rc_target_bitrate = std::max(bitrate_kbps, target_bitrate_kbps);
    }

    // Don't reconfigure qp limits during quality boost frames.
    if (active_layer_ == -1 ||
        layers_[active_layer_].state != TemporalLayer::State::kQualityBoost) {
      min_qp_ = cfg->rc_min_quantizer;
      max_qp_ = cfg->rc_max_quantizer;
      // After a dropped frame, a frame with max qp will be encoded and the
      // quality will then ramp up from there. To boost the speed of recovery,
      // encode the next frame with lower max qp. TL0 is the most important to
      // improve since the errors in this layer will propagate to TL1.
      // Currently, reduce max qp by 20% for TL0 and 15% for TL1.
      layers_[0].enhanced_max_qp = min_qp_ + (((max_qp_ - min_qp_) * 80) / 100);
      layers_[1].enhanced_max_qp = min_qp_ + (((max_qp_ - min_qp_) * 85) / 100);
    }
  }

  int avg_frame_size = (target_bitrate_kbps * 1000) / (8 * framerate);
  max_debt_bytes_ = 4 * avg_frame_size;

  return true;
}

void ScreenshareLayers::FrameEncoded(unsigned int size,
                                     uint32_t timestamp,
                                     int qp) {
  if (number_of_temporal_layers_ == 1)
    return;

  RTC_DCHECK_NE(-1, active_layer_);
  if (size == 0) {
    layers_[active_layer_].state = TemporalLayer::State::kDropped;
    ++stats_.num_overshoots_;
    return;
  }

  if (layers_[active_layer_].state == TemporalLayer::State::kDropped) {
    layers_[active_layer_].state = TemporalLayer::State::kQualityBoost;
  }

  if (qp != -1)
    layers_[active_layer_].last_qp = qp;

  if (active_layer_ == 0) {
    layers_[0].debt_bytes_ += size;
    layers_[1].debt_bytes_ += size;
    ++stats_.num_tl0_frames_;
    stats_.tl0_target_bitrate_sum_ += layers_[0].target_rate_kbps_;
    stats_.tl0_qp_sum_ += qp;
  } else if (active_layer_ == 1) {
    layers_[1].debt_bytes_ += size;
    ++stats_.num_tl1_frames_;
    stats_.tl1_target_bitrate_sum_ += layers_[1].target_rate_kbps_;
    stats_.tl1_qp_sum_ += qp;
  }
}

void ScreenshareLayers::PopulateCodecSpecific(bool base_layer_sync,
                                              CodecSpecificInfoVP8* vp8_info,
                                              uint32_t timestamp) {
  int64_t unwrapped_timestamp = time_wrap_handler_.Unwrap(timestamp);
  if (number_of_temporal_layers_ == 1) {
    vp8_info->temporalIdx = kNoTemporalIdx;
    vp8_info->layerSync = false;
    vp8_info->tl0PicIdx = kNoTl0PicIdx;
  } else {
    RTC_DCHECK_NE(-1, active_layer_);
    vp8_info->temporalIdx = active_layer_;
    if (base_layer_sync) {
      vp8_info->temporalIdx = 0;
      last_sync_timestamp_ = unwrapped_timestamp;
    } else if (last_base_layer_sync_ && vp8_info->temporalIdx != 0) {
      // Regardless of pattern the frame after a base layer sync will always
      // be a layer sync.
      last_sync_timestamp_ = unwrapped_timestamp;
    }
    vp8_info->layerSync = last_sync_timestamp_ != -1 &&
                          last_sync_timestamp_ == unwrapped_timestamp;
    if (vp8_info->temporalIdx == 0) {
      tl0_pic_idx_++;
    }
    last_base_layer_sync_ = base_layer_sync;
    vp8_info->tl0PicIdx = tl0_pic_idx_;
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

bool ScreenshareLayers::UpdateConfiguration(vpx_codec_enc_cfg_t* cfg) {
  if (max_qp_ == -1 || number_of_temporal_layers_ <= 1)
    return false;
  RTC_DCHECK_NE(-1, active_layer_);

  // If layer is in the quality boost state (following a dropped frame), update
  // the configuration with the adjusted (lower) qp and set the state back to
  // normal.
  unsigned int adjusted_max_qp;
  if (layers_[active_layer_].state == TemporalLayer::State::kQualityBoost &&
      layers_[active_layer_].enhanced_max_qp != -1) {
    adjusted_max_qp = layers_[active_layer_].enhanced_max_qp;
    layers_[active_layer_].state = TemporalLayer::State::kNormal;
  } else {
    if (max_qp_ == -1)
      return false;
    adjusted_max_qp = max_qp_;  // Set the normal max qp.
  }

  if (adjusted_max_qp == cfg->rc_max_quantizer)
    return false;

  cfg->rc_max_quantizer = adjusted_max_qp;
  return true;
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
        (stats_.num_dropped_frames_ == 0 ? 0 : total_frames /
                                                   stats_.num_dropped_frames_));
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
