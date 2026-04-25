/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/scenario/stats_collection.h"

#include <cmath>
#include <cstddef>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "api/rtc_event_log_output.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "call/audio_receive_stream.h"
#include "call/call.h"
#include "call/video_receive_stream.h"
#include "call/video_send_stream.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "rtc_base/checks.h"
#include "rtc_base/memory_usage.h"
#include "rtc_base/thread.h"
#include "system_wrappers/include/clock.h"
#include "test/logging/log_writer.h"
#include "test/scenario/performance_stats.h"

namespace webrtc {
namespace test {

VideoQualityAnalyzer::VideoQualityAnalyzer(
    VideoQualityAnalyzerConfig config,
    std::unique_ptr<RtcEventLogOutput> writer)
    : config_(config), writer_(std::move(writer)) {
  if (writer_) {
    PrintHeaders();
  }
}

VideoQualityAnalyzer::~VideoQualityAnalyzer() = default;

void VideoQualityAnalyzer::PrintHeaders() {
  writer_->Write(
      "capture_time render_time capture_width capture_height render_width "
      "render_height psnr\n");
}

std::function<void(const VideoFramePair&)> VideoQualityAnalyzer::Handler(
    Clock* clock) {
  return [this, clock](VideoFramePair pair) {
    HandleFramePair(pair, clock->CurrentTime());
  };
}

void VideoQualityAnalyzer::HandleFramePair(VideoFramePair sample,
                                           double psnr,
                                           Timestamp at_time) {
  layer_analyzers_[sample.layer_id].HandleFramePair(sample, psnr, writer_.get(),
                                                    at_time);
  cached_.reset();
}

void VideoQualityAnalyzer::HandleFramePair(VideoFramePair sample,
                                           Timestamp at_time) {
  double psnr = NAN;
  if (sample.decoded)
    psnr = I420PSNR(*sample.captured->ToI420(), *sample.decoded->ToI420());

  if (config_.thread) {
    config_.thread->PostTask([this, sample, psnr, at_time] {
      HandleFramePair(std::move(sample), psnr, at_time);
    });
  } else {
    HandleFramePair(std::move(sample), psnr, at_time);
  }
}

std::vector<VideoQualityStats> VideoQualityAnalyzer::layer_stats() const {
  std::vector<VideoQualityStats> res;
  for (auto& layer : layer_analyzers_)
    res.push_back(layer.second.stats_);
  return res;
}

VideoQualityStats& VideoQualityAnalyzer::stats() {
  if (!cached_) {
    cached_ = VideoQualityStats();
    for (auto& layer : layer_analyzers_)
      cached_->AddStats(layer.second.stats_);
  }
  return *cached_;
}

void VideoLayerAnalyzer::HandleFramePair(VideoFramePair sample,
                                         double psnr,
                                         RtcEventLogOutput* writer,
                                         Timestamp at_time) {
  RTC_CHECK(sample.captured);
  HandleCapturedFrame(sample);
  if (!sample.decoded) {
    // Can only happen in the beginning of a call or if the resolution is
    // reduced. Otherwise we will detect a freeze.
    ++stats_.lost_count;
    ++skip_count_;
  } else {
    stats_.psnr_with_freeze.AddSample({.value = psnr, .time = at_time});
    if (sample.repeated) {
      ++stats_.freeze_count;
      ++skip_count_;
    } else {
      stats_.psnr.AddSample({.value = psnr, .time = at_time});
      HandleRenderedFrame(sample);
    }
  }
  if (writer) {
    LogWriteFormat(writer, "%.3f %.3f %.3f %i %i %i %i %.3f\n",
                   sample.capture_time.seconds<double>(),
                   sample.render_time.seconds<double>(),
                   sample.captured->width(), sample.captured->height(),
                   sample.decoded ? sample.decoded->width() : 0,
                   sample.decoded ? sample.decoded->height() : 0, psnr);
  }
}

void VideoLayerAnalyzer::HandleCapturedFrame(const VideoFramePair& sample) {
  stats_.capture.AddFrameInfo(*sample.captured, sample.capture_time);
  if (last_freeze_time_.IsInfinite())
    last_freeze_time_ = sample.capture_time;
}

void VideoLayerAnalyzer::HandleRenderedFrame(const VideoFramePair& sample) {
  stats_.capture_to_decoded_delay.AddSample(
      sample.decoded_time - sample.capture_time, sample.capture_time);
  stats_.end_to_end_delay.AddSample(sample.render_time - sample.capture_time,
                                    sample.capture_time);
  stats_.render.AddFrameInfo(*sample.decoded, sample.render_time);
  stats_.skipped_between_rendered.AddSample(
      {.value = static_cast<double>(skip_count_), .time = sample.render_time});
  skip_count_ = 0;

  if (last_render_time_.IsFinite()) {
    RTC_DCHECK(sample.render_time.IsFinite());
    TimeDelta render_interval = sample.render_time - last_render_time_;
    TimeDelta mean_interval = stats_.render.frames.interval().Mean();
    if (render_interval > TimeDelta::Millis(150) + mean_interval ||
        render_interval > 3 * mean_interval) {
      stats_.freeze_duration.AddSample(render_interval, sample.capture_time);
      stats_.time_between_freezes.AddSample(
          last_render_time_ - last_freeze_time_, sample.capture_time);
      last_freeze_time_ = sample.render_time;
    }
  }
  last_render_time_ = sample.render_time;
}

void CallStatsCollector::AddStats(Call::Stats sample, Timestamp at_time) {
  if (sample.send_bandwidth_bps > 0)
    stats_.target_rate.AddSample(
        DataRate::BitsPerSec(sample.send_bandwidth_bps), at_time);
  if (sample.pacer_delay_ms > 0)
    stats_.pacer_delay.AddSample(TimeDelta::Millis(sample.pacer_delay_ms),
                                 at_time);
  if (sample.rtt_ms > 0)
    stats_.round_trip_time.AddSample(TimeDelta::Millis(sample.rtt_ms), at_time);
  stats_.memory_usage.AddSample(
      {.value = static_cast<double>(GetProcessResidentSizeBytes()),
       .time = at_time});
}

void AudioReceiveStatsCollector::AddStats(
    AudioReceiveStreamInterface::Stats sample,
    Timestamp at_time) {
  stats_.expand_rate.AddSample({.value = sample.expand_rate, .time = at_time});
  stats_.accelerate_rate.AddSample(
      {.value = sample.accelerate_rate, .time = at_time});
  stats_.jitter_buffer.AddSample(TimeDelta::Millis(sample.jitter_buffer_ms),
                                 at_time);
}

void VideoSendStatsCollector::AddStats(VideoSendStream::Stats sample,
                                       Timestamp at_time) {
  // It's not certain that we yet have estimates for any of these stats.
  // Check that they are positive before mixing them in.
  if (sample.encode_frame_rate <= 0)
    return;

  stats_.encode_frame_rate.AddSample(
      {.value = static_cast<double>(sample.encode_frame_rate),
       .time = at_time});
  stats_.encode_time.AddSample(TimeDelta::Millis(sample.avg_encode_time_ms),
                               at_time);
  stats_.encode_usage.AddSample(
      {.value = sample.encode_usage_percent / 100.0, .time = at_time});
  stats_.media_bitrate.AddSample(DataRate::BitsPerSec(sample.media_bitrate_bps),
                                 at_time);

  size_t fec_bytes = 0;
  for (const auto& kv : sample.substreams) {
    fec_bytes += kv.second.rtp_stats.fec.payload_bytes +
                 kv.second.rtp_stats.fec.padding_bytes;
  }
  if (last_update_.IsFinite()) {
    auto fec_delta = DataSize::Bytes(fec_bytes - last_fec_bytes_);
    auto time_delta = at_time - last_update_;
    stats_.fec_bitrate.AddSample(fec_delta / time_delta, at_time);
  }
  last_fec_bytes_ = fec_bytes;
  last_update_ = at_time;
}

void VideoReceiveStatsCollector::AddStats(
    VideoReceiveStreamInterface::Stats sample,
    Timestamp at_time) {
  if (sample.decode_ms > 0)
    stats_.decode_time.AddSample(TimeDelta::Millis(sample.decode_ms), at_time);
  if (sample.max_decode_ms > 0)
    stats_.decode_time_max.AddSample(TimeDelta::Millis(sample.max_decode_ms),
                                     at_time);
  if (sample.width > 0 && sample.height > 0) {
    stats_.decode_pixels.AddSample(
        {.value = static_cast<double>(sample.width * sample.height),
         .time = at_time});
    stats_.resolution.AddSample(
        {.value = static_cast<double>(sample.height), .time = at_time});
  }
}
}  // namespace test
}  // namespace webrtc
