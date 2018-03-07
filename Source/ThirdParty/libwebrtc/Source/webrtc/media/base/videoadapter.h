/*
 *  Copyright (c) 2010 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_BASE_VIDEOADAPTER_H_
#define MEDIA_BASE_VIDEOADAPTER_H_

#include "api/optional.h"
#include "media/base/videocommon.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/criticalsection.h"

namespace cricket {

// VideoAdapter adapts an input video frame to an output frame based on the
// specified input and output formats. The adaptation includes dropping frames
// to reduce frame rate and scaling frames.
// VideoAdapter is thread safe.
class VideoAdapter {
 public:
  VideoAdapter();
  // The output frames will have height and width that is divisible by
  // |required_resolution_alignment|.
  explicit VideoAdapter(int required_resolution_alignment);
  virtual ~VideoAdapter();

  // Return the adapted resolution and cropping parameters given the
  // input resolution. The input frame should first be cropped, then
  // scaled to the final output resolution. Returns true if the frame
  // should be adapted, and false if it should be dropped.
  bool AdaptFrameResolution(int in_width,
                            int in_height,
                            int64_t in_timestamp_ns,
                            int* cropped_width,
                            int* cropped_height,
                            int* out_width,
                            int* out_height);

  // Requests the output frame size and frame interval from
  // |AdaptFrameResolution| to not be larger than |format|. Also, the input
  // frame size will be cropped to match the requested aspect ratio. The
  // requested aspect ratio is orientation agnostic and will be adjusted to
  // maintain the input orientation, so it doesn't matter if e.g. 1280x720 or
  // 720x1280 is requested.
  void OnOutputFormatRequest(const VideoFormat& format);

  // Requests the output frame size from |AdaptFrameResolution| to have as close
  // as possible to |target_pixel_count| pixels (if set) but no more than
  // |max_pixel_count|.
  // |max_framerate_fps| is essentially analogous to |max_pixel_count|, but for
  // framerate rather than resolution.
  // Set |max_pixel_count| and/or |max_framerate_fps| to
  // std::numeric_limit<int>::max() if no upper limit is desired.
  void OnResolutionFramerateRequest(
      const rtc::Optional<int>& target_pixel_count,
      int max_pixel_count,
      int max_framerate_fps);

 private:
  // Determine if frame should be dropped based on input fps and requested fps.
  bool KeepFrame(int64_t in_timestamp_ns);

  int frames_in_;         // Number of input frames.
  int frames_out_;        // Number of output frames.
  int frames_scaled_;     // Number of frames scaled.
  int adaption_changes_;  // Number of changes in scale factor.
  int previous_width_;    // Previous adapter output width.
  int previous_height_;   // Previous adapter output height.
  // Resolution must be divisible by this factor.
  const int required_resolution_alignment_;
  // The target timestamp for the next frame based on requested format.
  rtc::Optional<int64_t> next_frame_timestamp_ns_
      RTC_GUARDED_BY(critical_section_);

  // Max number of pixels requested via calls to OnOutputFormatRequest,
  // OnResolutionRequest respectively.
  // The adapted output format is the minimum of these.
  rtc::Optional<VideoFormat> requested_format_
      RTC_GUARDED_BY(critical_section_);
  int resolution_request_target_pixel_count_ RTC_GUARDED_BY(critical_section_);
  int resolution_request_max_pixel_count_ RTC_GUARDED_BY(critical_section_);
  int max_framerate_request_ RTC_GUARDED_BY(critical_section_);

  // The critical section to protect the above variables.
  rtc::CriticalSection critical_section_;

  RTC_DISALLOW_COPY_AND_ASSIGN(VideoAdapter);
};

}  // namespace cricket

#endif  // MEDIA_BASE_VIDEOADAPTER_H_
