/*
 * Copyright (c) 2019, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

//  Test AOM timestamp handling

#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/util.h"
#include "test/video_source.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace {

const int kVideoSourceWidth = 320;
const int kVideoSourceHeight = 240;
const int kFramesToEncode = 3;

// A video source that exposes functions to set the timebase, framerate and
// starting pts.
class DummyTimebaseVideoSource : public ::libaom_test::DummyVideoSource {
 public:
  // Parameters num and den set the timebase for the video source.
  DummyTimebaseVideoSource(int num, int den)
      : framerate_numerator_(30), framerate_denominator_(1), starting_pts_(0) {
    SetSize(kVideoSourceWidth, kVideoSourceHeight);
    set_limit(kFramesToEncode);
    timebase_.num = num;
    timebase_.den = den;
  }

  void SetFramerate(int numerator, int denominator) {
    framerate_numerator_ = numerator;
    framerate_denominator_ = denominator;
  }

  // Returns one frames duration in timebase units as a double.
  double FrameDuration() const {
    return (static_cast<double>(timebase_.den) / timebase_.num) /
           (static_cast<double>(framerate_numerator_) / framerate_denominator_);
  }

  virtual aom_codec_pts_t pts() const {
    return static_cast<aom_codec_pts_t>(frame_ * FrameDuration() +
                                        starting_pts_ + 0.5);
  }

  virtual unsigned long duration() const {
    return static_cast<unsigned long>(FrameDuration() + 0.5);
  }

  virtual aom_rational_t timebase() const { return timebase_; }

  void set_starting_pts(int64_t starting_pts) { starting_pts_ = starting_pts; }

 private:
  aom_rational_t timebase_;
  int framerate_numerator_;
  int framerate_denominator_;
  int64_t starting_pts_;
};

class TimestampTest
    : public ::libaom_test::EncoderTest,
      public ::libaom_test::CodecTestWithParam<libaom_test::TestMode> {
 protected:
  TimestampTest() : EncoderTest(GET_PARAM(0)) {}
  virtual ~TimestampTest() {}

  virtual void SetUp() { InitializeConfig(GET_PARAM(1)); }
};

// Tests encoding in millisecond timebase.
TEST_P(TimestampTest, EncodeFrames) {
  DummyTimebaseVideoSource video(1, 1000);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
}

TEST_P(TimestampTest, TestMicrosecondTimebase) {
  // Set the timebase to microseconds.
  DummyTimebaseVideoSource video(1, 1000000);
  video.set_limit(1);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
}

TEST_P(TimestampTest, TestAv1Rollover) {
  DummyTimebaseVideoSource video(1, 1000);
  video.set_starting_pts(922337170351ll);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
}
#if CONFIG_REALTIME_ONLY
AV1_INSTANTIATE_TEST_SUITE(TimestampTest,
                           ::testing::Values(::libaom_test::kRealTime));
#else
AV1_INSTANTIATE_TEST_SUITE(TimestampTest,
                           ::testing::Values(::libaom_test::kRealTime,
                                             ::libaom_test::kTwoPassGood));
#endif

}  // namespace
