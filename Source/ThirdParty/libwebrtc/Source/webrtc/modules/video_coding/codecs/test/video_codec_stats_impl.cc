/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/video_codec_stats_impl.h"

#include <algorithm>

#include "api/numerics/samples_stats_counter.h"
#include "api/test/metrics/metrics_logger.h"
#include "rtc_base/checks.h"
#include "rtc_base/time_utils.h"

namespace webrtc {
namespace test {
namespace {
using Frame = VideoCodecStats::Frame;
using Stream = VideoCodecStats::Stream;

constexpr Frequency k90kHz = Frequency::Hertz(90000);

class LeakyBucket {
 public:
  LeakyBucket() : level_bits_(0) {}

  // Updates bucket level and returns its current level in bits. Data is remove
  // from bucket with rate equal to target bitrate of previous frame. Bucket
  // level is tracked with floating point precision. Returned value of bucket
  // level is rounded up.
  int Update(const Frame& frame) {
    RTC_CHECK(frame.target_bitrate) << "Bitrate must be specified.";

    if (prev_frame_) {
      RTC_CHECK_GT(frame.timestamp_rtp, prev_frame_->timestamp_rtp)
          << "Timestamp must increase.";
      TimeDelta passed =
          (frame.timestamp_rtp - prev_frame_->timestamp_rtp) / k90kHz;
      level_bits_ -=
          prev_frame_->target_bitrate->bps() * passed.us() / 1000000.0;
      level_bits_ = std::max(level_bits_, 0.0);
    }

    prev_frame_ = frame;

    level_bits_ += frame.frame_size.bytes() * 8;
    return static_cast<int>(std::ceil(level_bits_));
  }

