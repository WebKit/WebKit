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

#include "webrtc/modules/desktop_capture/desktop_and_cursor_composer.h"
#include "webrtc/modules/desktop_capture/desktop_capturer.h"
#include "webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "webrtc/modules/desktop_capture/desktop_frame.h"
#include "webrtc/modules/desktop_capture/mouse_cursor.h"
#include "webrtc/modules/desktop_capture/shared_desktop_frame.h"
#include "webrtc/system_wrappers/include/logging.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

namespace {

const int kScreenWidth = 100;
const int kScreenHeight = 100;
const int kCursorWidth = 10;
const int kCursorHeight = 10;

const int kTestCursorSize = 3;
const uint32_t kTestCursorData[kTestCursorSize][kTestCursorSize] = {
  { 0xffffffff, 0x99990000, 0xaa222222, },
  { 0x88008800, 0xaa0000aa, 0xaa333333, },
  { 0x00000000, 0xaa0000aa, 0xaa333333, },
};

uint32_t GetFakeFramePixelValue(const DesktopVector& p) {
  uint32_t r = 100 + p.x();
  uint32_t g = 100 + p.y();
  uint32_t b = 100 + p.x() + p.y();
  return b + (g << 8) + (r << 16) + 0xff000000;
}

uint32_t GetFramePixel(const DesktopFrame& frame, const DesktopVector& pos) {
  return *reinterpret_cast<uint32_t*>(frame.data() + pos.y() * frame.stride() +
                                      pos.x() * DesktopFrame::kBytesPerPixel);
}

// Blends two pixel values taking into account alpha.
uint32_t BlendPixels(uint32_t dest, uint32_t src) {
  uint8_t alpha = 255 - ((src & 0xff000000) >> 24);
  uint32_t r =
      ((dest & 0x00ff0000) >> 16) * alpha / 255 + ((src & 0x00ff0000) >> 16);
  uint32_t g =
      ((dest & 0x0000ff00) >> 8) * alpha / 255 + ((src & 0x0000ff00) >> 8);
  uint32_t b = (dest & 0x000000ff) * alpha / 255 + (src & 0x000000ff);
  return b + (g << 8) + (r << 16) + 0xff000000;
}

DesktopFrame* CreateTestFrame() {
  DesktopFrame* frame =
      new BasicDesktopFrame(DesktopSize(kScreenWidth, kScreenHeight));
  uint32_t* data = reinterpret_cast<uint32_t*>(frame->data());
  for (int y = 0; y < kScreenHeight; ++y) {
    for (int x = 0; x < kScreenWidth; ++x) {
      *(data++) = GetFakeFramePixelValue(DesktopVector(x, y));
    }
  }
  return frame;
}

class FakeScreenCapturer : public DesktopCapturer {
 public:
  FakeScreenCapturer() {}

  void Start(Callback* callback) override { callback_ = callback; }

  void CaptureFrame() override {
    callback_->OnCaptureResult(
        next_frame_ ? Result::SUCCESS : Result::ERROR_TEMPORARY,
        std::move(next_frame_));
  }

  void SetNextFrame(std::unique_ptr<DesktopFrame> next_frame) {
    next_frame_ = std::move(next_frame);
  }

 private:
  Callback* callback_ = nullptr;

  std::unique_ptr<DesktopFrame> next_frame_;
};

class FakeMouseMonitor : public MouseCursorMonitor {
 public:
  FakeMouseMonitor() : changed_(true) {}

  void SetState(CursorState state, const DesktopVector& pos) {
    state_ = state;
    position_ = pos;
  }

  void SetHotspot(const DesktopVector& hotspot) {
    if (!hotspot_.equals(hotspot))
      changed_ = true;
    hotspot_ = hotspot;
  }

  void Init(Callback* callback, Mode mode) override { callback_ = callback; }

  void Capture() override {
    if (changed_) {
      std::unique_ptr<DesktopFrame> image(
          new BasicDesktopFrame(DesktopSize(kCursorWidth, kCursorHeight)));
      uint32_t* data = reinterpret_cast<uint32_t*>(image->data());
      memset(data, 0, image->stride() * kCursorHeight);

      // Set four pixels near the hotspot and leave all other blank.
      for (int y = 0; y < kTestCursorSize; ++y) {
        for (int x = 0; x < kTestCursorSize; ++x) {
          data[(hotspot_.y() + y) * kCursorWidth + (hotspot_.x() + x)] =
              kTestCursorData[y][x];
        }
      }

      callback_->OnMouseCursor(new MouseCursor(image.release(), hotspot_));
    }

    callback_->OnMouseCursorPosition(state_, position_);
  }

