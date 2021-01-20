/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/adaptation/resource.h"

#include <memory>

#include "api/scoped_refptr.h"
#include "call/adaptation/test/fake_resource.h"
#include "call/adaptation/test/mock_resource_listener.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

using ::testing::_;
using ::testing::StrictMock;

class ResourceTest : public ::testing::Test {
 public:
  ResourceTest() : fake_resource_(FakeResource::Create("FakeResource")) {}

 protected:
  rtc::scoped_refptr<FakeResource> fake_resource_;
};

TEST_F(ResourceTest, RegisteringListenerReceivesCallbacks) {
  StrictMock<MockResourceListener> resource_listener;
  fake_resource_->SetResourceListener(&resource_listener);
  EXPECT_CALL(resource_listener, OnResourceUsageStateMeasured(_, _))
      .Times(1)
      .WillOnce([](rtc::scoped_refptr<Resource> resource,
                   ResourceUsageState usage_state) {
        EXPECT_EQ(ResourceUsageState::kOveruse, usage_state);
      });
  fake_resource_->SetUsageState(ResourceUsageState::kOveruse);
  fake_resource_->SetResourceListener(nullptr);
}

TEST_F(ResourceTest, UnregisteringListenerStopsCallbacks) {
  StrictMock<MockResourceListener> resource_listener;
  fake_resource_->SetResourceListener(&resource_listener);
  fake_resource_->SetResourceListener(nullptr);
  EXPECT_CALL(resource_listener, OnResourceUsageStateMeasured(_, _)).Times(0);
  fake_resource_->SetUsageState(ResourceUsageState::kOveruse);
}

}  // namespace webrtc
