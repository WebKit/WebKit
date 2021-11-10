/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/win/wgc_capturer_win.h"

#include <string>
#include <utility>
#include <vector>

#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capture_types.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/win/test_support/test_window.h"
#include "modules/desktop_capture/win/window_capture_utils.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/thread.h"
#include "rtc_base/time_utils.h"
#include "rtc_base/win/scoped_com_initializer.h"
#include "rtc_base/win/windows_version.h"
#include "system_wrappers/include/metrics.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

const char kWindowThreadName[] = "wgc_capturer_test_window_thread";
const WCHAR kWindowTitle[] = L"WGC Capturer Test Window";

const char kCapturerImplHistogram[] =
    "WebRTC.DesktopCapture.Win.DesktopCapturerImpl";

const char kCapturerResultHistogram[] =
    "WebRTC.DesktopCapture.Win.WgcCapturerResult";
const int kSuccess = 0;
const int kSessionStartFailure = 4;

const char kCaptureSessionResultHistogram[] =
    "WebRTC.DesktopCapture.Win.WgcCaptureSessionStartResult";
const int kSourceClosed = 1;

const char kCaptureTimeHistogram[] =
    "WebRTC.DesktopCapture.Win.WgcCapturerFrameTime";

const int kSmallWindowWidth = 200;
const int kSmallWindowHeight = 100;
const int kMediumWindowWidth = 300;
const int kMediumWindowHeight = 200;
const int kLargeWindowWidth = 400;
const int kLargeWindowHeight = 500;

// The size of the image we capture is slightly smaller than the actual size of
// the window.
const int kWindowWidthSubtrahend = 14;
const int kWindowHeightSubtrahend = 7;

// Custom message constants so we can direct our thread to close windows
// and quit running.
const UINT kNoOp = WM_APP;
const UINT kDestroyWindow = WM_APP + 1;
const UINT kQuitRunning = WM_APP + 2;

enum CaptureType { kWindowCapture = 0, kScreenCapture = 1 };

}  // namespace

