/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/video_quality_observer.h"

#include <algorithm>
#include <string>

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/string_builder.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {

namespace {
const int kMinFrameSamplesToDetectFreeze = 5;
const int kMinCallDurationMs = 3000;
const int kMinRequiredSamples = 1;
const int kMinIncreaseForFreezeMs = 150;
const int kPixelsInHighResolution = 960 * 540;  // CPU-adapted HD still counts.
const int kPixelsInMediumResolution = 640 * 360;
const int kBlockyQpThresholdVp8 = 70;
const int kBlockyQpThresholdVp9 = 60;  // TODO(ilnik): tune this value.
// TODO(ilnik): Add H264/HEVC thresholds.
}  // namespace

VideoQualityObserver::VideoQualityObserver(VideoContentType content_type)
    : last_frame_decoded_ms_(-1),
      num_frames_decoded_(0),
      first_frame_decoded_ms_(-1),
      last_frame_pixels_(0),
      last_frame_qp_(0),
      last_unfreeze_time_(0),
      time_in_resolution_ms_(3, 0),
      current_resolution_(Resolution::Low),
      num_resolution_downgrades_(0),
      time_in_blocky_video_ms_(0),
      content_type_(content_type),
      is_paused_(false) {}

VideoQualityObserver::~VideoQualityObserver() {
  UpdateHistograms();
}

void VideoQualityObserver::UpdateHistograms() {
  // Don't report anything on an empty video stream.
  if (num_frames_decoded_ == 0) {
    return;
  }

  char log_stream_buf[2 * 1024];
  rtc::SimpleStringBuilder log_stream(log_stream_buf);

  if (last_frame_decoded_ms_ > last_unfreeze_time_) {
    smooth_playback_durations_.Add(last_frame_decoded_ms_ -
                                   last_unfreeze_time_);
  }

  std::string uma_prefix = videocontenttypehelpers::IsScreenshare(content_type_)
                               ? "WebRTC.Video.Screenshare"
                               : "WebRTC.Video";

  auto mean_time_between_freezes =
      smooth_playback_durations_.Avg(kMinRequiredSamples);
  if (mean_time_between_freezes) {
    RTC_HISTOGRAM_COUNTS_SPARSE_100000(uma_prefix + ".MeanTimeBetweenFreezesMs",
                                       *mean_time_between_freezes);
    log_stream << uma_prefix << ".MeanTimeBetweenFreezesMs "
               << *mean_time_between_freezes << "\n";
  }
  auto avg_freeze_length = freezes_durations_.Avg(kMinRequiredSamples);
  if (avg_freeze_length) {
    RTC_HISTOGRAM_COUNTS_SPARSE_100000(uma_prefix + ".MeanFreezeDurationMs",
                                       *avg_freeze_length);
    log_stream << uma_prefix << ".MeanFreezeDurationMs " << *avg_freeze_length
               << "\n";
  }

  int64_t call_duration_ms = last_frame_decoded_ms_ - first_frame_decoded_ms_;

  if (call_duration_ms >= kMinCallDurationMs) {
    int time_spent_in_hd_percentage = static_cast<int>(
        time_in_resolution_ms_[Resolution::High] * 100 / call_duration_ms);
    RTC_HISTOGRAM_COUNTS_SPARSE_100(uma_prefix + ".TimeInHdPercentage",
                                    time_spent_in_hd_percentage);
    log_stream << uma_prefix << ".TimeInHdPercentage "
               << time_spent_in_hd_percentage << "\n";

    int time_with_blocky_video_percentage =
        static_cast<int>(time_in_blocky_video_ms_ * 100 / call_duration_ms);
    RTC_HISTOGRAM_COUNTS_SPARSE_100(uma_prefix + ".TimeInBlockyVideoPercentage",
                                    time_with_blocky_video_percentage);
    log_stream << uma_prefix << ".TimeInBlockyVideoPercentage "
               << time_with_blocky_video_percentage << "\n";

    int num_resolution_downgrades_per_minute =
        num_resolution_downgrades_ * 60000 / call_duration_ms;
    RTC_HISTOGRAM_COUNTS_SPARSE_100(
        uma_prefix + ".NumberResolutionDownswitchesPerMinute",
        num_resolution_downgrades_per_minute);
    log_stream << uma_prefix << ".NumberResolutionDownswitchesPerMinute "
               << num_resolution_downgrades_per_minute << "\n";

    int num_freezes_per_minute =
        freezes_durations_.NumSamples() * 60000 / call_duration_ms;
    RTC_HISTOGRAM_COUNTS_SPARSE_100(uma_prefix + ".NumberFreezesPerMinute",
                                    num_freezes_per_minute);
    log_stream << uma_prefix << ".NumberFreezesPerMinute "
               << num_freezes_per_minute << "\n";
  }
  RTC_LOG(LS_INFO) << log_stream.str();
}

void VideoQualityObserver::OnDecodedFrame(absl::optional<uint8_t> qp,
                                          int width,
                                          int height,
                                          int64_t now_ms,
                                          VideoCodecType codec) {
  if (num_frames_decoded_ == 0) {
    first_frame_decoded_ms_ = now_ms;
    last_unfreeze_time_ = now_ms;
  }

  ++num_frames_decoded_;

  if (!is_paused_ && num_frames_decoded_ > 1) {
    // Process inter-frame delay.
    int64_t interframe_delay_ms = now_ms - last_frame_decoded_ms_;
    interframe_delays_.Add(interframe_delay_ms);
    absl::optional<int> avg_interframe_delay =
        interframe_delays_.Avg(kMinFrameSamplesToDetectFreeze);
    // Check if it was a freeze.
    if (avg_interframe_delay &&
        interframe_delay_ms >=
            std::max(3 * *avg_interframe_delay,
                     *avg_interframe_delay + kMinIncreaseForFreezeMs)) {
      freezes_durations_.Add(interframe_delay_ms);
      smooth_playback_durations_.Add(last_frame_decoded_ms_ -
                                     last_unfreeze_time_);
      last_unfreeze_time_ = now_ms;
    } else {
      // Only count inter-frame delay as playback time if there
      // was no freeze.
      time_in_resolution_ms_[current_resolution_] += interframe_delay_ms;
      absl::optional<int> qp_blocky_threshold;
      // TODO(ilnik): add other codec types when we have QP for them.
      switch (codec) {
        case kVideoCodecVP8:
          qp_blocky_threshold = kBlockyQpThresholdVp8;
          break;
        case kVideoCodecVP9:
          qp_blocky_threshold = kBlockyQpThresholdVp9;
          break;
        default:
          qp_blocky_threshold = absl::nullopt;
      }
      if (qp_blocky_threshold && qp.value_or(0) > *qp_blocky_threshold) {
        time_in_blocky_video_ms_ += interframe_delay_ms;
      }
    }
  }

  if (is_paused_) {
    // If the stream was paused since the previous frame, do not count the
    // pause toward smooth playback. Explicitly count the part before it and
    // start the new smooth playback interval from this frame.
    is_paused_ = false;
    if (last_frame_decoded_ms_ > last_unfreeze_time_) {
      smooth_playback_durations_.Add(last_frame_decoded_ms_ -
                                     last_unfreeze_time_);
    }
    last_unfreeze_time_ = now_ms;
  }

  int64_t pixels = width * height;
  if (pixels >= kPixelsInHighResolution) {
    current_resolution_ = Resolution::High;
  } else if (pixels >= kPixelsInMediumResolution) {
    current_resolution_ = Resolution::Medium;
  } else {
    current_resolution_ = Resolution::Low;
  }

  if (pixels < last_frame_pixels_) {
    ++num_resolution_downgrades_;
  }

  last_frame_decoded_ms_ = now_ms;
  last_frame_qp_ = qp.value_or(0);
  last_frame_pixels_ = pixels;
}

void VideoQualityObserver::OnStreamInactive() {
  is_paused_ = true;
}
}  // namespace webrtc
