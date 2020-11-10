/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/adaptation/quality_scaler_resource.h"

#include <memory>

#include "absl/types/optional.h"
#include "api/task_queue/task_queue_base.h"
#include "api/video_codecs/video_encoder.h"
#include "call/adaptation/test/mock_resource_listener.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

using testing::_;
using testing::Eq;
using testing::StrictMock;

namespace {

class FakeDegradationPreferenceProvider : public DegradationPreferenceProvider {
 public:
  ~FakeDegradationPreferenceProvider() override = default;

  DegradationPreference degradation_preference() const override {
    return DegradationPreference::MAINTAIN_FRAMERATE;
  }
};

}  // namespace

class QualityScalerResourceTest : public ::testing::Test {
 public:
  QualityScalerResourceTest()
      : quality_scaler_resource_(QualityScalerResource::Create()) {
    quality_scaler_resource_->RegisterEncoderTaskQueue(
        TaskQueueBase::Current());
    quality_scaler_resource_->SetResourceListener(&fake_resource_listener_);
  }

  ~QualityScalerResourceTest() override {
    quality_scaler_resource_->SetResourceListener(nullptr);
  }

 protected:
  StrictMock<MockResourceListener> fake_resource_listener_;
  FakeDegradationPreferenceProvider degradation_preference_provider_;
  rtc::scoped_refptr<QualityScalerResource> quality_scaler_resource_;
};

TEST_F(QualityScalerResourceTest, ReportQpHigh) {
  EXPECT_CALL(fake_resource_listener_,
              OnResourceUsageStateMeasured(Eq(quality_scaler_resource_),
                                           Eq(ResourceUsageState::kOveruse)));
  quality_scaler_resource_->OnReportQpUsageHigh();
}

TEST_F(QualityScalerResourceTest, ReportQpLow) {
  EXPECT_CALL(fake_resource_listener_,
              OnResourceUsageStateMeasured(Eq(quality_scaler_resource_),
                                           Eq(ResourceUsageState::kUnderuse)));
  quality_scaler_resource_->OnReportQpUsageLow();
}

}  // namespace webrtc