class WgcCapturerWinTest : public ::testing::TestWithParam<CaptureType>,
                           public DesktopCapturer::Callback {
 public:
  void SetUp() override {
    if (rtc::rtc_win::GetVersion() < rtc::rtc_win::Version::VERSION_WIN10_RS5) {
      RTC_LOG(LS_INFO)
          << "Skipping WgcCapturerWinTests on Windows versions < RS5.";
      GTEST_SKIP();
    }

    com_initializer_ =
        std::make_unique<ScopedCOMInitializer>(ScopedCOMInitializer::kMTA);
    EXPECT_TRUE(com_initializer_->Succeeded());
  }

  void SetUpForWindowCapture(int window_width = kMediumWindowWidth,
                             int window_height = kMediumWindowHeight) {
    capturer_ = WgcCapturerWin::CreateRawWindowCapturer(
        DesktopCaptureOptions::CreateDefault());
    CreateWindowOnSeparateThread(window_width, window_height);
    StartWindowThreadMessageLoop();
    source_id_ = GetTestWindowIdFromSourceList();
  }

  void SetUpForScreenCapture() {
    capturer_ = WgcCapturerWin::CreateRawScreenCapturer(
        DesktopCaptureOptions::CreateDefault());
    source_id_ = GetScreenIdFromSourceList();
  }

  void TearDown() override {
    if (window_open_) {
      CloseTestWindow();
    }
  }

  // The window must live on a separate thread so that we can run a message pump
  // without blocking the test thread. This is necessary if we are interested in
  // having GraphicsCaptureItem events (i.e. the Closed event) fire, and it more
  // closely resembles how capture works in the wild.
  void CreateWindowOnSeparateThread(int window_width, int window_height) {
    window_thread_ = rtc::Thread::Create();
    window_thread_->SetName(kWindowThreadName, nullptr);
    window_thread_->Start();
    window_thread_->Invoke<void>(RTC_FROM_HERE, [this, window_width,
                                                 window_height]() {
      window_thread_id_ = GetCurrentThreadId();
      window_info_ =
          CreateTestWindow(kWindowTitle, window_height, window_width);
      window_open_ = true;

      while (!IsWindowResponding(window_info_.hwnd)) {
        RTC_LOG(LS_INFO) << "Waiting for test window to become responsive in "
                            "WgcWindowCaptureTest.";
      }

      while (!IsWindowValidAndVisible(window_info_.hwnd)) {
        RTC_LOG(LS_INFO) << "Waiting for test window to be visible in "
                            "WgcWindowCaptureTest.";
      }
    });

    ASSERT_TRUE(window_thread_->RunningForTest());
    ASSERT_FALSE(window_thread_->IsCurrent());
  }

  void StartWindowThreadMessageLoop() {
    window_thread_->PostTask(RTC_FROM_HERE, [this]() {
      MSG msg;
      BOOL gm;
      while ((gm = ::GetMessage(&msg, NULL, 0, 0)) != 0 && gm != -1) {
        ::DispatchMessage(&msg);
        if (msg.message == kDestroyWindow) {
          DestroyTestWindow(window_info_);
        }
        if (msg.message == kQuitRunning) {
          PostQuitMessage(0);
        }
      }
    });
  }

  void CloseTestWindow() {
    ::PostThreadMessage(window_thread_id_, kDestroyWindow, 0, 0);
    ::PostThreadMessage(window_thread_id_, kQuitRunning, 0, 0);
    window_thread_->Stop();
    window_open_ = false;
  }

  DesktopCapturer::SourceId GetTestWindowIdFromSourceList() {
    // Frequently, the test window will not show up in GetSourceList because it
    // was created too recently. Since we are confident the window will be found
    // eventually we loop here until we find it.
    intptr_t src_id;
    do {
      DesktopCapturer::SourceList sources;
      EXPECT_TRUE(capturer_->GetSourceList(&sources));

      auto it = std::find_if(
          sources.begin(), sources.end(),
          [&](const DesktopCapturer::Source& src) {
            return src.id == reinterpret_cast<intptr_t>(window_info_.hwnd);
          });

      src_id = it->id;
    } while (src_id != reinterpret_cast<intptr_t>(window_info_.hwnd));

    return src_id;
  }

  DesktopCapturer::SourceId GetScreenIdFromSourceList() {
    DesktopCapturer::SourceList sources;
    EXPECT_TRUE(capturer_->GetSourceList(&sources));
    EXPECT_GT(sources.size(), 0ULL);
    return sources[0].id;
  }

  void DoCapture() {
    // Sometimes the first few frames are empty becaues the capture engine is
    // still starting up. We also may drop a few frames when the window is
    // resized or un-minimized.
    do {
      capturer_->CaptureFrame();
    } while (result_ == DesktopCapturer::Result::ERROR_TEMPORARY);

    EXPECT_EQ(result_, DesktopCapturer::Result::SUCCESS);
    EXPECT_TRUE(frame_);

    EXPECT_GT(metrics::NumEvents(kCapturerResultHistogram, kSuccess),
              successful_captures_);
    ++successful_captures_;
  }

  void ValidateFrame(int expected_width, int expected_height) {
    EXPECT_EQ(frame_->size().width(), expected_width - kWindowWidthSubtrahend);
    EXPECT_EQ(frame_->size().height(),
              expected_height - kWindowHeightSubtrahend);

    // Verify the buffer contains as much data as it should, and that the right
    // colors are found.
    int data_length = frame_->stride() * frame_->size().height();

    // The first and last pixel should have the same color because they will be
    // from the border of the window.
    // Pixels have 4 bytes of data so the whole pixel needs a uint32_t to fit.
    uint32_t first_pixel = static_cast<uint32_t>(*frame_->data());
    uint32_t last_pixel = static_cast<uint32_t>(
        *(frame_->data() + data_length - DesktopFrame::kBytesPerPixel));
    EXPECT_EQ(first_pixel, last_pixel);

    // Let's also check a pixel from the middle of the content area, which the
    // TestWindow will paint a consistent color for us to verify.
    uint8_t* middle_pixel = frame_->data() + (data_length / 2);

    int sub_pixel_offset = DesktopFrame::kBytesPerPixel / 4;
    EXPECT_EQ(*middle_pixel, kTestWindowBValue);
    middle_pixel += sub_pixel_offset;
    EXPECT_EQ(*middle_pixel, kTestWindowGValue);
    middle_pixel += sub_pixel_offset;
    EXPECT_EQ(*middle_pixel, kTestWindowRValue);
    middle_pixel += sub_pixel_offset;

    // The window is opaque so we expect 0xFF for the Alpha channel.
    EXPECT_EQ(*middle_pixel, 0xFF);
  }

  // DesktopCapturer::Callback interface
  // The capturer synchronously invokes this method before `CaptureFrame()`
  // returns.
  void OnCaptureResult(DesktopCapturer::Result result,
                       std::unique_ptr<DesktopFrame> frame) override {
    result_ = result;
    frame_ = std::move(frame);
  }

 protected:
  std::unique_ptr<ScopedCOMInitializer> com_initializer_;
  DWORD window_thread_id_;
  std::unique_ptr<rtc::Thread> window_thread_;
  WindowInfo window_info_;
  intptr_t source_id_;
  bool window_open_ = false;
  DesktopCapturer::Result result_;
  int successful_captures_ = 0;
  std::unique_ptr<DesktopFrame> frame_;
  std::unique_ptr<DesktopCapturer> capturer_;
};

