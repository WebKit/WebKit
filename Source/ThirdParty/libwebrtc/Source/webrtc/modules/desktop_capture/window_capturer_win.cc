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

#include "modules/desktop_capture/cropped_desktop_frame.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_frame_win.h"
#include "modules/desktop_capture/win/screen_capture_utils.h"
#include "modules/desktop_capture/win/selected_window_context.h"
#include "modules/desktop_capture/win/window_capture_utils.h"
#include "modules/desktop_capture/window_finder_win.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/logging.h"
#include "rtc_base/string_utils.h"
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
  // Skip unresponsive windows. Set timout with 50ms, in case system is under
  // heavy load, the check can wait longer but wont' be too long to delay the
  // the enumeration.
  const UINT uTimeout = 50;  // ms
  if (!SendMessageTimeout(hwnd, WM_NULL, 0, 0, SMTO_ABORTIFHUNG, uTimeout,
                          nullptr)) {
    return TRUE;
  }

  // Skip the Program Manager window and the Start button.
  const size_t kClassLength = 256;
  WCHAR class_name[kClassLength];
  const int class_name_length = GetClassNameW(hwnd, class_name, kClassLength);
  if (class_name_length < 1)
    return TRUE;

  // Skip Program Manager window and the Start button. This is the same logic
  // that's used in Win32WindowPicker in libjingle. Consider filtering other
  // windows as well (e.g. toolbars).
  if (wcscmp(class_name, L"Progman") == 0 || wcscmp(class_name, L"Button") == 0)
    return TRUE;

  DesktopCapturer::Source window;
  window.id = reinterpret_cast<WindowId>(hwnd);

  const size_t kTitleLength = 500;
  WCHAR window_title[kTitleLength];
  // Truncate the title if it's longer than kTitleLength.
  GetWindowTextW(hwnd, window_title, kTitleLength);
  window.title = rtc::ToUtf8(window_title);

  // Skip windows when we failed to convert the title or it is empty.
  if (window.title.empty())
    return TRUE;

  list->push_back(window);

  return TRUE;
}

// Used to pass input/output data during the EnumWindows call to collect
// owned/pop-up windows that should be captured.
struct OwnedWindowCollectorContext : public SelectedWindowContext {
  OwnedWindowCollectorContext(HWND selected_window,
                              DesktopRect selected_window_rect,
                              WindowCaptureHelperWin* window_capture_helper,
                              std::vector<HWND>* owned_windows)
      : SelectedWindowContext(selected_window,
                              selected_window_rect,
                              window_capture_helper),
        owned_windows(owned_windows) {}

  std::vector<HWND>* owned_windows;
};

