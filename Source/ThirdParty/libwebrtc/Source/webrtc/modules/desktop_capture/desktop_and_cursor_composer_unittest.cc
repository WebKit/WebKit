/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/desktop_and_cursor_composer.h"

#include <cstdint>
#include <cstring>
#include <iterator>
#include <memory>
#include <ostream>
#include <utility>

#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/desktop_geometry.h"
#include "modules/desktop_capture/desktop_region.h"
#include "modules/desktop_capture/mouse_cursor.h"
#include "modules/desktop_capture/mouse_cursor_monitor.h"
#include "modules/desktop_capture/shared_desktop_frame.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

using testing::ElementsAre;

constexpr int kFrameXCoord = 100;
constexpr int kFrameYCoord = 200;
constexpr int kScreenWidth = 100;
constexpr int kScreenHeight = 100;
constexpr int kCursorWidth = 10;
constexpr int kCursorHeight = 10;

constexpr int kTestCursorSize = 3;
constexpr uint32_t kTestCursorData[kTestCursorSize][kTestCursorSize] = {
    {
        0xffffffff,
        0x99990000,
        0xaa222222,
    },
    {
        0x88008800,
        0xaa0000aa,
        0xaa333333,
    },
    {
        0x00000000,
        0xaa0000aa,
        0xaa333333,
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

DesktopFrame* CreateTestFrame(int width = kScreenWidth,
                              int height = kScreenHeight) {
  DesktopFrame* frame = new BasicDesktopFrame(DesktopSize(width, height));
  uint32_t* data = reinterpret_cast<uint32_t*>(frame->data());
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      *(data++) = GetFakeFramePixelValue(DesktopVector(x, y));
    }
  }
  return frame;
}

