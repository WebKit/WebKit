/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/desktop_frame_rotation.h"

#include "webrtc/modules/desktop_capture/desktop_frame.h"
#include "webrtc/modules/desktop_capture/desktop_region.h"
#include "webrtc/modules/desktop_capture/test_utils.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

namespace {

// A DesktopFrame implementation which stores data in an external int array.
class ArrayDesktopFrame : public DesktopFrame {
 public:
  ArrayDesktopFrame(DesktopSize size, uint32_t* data);
  ~ArrayDesktopFrame() override;
};

ArrayDesktopFrame::ArrayDesktopFrame(DesktopSize size, uint32_t* data)
    : DesktopFrame(size,
                   size.width() * kBytesPerPixel,
                   reinterpret_cast<uint8_t*>(data),
                   nullptr) {}

ArrayDesktopFrame::~ArrayDesktopFrame() = default;

}  // namespace

TEST(DesktopFrameRotationTest, CopyRect3x4) {
  // A DesktopFrame of 4-pixel width by 3-pixel height.
  static uint32_t frame_pixels[] = {
      0, 1,  2,  3,  //
      4, 5,  6,  7,  //
      8, 9, 10, 11,  //
  };
  ArrayDesktopFrame frame(DesktopSize(4, 3), frame_pixels);

  {
    BasicDesktopFrame target(DesktopSize(4, 3));
    RotateDesktopFrame(frame, DesktopRect::MakeSize(frame.size()),
                       Rotation::CLOCK_WISE_0, DesktopVector(), &target);
    ASSERT_TRUE(DesktopFrameDataEquals(frame, target));
  }

  // After Rotating clock-wise 90 degree
  {
    static uint32_t expected_pixels[] = {
         8, 4, 0,  //
         9, 5, 1,  //
        10, 6, 2,  //
        11, 7, 3,  //
    };
    ArrayDesktopFrame expected(DesktopSize(3, 4), expected_pixels);

    BasicDesktopFrame target(DesktopSize(3, 4));
    RotateDesktopFrame(frame, DesktopRect::MakeSize(frame.size()),
                       Rotation::CLOCK_WISE_90, DesktopVector(), &target);
    ASSERT_TRUE(DesktopFrameDataEquals(target, expected));
  }

  // After Rotating clock-wise 180 degree
  {
    static uint32_t expected_pixels[] = {
        11, 10, 9, 8,  //
         7,  6, 5, 4,  //
         3,  2, 1, 0,  //
    };
    ArrayDesktopFrame expected(DesktopSize(4, 3), expected_pixels);

    BasicDesktopFrame target(DesktopSize(4, 3));
    RotateDesktopFrame(frame, DesktopRect::MakeSize(frame.size()),
                       Rotation::CLOCK_WISE_180, DesktopVector(), &target);
    ASSERT_TRUE(DesktopFrameDataEquals(target, expected));
  }

  // After Rotating clock-wise 270 degree
  {
    static uint32_t expected_pixels[] = {
        3, 7, 11,  //
        2, 6, 10,  //
        1, 5,  9,  //
        0, 4,  8,  //
    };
    ArrayDesktopFrame expected(DesktopSize(3, 4), expected_pixels);

    BasicDesktopFrame target(DesktopSize(3, 4));
    RotateDesktopFrame(frame, DesktopRect::MakeSize(frame.size()),
                       Rotation::CLOCK_WISE_270, DesktopVector(), &target);
    ASSERT_TRUE(DesktopFrameDataEquals(target, expected));
  }
}

