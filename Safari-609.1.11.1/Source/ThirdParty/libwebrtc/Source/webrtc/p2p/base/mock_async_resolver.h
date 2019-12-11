/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_MOCK_ASYNC_RESOLVER_H_
#define P2P_BASE_MOCK_ASYNC_RESOLVER_H_

#include "api/async_resolver_factory.h"
#include "rtc_base/async_resolver_interface.h"
#include "test/gmock.h"

namespace rtc {

using ::testing::_;
using ::testing::InvokeWithoutArgs;

class MockAsyncResolver : public AsyncResolverInterface {
 public:
  MockAsyncResolver() {
    ON_CALL(*this, Start(_)).WillByDefault(InvokeWithoutArgs([this] {
      SignalDone(this);
    }));
  }
  ~MockAsyncResolver() = default;

  MOCK_METHOD1(Start, void(const rtc::SocketAddress&));
  MOCK_CONST_METHOD2(GetResolvedAddress, bool(int family, SocketAddress* addr));
  MOCK_CONST_METHOD0(GetError, int());

  // Note that this won't delete the object like AsyncResolverInterface says in
  // order to avoid sanitizer failures caused by this being a synchronous
  // implementation. The test code should delete the object instead.
  MOCK_METHOD1(Destroy, void(bool));
};

}  // namespace rtc

namespace webrtc {

class MockAsyncResolverFactory : public AsyncResolverFactory {
 public:
  MOCK_METHOD0(Create, rtc::AsyncResolverInterface*());
};

}  // namespace webrtc

#endif  // P2P_BASE_MOCK_ASYNC_RESOLVER_H_
