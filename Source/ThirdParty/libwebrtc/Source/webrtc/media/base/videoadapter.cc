/*
 *  Copyright (c) 2010 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/videoadapter.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <utility>

#include "api/optional.h"
#include "media/base/mediaconstants.h"
#include "media/base/videocommon.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace {
struct Fraction {
  int numerator;
  int denominator;

  // Determines number of output pixels if both width and height of an input of
  // |input_pixels| pixels is scaled with the fraction numerator / denominator.
  int scale_pixel_count(int input_pixels) {
    return (numerator * numerator * input_pixels) / (denominator * denominator);
  }
};

// Round |value_to_round| to a multiple of |multiple|. Prefer rounding upwards,
// but never more than |max_value|.
int roundUp(int value_to_round, int multiple, int max_value) {
  const int rounded_value =
      (value_to_round + multiple - 1) / multiple * multiple;
  return rounded_value <= max_value ? rounded_value
                                    : (max_value / multiple * multiple);
}

// Generates a scale factor that makes |input_pixels| close to |target_pixels|,
// but no higher than |max_pixels|.
Fraction FindScale(int input_pixels, int target_pixels, int max_pixels) {
  // This function only makes sense for a positive target.
  RTC_DCHECK_GT(target_pixels, 0);
  RTC_DCHECK_GT(max_pixels, 0);
  RTC_DCHECK_GE(max_pixels, target_pixels);

  // Don't scale up original.
  if (target_pixels >= input_pixels)
    return Fraction{1, 1};

  Fraction current_scale = Fraction{1, 1};
  Fraction best_scale = Fraction{1, 1};
  // The minimum (absolute) difference between the number of output pixels and
  // the target pixel count.
  int min_pixel_diff = std::numeric_limits<int>::max();
  if (input_pixels <= max_pixels) {
    // Start condition for 1/1 case, if it is less than max.
    min_pixel_diff = std::abs(input_pixels - target_pixels);
  }

  // Alternately scale down by 2/3 and 3/4. This results in fractions which are
  // effectively scalable. For instance, starting at 1280x720 will result in
  // the series (3/4) => 960x540, (1/2) => 640x360, (3/8) => 480x270,
  // (1/4) => 320x180, (3/16) => 240x125, (1/8) => 160x90.
  while (current_scale.scale_pixel_count(input_pixels) > target_pixels) {
    if (current_scale.numerator % 3 == 0 &&
        current_scale.denominator % 2 == 0) {
      // Multiply by 2/3.
      current_scale.numerator /= 3;
      current_scale.denominator /= 2;
    } else {
      // Multiply by 3/4.
      current_scale.numerator *= 3;
      current_scale.denominator *= 4;
    }

    int output_pixels = current_scale.scale_pixel_count(input_pixels);
    if (output_pixels <= max_pixels) {
      int diff = std::abs(target_pixels - output_pixels);
      if (diff < min_pixel_diff) {
        min_pixel_diff = diff;
        best_scale = current_scale;
      }
    }
  }

  return best_scale;
}
}  // namespace

namespace cricket {

VideoAdapter::VideoAdapter(int required_resolution_alignment)
    : frames_in_(0),
      frames_out_(0),
      frames_scaled_(0),
      adaption_changes_(0),
      previous_width_(0),
      previous_height_(0),
      required_resolution_alignment_(required_resolution_alignment),
      resolution_request_target_pixel_count_(std::numeric_limits<int>::max()),
      resolution_request_max_pixel_count_(std::numeric_limits<int>::max()),
      max_framerate_request_(std::numeric_limits<int>::max()) {}

VideoAdapter::VideoAdapter() : VideoAdapter(1) {}

VideoAdapter::~VideoAdapter() {}

bool VideoAdapter::KeepFrame(int64_t in_timestamp_ns) {
  rtc::CritScope cs(&critical_section_);
  if (max_framerate_request_ <= 0)
    return false;

  int64_t frame_interval_ns =
      requested_format_ ? requested_format_->interval : 0;

  // If |max_framerate_request_| is not set, it will default to maxint, which
  // will lead to a frame_interval_ns rounded to 0.
  frame_interval_ns = std::max<int64_t>(
      frame_interval_ns, rtc::kNumNanosecsPerSec / max_framerate_request_);

  if (frame_interval_ns <= 0) {
    // Frame rate throttling not enabled.
    return true;
  }

  if (next_frame_timestamp_ns_) {
    // Time until next frame should be outputted.
    const int64_t time_until_next_frame_ns =
        (*next_frame_timestamp_ns_ - in_timestamp_ns);

    // Continue if timestamp is within expected range.
    if (std::abs(time_until_next_frame_ns) < 2 * frame_interval_ns) {
      // Drop if a frame shouldn't be outputted yet.
      if (time_until_next_frame_ns > 0)
        return false;
      // Time to output new frame.
      *next_frame_timestamp_ns_ += frame_interval_ns;
      return true;
    }
  }

  // First timestamp received or timestamp is way outside expected range, so
  // reset. Set first timestamp target to just half the interval to prefer
  // keeping frames in case of jitter.
  next_frame_timestamp_ns_ = in_timestamp_ns + frame_interval_ns / 2;
  return true;
}

bool VideoAdapter::AdaptFrameResolution(int in_width,
                                        int in_height,
                                        int64_t in_timestamp_ns,
                                        int* cropped_width,
                                        int* cropped_height,
                                        int* out_width,
                                        int* out_height) {
  rtc::CritScope cs(&critical_section_);
  ++frames_in_;

  // The max output pixel count is the minimum of the requests from
  // OnOutputFormatRequest and OnResolutionRequest.
  int max_pixel_count = resolution_request_max_pixel_count_;
  if (requested_format_) {
    max_pixel_count = std::min(
        max_pixel_count, requested_format_->width * requested_format_->height);
  }
  int target_pixel_count =
      std::min(resolution_request_target_pixel_count_, max_pixel_count);

  // Drop the input frame if necessary.
  if (max_pixel_count <= 0 || !KeepFrame(in_timestamp_ns)) {
    // Show VAdapt log every 90 frames dropped. (3 seconds)
    if ((frames_in_ - frames_out_) % 90 == 0) {
      // TODO(fbarchard): Reduce to LS_VERBOSE when adapter info is not needed
      // in default calls.
      RTC_LOG(LS_INFO) << "VAdapt Drop Frame: scaled " << frames_scaled_
                       << " / out " << frames_out_ << " / in " << frames_in_
                       << " Changes: " << adaption_changes_
                       << " Input: " << in_width << "x" << in_height
                       << " timestamp: " << in_timestamp_ns << " Output: i"
                       << (requested_format_ ? requested_format_->interval : 0);
    }

    // Drop frame.
    return false;
  }

  // Calculate how the input should be cropped.
  if (!requested_format_ ||
      requested_format_->width == 0 || requested_format_->height == 0) {
    *cropped_width = in_width;
    *cropped_height = in_height;
  } else {
    // Adjust |requested_format_| orientation to match input.
    if ((in_width > in_height) !=
        (requested_format_->width > requested_format_->height)) {
      std::swap(requested_format_->width, requested_format_->height);
    }
    const float requested_aspect =
        requested_format_->width /
        static_cast<float>(requested_format_->height);
    *cropped_width =
        std::min(in_width, static_cast<int>(in_height * requested_aspect));
    *cropped_height =
        std::min(in_height, static_cast<int>(in_width / requested_aspect));
  }
  const Fraction scale = FindScale((*cropped_width) * (*cropped_height),
                                   target_pixel_count, max_pixel_count);
  // Adjust cropping slightly to get even integer output size and a perfect
  // scale factor. Make sure the resulting dimensions are aligned correctly
  // to be nice to hardware encoders.
  *cropped_width =
      roundUp(*cropped_width,
              scale.denominator * required_resolution_alignment_, in_width);
  *cropped_height =
      roundUp(*cropped_height,
              scale.denominator * required_resolution_alignment_, in_height);
  RTC_DCHECK_EQ(0, *cropped_width % scale.denominator);
  RTC_DCHECK_EQ(0, *cropped_height % scale.denominator);

  // Calculate final output size.
  *out_width = *cropped_width / scale.denominator * scale.numerator;
  *out_height = *cropped_height / scale.denominator * scale.numerator;
  RTC_DCHECK_EQ(0, *out_width % required_resolution_alignment_);
  RTC_DCHECK_EQ(0, *out_height % required_resolution_alignment_);

  ++frames_out_;
  if (scale.numerator != scale.denominator)
    ++frames_scaled_;

  if (previous_width_ && (previous_width_ != *out_width ||
                          previous_height_ != *out_height)) {
    ++adaption_changes_;
    RTC_LOG(LS_INFO) << "Frame size changed: scaled " << frames_scaled_
                     << " / out " << frames_out_ << " / in " << frames_in_
                     << " Changes: " << adaption_changes_
                     << " Input: " << in_width << "x" << in_height
                     << " Scale: " << scale.numerator << "/"
                     << scale.denominator << " Output: " << *out_width << "x"
                     << *out_height << " i"
                     << (requested_format_ ? requested_format_->interval : 0);
  }

  previous_width_ = *out_width;
  previous_height_ = *out_height;

  return true;
}

void VideoAdapter::OnOutputFormatRequest(const VideoFormat& format) {
  rtc::CritScope cs(&critical_section_);
  requested_format_ = format;
  next_frame_timestamp_ns_ = rtc::nullopt;
}

void VideoAdapter::OnResolutionFramerateRequest(
    const rtc::Optional<int>& target_pixel_count,
    int max_pixel_count,
    int max_framerate_fps) {
  rtc::CritScope cs(&critical_section_);
  resolution_request_max_pixel_count_ = max_pixel_count;
  resolution_request_target_pixel_count_ =
      target_pixel_count.value_or(resolution_request_max_pixel_count_);
  max_framerate_request_ = max_framerate_fps;
}

}  // namespace cricket
