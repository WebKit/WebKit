/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/desktop_frame.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>

#include "api/array_view.h"
#include "modules/desktop_capture/desktop_geometry.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

std::unique_ptr<DesktopFrame> CreateTestFrame(DesktopRect rect,
                                              int pixels_value) {
  DesktopSize size = rect.size();
  auto frame = std::make_unique<BasicDesktopFrame>(size);
  frame->set_top_left(rect.top_left());
  memset(frame->data(), pixels_value, frame->stride() * size.height());
  return frame;
}

struct TestData {
  const char* description;
  DesktopRect dest_frame_rect;
  DesktopRect src_frame_rect;
  double horizontal_scale;
  double vertical_scale;
  DesktopRect expected_overlap_rect;
};

void RunTest(const TestData& test) {
  // Copy a source frame with all bits set into a dest frame with none set.
  auto dest_frame = CreateTestFrame(test.dest_frame_rect, 0);
  auto src_frame = CreateTestFrame(test.src_frame_rect, 0xff);

  dest_frame->CopyIntersectingPixelsFrom(*src_frame, test.horizontal_scale,
                                         test.vertical_scale);

  // Translate the expected overlap rect to be relative to the dest frame/rect.
  DesktopVector dest_frame_origin = test.dest_frame_rect.top_left();
  DesktopRect relative_expected_overlap_rect = test.expected_overlap_rect;
  relative_expected_overlap_rect.Translate(-dest_frame_origin.x(),
                                           -dest_frame_origin.y());

  // Confirm bits are now set in the dest frame if & only if they fall in the
  // expected range.
  for (int y = 0; y < dest_frame->size().height(); ++y) {
    SCOPED_TRACE(y);

    for (int x = 0; x < dest_frame->size().width(); ++x) {
      SCOPED_TRACE(x);

      DesktopVector point(x, y);
      uint8_t* data = dest_frame->GetFrameDataAtPos(point);
      uint32_t pixel_value = *reinterpret_cast<uint32_t*>(data);
      bool was_copied = pixel_value == 0xffffffff;
      ASSERT_TRUE(was_copied || pixel_value == 0);

      bool expected_to_be_copied =
          relative_expected_overlap_rect.Contains(point);

      ASSERT_EQ(was_copied, expected_to_be_copied);
    }
  }
}

void RunTests(ArrayView<const TestData> tests) {
  for (const TestData& test : tests) {
    SCOPED_TRACE(test.description);

    RunTest(test);
  }
}

}  // namespace

TEST(DesktopFrameTest, NewFrameIsBlack) {
  auto frame = std::make_unique<BasicDesktopFrame>(DesktopSize(10, 10));
  EXPECT_TRUE(frame->FrameDataIsBlack());
}

TEST(DesktopFrameTest, EmptyFrameIsNotBlack) {
  auto frame = std::make_unique<BasicDesktopFrame>(DesktopSize());
  EXPECT_FALSE(frame->FrameDataIsBlack());
}

TEST(DesktopFrameTest, FrameHasDefaultDeviceScaleFactor) {
  auto frame = std::make_unique<BasicDesktopFrame>(DesktopSize());
  EXPECT_EQ(frame->device_scale_factor(), std::nullopt);
}

TEST(DesktopFrameTest, FrameSetsDeviceScaleFactorCorrectly) {
  auto frame = std::make_unique<BasicDesktopFrame>(DesktopSize());
  EXPECT_EQ(frame->device_scale_factor(), std::nullopt);
  float device_scale_factor = 1.5f;
  frame->set_device_scale_factor(device_scale_factor);
  EXPECT_EQ(frame->device_scale_factor(), device_scale_factor);
}

TEST(DesktopFrameTest, FrameDataSwitchesBetweenNonBlackAndBlack) {
  auto frame = CreateTestFrame(DesktopRect::MakeXYWH(0, 0, 10, 10), 0xff);
  EXPECT_FALSE(frame->FrameDataIsBlack());
  frame->SetFrameDataToBlack();
  EXPECT_TRUE(frame->FrameDataIsBlack());
}

TEST(DesktopFrameTest, CopyIntersectingPixelsMatchingRects) {
  const TestData tests[] = {
      {.description = "0 origin",
       .dest_frame_rect = DesktopRect::MakeXYWH(0, 0, 2, 2),
       .src_frame_rect = DesktopRect::MakeXYWH(0, 0, 2, 2),
       .horizontal_scale = 1.0,
       .vertical_scale = 1.0,
       .expected_overlap_rect = DesktopRect::MakeXYWH(0, 0, 2, 2)},

      {.description = "Negative origin",
       .dest_frame_rect = DesktopRect::MakeXYWH(-1, -1, 2, 2),
       .src_frame_rect = DesktopRect::MakeXYWH(-1, -1, 2, 2),
       .horizontal_scale = 1.0,
       .vertical_scale = 1.0,
       .expected_overlap_rect = DesktopRect::MakeXYWH(-1, -1, 2, 2)}};

  RunTests(tests);
}

