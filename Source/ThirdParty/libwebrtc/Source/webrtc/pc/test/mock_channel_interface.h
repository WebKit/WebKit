/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_TEST_MOCK_CHANNEL_INTERFACE_H_
#define PC_TEST_MOCK_CHANNEL_INTERFACE_H_

#include <string>
#include <vector>

#include "pc/channel_interface.h"
#include "test/gmock.h"

namespace cricket {

// Mock class for BaseChannel.
// Use this class in unit tests to avoid dependecy on a specific
// implementation of BaseChannel.
class MockChannelInterface : public cricket::ChannelInterface {
 public:
  MOCK_METHOD(cricket::MediaType, media_type, (), (const, override));
  MOCK_METHOD(MediaChannel*, media_channel, (), (const, override));
  MOCK_METHOD(const std::string&, transport_name, (), (const, override));
  MOCK_METHOD(const std::string&, content_name, (), (const, override));
  MOCK_METHOD(void, Enable, (bool), (override));
  MOCK_METHOD(void,
              SetFirstPacketReceivedCallback,
              (std::function<void()>),
              (override));
  MOCK_METHOD(bool,
              SetLocalContent,
              (const cricket::MediaContentDescription*,
               webrtc::SdpType,
               std::string*),
              (override));
  MOCK_METHOD(bool,
              SetRemoteContent,
              (const cricket::MediaContentDescription*,
               webrtc::SdpType,
               std::string*),
              (override));
  MOCK_METHOD(bool, SetPayloadTypeDemuxingEnabled, (bool), (override));
  MOCK_METHOD(const std::vector<StreamParams>&,
              local_streams,
              (),
              (const, override));
  MOCK_METHOD(const std::vector<StreamParams>&,
              remote_streams,
              (),
              (const, override));
  MOCK_METHOD(bool,
              SetRtpTransport,
              (webrtc::RtpTransportInternal*),
              (override));
};

}  // namespace cricket

#endif  // PC_TEST_MOCK_CHANNEL_INTERFACE_H_
