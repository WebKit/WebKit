/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <array>

#include "webrtc/common_audio/wav_file.h"
#include "webrtc/common_audio/wav_header.h"
#include "webrtc/test/fake_audio_device.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/testsupport/fileutils.h"

namespace webrtc {
namespace test {

namespace {
void RunTest(const std::vector<int16_t>& input_samples,
             const std::vector<int16_t>& expected_samples,
             size_t samples_per_frame) {
  const ::testing::TestInfo* const test_info =
      ::testing::UnitTest::GetInstance()->current_test_info();

  const std::string output_filename = test::OutputPath() +
      "BoundedWavFileWriterTest_" + test_info->name() + ".wav";

  static const size_t kSamplesPerFrame = 8;
  static const int kSampleRate = kSamplesPerFrame * 100;
  EXPECT_EQ(FakeAudioDevice::SamplesPerFrame(kSampleRate), kSamplesPerFrame);

  {
    std::unique_ptr<FakeAudioDevice::Renderer> writer =
        FakeAudioDevice::CreateBoundedWavFileWriter(output_filename, 800);

    for (size_t i = 0; i < input_samples.size(); i += kSamplesPerFrame) {
      EXPECT_TRUE(writer->Render(rtc::ArrayView<const int16_t>(
          &input_samples[i],
          std::min(kSamplesPerFrame, input_samples.size() - i))));
    }
  }

  {
    WavReader reader(output_filename);
    std::vector<int16_t> read_samples(expected_samples.size());
    EXPECT_EQ(expected_samples.size(),
              reader.ReadSamples(read_samples.size(), read_samples.data()));
    EXPECT_EQ(expected_samples, read_samples);

    EXPECT_EQ(0u, reader.ReadSamples(read_samples.size(), read_samples.data()));
  }

  remove(output_filename.c_str());
}
}  // namespace

TEST(BoundedWavFileWriterTest, NoSilence) {
  static const std::vector<int16_t> kInputSamples = {
      75, 1234, 243, -1231, -22222, 0, 3, 88,
      1222, -1213, -13222, -7, -3525, 5787, -25247, 8
  };
  static const std::vector<int16_t> kExpectedSamples = kInputSamples;
  RunTest(kInputSamples, kExpectedSamples, 8);
}

TEST(BoundedWavFileWriterTest, SomeStartSilence) {
  static const std::vector<int16_t> kInputSamples = {
      0, 0, 0, 0, 3, 0, 0, 0,
      0, 3, -13222, -7, -3525, 5787, -25247, 8
  };
  static const std::vector<int16_t> kExpectedSamples(kInputSamples.begin() + 10,
                                                     kInputSamples.end());
  RunTest(kInputSamples, kExpectedSamples, 8);
}

TEST(BoundedWavFileWriterTest, NegativeStartSilence) {
  static const std::vector<int16_t> kInputSamples = {
      0, -4, -6, 0, 3, 0, 0, 0,
      0, 3, -13222, -7, -3525, 5787, -25247, 8
  };
  static const std::vector<int16_t> kExpectedSamples(kInputSamples.begin() + 2,
                                                     kInputSamples.end());
  RunTest(kInputSamples, kExpectedSamples, 8);
}

TEST(BoundedWavFileWriterTest, SomeEndSilence) {
  static const std::vector<int16_t> kInputSamples = {
      75, 1234, 243, -1231, -22222, 0, 1, 0,
      0, 0, 0, 0, 0, 0, 0, 0
  };
  static const std::vector<int16_t> kExpectedSamples(kInputSamples.begin(),
                                                     kInputSamples.end() - 9);
  RunTest(kInputSamples, kExpectedSamples, 8);
}

TEST(BoundedWavFileWriterTest, DoubleEndSilence) {
  static const std::vector<int16_t> kInputSamples = {
      75, 1234, 243, -1231, -22222, 0, 0, 0,
      0, -1213, -13222, -7, -3525, 5787, 0, 0
  };
  static const std::vector<int16_t> kExpectedSamples(kInputSamples.begin(),
                                                     kInputSamples.end() - 2);
  RunTest(kInputSamples, kExpectedSamples, 8);
}

TEST(BoundedWavFileWriterTest, DoubleSilence) {
  static const std::vector<int16_t> kInputSamples = {
      0, -1213, -13222, -7, -3525, 5787, 0, 0
  };
  static const std::vector<int16_t> kExpectedSamples(kInputSamples.begin() + 1,
                                                     kInputSamples.end() - 2);
  RunTest(kInputSamples, kExpectedSamples, 8);
}

TEST(BoundedWavFileWriterTest, EndSilenceCutoff) {
  static const std::vector<int16_t> kInputSamples = {
      75, 1234, 243, -1231, -22222, 0, 1, 0,
      0, 0, 0
  };
  static const std::vector<int16_t> kExpectedSamples(kInputSamples.begin(),
                                                     kInputSamples.end() - 4);
  RunTest(kInputSamples, kExpectedSamples, 8);
}

}  // namespace test
}  // namespace webrtc
