/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>

#include <algorithm>
#include <initializer_list>
#include <memory>
#include <utility>

#include "webrtc/base/checks.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/base/logging.h"
#include "webrtc/modules/desktop_capture/desktop_capturer.h"
#include "webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "webrtc/modules/desktop_capture/desktop_frame.h"
#include "webrtc/modules/desktop_capture/desktop_region.h"
#include "webrtc/modules/desktop_capture/mock_desktop_capturer_callback.h"
#include "webrtc/modules/desktop_capture/rgba_color.h"
#include "webrtc/modules/desktop_capture/screen_drawer.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"

#if defined(WEBRTC_WIN)
#include "webrtc/modules/desktop_capture/win/screen_capturer_win_directx.h"
#endif  // defined(WEBRTC_WIN)

using ::testing::_;

namespace webrtc {

namespace {

ACTION_P(SaveUniquePtrArg, dest) {
  *dest = std::move(*arg1);
}

// Returns true if color in |rect| of |frame| is |color|.
bool ArePixelsColoredBy(const DesktopFrame& frame,
                        DesktopRect rect,
                        RgbaColor color,
                        bool may_partially_draw) {
  if (!may_partially_draw) {
    // updated_region() should cover the painted area.
    DesktopRegion updated_region(frame.updated_region());
    updated_region.IntersectWith(rect);
    if (!updated_region.Equals(DesktopRegion(rect))) {
      return false;
    }
  }

  // Color in the |rect| should be |color|.
  uint8_t* row = frame.GetFrameDataAtPos(rect.top_left());
  for (int i = 0; i < rect.height(); i++) {
    uint8_t* column = row;
    for (int j = 0; j < rect.width(); j++) {
      if (color != RgbaColor(column)) {
        return false;
      }
      column += DesktopFrame::kBytesPerPixel;
    }
    row += frame.stride();
  }
  return true;
}

}  // namespace

class ScreenCapturerIntegrationTest : public testing::Test {
 public:
  void SetUp() override {
    capturer_ = DesktopCapturer::CreateScreenCapturer(
        DesktopCaptureOptions::CreateDefault());
  }

 protected:
  void TestCaptureUpdatedRegion(
      std::initializer_list<DesktopCapturer*> capturers) {
    RTC_DCHECK(capturers.size() > 0);
    // A large enough area for the tests, which should be able to be fulfilled
    // by most systems.
    const int kTestArea = 512;
    const int kRectSize = 32;
    std::unique_ptr<ScreenDrawer> drawer = ScreenDrawer::Create();
    if (!drawer || drawer->DrawableRegion().is_empty()) {
      LOG(LS_WARNING) << "No ScreenDrawer implementation for current platform.";
      return;
    }
    if (drawer->DrawableRegion().width() < kTestArea ||
        drawer->DrawableRegion().height() < kTestArea) {
      LOG(LS_WARNING) << "ScreenDrawer::DrawableRegion() is too small for the "
                         "CaptureUpdatedRegion tests.";
      return;
    }

    for (DesktopCapturer* capturer : capturers) {
      capturer->Start(&callback_);
    }

    // Draw a set of |kRectSize| by |kRectSize| rectangles at (|i|, |i|), or
    // |i| by |i| rectangles at (|kRectSize|, |kRectSize|). One of (controlled
    // by |c|) its primary colors is |i|, and the other two are 0x7f. So we
    // won't draw a black or white rectangle.
    for (int c = 0; c < 3; c++) {
      // A fixed size rectangle.
      for (int i = 0; i < kTestArea - kRectSize; i += 16) {
        DesktopRect rect = DesktopRect::MakeXYWH(i, i, kRectSize, kRectSize);
        rect.Translate(drawer->DrawableRegion().top_left());
        RgbaColor color((c == 0 ? (i & 0xff) : 0x7f),
                        (c == 1 ? (i & 0xff) : 0x7f),
                        (c == 2 ? (i & 0xff) : 0x7f));
        TestCaptureOneFrame(capturers, drawer.get(), rect, color);
      }

      // A variable-size rectangle.
      for (int i = 0; i < kTestArea - kRectSize; i += 16) {
        DesktopRect rect = DesktopRect::MakeXYWH(kRectSize, kRectSize, i, i);
        rect.Translate(drawer->DrawableRegion().top_left());
        RgbaColor color((c == 0 ? (i & 0xff) : 0x7f),
                        (c == 1 ? (i & 0xff) : 0x7f),
                        (c == 2 ? (i & 0xff) : 0x7f));
        TestCaptureOneFrame(capturers, drawer.get(), rect, color);
      }
    }
  }

  void TestCaptureUpdatedRegion() {
    TestCaptureUpdatedRegion({capturer_.get()});
  }

#if defined(WEBRTC_WIN)
  // Enable allow_directx_capturer in DesktopCaptureOptions, but let
  // DesktopCapturer::CreateScreenCapturer() to decide whether a DirectX
  // capturer should be used.
  void MaybeCreateDirectxCapturer() {
    DesktopCaptureOptions options(DesktopCaptureOptions::CreateDefault());
    options.set_allow_directx_capturer(true);
    capturer_ = DesktopCapturer::CreateScreenCapturer(options);
  }

  bool CreateDirectxCapturer() {
    if (!ScreenCapturerWinDirectx::IsSupported()) {
      LOG(LS_WARNING) << "Directx capturer is not supported";
      return false;
    }

    MaybeCreateDirectxCapturer();
    return true;
  }

