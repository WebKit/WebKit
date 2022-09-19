/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/frame_analyzer/video_color_aligner.h"

#include <stddef.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "api/array_view.h"
#include "api/make_ref_counted.h"
#include "api/video/i420_buffer.h"
#include "rtc_base/checks.h"
#include "rtc_tools/frame_analyzer/linear_least_squares.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"
#include "third_party/libyuv/include/libyuv/scale.h"

namespace webrtc {
namespace test {

namespace {

// Helper function for AdjustColors(). This functions calculates a single output
// row for y with the given color coefficients. The u/v channels are assumed to
// be subsampled by a factor of 2, which is the case of I420.
void CalculateYChannel(rtc::ArrayView<const uint8_t> y_data,
                       rtc::ArrayView<const uint8_t> u_data,
                       rtc::ArrayView<const uint8_t> v_data,
                       const std::array<float, 4>& coeff,
                       rtc::ArrayView<uint8_t> output) {
  RTC_CHECK_EQ(y_data.size(), output.size());
  // Each u/v element represents two y elements. Make sure we have enough to
  // cover the Y values.
  RTC_CHECK_GE(u_data.size() * 2, y_data.size());
  RTC_CHECK_GE(v_data.size() * 2, y_data.size());

  // Do two pixels at a time since u/v are subsampled.
  for (size_t i = 0; i * 2 < y_data.size() - 1; ++i) {
    const float uv_contribution =
        coeff[1] * u_data[i] + coeff[2] * v_data[i] + coeff[3];

    const float val0 = coeff[0] * y_data[i * 2 + 0] + uv_contribution;
    const float val1 = coeff[0] * y_data[i * 2 + 1] + uv_contribution;

    // Clamp result to a byte.
    output[i * 2 + 0] = static_cast<uint8_t>(
        std::round(std::max(0.0f, std::min(val0, 255.0f))));
    output[i * 2 + 1] = static_cast<uint8_t>(
        std::round(std::max(0.0f, std::min(val1, 255.0f))));
  }

  // Handle the last pixel for odd widths.
  if (y_data.size() % 2 == 1) {
    const float val = coeff[0] * y_data[y_data.size() - 1] +
                      coeff[1] * u_data[(y_data.size() - 1) / 2] +
                      coeff[2] * v_data[(y_data.size() - 1) / 2] + coeff[3];
    output[y_data.size() - 1] =
        static_cast<uint8_t>(std::round(std::max(0.0f, std::min(val, 255.0f))));
  }
}

// Helper function for AdjustColors(). This functions calculates a single output
// row for either u or v, with the given color coefficients. Y, U, and V are
// assumed to be the same size, i.e. no subsampling.
void CalculateUVChannel(rtc::ArrayView<const uint8_t> y_data,
                        rtc::ArrayView<const uint8_t> u_data,
                        rtc::ArrayView<const uint8_t> v_data,
                        const std::array<float, 4>& coeff,
                        rtc::ArrayView<uint8_t> output) {
  RTC_CHECK_EQ(y_data.size(), u_data.size());
  RTC_CHECK_EQ(y_data.size(), v_data.size());
  RTC_CHECK_EQ(y_data.size(), output.size());

  for (size_t x = 0; x < y_data.size(); ++x) {
    const float val = coeff[0] * y_data[x] + coeff[1] * u_data[x] +
                      coeff[2] * v_data[x] + coeff[3];
    // Clamp result to a byte.
    output[x] =
        static_cast<uint8_t>(std::round(std::max(0.0f, std::min(val, 255.0f))));
  }
}

// Convert a frame to four vectors consisting of [y, u, v, 1].
std::vector<std::vector<uint8_t>> FlattenYuvData(
    const rtc::scoped_refptr<I420BufferInterface>& frame) {
  std::vector<std::vector<uint8_t>> result(
      4, std::vector<uint8_t>(frame->ChromaWidth() * frame->ChromaHeight()));

  // Downscale the Y plane so that all YUV planes are the same size.
  libyuv::ScalePlane(frame->DataY(), frame->StrideY(), frame->width(),
                     frame->height(), result[0].data(), frame->ChromaWidth(),
                     frame->ChromaWidth(), frame->ChromaHeight(),
                     libyuv::kFilterBox);

  libyuv::CopyPlane(frame->DataU(), frame->StrideU(), result[1].data(),
                    frame->ChromaWidth(), frame->ChromaWidth(),
                    frame->ChromaHeight());

  libyuv::CopyPlane(frame->DataV(), frame->StrideV(), result[2].data(),
                    frame->ChromaWidth(), frame->ChromaWidth(),
                    frame->ChromaHeight());

  std::fill(result[3].begin(), result[3].end(), 1u);

  return result;
}

ColorTransformationMatrix VectorToColorMatrix(
    const std::vector<std::vector<double>>& v) {
  ColorTransformationMatrix color_transformation;
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 4; ++j)
      color_transformation[i][j] = v[i][j];
  }
  return color_transformation;
}

}  // namespace

