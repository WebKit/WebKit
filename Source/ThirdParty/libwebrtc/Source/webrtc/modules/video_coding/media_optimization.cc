/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/media_optimization.h"

#include "webrtc/base/logging.h"
#include "webrtc/modules/video_coding/utility/frame_dropper.h"
#include "webrtc/system_wrappers/include/clock.h"

namespace webrtc {
namespace media_optimization {

struct MediaOptimization::EncodedFrameSample {
  EncodedFrameSample(size_t size_bytes,
                     uint32_t timestamp,
                     int64_t time_complete_ms)
      : size_bytes(size_bytes),
        timestamp(timestamp),
        time_complete_ms(time_complete_ms) {}

  size_t size_bytes;
  uint32_t timestamp;
  int64_t time_complete_ms;
};

MediaOptimization::MediaOptimization(Clock* clock)
    : crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
      clock_(clock),
      max_bit_rate_(0),
      codec_width_(0),
      codec_height_(0),
      user_frame_rate_(0),
      frame_dropper_(new FrameDropper),
      send_statistics_zero_encode_(0),
      max_payload_size_(1460),
      video_target_bitrate_(0),
      incoming_frame_rate_(0),
      encoded_frame_samples_(),
      avg_sent_bit_rate_bps_(0),
      avg_sent_framerate_(0),
      num_layers_(0) {
  memset(send_statistics_, 0, sizeof(send_statistics_));
  memset(incoming_frame_times_, -1, sizeof(incoming_frame_times_));
}

MediaOptimization::~MediaOptimization(void) {
}

void MediaOptimization::Reset() {
  CriticalSectionScoped lock(crit_sect_.get());
  SetEncodingDataInternal(0, 0, 0, 0, 0, 0, max_payload_size_);
  memset(incoming_frame_times_, -1, sizeof(incoming_frame_times_));
  incoming_frame_rate_ = 0.0;
  frame_dropper_->Reset();
  frame_dropper_->SetRates(0, 0);
  send_statistics_zero_encode_ = 0;
  video_target_bitrate_ = 0;
  codec_width_ = 0;
  codec_height_ = 0;
  user_frame_rate_ = 0;
  encoded_frame_samples_.clear();
  avg_sent_bit_rate_bps_ = 0;
  num_layers_ = 1;
}

void MediaOptimization::SetEncodingData(int32_t max_bit_rate,
                                        uint32_t target_bitrate,
                                        uint16_t width,
                                        uint16_t height,
                                        uint32_t frame_rate,
                                        int num_layers,
                                        int32_t mtu) {
  CriticalSectionScoped lock(crit_sect_.get());
  SetEncodingDataInternal(max_bit_rate, frame_rate, target_bitrate, width,
                          height, num_layers, mtu);
}

void MediaOptimization::SetEncodingDataInternal(int32_t max_bit_rate,
                                                uint32_t frame_rate,
                                                uint32_t target_bitrate,
                                                uint16_t width,
                                                uint16_t height,
                                                int num_layers,
                                                int32_t mtu) {
  // Everything codec specific should be reset here since this means the codec
  // has changed.

  max_bit_rate_ = max_bit_rate;
  video_target_bitrate_ = target_bitrate;
  float target_bitrate_kbps = static_cast<float>(target_bitrate) / 1000.0f;
  frame_dropper_->Reset();
  frame_dropper_->SetRates(target_bitrate_kbps, static_cast<float>(frame_rate));
  user_frame_rate_ = static_cast<float>(frame_rate);
  codec_width_ = width;
  codec_height_ = height;
  num_layers_ = (num_layers <= 1) ? 1 : num_layers;  // Can also be zero.
  max_payload_size_ = mtu;
}

uint32_t MediaOptimization::SetTargetRates(uint32_t target_bitrate) {
  CriticalSectionScoped lock(crit_sect_.get());

  video_target_bitrate_ = target_bitrate;

  // Cap target video bitrate to codec maximum.
  if (max_bit_rate_ > 0 && video_target_bitrate_ > max_bit_rate_) {
    video_target_bitrate_ = max_bit_rate_;
  }

  // Update encoding rates following protection settings.
  float target_video_bitrate_kbps =
      static_cast<float>(video_target_bitrate_) / 1000.0f;
  frame_dropper_->SetRates(target_video_bitrate_kbps, incoming_frame_rate_);

  return video_target_bitrate_;
}

uint32_t MediaOptimization::InputFrameRate() {
  CriticalSectionScoped lock(crit_sect_.get());
  return InputFrameRateInternal();
}

uint32_t MediaOptimization::InputFrameRateInternal() {
  ProcessIncomingFrameRate(clock_->TimeInMilliseconds());
  return uint32_t(incoming_frame_rate_ + 0.5f);
}

uint32_t MediaOptimization::SentFrameRate() {
  CriticalSectionScoped lock(crit_sect_.get());
  return SentFrameRateInternal();
}

uint32_t MediaOptimization::SentFrameRateInternal() {
  PurgeOldFrameSamples(clock_->TimeInMilliseconds());
  UpdateSentFramerate();
  return avg_sent_framerate_;
}

uint32_t MediaOptimization::SentBitRate() {
  CriticalSectionScoped lock(crit_sect_.get());
  const int64_t now_ms = clock_->TimeInMilliseconds();
  PurgeOldFrameSamples(now_ms);
  UpdateSentBitrate(now_ms);
  return avg_sent_bit_rate_bps_;
}

int32_t MediaOptimization::UpdateWithEncodedData(
    const EncodedImage& encoded_image) {
  size_t encoded_length = encoded_image._length;
  uint32_t timestamp = encoded_image._timeStamp;
  CriticalSectionScoped lock(crit_sect_.get());
  const int64_t now_ms = clock_->TimeInMilliseconds();
  PurgeOldFrameSamples(now_ms);
  if (encoded_frame_samples_.size() > 0 &&
      encoded_frame_samples_.back().timestamp == timestamp) {
    // Frames having the same timestamp are generated from the same input
    // frame. We don't want to double count them, but only increment the
    // size_bytes.
    encoded_frame_samples_.back().size_bytes += encoded_length;
    encoded_frame_samples_.back().time_complete_ms = now_ms;
  } else {
    encoded_frame_samples_.push_back(
        EncodedFrameSample(encoded_length, timestamp, now_ms));
  }
  UpdateSentBitrate(now_ms);
  UpdateSentFramerate();
  if (encoded_length > 0) {
    const bool delta_frame = encoded_image._frameType != kVideoFrameKey;
    frame_dropper_->Fill(encoded_length, delta_frame);
  }

  return VCM_OK;
}

void MediaOptimization::EnableFrameDropper(bool enable) {
  CriticalSectionScoped lock(crit_sect_.get());
  frame_dropper_->Enable(enable);
}

bool MediaOptimization::DropFrame() {
  CriticalSectionScoped lock(crit_sect_.get());
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

void MediaOptimization::PurgeOldFrameSamples(int64_t now_ms) {
  while (!encoded_frame_samples_.empty()) {
    if (now_ms - encoded_frame_samples_.front().time_complete_ms >
        kBitrateAverageWinMs) {
      encoded_frame_samples_.pop_front();
    } else {
      break;
    }
  }
}

void MediaOptimization::UpdateSentBitrate(int64_t now_ms) {
  if (encoded_frame_samples_.empty()) {
    avg_sent_bit_rate_bps_ = 0;
    return;
  }
  size_t framesize_sum = 0;
  for (FrameSampleList::iterator it = encoded_frame_samples_.begin();
       it != encoded_frame_samples_.end(); ++it) {
    framesize_sum += it->size_bytes;
  }
  float denom = static_cast<float>(
      now_ms - encoded_frame_samples_.front().time_complete_ms);
  if (denom >= 1.0f) {
    avg_sent_bit_rate_bps_ =
        static_cast<uint32_t>(framesize_sum * 8.0f * 1000.0f / denom + 0.5f);
  } else {
    avg_sent_bit_rate_bps_ = framesize_sum * 8;
  }
}

void MediaOptimization::UpdateSentFramerate() {
  if (encoded_frame_samples_.size() <= 1) {
    avg_sent_framerate_ = encoded_frame_samples_.size();
    return;
  }
  int denom = encoded_frame_samples_.back().timestamp -
              encoded_frame_samples_.front().timestamp;
  if (denom > 0) {
    avg_sent_framerate_ =
        (90000 * (encoded_frame_samples_.size() - 1) + denom / 2) / denom;
  } else {
    avg_sent_framerate_ = encoded_frame_samples_.size();
  }
}

// Allowing VCM to keep track of incoming frame rate.
void MediaOptimization::ProcessIncomingFrameRate(int64_t now) {
  int32_t num = 0;
  int32_t nr_of_frames = 0;
  for (num = 1; num < (kFrameCountHistorySize - 1); ++num) {
    if (incoming_frame_times_[num] <= 0 ||
        // don't use data older than 2 s
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
