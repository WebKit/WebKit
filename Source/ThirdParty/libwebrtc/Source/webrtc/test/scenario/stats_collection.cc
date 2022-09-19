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

#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "rtc_base/memory_usage.h"
#include "rtc_base/thread.h"

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

std::function<void(const VideoFramePair&)> VideoQualityAnalyzer::Handler() {
  return [this](VideoFramePair pair) { HandleFramePair(pair); };
}

void VideoQualityAnalyzer::HandleFramePair(VideoFramePair sample, double psnr) {
  layer_analyzers_[sample.layer_id].HandleFramePair(sample, psnr,
                                                    writer_.get());
  cached_.reset();
}

void VideoQualityAnalyzer::HandleFramePair(VideoFramePair sample) {
  double psnr = NAN;
  if (sample.decoded)
    psnr = I420PSNR(*sample.captured->ToI420(), *sample.decoded->ToI420());

  if (config_.thread) {
    config_.thread->PostTask(
        [this, sample, psnr] { HandleFramePair(std::move(sample), psnr); });
  } else {
    HandleFramePair(std::move(sample), psnr);
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
                                         RtcEventLogOutput* writer) {
  RTC_CHECK(sample.captured);
  HandleCapturedFrame(sample);
  if (!sample.decoded) {
    // Can only happen in the beginning of a call or if the resolution is
    // reduced. Otherwise we will detect a freeze.
    ++stats_.lost_count;
    ++skip_count_;
  } else {
    stats_.psnr_with_freeze.AddSample(psnr);
    if (sample.repeated) {
      ++stats_.freeze_count;
      ++skip_count_;
    } else {
      stats_.psnr.AddSample(psnr);
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
  stats_.capture_to_decoded_delay.AddSample(sample.decoded_time -
                                            sample.capture_time);
  stats_.end_to_end_delay.AddSample(sample.render_time - sample.capture_time);
  stats_.render.AddFrameInfo(*sample.decoded, sample.render_time);
  stats_.skipped_between_rendered.AddSample(skip_count_);
  skip_count_ = 0;

  if (last_render_time_.IsFinite()) {
    RTC_DCHECK(sample.render_time.IsFinite());
    TimeDelta render_interval = sample.render_time - last_render_time_;
    TimeDelta mean_interval = stats_.render.frames.interval().Mean();
    if (render_interval > TimeDelta::Millis(150) + mean_interval ||
        render_interval > 3 * mean_interval) {
      stats_.freeze_duration.AddSample(render_interval);
      stats_.time_between_freezes.AddSample(last_render_time_ -
                                            last_freeze_time_);
      last_freeze_time_ = sample.render_time;
    }
  }
  last_render_time_ = sample.render_time;
}

void CallStatsCollector::AddStats(Call::Stats sample) {
  if (sample.send_bandwidth_bps > 0)
    stats_.target_rate.AddSampleBps(sample.send_bandwidth_bps);
  if (sample.pacer_delay_ms > 0)
    stats_.pacer_delay.AddSample(TimeDelta::Millis(sample.pacer_delay_ms));
  if (sample.rtt_ms > 0)
    stats_.round_trip_time.AddSample(TimeDelta::Millis(sample.rtt_ms));
  stats_.memory_usage.AddSample(rtc::GetProcessResidentSizeBytes());
}

void AudioReceiveStatsCollector::AddStats(
    AudioReceiveStreamInterface::Stats sample) {
  stats_.expand_rate.AddSample(sample.expand_rate);
  stats_.accelerate_rate.AddSample(sample.accelerate_rate);
  stats_.jitter_buffer.AddSampleMs(sample.jitter_buffer_ms);
}

void VideoSendStatsCollector::AddStats(VideoSendStream::Stats sample,
                                       Timestamp at_time) {
  // It's not certain that we yet have estimates for any of these stats.
  // Check that they are positive before mixing them in.
  if (sample.encode_frame_rate <= 0)
    return;

  stats_.encode_frame_rate.AddSample(sample.encode_frame_rate);
  stats_.encode_time.AddSampleMs(sample.avg_encode_time_ms);
  stats_.encode_usage.AddSample(sample.encode_usage_percent / 100.0);
  stats_.media_bitrate.AddSampleBps(sample.media_bitrate_bps);

  size_t fec_bytes = 0;
  for (const auto& kv : sample.substreams) {
    fec_bytes += kv.second.rtp_stats.fec.payload_bytes +
                 kv.second.rtp_stats.fec.padding_bytes;
  }
  if (last_update_.IsFinite()) {
    auto fec_delta = DataSize::Bytes(fec_bytes - last_fec_bytes_);
    auto time_delta = at_time - last_update_;
    stats_.fec_bitrate.AddSample(fec_delta / time_delta);
  }
  last_fec_bytes_ = fec_bytes;
  last_update_ = at_time;
}

void VideoReceiveStatsCollector::AddStats(
    VideoReceiveStreamInterface::Stats sample) {
  if (sample.decode_ms > 0)
    stats_.decode_time.AddSampleMs(sample.decode_ms);
  if (sample.max_decode_ms > 0)
    stats_.decode_time_max.AddSampleMs(sample.max_decode_ms);
  if (sample.width > 0 && sample.height > 0) {
    stats_.decode_pixels.AddSample(sample.width * sample.height);
    stats_.resolution.AddSample(sample.height);
  }
}
}  // namespace test
}  // namespace webrtc
