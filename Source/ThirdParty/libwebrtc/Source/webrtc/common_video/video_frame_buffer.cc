/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/common_video/include/video_frame_buffer.h"

#include <string.h>

#include <algorithm>

#include "webrtc/base/checks.h"
#include "webrtc/base/keep_ref_until_done.h"
#include "libyuv/convert.h"
#include "libyuv/planar_functions.h"
#include "libyuv/scale.h"

// Aligning pointer to 64 bytes for improved performance, e.g. use SIMD.
static const int kBufferAlignment = 64;

namespace webrtc {

namespace {

int I420DataSize(int height, int stride_y, int stride_u, int stride_v) {
  return stride_y * height + (stride_u + stride_v) * ((height + 1) / 2);
}

}  // namespace

VideoFrameBuffer::~VideoFrameBuffer() {}

I420Buffer::I420Buffer(int width, int height)
    : I420Buffer(width, height, width, (width + 1) / 2, (width + 1) / 2) {
}

I420Buffer::I420Buffer(int width,
                       int height,
                       int stride_y,
                       int stride_u,
                       int stride_v)
    : width_(width),
      height_(height),
      stride_y_(stride_y),
      stride_u_(stride_u),
      stride_v_(stride_v),
      data_(static_cast<uint8_t*>(AlignedMalloc(
          I420DataSize(height, stride_y, stride_u, stride_v),
          kBufferAlignment))) {
  RTC_DCHECK_GT(width, 0);
  RTC_DCHECK_GT(height, 0);
  RTC_DCHECK_GE(stride_y, width);
  RTC_DCHECK_GE(stride_u, (width + 1) / 2);
  RTC_DCHECK_GE(stride_v, (width + 1) / 2);
}

I420Buffer::~I420Buffer() {
}

rtc::scoped_refptr<I420Buffer> I420Buffer::Create(int width, int height) {
  return new rtc::RefCountedObject<I420Buffer>(width, height);
}

rtc::scoped_refptr<I420Buffer> I420Buffer::Create(int width,
                                                  int height,
                                                  int stride_y,
                                                  int stride_u,
                                                  int stride_v) {
  return new rtc::RefCountedObject<I420Buffer>(
      width, height, stride_y, stride_u, stride_v);
}

void I420Buffer::InitializeData() {
  memset(data_.get(), 0,
         I420DataSize(height_, stride_y_, stride_u_, stride_v_));
}

int I420Buffer::width() const {
  return width_;
}

int I420Buffer::height() const {
  return height_;
}

const uint8_t* I420Buffer::DataY() const {
  return data_.get();
}
const uint8_t* I420Buffer::DataU() const {
  return data_.get() + stride_y_ * height_;
}
const uint8_t* I420Buffer::DataV() const {
  return data_.get() + stride_y_ * height_ + stride_u_ * ((height_ + 1) / 2);
}

uint8_t* I420Buffer::MutableDataY() {
  return const_cast<uint8_t*>(DataY());
}
uint8_t* I420Buffer::MutableDataU() {
  return const_cast<uint8_t*>(DataU());
}
uint8_t* I420Buffer::MutableDataV() {
  return const_cast<uint8_t*>(DataV());
}

int I420Buffer::StrideY() const {
  return stride_y_;
}
int I420Buffer::StrideU() const {
  return stride_u_;
}
int I420Buffer::StrideV() const {
  return stride_v_;
}

void* I420Buffer::native_handle() const {
  return nullptr;
}

rtc::scoped_refptr<VideoFrameBuffer> I420Buffer::NativeToI420Buffer() {
  RTC_NOTREACHED();
  return nullptr;
}

// static
rtc::scoped_refptr<I420Buffer> I420Buffer::Copy(
    const VideoFrameBuffer& source) {
  int width = source.width();
  int height = source.height();
  rtc::scoped_refptr<I420Buffer> target = I420Buffer::Create(width, height);
  RTC_CHECK(libyuv::I420Copy(source.DataY(), source.StrideY(),
                             source.DataU(), source.StrideU(),
                             source.DataV(), source.StrideV(),
                             target->MutableDataY(), target->StrideY(),
                             target->MutableDataU(), target->StrideU(),
                             target->MutableDataV(), target->StrideV(),
                             width, height) == 0);

  return target;
}

void I420Buffer::SetToBlack() {
  RTC_CHECK(libyuv::I420Rect(MutableDataY(), StrideY(),
                             MutableDataU(), StrideU(),
                             MutableDataV(), StrideV(),
                             0, 0, width(), height(),
                             0, 128, 128) == 0);
}

void I420Buffer::CropAndScaleFrom(
    const VideoFrameBuffer& src,
    int offset_x,
    int offset_y,
    int crop_width,
    int crop_height) {
  RTC_CHECK_LE(crop_width, src.width());
  RTC_CHECK_LE(crop_height, src.height());
  RTC_CHECK_LE(crop_width + offset_x, src.width());
  RTC_CHECK_LE(crop_height + offset_y, src.height());
  RTC_CHECK_GE(offset_x, 0);
  RTC_CHECK_GE(offset_y, 0);

  // Make sure offset is even so that u/v plane becomes aligned.
  const int uv_offset_x = offset_x / 2;
  const int uv_offset_y = offset_y / 2;
  offset_x = uv_offset_x * 2;
  offset_y = uv_offset_y * 2;

  const uint8_t* y_plane =
      src.DataY() + src.StrideY() * offset_y + offset_x;
  const uint8_t* u_plane =
      src.DataU() + src.StrideU() * uv_offset_y + uv_offset_x;
  const uint8_t* v_plane =
      src.DataV() + src.StrideV() * uv_offset_y + uv_offset_x;
  int res = libyuv::I420Scale(y_plane, src.StrideY(),
                              u_plane, src.StrideU(),
                              v_plane, src.StrideV(),
                              crop_width, crop_height,
                              MutableDataY(), StrideY(),
                              MutableDataU(), StrideU(),
                              MutableDataV(), StrideV(),
                              width(), height(), libyuv::kFilterBox);

  RTC_DCHECK_EQ(res, 0);
}

void I420Buffer::CropAndScaleFrom(
    const VideoFrameBuffer& src) {
  const int crop_width =
      std::min(src.width(), width() * src.height() / height());
  const int crop_height =
      std::min(src.height(), height() * src.width() / width());

  CropAndScaleFrom(
      src,
      (src.width() - crop_width) / 2, (src.height() - crop_height) / 2,
      crop_width, crop_height);
}

void I420Buffer::ScaleFrom(const VideoFrameBuffer& src) {
  CropAndScaleFrom(src, 0, 0, src.width(), src.height());
}

// static
rtc::scoped_refptr<VideoFrameBuffer> I420Buffer::Rotate(
    rtc::scoped_refptr<VideoFrameBuffer> src,
    VideoRotation rotation) {
  RTC_DCHECK(src->DataY());
  RTC_DCHECK(src->DataU());
  RTC_DCHECK(src->DataV());

  if (rotation == webrtc::kVideoRotation_0) {
    return src;
  }

  int rotated_width = src->width();
  int rotated_height = src->height();
  if (rotation == webrtc::kVideoRotation_90 ||
      rotation == webrtc::kVideoRotation_270) {
    std::swap(rotated_width, rotated_height);
  }

  rtc::scoped_refptr<webrtc::I420Buffer> buffer =
      I420Buffer::Create(rotated_width, rotated_height);

  int res = libyuv::I420Rotate(
      src->DataY(), src->StrideY(),
      src->DataU(), src->StrideU(),
      src->DataV(), src->StrideV(),
      buffer->MutableDataY(), buffer->StrideY(), buffer->MutableDataU(),
      buffer->StrideU(), buffer->MutableDataV(), buffer->StrideV(),
      src->width(), src->height(),
      static_cast<libyuv::RotationMode>(rotation));
  RTC_DCHECK_EQ(res, 0);

  return buffer;
}

NativeHandleBuffer::NativeHandleBuffer(void* native_handle,
                                       int width,
                                       int height)
    : native_handle_(native_handle), width_(width), height_(height) {
  RTC_DCHECK(native_handle != nullptr);
  RTC_DCHECK_GT(width, 0);
  RTC_DCHECK_GT(height, 0);
}

int NativeHandleBuffer::width() const {
  return width_;
}

int NativeHandleBuffer::height() const {
  return height_;
}

const uint8_t* NativeHandleBuffer::DataY() const {
  RTC_NOTREACHED();  // Should not be called.
  return nullptr;
}
const uint8_t* NativeHandleBuffer::DataU() const {
  RTC_NOTREACHED();  // Should not be called.
  return nullptr;
}
const uint8_t* NativeHandleBuffer::DataV() const {
  RTC_NOTREACHED();  // Should not be called.
  return nullptr;
}

int NativeHandleBuffer::StrideY() const {
  RTC_NOTREACHED();  // Should not be called.
  return 0;
}
int NativeHandleBuffer::StrideU() const {
  RTC_NOTREACHED();  // Should not be called.
  return 0;
}
int NativeHandleBuffer::StrideV() const {
  RTC_NOTREACHED();  // Should not be called.
  return 0;
}

void* NativeHandleBuffer::native_handle() const {
  return native_handle_;
}

WrappedI420Buffer::WrappedI420Buffer(int width,
                                     int height,
                                     const uint8_t* y_plane,
                                     int y_stride,
                                     const uint8_t* u_plane,
                                     int u_stride,
                                     const uint8_t* v_plane,
                                     int v_stride,
                                     const rtc::Callback0<void>& no_longer_used)
    : width_(width),
      height_(height),
      y_plane_(y_plane),
      u_plane_(u_plane),
      v_plane_(v_plane),
      y_stride_(y_stride),
      u_stride_(u_stride),
      v_stride_(v_stride),
      no_longer_used_cb_(no_longer_used) {
}

WrappedI420Buffer::~WrappedI420Buffer() {
  no_longer_used_cb_();
}

int WrappedI420Buffer::width() const {
  return width_;
}

int WrappedI420Buffer::height() const {
  return height_;
}

const uint8_t* WrappedI420Buffer::DataY() const {
  return y_plane_;
}
const uint8_t* WrappedI420Buffer::DataU() const {
  return u_plane_;
}
const uint8_t* WrappedI420Buffer::DataV() const {
  return v_plane_;
}

int WrappedI420Buffer::StrideY() const {
  return y_stride_;
}
int WrappedI420Buffer::StrideU() const {
  return u_stride_;
}
int WrappedI420Buffer::StrideV() const {
  return v_stride_;
}

void* WrappedI420Buffer::native_handle() const {
  return nullptr;
}

rtc::scoped_refptr<VideoFrameBuffer> WrappedI420Buffer::NativeToI420Buffer() {
  RTC_NOTREACHED();
  return nullptr;
}

}  // namespace webrtc
