/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <string.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xrender.h>
#include <X11/Xutil.h>

#include <algorithm>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/modules/desktop_capture/desktop_capturer.h"
#include "webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "webrtc/modules/desktop_capture/desktop_frame.h"
#include "webrtc/modules/desktop_capture/x11/shared_x_display.h"
#include "webrtc/modules/desktop_capture/x11/x_error_trap.h"
#include "webrtc/modules/desktop_capture/x11/x_server_pixel_buffer.h"
#include "webrtc/system_wrappers/include/logging.h"

namespace webrtc {

namespace {

// Convenience wrapper for XGetWindowProperty() results.
template <class PropertyType>
class XWindowProperty {
 public:
  XWindowProperty(Display* display, Window window, Atom property) {
    const int kBitsPerByte = 8;
    Atom actual_type;
    int actual_format;
    unsigned long bytes_after;  // NOLINT: type required by XGetWindowProperty
    int status = XGetWindowProperty(display, window, property, 0L, ~0L, False,
                                    AnyPropertyType, &actual_type,
                                    &actual_format, &size_,
                                    &bytes_after, &data_);
    if (status != Success) {
      data_ = nullptr;
      return;
    }
    if (sizeof(PropertyType) * kBitsPerByte != actual_format) {
      size_ = 0;
      return;
    }

    is_valid_ = true;
  }

  ~XWindowProperty() {
    if (data_)
      XFree(data_);
  }

  // True if we got properly value successfully.
  bool is_valid() const { return is_valid_; }

  // Size and value of the property.
  size_t size() const { return size_; }
  const PropertyType* data() const {
    return reinterpret_cast<PropertyType*>(data_);
  }
  PropertyType* data() {
    return reinterpret_cast<PropertyType*>(data_);
  }

 private:
  bool is_valid_ = false;
  unsigned long size_ = 0;  // NOLINT: type required by XGetWindowProperty
  unsigned char* data_ = nullptr;

  RTC_DISALLOW_COPY_AND_ASSIGN(XWindowProperty);
};

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

  // SharedXDisplay::XEventHandler interface.
  bool HandleXEvent(const XEvent& event) override;

 private:
  Display* display() { return x_display_->display(); }

  // Iterates through |window| hierarchy to find first visible window, i.e. one
  // that has WM_STATE property set to NormalState.
  // See http://tronche.com/gui/x/icccm/sec-4.html#s-4.1.3.1 .
  ::Window GetApplicationWindow(::Window window);

  // Returns true if the |window| is a desktop element.
  bool IsDesktopElement(::Window window);

  // Returns window title for the specified X |window|.
  bool GetWindowTitle(::Window window, std::string* title);

  Callback* callback_ = nullptr;

  rtc::scoped_refptr<SharedXDisplay> x_display_;

  Atom wm_state_atom_;
  Atom window_type_atom_;
  Atom normal_window_type_atom_;
  bool has_composite_extension_ = false;

  ::Window selected_window_ = 0;
  XServerPixelBuffer x_server_pixel_buffer_;

