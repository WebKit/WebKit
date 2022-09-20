/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <array>
#include <memory>
#include <utility>

#include "modules/audio_processing/aec_dump/mock_aec_dump.h"
#include "modules/audio_processing/audio_processing_impl.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/audio_processing/test/audio_processing_builder_for_testing.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Exactly;
using ::testing::StrictMock;

namespace {
rtc::scoped_refptr<webrtc::AudioProcessing> CreateAudioProcessing() {
  rtc::scoped_refptr<webrtc::AudioProcessing> apm(
      webrtc::AudioProcessingBuilderForTesting().Create());
  RTC_DCHECK(apm);
  return apm;
}

std::unique_ptr<webrtc::test::MockAecDump> CreateMockAecDump() {
  auto mock_aec_dump =
      std::make_unique<testing::StrictMock<webrtc::test::MockAecDump>>();
  EXPECT_CALL(*mock_aec_dump.get(), WriteConfig(_)).Times(AtLeast(1));
  EXPECT_CALL(*mock_aec_dump.get(), WriteInitMessage(_, _)).Times(AtLeast(1));
  return std::unique_ptr<webrtc::test::MockAecDump>(std::move(mock_aec_dump));
}

}  // namespace

TEST(AecDumpIntegration, ConfigurationAndInitShouldBeLogged) {
  auto apm = CreateAudioProcessing();

  apm->AttachAecDump(CreateMockAecDump());
}

TEST(AecDumpIntegration,
     RenderStreamShouldBeLoggedOnceEveryProcessReverseStream) {
  auto apm = CreateAudioProcessing();
  auto mock_aec_dump = CreateMockAecDump();
  constexpr int kNumChannels = 1;
  constexpr int kNumSampleRateHz = 16000;
  constexpr int kNumSamplesPerChannel = kNumSampleRateHz / 100;
  std::array<int16_t, kNumSamplesPerChannel * kNumChannels> frame;
  frame.fill(0.f);
  webrtc::StreamConfig stream_config(kNumSampleRateHz, kNumChannels);

  EXPECT_CALL(*mock_aec_dump.get(), WriteRenderStreamMessage(_, _, _))
      .Times(Exactly(1));

  apm->AttachAecDump(std::move(mock_aec_dump));
  apm->ProcessReverseStream(frame.data(), stream_config, stream_config,
                            frame.data());
}

TEST(AecDumpIntegration, CaptureStreamShouldBeLoggedOnceEveryProcessStream) {
  auto apm = CreateAudioProcessing();
  auto mock_aec_dump = CreateMockAecDump();
  constexpr int kNumChannels = 1;
  constexpr int kNumSampleRateHz = 16000;
  constexpr int kNumSamplesPerChannel = kNumSampleRateHz / 100;
  std::array<int16_t, kNumSamplesPerChannel * kNumChannels> frame;
  frame.fill(0.f);

  webrtc::StreamConfig stream_config(kNumSampleRateHz, kNumChannels);

  EXPECT_CALL(*mock_aec_dump.get(), AddCaptureStreamInput(_, _, _))
      .Times(AtLeast(1));

  EXPECT_CALL(*mock_aec_dump.get(), AddCaptureStreamOutput(_, _, _))
      .Times(Exactly(1));

  EXPECT_CALL(*mock_aec_dump.get(), AddAudioProcessingState(_))
      .Times(Exactly(1));

  EXPECT_CALL(*mock_aec_dump.get(), WriteCaptureStreamMessage())
      .Times(Exactly(1));

  apm->AttachAecDump(std::move(mock_aec_dump));
  apm->ProcessStream(frame.data(), stream_config, stream_config, frame.data());
}
