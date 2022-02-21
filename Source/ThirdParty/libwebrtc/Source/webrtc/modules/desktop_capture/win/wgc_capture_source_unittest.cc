/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/win/wgc_capture_source.h"

#include <windows.graphics.capture.h>
#include <wrl/client.h>

#include <utility>

#include "modules/desktop_capture/desktop_capture_types.h"
#include "modules/desktop_capture/desktop_geometry.h"
#include "modules/desktop_capture/win/screen_capture_utils.h"
#include "modules/desktop_capture/win/test_support/test_window.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/win/scoped_com_initializer.h"
#include "rtc_base/win/windows_version.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

const WCHAR kWindowTitle[] = L"WGC Capture Source Test Window";

const int kFirstXCoord = 25;
const int kFirstYCoord = 50;
const int kSecondXCoord = 50;
const int kSecondYCoord = 75;

enum SourceType { kWindowSource = 0, kScreenSource = 1 };

}  // namespace

class WgcCaptureSourceTest : public ::testing::TestWithParam<SourceType> {
 public:
  void SetUp() override {
    if (rtc::rtc_win::GetVersion() < rtc::rtc_win::Version::VERSION_WIN10_RS5) {
      RTC_LOG(LS_INFO)
          << "Skipping WgcCaptureSourceTests on Windows versions < RS5.";
      GTEST_SKIP();
    }

    com_initializer_ =
        std::make_unique<ScopedCOMInitializer>(ScopedCOMInitializer::kMTA);
    ASSERT_TRUE(com_initializer_->Succeeded());
  }

  void TearDown() override {
    if (window_open_) {
      DestroyTestWindow(window_info_);
    }
  }

  void SetUpForWindowSource() {
    window_info_ = CreateTestWindow(kWindowTitle);
    window_open_ = true;
    source_id_ = reinterpret_cast<DesktopCapturer::SourceId>(window_info_.hwnd);
    source_factory_ = std::make_unique<WgcWindowSourceFactory>();
  }

  void SetUpForScreenSource() {
    source_id_ = kFullDesktopScreenId;
    source_factory_ = std::make_unique<WgcScreenSourceFactory>();
  }

 protected:
  std::unique_ptr<ScopedCOMInitializer> com_initializer_;
  std::unique_ptr<WgcCaptureSourceFactory> source_factory_;
  std::unique_ptr<WgcCaptureSource> source_;
  DesktopCapturer::SourceId source_id_;
  WindowInfo window_info_;
  bool window_open_ = false;
};

// Window specific test
TEST_F(WgcCaptureSourceTest, WindowPosition) {
  SetUpForWindowSource();
  source_ = source_factory_->CreateCaptureSource(source_id_);
  ASSERT_TRUE(source_);
  EXPECT_EQ(source_->GetSourceId(), source_id_);

  MoveTestWindow(window_info_.hwnd, kFirstXCoord, kFirstYCoord);
  DesktopVector source_vector = source_->GetTopLeft();
  EXPECT_EQ(source_vector.x(), kFirstXCoord);
  EXPECT_EQ(source_vector.y(), kFirstYCoord);

  MoveTestWindow(window_info_.hwnd, kSecondXCoord, kSecondYCoord);
  source_vector = source_->GetTopLeft();
  EXPECT_EQ(source_vector.x(), kSecondXCoord);
  EXPECT_EQ(source_vector.y(), kSecondYCoord);
}

// Screen specific test
TEST_F(WgcCaptureSourceTest, ScreenPosition) {
  SetUpForScreenSource();
  source_ = source_factory_->CreateCaptureSource(source_id_);
  ASSERT_TRUE(source_);
  EXPECT_EQ(source_id_, source_->GetSourceId());

  DesktopRect screen_rect = GetFullscreenRect();
  DesktopVector source_vector = source_->GetTopLeft();
  EXPECT_EQ(source_vector.x(), screen_rect.left());
  EXPECT_EQ(source_vector.y(), screen_rect.top());
}

// Source agnostic test
TEST_P(WgcCaptureSourceTest, CreateSource) {
  if (GetParam() == SourceType::kWindowSource) {
    SetUpForWindowSource();
  } else {
    SetUpForScreenSource();
  }

  source_ = source_factory_->CreateCaptureSource(source_id_);
  ASSERT_TRUE(source_);
  EXPECT_EQ(source_id_, source_->GetSourceId());
  EXPECT_TRUE(source_->IsCapturable());

  Microsoft::WRL::ComPtr<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>
      item;
  EXPECT_TRUE(SUCCEEDED(source_->GetCaptureItem(&item)));
  EXPECT_TRUE(item);
}

INSTANTIATE_TEST_SUITE_P(SourceAgnostic,
                         WgcCaptureSourceTest,
                         ::testing::Values(SourceType::kWindowSource,
                                           SourceType::kScreenSource));

}  // namespace webrtc
