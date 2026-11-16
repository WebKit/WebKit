/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/desktop_capturer_differ_wrapper.h"

#include <initializer_list>
#include <memory>
#include <utility>
#include <vector>

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/desktop_frame_generator.h"
#include "modules/desktop_capture/desktop_geometry.h"
#include "modules/desktop_capture/desktop_region.h"
#include "modules/desktop_capture/differ_block.h"
#include "modules/desktop_capture/fake_desktop_capturer.h"
#include "modules/desktop_capture/mock_desktop_capturer_callback.h"
#include "rtc_base/random.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

// Compares and asserts `frame`.updated_region() equals to `rects`. This
// function does not care about the order of the `rects` and it does not expect
// DesktopRegion to return an exact area for each rectangle in `rects`.
template <template <typename, typename...> class T = std::initializer_list,
          typename... Rect>
void AssertUpdatedRegionIs(const DesktopFrame& frame,
                           const T<DesktopRect, Rect...>& rects) {
  DesktopRegion region;
  for (const auto& rect : rects) {
    region.AddRect(rect);
  }
  ASSERT_TRUE(frame.updated_region().Equals(region));
}

// Compares and asserts `frame`.updated_region() covers all rectangles in
// `rects`, but does not cover areas other than a kBlockSize expansion. This
// function does not care about the order of the `rects`, and it does not expect
// DesktopRegion to return an exact area of each rectangle in `rects`.
template <template <typename, typename...> class T = std::initializer_list,
          typename... Rect>
void AssertUpdatedRegionCovers(const DesktopFrame& frame,
                               const T<DesktopRect, Rect...>& rects) {
  DesktopRegion region;
  for (const auto& rect : rects) {
    region.AddRect(rect);
  }

  // Intersect of `rects` and `frame`.updated_region() should be `rects`. i.e.
  // `frame`.updated_region() should be a superset of `rects`.
  DesktopRegion intersect(region);
  intersect.IntersectWith(frame.updated_region());
  ASSERT_TRUE(region.Equals(intersect));

  // Difference between `rects` and `frame`.updated_region() should not cover
  // areas which have larger than twice of kBlockSize width and height.
  //
  // Explanation of the 'twice' of kBlockSize (indeed kBlockSize * 2 - 2) is
  // following,
  // (Each block in the following grid is a 8 x 8 pixels area. X means the real
  // updated area, m means the updated area marked by
  // DesktopCapturerDifferWrapper.)
  // +---+---+---+---+---+---+---+---+
  // | X | m | m | m | m | m | m | m |
  // +---+---+---+---+---+---+---+---+
  // | m | m | m | m | m | m | m | m |
  // +---+---+---+---+---+---+---+---+
  // | m | m | m | m | m | m | m | m |
  // +---+---+---+---+---+---+---+---+
  // | m | m | m | m | m | m | m | X |
  // +---+---+---+---+---+---+---+---+
  // The top left [0, 0] - [8, 8] and right bottom [56, 24] - [64, 32] blocks of
  // this area are updated. But since DesktopCapturerDifferWrapper compares
  // 32 x 32 blocks by default, this entire area is marked as updated. So the
  // [8, 8] - [56, 32] is expected to be covered in the difference.
  //
  // But if [0, 0] - [8, 8] and [64, 24] - [72, 32] blocks are updated,
  // +---+---+---+---+---+---+---+---+---+---+---+---+
  // | X | m | m | m |   |   |   |   | m | m | m | m |
  // +---+---+---+---+---+---+---+---+---+---+---+---+
  // | m | m | m | m |   |   |   |   | m | m | m | m |
  // +---+---+---+---+---+---+---+---+---+---+---+---+
  // | m | m | m | m |   |   |   |   | m | m | m | m |
  // +---+---+---+---+---+---+---+---+---+---+---+---+
  // | m | m | m | m |   |   |   |   | X | m | m | m |
  // +---+---+---+---+---+---+---+---+---+---+---+---+
  // the [8, 8] - [64, 32] is not expected to be covered in the difference. As
  // DesktopCapturerDifferWrapper should only mark [0, 0] - [32, 32] and
  // [64, 0] - [96, 32] as updated.
  DesktopRegion differ(frame.updated_region());
  differ.Subtract(region);
  for (DesktopRegion::Iterator it(differ); !it.IsAtEnd(); it.Advance()) {
    ASSERT_TRUE(it.rect().width() <= kBlockSize * 2 - 2 ||
                it.rect().height() <= kBlockSize * 2 - 2);
  }
}

// Executes a DesktopCapturerDifferWrapper::Capture() and compares its output
// DesktopFrame::updated_region() with `updated_region` if `check_result` is
// true. If `exactly_match` is true, AssertUpdatedRegionIs() will be used,
// otherwise AssertUpdatedRegionCovers() will be used.
template <template <typename, typename...> class T = std::initializer_list,
          typename... Rect>
