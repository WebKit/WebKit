/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/adaptation/broadcast_resource_listener.h"

#include "call/adaptation/test/fake_resource.h"
#include "call/adaptation/test/mock_resource_listener.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

using ::testing::_;
using ::testing::StrictMock;

TEST(BroadcastResourceListenerTest, CreateAndRemoveAdapterResource) {
  rtc::scoped_refptr<FakeResource> source_resource =
      FakeResource::Create("SourceResource");
  BroadcastResourceListener broadcast_resource_listener(source_resource);
  broadcast_resource_listener.StartListening();

  EXPECT_TRUE(broadcast_resource_listener.GetAdapterResources().empty());
  rtc::scoped_refptr<Resource> adapter =
      broadcast_resource_listener.CreateAdapterResource();
  StrictMock<MockResourceListener> listener;
  adapter->SetResourceListener(&listener);
  EXPECT_EQ(std::vector<rtc::scoped_refptr<Resource>>{adapter},
            broadcast_resource_listener.GetAdapterResources());

  // The removed adapter is not referenced by the broadcaster.
  broadcast_resource_listener.RemoveAdapterResource(adapter);
  EXPECT_TRUE(broadcast_resource_listener.GetAdapterResources().empty());
  // The removed adapter is not forwarding measurements.
  EXPECT_CALL(listener, OnResourceUsageStateMeasured(_, _)).Times(0);
  source_resource->SetUsageState(ResourceUsageState::kOveruse);
  // Cleanup.
  adapter->SetResourceListener(nullptr);
  broadcast_resource_listener.StopListening();
}

TEST(BroadcastResourceListenerTest, AdapterNameIsBasedOnSourceResourceName) {
  rtc::scoped_refptr<FakeResource> source_resource =
      FakeResource::Create("FooBarResource");
  BroadcastResourceListener broadcast_resource_listener(source_resource);
  broadcast_resource_listener.StartListening();

  rtc::scoped_refptr<Resource> adapter =
      broadcast_resource_listener.CreateAdapterResource();
  EXPECT_EQ("FooBarResourceAdapter", adapter->Name());

  broadcast_resource_listener.RemoveAdapterResource(adapter);
  broadcast_resource_listener.StopListening();
}

TEST(BroadcastResourceListenerTest, AdaptersForwardsUsageMeasurements) {
  rtc::scoped_refptr<FakeResource> source_resource =
      FakeResource::Create("SourceResource");
  BroadcastResourceListener broadcast_resource_listener(source_resource);
  broadcast_resource_listener.StartListening();

  StrictMock<MockResourceListener> destination_listener1;
  StrictMock<MockResourceListener> destination_listener2;
  rtc::scoped_refptr<Resource> adapter1 =
      broadcast_resource_listener.CreateAdapterResource();
  adapter1->SetResourceListener(&destination_listener1);
  rtc::scoped_refptr<Resource> adapter2 =
      broadcast_resource_listener.CreateAdapterResource();
  adapter2->SetResourceListener(&destination_listener2);

  // Expect kOveruse to be echoed.
  EXPECT_CALL(destination_listener1, OnResourceUsageStateMeasured(_, _))
      .Times(1)
      .WillOnce([adapter1](rtc::scoped_refptr<Resource> resource,
                           ResourceUsageState usage_state) {
        EXPECT_EQ(adapter1, resource);
        EXPECT_EQ(ResourceUsageState::kOveruse, usage_state);
      });
  EXPECT_CALL(destination_listener2, OnResourceUsageStateMeasured(_, _))
      .Times(1)
      .WillOnce([adapter2](rtc::scoped_refptr<Resource> resource,
                           ResourceUsageState usage_state) {
        EXPECT_EQ(adapter2, resource);
        EXPECT_EQ(ResourceUsageState::kOveruse, usage_state);
      });
  source_resource->SetUsageState(ResourceUsageState::kOveruse);

  // Expect kUnderuse to be echoed.
  EXPECT_CALL(destination_listener1, OnResourceUsageStateMeasured(_, _))
      .Times(1)
      .WillOnce([adapter1](rtc::scoped_refptr<Resource> resource,
                           ResourceUsageState usage_state) {
        EXPECT_EQ(adapter1, resource);
        EXPECT_EQ(ResourceUsageState::kUnderuse, usage_state);
      });
  EXPECT_CALL(destination_listener2, OnResourceUsageStateMeasured(_, _))
      .Times(1)
      .WillOnce([adapter2](rtc::scoped_refptr<Resource> resource,
                           ResourceUsageState usage_state) {
        EXPECT_EQ(adapter2, resource);
        EXPECT_EQ(ResourceUsageState::kUnderuse, usage_state);
      });
  source_resource->SetUsageState(ResourceUsageState::kUnderuse);

  // Adapters have to be unregistered before they or the broadcaster is
  // destroyed, ensuring safe use of raw pointers.
  adapter1->SetResourceListener(nullptr);
  adapter2->SetResourceListener(nullptr);

  broadcast_resource_listener.RemoveAdapterResource(adapter1);
  broadcast_resource_listener.RemoveAdapterResource(adapter2);
  broadcast_resource_listener.StopListening();
}

}  // namespace webrtc
