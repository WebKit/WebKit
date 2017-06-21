/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/video/video_frame_buffer.h"

#include "libyuv/convert.h"
#include "webrtc/api/video/i420_buffer.h"
#include "webrtc/base/checks.h"

namespace webrtc {

namespace {

// TODO(magjed): Remove this class. It is only used for providing a default
// implementation of ToI420() until external clients are updated. ToI420() will
// then be made pure virtual. This adapter adapts a VideoFrameBuffer (which is
// expected to be in I420 format) to I420BufferInterface. The reason this is
// needed is because of the return type mismatch in NativeToI420Buffer (returns
// VideoFrameBuffer) vs ToI420 (returns I420BufferInterface).
class I420InterfaceAdapter : public I420BufferInterface {
 public:
  explicit I420InterfaceAdapter(const VideoFrameBuffer* buffer)
      : buffer_(buffer) {}

  int width() const override { return buffer_->width(); }
  int height() const override { return buffer_->height(); }

  const uint8_t* DataY() const override { return buffer_->DataY(); }
  const uint8_t* DataU() const override { return buffer_->DataU(); }
  const uint8_t* DataV() const override { return buffer_->DataV(); }

  int StrideY() const override { return buffer_->StrideY(); }
  int StrideU() const override { return buffer_->StrideU(); }
  int StrideV() const override { return buffer_->StrideV(); }

 private:
  rtc::scoped_refptr<const VideoFrameBuffer> buffer_;
};

}  // namespace

// TODO(magjed): The default implementations in VideoFrameBuffer are provided in
// order to support the deprecated interface until external clients are updated.
// Remove once done.
VideoFrameBuffer::Type VideoFrameBuffer::type() const {
  return native_handle() ? Type::kNative : Type::kI420;
}

const uint8_t* VideoFrameBuffer::DataY() const {
  return GetI420()->DataY();
}

const uint8_t* VideoFrameBuffer::DataU() const {
  return GetI420()->DataU();
}

const uint8_t* VideoFrameBuffer::DataV() const {
  return GetI420()->DataV();
}

// Returns the number of bytes between successive rows for a given plane.
int VideoFrameBuffer::StrideY() const {
  return GetI420()->StrideY();
}

int VideoFrameBuffer::StrideU() const {
  return GetI420()->StrideU();
}

int VideoFrameBuffer::StrideV() const {
  return GetI420()->StrideV();
}

void* VideoFrameBuffer::native_handle() const {
  RTC_DCHECK(type() != Type::kNative);
  return nullptr;
}

rtc::scoped_refptr<VideoFrameBuffer> VideoFrameBuffer::NativeToI420Buffer() {
  return ToI420();
}

rtc::scoped_refptr<I420BufferInterface> VideoFrameBuffer::ToI420() {
  return new rtc::RefCountedObject<I420InterfaceAdapter>(NativeToI420Buffer());
}

rtc::scoped_refptr<I420BufferInterface> VideoFrameBuffer::GetI420() {
  RTC_CHECK(type() == Type::kI420);
  // TODO(magjed): static_cast to I420BufferInterface instead once external
  // clients are updated.
  return new rtc::RefCountedObject<I420InterfaceAdapter>(this);
}

rtc::scoped_refptr<const I420BufferInterface> VideoFrameBuffer::GetI420()
    const {
  RTC_CHECK(type() == Type::kI420);
  // TODO(magjed): static_cast to I420BufferInterface instead once external
  // clients are updated.
  return new rtc::RefCountedObject<I420InterfaceAdapter>(this);
}

I444BufferInterface* VideoFrameBuffer::GetI444() {
  RTC_CHECK(type() == Type::kI444);
  return static_cast<I444BufferInterface*>(this);
}

const I444BufferInterface* VideoFrameBuffer::GetI444() const {
  RTC_CHECK(type() == Type::kI444);
  return static_cast<const I444BufferInterface*>(this);
}

VideoFrameBuffer::Type I420BufferInterface::type() const {
  return Type::kI420;
}

int I420BufferInterface::ChromaWidth() const {
  return (width() + 1) / 2;
}

int I420BufferInterface::ChromaHeight() const {
  return (height() + 1) / 2;
}

rtc::scoped_refptr<I420BufferInterface> I420BufferInterface::ToI420() {
  return this;
}

VideoFrameBuffer::Type I444BufferInterface::type() const {
  return Type::kI444;
}

int I444BufferInterface::ChromaWidth() const {
  return width();
}

int I444BufferInterface::ChromaHeight() const {
  return height();
}

rtc::scoped_refptr<I420BufferInterface> I444BufferInterface::ToI420() {
  rtc::scoped_refptr<I420Buffer> i420_buffer =
      I420Buffer::Create(width(), height());
  libyuv::I444ToI420(DataY(), StrideY(), DataU(), StrideU(), DataV(), StrideV(),
                     i420_buffer->MutableDataY(), i420_buffer->StrideY(),
                     i420_buffer->MutableDataU(), i420_buffer->StrideU(),
                     i420_buffer->MutableDataV(), i420_buffer->StrideV(),
                     width(), height());
  return i420_buffer;
}

}  // namespace webrtc
