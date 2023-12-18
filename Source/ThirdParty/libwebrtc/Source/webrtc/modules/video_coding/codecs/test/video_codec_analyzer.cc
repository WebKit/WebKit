/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/video_codec_analyzer.h"

#include <memory>

#include "api/task_queue/default_task_queue_factory.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_codec_constants.h"
#include "api/video/video_frame.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/time_utils.h"
#include "third_party/libyuv/include/libyuv/compare.h"

namespace webrtc {
namespace test {

namespace {
using Psnr = VideoCodecStats::Frame::Psnr;

Psnr CalcPsnr(const I420BufferInterface& ref_buffer,
              const I420BufferInterface& dec_buffer) {
  RTC_CHECK_EQ(ref_buffer.width(), dec_buffer.width());
  RTC_CHECK_EQ(ref_buffer.height(), dec_buffer.height());

  uint64_t sse_y = libyuv::ComputeSumSquareErrorPlane(
      dec_buffer.DataY(), dec_buffer.StrideY(), ref_buffer.DataY(),
      ref_buffer.StrideY(), dec_buffer.width(), dec_buffer.height());

  uint64_t sse_u = libyuv::ComputeSumSquareErrorPlane(
      dec_buffer.DataU(), dec_buffer.StrideU(), ref_buffer.DataU(),
      ref_buffer.StrideU(), dec_buffer.width() / 2, dec_buffer.height() / 2);

  uint64_t sse_v = libyuv::ComputeSumSquareErrorPlane(
      dec_buffer.DataV(), dec_buffer.StrideV(), ref_buffer.DataV(),
      ref_buffer.StrideV(), dec_buffer.width() / 2, dec_buffer.height() / 2);

  int num_y_samples = dec_buffer.width() * dec_buffer.height();
  Psnr psnr;
  psnr.y = libyuv::SumSquareErrorToPsnr(sse_y, num_y_samples);
  psnr.u = libyuv::SumSquareErrorToPsnr(sse_u, num_y_samples / 4);
  psnr.v = libyuv::SumSquareErrorToPsnr(sse_v, num_y_samples / 4);

  return psnr;
}

}  // namespace

VideoCodecAnalyzer::VideoCodecAnalyzer(
    ReferenceVideoSource* reference_video_source)
    : reference_video_source_(reference_video_source), num_frames_(0) {
  sequence_checker_.Detach();
}

void VideoCodecAnalyzer::StartEncode(const VideoFrame& input_frame) {
  int64_t encode_start_us = rtc::TimeMicros();
  task_queue_.PostTask(
      [this, timestamp_rtp = input_frame.timestamp(), encode_start_us]() {
        RTC_DCHECK_RUN_ON(&sequence_checker_);

        RTC_CHECK(frame_num_.find(timestamp_rtp) == frame_num_.end());
        frame_num_[timestamp_rtp] = num_frames_++;

        stats_.AddFrame({.frame_num = frame_num_[timestamp_rtp],
                         .timestamp_rtp = timestamp_rtp,
                         .encode_start = Timestamp::Micros(encode_start_us)});
      });
}

void VideoCodecAnalyzer::FinishEncode(const EncodedImage& frame) {
  int64_t encode_finished_us = rtc::TimeMicros();

  task_queue_.PostTask([this, timestamp_rtp = frame.RtpTimestamp(),
                        spatial_idx = frame.SpatialIndex().value_or(0),
                        temporal_idx = frame.TemporalIndex().value_or(0),
                        width = frame._encodedWidth,
                        height = frame._encodedHeight,
                        frame_type = frame._frameType,
                        frame_size_bytes = frame.size(), qp = frame.qp_,
                        encode_finished_us]() {
    RTC_DCHECK_RUN_ON(&sequence_checker_);

    if (spatial_idx > 0) {
      VideoCodecStats::Frame* base_frame =
          stats_.GetFrame(timestamp_rtp, /*spatial_idx=*/0);

      stats_.AddFrame({.frame_num = base_frame->frame_num,
                       .timestamp_rtp = timestamp_rtp,
                       .spatial_idx = spatial_idx,
                       .encode_start = base_frame->encode_start});
    }

    VideoCodecStats::Frame* fs = stats_.GetFrame(timestamp_rtp, spatial_idx);
    fs->spatial_idx = spatial_idx;
    fs->temporal_idx = temporal_idx;
    fs->width = width;
    fs->height = height;
    fs->frame_size = DataSize::Bytes(frame_size_bytes);
    fs->qp = qp;
    fs->keyframe = frame_type == VideoFrameType::kVideoFrameKey;
    fs->encode_time = Timestamp::Micros(encode_finished_us) - fs->encode_start;
    fs->encoded = true;
  });
}

void VideoCodecAnalyzer::StartDecode(const EncodedImage& frame) {
  int64_t decode_start_us = rtc::TimeMicros();
  task_queue_.PostTask([this, timestamp_rtp = frame.RtpTimestamp(),
                        spatial_idx = frame.SpatialIndex().value_or(0),
                        frame_size_bytes = frame.size(), decode_start_us]() {
    RTC_DCHECK_RUN_ON(&sequence_checker_);

    VideoCodecStats::Frame* fs = stats_.GetFrame(timestamp_rtp, spatial_idx);
    if (fs == nullptr) {
      if (frame_num_.find(timestamp_rtp) == frame_num_.end()) {
        frame_num_[timestamp_rtp] = num_frames_++;
      }
      stats_.AddFrame({.frame_num = frame_num_[timestamp_rtp],
                       .timestamp_rtp = timestamp_rtp,
                       .spatial_idx = spatial_idx,
                       .frame_size = DataSize::Bytes(frame_size_bytes)});
      fs = stats_.GetFrame(timestamp_rtp, spatial_idx);
    }

    fs->decode_start = Timestamp::Micros(decode_start_us);
  });
}

void VideoCodecAnalyzer::FinishDecode(const VideoFrame& frame,
                                      int spatial_idx) {
  int64_t decode_finished_us = rtc::TimeMicros();
  task_queue_.PostTask([this, timestamp_rtp = frame.timestamp(), spatial_idx,
                        width = frame.width(), height = frame.height(),
                        decode_finished_us]() {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    VideoCodecStats::Frame* fs = stats_.GetFrame(timestamp_rtp, spatial_idx);
    fs->decode_time = Timestamp::Micros(decode_finished_us) - fs->decode_start;

    if (!fs->encoded) {
      fs->width = width;
      fs->height = height;
    }

    fs->decoded = true;
  });

  if (reference_video_source_ != nullptr) {
    // Copy hardware-backed frame into main memory to release output buffers
    // which number may be limited in hardware decoders.
    rtc::scoped_refptr<I420BufferInterface> decoded_buffer =
        frame.video_frame_buffer()->ToI420();

    task_queue_.PostTask([this, decoded_buffer,
                          timestamp_rtp = frame.timestamp(), spatial_idx]() {
      RTC_DCHECK_RUN_ON(&sequence_checker_);
      VideoFrame ref_frame = reference_video_source_->GetFrame(
          timestamp_rtp, {.width = decoded_buffer->width(),
                          .height = decoded_buffer->height()});
      rtc::scoped_refptr<I420BufferInterface> ref_buffer =
          ref_frame.video_frame_buffer()->ToI420();

      Psnr psnr = CalcPsnr(*decoded_buffer, *ref_buffer);

      VideoCodecStats::Frame* fs =
          this->stats_.GetFrame(timestamp_rtp, spatial_idx);
      fs->psnr = psnr;
    });
  }
}

std::unique_ptr<VideoCodecStats> VideoCodecAnalyzer::GetStats() {
  std::unique_ptr<VideoCodecStats> stats;
  rtc::Event ready;
  task_queue_.PostTask([this, &stats, &ready]() mutable {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    stats.reset(new VideoCodecStatsImpl(stats_));
    ready.Set();
  });
  ready.Wait(rtc::Event::kForever);
  return stats;
}

}  // namespace test
}  // namespace webrtc