TEST(DesktopFrameRotationTest, CopyRect3x5) {
  // A DesktopFrame of 5-pixel width by 3-pixel height.
  static uint32_t frame_pixels[] = {
       0,  1,  2,  3,  4,  //
       5,  6,  7,  8,  9,  //
      10, 11, 12, 13, 14,  //
  };
  ArrayDesktopFrame frame(DesktopSize(5, 3), frame_pixels);

  {
    BasicDesktopFrame target(DesktopSize(5, 3));
    RotateDesktopFrame(frame, DesktopRect::MakeSize(frame.size()),
                       Rotation::CLOCK_WISE_0, DesktopVector(), &target);
    ASSERT_TRUE(DesktopFrameDataEquals(target, frame));
  }

  // After Rotating clock-wise 90 degree
  {
    static uint32_t expected_pixels[] = {
        10, 5, 0,  //
        11, 6, 1,  //
        12, 7, 2,  //
        13, 8, 3,  //
        14, 9, 4,  //
    };
    ArrayDesktopFrame expected(DesktopSize(3, 5), expected_pixels);

    BasicDesktopFrame target(DesktopSize(3, 5));
    RotateDesktopFrame(frame, DesktopRect::MakeSize(frame.size()),
                       Rotation::CLOCK_WISE_90, DesktopVector(), &target);
    ASSERT_TRUE(DesktopFrameDataEquals(target, expected));
  }

  // After Rotating clock-wise 180 degree
  {
    static uint32_t expected_pixels[] {
        14, 13, 12, 11, 10,  //
         9,  8,  7,  6,  5,  //
         4,  3,  2,  1,  0,  //
    };
    ArrayDesktopFrame expected(DesktopSize(5, 3), expected_pixels);

    BasicDesktopFrame target(DesktopSize(5, 3));
    RotateDesktopFrame(frame, DesktopRect::MakeSize(frame.size()),
                       Rotation::CLOCK_WISE_180, DesktopVector(), &target);
    ASSERT_TRUE(DesktopFrameDataEquals(target, expected));
  }

  // After Rotating clock-wise 270 degree
  {
    static uint32_t expected_pixels[] = {
        4, 9, 14,  //
        3, 8, 13,  //
        2, 7, 12,  //
        1, 6, 11,  //
        0, 5, 10,  //
    };
    ArrayDesktopFrame expected(DesktopSize(3, 5), expected_pixels);

    BasicDesktopFrame target(DesktopSize(3, 5));
    RotateDesktopFrame(frame, DesktopRect::MakeSize(frame.size()),
                       Rotation::CLOCK_WISE_270, DesktopVector(), &target);
    ASSERT_TRUE(DesktopFrameDataEquals(target, expected));
  }
}

