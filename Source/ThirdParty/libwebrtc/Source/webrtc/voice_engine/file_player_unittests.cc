/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Unit tests for FilePlayer.

#include "webrtc/voice_engine/file_player.h"

#include <stdio.h>

#include <memory>
#include <string>

#include "gflags/gflags.h"
#include "webrtc/base/md5digest.h"
#include "webrtc/base/stringencode.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/testsupport/fileutils.h"

DEFINE_bool(file_player_output, false, "Generate reference files.");

namespace webrtc {

class FilePlayerTest : public ::testing::Test {
 protected:
  static const uint32_t kId = 0;
  static const FileFormats kFileFormat = kFileFormatWavFile;
  static const int kSampleRateHz = 8000;

  FilePlayerTest()
      : player_(FilePlayer::CreateFilePlayer(kId, kFileFormat)),
        output_file_(NULL) {}

  void SetUp() override {
    if (FLAGS_file_player_output) {
      std::string output_file =
          webrtc::test::OutputPath() + "file_player_unittest_out.pcm";
      output_file_ = fopen(output_file.c_str(), "wb");
      ASSERT_TRUE(output_file_ != NULL);
    }
  }

  void TearDown() override {
    if (output_file_)
      fclose(output_file_);
  }

  void PlayFileAndCheck(const std::string& input_file,
                        const std::string& ref_checksum,
                        int output_length_ms) {
    const float kScaling = 1;
    ASSERT_EQ(0, player_->StartPlayingFile(input_file.c_str(), false, 0,
                                           kScaling, 0, 0, NULL));
    rtc::Md5Digest checksum;
    for (int i = 0; i < output_length_ms / 10; ++i) {
      int16_t out[10 * kSampleRateHz / 1000] = {0};
      size_t num_samples;
      EXPECT_EQ(
          0, player_->Get10msAudioFromFile(out, &num_samples, kSampleRateHz));
      checksum.Update(out, num_samples * sizeof(out[0]));
      if (FLAGS_file_player_output) {
        ASSERT_EQ(num_samples,
                  fwrite(out, sizeof(out[0]), num_samples, output_file_));
      }
    }
    char checksum_result[rtc::Md5Digest::kSize];
    EXPECT_EQ(rtc::Md5Digest::kSize,
              checksum.Finish(checksum_result, rtc::Md5Digest::kSize));
    EXPECT_EQ(ref_checksum,
              rtc::hex_encode(checksum_result, sizeof(checksum_result)));
  }

  std::unique_ptr<FilePlayer> player_;
  FILE* output_file_;
};

#if defined(WEBRTC_IOS)
#define MAYBE_PlayWavPcmuFile DISABLED_PlayWavPcmuFile
#else
#define MAYBE_PlayWavPcmuFile PlayWavPcmuFile
#endif
TEST_F(FilePlayerTest, MAYBE_PlayWavPcmuFile) {
  const std::string kFileName =
      test::ResourcePath("utility/encapsulated_pcmu_8khz", "wav");
  // The file is longer than this, but keeping the output shorter limits the
  // runtime for the test.
  const int kOutputLengthMs = 10000;
  const std::string kRefChecksum = "c74e7fd432d439b1311e1c16815b3e9a";

  PlayFileAndCheck(kFileName, kRefChecksum, kOutputLengthMs);
}

#if defined(WEBRTC_IOS)
#define MAYBE_PlayWavPcm16File DISABLED_PlayWavPcm16File
#else
#define MAYBE_PlayWavPcm16File PlayWavPcm16File
#endif
TEST_F(FilePlayerTest, MAYBE_PlayWavPcm16File) {
  const std::string kFileName =
      test::ResourcePath("utility/encapsulated_pcm16b_8khz", "wav");
  // The file is longer than this, but keeping the output shorter limits the
  // runtime for the test.
  const int kOutputLengthMs = 10000;
  const std::string kRefChecksum = "e41d7e1dac8aeae9f21e8e03cd7ecd71";

  PlayFileAndCheck(kFileName, kRefChecksum, kOutputLengthMs);
}

}  // namespace webrtc
