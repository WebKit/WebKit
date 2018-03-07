/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/desktop_frame.h"

#include <utility>

#include <string.h>

#include "modules/desktop_capture/desktop_geometry.h"
#include "rtc_base/checks.h"
#include "rtc_base/ptr_util.h"

namespace webrtc {

DesktopFrame::DesktopFrame(DesktopSize size,
                           int stride,
                           uint8_t* data,
                           SharedMemory* shared_memory)
    : data_(data),
      shared_memory_(shared_memory),
      size_(size),
      stride_(stride),
      capture_time_ms_(0),
      capturer_id_(DesktopCapturerId::kUnknown) {
  RTC_DCHECK(size_.width() >= 0);
  RTC_DCHECK(size_.height() >= 0);
}

DesktopFrame::~DesktopFrame() = default;

void DesktopFrame::CopyPixelsFrom(const uint8_t* src_buffer, int src_stride,
                                  const DesktopRect& dest_rect) {
  RTC_CHECK(DesktopRect::MakeSize(size()).ContainsRect(dest_rect));

  uint8_t* dest = GetFrameDataAtPos(dest_rect.top_left());
  for (int y = 0; y < dest_rect.height(); ++y) {
    memcpy(dest, src_buffer, DesktopFrame::kBytesPerPixel * dest_rect.width());
    src_buffer += src_stride;
    dest += stride();
  }
}

void DesktopFrame::CopyPixelsFrom(const DesktopFrame& src_frame,
                                  const DesktopVector& src_pos,
                                  const DesktopRect& dest_rect) {
  RTC_CHECK(DesktopRect::MakeSize(src_frame.size()).ContainsRect(
      DesktopRect::MakeOriginSize(src_pos, dest_rect.size())));

  CopyPixelsFrom(src_frame.GetFrameDataAtPos(src_pos),
                 src_frame.stride(), dest_rect);
}

DesktopRect DesktopFrame::rect() const {
  return DesktopRect::MakeOriginSize(top_left(), size());
}

uint8_t* DesktopFrame::GetFrameDataAtPos(const DesktopVector& pos) const {
  return data() + stride() * pos.y() + DesktopFrame::kBytesPerPixel * pos.x();
}

void DesktopFrame::CopyFrameInfoFrom(const DesktopFrame& other) {
  set_dpi(other.dpi());
  set_capture_time_ms(other.capture_time_ms());
  set_capturer_id(other.capturer_id());
  *mutable_updated_region() = other.updated_region();
  set_top_left(other.top_left());
}

void DesktopFrame::MoveFrameInfoFrom(DesktopFrame* other) {
  set_dpi(other->dpi());
  set_capture_time_ms(other->capture_time_ms());
  set_capturer_id(other->capturer_id());
  mutable_updated_region()->Swap(other->mutable_updated_region());
  set_top_left(other->top_left());
}

BasicDesktopFrame::BasicDesktopFrame(DesktopSize size)
    : DesktopFrame(size, kBytesPerPixel * size.width(),
                   new uint8_t[kBytesPerPixel * size.width() * size.height()],
                   nullptr) {}

BasicDesktopFrame::~BasicDesktopFrame() {
  delete[] data_;
}

// static
DesktopFrame* BasicDesktopFrame::CopyOf(const DesktopFrame& frame) {
  DesktopFrame* result = new BasicDesktopFrame(frame.size());
  for (int y = 0; y < frame.size().height(); ++y) {
    memcpy(result->data() + y * result->stride(),
           frame.data() + y * frame.stride(),
           frame.size().width() * kBytesPerPixel);
  }
  result->CopyFrameInfoFrom(frame);
  return result;
}

// static
std::unique_ptr<DesktopFrame> SharedMemoryDesktopFrame::Create(
    DesktopSize size,
    SharedMemoryFactory* shared_memory_factory) {
  RTC_DCHECK(shared_memory_factory);

  size_t buffer_size = size.height() * size.width() * kBytesPerPixel;
  std::unique_ptr<SharedMemory> shared_memory =
      shared_memory_factory->CreateSharedMemory(buffer_size);
  if (!shared_memory)
    return nullptr;

  return rtc::MakeUnique<SharedMemoryDesktopFrame>(
      size, size.width() * kBytesPerPixel, std::move(shared_memory));
}

SharedMemoryDesktopFrame::SharedMemoryDesktopFrame(DesktopSize size,
                                                   int stride,
                                                   SharedMemory* shared_memory)
    : DesktopFrame(size,
                   stride,
                   reinterpret_cast<uint8_t*>(shared_memory->data()),
                   shared_memory) {}

SharedMemoryDesktopFrame::SharedMemoryDesktopFrame(
    DesktopSize size,
    int stride,
    std::unique_ptr<SharedMemory> shared_memory)
    : SharedMemoryDesktopFrame(size,
                               stride,
                               shared_memory.release()) {}

SharedMemoryDesktopFrame::~SharedMemoryDesktopFrame() {
  delete shared_memory_;
}

}  // namespace webrtc
