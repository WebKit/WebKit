/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/adaptation/resource.h"

#include "call/adaptation/test/fake_resource.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

using ::testing::_;
using ::testing::StrictMock;

class MockResourceListener : public ResourceListener {
 public:
  MOCK_METHOD(ResourceListenerResponse,
              OnResourceUsageStateMeasured,
              (const Resource& resource));
};

TEST(ResourceTest, AddingListenerReceivesCallbacks) {
  StrictMock<MockResourceListener> resource_listener;
  FakeResource fake_resource(ResourceUsageState::kStable);
  fake_resource.RegisterListener(&resource_listener);
  EXPECT_CALL(resource_listener, OnResourceUsageStateMeasured(_))
      .Times(1)
      .WillOnce([](const Resource& resource) {
        EXPECT_EQ(ResourceUsageState::kOveruse, resource.usage_state());
        return ResourceListenerResponse::kNothing;
      });
  fake_resource.set_usage_state(ResourceUsageState::kOveruse);
  fake_resource.UnregisterListener(&resource_listener);
}

TEST(ResourceTest, RemovingListenerStopsCallbacks) {
  StrictMock<MockResourceListener> resource_listener;
  FakeResource fake_resource(ResourceUsageState::kStable);
  fake_resource.RegisterListener(&resource_listener);
  fake_resource.UnregisterListener(&resource_listener);
  EXPECT_CALL(resource_listener, OnResourceUsageStateMeasured(_)).Times(0);
  fake_resource.set_usage_state(ResourceUsageState::kOveruse);
}

}  // namespace webrtc
