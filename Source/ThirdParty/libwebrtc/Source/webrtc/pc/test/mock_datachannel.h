/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_PC_TEST_MOCK_DATACHANNEL_H_
#define WEBRTC_PC_TEST_MOCK_DATACHANNEL_H_

#include "webrtc/pc/datachannel.h"
#include "webrtc/test/gmock.h"

namespace webrtc {

class MockDataChannel : public rtc::RefCountedObject<DataChannel> {
 public:
  MockDataChannel(int id, DataState state)
      : MockDataChannel(id, "MockDataChannel", state, "udp", 0, 0, 0, 0) {
  }
  MockDataChannel(
      int id,
      const std::string& label,
      DataState state,
      const std::string& protocol,
      uint32_t messages_sent,
      uint64_t bytes_sent,
      uint32_t messages_received,
      uint64_t bytes_received)
      : rtc::RefCountedObject<DataChannel>(
            nullptr, cricket::DCT_NONE, label) {
    EXPECT_CALL(*this, id()).WillRepeatedly(testing::Return(id));
    EXPECT_CALL(*this, state()).WillRepeatedly(testing::Return(state));
    EXPECT_CALL(*this, protocol()).WillRepeatedly(testing::Return(protocol));
    EXPECT_CALL(*this, messages_sent()).WillRepeatedly(
        testing::Return(messages_sent));
    EXPECT_CALL(*this, bytes_sent()).WillRepeatedly(
        testing::Return(bytes_sent));
    EXPECT_CALL(*this, messages_received()).WillRepeatedly(
        testing::Return(messages_received));
    EXPECT_CALL(*this, bytes_received()).WillRepeatedly(
        testing::Return(bytes_received));
  }
  MOCK_CONST_METHOD0(id, int());
  MOCK_CONST_METHOD0(state, DataState());
  MOCK_CONST_METHOD0(protocol, std::string());
  MOCK_CONST_METHOD0(messages_sent, uint32_t());
  MOCK_CONST_METHOD0(bytes_sent, uint64_t());
  MOCK_CONST_METHOD0(messages_received, uint32_t());
  MOCK_CONST_METHOD0(bytes_received, uint64_t());
};

}  // namespace webrtc

#endif  // WEBRTC_PC_TEST_MOCK_DATACHANNEL_H_
