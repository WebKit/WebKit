/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/frame_analyzer/video_geometry_aligner.h"

#include <map>

#include "api/video/i420_buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_tools/frame_analyzer/video_quality_analysis.h"
#include "third_party/libyuv/include/libyuv/scale.h"

namespace webrtc {
namespace test {

namespace {

bool IsValidRegion(const CropRegion& region,
                   const rtc::scoped_refptr<I420BufferInterface>& frame) {
  return region.left >= 0 && region.right >= 0 && region.top >= 0 &&
         region.bottom >= 0 && region.left + region.right < frame->width() &&
         region.top + region.bottom < frame->height();
}

}  // namespace

rtc::scoped_refptr<I420BufferInterface> CropAndZoom(
    const CropRegion& crop_region,
    const rtc::scoped_refptr<I420BufferInterface>& frame) {
  RTC_CHECK(IsValidRegion(crop_region, frame));

  const int uv_crop_left = crop_region.left / 2;
  const int uv_crop_top = crop_region.top / 2;

  const int cropped_width =
      frame->width() - crop_region.left - crop_region.right;
  const int cropped_height =
      frame->height() - crop_region.top - crop_region.bottom;

  // Crop by only adjusting pointers.
  const uint8_t* y_plane =
      frame->DataY() + frame->StrideY() * crop_region.top + crop_region.left;
  const uint8_t* u_plane =
      frame->DataU() + frame->StrideU() * uv_crop_top + uv_crop_left;
  const uint8_t* v_plane =
      frame->DataV() + frame->StrideV() * uv_crop_top + uv_crop_left;

  // Stretch the cropped frame to the original size using libyuv.
  rtc::scoped_refptr<I420Buffer> adjusted_frame =
      I420Buffer::Create(frame->width(), frame->height());
  libyuv::I420Scale(y_plane, frame->StrideY(), u_plane, frame->StrideU(),
                    v_plane, frame->StrideV(), cropped_width, cropped_height,
                    adjusted_frame->MutableDataY(), adjusted_frame->StrideY(),
                    adjusted_frame->MutableDataU(), adjusted_frame->StrideU(),
                    adjusted_frame->MutableDataV(), adjusted_frame->StrideV(),
                    frame->width(), frame->height(), libyuv::kFilterBilinear);

  return adjusted_frame;
}

CropRegion CalculateCropRegion(
    const rtc::scoped_refptr<I420BufferInterface>& reference_frame,
    const rtc::scoped_refptr<I420BufferInterface>& test_frame) {
  RTC_CHECK_EQ(reference_frame->width(), test_frame->width());
  RTC_CHECK_EQ(reference_frame->height(), test_frame->height());

  CropRegion best_region;
  double best_ssim = Ssim(reference_frame, test_frame);

  typedef int CropRegion::*CropParameter;
  CropParameter crop_parameters[4] = {&CropRegion::left, &CropRegion::top,
                                      &CropRegion::right, &CropRegion::bottom};

  while (true) {
    // Find the parameter in which direction SSIM improves the most.
    CropParameter best_parameter = nullptr;
    const CropRegion prev_best_region = best_region;

    for (CropParameter crop_parameter : crop_parameters) {
      CropRegion test_region = prev_best_region;
      ++(test_region.*crop_parameter);

      if (!IsValidRegion(test_region, reference_frame))
        continue;

      const double ssim =
          Ssim(CropAndZoom(test_region, reference_frame), test_frame);

      if (ssim > best_ssim) {
        best_ssim = ssim;
        best_parameter = crop_parameter;
        best_region = test_region;
      }
    }

    // No improvement among any direction, stop iteration.
    if (best_parameter == nullptr)
      break;

    // Iterate in the best direction as long as it improves SSIM.
    for (CropRegion test_region = best_region;
         IsValidRegion(test_region, reference_frame);
         ++(test_region.*best_parameter)) {
      const double ssim =
          Ssim(CropAndZoom(test_region, reference_frame), test_frame);
      if (ssim <= best_ssim)
        break;

      best_ssim = ssim;
      best_region = test_region;
    }
  }

  return best_region;
}

rtc::scoped_refptr<I420BufferInterface> AdjustCropping(
    const rtc::scoped_refptr<I420BufferInterface>& reference_frame,
    const rtc::scoped_refptr<I420BufferInterface>& test_frame) {
  return CropAndZoom(CalculateCropRegion(reference_frame, test_frame),
                     reference_frame);
}

rtc::scoped_refptr<Video> AdjustCropping(
    const rtc::scoped_refptr<Video>& reference_video,
    const rtc::scoped_refptr<Video>& test_video) {
  class CroppedVideo : public rtc::RefCountedObject<Video> {
   public:
    CroppedVideo(const rtc::scoped_refptr<Video>& reference_video,
                 const rtc::scoped_refptr<Video>& test_video)
        : reference_video_(reference_video), test_video_(test_video) {
      RTC_CHECK_EQ(reference_video->number_of_frames(),
                   test_video->number_of_frames());
      RTC_CHECK_EQ(reference_video->width(), test_video->width());
      RTC_CHECK_EQ(reference_video->height(), test_video->height());
    }

    int width() const override { return test_video_->width(); }
    int height() const override { return test_video_->height(); }
    size_t number_of_frames() const override {
      return test_video_->number_of_frames();
    }

    rtc::scoped_refptr<I420BufferInterface> GetFrame(
        size_t index) const override {
      const rtc::scoped_refptr<I420BufferInterface> reference_frame =
          reference_video_->GetFrame(index);

      // Only calculate cropping region once per frame since it's expensive.
      if (!crop_regions_.count(index)) {
        crop_regions_[index] =
            CalculateCropRegion(reference_frame, test_video_->GetFrame(index));
      }

      return CropAndZoom(crop_regions_[index], reference_frame);
    }

   private:
    const rtc::scoped_refptr<Video> reference_video_;
    const rtc::scoped_refptr<Video> test_video_;
    // Mutable since this is a cache that affects performance and not logical
    // behavior.
    mutable std::map<size_t, CropRegion> crop_regions_;
  };

  return new CroppedVideo(reference_video, test_video);
}

}  // namespace test
}  // namespace webrtc
