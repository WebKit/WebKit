/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/test_utils.h"

#include <stdint.h>

#include "modules/desktop_capture/desktop_geometry.h"
#include "modules/desktop_capture/rgba_color.h"
#include "rtc_base/checks.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

void PaintDesktopFrame(DesktopFrame* frame,
                       DesktopVector pos,
                       RgbaColor color) {
  ASSERT_TRUE(frame);
  ASSERT_TRUE(DesktopRect::MakeSize(frame->size()).Contains(pos));
  *reinterpret_cast<uint32_t*>(frame->GetFrameDataAtPos(pos)) =
      color.ToUInt32();
}

// A DesktopFrame implementation to store data in heap, but the stide is
// doubled.
class DoubleSizeDesktopFrame : public DesktopFrame {
 public:
  explicit DoubleSizeDesktopFrame(DesktopSize size);
  ~DoubleSizeDesktopFrame() override;
};

DoubleSizeDesktopFrame::DoubleSizeDesktopFrame(DesktopSize size)
    : DesktopFrame(
          size,
          kBytesPerPixel * size.width() * 2,
          new uint8_t[kBytesPerPixel * size.width() * size.height() * 2],
          nullptr) {}

DoubleSizeDesktopFrame::~DoubleSizeDesktopFrame() {
  delete[] data_;
}

}  // namespace

TEST(TestUtilsTest, BasicDataEqualsCases) {
  BasicDesktopFrame frame(DesktopSize(4, 4));
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      PaintDesktopFrame(&frame, DesktopVector(i, j), RgbaColor(4U * j + i));
    }
  }

  ASSERT_TRUE(DesktopFrameDataEquals(frame, frame));
  BasicDesktopFrame other(DesktopSize(4, 4));
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      PaintDesktopFrame(&other, DesktopVector(i, j), RgbaColor(4U * j + i));
    }
  }
  ASSERT_TRUE(DesktopFrameDataEquals(frame, other));
  PaintDesktopFrame(&other, DesktopVector(2, 2), RgbaColor(0U));
  ASSERT_FALSE(DesktopFrameDataEquals(frame, other));
}

TEST(TestUtilsTest, DifferentSizeShouldNotEqual) {
  BasicDesktopFrame frame(DesktopSize(4, 4));
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      PaintDesktopFrame(&frame, DesktopVector(i, j), RgbaColor(4U * j + i));
    }
  }

  BasicDesktopFrame other(DesktopSize(2, 8));
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 8; j++) {
      PaintDesktopFrame(&other, DesktopVector(i, j), RgbaColor(2U * j + i));
    }
  }

  ASSERT_FALSE(DesktopFrameDataEquals(frame, other));
}

TEST(TestUtilsTest, DifferentStrideShouldBeComparable) {
  BasicDesktopFrame frame(DesktopSize(4, 4));
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      PaintDesktopFrame(&frame, DesktopVector(i, j), RgbaColor(4U * j + i));
    }
  }

  ASSERT_TRUE(DesktopFrameDataEquals(frame, frame));
  DoubleSizeDesktopFrame other(DesktopSize(4, 4));
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      PaintDesktopFrame(&other, DesktopVector(i, j), RgbaColor(4U * j + i));
    }
  }
  ASSERT_TRUE(DesktopFrameDataEquals(frame, other));
}

}  // namespace webrtc