TEST(DesktopFrameRotationTest, PartialCopyRect3x5) {
  // A DesktopFrame of 5-pixel width by 3-pixel height.
  static uint32_t frame_pixels[] = {
       0,  1,  2,  3,  4,  //
       5,  6,  7,  8,  9,  //
      10, 11, 12, 13, 14,  //
  };
  ArrayDesktopFrame frame(DesktopSize(5, 3), frame_pixels);

  {
    static uint32_t expected_pixels[] = {
        0, 0, 0, 0, 0,  //
        0, 6, 7, 8, 0,  //
        0, 0, 0, 0, 0,  //
    };
    ArrayDesktopFrame expected(DesktopSize(5, 3), expected_pixels);

    BasicDesktopFrame target(DesktopSize(5, 3));
    ClearDesktopFrame(&target);
    RotateDesktopFrame(frame, DesktopRect::MakeXYWH(1, 1, 3, 1),
                       Rotation::CLOCK_WISE_0, DesktopVector(), &target);
    ASSERT_TRUE(DesktopFrameDataEquals(target, expected));
  }

  {
    static uint32_t expected_pixels[] = {
        0, 1, 2, 3, 0,  //
        0, 6, 7, 8, 0,  //
        0, 0, 0, 0, 0,  //
    };
    ArrayDesktopFrame expected(DesktopSize(5, 3), expected_pixels);

    BasicDesktopFrame target(DesktopSize(5, 3));
    ClearDesktopFrame(&target);
    RotateDesktopFrame(frame, DesktopRect::MakeXYWH(1, 0, 3, 2),
                       Rotation::CLOCK_WISE_0, DesktopVector(), &target);
    ASSERT_TRUE(DesktopFrameDataEquals(target, expected));
  }

  // After Rotating clock-wise 90 degree
  {
    static uint32_t expected_pixels[] = {
        0, 0, 0,  //
        0, 6, 0,  //
        0, 7, 0,  //
        0, 8, 0,  //
        0, 0, 0,  //
    };
    ArrayDesktopFrame expected(DesktopSize(3, 5), expected_pixels);

    BasicDesktopFrame target(DesktopSize(3, 5));
    ClearDesktopFrame(&target);
    RotateDesktopFrame(frame, DesktopRect::MakeXYWH(1, 1, 3, 1),
                       Rotation::CLOCK_WISE_90, DesktopVector(), &target);
    ASSERT_TRUE(DesktopFrameDataEquals(target, expected));
  }

  {
    static uint32_t expected_pixels[] = {
         0, 0, 0,  //
        11, 6, 0,  //
        12, 7, 0,  //
        13, 8, 0,  //
         0, 0, 0,  //
    };
    ArrayDesktopFrame expected(DesktopSize(3, 5), expected_pixels);

    BasicDesktopFrame target(DesktopSize(3, 5));
    ClearDesktopFrame(&target);
    RotateDesktopFrame(frame, DesktopRect::MakeXYWH(1, 1, 3, 2),
                       Rotation::CLOCK_WISE_90, DesktopVector(), &target);
    ASSERT_TRUE(DesktopFrameDataEquals(target, expected));
  }

  // After Rotating clock-wise 180 degree
  {
    static uint32_t expected_pixels[] = {
        0, 0, 0, 0, 0,  //
        0, 8, 7, 6, 0,  //
        0, 0, 0, 0, 0,  //
    };
    ArrayDesktopFrame expected(DesktopSize(5, 3), expected_pixels);

    BasicDesktopFrame target(DesktopSize(5, 3));
    ClearDesktopFrame(&target);
    RotateDesktopFrame(frame, DesktopRect::MakeXYWH(1, 1, 3, 1),
                       Rotation::CLOCK_WISE_180, DesktopVector(), &target);
    ASSERT_TRUE(DesktopFrameDataEquals(target, expected));
  }

  {
    static uint32_t expected_pixels[] = {
        0, 13, 12, 11, 0,  //
        0,  8,  7,  6, 0,  //
        0,  0,  0,  0, 0,  //
    };
    ArrayDesktopFrame expected(DesktopSize(5, 3), expected_pixels);

    BasicDesktopFrame target(DesktopSize(5, 3));
    ClearDesktopFrame(&target);
    RotateDesktopFrame(frame, DesktopRect::MakeXYWH(1, 1, 3, 2),
                       Rotation::CLOCK_WISE_180, DesktopVector(), &target);
    ASSERT_TRUE(DesktopFrameDataEquals(target, expected));
  }

  // After Rotating clock-wise 270 degree
  {
    static uint32_t expected_pixels[] = {
        0, 0, 0,  //
        0, 8, 0,  //
        0, 7, 0,  //
        0, 6, 0,  //
        0, 0, 0,  //
    };
    ArrayDesktopFrame expected(DesktopSize(3, 5), expected_pixels);

    BasicDesktopFrame target(DesktopSize(3, 5));
    ClearDesktopFrame(&target);
    RotateDesktopFrame(frame, DesktopRect::MakeXYWH(1, 1, 3, 1),
                       Rotation::CLOCK_WISE_270, DesktopVector(), &target);
    ASSERT_TRUE(DesktopFrameDataEquals(target, expected));
  }

  {
    static uint32_t expected_pixels[] = {
        0, 0, 0,  //
        3, 8, 0,  //
        2, 7, 0,  //
        1, 6, 0,  //
        0, 0, 0,  //
    };
    ArrayDesktopFrame expected(DesktopSize(3, 5), expected_pixels);

    BasicDesktopFrame target(DesktopSize(3, 5));
    ClearDesktopFrame(&target);
    RotateDesktopFrame(frame, DesktopRect::MakeXYWH(1, 0, 3, 2),
                       Rotation::CLOCK_WISE_270, DesktopVector(), &target);
    ASSERT_TRUE(DesktopFrameDataEquals(target, expected));
  }
}

