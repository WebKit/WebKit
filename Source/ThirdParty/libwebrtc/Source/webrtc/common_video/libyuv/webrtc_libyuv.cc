/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"

#include <string.h>

#include "webrtc/base/checks.h"
// TODO(nisse): Only needed for the deprecated ConvertToI420.
#include "webrtc/api/video/i420_buffer.h"

// NOTE(ajm): Path provided by gn.
#include "libyuv.h"  // NOLINT

namespace webrtc {

VideoType RawVideoTypeToCommonVideoVideoType(RawVideoType type) {
  switch (type) {
    case kVideoI420:
      return kI420;
    case kVideoIYUV:
      return kIYUV;
    case kVideoRGB24:
      return kRGB24;
    case kVideoARGB:
      return kARGB;
    case kVideoARGB4444:
      return kARGB4444;
    case kVideoRGB565:
      return kRGB565;
    case kVideoARGB1555:
      return kARGB1555;
    case kVideoYUY2:
      return kYUY2;
    case kVideoYV12:
      return kYV12;
    case kVideoUYVY:
      return kUYVY;
    case kVideoNV21:
      return kNV21;
    case kVideoNV12:
      return kNV12;
    case kVideoBGRA:
      return kBGRA;
    case kVideoMJPEG:
      return kMJPG;
    default:
      RTC_NOTREACHED();
  }
  return kUnknown;
}

size_t CalcBufferSize(VideoType type, int width, int height) {
  RTC_DCHECK_GE(width, 0);
  RTC_DCHECK_GE(height, 0);
  size_t buffer_size = 0;
  switch (type) {
    case kI420:
    case kNV12:
    case kNV21:
    case kIYUV:
    case kYV12: {
      int half_width = (width + 1) >> 1;
      int half_height = (height + 1) >> 1;
      buffer_size = width * height + half_width * half_height * 2;
      break;
    }
    case kARGB4444:
    case kRGB565:
    case kARGB1555:
    case kYUY2:
    case kUYVY:
      buffer_size = width * height * 2;
      break;
    case kRGB24:
      buffer_size = width * height * 3;
      break;
    case kBGRA:
    case kARGB:
      buffer_size = width * height * 4;
      break;
    default:
      RTC_NOTREACHED();
      break;
  }
  return buffer_size;
}

static int PrintPlane(const uint8_t* buf,
                      int width,
                      int height,
                      int stride,
                      FILE* file) {
  for (int i = 0; i < height; i++, buf += stride) {
    if (fwrite(buf, 1, width, file) != static_cast<unsigned int>(width))
      return -1;
  }
  return 0;
}

// TODO(nisse): Belongs with the test code?
int PrintVideoFrame(const VideoFrameBuffer& frame, FILE* file) {
  int width = frame.width();
  int height = frame.height();
  int chroma_width = (width + 1) / 2;
  int chroma_height = (height + 1) / 2;

  if (PrintPlane(frame.DataY(), width, height,
                 frame.StrideY(), file) < 0) {
    return -1;
  }
  if (PrintPlane(frame.DataU(),
                 chroma_width, chroma_height,
                 frame.StrideU(), file) < 0) {
    return -1;
  }
  if (PrintPlane(frame.DataV(),
                 chroma_width, chroma_height,
                 frame.StrideV(), file) < 0) {
    return -1;
  }
  return 0;
}

int PrintVideoFrame(const VideoFrame& frame, FILE* file) {
  return PrintVideoFrame(*frame.video_frame_buffer(), file);
}

int ExtractBuffer(const rtc::scoped_refptr<VideoFrameBuffer>& input_frame,
                  size_t size,
                  uint8_t* buffer) {
  RTC_DCHECK(buffer);
  if (!input_frame)
    return -1;
  int width = input_frame->width();
  int height = input_frame->height();
  size_t length = CalcBufferSize(kI420, width, height);
  if (size < length) {
     return -1;
  }

  int chroma_width = (width + 1) / 2;
  int chroma_height = (height + 1) / 2;

  libyuv::I420Copy(input_frame->DataY(),
                   input_frame->StrideY(),
                   input_frame->DataU(),
                   input_frame->StrideU(),
                   input_frame->DataV(),
                   input_frame->StrideV(),
                   buffer, width,
                   buffer + width*height, chroma_width,
                   buffer + width*height + chroma_width*chroma_height,
                   chroma_width,
                   width, height);

  return static_cast<int>(length);
}

int ExtractBuffer(const VideoFrame& input_frame, size_t size, uint8_t* buffer) {
  return ExtractBuffer(input_frame.video_frame_buffer(), size, buffer);
}

int ConvertNV12ToRGB565(const uint8_t* src_frame,
                        uint8_t* dst_frame,
                        int width, int height) {
  int abs_height = (height < 0) ? -height : height;
  const uint8_t* yplane = src_frame;
  const uint8_t* uvInterlaced = src_frame + (width * abs_height);

  return libyuv::NV12ToRGB565(yplane, width,
                              uvInterlaced, (width + 1) >> 1,
                              dst_frame, width,
                              width, height);
}

int ConvertRGB24ToARGB(const uint8_t* src_frame, uint8_t* dst_frame,
                       int width, int height, int dst_stride) {
  if (dst_stride == 0)
    dst_stride = width;
  return libyuv::RGB24ToARGB(src_frame, width,
                             dst_frame, dst_stride,
                             width, height);
}

libyuv::RotationMode ConvertRotationMode(VideoRotation rotation) {
  switch (rotation) {
    case kVideoRotation_0:
      return libyuv::kRotate0;
    case kVideoRotation_90:
      return libyuv::kRotate90;
    case kVideoRotation_180:
      return libyuv::kRotate180;
    case kVideoRotation_270:
      return libyuv::kRotate270;
  }
  RTC_NOTREACHED();
  return libyuv::kRotate0;
}

int ConvertVideoType(VideoType video_type) {
  switch (video_type) {
    case kUnknown:
      return libyuv::FOURCC_ANY;
    case  kI420:
      return libyuv::FOURCC_I420;
    case kIYUV:  // same as KYV12
    case kYV12:
      return libyuv::FOURCC_YV12;
    case kRGB24:
      return libyuv::FOURCC_24BG;
    case kABGR:
      return libyuv::FOURCC_ABGR;
    case kRGB565:
      return libyuv::FOURCC_RGBP;
    case kYUY2:
      return libyuv::FOURCC_YUY2;
    case kUYVY:
      return libyuv::FOURCC_UYVY;
    case kMJPG:
      return libyuv::FOURCC_MJPG;
    case kNV21:
      return libyuv::FOURCC_NV21;
    case kNV12:
      return libyuv::FOURCC_NV12;
    case kARGB:
      return libyuv::FOURCC_ARGB;
    case kBGRA:
      return libyuv::FOURCC_BGRA;
    case kARGB4444:
      return libyuv::FOURCC_R444;
    case kARGB1555:
      return libyuv::FOURCC_RGBO;
  }
  RTC_NOTREACHED();
  return libyuv::FOURCC_ANY;
}

// TODO(nisse): Delete this wrapper, let callers use libyuv directly.
int ConvertToI420(VideoType src_video_type,
                  const uint8_t* src_frame,
                  int crop_x,
                  int crop_y,
                  int src_width,
                  int src_height,
                  size_t sample_size,
                  VideoRotation rotation,
                  I420Buffer* dst_buffer) {
  int dst_width = dst_buffer->width();
  int dst_height = dst_buffer->height();
  // LibYuv expects pre-rotation values for dst.
  // Stride values should correspond to the destination values.
  if (rotation == kVideoRotation_90 || rotation == kVideoRotation_270) {
    std::swap(dst_width, dst_height);
  }
  return libyuv::ConvertToI420(
      src_frame, sample_size,
      dst_buffer->MutableDataY(), dst_buffer->StrideY(),
      dst_buffer->MutableDataU(), dst_buffer->StrideU(),
      dst_buffer->MutableDataV(), dst_buffer->StrideV(),
      crop_x, crop_y,
      src_width, src_height,
      dst_width, dst_height,
      ConvertRotationMode(rotation),
      ConvertVideoType(src_video_type));
}

int ConvertFromI420(const VideoFrame& src_frame,
                    VideoType dst_video_type,
                    int dst_sample_size,
                    uint8_t* dst_frame) {
  return libyuv::ConvertFromI420(
      src_frame.video_frame_buffer()->DataY(),
      src_frame.video_frame_buffer()->StrideY(),
      src_frame.video_frame_buffer()->DataU(),
      src_frame.video_frame_buffer()->StrideU(),
      src_frame.video_frame_buffer()->DataV(),
      src_frame.video_frame_buffer()->StrideV(),
      dst_frame, dst_sample_size,
      src_frame.width(), src_frame.height(),
      ConvertVideoType(dst_video_type));
}

// Compute PSNR for an I420 frame (all planes). Can upscale test frame.
double I420PSNR(const VideoFrameBuffer& ref_buffer,
                const VideoFrameBuffer& test_buffer) {
  RTC_DCHECK_GE(ref_buffer.width(), test_buffer.width());
  RTC_DCHECK_GE(ref_buffer.height(), test_buffer.height());
  if ((ref_buffer.width() != test_buffer.width()) ||
      (ref_buffer.height() != test_buffer.height())) {
    rtc::scoped_refptr<I420Buffer> scaled_buffer =
        I420Buffer::Create(ref_buffer.width(), ref_buffer.height());
    scaled_buffer->ScaleFrom(test_buffer);
    return I420PSNR(ref_buffer, *scaled_buffer);
  }
  double psnr = libyuv::I420Psnr(
      ref_buffer.DataY(), ref_buffer.StrideY(), ref_buffer.DataU(),
      ref_buffer.StrideU(), ref_buffer.DataV(), ref_buffer.StrideV(),
      test_buffer.DataY(), test_buffer.StrideY(), test_buffer.DataU(),
      test_buffer.StrideU(), test_buffer.DataV(), test_buffer.StrideV(),
      test_buffer.width(), test_buffer.height());
  // LibYuv sets the max psnr value to 128, we restrict it here.
  // In case of 0 mse in one frame, 128 can skew the results significantly.
  return (psnr > kPerfectPSNR) ? kPerfectPSNR : psnr;
}

// Compute PSNR for an I420 frame (all planes)
double I420PSNR(const VideoFrame* ref_frame, const VideoFrame* test_frame) {
  if (!ref_frame || !test_frame)
    return -1;
  return I420PSNR(*ref_frame->video_frame_buffer(),
                  *test_frame->video_frame_buffer());
}

// Compute SSIM for an I420 frame (all planes). Can upscale test_buffer.
double I420SSIM(const VideoFrameBuffer& ref_buffer,
                const VideoFrameBuffer& test_buffer) {
  RTC_DCHECK_GE(ref_buffer.width(), test_buffer.width());
  RTC_DCHECK_GE(ref_buffer.height(), test_buffer.height());
  if ((ref_buffer.width() != test_buffer.width()) ||
      (ref_buffer.height() != test_buffer.height())) {
    rtc::scoped_refptr<I420Buffer> scaled_buffer =
        I420Buffer::Create(ref_buffer.width(), ref_buffer.height());
    scaled_buffer->ScaleFrom(test_buffer);
    return I420SSIM(ref_buffer, *scaled_buffer);
  }
  return libyuv::I420Ssim(
      ref_buffer.DataY(), ref_buffer.StrideY(), ref_buffer.DataU(),
      ref_buffer.StrideU(), ref_buffer.DataV(), ref_buffer.StrideV(),
      test_buffer.DataY(), test_buffer.StrideY(), test_buffer.DataU(),
      test_buffer.StrideU(), test_buffer.DataV(), test_buffer.StrideV(),
      test_buffer.width(), test_buffer.height());
}
double I420SSIM(const VideoFrame* ref_frame, const VideoFrame* test_frame) {
  if (!ref_frame || !test_frame)
    return -1;
  return I420SSIM(*ref_frame->video_frame_buffer(),
                  *test_frame->video_frame_buffer());
}

void NV12Scale(std::vector<uint8_t>* tmp_buffer,
               const uint8_t* src_y, int src_stride_y,
               const uint8_t* src_uv, int src_stride_uv,
               int src_width, int src_height,
               uint8_t* dst_y, int dst_stride_y,
               uint8_t* dst_uv, int dst_stride_uv,
               int dst_width, int dst_height) {
  const int src_chroma_width = (src_width + 1) / 2;
  const int src_chroma_height = (src_height + 1) / 2;

  if (src_width == dst_width && src_height == dst_height) {
    // No scaling.
    tmp_buffer->clear();
    tmp_buffer->shrink_to_fit();
    libyuv::CopyPlane(src_y, src_stride_y, dst_y, dst_stride_y, src_width,
                      src_height);
    libyuv::CopyPlane(src_uv, src_stride_uv, dst_uv, dst_stride_uv,
                      src_chroma_width * 2, src_chroma_height);
    return;
  }

  // Scaling.
  // Allocate temporary memory for spitting UV planes and scaling them.
  const int dst_chroma_width = (dst_width + 1) / 2;
  const int dst_chroma_height = (dst_height + 1) / 2;
  tmp_buffer->resize(src_chroma_width * src_chroma_height * 2 +
                     dst_chroma_width * dst_chroma_height * 2);
  tmp_buffer->shrink_to_fit();

  uint8_t* const src_u = tmp_buffer->data();
  uint8_t* const src_v = src_u + src_chroma_width * src_chroma_height;
  uint8_t* const dst_u = src_v + src_chroma_width * src_chroma_height;
  uint8_t* const dst_v = dst_u + dst_chroma_width * dst_chroma_height;

  // Split source UV plane into separate U and V plane using the temporary data.
  libyuv::SplitUVPlane(src_uv, src_stride_uv,
                       src_u, src_chroma_width,
                       src_v, src_chroma_width,
                       src_chroma_width, src_chroma_height);

  // Scale the planes.
  libyuv::I420Scale(src_y, src_stride_y,
                    src_u, src_chroma_width,
                    src_v, src_chroma_width,
                    src_width, src_height,
                    dst_y, dst_stride_y,
                    dst_u, dst_chroma_width,
                    dst_v, dst_chroma_width,
                    dst_width, dst_height,
                    libyuv::kFilterBox);

  // Merge the UV planes into the destination.
  libyuv::MergeUVPlane(dst_u, dst_chroma_width,
                       dst_v, dst_chroma_width,
                       dst_uv, dst_stride_uv,
                       dst_chroma_width, dst_chroma_height);
}

void NV12ToI420Scaler::NV12ToI420Scale(
    const uint8_t* src_y, int src_stride_y,
    const uint8_t* src_uv, int src_stride_uv,
    int src_width, int src_height,
    uint8_t* dst_y, int dst_stride_y,
    uint8_t* dst_u, int dst_stride_u,
    uint8_t* dst_v, int dst_stride_v,
    int dst_width, int dst_height) {
  if (src_width == dst_width && src_height == dst_height) {
    // No scaling.
    tmp_uv_planes_.clear();
    tmp_uv_planes_.shrink_to_fit();
    libyuv::NV12ToI420(
        src_y, src_stride_y,
        src_uv, src_stride_uv,
        dst_y, dst_stride_y,
        dst_u, dst_stride_u,
        dst_v, dst_stride_v,
        src_width, src_height);
    return;
  }

  // Scaling.
  // Allocate temporary memory for spitting UV planes.
  const int src_uv_width = (src_width + 1) / 2;
  const int src_uv_height = (src_height + 1) / 2;
  tmp_uv_planes_.resize(src_uv_width * src_uv_height * 2);
  tmp_uv_planes_.shrink_to_fit();

  // Split source UV plane into separate U and V plane using the temporary data.
  uint8_t* const src_u = tmp_uv_planes_.data();
  uint8_t* const src_v = tmp_uv_planes_.data() + src_uv_width * src_uv_height;
  libyuv::SplitUVPlane(src_uv, src_stride_uv,
                       src_u, src_uv_width,
                       src_v, src_uv_width,
                       src_uv_width, src_uv_height);

  // Scale the planes into the destination.
  libyuv::I420Scale(src_y, src_stride_y,
                    src_u, src_uv_width,
                    src_v, src_uv_width,
                    src_width, src_height,
                    dst_y, dst_stride_y,
                    dst_u, dst_stride_u,
                    dst_v, dst_stride_v,
                    dst_width, dst_height,
                    libyuv::kFilterBox);
}

}  // namespace webrtc
