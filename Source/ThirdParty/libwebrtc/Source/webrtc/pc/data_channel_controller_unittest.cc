/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/data_channel_controller.h"

#include <memory>

#include "pc/peer_connection_internal.h"
#include "pc/sctp_data_channel.h"
#include "pc/test/mock_peer_connection_internal.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

using ::testing::NiceMock;
using ::testing::Return;

class DataChannelControllerTest : public ::testing::Test {
 protected:
  DataChannelControllerTest() {
    pc_ = rtc::make_ref_counted<NiceMock<MockPeerConnectionInternal>>();
    ON_CALL(*pc_, signaling_thread)
        .WillByDefault(Return(rtc::Thread::Current()));
  }

  rtc::AutoThread main_thread_;
  rtc::scoped_refptr<NiceMock<MockPeerConnectionInternal>> pc_;
};

TEST_F(DataChannelControllerTest, CreateAndDestroy) {
  DataChannelController dcc(pc_.get());
}

TEST_F(DataChannelControllerTest, CreateDataChannelEarlyRelease) {
  DataChannelController dcc(pc_.get());
  auto channel = dcc.InternalCreateDataChannelWithProxy(
      "label",
      std::make_unique<InternalDataChannelInit>(DataChannelInit()).get());
  channel = nullptr;  // dcc holds a reference to channel, so not destroyed yet
}

TEST_F(DataChannelControllerTest, CreateDataChannelLateRelease) {
  auto dcc = std::make_unique<DataChannelController>(pc_.get());
  auto channel = dcc->InternalCreateDataChannelWithProxy(
      "label",
      std::make_unique<InternalDataChannelInit>(DataChannelInit()).get());
  dcc.reset();
  channel = nullptr;
}

TEST_F(DataChannelControllerTest, CloseAfterControllerDestroyed) {
  auto dcc = std::make_unique<DataChannelController>(pc_.get());
  auto channel = dcc->InternalCreateDataChannelWithProxy(
      "label",
      std::make_unique<InternalDataChannelInit>(DataChannelInit()).get());
  // Connect to provider
  auto inner_channel =
      DowncastProxiedDataChannelInterfaceToSctpDataChannelForTesting(
          channel.get());
  dcc->ConnectDataChannel(inner_channel);
  dcc.reset();
  channel->Close();
}

}  // namespace
}  // namespace webrtc
