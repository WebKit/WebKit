/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/media_file/media_file.h"
#include "system_wrappers/include/sleep.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

class MediaFileTest : public testing::Test {
 protected:
  void SetUp() {
    // Use number 0 as the the identifier and pass to CreateMediaFile.
    media_file_ = webrtc::MediaFile::CreateMediaFile(0);
    ASSERT_TRUE(media_file_ != NULL);
  }
  void TearDown() {
    webrtc::MediaFile::DestroyMediaFile(media_file_);
    media_file_ = NULL;
  }
  webrtc::MediaFile* media_file_;
};

#if defined(WEBRTC_ANDROID) || defined(WEBRTC_IOS)
#define MAYBE_StartPlayingAudioFileWithoutError \
  DISABLED_StartPlayingAudioFileWithoutError
#else
#define MAYBE_StartPlayingAudioFileWithoutError \
  StartPlayingAudioFileWithoutError
#endif
TEST_F(MediaFileTest, MAYBE_StartPlayingAudioFileWithoutError) {
  // TODO(leozwang): Use hard coded filename here, we want to
  // loop through all audio files in future
  const std::string audio_file =
      webrtc::test::ResourcePath("voice_engine/audio_tiny48", "wav");
  ASSERT_EQ(0, media_file_->StartPlayingAudioFile(
      audio_file.c_str(),
      0,
      false,
      webrtc::kFileFormatWavFile));

  ASSERT_EQ(true, media_file_->IsPlaying());

  webrtc::SleepMs(1);

  ASSERT_EQ(0, media_file_->StopPlaying());
}

#if defined(WEBRTC_IOS)
#define MAYBE_WriteWavFile DISABLED_WriteWavFile
#else
#define MAYBE_WriteWavFile WriteWavFile
#endif
TEST_F(MediaFileTest, MAYBE_WriteWavFile) {
  // Write file.
  static const size_t kHeaderSize = 44;
  static const size_t kPayloadSize = 320;
  webrtc::CodecInst codec = {
    0, "L16", 16000, static_cast<int>(kPayloadSize), 1
  };
  std::string outfile = webrtc::test::OutputPath() + "wavtest.wav";
  ASSERT_EQ(0,
            media_file_->StartRecordingAudioFile(
                outfile.c_str(), webrtc::kFileFormatWavFile, codec));
  static const int8_t kFakeData[kPayloadSize] = {0};
  ASSERT_EQ(0, media_file_->IncomingAudioData(kFakeData, kPayloadSize));
  ASSERT_EQ(0, media_file_->StopRecording());

  // Check the file we just wrote.
  static const uint8_t kExpectedHeader[] = {
    'R', 'I', 'F', 'F',
    0x64, 0x1, 0, 0,  // size of whole file - 8: 320 + 44 - 8
    'W', 'A', 'V', 'E',
    'f', 'm', 't', ' ',
    0x10, 0, 0, 0,  // size of fmt block - 8: 24 - 8
    0x1, 0,  // format: PCM (1)
    0x1, 0,  // channels: 1
    0x80, 0x3e, 0, 0,  // sample rate: 16000
    0, 0x7d, 0, 0,  // byte rate: 2 * 16000
    0x2, 0,  // block align: NumChannels * BytesPerSample
    0x10, 0,  // bits per sample: 2 * 8
    'd', 'a', 't', 'a',
    0x40, 0x1, 0, 0,  // size of payload: 320
  };
  static_assert(sizeof(kExpectedHeader) == kHeaderSize, "header size");

  EXPECT_EQ(kHeaderSize + kPayloadSize, webrtc::test::GetFileSize(outfile));
  FILE* f = fopen(outfile.c_str(), "rb");
  ASSERT_TRUE(f);

  uint8_t header[kHeaderSize];
  ASSERT_EQ(1u, fread(header, kHeaderSize, 1, f));
  EXPECT_EQ(0, memcmp(kExpectedHeader, header, kHeaderSize));

  uint8_t payload[kPayloadSize];
  ASSERT_EQ(1u, fread(payload, kPayloadSize, 1, f));
  EXPECT_EQ(0, memcmp(kFakeData, payload, kPayloadSize));

  EXPECT_EQ(0, fclose(f));
}
