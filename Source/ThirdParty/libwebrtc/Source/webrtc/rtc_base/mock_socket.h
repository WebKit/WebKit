/*
 *  Copyright 2025 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_MOCK_SOCKET_H_
#define RTC_BASE_MOCK_SOCKET_H_

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "rtc_base/socket.h"
#include "rtc_base/socket_address.h"
#include "test/gmock.h"

namespace webrtc {

class MockSocket : public Socket {
 public:
  MOCK_METHOD(SocketAddress, GetLocalAddress, (), (const, override));
  MOCK_METHOD(SocketAddress, GetRemoteAddress, (), (const, override));
  MOCK_METHOD(int, Bind, (const SocketAddress& addr), (override));
  MOCK_METHOD(int, Connect, (const SocketAddress& addr), (override));
  MOCK_METHOD(int, Send, (const void* pv, size_t cb), (override));
  MOCK_METHOD(int,
              SendTo,
              (const void* pv, size_t cb, const SocketAddress& addr),
              (override));
  MOCK_METHOD(int, Recv, (void* pv, size_t cb, int64_t* timestamp), (override));
  MOCK_METHOD(int,
              RecvFrom,
              (void* pv, size_t cb, SocketAddress* paddr, int64_t* timestamp),
              (override));
  MOCK_METHOD(int, RecvFrom, (ReceiveBuffer & buffer), (override));
  MOCK_METHOD(int, Listen, (int backlog), (override));
  MOCK_METHOD(Socket*, Accept, (SocketAddress * paddr), (override));
  MOCK_METHOD(int, Close, (), (override));
  MOCK_METHOD(int, GetError, (), (const, override));
  MOCK_METHOD(void, SetError, (int error), (override));
  MOCK_METHOD(ConnState, GetState, (), (const, override));
  MOCK_METHOD(int, GetOption, (Option opt, int* value), (override));
  MOCK_METHOD(int, SetOption, (Option opt, int value), (override));
};

static_assert(!std::is_abstract_v<MockSocket>, "");

}  // namespace webrtc

#endif  // RTC_BASE_MOCK_SOCKET_H_
