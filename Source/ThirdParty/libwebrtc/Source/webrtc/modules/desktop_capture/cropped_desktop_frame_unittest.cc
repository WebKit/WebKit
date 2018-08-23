/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <utility>

#include "absl/memory/memory.h"
#include "modules/desktop_capture/cropped_desktop_frame.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "test/gtest.h"

namespace webrtc {

std::unique_ptr<DesktopFrame> CreateTestFrame() {
  return absl::make_unique<BasicDesktopFrame>(DesktopSize(10, 20));
}

TEST(CroppedDesktopFrameTest, DoNotCreateWrapperIfSizeIsNotChanged) {
  std::unique_ptr<DesktopFrame> original = CreateTestFrame();
  // owned by |original| and CroppedDesktopFrame.
  DesktopFrame* raw_original = original.get();
  std::unique_ptr<DesktopFrame> cropped = CreateCroppedDesktopFrame(
      std::move(original), DesktopRect::MakeWH(10, 20));
  ASSERT_EQ(cropped.get(), raw_original);
}

TEST(CroppedDesktopFrameTest, ReturnNullptrIfSizeIsNotSufficient) {
  ASSERT_EQ(nullptr, CreateCroppedDesktopFrame(CreateTestFrame(),
                                               DesktopRect::MakeWH(11, 10)));
}

TEST(CroppedDesktopFrameTest, ReturnNullIfCropRegionIsOutOfBounds) {
  std::unique_ptr<DesktopFrame> frame = CreateTestFrame();
  frame->set_top_left(DesktopVector(100, 200));
  ASSERT_EQ(nullptr,
            CreateCroppedDesktopFrame(
                std::move(frame), DesktopRect::MakeLTRB(101, 203, 109, 218)));
}

TEST(CroppedDesktopFrameTest, CropASubArea) {
  std::unique_ptr<DesktopFrame> cropped = CreateCroppedDesktopFrame(
      CreateTestFrame(), DesktopRect::MakeLTRB(1, 2, 9, 19));
  ASSERT_EQ(cropped->size().width(), 8);
  ASSERT_EQ(cropped->size().height(), 17);
  ASSERT_EQ(cropped->top_left().x(), 1);
  ASSERT_EQ(cropped->top_left().y(), 2);
}

TEST(CroppedDesktopFrameTest, SetTopLeft) {
  std::unique_ptr<DesktopFrame> frame = CreateTestFrame();
  frame->set_top_left(DesktopVector(100, 200));
  frame = CreateCroppedDesktopFrame(std::move(frame),
                                    DesktopRect::MakeLTRB(1, 3, 9, 18));
  ASSERT_EQ(frame->size().width(), 8);
  ASSERT_EQ(frame->size().height(), 15);
  ASSERT_EQ(frame->top_left().x(), 101);
  ASSERT_EQ(frame->top_left().y(), 203);
}

}  // namespace webrtc
