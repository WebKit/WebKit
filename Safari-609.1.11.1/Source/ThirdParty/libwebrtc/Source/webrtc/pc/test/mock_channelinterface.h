/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_TEST_MOCK_CHANNELINTERFACE_H_
#define PC_TEST_MOCK_CHANNELINTERFACE_H_

#include <string>

#include "pc/channelinterface.h"
#include "test/gmock.h"

namespace cricket {

// Mock class for BaseChannel.
// Use this class in unit tests to avoid dependecy on a specific
// implementation of BaseChannel.
class MockChannelInterface : public cricket::ChannelInterface {
 public:
  MOCK_CONST_METHOD0(media_type, cricket::MediaType());
  MOCK_CONST_METHOD0(media_channel, MediaChannel*());
  MOCK_CONST_METHOD0(transport_name, const std::string&());
  MOCK_CONST_METHOD0(content_name, const std::string&());
  MOCK_CONST_METHOD0(enabled, bool());
  MOCK_METHOD1(Enable, bool(bool));
  MOCK_METHOD0(SignalFirstPacketReceived,
               sigslot::signal1<ChannelInterface*>&());
  MOCK_METHOD3(SetLocalContent,
               bool(const cricket::MediaContentDescription*,
                    webrtc::SdpType,
                    std::string*));
  MOCK_METHOD3(SetRemoteContent,
               bool(const cricket::MediaContentDescription*,
                    webrtc::SdpType,
                    std::string*));
  MOCK_METHOD1(SetRtpTransport, bool(webrtc::RtpTransportInternal*));
};

}  // namespace cricket

#endif  // PC_TEST_MOCK_CHANNELINTERFACE_H_
