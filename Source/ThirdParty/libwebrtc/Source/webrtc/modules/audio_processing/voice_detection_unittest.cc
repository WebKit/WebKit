/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/audio_buffer.h"
#include "modules/audio_processing/test/audio_buffer_tools.h"
#include "modules/audio_processing/test/bitexactness_tools.h"
#include "modules/audio_processing/voice_detection.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

const int kNumFramesToProcess = 1000;

// Process one frame of data and produce the output.
bool ProcessOneFrame(int sample_rate_hz,
                     AudioBuffer* audio_buffer,
                     VoiceDetection* voice_detection) {
  if (sample_rate_hz > AudioProcessing::kSampleRate16kHz) {
    audio_buffer->SplitIntoFrequencyBands();
  }

  return voice_detection->ProcessCaptureAudio(audio_buffer);
}

// Processes a specified amount of frames, verifies the results and reports
// any errors.
void RunBitexactnessTest(int sample_rate_hz,
                         size_t num_channels,
                         bool stream_has_voice_reference) {
  int sample_rate_to_use = std::min(sample_rate_hz, 16000);
  VoiceDetection voice_detection(sample_rate_to_use,
                                 VoiceDetection::kLowLikelihood);

  int samples_per_channel = rtc::CheckedDivExact(sample_rate_hz, 100);
  const StreamConfig capture_config(sample_rate_hz, num_channels, false);
  AudioBuffer capture_buffer(
      capture_config.sample_rate_hz(), capture_config.num_channels(),
      capture_config.sample_rate_hz(), capture_config.num_channels(),
      capture_config.sample_rate_hz(), capture_config.num_channels());
  test::InputAudioFile capture_file(
      test::GetApmCaptureTestVectorFileName(sample_rate_hz));
  std::vector<float> capture_input(samples_per_channel * num_channels);
  bool stream_has_voice = false;
  for (int frame_no = 0; frame_no < kNumFramesToProcess; ++frame_no) {
    ReadFloatSamplesFromStereoFile(samples_per_channel, num_channels,
                                   &capture_file, capture_input);

    test::CopyVectorToAudioBuffer(capture_config, capture_input,
                                  &capture_buffer);

    stream_has_voice =
        ProcessOneFrame(sample_rate_hz, &capture_buffer, &voice_detection);
  }

  EXPECT_EQ(stream_has_voice_reference, stream_has_voice);
}

const bool kStreamHasVoiceReference = true;

}  // namespace

TEST(VoiceDetectionBitExactnessTest, Mono8kHz) {
  RunBitexactnessTest(8000, 1, kStreamHasVoiceReference);
}

TEST(VoiceDetectionBitExactnessTest, Mono16kHz) {
  RunBitexactnessTest(16000, 1, kStreamHasVoiceReference);
}

TEST(VoiceDetectionBitExactnessTest, Mono32kHz) {
  RunBitexactnessTest(32000, 1, kStreamHasVoiceReference);
}

TEST(VoiceDetectionBitExactnessTest, Mono48kHz) {
  RunBitexactnessTest(48000, 1, kStreamHasVoiceReference);
}

TEST(VoiceDetectionBitExactnessTest, Stereo8kHz) {
  RunBitexactnessTest(8000, 2, kStreamHasVoiceReference);
}

TEST(VoiceDetectionBitExactnessTest, Stereo16kHz) {
  RunBitexactnessTest(16000, 2, kStreamHasVoiceReference);
}

TEST(VoiceDetectionBitExactnessTest, Stereo32kHz) {
  RunBitexactnessTest(32000, 2, kStreamHasVoiceReference);
}

TEST(VoiceDetectionBitExactnessTest, Stereo48kHz) {
  RunBitexactnessTest(48000, 2, kStreamHasVoiceReference);
}

}  // namespace webrtc