TEST(DesktopFrameRotationTest, WithOffset) {
  // A DesktopFrame of 4-pixel width by 3-pixel height.
  static uint32_t frame_pixels[] = {
      0, 1,  2,  3,  //
      4, 5,  6,  7,  //
      8, 9, 10, 11,  //
  };
  ArrayDesktopFrame frame(DesktopSize(4, 3), frame_pixels);

  {
    static uint32_t expected_pixels[] = {
      0, 0, 0,  0,  0, 0, 0, 0,  //
      0, 0, 1,  2,  3, 0, 0, 0,  //
      0, 4, 5,  6,  7, 0, 0, 0,  //
      0, 8, 9, 10, 11, 0, 0, 0,  //
      0, 0, 0,  0,  0, 0, 0, 0,  //
      0, 0, 0,  0,  0, 0, 0, 0,  //
    };
    ArrayDesktopFrame expected(DesktopSize(8, 6), expected_pixels);

    BasicDesktopFrame target(DesktopSize(8, 6));
    ClearDesktopFrame(&target);
    RotateDesktopFrame(frame, DesktopRect::MakeSize(frame.size()),
                       Rotation::CLOCK_WISE_0, DesktopVector(1, 1), &target);
    ASSERT_TRUE(DesktopFrameDataEquals(target, expected));
    target.mutable_updated_region()->Subtract(
        DesktopRect::MakeOriginSize(DesktopVector(1, 1), frame.size()));
    ASSERT_TRUE(target.updated_region().is_empty());
  }

  {
    static uint32_t expected_pixels[] = {
      0,  0,  0, 0, 0, 0, 0, 0,  //
      0, 11, 10, 9, 8, 0, 0, 0,  //
      0,  7,  6, 5, 4, 0, 0, 0,  //
      0,  3,  2, 1, 0, 0, 0, 0,  //
      0,  0,  0, 0, 0, 0, 0, 0,  //
      0,  0,  0, 0, 0, 0, 0, 0,  //
    };
    ArrayDesktopFrame expected(DesktopSize(8, 6), expected_pixels);

    BasicDesktopFrame target(DesktopSize(8, 6));
    ClearDesktopFrame(&target);
    RotateDesktopFrame(frame, DesktopRect::MakeSize(frame.size()),
                       Rotation::CLOCK_WISE_180, DesktopVector(1, 1), &target);
    ASSERT_TRUE(DesktopFrameDataEquals(target, expected));
    target.mutable_updated_region()->Subtract(
        DesktopRect::MakeOriginSize(DesktopVector(1, 1), frame.size()));
    ASSERT_TRUE(target.updated_region().is_empty());
  }

  {
    static uint32_t expected_pixels[] = {
      0,  0, 0, 0, 0, 0,  //
      0,  8, 4, 0, 0, 0,  //
      0,  9, 5, 1, 0, 0,  //
      0, 10, 6, 2, 0, 0,  //
      0, 11, 7, 3, 0, 0,  //
      0,  0, 0, 0, 0, 0,  //
      0,  0, 0, 0, 0, 0,  //
      0,  0, 0, 0, 0, 0,  //
    };
    ArrayDesktopFrame expected(DesktopSize(6, 8), expected_pixels);

    BasicDesktopFrame target(DesktopSize(6, 8));
    ClearDesktopFrame(&target);
    RotateDesktopFrame(frame, DesktopRect::MakeSize(frame.size()),
                       Rotation::CLOCK_WISE_90, DesktopVector(1, 1), &target);
    ASSERT_TRUE(DesktopFrameDataEquals(target, expected));
    target.mutable_updated_region()->Subtract(
        DesktopRect::MakeXYWH(1, 1, 3, 4));
    ASSERT_TRUE(target.updated_region().is_empty());
  }

  {
    static uint32_t expected_pixels[] = {
      0, 0, 0,  0, 0, 0,  //
      0, 3, 7, 11, 0, 0,  //
      0, 2, 6, 10, 0, 0,  //
      0, 1, 5,  9, 0, 0,  //
      0, 0, 4,  8, 0, 0,  //
      0, 0, 0,  0, 0, 0,  //
      0, 0, 0,  0, 0, 0,  //
      0, 0, 0,  0, 0, 0,  //
    };
    ArrayDesktopFrame expected(DesktopSize(6, 8), expected_pixels);

    BasicDesktopFrame target(DesktopSize(6, 8));
    ClearDesktopFrame(&target);
    RotateDesktopFrame(frame, DesktopRect::MakeSize(frame.size()),
                       Rotation::CLOCK_WISE_270, DesktopVector(1, 1), &target);
    ASSERT_TRUE(DesktopFrameDataEquals(target, expected));
    target.mutable_updated_region()->Subtract(
        DesktopRect::MakeXYWH(1, 1, 3, 4));
    ASSERT_TRUE(target.updated_region().is_empty());
  }
}

