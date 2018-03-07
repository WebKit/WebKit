/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/mouse_cursor_monitor.h"

#include <assert.h>

#include <memory>

#include <ApplicationServices/ApplicationServices.h>
#include <Cocoa/Cocoa.h>
#include <CoreFoundation/CoreFoundation.h>

#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capture_types.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/mac/desktop_configuration.h"
#include "modules/desktop_capture/mac/desktop_configuration_monitor.h"
#include "modules/desktop_capture/mac/full_screen_chrome_window_detector.h"
#include "modules/desktop_capture/mac/window_list_utils.h"
#include "modules/desktop_capture/mouse_cursor.h"
#include "rtc_base/macutils.h"
#include "rtc_base/scoped_ref_ptr.h"

namespace webrtc {

namespace {
CGImageRef CreateScaledCGImage(CGImageRef image, int width, int height) {
  // Create context, keeping original image properties.
  CGColorSpaceRef colorspace = CGImageGetColorSpace(image);
  CGContextRef context = CGBitmapContextCreate(nullptr,
                                               width,
                                               height,
                                               CGImageGetBitsPerComponent(image),
                                               width * DesktopFrame::kBytesPerPixel,
                                               colorspace,
                                               CGImageGetBitmapInfo(image));

  if (!context) return nil;

  // Draw image to context, resizing it.
  CGContextDrawImage(context, CGRectMake(0, 0, width, height), image);
  // Extract resulting image from context.
  CGImageRef imgRef = CGBitmapContextCreateImage(context);
  CGContextRelease(context);

  return imgRef;
}
}  // namespace

class MouseCursorMonitorMac : public MouseCursorMonitor {
 public:
  MouseCursorMonitorMac(const DesktopCaptureOptions& options,
                        CGWindowID window_id,
                        ScreenId screen_id);
  ~MouseCursorMonitorMac() override;

  void Init(Callback* callback, Mode mode) override;
  void Capture() override;

 private:
  static void DisplaysReconfiguredCallback(CGDirectDisplayID display,
                                           CGDisplayChangeSummaryFlags flags,
                                           void *user_parameter);
  void DisplaysReconfigured(CGDirectDisplayID display,
                            CGDisplayChangeSummaryFlags flags);

  void CaptureImage(float scale);

