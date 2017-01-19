/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/voice_engine/test/auto_test/fixtures/after_streaming_fixture.h"
#include "webrtc/voice_engine/test/auto_test/voe_standard_test.h"


class FileTest : public AfterStreamingFixture {
 protected:
  // Creates the string åäö.pcm.
// TODO(henrika): enable this test once CreateTrickyFilenameInUtf8 no longer
// prevents compilation on Windows. Likely webrtc/base can be used here.
#if 0
  std::string CreateTrickyFilenameInUtf8() {
    char filename[16] = { (char)0xc3, (char)0xa5,
                          (char)0xc3, (char)0xa4,
                          (char)0xc3, (char)0xb6,
                          static_cast<char>(0) };
    return std::string(filename) + ".pcm";
  }
#endif  // 0
};

// TODO(henrika): enable this test once CreateTrickyFilenameInUtf8 no longer
// prevents compilation on Windows. Likely webrtc/base can be used here.
#if 0
TEST_F(FileTest, ManualRecordToFileForThreeSecondsAndPlayback) {
  if (!FLAGS_include_timing_dependent_tests) {
    TEST_LOG("Skipping test - running in slow execution environment...\n");
    return;
  }

  SwitchToManualMicrophone();

  std::string recording_filename =
      webrtc::test::OutputPath() + CreateTrickyFilenameInUtf8();

  TEST_LOG("Recording to %s for 3 seconds.\n", recording_filename.c_str());
  EXPECT_EQ(0, voe_file_->StartRecordingMicrophone(recording_filename.c_str()));
  Sleep(3000);
  EXPECT_EQ(0, voe_file_->StopRecordingMicrophone());

  TEST_LOG("Playing back %s.\n", recording_filename.c_str());
  EXPECT_EQ(0, voe_file_->StartPlayingFileLocally(
      channel_, recording_filename.c_str()));

  // Play the file to the user and ensure the is-playing-locally.
  // The clip is 3 seconds long.
  Sleep(250);
  EXPECT_EQ(1, voe_file_->IsPlayingFileLocally(channel_));
  Sleep(1500);
}
#endif  // 0

TEST_F(FileTest, ManualRecordPlayoutToWavFileForThreeSecondsAndPlayback) {
  webrtc::CodecInst send_codec;
  voe_codec_->GetSendCodec(channel_, send_codec);

  std::string recording_filename =
      webrtc::test::OutputPath() + "playout.wav";

  TEST_LOG("Recording playout to %s.\n", recording_filename.c_str());
  EXPECT_EQ(0, voe_file_->StartRecordingPlayout(
      channel_, recording_filename.c_str(), &send_codec));
  Sleep(3000);
  EXPECT_EQ(0, voe_file_->StopRecordingPlayout(channel_));

  TEST_LOG("Playing back the recording in looping mode.\n");
  EXPECT_EQ(0, voe_file_->StartPlayingFileAsMicrophone(
      channel_, recording_filename.c_str(), true, false,
      webrtc::kFileFormatWavFile));

  Sleep(2000);
  EXPECT_EQ(1, voe_file_->IsPlayingFileAsMicrophone(channel_));
  Sleep(2000);
  // We should still be playing since we're looping.
  EXPECT_EQ(1, voe_file_->IsPlayingFileAsMicrophone(channel_));
}
