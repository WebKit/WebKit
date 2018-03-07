/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/desktop_and_cursor_composer.h"

#include <string.h>

#include <utility>

#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/mouse_cursor.h"
#include "modules/desktop_capture/mouse_cursor_monitor.h"
#include "rtc_base/checks.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/ptr_util.h"

namespace webrtc {

namespace {

// Helper function that blends one image into another. Source image must be
// pre-multiplied with the alpha channel. Destination is assumed to be opaque.
void AlphaBlend(uint8_t* dest, int dest_stride,
                const uint8_t* src, int src_stride,
                const DesktopSize& size) {
  for (int y = 0; y < size.height(); ++y) {
    for (int x = 0; x < size.width(); ++x) {
      uint32_t base_alpha = 255 - src[x * DesktopFrame::kBytesPerPixel + 3];
      if (base_alpha == 255) {
        continue;
      } else if (base_alpha == 0) {
        memcpy(dest + x * DesktopFrame::kBytesPerPixel,
               src + x * DesktopFrame::kBytesPerPixel,
               DesktopFrame::kBytesPerPixel);
      } else {
        dest[x * DesktopFrame::kBytesPerPixel] =
            dest[x * DesktopFrame::kBytesPerPixel] * base_alpha / 255 +
            src[x * DesktopFrame::kBytesPerPixel];
        dest[x * DesktopFrame::kBytesPerPixel + 1] =
            dest[x * DesktopFrame::kBytesPerPixel + 1] * base_alpha / 255 +
            src[x * DesktopFrame::kBytesPerPixel + 1];
        dest[x * DesktopFrame::kBytesPerPixel + 2] =
            dest[x * DesktopFrame::kBytesPerPixel + 2] * base_alpha / 255 +
            src[x * DesktopFrame::kBytesPerPixel + 2];
      }
    }
    src += src_stride;
    dest += dest_stride;
  }
}

// DesktopFrame wrapper that draws mouse on a frame and restores original
// content before releasing the underlying frame.
class DesktopFrameWithCursor : public DesktopFrame {
 public:
  // Takes ownership of |frame|.
  DesktopFrameWithCursor(std::unique_ptr<DesktopFrame> frame,
                         const MouseCursor& cursor,
                         const DesktopVector& position);
  ~DesktopFrameWithCursor() override;

 private:
  const std::unique_ptr<DesktopFrame> original_frame_;

  DesktopVector restore_position_;
  std::unique_ptr<DesktopFrame> restore_frame_;

