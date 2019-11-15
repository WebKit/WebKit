/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/frame_analyzer/video_temporal_aligner.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <deque>
#include <iterator>
#include <limits>
#include <vector>

#include "api/video/i420_buffer.h"
#include "api/video/video_frame_buffer.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_tools/frame_analyzer/video_quality_analysis.h"

namespace webrtc {
namespace test {

namespace {

// This constant controls how many frames we look ahead while seeking for the
// match for the next frame. Note that we may span bigger gaps than this number
// since we reset the counter as soon as we find a better match. The seeking
// will stop when there is no improvement in the next kNumberOfFramesLookAhead
// frames. Typically, the SSIM will improve as we get closer and closer to the
// real match.
const int kNumberOfFramesLookAhead = 60;

// Helper class that takes a video and generates an infinite looping video.
class LoopingVideo : public rtc::RefCountedObject<Video> {
 public:
  explicit LoopingVideo(const rtc::scoped_refptr<Video>& video)
      : video_(video) {}

  int width() const override { return video_->width(); }
  int height() const override { return video_->height(); }
  size_t number_of_frames() const override {
    return std::numeric_limits<size_t>::max();
  }

  rtc::scoped_refptr<I420BufferInterface> GetFrame(
      size_t index) const override {
    return video_->GetFrame(index % video_->number_of_frames());
  }

 private:
  const rtc::scoped_refptr<Video> video_;
};

// Helper class that take a vector of frame indices and a video and produces a
// new video where the frames have been reshuffled.
class ReorderedVideo : public rtc::RefCountedObject<Video> {
 public:
  ReorderedVideo(const rtc::scoped_refptr<Video>& video,
                 const std::vector<size_t>& indices)
      : video_(video), indices_(indices) {}

  int width() const override { return video_->width(); }
  int height() const override { return video_->height(); }
  size_t number_of_frames() const override { return indices_.size(); }

  rtc::scoped_refptr<I420BufferInterface> GetFrame(
      size_t index) const override {
    return video_->GetFrame(indices_.at(index));
  }

 private:
  const rtc::scoped_refptr<Video> video_;
  const std::vector<size_t> indices_;
};

// Helper class that takes a video and produces a downscaled video.
class DownscaledVideo : public rtc::RefCountedObject<Video> {
 public:
  DownscaledVideo(float scale_factor, const rtc::scoped_refptr<Video>& video)
      : downscaled_width_(
            static_cast<int>(std::round(scale_factor * video->width()))),
        downscaled_height_(
            static_cast<int>(std::round(scale_factor * video->height()))),
        video_(video) {}

  int width() const override { return downscaled_width_; }
  int height() const override { return downscaled_height_; }
  size_t number_of_frames() const override {
    return video_->number_of_frames();
  }

  rtc::scoped_refptr<I420BufferInterface> GetFrame(
      size_t index) const override {
    const rtc::scoped_refptr<I420BufferInterface> frame =
        video_->GetFrame(index);
    rtc::scoped_refptr<I420Buffer> downscaled_frame =
        I420Buffer::Create(downscaled_width_, downscaled_height_);
    downscaled_frame->ScaleFrom(*frame);
    return downscaled_frame;
  }

 private:
  const int downscaled_width_;
  const int downscaled_height_;
  const rtc::scoped_refptr<Video> video_;
};

// Helper class that takes a video and caches the latest frame access. This
// improves performance a lot since the original source is often from a file.
class CachedVideo : public rtc::RefCountedObject<Video> {
 public:
  CachedVideo(int max_cache_size, const rtc::scoped_refptr<Video>& video)
      : max_cache_size_(max_cache_size), video_(video) {}

  int width() const override { return video_->width(); }
  int height() const override { return video_->height(); }
  size_t number_of_frames() const override {
    return video_->number_of_frames();
  }

