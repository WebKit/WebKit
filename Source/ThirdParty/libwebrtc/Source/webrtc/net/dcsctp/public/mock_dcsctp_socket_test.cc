/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/public/mock_dcsctp_socket.h"

#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace dcsctp {

// This test exists to ensure that all methods are mocked correctly, and to
// generate compiler errors if they are not.
TEST(MockDcSctpSocketTest, CanInstantiateAndConnect) {
  testing::StrictMock<MockDcSctpSocket> socket;

  EXPECT_CALL(socket, Connect);

  socket.Connect();
}

}  // namespace dcsctp
