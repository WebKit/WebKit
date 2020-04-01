/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/utility/quality_scaler.h"

#include <memory>
#include <string>

#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/task_queue_for_test.h"
#include "test/field_trial.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
static const int kFramerate = 30;
static const int kLowQp = 15;
static const int kHighQp = 40;
static const int kMinFramesNeededToScale = 60;  // From quality_scaler.cc.
static const size_t kDefaultTimeoutMs = 150;
}  // namespace

class MockAdaptationObserver : public AdaptationObserverInterface {
 public:
  virtual ~MockAdaptationObserver() {}

  void AdaptUp(AdaptReason r) override {
    adapt_up_events_++;
    event.Set();
  }
  bool AdaptDown(AdaptReason r) override {
    adapt_down_events_++;
    event.Set();
    return true;
  }

  rtc::Event event;
  int adapt_up_events_ = 0;
  int adapt_down_events_ = 0;
};

// Pass a lower sampling period to speed up the tests.
class QualityScalerUnderTest : public QualityScaler {
 public:
  explicit QualityScalerUnderTest(AdaptationObserverInterface* observer,
                                  VideoEncoder::QpThresholds thresholds)
      : QualityScaler(observer, thresholds, 5) {}
};

class QualityScalerTest : public ::testing::Test,
                          public ::testing::WithParamInterface<std::string> {
 protected:
  enum ScaleDirection {
    kKeepScaleAboveLowQp,
    kKeepScaleAtHighQp,
    kScaleDown,
    kScaleDownAboveHighQp,
    kScaleUp
  };

  QualityScalerTest()
      : scoped_field_trial_(GetParam()),
        task_queue_("QualityScalerTestQueue"),
        observer_(new MockAdaptationObserver()) {
    task_queue_.SendTask(
        [this] {
          qs_ = std::unique_ptr<QualityScaler>(new QualityScalerUnderTest(
              observer_.get(), VideoEncoder::QpThresholds(kLowQp, kHighQp)));
        },
        RTC_FROM_HERE);
  }

  ~QualityScalerTest() {
    task_queue_.SendTask([this] { qs_ = nullptr; }, RTC_FROM_HERE);
  }

  void TriggerScale(ScaleDirection scale_direction) {
    for (int i = 0; i < kFramerate * 5; ++i) {
      switch (scale_direction) {
        case kKeepScaleAboveLowQp:
          qs_->ReportQp(kLowQp + 1, 0);
          break;
        case kScaleUp:
          qs_->ReportQp(kLowQp, 0);
          break;
        case kScaleDown:
          qs_->ReportDroppedFrameByMediaOpt();
          break;
        case kKeepScaleAtHighQp:
          qs_->ReportQp(kHighQp, 0);
          break;
        case kScaleDownAboveHighQp:
          qs_->ReportQp(kHighQp + 1, 0);
          break;
      }
    }
  }

  test::ScopedFieldTrials scoped_field_trial_;
  TaskQueueForTest task_queue_;
  std::unique_ptr<QualityScaler> qs_;
  std::unique_ptr<MockAdaptationObserver> observer_;
};

INSTANTIATE_TEST_SUITE_P(
    FieldTrials,
    QualityScalerTest,
    ::testing::Values(
        "WebRTC-Video-QualityScaling/Enabled-1,2,3,4,5,6,7,8,0.9,0.99,1/",
        ""));

TEST_P(QualityScalerTest, DownscalesAfterContinuousFramedrop) {
  task_queue_.SendTask([this] { TriggerScale(kScaleDown); }, RTC_FROM_HERE);
  EXPECT_TRUE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(1, observer_->adapt_down_events_);
  EXPECT_EQ(0, observer_->adapt_up_events_);
}

TEST_P(QualityScalerTest, KeepsScaleAtHighQp) {
  task_queue_.SendTask([this] { TriggerScale(kKeepScaleAtHighQp); },
                       RTC_FROM_HERE);
  EXPECT_FALSE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(0, observer_->adapt_down_events_);
  EXPECT_EQ(0, observer_->adapt_up_events_);
}

TEST_P(QualityScalerTest, DownscalesAboveHighQp) {
  task_queue_.SendTask([this] { TriggerScale(kScaleDownAboveHighQp); },
                       RTC_FROM_HERE);
  EXPECT_TRUE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(1, observer_->adapt_down_events_);
  EXPECT_EQ(0, observer_->adapt_up_events_);
}

TEST_P(QualityScalerTest, DownscalesAfterTwoThirdsFramedrop) {
  task_queue_.SendTask(
      [this] {
        for (int i = 0; i < kFramerate * 5; ++i) {
          qs_->ReportDroppedFrameByMediaOpt();
          qs_->ReportDroppedFrameByMediaOpt();
          qs_->ReportQp(kHighQp, 0);
        }
      },
      RTC_FROM_HERE);
  EXPECT_TRUE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(1, observer_->adapt_down_events_);
  EXPECT_EQ(0, observer_->adapt_up_events_);
}

