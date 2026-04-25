/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/mappable_native_buffer.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "api/array_view.h"
#include "api/make_ref_counted.h"
#include "api/scoped_refptr.h"
#include "api/video/i420_buffer.h"
#include "api/video/nv12_buffer.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_buffer.h"
#include "api/video/video_rotation.h"
#include "rtc_base/checks.h"
#include "rtc_base/synchronization/mutex.h"

namespace webrtc {
namespace test {

namespace {

class NV12BufferWithDidConvertToI420 : public NV12Buffer {
 public:
  NV12BufferWithDidConvertToI420(int width, int height)
      : NV12Buffer(width, height), did_convert_to_i420_(false) {}

  bool did_convert_to_i420() const { return did_convert_to_i420_; }

  scoped_refptr<I420BufferInterface> ToI420() override {
    did_convert_to_i420_ = true;
    return NV12Buffer::ToI420();
  }

 private:
  bool did_convert_to_i420_;
};

}  // namespace

VideoFrame CreateMappableNativeFrame(int64_t ntp_time_ms,
                                     VideoFrameBuffer::Type mappable_type,
                                     int width,
                                     int height) {
  VideoFrame frame =
      VideoFrame::Builder()
          .set_video_frame_buffer(make_ref_counted<MappableNativeBuffer>(
              mappable_type, width, height))
          .set_rtp_timestamp(99)
          .set_timestamp_ms(99)
          .set_rotation(kVideoRotation_0)
          .build();
  frame.set_ntp_time_ms(ntp_time_ms);
  return frame;
}

scoped_refptr<MappableNativeBuffer> GetMappableNativeBufferFromVideoFrame(
    const VideoFrame& frame) {
  return scoped_refptr<MappableNativeBuffer>(
      static_cast<MappableNativeBuffer*>(frame.video_frame_buffer().get()));
}

MappableNativeBuffer::ScaledBuffer::ScaledBuffer(
    scoped_refptr<MappableNativeBuffer> parent,
    int width,
    int height)
    : parent_(std::move(parent)), width_(width), height_(height) {}

MappableNativeBuffer::ScaledBuffer::~ScaledBuffer() {}

scoped_refptr<VideoFrameBuffer>
MappableNativeBuffer::ScaledBuffer::CropAndScale(int offset_x,
                                                 int offset_y,
                                                 int crop_width,
                                                 int crop_height,
                                                 int scaled_width,
                                                 int scaled_height) {
  return make_ref_counted<ScaledBuffer>(parent_, scaled_width, scaled_height);
}

scoped_refptr<I420BufferInterface>
MappableNativeBuffer::ScaledBuffer::ToI420() {
  return parent_->GetOrCreateMappedBuffer(width_, height_)->ToI420();
}

scoped_refptr<VideoFrameBuffer>
MappableNativeBuffer::ScaledBuffer::GetMappedFrameBuffer(
    ArrayView<VideoFrameBuffer::Type> types) {
  if (absl::c_find(types, parent_->mappable_type_) == types.end())
    return nullptr;
  return parent_->GetOrCreateMappedBuffer(width_, height_);
}

MappableNativeBuffer::MappableNativeBuffer(VideoFrameBuffer::Type mappable_type,
                                           int width,
                                           int height)
    : mappable_type_(mappable_type), width_(width), height_(height) {
  RTC_DCHECK(mappable_type_ == VideoFrameBuffer::Type::kI420 ||
             mappable_type_ == VideoFrameBuffer::Type::kNV12);
}

MappableNativeBuffer::~MappableNativeBuffer() {}

scoped_refptr<VideoFrameBuffer> MappableNativeBuffer::CropAndScale(
    int offset_x,
    int offset_y,
    int crop_width,
    int crop_height,
    int scaled_width,
    int scaled_height) {
  return FullSizeBuffer()->CropAndScale(
      offset_x, offset_y, crop_width, crop_height, scaled_width, scaled_height);
}

scoped_refptr<I420BufferInterface> MappableNativeBuffer::ToI420() {
  return FullSizeBuffer()->ToI420();
}

scoped_refptr<VideoFrameBuffer> MappableNativeBuffer::GetMappedFrameBuffer(
    ArrayView<VideoFrameBuffer::Type> types) {
  return FullSizeBuffer()->GetMappedFrameBuffer(types);
}

std::vector<scoped_refptr<VideoFrameBuffer>>
MappableNativeBuffer::GetMappedFramedBuffers() const {
  MutexLock lock(&lock_);
  return mapped_buffers_;
}

bool MappableNativeBuffer::DidConvertToI420() const {
  if (mappable_type_ != VideoFrameBuffer::Type::kNV12)
    return false;
  MutexLock lock(&lock_);
  for (auto& mapped_buffer : mapped_buffers_) {
    if (static_cast<NV12BufferWithDidConvertToI420*>(mapped_buffer.get())
            ->did_convert_to_i420()) {
      return true;
    }
  }
  return false;
}

scoped_refptr<MappableNativeBuffer::ScaledBuffer>
MappableNativeBuffer::FullSizeBuffer() {
  return make_ref_counted<ScaledBuffer>(
      scoped_refptr<MappableNativeBuffer>(this), width_, height_);
}

scoped_refptr<VideoFrameBuffer> MappableNativeBuffer::GetOrCreateMappedBuffer(
    int width,
    int height) {
  MutexLock lock(&lock_);
  for (auto& mapped_buffer : mapped_buffers_) {
    if (mapped_buffer->width() == width && mapped_buffer->height() == height) {
      return mapped_buffer;
    }
  }
  scoped_refptr<VideoFrameBuffer> mapped_buffer;
  switch (mappable_type_) {
    case VideoFrameBuffer::Type::kI420: {
      scoped_refptr<I420Buffer> i420_buffer = I420Buffer::Create(width, height);
      I420Buffer::SetBlack(i420_buffer.get());
      mapped_buffer = i420_buffer;
      break;
    }
    case VideoFrameBuffer::Type::kNV12: {
      auto nv12_buffer =
          make_ref_counted<NV12BufferWithDidConvertToI420>(width, height);
      nv12_buffer->InitializeData();
      mapped_buffer = std::move(nv12_buffer);
      break;
    }
    default:
      RTC_DCHECK_NOTREACHED();
  }
  mapped_buffers_.push_back(mapped_buffer);
  return mapped_buffer;
}

}  // namespace test
}  // namespace webrtc
