/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/sdk/objc/Framework/Classes/Video/corevideo_frame_buffer.h"

#include "libyuv/convert.h"
#include "webrtc/api/video/i420_buffer.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"

namespace webrtc {

CoreVideoFrameBuffer::CoreVideoFrameBuffer(CVPixelBufferRef pixel_buffer,
                                           int adapted_width,
                                           int adapted_height,
                                           int crop_width,
                                           int crop_height,
                                           int crop_x,
                                           int crop_y)
    : pixel_buffer_(pixel_buffer),
      width_(adapted_width),
      height_(adapted_height),
      buffer_width_(CVPixelBufferGetWidth(pixel_buffer)),
      buffer_height_(CVPixelBufferGetHeight(pixel_buffer)),
      crop_width_(crop_width),
      crop_height_(crop_height),
      // Can only crop at even pixels.
      crop_x_(crop_x & ~1),
      crop_y_(crop_y & ~1) {
  CVBufferRetain(pixel_buffer_);
}

CoreVideoFrameBuffer::CoreVideoFrameBuffer(CVPixelBufferRef pixel_buffer)
    : pixel_buffer_(pixel_buffer),
      width_(CVPixelBufferGetWidth(pixel_buffer)),
      height_(CVPixelBufferGetHeight(pixel_buffer)),
      buffer_width_(width_),
      buffer_height_(height_),
      crop_width_(width_),
      crop_height_(height_),
      crop_x_(0),
      crop_y_(0) {
  CVBufferRetain(pixel_buffer_);
}

CoreVideoFrameBuffer::~CoreVideoFrameBuffer() {
  CVBufferRelease(pixel_buffer_);
}

VideoFrameBuffer::Type CoreVideoFrameBuffer::type() const {
  return Type::kNative;
}

int CoreVideoFrameBuffer::width() const {
  return width_;
}

int CoreVideoFrameBuffer::height() const {
  return height_;
}

rtc::scoped_refptr<I420BufferInterface> CoreVideoFrameBuffer::ToI420() {
  const OSType pixel_format = CVPixelBufferGetPixelFormatType(pixel_buffer_);
  RTC_DCHECK(pixel_format == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange ||
             pixel_format == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange);

  CVPixelBufferLockBaseAddress(pixel_buffer_, kCVPixelBufferLock_ReadOnly);
  const uint8_t* src_y = static_cast<const uint8_t*>(
      CVPixelBufferGetBaseAddressOfPlane(pixel_buffer_, 0));
  const int src_y_stride = CVPixelBufferGetBytesPerRowOfPlane(pixel_buffer_, 0);
  const uint8_t* src_uv = static_cast<const uint8_t*>(
      CVPixelBufferGetBaseAddressOfPlane(pixel_buffer_, 1));
  const int src_uv_stride =
      CVPixelBufferGetBytesPerRowOfPlane(pixel_buffer_, 1);

  // Crop just by modifying pointers.
  src_y += src_y_stride * crop_y_ + crop_x_;
  src_uv += src_uv_stride * (crop_y_ / 2) + crop_x_;

  // TODO(magjed): Use a frame buffer pool.
  NV12ToI420Scaler nv12_to_i420_scaler;
  rtc::scoped_refptr<I420Buffer> buffer =
      new rtc::RefCountedObject<I420Buffer>(width_, height_);
  nv12_to_i420_scaler.NV12ToI420Scale(
      src_y, src_y_stride,
      src_uv, src_uv_stride,
      crop_width_, crop_height_,
      buffer->MutableDataY(), buffer->StrideY(),
      buffer->MutableDataU(), buffer->StrideU(),
      buffer->MutableDataV(), buffer->StrideV(),
      buffer->width(), buffer->height());

  CVPixelBufferUnlockBaseAddress(pixel_buffer_, kCVPixelBufferLock_ReadOnly);

  return buffer;
}

bool CoreVideoFrameBuffer::RequiresCropping() const {
  return crop_width_ != buffer_width_ || crop_height_ != buffer_height_;
}

bool CoreVideoFrameBuffer::CropAndScaleTo(
    std::vector<uint8_t>* tmp_buffer,
    CVPixelBufferRef output_pixel_buffer) const {
  // Prepare output pointers.
  RTC_DCHECK_EQ(CVPixelBufferGetPixelFormatType(output_pixel_buffer),
                kCVPixelFormatType_420YpCbCr8BiPlanarFullRange);
  CVReturn cv_ret = CVPixelBufferLockBaseAddress(output_pixel_buffer, 0);
  if (cv_ret != kCVReturnSuccess) {
    LOG(LS_ERROR) << "Failed to lock base address: " << cv_ret;
    return false;
  }
  const int dst_width = CVPixelBufferGetWidth(output_pixel_buffer);
  const int dst_height = CVPixelBufferGetHeight(output_pixel_buffer);
  uint8_t* dst_y = reinterpret_cast<uint8_t*>(
      CVPixelBufferGetBaseAddressOfPlane(output_pixel_buffer, 0));
  const int dst_y_stride =
      CVPixelBufferGetBytesPerRowOfPlane(output_pixel_buffer, 0);
  uint8_t* dst_uv = reinterpret_cast<uint8_t*>(
      CVPixelBufferGetBaseAddressOfPlane(output_pixel_buffer, 1));
  const int dst_uv_stride =
      CVPixelBufferGetBytesPerRowOfPlane(output_pixel_buffer, 1);

  // Prepare source pointers.
  const OSType src_pixel_format =
      CVPixelBufferGetPixelFormatType(pixel_buffer_);
  RTC_DCHECK(
      src_pixel_format == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange ||
      src_pixel_format == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange);
  CVPixelBufferLockBaseAddress(pixel_buffer_, kCVPixelBufferLock_ReadOnly);
  const uint8_t* src_y = static_cast<const uint8_t*>(
      CVPixelBufferGetBaseAddressOfPlane(pixel_buffer_, 0));
  const int src_y_stride = CVPixelBufferGetBytesPerRowOfPlane(pixel_buffer_, 0);
  const uint8_t* src_uv = static_cast<const uint8_t*>(
      CVPixelBufferGetBaseAddressOfPlane(pixel_buffer_, 1));
  const int src_uv_stride =
      CVPixelBufferGetBytesPerRowOfPlane(pixel_buffer_, 1);

  // Crop just by modifying pointers.
  src_y += src_y_stride * crop_y_ + crop_x_;
  src_uv += src_uv_stride * (crop_y_ / 2) + crop_x_;

  if (crop_width_ == dst_width && crop_height_ == dst_height) {
    tmp_buffer->clear();
    tmp_buffer->shrink_to_fit();
  } else {
    const int src_chroma_width = (crop_width_ + 1) / 2;
    const int src_chroma_height = (crop_height_ + 1) / 2;
    const int dst_chroma_width = (dst_width + 1) / 2;
    const int dst_chroma_height = (dst_height + 1) / 2;
    tmp_buffer->resize(src_chroma_width * src_chroma_height * 2 +
                       dst_chroma_width * dst_chroma_height * 2);
    tmp_buffer->shrink_to_fit();
  }

  NV12Scale(tmp_buffer->data(),
            src_y, src_y_stride,
            src_uv, src_uv_stride,
            crop_width_, crop_height_,
            dst_y, dst_y_stride,
            dst_uv, dst_uv_stride,
            dst_width, dst_height);

  CVPixelBufferUnlockBaseAddress(pixel_buffer_, kCVPixelBufferLock_ReadOnly);
  CVPixelBufferUnlockBaseAddress(output_pixel_buffer, 0);

  return true;
}

}  // namespace webrtc