void ExecuteDifferWrapperCase(BlackWhiteDesktopFramePainter* frame_painter,
                              DesktopCapturerDifferWrapper* capturer,
                              MockDesktopCapturerCallback* callback,
                              const T<DesktopRect, Rect...>& updated_region,
                              bool check_result,
                              bool exactly_match) {
  EXPECT_CALL(*callback, OnCaptureResultPtr(DesktopCapturer::Result::SUCCESS,
                                            ::testing::_))
      .Times(1)
      .WillOnce([&updated_region, check_result, exactly_match](
                    DesktopCapturer::Result result,
                    std::unique_ptr<DesktopFrame>* frame) {
        ASSERT_EQ(result, DesktopCapturer::Result::SUCCESS);
        if (check_result) {
          if (exactly_match) {
            AssertUpdatedRegionIs(**frame, updated_region);
          } else {
            AssertUpdatedRegionCovers(**frame, updated_region);
          }
        }
      });
  for (const auto& rect : updated_region) {
    frame_painter->updated_region()->AddRect(rect);
  }
  capturer->CaptureFrame();
}

// Executes a DesktopCapturerDifferWrapper::Capture(), if updated_region() is
// not set, this function will reset DesktopCapturerDifferWrapper internal
// DesktopFrame into black.
void ExecuteCapturer(DesktopCapturerDifferWrapper* capturer,
                     MockDesktopCapturerCallback* callback) {
  EXPECT_CALL(*callback, OnCaptureResultPtr(DesktopCapturer::Result::SUCCESS,
                                            ::testing::_))
      .Times(1);
  capturer->CaptureFrame();
}

void ExecuteDifferWrapperTest(Clock* clock,
                              bool with_hints,
                              bool enlarge_updated_region,
                              bool random_updated_region,
                              bool check_result) {
  const bool updated_region_should_exactly_match =
      with_hints && !enlarge_updated_region && !random_updated_region;
  BlackWhiteDesktopFramePainter frame_painter;
  PainterDesktopFrameGenerator frame_generator;
  frame_generator.set_desktop_frame_painter(&frame_painter);
  std::unique_ptr<FakeDesktopCapturer> fake(new FakeDesktopCapturer());
  fake->set_frame_generator(&frame_generator);
  DesktopCapturerDifferWrapper capturer(std::move(fake));
  MockDesktopCapturerCallback callback;
  frame_generator.set_provide_updated_region_hints(with_hints);
  frame_generator.set_enlarge_updated_region(enlarge_updated_region);
  frame_generator.set_add_random_updated_region(random_updated_region);

  capturer.Start(&callback);

  EXPECT_CALL(callback, OnCaptureResultPtr(DesktopCapturer::Result::SUCCESS,
                                           ::testing::_))
      .Times(1)
      .WillOnce([](DesktopCapturer::Result result,
                   std::unique_ptr<DesktopFrame>* frame) {
        ASSERT_EQ(result, DesktopCapturer::Result::SUCCESS);
        AssertUpdatedRegionIs(**frame,
                              {DesktopRect::MakeSize((*frame)->size())});
      });
  capturer.CaptureFrame();

  ExecuteDifferWrapperCase(&frame_painter, &capturer, &callback,
                           {DesktopRect::MakeLTRB(100, 100, 200, 200),
                            DesktopRect::MakeLTRB(300, 300, 400, 400)},
                           check_result, updated_region_should_exactly_match);
  ExecuteCapturer(&capturer, &callback);

  ExecuteDifferWrapperCase(
      &frame_painter, &capturer, &callback,
      {DesktopRect::MakeLTRB(0, 0, 40, 40),
       DesktopRect::MakeLTRB(0, frame_generator.size()->height() - 40, 40,
                             frame_generator.size()->height()),
       DesktopRect::MakeLTRB(frame_generator.size()->width() - 40, 0,
                             frame_generator.size()->width(), 40),
       DesktopRect::MakeLTRB(frame_generator.size()->width() - 40,
                             frame_generator.size()->height() - 40,
                             frame_generator.size()->width(),
                             frame_generator.size()->height())},
      check_result, updated_region_should_exactly_match);

  Random random(clock->TimeInMilliseconds());
  // Fuzzing tests.
  for (int i = 0; i < 1000; i++) {
    if (enlarge_updated_region) {
      frame_generator.set_enlarge_range(random.Rand(1, 50));
    }
    frame_generator.size()->set(random.Rand(500, 2000), random.Rand(500, 2000));
    ExecuteCapturer(&capturer, &callback);
    std::vector<DesktopRect> updated_region;
    for (int j = random.Rand(50); j >= 0; j--) {
      // At least a 1 x 1 updated region.
      const int left = random.Rand(0, frame_generator.size()->width() - 2);
      const int top = random.Rand(0, frame_generator.size()->height() - 2);
      const int right = random.Rand(left + 1, frame_generator.size()->width());
      const int bottom = random.Rand(top + 1, frame_generator.size()->height());
      updated_region.push_back(DesktopRect::MakeLTRB(left, top, right, bottom));
    }
    ExecuteDifferWrapperCase(&frame_painter, &capturer, &callback,
                             updated_region, check_result,
                             updated_region_should_exactly_match);
  }
}

}  // namespace

