/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <string>
#include <utility>

#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/desktop_geometry.h"
#include "rtc_base/checks.h"
#include "test/gtest.h"

namespace webrtc {

class WindowCapturerTest : public ::testing::Test,
                           public DesktopCapturer::Callback {
 public:
  void SetUp() override {
    capturer_ = DesktopCapturer::CreateWindowCapturer(
        DesktopCaptureOptions::CreateDefault());
    ASSERT_TRUE(capturer_);
  }

  void TearDown() override {}

  // DesktopCapturer::Callback interface
  void OnCaptureResult(DesktopCapturer::Result result,
                       std::unique_ptr<DesktopFrame> frame) override {
    frame_ = std::move(frame);
  }

 protected:
  std::unique_ptr<DesktopCapturer> capturer_;
  std::unique_ptr<DesktopFrame> frame_;
};

// Verify that we can enumerate windows.
// TODO(bugs.webrtc.org/12950): Re-enable when libc++ issue is fixed
#if defined(WEBRTC_LINUX) && defined(MEMORY_SANITIZER)
#define MAYBE_Enumerate DISABLED_Enumerate
#else
#define MAYBE_Enumerate Enumerate
#endif
TEST_F(WindowCapturerTest, MAYBE_Enumerate) {
  DesktopCapturer::SourceList sources;
  EXPECT_TRUE(capturer_->GetSourceList(&sources));

  // Verify that window titles are set.
  for (auto it = sources.begin(); it != sources.end(); ++it) {
    EXPECT_FALSE(it->title.empty());
  }
}

// Flaky on Linux. See: crbug.com/webrtc/7830.
// Failing on macOS 11: See bugs.webrtc.org/12801
#if defined(WEBRTC_LINUX) || defined(WEBRTC_MAC)
#define MAYBE_Capture DISABLED_Capture
#else
#define MAYBE_Capture Capture
#endif
// Verify we can capture a window.
//
// TODO(sergeyu): Currently this test just looks at the windows that already
// exist. Ideally it should create a test window and capture from it, but there
// is no easy cross-platform way to create new windows (potentially we could
// have a python script showing Tk dialog, but launching code will differ
// between platforms).
TEST_F(WindowCapturerTest, MAYBE_Capture) {
  DesktopCapturer::SourceList sources;
  capturer_->Start(this);
  EXPECT_TRUE(capturer_->GetSourceList(&sources));

  // Verify that we can select and capture each window.
  for (auto it = sources.begin(); it != sources.end(); ++it) {
    frame_.reset();
    if (capturer_->SelectSource(it->id)) {
      capturer_->CaptureFrame();
    }

    // If we failed to capture a window make sure it no longer exists.
    if (!frame_.get()) {
      DesktopCapturer::SourceList new_list;
      EXPECT_TRUE(capturer_->GetSourceList(&new_list));
      for (auto new_list_it = new_list.begin(); new_list_it != new_list.end();
           ++new_list_it) {
        EXPECT_FALSE(it->id == new_list_it->id);
      }
      continue;
    }

    EXPECT_GT(frame_->size().width(), 0);
    EXPECT_GT(frame_->size().height(), 0);
  }
}

}  // namespace webrtc
