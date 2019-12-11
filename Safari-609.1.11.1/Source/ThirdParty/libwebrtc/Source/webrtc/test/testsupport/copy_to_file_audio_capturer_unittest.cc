/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/testsupport/copy_to_file_audio_capturer.h"

#include <memory>
#include <utility>

#include "absl/memory/memory.h"
#include "modules/audio_device/include/test_audio_device.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {
namespace test {

class CopyToFileAudioCapturerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    temp_filename_ = webrtc::test::TempFilename(
        webrtc::test::OutputPath(), "copy_to_file_audio_capturer_unittest");
    std::unique_ptr<TestAudioDeviceModule::Capturer> delegate =
        TestAudioDeviceModule::CreatePulsedNoiseCapturer(32000, 48000);
    capturer_ = absl::make_unique<CopyToFileAudioCapturer>(std::move(delegate),
                                                           temp_filename_);
  }

  void TearDown() override { ASSERT_EQ(remove(temp_filename_.c_str()), 0); }

  std::unique_ptr<CopyToFileAudioCapturer> capturer_;
  std::string temp_filename_;
};

TEST_F(CopyToFileAudioCapturerTest, Capture) {
  rtc::BufferT<int16_t> expected_buffer;
  ASSERT_TRUE(capturer_->Capture(&expected_buffer));
  ASSERT_TRUE(!expected_buffer.empty());
  // Destruct capturer to close wav file.
  capturer_.reset(nullptr);

  // Read resulted file content with |wav_file_capture| and compare with
  // what was captured.
  std::unique_ptr<TestAudioDeviceModule::Capturer> wav_file_capturer =
      TestAudioDeviceModule::CreateWavFileReader(temp_filename_, 48000);
  rtc::BufferT<int16_t> actual_buffer;
  wav_file_capturer->Capture(&actual_buffer);
  ASSERT_EQ(actual_buffer, expected_buffer);
}

}  // namespace test
}  // namespace webrtc