TEST_P(WgcCapturerWinTest, SelectValidSource) {
  if (GetParam() == CaptureType::kWindowCapture) {
    SetUpForWindowCapture();
  } else {
    SetUpForScreenCapture();
  }

  EXPECT_TRUE(capturer_->SelectSource(source_id_));
}

TEST_P(WgcCapturerWinTest, SelectInvalidSource) {
  if (GetParam() == CaptureType::kWindowCapture) {
    capturer_ = WgcCapturerWin::CreateRawWindowCapturer(
        DesktopCaptureOptions::CreateDefault());
    source_id_ = kNullWindowId;
  } else {
    capturer_ = WgcCapturerWin::CreateRawScreenCapturer(
        DesktopCaptureOptions::CreateDefault());
    source_id_ = kInvalidScreenId;
  }

  EXPECT_FALSE(capturer_->SelectSource(source_id_));
}

TEST_P(WgcCapturerWinTest, Capture) {
  if (GetParam() == CaptureType::kWindowCapture) {
    SetUpForWindowCapture();
  } else {
    SetUpForScreenCapture();
  }

  EXPECT_TRUE(capturer_->SelectSource(source_id_));

  capturer_->Start(this);
  EXPECT_GE(metrics::NumEvents(kCapturerImplHistogram,
                               DesktopCapturerId::kWgcCapturerWin),
            1);

  DoCapture();
  EXPECT_GT(frame_->size().width(), 0);
  EXPECT_GT(frame_->size().height(), 0);
}

TEST_P(WgcCapturerWinTest, CaptureTime) {
  if (GetParam() == CaptureType::kWindowCapture) {
    SetUpForWindowCapture();
  } else {
    SetUpForScreenCapture();
  }

  EXPECT_TRUE(capturer_->SelectSource(source_id_));
  capturer_->Start(this);

  int64_t start_time;
  do {
    start_time = rtc::TimeNanos();
    capturer_->CaptureFrame();
  } while (result_ == DesktopCapturer::Result::ERROR_TEMPORARY);

  int capture_time_ms =
      (rtc::TimeNanos() - start_time) / rtc::kNumNanosecsPerMillisec;
  EXPECT_TRUE(frame_);

  // The test may measure the time slightly differently than the capturer. So we
  // just check if it's within 5 ms.
  EXPECT_NEAR(frame_->capture_time_ms(), capture_time_ms, 5);
  EXPECT_GE(
      metrics::NumEvents(kCaptureTimeHistogram, frame_->capture_time_ms()), 1);
}