TEST(DesktopFrameTest, CopyIntersectingPixelsMatchingRectsScaled) {
  // The scale factors shouldn't affect matching rects (they're only applied
  // to any difference between the origins)
  const TestData tests[] = {
      {.description = "0 origin 2x",
       .dest_frame_rect = DesktopRect::MakeXYWH(0, 0, 2, 2),
       .src_frame_rect = DesktopRect::MakeXYWH(0, 0, 2, 2),
       .horizontal_scale = 2.0,
       .vertical_scale = 2.0,
       .expected_overlap_rect = DesktopRect::MakeXYWH(0, 0, 2, 2)},

      {.description = "0 origin 0.5x",
       .dest_frame_rect = DesktopRect::MakeXYWH(0, 0, 2, 2),
       .src_frame_rect = DesktopRect::MakeXYWH(0, 0, 2, 2),
       .horizontal_scale = 0.5,
       .vertical_scale = 0.5,
       .expected_overlap_rect = DesktopRect::MakeXYWH(0, 0, 2, 2)},

      {.description = "Negative origin 2x",
       .dest_frame_rect = DesktopRect::MakeXYWH(-1, -1, 2, 2),
       .src_frame_rect = DesktopRect::MakeXYWH(-1, -1, 2, 2),
       .horizontal_scale = 2.0,
       .vertical_scale = 2.0,
       .expected_overlap_rect = DesktopRect::MakeXYWH(-1, -1, 2, 2)},

      {.description = "Negative origin 0.5x",
       .dest_frame_rect = DesktopRect::MakeXYWH(-1, -1, 2, 2),
       .src_frame_rect = DesktopRect::MakeXYWH(-1, -1, 2, 2),
       .horizontal_scale = 0.5,
       .vertical_scale = 0.5,
       .expected_overlap_rect = DesktopRect::MakeXYWH(-1, -1, 2, 2)}};

  RunTests(tests);
}

TEST(DesktopFrameTest, CopyIntersectingPixelsFullyContainedRects) {
  const TestData tests[] = {
      {.description = "0 origin top left",
       .dest_frame_rect = DesktopRect::MakeXYWH(0, 0, 2, 2),
       .src_frame_rect = DesktopRect::MakeXYWH(0, 0, 1, 1),
       .horizontal_scale = 1.0,
       .vertical_scale = 1.0,
       .expected_overlap_rect = DesktopRect::MakeXYWH(0, 0, 1, 1)},

      {.description = "0 origin bottom right",
       .dest_frame_rect = DesktopRect::MakeXYWH(0, 0, 2, 2),
       .src_frame_rect = DesktopRect::MakeXYWH(1, 1, 1, 1),
       .horizontal_scale = 1.0,
       .vertical_scale = 1.0,
       .expected_overlap_rect = DesktopRect::MakeXYWH(1, 1, 1, 1)},

      {.description = "Negative origin bottom left",
       .dest_frame_rect = DesktopRect::MakeXYWH(-1, -1, 2, 2),
       .src_frame_rect = DesktopRect::MakeXYWH(-1, 0, 1, 1),
       .horizontal_scale = 1.0,
       .vertical_scale = 1.0,
       .expected_overlap_rect = DesktopRect::MakeXYWH(-1, 0, 1, 1)}};

  RunTests(tests);
}

