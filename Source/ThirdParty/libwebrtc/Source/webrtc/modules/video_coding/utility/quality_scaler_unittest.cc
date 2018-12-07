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

#include "rtc_base/event.h"
#include "rtc_base/task_queue.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
static const int kFramerate = 30;
static const int kLowQp = 15;
static const int kHighQp = 40;
static const int kMinFramesNeededToScale = 60;  // From quality_scaler.cc.
static const size_t kDefaultTimeoutMs = 150;
}  // namespace

#define DO_SYNC(q, block)        \
  do {                           \
    rtc::Event event;            \
    q->PostTask([this, &event] { \
      block;                     \
      event.Set();               \
    });                          \
    RTC_CHECK(event.Wait(1000)); \
  } while (0)

class MockAdaptationObserver : public AdaptationObserverInterface {
 public:
  virtual ~MockAdaptationObserver() {}

  void AdaptUp(AdaptReason r) override {
    adapt_up_events_++;
    event.Set();
  }
  void AdaptDown(AdaptReason r) override {
    adapt_down_events_++;
    event.Set();
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
        q_(new rtc::TaskQueue("QualityScalerTestQueue")),
        observer_(new MockAdaptationObserver()) {
    DO_SYNC(q_, {
      qs_ = std::unique_ptr<QualityScaler>(new QualityScalerUnderTest(
          observer_.get(), VideoEncoder::QpThresholds(kLowQp, kHighQp)));
    });
  }

  ~QualityScalerTest() {
    DO_SYNC(q_, { qs_.reset(nullptr); });
  }

  void TriggerScale(ScaleDirection scale_direction) {
    for (int i = 0; i < kFramerate * 5; ++i) {
      switch (scale_direction) {
        case kKeepScaleAboveLowQp:
          qs_->ReportQp(kLowQp + 1);
          break;
        case kScaleUp:
          qs_->ReportQp(kLowQp);
          break;
        case kScaleDown:
          qs_->ReportDroppedFrameByMediaOpt();
          break;
        case kKeepScaleAtHighQp:
          qs_->ReportQp(kHighQp);
          break;
        case kScaleDownAboveHighQp:
          qs_->ReportQp(kHighQp + 1);
          break;
      }
    }
  }

  test::ScopedFieldTrials scoped_field_trial_;
  std::unique_ptr<rtc::TaskQueue> q_;
  std::unique_ptr<QualityScaler> qs_;
  std::unique_ptr<MockAdaptationObserver> observer_;
};

INSTANTIATE_TEST_CASE_P(
    FieldTrials,
    QualityScalerTest,
    ::testing::Values(
        "WebRTC-Video-QualityScaling/Enabled-1,2,3,4,5,6,7,8,0.9,0.99,1/",
        ""));

TEST_P(QualityScalerTest, DownscalesAfterContinuousFramedrop) {
  DO_SYNC(q_, { TriggerScale(kScaleDown); });
  EXPECT_TRUE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(1, observer_->adapt_down_events_);
  EXPECT_EQ(0, observer_->adapt_up_events_);
}

TEST_P(QualityScalerTest, KeepsScaleAtHighQp) {
  DO_SYNC(q_, { TriggerScale(kKeepScaleAtHighQp); });
  EXPECT_FALSE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(0, observer_->adapt_down_events_);
  EXPECT_EQ(0, observer_->adapt_up_events_);
}

TEST_P(QualityScalerTest, DownscalesAboveHighQp) {
  DO_SYNC(q_, { TriggerScale(kScaleDownAboveHighQp); });
  EXPECT_TRUE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(1, observer_->adapt_down_events_);
  EXPECT_EQ(0, observer_->adapt_up_events_);
}

TEST_P(QualityScalerTest, DownscalesAfterTwoThirdsFramedrop) {
  DO_SYNC(q_, {
    for (int i = 0; i < kFramerate * 5; ++i) {
      qs_->ReportDroppedFrameByMediaOpt();
      qs_->ReportDroppedFrameByMediaOpt();
      qs_->ReportQp(kHighQp);
    }
  });
  EXPECT_TRUE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(1, observer_->adapt_down_events_);
  EXPECT_EQ(0, observer_->adapt_up_events_);
}

TEST_P(QualityScalerTest, DoesNotDownscaleAfterHalfFramedrop) {
  DO_SYNC(q_, {
    for (int i = 0; i < kFramerate * 5; ++i) {
      qs_->ReportDroppedFrameByMediaOpt();
      qs_->ReportQp(kHighQp);
    }
  });
  EXPECT_FALSE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(0, observer_->adapt_down_events_);
  EXPECT_EQ(0, observer_->adapt_up_events_);
}

TEST_P(QualityScalerTest, DownscalesAfterTwoThirdsIfFieldTrialEnabled) {
  const bool kDownScaleExpected = !GetParam().empty();
  DO_SYNC(q_, {
    for (int i = 0; i < kFramerate * 5; ++i) {
      qs_->ReportDroppedFrameByMediaOpt();
      qs_->ReportDroppedFrameByEncoder();
      qs_->ReportQp(kHighQp);
    }
  });
  EXPECT_EQ(kDownScaleExpected, observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(kDownScaleExpected ? 1 : 0, observer_->adapt_down_events_);
  EXPECT_EQ(0, observer_->adapt_up_events_);
}

TEST_P(QualityScalerTest, KeepsScaleOnNormalQp) {
  DO_SYNC(q_, { TriggerScale(kKeepScaleAboveLowQp); });
  EXPECT_FALSE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(0, observer_->adapt_down_events_);
  EXPECT_EQ(0, observer_->adapt_up_events_);
}

TEST_P(QualityScalerTest, UpscalesAfterLowQp) {
  DO_SYNC(q_, { TriggerScale(kScaleUp); });
  EXPECT_TRUE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(0, observer_->adapt_down_events_);
  EXPECT_EQ(1, observer_->adapt_up_events_);
}

TEST_P(QualityScalerTest, ScalesDownAndBackUp) {
  DO_SYNC(q_, { TriggerScale(kScaleDown); });
  EXPECT_TRUE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(1, observer_->adapt_down_events_);
  EXPECT_EQ(0, observer_->adapt_up_events_);
  DO_SYNC(q_, { TriggerScale(kScaleUp); });
  EXPECT_TRUE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(1, observer_->adapt_down_events_);
  EXPECT_EQ(1, observer_->adapt_up_events_);
}

TEST_P(QualityScalerTest, DoesNotScaleUntilEnoughFramesObserved) {
  DO_SYNC(q_, {
    // Not enough frames to make a decision.
    for (int i = 0; i < kMinFramesNeededToScale - 1; ++i) {
      qs_->ReportQp(kLowQp);
    }
  });
  EXPECT_FALSE(observer_->event.Wait(kDefaultTimeoutMs));
  DO_SYNC(q_, {
    // Send 1 more. Enough frames observed, should result in an adapt request.
    qs_->ReportQp(kLowQp);
  });
  EXPECT_TRUE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(0, observer_->adapt_down_events_);
  EXPECT_EQ(1, observer_->adapt_up_events_);

  // Samples should be cleared after an adapt request.
  DO_SYNC(q_, {
    // Not enough frames to make a decision.
    qs_->ReportQp(kLowQp);
  });
  EXPECT_FALSE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(0, observer_->adapt_down_events_);
  EXPECT_EQ(1, observer_->adapt_up_events_);
}

TEST_P(QualityScalerTest, ScalesDownAndBackUpWithMinFramesNeeded) {
  DO_SYNC(q_, {
    for (int i = 0; i < kMinFramesNeededToScale; ++i) {
      qs_->ReportQp(kHighQp + 1);
    }
  });
  EXPECT_TRUE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(1, observer_->adapt_down_events_);
  EXPECT_EQ(0, observer_->adapt_up_events_);
  // Samples cleared.
  DO_SYNC(q_, {
    for (int i = 0; i < kMinFramesNeededToScale; ++i) {
      qs_->ReportQp(kLowQp);
    }
  });
  EXPECT_TRUE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(1, observer_->adapt_down_events_);
  EXPECT_EQ(1, observer_->adapt_up_events_);
}

}  // namespace webrtc
#undef DO_SYNC
