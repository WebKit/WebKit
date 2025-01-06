/*
 * Copyright 2024 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include "video/corruption_detection/halton_frame_sampler.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "api/scoped_refptr.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame_buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "video/corruption_detection/halton_sequence.h"

namespace webrtc {
namespace {

constexpr int kMaxFramesBetweenSamples = 33;

// Corresponds to 1 second for RTP timestamps (which are 90kHz).
constexpr uint32_t kMaxDurationBetweenSamples = 90'000;

// The second *time* is always later than the first. If the second *timestamp*
// is smaller than the first, we interpret that as if one wraparound has
// occurred.
uint32_t EnoughTimeHasPassed(uint32_t from, uint32_t to) {
  return (to - from) >= kMaxDurationBetweenSamples;
}

}  // namespace

HaltonFrameSampler::HaltonFrameSampler()
    : coordinate_sampler_prng_(HaltonSequence(2)) {}

std::vector<HaltonFrameSampler::Coordinates>
HaltonFrameSampler::GetSampleCoordinatesForFrameIfFrameShouldBeSampled(
    bool is_key_frame,
    uint32_t rtp_timestamp,
    int num_samples) {
  if (num_samples < 1) {
    return {};
  }
  if (rtp_timestamp_last_frame_sampled_.has_value()) {
    RTC_CHECK_NE(*rtp_timestamp_last_frame_sampled_, rtp_timestamp);
  }
  if (is_key_frame || frames_until_next_sample_ <= 0 ||
      !rtp_timestamp_last_frame_sampled_.has_value() ||
      EnoughTimeHasPassed(*rtp_timestamp_last_frame_sampled_, rtp_timestamp)) {
    frames_until_next_sample_ =
        (kMaxFramesBetweenSamples - 1) - (frames_sampled_ % 8);
    ++frames_sampled_;
    rtp_timestamp_last_frame_sampled_ = rtp_timestamp;
    return GetSampleCoordinatesForFrame(num_samples);
  }
  --frames_until_next_sample_;
  return {};
}

std::vector<HaltonFrameSampler::Coordinates>
HaltonFrameSampler::GetSampleCoordinatesForFrame(int num_samples) {
  RTC_CHECK_GE(num_samples, 1);
  std::vector<Coordinates> coordinates;
  coordinates.reserve(num_samples);
  for (int i = 0; i < num_samples; ++i) {
    coordinates.push_back(GetNextSampleCoordinates());
  }
  return coordinates;
}

HaltonFrameSampler::Coordinates HaltonFrameSampler::GetNextSampleCoordinates() {
  std::vector<double> point = coordinate_sampler_prng_.GetNext();
  return {.row = point[0], .column = point[1]};
}

void HaltonFrameSampler::Restart() {
  coordinate_sampler_prng_.Reset();
}

int HaltonFrameSampler::GetCurrentIndex() const {
  return coordinate_sampler_prng_.GetCurrentIndex();
}

void HaltonFrameSampler::SetCurrentIndex(int index) {
  coordinate_sampler_prng_.SetCurrentIndex(index);
}

// Apply Gaussian filtering to the data.
double GetFilteredElement(int width,
                          int height,
                          int stride,
                          const uint8_t* data,
                          int row,
                          int column,
                          double std_dev) {
  RTC_CHECK_GE(row, 0);
  RTC_CHECK_LT(row, height);
  RTC_CHECK_GE(column, 0);
  RTC_CHECK_LT(column, width);
  RTC_CHECK_GE(stride, width);
  RTC_CHECK_GE(std_dev, 0.0);

  if (std_dev == 0.0) {
    return data[row * stride + column];
  }

  const double kCutoff = 0.2;
  const int kMaxDistance =
      std::ceil(sqrt(-2.0 * std::log(kCutoff) * std::pow(std_dev, 2.0))) - 1;
  RTC_CHECK_GE(kMaxDistance, 0);
  if (kMaxDistance == 0) {
    return data[row * stride + column];
  }

  double element_sum = 0.0;
  double total_weight = 0.0;
  for (int r = std::max(row - kMaxDistance, 0);
       r < std::min(row + kMaxDistance + 1, height); ++r) {
    for (int c = std::max(column - kMaxDistance, 0);
         c < std::min(column + kMaxDistance + 1, width); ++c) {
      double weight =
          std::exp(-1.0 * (std::pow(row - r, 2) + std::pow(column - c, 2)) /
                   (2.0 * std::pow(std_dev, 2)));
      element_sum += data[r * stride + c] * weight;
      total_weight += weight;
    }
  }
  return element_sum / total_weight;
}

std::vector<FilteredSample> GetSampleValuesForFrame(
    const scoped_refptr<I420BufferInterface> i420_frame_buffer,
    std::vector<HaltonFrameSampler::Coordinates> sample_coordinates,
    int scaled_width,
    int scaled_height,
    double std_dev_gaussian_blur) {
  // Validate input.
  if (i420_frame_buffer == nullptr) {
    RTC_LOG(LS_WARNING) << "The framebuffer must not be nullptr";
    return {};
  }
  if (sample_coordinates.empty()) {
    RTC_LOG(LS_WARNING) << "There must be at least one coordinate provided";
    return {};
  }
  for (HaltonFrameSampler::Coordinates coordinate : sample_coordinates) {
    if (coordinate.column < 0.0 || coordinate.column >= 1.0 ||
        coordinate.row < 0.0 || coordinate.row >= 1.0) {
      RTC_LOG(LS_WARNING) << "The coordinates must be in [0,1): column="
                          << coordinate.column << ", row=" << coordinate.row
                          << ".\n";
      return {};
    }
  }
  if (scaled_width <= 0 || scaled_height <= 0) {
    RTC_LOG(LS_WARNING)
        << "The width and height to scale to must be positive: width="
        << scaled_width << ", height=" << scaled_height << ".\n";
    return {};
  }
  if (std_dev_gaussian_blur < 0.0) {
    RTC_LOG(LS_WARNING)
        << "The standard deviation for the Gaussian blur must not be negative: "
        << std_dev_gaussian_blur << ".\n";
    return {};
  }

  // Scale the frame to the desired resolution:
  // 1. Create a new buffer with the desired resolution.
  // 2. Scale the old buffer to the size of the new buffer.
  scoped_refptr<I420Buffer> scaled_i420_buffer =
      I420Buffer::Create(scaled_width, scaled_height);
  scaled_i420_buffer->ScaleFrom(*i420_frame_buffer);

  // Treat the planes as if they would have the following 2-dimensional layout:
  // +------+---+
  // |      | U |
  // |  Y   +---+
  // |      | V |
  // +------+---+
  // where width:=(Y.width+U.width) and height:=Y.height.
  // When interpreting the 2D sample coordinates, we simply treat them
  // as if they were taken from the above layout. We then need to translate the
  // coordinates back to the corresponding plane's corresponding 2D coordinates.
  // Then we find the filtered value that corresponds to those coordinates.
  int width_merged_planes =
      scaled_i420_buffer->width() + scaled_i420_buffer->ChromaWidth();
  int height_merged_planes = scaled_i420_buffer->height();
  // Fetch the sample value for all of the requested coordinates.
  std::vector<FilteredSample> filtered_samples;
  filtered_samples.reserve(sample_coordinates.size());
  for (HaltonFrameSampler::Coordinates coordinate : sample_coordinates) {
    // Scale the coordinates from [0,1) to [0,`width_merged_planes`) and
    // [0,`height_merged_planes`). Truncation is intentional.
    int column = coordinate.column * width_merged_planes;
    int row = coordinate.row * height_merged_planes;

    // Map to plane coordinates and fetch the value.
    double value_for_coordinate;
    if (column < scaled_i420_buffer->width()) {
      // Y plane.
      value_for_coordinate = GetFilteredElement(
          scaled_i420_buffer->width(), scaled_i420_buffer->height(),
          scaled_i420_buffer->StrideY(), scaled_i420_buffer->DataY(), row,
          column, std_dev_gaussian_blur);
      filtered_samples.push_back(
          {.value = value_for_coordinate, .plane = ImagePlane::kLuma});
    } else if (row < scaled_i420_buffer->ChromaHeight()) {
      // U plane.
      column -= scaled_i420_buffer->width();
      value_for_coordinate = GetFilteredElement(
          scaled_i420_buffer->ChromaWidth(), scaled_i420_buffer->ChromaHeight(),
          scaled_i420_buffer->StrideU(), scaled_i420_buffer->DataU(), row,
          column, std_dev_gaussian_blur);
      filtered_samples.push_back(
          {.value = value_for_coordinate, .plane = ImagePlane::kChroma});
    } else {
      // V plane.
      column -= scaled_i420_buffer->width();
      row -= scaled_i420_buffer->ChromaHeight();
      value_for_coordinate = GetFilteredElement(
          scaled_i420_buffer->ChromaWidth(), scaled_i420_buffer->ChromaHeight(),
          scaled_i420_buffer->StrideV(), scaled_i420_buffer->DataV(), row,
          column, std_dev_gaussian_blur);
      filtered_samples.push_back(
          {.value = value_for_coordinate, .plane = ImagePlane::kChroma});
    }
  }
  return filtered_samples;
}

}  // namespace webrtc