  rtc::scoped_refptr<DesktopConfigurationMonitor> configuration_monitor_;
  CGWindowID window_id_;
  ScreenId screen_id_;
  Callback* callback_;
  Mode mode_;
  __strong NSImage* last_cursor_;
  rtc::scoped_refptr<FullScreenChromeWindowDetector>
      full_screen_chrome_window_detector_;
};

MouseCursorMonitorMac::MouseCursorMonitorMac(
    const DesktopCaptureOptions& options,
    CGWindowID window_id,
    ScreenId screen_id)
    : configuration_monitor_(options.configuration_monitor()),
      window_id_(window_id),
      screen_id_(screen_id),
      callback_(NULL),
      mode_(SHAPE_AND_POSITION),
      full_screen_chrome_window_detector_(
          options.full_screen_chrome_window_detector()) {
  assert(window_id == kCGNullWindowID || screen_id == kInvalidScreenId);
}

MouseCursorMonitorMac::~MouseCursorMonitorMac() {}

void MouseCursorMonitorMac::Init(Callback* callback, Mode mode) {
  assert(!callback_);
  assert(callback);

  callback_ = callback;
  mode_ = mode;
}

void MouseCursorMonitorMac::Capture() {
  assert(callback_);

  CursorState state = INSIDE;

  CGEventRef event = CGEventCreate(NULL);
  CGPoint gc_position = CGEventGetLocation(event);
  CFRelease(event);

  DesktopVector position(gc_position.x, gc_position.y);

  configuration_monitor_->Lock();
  MacDesktopConfiguration configuration =
      configuration_monitor_->desktop_configuration();
  configuration_monitor_->Unlock();
  float scale = GetScaleFactorAtPosition(configuration, position);

  CaptureImage(scale);

  if (mode_ != SHAPE_AND_POSITION)
    return;

  // If we are capturing cursor for a specific window then we need to figure out
  // if the current mouse position is covered by another window and also adjust
  // |position| to make it relative to the window origin.
  if (window_id_ != kCGNullWindowID) {
    CGWindowID on_screen_window = window_id_;
    if (full_screen_chrome_window_detector_) {
      CGWindowID full_screen_window =
          full_screen_chrome_window_detector_->FindFullScreenWindow(window_id_);

      if (full_screen_window != kCGNullWindowID)
        on_screen_window = full_screen_window;
    }

    // Get list of windows that may be covering parts of |on_screen_window|.
    // CGWindowListCopyWindowInfo() returns windows in order from front to back,
    // so |on_screen_window| is expected to be the last in the list.
    CFArrayRef window_array =
        CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly |
                                       kCGWindowListOptionOnScreenAboveWindow |
                                       kCGWindowListOptionIncludingWindow,
                                   on_screen_window);
    bool found_window = false;
    if (window_array) {
      CFIndex count = CFArrayGetCount(window_array);
      for (CFIndex i = 0; i < count; ++i) {
        CFDictionaryRef window = reinterpret_cast<CFDictionaryRef>(
            CFArrayGetValueAtIndex(window_array, i));

        // Skip the Dock window. Dock window covers the whole screen, but it is
        // transparent.
        CFStringRef window_name = reinterpret_cast<CFStringRef>(
            CFDictionaryGetValue(window, kCGWindowName));
        if (window_name && CFStringCompare(window_name, CFSTR("Dock"), 0) == 0)
          continue;

        CFDictionaryRef window_bounds = reinterpret_cast<CFDictionaryRef>(
            CFDictionaryGetValue(window, kCGWindowBounds));
        CFNumberRef window_number = reinterpret_cast<CFNumberRef>(
            CFDictionaryGetValue(window, kCGWindowNumber));

        if (window_bounds && window_number) {
          CGRect gc_window_rect;
          if (!CGRectMakeWithDictionaryRepresentation(window_bounds,
                                                      &gc_window_rect)) {
            continue;
          }
          DesktopRect window_rect =
              DesktopRect::MakeXYWH(gc_window_rect.origin.x,
                                    gc_window_rect.origin.y,
                                    gc_window_rect.size.width,
                                    gc_window_rect.size.height);

          CGWindowID window_id;
          if (!CFNumberGetValue(window_number, kCFNumberIntType, &window_id))
            continue;

          if (window_id == on_screen_window) {
            found_window = true;
            if (!window_rect.Contains(position))
              state = OUTSIDE;
            position = position.subtract(window_rect.top_left());

            assert(i == count - 1);
            break;
          } else if (window_rect.Contains(position)) {
            state = OUTSIDE;
            position.set(-1, -1);
            break;
          }
        }
      }
      CFRelease(window_array);
    }
    if (!found_window) {
      // If we failed to get list of windows or the window wasn't in the list
      // pretend that the cursor is outside the window. This can happen, e.g. if
      // the window was closed.
      state = OUTSIDE;
      position.set(-1, -1);
    }
  } else {
    assert(screen_id_ >= kFullDesktopScreenId);
    if (screen_id_ != kFullDesktopScreenId) {
      // For single screen capturing, convert the position to relative to the
      // target screen.
      const MacDisplayConfiguration* config =
          configuration.FindDisplayConfigurationById(
              static_cast<CGDirectDisplayID>(screen_id_));
      if (config) {
        if (!config->pixel_bounds.Contains(position))
          state = OUTSIDE;
        position = position.subtract(config->bounds.top_left());
      } else {
        // The target screen is no longer valid.
        state = OUTSIDE;
        position.set(-1, -1);
      }
    }
  }
  // Convert Density Independent Pixel to physical pixel.
  position = DesktopVector(round(position.x() * scale),
                           round(position.y() * scale));
  // TODO(zijiehe): Remove this overload.
  callback_->OnMouseCursorPosition(state, position);
  callback_->OnMouseCursorPosition(
      position.subtract(configuration.bounds.top_left()));
}