TEST(DesktopFrameTest, CopyIntersectingPixelsFullyContainedRectsScaled) {
  const TestData tests[] = {
      {.description = "0 origin top left 2x",
       .dest_frame_rect = DesktopRect::MakeXYWH(0, 0, 2, 2),
       .src_frame_rect = DesktopRect::MakeXYWH(0, 0, 1, 1),
       .horizontal_scale = 2.0,
       .vertical_scale = 2.0,
       .expected_overlap_rect = DesktopRect::MakeXYWH(0, 0, 1, 1)},

      {.description = "0 origin top left 0.5x",
       .dest_frame_rect = DesktopRect::MakeXYWH(0, 0, 2, 2),
       .src_frame_rect = DesktopRect::MakeXYWH(0, 0, 1, 1),
       .horizontal_scale = 0.5,
       .vertical_scale = 0.5,
       .expected_overlap_rect = DesktopRect::MakeXYWH(0, 0, 1, 1)},

      {.description = "0 origin bottom left 2x",
       .dest_frame_rect = DesktopRect::MakeXYWH(0, 0, 4, 4),
       .src_frame_rect = DesktopRect::MakeXYWH(1, 1, 2, 2),
       .horizontal_scale = 2.0,
       .vertical_scale = 2.0,
       .expected_overlap_rect = DesktopRect::MakeXYWH(2, 2, 2, 2)},

      {.description = "0 origin bottom middle 2x/1x",
       .dest_frame_rect = DesktopRect::MakeXYWH(0, 0, 4, 3),
       .src_frame_rect = DesktopRect::MakeXYWH(1, 1, 2, 2),
       .horizontal_scale = 2.0,
       .vertical_scale = 1.0,
       .expected_overlap_rect = DesktopRect::MakeXYWH(2, 1, 2, 2)},

      {.description = "0 origin middle 0.5x",
       .dest_frame_rect = DesktopRect::MakeXYWH(0, 0, 3, 3),
       .src_frame_rect = DesktopRect::MakeXYWH(2, 2, 1, 1),
       .horizontal_scale = 0.5,
       .vertical_scale = 0.5,
       .expected_overlap_rect = DesktopRect::MakeXYWH(1, 1, 1, 1)},

      {.description = "Negative origin bottom left 2x",
       .dest_frame_rect = DesktopRect::MakeXYWH(-1, -1, 3, 3),
       .src_frame_rect = DesktopRect::MakeXYWH(-1, 0, 1, 1),
       .horizontal_scale = 2.0,
       .vertical_scale = 2.0,
       .expected_overlap_rect = DesktopRect::MakeXYWH(-1, 1, 1, 1)},

      {.description = "Negative origin near middle 0.5x",
       .dest_frame_rect = DesktopRect::MakeXYWH(-2, -2, 2, 2),
       .src_frame_rect = DesktopRect::MakeXYWH(0, 0, 1, 1),
       .horizontal_scale = 0.5,
       .vertical_scale = 0.5,
       .expected_overlap_rect = DesktopRect::MakeXYWH(-1, -1, 1, 1)}};

  RunTests(tests);
}

TEST(DesktopFrameTest, CopyIntersectingPixelsPartiallyContainedRects) {
  const TestData tests[] = {
      {.description = "Top left",
       .dest_frame_rect = DesktopRect::MakeXYWH(0, 0, 2, 2),
       .src_frame_rect = DesktopRect::MakeXYWH(-1, -1, 2, 2),
       .horizontal_scale = 1.0,
       .vertical_scale = 1.0,
       .expected_overlap_rect = DesktopRect::MakeXYWH(0, 0, 1, 1)},

      {.description = "Top right",
       .dest_frame_rect = DesktopRect::MakeXYWH(0, 0, 2, 2),
       .src_frame_rect = DesktopRect::MakeXYWH(1, -1, 2, 2),
       .horizontal_scale = 1.0,
       .vertical_scale = 1.0,
       .expected_overlap_rect = DesktopRect::MakeXYWH(1, 0, 1, 1)},

      {.description = "Bottom right",
       .dest_frame_rect = DesktopRect::MakeXYWH(0, 0, 2, 2),
       .src_frame_rect = DesktopRect::MakeXYWH(1, 1, 2, 2),
       .horizontal_scale = 1.0,
       .vertical_scale = 1.0,
       .expected_overlap_rect = DesktopRect::MakeXYWH(1, 1, 1, 1)},

      {.description = "Bottom left",
       .dest_frame_rect = DesktopRect::MakeXYWH(0, 0, 2, 2),
       .src_frame_rect = DesktopRect::MakeXYWH(-1, 1, 2, 2),
       .horizontal_scale = 1.0,
       .vertical_scale = 1.0,
       .expected_overlap_rect = DesktopRect::MakeXYWH(0, 1, 1, 1)}};

  RunTests(tests);
}

