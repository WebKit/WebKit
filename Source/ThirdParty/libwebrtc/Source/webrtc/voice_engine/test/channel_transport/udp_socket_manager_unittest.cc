/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Tests for the UdpSocketManager interface.
// Note: This tests UdpSocketManager together with UdpSocketWrapper,
// due to the way the code is full of static-casts to the platform dependent
// subtypes.
// It also uses the static UdpSocketManager object.
// The most important property of these tests is that they do not leak memory.

#include "webrtc/system_wrappers/include/trace.h"
#include "webrtc/test/gtest.h"
#include "webrtc/voice_engine/test/channel_transport/udp_socket_manager_wrapper.h"
#include "webrtc/voice_engine/test/channel_transport/udp_socket_wrapper.h"

namespace webrtc {
namespace test {

TEST(UdpSocketManager, CreateCallsInitAndDoesNotLeakMemory) {
  int32_t id = 42;
  uint8_t threads = 1;
  UdpSocketManager* mgr = UdpSocketManager::Create(id, threads);
  // Create is supposed to have called init on the object.
  EXPECT_FALSE(mgr->Init(id, threads))
      << "Init should return false since Create is supposed to call it.";
  UdpSocketManager::Return();
}

// Creates a socket and adds it to the socket manager, and then removes it
// before destroying the socket manager.
TEST(UdpSocketManager, AddAndRemoveSocketDoesNotLeakMemory) {
  int32_t id = 42;
  uint8_t threads = 1;
  UdpSocketManager* mgr = UdpSocketManager::Create(id, threads);
  UdpSocketWrapper* socket =
      UdpSocketWrapper::CreateSocket(id,
                                     mgr,
                                     NULL,  // CallbackObj
                                     NULL,  // IncomingSocketCallback
                                     false,  // ipV6Enable
                                     false);  // disableGQOS
  // The constructor will do AddSocket on the manager.
  // RemoveSocket indirectly calls Delete.
  EXPECT_EQ(true, mgr->RemoveSocket(socket));
  UdpSocketManager::Return();
}

// Creates a socket and add it to the socket manager, but does not remove it
// before destroying the socket manager.
// On Posix, this destroys the socket.
// On Winsock2 Windows, it enters an infinite wait for all the sockets
// to go away.
TEST(UdpSocketManager, UnremovedSocketsGetCollectedAtManagerDeletion) {
#if defined(_WIN32)
  // It's hard to test an infinite wait, so we don't.
#else
  int32_t id = 42;
  uint8_t threads = 1;
  UdpSocketManager* mgr = UdpSocketManager::Create(id, threads);
  UdpSocketWrapper* unused_socket = UdpSocketWrapper::CreateSocket(
      id,
      mgr,
      NULL,  // CallbackObj
      NULL,  // IncomingSocketCallback
      false,  // ipV6Enable
      false);  // disableGQOS
  // The constructor will do AddSocket on the manager.
  // Call a member funtion to work around "set but not used" compliation
  // error on ChromeOS ARM.
  unused_socket->SetEventToNull();
  unused_socket = NULL;
  UdpSocketManager::Return();
#endif
}

}  // namespace test
}  // namespace webrtc
