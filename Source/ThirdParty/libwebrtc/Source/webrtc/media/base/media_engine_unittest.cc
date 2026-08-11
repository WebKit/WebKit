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

#include <cstdint>
#include <optional>
#include <vector>

#include "api/audio/audio_device.h"
#include "api/field_trials_view.h"
#include "api/rtp_parameters.h"
#include "api/rtp_transceiver_direction.h"
#include "api/scoped_refptr.h"
#include "call/audio_state.h"
#include "media/base/codec.h"
#include "rtc_base/system/file_wrapper.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::Return;
using ::testing::StrEq;
using ::webrtc::RtpExtension;
using ::webrtc::RtpHeaderExtensionCapability;
using ::webrtc::RtpTransceiverDirection;

namespace webrtc {
namespace {

class MockRtpHeaderExtensionQueryInterface
    : public RtpHeaderExtensionQueryInterface {
 public:
  MOCK_METHOD(std::vector<RtpHeaderExtensionCapability>,
              GetRtpHeaderExtensions,
              (const FieldTrialsView*),
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
  EXPECT_THAT(GetDefaultEnabledRtpHeaderExtensions(mock, nullptr),
              ElementsAre(Field(&RtpExtension::uri, StrEq("uri1")),
                          Field(&RtpExtension::uri, StrEq("uri2")),
                          Field(&RtpExtension::uri, StrEq("uri4")),
                          Field(&RtpExtension::uri, StrEq("uri5"))));
}

// This class mocks methods declared as pure virtual in the interface.
// Since the tests are aiming to check the patterns of overrides, the
// functions with default implementations are not mocked.
class MostlyMockVoiceEngineInterface : public VoiceEngineInterface {
 public:
  MOCK_METHOD(std::vector<RtpHeaderExtensionCapability>,
              GetRtpHeaderExtensions,
              (const FieldTrialsView*),
              (const, override));
  MOCK_METHOD(void, Init, (), (override));
  MOCK_METHOD(void, Terminate, (), (override));
  MOCK_METHOD(scoped_refptr<AudioState>, GetAudioState, (), (const, override));
  MOCK_METHOD(std::vector<Codec>&, LegacySendCodecs, (), (const, override));
  MOCK_METHOD(std::vector<Codec>&, LegacyRecvCodecs, (), (const, override));
  MOCK_METHOD(bool,
              StartAecDump,
              (FileWrapper file, int64_t max_size_bytes),
              (override));
  MOCK_METHOD(void, StopAecDump, (), (override));
  MOCK_METHOD(std::optional<AudioDeviceModule::Stats>,
              GetAudioDeviceStats,
              (),
              (override));
};

}  // namespace webrtc
