/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/libyuv/include/webrtc_libyuv.h"

#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include "api/scoped_refptr.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_buffer.h"
#include "common_video/include/video_frame_buffer.h"
#include "rtc_base/checks.h"
#include "third_party/libyuv/include/libyuv/compare.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/libyuv/include/libyuv/convert_from.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"
#include "third_party/libyuv/include/libyuv/rotate.h"
#include "third_party/libyuv/include/libyuv/scale.h"
#include "third_party/libyuv/include/libyuv/video_common.h"

namespace webrtc {

size_t CalcBufferSize(VideoType type, int width, int height) {
  RTC_DCHECK_GE(width, 0);
  RTC_DCHECK_GE(height, 0);
  switch (type) {
    case VideoType::kI420:
    case VideoType::kIYUV:
    case VideoType::kYV12:
    case VideoType::kNV12: {
      int half_width = (width + 1) >> 1;
      int half_height = (height + 1) >> 1;
      return width * height + half_width * half_height * 2;
    }
    case VideoType::kRGB565:
    case VideoType::kYUY2:
    case VideoType::kUYVY:
      return width * height * 2;
    case VideoType::kRGB24:
    case VideoType::kBGR24:
      return width * height * 3;
    case VideoType::kBGRA:
    case VideoType::kARGB:
    case VideoType::kABGR:
      return width * height * 4;
    case VideoType::kMJPEG:
    case VideoType::kUnknown:
      break;
  }
  RTC_DCHECK_NOTREACHED() << "Unexpected pixel format " << type;
  return 0;
}

int ExtractBuffer(const scoped_refptr<I420BufferInterface>& input_frame,
                  size_t size,
                  uint8_t* buffer) {
  RTC_DCHECK(buffer);
  if (!input_frame)
    return -1;
  int width = input_frame->width();
  int height = input_frame->height();
  size_t length = CalcBufferSize(VideoType::kI420, width, height);
  if (size < length) {
    return -1;
  }

  int chroma_width = input_frame->ChromaWidth();
  int chroma_height = input_frame->ChromaHeight();

  libyuv::I420Copy(input_frame->DataY(), input_frame->StrideY(),
                   input_frame->DataU(), input_frame->StrideU(),
                   input_frame->DataV(), input_frame->StrideV(), buffer, width,
                   buffer + width * height, chroma_width,
                   buffer + width * height + chroma_width * chroma_height,
                   chroma_width, width, height);

  return static_cast<int>(length);
}

int ExtractBuffer(const VideoFrame& input_frame, size_t size, uint8_t* buffer) {
  return ExtractBuffer(input_frame.video_frame_buffer()->ToI420(), size,
                       buffer);
}

int ConvertVideoType(VideoType video_type) {
  switch (video_type) {
    case VideoType::kUnknown:
      return libyuv::FOURCC_ANY;
    case VideoType::kI420:
      return libyuv::FOURCC_I420;
    case VideoType::kIYUV:  // same as VideoType::kYV12
    case VideoType::kYV12:
      return libyuv::FOURCC_YV12;
    case VideoType::kRGB24:
      return libyuv::FOURCC_24BG;
    case VideoType::kBGR24:
      return libyuv::FOURCC_RAW;
    case VideoType::kABGR:
      return libyuv::FOURCC_ABGR;
    case VideoType::kRGB565:
      return libyuv::FOURCC_RGBP;
    case VideoType::kYUY2:
      return libyuv::FOURCC_YUY2;
    case VideoType::kUYVY:
      return libyuv::FOURCC_UYVY;
    case VideoType::kMJPEG:
      return libyuv::FOURCC_MJPG;
    case VideoType::kARGB:
      return libyuv::FOURCC_ARGB;
    case VideoType::kBGRA:
      return libyuv::FOURCC_BGRA;
    case VideoType::kNV12:
      return libyuv::FOURCC_NV12;
  }
  RTC_DCHECK_NOTREACHED() << "Unexpected pixel format " << video_type;
  return libyuv::FOURCC_ANY;
}

int ConvertFromI420(const VideoFrame& src_frame,
                    VideoType dst_video_type,
                    int dst_sample_size,
                    uint8_t* dst_frame) {
  scoped_refptr<I420BufferInterface> i420_buffer =
      src_frame.video_frame_buffer()->ToI420();
  return libyuv::ConvertFromI420(
      i420_buffer->DataY(), i420_buffer->StrideY(), i420_buffer->DataU(),
      i420_buffer->StrideU(), i420_buffer->DataV(), i420_buffer->StrideV(),
      dst_frame, dst_sample_size, src_frame.width(), src_frame.height(),
      ConvertVideoType(dst_video_type));
}

int ConvertToI420(const uint8_t* sample,
                  size_t sample_size,
                  uint8_t* dst_y,
                  int dst_stride_y,
                  uint8_t* dst_u,
                  int dst_stride_u,
                  uint8_t* dst_v,
                  int dst_stride_v,
                  int src_width,
                  int src_height,
                  int src_stride,
                  int dst_width,
                  int dst_height,
                  uint32_t rotation,
                  uint32_t fourcc) {
  if (src_height == INT_MIN || dst_height == INT_MIN) {
    return -1;
  }

  const int abs_src_height = (src_height < 0) ? -src_height : src_height;
  const int abs_dst_height = (dst_height < 0) ? -dst_height : dst_height;

  if (!dst_y || !dst_u || !dst_v || !sample || src_width <= 0 ||
      src_width > INT_MAX / 4 || dst_width <= 0 || src_height == 0 ||
      dst_height == 0 || dst_width > src_width ||
      abs_dst_height > abs_src_height) {
    return -1;
  }

  uint32_t format = libyuv::CanonicalFourCC(fourcc);

  // Calculate the unpadded stride if no explicit value is set.
  if (src_stride == 0) {
    switch (format) {
      case libyuv::FOURCC_YUY2:
      case libyuv::FOURCC_UYVY:
      case libyuv::FOURCC_RGBP:
      case libyuv::FOURCC_RGBO:
      case libyuv::FOURCC_R444:
        src_stride = src_width * 2;
        break;
      case libyuv::FOURCC_24BG:
      case libyuv::FOURCC_RAW:
        src_stride = src_width * 3;
        break;
      case libyuv::FOURCC_ARGB:
      case libyuv::FOURCC_BGRA:
      case libyuv::FOURCC_ABGR:
      case libyuv::FOURCC_RGBA:
        src_stride = src_width * 4;
        break;
      case libyuv::FOURCC_I400:
      case libyuv::FOURCC_NV12:
      case libyuv::FOURCC_NV21:
      case libyuv::FOURCC_I420:
      case libyuv::FOURCC_YV12:
      case libyuv::FOURCC_I422:
      case libyuv::FOURCC_YV16:
        // Follow the V4L2 definition for strides of subsampled formats:
        // > To avoid ambiguities drivers must return a bytesperline value
        // > rounded up to a multiple of the scale factor.
        // https://www.kernel.org/doc/html/v7.1/userspace-api/media/v4l/pixfmt-v4l2.html
        src_stride = (src_width + 1) & ~1;
        break;
      case libyuv::FOURCC_I444:
      case libyuv::FOURCC_YV24:
        src_stride = src_width;
        break;
      case libyuv::FOURCC_MJPG:
        break;
      default:
        return -1;
    }
  } else {
    switch (format) {
      case libyuv::FOURCC_YUY2:
      case libyuv::FOURCC_UYVY:
      case libyuv::FOURCC_I400:
      case libyuv::FOURCC_NV12:
      case libyuv::FOURCC_NV21:
      case libyuv::FOURCC_I420:
      case libyuv::FOURCC_YV12:
      case libyuv::FOURCC_I422:
      case libyuv::FOURCC_YV16:
        // Follow the V4L2 definition for strides of subsampled formats:
        // > To avoid ambiguities drivers must return a bytesperline value
        // > rounded up to a multiple of the scale factor.
        // https://www.kernel.org/doc/html/v7.1/userspace-api/media/v4l/pixfmt-v4l2.html
        if ((src_stride % 2) != 0)
          return -1;
        break;
      default:
        break;
    }
  }

  int r = 0;
  const bool need_buf =
      (rotation && format != libyuv::FOURCC_I420 &&
       format != libyuv::FOURCC_NV12 && format != libyuv::FOURCC_NV21 &&
       format != libyuv::FOURCC_YV12) ||
      dst_y == sample;
  const int inv_dst_height =
      (src_height < 0) ? -abs_dst_height : abs_dst_height;
  uint8_t* rotate_buffer = NULL;
  uint8_t* tmp_y;
  uint8_t* tmp_u;
  uint8_t* tmp_v;
  int tmp_y_stride;
  int tmp_u_stride;
  int tmp_v_stride;

  // One pass rotation is available for some formats. For the rest, convert
  // to I420 (with optional vertical flipping) into a temporary I420 buffer,
  // and then rotate the I420 to the final destination buffer.
  // For in-place conversion, if destination dst_y is same as source sample,
  // also enable temporary buffer.
  if (need_buf) {
    size_t y_size = (size_t)dst_width * abs_dst_height;
    size_t uv_size = (size_t)((dst_width + 1) / 2) * ((abs_dst_height + 1) / 2);
    if (uv_size > SIZE_MAX / 2 || y_size > SIZE_MAX - uv_size * 2) {
      return -1;  // Invalid size.
    }
    const size_t rotate_buffer_size = y_size + uv_size * 2;
    rotate_buffer = new uint8_t[rotate_buffer_size];
    if (!rotate_buffer) {
      return 1;  // Out of memory runtime error.
    }
    tmp_y = dst_y;
    tmp_u = dst_u;
    tmp_v = dst_v;
    tmp_y_stride = dst_stride_y;
    tmp_u_stride = dst_stride_u;
    tmp_v_stride = dst_stride_v;
    dst_y = rotate_buffer;
    dst_u = dst_y + y_size;
    dst_v = dst_u + uv_size;
    dst_stride_y = dst_width;
    dst_stride_u = dst_stride_v = ((dst_width + 1) / 2);
  }

  switch (format) {
    // Single plane formats
    case libyuv::FOURCC_YUY2: {
      r = libyuv::YUY2ToI420(sample, src_stride, dst_y, dst_stride_y, dst_u,
                             dst_stride_u, dst_v, dst_stride_v, dst_width,
                             inv_dst_height);
      break;
    }
    case libyuv::FOURCC_UYVY: {
      r = libyuv::UYVYToI420(sample, src_stride, dst_y, dst_stride_y, dst_u,
                             dst_stride_u, dst_v, dst_stride_v, dst_width,
                             inv_dst_height);
      break;
    }
    case libyuv::FOURCC_RGBP:
      r = libyuv::RGB565ToI420(sample, src_stride, dst_y, dst_stride_y, dst_u,
                               dst_stride_u, dst_v, dst_stride_v, dst_width,
                               inv_dst_height);
      break;
    case libyuv::FOURCC_RGBO:
      r = libyuv::ARGB1555ToI420(sample, src_stride, dst_y, dst_stride_y, dst_u,
                                 dst_stride_u, dst_v, dst_stride_v, dst_width,
                                 inv_dst_height);
      break;
    case libyuv::FOURCC_R444:
      r = libyuv::ARGB4444ToI420(sample, src_stride, dst_y, dst_stride_y, dst_u,
                                 dst_stride_u, dst_v, dst_stride_v, dst_width,
                                 inv_dst_height);
      break;
    case libyuv::FOURCC_24BG:
      r = libyuv::RGB24ToI420(sample, src_stride, dst_y, dst_stride_y, dst_u,
                              dst_stride_u, dst_v, dst_stride_v, dst_width,
                              inv_dst_height);
      break;
    case libyuv::FOURCC_RAW:
      r = libyuv::RAWToI420(sample, src_stride, dst_y, dst_stride_y, dst_u,
                            dst_stride_u, dst_v, dst_stride_v, dst_width,
                            inv_dst_height);
      break;
    case libyuv::FOURCC_ARGB:
      r = libyuv::ARGBToI420(sample, src_stride, dst_y, dst_stride_y, dst_u,
                             dst_stride_u, dst_v, dst_stride_v, dst_width,
                             inv_dst_height);
      break;
    case libyuv::FOURCC_BGRA:
      r = libyuv::BGRAToI420(sample, src_stride, dst_y, dst_stride_y, dst_u,
                             dst_stride_u, dst_v, dst_stride_v, dst_width,
                             inv_dst_height);
      break;
    case libyuv::FOURCC_ABGR:
      r = libyuv::ABGRToI420(sample, src_stride, dst_y, dst_stride_y, dst_u,
                             dst_stride_u, dst_v, dst_stride_v, dst_width,
                             inv_dst_height);
      break;
    case libyuv::FOURCC_RGBA:
      r = libyuv::RGBAToI420(sample, src_stride, dst_y, dst_stride_y, dst_u,
                             dst_stride_u, dst_v, dst_stride_v, dst_width,
                             inv_dst_height);
      break;
    case libyuv::FOURCC_I400:
      r = libyuv::I400ToI420(sample, src_stride, dst_y, dst_stride_y, dst_u,
                             dst_stride_u, dst_v, dst_stride_v, dst_width,
                             inv_dst_height);
      break;
    // Biplanar formats
    case libyuv::FOURCC_NV12: {
      const uint8_t* src_y = sample;
      const uint8_t* src_uv = src_y + ((ptrdiff_t)src_stride * abs_src_height);
      r = libyuv::NV12ToI420Rotate(src_y, src_stride, src_uv, src_stride, dst_y,
                                   dst_stride_y, dst_u, dst_stride_u, dst_v,
                                   dst_stride_v, dst_width, inv_dst_height,
                                   (libyuv::RotationMode)rotation);
      break;
    }
    case libyuv::FOURCC_NV21: {
      const uint8_t* src_y = sample;
      const uint8_t* src_uv = src_y + ((ptrdiff_t)src_stride * abs_src_height);
      // Call NV12 but with dst_u and dst_v parameters swapped.
      r = libyuv::NV12ToI420Rotate(src_y, src_stride, src_uv, src_stride, dst_y,
                                   dst_stride_y, dst_v, dst_stride_v, dst_u,
                                   dst_stride_u, dst_width, inv_dst_height,
                                   (libyuv::RotationMode)rotation);
      break;
    }
    // Triplanar formats
    case libyuv::FOURCC_I420:
    case libyuv::FOURCC_YV12: {
      const uint8_t* src_y = sample;
      const uint8_t* src_u;
      const uint8_t* src_v;
      // Follow the V4L2 definition:
      // > When the image format is planar the bytesperline value applies to the
      // > first plane and is divided by the same factor as the width field for
      // > the other planes.
      // https://www.kernel.org/doc/html/v7.1/userspace-api/media/v4l/pixfmt-v4l2.html
      int halfstride = src_stride / 2;
      int halfheight = (abs_src_height + 1) / 2;
      if (format == libyuv::FOURCC_YV12) {
        src_v = src_y + (ptrdiff_t)src_stride * abs_src_height;
        src_u = src_v + halfstride * (ptrdiff_t)halfheight;
      } else {
        src_u = src_y + (ptrdiff_t)src_stride * abs_src_height;
        src_v = src_u + halfstride * (ptrdiff_t)halfheight;
      }
      r = libyuv::I420Rotate(src_y, src_stride, src_u, halfstride, src_v,
                             halfstride, dst_y, dst_stride_y, dst_u,
                             dst_stride_u, dst_v, dst_stride_v, dst_width,
                             inv_dst_height, (libyuv::RotationMode)rotation);
      break;
    }
    case libyuv::FOURCC_I422:
    case libyuv::FOURCC_YV16: {
      const uint8_t* src_y = sample;
      const uint8_t* src_u;
      const uint8_t* src_v;
      // Follow the V4L2 definition:
      // > When the image format is planar the bytesperline value applies to the
      // > first plane and is divided by the same factor as the width field for
      // > the other planes.
      // https://www.kernel.org/doc/html/v7.1/userspace-api/media/v4l/pixfmt-v4l2.html
      int halfstride = src_stride / 2;
      if (format == libyuv::FOURCC_YV16) {
        src_v = src_y + (ptrdiff_t)src_stride * abs_src_height;
        src_u = src_v + halfstride * (ptrdiff_t)abs_src_height;
      } else {
        src_u = src_y + (ptrdiff_t)src_stride * abs_src_height;
        src_v = src_u + halfstride * (ptrdiff_t)abs_src_height;
      }
      r = libyuv::I422ToI420(src_y, src_stride, src_u, halfstride, src_v,
                             halfstride, dst_y, dst_stride_y, dst_u,
                             dst_stride_u, dst_v, dst_stride_v, dst_width,
                             inv_dst_height);
      break;
    }
    case libyuv::FOURCC_I444:
    case libyuv::FOURCC_YV24: {
      const uint8_t* src_y = sample;
      const uint8_t* src_u;
      const uint8_t* src_v;
      if (format == libyuv::FOURCC_YV24) {
        src_v = src_y + src_stride * (ptrdiff_t)abs_src_height;
        src_u = src_v + src_stride * (ptrdiff_t)abs_src_height;
      } else {
        src_u = src_y + src_stride * (ptrdiff_t)abs_src_height;
        src_v = src_u + src_stride * (ptrdiff_t)abs_src_height;
      }
      r = libyuv::I444ToI420(src_y, src_stride, src_u, src_stride, src_v,
                             src_stride, dst_y, dst_stride_y, dst_u,
                             dst_stride_u, dst_v, dst_stride_v, dst_width,
                             inv_dst_height);
      break;
    }
#ifdef HAVE_LIBYUV_JPEG
    case libyuv::FOURCC_MJPG:
      r = libyuv::MJPGToI420(sample, sample_size, dst_y, dst_stride_y, dst_u,
                             dst_stride_u, dst_v, dst_stride_v, src_width,
                             abs_src_height, dst_width, inv_dst_height);
      break;
#endif
    default:
      r = -1;  // unknown fourcc - return failure code.
  }

  if (need_buf) {
    if (!r) {
      r = libyuv::I420Rotate(dst_y, dst_stride_y, dst_u, dst_stride_u, dst_v,
                             dst_stride_v, tmp_y, tmp_y_stride, tmp_u,
                             tmp_u_stride, tmp_v, tmp_v_stride, dst_width,
                             abs_dst_height, (libyuv::RotationMode)rotation);
    }
    free(rotate_buffer);
  }

  return r;
}

scoped_refptr<I420ABufferInterface> ScaleI420ABuffer(
    const I420ABufferInterface& buffer,
    int target_width,
    int target_height) {
  scoped_refptr<I420Buffer> yuv_buffer =
      I420Buffer::Create(target_width, target_height);
  yuv_buffer->ScaleFrom(buffer);
  scoped_refptr<I420Buffer> axx_buffer =
      I420Buffer::Create(target_width, target_height);
  libyuv::ScalePlane(buffer.DataA(), buffer.StrideA(), buffer.width(),
                     buffer.height(), axx_buffer->MutableDataY(),
                     axx_buffer->StrideY(), target_width, target_height,
                     libyuv::kFilterBox);
  scoped_refptr<I420ABufferInterface> merged_buffer = WrapI420ABuffer(
      yuv_buffer->width(), yuv_buffer->height(), yuv_buffer->DataY(),
      yuv_buffer->StrideY(), yuv_buffer->DataU(), yuv_buffer->StrideU(),
      yuv_buffer->DataV(), yuv_buffer->StrideV(), axx_buffer->DataY(),
      axx_buffer->StrideY(),
      // To keep references alive.
      [yuv_buffer, axx_buffer] {});
  return merged_buffer;
}

scoped_refptr<I420BufferInterface> ScaleVideoFrameBuffer(
    const I420BufferInterface& source,
    int dst_width,
    int dst_height) {
  scoped_refptr<I420Buffer> scaled_buffer =
      I420Buffer::Create(dst_width, dst_height);
  scaled_buffer->ScaleFrom(source);
  return scaled_buffer;
}

double I420SSE(const I420BufferInterface& ref_buffer,
               const I420BufferInterface& test_buffer) {
  RTC_DCHECK_EQ(ref_buffer.width(), test_buffer.width());
  RTC_DCHECK_EQ(ref_buffer.height(), test_buffer.height());
  const uint64_t width = test_buffer.width();
  const uint64_t height = test_buffer.height();
  const uint64_t sse_y = libyuv::ComputeSumSquareErrorPlane(
      ref_buffer.DataY(), ref_buffer.StrideY(), test_buffer.DataY(),
      test_buffer.StrideY(), width, height);
  const int width_uv = (width + 1) >> 1;
  const int height_uv = (height + 1) >> 1;
  const uint64_t sse_u = libyuv::ComputeSumSquareErrorPlane(
      ref_buffer.DataU(), ref_buffer.StrideU(), test_buffer.DataU(),
      test_buffer.StrideU(), width_uv, height_uv);
  const uint64_t sse_v = libyuv::ComputeSumSquareErrorPlane(
      ref_buffer.DataV(), ref_buffer.StrideV(), test_buffer.DataV(),
      test_buffer.StrideV(), width_uv, height_uv);
  const double samples = width * height + 2 * (width_uv * height_uv);
  const double sse = sse_y + sse_u + sse_v;
  return sse / (samples * 255.0 * 255.0);
}

// Compute PSNR for an I420A frame (all planes). Can upscale test frame.
double I420APSNR(const I420ABufferInterface& ref_buffer,
                 const I420ABufferInterface& test_buffer) {
  RTC_DCHECK_GE(ref_buffer.width(), test_buffer.width());
  RTC_DCHECK_GE(ref_buffer.height(), test_buffer.height());
  if ((ref_buffer.width() != test_buffer.width()) ||
      (ref_buffer.height() != test_buffer.height())) {
    scoped_refptr<I420ABufferInterface> scaled_buffer =
        ScaleI420ABuffer(test_buffer, ref_buffer.width(), ref_buffer.height());
    return I420APSNR(ref_buffer, *scaled_buffer);
  }
  const int width = test_buffer.width();
  const int height = test_buffer.height();
  const uint64_t sse_y = libyuv::ComputeSumSquareErrorPlane(
      ref_buffer.DataY(), ref_buffer.StrideY(), test_buffer.DataY(),
      test_buffer.StrideY(), width, height);
  const int width_uv = (width + 1) >> 1;
  const int height_uv = (height + 1) >> 1;
  const uint64_t sse_u = libyuv::ComputeSumSquareErrorPlane(
      ref_buffer.DataU(), ref_buffer.StrideU(), test_buffer.DataU(),
      test_buffer.StrideU(), width_uv, height_uv);
  const uint64_t sse_v = libyuv::ComputeSumSquareErrorPlane(
      ref_buffer.DataV(), ref_buffer.StrideV(), test_buffer.DataV(),
      test_buffer.StrideV(), width_uv, height_uv);
  const uint64_t sse_a = libyuv::ComputeSumSquareErrorPlane(
      ref_buffer.DataA(), ref_buffer.StrideA(), test_buffer.DataA(),
      test_buffer.StrideA(), width, height);
  const uint64_t samples = 2 * (uint64_t)width * (uint64_t)height +
                           2 * ((uint64_t)width_uv * (uint64_t)height_uv);
  const uint64_t sse = sse_y + sse_u + sse_v + sse_a;
  const double psnr = libyuv::SumSquareErrorToPsnr(sse, samples);
  return (psnr > kPerfectPSNR) ? kPerfectPSNR : psnr;
}

// Compute PSNR for an I420A frame (all planes)
double I420APSNR(const VideoFrame* ref_frame, const VideoFrame* test_frame) {
  if (!ref_frame || !test_frame)
    return -1;
  RTC_DCHECK(ref_frame->video_frame_buffer()->type() ==
             VideoFrameBuffer::Type::kI420A);
  RTC_DCHECK(test_frame->video_frame_buffer()->type() ==
             VideoFrameBuffer::Type::kI420A);
  return I420APSNR(*ref_frame->video_frame_buffer()->GetI420A(),
                   *test_frame->video_frame_buffer()->GetI420A());
}

// Compute PSNR for an I420 frame (all planes). Can upscale test frame.
double I420PSNR(const I420BufferInterface& ref_buffer,
                const I420BufferInterface& test_buffer) {
  RTC_DCHECK_GE(ref_buffer.width(), test_buffer.width());
  RTC_DCHECK_GE(ref_buffer.height(), test_buffer.height());
  if ((ref_buffer.width() != test_buffer.width()) ||
      (ref_buffer.height() != test_buffer.height())) {
    scoped_refptr<I420Buffer> scaled_buffer =
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
  return I420PSNR(*ref_frame->video_frame_buffer()->ToI420(),
                  *test_frame->video_frame_buffer()->ToI420());
}

double I420WeightedPSNR(const I420BufferInterface& ref_buffer,
                        const I420BufferInterface& test_buffer) {
  RTC_DCHECK_GE(ref_buffer.width(), test_buffer.width());
  RTC_DCHECK_GE(ref_buffer.height(), test_buffer.height());
  if ((ref_buffer.width() != test_buffer.width()) ||
      (ref_buffer.height() != test_buffer.height())) {
    scoped_refptr<I420Buffer> scaled_ref_buffer =
        I420Buffer::Create(test_buffer.width(), test_buffer.height());
    scaled_ref_buffer->ScaleFrom(ref_buffer);
    return I420WeightedPSNR(*scaled_ref_buffer, test_buffer);
  }

  // Luma.
  int width_y = test_buffer.width();
  int height_y = test_buffer.height();
  uint64_t sse_y = libyuv::ComputeSumSquareErrorPlane(
      ref_buffer.DataY(), ref_buffer.StrideY(), test_buffer.DataY(),
      test_buffer.StrideY(), width_y, height_y);
  uint64_t num_samples_y = (uint64_t)width_y * (uint64_t)height_y;
  double psnr_y = libyuv::SumSquareErrorToPsnr(sse_y, num_samples_y);

  // Chroma.
  int width_uv = (width_y + 1) >> 1;
  int height_uv = (height_y + 1) >> 1;
  uint64_t sse_u = libyuv::ComputeSumSquareErrorPlane(
      ref_buffer.DataU(), ref_buffer.StrideU(), test_buffer.DataU(),
      test_buffer.StrideU(), width_uv, height_uv);
  uint64_t num_samples_uv = (uint64_t)width_uv * (uint64_t)height_uv;
  double psnr_u = libyuv::SumSquareErrorToPsnr(sse_u, num_samples_uv);
  uint64_t sse_v = libyuv::ComputeSumSquareErrorPlane(
      ref_buffer.DataV(), ref_buffer.StrideV(), test_buffer.DataV(),
      test_buffer.StrideV(), width_uv, height_uv);
  double psnr_v = libyuv::SumSquareErrorToPsnr(sse_v, num_samples_uv);

  // Weights from Ohm et. al 2012.
  double psnr_yuv = (6.0 * psnr_y + psnr_u + psnr_v) / 8.0;
  return (psnr_yuv > kPerfectPSNR) ? kPerfectPSNR : psnr_yuv;
}

// Compute SSIM for an I420A frame (all planes). Can upscale test frame.
double I420ASSIM(const I420ABufferInterface& ref_buffer,
                 const I420ABufferInterface& test_buffer) {
  RTC_DCHECK_GE(ref_buffer.width(), test_buffer.width());
  RTC_DCHECK_GE(ref_buffer.height(), test_buffer.height());
  if ((ref_buffer.width() != test_buffer.width()) ||
      (ref_buffer.height() != test_buffer.height())) {
    scoped_refptr<I420ABufferInterface> scaled_buffer =
        ScaleI420ABuffer(test_buffer, ref_buffer.width(), ref_buffer.height());
    return I420ASSIM(ref_buffer, *scaled_buffer);
  }
  const double yuv_ssim = libyuv::I420Ssim(
      ref_buffer.DataY(), ref_buffer.StrideY(), ref_buffer.DataU(),
      ref_buffer.StrideU(), ref_buffer.DataV(), ref_buffer.StrideV(),
      test_buffer.DataY(), test_buffer.StrideY(), test_buffer.DataU(),
      test_buffer.StrideU(), test_buffer.DataV(), test_buffer.StrideV(),
      test_buffer.width(), test_buffer.height());
  const double a_ssim = libyuv::CalcFrameSsim(
      ref_buffer.DataA(), ref_buffer.StrideA(), test_buffer.DataA(),
      test_buffer.StrideA(), test_buffer.width(), test_buffer.height());
  return (yuv_ssim + (a_ssim * 0.8)) / 1.8;
}

// Compute SSIM for an I420A frame (all planes)
double I420ASSIM(const VideoFrame* ref_frame, const VideoFrame* test_frame) {
  if (!ref_frame || !test_frame)
    return -1;
  RTC_DCHECK(ref_frame->video_frame_buffer()->type() ==
             VideoFrameBuffer::Type::kI420A);
  RTC_DCHECK(test_frame->video_frame_buffer()->type() ==
             VideoFrameBuffer::Type::kI420A);
  return I420ASSIM(*ref_frame->video_frame_buffer()->GetI420A(),
                   *test_frame->video_frame_buffer()->GetI420A());
}

// Compute SSIM for an I420 frame (all planes). Can upscale test_buffer.
double I420SSIM(const I420BufferInterface& ref_buffer,
                const I420BufferInterface& test_buffer) {
  RTC_DCHECK_GE(ref_buffer.width(), test_buffer.width());
  RTC_DCHECK_GE(ref_buffer.height(), test_buffer.height());
  if ((ref_buffer.width() != test_buffer.width()) ||
      (ref_buffer.height() != test_buffer.height())) {
    scoped_refptr<I420Buffer> scaled_buffer =
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
  return I420SSIM(*ref_frame->video_frame_buffer()->ToI420(),
                  *test_frame->video_frame_buffer()->ToI420());
}

void NV12Scale(uint8_t* tmp_buffer,
               const uint8_t* src_y,
               int src_stride_y,
               const uint8_t* src_uv,
               int src_stride_uv,
               int src_width,
               int src_height,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_uv,
               int dst_stride_uv,
               int dst_width,
               int dst_height) {
  const int src_chroma_width = (src_width + 1) / 2;
  const int src_chroma_height = (src_height + 1) / 2;

  if (src_width == dst_width && src_height == dst_height) {
    // No scaling.
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

  uint8_t* const src_u = tmp_buffer;
  uint8_t* const src_v = src_u + src_chroma_width * src_chroma_height;
  uint8_t* const dst_u = src_v + src_chroma_width * src_chroma_height;
  uint8_t* const dst_v = dst_u + dst_chroma_width * dst_chroma_height;

  // Split source UV plane into separate U and V plane using the temporary data.
  libyuv::SplitUVPlane(src_uv, src_stride_uv, src_u, src_chroma_width, src_v,
                       src_chroma_width, src_chroma_width, src_chroma_height);

  // Scale the planes.
  libyuv::I420Scale(
      src_y, src_stride_y, src_u, src_chroma_width, src_v, src_chroma_width,
      src_width, src_height, dst_y, dst_stride_y, dst_u, dst_chroma_width,
      dst_v, dst_chroma_width, dst_width, dst_height, libyuv::kFilterBox);

  // Merge the UV planes into the destination.
  libyuv::MergeUVPlane(dst_u, dst_chroma_width, dst_v, dst_chroma_width, dst_uv,
                       dst_stride_uv, dst_chroma_width, dst_chroma_height);
}

NV12ToI420Scaler::NV12ToI420Scaler() = default;
NV12ToI420Scaler::~NV12ToI420Scaler() = default;

void NV12ToI420Scaler::NV12ToI420Scale(const uint8_t* src_y,
                                       int src_stride_y,
                                       const uint8_t* src_uv,
                                       int src_stride_uv,
                                       int src_width,
                                       int src_height,
                                       uint8_t* dst_y,
                                       int dst_stride_y,
                                       uint8_t* dst_u,
                                       int dst_stride_u,
                                       uint8_t* dst_v,
                                       int dst_stride_v,
                                       int dst_width,
                                       int dst_height) {
  if (src_width == dst_width && src_height == dst_height) {
    // No scaling.
    tmp_uv_planes_.clear();
    tmp_uv_planes_.shrink_to_fit();
    libyuv::NV12ToI420(src_y, src_stride_y, src_uv, src_stride_uv, dst_y,
                       dst_stride_y, dst_u, dst_stride_u, dst_v, dst_stride_v,
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
  libyuv::SplitUVPlane(src_uv, src_stride_uv, src_u, src_uv_width, src_v,
                       src_uv_width, src_uv_width, src_uv_height);

  // Scale the planes into the destination.
  libyuv::I420Scale(src_y, src_stride_y, src_u, src_uv_width, src_v,
                    src_uv_width, src_width, src_height, dst_y, dst_stride_y,
                    dst_u, dst_stride_u, dst_v, dst_stride_v, dst_width,
                    dst_height, libyuv::kFilterBox);
}

}  // namespace webrtc