  void CreateMagnifierCapturer() {
    DesktopCaptureOptions options(DesktopCaptureOptions::CreateDefault());
    options.set_allow_use_magnification_api(true);
    capturer_ = DesktopCapturer::CreateScreenCapturer(options);
  }
#endif  // defined(WEBRTC_WIN)

  std::unique_ptr<DesktopCapturer> capturer_;
  MockDesktopCapturerCallback callback_;

 private:
  // Repeats capturing the frame by using |capturers| one-by-one for 600 times,
  // typically 30 seconds, until they succeeded captured a |color| rectangle at
  // |rect|. This function uses |drawer|->WaitForPendingDraws() between two
  // attempts to wait for the screen to update.
  void TestCaptureOneFrame(std::vector<DesktopCapturer*> capturers,
                           ScreenDrawer* drawer,
                           DesktopRect rect,
                           RgbaColor color) {
    const int wait_capture_round = 600;
    drawer->Clear();
    size_t succeeded_capturers = 0;
    for (int i = 0; i < wait_capture_round; i++) {
      drawer->DrawRectangle(rect, color);
      drawer->WaitForPendingDraws();
      for (size_t j = 0; j < capturers.size(); j++) {
        if (capturers[j] == nullptr) {
          // DesktopCapturer should return an empty updated_region() if no
          // update detected. So we won't test it again if it has captured
          // the rectangle we drew.
          continue;
        }
        std::unique_ptr<DesktopFrame> frame = CaptureFrame(capturers[j]);
        if (!frame) {
          // CaptureFrame() has triggered an assertion failure already, we
          // only need to return here.
          return;
        }

        if (ArePixelsColoredBy(
            *frame, rect, color, drawer->MayDrawIncompleteShapes())) {
          capturers[j] = nullptr;
          succeeded_capturers++;
        }
      }

      if (succeeded_capturers == capturers.size()) {
        break;
      }
    }

    ASSERT_EQ(succeeded_capturers, capturers.size());
  }

  // Expects |capturer| to successfully capture a frame, and returns it.
  std::unique_ptr<DesktopFrame> CaptureFrame(DesktopCapturer* capturer) {
    std::unique_ptr<DesktopFrame> frame;
    EXPECT_CALL(callback_,
                OnCaptureResultPtr(DesktopCapturer::Result::SUCCESS, _))
        .WillOnce(SaveUniquePtrArg(&frame));
    capturer->CaptureFrame();
    EXPECT_TRUE(frame);
    return frame;
  }
};

// Disabled because it's flaky.
// https://bugs.chromium.org/p/webrtc/issues/detail?id=6666
TEST_F(ScreenCapturerIntegrationTest, DISABLED_CaptureUpdatedRegion) {
  TestCaptureUpdatedRegion();
}

// Disabled because it's flaky.
// https://bugs.chromium.org/p/webrtc/issues/detail?id=6666
TEST_F(ScreenCapturerIntegrationTest, DISABLED_TwoCapturers) {
  std::unique_ptr<DesktopCapturer> capturer2 = std::move(capturer_);
  SetUp();
  TestCaptureUpdatedRegion({capturer_.get(), capturer2.get()});
}

#if defined(WEBRTC_WIN)

// Disabled because it's flaky.
// https://bugs.chromium.org/p/webrtc/issues/detail?id=6666
TEST_F(ScreenCapturerIntegrationTest,
       DISABLED_CaptureUpdatedRegionWithDirectxCapturer) {
  if (!CreateDirectxCapturer()) {
    return;
  }

  TestCaptureUpdatedRegion();
}

// Disabled because it's flaky.
// https://bugs.chromium.org/p/webrtc/issues/detail?id=6666
TEST_F(ScreenCapturerIntegrationTest, DISABLED_TwoDirectxCapturers) {
  if (!CreateDirectxCapturer()) {
    return;
  }

  std::unique_ptr<DesktopCapturer> capturer2 = std::move(capturer_);
  RTC_CHECK(CreateDirectxCapturer());
  TestCaptureUpdatedRegion({capturer_.get(), capturer2.get()});
}

// Disabled because it's flaky.
// https://bugs.chromium.org/p/webrtc/issues/detail?id=6666
TEST_F(ScreenCapturerIntegrationTest,
       DISABLED_CaptureUpdatedRegionWithMagnifierCapturer) {
  CreateMagnifierCapturer();
  TestCaptureUpdatedRegion();
}

// Disabled because it's flaky.
// https://bugs.chromium.org/p/webrtc/issues/detail?id=6666
TEST_F(ScreenCapturerIntegrationTest, DISABLED_TwoMagnifierCapturers) {
  CreateMagnifierCapturer();
  std::unique_ptr<DesktopCapturer> capturer2 = std::move(capturer_);
  CreateMagnifierCapturer();
  TestCaptureUpdatedRegion({capturer_.get(), capturer2.get()});
}

// Disabled because it's flaky.
// https://bugs.chromium.org/p/webrtc/issues/detail?id=6666
TEST_F(ScreenCapturerIntegrationTest,
       DISABLED_MaybeCaptureUpdatedRegionWithDirectxCapturer) {
  // Even DirectX capturer is not supported in current system, we should be able
  // to select a usable capturer.
  MaybeCreateDirectxCapturer();
  TestCaptureUpdatedRegion();
}

#endif  // defined(WEBRTC_WIN)

}  // namespace webrtc
