/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <utility>

#include "absl/memory/memory.h"
#include "modules/audio_processing/aec_dump/mock_aec_dump.h"
#include "modules/audio_processing/include/audio_processing.h"

using testing::_;
using testing::AtLeast;
using testing::Exactly;
using testing::Matcher;
using testing::StrictMock;

namespace {
std::unique_ptr<webrtc::AudioProcessing> CreateAudioProcessing() {
  webrtc::Config config;
  std::unique_ptr<webrtc::AudioProcessing> apm(
      webrtc::AudioProcessingBuilder().Create(config));
  RTC_DCHECK(apm);
  return apm;
}

std::unique_ptr<webrtc::test::MockAecDump> CreateMockAecDump() {
  auto mock_aec_dump =
      absl::make_unique<testing::StrictMock<webrtc::test::MockAecDump>>();
  EXPECT_CALL(*mock_aec_dump.get(), WriteConfig(_)).Times(AtLeast(1));
  EXPECT_CALL(*mock_aec_dump.get(), WriteInitMessage(_, _)).Times(AtLeast(1));
  return std::unique_ptr<webrtc::test::MockAecDump>(std::move(mock_aec_dump));
}

std::unique_ptr<webrtc::AudioFrame> CreateFakeFrame() {
  auto fake_frame = absl::make_unique<webrtc::AudioFrame>();
  fake_frame->num_channels_ = 1;
  fake_frame->sample_rate_hz_ = 48000;
  fake_frame->samples_per_channel_ = 480;
  return fake_frame;
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
  auto fake_frame = CreateFakeFrame();

  EXPECT_CALL(*mock_aec_dump.get(),
              WriteRenderStreamMessage(Matcher<const webrtc::AudioFrame&>(_)))
      .Times(Exactly(1));

  apm->AttachAecDump(std::move(mock_aec_dump));
  apm->ProcessReverseStream(fake_frame.get());
}

TEST(AecDumpIntegration, CaptureStreamShouldBeLoggedOnceEveryProcessStream) {
  auto apm = CreateAudioProcessing();
  auto mock_aec_dump = CreateMockAecDump();
  auto fake_frame = CreateFakeFrame();

  EXPECT_CALL(*mock_aec_dump.get(),
              AddCaptureStreamInput(Matcher<const webrtc::AudioFrame&>(_)))
      .Times(AtLeast(1));

  EXPECT_CALL(*mock_aec_dump.get(),
              AddCaptureStreamOutput(Matcher<const webrtc::AudioFrame&>(_)))
      .Times(Exactly(1));

  EXPECT_CALL(*mock_aec_dump.get(), AddAudioProcessingState(_))
      .Times(Exactly(1));

  EXPECT_CALL(*mock_aec_dump.get(), WriteCaptureStreamMessage())
      .Times(Exactly(1));

  apm->AttachAecDump(std::move(mock_aec_dump));
  apm->ProcessStream(fake_frame.get());
}
