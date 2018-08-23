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

#include <memory>

#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_frame_win.h"
#include "modules/desktop_capture/win/screen_capture_utils.h"
#include "modules/desktop_capture/win/window_capture_utils.h"
#include "modules/desktop_capture/window_finder_win.h"
#include "rtc_base/checks.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/logging.h"
#include "rtc_base/stringutils.h"
#include "rtc_base/trace_event.h"
#include "rtc_base/win32.h"

namespace webrtc {

namespace {

BOOL CALLBACK WindowsEnumerationHandler(HWND hwnd, LPARAM param) {
  DesktopCapturer::SourceList* list =
      reinterpret_cast<DesktopCapturer::SourceList*>(param);

  // Skip windows that are invisible, minimized, have no title, or are owned,
  // unless they have the app window style set.
  int len = GetWindowTextLength(hwnd);
  HWND owner = GetWindow(hwnd, GW_OWNER);
  LONG exstyle = GetWindowLong(hwnd, GWL_EXSTYLE);
  if (len == 0 || IsIconic(hwnd) || !IsWindowVisible(hwnd) ||
      (owner && !(exstyle & WS_EX_APPWINDOW))) {
    return TRUE;
  }

  // Skip the Program Manager window and the Start button.
  const size_t kClassLength = 256;
  WCHAR class_name[kClassLength];
  const int class_name_length = GetClassName(hwnd, class_name, kClassLength);
  RTC_DCHECK(class_name_length)
      << "Error retrieving the application's class name";

  // Skip Program Manager window and the Start button. This is the same logic
  // that's used in Win32WindowPicker in libjingle. Consider filtering other
  // windows as well (e.g. toolbars).
  if (wcscmp(class_name, L"Progman") == 0 || wcscmp(class_name, L"Button") == 0)
    return TRUE;

  // Windows 8 introduced a "Modern App" identified by their class name being
  // either ApplicationFrameWindow or windows.UI.Core.coreWindow. The
  // associated windows cannot be captured, so we skip them.
  // http://crbug.com/526883.
  if (rtc::IsWindows8OrLater() &&
      (wcscmp(class_name, L"ApplicationFrameWindow") == 0 ||
       wcscmp(class_name, L"Windows.UI.Core.CoreWindow") == 0)) {
    return TRUE;
  }

  DesktopCapturer::Source window;
  window.id = reinterpret_cast<WindowId>(hwnd);

  const size_t kTitleLength = 500;
  WCHAR window_title[kTitleLength];
  // Truncate the title if it's longer than kTitleLength.
  GetWindowText(hwnd, window_title, kTitleLength);
  window.title = rtc::ToUtf8(window_title);

  // Skip windows when we failed to convert the title or it is empty.
  if (window.title.empty())
    return TRUE;

  list->push_back(window);

  return TRUE;
}

// Retrieves the rectangle of the window rect which is drawable by either OS or
// the owner application. The returned DesktopRect is in system coordinates.
// This function returns false if native APIs fail.
//
// When |window| is maximized, its borders and shadow effect will be ignored by
// OS and leave black. So we prefer to use GetCroppedWindowRect() when capturing
// its content to avoid the black area in the final DesktopFrame. But when the
// window is in normal mode, borders and shadow should be included.
bool GetWindowDrawableRect(HWND window,
                           DesktopRect* drawable_rect,
                           DesktopRect* original_rect) {
  if (!GetWindowRect(window, original_rect)) {
    return false;
  }

  bool is_maximized = false;
  if (!IsWindowMaximized(window, &is_maximized)) {
    return false;
  }

  if (is_maximized) {
    return GetCroppedWindowRect(window, drawable_rect,
                                /* original_rect */ nullptr);
  }
  *drawable_rect = *original_rect;
  return true;
}

class WindowCapturerWin : public DesktopCapturer {
 public:
  WindowCapturerWin();
  ~WindowCapturerWin() override;

  // DesktopCapturer interface.
  void Start(Callback* callback) override;
  void CaptureFrame() override;
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;
  bool FocusOnSelectedSource() override;
  bool IsOccluded(const DesktopVector& pos) override;

 private:
  Callback* callback_ = nullptr;

