/*
 * Copyright 2026 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include "test/video_frame_matcher.h"

#include <cstdint>
#include <ostream>

#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "api/scoped_refptr.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_buffer.h"
#include "rtc_base/checks.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::Matcher;
using ::testing::MatchResultListener;

void PrintOneChannelToOs(ArrayView<const uint8_t> data,
                         int width,
                         int height,
                         std::ostream& os) {
  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < width; ++j) {
      os << int{data[i * width + j]} << ",";
    }
    os << "\n";
  }
}

void PrintOneChannelToListener(ArrayView<const uint8_t> data,
                               int width,
                               int height,
                               MatchResultListener& listener) {
  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < width; ++j) {
      listener << int{data[i * width + j]} << ",";
    }
    listener << "\n";
  }
}

enum class SampleType { kY, kU, kV };

absl::string_view SampleTypeToString(SampleType type) {
  switch (type) {
    case SampleType::kY:
      return "Y";
    case SampleType::kU:
      return "U";
    case SampleType::kV:
      return "V";
  }
}

}  // namespace

class PixelValuesEqualMatcher {
 public:
  using is_gtest_matcher = void;

  explicit PixelValuesEqualMatcher(const VideoFrame& expected_frame)
      : expected_frame_buffer_(expected_frame.video_frame_buffer()->ToI420()),
        width_(expected_frame.width()),
        height_(expected_frame.height()) {}

  bool MatchAndExplain(const VideoFrame& actual_frame,
                       MatchResultListener* listener) const {
    if (actual_frame.width() != width_ || actual_frame.height() != height_) {
      *listener << "which has dimensions " << actual_frame.width() << "x"
                << actual_frame.height() << ", but expected dimensions are "
                << width_ << "x" << height_;
      return false;
    }

    const scoped_refptr<I420BufferInterface> actual_frame_buffer =
        actual_frame.video_frame_buffer()->ToI420();
    if (actual_frame_buffer->ChromaWidth() !=
            expected_frame_buffer_->ChromaWidth() ||
        actual_frame_buffer->ChromaHeight() !=
            expected_frame_buffer_->ChromaHeight()) {
      *listener << "which has chroma dimensions "
                << actual_frame_buffer->ChromaWidth() << "x"
                << actual_frame_buffer->ChromaHeight()
                << ", but expected chroma dimensions are "
                << expected_frame_buffer_->ChromaWidth() << "x"
                << expected_frame_buffer_->ChromaHeight();
      return false;
    }

    // The reason for going through all the channels is to make sure that all
    // channels are as expected. Even if one of the channels is not as expected,
    // the test would not terminate until all the channels are checked. This is
    // done in order to give a more detailed error message.
    bool is_same = true;
    is_same &= DataMatricesAreEqual(
        MakeArrayView(
            expected_frame_buffer_->DataY(),
            expected_frame_buffer_->width() * expected_frame_buffer_->height()),
        MakeArrayView(
            actual_frame_buffer->DataY(),
            actual_frame_buffer->width() * actual_frame_buffer->height()),
        width_, height_, SampleType::kY, *listener);

    is_same &= DataMatricesAreEqual(
        MakeArrayView(expected_frame_buffer_->DataU(),
                      expected_frame_buffer_->ChromaWidth() *
                          expected_frame_buffer_->ChromaHeight()),
        MakeArrayView(actual_frame_buffer->DataU(),
                      actual_frame_buffer->ChromaWidth() *
                          actual_frame_buffer->ChromaHeight()),
        width_ / 2, height_ / 2, SampleType::kU, *listener);

    is_same &= DataMatricesAreEqual(
        MakeArrayView(expected_frame_buffer_->DataV(),
                      expected_frame_buffer_->ChromaWidth() *
                          expected_frame_buffer_->ChromaHeight()),
        MakeArrayView(actual_frame_buffer->DataV(),
                      actual_frame_buffer->ChromaWidth() *
                          actual_frame_buffer->ChromaHeight()),
        width_ / 2, height_ / 2, SampleType::kV, *listener);

    return is_same;
  }

  void DescribeTo(std::ostream* os) const {
    *os << "is the actual frame to have the following Y channel:\n";
    PrintOneChannelToOs(MakeArrayView(expected_frame_buffer_->DataY(),
                                      expected_frame_buffer_->width() *
                                          expected_frame_buffer_->height()),
                        width_, height_, *os);

    *os << "is the actual frame to have the following U channel:\n";
    PrintOneChannelToOs(
        MakeArrayView(expected_frame_buffer_->DataU(),
                      expected_frame_buffer_->ChromaWidth() *
                          expected_frame_buffer_->ChromaHeight()),
        width_ / 2, height_ / 2, *os);

    *os << "or the actual frame to have the following V channel:\n";
    PrintOneChannelToOs(
        MakeArrayView(expected_frame_buffer_->DataV(),
                      expected_frame_buffer_->ChromaWidth() *
                          expected_frame_buffer_->ChromaHeight()),
        width_ / 2, height_ / 2, *os);
  }

  void DescribeNegationTo(std::ostream* os) const {
    *os << "is the actual frame to not have the following Y channel:\n";
    PrintOneChannelToOs(MakeArrayView(expected_frame_buffer_->DataY(),
                                      expected_frame_buffer_->width() *
                                          expected_frame_buffer_->height()),
                        width_, height_, *os);

    *os << "is the actual frame to not have the following U channel:\n";
    PrintOneChannelToOs(
        MakeArrayView(expected_frame_buffer_->DataU(),
                      expected_frame_buffer_->ChromaWidth() *
                          expected_frame_buffer_->ChromaHeight()),
        width_ / 2, height_ / 2, *os);

    *os << "and the actual frame to not have the following V channel:\n";
    PrintOneChannelToOs(
        MakeArrayView(expected_frame_buffer_->DataV(),
                      expected_frame_buffer_->ChromaWidth() *
                          expected_frame_buffer_->ChromaHeight()),
        width_ / 2, height_ / 2, *os);
  }

  bool DataMatricesAreEqual(ArrayView<const uint8_t> expected_data,
                            ArrayView<const uint8_t> actual_data,
                            int width,
                            int height,
                            SampleType sample_type,
                            MatchResultListener& listener) const {
    RTC_CHECK_LE(width, width_);
    RTC_CHECK_LE(height, height_);

    for (int i = 0; i < height * width; ++i) {
      if (expected_data[i] != actual_data[i]) {
        listener << "\n"
                 << SampleTypeToString(sample_type)
                 << " content is not the same. The actual data is:\n";
        PrintOneChannelToListener(actual_data, width, height, listener);
        listener << "First index to differ is at position (" << (i / width)
                 << ", " << (i % width)
                 << "). Here the expected value was: " << int{expected_data[i]}
                 << ", and the actual value was: " << int{actual_data[i]}
                 << ".\n";
        return false;
      }
    }
    return true;
  }

 private:
  const scoped_refptr<I420BufferInterface> expected_frame_buffer_;

  const int width_;
  const int height_;
};

Matcher<const VideoFrame&> PixelValuesEqual(const VideoFrame& expected_frame) {
  return PixelValuesEqualMatcher(expected_frame);
}

}  // namespace webrtc