MouseCursor* CreateTestCursor(DesktopVector hotspot) {
  std::unique_ptr<DesktopFrame> image(
      new BasicDesktopFrame(DesktopSize(kCursorWidth, kCursorHeight)));
  uint32_t* data = reinterpret_cast<uint32_t*>(image->data());
  // Set four pixels near the hotspot and leave all other blank.
  for (int y = 0; y < kTestCursorSize; ++y) {
    for (int x = 0; x < kTestCursorSize; ++x) {
      data[(hotspot.y() + y) * kCursorWidth + (hotspot.x() + x)] =
          kTestCursorData[y][x];
    }
  }
  return new MouseCursor(image.release(), hotspot);
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

  bool IsOccluded(const DesktopVector& /* pos */) override {
    return is_occluded_;
  }

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

  void Init(Callback* callback, Mode /* mode */) override {
    callback_ = callback;
  }

  void Capture() override {
    if (changed_) {
      callback_->OnMouseCursor(CreateTestCursor(hotspot_));
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

bool operator==(const DesktopRect& left, const DesktopRect& right) {
  return left.equals(right);
}

std::ostream& operator<<(std::ostream& out, const DesktopRect& rect) {
  out << "{" << rect.left() << "+" << rect.top() << "-" << rect.width() << "x"
      << rect.height() << "}";
  return out;
}

class DesktopAndCursorComposerTest : public ::testing::Test,
                                     public DesktopCapturer::Callback {
 public:
  explicit DesktopAndCursorComposerTest(bool include_cursor = true)
      : fake_screen_(new FakeScreenCapturer()),
        fake_cursor_(include_cursor ? new FakeMouseMonitor() : nullptr),
        blender_(fake_screen_, fake_cursor_) {
    blender_.Start(this);
  }

  // DesktopCapturer::Callback interface
  void OnCaptureResult(DesktopCapturer::Result /* result */,
                       std::unique_ptr<DesktopFrame> frame) override {
    frame_ = std::move(frame);
  }

 protected:
  // Owned by `blender_`.
  FakeScreenCapturer* fake_screen_;
  FakeMouseMonitor* fake_cursor_;

  DesktopAndCursorComposer blender_;
  std::unique_ptr<DesktopFrame> frame_;
};

class DesktopAndCursorComposerNoCursorMonitorTest
    : public DesktopAndCursorComposerTest {
 public:
  DesktopAndCursorComposerNoCursorMonitorTest()
      : DesktopAndCursorComposerTest(false) {}
};

TEST_F(DesktopAndCursorComposerTest, CursorShouldBeIgnoredIfNoFrameCaptured) {
  struct {
    int x, y;
    int hotspot_x, hotspot_y;
    bool inside;
  } tests[] = {
      {.x = 0, .y = 0, .hotspot_x = 0, .hotspot_y = 0, .inside = true},
      {.x = 50, .y = 50, .hotspot_x = 0, .hotspot_y = 0, .inside = true},
      {.x = 100, .y = 50, .hotspot_x = 0, .hotspot_y = 0, .inside = true},
      {.x = 50, .y = 100, .hotspot_x = 0, .hotspot_y = 0, .inside = true},
      {.x = 100, .y = 100, .hotspot_x = 0, .hotspot_y = 0, .inside = true},
      {.x = 0, .y = 0, .hotspot_x = 2, .hotspot_y = 5, .inside = true},
      {.x = 1, .y = 1, .hotspot_x = 2, .hotspot_y = 5, .inside = true},
      {.x = 50, .y = 50, .hotspot_x = 2, .hotspot_y = 5, .inside = true},
      {.x = 100, .y = 100, .hotspot_x = 2, .hotspot_y = 5, .inside = true},
      {.x = 0, .y = 0, .hotspot_x = 5, .hotspot_y = 2, .inside = true},
      {.x = 50, .y = 50, .hotspot_x = 5, .hotspot_y = 2, .inside = true},
      {.x = 100, .y = 100, .hotspot_x = 5, .hotspot_y = 2, .inside = true},
      {.x = 0, .y = 0, .hotspot_x = 0, .hotspot_y = 0, .inside = false},
  };

  for (size_t i = 0; i < std::size(tests); i++) {
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

TEST_F(DesktopAndCursorComposerTest, CursorShouldBeIgnoredIfFrameMayContainIt) {
  // We can't use a shared frame because we need to detect modifications
  // compared to a control.
  std::unique_ptr<DesktopFrame> control_frame(CreateTestFrame());
  control_frame->set_top_left(DesktopVector(kFrameXCoord, kFrameYCoord));

  struct {
    int x;
    int y;
    bool may_contain_cursor;
  } tests[] = {
      {.x = 100, .y = 200, .may_contain_cursor = true},
      {.x = 100, .y = 200, .may_contain_cursor = false},
      {.x = 150, .y = 250, .may_contain_cursor = true},
      {.x = 150, .y = 250, .may_contain_cursor = false},
  };
  for (size_t i = 0; i < std::size(tests); i++) {
    SCOPED_TRACE(i);

    std::unique_ptr<DesktopFrame> frame(CreateTestFrame());
    frame->set_top_left(DesktopVector(kFrameXCoord, kFrameYCoord));
    frame->set_may_contain_cursor(tests[i].may_contain_cursor);
    fake_screen_->SetNextFrame(std::move(frame));

    const DesktopVector abs_pos(tests[i].x, tests[i].y);
    fake_cursor_->SetState(MouseCursorMonitor::INSIDE, abs_pos);
    blender_.CaptureFrame();

    // If the frame may already have contained the cursor, then `CaptureFrame()`
    // should not have modified it, so it should be the same as the control.
    EXPECT_TRUE(frame_);
    const DesktopVector rel_pos(abs_pos.subtract(control_frame->top_left()));
    if (tests[i].may_contain_cursor) {
      EXPECT_EQ(
          *reinterpret_cast<uint32_t*>(frame_->GetFrameDataAtPos(rel_pos)),
          *reinterpret_cast<uint32_t*>(
              control_frame->GetFrameDataAtPos(rel_pos)));

    } else {
      // `CaptureFrame()` should have modified the frame to have the cursor.
      EXPECT_NE(
          *reinterpret_cast<uint32_t*>(frame_->GetFrameDataAtPos(rel_pos)),
          *reinterpret_cast<uint32_t*>(
              control_frame->GetFrameDataAtPos(rel_pos)));
      EXPECT_TRUE(frame_->may_contain_cursor());
    }
  }
}

TEST_F(DesktopAndCursorComposerTest,
       CursorShouldBeIgnoredIfItIsOutOfDesktopFrame) {
  std::unique_ptr<SharedDesktopFrame> frame(
      SharedDesktopFrame::Wrap(CreateTestFrame()));
  frame->set_top_left(DesktopVector(kFrameXCoord, kFrameYCoord));
  // The frame covers (100, 200) - (200, 300).

  DesktopVector tests[] = {
      {0, 0},    {50, 50},         {50, 150},      {100, 150}, {50, 200},
      {99, 200}, {100, 199},       {200, 300},     {200, 299}, {199, 300},
      {-1, -1},  {-10000, -10000}, {10000, 10000},
  };
  for (const DesktopVector& abs_pos : tests) {
    SCOPED_TRACE(abs_pos);

    fake_screen_->SetNextFrame(frame->Share());
    // The CursorState is ignored when using absolute cursor position.
    fake_cursor_->SetState(MouseCursorMonitor::OUTSIDE, abs_pos);
    blender_.CaptureFrame();
    VerifyFrame(*frame_, MouseCursorMonitor::OUTSIDE, DesktopVector(0, 0));
  }
}

TEST_F(DesktopAndCursorComposerTest, IsOccludedShouldBeConsidered) {
  std::unique_ptr<SharedDesktopFrame> frame(
      SharedDesktopFrame::Wrap(CreateTestFrame()));
  frame->set_top_left(DesktopVector(kFrameXCoord, kFrameYCoord));
  // The frame covers (100, 200) - (200, 300).

  DesktopVector tests[] = {
      {100, 200}, {101, 200}, {100, 201}, {101, 201}, {150, 250}, {199, 299},
  };
  fake_screen_->set_is_occluded(true);
  for (const DesktopVector& abs_pos : tests) {
    SCOPED_TRACE(abs_pos);

    fake_screen_->SetNextFrame(frame->Share());
    // The CursorState is ignored when using absolute cursor position.
    fake_cursor_->SetState(MouseCursorMonitor::OUTSIDE, abs_pos);
    blender_.CaptureFrame();
    VerifyFrame(*frame_, MouseCursorMonitor::OUTSIDE, DesktopVector());
  }
}

TEST_F(DesktopAndCursorComposerTest, CursorIncluded) {
  std::unique_ptr<SharedDesktopFrame> frame(
      SharedDesktopFrame::Wrap(CreateTestFrame()));
  frame->set_top_left(DesktopVector(kFrameXCoord, kFrameYCoord));
  // The frame covers (100, 200) - (200, 300).

  DesktopVector tests[] = {
      {100, 200}, {101, 200}, {100, 201}, {101, 201}, {150, 250}, {199, 299},
  };
  for (const DesktopVector& abs_pos : tests) {
    SCOPED_TRACE(abs_pos);

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

TEST_F(DesktopAndCursorComposerNoCursorMonitorTest,
       UpdatedRegionIncludesOldAndNewCursorRectsIfMoved) {
  std::unique_ptr<SharedDesktopFrame> frame(
      SharedDesktopFrame::Wrap(CreateTestFrame()));
  DesktopRect first_cursor_rect;
  {
    // Block to scope test_cursor, which is invalidated by OnMouseCursor.
    MouseCursor* test_cursor = CreateTestCursor(DesktopVector(0, 0));
    first_cursor_rect = DesktopRect::MakeSize(test_cursor->image()->size());
    blender_.OnMouseCursor(test_cursor);
  }
  blender_.OnMouseCursorPosition(DesktopVector(0, 0));
  fake_screen_->SetNextFrame(frame->Share());
  blender_.CaptureFrame();

  DesktopVector cursor_move_offset(1, 1);
  DesktopRect second_cursor_rect = first_cursor_rect;
  second_cursor_rect.Translate(cursor_move_offset);
  blender_.OnMouseCursorPosition(cursor_move_offset);
  fake_screen_->SetNextFrame(frame->Share());
  blender_.CaptureFrame();

  EXPECT_TRUE(frame->updated_region().is_empty());
  DesktopRegion expected_region;
  expected_region.AddRect(first_cursor_rect);
  expected_region.AddRect(second_cursor_rect);
  EXPECT_TRUE(frame_->updated_region().Equals(expected_region));
}

TEST_F(DesktopAndCursorComposerNoCursorMonitorTest,
       UpdatedRegionIncludesOldAndNewCursorRectsIfShapeChanged) {
  std::unique_ptr<SharedDesktopFrame> frame(
      SharedDesktopFrame::Wrap(CreateTestFrame()));
  DesktopRect first_cursor_rect;
  {
    // Block to scope test_cursor, which is invalidated by OnMouseCursor.
    MouseCursor* test_cursor = CreateTestCursor(DesktopVector(0, 0));
    first_cursor_rect = DesktopRect::MakeSize(test_cursor->image()->size());
    blender_.OnMouseCursor(test_cursor);
  }
  blender_.OnMouseCursorPosition(DesktopVector(0, 0));
  fake_screen_->SetNextFrame(frame->Share());
  blender_.CaptureFrame();

  // Create a second cursor, the same shape as the first. Since the code doesn't
  // compare the cursor pixels, this is sufficient, and avoids needing two test
  // cursor bitmaps.
  DesktopRect second_cursor_rect;
  {
    MouseCursor* test_cursor = CreateTestCursor(DesktopVector(0, 0));
    second_cursor_rect = DesktopRect::MakeSize(test_cursor->image()->size());
    blender_.OnMouseCursor(test_cursor);
  }
  fake_screen_->SetNextFrame(frame->Share());
  blender_.CaptureFrame();

  EXPECT_TRUE(frame->updated_region().is_empty());
  DesktopRegion expected_region;
  expected_region.AddRect(first_cursor_rect);
  expected_region.AddRect(second_cursor_rect);
  EXPECT_TRUE(frame_->updated_region().Equals(expected_region));
}

TEST_F(DesktopAndCursorComposerNoCursorMonitorTest,
       UpdatedRegionUnchangedIfCursorUnchanged) {
  std::unique_ptr<SharedDesktopFrame> frame(
      SharedDesktopFrame::Wrap(CreateTestFrame()));
  blender_.OnMouseCursor(CreateTestCursor(DesktopVector(0, 0)));
  blender_.OnMouseCursorPosition(DesktopVector(0, 0));
  fake_screen_->SetNextFrame(frame->Share());
  blender_.CaptureFrame();
  fake_screen_->SetNextFrame(frame->Share());
  blender_.CaptureFrame();

  EXPECT_TRUE(frame->updated_region().is_empty());
  EXPECT_TRUE(frame_->updated_region().is_empty());
}

}  // namespace webrtc