TEST(DesktopFrameTest, CopyIntersectingPixelsPartiallyContainedRectsScaled) {
  const TestData tests[] = {
      {.description = "Top left 2x",
       .dest_frame_rect = DesktopRect::MakeXYWH(0, 0, 2, 2),
       .src_frame_rect = DesktopRect::MakeXYWH(-1, -1, 3, 3),
       .horizontal_scale = 2.0,
       .vertical_scale = 2.0,
       .expected_overlap_rect = DesktopRect::MakeXYWH(0, 0, 1, 1)},

      {.description = "Top right 0.5x",
       .dest_frame_rect = DesktopRect::MakeXYWH(0, 0, 2, 2),
       .src_frame_rect = DesktopRect::MakeXYWH(2, -2, 2, 2),
       .horizontal_scale = 0.5,
       .vertical_scale = 0.5,
       .expected_overlap_rect = DesktopRect::MakeXYWH(1, 0, 1, 1)},

      {.description = "Bottom right 2x",
       .dest_frame_rect = DesktopRect::MakeXYWH(0, 0, 3, 3),
       .src_frame_rect = DesktopRect::MakeXYWH(-1, 1, 3, 3),
       .horizontal_scale = 2.0,
       .vertical_scale = 2.0,
       .expected_overlap_rect = DesktopRect::MakeXYWH(0, 2, 1, 1)},

      {.description = "Bottom left 0.5x",
       .dest_frame_rect = DesktopRect::MakeXYWH(0, 0, 2, 2),
       .src_frame_rect = DesktopRect::MakeXYWH(-2, 2, 2, 2),
       .horizontal_scale = 0.5,
       .vertical_scale = 0.5,
       .expected_overlap_rect = DesktopRect::MakeXYWH(0, 1, 1, 1)}};

  RunTests(tests);
}

TEST(DesktopFrameTest, CopyIntersectingPixelsUncontainedRects) {
  const TestData tests[] = {
      {.description = "Left",
       .dest_frame_rect = DesktopRect::MakeXYWH(0, 0, 2, 2),
       .src_frame_rect = DesktopRect::MakeXYWH(-1, 0, 1, 2),
       .horizontal_scale = 1.0,
       .vertical_scale = 1.0,
       .expected_overlap_rect = DesktopRect::MakeXYWH(0, 0, 0, 0)},

      {.description = "Top",
       .dest_frame_rect = DesktopRect::MakeXYWH(0, 0, 2, 2),
       .src_frame_rect = DesktopRect::MakeXYWH(0, -1, 2, 1),
       .horizontal_scale = 1.0,
       .vertical_scale = 1.0,
       .expected_overlap_rect = DesktopRect::MakeXYWH(0, 0, 0, 0)},

      {.description = "Right",
       .dest_frame_rect = DesktopRect::MakeXYWH(0, 0, 2, 2),
       .src_frame_rect = DesktopRect::MakeXYWH(2, 0, 1, 2),
       .horizontal_scale = 1.0,
       .vertical_scale = 1.0,
       .expected_overlap_rect = DesktopRect::MakeXYWH(0, 0, 0, 0)},

      {.description = "Bottom",
       .dest_frame_rect = DesktopRect::MakeXYWH(0, 0, 2, 2),
       .src_frame_rect = DesktopRect::MakeXYWH(0, 2, 2, 1),
       .horizontal_scale = 1.0,
       .vertical_scale = 1.0,
       .expected_overlap_rect = DesktopRect::MakeXYWH(0, 0, 0, 0)}};

  RunTests(tests);
}

TEST(DesktopFrameTest, CopyIntersectingPixelsUncontainedRectsScaled) {
  const TestData tests[] = {
      {.description = "Left 2x",
       .dest_frame_rect = DesktopRect::MakeXYWH(0, 0, 2, 2),
       .src_frame_rect = DesktopRect::MakeXYWH(-1, 0, 2, 2),
       .horizontal_scale = 2.0,
       .vertical_scale = 2.0,
       .expected_overlap_rect = DesktopRect::MakeXYWH(0, 0, 0, 0)},

      {.description = "Top 0.5x",
       .dest_frame_rect = DesktopRect::MakeXYWH(0, 0, 2, 2),
       .src_frame_rect = DesktopRect::MakeXYWH(0, -2, 2, 1),
       .horizontal_scale = 0.5,
       .vertical_scale = 0.5,
       .expected_overlap_rect = DesktopRect::MakeXYWH(0, 0, 0, 0)},

      {.description = "Right 2x",
       .dest_frame_rect = DesktopRect::MakeXYWH(0, 0, 2, 2),
       .src_frame_rect = DesktopRect::MakeXYWH(1, 0, 1, 2),
       .horizontal_scale = 2.0,
       .vertical_scale = 2.0,
       .expected_overlap_rect = DesktopRect::MakeXYWH(0, 0, 0, 0)},

      {.description = "Bottom 0.5x",
       .dest_frame_rect = DesktopRect::MakeXYWH(0, 0, 2, 2),
       .src_frame_rect = DesktopRect::MakeXYWH(0, 4, 2, 1),
       .horizontal_scale = 0.5,
       .vertical_scale = 0.5,
       .expected_overlap_rect = DesktopRect::MakeXYWH(0, 0, 0, 0)}};

  RunTests(tests);
}

}  // namespace webrtc