 private:
  Callback* callback_;
  CursorState state_;
  DesktopVector position_;
  DesktopVector hotspot_;
  bool changed_;
};

void VerifyFrame(const DesktopFrame& frame,
                 MouseCursorMonitor::CursorState state,
                 const DesktopVector& pos) {
  // Verify that all other pixels are set to their original values.
  DesktopRect image_rect =
      DesktopRect::MakeWH(kTestCursorSize, kTestCursorSize);
  image_rect.Translate(pos);

  for (int y = 0; y < kScreenHeight; ++y) {
    for (int x = 0; x < kScreenWidth; ++x) {
      DesktopVector p(x, y);
      if (state == MouseCursorMonitor::INSIDE && image_rect.Contains(p)) {
        EXPECT_EQ(BlendPixels(GetFakeFramePixelValue(p),
                              kTestCursorData[y - pos.y()][x - pos.x()]),
                  GetFramePixel(frame, p));
      } else {
        EXPECT_EQ(GetFakeFramePixelValue(p), GetFramePixel(frame, p));
      }
    }
  }
}

class DesktopAndCursorComposerTest : public testing::Test,
                                     public DesktopCapturer::Callback {
 public:
  DesktopAndCursorComposerTest()
      : fake_screen_(new FakeScreenCapturer()),
        fake_cursor_(new FakeMouseMonitor()),
        blender_(fake_screen_, fake_cursor_) {}

  // DesktopCapturer::Callback interface
  void OnCaptureResult(DesktopCapturer::Result result,
                       std::unique_ptr<DesktopFrame> frame) override {
    frame_ = std::move(frame);
  }

 protected:
  // Owned by |blender_|.
  FakeScreenCapturer* fake_screen_;
  FakeMouseMonitor* fake_cursor_;

  DesktopAndCursorComposer blender_;
  std::unique_ptr<DesktopFrame> frame_;
};

// Verify DesktopAndCursorComposer can handle the case when the screen capturer
// fails.
TEST_F(DesktopAndCursorComposerTest, Error) {
  blender_.Start(this);

  fake_cursor_->SetHotspot(DesktopVector());
  fake_cursor_->SetState(MouseCursorMonitor::INSIDE, DesktopVector());
  fake_screen_->SetNextFrame(nullptr);

  blender_.CaptureFrame();

  EXPECT_FALSE(frame_);
}

TEST_F(DesktopAndCursorComposerTest, Blend) {
  struct {
    int x, y;
    int hotspot_x, hotspot_y;
    bool inside;
  } tests[] = {
    {0, 0, 0, 0, true},
    {50, 50, 0, 0, true},
    {100, 50, 0, 0, true},
    {50, 100, 0, 0, true},
    {100, 100, 0, 0, true},
    {0, 0, 2, 5, true},
    {1, 1, 2, 5, true},
    {50, 50, 2, 5, true},
    {100, 100, 2, 5, true},
    {0, 0, 5, 2, true},
    {50, 50, 5, 2, true},
    {100, 100, 5, 2, true},
    {0, 0, 0, 0, false},
  };

  blender_.Start(this);

  for (size_t i = 0; i < (sizeof(tests) / sizeof(tests[0])); ++i) {
    SCOPED_TRACE(i);

    DesktopVector hotspot(tests[i].hotspot_x, tests[i].hotspot_y);
    fake_cursor_->SetHotspot(hotspot);

    MouseCursorMonitor::CursorState state = tests[i].inside
                                                ? MouseCursorMonitor::INSIDE
                                                : MouseCursorMonitor::OUTSIDE;
    DesktopVector pos(tests[i].x, tests[i].y);
    fake_cursor_->SetState(state, pos);

    std::unique_ptr<SharedDesktopFrame> frame(
        SharedDesktopFrame::Wrap(CreateTestFrame()));
    fake_screen_->SetNextFrame(frame->Share());

    blender_.CaptureFrame();

    VerifyFrame(*frame_, state, pos);

    // Verify that the cursor is erased before the frame buffer is returned to
    // the screen capturer.
    frame_.reset();
    VerifyFrame(*frame, MouseCursorMonitor::OUTSIDE, DesktopVector());
  }
}

}  // namespace

}  // namespace webrtc
