/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>

#include <X11/Xutil.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xrender.h>

#include <utility>

#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/window_finder_x11.h"
#include "modules/desktop_capture/x11/shared_x_display.h"
#include "modules/desktop_capture/x11/window_list_utils.h"
#include "modules/desktop_capture/x11/x_atom_cache.h"
#include "modules/desktop_capture/x11/x_server_pixel_buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/logging.h"
#include "rtc_base/scoped_ref_ptr.h"
#include "rtc_base/trace_event.h"

namespace webrtc {

namespace {

class WindowCapturerLinux : public DesktopCapturer,
                            public SharedXDisplay::XEventHandler {
 public:
  WindowCapturerLinux(const DesktopCaptureOptions& options);
  ~WindowCapturerLinux() override;

  // DesktopCapturer interface.
  void Start(Callback* callback) override;
  void CaptureFrame() override;
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;
  bool FocusOnSelectedSource() override;
  bool IsOccluded(const DesktopVector& pos) override;

  // SharedXDisplay::XEventHandler interface.
  bool HandleXEvent(const XEvent& event) override;

 private:
  Display* display() { return x_display_->display(); }

  // Returns window title for the specified X |window|.
  bool GetWindowTitle(::Window window, std::string* title);

  Callback* callback_ = nullptr;

  rtc::scoped_refptr<SharedXDisplay> x_display_;

  bool has_composite_extension_ = false;

  ::Window selected_window_ = 0;
  XServerPixelBuffer x_server_pixel_buffer_;
  XAtomCache atom_cache_;
  WindowFinderX11 window_finder_;

  RTC_DISALLOW_COPY_AND_ASSIGN(WindowCapturerLinux);
};

WindowCapturerLinux::WindowCapturerLinux(const DesktopCaptureOptions& options)
    : x_display_(options.x_display()),
      atom_cache_(display()),
      window_finder_(&atom_cache_) {
  int event_base, error_base, major_version, minor_version;
  if (XCompositeQueryExtension(display(), &event_base, &error_base) &&
      XCompositeQueryVersion(display(), &major_version, &minor_version) &&
      // XCompositeNameWindowPixmap() requires version 0.2
      (major_version > 0 || minor_version >= 2)) {
    has_composite_extension_ = true;
  } else {
    RTC_LOG(LS_INFO) << "Xcomposite extension not available or too old.";
  }

  x_display_->AddEventHandler(ConfigureNotify, this);
}

WindowCapturerLinux::~WindowCapturerLinux() {
  x_display_->RemoveEventHandler(ConfigureNotify, this);
}

bool WindowCapturerLinux::GetSourceList(SourceList* sources) {
  return GetWindowList(&atom_cache_, [this, sources](::Window window) {
    Source w;
    w.id = window;
    if (this->GetWindowTitle(window, &w.title)) {
      sources->push_back(w);
    }
    return true;
  });
}

bool WindowCapturerLinux::SelectSource(SourceId id) {
  if (!x_server_pixel_buffer_.Init(display(), id))
    return false;

  // Tell the X server to send us window resizing events.
  XSelectInput(display(), id, StructureNotifyMask);

  selected_window_ = id;

  // In addition to needing X11 server-side support for Xcomposite, it actually
  // needs to be turned on for the window. If the user has modern
  // hardware/drivers but isn't using a compositing window manager, that won't
  // be the case. Here we automatically turn it on.

  // Redirect drawing to an offscreen buffer (ie, turn on compositing). X11
  // remembers who has requested this and will turn it off for us when we exit.
  XCompositeRedirectWindow(display(), id, CompositeRedirectAutomatic);

  return true;
}

bool WindowCapturerLinux::FocusOnSelectedSource() {
  if (!selected_window_)
    return false;

  unsigned int num_children;
  ::Window* children;
  ::Window parent;
  ::Window root;
  // Find the root window to pass event to.
  int status = XQueryTree(display(), selected_window_, &root, &parent,
                          &children, &num_children);
  if (status == 0) {
    RTC_LOG(LS_ERROR) << "Failed to query for the root window.";
    return false;
  }

  if (children)
    XFree(children);

  XRaiseWindow(display(), selected_window_);

  // Some window managers (e.g., metacity in GNOME) consider it illegal to
  // raise a window without also giving it input focus with
  // _NET_ACTIVE_WINDOW, so XRaiseWindow() on its own isn't enough.
  Atom atom = XInternAtom(display(), "_NET_ACTIVE_WINDOW", True);
  if (atom != None) {
    XEvent xev;
    xev.xclient.type = ClientMessage;
    xev.xclient.serial = 0;
    xev.xclient.send_event = True;
    xev.xclient.window = selected_window_;
    xev.xclient.message_type = atom;

    // The format member is set to 8, 16, or 32 and specifies whether the
    // data should be viewed as a list of bytes, shorts, or longs.
    xev.xclient.format = 32;

    memset(xev.xclient.data.l, 0, sizeof(xev.xclient.data.l));

    XSendEvent(display(), root, False,
               SubstructureRedirectMask | SubstructureNotifyMask, &xev);
  }
  XFlush(display());
  return true;
}

void WindowCapturerLinux::Start(Callback* callback) {
  RTC_DCHECK(!callback_);
  RTC_DCHECK(callback);

  callback_ = callback;
}

void WindowCapturerLinux::CaptureFrame() {
  TRACE_EVENT0("webrtc", "WindowCapturerLinux::CaptureFrame");

  if (!x_server_pixel_buffer_.IsWindowValid()) {
    RTC_LOG(LS_ERROR) << "The window is no longer valid.";
    callback_->OnCaptureResult(Result::ERROR_PERMANENT, nullptr);
    return;
  }

  x_display_->ProcessPendingXEvents();

  if (!has_composite_extension_) {
    // Without the Xcomposite extension we capture when the whole window is
    // visible on screen and not covered by any other window. This is not
    // something we want so instead, just bail out.
    RTC_LOG(LS_ERROR) << "No Xcomposite extension detected.";
    callback_->OnCaptureResult(Result::ERROR_PERMANENT, nullptr);
    return;
  }

  if (GetWindowState(&atom_cache_, selected_window_) == IconicState) {
    // Window is in minimized. Return a 1x1 frame as same as OSX/Win does.
    std::unique_ptr<DesktopFrame> frame(
        new BasicDesktopFrame(DesktopSize(1, 1)));
    callback_->OnCaptureResult(Result::SUCCESS, std::move(frame));
    return;
  }

  std::unique_ptr<DesktopFrame> frame(
      new BasicDesktopFrame(x_server_pixel_buffer_.window_size()));

  x_server_pixel_buffer_.Synchronize();
  if (!x_server_pixel_buffer_.CaptureRect(DesktopRect::MakeSize(frame->size()),
                                          frame.get())) {
    RTC_LOG(LS_WARNING) << "Temporarily failed to capture winodw.";
    callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
    return;
  }

  frame->mutable_updated_region()->SetRect(
      DesktopRect::MakeSize(frame->size()));
  frame->set_top_left(x_server_pixel_buffer_.window_rect().top_left());

  callback_->OnCaptureResult(Result::SUCCESS, std::move(frame));
}

bool WindowCapturerLinux::IsOccluded(const DesktopVector& pos) {
  return window_finder_.GetWindowUnderPoint(pos) !=
         static_cast<WindowId>(selected_window_);
}

bool WindowCapturerLinux::HandleXEvent(const XEvent& event) {
  if (event.type == ConfigureNotify) {
    XConfigureEvent xce = event.xconfigure;
    if (xce.window == selected_window_) {
      if (!DesktopRectFromXAttributes(xce).equals(
              x_server_pixel_buffer_.window_rect())) {
        if (!x_server_pixel_buffer_.Init(display(), selected_window_)) {
          RTC_LOG(LS_ERROR)
              << "Failed to initialize pixel buffer after resizing.";
        }
      }
    }
  }

  // Always returns false, so other observers can still receive the events.
  return false;
}

bool WindowCapturerLinux::GetWindowTitle(::Window window, std::string* title) {
  int status;
  bool result = false;
  XTextProperty window_name;
  window_name.value = nullptr;
  if (window) {
    status = XGetWMName(display(), window, &window_name);
    if (status && window_name.value && window_name.nitems) {
      int cnt;
      char** list = nullptr;
      status =
          Xutf8TextPropertyToTextList(display(), &window_name, &list, &cnt);
      if (status >= Success && cnt && *list) {
        if (cnt > 1) {
          RTC_LOG(LS_INFO) << "Window has " << cnt
                           << " text properties, only using the first one.";
        }
        *title = *list;
        result = true;
      }
      if (list)
        XFreeStringList(list);
    }
    if (window_name.value)
      XFree(window_name.value);
  }
  return result;
}

}  // namespace

// static
std::unique_ptr<DesktopCapturer> DesktopCapturer::CreateRawWindowCapturer(
    const DesktopCaptureOptions& options) {
  if (!options.x_display())
    return nullptr;
  return std::unique_ptr<DesktopCapturer>(new WindowCapturerLinux(options));
}

}  // namespace webrtc
