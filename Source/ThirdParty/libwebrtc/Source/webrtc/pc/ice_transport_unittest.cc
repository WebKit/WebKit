/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/ice_transport.h"

#include <memory>
#include <utility>
#include <vector>

#include "api/ice_transport_factory.h"
#include "p2p/base/fake_ice_transport.h"
#include "p2p/base/fake_port_allocator.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

class IceTransportTest : public ::testing::Test {};

TEST_F(IceTransportTest, CreateNonSelfDeletingTransport) {
  auto cricket_transport =
      std::make_unique<cricket::FakeIceTransport>("name", 0, nullptr);
  rtc::scoped_refptr<IceTransportWithPointer> ice_transport =
      new rtc::RefCountedObject<IceTransportWithPointer>(
          cricket_transport.get());
  EXPECT_EQ(ice_transport->internal(), cricket_transport.get());
  ice_transport->Clear();
  EXPECT_NE(ice_transport->internal(), cricket_transport.get());
}

TEST_F(IceTransportTest, CreateSelfDeletingTransport) {
  std::unique_ptr<cricket::FakePortAllocator> port_allocator(
      std::make_unique<cricket::FakePortAllocator>(nullptr, nullptr));
  IceTransportInit init;
  init.set_port_allocator(port_allocator.get());
  auto ice_transport = CreateIceTransport(std::move(init));
  EXPECT_NE(nullptr, ice_transport->internal());
}

}  // namespace webrtc