  RTC_DISALLOW_COPY_AND_ASSIGN(DesktopFrameWithCursor);
};

DesktopFrameWithCursor::DesktopFrameWithCursor(
    std::unique_ptr<DesktopFrame> frame,
    const MouseCursor& cursor,
    const DesktopVector& position)
    : DesktopFrame(frame->size(),
                   frame->stride(),
                   frame->data(),
                   frame->shared_memory()),
      original_frame_(std::move(frame)) {
  MoveFrameInfoFrom(original_frame_.get());

  DesktopVector image_pos = position.subtract(cursor.hotspot());
  DesktopRect target_rect = DesktopRect::MakeSize(cursor.image()->size());
  target_rect.Translate(image_pos);
  DesktopVector target_origin = target_rect.top_left();
  target_rect.IntersectWith(DesktopRect::MakeSize(size()));

  if (target_rect.is_empty())
    return;

  // Copy original screen content under cursor to |restore_frame_|.
  restore_position_ = target_rect.top_left();
  restore_frame_.reset(new BasicDesktopFrame(target_rect.size()));
  restore_frame_->CopyPixelsFrom(*this, target_rect.top_left(),
                                 DesktopRect::MakeSize(restore_frame_->size()));

  // Blit the cursor.
  uint8_t* target_rect_data = reinterpret_cast<uint8_t*>(data()) +
                              target_rect.top() * stride() +
                              target_rect.left() * DesktopFrame::kBytesPerPixel;
  DesktopVector origin_shift = target_rect.top_left().subtract(target_origin);
  AlphaBlend(target_rect_data, stride(),
             cursor.image()->data() +
                 origin_shift.y() * cursor.image()->stride() +
                 origin_shift.x() * DesktopFrame::kBytesPerPixel,
             cursor.image()->stride(),
             target_rect.size());
}

DesktopFrameWithCursor::~DesktopFrameWithCursor() {
  // Restore original content of the frame.
  if (restore_frame_) {
    DesktopRect target_rect = DesktopRect::MakeSize(restore_frame_->size());
    target_rect.Translate(restore_position_);
    CopyPixelsFrom(restore_frame_->data(), restore_frame_->stride(),
                   target_rect);
  }
}

}  // namespace

DesktopAndCursorComposer::DesktopAndCursorComposer(
    DesktopCapturer* desktop_capturer,
    MouseCursorMonitor* mouse_monitor)
    : DesktopAndCursorComposer(desktop_capturer, mouse_monitor, false) {}

DesktopAndCursorComposer::DesktopAndCursorComposer(
    std::unique_ptr<DesktopCapturer> desktop_capturer,
    const DesktopCaptureOptions& options)
    : DesktopAndCursorComposer(desktop_capturer.release(),
                               MouseCursorMonitor::Create(options).release(),
                               true) {}

DesktopAndCursorComposer::DesktopAndCursorComposer(
    DesktopCapturer* desktop_capturer,
    MouseCursorMonitor* mouse_monitor,
    bool use_desktop_relative_cursor_position)
    : desktop_capturer_(desktop_capturer),
      mouse_monitor_(mouse_monitor),
      use_desktop_relative_cursor_position_(
          use_desktop_relative_cursor_position) {
  RTC_DCHECK(desktop_capturer_);
}

DesktopAndCursorComposer::~DesktopAndCursorComposer() = default;

void DesktopAndCursorComposer::Start(DesktopCapturer::Callback* callback) {
  callback_ = callback;
  if (mouse_monitor_)
    mouse_monitor_->Init(this, MouseCursorMonitor::SHAPE_AND_POSITION);
  desktop_capturer_->Start(this);
}

void DesktopAndCursorComposer::SetSharedMemoryFactory(
    std::unique_ptr<SharedMemoryFactory> shared_memory_factory) {
  desktop_capturer_->SetSharedMemoryFactory(std::move(shared_memory_factory));
}

void DesktopAndCursorComposer::CaptureFrame() {
  if (mouse_monitor_)
    mouse_monitor_->Capture();
  desktop_capturer_->CaptureFrame();
}

void DesktopAndCursorComposer::SetExcludedWindow(WindowId window) {
  desktop_capturer_->SetExcludedWindow(window);
}

void DesktopAndCursorComposer::OnCaptureResult(
    DesktopCapturer::Result result,
    std::unique_ptr<DesktopFrame> frame) {
  if (frame && cursor_) {
    if (use_desktop_relative_cursor_position_) {
      if (frame->rect().Contains(cursor_position_) &&
          !desktop_capturer_->IsOccluded(cursor_position_)) {
        const DesktopVector relative_position =
            cursor_position_.subtract(frame->top_left());
        frame = rtc::MakeUnique<DesktopFrameWithCursor>(
            std::move(frame), *cursor_, relative_position);
      }
    } else {
      if (cursor_state_ == MouseCursorMonitor::INSIDE) {
        frame = rtc::MakeUnique<DesktopFrameWithCursor>(
            std::move(frame), *cursor_, cursor_position_);
      }
    }
  }

  callback_->OnCaptureResult(result, std::move(frame));
}

void DesktopAndCursorComposer::OnMouseCursor(MouseCursor* cursor) {
  cursor_.reset(cursor);
}

void DesktopAndCursorComposer::OnMouseCursorPosition(
    MouseCursorMonitor::CursorState state,
    const DesktopVector& position) {
  if (!use_desktop_relative_cursor_position_) {
    cursor_state_ = state;
    cursor_position_ = position;
  }
}

void DesktopAndCursorComposer::OnMouseCursorPosition(
    const DesktopVector& position) {
  if (use_desktop_relative_cursor_position_) {
    cursor_position_ = position;
  }
}

}  // namespace webrtc
