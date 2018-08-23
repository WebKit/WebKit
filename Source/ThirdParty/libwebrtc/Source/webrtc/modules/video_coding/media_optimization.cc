/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/media_optimization.h"

#include <limits>

#include "modules/video_coding/utility/frame_dropper.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace media_optimization {
namespace {
const int64_t kFrameHistoryWinMs = 2000;
}  // namespace

MediaOptimization::MediaOptimization(Clock* clock)
    : clock_(clock),
      max_bit_rate_(0),
      max_frame_rate_(0),
      frame_dropper_(new FrameDropper),
      incoming_frame_rate_(0) {
  memset(incoming_frame_times_, -1, sizeof(incoming_frame_times_));
}

MediaOptimization::~MediaOptimization(void) {}

void MediaOptimization::Reset() {
  rtc::CritScope lock(&crit_sect_);
  memset(incoming_frame_times_, -1, sizeof(incoming_frame_times_));
  incoming_frame_rate_ = 0.0;
  frame_dropper_->Reset();
  frame_dropper_->SetRates(0, 0);
  max_bit_rate_ = 0;
  max_frame_rate_ = 0;
}

void MediaOptimization::SetEncodingData(int32_t max_bit_rate,
                                        uint32_t target_bitrate,
                                        uint32_t max_frame_rate) {
  rtc::CritScope lock(&crit_sect_);
  // Everything codec specific should be reset here since the codec has changed.
  max_bit_rate_ = max_bit_rate;
  max_frame_rate_ = static_cast<float>(max_frame_rate);
  float target_bitrate_kbps = static_cast<float>(target_bitrate) / 1000.0f;
  frame_dropper_->Reset();
  frame_dropper_->SetRates(target_bitrate_kbps, max_frame_rate_);
}

uint32_t MediaOptimization::SetTargetRates(uint32_t target_bitrate) {
  rtc::CritScope lock(&crit_sect_);

  // Cap target video bitrate to codec maximum.
  int video_target_bitrate = target_bitrate;
  if (max_bit_rate_ > 0 && video_target_bitrate > max_bit_rate_) {
    video_target_bitrate = max_bit_rate_;
  }
  float target_video_bitrate_kbps =
      static_cast<float>(video_target_bitrate) / 1000.0f;

  float framerate = incoming_frame_rate_;
  if (framerate == 0.0) {
    // No framerate estimate available, use configured max framerate instead.
    framerate = max_frame_rate_;
  }

  frame_dropper_->SetRates(target_video_bitrate_kbps, framerate);

  return video_target_bitrate;
}

uint32_t MediaOptimization::InputFrameRate() {
  rtc::CritScope lock(&crit_sect_);
  return InputFrameRateInternal();
}

uint32_t MediaOptimization::InputFrameRateInternal() {
  ProcessIncomingFrameRate(clock_->TimeInMilliseconds());
  uint32_t framerate = static_cast<uint32_t>(std::min<float>(
      std::numeric_limits<uint32_t>::max(), incoming_frame_rate_ + 0.5f));
  return framerate;
}

void MediaOptimization::UpdateWithEncodedData(
    const size_t encoded_image_length,
    const FrameType encoded_image_frametype) {
  size_t encoded_length = encoded_image_length;
  rtc::CritScope lock(&crit_sect_);
  if (encoded_length > 0) {
    const bool delta_frame = encoded_image_frametype != kVideoFrameKey;
    frame_dropper_->Fill(encoded_length, delta_frame);
  }
}

void MediaOptimization::EnableFrameDropper(bool enable) {
  rtc::CritScope lock(&crit_sect_);
  frame_dropper_->Enable(enable);
}

bool MediaOptimization::DropFrame() {
  rtc::CritScope lock(&crit_sect_);
  UpdateIncomingFrameRate();
  // Leak appropriate number of bytes.
  frame_dropper_->Leak((uint32_t)(InputFrameRateInternal() + 0.5f));
  return frame_dropper_->DropFrame();
}

void MediaOptimization::UpdateIncomingFrameRate() {
  int64_t now = clock_->TimeInMilliseconds();
  if (incoming_frame_times_[0] == 0) {
    // No shifting if this is the first time.
  } else {
    // Shift all times one step.
    for (int32_t i = (kFrameCountHistorySize - 2); i >= 0; i--) {
      incoming_frame_times_[i + 1] = incoming_frame_times_[i];
    }
  }
  incoming_frame_times_[0] = now;
  ProcessIncomingFrameRate(now);
}

// Allowing VCM to keep track of incoming frame rate.
void MediaOptimization::ProcessIncomingFrameRate(int64_t now) {
  int32_t num = 0;
  int32_t nr_of_frames = 0;
  for (num = 1; num < (kFrameCountHistorySize - 1); ++num) {
    if (incoming_frame_times_[num] <= 0 ||
        // Don't use data older than 2 s.
        now - incoming_frame_times_[num] > kFrameHistoryWinMs) {
      break;
    } else {
      nr_of_frames++;
    }
  }
  if (num > 1) {
    const int64_t diff =
        incoming_frame_times_[0] - incoming_frame_times_[num - 1];
    incoming_frame_rate_ = 0.0;  // No frame rate estimate available.
    if (diff > 0) {
      incoming_frame_rate_ = nr_of_frames * 1000.0f / static_cast<float>(diff);
    }
  }
}
}  // namespace media_optimization
}  // namespace webrtc