INSTANTIATE_TEST_SUITE_P(SourceAgnostic,
                         WgcCapturerWinTest,
                         ::testing::Values(CaptureType::kWindowCapture,
                                           CaptureType::kScreenCapture));

// Monitor specific tests.
TEST_F(WgcCapturerWinTest, FocusOnMonitor) {
  SetUpForScreenCapture();
  EXPECT_TRUE(capturer_->SelectSource(0));

  // You can't set focus on a monitor.
  EXPECT_FALSE(capturer_->FocusOnSelectedSource());
}

TEST_F(WgcCapturerWinTest, CaptureAllMonitors) {
  SetUpForScreenCapture();
  EXPECT_TRUE(capturer_->SelectSource(kFullDesktopScreenId));

  capturer_->Start(this);
  DoCapture();
  EXPECT_GT(frame_->size().width(), 0);
  EXPECT_GT(frame_->size().height(), 0);
}

// Window specific tests.
TEST_F(WgcCapturerWinTest, FocusOnWindow) {
  capturer_ = WgcCapturerWin::CreateRawWindowCapturer(
      DesktopCaptureOptions::CreateDefault());
  window_info_ = CreateTestWindow(kWindowTitle);
  source_id_ = GetScreenIdFromSourceList();

  EXPECT_TRUE(capturer_->SelectSource(source_id_));
  EXPECT_TRUE(capturer_->FocusOnSelectedSource());

  HWND hwnd = reinterpret_cast<HWND>(source_id_);
  EXPECT_EQ(hwnd, ::GetActiveWindow());
  EXPECT_EQ(hwnd, ::GetForegroundWindow());
  EXPECT_EQ(hwnd, ::GetFocus());
  DestroyTestWindow(window_info_);
}

TEST_F(WgcCapturerWinTest, SelectMinimizedWindow) {
  SetUpForWindowCapture();
  MinimizeTestWindow(reinterpret_cast<HWND>(source_id_));
  EXPECT_FALSE(capturer_->SelectSource(source_id_));

  UnminimizeTestWindow(reinterpret_cast<HWND>(source_id_));
  EXPECT_TRUE(capturer_->SelectSource(source_id_));
}

TEST_F(WgcCapturerWinTest, SelectClosedWindow) {
  SetUpForWindowCapture();
  EXPECT_TRUE(capturer_->SelectSource(source_id_));

  CloseTestWindow();
  EXPECT_FALSE(capturer_->SelectSource(source_id_));
}

TEST_F(WgcCapturerWinTest, UnsupportedWindowStyle) {
  // Create a window with the WS_EX_TOOLWINDOW style, which WGC does not
  // support.
  window_info_ = CreateTestWindow(kWindowTitle, kMediumWindowWidth,
                                  kMediumWindowHeight, WS_EX_TOOLWINDOW);
  capturer_ = WgcCapturerWin::CreateRawWindowCapturer(
      DesktopCaptureOptions::CreateDefault());
  DesktopCapturer::SourceList sources;
  EXPECT_TRUE(capturer_->GetSourceList(&sources));
  auto it = std::find_if(
      sources.begin(), sources.end(), [&](const DesktopCapturer::Source& src) {
        return src.id == reinterpret_cast<intptr_t>(window_info_.hwnd);
      });

  // We should not find the window, since we filter for unsupported styles.
  EXPECT_EQ(it, sources.end());
  DestroyTestWindow(window_info_);
}

