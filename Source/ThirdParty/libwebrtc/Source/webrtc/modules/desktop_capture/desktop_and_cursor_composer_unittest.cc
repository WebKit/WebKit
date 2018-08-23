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

#include "modules/desktop_capture/desktop_and_cursor_composer.h"
#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/mouse_cursor.h"
#include "modules/desktop_capture/shared_desktop_frame.h"
#include "rtc_base/arraysize.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

const int kScreenWidth = 100;
const int kScreenHeight = 100;
const int kCursorWidth = 10;
const int kCursorHeight = 10;

const int kTestCursorSize = 3;
const uint32_t kTestCursorData[kTestCursorSize][kTestCursorSize] = {
    {
        0xffffffff, 0x99990000, 0xaa222222,
    },
    {
        0x88008800, 0xaa0000aa, 0xaa333333,
    },
    {
        0x00000000, 0xaa0000aa, 0xaa333333,
    },
};

uint32_t GetFakeFramePixelValue(const DesktopVector& p) {
  uint32_t r = 100 + p.x();
  uint32_t g = 100 + p.y();
  uint32_t b = 100 + p.x() + p.y();
  return b + (g << 8) + (r << 16) + 0xff000000;
}

uint32_t GetFramePixel(const DesktopFrame& frame, const DesktopVector& pos) {
  return *reinterpret_cast<uint32_t*>(frame.GetFrameDataAtPos(pos));
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

  bool IsOccluded(const DesktopVector& pos) override { return is_occluded_; }

  void set_is_occluded(bool value) { is_occluded_ = value; }

 private:
  Callback* callback_ = nullptr;

  std::unique_ptr<DesktopFrame> next_frame_;
  bool is_occluded_ = false;
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

    callback_->OnMouseCursorPosition(position_);
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

}  // namespace

class DesktopAndCursorComposerTest : public testing::Test,
                                     public DesktopCapturer::Callback {
 public:
  DesktopAndCursorComposerTest()
      : fake_screen_(new FakeScreenCapturer()),
        fake_cursor_(new FakeMouseMonitor()),
        blender_(fake_screen_, fake_cursor_) {
    blender_.Start(this);
  }

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

TEST_F(DesktopAndCursorComposerTest, CursorShouldBeIgnoredIfNoFrameCaptured) {
  struct {
    int x, y;
    int hotspot_x, hotspot_y;
    bool inside;
  } tests[] = {
      {0, 0, 0, 0, true},    {50, 50, 0, 0, true},   {100, 50, 0, 0, true},
      {50, 100, 0, 0, true}, {100, 100, 0, 0, true}, {0, 0, 2, 5, true},
      {1, 1, 2, 5, true},    {50, 50, 2, 5, true},   {100, 100, 2, 5, true},
      {0, 0, 5, 2, true},    {50, 50, 5, 2, true},   {100, 100, 5, 2, true},
      {0, 0, 0, 0, false},
  };

  for (size_t i = 0; i < arraysize(tests); i++) {
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

    blender_.CaptureFrame();
    // If capturer captured nothing, then cursor should be ignored, not matter
    // its state or position.
    EXPECT_EQ(frame_, nullptr);
  }
}

TEST_F(DesktopAndCursorComposerTest,
       CursorShouldBeIgnoredIfItIsOutOfDesktopFrame) {
  std::unique_ptr<SharedDesktopFrame> frame(
      SharedDesktopFrame::Wrap(CreateTestFrame()));
  frame->set_top_left(DesktopVector(100, 200));
  // The frame covers (100, 200) - (200, 300).

  struct {
    int x;
    int y;
  } tests[] = {
      {0, 0},    {50, 50},         {50, 150},      {100, 150}, {50, 200},
      {99, 200}, {100, 199},       {200, 300},     {200, 299}, {199, 300},
      {-1, -1},  {-10000, -10000}, {10000, 10000},
  };
  for (size_t i = 0; i < arraysize(tests); i++) {
    SCOPED_TRACE(i);

    fake_screen_->SetNextFrame(frame->Share());
    // The CursorState is ignored when using absolute cursor position.
    fake_cursor_->SetState(MouseCursorMonitor::OUTSIDE,
                           DesktopVector(tests[i].x, tests[i].y));
    blender_.CaptureFrame();
    VerifyFrame(*frame_, MouseCursorMonitor::OUTSIDE, DesktopVector(0, 0));
  }
}

TEST_F(DesktopAndCursorComposerTest, IsOccludedShouldBeConsidered) {
  std::unique_ptr<SharedDesktopFrame> frame(
      SharedDesktopFrame::Wrap(CreateTestFrame()));
  frame->set_top_left(DesktopVector(100, 200));
  // The frame covers (100, 200) - (200, 300).

  struct {
    int x;
    int y;
  } tests[] = {
      {100, 200}, {101, 200}, {100, 201}, {101, 201}, {150, 250}, {199, 299},
  };
  fake_screen_->set_is_occluded(true);
  for (size_t i = 0; i < arraysize(tests); i++) {
    SCOPED_TRACE(i);

    fake_screen_->SetNextFrame(frame->Share());
    // The CursorState is ignored when using absolute cursor position.
    fake_cursor_->SetState(MouseCursorMonitor::OUTSIDE,
                           DesktopVector(tests[i].x, tests[i].y));
    blender_.CaptureFrame();
    VerifyFrame(*frame_, MouseCursorMonitor::OUTSIDE, DesktopVector());
  }
}

TEST_F(DesktopAndCursorComposerTest, CursorIncluded) {
  std::unique_ptr<SharedDesktopFrame> frame(
      SharedDesktopFrame::Wrap(CreateTestFrame()));
  frame->set_top_left(DesktopVector(100, 200));
  // The frame covers (100, 200) - (200, 300).

  struct {
    int x;
    int y;
  } tests[] = {
      {100, 200}, {101, 200}, {100, 201}, {101, 201}, {150, 250}, {199, 299},
  };
  for (size_t i = 0; i < arraysize(tests); i++) {
    SCOPED_TRACE(i);

    const DesktopVector abs_pos(tests[i].x, tests[i].y);
    const DesktopVector rel_pos(abs_pos.subtract(frame->top_left()));

    fake_screen_->SetNextFrame(frame->Share());
    // The CursorState is ignored when using absolute cursor position.
    fake_cursor_->SetState(MouseCursorMonitor::OUTSIDE, abs_pos);
    blender_.CaptureFrame();
    VerifyFrame(*frame_, MouseCursorMonitor::INSIDE, rel_pos);

    // Verify that the cursor is erased before the frame buffer is returned to
    // the screen capturer.
    frame_.reset();
    VerifyFrame(*frame, MouseCursorMonitor::OUTSIDE, DesktopVector());
  }
}

}  // namespace webrtc
