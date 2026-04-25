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

#include "api/environment/environment.h"
#include "api/ice_transport_factory.h"
#include "api/ice_transport_interface.h"
#include "api/make_ref_counted.h"
#include "api/scoped_refptr.h"
#include "p2p/test/fake_ice_transport.h"
#include "p2p/test/fake_port_allocator.h"
#include "rtc_base/internal/default_socket_server.h"
#include "rtc_base/socket_server.h"
#include "rtc_base/thread.h"
#include "test/create_test_environment.h"
#include "test/gtest.h"

namespace webrtc {

class IceTransportTest : public ::testing::Test {
 protected:
  IceTransportTest()
      : socket_server_(CreateDefaultSocketServer()),
        main_thread_(socket_server_.get()) {}

  SocketServer* socket_server() const { return socket_server_.get(); }

 private:
  std::unique_ptr<SocketServer> socket_server_;
  AutoSocketServerThread main_thread_;
};

TEST_F(IceTransportTest, CreateNonSelfDeletingTransport) {
  auto cricket_transport =
      std::make_unique<FakeIceTransportInternal>("name", 0, nullptr);
  auto ice_transport =
      make_ref_counted<IceTransportWithPointer>(cricket_transport.get());
  EXPECT_EQ(ice_transport->internal(), cricket_transport.get());
  ice_transport->Clear();
  EXPECT_NE(ice_transport->internal(), cricket_transport.get());
}

TEST_F(IceTransportTest, CreateSelfDeletingTransport) {
  Environment env = CreateTestEnvironment();
  FakePortAllocator port_allocator(env, socket_server());
  IceTransportInit init(env);
  init.set_port_allocator(&port_allocator);
  auto ice_transport = CreateIceTransport(std::move(init));
  EXPECT_NE(nullptr, ice_transport->internal());
}

}  // namespace webrtc