TEST_F(WgcCapturerWinTest, IncreaseWindowSizeMidCapture) {
  SetUpForWindowCapture(kSmallWindowWidth, kSmallWindowHeight);
  EXPECT_TRUE(capturer_->SelectSource(source_id_));

  capturer_->Start(this);
  DoCapture();
  ValidateFrame(kSmallWindowWidth, kSmallWindowHeight);

  ResizeTestWindow(window_info_.hwnd, kSmallWindowWidth, kMediumWindowHeight);
  DoCapture();
  // We don't expect to see the new size until the next capture, as the frame
  // pool hadn't had a chance to resize yet to fit the new, larger image.
  DoCapture();
  ValidateFrame(kSmallWindowWidth, kMediumWindowHeight);

  ResizeTestWindow(window_info_.hwnd, kLargeWindowWidth, kMediumWindowHeight);
  DoCapture();
  DoCapture();
  ValidateFrame(kLargeWindowWidth, kMediumWindowHeight);
}

TEST_F(WgcCapturerWinTest, ReduceWindowSizeMidCapture) {
  SetUpForWindowCapture(kLargeWindowWidth, kLargeWindowHeight);
  EXPECT_TRUE(capturer_->SelectSource(source_id_));

  capturer_->Start(this);
  DoCapture();
  ValidateFrame(kLargeWindowWidth, kLargeWindowHeight);

  ResizeTestWindow(window_info_.hwnd, kLargeWindowWidth, kMediumWindowHeight);
  // We expect to see the new size immediately because the image data has shrunk
  // and will fit in the existing buffer.
  DoCapture();
  ValidateFrame(kLargeWindowWidth, kMediumWindowHeight);

  ResizeTestWindow(window_info_.hwnd, kSmallWindowWidth, kMediumWindowHeight);
  DoCapture();
  ValidateFrame(kSmallWindowWidth, kMediumWindowHeight);
}

TEST_F(WgcCapturerWinTest, MinimizeWindowMidCapture) {
  SetUpForWindowCapture();
  EXPECT_TRUE(capturer_->SelectSource(source_id_));

  capturer_->Start(this);

  // Minmize the window and capture should continue but return temporary errors.
  MinimizeTestWindow(window_info_.hwnd);
  for (int i = 0; i < 10; ++i) {
    capturer_->CaptureFrame();
    EXPECT_EQ(result_, DesktopCapturer::Result::ERROR_TEMPORARY);
  }

  // Reopen the window and the capture should continue normally.
  UnminimizeTestWindow(window_info_.hwnd);
  DoCapture();
  // We can't verify the window size here because the test window does not
  // repaint itself after it is unminimized, but capturing successfully is still
  // a good test.
}

TEST_F(WgcCapturerWinTest, CloseWindowMidCapture) {
  SetUpForWindowCapture();
  EXPECT_TRUE(capturer_->SelectSource(source_id_));

  capturer_->Start(this);
  DoCapture();
  ValidateFrame(kMediumWindowWidth, kMediumWindowHeight);

  CloseTestWindow();

  // We need to call GetMessage to trigger the Closed event and the capturer's
  // event handler for it. If we are too early and the Closed event hasn't
  // arrived yet we should keep trying until the capturer receives it and stops.
  auto* wgc_capturer = static_cast<WgcCapturerWin*>(capturer_.get());
  while (wgc_capturer->IsSourceBeingCaptured(source_id_)) {
    // Since the capturer handles the Closed message, there will be no message
    // for us and GetMessage will hang, unless we send ourselves a message
    // first.
    ::PostThreadMessage(GetCurrentThreadId(), kNoOp, 0, 0);
    MSG msg;
    ::GetMessage(&msg, NULL, 0, 0);
    ::DispatchMessage(&msg);
  }

  // Occasionally, one last frame will have made it into the frame pool before
  // the window closed. The first call will consume it, and in that case we need
  // to make one more call to CaptureFrame.
  capturer_->CaptureFrame();
  if (result_ == DesktopCapturer::Result::SUCCESS)
    capturer_->CaptureFrame();

  EXPECT_GE(metrics::NumEvents(kCapturerResultHistogram, kSessionStartFailure),
            1);
  EXPECT_GE(metrics::NumEvents(kCaptureSessionResultHistogram, kSourceClosed),
            1);
  EXPECT_EQ(result_, DesktopCapturer::Result::ERROR_PERMANENT);
}

}  // namespace webrtc