TEST_P(QualityScalerTest, DoesNotDownscaleAfterHalfFramedrop) {
  task_queue_.SendTask(
      [this] {
        for (int i = 0; i < kFramerate * 5; ++i) {
          qs_->ReportDroppedFrameByMediaOpt();
          qs_->ReportQp(kHighQp, 0);
        }
      },
      RTC_FROM_HERE);
  EXPECT_FALSE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(0, observer_->adapt_down_events_);
  EXPECT_EQ(0, observer_->adapt_up_events_);
}

TEST_P(QualityScalerTest, DownscalesAfterTwoThirdsIfFieldTrialEnabled) {
  const bool kDownScaleExpected = !GetParam().empty();
  task_queue_.SendTask(
      [this] {
        for (int i = 0; i < kFramerate * 5; ++i) {
          qs_->ReportDroppedFrameByMediaOpt();
          qs_->ReportDroppedFrameByEncoder();
          qs_->ReportQp(kHighQp, 0);
        }
      },
      RTC_FROM_HERE);
  EXPECT_EQ(kDownScaleExpected, observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(kDownScaleExpected ? 1 : 0, observer_->adapt_down_events_);
  EXPECT_EQ(0, observer_->adapt_up_events_);
}

TEST_P(QualityScalerTest, KeepsScaleOnNormalQp) {
  task_queue_.SendTask([this] { TriggerScale(kKeepScaleAboveLowQp); },
                       RTC_FROM_HERE);
  EXPECT_FALSE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(0, observer_->adapt_down_events_);
  EXPECT_EQ(0, observer_->adapt_up_events_);
}

TEST_P(QualityScalerTest, UpscalesAfterLowQp) {
  task_queue_.SendTask([this] { TriggerScale(kScaleUp); }, RTC_FROM_HERE);
  EXPECT_TRUE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(0, observer_->adapt_down_events_);
  EXPECT_EQ(1, observer_->adapt_up_events_);
}

TEST_P(QualityScalerTest, ScalesDownAndBackUp) {
  task_queue_.SendTask([this] { TriggerScale(kScaleDown); }, RTC_FROM_HERE);
  EXPECT_TRUE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(1, observer_->adapt_down_events_);
  EXPECT_EQ(0, observer_->adapt_up_events_);
  task_queue_.SendTask([this] { TriggerScale(kScaleUp); }, RTC_FROM_HERE);
  EXPECT_TRUE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(1, observer_->adapt_down_events_);
  EXPECT_EQ(1, observer_->adapt_up_events_);
}

TEST_P(QualityScalerTest, DoesNotScaleUntilEnoughFramesObserved) {
  task_queue_.SendTask(
      [this] {
        // Not enough frames to make a decision.
        for (int i = 0; i < kMinFramesNeededToScale - 1; ++i) {
          qs_->ReportQp(kLowQp, 0);
        }
      },
      RTC_FROM_HERE);
  EXPECT_FALSE(observer_->event.Wait(kDefaultTimeoutMs));
  task_queue_.SendTask(
      [this] {
        // Send 1 more. Enough frames observed, should result in an adapt
        // request.
        qs_->ReportQp(kLowQp, 0);
      },
      RTC_FROM_HERE);
  EXPECT_TRUE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(0, observer_->adapt_down_events_);
  EXPECT_EQ(1, observer_->adapt_up_events_);

  // Samples should be cleared after an adapt request.
  task_queue_.SendTask(
      [this] {
        // Not enough frames to make a decision.
        qs_->ReportQp(kLowQp, 0);
      },
      RTC_FROM_HERE);
  EXPECT_FALSE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(0, observer_->adapt_down_events_);
  EXPECT_EQ(1, observer_->adapt_up_events_);
}

TEST_P(QualityScalerTest, ScalesDownAndBackUpWithMinFramesNeeded) {
  task_queue_.SendTask(
      [this] {
        for (int i = 0; i < kMinFramesNeededToScale; ++i) {
          qs_->ReportQp(kHighQp + 1, 0);
        }
      },
      RTC_FROM_HERE);
  EXPECT_TRUE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(1, observer_->adapt_down_events_);
  EXPECT_EQ(0, observer_->adapt_up_events_);
  // Samples cleared.
  task_queue_.SendTask(
      [this] {
        for (int i = 0; i < kMinFramesNeededToScale; ++i) {
          qs_->ReportQp(kLowQp, 0);
        }
      },
      RTC_FROM_HERE);
  EXPECT_TRUE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(1, observer_->adapt_down_events_);
  EXPECT_EQ(1, observer_->adapt_up_events_);
}

}  // namespace webrtc