TEST(DesktopCapturerDifferWrapperTest, CaptureWithoutHints) {
  Clock* clock = Clock::GetRealTimeClock();
  ExecuteDifferWrapperTest(clock, false, false, false, true);
}

TEST(DesktopCapturerDifferWrapperTest, CaptureWithHints) {
  Clock* clock = Clock::GetRealTimeClock();
  ExecuteDifferWrapperTest(clock, true, false, false, true);
}

TEST(DesktopCapturerDifferWrapperTest, CaptureWithEnlargedHints) {
  Clock* clock = Clock::GetRealTimeClock();
  ExecuteDifferWrapperTest(clock, true, true, false, true);
}

TEST(DesktopCapturerDifferWrapperTest, CaptureWithRandomHints) {
  Clock* clock = Clock::GetRealTimeClock();
  ExecuteDifferWrapperTest(clock, true, false, true, true);
}

TEST(DesktopCapturerDifferWrapperTest, CaptureWithEnlargedAndRandomHints) {
  Clock* clock = Clock::GetRealTimeClock();
  ExecuteDifferWrapperTest(clock, true, true, true, true);
}

// When hints are provided, DesktopCapturerDifferWrapper has a slightly better
// performance in current configuration, but not so significant. Following is
// one run result.
// [ RUN      ] DISABLED_CaptureWithoutHintsPerf
// [       OK ] DISABLED_CaptureWithoutHintsPerf (7118 ms)
// [ RUN      ] DISABLED_CaptureWithHintsPerf
// [       OK ] DISABLED_CaptureWithHintsPerf (5580 ms)
// [ RUN      ] DISABLED_CaptureWithEnlargedHintsPerf
// [       OK ] DISABLED_CaptureWithEnlargedHintsPerf (5974 ms)
// [ RUN      ] DISABLED_CaptureWithRandomHintsPerf
// [       OK ] DISABLED_CaptureWithRandomHintsPerf (6184 ms)
// [ RUN      ] DISABLED_CaptureWithEnlargedAndRandomHintsPerf
// [       OK ] DISABLED_CaptureWithEnlargedAndRandomHintsPerf (6347 ms)
TEST(DesktopCapturerDifferWrapperTest, DISABLED_CaptureWithoutHintsPerf) {
  Clock* clock = Clock::GetRealTimeClock();
  const Timestamp started = clock->CurrentTime();
  ExecuteDifferWrapperTest(clock, false, false, false, false);
  ASSERT_LE(clock->CurrentTime() - started, TimeDelta::Millis(15000));
}

TEST(DesktopCapturerDifferWrapperTest, DISABLED_CaptureWithHintsPerf) {
  Clock* clock = Clock::GetRealTimeClock();
  const Timestamp started = clock->CurrentTime();
  ExecuteDifferWrapperTest(clock, true, false, false, false);
  ASSERT_LE(clock->CurrentTime() - started, TimeDelta::Millis(15000));
}

TEST(DesktopCapturerDifferWrapperTest, DISABLED_CaptureWithEnlargedHintsPerf) {
  Clock* clock = Clock::GetRealTimeClock();
  const Timestamp started = clock->CurrentTime();
  ExecuteDifferWrapperTest(clock, true, true, false, false);
  ASSERT_LE(clock->CurrentTime() - started, TimeDelta::Millis(15000));
}

TEST(DesktopCapturerDifferWrapperTest, DISABLED_CaptureWithRandomHintsPerf) {
  Clock* clock = Clock::GetRealTimeClock();
  const Timestamp started = clock->CurrentTime();
  ExecuteDifferWrapperTest(clock, true, false, true, false);
  ASSERT_LE(clock->CurrentTime() - started, TimeDelta::Millis(15000));
}

TEST(DesktopCapturerDifferWrapperTest,
     DISABLED_CaptureWithEnlargedAndRandomHintsPerf) {
  Clock* clock = Clock::GetRealTimeClock();
  const Timestamp started = clock->CurrentTime();
  ExecuteDifferWrapperTest(clock, true, true, true, false);
  ASSERT_LE(clock->CurrentTime() - started, TimeDelta::Millis(15000));
}

}  // namespace webrtc
