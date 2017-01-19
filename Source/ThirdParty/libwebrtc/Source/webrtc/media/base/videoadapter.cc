/*
 *  Copyright (c) 2010 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/media/base/videoadapter.h"

#include <algorithm>
#include <cstdlib>
#include <limits>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/media/base/mediaconstants.h"
#include "webrtc/media/base/videocommon.h"

namespace {

struct Fraction {
  int numerator;
  int denominator;
};

// Scale factors optimized for in libYUV that we accept.
// Must be sorted in decreasing scale factors for FindScaleLargerThan to work.
const Fraction kScaleFractions[] = {
  {1, 1},
  {3, 4},
  {1, 2},
  {3, 8},
  {1, 4},
  {3, 16},
};

// Round |valueToRound| to a multiple of |multiple|. Prefer rounding upwards,
// but never more than |maxValue|.
int roundUp(int valueToRound, int multiple, int maxValue) {
  const int roundedValue = (valueToRound + multiple - 1) / multiple * multiple;
  return roundedValue <= maxValue ? roundedValue
                                  : (maxValue / multiple * multiple);
}

Fraction FindScaleLessThanOrEqual(int input_num_pixels, int target_num_pixels) {
  float best_distance = std::numeric_limits<float>::max();
  Fraction best_scale = {0, 1};  // Default to 0 if nothing matches.
  for (const auto& fraction : kScaleFractions) {
    const float scale =
        fraction.numerator / static_cast<float>(fraction.denominator);
    float test_num_pixels = input_num_pixels * scale * scale;
    float diff = target_num_pixels - test_num_pixels;
    if (diff < 0) {
      continue;
    }
    if (diff < best_distance) {
      best_distance = diff;
      best_scale = fraction;
      if (best_distance == 0) {  // Found exact match.
        break;
      }
    }
  }
  return best_scale;
}

Fraction FindScaleLargerThan(int input_num_pixels,
                             int target_num_pixels,
                             int* resulting_number_of_pixels) {
  float best_distance = std::numeric_limits<float>::max();
  Fraction best_scale = {1, 1};  // Default to unscaled if nothing matches.
  // Default to input number of pixels.
  float best_number_of_pixels = input_num_pixels;
  for (const auto& fraction : kScaleFractions) {
    const float scale =
        fraction.numerator / static_cast<float>(fraction.denominator);
    float test_num_pixels = input_num_pixels * scale * scale;
    float diff = test_num_pixels - target_num_pixels;
    if (diff <= 0) {
      break;
    }
    if (diff < best_distance) {
      best_distance = diff;
      best_scale = fraction;
      best_number_of_pixels = test_num_pixels;
    }
  }

  *resulting_number_of_pixels = static_cast<int>(best_number_of_pixels + .5f);
  return best_scale;
}

Fraction FindScale(int input_num_pixels,
                   int max_pixel_count_step_up,
                   int max_pixel_count) {
  // Try scale just above |max_pixel_count_step_up_|.
  if (max_pixel_count_step_up > 0) {
    int resulting_pixel_count;
    const Fraction scale = FindScaleLargerThan(
        input_num_pixels, max_pixel_count_step_up, &resulting_pixel_count);
    if (resulting_pixel_count <= max_pixel_count)
      return scale;
  }
  // Return largest scale below |max_pixel_count|.
  return FindScaleLessThanOrEqual(input_num_pixels, max_pixel_count);
}

}  // namespace

namespace cricket {

VideoAdapter::VideoAdapter()
    : frames_in_(0),
      frames_out_(0),
      frames_scaled_(0),
      adaption_changes_(0),
      previous_width_(0),
      previous_height_(0),
      resolution_request_max_pixel_count_(std::numeric_limits<int>::max()),
      resolution_request_max_pixel_count_step_up_(0) {}

VideoAdapter::~VideoAdapter() {}

bool VideoAdapter::KeepFrame(int64_t in_timestamp_ns) {
  rtc::CritScope cs(&critical_section_);
  if (!requested_format_ || requested_format_->interval == 0)
    return true;

  if (next_frame_timestamp_ns_) {
    // Time until next frame should be outputted.
    const int64_t time_until_next_frame_ns =
        (*next_frame_timestamp_ns_ - in_timestamp_ns);

    // Continue if timestamp is withing expected range.
    if (std::abs(time_until_next_frame_ns) < 2 * requested_format_->interval) {
      // Drop if a frame shouldn't be outputted yet.
      if (time_until_next_frame_ns > 0)
        return false;
      // Time to output new frame.
      *next_frame_timestamp_ns_ += requested_format_->interval;
      return true;
    }
  }

  // First timestamp received or timestamp is way outside expected range, so
  // reset. Set first timestamp target to just half the interval to prefer
  // keeping frames in case of jitter.
  next_frame_timestamp_ns_ =
      rtc::Optional<int64_t>(in_timestamp_ns + requested_format_->interval / 2);
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

  // Drop the input frame if necessary.
  if (max_pixel_count == 0 || !KeepFrame(in_timestamp_ns)) {
    // Show VAdapt log every 90 frames dropped. (3 seconds)
    if ((frames_in_ - frames_out_) % 90 == 0) {
      // TODO(fbarchard): Reduce to LS_VERBOSE when adapter info is not needed
      // in default calls.
      LOG(LS_INFO) << "VAdapt Drop Frame: scaled " << frames_scaled_
                   << " / out " << frames_out_
                   << " / in " << frames_in_
                   << " Changes: " << adaption_changes_
                   << " Input: " << in_width
                   << "x" << in_height
                   << " timestamp: " << in_timestamp_ns
                   << " Output: i"
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

  // Find best scale factor.
  const Fraction scale =
      FindScale(*cropped_width * *cropped_height,
                resolution_request_max_pixel_count_step_up_, max_pixel_count);

  // Adjust cropping slightly to get even integer output size and a perfect
  // scale factor.
  *cropped_width = roundUp(*cropped_width, scale.denominator, in_width);
  *cropped_height = roundUp(*cropped_height, scale.denominator, in_height);
  RTC_DCHECK_EQ(0, *cropped_width % scale.denominator);
  RTC_DCHECK_EQ(0, *cropped_height % scale.denominator);

  // Calculate final output size.
  *out_width = *cropped_width / scale.denominator * scale.numerator;
  *out_height = *cropped_height / scale.denominator * scale.numerator;

  ++frames_out_;
  if (scale.numerator != scale.denominator)
    ++frames_scaled_;

  if (previous_width_ && (previous_width_ != *out_width ||
                          previous_height_ != *out_height)) {
    ++adaption_changes_;
    LOG(LS_INFO) << "Frame size changed: scaled " << frames_scaled_ << " / out "
                 << frames_out_ << " / in " << frames_in_
                 << " Changes: " << adaption_changes_ << " Input: " << in_width
                 << "x" << in_height
                 << " Scale: " << scale.numerator << "/" << scale.denominator
                 << " Output: " << *out_width << "x" << *out_height << " i"
                 << (requested_format_ ? requested_format_->interval : 0);
  }

  previous_width_ = *out_width;
  previous_height_ = *out_height;

  return true;
}

void VideoAdapter::OnOutputFormatRequest(const VideoFormat& format) {
  rtc::CritScope cs(&critical_section_);
  requested_format_ = rtc::Optional<VideoFormat>(format);
  next_frame_timestamp_ns_ = rtc::Optional<int64_t>();
}

void VideoAdapter::OnResolutionRequest(
    rtc::Optional<int> max_pixel_count,
    rtc::Optional<int> max_pixel_count_step_up) {
  rtc::CritScope cs(&critical_section_);
  resolution_request_max_pixel_count_ =
      max_pixel_count.value_or(std::numeric_limits<int>::max());
  resolution_request_max_pixel_count_step_up_ =
      max_pixel_count_step_up.value_or(0);
}

}  // namespace cricket