void MouseCursorMonitorMac::CaptureImage(float scale) {
  NSCursor* nscursor = [NSCursor currentSystemCursor];

  NSImage* nsimage = [nscursor image];
  if (nsimage == nil || !nsimage.isValid) {
    return;
  }
  NSSize nssize = [nsimage size];  // DIP size

  // No need to caputre cursor image if it's unchanged since last capture.
  if ([[nsimage TIFFRepresentation] isEqual:[last_cursor_ TIFFRepresentation]]) return;
  last_cursor_ = nsimage;

  DesktopSize size(round(nssize.width * scale),
                   round(nssize.height * scale));  // Pixel size
  NSPoint nshotspot = [nscursor hotSpot];
  DesktopVector hotspot(
      std::max(0,
               std::min(size.width(), static_cast<int>(nshotspot.x * scale))),
      std::max(0,
               std::min(size.height(), static_cast<int>(nshotspot.y * scale))));
  CGImageRef cg_image =
      [nsimage CGImageForProposedRect:NULL context:nil hints:nil];
  if (!cg_image)
    return;

  // Before 10.12, OSX may report 1X cursor on Retina screen. (See
  // crbug.com/632995.) After 10.12, OSX may report 2X cursor on non-Retina
  // screen. (See crbug.com/671436.) So scaling the cursor if needed.
  CGImageRef scaled_cg_image = nil;
  if (CGImageGetWidth(cg_image) != static_cast<size_t>(size.width())) {
    scaled_cg_image = CreateScaledCGImage(cg_image, size.width(), size.height());
    if (scaled_cg_image != nil) {
      cg_image = scaled_cg_image;
    }
  }
  if (CGImageGetBitsPerPixel(cg_image) != DesktopFrame::kBytesPerPixel * 8 ||
      CGImageGetWidth(cg_image) != static_cast<size_t>(size.width()) ||
      CGImageGetBitsPerComponent(cg_image) != 8) {
    if (scaled_cg_image != nil) CGImageRelease(scaled_cg_image);
    return;
  }

  CGDataProviderRef provider = CGImageGetDataProvider(cg_image);
  CFDataRef image_data_ref = CGDataProviderCopyData(provider);
  if (image_data_ref == NULL) {
    if (scaled_cg_image != nil) CGImageRelease(scaled_cg_image);
    return;
  }

  const uint8_t* src_data =
      reinterpret_cast<const uint8_t*>(CFDataGetBytePtr(image_data_ref));

  // Create a MouseCursor that describes the cursor and pass it to
  // the client.
  std::unique_ptr<DesktopFrame> image(
      new BasicDesktopFrame(DesktopSize(size.width(), size.height())));

  int src_stride = CGImageGetBytesPerRow(cg_image);
  image->CopyPixelsFrom(src_data, src_stride, DesktopRect::MakeSize(size));

  CFRelease(image_data_ref);
  if (scaled_cg_image != nil) CGImageRelease(scaled_cg_image);

  std::unique_ptr<MouseCursor> cursor(
      new MouseCursor(image.release(), hotspot));

  callback_->OnMouseCursor(cursor.release());
}

MouseCursorMonitor* MouseCursorMonitor::CreateForWindow(
    const DesktopCaptureOptions& options, WindowId window) {
  return new MouseCursorMonitorMac(options, window, kInvalidScreenId);
}

MouseCursorMonitor* MouseCursorMonitor::CreateForScreen(
    const DesktopCaptureOptions& options,
    ScreenId screen) {
  return new MouseCursorMonitorMac(options, kCGNullWindowID, screen);
}

std::unique_ptr<MouseCursorMonitor> MouseCursorMonitor::Create(
    const DesktopCaptureOptions& options) {
  return std::unique_ptr<MouseCursorMonitor>(
      CreateForScreen(options, kFullDesktopScreenId));
}

}  // namespace webrtc