  // HWND and HDC for the currently selected window or nullptr if window is not
  // selected.
  HWND window_ = nullptr;

  DesktopSize previous_size_;

  WindowCaptureHelperWin window_capture_helper_;

  // This map is used to avoid flickering for the case when SelectWindow() calls
  // are interleaved with Capture() calls.
  std::map<HWND, DesktopSize> window_size_map_;

  WindowFinderWin window_finder_;

  RTC_DISALLOW_COPY_AND_ASSIGN(WindowCapturerWin);
};

WindowCapturerWin::WindowCapturerWin() {}
WindowCapturerWin::~WindowCapturerWin() {}

bool WindowCapturerWin::GetSourceList(SourceList* sources) {
  SourceList result;
  LPARAM param = reinterpret_cast<LPARAM>(&result);
  // EnumWindows only enumerates root windows.
  if (!EnumWindows(&WindowsEnumerationHandler, param))
    return false;

  for (auto it = result.begin(); it != result.end();) {
    if (!window_capture_helper_.IsWindowOnCurrentDesktop(
            reinterpret_cast<HWND>(it->id))) {
      it = result.erase(it);
    } else {
      ++it;
    }
  }
  sources->swap(result);

  std::map<HWND, DesktopSize> new_map;
  for (const auto& item : *sources) {
    HWND hwnd = reinterpret_cast<HWND>(item.id);
    new_map[hwnd] = window_size_map_[hwnd];
  }
  window_size_map_.swap(new_map);

  return true;
}

bool WindowCapturerWin::SelectSource(SourceId id) {
  HWND window = reinterpret_cast<HWND>(id);
  if (!IsWindow(window) || !IsWindowVisible(window) || IsIconic(window))
    return false;
  window_ = window;
  // When a window is not in the map, window_size_map_[window] will create an
  // item with DesktopSize (0, 0).
  previous_size_ = window_size_map_[window];
  return true;
}

bool WindowCapturerWin::FocusOnSelectedSource() {
  if (!window_)
    return false;

  if (!IsWindow(window_) || !IsWindowVisible(window_) || IsIconic(window_))
    return false;

  return BringWindowToTop(window_) != FALSE &&
         SetForegroundWindow(window_) != FALSE;
}

bool WindowCapturerWin::IsOccluded(const DesktopVector& pos) {
  DesktopVector sys_pos = pos.add(GetFullscreenRect().top_left());
  return reinterpret_cast<HWND>(window_finder_.GetWindowUnderPoint(sys_pos)) !=
         window_;
}

void WindowCapturerWin::Start(Callback* callback) {
  assert(!callback_);
  assert(callback);

  callback_ = callback;
}

void WindowCapturerWin::CaptureFrame() {
  TRACE_EVENT0("webrtc", "WindowCapturerWin::CaptureFrame");

  if (!window_) {
    RTC_LOG(LS_ERROR) << "Window hasn't been selected: " << GetLastError();
    callback_->OnCaptureResult(Result::ERROR_PERMANENT, nullptr);
    return;
  }

  // Stop capturing if the window has been closed.
  if (!IsWindow(window_)) {
    RTC_LOG(LS_ERROR) << "target window has been closed";
    callback_->OnCaptureResult(Result::ERROR_PERMANENT, nullptr);
    return;
  }

  DesktopRect cropped_rect;
  DesktopRect original_rect;
  if (!GetWindowDrawableRect(window_, &cropped_rect, &original_rect)) {
    RTC_LOG(LS_WARNING) << "Failed to get drawable window area: "
                        << GetLastError();
    callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
    return;
  }

  // Return a 1x1 black frame if the window is minimized or invisible on current
  // desktop, to match behavior on mace. Window can be temporarily invisible
  // during the transition of full screen mode on/off.
  if (original_rect.is_empty() ||
      !window_capture_helper_.IsWindowVisibleOnCurrentDesktop(window_)) {
    std::unique_ptr<DesktopFrame> frame(
        new BasicDesktopFrame(DesktopSize(1, 1)));
    memset(frame->data(), 0, frame->stride() * frame->size().height());

    previous_size_ = frame->size();
    window_size_map_[window_] = previous_size_;
    callback_->OnCaptureResult(Result::SUCCESS, std::move(frame));
    return;
  }

  HDC window_dc = GetWindowDC(window_);
  if (!window_dc) {
    RTC_LOG(LS_WARNING) << "Failed to get window DC: " << GetLastError();
    callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
    return;
  }

  DesktopSize window_dc_size;
  if (GetDcSize(window_dc, &window_dc_size)) {
    // The |window_dc_size| is used to detect the scaling of the original
    // window. If the application does not support high-DPI settings, it will
    // be scaled by Windows according to the scaling setting.
    // https://www.google.com/search?q=windows+scaling+settings&ie=UTF-8
    // So the size of the |window_dc|, i.e. the bitmap we can retrieve from
    // PrintWindow() or BitBlt() function, will be smaller than
    // |original_rect| and |cropped_rect|. Part of the captured desktop frame
    // will be black. See
    // bug https://bugs.chromium.org/p/webrtc/issues/detail?id=8112 for
    // details.

    // If |window_dc_size| is smaller than |window_rect|, let's resize both
    // |original_rect| and |cropped_rect| according to the scaling factor.
    const double vertical_scale =
        static_cast<double>(window_dc_size.width()) / original_rect.width();
    const double horizontal_scale =
        static_cast<double>(window_dc_size.height()) / original_rect.height();
    original_rect.Scale(vertical_scale, horizontal_scale);
    cropped_rect.Scale(vertical_scale, horizontal_scale);
  }

  std::unique_ptr<DesktopFrameWin> frame(
      DesktopFrameWin::Create(cropped_rect.size(), nullptr, window_dc));
  if (!frame.get()) {
    RTC_LOG(LS_WARNING) << "Failed to create frame.";
    ReleaseDC(window_, window_dc);
    callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
    return;
  }

  HDC mem_dc = CreateCompatibleDC(window_dc);
  HGDIOBJ previous_object = SelectObject(mem_dc, frame->bitmap());
  BOOL result = FALSE;

  // When desktop composition (Aero) is enabled each window is rendered to a
  // private buffer allowing BitBlt() to get the window content even if the
  // window is occluded. PrintWindow() is slower but lets rendering the window
  // contents to an off-screen device context when Aero is not available.
  // PrintWindow() is not supported by some applications.
  //
  // If Aero is enabled, we prefer BitBlt() because it's faster and avoids
  // window flickering. Otherwise, we prefer PrintWindow() because BitBlt() may
  // render occluding windows on top of the desired window.
  //
  // When composition is enabled the DC returned by GetWindowDC() doesn't always
  // have window frame rendered correctly. Windows renders it only once and then
  // caches the result between captures. We hack it around by calling
  // PrintWindow() whenever window size changes, including the first time of
  // capturing - it somehow affects what we get from BitBlt() on the subsequent
  // captures.

  if (!window_capture_helper_.IsAeroEnabled() ||
      !previous_size_.equals(frame->size())) {
    result = PrintWindow(window_, mem_dc, 0);
  }

  // Aero is enabled or PrintWindow() failed, use BitBlt.
  if (!result) {
    result = BitBlt(mem_dc, 0, 0, frame->size().width(), frame->size().height(),
                    window_dc, cropped_rect.left() - original_rect.left(),
                    cropped_rect.top() - original_rect.top(), SRCCOPY);
  }

  SelectObject(mem_dc, previous_object);
  DeleteDC(mem_dc);
  ReleaseDC(window_, window_dc);

  previous_size_ = frame->size();
  window_size_map_[window_] = previous_size_;

  frame->mutable_updated_region()->SetRect(
      DesktopRect::MakeSize(frame->size()));
  frame->set_top_left(
      cropped_rect.top_left().subtract(GetFullscreenRect().top_left()));

  if (result) {
    callback_->OnCaptureResult(Result::SUCCESS, std::move(frame));
  } else {
    RTC_LOG(LS_ERROR) << "Both PrintWindow() and BitBlt() failed.";
    callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
  }
}

}  // namespace

// static
std::unique_ptr<DesktopCapturer> DesktopCapturer::CreateRawWindowCapturer(
    const DesktopCaptureOptions& options) {
  return std::unique_ptr<DesktopCapturer>(new WindowCapturerWin());
}

}  // namespace webrtc
