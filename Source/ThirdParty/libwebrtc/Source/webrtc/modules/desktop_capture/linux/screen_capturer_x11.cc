/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/linux/screen_capturer_x11.h"

#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xfixes.h>

#include <memory>
#include <set>
#include <utility>

#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/linux/x_server_pixel_buffer.h"
#include "modules/desktop_capture/screen_capture_frame_queue.h"
#include "modules/desktop_capture/screen_capturer_helper.h"
#include "modules/desktop_capture/shared_desktop_frame.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/timeutils.h"
#include "rtc_base/trace_event.h"

namespace webrtc {

ScreenCapturerX11::ScreenCapturerX11() {
  helper_.SetLogGridSize(4);
}

ScreenCapturerX11::~ScreenCapturerX11() {
  options_.x_display()->RemoveEventHandler(ConfigureNotify, this);
  if (use_damage_) {
    options_.x_display()->RemoveEventHandler(damage_event_base_ + XDamageNotify,
                                             this);
  }
  DeinitXlib();
}

bool ScreenCapturerX11::Init(const DesktopCaptureOptions& options) {
  TRACE_EVENT0("webrtc", "ScreenCapturerX11::Init");
  options_ = options;

  root_window_ = RootWindow(display(), DefaultScreen(display()));
  if (root_window_ == BadValue) {
    RTC_LOG(LS_ERROR) << "Unable to get the root window";
    DeinitXlib();
    return false;
  }

  gc_ = XCreateGC(display(), root_window_, 0, NULL);
  if (gc_ == NULL) {
    RTC_LOG(LS_ERROR) << "Unable to get graphics context";
    DeinitXlib();
    return false;
  }

  options_.x_display()->AddEventHandler(ConfigureNotify, this);

  // Check for XFixes extension. This is required for cursor shape
  // notifications, and for our use of XDamage.
  if (XFixesQueryExtension(display(), &xfixes_event_base_,
                           &xfixes_error_base_)) {
    has_xfixes_ = true;
  } else {
    RTC_LOG(LS_INFO) << "X server does not support XFixes.";
  }

  // Register for changes to the dimensions of the root window.
  XSelectInput(display(), root_window_, StructureNotifyMask);

  if (!x_server_pixel_buffer_.Init(display(), DefaultRootWindow(display()))) {
    RTC_LOG(LS_ERROR) << "Failed to initialize pixel buffer.";
    return false;
  }

  if (options_.use_update_notifications()) {
    InitXDamage();
  }

  return true;
}

void ScreenCapturerX11::InitXDamage() {
  // Our use of XDamage requires XFixes.
  if (!has_xfixes_) {
    return;
  }

  // Check for XDamage extension.
  if (!XDamageQueryExtension(display(), &damage_event_base_,
                             &damage_error_base_)) {
    RTC_LOG(LS_INFO) << "X server does not support XDamage.";
    return;
  }

  // TODO(lambroslambrou): Disable DAMAGE in situations where it is known
  // to fail, such as when Desktop Effects are enabled, with graphics
  // drivers (nVidia, ATI) that fail to report DAMAGE notifications
  // properly.

  // Request notifications every time the screen becomes damaged.
  damage_handle_ =
      XDamageCreate(display(), root_window_, XDamageReportNonEmpty);
  if (!damage_handle_) {
    RTC_LOG(LS_ERROR) << "Unable to initialize XDamage.";
    return;
  }

  // Create an XFixes server-side region to collate damage into.
  damage_region_ = XFixesCreateRegion(display(), 0, 0);
  if (!damage_region_) {
    XDamageDestroy(display(), damage_handle_);
    RTC_LOG(LS_ERROR) << "Unable to create XFixes region.";
    return;
  }

  options_.x_display()->AddEventHandler(damage_event_base_ + XDamageNotify,
                                        this);

  use_damage_ = true;
  RTC_LOG(LS_INFO) << "Using XDamage extension.";
}

void ScreenCapturerX11::Start(Callback* callback) {
  RTC_DCHECK(!callback_);
  RTC_DCHECK(callback);

  callback_ = callback;
}

void ScreenCapturerX11::CaptureFrame() {
  TRACE_EVENT0("webrtc", "ScreenCapturerX11::CaptureFrame");
  int64_t capture_start_time_nanos = rtc::TimeNanos();

  queue_.MoveToNextFrame();
  RTC_DCHECK(!queue_.current_frame() || !queue_.current_frame()->IsShared());

  // Process XEvents for XDamage and cursor shape tracking.
  options_.x_display()->ProcessPendingXEvents();

  // ProcessPendingXEvents() may call ScreenConfigurationChanged() which
  // reinitializes |x_server_pixel_buffer_|. Check if the pixel buffer is still
  // in a good shape.
  if (!x_server_pixel_buffer_.is_initialized()) {
    // We failed to initialize pixel buffer.
    RTC_LOG(LS_ERROR) << "Pixel buffer is not initialized.";
    callback_->OnCaptureResult(Result::ERROR_PERMANENT, nullptr);
    return;
  }

  // If the current frame is from an older generation then allocate a new one.
  // Note that we can't reallocate other buffers at this point, since the caller
  // may still be reading from them.
  if (!queue_.current_frame()) {
    queue_.ReplaceCurrentFrame(
        SharedDesktopFrame::Wrap(std::unique_ptr<DesktopFrame>(
            new BasicDesktopFrame(x_server_pixel_buffer_.window_size()))));
  }

  std::unique_ptr<DesktopFrame> result = CaptureScreen();
  if (!result) {
    RTC_LOG(LS_WARNING) << "Temporarily failed to capture screen.";
    callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
    return;
  }

  last_invalid_region_ = result->updated_region();
  result->set_capture_time_ms((rtc::TimeNanos() - capture_start_time_nanos) /
                              rtc::kNumNanosecsPerMillisec);
  callback_->OnCaptureResult(Result::SUCCESS, std::move(result));
}

bool ScreenCapturerX11::GetSourceList(SourceList* sources) {
  RTC_DCHECK(sources->size() == 0);
  // TODO(jiayl): implement screen enumeration.
  sources->push_back({0});
  return true;
}

bool ScreenCapturerX11::SelectSource(SourceId id) {
  // TODO(jiayl): implement screen selection.
  return true;
}

bool ScreenCapturerX11::HandleXEvent(const XEvent& event) {
  if (use_damage_ && (event.type == damage_event_base_ + XDamageNotify)) {
    const XDamageNotifyEvent* damage_event =
        reinterpret_cast<const XDamageNotifyEvent*>(&event);
    if (damage_event->damage != damage_handle_)
      return false;
    RTC_DCHECK(damage_event->level == XDamageReportNonEmpty);
    return true;
  } else if (event.type == ConfigureNotify) {
    ScreenConfigurationChanged();
    return true;
  }
  return false;
}

std::unique_ptr<DesktopFrame> ScreenCapturerX11::CaptureScreen() {
  std::unique_ptr<SharedDesktopFrame> frame = queue_.current_frame()->Share();
  RTC_DCHECK(x_server_pixel_buffer_.window_size().equals(frame->size()));

  // Pass the screen size to the helper, so it can clip the invalid region if it
  // expands that region to a grid.
  helper_.set_size_most_recent(frame->size());

  // In the DAMAGE case, ensure the frame is up-to-date with the previous frame
  // if any.  If there isn't a previous frame, that means a screen-resolution
  // change occurred, and |invalid_rects| will be updated to include the whole
  // screen.
  if (use_damage_ && queue_.previous_frame())
    SynchronizeFrame();

  DesktopRegion* updated_region = frame->mutable_updated_region();

  x_server_pixel_buffer_.Synchronize();
  if (use_damage_ && queue_.previous_frame()) {
    // Atomically fetch and clear the damage region.
    XDamageSubtract(display(), damage_handle_, None, damage_region_);
    int rects_num = 0;
    XRectangle bounds;
    XRectangle* rects = XFixesFetchRegionAndBounds(display(), damage_region_,
                                                   &rects_num, &bounds);
    for (int i = 0; i < rects_num; ++i) {
      updated_region->AddRect(DesktopRect::MakeXYWH(
          rects[i].x, rects[i].y, rects[i].width, rects[i].height));
    }
    XFree(rects);
    helper_.InvalidateRegion(*updated_region);

    // Capture the damaged portions of the desktop.
    helper_.TakeInvalidRegion(updated_region);

    // Clip the damaged portions to the current screen size, just in case some
    // spurious XDamage notifications were received for a previous (larger)
    // screen size.
    updated_region->IntersectWith(
        DesktopRect::MakeSize(x_server_pixel_buffer_.window_size()));

    for (DesktopRegion::Iterator it(*updated_region); !it.IsAtEnd();
         it.Advance()) {
      if (!x_server_pixel_buffer_.CaptureRect(it.rect(), frame.get()))
        return nullptr;
    }
  } else {
    // Doing full-screen polling, or this is the first capture after a
    // screen-resolution change.  In either case, need a full-screen capture.
    DesktopRect screen_rect = DesktopRect::MakeSize(frame->size());
    if (!x_server_pixel_buffer_.CaptureRect(screen_rect, frame.get()))
      return nullptr;
    updated_region->SetRect(screen_rect);
  }

  return std::move(frame);
}

void ScreenCapturerX11::ScreenConfigurationChanged() {
  TRACE_EVENT0("webrtc", "ScreenCapturerX11::ScreenConfigurationChanged");
  // Make sure the frame buffers will be reallocated.
  queue_.Reset();

  helper_.ClearInvalidRegion();
  if (!x_server_pixel_buffer_.Init(display(), DefaultRootWindow(display()))) {
    RTC_LOG(LS_ERROR) << "Failed to initialize pixel buffer after screen "
                         "configuration change.";
  }
}

void ScreenCapturerX11::SynchronizeFrame() {
  // Synchronize the current buffer with the previous one since we do not
  // capture the entire desktop. Note that encoder may be reading from the
  // previous buffer at this time so thread access complaints are false
  // positives.

  // TODO(hclam): We can reduce the amount of copying here by subtracting
  // |capturer_helper_|s region from |last_invalid_region_|.
  // http://crbug.com/92354
  RTC_DCHECK(queue_.previous_frame());

  DesktopFrame* current = queue_.current_frame();
  DesktopFrame* last = queue_.previous_frame();
  RTC_DCHECK(current != last);
  for (DesktopRegion::Iterator it(last_invalid_region_); !it.IsAtEnd();
       it.Advance()) {
    current->CopyPixelsFrom(*last, it.rect().top_left(), it.rect());
  }
}

void ScreenCapturerX11::DeinitXlib() {
  if (gc_) {
    XFreeGC(display(), gc_);
    gc_ = nullptr;
  }

  x_server_pixel_buffer_.Release();

  if (display()) {
    if (damage_handle_) {
      XDamageDestroy(display(), damage_handle_);
      damage_handle_ = 0;
    }

    if (damage_region_) {
      XFixesDestroyRegion(display(), damage_region_);
      damage_region_ = 0;
    }
  }
}

// static
std::unique_ptr<DesktopCapturer> ScreenCapturerX11::CreateRawScreenCapturer(
    const DesktopCaptureOptions& options) {
  if (!options.x_display())
    return nullptr;

  std::unique_ptr<ScreenCapturerX11> capturer(new ScreenCapturerX11());
  if (!capturer.get()->Init(options)) {
    return nullptr;
  }

  return std::move(capturer);
}

}  // namespace webrtc
