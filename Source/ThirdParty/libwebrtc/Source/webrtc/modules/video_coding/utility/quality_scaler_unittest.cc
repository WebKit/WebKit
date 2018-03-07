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

#include "rtc_base/event.h"
#include "rtc_base/task_queue.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
static const int kFramerate = 30;
static const int kLowQp = 15;
static const int kLowQpThreshold = 18;
static const int kHighQp = 40;
static const size_t kDefaultTimeoutMs = 150;
}  // namespace

#define DO_SYNC(q, block) do {    \
  rtc::Event event(false, false); \
  q->PostTask([this, &event] {    \
    block;                        \
    event.Set();                  \
  });                             \
  RTC_CHECK(event.Wait(1000));    \
  } while (0)


class MockAdaptationObserver : public AdaptationObserverInterface {
 public:
  MockAdaptationObserver() : event(false, false) {}
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

class QualityScalerTest : public ::testing::Test {
 protected:
  enum ScaleDirection {
    kKeepScaleAtHighQp,
    kScaleDown,
    kScaleDownAboveHighQp,
    kScaleUp
  };

  QualityScalerTest()
      : q_(new rtc::TaskQueue("QualityScalerTestQueue")),
        observer_(new MockAdaptationObserver()) {
    DO_SYNC(q_, {
      qs_ = std::unique_ptr<QualityScaler>(new QualityScalerUnderTest(
          observer_.get(),
          VideoEncoder::QpThresholds(kLowQpThreshold, kHighQp)));});
  }

  ~QualityScalerTest() {
    DO_SYNC(q_, {qs_.reset(nullptr);});
  }

  void TriggerScale(ScaleDirection scale_direction) {
    for (int i = 0; i < kFramerate * 5; ++i) {
      switch (scale_direction) {
        case kScaleUp:
          qs_->ReportQP(kLowQp);
          break;
        case kScaleDown:
          qs_->ReportDroppedFrame();
          break;
        case kKeepScaleAtHighQp:
          qs_->ReportQP(kHighQp);
          break;
        case kScaleDownAboveHighQp:
          qs_->ReportQP(kHighQp + 1);
          break;
      }
    }
  }

  std::unique_ptr<rtc::TaskQueue> q_;
  std::unique_ptr<QualityScaler> qs_;
  std::unique_ptr<MockAdaptationObserver> observer_;
};

TEST_F(QualityScalerTest, DownscalesAfterContinuousFramedrop) {
  DO_SYNC(q_, { TriggerScale(kScaleDown); });
  EXPECT_TRUE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(1, observer_->adapt_down_events_);
}

TEST_F(QualityScalerTest, KeepsScaleAtHighQp) {
  DO_SYNC(q_, { TriggerScale(kKeepScaleAtHighQp); });
  EXPECT_FALSE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(0, observer_->adapt_down_events_);
  EXPECT_EQ(0, observer_->adapt_up_events_);
}

TEST_F(QualityScalerTest, DownscalesAboveHighQp) {
  DO_SYNC(q_, { TriggerScale(kScaleDownAboveHighQp); });
  EXPECT_TRUE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(1, observer_->adapt_down_events_);
  EXPECT_EQ(0, observer_->adapt_up_events_);
}

TEST_F(QualityScalerTest, DownscalesAfterTwoThirdsFramedrop) {
  DO_SYNC(q_, {
    for (int i = 0; i < kFramerate * 5; ++i) {
      qs_->ReportDroppedFrame();
      qs_->ReportDroppedFrame();
      qs_->ReportQP(kHighQp);
    }
  });
  EXPECT_TRUE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(1, observer_->adapt_down_events_);
  EXPECT_EQ(0, observer_->adapt_up_events_);
}

TEST_F(QualityScalerTest, DoesNotDownscaleOnNormalQp) {
  DO_SYNC(q_, { TriggerScale(kScaleDownAboveHighQp); });
  EXPECT_TRUE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(1, observer_->adapt_down_events_);
  EXPECT_EQ(0, observer_->adapt_up_events_);
}

TEST_F(QualityScalerTest, DoesNotDownscaleAfterHalfFramedrop) {
  DO_SYNC(q_, {
    for (int i = 0; i < kFramerate * 5; ++i) {
      qs_->ReportDroppedFrame();
      qs_->ReportQP(kHighQp);
    }
  });
  EXPECT_FALSE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(0, observer_->adapt_down_events_);
  EXPECT_EQ(0, observer_->adapt_up_events_);
}

TEST_F(QualityScalerTest, UpscalesAfterLowQp) {
  DO_SYNC(q_, { TriggerScale(kScaleUp); });
  EXPECT_TRUE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(0, observer_->adapt_down_events_);
  EXPECT_EQ(1, observer_->adapt_up_events_);
}

TEST_F(QualityScalerTest, ScalesDownAndBackUp) {
  DO_SYNC(q_, { TriggerScale(kScaleDown); });
  EXPECT_TRUE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(1, observer_->adapt_down_events_);
  EXPECT_EQ(0, observer_->adapt_up_events_);
  DO_SYNC(q_, { TriggerScale(kScaleUp); });
  EXPECT_TRUE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(1, observer_->adapt_down_events_);
  EXPECT_EQ(1, observer_->adapt_up_events_);
}

TEST_F(QualityScalerTest, DoesNotScaleUntilEnoughFramesObserved) {
  DO_SYNC(q_, {
      // Send 30 frames. This should not be enough to make a decision.
      for (int i = 0; i < kFramerate; ++i) {
        qs_->ReportQP(kLowQp);
      }
    });
  EXPECT_FALSE(observer_->event.Wait(kDefaultTimeoutMs));
  DO_SYNC(q_, {
      // Send 30 more. This should result in an adapt request as
      // enough frames have now been observed.
      for (int i = 0; i < kFramerate; ++i) {
        qs_->ReportQP(kLowQp);
      }
    });
  EXPECT_TRUE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(0, observer_->adapt_down_events_);
  EXPECT_EQ(1, observer_->adapt_up_events_);
}
}  // namespace webrtc
#undef DO_SYNC
