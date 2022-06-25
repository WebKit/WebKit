/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/cropped_desktop_frame.h"

#include <memory>
#include <utility>

#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/shared_desktop_frame.h"
#include "test/gtest.h"

namespace webrtc {

std::unique_ptr<DesktopFrame> CreateTestFrame() {
  return std::make_unique<BasicDesktopFrame>(DesktopSize(10, 20));
}

TEST(CroppedDesktopFrameTest, DoNotCreateWrapperIfSizeIsNotChanged) {
  std::unique_ptr<DesktopFrame> original = CreateTestFrame();
  // owned by `original` and CroppedDesktopFrame.
  DesktopFrame* raw_original = original.get();
  std::unique_ptr<DesktopFrame> cropped = CreateCroppedDesktopFrame(
      std::move(original), DesktopRect::MakeWH(10, 20));
  ASSERT_EQ(cropped.get(), raw_original);
}

TEST(CroppedDesktopFrameTest, CropWhenPartiallyOutOfBounds) {
  std::unique_ptr<DesktopFrame> cropped =
      CreateCroppedDesktopFrame(CreateTestFrame(), DesktopRect::MakeWH(11, 10));
  ASSERT_NE(nullptr, cropped);
  ASSERT_EQ(cropped->size().width(), 10);
  ASSERT_EQ(cropped->size().height(), 10);
  ASSERT_EQ(cropped->top_left().x(), 0);
  ASSERT_EQ(cropped->top_left().y(), 0);
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

TEST(CroppedDesktopFrameTest, InitializedWithZeros) {
  std::unique_ptr<DesktopFrame> frame = CreateTestFrame();
  const DesktopVector frame_origin = frame->top_left();
  const DesktopSize frame_size = frame->size();
  std::unique_ptr<DesktopFrame> cropped = CreateCroppedDesktopFrame(
      std::move(frame), DesktopRect::MakeOriginSize(frame_origin, frame_size));
  for (int j = 0; j < cropped->size().height(); ++j) {
    for (int i = 0; i < cropped->stride(); ++i) {
      ASSERT_EQ(cropped->data()[i + j * cropped->stride()], 0);
    }
  }
}

TEST(CroppedDesktopFrameTest, IccProfile) {
  const uint8_t fake_icc_profile_data_array[] = {0x1a, 0x00, 0x2b, 0x00,
                                                 0x3c, 0x00, 0x4d};
  const std::vector<uint8_t> icc_profile(
      fake_icc_profile_data_array,
      fake_icc_profile_data_array + sizeof(fake_icc_profile_data_array));

  std::unique_ptr<DesktopFrame> frame = CreateTestFrame();
  EXPECT_EQ(frame->icc_profile().size(), 0UL);

  frame->set_icc_profile(icc_profile);
  EXPECT_EQ(frame->icc_profile().size(), 7UL);
  EXPECT_EQ(frame->icc_profile(), icc_profile);

  frame = CreateCroppedDesktopFrame(std::move(frame),
                                    DesktopRect::MakeLTRB(2, 2, 8, 18));
  EXPECT_EQ(frame->icc_profile().size(), 7UL);
  EXPECT_EQ(frame->icc_profile(), icc_profile);

  std::unique_ptr<SharedDesktopFrame> shared =
      SharedDesktopFrame::Wrap(std::move(frame));
  EXPECT_EQ(shared->icc_profile().size(), 7UL);
  EXPECT_EQ(shared->icc_profile(), icc_profile);

  std::unique_ptr<DesktopFrame> shared_other = shared->Share();
  EXPECT_EQ(shared_other->icc_profile().size(), 7UL);
  EXPECT_EQ(shared_other->icc_profile(), icc_profile);
}

}  // namespace webrtc