  rtc::scoped_refptr<I420BufferInterface> GetFrame(
      size_t index) const override {
    for (const CachedFrame& cached_frame : cache_) {
      if (cached_frame.index == index)
        return cached_frame.frame;
    }

    rtc::scoped_refptr<I420BufferInterface> frame = video_->GetFrame(index);
    cache_.push_front({index, frame});
    if (cache_.size() > max_cache_size_)
      cache_.pop_back();

    return frame;
  }

 private:
  struct CachedFrame {
    size_t index;
    rtc::scoped_refptr<I420BufferInterface> frame;
  };

  const size_t max_cache_size_;
  const rtc::scoped_refptr<Video> video_;
  mutable std::deque<CachedFrame> cache_;
};

// Try matching the test frame against all frames in the reference video and
// return the index of the best matching frame.
size_t FindBestMatch(const rtc::scoped_refptr<I420BufferInterface>& test_frame,
                     const Video& reference_video) {
  std::vector<double> ssim;
  for (const auto& ref_frame : reference_video)
    ssim.push_back(Ssim(test_frame, ref_frame));
  return std::distance(ssim.begin(),
                       std::max_element(ssim.begin(), ssim.end()));
}

// Find and return the index of the frame matching the test frame. The search
// starts at the starting index and continues until there is no better match
// within the next kNumberOfFramesLookAhead frames.
size_t FindNextMatch(const rtc::scoped_refptr<I420BufferInterface>& test_frame,
                     const Video& reference_video,
                     size_t start_index) {
  const double start_ssim =
      Ssim(test_frame, reference_video.GetFrame(start_index));
  for (int i = 1; i < kNumberOfFramesLookAhead; ++i) {
    const size_t next_index = start_index + i;
    // If we find a better match, restart the search at that point.
    if (start_ssim < Ssim(test_frame, reference_video.GetFrame(next_index)))
      return FindNextMatch(test_frame, reference_video, next_index);
  }
  // The starting index was the best match.
  return start_index;
}

}  // namespace

std::vector<size_t> FindMatchingFrameIndices(
    const rtc::scoped_refptr<Video>& reference_video,
    const rtc::scoped_refptr<Video>& test_video) {
  // This is done to get a 10x speedup. We don't need the full resolution in
  // order to match frames, and we should limit file access and not read the
  // same memory tens of times.
  const float kScaleFactor = 0.25f;
  const rtc::scoped_refptr<Video> cached_downscaled_reference_video =
      new CachedVideo(kNumberOfFramesLookAhead,
                      new DownscaledVideo(kScaleFactor, reference_video));
  const rtc::scoped_refptr<Video> downscaled_test_video =
      new DownscaledVideo(kScaleFactor, test_video);

  // Assume the video is looping around.
  const rtc::scoped_refptr<Video> looping_reference_video =
      new LoopingVideo(cached_downscaled_reference_video);

  std::vector<size_t> match_indices;
  for (const rtc::scoped_refptr<I420BufferInterface>& test_frame :
       *downscaled_test_video) {
    if (match_indices.empty()) {
      // First frame.
      match_indices.push_back(
          FindBestMatch(test_frame, *cached_downscaled_reference_video));
    } else {
      match_indices.push_back(FindNextMatch(
          test_frame, *looping_reference_video, match_indices.back()));
    }
  }

  return match_indices;
}

rtc::scoped_refptr<Video> ReorderVideo(const rtc::scoped_refptr<Video>& video,
                                       const std::vector<size_t>& indices) {
  return new ReorderedVideo(new LoopingVideo(video), indices);
}

rtc::scoped_refptr<Video> GenerateAlignedReferenceVideo(
    const rtc::scoped_refptr<Video>& reference_video,
    const rtc::scoped_refptr<Video>& test_video) {
  return GenerateAlignedReferenceVideo(
      reference_video, FindMatchingFrameIndices(reference_video, test_video));
}

rtc::scoped_refptr<Video> GenerateAlignedReferenceVideo(
    const rtc::scoped_refptr<Video>& reference_video,
    const std::vector<size_t>& indices) {
  return ReorderVideo(reference_video, indices);
}

}  // namespace test
}  // namespace webrtc