// On a typical machine (Intel(R) Xeon(R) E5-1650 v3 @ 3.50GHz, with O2
// optimization, the following case uses ~1.4s to finish. It means entirely
// rotating one 2048 x 1536 frame, which is a large enough number to cover most
// of desktop computer users, uses around 14ms.
TEST(DesktopFrameRotationTest, DISABLED_PerformanceTest) {
  BasicDesktopFrame frame(DesktopSize(2048, 1536));
  BasicDesktopFrame target(DesktopSize(1536, 2048));
  BasicDesktopFrame target2(DesktopSize(2048, 1536));
  for (int i = 0; i < 100; i++) {
    RotateDesktopFrame(frame, DesktopRect::MakeSize(frame.size()),
                       Rotation::CLOCK_WISE_90, DesktopVector(), &target);
    RotateDesktopFrame(frame, DesktopRect::MakeSize(frame.size()),
                       Rotation::CLOCK_WISE_270, DesktopVector(), &target);
    RotateDesktopFrame(frame, DesktopRect::MakeSize(frame.size()),
                       Rotation::CLOCK_WISE_0, DesktopVector(), &target2);
    RotateDesktopFrame(frame, DesktopRect::MakeSize(frame.size()),
                       Rotation::CLOCK_WISE_180, DesktopVector(), &target2);
  }
}

// On a typical machine (Intel(R) Xeon(R) E5-1650 v3 @ 3.50GHz, with O2
// optimization, the following case uses ~6.7s to finish. It means entirely
// rotating one 4096 x 3072 frame uses around 67ms.
TEST(DesktopFrameRotationTest, DISABLED_PerformanceTestOnLargeScreen) {
  BasicDesktopFrame frame(DesktopSize(4096, 3072));
  BasicDesktopFrame target(DesktopSize(3072, 4096));
  BasicDesktopFrame target2(DesktopSize(4096, 3072));
  for (int i = 0; i < 100; i++) {
    RotateDesktopFrame(frame, DesktopRect::MakeSize(frame.size()),
                       Rotation::CLOCK_WISE_90, DesktopVector(), &target);
    RotateDesktopFrame(frame, DesktopRect::MakeSize(frame.size()),
                       Rotation::CLOCK_WISE_270, DesktopVector(), &target);
    RotateDesktopFrame(frame, DesktopRect::MakeSize(frame.size()),
                       Rotation::CLOCK_WISE_0, DesktopVector(), &target2);
    RotateDesktopFrame(frame, DesktopRect::MakeSize(frame.size()),
                       Rotation::CLOCK_WISE_180, DesktopVector(), &target2);
  }
}

}  // namespace webrtc