ColorTransformationMatrix CalculateColorTransformationMatrix(
    const rtc::scoped_refptr<I420BufferInterface>& reference_frame,
    const rtc::scoped_refptr<I420BufferInterface>& test_frame) {
  IncrementalLinearLeastSquares incremental_lls;
  incremental_lls.AddObservations(FlattenYuvData(test_frame),
                                  FlattenYuvData(reference_frame));
  return VectorToColorMatrix(incremental_lls.GetBestSolution());
}

ColorTransformationMatrix CalculateColorTransformationMatrix(
    const rtc::scoped_refptr<Video>& reference_video,
    const rtc::scoped_refptr<Video>& test_video) {
  RTC_CHECK_GE(reference_video->number_of_frames(),
               test_video->number_of_frames());

  IncrementalLinearLeastSquares incremental_lls;
  for (size_t i = 0; i < test_video->number_of_frames(); ++i) {
    incremental_lls.AddObservations(
        FlattenYuvData(test_video->GetFrame(i)),
        FlattenYuvData(reference_video->GetFrame(i)));
  }

  return VectorToColorMatrix(incremental_lls.GetBestSolution());
}

rtc::scoped_refptr<Video> AdjustColors(
    const ColorTransformationMatrix& color_transformation,
    const rtc::scoped_refptr<Video>& video) {
  class ColorAdjustedVideo : public Video {
   public:
    ColorAdjustedVideo(const ColorTransformationMatrix& color_transformation,
                       const rtc::scoped_refptr<Video>& video)
        : color_transformation_(color_transformation), video_(video) {}

    int width() const override { return video_->width(); }
    int height() const override { return video_->height(); }
    size_t number_of_frames() const override {
      return video_->number_of_frames();
    }

    rtc::scoped_refptr<I420BufferInterface> GetFrame(
        size_t index) const override {
      return AdjustColors(color_transformation_, video_->GetFrame(index));
    }

   private:
    const ColorTransformationMatrix color_transformation_;
    const rtc::scoped_refptr<Video> video_;
  };

  return rtc::make_ref_counted<ColorAdjustedVideo>(color_transformation, video);
}

rtc::scoped_refptr<I420BufferInterface> AdjustColors(
    const ColorTransformationMatrix& color_matrix,
    const rtc::scoped_refptr<I420BufferInterface>& frame) {
  // Allocate I420 buffer that will hold the color adjusted frame.
  rtc::scoped_refptr<I420Buffer> adjusted_frame =
      I420Buffer::Create(frame->width(), frame->height());

  // Create a downscaled Y plane with the same size as the U/V planes to
  // simplify converting the U/V planes.
  std::vector<uint8_t> downscaled_y_plane(frame->ChromaWidth() *
                                          frame->ChromaHeight());
  libyuv::ScalePlane(frame->DataY(), frame->StrideY(), frame->width(),
                     frame->height(), downscaled_y_plane.data(),
                     frame->ChromaWidth(), frame->ChromaWidth(),
                     frame->ChromaHeight(), libyuv::kFilterBox);

  // Fill in the adjusted data row by row.
  for (int y = 0; y < frame->height(); ++y) {
    const int half_y = y / 2;
    rtc::ArrayView<const uint8_t> y_row(frame->DataY() + frame->StrideY() * y,
                                        frame->width());
    rtc::ArrayView<const uint8_t> u_row(
        frame->DataU() + frame->StrideU() * half_y, frame->ChromaWidth());
    rtc::ArrayView<const uint8_t> v_row(
        frame->DataV() + frame->StrideV() * half_y, frame->ChromaWidth());
    rtc::ArrayView<uint8_t> output_y_row(
        adjusted_frame->MutableDataY() + adjusted_frame->StrideY() * y,
        frame->width());

    CalculateYChannel(y_row, u_row, v_row, color_matrix[0], output_y_row);

    // Chroma channels only exist every second row for I420.
    if (y % 2 == 0) {
      rtc::ArrayView<const uint8_t> downscaled_y_row(
          downscaled_y_plane.data() + frame->ChromaWidth() * half_y,
          frame->ChromaWidth());
      rtc::ArrayView<uint8_t> output_u_row(
          adjusted_frame->MutableDataU() + adjusted_frame->StrideU() * half_y,
          frame->ChromaWidth());
      rtc::ArrayView<uint8_t> output_v_row(
          adjusted_frame->MutableDataV() + adjusted_frame->StrideV() * half_y,
          frame->ChromaWidth());

      CalculateUVChannel(downscaled_y_row, u_row, v_row, color_matrix[1],
                         output_u_row);
      CalculateUVChannel(downscaled_y_row, u_row, v_row, color_matrix[2],
                         output_v_row);
    }
  }

  return adjusted_frame;
}

}  // namespace test
}  // namespace webrtc