  RTC_DISALLOW_COPY_AND_ASSIGN(WindowCapturerLinux);
};

WindowCapturerLinux::WindowCapturerLinux(const DesktopCaptureOptions& options)
    : x_display_(options.x_display()) {
  // Create Atoms so we don't need to do it every time they are used.
  wm_state_atom_ = XInternAtom(display(), "WM_STATE", True);
  window_type_atom_ = XInternAtom(display(), "_NET_WM_WINDOW_TYPE", True);
  normal_window_type_atom_ = XInternAtom(
      display(), "_NET_WM_WINDOW_TYPE_NORMAL", True);

  int event_base, error_base, major_version, minor_version;
  if (XCompositeQueryExtension(display(), &event_base, &error_base) &&
      XCompositeQueryVersion(display(), &major_version, &minor_version) &&
      // XCompositeNameWindowPixmap() requires version 0.2
      (major_version > 0 || minor_version >= 2)) {
    has_composite_extension_ = true;
  } else {
    LOG(LS_INFO) << "Xcomposite extension not available or too old.";
  }

  x_display_->AddEventHandler(ConfigureNotify, this);
}

WindowCapturerLinux::~WindowCapturerLinux() {
  x_display_->RemoveEventHandler(ConfigureNotify, this);
}

bool WindowCapturerLinux::GetSourceList(SourceList* sources) {
  SourceList result;

  XErrorTrap error_trap(display());

  int num_screens = XScreenCount(display());
  for (int screen = 0; screen < num_screens; ++screen) {
    ::Window root_window = XRootWindow(display(), screen);
    ::Window parent;
    ::Window *children;
    unsigned int num_children;
    int status = XQueryTree(display(), root_window, &root_window, &parent,
                            &children, &num_children);
    if (status == 0) {
      LOG(LS_ERROR) << "Failed to query for child windows for screen "
                    << screen;
      continue;
    }

    for (unsigned int i = 0; i < num_children; ++i) {
      // Iterate in reverse order to return windows from front to back.
      ::Window app_window =
          GetApplicationWindow(children[num_children - 1 - i]);
      if (app_window && !IsDesktopElement(app_window)) {
        Source w;
        w.id = app_window;
        if (GetWindowTitle(app_window, &w.title))
          result.push_back(w);
      }
    }

    if (children)
      XFree(children);
  }

  sources->swap(result);

  return true;
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
  int status = XQueryTree(
      display(), selected_window_, &root, &parent, &children, &num_children);
  if (status == 0) {
    LOG(LS_ERROR) << "Failed to query for the root window.";
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

    XSendEvent(display(),
               root,
               False,
               SubstructureRedirectMask | SubstructureNotifyMask,
               &xev);
  }
  XFlush(display());
  return true;
}

void WindowCapturerLinux::Start(Callback* callback) {
  assert(!callback_);
  assert(callback);

  callback_ = callback;
}

void WindowCapturerLinux::CaptureFrame() {
  if (!x_server_pixel_buffer_.IsWindowValid()) {
    LOG(LS_INFO) << "The window is no longer valid.";
    callback_->OnCaptureResult(Result::ERROR_PERMANENT, nullptr);
    return;
  }

  x_display_->ProcessPendingXEvents();

  if (!has_composite_extension_) {
    // Without the Xcomposite extension we capture when the whole window is
    // visible on screen and not covered by any other window. This is not
    // something we want so instead, just bail out.
    LOG(LS_INFO) << "No Xcomposite extension detected.";
    callback_->OnCaptureResult(Result::ERROR_PERMANENT, nullptr);
    return;
  }

  std::unique_ptr<DesktopFrame> frame(
      new BasicDesktopFrame(x_server_pixel_buffer_.window_size()));

  x_server_pixel_buffer_.Synchronize();
  if (!x_server_pixel_buffer_.CaptureRect(DesktopRect::MakeSize(frame->size()),
                                          frame.get())) {
    callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
    return;
  }

  frame->mutable_updated_region()->SetRect(
      DesktopRect::MakeSize(frame->size()));

  callback_->OnCaptureResult(Result::SUCCESS, std::move(frame));
}

bool WindowCapturerLinux::HandleXEvent(const XEvent& event) {
  if (event.type == ConfigureNotify) {
    XConfigureEvent xce = event.xconfigure;
    if (!DesktopSize(xce.width, xce.height).equals(
            x_server_pixel_buffer_.window_size())) {
      if (!x_server_pixel_buffer_.Init(display(), selected_window_)) {
        LOG(LS_ERROR) << "Failed to initialize pixel buffer after resizing.";
      }
      return true;
    }
  }
  return false;
}

::Window WindowCapturerLinux::GetApplicationWindow(::Window window) {
  // Get WM_STATE property of the window.
  XWindowProperty<uint32_t> window_state(display(), window, wm_state_atom_);

  // WM_STATE is considered to be set to WithdrawnState when it missing.
  int32_t state = window_state.is_valid() ?
      *window_state.data() : WithdrawnState;

  if (state == NormalState) {
    // Window has WM_STATE==NormalState. Return it.
    return window;
  } else if (state == IconicState) {
    // Window is in minimized. Skip it.
    return 0;
  }

  // If the window is in WithdrawnState then look at all of its children.
  ::Window root, parent;
  ::Window *children;
  unsigned int num_children;
  if (!XQueryTree(display(), window, &root, &parent, &children,
                  &num_children)) {
    LOG(LS_ERROR) << "Failed to query for child windows although window"
                  << "does not have a valid WM_STATE.";
    return 0;
  }
  ::Window app_window = 0;
  for (unsigned int i = 0; i < num_children; ++i) {
    app_window = GetApplicationWindow(children[i]);
    if (app_window)
      break;
  }

  if (children)
    XFree(children);
  return app_window;
}

bool WindowCapturerLinux::IsDesktopElement(::Window window) {
  if (window == 0)
    return false;

  // First look for _NET_WM_WINDOW_TYPE. The standard
  // (http://standards.freedesktop.org/wm-spec/latest/ar01s05.html#id2760306)
  // says this hint *should* be present on all windows, and we use the existence
  // of _NET_WM_WINDOW_TYPE_NORMAL in the property to indicate a window is not
  // a desktop element (that is, only "normal" windows should be shareable).
  XWindowProperty<uint32_t> window_type(display(), window, window_type_atom_);
  if (window_type.is_valid() && window_type.size() > 0) {
    uint32_t* end = window_type.data() + window_type.size();
    bool is_normal = (end != std::find(
        window_type.data(), end, normal_window_type_atom_));
    return !is_normal;
  }

  // Fall back on using the hint.
  XClassHint class_hint;
  Status status = XGetClassHint(display(), window, &class_hint);
  bool result = false;
  if (status == 0) {
    // No hints, assume this is a normal application window.
    return result;
  }

  if (strcmp("gnome-panel", class_hint.res_name) == 0 ||
      strcmp("desktop_window", class_hint.res_name) == 0) {
    result = true;
  }
  XFree(class_hint.res_name);
  XFree(class_hint.res_class);
  return result;
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
      status = Xutf8TextPropertyToTextList(display(), &window_name, &list,
                                           &cnt);
      if (status >= Success && cnt && *list) {
        if (cnt > 1) {
          LOG(LS_INFO) << "Window has " << cnt
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
