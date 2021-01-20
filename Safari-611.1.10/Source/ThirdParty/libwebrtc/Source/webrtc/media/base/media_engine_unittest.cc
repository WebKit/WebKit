/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/media_engine.h"

#include "test/gmock.h"

using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::Return;
using ::testing::StrEq;
using ::webrtc::RtpExtension;
using ::webrtc::RtpHeaderExtensionCapability;
using ::webrtc::RtpTransceiverDirection;

namespace cricket {
namespace {

class MockRtpHeaderExtensionQueryInterface
    : public RtpHeaderExtensionQueryInterface {
 public:
  MOCK_METHOD(std::vector<RtpHeaderExtensionCapability>,
              GetRtpHeaderExtensions,
              (),
              (const, override));
};

}  // namespace

TEST(MediaEngineTest, ReturnsNotStoppedHeaderExtensions) {
  MockRtpHeaderExtensionQueryInterface mock;
  std::vector<RtpHeaderExtensionCapability> extensions(
      {RtpHeaderExtensionCapability("uri1", 1,
                                    RtpTransceiverDirection::kInactive),
       RtpHeaderExtensionCapability("uri2", 2,
                                    RtpTransceiverDirection::kSendRecv),
       RtpHeaderExtensionCapability("uri3", 3,
                                    RtpTransceiverDirection::kStopped),
       RtpHeaderExtensionCapability("uri4", 4,
                                    RtpTransceiverDirection::kSendOnly),
       RtpHeaderExtensionCapability("uri5", 5,
                                    RtpTransceiverDirection::kRecvOnly)});
  EXPECT_CALL(mock, GetRtpHeaderExtensions).WillOnce(Return(extensions));
  EXPECT_THAT(GetDefaultEnabledRtpHeaderExtensions(mock),
              ElementsAre(Field(&RtpExtension::uri, StrEq("uri1")),
                          Field(&RtpExtension::uri, StrEq("uri2")),
                          Field(&RtpExtension::uri, StrEq("uri4")),
                          Field(&RtpExtension::uri, StrEq("uri5"))));
}

}  // namespace cricket