 private:
  absl::optional<Frame> prev_frame_;
  double level_bits_;
};

// Merges spatial layer frames into superframes.
std::vector<Frame> Merge(const std::vector<Frame>& frames) {
  std::vector<Frame> superframes;
  // Map from frame timestamp to index in `superframes` vector.
  std::map<uint32_t, int> index;

  for (const auto& f : frames) {
    if (index.find(f.timestamp_rtp) == index.end()) {
      index[f.timestamp_rtp] = static_cast<int>(superframes.size());
      superframes.push_back(f);
      continue;
    }

    Frame& sf = superframes[index[f.timestamp_rtp]];

    sf.width = std::max(sf.width, f.width);
    sf.height = std::max(sf.height, f.height);
    sf.frame_size += f.frame_size;
    sf.keyframe |= f.keyframe;

    sf.encode_time = std::max(sf.encode_time, f.encode_time);
    sf.decode_time = std::max(sf.decode_time, f.decode_time);

    if (f.spatial_idx > sf.spatial_idx) {
      if (f.qp) {
        sf.qp = f.qp;
      }
      if (f.psnr) {
        sf.psnr = f.psnr;
      }
    }

    sf.spatial_idx = std::max(sf.spatial_idx, f.spatial_idx);
    sf.temporal_idx = std::max(sf.temporal_idx, f.temporal_idx);

    sf.encoded |= f.encoded;
    sf.decoded |= f.decoded;
  }

  return superframes;
}

Timestamp RtpToTime(uint32_t timestamp_rtp) {
  return Timestamp::Micros((timestamp_rtp / k90kHz).us());
}

SamplesStatsCounter::StatsSample StatsSample(double value, Timestamp time) {
  return SamplesStatsCounter::StatsSample{value, time};
}

TimeDelta CalcTotalDuration(const std::vector<Frame>& frames) {
  RTC_CHECK(!frames.empty());
  TimeDelta duration = TimeDelta::Zero();
  if (frames.size() > 1) {
    duration +=
        (frames.rbegin()->timestamp_rtp - frames.begin()->timestamp_rtp) /
        k90kHz;
  }

  // Add last frame duration. If target frame rate is provided, calculate frame
  // duration from it. Otherwise, assume duration of last frame is the same as
  // duration of preceding frame.
  if (frames.rbegin()->target_framerate) {
    duration += 1 / *frames.rbegin()->target_framerate;
  } else {
    RTC_CHECK_GT(frames.size(), 1u);
    duration += (frames.rbegin()->timestamp_rtp -
                 std::next(frames.rbegin())->timestamp_rtp) /
                k90kHz;
  }

  return duration;
}
}  // namespace

std::vector<Frame> VideoCodecStatsImpl::Slice(
    absl::optional<Filter> filter) const {
  std::vector<Frame> frames;
  for (const auto& [frame_id, f] : frames_) {
    if (filter.has_value()) {
      if (filter->first_frame.has_value() &&
          f.frame_num < *filter->first_frame) {
        continue;
      }
      if (filter->last_frame.has_value() && f.frame_num > *filter->last_frame) {
        continue;
      }
      if (filter->spatial_idx.has_value() &&
          f.spatial_idx != *filter->spatial_idx) {
        continue;
      }
      if (filter->temporal_idx.has_value() &&
          f.temporal_idx > *filter->temporal_idx) {
        continue;
      }
    }
    frames.push_back(f);
  }
  return frames;
}

Stream VideoCodecStatsImpl::Aggregate(const std::vector<Frame>& frames) const {
  std::vector<Frame> superframes = Merge(frames);
  RTC_CHECK(!superframes.empty());

  LeakyBucket leacky_bucket;
  Stream stream;
  for (size_t i = 0; i < superframes.size(); ++i) {
    Frame& f = superframes[i];
    Timestamp time = RtpToTime(f.timestamp_rtp);

    if (!f.frame_size.IsZero()) {
      stream.width.AddSample(StatsSample(f.width, time));
      stream.height.AddSample(StatsSample(f.height, time));
      stream.frame_size_bytes.AddSample(
          StatsSample(f.frame_size.bytes(), time));
      stream.keyframe.AddSample(StatsSample(f.keyframe, time));
      if (f.qp) {
        stream.qp.AddSample(StatsSample(*f.qp, time));
      }
    }

    if (f.encoded) {
      stream.encode_time_ms.AddSample(StatsSample(f.encode_time.ms(), time));
    }

    if (f.decoded) {
      stream.decode_time_ms.AddSample(StatsSample(f.decode_time.ms(), time));
    }

    if (f.psnr) {
      stream.psnr.y.AddSample(StatsSample(f.psnr->y, time));
      stream.psnr.u.AddSample(StatsSample(f.psnr->u, time));
      stream.psnr.v.AddSample(StatsSample(f.psnr->v, time));
    }

    if (f.target_framerate) {
      stream.target_framerate_fps.AddSample(
          StatsSample(f.target_framerate->millihertz() / 1000.0, time));
    }

    if (f.target_bitrate) {
      stream.target_bitrate_kbps.AddSample(
          StatsSample(f.target_bitrate->bps() / 1000.0, time));

      int buffer_level_bits = leacky_bucket.Update(f);
      stream.transmission_time_ms.AddSample(
          StatsSample(buffer_level_bits * rtc::kNumMillisecsPerSec /
                          f.target_bitrate->bps(),
                      RtpToTime(f.timestamp_rtp)));
    }
  }

  TimeDelta duration = CalcTotalDuration(superframes);
  DataRate encoded_bitrate =
      DataSize::Bytes(stream.frame_size_bytes.GetSum()) / duration;

  int num_encoded_frames = stream.frame_size_bytes.NumSamples();
  Frequency encoded_framerate = num_encoded_frames / duration;

  absl::optional<double> bitrate_mismatch_pct;
  if (auto target_bitrate = superframes.begin()->target_bitrate;
      target_bitrate) {
    bitrate_mismatch_pct = 100.0 *
                           (encoded_bitrate.bps() - target_bitrate->bps()) /
                           target_bitrate->bps();
  }

  absl::optional<double> framerate_mismatch_pct;
  if (auto target_framerate = superframes.begin()->target_framerate;
      target_framerate) {
    framerate_mismatch_pct =
        100.0 *
        (encoded_framerate.millihertz() - target_framerate->millihertz()) /
        target_framerate->millihertz();
  }

  for (auto& f : superframes) {
    Timestamp time = RtpToTime(f.timestamp_rtp);
    stream.encoded_bitrate_kbps.AddSample(
        StatsSample(encoded_bitrate.bps() / 1000.0, time));

    stream.encoded_framerate_fps.AddSample(
        StatsSample(encoded_framerate.millihertz() / 1000.0, time));

    if (bitrate_mismatch_pct) {
      stream.bitrate_mismatch_pct.AddSample(
          StatsSample(*bitrate_mismatch_pct, time));
    }

    if (framerate_mismatch_pct) {
      stream.framerate_mismatch_pct.AddSample(
          StatsSample(*framerate_mismatch_pct, time));
    }
  }

  return stream;
}

void VideoCodecStatsImpl::AddFrame(const Frame& frame) {
  FrameId frame_id{.timestamp_rtp = frame.timestamp_rtp,
                   .spatial_idx = frame.spatial_idx};
  RTC_CHECK(frames_.find(frame_id) == frames_.end())
      << "Frame with timestamp_rtp=" << frame.timestamp_rtp
      << " and spatial_idx=" << frame.spatial_idx << " already exists";

  frames_[frame_id] = frame;
}

Frame* VideoCodecStatsImpl::GetFrame(uint32_t timestamp_rtp, int spatial_idx) {
  FrameId frame_id{.timestamp_rtp = timestamp_rtp, .spatial_idx = spatial_idx};
  if (frames_.find(frame_id) == frames_.end()) {
    return nullptr;
  }
  return &frames_.find(frame_id)->second;
}

}  // namespace test
}  // namespace webrtc