// Called via EnumWindows for each root window; adds owned/pop-up windows that
// should be captured to a vector it's passed.
BOOL CALLBACK OwnedWindowCollector(HWND hwnd, LPARAM param) {
  OwnedWindowCollectorContext* context =
      reinterpret_cast<OwnedWindowCollectorContext*>(param);
  if (hwnd == context->selected_window()) {
    // Windows are enumerated in top-down z-order, so we can stop enumerating
    // upon reaching the selected window.
    return FALSE;
  }

  // Skip windows that aren't visible pop-up windows.
  if (!(GetWindowLong(hwnd, GWL_STYLE) & WS_POPUP) ||
      !context->window_capture_helper()->IsWindowVisibleOnCurrentDesktop(
          hwnd)) {
    return TRUE;
  }

  // Owned windows that intersect the selected window should be captured.
  if (context->IsWindowOwnedBySelectedWindow(hwnd) &&
      context->IsWindowOverlappingSelectedWindow(hwnd)) {
    // Skip windows that draw shadows around menus. These "SysShadow" windows
    // would otherwise be captured as solid black bars with no transparency
    // gradient (since this capturer doesn't detect / respect variations in the
    // window alpha channel). Any other semi-transparent owned windows will be
    // captured fully-opaque. This seems preferable to excluding them (at least
    // when they have content aside from a solid fill color / visual adornment;
    // e.g. some tooltips have the transparent style set).
    if (GetWindowLong(hwnd, GWL_EXSTYLE) & WS_EX_TRANSPARENT) {
      const WCHAR kSysShadow[] = L"SysShadow";
      const size_t kClassLength = arraysize(kSysShadow);
      WCHAR class_name[kClassLength];
      const int class_name_length =
          GetClassNameW(hwnd, class_name, kClassLength);
      if (class_name_length == kClassLength - 1 &&
          wcscmp(class_name, kSysShadow) == 0) {
        return TRUE;
      }
    }

    context->owned_windows->push_back(hwnd);
  }

  return TRUE;
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
  struct CaptureResults {
    Result result;
    std::unique_ptr<DesktopFrame> frame;
  };

  CaptureResults CaptureFrame(bool capture_owned_windows);

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

  std::vector<HWND> owned_windows_;
  std::unique_ptr<WindowCapturerWin> owned_window_capturer_;

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
    if (!window_capture_helper_.IsWindowVisibleOnCurrentDesktop(
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
  HWND hwnd =
      reinterpret_cast<HWND>(window_finder_.GetWindowUnderPoint(sys_pos));

  return hwnd != window_ &&
         std::find(owned_windows_.begin(), owned_windows_.end(), hwnd) ==
             owned_windows_.end();
}

void WindowCapturerWin::Start(Callback* callback) {
  assert(!callback_);
  assert(callback);

  callback_ = callback;
}

void WindowCapturerWin::CaptureFrame() {
  CaptureResults results = CaptureFrame(/*capture_owned_windows*/ true);

  callback_->OnCaptureResult(results.result, std::move(results.frame));
}

WindowCapturerWin::CaptureResults WindowCapturerWin::CaptureFrame(
    bool capture_owned_windows) {
  TRACE_EVENT0("webrtc", "WindowCapturerWin::CaptureFrame");

  if (!window_) {
    RTC_LOG(LS_ERROR) << "Window hasn't been selected: " << GetLastError();
    return {Result::ERROR_PERMANENT, nullptr};
  }

  // Stop capturing if the window has been closed.
  if (!IsWindow(window_)) {
    RTC_LOG(LS_ERROR) << "target window has been closed";
    return {Result::ERROR_PERMANENT, nullptr};
  }

  // Determine the window region excluding any resize border, and including
  // any visible border if capturing an owned window / dialog. (Don't include
  // any visible border for the selected window for consistency with
  // CroppingWindowCapturerWin, which would expose a bit of the background
  // through the partially-transparent border.)
  const bool avoid_cropping_border = !capture_owned_windows;
  DesktopRect cropped_rect;
  DesktopRect original_rect;

  if (!GetCroppedWindowRect(window_, avoid_cropping_border, &cropped_rect,
                            &original_rect)) {
    RTC_LOG(LS_WARNING) << "Failed to get drawable window area: "
                        << GetLastError();
    return {Result::ERROR_TEMPORARY, nullptr};
  }

  // Return a 1x1 black frame if the window is minimized or invisible on current
  // desktop, to match behavior on mace. Window can be temporarily invisible
  // during the transition of full screen mode on/off.
  if (original_rect.is_empty() ||
      !window_capture_helper_.IsWindowVisibleOnCurrentDesktop(window_)) {
    std::unique_ptr<DesktopFrame> frame(
        new BasicDesktopFrame(DesktopSize(1, 1)));

    previous_size_ = frame->size();
    window_size_map_[window_] = previous_size_;
    return {Result::SUCCESS, std::move(frame)};
  }

  HDC window_dc = GetWindowDC(window_);
  if (!window_dc) {
    RTC_LOG(LS_WARNING) << "Failed to get window DC: " << GetLastError();
    return {Result::ERROR_TEMPORARY, nullptr};
  }

  DesktopRect unscaled_cropped_rect = cropped_rect;
  double horizontal_scale = 1.0;
  double vertical_scale = 1.0;

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
    horizontal_scale =
        static_cast<double>(window_dc_size.width()) / original_rect.width();
    vertical_scale =
        static_cast<double>(window_dc_size.height()) / original_rect.height();
    original_rect.Scale(horizontal_scale, vertical_scale);
    cropped_rect.Scale(horizontal_scale, vertical_scale);
  }

  std::unique_ptr<DesktopFrameWin> frame(
      DesktopFrameWin::Create(original_rect.size(), nullptr, window_dc));
  if (!frame.get()) {
    RTC_LOG(LS_WARNING) << "Failed to create frame.";
    ReleaseDC(window_, window_dc);
    return {Result::ERROR_TEMPORARY, nullptr};
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
  //
  // For Windows 8.1 and later, we want to always use PrintWindow when the
  // cropping screen capturer falls back to the window capturer. I.e.
  // on Windows 8.1 and later, PrintWindow is only used when the window is
  // occluded. When the window is not occluded, it is much faster to capture
  // the screen and to crop it to the window position and size.
  if (rtc::IsWindows8OrLater()) {
    // Special flag that makes PrintWindow to work on Windows 8.1 and later.
    // Indeed certain apps (e.g. those using DirectComposition rendering) can't
    // be captured using BitBlt or PrintWindow without this flag. Note that on
    // Windows 8.0 this flag is not supported so the block below will fallback
    // to the other call to PrintWindow. It seems to be very tricky to detect
    // Windows 8.0 vs 8.1 so a try/fallback is more approriate here.
    const UINT flags = PW_RENDERFULLCONTENT;
    result = PrintWindow(window_, mem_dc, flags);
  }

  if (!result && (!window_capture_helper_.IsAeroEnabled() ||
                  !previous_size_.equals(frame->size()))) {
    result = PrintWindow(window_, mem_dc, 0);
  }

  // Aero is enabled or PrintWindow() failed, use BitBlt.
  if (!result) {
    result = BitBlt(mem_dc, 0, 0, frame->size().width(), frame->size().height(),
                    window_dc, 0, 0, SRCCOPY);
  }

  SelectObject(mem_dc, previous_object);
  DeleteDC(mem_dc);
  ReleaseDC(window_, window_dc);

  previous_size_ = frame->size();
  window_size_map_[window_] = previous_size_;

  frame->mutable_updated_region()->SetRect(
      DesktopRect::MakeSize(frame->size()));
  frame->set_top_left(
      original_rect.top_left().subtract(GetFullscreenRect().top_left()));

  if (!result) {
    RTC_LOG(LS_ERROR) << "Both PrintWindow() and BitBlt() failed.";
    return {Result::ERROR_TEMPORARY, nullptr};
  }

  // Rect for the data is relative to the first pixel of the frame.
  cropped_rect.Translate(-original_rect.left(), -original_rect.top());
  std::unique_ptr<DesktopFrame> cropped_frame =
      CreateCroppedDesktopFrame(std::move(frame), cropped_rect);
  RTC_DCHECK(cropped_frame);

  if (capture_owned_windows) {
    // If any owned/pop-up windows overlap the selected window, capture them
    // and copy/composite their contents into the frame.
    owned_windows_.clear();
    OwnedWindowCollectorContext context(window_, unscaled_cropped_rect,
                                        &window_capture_helper_,
                                        &owned_windows_);

    if (context.IsSelectedWindowValid()) {
      EnumWindows(OwnedWindowCollector, reinterpret_cast<LPARAM>(&context));

      if (!owned_windows_.empty()) {
        if (!owned_window_capturer_) {
          owned_window_capturer_ = std::make_unique<WindowCapturerWin>();
        }

        // Owned windows are stored in top-down z-order, so this iterates in
        // reverse to capture / draw them in bottom-up z-order
        for (auto it = owned_windows_.rbegin(); it != owned_windows_.rend();
             it++) {
          HWND hwnd = *it;
          if (owned_window_capturer_->SelectSource(
                  reinterpret_cast<SourceId>(hwnd))) {
            CaptureResults results = owned_window_capturer_->CaptureFrame(
                /*capture_owned_windows*/ false);

            if (results.result != DesktopCapturer::Result::SUCCESS) {
              // Simply log any error capturing an owned/pop-up window without
              // bubbling it up to the caller (an expected error here is that
              // the owned/pop-up window was closed; any unexpected errors won't
              // fail the outer capture).
              RTC_LOG(LS_INFO) << "Capturing owned window failed (previous "
                                  "error/warning pertained to that)";
            } else {
              // Copy / composite the captured frame into the outer frame. This
              // may no-op if they no longer intersect (if the owned window was
              // moved outside the owner bounds since scheduled for capture.)
              cropped_frame->CopyIntersectingPixelsFrom(
                  *results.frame, horizontal_scale, vertical_scale);
            }
          }
        }
      }
    }
  }

  return {Result::SUCCESS, std::move(cropped_frame)};
}

}  // namespace

// static
std::unique_ptr<DesktopCapturer> DesktopCapturer::CreateRawWindowCapturer(
    const DesktopCaptureOptions& options) {
  return std::unique_ptr<DesktopCapturer>(new WindowCapturerWin());
}

}  // namespace webrtc
